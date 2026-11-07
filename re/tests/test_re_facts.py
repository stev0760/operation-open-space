"""Mechanical facts of docs/re/{structs,globals,timing,rng,ispace,formats,shell,palette-tables,install,sound,
nav,world}.md
and of re/artifacts/sounds.tsv.

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
    """ispace.md: the game's FACTORY.EXE requests ordinals 1-10, 12-20, 29 and 36."""
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
    mods = {"inspace": ORIGINAL / "game" / "INSPACE.ex_",
            "ispace": ORIGINAL / "game" / "ISPACE.dll",
            "factory-1998a": ORIGINAL / "game" / "FACTORY.ex_",
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

    d = require(ORIGINAL / "game")
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


# ------------------------------------------------------------------ shell.md (start-up, states, intro, geometry)


def _imm_push_before(m: Module, site: str) -> int:
    """The immediate of the last ``push <imm>`` before the call at ``site`` (near calls
    within a segment are preceded by ``push cs``)."""
    i = m.at(site)
    while True:
        i = m.step(i, -1)
        if i.mnem == "push" and _IMM_RE.fullmatch(i.ops[0]):
            return s16(int(i.ops[0], 0) & 0xFFFF)
        assert i.mnem in ("push", "nop"), (site, i.text)


SHELL_WRITERS = {
    # DGROUP word: {writer: stored constant or None when computed}
    0x90F8: {"seg2:0x874": 1, "seg6:0x1c16": 0, "seg6:0x1c2d": 1, "seg6:0x1c53": 1,
             "seg11:0x792c": 0},
    0x8E66: {"seg6:0x13b9": None},
    0x8F66: {"seg2:0x4be": 1},
    0x8E5E: {"seg2:0x85d": None},
    0x8F6A: {"seg2:0xb6e": 1, "seg2:0x11ea": 1, "seg2:0x1211": 0, "seg6:0x6be": 0},
    0x0020: {"seg2:0xfd1": 1, "seg2:0x11ac": 0, "seg6:0x1a": None},
    0x0022: {"seg7:0x19": 1, "seg7:0xff": 0, "seg7:0x197": 0, "seg7:0x1bf": 0, "seg7:0x211": 0},
    0x93C6: {"seg2:0x10b2": 6, "seg2:0x10fa": 2, "seg2:0x1149": 5, "seg2:0x1559": 4,
             "seg2:0x1580": 5, "seg2:0x1588": 6, "seg2:0x1590": 1, "seg2:0x1598": 0,
             "seg11:0x236e": 1, "seg11:0x2376": 0},
    0x93A6: {"seg9:0x5dc7": 0, "seg18:0x111": 1},
    0x95B8: {"seg11:0x79b7": 5, "seg11:0x7ab1": 6, "seg11:0x7abe": 7, "seg15:0x12c3": 7,
             "seg15:0x5809": 0, "seg15:0x5a42": None, "seg15:0x5a70": 7},
    0x0D1E: {"seg6:0x1f82": None, "seg6:0x2ff1": 0},
    0x0A70: {"seg2:0xbfc": 85},
    0x9150: {"seg6:0xb07": None, "seg9:0x2408": None, "seg10:0x9b8d": None},
    0x9156: {"seg6:0xafd": None, "seg6:0x95ca": None, "seg9:0x23bd": None, "seg9:0x23fe": None,
             "seg10:0x9ade": None, "seg10:0x9b85": None, "seg15:0x21c5": 1,
             "seg15:0x21d8": None, "seg15:0x2bdc": 1, "seg15:0x2bf9": None, "seg18:0x131": 0,
             "seg18:0x72d": None, "seg18:0x800": None, "seg18:0x903": 1, "seg18:0x910": None,
             "seg18:0x2753": 1, "seg18:0x2ac3": None},
    0x1212: {"seg15:0x6009": 1},
}


@pytest.mark.parametrize("lo", sorted(SHELL_WRITERS), ids=lambda v: f"DGROUP:{v:#x}")
def test_shell_word_writers(ins, lo):
    """shell.md, globals.md: the complete writer lists of start-up, screen and view words."""
    assert _write_values(ins, lo, lo + 1) == SHELL_WRITERS[lo]


SHELL_DWORD_WRITERS = {
    # DGROUP dword deadline: writers of its low word (16-bit high-word partners skipped)
    0x1E70: ["seg6:0x36b2", "seg7:0x386"],
    0x1E74: ["seg6:0x36a5", "seg9:0x6bdf", "seg9:0x6ce1", "seg9:0x6fe1"],
    0x1E78: ["seg9:0xd97"],
    0x1E7C: ["seg6:0x5db6", "seg6:0x5f6b"],
    0x1E80: ["seg7:0xf0", "seg7:0x6a0", "seg8:0x22f", "seg9:0xd2f", "seg11:0xb04c",
             "seg12:0x1e3b", "seg12:0x246b", "seg12:0x2adf", "seg16:0x45e", "seg16:0xae6",
             "seg16:0xdb2"],
    0x1E84: ["seg7:0xec", "seg7:0x1d8", "seg7:0x6f7", "seg8:0x244", "seg9:0xd2b",
             "seg11:0xb048", "seg12:0x2ac1", "seg12:0x2acf", "seg16:0x45a", "seg16:0xacd",
             "seg16:0xad3", "seg16:0xdae", "seg16:0x7367", "seg18:0x57c"],
    0x1E88: ["seg7:0x5e1", "seg9:0xda4", "seg9:0x5d58", "seg16:0x568"],
    0x1D40: ["seg2:0x87f", "seg6:0x2ede"],
}


@pytest.mark.parametrize("lo", sorted(SHELL_DWORD_WRITERS), ids=lambda v: f"DGROUP:{v:#x}")
def test_shell_deadline_writers(ins, lo):
    """shell.md Screen states: the writers of the screen deadlines and of DGROUP:0x1d40."""
    assert [i.addr for i in ins.writers(lo, lo + 3) if i.mem.disp == lo] == \
        SHELL_DWORD_WRITERS[lo]


def test_shell_reference_lists(ins, dgroup):
    """shell.md: words with no writer or a single reader, and their initial values."""
    assert _sites(ins.refs(0x90FC, 0x90FD)) == ["seg6:0x1300", "seg6:0x137f", "seg6:0x138a",
                                                "seg6:0x13a0"]
    assert ins.writers(0x90FC, 0x90FD) == []
    assert _sites(ins.refs(0x8F6E, 0x8F6F)) == ["seg2:0x1214"]
    assert [i.addr for i in ins.refs(0x1D40, 0x1D43) if not is_write(i)] == []
    assert [i.addr for i in ins.refs(0x8E5E, 0x8E5F) if not is_write(i)] == [
        "seg2:0x6ba", "seg2:0x715", "seg6:0x31f0", "seg9:0x234e", "seg11:0x7925",
        "seg14:0x234e", "seg14:0x246c", "seg14:0x24bb", "seg14:0x250c", "seg15:0x57c4"]
    assert _sites(ins.refs(0xA70, 0xA71)) == ["seg2:0xb3a", "seg2:0xbfc", "seg2:0x1b25"]
    assert _sites(ins.refs(0x5B0, 0x5B3)) == ["seg7:0x739", "seg7:0x745", "seg7:0x74c"]
    assert not [i.addr for i in ins.insns if i.mem is not None and i.mem.ds
                and (i.mem.regs or i.mnem == "lea") and 0x590 <= i.mem.disp <= 0x5C0]
    assert [i.addr for i in ins.refs(0x95BA, 0x95BB) if not is_write(i)] == [
        "seg6:0x3155", "seg6:0x31b1", "seg6:0x3c4c", "seg6:0x3c8f", "seg6:0x3c9a",
        "seg15:0x5acf"]
    assert struct.unpack_from("<H", dgroup, 0x22)[0] == 1
    assert struct.unpack_from("<H", dgroup, 0xA70)[0] == 85
    assert dgroup[0x5B0:0x5B4] == bytes(4)


SHELL_CALLERS = {
    "seg6:0x1c0c": ["seg2:0x9f", "seg6:0x12e7"],
    "seg2:0x190a": ["seg2:0x156b", "seg6:0x2379"],
    "seg10:0x87de": ["seg2:0xe3b"],
    "seg9:0x6cc4": ["seg9:0x6cb8", "seg16:0x6abd"],
    "seg9:0x5cca": ["seg7:0x38f", "seg9:0xd5f"],
    "seg9:0xd6c": ["seg7:0xf4", "seg7:0x437", "seg7:0x68f", "seg7:0x755", "seg11:0x6c67",
                   "seg11:0x7b55"],
    "seg6:0x5d58": ["seg7:0x1e1"],
    "seg15:0x10be": ["seg15:0x59b3", "seg16:0x1b7f", "seg16:0x6a4f"],
    "seg6:0x1d9a": ["seg11:0x7ad1", "seg15:0x12d6", "seg15:0x5b96"],
    "seg6:0x92b8": ["seg7:0x64d", "seg11:0x7975", "seg15:0x5b14", "seg15:0x5b61"],
    "seg10:0x885e": ["seg6:0x13e6", "seg10:0x833d", "seg11:0x79f9", "seg15:0x5803",
                     "seg15:0x6f9d"],
    "seg10:0x8568": ["seg2:0x55e", "seg2:0xe5d", "seg2:0x1500"],
    "seg10:0x7f18": ["seg2:0x14ca"],
    "seg6:0x1c72": ["seg7:0x3ac", "seg11:0x7ad8", "seg11:0x7ba1", "seg15:0x12dd",
                    "seg15:0x5b9d"],
    "seg6:0x301a": ["seg6:0x1eb0", "seg6:0x2384"],
    "seg11:0x2f0": ["seg6:0x5da4", "seg9:0xd81", "seg9:0x6d5c", "seg10:0x259", "seg11:0x2e5",
                    "seg16:0xd96", "seg16:0x697a", "seg16:0x7223", "seg18:0x3aa"],
}


@pytest.mark.parametrize("func", sorted(SHELL_CALLERS, key=A))
def test_shell_call_sites(ins, func):
    """shell.md: exhaustive caller lists of the start-up, screen and intro routines."""
    assert ins.call_sites(func) == SHELL_CALLERS[func]


def test_dialog_wrapper_commands(ins):
    """shell.md Game-start dialogs: the ordinal 10 command each seg10:0x82a2 caller passes."""
    assert {s: _imm_push_before(ins, s) for s in ins.call_sites("seg10:0x82a2")} == {
        "seg2:0x548": 7, "seg2:0xe0f": 5, "seg2:0xe35": 12, "seg10:0x80c7": 8,
        "seg10:0x882d": 14, "seg11:0x796b": 4, "seg11:0x7a43": 11, "seg11:0x7a7c": 1,
        "seg16:0x1365": 6, "seg16:0x4811": -100}


def test_shell_import_sites(ins):
    """shell.md Start-up, Exit, Help: RegisterClass, PostQuitMessage, WinHelp, DestroyWindow."""
    imp = {k: sorted(v, key=A) for k, v in ins.xj["imports"].items()}
    assert imp["USER.57 RegisterClass"] == [
        "seg6:0x14c7", "seg6:0x1520", "seg6:0x1579", "seg6:0x15d2", "seg6:0x1635",
        "seg6:0x1698", "seg6:0x16ef"]
    assert imp["USER.6 PostQuitMessage"] == ["seg2:0x83a", "seg6:0x19b0"]
    assert imp["USER.171 WinHelp"] == ["seg2:0x1987"]
    destroy = imp["USER.53 DestroyWindow"]
    assert len(destroy) == 27
    main = [s for s in destroy if ins.step(ins.at(s), -1).text == "push word ptr [0x16]"]
    assert main == ["seg2:0x742", "seg2:0xe44", "seg10:0x864c"]
    # no instruction loads ENG_LANG strings 2114 or 2115 by immediate
    assert not [i.addr for i in ins.insns
                if any(_IMM_RE.fullmatch(o) and int(o, 0) in (0x842, 0x843) for o in i.ops)]
    assert ins.at("seg6:0x146f").text == "mov word ptr [bp + 6], 0x300b"   # main class style
    assert ins.at("seg2:0xd88").text == "and al, 0xf0"                     # WM_SYSCOMMAND mask


def test_shell_jump_tables(ins):
    """shell.md: the state-0 phase and step tables, the repaint table and the 8 and 9 frames."""
    expect = {"seg15:0x59c2": 8, "seg15:0x5cfb": 6, "seg15:0x1307": 10, "seg11:0x16be": 9,
              "seg16:0x6adb": 11, "seg16:0x7375": 10}
    assert {a: ins.switches[a]["count"] for a in expect} == expect
    assert ins.owners(*A("seg11:0x16be")) == {"seg11:0xafe"}
    assert ins.at("seg11:0x16b6").text == "cmp ax, 8"


def test_palette_imports_and_ordinal_14_callers(ins, isp):
    """shell.md Palette: AnimatePalette once in ISPACE.DLL, no palette editors in either
    module, and 17 calls of the ordinal 32 colour helper ISPACE.DLL seg8:0x27f0."""
    assert isp.xj["imports"]["GDI.367 AnimatePalette"] == ["seg5:0x441a"]
    assert sorted(isp.xj["imports"]["GDI.360 CreatePalette"], key=A) == ["seg5:0x2c68",
                                                                          "seg5:0x38e0"]
    editors = ("SetPaletteEntries", "GetPaletteEntries", "GetNearestPaletteIndex",
               "ResizePalette", "SetSystemPaletteUse")
    for m in (ins, isp):
        assert not [k for k in m.xj["imports"] if k.split()[-1] in editors]
    assert not [k for k in ins.xj["imports"] if k.endswith(("AnimatePalette", "CreatePalette"))]
    assert ins.xj["imports"]["GDI.366 UpdateColors"] == ["seg2:0x5a2"]
    assert len(isp.call_sites("seg8:0x27f0")) == 17


# ------------------------------------------------------------------ intro graphics, bitmap font, colour tables, credits


@functools.lru_cache(maxsize=None)
def _ne(name: str):
    from isre.ne import NEModule

    return NEModule.load(require(ORIGINAL / "game" / name))


def _resource(name: str, rtype, rid) -> bytes:
    m = _ne(name)
    hits = [r for r in m.resources if r.type_id == rtype and r.id == rid]
    assert len(hits) == 1, (name, rtype, rid)
    return m.data[hits[0].file_off:hits[0].file_off + hits[0].length]


def _dib(data: bytes) -> tuple[int, int, int, list[str]]:
    """(width, height, bpp, colour table as RRGGBB) of a packed DIB, with or without BM header."""
    o = 14 if data[:2] == b"BM" else 0
    _size, w, h, _planes, bpp, comp = struct.unpack_from("<IiiHHI", data, o)
    assert comp == 0
    table = [data[o + 40 + 4 * i:o + 43 + 4 * i][::-1].hex() for i in range(1 << bpp)]
    return w, h, bpp, table


def test_intro_graphic_resources():
    """ispace.md ordinals 1, 3, 33; shell.md logo and credits: sizes and colour tables of the
    intro graphics, SD_LOGO, ALPHABET and #5500 100."""
    grey = ["a0a0a0", "9b9b9b", "919191", "878787", "7d7d7d", "737373", "696969", "5f5f5f",
            "4b4b4b", "3c3c3c", "323232"]
    assert _dib(_resource("ISPACE.dll", 4000, 220)) == (40, 40, 4, grey + ["ffffff"] * 4 + ["000000"])
    w, h, bpp, t = _dib(_resource("ISPACE.dll", 4000, 221))
    assert (w, h, bpp, t[0], t[10], t[11:]) == (360, 40, 4, "ffffff", "5a0000", ["000000"] * 5)
    assert _dib(_resource("ISPACE.dll", 4000, 300)) == (138, 138, 4, TITLE_300_COLOURS + ["000000"])
    assert _resource("ISPACE.dll", 4000, 300)[:2] == b"BM"
    assert _dib(_resource("ISPACE.dll", 2, "SD_LOGO")) == (130, 32, 1, ["ffffff", "000000"])
    assert _dib(_resource("IGRAPHIC.DL1", 2, "ALPHABET")) == (828, 20, 1, ["ffffff", "000000"])
    assert 828 == 46 * 18
    assert _dib(_resource("IGRAPHIC.DL1", 4000, 340))[:3] == (333, 37, 4)
    assert not {r.id for r in _ne("IGRAPHIC.DL1").resources if r.type_id == 4000} & {220, 221, 300}
    raw = _resource("ISPACE.dll", 5500, 100)
    assert len(raw) == 288 and raw[276:] == bytes(12)
    trip = [raw[3 * k:3 * k + 3].hex() for k in range(92)]
    assert "000000" not in trip
    assert trip[:4] == ["00ffff", "00e9ff", "00d3ff", "00bdff"]
    assert trip[-4:] == ["007200", "005e00", "005000", "003f00"]


def test_graphic_loader_and_strip_constants(ins, isp):
    """ispace.md ordinals 1 and 2: ISPACE.DLL's own ids 220, 221 and 300, the BM check, and the
    logo strip's cell size, gap and id -1000."""
    assert [isp.at(a).ops[1] for a in ("seg5:0x1c23", "seg5:0x1c2a", "seg5:0x1c31")] == \
        ["0xdc", "0xdd", "0x12c"]
    assert isp.at("seg5:0x1c38").mem.disp == 0x1A30 and isp.at("seg5:0x1c3d").mem.disp == 0x692
    assert isp.at("seg5:0x1c56").text == "push 0xfa0" and "FindResource" in isp.at("seg5:0x1c59").text
    assert isp.at("seg5:0x1cba").ops[1] == "0x4d42"
    assert isp.at("seg5:0x17f5").text == "mov word ptr [bp - 0x20], 0x28"
    assert isp.at("seg6:0xd").text == "cmp word ptr [bp + 0x20], 0xfc18"
    assert isp.call_sites("seg5:0x17c2") == ["seg6:0x1d"]
    assert ins.at("seg10:0x39bd").text == "push 0xfc18" and ins.at("seg10:0x39ca").text == "push 6"
    assert ins.at("seg10:0x39dc").mem.disp == 0x3C30 and ins.at("seg10:0x39e0").mem.disp == 0x3ADA
    assert isp.at("seg6:0xda").text == "cmp byte ptr [bp + 8], 0xff"
    assert isp.at("seg6:0xe0").text == "cmp byte ptr [bp + 6], 0xff"
    assert isp.call_sites("seg7:0x12d8") == ["seg6:0x10d"]


def test_intro_dgroup_data(dgroup):
    """shell.md Logo graphics and Title: the pattern-brush rows and the title strings."""
    assert dgroup[0x6C6:0x6D6] == bytes([0x82, 0, 0x10, 0, 0x45, 0, 0x10, 0, 0x84, 0, 0x20, 0,
                                         0x02, 0, 0x90, 0])
    assert sum(bin(b).count("1") for b in dgroup[0x6C6:0x6D6]) == 13
    assert dgroup[0x6B6:0x6C6] == bytes([0xAA, 0, 0x55, 0]) * 4
    strings = {0x129D: b" For best results, change Windows to 256 color mode ",
               0x12D2: b"\xa9 1994-2000 Software Dynamics, inc. All rights reserved.",
               0x1285: b"\x99", 0x1287: b"", 0x1288: b" - ", 0x128C: b" ", 0x129C: b"",
               0x1216: b"SD_LOGO", 0x744: b"", 0x7DE: b"", 0x130B: b""}
    assert {o: _cstr(dgroup, o) for o in strings} == strings
    assert (len(strings[0x129D]), len(strings[0x12D2])) == (52, 56)


def test_ispace_font_and_logo_names(isp):
    """ispace.md ordinals 3 and 33: the ALPHABET handle's references and the glyph cell."""
    dg = unpacked_seg("ispace", 11)
    assert (_cstr(dg, 0x192), _cstr(dg, 0x26)) == (b"ALPHABET", b"SD_LOGO")
    assert _sites(isp.refs(0x1EA2, 0x1EA3)) == ["seg5:0x3707", "seg5:0x3717", "seg5:0x3b95",
                                                "seg5:0x3b9e", "seg8:0x3cb"]
    assert _write_values(isp, 0x1EA2, 0x1EA3) == {"seg5:0x3717": None, "seg5:0x3b9e": 0}
    assert isp.at("seg8:0x519").text == "imul ax, word ptr [bp - 0x12], 0x12"
    assert (isp.at("seg8:0x520").text, isp.at("seg8:0x522").text) == ("push 0x12", "push 0x14")
    assert isp.at("seg8:0x5a9").text == "retf 0x16"


def _cdecl_pushes(m: Module, site: str, n: int) -> list[str]:
    """Operands of the ``n`` pushes before the call at ``site``, nearest first."""
    out, i = [], m.at(site)
    while len(out) < n:
        i = m.step(i, -1)
        if i.mnem == "push":
            out.append(i.ops[0])
    return out


def test_bitmap_font_wrapper(ins):
    """ispace.md ordinal 33: the cell sizes of fonts 0-2 and the align of all 39 calls."""
    assert [ins.at(a).ops[1] for a in ("seg10:0x94d6", "seg10:0x94e0", "seg10:0x94ea")] == \
        ["0x140012", "0x12000c", "0xa0009"]
    sites = ins.call_sites("seg10:0x94c4")
    assert len(sites) == 39
    aligns = {int(_cdecl_pushes(ins, s, 2)[1], 0) for s in sites}
    assert aligns <= {0, 2, 6}
    assert _cdecl_pushes(ins, "seg6:0x1e1a", 4) == ["0", "6", "0xff00", "0"]
    assert _sites(ins.refs(0x912C, 0x912D)) == ["seg6:0x199e", "seg10:0x503", "seg10:0x50a",
                                                "seg10:0x513"]


INTRO_WRITERS = {
    0x95D2: {"seg15:0x5834": 5000, "seg15:0x5edc": 0, "seg15:0x5f36": None, "seg15:0x5f42": 0,
             "seg15:0x608d": 45, "seg15:0x60ac": 620, "seg15:0x627e": 4, "seg15:0x6376": 3940,
             "seg15:0x63d5": 45, "seg15:0x7cf7": 6, "seg15:0x7cfe": 3, "seg15:0x7d0a": 0},
    0x95D0: {"seg15:0x582e": 290, "seg15:0x5bda": 400, "seg15:0x5edf": 0, "seg15:0x5f5b": None,
             "seg15:0x600f": 240, "seg15:0x7b85": 50, "seg15:0x7d18": 6, "seg15:0x7d1f": 3,
             "seg15:0x7d2c": 300},
    0x95DA: {"seg15:0x7e31": 0, "seg15:0x7e49": 2, "seg15:0x7e50": 4},
    0x1C70: {"seg15:0x8276": 0, "seg15:0x82fc": None},
}


@pytest.mark.parametrize("lo", sorted(INTRO_WRITERS), ids=lambda v: f"DGROUP:{v:#x}")
def test_intro_counter_writers(ins, lo):
    """shell.md Logo and Credits: the writers of the logo, credits and typing counters."""
    assert _write_values(ins, lo, lo + 1) == INTRO_WRITERS[lo]


def test_credits_and_title_constants(ins):
    """shell.md Title and Credits: deadlines, Calibrate thresholds and the reveal count."""
    expect = {"seg15:0x7b42": "add eax, 0x15f90",
              "seg15:0x7ce7": "mov dword ptr [bp - 8], 0x260004",
              "seg15:0x7cef": "cmp word ptr [0x8ffc], 0x320",
              "seg15:0x7d10": "cmp word ptr [0x8ffc], 0x320",
              "seg15:0x7e3a": "cmp word ptr [0x8ffc], 0x384",
              "seg15:0x80eb": "cmp word ptr [0x8ffc], 0x578",
              "seg15:0x7da8": "cmp word ptr [0x95d0], 0x12c",
              "seg15:0x7657": "cmp word ptr [0x95d4], 0x69",
              "seg15:0x62c7": "mov dword ptr [bp - 0x18], 0x808000",
              "seg15:0x6267": "push 0xc000ca"}
    assert {a: ins.at(a).text for a in expect} == expect


INTRO_CALLERS = {
    "seg15:0x7b34": ["seg15:0x5c89"],
    "seg15:0x7ba6": [],
    "seg15:0x811a": ["seg15:0x7b9b", "seg15:0x7dc8"],
    "seg15:0x8174": ["seg15:0x7dea", "seg15:0x7e06", "seg15:0x7e2b"],
    "seg15:0x80b0": ["seg15:0x7edf", "seg15:0x7ffe", "seg15:0x837b"],
    "seg15:0x7a62": ["seg15:0x8416"],
    "seg10:0x6d7a": ["seg15:0x363d", "seg15:0x367b", "seg16:0x21bf"],
    "seg10:0x39a8": ["seg9:0xd4d", "seg9:0x2665", "seg10:0x95d0", "seg15:0x58ba"],
    "seg10:0x4b2e": ["seg10:0xce", "seg15:0x57ac", "seg15:0x5c0e"],
}


@pytest.mark.parametrize("func", sorted(INTRO_CALLERS, key=A))
def test_intro_call_sites(ins, func):
    """shell.md Logo graphics, Credits, Palette: exhaustive caller lists."""
    assert ins.call_sites(func) == INTRO_CALLERS[func]


def test_intro_call_arguments(ins):
    """shell.md: seg10:0x39a8 is passed 0 only by the state-0 entry."""
    assert {s: _imm_push_before(ins, s) for s in ins.call_sites("seg10:0x39a8")} == {
        "seg9:0xd4d": 1, "seg9:0x2665": 1, "seg10:0x95d0": 1, "seg15:0x58ba": 0}


# The colours of ISPACE.DLL #4000 300 (entries 0-14) and the title ramps of palette-tables.md.
TITLE_300_COLOURS = ["ffe66e", "ecd162", "d8bc56", "c4a74a", "b0923e", "9c7d31", "886825",
                     "745319", "603e0d", "4c2800", "d7d7d6", "b9baba", "9e9e9e", "737373", "565552"]


def _trunc(a: int, b: int) -> int:
    return abs(a) // b * (1 if a >= 0 else -1)


def _emulate_5846(isp: Module) -> bytearray:
    """ISPACE.DLL seg5:0x5846 run from zeroed memory: constant blocks and the two loops."""
    mem = bytearray(0x2000)
    code = isp.func_insns("seg5:0x5846")

    def block(lo, hi):
        al = None
        for i in code:
            if not lo <= i.off < hi:
                continue
            if i.mnem == "mov" and i.ops[0] == "al":
                al = int(i.ops[1], 0) & 0xFF
            elif i.mnem == "xor" and i.ops == ("al", "al"):
                al = 0
            elif i.mnem == "mov" and i.abs_ds and i.mem.size == 1:
                mem[i.mem.disp] = al if i.ops[1] == "al" else int(i.ops[1], 0) & 0xFF

    block(0x5846, 0x596C)
    for r in range(18):
        b = 0x117C + 30 * r
        for k in range(1, 9):
            for j in range(3):
                mem[b + 3 * k + j] = (mem[b + j] - _trunc((mem[b + j] - mem[b + 27 + j]) * k, 9)) & 0xFF
    for r in range(18):
        b = 0x1CDC + 15 * r
        for k in range(1, 4):
            for j in range(3):
                mem[b + 3 * k + j] = (mem[b + j] - _trunc((mem[b + j] - mem[b + 12 + j]) * k, 4)) & 0xFF
    block(0x5A3B, 0x61B2)
    return mem


def _doc_rows(heading: str, width: int) -> list[list[str]]:
    text = (REPO_ROOT / "docs" / "re" / "palette-tables.md").read_text()
    sect = text.split(f"## {heading}\n", 1)[1].split("\n## ", 1)[0]
    rows = []
    for line in sect.splitlines():
        cells = [c.strip() for c in line.strip("|").split("|")]
        if cells and cells[0].isdigit():
            rows.append([c for c in cells if re.fullmatch(r"(?:[0-9a-f]{6} ?)+", c)][0].split())
    assert all(len(r) == width for r in rows)
    return rows


def test_colour_tables_of_seg5_5846(isp):
    """palette-tables.md and shell.md Palette: seg5:0x5846 calls nothing, stores only into the
    two tables, and its emulation equals the documented rows."""
    code = isp.func_insns("seg5:0x5846")
    assert not [i.addr for i in code if i.mnem in ("call", "lcall")]
    stores = {i.mem.disp for i in code if i.abs_ds and is_write(i)}
    assert all(0x117C <= d <= 0x1397 or 0x1CDC <= d <= 0x1DE9 for d in stores)
    assert isp.call_sites("seg5:0x5846") == ["seg5:0x39dd"]
    assert isp.call_sites("seg7:0x4ee8") == ["seg7:0x1726", "seg7:0x1a96", "seg7:0x1c59",
                                             "seg7:0x1ca4"]
    mem = _emulate_5846(isp)
    t1 = [[mem[0x117C + 30 * r + 3 * k:0x117C + 30 * r + 3 * k + 3].hex() for k in range(10)]
          for r in range(18)]
    t2 = [[mem[0x1CDC + 15 * r + 3 * k:0x1CDC + 15 * r + 3 * k + 3].hex() for k in range(5)]
          for r in range(18)]
    assert _doc_rows("Ten-colour schemes", 10) == t1
    assert _doc_rows("Five-colour schemes", 5) == t2
    assert t1[1] == TITLE_300_COLOURS[:10]
    assert t1[12] == ["000000"] * 10 and t2[12] == ["000000"] * 5


def test_title_palette_ramps():
    """palette-tables.md Title palette: entries 185-229 from graphic 300's colours, and the
    ramp offsets ISPACE.DLL DGROUP:0x178 / 0x17c."""
    c = [bytes.fromhex(x) for x in TITLE_300_COLOURS]

    def ramp(ks, n, end):
        out = [bytes((c[k][j] + s * _trunc(c[k + 1][j] - c[k][j], n)) & 0xFF
                     for j in range(3)).hex() for k in ks for s in range(n)]
        return out + [c[end].hex()]

    gold, grey = ramp(range(0, 9), 3, 9), ramp(range(10, 14), 4, 14)
    assert (len(gold), len(grey)) == (28, 17)
    text = (REPO_ROOT / "docs" / "re" / "palette-tables.md").read_text()
    rows = {m.group(1): m.group(2).split() for m in
            re.finditer(r"^\| (185–212|213–229) \| ([0-9a-f ]+) \|$", text, re.M)}
    assert rows == {"185–212": gold, "213–229": grey}
    dg = unpacked_seg("ispace", 11)
    assert (struct.unpack_from("<H", dg, 0x178)[0], struct.unpack_from("<H", dg, 0x17C)[0]) == \
        (185 * 4, 213 * 4)


# ------------------------------------------------------------------ registration, dialogs, briefing, desk


def _texts(m: Module, expect: dict[str, str]) -> dict[str, str]:
    return {a: m.at(a).text for a in expect}


def test_edition_check_constants(ins):
    """shell.md Edition check: the day block and the decision constants of seg10:0x885e."""
    expect = {"seg10:0x886d": "cmp word ptr [0x920c], -1",
              "seg10:0x8899": "sub ax, 0x5d",
              "seg10:0x88a0": "imul ax, ax, 0x16d",
              "seg10:0x88ab": "mov word ptr [0x922c], 1",
              "seg10:0x88d9": "push 0x28",
              "seg10:0x88f3": "push 0x15",
              "seg10:0x891a": "push 0x9ef",
              "seg10:0x8954": "push 0x9f0",
              "seg10:0x8947": "push 0x30",
              "seg10:0x894e": "mov ax, 0xfff6",
              "seg10:0x8981": "push 2",
              "seg10:0x89d0": "cmp ax, 1",
              "seg10:0x8a75": "sub ax, 0x4cf",
              "seg10:0x8a91": "xor al, 0xff",
              "seg10:0x8aa8": "mov word ptr [0x9224], 0xc8",
              "seg10:0x8ab8": "mov ax, 0xffff",
              "seg10:0x8ac9": "cmp word ptr [bp - 0x152], 0x23",
              "seg10:0x8aec": "cmp word ptr [bp - 0x152], 0xe",
              "seg10:0x8afc": "cmp word ptr [bp - 0x16e], 0x3e8"}
    assert _texts(ins, expect) == expect
    # strings 2543 and 2544, and the only ordinal-24 calls with commands 0x15 and 2
    assert (0x9EF, 0x9F0) == (2543, 2544)
    assert [s for s in ("seg10:0x88f5", "seg10:0x8983") if ins.at(s).text == "lcall [0x3c68]"] == \
        ["seg10:0x88f5", "seg10:0x8983"]


def test_edition_check_dead_constants(ins, dgroup):
    """shell.md Edition check: DGROUP:0x5d0, 0x1120 and 0x1122 are zero with no writer, so
    DGROUP:0x920c = 3 and the CD flag are dead; DGROUP:0x922c is written once and never read."""
    assert [struct.unpack_from("<H", dgroup, o)[0] for o in (0x5D0, 0x1120, 0x1122)] == [0, 0, 0]
    assert _sites(ins.refs(0x5D0, 0x5D1)) == ["seg10:0x8959", "seg10:0x89b1"]
    assert _sites(ins.refs(0x1120, 0x1121)) == ["seg10:0x8969", "seg10:0x89bf"]
    assert _sites(ins.refs(0x1122, 0x1123)) == ["seg10:0x8960", "seg10:0x89b8"]
    assert ins.writers(0x5D0, 0x5D1) == [] and ins.writers(0x1120, 0x1123) == []
    assert not [i.addr for i in ins.insns if i.mem is not None and i.mem.ds
                and (i.mem.regs or i.mnem == "lea")
                and any(lo <= i.mem.disp <= lo + 1 for lo in (0x5D0, 0x1120, 0x1122))]
    assert _write_values(ins, 0x922C, 0x922D) == {"seg10:0x88ab": 1}
    assert _sites(ins.refs(0x922C, 0x922D)) == ["seg10:0x88ab"]
    threes = [a for a, v in _write_values(ins, 0x920C, 0x920D).items() if v == 3]
    assert threes == ["seg10:0x89c7"]
    assert {a for a, v in _write_values(ins, 0x11AA, 0x11AB).items() if v == 1} == {"seg10:0x8907"}


def test_ordinal_24_command_table(isp):
    """ispace.md ordinal 24: the 22-entry switch of ISPACE.DLL seg10:0x48d2 and its retf."""
    sw = isp.switches["seg10:0x4ae0"]
    assert (sw["table"], sw["count"]) == ("seg10:0x4ae5", 22)
    t = sw["targets"]
    assert {c: t[c] for c in (0, 2, 9, 10, 11, 13, 14, 17, 19, 20, 21)} == {
        0: "seg10:0x490b", 2: "seg10:0x4a6b", 9: "seg10:0x4a7e", 10: "seg10:0x4a99",
        11: "seg10:0x4933", 13: "seg10:0x490b", 14: "seg10:0x4b11", 17: "seg10:0x4a7a",
        19: "seg10:0x4b11", 20: "seg10:0x4a12", 21: "seg10:0x49bf"}
    assert isp.at("seg10:0x4b17").text == "retf 0xa"
    assert isp.owners(*A("seg10:0x4ae0")) == {"seg10:0x48d2"}


def test_registration_record_constants(isp):
    """formats.md Resources rewritten in place: the install record, the name scramble and the
    INI writer seg10:0x5b24."""
    expect = {
        # install record: D = rand() % 7651 + 5432, x = D mod 653 + 654, resource #5500 110
        "seg10:0x4bd0": "mov cx, 0x1de3", "seg10:0x4bd8": "add ax, 0x1538",
        "seg10:0x4c09": "mov cx, 0x28d", "seg10:0x4c10": "add dx, 0x28e",
        "seg10:0x4c1b": "push 0x6e", "seg10:0x4c1d": "push 0x157c",
        # the command-2 relation in seg10:0x51b6
        "seg10:0x5243": "mov cx, 0x28d", "seg10:0x524a": "add dx, 0x28e",
        # name scramble: key 2A 32 2D, rand() % 96 + 2
        "seg10:0x4dc9": "mov byte ptr [bp - 0xc], 0x2a",
        "seg10:0x4dcd": "mov byte ptr [bp - 0xb], 0x32",
        "seg10:0x4dd1": "mov byte ptr [bp - 0xa], 0x2d",
        "seg10:0x4df0": "mov cx, 0x60", "seg10:0x4df6": "add dl, 2",
        "seg10:0x4e13": "jl seg10:0x4e20", "seg10:0x4e20": "mov byte ptr es:[bx + si], 0",
        # INI writer: s = rand() % 1111 + rand() % 8999 + 1987, X = 2s - P - 1231
        "seg10:0x5b4a": "mov cx, 0x457", "seg10:0x5b57": "mov cx, 0x2327",
        "seg10:0x5b5f": "add si, 0x7c3", "seg10:0x5d52": "sub ax, 0x4cf",
        "seg10:0x5c0d": "mov cx, 0x1a79", "seg10:0x5c13": "add dx, 0xf49",
        "seg10:0x5c44": "xor al, 0xff", "seg10:0x5c51": "mov cx, 0xc8",
        "seg10:0x5c5c": "sub dx, 0xc7"}
    assert _texts(isp, expect) == expect
    assert (0x1DE3, 0x1538, 0x28D, 0x28E, 0x157C) == (7651, 5432, 653, 654, 5500)
    assert (0x457, 0x2327, 0x7C3, 0x1A79, 0xF49) == (1111, 8999, 1987, 6777, 3913)
    assert isp.call_sites("seg10:0x4dba") and isp.call_sites("seg10:0x5b24")


def test_registration_resources_on_the_game():
    """formats.md: the game's #5500 110 is all zero and #5000 143 holds the clear text."""
    m = _ne("INSPACE.ex_")
    rec = {(r.type_id, r.id): r for r in m.resources}
    r110, r143 = rec[(5500, 110)], rec[(5000, 143)]
    assert (r110.file_off, r143.file_off) == (0x4B840, 0x4B860)
    assert m.data[r110.file_off:r110.file_off + 10] == bytes(10)
    assert m.data[r143.file_off:r143.file_off + 50].startswith(b"Unregistered Copy   ILLEGAL")


def _refused_ships(isp: Module) -> list[int]:
    """Emulate the sub/je chain of ISPACE.DLL seg10:0x61da over MASTER.BOD word +0x21."""
    out = []
    for v in range(2000):
        ax, pc = v, A("seg10:0x61da")[1]
        while pc not in (0x61C0, 0x6167):
            i = isp.at(f"seg10:{pc:#x}")
            nxt = isp.step(i, 1).off
            tgt = int(i.ops[0].split(":")[1], 16) if i.mnem.startswith("j") else None
            if i.mnem == "sub":
                ax -= int(i.ops[1], 0)
            elif i.mnem == "dec":
                ax -= 1
            taken = {"je": ax == 0, "jne": ax != 0, "jge": ax >= 0, "jle": ax <= 0,
                     "jno": True, "jmp": True}.get(i.mnem)
            pc = tgt if taken else nxt
        if pc == 0x61C0:
            out.append(v)
    return out


def test_ship_availability_list(isp):
    """shell.md Ship availability: the MASTER.BOD ids that seg10:0x611c refuses."""
    assert _refused_ships(isp) == [178, 310, 320, 400, 440, 450, 520, 550, 560, 590, 600, 604,
                                   605, 710, 720, 730, 750, 890, 900, 930, 950]


def test_dialog_constants(isp):
    """shell.md Game-start dialogs: fonts, the Order edit message, runtime windows and ids."""
    expect = {"seg9:0x29ca": "push 0xe", "seg9:0x29cc": "push 6", "seg9:0x29f8": "push 0x2da",
              "seg10:0x12b": "push 0x2bd", "seg10:0x134": "push 0x401",
              "seg10:0xbee": "push 0x5021", "seg10:0xbf1": "push 0x102",
              "seg10:0xbf4": "push 0x41", "seg10:0xbf6": "push 0xa", "seg10:0xbf8": "push 0x8c",
              "seg10:0xbfb": "push 0x1e", "seg10:0xc00": "push 0x14c",
              "seg10:0x1659": "push 0x5000", "seg10:0x165e": "push 4",
              "seg10:0x1663": "sub ax, 0x71", "seg10:0x166e": "push 0x64",
              "seg10:0x22cb": "sub ax, 0x68", "seg10:0x22d2": "sub ax, 0x67",
              "seg10:0x22d6": "push 0x60"}
    assert _texts(isp, expect) == expect
    for site in ("seg9:0x29e8", "seg9:0x2a0f"):
        assert "CreateFont" in isp.at(site).text
    for site in ("seg10:0xc0b", "seg10:0x1678", "seg10:0x22e7"):
        assert "CreateWindow" in isp.at(site).text
    assert (0x2DA, 0x2BD, 0x14C, 0x71, 0x68, 0x67) == (730, 701, 332, 113, 104, 103)


def test_briefing_constants(ins):
    """shell.md State 3: box geometry, label strings, typewriter timings and phase 4's PatBlt."""
    expect = {"seg9:0x8943": "cmp word ptr [0x90ea], 0x280", "seg9:0x8959": "cmp ax, 0x26c",
              "seg9:0x8965": "cmp word ptr [0x10], 0x1b8", "seg9:0x897b": "cmp ax, 0x1cc",
              "seg9:0x89c4": "sub ax, 0xb4", "seg9:0x89ca": "cmp word ptr [0x10], 0x190",
              "seg9:0x8a9b": "push 0x9e9", "seg9:0x8ad7": "push 0x8dc",
              "seg9:0x8b4a": "push 0x9d3", "seg9:0x8b53": "push 0x90c",
              "seg9:0x8d41": "add ax, 0x19", "seg9:0x8d4b": "add cx, 0x37",
              "seg9:0x8d6c": "add eax, 0x1f4", "seg9:0x8dae": "mov word ptr [bp - 0x48], 0x7d0",
              "seg9:0x8df1": "add eax, 0x190", "seg9:0x8e78": "push 0x11",
              "seg9:0x8f22": "add word ptr [0x1832], 0x19", "seg9:0x9019": "add eax, 0x7d0",
              "seg15:0x6a16": "sub ax, 0x73", "seg15:0x6a1e": "add cx, 0x95"}
    assert _texts(ins, expect) == expect
    assert (0x9E9, 0x8DC, 0x9D3, 0x90C) == (2537, 2268, 2515, 2316)
    assert "SetBkColor" in ins.at("seg15:0x6956").text and "PatBlt" in ins.at("seg15:0x6a27").text
    assert ins.switches["seg17:0x350"]["count"] == 19


def test_desk_typing_and_launch_constants(ins):
    """shell.md Game-start dialogs items 7-9: desk typing, fly-up and CRT flash constants."""
    expect = {"seg15:0x1599": "sub ax, 0x3f", "seg15:0x15a8": "mov cx, 0x15",
              "seg15:0x1605": "add ax, 0x1f4", "seg15:0x167a": "add ax, 0x28",
              "seg15:0x1872": "add ax, 0x76c", "seg15:0x1844": "push 0x2b",
              "seg15:0xce": "mov word ptr [0x92d6], 0x3c",
              "seg15:0x169": "mov word ptr [0x17c8], 0x96",
              "seg15:0x175": "mov word ptr [0x17d0], 0x64",
              "seg15:0x2c4": "cmp word ptr [0x8ffc], 0x578", "seg15:0x611": "push 0x4c",
              "seg15:0x72f": "add ax, 0x5b4", "seg15:0x84d3": "push 0x14",
              "seg15:0x8553": "cmp word ptr [bp - 4], 0xc8"}
    assert _texts(ins, expect) == expect


def test_credits_typing_counter_references(ins):
    """shell.md Credits: DGROUP:0x95da has exactly five direct accesses, all keeping it even."""
    refs = ins.refs(0x95DA, 0x95DA)
    assert _sites(refs) == ["seg15:0x7de1", "seg15:0x7e31", "seg15:0x7e49", "seg15:0x7e50",
                            "seg15:0x7e55"]
    assert ins.at("seg15:0x7e55").text == "test byte ptr [0x95da], 1"


def _ord14(v: int, p: int) -> int:
    q = s16(v * p & 0xFFFF)
    q = int(q / 100)                          # idiv truncates toward zero
    return 255 if q > 255 else q & 0xFF


def test_ordinal_14_wrap(isp):
    """shell.md Palette: ordinal 14 sign-extends the 16-bit product (byte 0x99, CWD)."""
    assert isp.at("seg5:0x518c").raw == b"\x99"
    assert isp.at("seg5:0x5186").text == "imul word ptr [bp + 0xa]"
    assert isp.at("seg5:0x518f").text == "cmp ax, 0xff"
    assert {v: _ord14(v, 204) for v in (125, 126, 160, 161, 180, 195, 200, 220, 240, 255)} == {
        125: 255, 126: 255, 160: 255, 161: 186, 180: 224, 195: 255, 200: 9, 220: 50, 240: 91,
        255: 121}
    assert all(_ord14(v, 204) == v * 204 // 100 for v in range(126))
    assert {v: _ord14(v, 137) for v in (186, 187, 239, 240, 250, 255)} == {
        186: 254, 187: 255, 239: 255, 240: 186, 250: 200, 255: 206}


def _pixels4(data: bytes) -> Counter:
    o = 14 if data[:2] == b"BM" else 0
    _size, w, h, _planes, bpp, _comp = struct.unpack_from("<IiiHHI", data, o)
    assert bpp == 4
    stride = (w * 4 + 31) // 32 * 4
    bits = o + 40 + 4 * 16
    out: Counter = Counter()
    for y in range(abs(h)):
        row = data[bits + y * stride:bits + y * stride + stride]
        for x in range(w):
            b = row[x // 2]
            out[(b >> 4) if x % 2 == 0 else (b & 15)] += 1
    return out


def test_credits_logo_340():
    """shell.md Credits: IGRAPHIC.DLL #4000 340's colour table and pixel counts."""
    data = _resource("IGRAPHIC.DL1", 4000, 340)
    w, h, bpp, table = _dib(data)
    assert (w, h, bpp) == (333, 37, 4)
    assert table == ["ffffff", "e0e0e0", "b4b4ff", "a0a0dc", "8c8cff", "6969ff", "4040ff",
                     "0000ff", "0000b4", "000080"] + ["000000"] * 6
    assert dict(_pixels4(data)) == {1: 156, 2: 836, 3: 4, 4: 1550, 5: 769, 6: 1768, 7: 1010,
                                    8: 6, 9: 1424, 15: 4798}


def test_desk_scene_resource():
    """formats.md IGRAPHIC.DLL #6600: scene 100's header colours, conditions, records and slots;
    shell.md: BITMAP CORNER."""
    d = _resource("IGRAPHIC.DL1", 6600, 100)
    w = lambda o: struct.unpack_from("<h", d, o)[0]              # noqa: E731
    assert [w(2 * k) for k in range(9)] == [255, 255, 255, 0, 224, 0, 220, 202, 70]
    assert [w(0x24 + 2 * k) for k in range(4)] == [3, 4, 4, 8]
    assert [w(0x2C + 2 * k) for k in range(9)] == [32, 32, 32, 0, 100, 0, 120, 108, 38]
    assert (d[0x50], d[0x51], d[0x52], d[0x53], d[0x5C]) == (6, 7, 3, 5, 0xF4)
    assert struct.unpack_from("<ii", d, 0x5C) == (500, 217)
    assert d[0x6C:0x80] == bytes(0x14)
    n = (len(d) - 0x80) // 0x26
    recs = [d[0x80 + 0x26 * k:0x80 + 0x26 * (k + 1)] for k in range(n)]
    assert n == 62 and recs[61] == bytes(0x26)
    sb = lambda b: b - 256 if b > 127 else b                    # noqa: E731
    slots = {k: sb(r[0x1F]) for k, r in enumerate(recs) if r[0x1E] == 1 and sb(r[0x1F]) >= 0}
    assert slots == {42: 0, 17: 1, 5: 2, 18: 3, 60: 4}
    assert sb(recs[60][0x1D]) == -1
    cond = {k: sb(r[0x1F]) for k, r in enumerate(recs) if r[0x1E] == 1 and sb(r[0x1F]) < -1}
    assert cond == {2: -20, 36: -11, 37: -10, 38: -11, 39: -10}
    assert {k: (r[0x1E], sb(r[0x1F])) for k, r in enumerate(recs) if 32 <= k <= 35} == {
        32: (2, -11), 33: (2, -11), 34: (2, -10), 35: (2, -10)}
    assert all(r[0x19:0x1B] == b"\0\0" and r[0x21:0x26] == bytes(5) for r in recs)
    assert d[0x48 + 10] == 3 and d[0x48 + 11] == 5 and d[0x48 + 20] == 0xF4
    assert _dib(_resource("IGRAPHIC.DL1", 2, "CORNER"))[:3] == (8, 5, 4)


# ------------------------------------------------------------------ stage 3c: installer, desk, briefing, load screen


@pytest.fixture(scope="module")
def inst() -> Module:
    return module("install")


def _c_string(data: bytes, off: int) -> str:
    return data[off:data.index(b"\0", off)].decode("latin-1")


def test_install_module_header():
    """install.md Module: I_INSTALL, OPTLOADER-packed, four segments, DGROUP seg4, imports."""
    m = _ne("INSTALL.exe")
    assert (m.module_name, m.description) == ("I_INSTALL", "IS Installer")
    assert len(m.segments) == 4 and m.autodata == 4
    assert m.modules == ["KERNEL", "USER", "GDI", "LZEXPAND", "DDEML"]
    assert [(r.type_id, r.id) for r in m.resources] == [
        (3, 1), (14, "GAME"), (5500, 110), (5500, 120), (10, "DLGINCLUDE"), (5, 135), (5, 300),
        (5, 100), (5, 1922), (5, 1774), (5, 891), (5, 200), (5, 175), (5, 333)]
    assert _resource("INSTALL.exe", 5500, 110) == bytes(32)
    assert _resource("INSTALL.exe", 5500, 120) == bytes(128)


def _dialog(name: str, rid: int) -> dict:
    from isre.res_formats import parse_dialog

    return parse_dialog(_resource(name, 5, rid))


def test_install_dialog_controls():
    """install.md Dialogs: captions and control ids of the installer's templates."""
    ids = {rid: [c["id"] for c in _dialog("INSTALL.exe", rid)["controls"]]
           for rid in (300, 135, 1922, 1774, 100, 200)}
    assert ids[300] == [301, 119, 124, 125, 135, 306, 304, 127, 128, 129, 130, 0xFFFF, 132, 133,
                        134, 302, 100, 2, 303, 131, 305, 308]
    assert ids[135] == [101, 102, 103, 2, 3, 10]
    assert ids[1922] == [1, 101, 142, 143, 144, 145, 146, 136, 137, 138, 139, 140, 0xFFFF, 147]
    assert ids[1774] == [1, 101, 147, 1775, 1776]
    assert ids[100] == [130, 1] and ids[200] == [120, 1, 130]
    caps = {rid: _dialog("INSTALL.exe", rid)["caption"] for rid in (300, 135, 1922, 1774, 100, 200)}
    assert caps == {300: "Inner Space New Valid Install", 135: " Inner Space",
                    1922: "Game Difficulty and Speed", 1774: "Play Full Screen or in a Window",
                    100: "Inner Space", 200: "Installing..."}
    d300 = {c["id"]: c for c in _dialog("INSTALL.exe", 300)["controls"]}
    # Required (132) sits on the "Disk Space Required:" row, Available (131) after its label
    assert d300[303]["text"] == "Disk Space Required:" and d300[132]["x"] < d300[0xFFFF]["x"]
    assert d300[131]["x"] > d300[0xFFFF]["x"]


def test_install_ini_on_the_game():
    """install.md INSTALL.INI: [Vers], [Dirs] and the groups with their destination codes."""
    import configparser

    cp = configparser.ConfigParser(delimiters=("=",))
    cp.optionxform = str
    cp.read_string(require(ORIGINAL / "game" / "INSTALL.INI").read_text("latin-1"))
    assert dict(cp["Vers"]) == {"V": "12908", "Def": "1"}
    assert list(cp["Dirs"]) == ["SHIPS", "SOUNDS", "GAMES", "RETIRED"]
    groups = {g: {k: int(v) for k, v in cp[str(g)].items()} for g in range(8)}
    assert groups == {
        0: {"MASTER.BOD": 1, "MASTER.SHP": 1, "WAVEMIX.DLL": 8, "WAVEMIX.INI": 8, "CTL3D.DLL": 8},
        1: {n: 0 for n in ("ISPACE.DLL", "ENG_LANG.DLL", "IGRAPHIC.DLL", "INSPACE.EXE",
                           "INSTALL.EXE", "ENG_HELP.HLP", "README.TXT", "ORDER.TXT")},
        2: {"FACTORY.EXE": 0}, 3: {"SND_FX.DLL": 2}, 4: {"ENG_FX.DLL": 2},
        5: {"MAIN.MID": 2, "RACE.MID": 2, "DEMON.MID": 2, "ANTHEM.MID": 2},
        6: {"INSPACE.SCR": 8, "INRUN.AD": 9}, 7: {"INSPACE.INI": 0}}
    assert list(cp) == ["DEFAULT", "Vers", "Dirs"] + [str(g) for g in range(8)] + ["Temp"]
    # the '_' sources are plain MZ files, the '1' sources SZDD
    sd = ORIGINAL / "game"
    assert (sd / "INSPACE.ex_").read_bytes()[:2] == b"MZ" == (sd / "FACTORY.ex_").read_bytes()[:2]
    assert (sd / "MASTER.BO1").read_bytes()[:4] == b"SZDD"


def test_install_dgroup_strings():
    """install.md: the strings INSTALL.EXE's code names, and no Def or Limited key name."""
    d = unpacked_seg("install", 4)
    expect = {0x10: "IS Install", 0x22: " KB", 0x1b1: "Inner Space Demo Install",
              0x1ca: "Inner Space Valid ReInstall", 0x213: "N/A", 0x40f: "Installation Cancelled.",
              0x724: "name", 0x729: "organization", 0x737: " - ", 0x73b: "ThePlayer",
              0x745: "Detail", 0x74c: "SD_LOGO", 0x75a: "Full", 0x75f: "Complex",
              0x767: "Braking", 0x76f: "FrameRateSet", 0x77c: "SECURE.DAT", 0x8cf: "Can't find ISPACE.DLL!!",
              0x8e7: "ISPACE.DLL is faulty!!", 0x8fe: "Library file ISPACE.DLL is corrupted!",
              0xc4e: "PROGMAN", 0xc57: "V", 0xc59: "Vers", 0xc5e: "V0.00.00",
              0x1e6: "FACTORY.EX_", 0x3e: "INSPACE.SC1", 0x1f2: "MAIN.MI1", 0x1fb: "SND_FX.DLL",
              0x206: "ENG_FX.DLL"}
    assert {o: _c_string(d, o) for o in expect} == expect
    assert _c_string(d, 0x6b2).startswith("Inner Space has been installed before.")
    assert _c_string(d, 0x7ef).startswith("Make sure any running programs")
    assert _c_string(d, 0x849).startswith("Installer cannot run if install disk is write-protected.")
    assert _c_string(d, 0x21b).startswith("The install path you typed in is too long or invalid.")
    assert not re.search(rb"(?i)def|limited", d)


def test_install_constants(inst):
    """install.md: ordinals, dialog constants, speed test, calibration and the registration call."""
    expect = {
        # ISPACE.DLL ordinals 2, 1, 3, 4, 35, 7, 9, 10, 24 and where they go
        "seg2:0x3ebb": "push 2", "seg2:0x3ec2": "mov word ptr [0x13d0], ax",
        "seg2:0x3edd": "push 1", "seg2:0x3ee4": "mov word ptr [0x13c8], ax",
        "seg2:0x3eff": "push 3", "seg2:0x3f06": "mov word ptr [0x13cc], ax",
        "seg2:0x3f21": "push 4", "seg2:0x3f28": "mov word ptr [0x13d4], ax",
        "seg2:0x3f43": "push 0x23", "seg2:0x3f4a": "mov word ptr [0x13e8], ax",
        "seg2:0x3f65": "push 7", "seg2:0x3f6c": "mov word ptr [0x13d8], ax",
        "seg2:0x3f87": "push 9", "seg2:0x3f8e": "mov word ptr [0x13dc], ax",
        "seg2:0x3fa9": "push 0xa", "seg2:0x3fb0": "mov word ptr [0x13e0], ax",
        "seg2:0x3fcb": "push 0x18", "seg2:0x3fd2": "mov word ptr [0x13e4], ax",
        "seg2:0x3ea5": "cmp ax, 0x20", "seg2:0x38c9": "push 0x1000",
        # FORCE and RELEASE zeroed after the command line
        "seg2:0x38": "mov word ptr [0x13f4], ax", "seg2:0x3b": "mov word ptr [0x1400], ax",
        # installed-before box type and keep flags
        "seg2:0x2918": "push 0x124", "seg2:0x2924": "cmp ax, 6",
        "seg2:0x292c": "mov word ptr [0x13ec], ax", "seg2:0x292f": "mov word ptr [0x13ee], ax",
        # dialog 300: sizes 771, 37, 316, 336, 239, 21 KB; Required base 46
        "seg2:0x793": "push 0x303", "seg2:0x7c7": "push 0x25", "seg2:0x7fb": "push 0x13c",
        "seg2:0x830": "push 0x150", "seg2:0x865": "push 0xef", "seg2:0x89a": "push 0x15",
        "seg2:0xb6a": "mov word ptr [0x10c0], 0x2e", "seg2:0xb7e": "add dword ptr [0x10c0], 0x303",
        "seg2:0xe73": "push 0x10",
        # group flags on OK
        "seg2:0xe7d": "mov word ptr [0x1290], 1", "seg2:0xe91": "mov word ptr [0x1292], 1",
        "seg2:0xea5": "mov word ptr [0x1294], 1", "seg2:0xeba": "mov word ptr [0x1298], 1",
        "seg2:0xece": "mov word ptr [0x1296], 1", "seg2:0xee3": "mov word ptr [0x129a], 1",
        "seg2:0xef8": "mov word ptr [0x129c], 1",
        # source names: '1' and '_' probes, chunk size, INSTALL.EXE -> BACKUP.EXE compare
        "seg2:0x36f4": "mov byte ptr [bp + si - 0x7f], 0x31",
        "seg2:0x371f": "mov word ptr [0x1594], 1",
        "seg2:0x3747": "mov byte ptr [bp + si - 0x7f], 0x5f", "seg2:0x37c8": "sub sp, 0x802",
        "seg2:0x1b2a": "push 0x4e7",
        # dialogs 1922 and 1774
        "seg2:0x34bc": "push 0x8e0092", "seg2:0x34c2": "push 0x90",
        "seg2:0x34cd": "push 0x89008c", "seg2:0x34d3": "push 0x8a",
        "seg2:0x35d0": "mov word ptr [bp - 0xe], 0x14", "seg2:0x35e6": "mov word ptr [bp - 0xe], 0x24",
        "seg2:0x35fc": "mov word ptr [bp - 0xe], 0x36", "seg2:0x3603": "mov word ptr [bp - 0xe], 0x48",
        "seg2:0x3410": "push 0x6ef06f0", "seg2:0x3416": "push 0x6ef",
        # dialog 135: limits 20 and 25, GetWindowText 0x12 and 0x16, first-space cut within 20
        "seg2:0x2997": "push 0x415", "seg2:0x299a": "push 0x14",
        "seg2:0x2aae": "push 0x12", "seg2:0x2ac5": "push 0x16",
        "seg2:0x2b25": "cmp word ptr [bp - 0xde], 0x14",
        # speed test: 1000 ms, thresholds 350000, 180000, 70000
        "seg2:0x2b7b": "add cx, 0x3e8", "seg2:0x2b8c": "cmp dword ptr [bp - 0xd8], 0x55730",
        "seg2:0x2b9e": "cmp dword ptr [bp - 0xd8], 0x2bf20",
        "seg2:0x2bb0": "cmp dword ptr [bp - 0xd8], 0x11170",
        # splash calibration
        "seg2:0x2c2b": "mov word ptr [0x1424], 0x1388", "seg2:0x2c31": "mov word ptr [0x1422], 0x1a4",
        "seg2:0x2c9f": "add ax, 0x6a4", "seg2:0x2cd8": "add cx, 0x1f4",
        "seg2:0x2ce9": "cmp dword ptr [bp - 0xc], 0x2ab98", "seg2:0x2cf5": "push 0xbb8",
        "seg2:0x2d01": "add ax, 0x5a", "seg2:0x2d09": "cmp dword ptr [bp - 0xc], 0x15f90",
        "seg2:0x2d21": "add ax, 0x1e", "seg2:0x2d26": "cmp dword ptr [bp - 0xc], 0x88b8",
        "seg2:0x2d3e": "add ax, 5",
        # registration: cmd 13 or 0, else ordinal 10 (0, 0, 0, day, 0x54, 0, 9)
        "seg2:0x43ed": "cmp word ptr [0x15b4], 0", "seg2:0x43fb": "cmp byte ptr [0x1426], 0x41",
        "seg2:0x4402": "cmp byte ptr [0x1426], 0x42", "seg2:0x4416": "cmp word ptr [0x15d4], 0",
        "seg2:0x4425": "push 0xd", "seg2:0x4431": "push 0", "seg2:0x4433": "lcall [0x13e4]",
        "seg2:0x4458": "sub ax, 0x5d", "seg2:0x445e": "imul ax, ax, 0x16d",
        "seg2:0x4475": "push 0x54", "seg2:0x4479": "push 9", "seg2:0x447b": "lcall [0x13e0]"}
    assert _texts(inst, expect) == expect
    assert (0x303, 0x25, 0x13C, 0x150, 0xEF, 0x15, 0x2E) == (771, 37, 316, 336, 239, 21, 46)
    assert 46 + 771 + 37 + 316 + 336 + 239 + 21 == 1766
    assert (0x55730, 0x2BF20, 0x11170, 0x2AB98, 0x15F90, 0x88B8) == (
        350000, 180000, 70000, 175000, 90000, 35000)
    assert (0x1388, 0x1A4, 0x6A4, 0xBB8, 0x5A, 0x1E) == (5000, 420, 1700, 3000, 90, 30)
    assert _c_string(unpacked_seg("install", 4), 0x4e7) == "INSTALL.EXE"


def test_install_dead_paths(inst):
    """install.md Dead paths: the FORCE/RELEASE flags are only zeroed, the DDE routine is uncalled."""
    assert _sites(inst.refs(0x13F4, 0x13F5)) == ["seg2:0x38", "seg2:0x44fe"]
    assert _sites(inst.refs(0x1400, 0x1401)) == ["seg2:0x3b", "seg2:0x4498"]
    assert inst.stored_value(inst.at("seg2:0x38")) == 0 == inst.stored_value(inst.at("seg2:0x3b"))
    assert inst.funcs["seg2:0x49ae"]["callers"] == []


def test_theworks_setup_never_calls_ordinal_24():
    """install.md, pitfalls.md: theworks/SETUP.EXE fetches ordinal 24 into DGROUP:0x13c0 only."""
    s = module("setup-1998")
    assert s.at("seg2:0x258a").text == "push 0x18"
    assert _sites(s.refs(0x13C0, 0x13C3)) == ["seg2:0x2591", "seg2:0x2594", "seg2:0x259a"]
    assert not [i.addr for i in s.insns if i.mnem in ("lcall", "ljmp") and "0x13c0" in i.text]


def test_ispace_installer_commands(isp):
    """ispace.md ordinal 24: the media writer's drive test and the serial routine's constants."""
    expect = {"seg10:0x5628": "sub ax, 0x40", "seg10:0x562e": "cmp ax, 2",
              "seg10:0x5631": "jbe seg10:0x5638", "seg10:0x5638": "cmp word ptr [0x1ac], 0",
              "seg10:0x564a": "test ah, 0x40",
              # seg10:0x5dfe: counter > 8, record write-back, controls 700 and 701
              "seg10:0x5ea1": "cmp word ptr es:[bx + 4], 8", "seg10:0x5ea6": "jg seg10:0x5eb0",
              "seg10:0x5f10": "push 0x6e", "seg10:0x5f12": "push 0x157c",
              "seg10:0x5f67": "push 0x2bc",
              # seg10:0x492: the serial generator
              "seg10:0x4b1": "mov cx, 0xa", "seg10:0x4b7": "add dx, 3", "seg10:0x4ca": "mov cx, 5",
              "seg10:0x4d1": "imul ax, dx, 0xb73", "seg10:0x4e2": "mov cx, 0x20",
              "seg10:0x4ea": "shl ax, 2", "seg10:0x4fb": "mov cx, 0x32",
              "seg10:0x501": "imul ax, dx, 0x11", "seg10:0x512": "mov cx, 0x141",
              "seg10:0x51f": "mov cx, 0x4da", "seg10:0x532": "mov ax, word ptr [0x213a]",
              "seg10:0x543": "mov cx, 0x11", "seg10:0x54c": "add ax, 0x32d",
              "seg10:0x570": "cmp word ptr [0x154c], 0x5398", "seg10:0x57d": "mov cx, 4",
              "seg10:0x585": "imul ax, dx, 0x4d2", "seg10:0x597": "mov cx, 0x3f2",
              "seg10:0x5bf": "cmp dx, 9", "seg10:0x5c6": "cmp ax, 0x27c0",
              "seg10:0x5cb": "cmp dx, 0xa", "seg10:0x5d2": "cmp ax, 0xae60",
              "seg10:0x5d7": "sub word ptr [0x154c], 0xa120", "seg10:0x5dd": "sbb word ptr [0x154e], 7",
              "seg10:0x5e7": "mov cx, 6", "seg10:0x5f6": "push 1", "seg10:0x5f8": "push 0x86a0",
              "seg10:0x639": "push 0x2bc", "seg10:0x64e": "push 0x2bd"}
    assert _texts(isp, expect) == expect
    assert (0xB73, 0x141, 0x4DA, 0x32D, 0x5398, 0x4D2, 0x3F2) == (2931, 321, 1242, 813, 21400,
                                                                  1234, 1010)
    assert (0x927C0, 0xAAE60, 0x7A120, 0x186A0) == (600000, 700000, 500000, 100000)
    assert (9 << 16 | 0x27C0, 0xA << 16 | 0xAE60, 7 << 16 | 0xA120, 1 << 16 | 0x86A0) == (
        600000, 700000, 500000, 100000)
    assert _resource("IGRAPHIC.DL1", 5500, 110) == bytes(32)


def _ms_rand(seed: int):
    while True:
        seed = (seed * 0x343FD + 0x269EC3) & 0xFFFFFFFF
        yield (seed >> 16) & 0x7FFF


def _serial(seed: int, d: int) -> int:
    """shell.md Order, Serial: ISPACE.DLL seg10:0x492 in Python."""
    r = _ms_rand(seed)
    n = next(r) % 10 + 3
    v = 0
    for _ in range(n):
        v = s16((next(r) % 5 + 1) * 2931)
        v += s16((next(r) % 32) * 4)
        v += s16((next(r) % 50) * 17)
        a = next(r) % 321
        v += s16(a + next(r) % 1242)
        v += d * (next(r) % 17 + 813)
        if v < 21400:
            v += s16((next(r) % 4 + 2) * 1234)
            v += next(r) % 1010
        v = abs(v)
        if 600000 < v < 700000:
            v -= 500000
        v += (next(r) % 6) * 100000
    return v


def test_order_serial_model():
    """shell.md Order, Serial: the model reproduces the oracle's two serials at D = 12342."""
    d = (126 - 93) * 365 + 297
    assert d == 12342
    assert _serial(13176, d) == 10127790
    assert _serial(17457, d) == 10543958 == _serial(36978, d)


def test_drive_list_constants(isp):
    """shell.md DRIVE: the strings and constants of ISPACE.DLL seg10:0x1e7c's drive list."""
    d = unpacked_seg("ispace", 11)
    assert {o: _c_string(d, o) for o in (0x304, 0x311, 0x319, 0x320, 0x32b)} == {
        0x304: "Test", 0x311: "C", 0x319: "A:\\*.*", 0x320: "Unknown_HD", 0x32b: "C: "}
    expect = {"seg10:0x20b1": "mov word ptr [bp - 0x24], 3",
              "seg10:0x2104": "cmp word ptr [bp - 0x24], 0x1a", "seg10:0x2110": "call far seg3:0x466",
              "seg10:0x211f": "push 0x319", "seg10:0x2133": "push 8",
              "seg10:0x2138": "call far seg3:0x4c2", "seg10:0x214c": "push 0x320",
              "seg10:0x216d": "push 0x32b", "seg10:0x2178": "sub al, 3",
              "seg10:0x2197": "cmp ax, 7", "seg10:0x219f": "cmp byte ptr [bp - 0x86], 0x2e",
              "seg10:0x21a9": "mov word ptr [bp - 0x26], 8", "seg10:0x20f2": "push 0x403",
              "seg10:0x21f7": "add ax, 0x1e", "seg10:0x21b4": "call far seg3:0x466"}
    assert _texts(isp, expect) == expect
    called = {i.text for i in isp.func_insns("seg10:0x1e7c") if i.mnem in ("call", "lcall")}
    assert not called & {"call far seg10:0x62f0", "call seg10:0x62f0", "call seg10:0x628e",
                         "call seg10:0x44cc"}


def _tdiv(a: int, b: int) -> int:
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b >= 0) else -q


def _w16(v: int) -> int:
    """The low 16 bits of v as a signed word."""
    return s16(v & 0xFFFF)


def _desk_slots(data: bytes, w: int, h: int) -> tuple[dict, dict]:
    """ispace.md ordinal 32: projection constants, layout solver and box slots, as documented."""
    hdr, body = data[:0x6C], data[0x80:]
    n = len(body) // 0x26
    hl = lambda o: struct.unpack_from("<i", hdr, o)[0]               # noqa: E731
    recs = []
    for i in range(n):
        b = body[i * 0x26:(i + 1) * 0x26]
        recs.append({"flag": b[0], "w": list(struct.unpack_from("<12h", b, 1)),
                     "p1": b[0x1B], "p2": b[0x1C], "col": struct.unpack_from("b", b, 0x1D)[0],
                     "typ": b[0x1E], "slot": struct.unpack_from("b", b, 0x1F)[0]})

    def mode(m, v):
        return {6: _tdiv(v * w, 1000), 7: _tdiv(v * h, 1000), 8: _w16(v)}[m]

    d28 = _tdiv(h << 2, 3)
    d2c, d2e = mode(hdr[0x50], hl(0x5C)), mode(hdr[0x51], hl(0x60))
    d4c, c6c = _tdiv(hl(0x64) * d28, 1000), _tdiv(hl(0x68) * h, 1000)
    d48 = _tdiv((c6c - d2e) * 2000, d28)
    dv = _tdiv(d28 - w, 2) + d2c or 1
    d44 = _tdiv((c6c - d2e) * 1000, dv) or 1
    consts = {"d28": d28, "d2c": d2c, "d2e": d2e, "d4c": d4c, "c6c": c6c, "d48": d48, "d44": d44}
    proj_s = lambda y: _tdiv(d4c * d28, _tdiv(y * d28, 1000) + d4c)   # noqa: E731
    proj_yb = lambda s: _tdiv(s * d48, 2000) + d2e                     # noqa: E731
    scale = {6: w, 7: h, 9: w, 10: h}            # SM_CXSCREEN, SM_CYSCREEN taken as W, H
    progress = True
    while progress:
        progress = False
        for r in recs[1:]:
            par = recs[r["p1"]]
            if r["flag"] or not par["flag"] or r["typ"] not in (1, 2, 3, 4, 5):
                continue
            if r["typ"] == 2 and not recs[r["p2"]]["flag"]:
                continue
            x = r["w"]
            for k in range(3):
                code, ref = x[6 + k], -1
                if code == 0:
                    ref = par["w"][3 + k]
                elif code == 5:
                    ref = recs[0]["w"][3]
                elif code < 6:
                    ref = par["w"][2 + code]
                if ref != -1:
                    x[k] = _w16(_tdiv(x[k] * ref, 1000) + par["w"][k])
                    if k == 0:
                        x[0] = _w16(x[0] - _tdiv(par["w"][3], 2))
                    if k == 2:
                        x[2] = _w16(x[2] + par["w"][5])
                c = x[9 + k]
                if r["typ"] == 2:
                    q = recs[r["p2"]]
                    ref = q["w"][3 + k] if c == 0 else recs[0]["w"][3] if c == 5 else q["w"][2 + c]
                    x[3 + k] = _w16(_tdiv(x[3 + k] * ref, 1000) + q["w"][k])
                    if k == 0:
                        x[3] = _w16(x[3] - _tdiv(q["w"][3], 2))
                    if k == 2:
                        x[5] = _w16(x[5] + q["w"][5])
                else:
                    ref = -1
                    if c == 0:
                        ref = par["w"][3]
                    elif k > 0 and c == 4:
                        ref = x[3]
                    elif c == 5:
                        ref = recs[0]["w"][3]
                    elif c < 6:
                        ref = par["w"][2 + c]
                    if ref != -1:
                        x[3 + k] = _w16(_tdiv(x[3 + k] * ref, 1000))
            s = proj_s(x[1])
            yb = proj_yb(s)
            for j, cj in ((0, 6), (2, 8)):
                c, v = x[cj], None
                if c in scale:
                    v = _tdiv(x[j] * scale[c], 1000)
                elif c == 8:
                    v = x[j]
                elif c > 10:
                    v = max(_tdiv(x[j] * h, 1000), c)
                if v is not None and j == 0:
                    x[0] = _w16(_tdiv((v - _tdiv((c6c - yb) * 1000, d44) - _tdiv(w - d28, 2)) * 1000, s))
                elif v is not None:
                    x[2] = _w16(_tdiv((yb - v) * 1000, s))
            for j, cj in ((3, 9), (5, 11)):
                c, v = x[cj], x[j]
                m = {6: v * w, 7: v * h, 8: v * 1000, 9: v * w, 10: v * h}.get(c)
                if c > 10:
                    m = max(v * h, c * 1000)
                if m is not None:
                    x[j] = _w16(_tdiv(m, s))
            r["flag"], progress = 1, True

    def face(x_, y_, sx, z, sz):
        s = proj_s(y_)
        yb = proj_yb(s)
        sxp, szp = _tdiv(sx * s, 1000), _tdiv(sz * s, 1000)
        centre = _tdiv((c6c - yb) * 1000, d44) + _tdiv(x_ * s, 1000) + _tdiv(w - d28, 2)
        return centre - _tdiv(sxp, 2), yb - szp - _tdiv(z * s, 1000), sxp, szp

    slots = {}
    for i, r in enumerate(recs):
        if i == 0 or r["typ"] != 1 or r["slot"] < 0:
            continue
        x = r["w"]
        left, top, wd, szp = face(x[0], x[1], x[3], x[2], x[5])
        if r["col"] >= 0 and left > face(x[0], x[1] + x[4], x[3], x[2], x[5])[0]:
            left, wd = left + 2, wd - 2
        slots[r["slot"]] = (i, left, top, left + wd, top + szp + 1)
    return consts, slots


def test_desk_projection_and_slots(isp):
    """ispace.md ordinal 32: the constants and the five slots of scene 100 at 1024x768."""
    expect = {"seg8:0x1b": "shl ax, 2", "seg8:0x1e": "mov cx, 3",
              "seg8:0x24": "mov word ptr [0xd28], ax", "seg8:0xd0": "cmp byte ptr es:[bx + 0x50], 6",
              "seg8:0xe1": "cmp byte ptr es:[bx + 0x50], 7", "seg8:0x109": "cmp byte ptr es:[bx + 0x50], 8",
              "seg8:0xd9": "push 0x3e8", "seg8:0x1f5": "mov cx, 0x7d0",
              "seg8:0x20e": "mov word ptr [0xd48], ax", "seg8:0x286": "mov cx, 0x3e8",
              "seg8:0x292": "mov word ptr [0xd44], ax", "seg8:0x29d": "mov word ptr [0xd44], 1",
              "seg8:0x26f": "mov word ptr [bp - 0xa], 1",
              # no depth sort: the identity order and the jump past the bubble sort
              "seg8:0x1b82": "mov byte ptr [bx + 0x1a3a], al", "seg8:0x1b9e": "jge seg8:0x1c0b",
              # the +2 shift whose width test compares a boolean with 45, 35, 20
              "seg8:0x3475": "cmp ax, 0x2d", "seg8:0x348e": "cmp ax, 0x23",
              "seg8:0x34a7": "cmp ax, 0x14", "seg8:0x34ac": "mov word ptr [bp + 0x24], 2"}
    assert _texts(isp, expect) == expect
    into = [i.addr for i in isp.insns
            if i.ops and re.fullmatch(r"seg8:0x[0-9a-f]+", i.ops[-1])
            and 0x1BAA <= A(i.ops[-1])[1] < 0x1C0B and not (i.seg == 8 and 0x1BAA <= i.off < 0x1C0B)]
    assert into == []                       # nothing outside the bubble sort branches into it
    assert "seg8:0x1c14" in {i.ops[-1] for i in isp.insns if i.seg == 8 and 0x1BAA <= i.off < 0x1C0B
                             and i.mnem == "call"}
    consts, slots = _desk_slots(_resource("IGRAPHIC.DL1", 6600, 100), 1024, 768)
    assert consts == {"d28": 1024, "d2c": 512, "d2e": 166, "d4c": 1024, "c6c": 0,
                      "d48": -324, "d44": -324}
    assert slots == {0: (42, 350, 22, 672, 253), 1: (17, 197, 435, 300, 684),
                     2: (5, 281, 268, 744, 328), 3: (18, 42, 345, 978, 360),
                     4: (60, 812, 267, 873, 329)}
    assert max(slots[2][4], slots[3][4]) == 360
    # the front-face shade of record 18 (white, brightness 40, Yb 40): grey 142
    assert ((_tdiv(_tdiv(2 * 255 * 40 // 50 * 12, 13) * (40 - 166), 0 - 166)) >> 1) == 142


def test_briefing_slide_in_constants(ins):
    """shell.md State 3: slide-in kinds, the 16-bit growth product, the icon label, phase 8/9 page."""
    expect = {"seg17:0x1b68": "mov word ptr [bp - 0x14], 7",
              "seg17:0x1b6d": "mov word ptr [bp - 0x10], 0x96",
              "seg17:0x1b72": "mov word ptr [bp - 0x12], 0x12",
              "seg17:0x1b77": "mov word ptr [bp - 0x18], 0x2c",
              "seg17:0x1bbd": "mov word ptr [bp - 0x14], 5",
              "seg17:0x1bc2": "mov word ptr [bp - 0x10], 0xbe",
              "seg17:0x1bc7": "mov word ptr [bp - 0x12], 0xa",
              "seg17:0x1bcc": "mov word ptr [bp - 0x18], 0x28",
              "seg17:0x1bd3": "mov word ptr [bp - 0x14], 0x16",
              "seg17:0x1bd8": "mov word ptr [bp - 0x10], 0x66",
              "seg17:0x1bdd": "mov word ptr [bp - 0x12], 9",
              "seg17:0x1be2": "mov word ptr [bp - 0x18], 0x26",
              "seg17:0x1bf2": "mov ax, word ptr [bx + 0x3d6e]",
              "seg17:0x1bf9": "mov word ptr [bp - 0x10], 0x70",
              "seg17:0x1bfe": "mov word ptr [bp - 0x12], 0x10",
              "seg17:0x1c08": "mov al, byte ptr [bx + 0x4d98]",
              "seg17:0x1c2e": "mov ax, word ptr es:[bx + 0x22]",
              "seg17:0x1c54": "mov dx, word ptr es:[bx + 0x24]",
              "seg17:0x1c3f": "imul word ptr [0x18ba]", "seg17:0x1c43": "mov dx, 0x37",
              "seg17:0x1c49": "idiv si",
              # icon label: Arial 12, TA_CENTER, 0x0200FF80, 35-byte records, (x + 32, y + 68)
              "seg17:0xc3b": "push 0xc", "seg17:0xc90": "push 6", "seg17:0xc9b": "push 0x200",
              "seg17:0xc9e": "push -0x80", "seg17:0xcb0": "imul ax, word ptr [bp + 6], 0x23",
              "seg17:0xcb4": "add ax, 0x57b4", "seg17:0xcc5": "add ax, 0x20", "seg17:0xccc": "add ax, 0x44",
              # phase 8/9 page: difficulty thresholds 1200 and 700
              "seg17:0x763d": "cmp word ptr [bx + 0x4d9a], 0x4b0",
              "seg17:0x7645": "mov word ptr [bp - 6], 0x81f",
              "seg17:0x7656": "cmp word ptr [bx + 0x4d9a], 0x2bc",
              "seg17:0x765e": "mov word ptr [bp - 6], 0x820",
              # page kind 402 goes to seg17:0x76e4
              "seg17:0x313a": "sub ax, 0x182", "seg17:0x313d": "je seg17:0x316b",
              "seg17:0x316c": "call seg17:0x76e4"}
    assert _texts(ins, expect) == expect
    assert 2 + 6 + 1 + 2 + 1 + 4 + 0x182 == 402
    assert (0x81F, 0x820, 0x4B0, 0x2BC) == (2079, 2080, 1200, 700)
    # the 16-bit growth product: kind 9 stops at 70, the others reach 2 * base at n = 110
    grow = lambda base, n: min(2 * base, s16(base * n & 0xFFFF) // 55)   # noqa: E731
    assert grow(38, 102) == 70 and all(grow(b, 110) == 2 * b for b in (44, 40))


def test_load_screen_constants(ins):
    """shell.md Game-start dialogs items 11-13: the 2000 ms window, greeting and Navigation."""
    expect = {"seg10:0x96": "add ax, 0x7d0", "seg10:0xa9": "mov word ptr [0x92e6], ax",
              "seg15:0x6658": "mov ax, word ptr [0x90ec]", "seg15:0x6660": "sub ax, 0xf0",
              "seg15:0x666d": "push 0x6f", "seg15:0x6691": "cmp word ptr [bp - 0x90], 3",
              "seg15:0x669c": "push 0x24e", "seg15:0x66c2": "push 0x137",
              "seg15:0x6749": "mov ax, word ptr [0x10]", "seg15:0x6751": "sub ax, 0x5a",
              "seg15:0x6760": "push -0x80", "seg15:0x6772": "push 0x285",
              "seg15:0x678e": "sub ax, 0x37", "seg15:0x67af": "push 0x3a8",
              "seg15:0x6809": "push 0xff00",
              "seg6:0x206d": "mov dword ptr es:[bx + 0xc], 0x15e0230",
              "seg6:0x201e": "add ax, 3", "seg6:0x2021": "cmp ax, 0xe",
              "seg6:0x2507": "push 0x41500c8", "seg6:0x2527": "push 0x50a30b53"}
    assert _texts(ins, expect) == expect
    assert (0x24E, 0x137, 0x285, 0x3A8, 0x15E, 0x230) == (590, 311, 645, 936, 350, 560)
    assert _sites(ins.writers(0x92E6, 0x92E7)) == ["seg9:0x27b3", "seg10:0xa9"]
    assert _sites([i for i in ins.refs(0x92E6, 0x92E7) if not is_write(i)]) == [
        "seg9:0x22e9", "seg9:0x2770", "seg9:0x27e4"]


def _weapon_table(ins: Module, edition: int) -> dict[int, int]:
    """Run the straight-line fill of seg10:0x1fb2 for DGROUP:0x920c = edition."""
    regs: dict[str, Optional[int]] = {}
    mem: dict[int, int] = {}
    flag_eq = False
    pc = A("seg10:0x1fbc")[1]
    end = A("seg10:0x2ff9")[1]
    while pc < end:
        i = ins.at(f"seg10:{pc:#x}")
        nxt = ins.step(i, 1).off
        m = i.mnem
        if m == "mov" and i.ops[0] in _FAMILY and _IMM_RE.fullmatch(i.ops[1]):
            regs[i.ops[0]] = int(i.ops[1], 0) & 0xFFFF
        elif m == "xor" and i.ops[0] == i.ops[1]:
            regs[i.ops[0]] = 0
        elif m == "mov" and i.abs_ds and i.mem.index == 0:
            src = i.ops[1]
            mem[i.mem.disp] = int(src, 0) & 0xFFFF if _IMM_RE.fullmatch(src) else regs.get(src)
        elif m == "cmp" and i.abs_ds and i.mem.disp == 0x920C:
            src = i.ops[1]
            v = int(src, 0) if _IMM_RE.fullmatch(src) else regs[src]
            flag_eq = edition == v
        elif m == "jne":
            pc = int(i.ops[0].split(":")[1], 16) if not flag_eq else nxt
            continue
        elif m == "jmp":
            pc = int(i.ops[0].split(":")[1], 16)
            continue
        pc = nxt
    out = {}
    for k in range(27):
        v = mem.get(0x2F8A + 0x50 * k)
        assert v is not None, k
        out[k] = s16(v)
        assert mem[0x2F54 + 0x50 * k] == 186 + k
    return out


def test_weapon_replacement_table(ins):
    """shell.md Plus content: field +0x36 of the 27 weapon records for both DGROUP:0x920c cases."""
    std = {k: v for k, v in _weapon_table(ins, 0).items() if v != -1}
    assert std == {1: 17, 2: 15, 3: 14, 6: 8, 9: 10, 12: 5, 13: 16, 18: 4, 19: 21, 20: 11}
    t3 = _weapon_table(ins, 3)
    assert {k for k, v in t3.items() if v == -1} == {0, 5, 7, 10} and t3[9] == 10
    assert {k for k, v in t3.items() if v == 5} == {1, 2, 3, 4, 15, 23, 24, 26}
    assert {k for k, v in t3.items() if v == 7} == {6, 8, 11, 14, 18, 19, 20, 21, 22, 25}
    assert {k for k, v in t3.items() if v == 10} == {9, 12, 13, 16, 17}


def test_new_shell_globals(ins):
    """globals.md: the writer and reader lists of DGROUP:0x1a, 0x92e4 and 0x951c."""
    vals = lambda lo: {i.addr: ins.stored_value(i) for i in ins.writers(lo, lo + 1)}  # noqa: E731
    assert vals(0x1A) == {"seg2:0xe07": 1, "seg2:0xe65": 0, "seg7:0x441": 0, "seg10:0x2e": 0,
                          "seg10:0x23c": 1, "seg11:0x7b5f": 0, "seg15:0x57fc": 0}
    assert _sites([i for i in ins.refs(0x1A, 0x1B) if not is_write(i)]) == [
        "seg7:0x10", "seg7:0x868", "seg7:0x9b3"]
    assert vals(0x92E4) == {"seg6:0x98c4": 1, "seg9:0x698f": 0, "seg9:0x6ceb": 0, "seg10:0x9d28": 1,
                            "seg10:0x9da5": 0, "seg11:0x7a6c": 0, "seg16:0x1c3b": 1,
                            "seg16:0x1c46": 0, "seg18:0x5aa": 1, "seg18:0x724": 0,
                            "seg18:0x351c": 1, "seg18:0x354a": 0}
    assert _sites(ins.writers(0x951C, 0x951D)) == ["seg6:0x98bb", "seg10:0x850b", "seg11:0x6f00"]
    assert _sites([i for i in ins.refs(0x951C, 0x951D) if not is_write(i)]) == [
        "seg11:0x535", "seg11:0x6d58", "seg11:0x6ef7"]
    expect = {"seg11:0x6ee7": "cmp word ptr [0x920c], 2", "seg11:0x6eee": "cmp word ptr [0x920c], -1",
              "seg11:0x6ef7": "cmp word ptr [0x951c], 1", "seg11:0x6f0c": "push 0x49",
              "seg11:0x6f14": "mov dword ptr [0x5018], 0xffffffff",
              "seg2:0x306": "sub ax, 0x10b", "seg2:0x309": "je seg2:0x5cb",
              "seg2:0x30f": "je seg2:0x56d", "seg2:0x5cb": "cmp word ptr [0x920e], 0"}
    assert _texts(ins, expect) == expect
    assert 0x10B + 0x204 == 0x30F and 0x30F + 2 == 0x311


def test_registration_strings_have_no_user(ins, isp):
    """shell.md Editions: the only pushes of 0x84a/0x84b are DGROUP string pointers, not ids."""
    hits = {(m.name, i.addr) for m in (ins, isp) for i in m.insns
            if i.mnem == "push" and i.ops and i.ops[0] in ("0x84a", "0x84b")}
    assert hits == {("inspace", "seg10:0x8485"), ("ispace", "seg9:0x17f5")}
    assert _c_string(unpacked_seg("inspace", 19), 0x84A) == "ThePlayer"
    assert _c_string(unpacked_seg("ispace", 11), 0x84B) == "Size"


# ------------------------------------------------------------------ ship previews, Order logo, desk launch


def _calls(m: Module, expect: dict[str, str]) -> dict[str, bool]:
    """Each call site in ``expect`` names the import or target given."""
    return {a: name in m.at(a).text and m.at(a).mnem in ("call", "lcall") for a, name in expect.items()}


def test_levclass_frame(isp):
    """shell.md Ship previews: the LevClass frame of ISPACE.DLL seg10:0x309c and its net pixels
    on the 96 x 96 client."""
    calls = {"seg10:0x3122": "SetRect", "seg10:0x3137": "FillRect", "seg10:0x3151": "SetRect",
             "seg10:0x3166": "FillRect", "seg10:0x3183": "SetRect", "seg10:0x3198": "FillRect",
             "seg10:0x31b6": "SetRect", "seg10:0x31cb": "FillRect", "seg10:0x31ee": "SetPixel",
             "seg10:0x320b": "SetPixel", "seg10:0x322c": "SetPixel", "seg10:0x324c": "SetPixel",
             "seg10:0x3268": "SelectObject", "seg10:0x327b": "MoveTo", "seg10:0x3289": "LineTo",
             "seg10:0x3299": "LineTo", "seg10:0x32a9": "SelectObject", "seg10:0x32bb": "LineTo",
             "seg10:0x32cd": "LineTo", "seg10:0x32d8": "SelectObject"}
    assert all(_calls(isp, calls).values())
    expect = {"seg10:0x311b": "add ax, 3", "seg10:0x312f": "push 1", "seg10:0x314d": "add ax, 3",
              "seg10:0x315e": "push 1", "seg10:0x3179": "sub ax, 3", "seg10:0x3190": "push 3",
              "seg10:0x31a5": "sub ax, 3", "seg10:0x31c3": "push 3",
              "seg10:0x31e8": "push 0xc0", "seg10:0x31eb": "push 0xc0c0",
              "seg10:0x3205": "push 0x80", "seg10:0x3208": "push 0x8080",
              "seg10:0x3226": "push 0xc0", "seg10:0x3229": "push 0xc0c0",
              "seg10:0x3248": "push 0", "seg10:0x324a": "push 0",
              "seg10:0x3254": "cmp word ptr [bp - 0x38], 3", "seg10:0x3260": "push 6",
              "seg10:0x32a1": "push 7"}
    assert _texts(isp, expect) == expect
    # The documented drawing list, rasterised (LineTo leaves out its end point).
    L, T, R, B = 0, 0, 96, 96
    px = {}
    for (l, t, r, b), c in [((L, T, L + 3, B), "c0c0c0"), ((L, T, R, T + 3), "c0c0c0"),
                            ((L + 2, B - 3, R, B), "404040"), ((R - 3, T + 1, R, B - 1), "404040")]:
        px.update({(x, y): c for x in range(l, r) for y in range(t, b)})
    for i in range(3):
        px.update({(R - 2 - i, T + i): "c0c0c0", (R - 1 - i, T + i): "808080",
                   (L + 2 - i, B - 3 + i): "c0c0c0", (L + 3 - i, B - 3 + i): "000000"})

    def line(a, b, c):
        (x0, y0), (x1, y1) = a, b
        n = max(abs(x1 - x0), abs(y1 - y0))
        for k in range(n):
            px[(x0 + (x1 - x0) * k // n, y0 + (y1 - y0) * k // n)] = c
        return b

    p = line((L, B - 1), (L, T), "ffffff")
    p = line(p, (R - 1, T), "ffffff")
    p = line(p, (R - 1, B - 1), "000000")
    line(p, (L - 1, B - 1), "000000")
    assert all(px[(0, y)] == "ffffff" for y in range(95)) and all(px[(x, 0)] == "ffffff" for x in range(95))
    assert all(px[(95, y)] == "000000" and px[(y, 95)] == "000000" for y in range(96))


def test_preview_slot_paint_and_stretch(isp):
    """shell.md Ship previews and ispace.md ordinal 10: seg10:0x2c68 and the stretch helper
    seg10:0x2d3c."""
    expect = {"seg10:0x2cc7": "push 3", "seg10:0x2cc9": "push 3", "seg10:0x2ccb": "push 0x5a",
              "seg10:0x2ccd": "push 0x5a", "seg10:0x2ccf": "push 0", "seg10:0x2cd1": "push 0x42",
              "seg10:0x2ce1": "push 0xcc", "seg10:0x2ce4": "push 0x20",
              "seg10:0x2cf7": "add ax, ax", "seg10:0x2cfb": "mov ax, 0x30",
              "seg10:0x2cfe": "sub ax, word ptr [bp - 8]",
              "seg10:0x2d51": "push 2", "seg10:0x2d53": "push 2",
              "seg10:0x2d75": "cmp cx, 8", "seg10:0x2d87": "cmp word ptr [0x1f7e], 0",
              "seg10:0x2d97": "add ax, ax", "seg10:0x2da1": "add ax, ax"}
    assert _texts(isp, expect) == expect
    assert all(_calls(isp, {"seg10:0x2c8e": "GetDC", "seg10:0x2cd5": "PatBlt",
                            "seg10:0x2d09": "seg10:0x2d3c", "seg10:0x2d2e": "ReleaseDC",
                            "seg10:0x2d55": "CreateCompatibleBitmap", "seg10:0x2d65": "GetObject",
                            "seg10:0x2de4": "seg10:0x2e1a",
                            "seg10:0x2e0d": "StretchBlt"}).values())
    assert isp.call_sites("seg10:0x2d3c") == ["seg10:0x2d09"]
    assert isp.call_sites("seg10:0x2c68") == ["seg10:0x2c5b", "seg10:0x3311"]
    assert 0x5A == 90 and 0x30 == 48


def test_preview_bitmap_builder(isp):
    """ispace.md ordinal 10 Ship previews: seg10:0x2aac and the hdc slot DGROUP:0x2012."""
    expect = {"seg10:0x2aba": "cmp word ptr [bp + 8], 2", "seg10:0x2ac5": "mov word ptr [bp - 4], 2",
              "seg10:0x2b26": "cmp word ptr [bp + 6], -1",
              "seg10:0x2b32": "imul bx, word ptr [bp + 6], 0x41",
              "seg10:0x2b4a": "imul ax, word ptr es:[bx + 0x21], 0x2c",
              "seg10:0x2b59": "add ax, 0x23", "seg10:0x2b85": "push 0x2014",
              "seg10:0x2bd0": "mov ax, word ptr [0x2018]", "seg10:0x2bd3": "mov word ptr [0x1f88], ax",
              "seg10:0x2bf8": "mov al, byte ptr es:[bx + si + 0x1a]",
              "seg10:0x2c0b": "cmp word ptr [bp + 0xa], 2", "seg10:0x2c29": "push 0x42",
              "seg10:0x2c30": "sub word ptr [bp + 0xa], 2", "seg10:0x2c50": "push 0x42"}
    assert _texts(isp, expect) == expect
    assert all(_calls(isp, {"seg10:0x2b70": "seg5:0x0", "seg10:0x2b8a": "GetObject",
                            "seg10:0x2b93": "CreateCompatibleDC", "seg10:0x2c2b": "PatBlt",
                            "seg10:0x2c52": "PatBlt", "seg10:0x2c5b": "seg10:0x2c68"}).values())
    assert _sites(isp.writers(0x2012, 0x2013)) == ["seg10:0x42f8"]
    assert _sites([i for i in isp.refs(0x2012, 0x2013) if not is_write(i)]) == ["seg10:0x2b8f"]
    assert isp.call_sites("seg10:0x2aac") == ["seg10:0x19e2", "seg10:0x1b0a", "seg10:0x1c57",
                                              "seg10:0x24bd", "seg10:0x29db", "seg10:0x2a90",
                                              "seg10:0x3a83"]
    assert [_cdecl_pushes(isp, s, 4)[3] for s in ("seg10:0x1b0a", "seg10:0x2a90", "seg10:0x3a83")] \
        == ["0", "0", "0"]
    assert isp.at("seg10:0x1b02").text == "mov ax, 0xffff"
    assert isp.at("seg10:0x1c49").text == "mov al, byte ptr es:[bx + 0x170]"


def test_preview_transformer(master, isp):
    """shell.md Ship previews: the oracle's TRANSFORMER preview, its body graphic and the scheme
    rows that recolour it."""
    bod, shp = master
    r = shp[65:130]
    assert (_cstr(r, 0x0A), r[7], r[8], struct.unpack_from("<H", r, 0x21)[0]) == \
        (b"TRANSFORMER", 0, 10, 33)
    assert struct.unpack_from("<H", bod, 33 * 44 + 0x21)[0] == 60
    assert _dib(_resource("IGRAPHIC.DL1", 4000, 60))[:3] == (132, 33, 4)
    assert (48 - 33, 2 * 33, 96 * 96) == (15, 66, 9216)
    mem = _emulate_5846(isp)
    row0 = [mem[0x117C + 3 * k:0x117C + 3 * k + 3].hex() for k in range(10)]
    row10 = [mem[0x1CDC + 150 + 3 * k:0x1CDC + 150 + 3 * k + 3].hex() for k in range(5)]
    assert (row0[0], row0[1], row0[9]) == ("ffffff", "e6e6e8", "1e1e28")
    assert (row10[0], row10[4]) == ("c8ff00", "004b00")


def test_order_logo(isp):
    """shell.md Order: the logo window, graphic 300, the mask patch and the two blits."""
    expect = {"seg10:0x289": "push 5", "seg10:0x290": "push 1", "seg10:0x2a3": "push 0x5000",
              "seg10:0x2ae": "sub ax, 0xa1", "seg10:0x2b2": "push 0x46", "seg10:0x2b4": "push 0x92",
              "seg10:0x2b7": "push 0x72", "seg10:0x2bc": "push 0x64",
              "seg10:0x30e": "cmp word ptr [0x1584], 0", "seg10:0x31f": "push 0x12c",
              "seg10:0x327": "push 3", "seg10:0x329": "push -1", "seg10:0x3ad": "push 4",
              "seg10:0x3c0": "push 0x7d", "seg10:0x3c2": "push 0x1d", "seg10:0x3c4": "push 2",
              "seg10:0x3c6": "push 2", "seg10:0x3c8": "push 0xf0", "seg10:0x3cb": "push 0x21",
              "seg10:0x3d5": "push 0x23", "seg10:0x3d7": "push 0x2f",
              "seg10:0x3e5": "push 0x69", "seg10:0x3e7": "push 0x30",
              "seg10:0x400": "push 4", "seg10:0x402": "push 4", "seg10:0x404": "push 0x8a",
              "seg10:0x407": "push 0x6a", "seg10:0x410": "push 0x88", "seg10:0x413": "push 0xc6",
              "seg10:0x42e": "push 0xee", "seg10:0x431": "push 0x86"}
    assert _texts(isp, expect) == expect
    assert all(_calls(isp, {"seg10:0x28b": "seg5:0x446e", "seg10:0x293": "seg10:0x4250",
                            "seg10:0x2c6": "CreateWindow", "seg10:0x32b": "seg5:0x402",
                            "seg10:0x341": "seg5:0x650", "seg10:0x3cd": "PatBlt",
                            "seg10:0x3dd": "FloodFill", "seg10:0x3ed": "FloodFill",
                            "seg10:0x416": "BitBlt", "seg10:0x434": "BitBlt"}).values())
    assert (0xA1, 0x46, 0x92, 0x72, 0x7D, 0x1D, 0x23, 0x2F, 0x69, 0x30, 0x8A, 0x6A) == \
        (161, 70, 146, 114, 125, 29, 35, 47, 105, 48, 138, 106)
    raw = _resource("ISPACE.dll", 4000, 300)
    w, h, bpp, table = _dib(raw)
    off, stride = struct.unpack_from("<I", raw, 10)[0], (w * bpp + 31) // 32 * 4
    black = {k for k, c in enumerate(table) if c == "000000"}

    def pix(x, y):  # top-down y; the DIB is bottom-up
        b = raw[off + (h - 1 - y) * stride + x // 2]
        return b >> 4 if x % 2 == 0 else b & 15

    assert (w, h) == (138, 138)
    assert all(pix(x, y) in black for y in range(106, 138) for x in range(w))


def _launch_heights(s: int) -> list[tuple[int, int]]:
    step = _trunc(4 * s + _trunc(2 * s * -27, 100), 15)
    w, out = 4 * s, [(4 * s, 4 * s)]
    for i in range(1, 16):
        p = 95 - 6 * (i - 1) if i < 10 else 41 + 13 * (i - 10)
        w -= step
        out.append((w, _trunc(w * p, 100)))
    return out


def test_launch_ship_frames(ins):
    """shell.md Game-start dialogs item 7: seg15:0x9e6's 16 frame sizes."""
    expect = {"seg15:0x9f4": "cmp word ptr [0x90ee], 8", "seg15:0x9fb": "mov word ptr [bp - 0x12], 4",
              "seg15:0xa02": "mov word ptr [bp - 0x12], 2", "seg15:0xa35": "mov word ptr [0x92da], ax",
              "seg15:0xa49": "shl cx, 2", "seg15:0xa54": "imul ax, ax, -0x1b",
              "seg15:0xa57": "mov dx, 0x64", "seg15:0xa61": "mov dx, 0xf",
              "seg15:0xa6f": "mov word ptr [0x3ce8], 0", "seg15:0xa75": "mov word ptr [bp - 0x10], 0x5f",
              "seg15:0xa84": "sub word ptr [bp - 0x10], 6", "seg15:0xa88": "cmp word ptr [bp - 0xa], 0xa",
              "seg15:0xac0": "add word ptr [bp - 0x10], 0xd",
              "seg15:0xac4": "cmp word ptr [bp - 0xa], 0x10", "seg15:0xbec": "push 3",
              "seg15:0xbfd": "push 0xcc", "seg15:0xc00": "push 0x20",
              "seg15:0xc56": "mov word ptr [0x3cce], ax"}
    assert _texts(ins, expect) == expect
    assert all(_calls(ins, {"seg15:0xbee": "SetStretchBltMode", "seg15:0xc2e": "seg10:0x9962",
                            "seg15:0xc50": "seg10:0x6ec6", "seg15:0xa16": "seg15:0x2a08"}).values())
    sizes = _launch_heights(33)
    assert [w for w, _ in sizes] == [132 - 7 * i for i in range(16)]
    heights = ", ".join(str(hh) for _, hh in sizes)
    text = (REPO_ROOT / "docs" / "re" / "shell.md").read_text()
    assert heights.rsplit(", ", 1)[0] + " and " + heights.rsplit(", ", 1)[1] in text
    assert sizes[-1] == (27, 28)
    assert _sites(ins.writers(0x92DA, 0x92DB)) == ["seg15:0xa35"]


def _launch_calls(y0: int, cal: int, ye: int = 60) -> int:
    pos, f, n = 10 * y0, 0, 0
    while True:
        n += 1
        if f < 10:
            f = _trunc(y0 - _trunc(pos, 10), max(1, _trunc(_trunc(y0 - ye, 2), 10)))
        else:
            f = _trunc(_trunc(ye - y0, 2) - _trunc(pos, 10) + y0, max(1, _trunc(_trunc(y0 - ye, 2), 6))) + 10
        f = min(f, 15)
        speed = max(9, 150 - (4 * f if cal >= 1400 else _trunc(f, 2)))
        pos += -_trunc(speed * (3 if cal >= 1400 else 5), 7)
        if 10 * ye > pos:
            return n


def test_launch_step_rule(ins):
    """shell.md Game-start dialogs item 9: seg15:0x0's step constants and the call counts."""
    expect = {"seg15:0x14f": "imul ax, word ptr [0x92d8], 0xa", "seg15:0x191": "cmp ax, 0xa",
              "seg15:0x199": "mov cx, 0xfff6", "seg15:0x1b2": "mov cx, 0xa",
              "seg15:0x1bf": "mov word ptr [0x17c6], 1", "seg15:0x1fb": "mov cx, 6",
              "seg15:0x216": "add ax, 0xa", "seg15:0x219": "cmp ax, 0xf",
              "seg15:0x2c4": "cmp word ptr [0x8ffc], 0x578", "seg15:0x2d9": "shl ax, 2",
              "seg15:0x2dc": "sub ax, 0x96", "seg15:0x2e4": "cmp ax, 9",
              "seg15:0x5a0": "imul ax, word ptr [0x17c8], 5", "seg15:0x5a7": "imul ax, word ptr [0x17c8], 3",
              "seg15:0x5ac": "mov cx, 0xfff9", "seg15:0x5b6": "imul ax, word ptr [0x92d6], 0xa"}
    assert _texts(ins, expect) == expect
    assert all(_calls(ins, {"seg15:0x163": "seg15:0x3bfa", "seg15:0x58d": "BitBlt"}).values())
    assert (_launch_calls(574, 13625), _launch_calls(574, 1000)) == (108, 50)


def test_play_height_and_pad(ins, dgroup):
    """globals.md DGROUP:0x90ec and shell.md item 7: the play height and the pad at 1024 x 768."""
    assert _sites(ins.writers(0x90EC, 0x90ED)) == ["seg2:0xb34", "seg2:0xb3d", "seg2:0xc05",
                                                   "seg2:0x1b29"]
    assert len([i for i in ins.refs(0x90EC, 0x90ED) if not is_write(i)]) == 95
    expect = {"seg2:0xb37": "mov word ptr [0x10], ax", "seg2:0xb3d": "sub word ptr [0x90ec], ax",
              "seg2:0xbfc": "mov word ptr [0xa70], 0x55", "seg2:0x1b25": "sub ax, word ptr [0xa70]",
              "seg15:0x484b": "sub ax, 0x44c", "seg15:0x485e": "mov dx, 0x24",
              "seg15:0x486a": "add ax, 0xa4", "seg15:0x48af": "mov cx, 0x1e",
              "seg15:0x48bc": "add ax, 0x50"}
    assert _texts(ins, expect) == expect
    assert struct.unpack_from("<H", dgroup, 0xA70)[0] == 0x55
    k = _trunc(4 * (768 - 85), 3) - 1100
    assert (k, _trunc(k, 36) + 164, _trunc(k, 30) + 80, _trunc(k, 30) + 80 + 52) == (-190, 159, 74, 126)


def test_state1_repaint_dispatch(ins):
    """shell.md State-1 repaint: case 1 of seg11:0xafe's state dispatch."""
    expect = {"seg11:0x15b9": "cmp word ptr [0x951a], 0", "seg11:0x15c2": "cmp word ptr [0x9130], 0",
              "seg11:0x15d7": "mov word ptr es:[bx + 0x1c], 1", "seg11:0x15ea": "mov word ptr [0x58c], 1"}
    assert _texts(ins, expect) == expect
    assert all(_calls(ins, {"seg11:0x15e0": "seg7:0x11cc", "seg11:0x15e5": "seg7:0x1208",
                            "seg11:0x15f0": "seg7:0x1f38", "seg11:0x15f6": "seg11:0x33cc"}).values())


# ------------------------------------------------------------------ sound.md


SOUNDS_TSV = REPO_ROOT / "re" / "artifacts" / "sounds.tsv"
SLOT_FLAGS = {0: [0, 3, 6, 8, 0x11, 0x12, 0x23, 0x24, 0x25, 0x27, 0x45], 1: [0x26],
              2: [1, 2, 4, 7, *range(0x13, 0x23), 0x29, 0x2B, 0x41, 0x46],
              3: [0x2A, *range(0x2C, 0x41), 0x44, 0x47, 0x48, 0x49], 4: [0x28],
              6: [5, *range(9, 0x11), 0x42, 0x4A, 0x4E], 7: [0x43, 0x4B, 0x4C, 0x4D, 0x4F, 0x50, 0x51]}
HOLDS = {0x26: 900, 0x2C: 730, 0x2D: 900, 0x2E: 1000, 0x2F: 900, 0x30: 600, 0x31: 1300, 0x32: 1100,
         0x33: 1250, 0x34: 510, 0x35: 300, 0x36: 700, 0x37: 540, 0x38: 300, 0x3D: 350, 0x3E: 640,
         0x3F: 750, 0x40: 520, 0x47: 660, 0x48: 960, 0x49: 700}
MISSING_WAVS = {112, 142, 165, 181}


def _flag_of(isp: Module) -> dict[int, int]:
    """Slot id -> flag byte from the 82-entry switch ISPACE.DLL seg9:0x265b."""
    sw = isp.switches["seg9:0x265b"]
    assert (sw["table"], sw["count"]) == ("seg9:0x2660", 82)
    out = {}
    for sid, t in enumerate(sw["targets"]):
        if t == "seg9:0x2704":                      # the preset 2 of seg9:0x25fa stays
            out[sid] = 2
            continue
        store = isp.step(isp.at(t), 1)
        assert store.text.startswith("mov byte ptr [bx + 0x2224], "), store.text
        out[sid] = int(store.ops[1], 0)
    return out


def test_sound_slot_flag_table(isp):
    """sound.md Slots: the flag byte of each of the 82 slots, the preset and the bound."""
    assert isp.at("seg9:0x25fa").text == "mov byte ptr [bx + 0x2224], 2"
    assert isp.at("seg9:0x2650").text == "cmp ax, 0x51"
    flags = _flag_of(isp)
    assert {v: sorted(k for k in flags if flags[k] == v) for v in set(flags.values())} == SLOT_FLAGS
    assert [s for s, t in enumerate(isp.switches["seg9:0x265b"]["targets"]) if t == "seg9:0x2704"] == [0x21, 0x41]


def test_sound_resource_rule_and_purge(isp):
    """sound.md Slots: FindResource(module[flag & 1], id + 100, 100) on both paths; purge tests."""
    for call, add in (("seg9:0x2780", "seg9:0x2774"), ("seg9:0x27e3", "seg9:0x27d7")):
        assert "KERNEL.60 FindResource" in isp.at(call).text
        assert isp.at(add).text == "add ax, 0x64"
        assert isp.step(isp.at(call), -2).text == "push 0x64"
    assert isp.at("seg9:0x276d").text == "push word ptr [bx + 0x68c]"


def _wav(data: bytes) -> tuple[int, int, int, int, int]:
    """(format tag, channels, rate, bits, data bytes) of a RIFF WAVE image."""
    assert data[:4] == b"RIFF" and data[8:12] == b"WAVE"
    p, fmt, size = 12, None, None
    while p + 8 <= len(data):
        cid, n = data[p:p + 4], struct.unpack_from("<I", data, p + 4)[0]
        if cid == b"fmt ":
            tag, ch, rate, _br, _ba, bits = struct.unpack_from("<HHIIHH", data, p + 8)
            fmt = (tag, ch, rate, bits)
        elif cid == b"data":
            size = n
        p += 8 + n + (n & 1)
    return (*fmt, size)


@functools.lru_cache(maxsize=None)
def _wavs() -> dict[int, tuple[str, tuple]]:
    """Resource id -> (module, WAV format) for both FX DLLs."""
    out = {}
    for name, label in (("SND_FX.DLL", "SND_FX.DLL"), ("eng_fx.dll", "ENG_FX.DLL")):
        m = _ne(name)
        assert {r.type_id for r in m.resources} == {100}
        for r in m.resources:
            assert r.id not in out
            out[r.id] = (label, _wav(m.data[r.file_off:r.file_off + r.length]))
    return out


def test_sound_wav_resources():
    """sound.md Slots and resources.md #100: 46 + 32 WAVs, all PCM 8-bit mono 11025 Hz, four missing."""
    w = _wavs()
    snd = sorted(k for k, (mod, _f) in w.items() if mod == "SND_FX.DLL")
    eng = sorted(k for k, (mod, _f) in w.items() if mod == "ENG_FX.DLL")
    assert snd == [*range(100, 112), *range(113, 138), 139, 140, 141, 143, 166, 169, 170, 174, 178]
    assert eng == [138, *range(144, 165), 167, 168, 171, 172, 173, 175, 176, 177, 179, 180]
    assert (len(snd), len(eng)) == (46, 32)
    assert set(range(100, 182)) - set(w) == MISSING_WAVS
    assert {f[:4] for _m, f in w.values()} == {(1, 1, 11025, 8)}


def test_sounds_tsv_matches_the_bytes(isp):
    """re/artifacts/sounds.tsv: module, resource, flag, length and hold of every slot row."""
    import csv

    rows = {r["id"]: r for r in csv.DictReader(SOUNDS_TSV.open(newline=""), delimiter="\t")}
    special = ["-1", *(f"{i:#04x}" for i in range(0x64, 0x77))]
    assert list(rows) == [f"{i:#04x}" for i in range(0x52)] + special
    flags, w = _flag_of(isp), _wavs()
    for sid in range(0x52):
        r = rows[f"{sid:#04x}"]
        res = sid + 100
        mod = "ENG_FX.DLL" if flags[sid] & 1 else "SND_FX.DLL"
        assert (r["module"], r["resource"], r["flag"]) == (mod, str(res), str(flags[sid])), sid
        if res in MISSING_WAVS:
            assert r["ms"] == "absent" and not r["same_as"]
        else:
            assert w[res][0] == mod
            assert r["ms"] == str(w[res][1][4] * 1000 // 11025), sid
        assert r["hold"] == (str(HOLDS[sid]) if sid in HOLDS else ""), sid
        assert bool(r["transcript"]) == (mod == "ENG_FX.DLL" and res not in MISSING_WAVS), sid
    for sid in special:
        assert not any(rows[sid][k] for k in ("module", "resource", "flag", "ms", "hold"))


def test_sound_hold_table(isp):
    """sound.md Playing: the 36-entry hold switch at ISPACE.DLL seg9:0xf93 (ids 0x26-0x49)."""
    sw = isp.switches["seg9:0xf93"]
    assert (sw["table"], sw["count"]) == ("seg9:0xf98", 36)
    holds = {}
    for k, t in enumerate(sw["targets"]):
        if t == "seg9:0xfe0":
            continue
        add = isp.step(isp.at(t), 2)
        assert add.mnem == "add" and add.ops[0] == "ax", add.text
        holds[0x26 + k] = int(add.ops[1], 0)
    assert holds == HOLDS
    assert isp.at("seg9:0xead").text == "cmp word ptr [bp + 0x16], 0"


def test_ordinal9_remaps_and_loop_words(isp):
    """sound.md Playing: the id remaps and the loop words of ordinal 9."""
    expect = {"seg9:0xdb6": "mov word ptr [bp + 0x1c], 0xb",
              "seg9:0xd87": "mov word ptr [bp + 0x1c], 0x23",
              "seg9:0xcd5": "add dx, 0x30",
              "seg9:0xd45": "mov word ptr [bp + 0x1c], 0x47",
              "seg9:0xd75": "mov word ptr [bp + 0x1c], 0x2e",
              "seg9:0xdbe": "mov word ptr [bp - 0x90], 7", "seg9:0xdc4": "mov word ptr [bp + 0x1c], 0x46",
              "seg9:0xdcc": "mov word ptr [bp - 0x90], 8", "seg9:0xdd2": "mov word ptr [bp + 0x1c], 0x4a",
              "seg9:0xdda": "mov ax, 0x14",
              "seg9:0xde7": "mov word ptr [bp - 0x90], 0x14", "seg9:0xded": "mov word ptr [bp + 0x1c], 0x17",
              "seg9:0xdf5": "mov word ptr [bp + 0x1c], 0x11",
              "seg9:0xd96": "mov word ptr [0x21a0], 0", "seg9:0xd9c": "mov word ptr [0x21a2], 8",
              "seg9:0xe63": "cmp word ptr [bp + 0x18], 0xd499",
              "seg9:0x106d": "add ax, 0x4b0"}
    assert _texts(isp, expect) == expect


def test_sound_play_flags(isp):
    """sound.md Playing and Session: sndPlaySound flags, MIXPLAYPARAMS, MIXCONFIG, the channel open."""
    expect = {"seg9:0x1311": "mov word ptr [bp - 0x12], 3", "seg9:0x131e": "mov word ptr [bp - 0x12], 0xf",
              "seg9:0x1325": "mov word ptr [bp - 0x12], 7",
              "seg9:0x1383": "mov word ptr [0x2148], 0x12", "seg9:0x1395": "mov word ptr [0x2154], 5",
              "seg9:0x139b": "mov word ptr [0x2156], 0", "seg9:0x13a4": "mov word ptr [0x2158], ax",
              "seg9:0x5b9": "mov word ptr [0x171c], 0x14", "seg9:0x5cf": "mov word ptr [0x213e], 0xa",
              "seg9:0x5d5": "mov word ptr [0x2140], 1", "seg9:0x5db": "mov word ptr [0x2142], 0",
              "seg9:0x5e1": "mov word ptr [0x2144], 1", "seg9:0x5fd": "push 2",
              "seg9:0x60e": "push 1", "seg9:0x614": "mov word ptr [0x2222], 1",
              "seg9:0x11a3": "push 1"}
    assert _texts(isp, expect) == expect
    assert all(_calls(isp, {"seg9:0x1333": "sndPlaySound", "seg9:0x11e8": "sndPlaySound"}).values())
    for site, ptr in (("seg9:0x5c3", 0x17CC), ("seg9:0x5eb", 0x17D0), ("seg9:0x5ff", 0x19D0),
                      ("seg9:0x610", 0x17D4), ("seg9:0x13be", 0x19D4), ("seg9:0x11a5", 0x19E0)):
        assert isp.at(site).text == f"lcall [{ptr:#x}]"


def _profile_ints(m: Module, func: str) -> list[tuple[str, int, int]]:
    """(key, default, store) of each GetPrivateProfileInt call in ``func``; store is the DGROUP
    word the result goes to, or -1 for a local."""
    dg = unpacked_seg("ispace", 11)
    out = []
    for i in m.func_insns(func):
        if "GetPrivateProfileInt" not in i.text:
            continue
        key, dflt, store = m.step(i, -4), m.step(i, -3), m.step(i, 1)
        target = store.mem.disp if store.mnem == "mov" and store.abs_ds else -1
        imm = _IMM_RE.fullmatch(dflt.ops[0])
        value = s16(int(dflt.ops[0], 0) & 0xFFFF) if imm else None      # None: a computed default
        if not _IMM_RE.fullmatch(key.ops[0]):
            continue                                    # a computed key (JBtn<n>)
        out.append((_cstr(dg, int(key.ops[0], 0)).decode(), value, target))
    return out


def test_sound_ini_keys(isp):
    """sound.md Settings: keys, defaults and DGROUP words of the sound INI reads."""
    reads = _profile_ints(isp, "seg9:0x22ca")
    assert reads[:8] == [("SoundFX", 1, 0x694), ("VoiceFX", 1, 0x696), ("Music", 1, 0x221E),
                         ("SoundVolume", 100, 0x69E), ("FXMusic", 50, 0x6A2), ("StereoEffect", 2, 0x228C),
                         ("PurgeSound", 1, 0x698), ("StereoSwap", 0, 0x69A)]
    assert ("AllowSound", 0, -1) in reads and ("SoundSystem", -1, 0x2280) in reads
    assert ("MixChannels", None, 0x219E) in _profile_ints(isp, "seg9:0x4e0")
    assert isp.at("seg9:0x55d").text == "mov word ptr [bp - 6], 4"         # its default
    every = [k for f in isp.funcs for k in _profile_ints(isp, f)]
    assert ("SaverVolume", 0, 0x69E) in every
    assert any(k == "ForceVol" and d == 0 for k, d, _s in every)
    # the device test: SYNC 0x10, VOLUME 4, LRVOLUME 8; MIDI VOLUME 1, LRVOLUME 2
    expect = {"seg9:0x244c": "test byte ptr [bp - 6], 0x10", "seg9:0x250a": "test al, 4",
              "seg9:0x250e": "test byte ptr [bp - 6], 8", "seg9:0x24e7": "test al, 1",
              "seg9:0x24eb": "test byte ptr [bp - 0x36], 2", "seg9:0x2422": "mov word ptr [0x227e], ax",
              "seg9:0x25b7": "jle seg9:0x25bc"}
    assert _texts(isp, expect) == expect


MCI_SITES = {"seg9:0x381b": ("0x803", "0x2200"), "seg9:0x3851": ("0x814", "0x100"),
             "seg9:0x38bb": ("0x80d", "0x400"), "seg9:0x3a2f": ("0x804", "2"),
             "seg9:0x3a80": ("0x809", "0"), "seg9:0x3b2f": ("0x807", "0xa"),
             "seg9:0x3b4d": ("0x807", "9"), "seg9:0x3bc1": ("0x806", "9")}


def test_mci_commands(isp):
    """sound.md Music: the eight mciSendCommand calls, their messages and flags; no MCI_STOP."""
    sites = [i.addr for i in isp.insns if "mciSendCommand" in i.text and i.mnem in ("call", "lcall")]
    assert sites == list(MCI_SITES)
    for site, (msg, flags) in MCI_SITES.items():
        window = [isp.step(isp.at(site), -k) for k in range(1, 9)]
        assert any(w.text == f"push {msg}" for w in window), site
        if site == "seg9:0x3bc1":
            assert isp.at("seg9:0x3bb7").text == "mov ax, 9"
        else:
            assert any(w.text == f"push {flags}" for w in window), site
    assert not any(i.text == "push 0x808" for i in isp.insns if i.seg == 9)
    expect = {"seg9:0x3833": "mov word ptr [bp - 0x22], 0x4003", "seg9:0x37f8": "mov word ptr [bp - 0x12], 0xae4",
              "seg9:0x3866": "cmp word ptr [bp - 0x26], -1", "seg9:0x3870": "push 0x8ab",
              "seg9:0x388e": "push 4", "seg9:0x3895": "cmp ax, 7", "seg9:0x38d3": "push 1"}
    assert _texts(isp, expect) == expect


def test_music_tracks_and_files(isp):
    """sound.md Music: the track tables at ISPACE.DLL DGROUP:0xaa8/0xac0/0xad8 and the file names."""
    dg = unpacked_seg("ispace", 11)
    start = struct.unpack_from("<12h", dg, 0xAA8)
    length = struct.unpack_from("<12h", dg, 0xAC0)
    files = dg[0xAD8:0xAD8 + 12]
    assert start == (0, 45, 1700, 0, 340, 2000, 190, 2870, 2920, 0, 89, 1810)
    assert length == (1810, 1495, 1170, 290, 2550, 166, 2030, 50, 90, 89, 101, 190)
    assert list(files) == [2, 0, 0, 1, 1, 2, 3, 0, 0, 3, 3, 2]
    assert [_cstr(dg, o).decode() for o in (0xAEF, 0xAF4, 0xAF9, 0xB00, 0xB06, 0xAE4)] == \
        ["main", "race", "anthem", "demon", ".mid", "sequencer"]
    assert dg[0xAEE] == 0


def test_midi_files():
    """sound.md Music: sizes, SMF format, division and track counts of the four MIDI files."""
    from isre import szdd

    d = require(ORIGINAL / "game")
    got = {}
    for name in ("MAIN", "RACE", "ANTHEM", "DEMON"):
        data = szdd.expand(require(d / f"{name}.MI1").read_bytes())
        assert data[:4] == b"MThd"
        got[name] = (len(data), *struct.unpack_from(">HHH", data, 8))
    assert got == {"MAIN": (97465, 1, 15, 480), "RACE": (55863, 1, 13, 480),
                   "ANTHEM": (33595, 1, 14, 480), "DEMON": (57931, 1, 13, 480)}


def test_wavemix_ini_rate():
    """sound.md Session: the shipped WAVEMIX.INI [default] rate and blocks."""
    import configparser

    from isre import szdd

    text = szdd.expand(require(ORIGINAL / "game" / "WAVEMIX.IN1").read_bytes()).decode("latin-1")
    cp = configparser.ConfigParser(strict=False, inline_comment_prefixes=(";",))
    cp.optionxform = str
    cp.read_string(text)
    assert (cp["default"]["SamplesPerSec"], cp["default"]["WaveBlocks"]) == ("11", "3")
    assert cp["SB16 Wave Out"]["SamplesPerSec"] == "22"


MUSIC_CALLS = ["seg2:0x4cf", "seg2:0x8df", "seg6:0x1dac", "seg6:0x5e25", "seg9:0x65de", "seg9:0x6bd4",
               "seg10:0x148", "seg10:0x7f34", "seg10:0x7f7e", "seg10:0x82bb", "seg10:0x841d",
               "seg10:0x857f", "seg10:0x8679", "seg11:0x476e", "seg11:0x7aa2", "seg13:0xae4e",
               "seg13:0xbb72", "seg15:0x10dc", "seg15:0x3058", "seg15:0x57b5", "seg15:0x58ec",
               "seg15:0x5bf5", "seg16:0x1e4b", "seg16:0x663c", "seg17:0x6018", "seg18:0x54",
               "seg18:0x129", "seg18:0x1e0", "seg18:0x490", "seg18:0x8d9", "seg18:0xacb",
               "seg18:0x4154", "seg18:0x532c", "seg18:0x5f41", "seg18:0x5f4d", "seg18:0x632b"]


def test_music_wrapper_call_sites(ins):
    """sound.md Music: the 36 calls of seg10:0x7480 and its rewrites."""
    assert ins.call_sites("seg10:0x7480") == sorted(MUSIC_CALLS, key=A)
    expect = {"seg10:0x74b7": "mov word ptr [bp + 6], 9", "seg10:0x74c9": "mov byte ptr [bx + 0x7e0], al",
              "seg10:0x74cd": "mov word ptr [0x9222], 0xfffe", "seg10:0x74d5": "mov word ptr [0x9222], 0xfffd",
              "seg10:0x74f0": "mov word ptr [bp + 6], ax", "seg10:0x7504": "mov word ptr [bp + 6], 2",
              "seg10:0x7524": "add ax, 0x2328", "seg10:0x753c": "push 0xb"}
    assert _texts(ins, expect) == expect
    # the in-wave selectors pass flag 0, the others 1 (seg2:0x4cf and seg2:0x8df push no flag)
    assert ins.at("seg13:0xae49").text == ins.at("seg13:0xbb6d").text == "push 0"


def test_sound_wrapper_sequences(ins):
    """sound.md INSPACE wrappers: the 13-entry follow-up switch of seg10:0x754a and the call counts."""
    sw = ins.switches["seg10:0x7658"]
    assert (sw["table"], sw["count"]) == ("seg10:0x765d", 13)
    assert [k for k, t in enumerate(sw["targets"]) if t == "seg10:0x7642"] == [4, 5, 6, 11]
    expect = {"seg10:0x7582": "mov word ptr [bp + 6], 0x20", "seg10:0x7587": "mov word ptr [0x9292], 0x21",
              "seg10:0x7594": "add ax, 0x3e8", "seg10:0x75b6": "add ax, 0x1a4", "seg10:0x75cd": "add ax, 0x181",
              "seg10:0x75eb": "add ax, 0x294", "seg10:0x7600": "add ax, 0xc8", "seg10:0x7617": "add ax, 0x2bc",
              "seg10:0x762f": "add ax, 0x190", "seg10:0x7622": "mov word ptr [0x9292], 0x66",
              "seg10:0x755e": "mov word ptr [bp + 0xc], 0", "seg10:0x7563": "cmp word ptr [0x920c], 3",
              "seg10:0x775c": "mov ax, 0xd499"}
    assert _texts(ins, expect) == expect
    counts = {t: len(ins.call_sites(t)) for t in ("seg10:0x744c", "seg10:0x754a", "seg10:0x7944")}
    assert counts == {"seg10:0x744c": 152, "seg10:0x754a": 114, "seg10:0x7944": 4}


def test_sound_dialog_controls():
    """sound.md Audio Options and Options: the control ids of ENG_LANG.DLL DIALOG 999 and 100."""
    d999 = _dialog("ENG_LANG.dl1", 999)
    assert (d999["caption"], d999["x"], d999["y"], d999["cx"], d999["cy"]) == ("Audio Options", 73, 31, 171, 158)
    assert [c["id"] for c in d999["controls"]] == [102, 103, 101, 1001, 128, 1008, 1003, 1004, 129, 112,
                                                   113, 1000, 1, 2, 106, 1007]
    d100 = {c["id"]: c for c in _dialog("ENG_LANG.dl1", 100)["controls"]}
    assert {i: (d100[i]["class"], d100[i]["text"]) for i in (139, 111, 118, 104, 140, 143, 144, 145, 146)} == {
        139: ("BUTTON", "&Audio.."), 111: ("SCROLLBAR", ""), 118: ("COMBOBOX", ""), 104: ("BUTTON", "SOUND"),
        140: ("STATIC", "Master Volume"), 143: ("STATIC", "Sound FX:"), 144: ("STATIC", "Normal"),
        145: ("STATIC", "Voice FX:"), 146: ("STATIC", "Normal")}


def test_sound_stage3_bytes(isp, ins):
    """sound.md Session, Playing, Music and Options; shell.md Load and Exit: the M7 stage 3 facts."""
    expect = {"seg9:0x585": "cmp word ptr [bp + 6], 0", "seg9:0x58e": "call seg9:0x624",
              "seg9:0x592": "cmp word ptr [0x219c], 0", "seg9:0x599": "mov word ptr [0x219c], 1",
              "seg9:0x5a2": "inc word ptr [0x219c]", "seg9:0x5a6": "cmp word ptr [0x219c], 1",
              "seg9:0xe31": "cmp word ptr [bp - 6], 0", "seg9:0xe35": "jne seg9:0xe6a",
              "seg9:0xe41": "or ax, word ptr [bx + 0x187c]", "seg9:0xe4d": "call seg9:0x25c6",
              "seg9:0xe50": "add sp, 4", "seg9:0xe61": "je seg9:0xe81",
              "seg9:0x2999": "mov ax, 0xe890", "seg9:0x29a1": "push 1", "seg9:0x29a3": "push 0",
              "seg9:0x29a5": "push 0", "seg9:0x29b1": "push 0x9a9", "seg9:0x29b5": "call seg9:0xc42",
              "seg9:0x3aa8": "mov ax, word ptr [0xaac]", "seg9:0x3ab3": "mov word ptr [0x2292], 4",
              "seg9:0x3ad5": "push 0x64", "seg9:0x3ae2": "mov word ptr [0x1a28], ax",
              "seg9:0x3ae5": "mov word ptr [0x1a2a], 0", "seg9:0x3bad": "mov word ptr [0xaa6], ax",
              "seg9:0x3dc8": "sub dx, dx", "seg9:0x3dcc": "mul word ptr [0x2288]", "seg9:0x3dd0": "sub dx, dx",
              "seg9:0x3dd9": "jbe seg9:0x3ddd", "seg9:0x3ddd": "mov bx, 0xffff",
              "seg9:0x168e": "add ax, 0x8ad", "seg9:0x16c7": "mov ax, 3",
              "seg9:0x1a34": "cmp ax, 0x4b0", "seg9:0x1a4a": "cmp word ptr [bp - 0x1c], 0x24",
              "seg9:0x1a3d": "mov word ptr [bp - 0x20], 0x3e8", "seg9:0x1aad": "mov word ptr [0xd00], ax",
              "seg9:0x1d19": "cmp ax, 0x3e8", "seg9:0x1d2d": "cmp ax, word ptr [0xd00]",
              "seg9:0x1e62": "mov ax, 0x1f4", "seg9:0x1e74": "push 0x6b",
              "seg9:0x1ffd": "sub ax, 3", "seg9:0x2002": "mov word ptr [0x228a], ax",
              "seg9:0x2225": "push word ptr [0xaa4]"}
    assert _texts(isp, expect) == expect
    assert isp.at("seg9:0x3bad").off < isp.at("seg9:0x3bc1").off
    assert "seg9:0x3906" in isp.at("seg9:0x222b").text
    assert "seg3:0x1d6" in isp.at("seg9:0x3de4").text
    sw = isp.switches["seg9:0x1d01"]                      # Play Speed WM_HSCROLL codes 0-7
    assert (sw["table"], sw["count"]) == ("seg9:0x1d06", 8)
    t = sw["targets"]
    assert t[0] == t[2] and t[1] == t[3] and t[4] == t[5] and len({t[0], t[1], t[4], t[6], t[7]}) == 5
    dg = unpacked_seg("ispace", 11)
    assert dg[0xAA2:0xAA8] == b"\xff" * 6
    assert struct.unpack_from("<h", dg, 0xAAC) == (1700,)
    assert len(dg) <= 0xD06 or dg[0xD06:0xD0A] == bytes(4)
    assert [_cstr(dg, o).decode() for o in (0x85D, 0x866, 0x874, 0x883)] == \
        ["Full Out", "%d frames/sec", "Play Speed  %s", "V0.00.00"]
    # INSPACE: the load's 0x6e precedes the DGROUP:0x92e6 retest; the briefing page clicks
    iexp = {"seg9:0x22e9": "cmp word ptr [0x92e6], 0", "seg9:0x2768": "push 0x6e",
            "seg9:0x2770": "cmp word ptr [0x92e6], 0", "seg9:0x2775": "jne seg9:0x27b3",
            "seg10:0xa9": "mov word ptr [0x92e6], ax",
            "seg17:0x23d7": "push 4", "seg17:0x23e2": "push 0x64", "seg17:0x23e4": "push 0x13",
            "seg17:0x2c11": "push 4", "seg17:0x2c1c": "push 0x64", "seg17:0x2c1e": "push 0x13",
            "seg17:0x2aec": "add ax, 0xfa", "seg17:0x2af0": "push 0x15", "seg17:0x2af2": "jmp seg17:0x2c20",
            "seg10:0x150": "push 1"}
    assert _texts(ins, iexp) == iexp
    for site, target in (("seg9:0x276a", "seg10:0x744c"), ("seg17:0x23e6", "seg10:0x754a"),
                         ("seg17:0x2c20", "seg10:0x754a"), ("seg10:0xa2", "seg15:0x6640"),
                         ("seg10:0xad", "seg9:0x22b4"), ("seg10:0x148", "seg10:0x7480"),
                         ("seg10:0x152", "seg15:0x1328"), ("seg10:0x252", "seg10:0x136")):
        assert target in ins.at(site).text, site


def test_midi_microsoft_meta():
    """sound.md Music: each MIDI file carries FF 7F 03 00 00 41 once, in track 0 at offset 23."""
    from isre import szdd

    d = require(ORIGINAL / "game")
    for name in ("MAIN", "RACE", "ANTHEM", "DEMON"):
        data = szdd.expand(require(d / f"{name}.MI1").read_bytes())
        meta = bytes.fromhex("ff7f0300 0041")
        assert data.count(meta) == 1 and data.find(meta) == 23, name
        assert data[14:18] == b"MTrk" and data[22] == 0
        assert 23 < 22 + struct.unpack_from(">I", data, 18)[0]


# ------------------------------------------------------------------ M3 world: Navigation, wave set-up, flight, HUD, radar


def _eng(sid: int) -> str:
    """ENG_LANG.DLL string ``sid`` from its STRING block (sid >> 4) + 1."""
    from isre.res_formats import decode_string_block

    block = (sid >> 4) + 1
    return dict(decode_string_block(block, _resource("ENG_LANG.dl1", 6, block)))[sid]


def _pool_of(m: Module, site: str) -> int:
    """The pool whose far pointer ``les bx, ptr [0x1df8 + 4k]`` the store at ``site`` uses."""
    i = m.at(site)
    for _ in range(8):
        i = m.step(i, -1)
        if i.mnem == "les" and i.abs_ds and 0x1DF8 <= i.mem.disp < 0x1E4C:
            return (i.mem.disp - 0x1DF8) // 4
    raise AssertionError(f"no pool pointer before {site}")


def test_m3_strings():
    """nav.md, world.md: the ENG_LANG.DLL strings the Navigation window and the HUD load."""
    assert {s: _eng(s) for s in (736, 739, 740, 741, 742, 750, 760, 765, 938, 939, 941, 942, 943,
                                 2055, 2056, 2063, 2323, 2515, 3028, 3029)} == {
        736: " - DUEL", 739: " - RACE", 740: "Lazy Loop", 741: "SDi Speedway", 742: "Grand Prix",
        750: "Dodge City", 760: "DEMON'S GATE", 765: "This directory is complete",
        938: "No apps", 939: "Select another directory to enter.", 941: "FILE",
        942: " IN THIS DIRECTORY", 943: "YOU HAVE NOT BEEN HERE BEFORE", 2055: "THERE IS",
        2056: "THERE ARE", 2063: "SCORE", 2323: "To enter, you must pay for the full game version.",
        2515: "ICON", 3028: "%? %# IN %#.", 3029: "%# CAPTURED AND %# DESTROYED."}
    assert {s: _eng(s) for s in range(927, 936)} == {
        927: " frames/ second", 928: "GAME STATUS", 929: "MESSAGE from", 930: "CURRENT ",
        931: "TARGET  (", 932: "AUTO", 933: "MANUAL", 934: "WEAPON SYSTEM", 935: "FUEL - "}
    assert (_eng(152), _eng(2191), _eng(2192), _eng(2193)) == ("Icon ^N", "FULL OUT", " SET", " ACTUAL")


def test_navigation_tree_suffix(ins, dgroup):
    """N01, N02: seg6:0x4118 appends three '0' digits and adds depth, sub and state."""
    expect = {"seg6:0x412b": "push 0xeb2", "seg6:0x4141": "mov al, byte ptr [bp + 0xa]",
              "seg6:0x4144": "add byte ptr es:[bx + si - 1], al", "seg6:0x414b": "push 0xeb4",
              "seg6:0x4161": "mov al, byte ptr [bp + 0xc]", "seg6:0x416b": "push 0xeb6",
              "seg6:0x4181": "mov al, byte ptr [bp + 0xe]", "seg6:0x418f": "retf",
              "seg6:0x38cd": "push 1", "seg6:0x39e8": "push 1"}
    assert _texts(ins, expect) == expect
    assert dgroup[0xEB2:0xEB8] == b"0\x000\x000\x00"
    assert {"seg6:0x38d5", "seg6:0x39f0", "seg6:0x3a58", "seg6:0x3b04", "seg6:0x3ba5",
            "seg6:0x50c7"} <= set(ins.call_sites("seg6:0x4118"))


def test_navigation_glyphs_and_icons(ins):
    """N14, N27, N42: the FLDR and FILES bitmaps of IGRAPHIC.DLL and ISPACE.DLL GROUP_ICON 105-107."""
    expect = {"seg6:0x2401": "push 0xd56", "seg6:0x4bc9": "add word ptr [bp - 0x7c], 0x3c",
              "seg6:0x4bd0": "add word ptr [bp - 0x7c], 0x69"}
    assert _texts(ins, expect) == expect
    assert "seg10:0x6d38" in ins.at("seg6:0x2404").text
    assert _cstr(unpacked_seg("inspace", 19), 0xD56) == b"FLDR"
    assert _dib(_resource("IGRAPHIC.DL1", 2, "FLDR"))[:3] == (135, 11, 4)
    assert 135 == 9 * 15
    assert _dib(_resource("IGRAPHIC.DL1", 2, "FILES"))[:3] == (33, 12, 1)
    assert 33 == 3 * 11
    icons = {r.id for r in _ne("ISPACE.dll").resources if r.type_id == 14}
    assert {105, 106, 107} <= icons


def test_navigation_path_buffers(ins):
    """N41: the INI Track read passes 0x6d = 109 bytes into the 100-byte DGROUP:0x6990."""
    expect = {"seg6:0x3c79": "push 0xe99", "seg6:0x3c81": "push 0x6990", "seg6:0x3c84": "push 0x6d",
              "seg6:0x37d4": "sub sp, 0xf2"}
    assert _texts(ins, expect) == expect
    assert 0x69F4 - 0x6990 == 100 and 0x6D == 109
    assert _cstr(unpacked_seg("inspace", 19), 0xE99) == b"Track"
    body = " ".join(i.text for i in ins.func_insns("seg6:0x37ca"))
    assert "[bp - 0x80]" in body and "[bp - 0xee]" in body
    assert 0x1C0 - 0x152 == 110


def test_wave_budget(ins, dgroup):
    """W01, W02: seg6:0x11ec reads Complex and sets the budget DGROUP:0x932a and DGROUP:0x9000."""
    expect = {"seg6:0x11fb": "push 0x495", "seg6:0x11fe": "push 0",
              "seg6:0x1209": "mov word ptr [0x8ff8], ax",
              "seg6:0x120e": "mov word ptr [0x932a], 0x82", "seg6:0x1214": "mov word ptr [0x9000], 0",
              "seg6:0x121c": "mov word ptr [0x932a], 0xdc", "seg6:0x1222": "jmp seg6:0x1214",
              "seg6:0x1224": "mov word ptr [0x932a], 0x136", "seg6:0x122a": "mov word ptr [0x9000], 1",
              "seg6:0x1232": "mov word ptr [0x932a], 0x1a4", "seg6:0x1238": "mov word ptr [0x9000], 2",
              "seg6:0x1240": "mov word ptr [0x932a], 0x7d0", "seg6:0x1246": "jmp seg6:0x1238",
              "seg6:0x1258": "cmp word ptr [0x91f8], 0xa", "seg6:0x125d": "jge seg6:0x1288",
              "seg6:0x1265": "sar ax, 1", "seg6:0x126c": "imul word ptr [0x932a]",
              "seg6:0x1270": "mov dx, 0x14", "seg6:0x1276": "idiv bx",
              "seg6:0x127a": "cmp cx, word ptr [0x932a]"}
    assert _texts(ins, expect) == expect
    assert ins.at("seg6:0x1275").raw == b"\x99"           # cwd: the product's high word is dropped
    assert (0x82, 0xDC, 0x136, 0x1A4, 0x7D0) == (130, 220, 310, 420, 2000)
    assert _cstr(dgroup, 0x495) == b"Complex"


def test_wave_pool_counts(ins):
    """W05, W06, W07, W44: the pool counts seg9:0x22b4 sets after the generator."""
    expect = {
        "seg9:0x2495": "call seg9:0x3202", "seg9:0x2498": "cmp word ptr [0x9432], 0xfa",
        "seg9:0x249e": "jle seg9:0x24a7", "seg9:0x24a0": "imul ax, word ptr [0x9432], 7",
        "seg9:0x24a7": "imul ax, word ptr [0x9432], 9", "seg9:0x24ac": "mov cx, 0xa",
        "seg9:0x24b0": "idiv cx", "seg9:0x24b2": "mov word ptr [0x9432], ax",
        "seg9:0x24b5": "mov word ptr [0x9406], 8", "seg9:0x24bd": "mov word ptr [0x9408], ax",
        "seg9:0x24c0": "mov word ptr [0x940a], ax", "seg9:0x24c3": "mov bx, word ptr [0x9328]",
        "seg9:0x24c9": "mov word ptr [bx - 0x6bfa], ax", "seg9:0x24cd": "mov word ptr [0x92ea], 0xe",
        "seg9:0x24d3": "mov dword ptr [0x92ec], 0x20008", "seg9:0x24e0": "shl cx, 3",
        "seg9:0x24e3": "mov word ptr [0x92f0], cx", "seg9:0x24e7": "mov word ptr [0x9412], ax",
        "seg9:0x24ea": "mov word ptr [0x9414], ax", "seg9:0x24ed": "mov word ptr [0x941a], ax",
        "seg9:0x24f0": "mov word ptr [0x941c], ax", "seg9:0x24f3": "mov word ptr [0x9420], ax",
        "seg9:0x24f6": "mov word ptr [bx - 0x6d14], ax", "seg9:0x24fa": "mov cx, 1",
        "seg9:0x24fd": "mov word ptr [0x92f8], cx", "seg9:0x2501": "mov word ptr [0x92fa], cx",
        "seg9:0x2505": "mov cx, 3", "seg9:0x2508": "mov word ptr [0x9300], cx",
        "seg9:0x250c": "mov word ptr [0x9302], cx", "seg9:0x2510": "mov word ptr [0x9306], ax",
        # pool 3: rocks
        "seg9:0x2513": "mov ax, word ptr [0x933c]", "seg9:0x2516": "add ax, word ptr [0x933a]",
        "seg9:0x251a": "mov word ptr [0x940c], ax", "seg9:0x251d": "mov word ptr [0x92f2], ax",
        "seg9:0x2540": "mov word ptr [0x92ea], cx", "seg9:0x2544": "cmp ax, 0x7f",
        "seg9:0x2549": "mov word ptr [0x940c], 0x78", "seg9:0x254f": "mov word ptr [0x92f2], 0x398",
        "seg9:0x2555": "mov ax, 0x3c", "seg9:0x2558": "mov word ptr [0x933a], ax",
        "seg9:0x255b": "mov word ptr [0x933c], ax",
        # pools 8, 9 and 12
        "seg9:0x2520": "mov cx, word ptr [0x933e]", "seg9:0x2524": "add cx, word ptr [0x9342]",
        "seg9:0x2528": "mov word ptr [0x9416], cx", "seg9:0x252c": "mov word ptr [0x92fc], cx",
        "seg9:0x2534": "mov word ptr [0x9418], cx", "seg9:0x2538": "mov word ptr [0x92fe], cx",
        "seg9:0x255e": "mov ax, word ptr [0x9318]", "seg9:0x2561": "add ax, ax",
        "seg9:0x2563": "add word ptr [0x9416], ax", "seg9:0x2567": "mov word ptr [0x941e], ax",
        "seg9:0x256a": "mov word ptr [0x9304], ax", "seg9:0x256d": "add word ptr [0x92fc], ax",
        "seg9:0x2571": "cmp word ptr [0x92fc], 0x50", "seg9:0x257b": "mov word ptr [0x9416], ax",
        # Demon's Gate and race
        "seg9:0x2581": "cmp word ptr [0x92e8], 0", "seg9:0x2588": "mov word ptr [0x9326], 1",
        "seg9:0x2590": "mov word ptr [0x940c], ax", "seg9:0x2599": "cmp word ptr [0x9320], 6",
        "seg9:0x25a3": "mov dword ptr [0x940a], eax"}
    assert _texts(ins, expect) == expect
    # word arrays: allocated DGROUP:0x92ec + 2p, in use DGROUP:0x9406 + 2p
    assert ((0x92F8 - 0x92EC) // 2, (0x9300 - 0x92EC) // 2, (0x9306 - 0x92EC) // 2) == (6, 10, 13)
    assert ((0x9412 - 0x9406) // 2, (0x941A - 0x9406) // 2, (0x9420 - 0x9406) // 2) == (6, 10, 13)
    assert ((0x940C - 0x9406) // 2, (0x9416 - 0x9406) // 2, (0x941E - 0x9406) // 2) == (3, 8, 12)
    assert (0x6BFA, 0x6D14) == (0x10000 - 0x9406, 0x10000 - 0x92EC)
    assert (0x78, 0x398, 0x3C) == (120, 920, 60)


def test_wave_generator_counts(ins):
    """W08, W09, W10: the gate count, the pool-4 count and DGROUP:0x932e in seg9:0x3202."""
    expect = {
        # gates
        "seg9:0x4496": "mov word ptr [0x9318], 1", "seg9:0x449c": "cmp word ptr [0x91f8], 1",
        "seg9:0x44a1": "jle seg9:0x4683", "seg9:0x44aa": "test al, 1",
        "seg9:0x44ae": "cmp word ptr [0x92e8], 0", "seg9:0x44b5": "cmp word ptr [0x9320], 6",
        "seg9:0x44bc": "cmp word ptr [0x9320], 7", "seg9:0x44c3": "inc word ptr [0x9318]",
        "seg9:0x44c7": "cmp word ptr [0x91f8], 3", "seg9:0x44d3": "mov cx, 0xa", "seg9:0x44d9": "dec dx",
        "seg9:0x44dc": "inc word ptr [0x9318]", "seg9:0x44e0": "cmp word ptr [0x91f8], 7",
        "seg9:0x44ec": "mov cx, 5", "seg9:0x44f8": "inc word ptr [0x9318]",
        "seg9:0x4663": "imul bx, word ptr [0x94a2], 0x297", "seg9:0x4669": "add bx, word ptr [0x69f4]",
        "seg9:0x467a": "mov al, byte ptr es:[bx + 0xad]", "seg9:0x467f": "cwde",
        "seg9:0x4680": "mov word ptr [0x9318], ax",
        # pool 4
        "seg9:0x3e2b": "mov cx, word ptr [0x9338]", "seg9:0x3e2f": "inc cx", "seg9:0x3e30": "shl cx, 2",
        "seg9:0x3e36": "mov word ptr [0x940e], dx", "seg9:0x3e3a": "cmp dx, 3",
        "seg9:0x3e3f": "mov word ptr [0x940e], 3", "seg9:0x3e45": "cmp word ptr [0x9320], 4",
        "seg9:0x3e51": "mov cx, 0xf", "seg9:0x3e57": "add dx, 9", "seg9:0x3e5a": "add word ptr [0x940e], dx",
        "seg9:0x3e63": "mov cx, 0x19", "seg9:0x3e69": "cmp dx, 4",
        "seg9:0x3e6e": "cmp word ptr [0x9208], 1", "seg9:0x3e75": "mov ax, 0x11",
        "seg9:0x3e80": "mov word ptr [0x92f4], ax", "seg9:0x3e83": "cmp word ptr [0x92e8], 0",
        "seg9:0x3e8a": "mov ax, 1", "seg9:0x3e8f": "cmp word ptr [0x9320], 6",
        "seg9:0x3e9a": "mov al, byte ptr [bx - 0x6141]", "seg9:0x3e9f": "mov word ptr [0x940e], ax",
        # DGROUP:0x932e
        "seg9:0x46ef": "cmp word ptr [0x91f8], 1", "seg9:0x46f6": "mov word ptr [0x932e], 0",
        "seg9:0x4703": "mov cx, 3", "seg9:0x4709": "mov ax, word ptr [0x9334]", "seg9:0x4711": "sar ax, 1",
        "seg9:0x4715": "dec cx", "seg9:0x4716": "mov word ptr [0x932e], cx",
        "seg9:0x4721": "mov word ptr [0x932e], 0", "seg9:0x4727": "cmp word ptr [0x9208], 2",
        "seg9:0x4735": "mov word ptr [0x932e], 4", "seg9:0x474b": "mov word ptr [0x932e], 3",
        "seg9:0x4753": "cmp word ptr [0x9208], 1", "seg9:0x4761": "mov word ptr [0x932e], 2",
        "seg9:0x4767": "cmp word ptr [0x92e8], 0", "seg9:0x476e": "cmp word ptr [0x9320], 6",
        "seg9:0x4775": "mov word ptr [0x932e], 0"}
    assert _texts(ins, expect) == expect
    assert (0x10000 - 0x6141) == 0x9EBF
    for s in ("seg9:0x44a5", "seg9:0x44ce", "seg9:0x44e7", "seg9:0x3e26", "seg9:0x3e4c",
              "seg9:0x3e5e", "seg9:0x46fe"):
        assert "seg5:0xdc" in ins.at(s).text, s                 # rand()
    assert all(ins.in_func(ins.at(s), "seg9:0x3202") for s in expect)


def test_object_templates(ins):
    """W12, W13, R21, R22: the per-pool record fields seg10:0xb94 writes."""
    stores = {   # site: (pool, text)
        # pool 0, icons
        "seg10:0xbbd": (0, "mov word ptr es:[bx + si + 0x22], ax"),
        "seg10:0xbc5": (0, "mov word ptr es:[bx + si + 0x20], 0x100"),
        "seg10:0xbda": (0, "mov word ptr es:[bx + si + 0x24], ax"),
        "seg10:0xbe5": (0, "mov word ptr es:[bx + si + 0x28], ax"),
        "seg10:0xbed": (0, "mov word ptr es:[bx + si + 0x44], 0xa0"),
        "seg10:0xbf7": (0, "mov word ptr es:[bx + si + 0x42], 0x12"),
        "seg10:0xc01": (0, "mov word ptr es:[bx + si + 0x46], 2"),
        "seg10:0xc0b": (0, "mov word ptr es:[bx + si + 0x62], 0x39d8"),
        "seg10:0xc1b": (0, "mov word ptr es:[bx + si + 0x5e], 0x233c"),
        "seg10:0xc2b": (0, "mov byte ptr es:[bx + si + 0x53], 1"),
        # pools 1 and 2
        "seg10:0xc9a": (1, "mov word ptr es:[bx + si + 0x4c], 0x8000"),
        "seg10:0xca0": (1, "mov word ptr es:[bx + si + 0x4e], 0x80"),
        "seg10:0xcec": (1, "mov word ptr es:[bx + si + 0x62], 0x3914"),
        "seg10:0xd90": (2, "mov word ptr es:[bx + si + 0x4c], 0xffff"),
        "seg10:0xd96": (2, "mov word ptr es:[bx + si + 0x4e], 0"),
        "seg10:0xda0": (2, "mov word ptr es:[bx + si + 0x62], 0x3914"),
        # pool 3, small rocks, then the shared tail
        "seg10:0xdee": (3, "mov word ptr es:[bx + si + 0x22], ax"),
        "seg10:0xdf6": (3, "mov word ptr es:[bx + si + 0x20], 0xa8"),
        "seg10:0xe00": (3, "mov word ptr es:[bx + si + 0x44], 0x15e"),
        "seg10:0xe0a": (3, "mov word ptr es:[bx + si + 0x62], 0x3914"),
        "seg10:0xe1a": (3, "mov word ptr es:[bx + si + 0x2c], 0xc8"),
        "seg10:0xe2f": (3, "mov word ptr es:[bx + si + 0x24], ax"),
        "seg10:0xe3e": (3, "mov word ptr es:[bx + si + 0x24], 4"),
        "seg10:0xe5a": (3, "mov word ptr es:[bx + si + 0x24], 2"),
        "seg10:0xe76": (3, "mov word ptr es:[bx + si + 0x24], 1"),
        "seg10:0xe84": (3, "mov word ptr es:[bx + si + 0x48], 9"),
        "seg10:0xe8e": (3, "mov word ptr es:[bx + si + 0x42], 8"),
        "seg10:0xe98": (3, "mov word ptr es:[bx + si + 0x46], 4"),
        "seg10:0xea2": (3, "mov word ptr es:[bx + si + 0x4c], 0xc0c0"),
        "seg10:0xea8": (3, "mov word ptr es:[bx + si + 0x4e], 0xc0"),
        "seg10:0xeb2": (3, "mov word ptr es:[bx + si + 0x5e], 0x5090"),
        # pool 3, big rocks
        "seg10:0xefa": (3, "mov word ptr es:[bx + si + 0x22], ax"),
        "seg10:0xf02": (3, "mov word ptr es:[bx + si + 0x20], 0x158"),
        "seg10:0xf0c": (3, "mov word ptr es:[bx + si + 0x44], 0x578"),
        "seg10:0xf16": (3, "mov word ptr es:[bx + si + 0x62], 0x3bae"),
        "seg10:0xf26": (3, "mov word ptr es:[bx + si + 0x2c], 0x3c"),
        # pools 4 to 11
        "seg10:0xfb7": (4, "mov word ptr es:[bx + si + 0x4c], 0xff"),
        "seg10:0xfd1": (4, "mov word ptr es:[bx + si + 0x46], 5"),
        "seg10:0xfdb": (4, "mov word ptr es:[bx + si + 0x62], 0x3bae"),
        "seg10:0x103a": (5, "mov word ptr es:[bx + si + 0x4c], 0xff"),
        "seg10:0x104a": (5, "mov word ptr es:[bx + si + 0x62], 0x3914"),
        "seg10:0x10a6": (6, "mov word ptr es:[bx + si + 0x4c], 0x7f00"),
        "seg10:0x10ca": (6, "mov word ptr es:[bx + si + 0x62], 0x3914"),
        "seg10:0x115f": (7, "mov word ptr es:[bx + 0x46], 1"),
        "seg10:0x1183": (7, "mov word ptr es:[bx + 0x62], 0x37a0"),
        "seg10:0x1207": (9, "mov word ptr es:[bx + si + 0x20], 0x128"),
        "seg10:0x1242": (9, "mov word ptr es:[bx + si + 0x46], 6"),
        "seg10:0x1260": (9, "mov word ptr es:[bx + si + 0x62], 0x39d8"),
        "seg10:0x1327": (10, "mov word ptr es:[bx + si + 0x46], 4"),
        "seg10:0x1331": (10, "mov word ptr es:[bx + si + 0x4c], 0x7f7f"),
        "seg10:0x1337": (10, "mov word ptr es:[bx + si + 0x4e], 0x7f"),
        "seg10:0x1365": (10, "mov word ptr es:[bx + si + 0x62], 0x3914"),
        "seg10:0x13e2": (11, "mov word ptr es:[bx + si + 0x46], 2"),
        "seg10:0x1400": (11, "mov word ptr es:[bx + si + 0x62], 0x39d8")}
    assert {s: (_pool_of(ins, s), ins.at(s).text) for s in stores} == stores
    sizes = {"seg10:0xbae": "mov ax, 0x20", "seg10:0xbd3": "mov ax, 6", "seg10:0xddf": "mov ax, 0x16",
             "seg10:0xe28": "mov ax, 4", "seg10:0xeeb": "mov ax, 0x2c", "seg10:0xf30": "mov ax, 4",
             "seg10:0x11f0": "mov ax, 0x26", "seg10:0xbb1": "shl ax, cl", "seg10:0xbb3": "sar ax, 1",
             "seg10:0xe33": "cmp word ptr [0x9156], 2", "seg10:0xe4b": "cmp word ptr [0x740], 0",
             "seg10:0xed3": "mov ax, word ptr [0x92f2]", "seg10:0xedb": "sub ax, word ptr [0x92ea]",
             "seg10:0xedf": "cmp ax, word ptr [bp - 4]", "seg10:0xee2": "jg seg10:0xee7"}
    assert _texts(ins, sizes) == sizes
    # segment selectors of the far pointers
    for site, seg in (("seg10:0xc11", 11), ("seg10:0xc21", 7), ("seg10:0xeb8", 7), ("seg10:0xf1c", 11),
                      ("seg10:0xfe1", 11), ("seg10:0x1189", 11)):
        assert ins.at(site).text.startswith("mov word ptr es:[bx") and f"selector of seg{seg}" in \
            (WORK / "asm" / "inspace" / "seg10.asm").read_text().split(site + " ", 1)[1].split("\n", 1)[0]
    assert (0x100, 0x158, 0x578, 0xA8, 0x15E, 0x128) == (256, 344, 1400, 168, 350, 296)


def test_world_size_and_integrator(ins):
    """W15, W16, W17, F12: DGROUP:0x5a0/0x5a2 have no writer, and seg8:0x1b54.

    Their initial value 3200 is test_initialised_timing_and_sound_data's."""
    assert ins.writers(0x5A0, 0x5A3) == [] and 3200 * 8 == 25600
    expect = {
        "seg8:0x1b72": "cmp ax, 0xe", "seg8:0x1b7e": "cmp byte ptr es:[bx + si + 0x54], ah",
        "seg8:0x1b84": "cmp word ptr [0x92e8], 0", "seg8:0x1b8b": "cmp word ptr [0x9e49], 0xa",
        "seg8:0x1b90": "je seg8:0x1cb3", "seg8:0x1ba0": "cmp word ptr es:[bx + si + 0x32], -1",
        "seg8:0x1ba7": "cmp ax, 0xa", "seg8:0x1baf": "call seg8:0x1e32",
        "seg8:0x1bb2": "mov ax, word ptr [0x9328]", "seg8:0x1bc0": "mov word ptr [0x9006], 0xa",
        "seg8:0x1bc6": "cmp word ptr [bp + 6], 1", "seg8:0x1be0": "add word ptr es:[si + 4], cx",
        "seg8:0x1c06": "imul word ptr [bp + 6]", "seg8:0x1c1a": "add word ptr es:[bx + 6], ax",
        "seg8:0x1c1e": "cmp word ptr [0x9130], 0", "seg8:0x1c23": "jne seg8:0x1cb3",
        "seg8:0x1c2a": "shl ax, 3", "seg8:0x1c3b": "cmp word ptr es:[bx + si + 4], ax",
        "seg8:0x1c3f": "jle seg8:0x1c47", "seg8:0x1c41": "mov word ptr es:[bx + si + 4], 0",
        "seg8:0x1c55": "cmp word ptr es:[bx + si + 4], 0", "seg8:0x1c5a": "jge seg8:0x1c66",
        "seg8:0x1c62": "mov word ptr es:[bx + si + 4], ax",
        "seg8:0x1c80": "mov word ptr es:[bx + si + 6], 0",
        "seg8:0x1ca1": "mov word ptr es:[bx + si + 6], ax", "seg8:0x1cb0": "mov word ptr [0x9006], ax",
        "seg8:0x1cba": "retf 4"}
    assert _texts(ins, expect) == expect


def test_random_placement(ins):
    """W20, W31: seg10:0x864 (rocks) and seg10:0x736 (icons) place in rand() order."""
    expect = {
        "seg10:0x88e": "sub cx, 0x96", "seg10:0x895": "add dx, 0x96", "seg10:0x899": "shl dx, 3",
        "seg10:0x8a0": "mov word ptr es:[bx + si + 4], dx", "seg10:0x8ad": "sub cx, 0x96",
        "seg10:0x8bf": "mov word ptr es:[bx + si + 6], dx",
        "seg10:0x8c7": "mov word ptr es:[bx + si + 8], 0xfc18",
        "seg10:0x8e4": "mov word ptr es:[bx + si + 0xa], 0xfc18", "seg10:0x901": "mov cx, 0x32",
        "seg10:0x907": "sub dx, 0x19", "seg10:0x90a": "neg dx",
        "seg10:0x90c": "mov word ptr es:[bx + si + 0xc], dx",
        "seg10:0x924": "mov word ptr es:[bx + si + 0xe], dx", "seg10:0x936": "cmp ax, 0xa",
        "seg10:0x949": "cmp ax, 0xa", "seg10:0x952": "add word ptr es:[bx + si + 0xc], 0x14",
        "seg10:0x95f": "mov word ptr es:[bx + si + 0x18], 0", "seg10:0x969": "mov byte ptr es:[bx + si + 0x2f], 0",
        "seg10:0x972": "mov byte ptr es:[bx + si + 0x2e], 0", "seg10:0x98a": "mov cx, 0x6a4",
        "seg10:0x994": "mov word ptr es:[bx + si + 0x26], dx",
        "seg10:0x752": "mov byte ptr es:[bx + si + 0x54], 0", "seg10:0x760": "sub cx, 0x96",
        "seg10:0x799": "mov word ptr es:[bx + si + 8], 0xfc18",
        "seg10:0x7d0": "mov byte ptr es:[bx + di + 0x2f], 0", "seg10:0x7f7": "mov cl, byte ptr [0x9150]"}
    assert _texts(ins, expect) == expect
    assert all(_calls(ins, {"seg10:0x930": "seg9:0x88ba", "seg10:0x943": "seg9:0x88ba",
                            "seg10:0x82e": "SetRect", "seg10:0x844": "PtInRect"}).values())
    assert (0x96, 0x6A4, s16(0xFC18), 3200 - 150) == (150, 1700, -1000, 3050)
    assert _pool_of(ins, "seg10:0x8a0") == 3 and _pool_of(ins, "seg10:0x772") == 0


def test_heading_and_ring_tables(ins, dgroup):
    """F01, W22, F43: the heading table DGROUP:0x1050, the pool-4 ring, the joystick map."""
    heads = [struct.unpack_from("<hh", dgroup, 0x1050 + 4 * i) for i in range(16)]
    assert heads == [(0, -16), (6, -15), (11, -11), (15, -6), (16, 0), (15, 6), (11, 11), (6, 15),
                     (0, 16), (-6, 16), (-11, 11), (-15, 6), (-16, 0), (-15, -6), (-11, -11), (-6, -15)]
    # the ring of seg9:0xdf6 reads word DGROUP:0x104c + 4k (y) and DGROUP:0x104e + 4k (x), k = 1..16
    expect = {"seg9:0xfe1": "cmp word ptr [0x940e], 0x11",
              "seg9:0x1018": "imul ax, word ptr [bx + 0x104c], 0xd0",
              "seg9:0x103e": "imul ax, word ptr [si + 0x104e], 0xd0"}
    assert _texts(ins, expect) == expect
    assert struct.unpack_from("<hh", dgroup, 0x104C + 4) == (0, -16)
    assert struct.unpack_from("<hh", dgroup, 0x104C + 8) == (6, -15)
    joy = [b - 256 if b > 127 else b for b in dgroup[0x1018:0x1018 + 25]]
    assert joy == [14, 15, 0, 1, 2, 13, -1, -1, -1, 3, 12, -1, -1, -1, 4, 11, -1, -1, -1, 5,
                   10, 9, 8, 7, 6]
    assert struct.unpack_from("<h", dgroup, 0x1034) == (1,)


def _sin128(tab: bytes, a: int) -> int:
    a = (a + 360) % 360 if a + 360 >= 0 else -((-(a + 360)) % 360)
    if a < 90:
        return tab[a]
    if a < 180:
        return tab[180 - a]
    if a < 270:
        return -tab[a - 180]
    return -tab[360 - a]


def test_sine_table(ins, dgroup):
    """F29: the 91 sine bytes at DGROUP:0xf00 and the quadrant folding of seg6:0x71b2."""
    import math

    tab = dgroup[0xF00:0xF5B]
    assert list(tab[:5]) == [0, 2, 4, 6, 9] and list(tab[-4:]) == [127, 128, 128, 128]
    assert tab[25] == 55 and round(math.sin(math.radians(25)) * 128) == 54
    assert sum(tab[i] != round(math.sin(math.radians(i)) * 128) for i in range(91)) == 57
    expect = {"seg6:0x71c2": "mov cx, 0x168", "seg6:0x71c8": "idiv cx", "seg6:0x71cc": "cmp dx, 0x5a",
              "seg6:0x71d3": "mov al, byte ptr [bx + 0xf00]", "seg6:0x71db": "cmp dx, 0xb4",
              "seg6:0x71e1": "mov bx, 0xfb4", "seg6:0x71ea": "cmp dx, 0x10e",
              "seg6:0x71f2": "mov al, byte ptr [bx + 0xe4c]", "seg6:0x71f8": "neg ax",
              "seg6:0x71fc": "mov bx, 0x1068", "seg6:0x721c": "mov cx, 0x168"}
    assert _texts(ins, expect) == expect
    assert (0xFB4 - 0xF00, 0xF00 - 0xE4C, 0x1068 - 0xF00) == (180, 180, 360)
    assert [_sin128(tab, a) for a in (0, 90, 180, 270, -90, 30)] == [0, 128, 0, -128, -128, tab[30]]


def test_flight_helpers(ins):
    """F30, F31: seg8:0x35d0 (one Newton step) and seg8:0x19b0 (direction of vy over vx)."""
    expect = {"seg8:0x35e3": "call far seg9:0x88dc", "seg8:0x360f": "sar dx, 1",
              "seg8:0x361f": "sar eax, 1", "seg8:0x3646": "call far seg5:0x314",
              "seg8:0x3668": "idiv ecx", "seg8:0x366b": "add eax, dword ptr [bp - 0xe]",
              "seg8:0x366f": "sar eax, 1", "seg8:0x367b": "mov ax, 1", "seg8:0x3686": "retf",
              "seg8:0x19bf": "movsx eax, word ptr [bp + 6]", "seg8:0x19c8": "mov ax, word ptr [bp + 8]",
              "seg8:0x19f2": "mov dword ptr [bp - 6], 1", "seg8:0x19fe": "mov ecx, 0x64",
              "seg8:0x1a09": "idiv dword ptr [bp - 6]", "seg8:0x1a14": "cmp eax, 0x1f4",
              "seg8:0x1a23": "cmp ax, 0xfe0c", "seg8:0x1b51": "retf 4"}
    assert _texts(ins, expect) == expect
    # (upper bound exclusive, lower bound exclusive, value when num > 0, otherwise)
    ranges = [("seg8:0x1a3b", 0x1F5, "seg8:0x1a45", 0x9A, 7, 15),
              ("seg8:0x1a66", 0x9B, "seg8:0x1a70", 0x41, 6, 14),
              ("seg8:0x1a8e", 0x42, "seg8:0x1a95", 0x13, 5, 13),
              ("seg8:0x1ab3", 0x14, "seg8:0x1aba", -0x14, 4, 12),
              ("seg8:0x1ad6", -0x13, "seg8:0x1add", -0x41, 11, 3),
              ("seg8:0x1af9", -0x40, "seg8:0x1b00", -0x9A, 10, 2),
              ("seg8:0x1b1f", -0x99, "seg8:0x1b29", -0x1F5, 9, 1)]
    for hi, hv, lo, lv, pos, neg in ranges:
        assert s16(int(ins.at(hi).ops[1], 0) & 0xFFFF) == hv, hi
        assert s16(int(ins.at(lo).ops[1], 0) & 0xFFFF) == lv, lo
        a = ins.step(ins.at(lo), 4)
        b = ins.step(a, 2)
        assert (int(a.ops[1], 0), int(b.ops[1], 0)) == (pos, neg), lo
    # the -19..19 band tests the denominator, the others the numerator
    assert ins.at("seg8:0x1ac1").text == "cmp dword ptr [bp - 0xe], 0"
    assert ins.at("seg8:0x1a4f").text == "cmp dword ptr [bp - 0x12], 0"
    assert (155, 66, 20, -19, -64, -153, -500) == (0x9A + 1, 0x41 + 1, 0x13 + 1, -0x14 + 1,
                                                    -0x41 + 1, -0x9A + 1, -0x1F5 + 1)


def _hud_layout(w: int, race: bool) -> dict[int, int]:
    half = _trunc(w, 2)
    l, lw = (1, 0x49) if w < 660 else (1, 0x5A) if w < 760 else (4, 0x86)
    g = {0x93B8: l, 0x93DC: lw}
    g[0x5024] = half - lw - 96 + (9 if w < 700 else 0)
    g[0x93BA] = lw + l + 4 - (2 if w < 700 else 0)
    g[0x93BE] = half - 50
    g[0x93BC] = g[0x93BE] - 30 - (4 if w >= 700 else 0)
    g[0x93C0] = half + 68 - (14 if race else 0)
    g[0x93C2] = g[0x93C0] + 28
    g[0x93C4] = g[0x93C0] + 88
    g[0x5026] = half - 160 + (4 if w < 700 else 0) + (14 if race else 0)
    return g


def test_hud_layout(ins):
    """D03, D04: the HUD layout of seg11:0xafe and its values at 1024 x 768."""
    expect = {"seg11:0xb77": "cmp word ptr [0x90ea], 0x294", "seg11:0xb85": "mov word ptr [0x93dc], 0x49",
              "seg11:0xb8d": "cmp word ptr [0x90ea], 0x2f8", "seg11:0xb9b": "mov word ptr [0x93dc], 0x5a",
              "seg11:0xba3": "mov word ptr [0x93b8], 4", "seg11:0xba9": "mov word ptr [0x93dc], 0x86",
              "seg11:0xbb7": "sub ax, word ptr [0x93dc]", "seg11:0xbbb": "sub ax, 0x60",
              "seg11:0xbc1": "cmp word ptr [0x90ea], 0x2bc", "seg11:0xbc9": "add ax, 9",
              "seg11:0xbd6": "add ax, 4", "seg11:0xbf1": "sub ax, 0x32", "seg11:0xbf7": "sub ax, 0x1e",
              "seg11:0xc03": "jl seg11:0xc0b", "seg11:0xc05": "sub ax, 4", "seg11:0xc13": "add ax, 0x44",
              "seg11:0xc19": "cmp word ptr [0x94e], 0", "seg11:0xc20": "sub ax, 0xe",
              "seg11:0xc26": "add ax, 0x1c", "seg11:0xc2f": "add ax, 0x58", "seg11:0xc3d": "sub ax, 0xa0",
              "seg11:0xc4b": "add ax, 4", "seg11:0xc58": "add word ptr [0x5026], 0xe",
              "seg2:0xc4b": "sub ax, 0x55", "seg2:0xc4e": "mov word ptr [0x8e5a], ax"}
    assert _texts(ins, expect) == expect
    assert (0x294, 0x2F8, 0x2BC) == (660, 760, 700)
    g = _hud_layout(1024, False)
    assert {k: g[k] for k in (0x93BA, 0x5024, 0x93BC, 0x93BE, 0x93C0, 0x93C2, 0x93C4, 0x5026)} == {
        0x93BA: 142, 0x5024: 282, 0x93BC: 428, 0x93BE: 462, 0x93C0: 580, 0x93C2: 608, 0x93C4: 668,
        0x5026: 352}
    h = 768 - 85
    # the oracle's child windows: scrollbar (W/2 + 50, H'), the two IS_INFO windows (+5, +5)
    assert (512 + 50, h) == (562, 683)
    assert (g[0x93BA] + 5, h + 5, g[0x5024] - 9) == (147, 688, 273)
    assert (g[0x93C4] + 5, g[0x5026] - 9) == (673, 343)


def test_hud_children(ins):
    """D05: the scrollbar and the two IS_INFO windows of each HUD repaint."""
    expect = {"seg11:0xcaa": "push 0xaab", "seg11:0xcb1": "push 0x5000", "seg11:0xcb4": "push 1",
              "seg11:0xcbe": "add ax, 0x32", "seg11:0xcc6": "push 0xe", "seg11:0xcc8": "push 0x55",
              "seg11:0xcce": "push 0xc8", "seg11:0xce2": "push 2", "seg11:0xce4": "push 0x64",
              "seg11:0xce6": "push 0x1f4", "seg11:0xcf6": "mov ax, 0x258",
              "seg11:0xcf9": "sub ax, word ptr [0xa6c]", "seg11:0xd1c": "push 0xab6",
              "seg11:0xd23": "push 0x5000", "seg11:0xd26": "push 0"}
    assert _texts(ins, expect) == expect
    assert all(_calls(ins, {"seg11:0xcd9": "CreateWindow", "seg11:0xceb": "SetScrollRange",
                            "seg11:0xd00": "SetScrollPos", "seg11:0xc97": "DestroyWindow"}).values())
    dg = unpacked_seg("inspace", 19)
    assert (_cstr(dg, 0xAAB), _cstr(dg, 0xAB6)) == (b"scrollbar", b"IS_INFO")


def _tile_classes(ins: Module, n: int) -> list[int]:
    """Emulate seg11:0x170a: the reset call, then ``n`` tile picks."""
    sw = ins.switches["seg11:0x1756"]
    value = {t: int(ins.at(t).ops[1], 0) for t in set(sw["targets"])}
    table = [value[t] for t in sw["targets"]]
    counter, out = -1, []
    for k in range(n + 1):
        counter += 1
        if counter > 6:
            counter = 0
        if k:
            out.append(table[counter])
    return out


def test_hud_border_and_bevel(ins):
    """D07, D08, D09, D10, D11, D12: strip fill, border tiles, bevel bands and the top line."""
    expect = {
        "seg11:0xdab": "push word ptr [0x500a]", "seg11:0xdc5": "push 0x55", "seg11:0xdc7": "push 0xf0",
        "seg11:0xdca": "push 0x21", "seg11:0xddd": "cmp word ptr [0xa78], 0",
        "seg11:0xde2": "je seg11:0x11a6", "seg11:0xdef": "mov cx, 0x3a", "seg11:0xdfc": "sar ax, 1",
        "seg11:0xe12": "inc ax", "seg11:0xe16": "push 1", "seg11:0xe1c": "pop bx",
        "seg11:0xe2a": "push word ptr [0x5044]", "seg11:0xe4b": "imul ax, word ptr [bp - 0xe], 0x3a",
        "seg11:0xe5c": "imul ax, word ptr [bp - 0x48], 0x3a", "seg11:0xe63": "push 0xcc",
        "seg11:0xe73": "inc ax", "seg11:0xe77": "jg seg11:0xe3d",
        "seg11:0xe8e": "mov word ptr [bp - 0xe], 1", "seg11:0xea6": "neg ax",
        "seg11:0xeec": "push word ptr [bp - 0x46]", "seg11:0xf1f": "sub ax, 0x3a",
        # bevel: PALETTERGB(v, v, v), v = (14 - i) * 10, then (12 - i) * 20
        "seg11:0xf9e": "mov al, 0xe", "seg11:0xfa3": "mov cl, 0xa", "seg11:0xfab": "mov ah, 2",
        "seg11:0xfb3": "imul dx, dx, 0xa00", "seg11:0x1126": "mov al, 0xc", "seg11:0x112b": "mov cl, 0x14",
        "seg11:0x113b": "imul dx, dx, 0x1400", "seg11:0x1117": "cmp word ptr [bp - 0xe], 0xa",
        "seg11:0x1155": "mov ax, 0xffe2", "seg11:0x1168": "add ax, 0x1d", "seg11:0x117b": "sub ax, 0x3a",
        # top line of the strip
        "seg11:0x11ae": "push 0xc0c0c0", "seg11:0x11ac": "push 1"}
    assert _texts(ins, expect) == expect
    assert all(_calls(ins, {"seg11:0xdcc": "PatBlt", "seg11:0xe19": "seg11:0x170a",
                            "seg11:0xe68": "BitBlt", "seg11:0x10fd": "PatBlt",
                            "seg11:0x1184": "PatBlt", "seg11:0x11b4": "CreatePen",
                            "seg11:0x11cc": "MoveTo", "seg11:0x11dd": "LineTo"}).values())
    sw = ins.switches["seg11:0x1756"]
    assert sw["count"] == 7
    assert _tile_classes(ins, 14) == [1, 2, 1, 0, 2, 1, 0, 1, 2, 1, 0, 2, 1, 0]
    assert ins.at("seg11:0x171d").text == "mov word ptr [0x1852], 0xffff"
    assert ins.at("seg11:0x1727").text == "cmp word ptr [0x1852], 6"
    off = _trunc(58 - 1024 % 58, 2)
    assert (off, 1024 // 58 + 1) == (10, 18)
    # bevel at 1024 x 768: the play area is 68..956 x 78..663, half-open
    h = 768 - 85
    assert (2 * 9 + 58 - off + 2, 2 * 9 + 58 + 2) == (68, 78)
    assert (1024 + off - 60 - 2 * 9, h - 2 - 2 * 9) == (956, 663)


def test_hud_panels(ins):
    """D14, D15, D16, D17, D18, D21: the SCORE box, the panel frames, fuel, damage and clock."""
    expect = {
        "seg11:0x11f8": "push 0x80f", "seg11:0x1200": "push 6", "seg11:0x1224": "add ax, 0xc",
        "seg11:0x122c": "mov ax, 0x2d", "seg11:0x1235": "push 0x42", "seg11:0x127c": "imul ax, si, 0xe",
        "seg11:0x1283": "add ax, 6", "seg11:0x1289": "push 0xff", "seg11:0x12b2": "mov ax, 0x3c",
        "seg11:0x12e9": "imul ax, si, 0x12", "seg11:0x1341": "imul ax, si, 0x16",
        "seg11:0x1348": "add ax, 0x19", "seg11:0x134e": "push 0xff",
        "seg11:0x1366": "mov dword ptr [0x5018], 0xffffffff", "seg11:0x1370": "call seg11:0x4ea",
        "seg11:0x137b": "push 0xc0c0c0",
        "seg11:0x13f0": "add ax, 0x54", "seg11:0x1404": "push 2", "seg11:0x1407": "call seg11:0x1774",
        "seg11:0x1432": "push 0x1e", "seg11:0x1434": "push 0x55", "seg11:0x145a": "mov word ptr [0x93d4], 0xffff",
        "seg11:0x1461": "call seg11:0x978", "seg11:0x146e": "add ax, 0x63", "seg11:0x147a": "push 4",
        "seg11:0x147d": "call seg11:0x1774", "seg11:0x14a7": "add ax, 0x18", "seg11:0x14ab": "push 0x54",
        "seg11:0x14ad": "push 0x3d", "seg11:0x14d3": "push 1", "seg11:0x14d6": "call seg11:0x5fe",
        "seg11:0x14da": "mov eax, dword ptr [0x1e50]", "seg11:0x14de": "mov dword ptr [0x5028], eax",
        "seg11:0x14e2": "cmp word ptr [0x93d8], 1", "seg11:0x14ef": "call seg11:0x894",
        "seg11:0x14f2": "push 0", "seg11:0x14f4": "push 0x11", "seg11:0x14f6": "push 0x37",
        "seg11:0x151f": "push 0xff00", "seg11:0x1533": "push word ptr [0x5038]",
        "seg11:0x1546": "add ax, 0x17", "seg11:0x158f": "call seg11:0x1ef6"}
    assert _texts(ins, expect) == expect
    assert 0x80F == 2063
    assert all(_calls(ins, {"seg11:0x1202": "LoadString", "seg11:0x1237": "PatBlt",
                            "seg11:0x128f": "seg11:0x2154", "seg11:0x12fa": "seg10:0x94c4",
                            "seg11:0x1355": "seg10:0x94c4", "seg11:0x1442": "BitBlt",
                            "seg11:0x14bb": "BitBlt", "seg11:0x1505": "seg10:0x949c",
                            "seg11:0x1522": "CreateSolidBrush", "seg11:0x1562": "BitBlt",
                            "seg11:0x15ad": "seg11:0x1774"}).values())
    weapon = {"seg11:0x1596": "add ax, 0x54", "seg11:0x159a": "mov ax, word ptr [0x5026]",
              "seg11:0x159d": "add ax, word ptr [0x93c4]", "seg11:0x15a6": "push word ptr [0x93c4]",
              "seg11:0x15aa": "push 2", "seg11:0x154d": "add ax, 4", "seg11:0x1551": "push 0xa",
              "seg11:0x1553": "push 0xa", "seg11:0x1558": "push 0x64", "seg11:0x155a": "push 0",
              "seg11:0x155c": "push 0xc000ca"}
    assert _texts(ins, weapon) == weapon


def test_damage_lamps(ins):
    """D19: seg11:0x3cc2 composes four 32 x 11 lamps into DGROUP:0x503c from DGROUP:0x5030."""
    expect = {"seg11:0x3cdd": "push word ptr [0x5030]", "seg11:0x3cf5": "push word ptr [0x503c]",
              "seg11:0x3d0e": "cmp byte ptr [bx + 0x7177], 0", "seg11:0x3d15": "mov word ptr [bp - 0x10], 4",
              "seg11:0x3d21": "imul si, word ptr [bx + 0x70de], 0xad",
              "seg11:0x3d2c": "mov ax, word ptr [bx + si - 0x690c]", "seg11:0x3d33": "cmp word ptr [bp - 0x10], -1",
              "seg11:0x3d3c": "push 1", "seg11:0x3d4f": "push 0x29", "seg11:0x3d51": "imul ax, word ptr [bp - 0xe], 0xe",
              "seg11:0x3d55": "add ax, 5", "seg11:0x3d59": "push 0x20", "seg11:0x3d5b": "push 0xb",
              "seg11:0x3d5d": "push 0xf0", "seg11:0x3d8d": "inc ax", "seg11:0x3d8e": "imul ax, ax, 0xb",
              "seg11:0x3d92": "push 0xcc", "seg11:0x3d9f": "cmp word ptr [bp - 0xe], 4",
              "seg11:0x3da5": "cmp word ptr [bp - 0xe], 3", "seg11:0x3da9": "jne seg11:0x3d1c",
              "seg11:0x3db2": "cmp byte ptr [bx + 0x7179], 0", "seg11:0x3db7": "je seg11:0x3d09",
              "seg11:0x3dbb": "mov word ptr [bp - 0x10], 5"}
    assert _texts(ins, expect) == expect
    assert 0x10000 - 0x690C == 0x96F4
    assert all(_calls(ins, {"seg11:0x3d3e": "GetStockObject", "seg11:0x3d62": "PatBlt",
                            "seg11:0x3d97": "BitBlt"}).values())


def test_player_sprite_draw(ins):
    """D25: seg7:0x1208's gates, cell inset and the call of seg8:0xb76."""
    expect = {"seg7:0x1216": "cmp word ptr [0x90f4], 1", "seg7:0x1226": "cmp word ptr es:[bx + si + 0x18], 0",
              "seg7:0x122d": "cmp word ptr [0x951a], 0", "seg7:0x1234": "cmp word ptr [0x9130], 0",
              "seg7:0x123e": "cmp word ptr [0x9e49], 3", "seg7:0x1245": "cmp word ptr [0x9e49], 6",
              "seg7:0x124c": "mov ax, word ptr [0x952e]", "seg7:0x1257": "cmp word ptr [0x3a38], 0",
              "seg7:0x1266": "cmp byte ptr [bx + 0x717c], 0", "seg7:0x1277": "mov ax, word ptr [0x9434]",
              "seg7:0x127f": "imul bx, word ptr [bp - 0xc], 0x14", "seg7:0x1283": "cmp word ptr [bx + 0x1e96], 0xe",
              "seg7:0x128d": "cmp word ptr [bx + 0x1e98], ax", "seg7:0x1293": "cmp word ptr [bx + 0x1e90], 0",
              "seg7:0x129a": "cmp word ptr [bx + 0x1e90], 9", "seg7:0x12c4": "push 0x1e5c",
              "seg7:0x12cc": "inc cx", "seg7:0x12dc": "dec ax",
              "seg7:0x12ee": "mov ax, word ptr [bx + 0x70a0]", "seg7:0x12fa": "cmp byte ptr [bx + 0x7174], 0",
              "seg7:0x130b": "mov ax, word ptr [bx + 0x70e0]", "seg7:0x130f": "add ax, 0x28",
              "seg7:0x1315": "cmp word ptr [bp - 0x12], 0", "seg7:0x131b": "cmp word ptr [bp - 0x12], 7",
              "seg7:0x132e": "mov ax, word ptr es:[bx + 0x28]", "seg7:0x1338": "imul cx",
              "seg7:0x133d": "mov word ptr es:[bx + 0x10], ax", "seg7:0x1346": "shl ax, 4",
              "seg7:0x1349": "imul cx, word ptr [bp - 0x10], 0x11", "seg7:0x135c": "cmp word ptr [bp - 0x12], 3",
              "seg7:0x1365": "add ax, ax", "seg7:0x1371": "cmp byte ptr [bx + 0x7179], 0",
              "seg7:0x1378": "mov ax, word ptr [bp - 0x10]", "seg7:0x1384": "mov word ptr es:[bx + si + 0x12], ax",
              "seg7:0x138c": "or ah, 0xe", "seg7:0x1390": "push 1",
              "seg7:0x1397": "push word ptr [bx + 0x39e8]", "seg7:0x139b": "push 0",
              "seg7:0x13a2": "mov word ptr [0x58a], 0"}
    assert _texts(ins, expect) == expect
    assert all(_calls(ins, {"seg7:0x12e4": "SetRect", "seg7:0x139d": "seg8:0xb76"}).values())


def test_radar_bitmap_and_mapping(ins, dgroup):
    """R01, R02, R03, R05, R07, R08, R09, R11: the radar's bitmap, centre, clip, scale and blit."""
    assert struct.unpack_from("<hh", dgroup, 0xA72) == (86, 71)
    assert struct.unpack_from("<i", dgroup, 0xA6C) == (120,)
    assert struct.unpack_from("<ii", dgroup, 0xA64) == (86, 71)
    expect = {
        "seg11:0xd": "cmp word ptr [0x5042], 0", "seg11:0x12": "jne seg11:0x1d8",
        "seg11:0x1b": "mov ax, 0x64", "seg11:0x1e": "imul word ptr [0x5a0]",
        "seg11:0x22": "mov word ptr [0x501c], ax", "seg11:0x25": "mov word ptr [0x501e], dx",
        "seg11:0x2c": "imul word ptr [0x5a2]", "seg11:0x30": "mov word ptr [0x5020], ax",
        "seg11:0x3b": "push word ptr [0xa72]", "seg11:0x3f": "push word ptr [0xa74]",
        "seg11:0x48": "mov word ptr [0x5042], ax", "seg11:0x187": "mov word ptr [0x93ce], ax",
        "seg11:0x18b": "push word ptr [0x5042]",
        "seg11:0xb0c": "mov ax, word ptr [0xa72]", "seg11:0xb12": "sar ax, 1",
        "seg11:0xb14": "mov word ptr [0x5050], ax", "seg11:0xb1f": "mov word ptr [0x5052], ax",
        "seg11:0x357f": "push 0x5048", "seg11:0x358c": "sub ax, word ptr [0x5050]", "seg11:0x3590": "neg ax",
        "seg11:0x3592": "inc ax", "seg11:0x35a6": "add cx, word ptr [0x5050]", "seg11:0x35aa": "dec cx",
        "seg11:0x35ac": "add dx, word ptr [0x5052]", "seg11:0x35b0": "dec dx",
        "seg11:0x7215": "mov eax, dword ptr [0xa6c]", "seg11:0x7226": "cmp eax, 0x1f4",
        "seg11:0x7268": "mov eax, dword ptr [0xa6c]", "seg11:0x7279": "cmp eax, 0x64",
        "seg11:0x723a": "sub ax, 0x258",
        "seg11:0x3418": "cmp word ptr [0x13dc], 0", "seg11:0x3635": "sub ax, 0x2b",
        "seg11:0x3639": "mov ax, word ptr [0x8e5a]", "seg11:0x363c": "add ax, 7",
        "seg11:0x3648": "push word ptr [0x93ce]", "seg11:0x3650": "push 0xcc",
        "seg11:0x35c7": "mov ax, word ptr [0x9326]", "seg11:0x35da": "cmp word ptr [bx - 0x6bfa], ax",
        "seg11:0x35ef": "cmp word ptr es:[bx + 0x18], 0", "seg11:0x35f6": "cmp word ptr es:[bx + 0x44], 0",
        "seg11:0x35fb": "jle seg11:0x35cf", "seg11:0x3605": "lcall es:[bx + 0x62]"}
    assert _texts(ins, expect) == expect
    assert all(_calls(ins, {"seg11:0x43": "CreateCompatibleBitmap", "seg11:0x182": "CreateCompatibleDC",
                            "seg11:0x18f": "SelectObject", "seg11:0x35b2": "SetRect",
                            "seg11:0x384d": "PtInRect", "seg11:0x39ad": "PtInRect",
                            "seg11:0x3a71": "PtInRect", "seg11:0x3c47": "PtInRect",
                            "seg11:0x7242": "SetScrollPos", "seg11:0x7247": "seg11:0x33cc",
                            "seg11:0x3655": "BitBlt", "seg11:0x3668": "ReleaseDC"}).values())
    c = (_trunc(86, 2), _trunc(71, 2))
    assert c == (43, 35)
    assert (1 - (43 - c[0]), 1 - (35 - c[1]), 43 + c[0] - 1, 35 + c[1] - 1) == (1, 1, 85, 69)
    # R09: RadarLeft/RadarTop are only read at start-up and written back under DGROUP:0x12a
    assert _sites(ins.refs(0x1DD4, 0x1DD7)) == ["seg6:0x19b", "seg6:0x1b1", "seg6:0x3b5", "seg6:0x3db"]
    assert struct.unpack_from("<h", dgroup, 0x12A) == (0,)
    assert _write_values(ins, 0x12A, 0x12B) == {"seg6:0x6c4": 0}


def test_radar_brushes_and_callbacks(ins):
    """R15, R16, R17, R18, R19, R20: the brush table DGROUP:0x5004 and the radar callbacks."""
    expect = {
        "seg11:0xe5": "push word ptr [0x5032]", "seg11:0xee": "mov word ptr [0x500a], ax",
        "seg11:0xf8": "mov word ptr [bp - 6], 0x60", "seg11:0xff": "mov word ptr [bp - 6], 0x40",
        "seg11:0x109": "push 0x200", "seg11:0x112": "mov word ptr [0x5004], ax",
        "seg11:0x115": "push 0x2ffc800", "seg11:0x120": "mov word ptr [0x5012], ax",
        "seg11:0x123": "push 0xffa080", "seg11:0x12e": "mov word ptr [0x5010], ax",
        "seg11:0x138": "mov word ptr [bp - 6], 0x80", "seg11:0x13f": "mov word ptr [bp - 6], 0x3f",
        "seg11:0x149": "push 0x2ff", "seg11:0x152": "mov word ptr [0x5008], ax",
        "seg11:0x155": "push 0xc0c0c0", "seg11:0x160": "mov word ptr [0x500c], ax",
        "seg11:0x163": "push 0x40", "seg11:0x165": "push 0xc0ff", "seg11:0x16d": "mov word ptr [0x5006], ax",
        "seg11:0x170": "push 0x20040ff", "seg11:0x17b": "mov word ptr [0x500e], ax",
        # callbacks
        "seg11:0x39c3": "push dword ptr es:[bx + di + 0x4c]", "seg11:0x39d5": "retf 6",
        "seg11:0x3a81": "mov bx, word ptr es:[bx + di + 0x46]", "seg11:0x3a9c": "sub ax, 0xc8",
        "seg11:0x3a9f": "mov cx, 0x96", "seg11:0x3aaa": "mov cx, 0xc8", "seg11:0x3ab5": "push 0xf0",
        "seg11:0x3c50": "cmp dword ptr [0xa6c], 0xc8", "seg11:0x3c59": "jle seg11:0x3c96",
        "seg11:0x3c80": "mov cx, 0x96", "seg11:0x3c88": "push ax", "seg11:0x3c89": "push ax",
        "seg11:0x3cad": "push dword ptr es:[bx + si + 0x4c]",
        "seg11:0x37af": "cmp word ptr [bp + 0xa], 0xe", "seg11:0x37ba": "cmp byte ptr [bx + 0x71d7], 0",
        "seg11:0x3884": "mov bx, word ptr es:[bx + si + 0x46]",
        "seg11:0x38c0": "imul ax, word ptr es:[bx + si + 0x28], 5", "seg11:0x38df": "push 0x50",
        "seg11:0x38e3": "push 0xc000ca",
        "seg11:0x38ee": "cmp word ptr [0x94e], 0", "seg11:0x3901": "mov word ptr [bx + 0x5054], ax",
        "seg11:0x3905": "mov word ptr [bx + 0x5056], dx"}
    assert _texts(ins, expect) == expect
    assert all(_calls(ins, {"seg11:0xe9": "CreatePatternBrush", "seg11:0x10d": "CreateSolidBrush",
                            "seg11:0x39c8": "SetPixel", "seg11:0x3a8b": "SelectObject",
                            "seg11:0x3aba": "PatBlt", "seg11:0x3c8f": "PatBlt", "seg11:0x3cb2": "SetPixel",
                            "seg11:0x38e9": "BitBlt"}).values())
    assert "seg10:0x6d38" in ins.at("seg11:0x4f").text and ins.at("seg11:0x4c").text == "push 0xa80"
    assert _cstr(unpacked_seg("inspace", 19), 0xA80) == b"METAL"
    # the PtInRect-fail branch reaches the race store at seg11:0x38ee
    assert "seg11:0x3854" in (WORK / "asm" / "inspace" / "seg11.asm").read_text() \
        .split("seg11:0x38ee ", 1)[1].split("\n", 1)[0]
    box = [(_trunc(z - 200, 150) + 2, _trunc(z - 200, 200) + 2) for z in (100, 349, 500)]
    assert box == [(2, 2), (2, 2), (4, 3)]


def test_pool8_radar_fields(ins):
    """R23: pool-8 records get radar_draw seg11:0x39d8 with brush 1 or 4 in seg9:0x10d4."""
    stores = {"seg9:0x11e7": (8, "mov word ptr es:[bx + si + 0x44], 0xdac"),
              "seg9:0x1218": (8, "mov word ptr es:[bx + si + 0x46], 1"),
              "seg9:0x18b8": (8, "mov word ptr es:[bx + si + 0x44], 0xdac"),
              "seg9:0x18cc": (8, "mov word ptr es:[bx + si + 0x46], 4"),
              "seg9:0x1aaf": (8, "mov word ptr es:[bx + si + 0x44], ax"),
              "seg9:0x1be0": (8, "mov word ptr es:[bx + si + 0x46], 4")}
    assert {s: (_pool_of(ins, s), ins.at(s).text) for s in stores} == stores
    assert ins.at("seg9:0x1aa3").text == "mov ax, word ptr [bx + 0x4d9a]"
    assert ins.at("seg9:0x1222").text == "mov word ptr es:[bx + si + 0x62], 0x39d8"
    assert 0xDAC == 3500


def test_icon_walk(ins, dgroup):
    """I01, I02, I04, I05, I11, I12: the wave icon walk and the IS_ICON window."""
    expect = {
        "seg9:0x46": "push 0x71c", "seg9:0x62": "cmp word ptr [0x9206], 9", "seg9:0xb8": "push 0x722",
        "seg9:0xd4": "cmp word ptr [0x9206], 9", "seg9:0xbf5": "push 8",
        "seg9:0x176": "push -1", "seg9:0x21f": "cmp word ptr [0x9206], 9",
        "seg9:0x229": "cmp word ptr [bp - 6], ax",
        "seg9:0x979": "cmp word ptr es:[bx + si + 0x18], 0",
        "seg9:0x99a": "mov byte ptr es:[bx + si + 0x1d0], 1", "seg9:0x9a0": "inc word ptr [0x9206]",
        "seg9:0x1ad": "cmp ax, 1",
        "seg7:0x6427": "cmp word ptr [0xa7c], 0", "seg7:0x642e": "cmp word ptr [0x9458], 0",
        "seg7:0x6435": "cmp word ptr [0x91fc], 0", "seg7:0x6443": "imul bx, word ptr [bp - 4], 0x1e",
        "seg7:0x6447": "mov word ptr [bx + 0x6eae], 0", "seg7:0x6450": "mov word ptr [bx + 0x6eac], ax",
        "seg7:0x6454": "mov word ptr [bx + 0x6ea8], 2", "seg7:0x645a": "imul ax, ax, 0x2e",
        "seg7:0x645f": "mov word ptr [bx + 0x6eaa], ax", "seg7:0x6466": "mov ax, word ptr [0x91fe]",
        "seg7:0x646f": "push 0xb99", "seg7:0x6473": "push 0xb98", "seg7:0x6476": "push 0x10cc",
        "seg7:0x6479": "push 0", "seg7:0x647b": "push word ptr [0x1de4]",
        "seg7:0x647f": "push word ptr [0x1de6]", "seg7:0x6483": "push 0x32", "seg7:0x6485": "push 0x96",
        "seg7:0x62d5": "mov cl, byte ptr [0xb6e]", "seg7:0x62d9": "mov ax, 0x32", "seg7:0x62dc": "sar ax, cl",
        "seg7:0x62e1": "mov word ptr es:[bx + 0x10], ax", "seg7:0x62eb": "mov word ptr es:[bx + 0xc], ax",
        "seg7:0x62fd": "imul cx, word ptr [0x91fe], 0x17", "seg7:0x6304": "add ax, 3",
        "seg7:0x630a": "mov word ptr es:[bx + 0x12], ax", "seg7:0x630e": "mov word ptr es:[bx + 0xe], ax",
        "seg7:0x631c": "imul cx, word ptr [0x91fe], 0x2e", "seg7:0x6323": "add ax, 6",
        "seg7:0x61e9": "push 0xb80"}
    assert _texts(ins, expect) == expect
    assert all(_calls(ins, {"seg9:0x58": "seg5:0x364", "seg9:0x8e": "seg9:0x156",
                            "seg9:0xca": "seg5:0x364", "seg9:0x100": "seg9:0x156",
                            "seg9:0x178": "ExtractIcon", "seg9:0x1a5": "ExtractIcon",
                            "seg9:0x18f": "seg9:0x94c", "seg9:0x1d2": "PatBlt", "seg9:0x1e0": "DrawIcon",
                            "seg9:0x1e8": "DestroyIcon", "seg7:0x6496": "CreateWindow",
                            "seg7:0x62f8": "GetSystemMetrics", "seg7:0x6317": "GetSystemMetrics"}).values())
    assert ins.at("seg9:0x1b0").mnem == "jbe"            # HICON > 1, unsigned
    assert dgroup[0x71C:0x728] == b"*.exe\x00*.dll\x00"
    assert (_cstr(dgroup, 0xB99), _cstr(dgroup, 0xB98), _cstr(dgroup, 0xB80)) == \
        (b"IS_ICON", b"", b"1/2x Scale")
    assert len(ins.call_sites("seg9:0x0")) == 1
    assert ins.call_sites("seg9:0x0") == ["seg9:0xbf7"] and ins.step(ins.at("seg9:0xbf5"), 1).off == 0xBF7


def _mark_segments(ins: Module, case: str) -> list[tuple]:
    """Emulate a case of seg11:0x2dac's direction switch: the MoveTo/LineTo points."""
    regs = {"[bp - 6]": "x", "[bp - 4]": "y"}
    stack, out, ax = [], [], None
    i = ins.at(case)
    for _ in range(80):
        t = i.text
        if t == "push word ptr [bp + 6]":
            stack.append("hdc")
        elif t.startswith("push word ptr [bp - "):
            stack.append((regs[t[14:]], 0))
        elif t.startswith("mov ax, word ptr [bp - "):
            ax = (regs[t[17:]], 0)
        elif t.startswith("add ax, "):
            ax = (ax[0], ax[1] + int(t[8:], 0))
        elif t == "push ax":
            stack.append(ax)
        elif i.mnem == "lcall" or "MoveTo" in t or "LineTo" in t:
            y, x, _hdc = stack.pop(), stack.pop(), stack.pop()
            assert x[0] == "x" and y[0] == "y"
            out.append(("M" if "MoveTo" in t else "L", x[1], y[1]))
        elif i.mnem == "jmp":
            tgt = i.ops[0]
            if tgt == "seg11:0x2f60":
                return out
            i = ins.at(tgt)
            continue
        i = ins.step(i, 1)
    raise AssertionError(case)


def test_direction_mark_segments(ins):
    """radar D03: the eight segments of seg11:0x2dac's 43 x 43 frame."""
    sw = ins.switches["seg11:0x2f4b"]
    assert (sw["table"], sw["count"]) == ("seg11:0x2f50", 8)
    assert ins.at("seg11:0x2f43").text == "cmp ax, 7"
    got = [_mark_segments(ins, t) for t in sw["targets"]]
    assert got == [
        [("M", 14, 0), ("L", 29, 0)],
        [("M", 29, 0), ("L", 43, 0), ("L", 43, 14)],
        [("M", 43, 14), ("L", 43, 29)],
        [("M", 29, 43), ("L", 43, 43), ("L", 43, 29)],
        [("M", 14, 43), ("L", 29, 43)],
        [("M", 14, 43), ("L", 0, 43), ("L", 0, 29)],
        [("M", 0, 14), ("L", 0, 29)],
        [("M", 14, 0), ("L", 0, 0), ("L", 0, 14)]]


def test_beam_slot_loop(ins, dgroup):
    """W38, W39: the wave-frame pre-loop of seg7:0x0 over the beam slots of seg8:0x3aec."""
    expect = {"seg7:0x93a": "mov word ptr [0x9136], ax", "seg7:0x942": "push dword ptr [bp - 6]",
              "seg7:0x94b": "mov word ptr [0x9136], ax", "seg7:0x94e": "inc word ptr [bp - 4]",
              "seg7:0x951": "mov ax, word ptr [0x598]", "seg7:0x954": "add ax, word ptr [0x594]",
              "seg7:0x95d": "inc dword ptr [0x5cc]", "seg7:0x965": "cmp word ptr [bp - 6], 5",
              "seg8:0x3afb": "imul bx, word ptr [bp + 8], 0x94", "seg8:0x3fba": "retf 4"}
    assert _texts(ins, expect) == expect
    assert "seg8:0x3aec" in ins.at("seg7:0x946").text
    assert struct.unpack_from("<hh", dgroup, 0x594) == (3, 0) and struct.unpack_from("<h", dgroup, 0x598) == (2,)
    assert _write_values(ins, 0x594, 0x595) == {"seg11:0x4e19": 7}
    assert ins.writers(0x598, 0x599) == []


def test_world_global_writers(ins):
    """globals.md Global table: the writers of the globals the wave set-up reads."""
    assert _write_values(ins, 0x932A, 0x932B) == {
        "seg6:0x120e": 130, "seg6:0x121c": 220, "seg6:0x1224": 310, "seg6:0x1232": 420,
        "seg6:0x1240": 2000, "seg6:0x1284": None}
    assert _write_values(ins, 0x9000, 0x9001) == {"seg6:0x1214": 0, "seg6:0x122a": 1, "seg6:0x1238": 2}
    assert _write_values(ins, 0x8FF8, 0x8FF9) == {"seg6:0x1209": None}
    assert _write_values(ins, 0x92EA, 0x92EB) == {"seg9:0x24cd": 14, "seg9:0x2540": None}
    assert _sites(ins.refs(0x92EA, 0x92EB)) == ["seg9:0x24cd", "seg9:0x2540", "seg10:0xedb"]
    assert _write_values(ins, 0x9318, 0x9319) == {
        "seg9:0x4496": 1, "seg9:0x44c3": None, "seg9:0x44dc": None, "seg9:0x44f8": None,
        "seg9:0x4680": None}
    assert _write_values(ins, 0x9322, 0x9323) == {"seg9:0x3de6": None}
    assert _write_values(ins, 0x92E8, 0x92E9) == {"seg6:0x3049": 0, "seg6:0x30e1": 0, "seg6:0x30e7": 1,
                                                  "seg6:0x310e": 0}
    assert _write_values(ins, 0x9326, 0x9327) == {"seg9:0x22cd": 0, "seg9:0x2588": 1, "seg18:0x428": 0,
                                                  "seg18:0x435": 1, "seg18:0x979": 15}
    assert _write_values(ins, 0x9158, 0x9159) == {"seg8:0x551": 2, "seg8:0x62a": 4, "seg8:0x741": 1}
    assert _write_values(ins, 0x94B0, 0x94B1) == {"seg6:0x36b6": 1, "seg6:0x36ed": None}
    assert _sites(ins.writers(0x94A2, 0x94A3)) == ["seg6:0x36f3", "seg6:0x3791", "seg6:0x9642",
                                                   "seg10:0x38d3"]
    # the icon walk's counters
    assert _write_values(ins, 0x9200, 0x9201) == {"seg9:0x1a": 0, "seg9:0x241": None, "seg9:0xaf9": None}
    assert _write_values(ins, 0x9202, 0x9203) == {"seg9:0x17": 0, "seg9:0xaa1": None}
    assert _write_values(ins, 0x9204, 0x9205) == {"seg9:0x12": None, "seg9:0x148": 8}
    assert _write_values(ins, 0x9206, 0x9207) == {"seg9:0x1d": 0, "seg9:0x9a0": None, "seg9:0xab7": None}
    assert _sites(ins.writers(0x91FE, 0x91FF)) == ["seg9:0x145"]
    assert {"seg9:0x13f", "seg10:0xc8"} == set(_sites(ins.writers(0x91FA, 0x91FB)))
    assert _write_values(ins, 0x95F6, 0x95F7) == {"seg13:0x18": None, "seg13:0x23": 0,
                                                  "seg18:0x404d": None, "seg18:0x4058": 0}
    assert ins.at("seg13:0x1c").text == "cmp word ptr [0x95f6], 8"


def test_pool_role_sites(ins):
    """structs.md Pool roles: DGROUP:0x934a indexes the rock mover's pool, DGROUP:0x9328 pool 5's mover."""
    assert ins.at("seg7:0x3bd2").text == "mov si, word ptr [0x934a]"
    assert ins.owners(*A("seg7:0x3bd2")) == {"seg7:0x3bae"}
    assert ins.at("seg7:0x4146").text == "mov si, word ptr [0x9328]"
    assert ins.owners(*A("seg7:0x4146")) == {"seg7:0x40c4"}
    assert ins.at("seg11:0x35da").text == "cmp word ptr [bx - 0x6bfa], ax"


def test_body_thrust_bytes_are_ship_record_fields(ins):
    """structs.md Ship records +0x5a, +0x5b, +0x5c: the body-thrust animation bytes sit in the
    ship record (DGROUP:0x7090 + 0x15a·i), not in the object record."""
    expect = {"seg13:0x2c0": "mov al, byte ptr [bx + 0x70ea]", "seg13:0x2c4": "add byte ptr [bx + 0x70ec], al",
              "seg13:0x2c8": "cmp byte ptr [bx + 0x7176], 0", "seg13:0x2d3": "cmp byte ptr [bx + 0x70eb], al",
              "seg13:0x2d9": "mov byte ptr [bx + 0x70ec], 0", "seg13:0x2fa": "mov byte ptr [bx + 0x70ea], 0xff",
              "seg13:0x311": "mov byte ptr [bx + 0x70ea], 1", "seg13:0x316": "imul bx, word ptr [bp - 0x10], 0x15a",
              "seg13:0x31b": "mov al, byte ptr [bx + 0x70ec]", "seg13:0x25f": "cmp byte ptr [bx + 0x7174], 0",
              "seg13:0x268": "cmp word ptr [bx + 0x7092], 0"}
    assert _texts(ins, expect) == expect
    assert (0x70EA - 0x7090, 0x70EB - 0x7090, 0x70EC - 0x7090) == (0x5A, 0x5B, 0x5C)


# ------------------------------------------------------------------ M3 stage 3: verified world facts


def test_ship_entry_pushes_only_the_first_later_ship(ins):
    """world.md Ship entry: the +0x88 store falls through out of the j loop, so only the first
    later waiting ship gets now + 5000; the time test is unsigned; the gate draw precedes the
    player's g = 0."""
    expect = {"seg7:0x1d40": "jg seg7:0x1d78", "seg7:0x1d51": "jne seg7:0x1d37",
              "seg7:0x1d65": "add ax, 0x1388", "seg7:0x1d70": "mov word ptr [bx + 0x7118], ax",
              "seg7:0x1d74": "mov word ptr [bx + 0x711a], dx",
              "seg7:0x1d78": "imul si, word ptr [bp - 4], 0x70", "seg7:0x1d85": "jne seg7:0x1ef3",
              "seg7:0x1d9d": "call far seg5:0xdc", "seg7:0x1da3": "idiv word ptr [0x9318]",
              "seg7:0x1db2": "mov word ptr [bp - 0xa], 0",
              "seg7:0x1f0a": "cmp word ptr [bx + 0x711a], dx", "seg7:0x1f0e": "ja seg7:0x1ef3",
              "seg7:0x1f10": "jb seg7:0x1f18", "seg7:0x1f16": "jae seg7:0x1ef3",
              "seg7:0x1f2c": "jmp seg7:0x1d3a"}
    assert _texts(ins, expect) == expect
    assert ins.step(ins.at("seg7:0x1d74"), 1).addr == "seg7:0x1d78"     # no jump back into the j loop
    assert 0x1388 == 5000 and 0x7118 - 0x7090 == 0x88


def _key_decode(ins: Module) -> dict[int, str]:
    """Emulate seg11:0x6b64's WM_KEYDOWN decode chain at seg11:0x6e42 for every key 0 ... 0xff."""
    out = {}
    for key in range(0x100):
        al, i = key, ins.at("seg11:0x6e45")
        assert i.text == "cmp ax, 0xbd"
        if key == 0xBD:
            out[key] = ins.step(i, 1).ops[0]
            continue
        if key > 0xBD:
            out[key] = ins.step(i, 2).ops[0]
            continue
        i = ins.step(i, 3)
        while True:
            if i.mnem == "sub":
                al = (al - int(i.ops[1], 0)) & 0xFF
            elif i.mnem == "dec":
                al = (al - 1) & 0xFF
            elif i.mnem == "jmp":
                out[key] = i.ops[0]
                break
            else:
                raise AssertionError(i.text)
            j = ins.step(i, 1)
            assert j.mnem == "je", j.text
            if al == 0:
                out[key] = j.ops[0]
                break
            i = ins.step(j, 1)
    return out


def test_wave_key_decode(ins):
    """world.md Key messages, Decode: the WM_KEYDOWN targets of seg11:0x6e42-0x6ee4."""
    got = _key_decode(ins)
    expect = {0x09: "0x71e3", 0x0D: "0x6f58", 0x2E: "0x6ee7", 0x41: "0x7065", 0x42: "0x71d4",
              0x43: "0x7019", 0x44: "0x6f20", 0x45: "0x70a5", 0x46: "0x6feb", 0x47: "0x6fff",
              0x48: "0x6f7a", 0x49: "0x716c", 0x4D: "0x71a9", 0x52: "0x718b", 0x53: "0x7033",
              0x54: "0x7132", 0x55: "0x7183", 0x56: "0x7187", 0x57: "0x7127", 0x58: "0x6f3a",
              0x59: "0x717f", 0x5A: "0x6f58", 0x6B: "0x71ef", 0xBB: "0x71ef", 0x6D: "0x724e",
              0xBD: "0x724e"}
    assert {k: got[k] for k in expect} == {k: "seg11:" + v for k, v in expect.items()}
    assert {v for k, v in got.items() if k not in expect} == {"seg11:0x72a1"}


def test_key_map_strings_overlap(ins):
    """world.md Router, Key map: strings 2900/2901 overlap in DGROUP and map only Q to H."""
    expect = {"seg6:0x1789": "push 0xb54", "seg6:0x178d": "push 0x94e4", "seg6:0x1790": "push 0x28",
              "seg6:0x179b": "push 0xb55", "seg6:0x179f": "push 0x94fa", "seg6:0x17a2": "push 0x28",
              "seg11:0xb313": "cmp byte ptr [0x94fa], 0x59"}
    assert _texts(ins, expect) == expect
    assert all(_calls(ins, {"seg6:0x1792": "LoadString", "seg6:0x17a4": "LoadString"}).values())
    assert (0xB54, 0xB55, 0x94FA - 0x94E4) == (2900, 2901, 0x16)
    a, b = _eng(2900), _eng(2901)
    assert (a, b) == ("YHADTGSCDVPBWGXSIEFSCM", "NQADTGSCDVPBWGXSIEFSCM")
    buf = bytearray(0x16 + 0x28)
    buf[0:len(a) + 1] = a.encode() + b"\0"
    buf[0x16:0x16 + len(b) + 1] = b.encode() + b"\0"          # 2901 overwrites 2900's tail
    assert buf[0x16] != ord("Y")

    def remap(key: int) -> int:
        for j in range(1, 22):
            if buf[0x16 + j] == key:
                return buf[j]
        return key
    assert {chr(k): chr(remap(k)) for k in range(0x20, 0x7F) if remap(k) != k} == {"Q": "H"}


def test_image_slot_tables(ins):
    """structs.md Image slots: four parallel 58-slot word arrays 0x74 bytes apart."""
    expect = {"seg10:0x544": "cmp word ptr [bp - 4], 0x3a", "seg10:0x562": "cmp word ptr [bx + 0x39e8], 0",
              "seg10:0x5c2": "cmp word ptr [bx + 0x3a5c], 0", "seg10:0x5e1": "push word ptr [bx + 0x3974]",
              "seg10:0x5fb": "cmp word ptr [si + 0x3ad0], 0"}
    assert _texts(ins, expect) == expect
    assert "seg10:0x552" in ins.at("seg10:0x53b").text
    assert 0x39E8 - 0x3974 == 0x3A5C - 0x39E8 == 0x3AD0 - 0x3A5C == 0x74 == 2 * 0x3A == 2 * 58
    assert ((0x3A02 - 0x39E8) // 2, (0x3A28 - 0x39E8) // 2, (0x3A38 - 0x39E8) // 2) == (13, 0x20, 0x28)
    assert (0x39F4 - 0x39E8) // 2 == 6 and (0x3ADC - 0x3AD0) // 2 == 6


def test_rock_image_sizes(ins):
    """world.md Rock images: seg10:0x3a2e's 869 x 53 and 473 x 31 bitmaps from 43, 21 and gap 5."""
    expect = {"seg10:0x3a46": "mov word ptr [bp - 0x30], 0x2b",
              "seg10:0x3a4b": "mov dword ptr [bp - 0x34], 0x50015",
              "seg10:0x3acd": "imul ax, word ptr [bp - 0x32], 0x13",
              "seg10:0x3ad1": "imul cx, word ptr [bp - 0x30], 0x12",
              "seg10:0x3b0d": "imul ax, word ptr [bp - 0x34], 0x12",
              "seg10:0x3afc": "mov word ptr [0x3adc], ax"}
    assert _texts(ins, expect) == expect
    assert all(_calls(ins, {"seg10:0x3af7": "CreateBitmap", "seg10:0x3b05": "CreateCompatibleBitmap",
                            "seg10:0x3b28": "CreateBitmap", "seg10:0x3b36": "CreateCompatibleBitmap"}).values())
    big, small, gap = 0x2B, 0x15, 5
    assert (gap * 19 + big * 18, 2 * gap + big) == (869, 53)
    assert (gap * 19 + small * 18, 2 * gap + small) == (473, 31)
    assert _dib(_resource("IGRAPHIC.DL1", 2, "ROCKS"))[:3] == (18 * 43, 43, 1)


def test_hud_graphics(ins, dgroup):
    """world.md Graphics: seg11:0x0's loads, DIGITAL and SYSTEMS, the lamp cache and clock strings."""
    expect = {"seg11:0x57": "mov word ptr [0x5032], ax", "seg11:0x5e": "push 0xfa",
              "seg11:0x69": "mov word ptr [0x5034], ax", "seg11:0x70": "push 0xf0",
              "seg11:0x7b": "mov word ptr [0x5036], ax", "seg11:0x87": "push 0x2d",
              "seg11:0x91": "mov word ptr [0x5044], ax", "seg11:0x94": "cmp word ptr [0x90f0], 0",
              "seg11:0xa3": "push 3", "seg11:0xa5": "push 0x104", "seg11:0xb0": "mov word ptr [0x503a], ax",
              "seg11:0xb4": "push 0xa86", "seg11:0xbf": "mov word ptr [0x5038], ax",
              "seg11:0xc3": "push 0xa8e", "seg11:0xce": "mov word ptr [0x5046], ax",
              "seg11:0xd2": "push 0xa96", "seg11:0xdd": "mov word ptr [0x5030], ax",
              "seg11:0x1d2": "mov word ptr [0x93d8], 1"}
    assert _texts(ins, expect) == expect
    assert (0xFA, 0xF0, 0x2D, 0x104) == (250, 240, 45, 260)
    assert [_cstr(dgroup, o) for o in (0xA80, 0xA86, 0xA8E, 0xA96)] == [b"METAL", b"DIGITAL", b"RADSHIP",
                                                                       b"SYSTEMS"]
    assert _dib(_resource("IGRAPHIC.DL1", 2, "DIGITAL"))[:3] == (14 * 10, 10, 1)
    assert _dib(_resource("IGRAPHIC.DL1", 2, "SYSTEMS"))[:3] == (32, 7 * 11, 4)
    assert _dib(_resource("IGRAPHIC.DL1", 4000, 45))[:2] == (3 * 58, 58)
    assert _dib(_resource("IGRAPHIC.DL1", 4000, 240))[:2] == (2 * 20, 20)     # the red button's cells
    assert _dib(_resource("IGRAPHIC.DL1", 4000, 250))[:2] == (27, 3 * 6)      # three lamp rows
    assert _dib(_resource("IGRAPHIC.DL1", 4000, 260))[:2] == (14, 60)
    assert dgroup[0xAA2:0xAAA] == b"\xfe\xff" * 4                          # lamp cache, -2 each
    assert (_cstr(dgroup, 0xAC7), _cstr(dgroup, 0xAC9)) == (b" ", b"0")


def test_radar_brush_creation_order(ins):
    """world.md Radar colours: seg11:0x0 creates the brush table entries in the order 3, 0, 7, 6, 2, 4, 1, 5."""
    stores = [i for i in ins.func_insns("seg11:0x0")
              if i.mnem == "mov" and i.abs_ds and 0x5004 <= i.mem.disp <= 0x5012 and i.ops[1] == "ax"]
    assert [(i.mem.disp - 0x5004) // 2 for i in stores] == [3, 0, 7, 6, 2, 4, 1, 5]


def test_world_frame_cycles(ins):
    """world.md Update loop step 8 and Ships Head: DGROUP:0x9142 0 ... 54, DGROUP:0x134e 0 ... 100,
    the 9000 ms deadline DGROUP:0x1350 and the pool-14 counts."""
    expect = {"seg7:0x9dc": "inc word ptr [0x9142]", "seg7:0x9e0": "cmp word ptr [0x9142], 0x36",
              "seg7:0x9e7": "mov word ptr [0x9142], 0", "seg7:0xb2d": "inc dword ptr [0x5a8]",
              "seg7:0xb32": "inc dword ptr [0x1e68]",
              "seg13:0x29": "inc word ptr [0x134e]", "seg13:0x2d": "cmp word ptr [0x134e], 0x64",
              "seg13:0x34": "mov word ptr [0x134e], 0", "seg13:0x44": "mov word ptr [bx - 0x6bfa], ax",
              "seg13:0x48": "mov word ptr [bx - 0x6d14], ax", "seg13:0x60": "add ax, 0x2328"}
    assert _texts(ins, expect) == expect
    assert (0x36, 0x64, 0x2328) == (54, 100, 9000)
    assert (0x10000 - 0x6BFA, 0x10000 - 0x6D14) == (0x9406, 0x92EC)


def test_speed_cap_and_autopilot_step(ins):
    """structs.md ship +0x61, +0xda; world.md Thrust: the speed cap and the autopilot step."""
    expect = {"seg11:0x5c5d": "cmp byte ptr [bx + 0x70f1], 0", "seg11:0x5c64": "cmp word ptr [bx + 0x70ba], 0x4b",
              "seg11:0x5c75": "cmp word ptr [bx + 0x70c2], 0xc",
              "seg11:0x5cc6": "imul ax, word ptr [bx + 0x709c], 0x13",
              "seg11:0x5ccc": "idiv word ptr [bx + 0x709e]", "seg11:0x5cd0": "mov word ptr [bx + 0x716a], ax",
              "seg11:0x5f2a": "cmp ax, 0x28", "seg11:0x5f56": "cmp ax, 0x1e", "seg11:0x5f70": "cmp ax, 0x1e"}
    assert _texts(ins, expect) == expect
    assert [d - 0x7090 for d in (0x70F1, 0x70BA, 0x70C2, 0x709C, 0x709E, 0x716A)] == \
        [0x61, 0x2A, 0x32, 0x0C, 0x0E, 0xDA]
    assert (0x4B, 0xC, 0x13) == (75, 12, 19)
    assert _trunc(_w16(205 * 19), 25) == 155


def test_icon_dissolve_and_direction_table(ins):
    """world.md Icon dissolve: size and gap from pool-0 record 0; Direction mark Table: +0x1a, +0x1c."""
    expect = {"seg7:0x3a51": "les bx, ptr [0x1df8]", "seg7:0x3a55": "mov ax, word ptr es:[bx + 0x22]",
              "seg7:0x3a5c": "mov ax, word ptr es:[bx + 0x24]",
              "seg13:0x2539": "cmp word ptr [0x6e72], -1", "seg13:0x2548": "mov ax, word ptr [0x6e70]",
              "seg13:0x2553": "mov ax, word ptr [0x6e90]", "seg13:0x2559": "mov ax, word ptr [0x6e8e]"}
    assert _texts(ins, expect) == expect
    assert [(0x6E38 + 0x1E * d + 0x1A, 0x6E38 + 0x1E * d + 0x1C) for d in range(3)] == \
        [(0x6E52, 0x6E54), (0x6E70, 0x6E72), (0x6E8E, 0x6E90)]
    assert 0x6E44 - 0x6E38 == 0xC


def test_probe_wave_dump():
    """world.md Probe wave, structs.md Probe values, world.md Refresh: values of the probe-0001
    DGROUP images."""
    dumps = WORK / "probe" / "dumps"
    wave = require(dumps / "wave-inspace.bin").read_bytes()
    w = lambda o: struct.unpack_from("<h", wave, o)[0]
    assert struct.unpack_from("<I", wave, 0x144C)[0] == 0xE43E04A1
    assert {o: w(o) for o in (0x9320, 0x9318, 0x933A, 0x933C, 0x940C, 0x940E, 0x9406 + 16, 0x9406 + 24,
                              0x992, 0x94E2, 0x9332, 0x932E, 0x9346, 0x93D6, 0x93C0, 0x93C2, 0x9646)} == {
        0x9320: 0, 0x9318: 1, 0x933A: 24, 0x933C: 14, 0x940C: 38, 0x940E: 3, 0x9416: 2, 0x941E: 2,
        0x992: 50, 0x94E2: 38, 0x9332: 2, 0x932E: 0, 0x9346: -1, 0x93D6: 6, 0x93C0: 580, 0x93C2: 608,
        0x9646: 0}
    assert [w(0x6678 + 2 * k) for k in range(4)] == [6424, 14032, 7624, 14080]
    b = 0x7090
    assert [w(b + o) for o in (0x0C, 0x0E, 0x14, 0x26, 0x28, 0x2A, 0x2C, 0x1E, 0x18, 0xDA)] == \
        [205, 25, 21, 100, 100, 100, 100, 1320, 1, 155]
    assert [wave[b + o] for o in (0xAE, 0xAF, 0xB9, 0xCB, 0xE4, 0xE5, 0xE9, 0xEC, 0x148, 0x149)] == \
        [1, 20, 8, 2, 1, 1, 1, 1, 0xFF, 0xFF]
    assert _cstr(wave, 0x9742) == b"TRANSFORMER" and 40 * 33 == 1320
    for name in ("title", "navigation"):
        d = require(dumps / f"{name}-inspace.bin").read_bytes()
        assert struct.unpack_from("<h", d, 0x93D6)[0] == 0
    nav = (dumps / "navigation-inspace.bin").read_bytes()
    assert struct.unpack_from("<h", nav, 0x9346)[0] == 0
