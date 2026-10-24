"""Recursive-descent disassembly core for 16-bit NE code segments (``isre asm``).

The engine works on code segments with their relocation sites already
resolved (``Site``). It decodes with capstone in 16-bit mode, follows near
branches, near calls and relocated far calls, and keeps a per-byte state map
so that no byte is decoded twice and no relocation site is decoded as an
opcode:

* an instruction is accepted only if every relocation site it touches lies
  wholly inside its operand bytes (after the prefixes, opcode and ModRM);
* a direct far call or jump must carry a relocation on its segment word
  (protected-mode code can't hold a literal selector);
* a path that runs into the middle of an earlier instruction, into data or
  past the segment's data is stopped and the conflict recorded.

Seeds arrive through ``add_seed`` with a priority; lower priorities are
decoded first, so reliable seeds claim bytes before fallback ones.
Tentative seeds (far pointers, prologue matches, gap starts) are decoded into
a scratch state and committed only when the whole reachable body decodes
without a conflict or a suspicious instruction.

Indirect jumps through ``cs:[reg+table]`` are resolved as switch tables when
a ``cmp reg, N`` followed by an unsigned conditional jump guards the path
(Borland C's dense switch: ``dec ax; cmp ax,N; ja default; add ax,ax;
xchg bx,ax; jmp cs:[bx+table]``). The table bytes become a data region.
"""
from __future__ import annotations

import heapq
import re
import struct
from array import array
from dataclasses import dataclass, field
from typing import Callable, Iterable

from capstone import CS_ARCH_X86, CS_MODE_16, Cs
from capstone import x86 as X

UNKNOWN, HEAD, BODY, DATA = 0, 1, 2, 3

PREFIXES = frozenset({0xF0, 0xF2, 0xF3, 0x2E, 0x36, 0x3E, 0x26, 0x64, 0x65, 0x66, 0x67})

# Control-flow kinds.
(F_SEQ, F_COND, F_JMP, F_JMPI, F_JMPF, F_CALL, F_CALLI, F_CALLF, F_CALLFI, F_RET, F_INT,
 F_STOP) = range(12)
FALLS_THROUGH = frozenset({F_SEQ, F_COND, F_CALL, F_CALLI, F_CALLF, F_CALLFI, F_INT})

UNSIGNED_JCC = frozenset({"ja", "jae", "jb", "jbe"})

# Mnemonics that compiled Win16 application code doesn't contain; a tentative
# (fallback) decode that meets one is rejected.
SUSPICIOUS = frozenset({
    "hlt", "in", "out", "insb", "insw", "insd", "outsb", "outsw", "outsd", "into", "int3",
    "int1", "arpl", "bound", "salc", "aaa", "aas", "daa", "das", "aam", "aad", "lock",
    "ud0", "ud1", "ud2", "lds", "les", "lss", "lfs", "lgs", "iret", "iretd", "sti", "cli",
    "wait", "clts", "lgdt", "lidt", "lldt", "ltr", "lmsw", "smsw", "sgdt", "sidt", "sldt",
    "str", "verr", "verw", "lar", "lsl", "rdtsc", "cpuid", "pushal", "popal",
})
# Allowed in tentative decodes despite looking odd: Borland code uses les/lds.
TENTATIVE_OK = frozenset({"les", "lds"})

# Instructions that write AL/AH/AX without naming it as their first operand.
IMPLICIT_AX = frozenset({
    "mul", "imul", "div", "idiv", "cbw", "cwde", "cwd", "cdq", "lodsb", "lodsw", "lodsd",
    "in", "xlatb", "lahf", "aaa", "aas", "aad", "aam", "daa", "das", "salc", "popa", "popal",
    "popaw", "cmpxchg", "xchg", "xadd",
})
_AX_RE = re.compile(r"\b(al|ah|ax|eax)\b")

_REG_FAMILY = {}
for _fam, _regs in (("a", ("al", "ah", "ax", "eax")), ("b", ("bl", "bh", "bx", "ebx")),
                    ("c", ("cl", "ch", "cx", "ecx")), ("d", ("dl", "dh", "dx", "edx")),
                    ("si", ("si", "esi")), ("di", ("di", "edi")), ("bp", ("bp", "ebp")),
                    ("sp", ("sp", "esp"))):
    for _r in _regs:
        _REG_FAMILY[_r] = _fam


@dataclass
class Site:
    """One relocation site: ``width`` bytes at ``off`` that the loader patches."""
    seg: int
    off: int
    width: int
    kind: str                      # import | selector | internal | osfloat
    target_type: str               # OptLoader / NE target type (PTR16, OFF16, SEG16, ...)
    head: int                      # offset of the chain head (the record's offset)
    label: str                     # "KERNEL.102 DOS3Call", "seg12", "seg6:0xf1a"
    module: str | None = None
    ordinal: int | None = None
    ref_seg: int | None = None     # selector / internal target segment
    target: tuple[int, int] | None = None   # internal far pointer target

    @property
    def addr(self) -> str:
        return f"seg{self.seg}:{self.off:#x}"


@dataclass
class CodeSeg:
    n: int
    data: bytes          # image bytes (zero-padded to minalloc)
    length: int          # bytes that hold code or data; decoding stops here
    flags: int
    sites: dict[int, Site] = field(default_factory=dict)


@dataclass
class DataRegion:
    seg: int
    off: int
    size: int
    kind: str                       # jumptable | cs_data | header
    ref: tuple[int, int] | None = None
    note: str = ""


@dataclass
class Mem:
    segreg: str          # explicit segment override ("" when default)
    base: str
    index: str
    disp: int            # 16-bit displacement, unsigned
    size: int

    @property
    def absolute(self) -> bool:
        return not self.base and not self.index


class Insn:
    __slots__ = ("seg", "off", "size", "mnem", "ops", "flow", "target", "op_start",
                 "mem", "imm", "far_off", "noreturn")

    def __init__(self, seg: int, off: int, size: int, mnem: str, ops: str, flow: int,
                 target: int | None, op_start: int, mem: Mem | None,
                 imm: tuple[tuple[int, int], ...], far_off: int | None):
        self.seg = seg
        self.off = off
        self.size = size
        self.mnem = mnem
        self.ops = ops
        self.flow = flow
        self.target = target
        self.op_start = op_start
        self.mem = mem
        self.imm = imm
        self.far_off = far_off
        self.noreturn = False

    @property
    def end(self) -> int:
        return self.off + self.size


@dataclass
class Call:
    seg: int
    off: int                          # the calling instruction
    kind: str                         # near | far | import | jmp_far | far_ptr
    target: tuple[int, int] | None    # internal target
    site: Site | None = None          # import / selector site


@dataclass
class Switch:
    seg: int
    off: int              # the indirect jmp
    table: int
    count: int
    cmp_off: int
    targets: list[int]


@dataclass
class Rejected:
    seg: int
    off: int
    seed: str
    reason: str


@dataclass
class _Scratch:
    insns: dict[int, Insn] = field(default_factory=dict)
    claimed: dict[int, int] = field(default_factory=dict)       # byte -> HEAD/BODY
    edges: list[tuple[int, int, str]] = field(default_factory=list)   # (to, from, kind)
    calls: list[Call] = field(default_factory=list)
    seeds: list[tuple[int, int, int, str, bool]] = field(default_factory=list)
    pending: list[int] = field(default_factory=list)
    noreturn: list[int] = field(default_factory=list)
    errors: list[tuple[int, str]] = field(default_factory=list)
    suspicious: list[tuple[int, str]] = field(default_factory=list)


def operand_start(code: bytes, ins) -> int:
    """Offset of the first operand byte (displacement or immediate) in ``ins``."""
    p = 0
    while p < len(code) and code[p] in PREFIXES:
        p += 1
    mo = ins.modrm_offset
    if mo:
        q = mo + 1
        if 0x67 in code[:p] and (ins.modrm & 7) == 4 and (ins.modrm >> 6) != 3:
            q += 1                                   # SIB byte (32-bit addressing)
        return q
    if p < len(code) and code[p] == 0x0F:
        return p + (3 if p + 1 < len(code) and code[p + 1] in (0x38, 0x3A) else 2)
    return p + 1


class Disassembler:
    """Global recursive descent over one module's code segments."""

    def __init__(self, segs: dict[int, CodeSeg],
                 is_dos_call: Callable[[Site], bool] | None = None,
                 import_noreturn: Callable[[Site], bool] | None = None):
        self.segs = segs
        self.md = Cs(CS_ARCH_X86, CS_MODE_16)
        self.md.detail = True
        self.is_dos_call = is_dos_call or (lambda s: False)
        self.import_noreturn = import_noreturn or (lambda s: False)
        self.state = {n: bytearray(len(s.data)) for n, s in segs.items()}
        self.insns: dict[int, dict[int, Insn]] = {n: {} for n in segs}
        self.preds: dict[int, dict[int, list[tuple[int, str]]]] = {n: {} for n in segs}
        self.site_at: dict[int, array] = {}
        for n, s in segs.items():
            arr = array("i", [-1]) * len(s.data)
            for so, site in s.sites.items():
                for b in range(so, min(so + site.width, len(s.data))):
                    arr[b] = so
            self.site_at[n] = arr
        self.functions: dict[tuple[int, int], set[str]] = {}
        self.calls: list[Call] = []
        self.data: dict[tuple[int, int], DataRegion] = {}
        self.switches: dict[tuple[int, int], Switch] = {}
        self.unresolved: dict[tuple[int, int], str] = {}
        self.errors: list[tuple[int, int, str]] = []
        self.suspicious: list[tuple[int, int, str]] = []
        self.rejected: list[Rejected] = []
        self._queue: list[tuple[int, int, int, str, bool]] = []
        self._queued: set[tuple[int, int, str]] = set()
        self._pending: list[tuple[int, int]] = []
        self._noreturn: list[tuple[int, int]] = []
        self._farptr_done: set[tuple[int, int]] = set()
        self._cs_cache: dict[tuple[int, int], object] = {}

    # ------------------------------------------------------------ seeds

    def add_seed(self, seg: int, off: int, seed: str, prio: int, tentative: bool = False) -> None:
        key = (seg, off, seed)
        if key in self._queued:
            return
        self._queued.add(key)
        heapq.heappush(self._queue, (prio, seg, off, seed, tentative))

    def mark_data(self, region: DataRegion) -> bool:
        """Claim a data region; False if any byte is already code."""
        st = self.state.get(region.seg)
        if st is None:
            return False
        end = min(region.off + region.size, len(st))
        if any(st[b] in (HEAD, BODY) for b in range(region.off, end)):
            return False
        for b in range(region.off, end):
            st[b] = DATA
        key = (region.seg, region.off)
        old = self.data.get(key)
        if old is None or old.size < region.size:
            self.data[key] = region
        return True

    # ------------------------------------------------------------ decoding

    def _cs(self, seg: int, off: int):
        """Capstone instruction (with detail) at seg:off, cached."""
        key = (seg, off)
        ins = self._cs_cache.get(key)
        if ins is None:
            s = self.segs[seg]
            ins = next(self.md.disasm(s.data[off:min(off + 15, s.length)], off, 1), None)
            if len(self._cs_cache) > 4096:
                self._cs_cache.clear()
            self._cs_cache[key] = ins
        return ins

    def decode(self, seg: int, off: int) -> tuple[Insn | None, object, str]:
        """Decode one instruction; returns (Insn, capstone insn, error)."""
        s = self.segs[seg]
        if off >= s.length:
            return None, None, "past the end of the segment data"
        code = s.data[off:min(off + 15, s.length)]
        ins = next(self.md.disasm(code, off, 1), None)
        if ins is None:
            return None, None, f"invalid opcode {code[:2].hex()}"
        n = ins.size
        ops = operand_start(code, ins)
        sa = self.site_at[seg]
        b = off
        while b < off + n:
            so = sa[b]
            if so >= 0:
                site = s.sites[so]
                if so < off + ops or so + site.width > off + n:
                    return None, ins, (f"relocation site {site.addr} ({site.label}) "
                                       "not in operand bytes")
                b = so + site.width
            else:
                b += 1
        m = ins.mnemonic
        g = ins.groups
        target = None
        far_off = None
        mem = None
        imms = []
        is_imm_target = False
        for op in ins.operands:
            if op.type == X.X86_OP_MEM and mem is None:
                mm = op.mem
                mem = Mem(ins.reg_name(mm.segment) if mm.segment else "",
                          ins.reg_name(mm.base) if mm.base else "",
                          ins.reg_name(mm.index) if mm.index else "",
                          mm.disp & 0xFFFF, op.size)
            elif op.type == X.X86_OP_IMM:
                imms.append((op.imm, op.size))
                is_imm_target = True
        if m in ("ret", "retf", "iret", "iretd") or X.X86_GRP_RET in g or X.X86_GRP_IRET in g:
            flow = F_RET
        elif m == "ljmp":
            flow = F_JMPF
            if len(imms) == 2:
                far_off = struct.unpack_from("<H", s.data, off + ops)[0]
        elif m == "jmp":
            flow = F_JMP if is_imm_target and mem is None else F_JMPI
        elif X.X86_GRP_JUMP in g or (X.X86_GRP_BRANCH_RELATIVE in g and m not in ("call", "lcall")):
            flow = F_COND                            # jcc, loop*, jcxz
        elif m == "lcall":
            if len(imms) == 2:
                flow = F_CALLF
                far_off = struct.unpack_from("<H", s.data, off + ops)[0]
            else:
                flow = F_CALLFI
        elif m == "call":
            flow = F_CALL if is_imm_target and mem is None else F_CALLI
        elif m in ("int", "int1", "int3", "into"):
            flow = F_INT
        elif m in ("hlt", "ud0", "ud1", "ud2"):
            flow = F_STOP
        else:
            flow = F_SEQ
        if flow in (F_JMP, F_COND, F_CALL):
            target = imms[0][0] & 0xFFFF
            imms = []
        if far_off is not None:
            imms = []
        insn = Insn(seg, off, n, m, ins.op_str, flow, target, ops, mem, tuple(imms), far_off)
        return insn, ins, ""

    def far_site(self, insn: Insn) -> Site | None:
        """The relocation site of a direct far call/jmp: offset word or segment word."""
        s = self.segs[insn.seg]
        site = s.sites.get(insn.off + insn.op_start)
        if site is not None and site.width == 4:
            return site
        return s.sites.get(insn.off + insn.op_start + 2)

    # ------------------------------------------------------------ AX tracking

    @staticmethod
    def _ax_update(cs_ins, al: int | None, ah: int | None) -> tuple[int | None, int | None]:
        m = cs_ins.mnemonic
        ops = cs_ins.operands
        if len(ops) == 2 and ops[0].type == X.X86_OP_REG:
            r0 = ops[0].reg
            if m == "mov" and ops[1].type == X.X86_OP_IMM:
                v = ops[1].imm
                if r0 == X.X86_REG_AL:
                    return v & 0xFF, ah
                if r0 == X.X86_REG_AH:
                    return al, v & 0xFF
                if r0 in (X.X86_REG_AX, X.X86_REG_EAX):
                    return v & 0xFF, (v >> 8) & 0xFF
            if ops[1].type == X.X86_OP_REG:
                r1 = ops[1].reg
                if m == "mov" and r0 == X.X86_REG_AH and r1 == X.X86_REG_AL:
                    return al, al
                if m == "mov" and r0 == X.X86_REG_AL and r1 == X.X86_REG_AH:
                    return ah, ah
                if m == "xchg" and {r0, r1} == {X.X86_REG_AL, X.X86_REG_AH}:
                    return ah, al
                if m in ("xor", "sub") and r0 == r1:
                    if r0 in (X.X86_REG_AX, X.X86_REG_EAX):
                        return 0, 0
                    if r0 == X.X86_REG_AL:
                        return 0, ah
                    if r0 == X.X86_REG_AH:
                        return al, 0
        if m not in IMPLICIT_AX and not _AX_RE.search(cs_ins.op_str):
            return al, ah
        try:
            _, written = cs_ins.regs_access()
        except Exception:                      # capstone can't describe some insns
            return None, None
        w = set(written)
        if w & {X.X86_REG_AX, X.X86_REG_EAX}:
            return None, None
        if X.X86_REG_AL in w:
            al = None
        if X.X86_REG_AH in w:
            ah = None
        return al, ah

    def dos_call_site(self, insn: Insn) -> bool:
        if insn.flow == F_INT:
            return insn.imm[:1] == ((0x21, 1),) or insn.ops.strip() == "0x21"
        if insn.flow == F_CALLF:
            site = self.far_site(insn)
            return site is not None and site.kind == "import" and self.is_dos_call(site)
        return False

    def reg_values(self, seg: int, off: int, reg: str, depth: int = 0) -> tuple[set[int], bool]:
        """Possible constant values of ``reg`` (al or ah) just before seg:off.

        Walks predecessors backwards. Returns (values, incomplete) where
        incomplete means some path left the value unknown.
        """
        from isre.x86names import DOS_PRESERVES_AX

        vals: set[int] = set()
        incomplete = False
        stack = [(off, reg, 0)]
        seen: set[tuple[int, str]] = set()
        preds = self.preds[seg]
        insns = self.insns[seg]
        while stack:
            cur, r, d = stack.pop()
            if (cur, r) in seen:
                continue
            seen.add((cur, r))
            ps = preds.get(cur)
            if not ps or d > 48:
                incomplete = True
                continue
            for p, _kind in ps:
                pi = insns.get(p)
                if pi is None:
                    incomplete = True
                    continue
                if pi.flow in (F_CALL, F_CALLI, F_CALLF, F_CALLFI, F_INT):
                    if depth < 3 and self.dos_call_site(pi):
                        ahs, inc = self.reg_values(seg, p, "ah", depth + 1)
                        if not inc and ahs and ahs <= DOS_PRESERVES_AX:
                            stack.append((p, r, d + 1))
                            continue
                    incomplete = True
                    continue
                ci = self._cs(seg, p)
                if ci is None:
                    incomplete = True
                    continue
                al, ah = self._ax_update(ci, -1, -2)
                v = al if r == "al" else ah
                if v is None:
                    incomplete = True
                elif v == -1:
                    stack.append((p, "al", d + 1))
                elif v == -2:
                    stack.append((p, "ah", d + 1))
                else:
                    vals.add(v)
        return vals, incomplete

    # ------------------------------------------------------------ exploration

    def _explore(self, seg: int, start: int, tentative: bool) -> _Scratch:
        from isre.x86names import DOS_NORETURN, DOS_PRESERVES_AX

        sc = _Scratch()
        st = self.state[seg]
        s = self.segs[seg]
        stack: list[tuple[int, int | None, int | None]] = [(start, None, None)]
        while stack:
            off, al, ah = stack.pop()
            while True:
                if off >= s.length:
                    sc.errors.append((off, "runs past the end of the segment data"))
                    break
                if off in sc.insns or st[off] == HEAD:
                    break
                state = sc.claimed.get(off, st[off])
                if state == BODY:
                    sc.errors.append((off, "jumps into the middle of an instruction"))
                    break
                if state == DATA:
                    sc.errors.append((off, "runs into data"))
                    break
                insn, ci, err = self.decode(seg, off)
                if insn is None:
                    sc.errors.append((off, err))
                    break
                clash = next((b for b in range(off + 1, off + insn.size)
                              if sc.claimed.get(b, st[b]) != UNKNOWN), None)
                if clash is not None:
                    sc.errors.append((off, f"overlaps code or data at seg{seg}:{clash:#x}"))
                    break
                if insn.mnem in SUSPICIOUS and not (tentative and insn.mnem in TENTATIVE_OK):
                    sc.suspicious.append((off, insn.mnem))
                if tentative and s.data[off:off + 2] == b"\x00\x00":
                    sc.suspicious.append((off, "zero bytes"))
                sc.insns[off] = insn
                sc.claimed[off] = HEAD
                for b in range(off + 1, off + insn.size):
                    sc.claimed[b] = BODY
                nxt = insn.end
                fl = insn.flow
                if fl == F_COND:
                    sc.edges.append((insn.target, off, "jump"))
                    stack.append((insn.target, al, ah))
                elif fl == F_JMP:
                    sc.edges.append((insn.target, off, "jump"))
                    stack.append((insn.target, al, ah))
                    break
                elif fl == F_JMPI:
                    sc.pending.append(off)
                    break
                elif fl == F_JMPF:
                    if insn.far_off is not None:
                        site = self.far_site(insn)
                        if site is None:
                            sc.errors.append((off, "far jmp without a relocation"))
                        elif site.kind == "selector":
                            tgt = (site.ref_seg, insn.far_off)
                            sc.calls.append(Call(seg, off, "jmp_far", tgt, site))
                            sc.seeds.append((tgt[0], tgt[1], 1, "far_jmp", False))
                        elif site.target is not None:
                            sc.calls.append(Call(seg, off, "jmp_far", site.target, site))
                            sc.seeds.append((site.target[0], site.target[1], 1, "far_jmp", False))
                        else:
                            sc.calls.append(Call(seg, off, "import", None, site))
                    break
                elif fl == F_RET or fl == F_STOP:
                    break
                elif fl == F_CALL:
                    sc.calls.append(Call(seg, off, "near", (seg, insn.target)))
                    sc.seeds.append((seg, insn.target, 2, "near_call", False))
                    al = ah = None
                elif fl == F_CALLF:
                    site = self.far_site(insn)
                    if site is None:
                        sc.errors.append((off, "far call without a relocation"))
                        al = ah = None
                    elif site.kind == "selector" or site.target is not None:
                        tgt = ((site.ref_seg, insn.far_off) if site.kind == "selector"
                               else site.target)
                        sc.calls.append(Call(seg, off, "far", tgt, site))
                        sc.seeds.append((tgt[0], tgt[1], 1, "far_call", False))
                        al = ah = None
                    else:
                        sc.calls.append(Call(seg, off, "import", None, site))
                        if self.is_dos_call(site):
                            if ah in DOS_NORETURN:
                                insn.noreturn = True
                                sc.noreturn.append(off)
                                break
                            if ah not in DOS_PRESERVES_AX:
                                al = ah = None
                        elif self.import_noreturn(site):
                            insn.noreturn = True
                            break
                        else:
                            al = ah = None
                elif fl in (F_CALLI, F_CALLFI):
                    al = ah = None
                elif fl == F_INT:
                    if insn.imm[:1] == ((0x20, 1),):
                        insn.noreturn = True
                        break
                    if insn.imm[:1] == ((0x21, 1),):
                        if ah in DOS_NORETURN:
                            insn.noreturn = True
                            sc.noreturn.append(off)
                            break
                        if ah not in DOS_PRESERVES_AX:
                            al = ah = None
                    else:
                        al = ah = None
                else:
                    al, ah = self._ax_update(ci, al, ah)
                sc.edges.append((nxt, off, "fall"))
                off = nxt
        return sc

    def _commit(self, seg: int, sc: _Scratch) -> None:
        st = self.state[seg]
        ins = self.insns[seg]
        for off, insn in sc.insns.items():
            ins[off] = insn
            st[off] = HEAD
            for b in range(off + 1, insn.end):
                st[b] = BODY
        preds = self.preds[seg]
        for to, frm, kind in sc.edges:
            lst = preds.setdefault(to, [])
            if (frm, kind) not in lst:
                lst.append((frm, kind))
        self.calls.extend(sc.calls)
        for tseg, toff, prio, seed, tent in sc.seeds:
            self.add_seed(tseg, toff, seed, prio, tent)
        self._pending.extend((seg, o) for o in sc.pending)
        self._noreturn.extend((seg, o) for o in sc.noreturn)
        self.errors.extend((seg, o, e) for o, e in sc.errors)
        self.suspicious.extend((seg, o, e) for o, e in sc.suspicious)

    def _drain(self) -> bool:
        progressed = False
        while self._queue:
            prio, seg, off, seed, tentative = heapq.heappop(self._queue)
            if seg not in self.segs:
                self.rejected.append(Rejected(seg, off, seed, "not a code segment"))
                continue
            s = self.segs[seg]
            if off >= s.length:
                self.rejected.append(Rejected(seg, off, seed, "outside the segment data"))
                continue
            st = self.state[seg][off]
            if st == HEAD:
                self.functions.setdefault((seg, off), set()).add(seed)
                continue
            if st != UNKNOWN:
                what = "an instruction" if st == BODY else "data"
                self.rejected.append(Rejected(seg, off, seed, f"inside {what}"))
                continue
            sc = self._explore(seg, off, tentative)
            if off not in sc.insns:
                reason = sc.errors[0][1] if sc.errors else "no instruction"
                self.rejected.append(Rejected(seg, off, seed, reason))
                continue
            if tentative and (sc.errors or sc.suspicious):
                if sc.errors:
                    o, e = sc.errors[0]
                else:
                    o, e = sc.suspicious[0]
                    e = f"suspicious {e}"
                self.rejected.append(Rejected(seg, off, seed, f"{e} at seg{seg}:{o:#x}"))
                continue
            self._commit(seg, sc)
            self.functions.setdefault((seg, off), set()).add(seed)
            progressed = True
        return progressed

    # ------------------------------------------------------------ switch tables

    def _switch_bound(self, seg: int, off: int, reg: str) -> tuple[int, int] | None:
        """(entry count, cmp offset) for a jump table indexed by ``reg`` at seg:off."""
        fam = _REG_FAMILY.get(reg)
        if fam is None:
            return None
        preds = self.preds[seg]
        insns = self.insns[seg]
        # state: (instruction offset reached, tracked family, scaled seen, guard)
        stack = [(off, fam, False, None, 0)]
        seen = set()
        while stack:
            cur, f, scaled, guard, depth = stack.pop()
            if (cur, f, scaled, guard) in seen or depth > 16:
                continue
            seen.add((cur, f, scaled, guard))
            for p, kind in sorted(preds.get(cur, [])):
                pi = insns.get(p)
                if pi is None:
                    continue
                if pi.flow == F_COND:
                    g = guard
                    if g is None and pi.mnem in UNSIGNED_JCC:
                        g = (pi.mnem, kind)
                    stack.append((p, f, scaled, g, depth + 1))
                    continue
                if pi.flow not in (F_SEQ, F_JMP):
                    continue
                if pi.flow == F_JMP:
                    stack.append((p, f, scaled, guard, depth + 1))
                    continue
                ci = self._cs(seg, p)
                if ci is None:
                    continue
                ops = ci.operands
                m = ci.mnemonic
                r0 = ci.reg_name(ops[0].reg) if ops and ops[0].type == X.X86_OP_REG else ""
                r1 = ci.reg_name(ops[1].reg) if len(ops) > 1 and ops[1].type == X.X86_OP_REG else ""
                f0 = _REG_FAMILY.get(r0)
                if m == "cmp" and f0 == f and len(ops) == 2 and ops[1].type == X.X86_OP_IMM:
                    if guard is None:
                        continue
                    n = ops[1].imm & 0xFFFF
                    jcc, edge = guard
                    if (jcc, edge) in (("ja", "fall"), ("jbe", "jump")):
                        count = n + 1
                    elif (jcc, edge) in (("jae", "fall"), ("jb", "jump")):
                        count = n
                    else:
                        continue
                    if not scaled:
                        count = (count + 1) // 2
                    if 0 < count <= 1024:
                        return count, p
                    continue
                if f0 != f:
                    try:
                        _, written = ci.regs_access()
                    except Exception:
                        continue
                    names = {ci.reg_name(w) for w in written}
                    if any(_REG_FAMILY.get(w) == f for w in names):
                        continue        # some other write to the tracked register
                    stack.append((p, f, scaled, guard, depth + 1))
                    continue
                if m == "xchg" and r1:
                    stack.append((p, _REG_FAMILY.get(r1), scaled, guard, depth + 1))
                elif m == "mov" and r1:
                    stack.append((p, _REG_FAMILY.get(r1), scaled, guard, depth + 1))
                elif m == "add" and r1:
                    # add r,r doubles the index; Borland also emits mov bx,ax ... add ax,bx
                    stack.append((p, f, True, guard, depth + 1))
                elif m in ("shl", "sal") and len(ops) == 2 and ops[1].type == X.X86_OP_IMM \
                        and ops[1].imm == 1:
                    stack.append((p, f, True, guard, depth + 1))
                elif m in ("dec", "inc") or (m in ("sub", "add") and len(ops) == 2
                                             and ops[1].type == X.X86_OP_IMM):
                    stack.append((p, f, scaled, guard, depth + 1))
                else:
                    continue
        return None

    def _resolve_pending(self) -> bool:
        progressed = False
        pend, self._pending = self._pending, []
        for seg, off in pend:
            if (seg, off) in self.switches:
                continue
            insn = self.insns[seg].get(off)
            mem = insn.mem if insn else None
            if mem is None:
                self.unresolved[(seg, off)] = "register target"
                continue
            if mem.segreg != "cs" or not mem.base or mem.index:
                self.unresolved[(seg, off)] = "not a cs:[reg+table] jump"
                continue
            bound = self._switch_bound(seg, off, mem.base)
            if bound is None:
                self.unresolved[(seg, off)] = "no cmp/jcc bound found"
                continue
            count, cmp_off = bound
            s = self.segs[seg]
            table = mem.disp
            if table + 2 * count > s.length:
                self.unresolved[(seg, off)] = f"table at {table:#x} runs past the segment"
                continue
            targets = [struct.unpack_from("<H", s.data, table + 2 * i)[0] for i in range(count)]
            if any(t >= s.length for t in targets):
                self.unresolved[(seg, off)] = "table entry outside the segment"
                continue
            region = DataRegion(seg, table, 2 * count, "jumptable", (seg, off),
                                f"{count} entries, bound from seg{seg}:{cmp_off:#x}")
            if not self.mark_data(region):
                self.unresolved[(seg, off)] = f"table at {table:#x} overlaps decoded code"
                continue
            self.unresolved.pop((seg, off), None)
            self.switches[(seg, off)] = Switch(seg, off, table, count, cmp_off, targets)
            for t in sorted(set(targets)):
                sc = self._explore(seg, t, False)
                sc.edges.append((t, off, "switch"))
                self._commit(seg, sc)
            progressed = True
        return progressed

    # ------------------------------------------------------------ passes

    def _noreturn_recheck(self) -> bool:
        """Undo a no-return verdict when another path reaches the call with a different AH."""
        from isre.x86names import DOS_NORETURN

        progressed = False
        keep = []
        for seg, off in self._noreturn:
            insn = self.insns[seg][off]
            vals, inc = self.reg_values(seg, off, "ah")
            if not inc and vals and vals <= DOS_NORETURN:
                keep.append((seg, off))
                continue
            insn.noreturn = False
            sc = self._explore(seg, insn.end, False)
            sc.edges.append((insn.end, off, "fall"))
            self._commit(seg, sc)
            progressed = True
        self._noreturn = keep
        return progressed

    def _far_ptr_pass(self, prio: int = 2) -> bool:
        """Seed far pointers built from a selector relocation and a nearby offset."""
        added = False
        for seg, s in sorted(self.segs.items()):
            insns = self.insns[seg]
            for so, site in sorted(s.sites.items()):
                if site.kind != "selector" or (seg, so) in self._farptr_done:
                    continue
                st = self.state[seg]
                if st[so] != BODY:
                    continue
                start = so
                while start > 0 and st[start] != HEAD:
                    start -= 1
                insn = insns.get(start)
                if insn is None or insn.flow in (F_CALLF, F_JMPF):
                    continue
                self._farptr_done.add((seg, so))
                off_val = self._pair_offset(seg, insn, so)
                if off_val is None:
                    continue
                self.calls.append(Call(seg, insn.off, "far_ptr", (site.ref_seg, off_val), site))
                self.add_seed(site.ref_seg, off_val, "far_ptr", prio, True)
                added = True
        return added

    def _pair_offset(self, seg: int, insn: Insn, site_off: int) -> int | None:
        """Offset half of a far pointer whose selector half ``insn`` stores.

        ``push seg; push off`` pairs with the next instruction. A ``mov`` of the
        selector into [x+2] pairs with a ``mov`` of a constant into [x] among the
        four instructions that fall through into it, else among the four it
        falls through to; that search ends at a jmp, ret or other transfer that
        does not fall through.
        """
        insns = self.insns[seg]
        if insn.mnem == "push" and insn.mem is None:
            nxt = insns.get(insn.end)
            if nxt is not None and nxt.mnem == "push" and nxt.mem is None and nxt.imm:
                return nxt.imm[0][0] & 0xFFFF
            return None
        if insn.mnem == "mov" and insn.mem is not None and insn.imm:
            want = (insn.mem.disp - 2) & 0xFFFF
            near = []
            o = insn.off
            for _ in range(4):                       # preceding fall-through chain first
                ps = sorted(p for p, k in self.preds[seg].get(o, []) if k == "fall")
                if not ps:
                    break
                o = ps[0]
                near.append(insns[o])
            cur = insn
            for _ in range(4):                       # then the following fall-through chain,
                if cur.flow not in FALLS_THROUGH or cur.noreturn:    # which ends at jmp/ret
                    break
                cur = insns.get(cur.end)
                if cur is None:
                    break
                near.append(cur)
            for c in near:
                if (c.mnem == "mov" and c.mem is not None and c.imm
                        and c.mem.base == insn.mem.base and c.mem.index == insn.mem.index
                        and c.mem.segreg == insn.mem.segreg and c.mem.disp == want):
                    return c.imm[0][0] & 0xFFFF
        return None

    def _csdata_pass(self) -> bool:
        """Mark the targets of absolute ``cs:[disp]`` operands as data."""
        added = False
        for seg in sorted(self.insns):
            st = self.state[seg]
            n = self.segs[seg].length
            for off in sorted(self.insns[seg]):
                m = self.insns[seg][off].mem
                if m is None or m.segreg != "cs" or not m.absolute or m.disp >= n:
                    continue
                size = max(1, min(m.size or 2, n - m.disp))
                if all(st[b] == UNKNOWN for b in range(m.disp, m.disp + size)):
                    added |= self.mark_data(DataRegion(seg, m.disp, size, "cs_data", (seg, off)))
        return added

    def gaps(self, seg: int) -> list[tuple[int, int]]:
        """Undecoded [start, end) ranges within the segment data."""
        st = self.state[seg]
        n = self.segs[seg].length
        out = []
        i = 0
        while i < n:
            if st[i] == UNKNOWN:
                j = i
                while j < n and st[j] == UNKNOWN:
                    j += 1
                out.append((i, j))
                i = j
            else:
                i += 1
        return out

    def _scan_pass(self, finder: Callable[[int, bytes, int, int], Iterable[int]],
                   seed: str, prio: int) -> bool:
        added = False
        for seg, s in sorted(self.segs.items()):
            for a, b in self.gaps(seg):
                for off in finder(seg, s.data, a, b):
                    key = (seg, off, seed)
                    if key in self._queued:
                        continue
                    self.add_seed(seg, off, seed, prio, True)
                    added = True
        if added:
            added = self._drain()
        return added

    def run(self, fallbacks: list[tuple[Callable[[int, bytes, int, int], Iterable[int]], str, int]]
            = ()) -> None:
        """Decode until no pass finds anything new."""
        while True:
            self._drain()
            if self._resolve_pending():
                continue
            if self._noreturn_recheck():
                continue
            if self._far_ptr_pass():
                continue
            self._csdata_pass()
            if any(self._scan_pass(f, seed, prio) for f, seed, prio in fallbacks):
                continue
            if self._queue:
                continue
            break

    # ------------------------------------------------------------ results

    def successors(self, insn: Insn) -> list[tuple[int, str]]:
        out = []
        fl = insn.flow
        if fl in FALLS_THROUGH and not insn.noreturn:
            out.append((insn.end, "fall"))
        if fl in (F_COND, F_JMP):
            out.append((insn.target, "jump"))
        if fl == F_JMPI:
            sw = self.switches.get((insn.seg, insn.off))
            if sw:
                out.extend((t, "switch") for t in sorted(set(sw.targets)))
        return out

    def bodies(self) -> dict[tuple[int, int], tuple[list[int], list[int]]]:
        """Per function entry: (sorted instruction offsets, offsets of other entries reached)."""
        out = {}
        for seg, e in sorted(self.functions):
            insns = self.insns[seg]
            body: set[int] = set()
            tails: set[int] = set()
            stack = [e]
            while stack:
                o = stack.pop()
                if o in body:
                    continue
                if o != e and (seg, o) in self.functions:
                    tails.add(o)
                    continue
                insn = insns.get(o)
                if insn is None:
                    continue
                body.add(o)
                for t, _k in self.successors(insn):
                    stack.append(t)
            out[(seg, e)] = (sorted(body), sorted(tails))
        return out
