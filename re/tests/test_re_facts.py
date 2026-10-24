"""Mechanical facts of docs/re/{structs,globals,timing,rng,ispace,formats}.md.

CLAUDE.md asks for a test behind every count, offset, table value and writer
list in docs/. These tests read the ``isre asm`` listings and indexes
(``work/asm/<module>/``), the unpacked segments (``work/unpacked/<module>/``)
and the original files, and skip when those are absent.

Conventions of this file:

* A *reference* to a DGROUP location is an instruction whose memory operand
  is DS-relative with no base or index register and whose bytes overlap the
  location. Accesses through computed pointers are outside a static scan.
* A *writer* is such an instruction that stores to the operand (``mov``,
  arithmetic, ``inc``/``dec``, ``pop``, ``xchg`` ...).
* Addresses use the canonical ``segN:0xOFF`` form of the listings.
"""
from __future__ import annotations

import functools
import json
import re
import struct
from bisect import bisect_right
from collections import Counter, defaultdict
from pathlib import Path
from typing import NamedTuple, Optional

import pytest
from conftest import ORIGINAL, REPO_ROOT, require

from isre.asm import parse_line

WORK = REPO_ROOT / "work"

# ------------------------------------------------------------------ listing model


class Mem(NamedTuple):
    segreg: str       # explicit segment override, "" for the default
    regs: tuple       # base and index registers
    disp: int         # 16-bit displacement
    size: int         # bytes accessed; 1 for lea (an address, not an access)
    index: int        # operand position

    @property
    def ds(self) -> bool:
        return self.segreg == "ds" or (not self.segreg and "bp" not in self.regs)


class Insn(NamedTuple):
    seg: int
    off: int
    raw: bytes
    text: str
    mnem: str
    ops: tuple
    target: bool      # a jump or a switch case lands here
    mem: Optional[Mem]

    @property
    def addr(self) -> str:
        return f"seg{self.seg}:{self.off:#x}"

    @property
    def abs_ds(self) -> bool:
        return self.mem is not None and self.mem.ds and not self.mem.regs


_MEM_RE = re.compile(r"(?:(byte|word|dword) )?(?:ptr )?(?:([a-z]s):)?\[([^\]]*)\]")
_SIZES = {"byte": 1, "word": 2, "dword": 4}
_IMM_RE = re.compile(r"-?(?:0x[0-9a-f]+|\d+)")
_PREFIXES = {0x26, 0x2E, 0x36, 0x3E, 0x64, 0x65, 0x66, 0x67, 0xF0, 0xF2, 0xF3}


def A(a: str) -> tuple[int, int]:
    """(segment, offset) of ``segN:0xOFF``."""
    s, o = a[3:].split(":")
    return int(s), int(o, 16)


def _mem(mnem: str, ops: tuple) -> Optional[Mem]:
    for i, op in enumerate(ops):
        m = _MEM_RE.search(op)
        if m is None:
            continue
        regs, disp, sign = [], 0, 1
        for tok in re.findall(r"[+-]|[^+\-\s]+", m.group(3)):
            if tok == "+":
                sign = 1
            elif tok == "-":
                sign = -1
            elif tok in ("bx", "si", "di", "bp"):
                regs.append(tok)
            else:
                disp += sign * int(tok, 0)
        if mnem == "lea":
            size = 1
        elif m.group(1):
            size = _SIZES[m.group(1)]
        else:
            size = 4                    # les/lds "ptr [..]", lcall/ljmp "[..]"
        return Mem(m.group(2) or "", tuple(regs), disp & 0xFFFF, size, i)
    return None


def _parse_listing(path: Path) -> list[Insn]:
    out = []
    for line in path.read_text().splitlines():
        if not line.startswith("seg"):
            continue
        row = parse_line(line)
        if row is None or not row[2]:
            continue
        seg, off, raw, text, comment = row
        mnem, _, rest = text.partition(" ")
        if mnem in ("db", "dw", "dd"):
            continue
        ops = tuple(o.strip() for o in rest.split(", ")) if rest else ()
        target = any(p.startswith(("<- ", "case ")) for p in comment.split("; "))
        out.append(Insn(seg, off, raw, text, mnem, ops, target, _mem(mnem, ops)))
    return out


_WRITE_MNEMS = {"mov", "add", "adc", "sub", "sbb", "and", "or", "xor", "inc", "dec", "neg",
                "not", "shl", "sal", "shr", "sar", "rol", "ror", "rcl", "rcr", "pop", "shld",
                "shrd", "bts", "btr", "btc", "xadd", "cmpxchg", "fst", "fstp", "fist",
                "fistp", "fbstp", "fnstsw", "fstsw", "fnstcw", "fstcw"}


def is_write(i: Insn) -> bool:
    if i.mem is None:
        return False
    if i.mnem == "xchg":
        return True
    return i.mem.index == 0 and (i.mnem in _WRITE_MNEMS or i.mnem.startswith("set"))


_FAMILY = {r: fam for fam in (("al", "ah", "ax", "eax"), ("bl", "bh", "bx", "ebx"),
                              ("cl", "ch", "cx", "ecx"), ("dl", "dh", "dx", "edx"),
                              ("si", "esi"), ("di", "edi"), ("bp", "ebp"))
           for r in fam}
_WIDTH = {r: (4 if r.startswith("e") else 1 if r[1] in "lh" and r not in ("si", "di", "bp")
              else 2) for r in _FAMILY}
_FLOW = {"call", "lcall", "jmp", "ljmp", "ret", "retf", "iret", "int", "into"}
_STRING = ("rep", "movs", "stos", "lods", "cmps", "scas", "ins", "outs")


def _implicit_writes(i: Insn) -> set[str]:
    """Register families an instruction writes without naming them first."""
    m = i.mnem
    if m in ("call", "lcall"):
        return {"ax", "bx", "cx", "dx"}            # Borland far calls keep si, di
    if m in ("mul", "div", "idiv") or (m == "imul" and len(i.ops) == 1):
        return {"ax", "dx"}
    if m in ("cwd", "cdq"):
        return {"dx"}
    if m in ("cbw", "cwde", "lahf", "in", "xlatb", "aam", "aad", "daa", "das", "aaa", "aas"):
        return {"ax"}
    if m.startswith("loop") or m == "jcxz":
        return {"cx"}
    if m.startswith(_STRING) and m not in ("movsx", "movzx"):
        return {"ax", "cx", "si", "di"}
    return set()


def writes_reg(i: Insn, reg: str) -> bool:
    """Whether ``i`` changes 16-bit register ``reg`` (or a part of it)."""
    fam = _FAMILY[reg]
    if any(_FAMILY[r] == fam for r in _implicit_writes(i) if r in _FAMILY):
        return True
    if not i.ops or i.mnem in ("cmp", "test", "push", "out", "bt") or i.mnem.startswith("j"):
        return False
    if i.mnem in ("mul", "div", "idiv", "imul") and len(i.ops) == 1:
        return False
    if i.mnem == "xchg":
        return any(o in fam for o in i.ops)
    return i.ops[0] in fam


class Module:
    """One module's listings, functions.json, xrefs.json and unpacked segments."""

    def __init__(self, name: str):
        d = require(WORK / "asm" / name)
        self.name = name
        self.insns: list[Insn] = []
        for p in sorted(d.glob("seg*.asm")):
            self.insns.extend(_parse_listing(p))
        self.insns.sort(key=lambda i: (i.seg, i.off))
        self.pos = {(i.seg, i.off): n for n, i in enumerate(self.insns)}
        self.fj = json.loads(require(d / "functions.json").read_text())
        self.xj = json.loads(require(d / "xrefs.json").read_text())
        self.funcs = {f["addr"]: f for f in self.fj["functions"]}
        self.switches = {s["at"]: s for s in self.fj["switches"]}
        spans = defaultdict(list)
        for f in self.fj["functions"]:
            for a, b in f["ranges"]:
                spans[f["seg"]].append((A(a)[1], A(b)[1], f["addr"]))
        self._spans = {s: sorted(v) for s, v in spans.items()}
        self._starts = {s: [x[0] for x in v] for s, v in self._spans.items()}

    def at(self, a: str) -> Insn:
        n = self.pos.get(A(a))
        assert n is not None, f"{self.name} {a} is not an instruction start"
        return self.insns[n]

    def step(self, i: Insn, k: int) -> Insn:
        j = self.insns[self.pos[(i.seg, i.off)] + k]
        assert j.seg == i.seg
        return j

    def owners(self, seg: int, off: int) -> set[str]:
        spans = self._spans.get(seg, [])
        n = bisect_right(self._starts.get(seg, []), off)
        return {f for a, b, f in spans[:n] if a <= off < b}

    def in_func(self, i: Insn, func: str) -> bool:
        return any(A(a)[1] <= i.off < A(b)[1] and A(a)[0] == i.seg
                   for a, b in self.funcs[func]["ranges"])

    def func_insns(self, func: str) -> list[Insn]:
        return [i for i in self.insns if i.seg == self.funcs[func]["seg"] and self.in_func(i, func)]

    def call_sites(self, target: str) -> list[str]:
        """Call instructions (near, far, jmp far) whose target is ``target``."""
        return sorted({a for a, k in self.funcs[target]["callers"]
                       if k in ("near", "far", "jmp_far")}, key=A)

    def refs(self, lo: int, hi: int) -> list[Insn]:
        """Instructions whose direct DS operand overlaps DGROUP:lo..hi (inclusive)."""
        return [i for i in self.insns
                if i.abs_ds and i.mem.disp <= hi and i.mem.disp + i.mem.size > lo]

    def writers(self, lo: int, hi: int) -> list[Insn]:
        return [i for i in self.refs(lo, hi) if is_write(i)]

    def stored_value(self, i: Insn) -> Optional[int]:
        """The constant that ``mov <mem>, src`` stores, or None when computed.

        A register source is followed back through straight-line code to the
        instruction that sets it; a jump target on the way makes it unknown.
        """
        src = i.ops[-1]
        if _IMM_RE.fullmatch(src):
            return int(src, 0) & 0xFFFF
        if src not in _FAMILY:
            return None
        n = self.pos[(i.seg, i.off)]
        cur = i
        while not cur.target:
            n -= 1
            cur = self.insns[n]
            if cur.seg != i.seg or cur.mnem in _FLOW:
                return None
            if writes_reg(cur, src):
                wide = cur.ops and cur.ops[0] in _FAMILY and _WIDTH[cur.ops[0]] >= _WIDTH[src]
                if cur.mnem == "mov" and wide and _IMM_RE.fullmatch(cur.ops[1]):
                    return int(cur.ops[1], 0) & 0xFFFF
                if cur.mnem in ("xor", "sub") and wide and cur.ops[1] == cur.ops[0]:
                    return 0
                return None
        return None


@functools.lru_cache(maxsize=None)
def module(name: str) -> Module:
    return Module(name)


def unpacked_seg(name: str, seg: int) -> bytes:
    return require(WORK / "unpacked" / name / f"seg{seg:02d}.bin").read_bytes()


def unpacked_segs(name: str) -> dict[int, bytes]:
    d = require(WORK / "unpacked" / name)
    return {int(p.stem[3:]): p.read_bytes() for p in sorted(d.glob("seg*.bin"))}


def s16(v: int) -> int:
    return v - 0x10000 if v & 0x8000 else v


@pytest.fixture(scope="module")
def ins() -> Module:
    return module("inspace")


@pytest.fixture(scope="module")
def isp() -> Module:
    return module("ispace")


@pytest.fixture(scope="module")
def fac() -> Module:
    return module("factory-1998a")


@pytest.fixture(scope="module")
def dgroup() -> bytes:
    return unpacked_seg("inspace", 19)


def _sites(insns) -> list[str]:
    return [i.addr for i in insns]


# ------------------------------------------------------------------ jump tables (globals.md)


def test_inspace_largest_jump_tables(ins):
    """globals.md Jump tables: the five largest tables and their functions."""
    top = sorted(ins.fj["switches"], key=lambda s: -s["count"])[:5]
    assert [(s["at"], s["count"]) for s in top] == [
        ("seg3:0x1351", 192), ("seg16:0x1e69", 65), ("seg11:0xadb5", 23),
        ("seg17:0x350", 19), ("seg17:0x5d1", 17)]
    owners = [ins.owners(*A(s["at"])) for s in top]
    assert owners == [{"seg3:0x6ec"}, {"seg16:0xcfe"}, {"seg11:0xac86"}, {"seg17:0x0"},
                      {"seg17:0x3b2"}]


def test_tick_jump_table(ins):
    """globals.md Game tick, timing.md: 10 entries at seg7:0x49c, cases per state."""
    sw = ins.switches["seg7:0x497"]
    assert (sw["table"], sw["count"]) == ("seg7:0x49c", 10)
    assert sw["targets"] == ["seg7:0x20a", "seg7:0x4b0", "seg7:0x363", "seg7:0x2cc",
                             "seg7:0x3f1", "seg7:0x42c", "seg7:0xff", "seg7:0x4b0",
                             "seg7:0x197", "seg7:0x1bf"]
    bound = ins.at(sw["bound_at"])
    assert bound.text == "cmp ax, 9"
    assert ins.step(bound, 1).text == "ja seg7:0x4b0"     # values outside 0-9 share state 1's case
    assert ins.owners(*A("seg7:0x497")) == {"seg7:0x0"}


def test_gameplay_input_handler_has_no_jump_table(ins):
    """globals.md Input path: seg11:0x6b64 compares msg and wParam directly."""
    assert not [s for s in ins.fj["switches"] if "seg11:0x6b64" in ins.owners(*A(s["at"]))]


# ------------------------------------------------------------------ pools and ship records (structs.md)


def test_pool15_pointer_referenced_only_in_seg18(ins):
    """structs.md Pool roles: every reference to DGROUP:0x1e34 lies in seg18."""
    refs = ins.refs(0x1E34, 0x1E37)
    assert {"seg18:0xe81", "seg18:0x10b7", "seg18:0x18fc"} <= set(_sites(refs))
    assert {i.seg for i in refs} == {18}
    assert not [i.addr for i in ins.insns if i.mem is not None and i.mem.ds and i.mem.regs
                and 0x1E34 <= i.mem.disp <= 0x1E37 and i.seg != 18]


def test_pool14_pointer_referenced_from_14_segments(ins):
    """globals.md Global table: code in most segments reads DGROUP:0x1e30 directly."""
    segs = {i.seg for i in ins.refs(0x1E30, 0x1E33)}
    assert segs == {2, 3, 4, 6, 7, 8, 9, 10, 11, 12, 13, 16, 17, 18}
    assert len(segs) == 14


def test_no_operand_address_in_ship_records_2_to_7(ins):
    """structs.md Ship records, Count: no operand addresses DGROUP:0x7280-0x7b5f."""
    hits = [i.addr for i in ins.insns
            if i.mem is not None and i.mem.ds and 0x7280 <= i.mem.disp <= 0x7B5F]
    assert hits == []
    # the table after the eighth record
    assert ins.at("seg10:0x14be").mem.disp == 0x7B60 == 0x7090 + 8 * 0x15A


def test_no_operand_extent_reaches_0x7282(ins):
    """structs.md Ship records, Count: only seg11:0x9fbc (DGROUP:0x727e, a dword) reaches
    DGROUP:0x7280, and no operand reaches DGROUP:0x7282-0x7b5f."""
    assert _sites(ins.refs(0x7280, 0x7B5F)) == ["seg11:0x9fbc"]
    assert ins.refs(0x7282, 0x7B5F) == []


def _stride_operands(m: Module) -> list[tuple[Insn, int]]:
    """DS operands indexed by a register that ``imul r, x, 0x15a`` set.

    Tracking survives jump targets (a multiple reaches them on some path), ends
    after unconditional transfers, and ends for a register when anything writes
    it. Far calls clobber ax, bx, cx and dx.
    """
    out = []
    tracked: set[str] = set()
    prev_seg = None
    for i in m.insns:
        if i.seg != prev_seg:
            tracked, prev_seg = set(), i.seg
        if i.mem is not None and i.mem.ds and tracked & set(i.mem.regs):
            out.append((i, i.mem.disp))
        if i.mnem == "imul" and len(i.ops) == 3 and i.ops[2] == "0x15a":
            tracked.add(i.ops[0])
            continue
        tracked = {r for r in tracked if not writes_reg(i, r)}
        if i.mnem in ("jmp", "ljmp", "ret", "retf", "iret"):
            tracked = set()
    return out


def test_ship_record_stride_operands_lie_in_record_0(ins):
    """structs.md Ship records, Base and stride."""
    ops = _stride_operands(ins)
    assert ops
    outside = [(i.addr, hex(d)) for i, d in ops if not (0x7090 <= d and d + i.mem.size <= 0x71EA)]
    assert outside == []
    lo = min(ops, key=lambda t: t[1])
    hi = max(ops, key=lambda t: t[1] + t[0].mem.size)
    assert (lo[0].addr, lo[1], lo[0].mem.size) == ("seg4:0x7084", 0x7090, 2)
    assert (hi[0].addr, hi[1], hi[0].mem.size) == ("seg7:0xdbc", 0x71E8, 2)


# ------------------------------------------------------------------ writer lists (globals.md)


def _write_values(m: Module, lo: int, hi: int) -> dict[str, Optional[int]]:
    return {i.addr: m.stored_value(i) for i in m.writers(lo, hi)}


def test_wave_kind_writers(ins):
    """globals.md Game state globals, DGROUP:0x9320: only seg9:0x3202 writes it."""
    w = ins.writers(0x9320, 0x9321)
    assert w and all(ins.in_func(i, "seg9:0x3202") for i in w)
    vals = {i.addr: ins.stored_value(i) for i in w}
    for site in ("seg9:0x364e", "seg9:0x3972", "seg9:0x3769", "seg9:0x3719", "seg9:0x36de",
                 "seg9:0x3756", "seg9:0x3ac4", "seg9:0x3e00", "seg9:0x366f", "seg9:0x36a6",
                 "seg9:0x3928", "seg9:0x3d72"):
        assert site in vals
    consts = {s16(v) for v in vals.values() if v is not None}
    assert consts == {-1, 0, 2, 3, 4, 5, 6, 7}
    # bytes: seg9:0x366f stores ax after xor ax, ax; seg9:0x36a6 stores cx after mov cx, 2
    assert (vals["seg9:0x366f"], vals["seg9:0x36a6"]) == (0, 2)
    assert [s for s, v in vals.items() if v is None] == ["seg9:0x3928"]


GAME_STATE_WRITERS = {
    "seg6:0x18f0": 0,
    "seg9:0x60ff": 1, "seg16:0x1bcf": 1, "seg16:0x6ac5": 1, "seg17:0x5fff": 1,
    "seg9:0x6ce5": 2, "seg9:0x5d19": 3, "seg9:0xd9b": 4, "seg6:0x5dbd": 5,
    "seg7:0x5fa": 6, "seg18:0xff": 6, "seg17:0x57f5": 7,
    "seg7:0x659": 8, "seg9:0x6c62": 8,
    "seg2:0xe2d": 9, "seg10:0x8824": 9, "seg16:0x1bc7": 9,
    "seg2:0x1db9": None,                       # copies DGROUP:0x968e
}


def test_game_state_writers(ins):
    """globals.md Game state globals: the complete writer list of DGROUP:0x90f4."""
    assert _write_values(ins, 0x90F4, 0x90F5) == GAME_STATE_WRITERS
    copy = ins.at("seg2:0x1db9")
    assert ins.step(copy, -1).text == "mov ax, word ptr [0x968e]"
    assert [s for s, v in GAME_STATE_WRITERS.items() if v == 7] == ["seg17:0x57f5"]


def test_next_state_writers(ins):
    """globals.md Game tick: DGROUP:0x968e gets 0, 1 or 4, at these sites."""
    assert _write_values(ins, 0x968E, 0x968F) == {
        "seg6:0x36a0": 1, "seg7:0xe9": 0, "seg7:0x402": 0, "seg7:0x44d": 4,
        "seg7:0x69a": 0, "seg11:0x6c61": 0, "seg11:0x78e7": 0}


def test_state7_window_writers(ins):
    """globals.md State 7: seg17:0x57fe and seg17:0x600e are the only writes of DGROUP:0x9460."""
    assert _sites(ins.writers(0x9460, 0x9461)) == ["seg17:0x57fe", "seg17:0x600e"]
    assert ins.stored_value(ins.at("seg17:0x600e")) == 0
    assert all(ins.in_func(ins.at(s), "seg17:0x5782") for s in ("seg17:0x57fe", "seg17:0x600e"))


def test_screen_state_5_threshold_writer(ins):
    """timing.md The frame gate: only seg6:0xa6c writes DGROUP:0x9002."""
    w = ins.writers(0x9002, 0x9003)
    assert _sites(w) == ["seg6:0xaa8"] and ins.in_func(w[0], "seg6:0xa6c")


def test_95bc_writers(ins):
    """timing.md Window timers: the writers of DGROUP:0x95bc."""
    assert _sites(ins.writers(0x95BC, 0x95BD)) == [
        "seg4:0x4cc", "seg4:0x5d8", "seg8:0x142", "seg8:0x179", "seg13:0x1925",
        "seg13:0x1941", "seg15:0x5974", "seg15:0x5a64"]


def test_9142_counter_and_readers(ins):
    """timing.md Adaptive wave threshold: seg7:0x9dc counts DGROUP:0x9142; its readers."""
    refs = ins.refs(0x9142, 0x9143)
    counter = {"seg7:0x9dc", "seg7:0x9e0", "seg7:0x9e7"}
    assert counter <= set(_sites(refs))
    assert ins.at("seg7:0x9e0").text == "cmp word ptr [0x9142], 0x36"     # 0-54
    assert {i.addr for i in refs if is_write(i)} == {"seg7:0x9dc", "seg7:0x9e7"}
    assert [i.addr for i in refs if i.addr not in counter] == [
        "seg7:0x317f", "seg7:0x3193", "seg7:0x32f4", "seg7:0x3e63", "seg12:0xb2d",
        "seg12:0x5417"]


def test_calibrate_comparisons_in_seg15_seg16(ins):
    """timing.md Calibrate: the constants seg15 and seg16 compare DGROUP:0x8ffc with."""
    refs = [i for i in ins.refs(0x8FFC, 0x8FFD) if i.seg in (15, 16)]
    assert refs and all(i.mnem == "cmp" and _IMM_RE.fullmatch(i.ops[1]) for i in refs)
    assert {int(i.ops[1], 0) for i in refs} == {300, 800, 850, 900, 1300, 1400, 1500, 1600, 2000}
    assert {"seg15:0x394f", "seg15:0x7cef", "seg16:0x272b"} <= set(_sites(refs))


def test_rate_settings_reader_callers(ins):
    """timing.md Rate settings: seg6:0xb3e is called only from seg6:0xa6c and seg10:0x7f18."""
    sites = ins.call_sites("seg6:0xb3e")
    assert [ins.owners(*A(s)) for s in sites] == [{"seg6:0xa6c"}, {"seg10:0x7f18"}]


SCORE_ADDITIONS = {
    "seg8:0x7c3b": None, "seg4:0x2ef8": None, "seg7:0x196a": 800, "seg8:0x6785": 3000,
    "seg8:0x7971": 3000, "seg8:0x67a6": 1000, "seg13:0x1b4a": 1000, "seg8:0x92cd": 400,
    "seg16:0x7228": 50000, "seg13:0x1c32": None}


def test_score_writers(ins):
    """globals.md Scoring: the additions to DGROUP:0x1e58, plus clear and load."""
    w = [i for i in ins.writers(0x1E58, 0x1E5B) if i.mem.disp == 0x1E58]   # adc partners skipped
    adds = {i.addr: (int(i.ops[1], 0) if _IMM_RE.fullmatch(i.ops[1]) else None)
            for i in w if i.mnem == "add"}
    assert adds == SCORE_ADDITIONS
    assert {i.addr for i in w if i.mnem != "add"} == {"seg9:0xc4e", "seg6:0x968a"}
    for s, amount in SCORE_ADDITIONS.items():     # 16-bit adds carry into the high word
        i = ins.at(s)
        if i.mem.size == 2:
            assert ins.step(i, 1).text.startswith("adc word ptr [0x1e5a], ")


def test_rating_writers(ins):
    """globals.md Scoring: the sites that change DGROUP:0x5078.

    The damage row names seg8:0x46f4 and cites seg8:0x4736 and seg8:0x476c as
    evidence; all three are writers.
    """
    w = [i for i in ins.writers(0x5078, 0x507B) if i.mem.disp == 0x5078]
    assert set(_sites(w)) == {
        "seg8:0x7c34", "seg8:0x7c57", "seg8:0x7c8c", "seg8:0x7cad", "seg8:0x7cd0",
        "seg4:0x2ef0", "seg13:0x1b53", "seg13:0x1c3a", "seg8:0x46f4", "seg8:0x4736",
        "seg8:0x476c", "seg16:0xb19", "seg16:0x34e0",
        "seg9:0xc52", "seg6:0x969b"}                  # clear and load
    assert ins.at("seg13:0x1b53").text == "add dword ptr [0x5078], 0x3e8"


def test_race_difficulty_writers(ins):
    """globals.md Race bonus: constants 0-2 to DGROUP:0x94a8 and 0-5 to DGROUP:0x94aa."""
    for lo, consts in ((0x94A8, {0, 1, 2}), (0x94AA, {0, 1, 2, 3, 4, 5})):
        w = ins.writers(lo, lo + 1)
        wave = [i for i in w if ins.in_func(i, "seg9:0x3202")]
        assert {ins.stored_value(i) for i in wave} == consts
        assert {i.seg for i in w if i not in wave} == {6}
    assert {"seg6:0x3267", "seg6:0x326a"} <= {i.addr for i in ins.writers(0x94A8, 0x94AB)}


# ------------------------------------------------------------------ initialised data


def test_scoring_tables(dgroup):
    """globals.md Scoring: P, W and the rating thresholds."""
    assert struct.unpack_from("<21h", dgroup, 0xB0E) == (
        1000, 600, 1200, 900, 200, 0, 1000, 0, 3400, 500, 500, 0, 0, 5000) + (0,) * 7
    assert struct.unpack_from("<21h", dgroup, 0xB38) == (
        1, 1, 1, 2, 0, 0, 0, 0, 4, 3, 0, 0, 0, 0, 8) + (0,) * 6
    assert struct.unpack_from("<12i", dgroup, 0x9BC) == (
        26000, 40000, 55000, 70000, 90000, 120000, 150000, 200000, 280000, 400000, 550000,
        700000)


def test_initialised_timing_and_sound_data(dgroup):
    """timing.md Rate settings (rate 36, F 27, S 26); ispace.md ordinal 7 (3200, 3200)."""
    assert struct.unpack_from("<h", dgroup, 0x59C) == (36,)
    assert struct.unpack_from("<2I", dgroup, 0x5C4) == (27, 26)
    assert struct.unpack_from("<2h", dgroup, 0x5A0) == (3200, 3200)


def _cstr(data: bytes, off: int) -> bytes:
    return data[off:data.index(b"\0", off)]


def test_initialised_strings(dgroup):
    """globals.md Debug traces and WinMain; ispace.md Loading and Calling convention."""
    assert {o: _cstr(dgroup, o) for o in (0x98, 0xAF, 0x13E0, 0x13FD, 0x141A, 0x5E5, 0xC5,
                                          0x82, 0x8E, 0x1EA, 0x4EA, 0x783, 0x78F, 0x7C6)} == {
        0x98: b"GMAIN iMainSmart = ", 0xAF: b"WndProc iMainSmart = ",
        0x13E0: b"GameFrame 2 iMainSmart = ", 0x13FD: b"GameFrame 3 iMainSmart = ",
        0x141A: b"GameFrame 1 iMainSmart = ", 0x5E5: b"Frame iMainSmart = ",
        0xC5: b": Message is ", 0x82: b"Inner Space", 0x8E: b"MainAccel", 0x1EA: b"Aux",
        0x4EA: b"All timers used. Can't run game.", 0x783: b"Can't find ",
        0x78F: b". Try reinstalling, do not preserve previous settings.",
        0x7C6: b" is faulty"}
    assert [_cstr(dgroup, o) for o in (0x744, 0x7DE, 0x7E4)] == [b"", b"", b""]


def test_roster_class_band_tables(dgroup):
    """formats.md Ship roster records +0x76: the band edges of DGROUP:0x952 and 0x96c."""
    edges = (0, 28, 34, 40, 48, 55, 63, 70, 80, 97, 114)
    assert struct.unpack_from("<11h", dgroup, 0x952 + 2) == edges
    assert struct.unpack_from("<11h", dgroup, 0x96C) == edges


def test_wndproc_is_export_1(ins):
    """globals.md WndProc (seg2:0x224, export ordinal 1)."""
    assert [f["addr"] for f in ins.fj["functions"] if "1" in f["export"]] == ["seg2:0x224"]


def test_dgroup_segments(ins, isp, fac):
    """CLAUDE.md and formats.md: DGROUP is seg19, seg11 and seg6 (both Factory builds)."""
    from isre.ne import NEModule

    assert (ins.fj["dgroup"], isp.fj["dgroup"], fac.fj["dgroup"]) == (19, 11, 6)
    assert NEModule.load(require(ORIGINAL / "theworks" / "FACTORY.EXE")).autodata == 6


# ------------------------------------------------------------------ rng.md


def test_inspace_rand_and_srand(ins):
    """rng.md The generator: constants, helper, output bits and srand's stores."""
    rand = [i.text for i in ins.func_insns("seg5:0xdc")]
    assert rand[4:6] == ["mov ax, 0x43fd", "mov dx, 3"]                 # 0x000343fd
    assert "call far seg5:0x314" in rand
    k = rand.index("add ax, 0x9ec3")
    assert rand[k:k + 6] == ["add ax, 0x9ec3", "adc dx, 0x26",              # 0x00269ec3
                             "mov word ptr [0x144c], ax", "mov word ptr [0x144e], dx",
                             "mov ax, dx", "and ah, 0x7f"]                 # bits 16-30
    assert ins.at("seg5:0xcc").text == "mov word ptr [0x144c], ax"
    assert ins.at("seg5:0xcf").text == "mov word ptr [0x144e], 0"
    assert ins.at("seg5:0xc9").text == "mov ax, word ptr [bp + 6]"


def test_rand_state_referenced_only_by_rand_and_srand(ins, dgroup):
    """rng.md: only seg5:0xc4 and seg5:0xdc touch DGROUP:0x144c; it starts at 1."""
    refs = ins.refs(0x144C, 0x144F)
    assert _sites(refs) == ["seg5:0xcc", "seg5:0xcf", "seg5:0xe9", "seg5:0xed", "seg5:0xfc",
                            "seg5:0xff"]
    assert {f for i in refs for f in ins.owners(i.seg, i.off)} == {"seg5:0xc4", "seg5:0xdc"}
    assert not [i for i in ins.insns if i.mem is not None and i.mem.regs and i.mem.ds
                and 0x144C <= i.mem.disp <= 0x144F]
    assert struct.unpack_from("<I", dgroup, 0x144C) == (1,)


def test_rand_call_sites(ins):
    """rng.md Callers: 382 sites in 101 functions, by segment."""
    sites = [A(s) for s in ins.call_sites("seg5:0xdc")]
    assert len(sites) == 382
    assert Counter(s for s, _o in sites) == {
        9: 107, 12: 41, 6: 39, 4: 28, 8: 28, 15: 24, 18: 22, 14: 21, 11: 19, 13: 16, 7: 14,
        10: 13, 16: 8, 2: 1, 3: 1}
    owners = Counter(f for s in sites for f in ins.owners(*s))
    assert len(owners) == 101
    assert owners["seg10:0x754a"] == owners["seg10:0x7944"] == 0       # sound wrappers
    assert owners["seg11:0xa4a6"] == 14
    assert owners["seg10:0xa332"] == 1                                  # formatter's R


def test_srand_call_sites(ins):
    """rng.md Seeds: three srand calls, each with AX of GetCurrentTime."""
    sites = ins.call_sites("seg5:0xc4")
    assert sites == ["seg2:0x1b6b", "seg11:0xb28", "seg11:0xa4bb"]
    assert [ins.owners(*A(s)) for s in sites] == [{"seg2:0x1ac8"}, {"seg11:0xafe"},
                                                  {"seg11:0xa4a6"}]
    for s in sites:
        call = ins.at(s)
        assert ins.step(call, -1).text == "push ax"
        assert ins.step(call, -2).text == "call far USER.15 GetCurrentTime"
    assert ins.call_sites("seg11:0xafe") == ["seg2:0x9b0", "seg11:0x3e48", "seg16:0x1e57",
                                             "seg18:0x74d"]
    a4a6 = ins.call_sites("seg11:0xa4a6")
    assert a4a6[:2] == ["seg11:0xa371", "seg15:0x80ce"]
    assert len(a4a6) == 6 and {A(s)[0] for s in a4a6[2:]} == {18}


def test_rand_result_uses(ins):
    """rng.md Callers and range mapping: 270 constant moduli, 16 bit tests, 96 others."""
    kinds, moduli = Counter(), Counter()
    for s in ins.call_sites("seg5:0xdc"):
        call = ins.at(s)
        n1, n2, n3 = (ins.step(call, k).text for k in (1, 2, 3))
        m = re.fullmatch(r"mov cx, (0x[0-9a-f]+|\d+)", n1)
        if m and n2 in ("cwd", "cdq") and n3 == "idiv cx":
            kinds["mod"] += 1
            moduli[int(m.group(1), 0)] += 1
        elif re.fullmatch(r"test al, [137]|and ax, 1", n1):
            kinds["bit"] += 1
        else:
            kinds["other"] += 1
    assert kinds == {"mod": 270, "bit": 16, "other": 96}
    assert (min(moduli), max(moduli)) == (1, 26000)
    assert [moduli[n] for n in (3, 2, 100, 4, 6, 10)] == [37, 28, 18, 14, 14, 13]
    assert [n for n, _c in moduli.most_common(6)] == [3, 2, 100, 4, 6, 10]
    for s, n in (("seg4:0x847e", 4000), ("seg9:0xe81", 50), ("seg9:0xe99", 50),
                 ("seg10:0x8f8", 50), ("seg10:0x910", 50), ("seg10:0xab1", 50),
                 ("seg10:0xac9", 50)):
        call = ins.at(s)
        assert ins.step(call, 1).mnem == "les"
        assert ins.step(call, 2).text == f"mov cx, {n:#x}"
        assert ins.step(call, 4).text == "idiv cx"


def test_ispace_rand(isp):
    """rng.md ISPACE.DLL: srand seg3:0x650, rand seg3:0x668, state DGROUP:0xb8c, callers."""
    refs = isp.refs(0xB8C, 0xB8F)
    assert {f for i in refs for f in isp.owners(i.seg, i.off)} == {"seg3:0x650", "seg3:0x668"}
    assert isp.at("seg3:0x66d").text == "mov ax, 0x43fd"
    assert "call far seg3:0xfe" in [i.text for i in isp.func_insns("seg3:0x668")]
    seeders = [isp.owners(*A(s)) for s in isp.call_sites("seg3:0x650")]
    assert seeders == [{"seg10:0x492"}, {"seg10:0x4dba"}, {"seg10:0x5b24"}]
    for s in isp.call_sites("seg3:0x650"):
        assert isp.step(isp.at(s), -2).text == "call far USER.15 GetCurrentTime"
    sites = isp.call_sites("seg3:0x668")
    assert len(sites) == 20
    assert Counter(f for s in sites for f in isp.owners(*A(s))) == {
        "seg10:0x492": 10, "seg10:0x5b24": 5, "seg9:0xc42": 2, "seg10:0x4dba": 1,
        "seg10:0x4b1a": 1, "seg10:0x5266": 1}
    assert sorted(s for s in sites if A(s)[0] == 9) == ["seg9:0xcca", "seg9:0xd0e"]


# (module, code segment, srand offset, rand offset, state, DGROUP segment)
RAND_COPIES = [
    ("inspace", 5, 0xC4, 0xDC, 0x144C, 19),
    ("ispace", 3, 0x650, 0x668, 0xB8C, 11),
    ("factory-1998a", 3, 0xC4, 0xDC, 0x72C, 6),
    ("factory-1998b", 3, 0xC4, 0xDC, 0x9D2, 6),
]


@pytest.mark.parametrize("name,seg,srand,rand,state,dg", RAND_COPIES,
                         ids=[c[0] for c in RAND_COPIES])
def test_rand_copies_bytes(name, seg, srand, rand, state, dg):
    """rng.md: the same srand/rand pair in all four modules, state initialised to 1."""
    code = unpacked_seg(name, seg)
    lo, hi = struct.pack("<H", state), struct.pack("<H", state + 2)
    assert code[srand:srand + 24] == (bytes.fromhex("45558bec1e8b4606a3") + lo + b"\xc7\x06"
                                      + hi + bytes.fromhex("00008d66fe1f5d4dcb"))
    head = bytes.fromhex("45558bec1eb8fd43ba03005250ff36") + hi + b"\xff\x36" + lo + b"\x9a"
    tail = (bytes.fromhex("05c39e83d226a3") + lo + b"\x89\x16" + hi
            + bytes.fromhex("8bc280e47f"))
    assert code[rand:rand + len(head)] == head
    assert code[rand + len(head) + 4:rand + len(head) + 4 + len(tail)] == tail
    assert struct.unpack_from("<I", unpacked_seg(name, dg), state) == (1,)


FALSE_MOV_15A = ["seg4:0x73b5", "seg4:0x7409", "seg4:0x7433", "seg10:0xa270", "seg15:0x55b6",
                 "seg16:0x7652"]


def test_no_other_lcg_constants(ins, isp, fac):
    """rng.md Other generators: no Borland or ANSI constants; 0x43fd only in rand().

    The byte scan covers every segment of all four modules, theworks/FACTORY.EXE
    included; ``b8+r`` followed by the word is the only encoding of ``mov r16, imm16``.
    """
    words = {0x4E35: [], 0x41C6: [], 0x4E6D: [], 0x015A: FALSE_MOV_15A}
    rand_mov = {"inspace": "seg5:0xe1", "ispace": "seg3:0x66d", "factory-1998a": "seg3:0xe1",
                "factory-1998b": "seg3:0xe1"}
    for name in ("inspace", "ispace", "factory-1998a", "factory-1998b"):
        found = defaultdict(list)
        for seg, data in unpacked_segs(name).items():
            for v in (0x015A4E35, 0x41C64E6D, 0x000343FD):
                assert struct.pack("<I", v) not in data, f"{name} seg{seg} holds {v:#010x}"
            for w in list(words) + [0x43FD]:
                pat = b"[\xb8-\xbf]" + re.escape(struct.pack("<H", w))
                found[w] += [f"seg{seg}:{m.start():#x}" for m in re.finditer(pat, data)]
        assert found[0x43FD] == [rand_mov[name]], name
        for w, expect in words.items():
            assert found[w] == (expect if name == "inspace" else []), (name, hex(w))
    for m, site in ((ins, "seg5:0xe1"), (isp, "seg3:0x66d"), (fac, "seg3:0xe1")):
        movs = [i.addr for i in m.insns if i.mnem == "mov" and len(i.ops) == 2
                and i.ops[0] in ("ax", "bx", "cx", "dx", "si", "di", "bp", "sp")
                and i.ops[1] in ("0x43fd", "0x4e35", "0x41c6", "0x4e6d", "0x15a")]
        assert movs == [site]


def test_false_mov_0x15a_patterns(ins):
    """rng.md Other generators: the six b8+r 5a 01 byte patterns lie inside other insns."""
    found = []
    for seg, data in unpacked_segs("inspace").items():
        found += [f"seg{seg}:{m.start():#x}" for m in re.finditer(rb"[\xb8-\xbf]\x5a\x01", data)]
    assert found == FALSE_MOV_15A
    for start in ("seg4:0x73b3", "seg4:0x7407", "seg4:0x7431"):
        assert ins.at(start).text == "imul bx, word ptr [bp - 0x42], 0x15a"
    for start in ("seg10:0xa26e", "seg15:0x55b4", "seg16:0x7650"):
        assert ins.at(start).text == "cmp word ptr es:[bx + 0x15a], 0"


# ------------------------------------------------------------------ ispace.md

# ordinal: (pointer slot, ISPACE.DLL entry, retf operand)
ISPACE_ORDINALS = {
    1: (0x3C28, "seg5:0x402", 14), 2: (0x3C30, "seg6:0x0", 30), 3: (0x3C2C, "seg5:0x612", 4),
    4: (0x3C50, "seg5:0x34c8", 14), 5: (0x3C4C, "seg6:0xb0", 0), 6: (0x3C40, "seg5:0x650", 6),
    7: (0x3C54, "seg9:0x0", 6), 8: (0x3C58, "seg9:0x76c", 12), 9: (0x3C5C, "seg9:0xc42", 24),
    10: (0x3C64, "seg9:0x145e", 14), 12: (0x3C34, "seg6:0xc4", 24),
    14: (0x3C7C, "seg5:0x50cc", 10), 15: (0x3C44, "seg5:0x11a4", 18),
    17: (0x3C38, "seg6:0x5538", 28), 18: (0x3C3C, "seg6:0x5e76", 12),
    24: (0x3C68, "seg10:0x48d2", 10), 29: (0x3C6C, "seg2:0x3c", 48),
    32: (0x3C78, "seg8:0x0", 34), 33: (0x3C74, "seg8:0x37a", 22), 34: (0x3C70, "seg8:0x5ac", 12),
    35: (0x3C80, "seg8:0x768", 12), 36: (0x3C48, "seg10:0x2e1a", 24),
    41: (0x3C84, "seg9:0x3e8a", 6),
}

# ordinal: [(indirect call, wrapper, wrapper call sites: count or exact list)]
ISPACE_CALLS = {
    1: [("seg10:0x6c57", "seg10:0x6c02", 30), ("seg10:0x6cfa", "seg10:0x6c96", 0)],
    2: [("seg10:0x6f76", "seg10:0x6ee6", 6), ("seg10:0x7026", "seg10:0x6fec", 4),
        ("seg10:0x39dc", "seg10:0x39a8", 4)],
    3: [("seg10:0x6d46", "seg10:0x6d38", 18)],
    4: [("seg10:0x6d9a", "seg10:0x6d7a", 3), ("seg10:0x6dc4", "seg10:0x6da6", 5),
        ("seg10:0x6df1", "seg10:0x6dd4", 6), ("seg10:0x6e18", "seg10:0x6e00", 2),
        ("seg10:0x6e40", "seg10:0x6e26", 2), ("seg10:0x6e6a", "seg10:0x6e4c", 8),
        ("seg10:0x6e90", "seg10:0x6e76", 16), ("seg10:0x6ebb", "seg10:0x6ea0", 2)],
    5: [("seg10:0x86d7", "seg10:0x86ca", 5)],
    6: [("seg10:0x6edb", "seg10:0x6ec6", 20)],
    7: [("seg10:0x6b2f", "seg10:0x6656", 1), ("seg10:0x6b3e", "seg10:0x6656", 1),
        ("seg10:0x6b8e", "seg10:0x6b70", 3), ("seg10:0x6b99", "seg10:0x6b70", 3),
        ("seg10:0x7400", "seg10:0x73e2", 2), ("seg10:0x7441", "seg10:0x740c", 37),
        ("seg10:0x753e", "seg10:0x7480", 36), ("seg10:0x7dac", "seg10:0x7d8e", 1),
        ("seg10:0x7dc9", "seg10:0x7db8", 2), ("seg10:0x869d", "seg10:0x8686", 1),
        ("seg10:0x86bf", "seg10:0x86a8", 1)],
    8: [("seg10:0x7d3f", "seg10:0x7b48", ["seg7:0xcaf"]), ("seg10:0x7d82", "seg10:0x7d58", 6)],
    9: [("seg10:0x785e", "seg10:0x754a", 114), ("seg10:0x7aef", "seg10:0x7944", 4),
        ("seg10:0x7b11", "seg10:0x7944", 4)],
    10: [("seg10:0x7f4e", "seg10:0x7f18", ["seg2:0x14ca"]), ("seg10:0x8314", "seg10:0x82a2", 10),
         ("seg10:0x8332", "seg10:0x82a2", 10), ("seg10:0x83e3", "seg10:0x82a2", 10),
         ("seg10:0x8606", "seg10:0x8568", 3)],
    12: [("seg10:0x73d6", "seg10:0x73a6", ["seg15:0x2165", "seg15:0x2b8f", "seg15:0x757c"])],
    14: [("seg10:0x9956", "seg10:0x993a", 8)],
    15: [("seg10:0x739a", "seg10:0x7370", ["seg8:0x30d4", "seg8:0x347d"])],
    17: [("seg10:0x7365", "seg10:0x7328", ["seg11:0x4c32"])],
    18: [("seg10:0x7051", "seg10:0x7032", ["seg10:0x5c80", "seg15:0x21ca", "seg15:0x2be1"])],
    24: [("seg10:0x88f5", "seg10:0x885e", 5), ("seg10:0x8983", "seg10:0x885e", 5),
         ("seg10:0x9482", "seg10:0x946a", 2)],
    29: [("seg10:0x7e0e", "seg10:0x7dd4",
          ["seg8:0x39f6", "seg8:0x57db", "seg8:0x5a10", "seg8:0x7422", "seg18:0x2ddb"])],
    32: [("seg10:0x955d", "seg10:0x9526", 6)],
    33: [("seg10:0x951b", "seg10:0x94c4", 39)],
    34: [("seg10:0x94b8", "seg10:0x949c", 11)],
    35: [("seg10:0x6d6e", "seg10:0x6d52", 6)],
    36: [("seg10:0x6bf6", "seg10:0x6bca", ["seg10:0x99f3"])],
    41: [("seg10:0x6b65", "seg10:0x6b54", ["seg11:0x72c9"])],
}

# segments of the wrapper call sites where the table names them
ISPACE_WRAPPER_SEGS = {"seg10:0x6c02": {7, 10, 11, 13, 15, 16}, "seg10:0x6fec": {18},
                       "seg10:0x993a": {15}, "seg10:0x9526": {15}}

# the command each indirect call passes (the last push before it)
ISPACE_CMDS = {
    "seg10:0x6d9a": 6, "seg10:0x6dc4": 4, "seg10:0x6df1": 5, "seg10:0x6e18": 0,
    "seg10:0x6e40": 2, "seg10:0x6e6a": 3, "seg10:0x6e90": 1, "seg10:0x6ebb": 7,
    "seg10:0x6b2f": 5, "seg10:0x6b3e": 7, "seg10:0x6b8e": 6, "seg10:0x6b99": 8,
    "seg10:0x7400": 0, "seg10:0x7441": 9, "seg10:0x753e": 11, "seg10:0x7dac": 1,
    "seg10:0x7dc9": 2, "seg10:0x869d": 3, "seg10:0x86bf": 4, "seg10:0x7f4e": 0,
    "seg10:0x9482": 10,
}


def _lookups(m: Module, func: str) -> dict[int, tuple[int, str]]:
    """GetProcAddress by ordinal in ``func``: {ordinal: (slot, call site)}."""
    out = {}
    for i in m.func_insns(func):
        if i.text == "call far KERNEL.50 GetProcAddress":
            push, store = m.step(i, -1), m.step(i, 1)
            assert m.step(i, -2).text == "push 0" and push.mnem == "push"
            assert store.ops[1:] == ("ax",) and store.abs_ds
            out[int(push.ops[0], 0)] = (store.mem.disp, i.addr)
    return out


def test_ispace_lookups(ins):
    """ispace.md: 23 GetProcAddress-by-ordinal lookups in seg10:0x6656, ordinal to slot."""
    found = _lookups(ins, "seg10:0x6656")
    assert {o: s for o, (s, _a) in found.items()} == {o: v[0] for o, v in ISPACE_ORDINALS.items()}
    calls = sorted((A(a) for _s, a in found.values()))
    assert calls[0] == A("seg10:0x67e7") and calls[-1] == A("seg10:0x6ad3")
    slots = sorted(v[0] for v in ISPACE_ORDINALS.values())
    assert sorted(slots + [0x3C60]) == list(range(0x3C28, 0x3C88, 4))   # DGROUP:0x3c60 unused
    relocs = json.loads(require(WORK / "unpacked" / "inspace" / "relocs.json").read_text())
    assert "ISPACE" not in relocs["modules"].values()                    # no import records


def _retf_operands(m: Module, func: str) -> set[int]:
    return {int(i.ops[0], 0) if i.ops else 0 for i in m.func_insns(func) if i.mnem == "retf"}


def test_ispace_export_entries_and_retf(isp):
    """ispace.md The 23 ordinals: entry table and retf operands."""
    exports = {int(e.split()[0]): f["addr"] for f in isp.fj["functions"] for e in f["export"]}
    for o, (_slot, entry, retf) in ISPACE_ORDINALS.items():
        assert exports[o] == entry, o
        assert _retf_operands(isp, entry) == {retf}, o


def _pushed_bytes(m: Module, call: Insn) -> int:
    """Argument bytes pushed before ``call``, back to the previous call or the prologue.

    Pushes of si and di right after the prologue save registers and are no
    arguments. A push with an operand-size prefix pushes four bytes.
    """
    n = m.pos[(call.seg, call.off)]
    total = 0
    while True:
        n -= 1
        i = m.insns[n]
        assert i.seg == call.seg
        if i.mnem == "push":
            pre = i.raw[:next(k for k, b in enumerate(i.raw) if b not in _PREFIXES)]
            total += 4 if 0x66 in pre else 2
        elif i.mnem in _FLOW:
            return total
        elif i.text in ("mov bp, sp", "mov ds, ax") or (i.ops and i.ops[0] == "sp"):
            break                                        # prologue or stack adjustment
    k = n + 1
    while m.insns[k].text in ("push si", "push di") and m.insns[k].off < call.off:
        total -= 2
        k += 1
    return total


def test_ispace_call_sites_push_retf_bytes(ins):
    """ispace.md Calling convention: pushed bytes equal the export's retf operand."""
    for o, calls in ISPACE_CALLS.items():
        for site, _wrapper, _callers in calls:
            assert _pushed_bytes(ins, ins.at(site)) == ISPACE_ORDINALS[o][2], (o, site)


def test_ispace_indirect_calls_and_wrappers(ins):
    """ispace.md Call sites in INSPACE: calls, wrappers and wrapper call sites."""
    slot_ord = {v[0]: o for o, v in ISPACE_ORDINALS.items()}
    found = defaultdict(set)
    for i in ins.insns:
        if i.mnem == "lcall" and i.abs_ds and i.mem.disp in slot_ord:
            found[slot_ord[i.mem.disp]].add(i.addr)
    assert dict(found) == {o: {c for c, _w, _n in v} for o, v in ISPACE_CALLS.items()}
    for o, calls in ISPACE_CALLS.items():
        for site, wrapper, callers in calls:
            assert ins.owners(*A(site)) == {wrapper}, site
            sites = ins.call_sites(wrapper)
            if isinstance(callers, list):
                assert sites == callers, wrapper
            else:
                assert len(sites) == callers, wrapper
            if wrapper in ISPACE_WRAPPER_SEGS:
                assert {A(s)[0] for s in sites} == ISPACE_WRAPPER_SEGS[wrapper]
    ord35 = ins.call_sites("seg10:0x6d52")
    assert ord35[0] == "seg9:0x281c" and {A(s)[0] for s in ord35[1:]} == {16}
    # no other code reads the slots: every reference is an indirect call or the loader
    for slot in slot_ord:
        for i in ins.refs(slot, slot + 3):
            assert (i.mnem == "lcall" and i.seg == 10) or ins.in_func(i, "seg10:0x6656"), i.addr


def test_ispace_wrapper_commands(ins):
    """ispace.md Call sites: the commands of the ordinal 4, 7, 10 and 24 wrappers."""
    for site, cmd in ISPACE_CMDS.items():
        push = ins.step(ins.at(site), -1)
        assert push.mnem == "push" and int(push.ops[0], 0) == cmd, site
    ord4 = [w for _c, w, _n in ISPACE_CALLS[4]]
    assert [w for w in ord4 if ["seg10:0x6e4c", "near"] in ins.funcs[w]["calls"]] == [
        "seg10:0x6da6", "seg10:0x6dd4", "seg10:0x6e76"]      # then command 3


OTHER_EXPORTS = {
    11: "seg9:0x15cc", 22: "seg10:0xf26", 23: "seg10:0x3346", 25: "seg10:0xec2",
    26: "seg10:0xdc0", 27: "seg10:0x103c", 28: "seg10:0xf9c", 30: "seg10:0x664",
    37: "seg9:0x2bf8", 38: "seg9:0x2f30", 39: "seg10:0x124a", 40: "seg10:0x5f96",
    42: "seg9:0x40da", 21: "seg10:0x309c", 31: "seg9:0x2ac4", 13: "seg5:0x505a",
    16: "seg5:0x700a", 19: "seg6:0x79b8", 20: "seg6:0x8266", 43: "seg2:0x26", 44: "seg3:0xbc2",
}


def test_ispace_all_exports(isp):
    """ispace.md Exports INSPACE does not request; every export ends in retf."""
    exports = {int(e.split()[0]): f for f in isp.fj["functions"] for e in f["export"]}
    assert sorted(exports) == list(range(1, 45))
    assert {o: exports[o]["addr"] for o in OTHER_EXPORTS} == OTHER_EXPORTS
    assert exports[43]["export"] == ["43 WEP"]
    assert all(f["returns"] == ["retf"] for f in exports.values())


def test_ispace_dispatch_tables(isp):
    """ispace.md Ordinal notes: the command tables of ordinals 4, 7, 10 and 24."""
    def table(at: str, owner: str) -> list[str]:
        sw = isp.switches[at]
        assert isp.owners(*A(at)) == {owner}
        return sw["targets"]

    ord4 = table("seg5:0x3573", "seg5:0x34c8")
    assert isp.switches["seg5:0x3573"]["table"] == "seg5:0x3578" and len(ord4) == 9
    routines = []
    for t in ord4:
        i = isp.at(t)
        while not (i.mnem == "call" and i.ops[0].startswith("seg")):
            i = isp.step(i, 1)
        routines.append(i.ops[0])
    assert routines == ["seg5:0x3596", "seg5:0x44ac", "seg5:0x3aae", "seg5:0x4378",
                        "seg5:0x484a", "seg5:0x474c", "seg5:0x3dee", "seg5:0x446e",
                        "seg5:0x3bae"]
    assert isp.switches["seg9:0x91"]["table"] == "seg9:0x96"
    assert len(table("seg9:0x91", "seg9:0x0")) == 12                    # commands 0-11
    assert len(table("seg10:0x4ae0", "seg10:0x48d2")) == 22             # commands 0-21
    ord10 = table("seg9:0x1517", "seg9:0x145e")
    assert len(ord10) == 16                                             # commands 0-15
    shared = {k for k, t in enumerate(ord10) if t == ord10[1]}
    assert shared == {1, 2, 3, 4, 5, 6, 7, 8, 11, 12, 14}
    out_of_range = isp.step(isp.at(isp.switches["seg9:0x1517"]["bound_at"]), 1)
    assert out_of_range.mnem == "ja"
    assert ord10[10] == ord10[13] == out_of_range.ops[0]

    def first_far_call(t: str) -> str:
        i = isp.at(t)
        while i.mnem != "call" or not i.text.startswith("call far "):
            i = isp.step(i, 1)
        return i.text

    assert first_far_call(ord10[1]) == "call far seg10:0x42c6"
    assert first_far_call(ord10[9]) == "call far seg10:0x5b24"


def test_factory_requests_21_ordinals(fac):
    """ispace.md: shipdisk FACTORY.EXE requests ordinals 1-10, 12-20, 29 and 36."""
    by_ordinal, ords = [], []
    for s in fac.xj["imports"]["KERNEL.50 GetProcAddress"]:
        call = fac.at(s)
        if fac.step(call, -2).text == "push 0":          # selector 0: the offset is an ordinal
            by_ordinal.append(s)
            ords.append(int(fac.step(call, -1).ops[0], 0))
        else:                                            # the others look up names
            assert fac.step(call, -2).text == "push ds", s
    assert len(ords) == 21
    assert sorted(ords) == list(range(1, 11)) + list(range(12, 21)) + [29, 36]
    assert {A(s)[0] for s in by_ordinal} == {4}
    assert (by_ordinal[0], by_ordinal[-1]) == ("seg4:0x30eb", "seg4:0x3393")
    # formats.md MASTER.BOD: seg4:0x323f sets DGROUP:0xba0 to ordinal 19
    assert fac.step(fac.at("seg4:0x323f"), -1).text == "push 0x13"
    assert fac.step(fac.at("seg4:0x323f"), 1).text == "mov word ptr [0xba0], ax"


def test_file_io_import_counts(ins, isp):
    """ispace.md What ISPACE.DLL owns: OpenFile, _lread and _lwrite calls."""
    def n(m, imp):
        return len(m.xj["imports"].get(imp, []))

    assert (n(ins, "KERNEL.74 OpenFile"), n(ins, "KERNEL.82 _lread"),
            n(ins, "KERNEL.86 _lwrite")) == (20, 1, 2)
    assert (n(isp, "KERNEL.74 OpenFile"), n(isp, "KERNEL.82 _lread"),
            n(isp, "KERNEL.86 _lwrite")) == (8, 9, 1)
    assert isp.owners(*A(isp.xj["imports"]["KERNEL.86 _lwrite"][0])) == {"seg10:0x4696"}


def test_set_timer_sites(ins, isp):
    """timing.md Window timers: SetTimer at seg6:0x1397 and seg6:0xa0f; ISPACE.DLL seg9:0x46d4."""
    assert sorted(ins.xj["imports"]["USER.10 SetTimer"], key=A) == ["seg6:0xa0f", "seg6:0x1397"]
    assert ins.owners(*A("seg6:0xa0f")) == {"seg6:0x9f8"}
    assert isp.xj["imports"]["USER.10 SetTimer"] == ["seg9:0x46d4"]
    assert isp.owners(*A("seg9:0x46d4")) == {"seg9:0x4298"}


def _imports(name: str, ne_file: Path) -> set[tuple[str, int]]:
    """(module, ordinal) of every import-by-ordinal record of a module."""
    from isre.ne import NEModule

    relocs = json.loads(require(WORK / "unpacked" / name / "relocs.json").read_text())
    out = {(r["module"], r["ordinal"]) for r in relocs["relocs"] if r["kind"] == "import_ordinal"}
    for recs in NEModule.load(require(ne_file)).std_relocs.values():    # the loader stub
        out |= {(r.target["module"], r.target["ordinal"]) for r in recs if "ordinal" in r.target}
    return out


def test_palette_dib_and_mmsystem_imports():
    """ispace.md What ISPACE.DLL owns, Imports."""
    mods = {"inspace": ORIGINAL / "shipdisk" / "INSPACE.ex_",
            "ispace": ORIGINAL / "shipdisk" / "ISPACE.dll",
            "factory-1998a": ORIGINAL / "shipdisk" / "FACTORY.ex_",
            "factory-1998b": ORIGINAL / "theworks" / "FACTORY.EXE"}
    imports = {n: _imports(n, p) for n, p in mods.items()}
    for o in (360, 367, 375, 441, 442):
        assert {n for n, imp in imports.items() if ("GDI", o) in imp} == {"ispace"}, o
    for o in (439, 443):
        assert not [n for n, imp in imports.items() if ("GDI", o) in imp], o
    assert sorted(o for m, o in imports["ispace"] if m == "MMSYSTEM") == [
        2, 202, 211, 212, 401, 402, 415, 416, 607, 701]
    names = {k.split(" ")[1] for k in module("ispace").xj["imports"] if k.startswith("MMSYSTEM.")}
    assert names == {"sndPlaySound", "midiOutGetDevCaps", "midiOutGetVolume",
                     "midiOutSetVolume", "waveOutGetNumDevs", "waveOutGetDevCaps",
                     "waveOutGetVolume", "waveOutSetVolume", "timeGetTime", "mciSendCommand"}


WAVEMIX = [("WaveMixGetInfo", 0x17CC), ("WaveMixConfigureInit", 0x17D0),
           ("WaveMixOpenChannel", 0x19D0), ("WaveMixActivate", 0x17D4),
           ("WaveMixFlushChannel", 0x19E0), ("WaveMixOpenWave", 0x19CC),
           ("WaveMixPlay", 0x19D4), ("WaveMixFreeWave", 0x1730), ("WaveMixPump", 0x1738),
           ("WaveMixCloseChannel", 0x1718), ("WaveMixCloseSession", 0x1734)]


def test_wavemix_name_table(isp):
    """ispace.md WaveMix: eleven names at ISPACE.DLL DGROUP:0x772 and their pointers."""
    dg = unpacked_seg("ispace", 11)
    ptrs = struct.unpack_from("<11H", dg, 0x772)
    assert [_cstr(dg, p).decode() for p in ptrs] == [n for n, _p in WAVEMIX]
    pairs = []
    for i in isp.func_insns("seg9:0x362e"):
        if i.text == "call far KERNEL.50 GetProcAddress":
            name_ptr, store = isp.step(i, -1), isp.step(i, 1)
            pairs.append((name_ptr.mem.disp, store.mem.disp))
    assert pairs == [(0x772 + 2 * k, p) for k, (_n, p) in enumerate(WAVEMIX)]


# ------------------------------------------------------------------ formats.md


@pytest.fixture(scope="module")
def master():
    from isre import szdd

    d = require(ORIGINAL / "shipdisk")
    return (szdd.expand(require(d / "MASTER.BO1").read_bytes()),
            szdd.expand(require(d / "MASTER.SH1").read_bytes()))


def test_master_bod(master):
    """formats.md MASTER.BOD: 52 records of 44 bytes and 16 zero bytes."""
    bod = master[0]
    assert len(bod) == 52 * 44 + 16 == 72 * 32
    assert bod[52 * 44:] == bytes(16)
    recs = [bod[k * 44:(k + 1) * 44] for k in range(52)]
    assert {struct.unpack_from("<H", r, 0x15)[0] for r in recs} == {1}
    last = recs[-1]
    assert last[:21] == bytes(21) and [k for k, r in enumerate(recs) if r[0] == 0] == [51]
    assert struct.unpack_from("<H", last, 0x21) == (0xFFFF,)
    assert _cstr(last, 0x23) == b"IDRAGON"
    require(ORIGINAL / "theworks" / "IDRAGON.DIB")


def test_master_shp(master):
    """formats.md MASTER.SHP: 118 records of 65 bytes and 10 zero bytes."""
    shp = master[1]
    assert len(shp) == 118 * 65 + 10 == 240 * 32
    assert shp[118 * 65:] == bytes(10)
    thrusters = [shp[k * 65 + 1] for k in range(118)]
    assert (min(thrusters), max(thrusters)) == (23, 60)


def test_scene_record_ids(ins):
    """formats.md IGRAPHIC.DLL #6600: the ids INSPACE passes through seg10:0x9526."""
    pushes = {"seg15:0xd9b": 100, "seg15:0x3405": 100, "seg15:0x31fd": 105,
              "seg15:0x3284": 110, "seg15:0x903": 115, "seg15:0x332b": 120}
    for site, n in pushes.items():
        assert ins.at(site).text == f"push {n:#x}", site
    assert ins.call_sites("seg10:0x9526") == ["seg15:0x911", "seg15:0xda9", "seg15:0x320b",
                                              "seg15:0x3292", "seg15:0x3339", "seg15:0x3413"]
