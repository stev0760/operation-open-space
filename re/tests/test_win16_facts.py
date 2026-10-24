"""Mechanical facts of docs/re/win16-translation.md.

CLAUDE.md asks for a test behind every count, site list and tally in docs/.
These tests read the ``isre asm`` listings and indexes (``work/asm/<module>/``),
the unpacked segments and relocations (``work/unpacked/<module>/``), the NE
summaries (``work/ne/``) and the ``isre res`` dialogs (``work/assets/``), and
skip when those are absent. FACTORY.EXE is the shipdisk copy
(``factory-1998a``).

Conventions of this file:

* A *call site* is a call instruction; import call sites come from
  ``xrefs.json``.
* Import arguments come from a forward simulation of the stack (``CallArgs``).
  Each Win16 API is ``pascal`` with the argument sizes of Wine's ``.spec``
  files (``re/wine-specs/``); internal callees pop what their ``ret`` operand
  says. Constants are followed through ``mov``, ``xor``/``sub`` of a register
  with itself and ``cwd``/``cdq``. Where jumps join, a stack word or register
  keeps its value only when every incoming path agrees, so a call instruction
  that two paths share with different pushes has a *computed* argument.
* A message a procedure *decodes* is a value its msg parameter
  (``[bp + 0xc]``) is tested for equality with: a ``cmp`` against the
  parameter, or a chain of ``sub``/``dec``/``cmp`` on AX (or AL below a range
  check) loaded from it, followed by ``je``/``jne``, and the cases of a jump
  table on it.
* Where the bytes disagree with the doc, the test pins the bytes and a
  companion ``test_doc_*`` marked ``xfail(strict=True)`` states the doc's
  claim.
"""
from __future__ import annotations

import functools
import json
import re
import struct
from collections import Counter, defaultdict

import pytest
from conftest import REPO_ROOT, require

from isre.asm import parse_line
from isre.x86names import MODULE_SPECS, default_spec_dir, load_specs
from test_re_facts import (WORK, _FAMILY, _IMM_RE, _PREFIXES, A, Module, _cstr, module,
                           unpacked_seg, unpacked_segs, writes_reg)

NAMES = ("inspace", "ispace", "factory-1998a")
RTL = {"inspace": 5, "ispace": 3, "factory-1998a": 3}


@pytest.fixture(scope="module")
def ins() -> Module:
    return module("inspace")


@pytest.fixture(scope="module")
def isp() -> Module:
    return module("ispace")


@pytest.fixture(scope="module")
def fac() -> Module:
    return module("factory-1998a")


def sites(m: Module, imp: str) -> list[str]:
    """Call sites of an import named ``MODULE.ordinal Name``, in address order."""
    return sorted(m.xj["imports"].get(imp, []), key=A)


def nsites(m: Module, imp: str) -> int:
    return len(m.xj["imports"].get(imp, []))


def exports(m: Module) -> dict[int, str]:
    return {int(e.split()[0]): f["addr"] for f in m.fj["functions"] for e in f["export"]}


def owner(m: Module, a: str) -> str:
    (f,) = m.owners(*A(a))
    return f


def text(m: Module, a: str) -> str:
    return m.at(a).text


def prefixes(raw: bytes) -> bytes:
    n = 0
    while n < len(raw) and raw[n] in _PREFIXES:
        n += 1
    return raw[:n]


# ------------------------------------------------------------------ import arguments

_SPEC_RE = re.compile(r"(\d+)\s+(pascal|varargs|cdecl)\s+(?:-\S+\s+)*\w+\(([^)]*)\)")
_DWORD_ARGS = {"long", "ptr", "str", "segptr", "segstr", "wstr"}
_SEGREGS = {"cs", "ds", "es", "ss", "fs", "gs"}
_LOOPS = {"loop", "loope", "loopne", "loopz", "loopnz", "jcxz", "jecxz"}
_END = {"jmp", "ljmp", "ret", "retf", "iret"}


@functools.lru_cache(maxsize=None)
def spec_args() -> dict[str, tuple[bool, tuple[int, ...]]]:
    """{``MODULE.ordinal``: (callee pops, argument sizes in words)} from Wine's specs."""
    out = {}
    for mod, fname in MODULE_SPECS.items():
        p = default_spec_dir() / fname
        if not p.is_file():
            continue
        for line in p.read_text(encoding="latin-1").splitlines():
            m = _SPEC_RE.match(line.split("#", 1)[0].strip())
            if m:
                out[f"{mod}.{m.group(1)}"] = (
                    m.group(2) == "pascal",
                    tuple(2 if t in _DWORD_ARGS else 1 for t in m.group(3).split()))
    return out


def _imm(op: str, bits: int = 16):
    return int(op, 0) & ((1 << bits) - 1) if _IMM_RE.fullmatch(op) else None


def _is_jump(i) -> bool:
    return (i.mnem.startswith("j") or i.mnem in _LOOPS) and bool(i.ops) \
        and i.ops[0].startswith("seg")


def _merge_stacks(stacks: list[list]) -> list:
    if len({len(s) for s in stacks}) != 1:
        return list(stacks[0])
    return [col[0] if all(c == col[0] for c in col) else None for col in zip(*stacks)]


def _merge_regs(regs: list[dict]) -> dict:
    return {r: v for r, v in regs[0].items() if all(o.get(r) == v for o in regs[1:])}


class CallArgs:
    """Arguments of every import call of a module (see the module docstring).

    ``calls[site] = (import, args)``: one entry per spec argument, an int for
    a constant, ``(segreg, offset)`` for a far pointer into a segment register
    and None when computed.
    """

    def __init__(self, m: Module):
        self.m = m
        self.specs = spec_args()
        self.calls: dict[str, tuple[str, list]] = {}
        rets = defaultdict(set)
        for i in m.insns:
            if i.mnem in ("ret", "retf"):
                for f in m.owners(i.seg, i.off):
                    rets[f].add(int(i.ops[0], 0) if i.ops else 0)
        self.pops = {f: v.pop() // 2 for f, v in rets.items() if len(v) == 1}
        self.sources = defaultdict(list)
        for i in m.insns:
            if _is_jump(i):
                self.sources[i.ops[0]].append(A(i.addr))
        self.switch_at = {}
        for s in m.fj["switches"]:
            self.switch_at[s["at"]] = s["targets"]
            for t in s["targets"]:
                self.sources[t].append(A(s["at"]))
        self._run()

    def args(self, imp: str) -> dict[str, list]:
        return {a: v for a, (k, v) in self.calls.items() if k == imp}

    def _run(self) -> None:
        m = self.m
        stack, regs, saved, prev = [], {}, defaultdict(list), None
        for i in m.insns:
            fall = prev is not None and prev.seg == i.seg and prev.mnem not in _END
            if i.addr in m.funcs or prev is None or prev.seg != i.seg:
                saved.pop(i.addr, None)
                stack, regs = [], {}
            elif i.addr in self.sources:
                states = saved.pop(i.addr, []) + ([(stack, regs)] if fall else [])
                back = any(s >= A(i.addr) for s in self.sources[i.addr])
                if states:
                    stack = _merge_stacks([s for s, _r in states])
                    regs = {} if back else _merge_regs([r for _s, r in states])
                else:
                    stack, regs = [], {}
            elif not fall:
                stack, regs = [], {}
            prev = i
            self._step(i, stack, regs)
            if _is_jump(i):
                saved[i.ops[0]].append((list(stack), dict(regs)))
            for t in self.switch_at.get(i.addr, ()):
                saved[t].append((list(stack), dict(regs)))

    @staticmethod
    def _value(op: str, regs: dict, bits: int = 16):
        v = _imm(op, bits)
        return v if v is not None else regs.get(op)

    @staticmethod
    def _kill(reg: str, regs: dict) -> None:
        for r in [r for r in regs if _FAMILY[r] == _FAMILY[reg]]:
            del regs[r]

    def _step(self, i, stack: list, regs: dict) -> None:
        mn, ops = i.mnem, i.ops
        wide = 0x66 in prefixes(i.raw)
        if mn == "push":
            op = ops[0]
            if op in _SEGREGS:
                stack.extend([op] * (2 if wide else 1))
            elif wide or op.startswith("dword"):
                v = self._value(op, regs, 32)
                stack.extend([None, None] if v is None else [v >> 16, v & 0xFFFF])
            else:
                v = self._value(op, regs)
                stack.append(None if v is None else v & 0xFFFF)
            return
        if mn in ("pop", "popf", "popa", "pushf", "pusha"):
            n = 8 if mn in ("popa", "pusha") else 2 if wide else 1
            if mn.startswith("push"):
                stack.extend([None] * n)
            else:
                del stack[max(0, len(stack) - n):]
        elif ops and ops[0] in ("sp", "esp"):
            k = _imm(ops[1]) if len(ops) > 1 else None
            if mn == "add" and k is not None:
                del stack[max(0, len(stack) - k // 2):]
            elif mn == "sub" and k is not None:
                stack.extend([None] * (k // 2))
            else:
                stack.clear()
        elif mn == "leave":
            stack.clear()
        elif mn in ("call", "lcall"):
            self._call(i, stack)
        elif mn == "mov" and len(ops) == 2 and ops[0] in _FAMILY:
            v = self._value(ops[1], regs, 32 if ops[0].startswith("e") else 16)
            self._kill(ops[0], regs)
            if v is not None and not ops[0].endswith(("l", "h")):
                regs[ops[0]] = v
            return
        elif mn in ("xor", "sub") and len(ops) == 2 and ops[0] == ops[1] and ops[0] in _FAMILY:
            self._kill(ops[0], regs)
            regs[ops[0]] = 0
            return
        elif mn == "cdq":                      # 0x99: CWD, or CDQ with the 0x66 prefix
            src, dst, top = ("eax", "edx", 31) if wide else ("ax", "dx", 15)
            v = regs.get(src)
            self._kill(dst, regs)
            if v is not None:
                regs[dst] = ((1 << (top + 1)) - 1) if v >> top else 0
            return
        for r in [r for r in regs if writes_reg(i, r)]:
            del regs[r]

    def _call(self, i, stack: list) -> None:
        imp = re.match(r"call far ([A-Z0-9]+\.\d+) ", i.text + " ")
        if imp:
            spec = self.specs.get(imp.group(1))
            if spec is None:
                stack.clear()
                return
            callee_pops, words = spec
            total = sum(words)
            got = ([None] * total + stack)[-total:] if total else []
            args, k = [], 0
            for w in words:
                if w == 1:
                    args.append(got[k])
                else:
                    hi, lo = got[k], got[k + 1]
                    if isinstance(hi, int) and isinstance(lo, int):
                        args.append(hi << 16 | lo)
                    elif isinstance(hi, str) and isinstance(lo, int):
                        args.append((hi, lo))
                    else:
                        args.append(None)
                k += w
            self.calls[i.addr] = (imp.group(1), args)
            if callee_pops:
                del stack[max(0, len(stack) - total):]
            return
        if re.fullmatch(r"call seg\d+:0x[0-9a-f]+", i.text) and stack and stack[-1] == "cs":
            stack.pop()                                   # push cs; call near: a far call
        target = re.fullmatch(r"call (?:far )?(seg\d+:0x[0-9a-f]+)", i.text)
        n = self.pops.get(target.group(1)) if target else None
        if n is None:
            stack.clear()
        else:
            del stack[max(0, len(stack) - n):]


@functools.lru_cache(maxsize=None)
def call_args(name: str) -> CallArgs:
    return CallArgs(module(name))


def test_call_args_cover_every_import_call():
    """Every import call with a spec entry gets its arguments recorded."""
    for name in NAMES:
        m, ca = module(name), call_args(name)
        for imp, calls in m.xj["imports"].items():
            if imp.split()[0] in ca.specs:
                assert set(calls) <= set(ca.calls), (name, imp)


# ------------------------------------------------------------------ relocations and imports

_RELOC_RE = re.compile(r"(?:(seg\d+:0x[0-9a-f]+) )?reloc ([A-Z0-9]+\.\d+ \S+)")


@functools.lru_cache(maxsize=None)
def reloc_sites(name: str) -> dict[str, list[str]]:
    """Sites of the relocations that the listings annotate (all but import calls)."""
    out = defaultdict(list)
    for p in sorted(require(WORK / "asm" / name).glob("seg*.asm")):
        for line in p.read_text().splitlines():
            row = parse_line(line)
            if row is None:
                continue
            seg, off, _raw, _text, comment = row
            for part in comment.split("; "):
                m = _RELOC_RE.fullmatch(part.strip())
                if m:
                    out[m.group(2)].append(m.group(1) or f"seg{seg}:{off:#x}")
    return dict(out)


@functools.lru_cache(maxsize=None)
def ne_info(name: str) -> dict:
    return json.loads(require(WORK / "ne" / f"{name}.json").read_text())


@functools.lru_cache(maxsize=None)
def relocs(name: str) -> dict:
    return json.loads(require(WORK / "unpacked" / name / "relocs.json").read_text())


def imported(name: str) -> set[tuple[str, int]]:
    """(module, ordinal) of every import record: packed segments and the loader stub."""
    out = {(r["module"], r["ordinal"]) for r in relocs(name)["relocs"]
           if r["kind"] == "import_ordinal"}
    for recs in ne_info(name)["segment_relocations"].values():
        out |= {(r["module"], r["ordinal"]) for r in recs if r["target_type"] == "IMPORTORDINAL"}
    return out


@functools.lru_cache(maxsize=None)
def ordinal_of() -> dict[tuple[str, str], int]:
    return {(mod, n): o for mod, table in load_specs(default_spec_dir()).items()
            for o, n in table.items()}


# ------------------------------------------------------------------ code generation


def test_operand_size_prefix_counts():
    """Code generation: instructions with the 0x66 prefix: 5,147, none, 439."""
    counts = {n: sum(1 for i in module(n).insns if 0x66 in prefixes(i.raw)) for n in NAMES}
    assert counts == {"inspace": 5147, "ispace": 0, "factory-1998a": 439}


def test_no_x87_instructions():
    """Floating point: no decoded instruction has an x87 opcode (0xD8-0xDF) or WAIT."""
    for n in NAMES:
        hits = [i.addr for i in module(n).insns
                if (lambda op: 0xD8 <= op <= 0xDF or op == 0x9B)(i.raw[len(prefixes(i.raw))])]
        assert hits == [], n


def test_no_float_fixups_or_win87em():
    """Floating point: no OSFIXUP records, no WIN87EM, interrupts 21h, 2Fh and 31h only."""
    ints = set()
    for n in NAMES:
        assert "WIN87EM" not in relocs(n)["modules"].values()
        assert "WIN87EM" not in ne_info(n)["modules"]
        assert not [r for r in relocs(n)["relocs"] if r["kind"] not in ("import_ordinal",
                                                                        "special_base")]
        for recs in ne_info(n)["segment_relocations"].values():
            assert not [r for r in recs if r["target_type"] == "OSFIXUP"]
        ints |= {int(i.ops[0], 0) for i in module(n).insns if i.mnem == "int"}
        assert not [i.addr for i in module(n).insns if i.mnem in ("int3", "into")]
    assert ints == {0x21, 0x2F, 0x31}


def test_code_bytes_outside_the_stub_decode():
    """Floating point: all code outside seg1 decodes but padding and 11 bytes of ISPACE seg3."""
    for n in NAMES:
        left = {s["seg"]: s["undecoded"] - s["undecoded_padding"]
                for s in module(n).fj["segments"] if s["seg"] != 1}
        assert {s: k for s, k in left.items() if k} == ({3: 11} if n == "ispace" else {}), n
    assert [u["addr"] for u in module("ispace").fj["undecoded"]
            if not u["padding"] and not u["addr"].startswith("seg1:")] == ["seg3:0x49"]


# ------------------------------------------------------------------ reading the listings


def test_listing_prints_cwd_and_cbw_as_cdq_and_cwde(ins):
    """Reading the listings: 99 prints as cdq and 98 as cwde without the prefix."""
    for a, raw, txt in (("seg2:0x3e", "99", "cdq"), ("seg4:0x330f", "98", "cwde"),
                        ("seg8:0x12b0", "6699", "cdq")):
        assert (ins.at(a).raw.hex(), text(ins, a)) == (raw, txt), a


def test_dword_pushes_carry_two_arguments(ins):
    """Reading the listings: push imm32 supplies msg and wParam; WndProc's dword pushes."""
    assert text(ins, "seg6:0x23b8") == "push 0x112f060"
    assert call_args("inspace").calls["seg6:0x23c2"] == ("USER.110", [None, 0x112, 0xF060, 0])
    assert text(ins, "seg6:0x2507") == "push 0x41500c8"
    assert call_args("inspace").calls["seg6:0x2511"][1][1:3] == [0x415, 200]
    assert [text(ins, a) for a in ("seg2:0x445", "seg2:0x449", "seg2:0x44d", "seg2:0x450")] == [
        "push dword ptr [bp + 0xc]", "push dword ptr [bp + 8]", "push word ptr [bp + 6]",
        "call far USER.107 DefWindowProc"]


def test_outer_arguments_pushed_before_an_inner_call(ins):
    """Reading the listings: CB_SETCURSEL at seg6:0x2840 belongs to SendMessage seg6:0x285e."""
    assert text(ins, "seg6:0x2840") == "push 0x40e"
    assert text(ins, "seg6:0x2851") == "call far KERNEL.127 GetPrivateProfileInt"
    assert call_args("inspace").calls["seg6:0x285e"][1][1] == 0x40E


# ------------------------------------------------------------------ integer semantics

CITED_INSNS = {
    "inspace": {
        "seg7:0xa44": "cdq", "seg9:0x4180": "imul word ptr [0x9ea2]", "seg9:0x4187": "adc dx, 0",
        "seg9:0x418b": "push ax", "seg9:0x4190": "imul ecx", "seg4:0x3b73": "cwde",
        "seg7:0x17b": "jl seg7:0x1aa", "seg7:0x149": "jae seg7:0x153",
        "seg7:0x2b": "cmp word ptr [0x1e56], dx", "seg7:0x2f": "ja seg7:0xb0",
        "seg7:0x37": "jae seg7:0xb0", "seg2:0x41": "sar ax, 1", "seg3:0x2498": "sar ax, 2",
        "seg4:0x1649": "sar ax, cl", "seg8:0x1a04": "imul ecx", "seg8:0x1a07": "cdq",
        "seg8:0x1a09": "idiv dword ptr [bp - 6]", "seg4:0x3310": "idiv cl",
    },
    "ispace": {
        "seg10:0x636d": "sub ah, ah", "seg2:0x888": "jg seg2:0x89d", "seg2:0x88a": "jl seg2:0x891",
        "seg2:0x88f": "jae seg2:0x89d", "seg5:0x6881": "sub ah, ah",
        "seg5:0x6883": "div byte ptr [bp + 0xa]", "seg6:0x51a": "cdq", "seg6:0x529": "shl dx, cl",
        "seg6:0x2449": "add ax, 1", "seg6:0x244c": "sbb cx, cx", "seg6:0x244e": "and cx, 0xffff",
        "seg6:0x2452": "add dx, cx",
    },
}


def test_cited_integer_instructions(ins, isp):
    """Integer semantics, Division, Huge pointers: the instructions the doc cites."""
    for m in (ins, isp):
        for a, txt in CITED_INSNS[m.name].items():
            assert text(m, a) == txt, (m.name, a)
    assert ins.at("seg7:0xa44").raw == b"\x99" and ins.step(ins.at("seg7:0xa44"), -1).text \
        .startswith("imul word ptr")
    assert ins.at("seg8:0x1a07").raw == b"\x66\x99"
    assert ins.step(ins.at("seg7:0x149"), -1).text.startswith("cmp eax, ")
    assert [text(ins, a) for a in ("seg2:0x3e", "seg2:0x3f")] == ["cdq", "sub ax, dx"]
    assert [ins.step(ins.at("seg3:0x2498"), k).text for k in (-3, -2, -1)] == [
        "cdq", "and dx, 3", "add ax, dx"]
    assert ins.step(ins.at("seg4:0x1649"), -1).text == "mov cl, byte ptr [0x9156]"
    assert isp.at("seg6:0x244e").raw[2:] == b"\xff\xff"      # __AHINCR is relocated in
    assert "reloc KERNEL.114 __AHINCR" in _comment(isp, "seg6:0x244e")


def _comment(m: Module, a: str) -> str:
    seg, off = A(a)
    for line in (WORK / "asm" / m.name / f"seg{seg:02d}.asm").read_text().splitlines():
        row = parse_line(line)
        if row and row[0] == seg and row[1] == off:
            return row[4]
    raise AssertionError(a)


# ------------------------------------------------------------------ division


def _divisions(m: Module) -> Counter:
    """(width, mnemonic, previous instruction) of each div and idiv outside the RTL."""
    out = Counter()
    for n, i in enumerate(m.insns):
        if i.mnem not in ("div", "idiv") or i.seg in (1, RTL[m.name]):
            continue
        op = i.ops[0]
        width = 1 if op.startswith("byte") or op in _BYTE_REGS else \
            4 if op.startswith("dword") or op.startswith("e") else 2
        prev = m.insns[n - 1]
        out[(width, i.mnem, prev.raw.hex() if prev.mnem in ("cdq", "cwde") else prev.text)] += 1
    return out


_BYTE_REGS = {"al", "ah", "bl", "bh", "cl", "ch", "dl", "dh"}


def test_division_forms():
    """Division and shifts: 16-bit and byte divisions outside the RTL and their set-up."""
    forms = {n: {k: v for k, v in _divisions(module(n)).items() if k[0] < 4} for n in NAMES}
    assert forms["inspace"] == {(2, "idiv", "99"): 1015, (2, "div", "sub dx, dx"): 7,
                                (1, "idiv", "98"): 23}
    assert forms["ispace"] == {(2, "idiv", "99"): 191, (2, "div", "sub dx, dx"): 10,
                               (1, "div", "sub ah, ah"): 5}
    assert forms["factory-1998a"] == {(2, "idiv", "99"): 66, (1, "idiv", "98"): 8}
    ins = module("inspace")
    byte_divs = [i for i in ins.insns if i.mnem == "idiv" and i.ops[0] in _BYTE_REGS]
    assert len(byte_divs) == 23 and "seg4:0x3310" in {i.addr for i in byte_divs}
    assert [i.addr for i in module("ispace").insns
            if i.mnem == "div" and i.ops[0].startswith("byte")][:1] == ["seg5:0x6883"]
    assert text(ins, "seg2:0xcbf").startswith("idiv ") and text(ins, "seg6:0x9911").startswith(
        "div ") and text(module("ispace"), "seg9:0x9d4").startswith("div ")


def test_divide_fault_handler(ins):
    """Division: seg5:0x996 installs seg5:0xb5c as INT 0 handler; error 3 via FatalAppExit."""
    assert owner(ins, "seg5:0x9ce") == "seg5:0x996"
    assert [text(ins, a) for a in ("seg5:0x9a0", "seg5:0x9bf", "seg5:0x9c2", "seg5:0x9ce")] == [
        "mov ax, 0x3500", "mov ax, 0x2500", "mov dx, 0xb5c", "call far KERNEL.102 DOS3Call"]
    assert "seg5:0xb5c" in ins.funcs and text(ins, "seg5:0xb5e") == "mov ax, 3"
    assert text(ins, "seg5:0xb95") == "call far KERNEL.137 FatalAppExit"
    assert "seg5:0xb5c" in ins.owners(*A("seg5:0xb95"))
    assert sites(ins, "KERNEL.137 FatalAppExit") == ["seg5:0xb95"]
    dg = unpacked_seg("inspace", 19)
    assert _cstr(dg, 0x1664) == b"<<NMSG>>"
    assert struct.unpack_from("<H", dg, 0x1688) == (3,)
    assert b"integer divide by 0" in _cstr(dg, 0x168a)
    assert _cstr(dg, 0x178c) == b"- floating-point support not loaded\r\n"


# ------------------------------------------------------------------ 32-bit helpers

HELPERS = {   # operation: (INSPACE, ISPACE, FACTORY) entry and caller count
    "mul": (("seg5:0x314", 144), ("seg3:0xfe", 349), ("seg3:0x1fc", 5)),
    "sdiv": (("seg5:0x110", 285), ("seg3:0x130", 337), ("seg3:0x22e", 9)),
    "srem": (("seg5:0x1aa", 9), ("seg3:0x310", 1), None),
    "udiv": (("seg5:0x2b4", 23), ("seg3:0x1d6", 7), ("seg3:0x19c", 3)),
    "urem": (("seg5:0x24a", 0), None, ("seg3:0x362", 0)),
    "shl": (("seg5:0x8c0", 41), ("seg3:0x1ca", 25), None),
    "sar": (("seg5:0x346", 36), ("seg3:0x2b0", 3), None),
    "shr": (("seg5:0x8cc", 1), ("seg3:0x2bc", 2), None),
}


def _body(name: str, a: str) -> bytes:
    f = module(name).funcs[a]
    seg, off = A(a)
    return unpacked_seg(name, seg)[off:A(f["end"])[1]]


def test_helper_caller_counts():
    """32-bit helpers: the entries in the RTL segment and their call sites."""
    for op, row in HELPERS.items():
        for name, cell in zip(NAMES, row):
            if cell is None:
                continue
            a, callers = cell
            assert A(a)[0] == RTL[name], (op, name)
            assert len(module(name).call_sites(a)) == callers, (op, name)


def test_helpers_are_byte_identical_copies():
    """32-bit helpers: each helper is the same code in every module that has it,
    FACTORY.EXE seg3:0x362 included; no module holds a copy the table marks none."""
    for op, row in HELPERS.items():
        ref = _body("inspace", row[0][0])
        for name, cell in zip(NAMES, row):
            if cell is not None:
                assert _body(name, cell[0]) == ref, (op, name)
            else:
                hits = [s for s, data in unpacked_segs(name).items() if ref in data]
                assert hits == [], (op, name)
    assert _body("factory-1998a", "seg3:0x362") == _body("inspace", "seg5:0x24a")


def test_helper_conventions(ins):
    """32-bit helpers: retf 8 for multiply, divide and remainder; the shift loop."""
    for op in ("mul", "sdiv", "srem", "udiv", "urem"):
        rets = {i.text for i in ins.func_insns(HELPERS[op][0][0]) if i.mnem == "retf"}
        assert rets == {"retf 8"}, op
    assert text(ins, "seg5:0x1a7") == "retf 8"
    assert [text(ins, a) for a in ("seg5:0x346", "seg5:0x348", "seg5:0x34e", "seg5:0x350")] == [
        "xor ch, ch", "jcxz seg5:0x350", "loop seg5:0x34a", "retf"]
    for op in ("shl", "sar", "shr"):
        body = ins.func_insns(HELPERS[op][0][0])
        assert body[0].text == "xor ch, ch" and body[-2].mnem == "loop", op
    assert [text(ins, a) for a in ("seg5:0x11d", "seg5:0x135")] == ["jge seg5:0x130",
                                                                   "jge seg5:0x148"]


# ------------------------------------------------------------------ memory


def test_global_alloc_flags():
    """Global blocks: 36, 36 and 7 GlobalAlloc calls, every one with 0x42."""
    for name, n in zip(NAMES, (36, 36, 7)):
        flags = [v[0] for v in call_args(name).args("KERNEL.15").values()]
        assert len(flags) == nsites(module(name), "KERNEL.15 GlobalAlloc") == n
        assert set(flags) == {0x42}, name
    assert "seg6:0x1bb8" in call_args("inspace").args("KERNEL.15")
    assert text(module("inspace"), "seg9:0x4174") == "push 0x42"


def test_global_realloc_flags():
    """Global blocks: the application's GlobalReAlloc passes 0x42; the RTL 2 or 0x20."""
    for name in NAMES:
        m = module(name)
        for a, v in call_args(name).args("KERNEL.16").items():
            if A(a)[0] == RTL[name]:
                assert v[2] is None, a                    # the far-heap code below
            else:
                assert v[2] == 0x42, (name, a)
        assert len([a for a in sites(m, "KERNEL.16 GlobalReAlloc") if A(a)[0] == RTL[name]]) == 1
    assert call_args("inspace").calls["seg7:0x52f5"][0] == "KERNEL.16"
    for name, (lo, hi, call), (pm, rm) in (
            ("inspace", ("seg5:0x1e56", "seg5:0x1e5f", "seg5:0x1e66"), ("2", "0x20")),
            ("ispace", ("seg3:0xb4d", "seg3:0xb56", "seg3:0xb5d"), ("0x2002", "0x2020")),
            ("factory-1998a", ("seg3:0xa98", "seg3:0xaa1", "seg3:0xaa8"), ("2", "0x20"))):
        m = module(name)
        assert (text(m, lo), text(m, hi), text(m, call)) == (
            f"mov ax, {pm}", f"mov ax, {rm}", "call far KERNEL.16 GlobalReAlloc")
        assert m.step(m.at(lo), 1).text == "test bx, 1"        # WF_PMODE of __WINFLAGS


def test_sizeof_resource_sites(ins, isp, fac):
    """Global blocks: SizeofResource sites."""
    assert sites(ins, "KERNEL.65 SizeofResource") == ["seg10:0x3913", "seg17:0x3255"]
    assert sites(isp, "KERNEL.65 SizeofResource") == ["seg5:0x1c69", "seg5:0x257d", "seg8:0x5bad"]
    assert sites(fac, "KERNEL.65 SizeofResource") == []


def test_rtl_only_memory_calls():
    """Global blocks: Local* one site each, Lock/UnlockSegment, all in the RTL segment."""
    for name in NAMES:
        m = module(name)
        for imp in ("KERNEL.5 LocalAlloc", "KERNEL.6 LocalReAlloc", "KERNEL.7 LocalFree",
                    "KERNEL.10 LocalSize"):
            s = sites(m, imp)
            assert len(s) == 1 and A(s[0])[0] == RTL[name], (name, imp)
        for imp in ("KERNEL.23 LockSegment", "KERNEL.24 UnlockSegment"):
            assert {A(a)[0] for a in sites(m, imp)} == {RTL[name]}, (name, imp)
    assert sites(module("inspace"), "KERNEL.5 LocalAlloc") == ["seg5:0x1f9d"]
    assert sites(module("ispace"), "KERNEL.5 LocalAlloc") == ["seg3:0xbf9"]


def test_global_compact_probe(ins, fac):
    """Global blocks: GlobalCompact(-1) and (0) in the uncalled probe seg6:0x1a68."""
    assert sites(ins, "KERNEL.25 GlobalCompact") == ["seg6:0x1ab0", "seg6:0x1ab9"]
    ca = call_args("inspace").args("KERNEL.25")
    assert (ca["seg6:0x1ab0"], ca["seg6:0x1ab9"]) == ([0xFFFFFFFF], [0])
    assert {owner(ins, a) for a in ca} == {"seg6:0x1a68"}
    assert ins.funcs["seg6:0x1a68"]["callers"] == []
    assert not [a for a, h in ins.xj["calls"].get("seg6:0x1a68", [])]
    assert call_args("inspace").calls["seg6:0x1a94"][0] == "KERNEL.15"
    assert sites(fac, "KERNEL.25 GlobalCompact") == ["seg4:0x1d0c", "seg4:0x1d15"]
    fa = call_args("factory-1998a").args("KERNEL.25")
    assert (fa["seg4:0x1d0c"], fa["seg4:0x1d15"]) == ([0xFFFFFFFF], [0])
    assert sites(module("ispace"), "KERNEL.25 GlobalCompact") == []


def test_huge_pointer_references():
    """Huge pointers: __AHSHIFT and __AHINCR references per module and segment."""
    isp = reloc_sites("ispace")
    assert Counter(A(a)[0] for a in isp["KERNEL.113 __AHSHIFT"]) == {5: 97, 6: 225, 7: 194}
    assert len(isp["KERNEL.113 __AHSHIFT"]) == 516
    assert Counter(A(a)[0] for a in isp["KERNEL.114 __AHINCR"]) == {1: 1, 3: 3, 6: 51, 7: 17}
    assert len(isp["KERNEL.114 __AHINCR"]) == 72
    ins, fac = reloc_sites("inspace"), reloc_sites("factory-1998a")
    m = module("inspace")
    used = [a for k in ("KERNEL.113 __AHSHIFT", "KERNEL.114 __AHINCR") for a in ins[k]
            if not a.startswith("seg1:")]
    assert sorted(used, key=A) == ["seg6:0x9ec3", "seg14:0x4060"]
    assert [owner(m, a) for a in sorted(used, key=A)] == ["seg6:0x9e66", "seg14:0x4006"]
    fused = [a for k in ("KERNEL.113 __AHSHIFT", "KERNEL.114 __AHINCR") for a in fac[k]
             if not a.startswith("seg1:")]
    assert sorted(fused, key=A) == ["seg4:0x274f", "seg4:0x295a"]
    # the RTL huge copy and fill hold ISPACE.DLL's three seg3 references
    seg3 = sorted((a for a in isp["KERNEL.114 __AHINCR"] if a.startswith("seg3:")), key=A)
    assert seg3 == ["seg3:0x293", "seg3:0x29e", "seg3:0x2f9"]
    assert [owner(module("ispace"), a) for a in seg3] == ["seg3:0x252", "seg3:0x252",
                                                          "seg3:0x2c8"]


def test_stub_references():
    """Huge pointers: every OptLoader stub references __AHINCR and AllocCStoDSAlias."""
    for name in NAMES:
        r = reloc_sites(name)
        assert r["KERNEL.114 __AHINCR"][0] == "seg1:0x2c", name
        assert r["KERNEL.170 AllocCStoDSAlias"] == ["seg1:0x28"], name


def test_chunked_file_helpers(ins, fac):
    """Huge pointers: _lwrite and _lread of the chunked helpers, at most 0x7fff bytes."""
    assert sites(ins, "KERNEL.86 _lwrite") == ["seg6:0x9ea4", "seg6:0x9ed8"]
    assert sites(ins, "KERNEL.82 _lread") == ["seg14:0x403f"]
    assert {owner(ins, a) for a in ("seg6:0x9ea4", "seg6:0x9ed8")} == {"seg6:0x9e66"}
    assert owner(ins, "seg14:0x403f") == "seg14:0x4006"
    assert call_args("inspace").calls["seg6:0x9ea4"][1][2] == 0x7FFF
    assert call_args("factory-1998a").calls["seg4:0x2731"][1][2] == 0x7FFF


# ------------------------------------------------------------------ messages and window procedures

_MSG = "word ptr [bp + 0xc]"


def decoded_messages(m: Module, func: str) -> set[int]:
    """Messages ``func`` decodes (see the module docstring)."""
    out = set()
    bound = {s["bound_at"]: s for s in m.fj["switches"]}
    work = []
    for i in m.func_insns(func):
        if i.mnem == "cmp" and i.ops[0] == _MSG and _imm(i.ops[1]) is not None:
            out.add(_imm(i.ops[1]))
        if i.mnem == "mov" and i.ops == ("ax", _MSG):
            work.append((m.pos[(i.seg, i.off)] + 1, 0))
    seen = set()
    while work:
        n, acc = work.pop()                    # AX holds msg - acc
        while (n, acc) not in seen:
            seen.add((n, acc))
            i = m.insns[n]
            if not m.in_func(i, func):
                break
            mn, ops = i.mnem, i.ops
            if i.addr in bound:
                sw = bound[i.addr]
                assert mn == "cmp" and ops[0] == "ax", i.addr
                nxt = m.insns[n + 1]
                default = nxt.ops[0] if nxt.mnem == "ja" else None
                out |= {(acc + k) & 0xFFFF for k, t in enumerate(sw["targets"]) if t != default}
                if default:
                    work.append((m.pos[A(default)], acc))
                break
            if ops and ops[0] in ("ax", "al") and mn in ("sub", "add", "dec", "inc", "cmp",
                                                         "or", "test"):
                k = _imm(ops[1]) if len(ops) > 1 else None
                if (mn in ("sub", "add", "cmp") and k is None) or \
                        (mn in ("or", "test") and ops[1] != ops[0]):
                    break
                if ops[0] == "al" and k is not None:
                    k &= 0xFF
                acc = {"sub": acc + (k or 0), "add": acc - (k or 0), "dec": acc + 1,
                       "inc": acc - 1}.get(mn, acc) & 0xFFFF
                val = (acc + k) & 0xFFFF if mn == "cmp" else acc
                j = n + 1
                while m.insns[j].mnem.startswith("j") and m.insns[j].mnem != "jmp":
                    if m.insns[j].mnem in ("je", "jne"):
                        out.add(val)
                        break
                    j += 1
            elif _is_jump(i) and mn != "jmp":
                work.append((m.pos[A(ops[0])], acc))
            elif mn == "jmp":
                if not ops[0].startswith("seg"):
                    break
                n = m.pos[A(ops[0])]
                continue
            elif mn in ("ret", "retf") or writes_reg(i, "ax"):
                break
            n += 1
    return out


WM = {"WM_CREATE": 1, "WM_MOVE": 3, "WM_SIZE": 5, "WM_SETFOCUS": 7, "WM_PAINT": 0xF,
      "WM_CLOSE": 0x10, "WM_SYSCOMMAND": 0x112, "WM_COMMAND": 0x111, "WM_CTLCOLOR": 0x19,
      "WM_DRAWITEM": 0x2B, "WM_MEASUREITEM": 0x2C, "WM_GETMINMAXINFO": 0x24,
      "WM_LBUTTONDBLCLK": 0x203, "WM_INITDIALOG": 0x110, "WM_HSCROLL": 0x114,
      "WM_NCPAINT": 0x85, "WM_TIMER": 0x113, "MM_MCINOTIFY": 0x3B9, "MM_WOM_DONE": 0x3BD,
      "WM_ACTIVATE": 6, "WM_ACTIVATEAPP": 0x1C, "WM_MENUSELECT": 0x11F, "WM_PARENTNOTIFY": 0x210}
COMMON = {WM[k] for k in ("WM_CREATE", "WM_MOVE", "WM_SIZE", "WM_SETFOCUS", "WM_PAINT",
                          "WM_CLOSE", "WM_SYSCOMMAND")}
CLASS_PROCS = {   # beyond the common set
    "seg6:0x1f22": {"WM_COMMAND", "WM_CTLCOLOR", "WM_DRAWITEM", "WM_MEASUREITEM",
                    "WM_GETMINMAXINFO"},                                           # IS_NAV
    "seg17:0x5782": {"WM_COMMAND", "WM_CTLCOLOR", "WM_DRAWITEM", "WM_MEASUREITEM"},  # IS_HELP
    "seg13:0x4f4e": {"WM_GETMINMAXINFO"}, "seg7:0x6184": {"WM_GETMINMAXINFO"},
    "seg13:0x613a": {"WM_GETMINMAXINFO"},                         # IS_WATCH, IS_ICON, IS_SMART
}
ONLY_PROCS = {    # ("inspace" | "ispace", procedure): every message it decodes
    ("inspace", "seg11:0x22fa"): {WM["WM_PAINT"], WM["WM_LBUTTONDBLCLK"]},          # IS_INFO
    ("inspace", "seg9:0x90fe"): {WM["WM_INITDIALOG"]},                              # #101
    ("ispace", "seg10:0x309c"): {WM["WM_PAINT"], WM["WM_NCPAINT"], WM["WM_COMMAND"]},  # LevClass
    ("ispace", "seg9:0x2ac4"): {WM["WM_TIMER"], WM["MM_MCINOTIFY"], WM["MM_WOM_DONE"],
                                0x84C},                                            # NotifyClass
}
ISPACE_DIALOG_ORDINALS = (11, 22, 23, 25, 26, 27, 28, 30, 37, 38, 39, 40, 42)


def test_class_procedures_decode_the_common_set_and_their_extras(ins):
    """Procedures: IS_NAV, IS_HELP, IS_WATCH, IS_ICON, IS_SMART."""
    union = set()
    for proc, extra in CLASS_PROCS.items():
        got = decoded_messages(ins, proc)
        assert got - COMMON == {WM[k] for k in extra}, proc
        union |= got & COMMON
    assert union == COMMON


def test_procedures_with_only_cells(ins, isp):
    """Procedures: the cells that end in "only"."""
    for (name, proc), msgs in ONLY_PROCS.items():
        assert decoded_messages(module(name), proc) == msgs, proc
    ex = exports(isp)
    assert (ex[21], ex[31]) == ("seg10:0x309c", "seg9:0x2ac4")


DIALOG_PROCS = {   # what the bytes show beyond the common set
    "seg16:0x4104": {"WM_INITDIALOG", "WM_COMMAND", "WM_DRAWITEM", "WM_MEASUREITEM"},  # #194
    "seg16:0x42a2": {"WM_INITDIALOG", "WM_COMMAND", "WM_DRAWITEM", "WM_MEASUREITEM"},  # #378
    "seg17:0x8418": {"WM_INITDIALOG", "WM_COMMAND"},                                   # #404
}


def test_dialog_procedures(ins, isp):
    """Procedures: the dialog rows. Every dialog procedure also decodes WM_INITDIALOG,
    and ISPACE.DLL ordinal 30 decodes the private message 0xa0e."""
    for proc, msgs in DIALOG_PROCS.items():
        assert decoded_messages(ins, proc) == {WM[k] for k in msgs}, proc
    ex = exports(isp)
    for o in ISPACE_DIALOG_ORDINALS:
        want = {WM["WM_INITDIALOG"], WM["WM_COMMAND"]}
        want |= {WM["WM_HSCROLL"]} if o in (11, 38) else {0xA0E} if o == 30 else set()
        assert decoded_messages(isp, ex[o]) == want, o


def test_no_procedure_tests_activation_messages(ins, isp):
    """Procedures: no procedure of the table tests WM_ACTIVATE, WM_ACTIVATEAPP,
    WM_MENUSELECT or WM_PARENTNOTIFY."""
    banned = {WM[k] for k in ("WM_ACTIVATE", "WM_ACTIVATEAPP", "WM_MENUSELECT",
                              "WM_PARENTNOTIFY")}
    procs = [(ins, p) for p in ["seg2:0x224", *CLASS_PROCS, *DIALOG_PROCS]]
    procs += [(module(n), p) for n, p in ONLY_PROCS]
    procs += [(isp, exports(isp)[o]) for o in ISPACE_DIALOG_ORDINALS]
    for m, p in procs:
        got = decoded_messages(m, p)
        assert got and not got & banned, p


def test_absent_imports():
    """MakeProcInstance, joySetCapture, timeBeginPeriod, timeEndPeriod, timeSetEvent,
    _lopen, _lcreat, _hread and _hwrite: no module imports them."""
    names = [("KERNEL", "MakeProcInstance"), ("MMSYSTEM", "joySetCapture"),
             ("MMSYSTEM", "timeBeginPeriod"), ("MMSYSTEM", "timeEndPeriod"),
             ("MMSYSTEM", "timeSetEvent"), ("KERNEL", "_lopen"), ("KERNEL", "_lcreat"),
             ("KERNEL", "_hread"), ("KERNEL", "_hwrite")]
    wanted = {(mod, ordinal_of()[(mod, n)]) for mod, n in names}
    assert ordinal_of()[("MMSYSTEM", "joySetCapture")] == 106
    for name in NAMES:
        assert not imported(name) & wanted, name
        assert ("USER", 15) in imported(name)                     # the scan sees imports


def test_command_notification_sites(ins, isp, fac):
    """Parameter packing: EN_CHANGE and LBN_DBLCLK tests of HIWORD(lParam)."""
    assert [i.addr for i in ins.insns if i.mnem == "cmp" and i.ops[-1] == "0x300"] == [
        "seg17:0x8456"]
    assert [i.addr for i in isp.insns if i.mnem == "cmp" and i.ops[-1] == "0x300"] == [
        "seg10:0x739"]
    assert [i.addr for i in fac.insns if i.mnem == "cmp" and i.ops[-1] == "0x300"] == []
    assert text(isp, "seg10:0x733") == "mov ax, word ptr [bp + 8]"
    procs = ["seg2:0x224", *CLASS_PROCS, *DIALOG_PROCS]
    dbl = [i.addr for i in ins.insns if i.text == "cmp word ptr [bp + 8], 2"
           and ins.owners(i.seg, i.off) & set(procs)]
    assert dbl == ["seg6:0x230f", "seg16:0x4240", "seg16:0x4261", "seg16:0x4390",
                   "seg16:0x43b1"]


def test_draw_item_and_min_max_sites(ins):
    """Parameter packing: itemID = -1 tests and WM_GETMINMAXINFO stores."""
    procs = set(CLASS_PROCS) | set(DIALOG_PROCS)
    item = [i.addr for i in ins.insns if i.text == "cmp word ptr es:[bx + 4], -1"
            and ins.owners(i.seg, i.off) & procs]
    assert item == ["seg6:0x1f95", "seg16:0x412a", "seg16:0x42c8", "seg17:0x584a"]
    stores = {"seg2:0x3f6": 0xC, "seg2:0x3ff": 0xE, "seg2:0x41e": 0x10, "seg2:0x430": 0xE,
              "seg13:0x4fef": 0xC, "seg13:0x5013": 0x12, "seg7:0x62e1": 0x10,
              "seg7:0x630a": 0x12, "seg13:0x6299": 0x10, "seg13:0x62c6": 0xE}
    for a, off in stores.items():
        assert text(ins, a).startswith(f"mov word ptr es:[bx + {off:#x}], "), a
    assert text(ins, "seg6:0x206d") == "mov dword ptr es:[bx + 0xc], 0x15e0230"
    assert (0x15E0230 >> 16, 0x15E0230 & 0xFFFF) == (350, 560)


def test_scroll_and_ctlcolor_sites(ins, isp):
    """Parameter packing: WM_CTLCOLOR wrapper callers, scroll bar sites, MM_MCINOTIFY."""
    assert ins.call_sites("seg2:0x1a5a") == ["seg6:0x204e", "seg17:0x5913"]
    assert [owner(ins, a) for a in ("seg6:0x204e", "seg17:0x5913")] == ["seg6:0x1f22",
                                                                        "seg17:0x5782"]
    assert [text(isp, a) for a in ("seg9:0x1cb6", "seg9:0x1cce", "seg9:0x1cf1")] == [
        "push word ptr [bp + 8]", "mov ax, word ptr [bp + 0xa]", "mov ax, word ptr [bp + 6]"]
    assert ins.step(ins.at("seg2:0x6a7"), 2).text == "call far seg11:0x7798"
    assert text(ins, "seg11:0x77e0") == "sub ax, word ptr [bp + 8]"
    assert text(isp, "seg9:0x2b18") == "cmp word ptr [bp + 0xa], 1"
    lparam = [i for i in isp.func_insns("seg9:0x2ac4") if "[bp + 6]" in i.text
              or "[bp + 8]" in i.text]                  # only forwarded to DefWindowProc
    assert [i.addr for i in lparam] == ["seg9:0x2af5", "seg9:0x2af8"]
    assert isp.step(lparam[-1], 1).text == "call far USER.107 DefWindowProc"


def test_private_messages(ins, isp):
    """Parameter packing: private messages 0x8e2-0xa04 (INSPACE) and 0x84c, 0xa0e (ISPACE)."""
    def private(name: str, procs) -> set[int]:
        out = set()
        for imp in ("USER.110", "USER.111"):
            out |= {v[1] for v in call_args(name).args(imp).values()
                    if isinstance(v[1], int) and 0x430 <= v[1] < 0xC000}
        for p in procs:
            out |= {x for x in decoded_messages(module(name), p) if 0x430 <= x < 0xC000}
        return out

    got = private("inspace", ["seg2:0x224", *CLASS_PROCS, *DIALOG_PROCS, "seg11:0x22fa",
                              "seg9:0x90fe"])
    assert (min(got), max(got)) == (0x8E2, 0xA04)
    ex = exports(isp)
    assert private("ispace", [ex[o] for o in (*ISPACE_DIALOG_ORDINALS, 21, 31)]) == {0x84C, 0xA0E}
    posted = call_args("inspace").args("USER.110")
    assert posted["seg2:0x4b6"][1:] == [0x9D0, 0, 0]
    assert posted["seg16:0x6492"][1] == 0x9C5 and posted["seg16:0x6492"][3] is None


def test_posted_wm_paint(ins):
    """Parameter packing: WM_PAINT posted to DGROUP:0xa7c; IS_INFO invalidates first."""
    assert call_args("inspace").calls["seg7:0xaf2"] == ("USER.110", [None, 0xF, 0, 0])
    assert ins.step(ins.at("seg7:0xaf2"), -5).text == "push word ptr [0xa7c]"
    assert text(ins, "seg11:0xd53").startswith("mov word ptr [0xa7c], ")
    assert text(ins, "seg11:0x232f") == "call far USER.125 InvalidateRect"
    assert text(ins, "seg11:0x233c") == "call far USER.39 BeginPaint"
    assert text(ins, "seg11:0x2349") == "call seg11:0x238e"
    assert text(ins, "seg11:0x23b2") == "call far USER.66 GetDC"
    assert owner(ins, "seg11:0x23b2") == "seg11:0x238e"
    assert "seg13:0xb446" in ins.call_sites("seg11:0x238e")


def test_send_message_counts_and_messages():
    """Control messages: 213, 120 and 162 SendMessage sites; the commonest messages."""
    total = Counter()
    for name, n in zip(NAMES, (213, 120, 162)):
        msgs = [v[1] for v in call_args(name).args("USER.111").values()]
        assert len(msgs) == nsites(module(name), "USER.111 SendMessage") == n
        total.update(msgs)
    top = [k for k, _ in total.most_common(5)]
    assert top[:2] == [0x30, 0x403] and set(top[2:]) == {0x407, 0x40E, 0x40A}
    ca = call_args("inspace").calls
    assert ca["seg6:0x285e"][1][1] == 0x40E                                 # CB_SETCURSEL
    assert ca["seg6:0x3e21"][1][1] == 0x423                                 # LB_FINDSTRINGEXACT
    assert [text(module("inspace"), a) for a in ("seg17:0x8478", "seg17:0x8481")] == [
        "push 0x199", "push 0x40d"]                                         # list box 409


def test_get_object_and_timecaps_sizes():
    """Structures: GetObject passes 14 bytes at all 29 sites; timeGetDevCaps 4."""
    sizes = [v[1] for n in NAMES for v in call_args(n).args("GDI.82").values()]
    assert len(sizes) == 29 and set(sizes) == {14}
    assert "seg10:0x195b" in call_args("inspace").args("GDI.82")
    assert call_args("inspace").args("MMSYSTEM.604") == {"seg6:0x1348": [None, 4]}
    assert text(module("inspace"), "seg6:0x1346") == "push 4"
    assert call_args("factory-1998a").args("MMSYSTEM.604") == {"seg4:0xa0f": [None, 4]}


def test_structures_written_by_the_api(ins):
    """Structures: GetWindowRect into DGROUP:0x1d20 and GetCursorPos into DGROUP:0x1d48."""
    ca = call_args("inspace").calls
    assert ca["seg2:0xb7c"] == ("USER.32", [None, ("ds", 0x1D20)])
    assert ca["seg2:0x55"] == ("USER.17", [("ds", 0x1D48)])


# ------------------------------------------------------------------ dialogs and resources


def test_dialog_and_menu_loads():
    """Templates: DialogBox, CreateDialog and LoadMenu sites with ids and procedures."""
    ins_dlg = call_args("inspace").args("USER.87")
    assert {a: (v[1], v[3] & 0xFFFF) for a, v in ins_dlg.items()} == {
        "seg16:0x133f": (194, 0x4104), "seg16:0x4211": (378, 0x42A2),
        "seg17:0x594a": (404, 0x8418)}
    assert {a: (v[1], v[3] & 0xFFFF) for a, v in call_args("inspace").args("USER.89").items()} \
        == {"seg9:0x667e": (101, 0x90FE), "seg10:0x9a95": (101, 0x90FE)}
    m = module("inspace")
    for a in ("seg16:0x133f", "seg16:0x4211", "seg17:0x594a", "seg9:0x667e", "seg10:0x9a95"):
        pushes = [m.step(m.at(a), -k).text for k in range(1, 8)]
        assert "push word ptr [0x9212]" in pushes, a                  # the language DLL
    menus = call_args("inspace").args("USER.150")
    assert menus == {"seg2:0x1361": [None, ("ds", 0xFF)], "seg2:0x16ed": [None, None],
                     "seg6:0x17f5": [None, 101]}
    assert [text(m, a) for a in ("seg2:0x16e1", "seg2:0x16eb")] == ["push 0x66", "push 0x65"]
    assert text(m, "seg2:0x16e3") == "jmp seg2:0x16ed"
    assert _cstr(unpacked_seg("inspace", 19), 0xFF) == b"PopupMenu"
    assert text(m, "seg2:0x1370").startswith("call far USER.159 GetSubMenu")
    assert sites(m, "USER.416 TrackPopupMenu") == ["seg2:0x139b"]


def test_load_string_call_sites(ins):
    """Templates: LoadString has 246 call sites in INSPACE.EXE."""
    assert nsites(ins, "USER.176 LoadString") == 246


@pytest.fixture(scope="module")
def eng_dialogs() -> list[dict]:
    return json.loads(require(WORK / "assets" / "eng_lang.dll" / "dialogs.json").read_text())


def test_eng_lang_templates(eng_dialogs):
    """Templates: DS_SETFONT, fonts, predefined classes and owner-drawn list boxes."""
    assert len(eng_dialogs) == 22
    assert all(int(d["style"], 16) & 0x40 for d in eng_dialogs)            # DS_SETFONT
    fonts = Counter((d["font"]["face"], d["font"]["point_size"]) for d in eng_dialogs)
    assert fonts == {("Helv", 8): 21, ("Arial", 9): 1}
    assert [d["id"] for d in eng_dialogs if d["font"]["face"] == "Arial"] == [101]
    assert not [d["id"] for d in eng_dialogs if d["class"]]
    classes = {c["class"] for d in eng_dialogs for c in d["controls"]}
    assert classes == {"BUTTON", "STATIC", "COMBOBOX", "EDIT", "SCROLLBAR", "LISTBOX"}
    assert all(c["class_atom"] for d in eng_dialogs for c in d["controls"])
    owner_drawn = [(d["id"], c["id"]) for d in eng_dialogs for c in d["controls"]
                   if c["class"] == "LISTBOX" and int(c["style"], 16) & 0x10]
    assert len(owner_drawn) == 4 and {d for d, _c in owner_drawn} == {194, 378}


def test_owner_drawn_list_boxes_created_by_is_nav_and_is_help(ins):
    """Templates: IS_NAV and IS_HELP create owner-drawn list boxes."""
    ca = call_args("inspace").calls
    dg = unpacked_seg("inspace", 19)
    for a in ("seg6:0x24f3", "seg17:0x6b56"):
        imp, args = ca[a]
        assert imp == "USER.41" and args[2] & 0x10, a                    # LBS_OWNERDRAWFIXED
        seg, off = args[0]
        assert seg == "ds" and _cstr(dg, off).upper() == b"LISTBOX", a


def test_registered_window_classes(ins, isp):
    """Templates: seven RegisterClass calls in seg6:0x14c7-0x16ef; LevClass, NotifyClass."""
    calls = sites(ins, "USER.57 RegisterClass")
    assert calls == ["seg6:0x14c7", "seg6:0x1520", "seg6:0x1579", "seg6:0x15d2", "seg6:0x1635",
                     "seg6:0x1698", "seg6:0x16ef"]
    dg = unpacked_seg("inspace", 19)
    names = []
    for prev, call in zip(calls, calls[1:]):
        refs = [_imm(i.ops[1]) for i in ins.insns if i.seg == 6
                and A(prev)[1] < i.off < A(call)[1] and i.mnem == "mov"
                and i.ops[0].startswith("word ptr [bp") and _imm(i.ops[1]) is not None]
        names += [_cstr(dg, v) for v in refs if dg[v:v + 3] == b"IS_"]
    assert names == [b"IS_WATCH", b"IS_ICON", b"IS_SMART", b"IS_NAV", b"IS_HELP", b"IS_INFO"]
    assert sites(isp, "USER.57 RegisterClass") == ["seg9:0x2ab8", "seg10:0x42b9"]
    idg = unpacked_seg("ispace", 11)
    assert text(isp, "seg9:0x2aaa") == "mov word ptr [0x2038], 0x9aa"
    assert _cstr(idg, 0x9AA) == b"NotifyClass" and _cstr(idg, 0x4C8) == b"LevClass"
    assert "seg10:0x42ab" in [a for a, _h in isp.xj["dgroup"]["DGROUP:0x4c8"]]


def test_ctl3d_loader(ins):
    """CTL3D: OpenFile(OF_EXIST), LoadLibrary, five entry points by name and their uses."""
    ca = call_args("inspace").calls
    assert ca["seg2:0x19d3"][1][2] == 0x4000
    assert text(ins, "seg2:0x19bd").startswith("push 0x11d")
    assert _cstr(unpacked_seg("inspace", 19), 0x11D) == b"\\CTL3D.DLL"
    assert ca["seg2:0x19e0"][0] == "KERNEL.95"
    assert text(ins, "seg2:0x19e5") == "mov word ptr [0x24], ax"
    assert owner(ins, "seg2:0x19d3") == "seg2:0x1994"
    dg = unpacked_seg("inspace", 19)
    names = {0x78: b"Ctl3dRegister", 0x7A: b"Ctl3dAutoSubclass", 0x7C: b"Ctl3dUnregister",
             0x7E: b"Ctl3dSubclassCtl", 0x80: b"Ctl3dCtlColorEx"}
    gpa = [a for a in sites(ins, "KERNEL.50 GetProcAddress") if owner(ins, a) == "seg2:0x1994"]
    assert gpa == ["seg2:0x19f2", "seg2:0x1a07", "seg2:0x1a1c", "seg2:0x1a31", "seg2:0x1a46"]
    for call, (ptr, name), slot in zip(gpa, names.items(), (0x1D4C, 0x1D50, 0x1D54, 0x1D58,
                                                            0x1D5C)):
        assert ins.step(ins.at(call), -1).text == f"push word ptr [{ptr:#x}]"
        assert _cstr(dg, struct.unpack_from("<H", dg, ptr)[0]) == name
        assert ins.step(ins.at(call), 1).text == f"mov word ptr [{slot:#x}], ax"
    uses = {i.mem.disp: i.addr for i in ins.insns if i.mnem == "lcall" and i.abs_ds
            and 0x1D4C <= i.mem.disp <= 0x1D5C}
    assert uses == {0x1D4C: "seg2:0x91", 0x1D50: "seg2:0x99", 0x1D54: "seg2:0x20d",
                    0x1D58: "seg2:0x1a8e", 0x1D5C: "seg2:0x1a6e"}
    assert [owner(ins, uses[s]) for s in (0x1D58, 0x1D5C)] == ["seg2:0x1a7a", "seg2:0x1a5a"]
    callers = ins.call_sites("seg2:0x1a7a")
    assert len(callers) == 15 and {A(a)[0] for a in callers} == {6, 17}


def test_ispace_has_no_ctl3d_reference():
    """CTL3D: ISPACE.DLL does not reference CTL3D."""
    assert "CTL3D" not in relocs("ispace")["modules"].values()
    for seg, data in unpacked_segs("ispace").items():
        assert b"ctl3d" not in data.lower(), seg


def test_factory_ctl3d_names():
    """Ship Factory: CTL3D names at FACTORY.EXE DGROUP:0x36-0x77, \\CTL3D.DLL at 0xe2."""
    dg = unpacked_seg("factory-1998a", 6)
    names, p = [], 0x36
    while p <= 0x77:
        names.append(_cstr(dg, p))
        p += len(names[-1]) + 1
    assert names == [b"Ctl3dRegister", b"Ctl3dAutoSubclass", b"Ctl3dUnregister",
                     b"Ctl3dSubclassCtl", b"Ctl3dCtlColorEx"]
    assert _cstr(dg, 0xE2) == b"\\CTL3D.DLL"


# ------------------------------------------------------------------ time


def test_time_call_sites(ins, isp, fac):
    """Time: GetCurrentTime, timeGetTime, timeGetDevCaps, SetTimer and KillTimer."""
    assert [nsites(m, "USER.15 GetCurrentTime") for m in (ins, isp, fac)] == [98, 13, 3]
    assert sites(ins, "MMSYSTEM.607 timeGetTime") == ["seg7:0xb7", "seg15:0xd7a", "seg15:0xde6"]
    tgt = sites(isp, "MMSYSTEM.607 timeGetTime")
    assert len(tgt) == 6 and {owner(isp, a) for a in tgt} == {exports(isp)[17]}
    assert A(exports(isp)[17])[0] == 6
    assert sites(fac, "MMSYSTEM.607 timeGetTime") == ["seg2:0x1de3"]
    assert sites(ins, "MMSYSTEM.604 timeGetDevCaps") == ["seg6:0x1348"]
    assert sites(isp, "MMSYSTEM.604 timeGetDevCaps") == []
    assert sites(fac, "MMSYSTEM.604 timeGetDevCaps") == ["seg4:0xa0f"]
    assert sites(isp, "USER.10 SetTimer") == ["seg9:0x46d4"]
    assert sites(fac, "USER.10 SetTimer") == ["seg4:0x54c", "seg4:0x7a3"]
    assert "seg6:0x1397" in sites(ins, "USER.10 SetTimer")
    assert call_args("inspace").calls["seg6:0x13b1"] == ("USER.12", [None, 1])
    assert text(ins, "seg7:0x1f").startswith("call far USER.15 GetCurrentTime")


# ------------------------------------------------------------------ files


def test_dos_calls():
    """DOS calls: DOS3Call sites per module, all in the RTL; the DOS functions."""
    ahs = {}
    for name, n in zip(NAMES, (39, 16, 13)):
        m = module(name)
        d3 = sites(m, "KERNEL.102 DOS3Call")
        assert len(d3) == n and {A(a)[0] for a in d3} == {RTL[name]}, name
        dos = m.xj["dos"]
        assert sorted((x["at"] for x in dos if x["via"] == "DOS3Call"), key=A) == d3
        ahs[name] = {ah for x in dos if x["via"] == "DOS3Call" for ah in x["ah"]}
        # each DOS3Call has an int 21h twin in the RTL
        assert len([x for x in dos if x["via"] == "int 21h" and A(x["at"])[0] == RTL[name]]) == n
    assert ahs["inspace"] == {0x30, 0x2F, 0x1A, 0x4E, 0x4F, 0x2A, 0x2C, 0x35, 0x25, 0x4C, 0x3C,
                              0x3D, 0x3E, 0x3F, 0x40, 0x42, 0x43, 0x44, 0x19, 0x0E}
    assert ahs["ispace"] - ahs["inspace"] == {0x36, 0x57}
    assert ahs["factory-1998a"] <= ahs["inspace"]


def test_rtl_dos_call_pair(ins):
    """DOS calls: WF_PMODE of __WINFLAGS at seg5:0x10 picks DOS3Call or int 21h."""
    assert [text(ins, a) for a in ("seg5:0x54", "seg5:0x5b", "seg5:0x5d", "seg5:0x64")] == [
        "test word ptr cs:[0x10], 1", "je seg5:0x64", "call far KERNEL.102 DOS3Call",
        "int 0x21"]
    assert reloc_sites("inspace")["KERNEL.178 __WINFLAGS"] == ["seg5:0x10"]


def test_only_direct_int21_outside_an_rtl():
    """DOS calls: the only int 21h outside an RTL is ISPACE.DLL seg10:0x6279."""
    outside = [(n, i.addr) for n in NAMES for i in module(n).insns
               if i.text == "int 0x21" and i.seg != RTL[n]]
    assert outside == [("ispace", "seg10:0x6279")]
    assert [x for x in module("ispace").xj["dos"] if x["at"] == "seg10:0x6279"][0]["ah"] == [0x44]
    assert text(module("ispace"), "seg10:0x461e") == "int 0x31"


def test_new_game_drive_probe(isp):
    """Directory walk: get and select drive under SetErrorMode(SEM_FAILCRITICALERRORS)."""
    assert call_args("ispace").args("KERNEL.107") == {"seg10:0x2087": [1], "seg10:0x55df": [1]}
    dos = {x["at"]: x["ah"] for x in isp.xj["dos"]}
    assert (dos["seg3:0x451"], dos["seg3:0x47f"]) == ([0x19], [0x0E])
    assert (owner(isp, "seg3:0x451"), owner(isp, "seg3:0x47f")) == ("seg3:0x43c", "seg3:0x466")
    assert text(isp, "seg10:0x207d") == "call far seg3:0x43c"


def test_open_file_styles():
    """File API: OpenFile counts and styles."""
    want = {"inspace": {0: 11, 0x4000: 5, 0x1002: 2, 0x200: 2},
            "ispace": {0: 4, 0x4000: 3, None: 1},
            "factory-1998a": {0: 2, 0x4000: 4, 0x1002: 1}}
    for name, styles in want.items():
        got = Counter(v[2] for v in call_args(name).args("KERNEL.74").values())
        assert got == styles, name
        assert sum(got.values()) == nsites(module(name), "KERNEL.74 OpenFile")
    assert call_args("ispace").calls["seg10:0x46b9"][1][2] is None
    assert call_args("inspace").calls["seg6:0x8a78"][1][2] == 0x1002
    assert call_args("factory-1998a").calls["seg4:0x26bf"][1][2] == 0x1002


def test_low_level_file_calls(ins, isp, fac):
    """File API: _lread, _lwrite, _llseek per module; ISPACE.DLL's _lwrite."""
    for m, counts in ((ins, (1, 2, 0)), (isp, (9, 1, 5)), (fac, (1, 2, 0))):
        assert tuple(nsites(m, k) for k in ("KERNEL.82 _lread", "KERNEL.86 _lwrite",
                                            "KERNEL.84 _llseek")) == counts, m.name
    assert sites(isp, "KERNEL.86 _lwrite") == ["seg10:0x47dc"]


def test_win_exec(ins, isp):
    """Profiles: WinExec sites, SW_SHOW, the NOTEPAD strings; no GRMSETUP.EXE shipped."""
    assert call_args("inspace").args("KERNEL.166") == {"seg6:0x7d1": [None, 5]}
    assert call_args("inspace").calls["seg6:0x7c1"][1][2] == 0x4000
    assert call_args("ispace").args("KERNEL.166") == {"seg10:0x71b": [None, 5],
                                                     "seg10:0x5ffa": [None, 5]}
    dg = unpacked_seg("ispace", 11)
    assert [_cstr(dg, o) for o in (0x1E7, 0x1F0, 0x64A, 0x653)] == [
        b"NOTEPAD ", b"README.TXT", b"NOTEPAD ", b"ORDER.TXT"]
    listed = require(REPO_ROOT / "original.sha256").read_text().upper()
    assert "INSPACE.EX" in listed and "GRMSETUP" not in listed


# ------------------------------------------------------------------ graphics


def test_palette_calls(ins, isp):
    """Palettes: SelectPalette, AnimatePalette, LOGPALETTE, CreateDIBitmap, GetDIBits."""
    assert nsites(ins, "USER.282 SelectPalette") == 233
    assert call_args("ispace").args("GDI.367") == {"seg5:0x441a": [None, 10, None, None]}
    assert text(isp, "seg5:0x3619") == "mov word ptr es:[si], 0x300"
    assert text(isp, "seg5:0x38bf") == "mov byte ptr es:[bx + si + 0x2f], 1"
    assert text(isp, "seg5:0x38e0") == "call far GDI.360 CreatePalette"
    assert sites(isp, "GDI.442 CreateDIBitmap") == ["seg5:0x215f"]
    assert sites(isp, "GDI.441 GetDIBits") == ["seg5:0x2295"]
    assert [text(ins, a) for a in ("seg2:0x59a", "seg2:0x5a2", "seg2:0x1c8f")] == [
        "call far USER.283 RealizePalette", "call far GDI.366 UpdateColors",
        "call far USER.282 SelectPalette"]
    assert {0x30F, 0x311} <= decoded_messages(ins, "seg2:0x224")


def test_device_queries(ins):
    """Device queries: indices, the globals they fill and the references to them."""
    gdc = call_args("inspace").args("GDI.80")
    assert {a: v[1] for a, v in gdc.items()} == {
        "seg6:0x1716": 8, "seg6:0x1723": 10, "seg6:0x1730": 12, "seg6:0x173d": 14,
        "seg6:0x1768": 94}
    for lo, store in ((0x90E6, "seg6:0x171b"), (0x90E8, "seg6:0x1728"), (0x90F2, "seg6:0x1776")):
        assert [i.addr for i in ins.refs(lo, lo + 1)] == [store], hex(lo)
        assert [a for a, _h in ins.xj["dgroup"][f"DGROUP:{lo:#x}"]] == [store]
    depth = ins.refs(0x90EE, 0x90EF)
    assert "seg6:0x174d" in {i.addr for i in depth} and len(depth) - 1 == 32
    assert len(ins.xj["dgroup"]["DGROUP:0x90ee"]) == 33
    assert [text(ins, a) for a in ("seg6:0x1745", "seg6:0x174d", "seg6:0x1750")] == [
        "cmp ax, word ptr [bp - 6]", "mov word ptr [0x90ee], ax", "cmp ax, 8"]
    assert ins.step(ins.at("seg6:0x1768"), 1).text == "and ax, 1"          # C1_TRANSPARENT
    assert {a: v[1] for a, v in call_args("ispace").args("GDI.80").items()} == {
        "seg5:0x3644": 104, "seg5:0x3651": 106, "seg5:0x365e": 12, "seg5:0x366b": 14}
    assert {a: v[1] for a, v in call_args("factory-1998a").args("GDI.80").items()} == {
        "seg4:0x997": 8, "seg4:0x9a4": 10, "seg4:0x9b1": 12, "seg4:0x9be": 14, "seg4:0x9e9": 94}


_ROPS = {"GDI.34": "BitBlt", "GDI.35": "StretchBlt", "GDI.29": "PatBlt"}


def rop_tally(name: str) -> dict:
    out = defaultdict(Counter)
    for a, (imp, args) in call_args(name).calls.items():
        if imp in _ROPS:
            rop = args[-1] if isinstance(args[-1], int) else "computed"
            out[rop][_ROPS[imp]] += 1
    return {k: dict(v) for k, v in out.items()}


ROPS_BYTES = {
    "inspace": {
        0xCC0020: {"BitBlt": 147, "StretchBlt": 22}, 0x8800C6: {"BitBlt": 26},
        0xEE0086: {"BitBlt": 26}, 0xC000CA: {"BitBlt": 8, "StretchBlt": 1},
        0xC00746: {"BitBlt": 2}, 0xA803A9: {"BitBlt": 4}, 0x220326: {"BitBlt": 2},
        0xBB0226: {"BitBlt": 1}, 0x000042: {"PatBlt": 246}, 0xF00021: {"PatBlt": 43},
        0xFF0062: {"PatBlt": 5}, 0xA000C9: {"PatBlt": 5},
        "computed": {"BitBlt": 1, "StretchBlt": 2}},
    "ispace": {
        0xCC0020: {"BitBlt": 6, "StretchBlt": 2}, 0x8800C6: {"BitBlt": 5, "StretchBlt": 8},
        0xEE0086: {"BitBlt": 4, "StretchBlt": 1}, 0xC000CA: {"StretchBlt": 1},
        0x220326: {"BitBlt": 2, "StretchBlt": 2}, 0x000042: {"PatBlt": 15},
        0xF00021: {"PatBlt": 38}, 0xFF0062: {"PatBlt": 6},
        "computed": {"BitBlt": 1, "StretchBlt": 2}},
    "factory-1998a": {
        0xCC0020: {"BitBlt": 22, "StretchBlt": 5}, 0x8800C6: {"BitBlt": 7},
        0xEE0086: {"BitBlt": 8}, 0xC000CA: {"BitBlt": 4}, 0x000042: {"PatBlt": 28},
        "computed": {"BitBlt": 1, "StretchBlt": 1}},
}


def test_raster_operations(ins):
    """Raster operations: the ROP of every BitBlt, StretchBlt and PatBlt call.

    INSPACE.EXE seg8:0xb64 is one BitBlt that SRCCOPY (fall-through) and
    SRCPAINT (from seg8:0xaab) share; seg9:0xddd and seg18:0x181e push
    BLACKNESS's high word from AX, which holds 0 on every path.
    """
    for name, want in ROPS_BYTES.items():
        assert rop_tally(name) == want, name
        for imp, fn in _ROPS.items():
            assert sum(c.get(fn, 0) for c in want.values()) == nsites(
                module(name), f"{imp} {fn}"), (name, fn)
    rops = {a: v[-1] for a, (k, v) in call_args("inspace").calls.items() if k in _ROPS}
    assert (rops["seg11:0x208c"], rops["seg11:0x2278"], rops["seg7:0x2463"]) == (
        0xC00746, 0xC00746, 0xBB0226)
    assert rops["seg8:0xb64"] is None
    assert ins.step(ins.at("seg8:0xaab"), -1).text == "push 0xee0086"
    assert [ins.step(ins.at("seg8:0xb64"), k).text for k in (-2, -1)] == ["push 0xcc", "push 0x20"]
    assert (rops["seg9:0xddd"], rops["seg18:0x181e"]) == (0x42, 0x42)
    assert [ins.step(ins.at(a), -2).text for a in ("seg9:0xddd", "seg18:0x181e")] == [
        "push ax", "push ax"]


def test_win16_only_gdi_calls(ins, isp, fac):
    """Win16-only GDI calls: MoveTo, GetTextExtent, window origin, current position."""
    assert [nsites(m, "GDI.20 MoveTo") for m in (ins, isp, fac)] == [56, 7, 7]
    assert [nsites(m, "GDI.91 GetTextExtent") for m in (ins, isp, fac)] == [13, 5, 2]
    assert sites(ins, "GDI.11 SetWindowOrg") == ["seg16:0x6541", "seg16:0x68bc"]
    assert sites(ins, "GDI.97 GetWindowOrg") == ["seg18:0x17b2"]
    assert sites(ins, "GDI.78 GetCurrentPosition") == ["seg15:0x7a73", "seg15:0x82f4"]
    for m in (isp, fac):
        for imp in ("GDI.11 SetWindowOrg", "GDI.97 GetWindowOrg", "GDI.78 GetCurrentPosition"):
            assert sites(m, imp) == [], (m.name, imp)


# ------------------------------------------------------------------ sound and input


def test_snd_play_sound(isp):
    """Sound: sndPlaySound(NULL, 0) at seg9:0x11e8; flags 3, 7 or 0xF for seg9:0x1333."""
    assert call_args("ispace").args("MMSYSTEM.2")["seg9:0x11e8"] == [0, 0]
    assert sites(isp, "MMSYSTEM.2 sndPlaySound") == ["seg9:0x11e8", "seg9:0x1333"]
    flags = {text(isp, a) for a in ("seg9:0x1311", "seg9:0x131e", "seg9:0x1325")}
    assert flags == {"mov word ptr [bp - 0x12], 3", "mov word ptr [bp - 0x12], 0xf",
                     "mov word ptr [bp - 0x12], 7"}
    assert isp.step(isp.at("seg9:0x1333"), -1).text == "push word ptr [bp - 0x12]"


def test_mci_send_command_sites(isp):
    """Sound: the mciSendCommand calls, their commands and flags."""
    got = {a: (v[1], v[2]) for a, v in call_args("ispace").args("MMSYSTEM.701").items()}
    assert got == {"seg9:0x381b": (0x803, 0x2200), "seg9:0x3851": (0x814, 0x100),
                   "seg9:0x38bb": (0x80D, 0x400), "seg9:0x3a2f": (0x804, 0x2),
                   "seg9:0x3a80": (0x809, 0x0), "seg9:0x3b2f": (0x807, 0xA),
                   "seg9:0x3b4d": (0x807, 0x9), "seg9:0x3bc1": (0x806, 0x9)}
    assert _cstr(unpacked_seg("ispace", 11), 0xAE4) == b"sequencer"


def test_wavemix_library(isp):
    """Sound: ISPACE.DLL loads WAVEMIX.DLL at seg9:0x3677."""
    assert "seg9:0x3677" in sites(isp, "KERNEL.95 LoadLibrary")
    assert isp.step(isp.at("seg9:0x3677"), -1).text == "push 0xa95"
    assert _cstr(unpacked_seg("ispace", 11), 0xA95).upper().endswith(b"WAVEMIX.DLL")


def test_game_port_instructions():
    """Joystick: the only in, out, cli, sti and pushf are ISPACE.DLL's game-port loop."""
    priv = {"in", "out", "cli", "sti", "pushf", "popf", "hlt", "insb", "insw", "outsb", "outsw"}
    got = [(n, i.addr, i.text) for n in NAMES for i in module(n).insns if i.mnem in priv]
    assert got == [("ispace", "seg9:0x4010", "in al, dx"), ("ispace", "seg9:0x401e", "pushf"),
                   ("ispace", "seg9:0x401f", "cli"), ("ispace", "seg9:0x4058", "out dx, al"),
                   ("ispace", "seg9:0x405d", "in al, dx"), ("ispace", "seg9:0x40a7", "sti")]
    isp = module("ispace")
    assert text(isp, "seg9:0x400d") == "mov dx, 0x201"
    assert {owner(isp, a) for _n, a, _t in got} == {"seg9:0x3ffe"}
    # ordinal 41 reads the port and scales the count by 771 / 1000 + 14
    assert exports(isp)[41] == owner(isp, "seg9:0x3eb9") == owner(isp, "seg9:0x3ed4")
    assert text(isp, "seg9:0x3eb9") == "call seg9:0x3ffe"
    assert [isp.step(isp.at("seg9:0x3ed4"), k).text for k in (-2, -1, 0, 1, 4, 5)] == [
        "push 0", "push 0x3e8", "mov ax, 0x303", "mul word ptr [0x229e]", "call far seg3:0x130",
        "add ax, 0xe"]


def test_get_async_key_state_sites(ins):
    """Keyboard: the eleven GetAsyncKeyState calls, their keys and the bits they test."""
    keys = {a: v[0] for a, v in call_args("inspace").args("USER.249").items()}
    assert keys == {"seg11:0x4dde": 0x10, "seg11:0x4dea": 0x20, "seg11:0x5973": 0x27,
                    "seg11:0x5987": 0x12, "seg11:0x5a25": 0x12, "seg11:0x5a11": 0x25,
                    "seg11:0x5f7c": 0x28, "seg11:0x5fb4": 0x26, "seg11:0x6456": 0x11,
                    "seg13:0xcd4": 0x0D, "seg13:0xce0": 0x5A}
    tests = {a: ins.step(ins.at(a), 1).text for a in keys}
    assert {a for a, t in tests.items() if t == "test ax, 0x8001"} == {"seg11:0x5f7c",
                                                                      "seg11:0x5fb4"}
    bit15 = {a for a, t in tests.items() if t in ("test ah, 0x80", "and ax, 0x8000")}
    assert len(bit15) == 8 and bit15 | {"seg11:0x5f7c", "seg11:0x5fb4", "seg11:0x6456"} == set(keys)


def test_key_state_calls(ins):
    """Keyboard: GetKeyState for VK_CONTROL and VK_MENU; Get/SetKeyboardState."""
    assert {a: v[0] for a, v in call_args("inspace").args("USER.106").items()} == {
        "seg11:0x6cde": 0x11, "seg11:0x6cfd": 0x12, "seg16:0xd46": 0x11, "seg16:0x6937": 0x11}
    assert sites(ins, "USER.222 GetKeyboardState") == ["seg2:0xa1a"]
    assert sites(ins, "USER.223 SetKeyboardState") == ["seg2:0xa3e"]


# ------------------------------------------------------------------ Win16-only APIs


def test_win16_only_api_sites(ins, isp, fac):
    """Win16-only APIs: the table's call sites."""
    assert {a: v for a, v in call_args("inspace").args("USER.284").items()} == {
        "seg6:0x140b": [1], "seg7:0x42": [1]}
    assert text(ins, "seg6:0x1413") == "cmp ax, 0x1e"
    assert sites(isp, "KERNEL.132 GetWinFlags") == ["seg8:0x5f4c", "seg10:0x444d",
                                                    "seg10:0x5645", "seg10:0x58eb", "seg10:0x62a0"]
    assert sites(fac, "KERNEL.132 GetWinFlags") == ["seg4:0x971"]
    assert sites(ins, "KERNEL.132 GetWinFlags") == []
    assert reloc_sites("ispace")["KERNEL.178 __WINFLAGS"] == ["seg3:0x10", "seg3:0x17"]
    assert reloc_sites("factory-1998a")["KERNEL.178 __WINFLAGS"] == ["seg3:0x10"]
    assert sites(ins, "USER.224 GetWindowTask") == ["seg2:0x12cb", "seg2:0x12d6",
                                                    "seg10:0x7425", "seg10:0x7430"]
    assert {"seg2:0x12bf", "seg10:0x7419"} <= set(sites(ins, "USER.60 GetActiveWindow"))
    assert sites(fac, "USER.224 GetWindowTask") == ["seg2:0x633", "seg2:0x63e"]
    assert call_args("inspace").args("KERNEL.54") == {"seg6:0x12d2": [None, 0x16, 2]}
    assert text(ins, "seg6:0x1305") == "push 0x4a0"
    assert _cstr(unpacked_seg("inspace", 19), 0x4A0).startswith(
        b"Another version of game already running")
    assert sites(fac, "KERNEL.54 GetInstanceData") == ["seg4:0x6f9"]
    assert sites(ins, "USER.266 SetMessageQueue") == ["seg6:0x1370"]
    assert sites(fac, "USER.266 SetMessageQueue") == ["seg4:0x763"]
    assert sites(ins, "USER.171 WinHelp") == ["seg2:0x1987"]
    assert sites(isp, "USER.171 WinHelp") == ["seg9:0x330e", "seg9:0x420c"]
    assert sites(fac, "USER.171 WinHelp") == ["seg2:0x13bd"]
    assert sites(ins, "KERNEL.3 GetVersion") == ["seg5:0x48", "seg10:0xd3"]
    assert sites(ins, "USER.249 GetAsyncKeyState")[0] == "seg11:0x4dde"


def test_get_win_flags_tests(isp):
    """Win16-only APIs: WF_CPU286 at seg8:0x5f4c; WF_WINNT (0x4000) at three sites."""
    def first_test(a: str) -> str:
        i = isp.at(a)
        while not i.mnem == "test":
            i = isp.step(i, 1)
        return i.text

    assert first_test("seg8:0x5f4c") == "test al, 2"
    for a in ("seg10:0x444d", "seg10:0x5645", "seg10:0x62a0"):
        assert first_test(a) == "test ah, 0x40", a


def test_dpmi_and_drive_calls(isp):
    """Win16-only APIs: GlobalDOSAlloc/Free, DPMI int 31h, IOCTL and GetDriveType."""
    assert sites(isp, "KERNEL.184 GlobalDOSAlloc") == ["seg10:0x457b", "seg10:0x566e",
                                                       "seg10:0x5998"]
    assert sites(isp, "KERNEL.185 GlobalDOSFree") == ["seg10:0x45f0", "seg10:0x5891",
                                                      "seg10:0x5ad9"]
    assert owner(isp, "seg10:0x461e") == "seg10:0x4602"
    assert text(isp, "seg10:0x4610") == "mov ax, 0x300"
    for a, cx in (("seg10:0x45a9", 0x866), ("seg10:0x5698", 0x866), ("seg10:0x59c5", 0x866),
                  ("seg10:0x583e", 0x846)):
        assert text(isp, a) == f"mov word ptr [{0x1F42 + 0x18:#x}], {cx:#x}", a
    assert [i.addr for i in isp.insns if i.text.startswith("mov word ptr [0x1f5a], ")] == [
        "seg10:0x45a9", "seg10:0x5698", "seg10:0x583e", "seg10:0x59c5"]
    assert text(isp, "seg10:0x6271") == "mov cl, 0x60"
    assert (text(isp, "seg10:0x62cd"), text(isp, "seg10:0x62d5")) == ("mov ax, 0x150b",
                                                                      "int 0x2f")
    assert sites(isp, "KERNEL.136 GetDriveType") == ["seg10:0x631b"]
    assert owner(isp, "seg10:0x631b") == "seg10:0x62f0"
    callees = {t for t, _k in isp.funcs["seg10:0x62f0"]["calls"]}
    assert {owner(isp, "seg10:0x6279"), owner(isp, "seg10:0x62d5")} <= callees


def test_runtime_support_imports_in_rtl_or_entry():
    """Win16-only APIs: start-up imports sit in the RTL, the library entry or the stub."""
    imps = ("KERNEL.91 InitTask", "USER.5 InitApp", "KERNEL.30 WaitEvent",
            "KERNEL.131 GetDOSEnvironment", "KERNEL.23 LockSegment", "KERNEL.24 UnlockSegment",
            "KERNEL.4 LocalInit", "KERNEL.48 GetModuleUsage", "KERNEL.1 FatalExit")
    for name in NAMES:
        m = module(name)
        cs = ne_info(name)["header"]["cs"]
        for imp in imps:
            assert {A(a)[0] for a in sites(m, imp)} <= {RTL[name], cs}, (name, imp)
    assert ne_info("ispace")["header"]["cs"] == 4
    assert sites(module("ispace"), "KERNEL.4 LocalInit") == ["seg3:0x7b", "seg4:0x16"]


def test_window_word_and_icon_calls(ins, isp, fac):
    """Win16-only APIs: SetWindowWord, SetClassWord, GetWindowWord, ExtractIcon."""
    assert {a: v[1] for a, v in call_args("inspace").args("USER.134").items()} == {
        "seg2:0xf75": 0xFFF4, "seg2:0xf8b": 0xFFF4, "seg2:0x16bb": 0xFFF4, "seg6:0xd72": 0xFFF4}
    assert sites(ins, "USER.158 SetMenu") and "seg2:0x1701" in sites(ins, "USER.158 SetMenu")
    assert [text(ins, a) for a in ("seg6:0x17fa", "seg2:0x16f2")] == [
        "mov word ptr [0x90fa], ax"] * 2
    assert {a: v[1] for a, v in call_args("inspace").args("USER.130").items()} == {
        "seg6:0x2e8": 0xFFF2, "seg6:0xa5d": 0xFFF2}
    gww = call_args("ispace").args("USER.133")
    assert len(gww) == 5 and {v[1] for v in gww.values()} == {0xFFFA}
    assert min(gww, key=A) == "seg9:0x164e"
    assert call_args("factory-1998a").args("USER.133") == {"seg2:0x72c": [None, 0xFFF4]}
    icons = {a: v[2] for a, v in call_args("inspace").args("SHELL.34").items()}
    assert icons == {"seg6:0x442c": 0xFFFF, "seg9:0x178": 0xFFFF, "seg6:0x4469": 0,
                     "seg9:0x1a5": None, "seg9:0x251": 0}
