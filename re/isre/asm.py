"""``isre asm``: recursive-descent disassembly of the unpacked NE modules.

Usage::

    isre asm --work WORK [--module NAME ...] [--out DIR] [--index-dir DIR]
    isre asm --ne FILE --out DIR
    isre asm --work WORK --module NAME --show segN:0xOFF

``--work`` reads ``WORK/ne/<name>.json`` and ``WORK/unpacked/<name>/`` (as
written by ``isre all``) and writes ``<out>/<name>/segNN.asm``,
``functions.json`` and ``xrefs.json``; ``<out>`` defaults to ``WORK/asm``.
``--ne`` unpacks one OPTLOADER module in memory instead. ``--index-dir``
also writes the compact ``functions-<name>.json`` index there. ``--show``
prints the listing of the function holding an address from an earlier run:
its header (seeds, callers), then every piece of it in address order (body
ranges, shared tails, unreachable blocks, jump tables), with one ``; ....``
line for each stretch in between that belongs elsewhere.

Listing lines are ``address  bytes  text ; comments``: the bytes start in
column 14 and the text in column 41 (an instruction of more than 9 bytes
pushes it right but keeps a space); ``parse_line`` reads them back. A
relocation site is named in a comment; an additive one also shows the
addend the loader adds the target to. ``functions.json`` gives each
function's and each unreachable block's ``ranges``: runs of adjacent
instructions, end exclusive.

Seeds, in decoding priority (``seeds`` in functions.json):

    cs_ip      the NE header's CS:IP
    export     entry-table entries
    selfload   the OPTLOADER stub's self-load table (BootApp, LoadAppSeg, ExitProc)
    far_call   a far call whose segment word carries a special-base (selector)
               relocation: the target is <selector segment>:<offset word>
    far_jmp    the same for a far jmp
    near_call  the target of a decoded near call
    far_ptr    a far pointer assembled from a selector relocation and the
               offset stored next to it (push seg / push off, or two movs into
               adjacent words); decoded tentatively
    prologue   a Borland/Win16 prologue found in undecoded bytes; tentative
    gap        the first non-padding byte after a decoded function's last
               instruction; tentative. A gap block that sits inside another
               function and runs back into it is listed as unreachable code
               of that function instead (``unreachable_blocks``).

OPTLOADER special-base records encode internal far pointers like this: the
record names a segment number and the head of a chain of OFF16 sites; the
loader writes that segment's selector into each site (nappload.asm
PROCESS_RELOCS label 3$). At a far call ``9a oo oo ss ss`` the site is the
segment word, so the offset word ``oo oo`` still holds the real target
offset and the chain links live in the segment words.

Annotations assume DS = DGROUP (the module's autodata segment) and SS = DS
for BP-based operands, which holds for Borland's Win16 code outside a few
runtime routines that load DS themselves.
"""
from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from bisect import bisect_left, bisect_right
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Sequence

from isre import x86names as N
from isre.x86dis import (BODY, DATA, F_CALL, F_CALLF, F_COND, F_INT, F_JMP, F_JMPF, F_JMPI,
                         HEAD, UNKNOWN, CodeSeg, DataRegion, Disassembler, Insn, Site)
from isre.util import write_if_changed

DEFAULT_MODULES = ["inspace", "ispace", "factory-1998a"]

SEED_ORDER = ["cs_ip", "export", "selfload", "far_call", "far_jmp", "near_call", "far_ptr",
              "prologue", "gap"]

# Borland C++ Win16 function prologues [BYTES INSPACE.EXE seg5:0x364]. The
# first three are far functions with DS setup (smart callbacks: mov ax,ss;
# exported: mov ax,ds; push ds/pop ax), then far without DS setup, then the
# plain frame used by near functions.
PROLOGUES_STRONG = [bytes.fromhex("8cd09045558bec"), bytes.fromhex("8cd89045558bec"),
                    bytes.fromhex("1e589045558bec")]
PROLOGUE_FAR = bytes.fromhex("45558bec")
PROLOGUE_NEAR = bytes.fromhex("558bec")
PADDING = frozenset({0x00, 0x90, 0xCC})

PRINTABLE = frozenset(range(0x20, 0x7F)) | {0x09, 0x0A, 0x0D}

# Listing columns: address, instruction bytes, text, comments. The bytes column
# holds 9 bytes (the longest instruction in the three modules); a longer one
# still keeps one space before the text, and a long text one before the ';'.
ADDR_W = 14
HEX_W = 27
COMMENT_COL = 73


def addr(seg: int, off: int) -> str:
    return f"seg{seg}:{off:#x}"


# ------------------------------------------------------------------ module image

@dataclass
class SegInfo:
    n: int
    data: bytes
    length: int
    flags: int
    minalloc: int
    packed: bool
    file_off: int
    file_len: int

    @property
    def is_code(self) -> bool:
        return not (self.flags & 1)


@dataclass
class ModuleImage:
    name: str
    file: str
    module_name: str
    segs: dict[int, SegInfo]
    entries: list[tuple[int, int, int, str]]          # ordinal, seg, offset, name
    cs_ip: tuple[int, int] | None
    autodata: int
    records: dict[int, list[dict[str, Any]]]          # OPTLOADER records per segment
    std_relocs: dict[int, list[dict[str, Any]]] = field(default_factory=dict)
    selfload: bool = False

    @property
    def dgroup(self) -> SegInfo | None:
        return self.segs.get(self.autodata)


def _entry_names(names: list[dict[str, Any]]) -> dict[int, str]:
    return {n["ordinal"]: n["name"] for n in names if n["ordinal"]}


def load_work(work: Path, name: str) -> ModuleImage:
    """Read ``work/ne/<name>.json`` and ``work/unpacked/<name>/``."""
    work = Path(work)
    ne = json.loads((work / "ne" / f"{name}.json").read_text())
    upd = work / "unpacked" / name
    manifest = json.loads((upd / "unpack.json").read_text())
    rel = json.loads((upd / "relocs.json").read_text())
    man = {s["seg"]: s for s in manifest["segments"]}
    segs = {}
    for s in ne["segments"]:
        n = s["n"]
        data = (upd / f"seg{n:02d}.bin").read_bytes()
        m = man[n]
        length = m["decomp_len"] if m["packed"] else (s["file_len"] if s["sector"] else 0)
        segs[n] = SegInfo(n, data, length, int(s["flags"], 16), s["minalloc"], m["packed"],
                          s["file_off"], s["file_len"])
    names = _entry_names(ne["resident_names"] + ne["nonresident_names"])
    entries = [(e["ordinal"], e["seg"], e["offset"], names.get(e["ordinal"], ""))
               for e in ne["entries"] if not e["constant"]]
    h = ne["header"]
    records: dict[int, list] = defaultdict(list)
    for r in rel["relocs"]:
        records[r["seg"]].append(r)
    std = {int(k): v for k, v in ne["segment_relocations"].items()}
    return ModuleImage(name, ne["file"], ne["module_name"], segs, entries,
                       (h["cs"], h["ip"]) if h["cs"] else None, h["autodata"], dict(records),
                       std, bool(ne["optloader"].get("packed")))


def load_ne(mod, up, name: str | None = None) -> ModuleImage:
    """Build the image from an ``NEModule`` and its ``optloader.unpack`` result."""
    segs = {}
    for s in up.segments:
        seg = mod.segment(s.n)
        length = s.decomp_len if s.packed else (s.file_len if seg.has_file_data else 0)
        segs[s.n] = SegInfo(s.n, s.image, length, s.flags, s.minalloc, s.packed,
                            s.file_off, s.file_len)
    names = _entry_names(mod.resident_names + mod.nonresident_names)
    entries = [(e.ordinal, e.seg, e.offset, names.get(e.ordinal, ""))
               for e in mod.entries if e.seg != 0xFE]
    h = mod.header
    records = {s.n: list(s.records) for s in up.segments if s.records}
    std = {n: [r.to_json() for r in recs] for n, recs in mod.std_relocs.items()}
    return ModuleImage(name or Path(mod.name).stem.lower(), mod.name, mod.module_name, segs,
                       entries, (h["cs"], h["ip"]) if h["cs"] else None, h["autodata"],
                       records, std, bool(mod.optloader()["packed"]))


# ------------------------------------------------------------------ relocation sites

def _walk_chain(data: bytes, head: int, width: int, single: bool) -> list[int]:
    out = []
    pos = head
    seen = set()
    while 0 <= pos <= len(data) - width and pos not in seen:
        seen.add(pos)
        out.append(pos)
        if single:
            break
        (link,) = struct.unpack_from("<H", data, pos)
        if link == 0xFFFF:
            break
        pos = link
    return out


def _import_label(specs: dict[str, dict[int, str]], module: str, ordinal: int) -> str:
    name = specs.get(module, {}).get(ordinal)
    return f"{module}.{ordinal} {name}" if name else f"{module}.{ordinal}"


def build_sites(img: ModuleImage, specs: dict[str, dict[int, str]]) -> dict[int, dict[int, Site]]:
    """Every relocation site of every segment, chains walked."""
    entry_at = {o: (s, off) for o, s, off, _ in img.entries}
    out: dict[int, dict[int, Site]] = defaultdict(dict)
    for n, recs in sorted(img.records.items()):
        data = img.segs[n].data
        for r in recs:
            tt = r["target_type"]
            width = 4 if tt.startswith("PTR16") else (1 if tt.startswith("BYTE") else 2)
            single = tt.endswith("_A") or tt == "BYTE" or r["kind"] == "osfloat"
            kind, label = "osfloat", "osfloat"
            module = ordinal = ref_seg = target = None
            if r["kind"] in ("import_ordinal", "import_name"):
                kind = "import"
                module = r.get("module")
                if r["kind"] == "import_ordinal":
                    ordinal = r["ordinal"]
                    label = _import_label(specs, module, ordinal)
                else:
                    label = f"{module}.{r['name']}"
            elif r["kind"] == "special_base":
                kind = "selector"
                ref_seg = r["selector_idx"]
                label = f"seg{ref_seg}"
            elif r["kind"] == "internal":
                raw = r["ref_seg_raw"]
                if raw == 0xFF:
                    target = entry_at.get(r["value"])
                    ref_seg = target[0] if target else None
                else:
                    ref_seg = raw
                    target = (raw, r["value"])
                if tt.startswith("SEG16"):
                    kind, target, label = "selector", None, f"seg{ref_seg}"
                else:
                    kind = "internal"
                    label = addr(*target) if target else f"entry {r['value']}"
            for so in _walk_chain(data, r["offset"], width, single):
                out[n][so] = Site(n, so, width, kind, tt, r["offset"], label, module, ordinal,
                                  ref_seg, target)
    src_width = {"FAR_ADDR": 4, "SEGMENT": 2, "OFFSET": 2, "LOBYTE": 1}
    for n, recs in sorted(img.std_relocs.items()):
        data = img.segs[n].data
        for r in recs:
            width = src_width.get(r["src_type"], 2)
            single = r["additive"] or r["src_type"] == "LOBYTE"
            module = ordinal = ref_seg = target = None
            if r["target_type"] in ("IMPORTORDINAL", "IMPORTNAME"):
                kind = "import"
                module = r["module"]
                if "ordinal" in r:
                    ordinal = r["ordinal"]
                    label = _import_label(specs, module, ordinal)
                else:
                    label = f"{module}.{r['name']}"
            elif r["target_type"] == "INTERNALREF":
                ref_seg = r["target_seg"] if isinstance(r["target_seg"], int) else None
                if r["src_type"] == "SEGMENT":
                    kind, label = "selector", f"seg{ref_seg}"
                else:
                    kind = "internal"
                    target = (ref_seg, r.get("target_offset", 0)) if ref_seg else None
                    label = addr(*target) if target else "movable entry"
            else:
                kind, label = "osfloat", "osfixup"
            tt = {"FAR_ADDR": "PTR16", "SEGMENT": "SEG16", "OFFSET": "OFF16",
                  "LOBYTE": "BYTE"}.get(r["src_type"], r["src_type"]) + \
                ("_A" if r["additive"] else "")
            for so in _walk_chain(data, r["offset"], width, single):
                out[n][so] = Site(n, so, width, kind, tt, r["offset"], label, module, ordinal,
                                  ref_seg, target)
    return dict(out)


# ------------------------------------------------------------------ fallback finders

def is_padding(chunk: bytes) -> bool:
    """Alignment fill between functions: only NOP, zero or INT 3 bytes."""
    return all(c in PADDING for c in chunk)


def _skip_padding(data: bytes, a: int, b: int) -> int:
    while a < b and data[a] in PADDING:
        a += 1
    return a


def find_prologues(seg: int, data: bytes, a: int, b: int) -> list[int]:
    chunk = data[a:b]
    hits: set[int] = set()
    covered: set[int] = set()
    for sig in PROLOGUES_STRONG:
        i = chunk.find(sig)
        while i >= 0:
            hits.add(a + i)
            covered.update(range(a + i, a + i + len(sig)))
            i = chunk.find(sig, i + 1)
    for sig in (PROLOGUE_FAR, PROLOGUE_NEAR):
        i = chunk.find(sig)
        while i >= 0:
            if a + i not in covered:
                hits.add(a + i)
                covered.update(range(a + i, a + i + len(sig)))
            i = chunk.find(sig, i + 1)
    return sorted(hits)


def make_gap_finder(dis: Disassembler):
    def find_gap_starts(seg: int, data: bytes, a: int, b: int) -> list[int]:
        st = dis.state[seg]
        if a > 0 and st[a - 1] in (HEAD, BODY):
            o = a - 1
            while st[o] != HEAD:
                o -= 1
            prev = dis.insns[seg][o]
            if prev.flow not in (F_JMP, F_JMPI, F_JMPF) and prev.mnem not in ("ret", "retf") \
                    and not prev.noreturn:
                return []
        p = _skip_padding(data, a, b)
        return [p] if p < b else []
    return find_gap_starts


# ------------------------------------------------------------------ analysis

@dataclass
class AsmResult:
    img: ModuleImage
    dis: Disassembler
    sites: dict[int, dict[int, Site]]
    specs: dict[str, dict[int, str]]
    bodies: dict[tuple[int, int], tuple[list[int], list[int]]]
    owners: dict[tuple[int, int], list[int]]
    exports: dict[tuple[int, int], list[str]]                # "ordinal [name]"
    # gap-decoded blocks that lie inside another function's extent: dead code
    # (an unreachable loop the compiler kept, or the tail after a call that
    # does not return), not functions. (seg, off) -> (parent entry, offsets)
    unreachable: dict[tuple[int, int], tuple[int, list[int]]] = field(default_factory=dict)
    stub_seg: int | None = None


def analyse(img: ModuleImage, specs: dict[str, dict[int, str]], fallback: bool = True,
            gap: bool = True) -> AsmResult:
    sites = build_sites(img, specs)
    # A packed segment decodes up to minalloc: the loader zero-fills past the
    # decompressed bytes, and the compressor drops trailing zero bytes (FACTORY
    # seg4 ends in "retf 0xa" whose last byte lies in that fill).
    code = {n: CodeSeg(n, s.data, len(s.data) if s.packed else s.length, s.flags,
                       sites.get(n, {}))
            for n, s in img.segs.items() if s.is_code and s.length}
    dis = Disassembler(
        code,
        is_dos_call=lambda st: (st.module, st.ordinal) == ("KERNEL", 102),
        import_noreturn=lambda st: (st.module, st.ordinal) in N.IMPORT_NORETURN,
    )
    if img.selfload and 1 in code and code[1].data[:2] == b"A0":
        _seed_selfload(dis, code[1])
    if img.cs_ip:
        dis.add_seed(img.cs_ip[0], img.cs_ip[1], "cs_ip", 0)
    exports: dict[tuple[int, int], list[str]] = defaultdict(list)
    for ordinal, seg, off, name in img.entries:
        exports[(seg, off)].append(f"{ordinal} {name}" if name else str(ordinal))
        dis.add_seed(seg, off, "export", 0)
    for n, cs in sorted(code.items()):
        for so, site in sorted(cs.sites.items()):
            if so >= 3 and site.kind == "selector" and cs.data[so - 3] in (0x9A, 0xEA):
                (toff,) = struct.unpack_from("<H", cs.data, so - 2)
                dis.add_seed(site.ref_seg, toff,
                             "far_call" if cs.data[so - 3] == 0x9A else "far_jmp", 1)
            elif (so >= 1 and site.kind == "internal" and site.width == 4 and site.target
                  and cs.data[so - 1] in (0x9A, 0xEA)):
                dis.add_seed(site.target[0], site.target[1],
                             "far_call" if cs.data[so - 1] == 0x9A else "far_jmp", 1)
    finders = []
    if fallback:
        finders.append((find_prologues, "prologue", 3))
        if gap:
            finders.append((make_gap_finder(dis), "gap", 4))
    dis.run(finders)
    bodies = dis.bodies()
    parents = _dead_gap_blocks(dis, bodies)
    for key in parents:
        del dis.functions[key]
    if parents:
        bodies = dis.bodies()
    owners: dict[tuple[int, int], list[int]] = defaultdict(list)
    for (seg, e), (body, _t) in bodies.items():
        for o in body:
            owners[(seg, o)].append(e)
    unreachable = {}
    for (seg, e), parent in sorted(parents.items()):
        block, stack = set(), [e]
        while stack:
            o = stack.pop()
            insn = dis.insns[seg].get(o)
            if o in block or insn is None or (seg, o) in owners:
                continue
            block.add(o)
            stack.extend(t for t, _k in dis.successors(insn))
        unreachable[(seg, e)] = (parent, sorted(block))
    stub = 1 if img.selfload and 1 in code and code[1].data[:2] == b"A0" else None
    return AsmResult(img, dis, sites, specs, bodies, dict(owners), dict(exports), unreachable,
                     stub)


def _dead_gap_blocks(dis: Disassembler, bodies) -> dict[tuple[int, int], int]:
    """Gap-seeded entries that are dead code of the function around them.

    The instruction just before the entry belongs to function G and ends
    exactly at the entry (no padding between), and the block decoded from
    the entry runs back into G's body: an unreachable loop the compiler kept,
    or the tail after a call that does not return.
    """
    owner: dict[tuple[int, int], int] = {}
    for (seg, g), (body, _t) in bodies.items():
        if dis.functions[(seg, g)] == {"gap"}:
            continue
        for o in body:
            owner.setdefault((seg, o), g)
    out = {}
    for seg, e in sorted(bodies):
        if dis.functions[(seg, e)] != {"gap"} or e == 0:
            continue
        st = dis.state[seg]
        o = e - 1
        while o > 0 and st[o] == BODY:
            o -= 1
        prev = dis.insns[seg].get(o)
        if prev is None or prev.end != e or (seg, o) not in owner:
            continue
        g = owner[(seg, o)]
        if any(owner.get((seg, b)) == g for b in bodies[(seg, e)][0]):
            out[(seg, e)] = g
    return out


def _seed_selfload(dis: Disassembler, cs: CodeSeg) -> None:
    """Self-load table at the stub's start [REF nappload.asm START]: BootApp at +4,
    LoadAppSeg at +8, ExitProc at +0x18; the table and the loader's import
    cells end where the first of those procedures starts."""
    offs = []
    for p in (4, 8, 0x18):
        (o,) = struct.unpack_from("<H", cs.data, p)
        if 0x28 <= o < cs.length:
            offs.append(o)
    end = min(offs) if offs else 0x28
    dis.mark_data(DataRegion(cs.n, 0, end, "header",
                             note="self-load table and OPTLOADER import cells"))
    for o in offs:
        dis.add_seed(cs.n, o, "selfload", 0)


# ------------------------------------------------------------------ annotation

def _signed(v: int, bits: int) -> str:
    return f" ({v - (1 << bits)})" if v >= 1 << (bits - 1) else ""


def additive(cs: CodeSeg, site: Site) -> tuple[str, str]:
    """(label suffix, comment) for an additive relocation, ("", "") otherwise.

    The loader adds the target to the bytes stored at the site (nappload.asm
    RELOC_INT_BYTE_A, RELOC_INT_OFF16_A, RELOC_INT_SEG16_A, RELOC_INT_PTR16_A):
    the target offset to the offset word, and for PTR16_A also the selector
    to the selector word.
    """
    tt = site.target_type
    if not tt.endswith("_A") or site.off + site.width > len(cs.data):
        return "", ""
    if site.width == 1:
        v = cs.data[site.off]
        return f"+{v:#x}", f"additive {tt}: +{v:#x}{_signed(v, 8)}"
    (v,) = struct.unpack_from("<H", cs.data, site.off)
    if tt.startswith("SEG16"):
        return "", f"additive {tt}: selector +{v:#x}"
    note = f"additive {tt}: target offset +{v:#x}{_signed(v, 16)}"
    if site.width == 4:
        (sv,) = struct.unpack_from("<H", cs.data, site.off + 2)
        if sv:
            note += f", selector +{sv:#x}"
    return (f"+{v:#x}" if v else ""), note


class Annotator:
    def __init__(self, res: AsmResult):
        self.res = res
        self.dis = res.dis
        dg = res.img.dgroup
        self.dg_seg = res.img.autodata
        self.dg_data = dg.data if dg else b""
        self.dg_init = dg.length if dg else 0
        self.xref_dgroup: dict[int, list[tuple[str, str]]] = defaultdict(list)
        self.xref_csdata: dict[tuple[int, int], list[str]] = defaultdict(list)
        self.dos: list[dict[str, Any]] = []

    def string_at(self, off: int, need_start: bool) -> str | None:
        d = self.dg_data
        if off >= self.dg_init:
            return None
        if need_start and off > 0 and d[off - 1] != 0:
            return None
        end = d.find(b"\0", off, min(self.dg_init, off + 200))
        if end < 0 or end - off < (2 if need_start else 3):
            return None
        s = d[off:end]
        if not all(c in PRINTABLE for c in s):
            return None
        text = s.decode("ascii").encode("unicode_escape").decode("ascii").replace('"', '\\"')
        return f'"{text[:60]}"' + ("..." if len(text) > 60 else "")

    def render(self, insn: Insn) -> tuple[str, list[str]]:
        """Instruction text and comments."""
        dis = self.dis
        seg = insn.seg
        here = addr(seg, insn.off)
        text = f"{insn.mnem} {insn.ops}".rstrip()
        comments: list[str] = []
        cs = dis.segs[seg]
        if insn.flow in (F_CALLF, F_JMPF) and insn.far_off is not None:
            site = dis.far_site(insn)
            verb = "call far" if insn.flow == F_CALLF else "jmp far"
            if site is None:
                comments.append("far transfer without a relocation")
            elif site.kind == "selector":
                text = f"{verb} {addr(site.ref_seg, insn.far_off)}"
            elif site.kind == "internal" and site.target:
                text = f"{verb} {addr(*site.target)}"
            else:
                text = f"{verb} {site.label}"
            if site is not None:
                suffix, note = additive(cs, site)
                text += suffix
                if note:
                    comments.append(note)
        elif insn.flow in (F_CALL, F_JMP, F_COND):
            text = f"{insn.mnem} {addr(seg, insn.target)}"
        sa = dis.site_at[seg]
        inside = sorted({sa[b] for b in range(insn.off, insn.end) if sa[b] >= 0})
        if not (insn.flow in (F_CALLF, F_JMPF) and insn.far_off is not None):
            for so in inside:
                site = cs.sites[so]
                what = "selector of" if site.kind == "selector" else "reloc"
                suffix, note = additive(cs, site)
                comments.append(f"{what} {site.label}{suffix}")
                if note:
                    comments.append(note)
        if insn.noreturn:
            comments.append("does not return")
        if dis.dos_call_site(insn):
            self._dos_comment(insn, comments)
        if insn.flow == F_JMPI:
            sw = dis.switches.get((seg, insn.off))
            if sw:
                comments.append(f"switch: {sw.count} entries at {addr(seg, sw.table)}")
            elif (seg, insn.off) in dis.unresolved:
                comments.append(f"indirect jump not resolved ({dis.unresolved[(seg, insn.off)]})")
        has_site = bool(inside)
        m = insn.mem
        if seg == self.res.stub_seg:
            m = None                       # the OPTLOADER stub runs with its own DS
            has_site = True
        if m is not None:
            ds_default = m.segreg in ("", "ds") and m.base not in ("bp", "ebp")
            if m.segreg == "ds" or (m.segreg == "" and ds_default):
                strop = insn.mnem.startswith(("movs", "lods", "stos", "cmps", "scas"))
                if m.absolute and not strop:
                    s = self.string_at(m.disp, False)
                    comments.append(f"DGROUP:{m.disp:#x}" + (f" {s}" if s else ""))
                    self.xref_dgroup[m.disp].append((here, "lea" if insn.mnem == "lea" else "mem"))
                elif m.disp >= 0x100 and (m.base or m.index):
                    regs = "+".join(r for r in (m.base, m.index) if r)
                    comments.append(f"DGROUP:{m.disp:#x}+{regs}")
                    self.xref_dgroup[m.disp].append((here, "indexed"))
            elif m.segreg == "cs" and m.absolute:
                comments.append(f"code data {addr(seg, m.disp)}")
                self.xref_csdata[(seg, m.disp)].append(here)
        if insn.imm and not has_site and insn.mnem in ("mov", "push"):
            v, size = insn.imm[0]
            if size == 2 and (v & 0xFFFF) >= 0x100:      # small values are counts and flags
                s = self.string_at(v & 0xFFFF, True)
                if s:
                    comments.append(f"-> DGROUP:{v & 0xFFFF:#x} {s}")
                    self.xref_dgroup[v & 0xFFFF].append((here, "imm"))
        return text, comments

    def _dos_comment(self, insn: Insn, comments: list[str]) -> None:
        dis = self.dis
        ahs, ah_inc = dis.reg_values(insn.seg, insn.off, "ah")
        als, al_inc = dis.reg_values(insn.seg, insn.off, "al")
        via = "int 21h" if insn.flow == F_INT else "DOS3Call"
        if ahs:
            names = "/".join(n for n in (N.dos_name(a) for a in sorted(ahs)) if n)
            txt = "AH=" + "/".join(f"{a:02X}h" for a in sorted(ahs)) + ("|?" if ah_inc else "")
            if names:
                txt += f" {names}"
        else:
            txt = "AH=?"
        if als and ahs & N.DOS_AL_INPUT:
            txt += " AL=" + "/".join(f"{a:02X}h" for a in sorted(als)) + ("|?" if al_inc else "")
        comments.append(txt)
        self.dos.append({"at": addr(insn.seg, insn.off), "via": via,
                         "ah": sorted(ahs), "ah_complete": not ah_inc,
                         "al": sorted(als), "al_complete": not al_inc,
                         "names": [N.dos_name(a) for a in sorted(ahs)]})


# ------------------------------------------------------------------ outputs

def _hexbytes(data: bytes, off: int, n: int) -> str:
    return " ".join(f"{b:02x}" for b in data[off:off + n])


def _line(a: str, hexs: str, text: str, comments: list[str]) -> str:
    s = f"{a:<{ADDR_W}s}{hexs:<{HEX_W - 1}s} {text}"
    if comments:
        s = f"{s:<{COMMENT_COL - 1}s} ; " + "; ".join(comments)
    return s.rstrip()


def parse_line(line: str) -> tuple[int, int, bytes, str, str] | None:
    """(seg, off, instruction bytes, text, comment) of one listing line, or None.

    The bytes start in column ADDR_W; data rows have none there. Data and
    padding lines parse too (their ``db`` is the text). The comment is
    everything after the first ``" ; "`` separator.
    """
    m = _ADDR_RE.match(line)
    if m is None or len(line) <= ADDR_W or line[m.end():ADDR_W].strip():
        return None
    pos = ADDR_W
    raw = []
    while _HEX2_RE.match(line, pos):
        raw.append(int(line[pos:pos + 2], 16))
        pos += 3
    text, sep, comment = line[pos:].strip().partition(" ; ")
    if not sep and text.startswith("; "):
        text, comment = "", text[2:]
    return int(m.group(1)), int(m.group(2), 16), bytes(raw), text.rstrip(), comment


_ADDR_RE = re.compile(r"seg(\d+):0x([0-9a-f]+)(?= )")
_HEX2_RE = re.compile(r"[0-9a-f]{2}(?: |$)")


def seg_stats(res: AsmResult, seg: int) -> dict[str, Any]:
    dis = res.dis
    st = dis.state[seg]
    n = dis.segs[seg].length
    data = dis.segs[seg].data
    counts = Counter(st[:n])
    undecoded = counts[UNKNOWN]
    padding = sum(b - a for a, b in dis.gaps(seg) if is_padding(data[a:b]))
    funcs = sum(1 for (s, _o) in res.bodies if s == seg)
    return {"seg": seg, "bytes": n, "decoded": counts[HEAD] + counts[BODY], "data": counts[DATA],
            "undecoded": undecoded, "undecoded_padding": padding,
            "instructions": len(dis.insns[seg]), "functions": funcs,
            "coverage": round((counts[HEAD] + counts[BODY]) / n, 4) if n else 0.0}


def relocation_check(res: AsmResult) -> dict[str, Any]:
    """Where every relocation site of every code segment lies (S2c invariant)."""
    dis = res.dis
    out = Counter()
    uncovered = []
    regions = sorted(dis.data.values(), key=lambda r: (r.seg, r.off))
    for seg in sorted(dis.segs):
        cs = dis.segs[seg]
        st = dis.state[seg]
        for so, site in sorted(cs.sites.items()):
            out["sites"] += 1
            state = st[so] if so < len(st) else UNKNOWN
            if state in (HEAD, BODY):
                o = so
                while st[o] != HEAD:
                    o -= 1
                insn = dis.insns[seg][o]
                ok = (o + insn.op_start <= so and so + site.width <= insn.end)
                if ok:
                    out["in_operands"] += 1
                    continue
                out["bad"] += 1
                uncovered.append({"site": site.addr, "label": site.label,
                                  "where": f"opcode bytes of {addr(seg, o)}"})
                continue
            if state == DATA:
                region = next((r for r in regions if r.seg == seg and r.off <= so < r.off + r.size),
                              None)
                out["in_data"] += 1
                uncovered.append({"site": site.addr, "label": site.label,
                                  "where": f"data {region.kind} {addr(seg, region.off)}"
                                  if region else "data"})
                continue
            out["undecoded"] += 1
            uncovered.append({"site": site.addr, "label": site.label, "where": "undecoded"})
    return {**{k: out[k] for k in ("sites", "in_operands", "in_data", "undecoded", "bad")},
            "uncovered": uncovered}


def build_outputs(res: AsmResult) -> tuple[dict[str, str], dict[str, Any], dict[str, Any]]:
    """(listings by file name, functions.json content, xrefs.json content)."""
    img = res.img
    dis = res.dis
    ann = Annotator(res)
    callers: dict[tuple[int, int], list[tuple[str, str]]] = defaultdict(list)
    for c in dis.calls:
        if c.target is not None:
            callers[c.target].append((addr(c.seg, c.off), c.kind))
    jump_srcs: dict[tuple[int, int], list[str]] = defaultdict(list)
    for seg, preds in dis.preds.items():
        for to, lst in preds.items():
            for frm, kind in lst:
                if kind != "fall":
                    jump_srcs[(seg, to)].append(addr(seg, frm))
    listings = {}
    rendered: dict[tuple[int, int], tuple[str, list[str]]] = {}
    for seg in sorted(dis.segs):
        for off in sorted(dis.insns[seg]):
            rendered[(seg, off)] = ann.render(dis.insns[seg][off])
    for seg in sorted(dis.segs):
        listings[f"seg{seg:02d}.asm"] = _listing(res, seg, rendered, callers, jump_srcs)

    calls_by_func: dict[tuple[int, int], list] = defaultdict(list)
    for c in dis.calls:
        for e in res.owners.get((c.seg, c.off), []):
            calls_by_func[(c.seg, e)].append(c)
    funcs = []
    for (seg, e), (body, tails) in sorted(res.bodies.items()):
        insns = dis.insns[seg]
        rets = sorted({insns[o].mnem for o in body if insns[o].mnem in ("ret", "retf", "iret")})
        calls_out, imports = set(), set()
        for c in calls_by_func.get((seg, e), []):
            if c.target is not None:
                calls_out.add((c.target, c.kind))
            elif c.site is not None:
                imports.add(c.site.label)
        end = max(insns[o].end for o in body) if body else e
        funcs.append({
            "addr": addr(seg, e), "seg": seg, "off": e,
            "seeds": sorted(dis.functions[(seg, e)], key=SEED_ORDER.index),
            "export": res.exports.get((seg, e), []),
            "insns": len(body), "bytes": sum(insns[o].size for o in body),
            "end": addr(seg, end), "returns": rets,
            "calls": [[addr(*t), k] for t, k in sorted(calls_out)],
            "imports": sorted(imports),
            "callers": [[f, k] for f, k in sorted(set(callers.get((seg, e), [])), key=_ref_key)],
            "tail_jumps": [addr(seg, t) for t in tails],
            "ranges": _ranges(seg, insns, body),
        })
    seed_counts = Counter(s for f in funcs for s in f["seeds"][:1])
    seg_list = [seg_stats(res, seg) for seg in sorted(dis.segs)]
    undecoded = []
    for seg in sorted(dis.segs):
        cs = dis.segs[seg]
        for a, b in dis.gaps(seg):
            sites_in = [cs.sites[o].addr for o in sorted(cs.sites) if a <= o < b]
            undecoded.append({"addr": addr(seg, a), "size": b - a,
                              "padding": is_padding(cs.data[a:b]), "sites": sites_in})
    functions_json = {
        "module": img.name, "file": img.file, "module_name": img.module_name,
        "tool": "isre asm", "dgroup": img.autodata,
        "format": {"functions.calls": "[[target, kind]]", "functions.callers": "[[from, kind]]",
                   "kind": "near | far | jmp_far | far_ptr | import",
                   "ranges": "[[first, end]]: runs of adjacent body instructions, end exclusive"},
        "dgroup_init_len": img.dgroup.length if img.dgroup else 0,
        "first_seed_counts": {k: seed_counts[k] for k in SEED_ORDER if seed_counts[k]},
        "segments": seg_list,
        "functions": funcs,
        "data": [{"addr": addr(r.seg, r.off), "size": r.size, "kind": r.kind,
                  "ref": addr(*r.ref) if r.ref else None, "note": r.note}
                 for _k, r in sorted(dis.data.items())],
        "switches": [{"at": addr(s.seg, s.off), "table": addr(s.seg, s.table), "count": s.count,
                      "bound_at": addr(s.seg, s.cmp_off),
                      "targets": [addr(s.seg, t) for t in s.targets]}
                     for _k, s in sorted(dis.switches.items())],
        "unresolved_indirect": [{"at": addr(*k), "insn": rendered[k][0], "reason": v}
                                for k, v in sorted(dis.unresolved.items())],
        "unreachable_blocks": [{"addr": addr(seg, e), "in_function": addr(seg, parent),
                                "insns": len(block),
                                "bytes": sum(dis.insns[seg][o].size for o in block),
                                "ranges": _ranges(seg, dis.insns[seg], block)}
                               for (seg, e), (parent, block) in sorted(res.unreachable.items())],
        "undecoded": undecoded,
        "rejected_seeds": [{"addr": addr(r.seg, r.off), "seed": r.seed, "reason": r.reason}
                           for r in sorted(dis.rejected, key=lambda r: (r.seg, r.off, r.seed))],
        "conflicts": [{"at": addr(s, o), "reason": e} for s, o, e in sorted(set(dis.errors))],
        "relocation_check": relocation_check(res),
    }
    imports_x: dict[str, list[str]] = defaultdict(list)
    calls_x: dict[str, list[list[str]]] = defaultdict(list)
    for c in sorted(dis.calls, key=lambda c: (c.seg, c.off, c.kind)):
        if c.target is not None:
            calls_x[addr(*c.target)].append([addr(c.seg, c.off), c.kind])
        elif c.site is not None:
            imports_x[c.site.label].append(addr(c.seg, c.off))
    xrefs_json = {
        "module": img.name,
        "format": {"calls": "target: [[from, kind]]; kind near|far|jmp_far|far_ptr",
                   "imports": "import: [call sites]",
                   "dgroup": "DGROUP:off: [[from, how]]; how mem|lea|indexed|imm",
                   "code_data": "cs:[off] target: [readers]"},
        "calls": {k: calls_x[k] for k in sorted(calls_x, key=_addr_key)},
        "imports": {k: imports_x[k] for k in sorted(imports_x)},
        "dgroup": {f"DGROUP:{k:#x}": [[f, h] for f, h in sorted(set(v), key=_ref_key)]
                   for k, v in sorted(ann.xref_dgroup.items())},
        "code_data": {addr(*k): sorted(set(v)) for k, v in sorted(ann.xref_csdata.items())},
        "dos": sorted(ann.dos, key=lambda d: _addr_key(d["at"])),
    }
    return listings, functions_json, xrefs_json


def _ranges(seg: int, insns: dict[int, Insn], offs) -> list[list[str]]:
    """Runs of adjacent instructions as [[first, end]], end exclusive."""
    runs: list[list[int]] = []
    for o in sorted(offs):
        if runs and runs[-1][1] == o:
            runs[-1][1] = insns[o].end
        else:
            runs.append([o, insns[o].end])
    return [[addr(seg, a), addr(seg, b)] for a, b in runs]


def _ref_key(t: tuple[str, str]) -> tuple[int, int, str]:
    return (*_addr_key(t[0]), t[1])


def _addr_key(a: str) -> tuple[int, int]:
    s, o = a[3:].split(":")
    return int(s), int(o, 16)


def _seg_header(res: AsmResult, seg: int) -> list[str]:
    img = res.img
    si = img.segs[seg]
    st = seg_stats(res, seg)
    flags = si.flags
    names = [nm for bit, nm in ((0x8, "ITERATED"), (0x10, "MOVABLE"), (0x20, "SHARED"),
                                (0x40, "PRELOAD"), (0x100, "RELOCINFO"), (0x1000, "DISCARDABLE"))
             if flags & bit]
    storage = (f"OPTLOADER-packed, {si.file_len} bytes on disk at file:{si.file_off:#x}, "
               f"{si.length} bytes decompressed" if si.packed
               else f"stored raw at file:{si.file_off:#x}")
    n = res.dis.segs[seg].length
    out = [
        f"; {img.file} ({img.module_name}) seg{seg}  CODE  flags {flags:#06x} {' '.join(names)}",
        f"; {addr(seg, 0)}..{addr(seg, max(n - 1, 0))}: {n} bytes ({storage}; minalloc "
        f"{si.minalloc})",
        f"; decoded {st['decoded']} bytes in {st['instructions']} instructions, "
        f"{st['functions']} functions; {st['data']} data bytes; {st['undecoded']} undecoded "
        f"({st['undecoded_padding']} alignment padding)",
    ]
    if seg == res.stub_seg:
        out.append("; OPTLOADER stub: it decompresses and patches part of itself at run time "
                   "(nappload.asm BOOTAPP, COMPRESS_SKIP), so most of it is not decodable here; "
                   "no DGROUP annotations")
    else:
        out.append(f"; DGROUP is seg{img.autodata}; DGROUP annotations assume DS = DGROUP")
    out += ["; generated by isre asm", ""]
    return out


def _listing(res: AsmResult, seg: int, rendered, callers, jump_srcs) -> str:
    dis = res.dis
    cs = dis.segs[seg]
    st = dis.state[seg]
    data = cs.data
    lines = _seg_header(res, seg)
    regions = {o: r for (s, o), r in dis.data.items() if s == seg}
    switch_case: dict[int, list[str]] = defaultdict(list)
    for (s, o), sw in dis.switches.items():
        if s == seg:
            for i, t in enumerate(sw.targets):
                switch_case[t].append(f"case {i} of {addr(seg, o)}")
    i = 0
    n = cs.length
    while i < n:
        state = st[i]
        if state == HEAD:
            if (seg, i) in dis.functions:
                lines.extend(_func_header(res, seg, i, callers))
            elif (seg, i) in res.unreachable:
                parent, block = res.unreachable[(seg, i)]
                lines.extend(["", f"; ---- unreachable code {addr(seg, i)} inside function "
                                  f"{addr(seg, parent)} ({len(block)} insns, decoded from a gap)"])
            insn = dis.insns[seg][i]
            text, comments = rendered[(seg, i)]
            srcs = jump_srcs.get((seg, i))
            pre = []
            if srcs:
                shown = ", ".join(sorted(srcs, key=_addr_key)[:4])
                more = f" (+{len(srcs) - 4})" if len(srcs) > 4 else ""
                pre.append(f"<- {shown}{more}")
            pre.extend(switch_case.get(i, []))
            lines.append(_line(addr(seg, i), _hexbytes(data, i, insn.size), text, pre + comments))
            if insn.flow in (F_JMP, F_JMPI, F_JMPF) or insn.mnem in ("ret", "retf", "iret") \
                    or insn.noreturn:
                lines.append("")
            i = insn.end
        elif state == DATA:
            r = regions.get(i)
            if r is not None and r.kind == "jumptable":
                sw = dis.switches[r.ref]
                lines.append(f"; jump table for {addr(*r.ref)}: {r.note}")
                for k, t in enumerate(sw.targets):
                    o = i + 2 * k
                    lines.append(_line(addr(seg, o), _hexbytes(data, o, 2), f"dw {t:#x}",
                                       [f"case {k} -> {addr(seg, t)}"]))
                lines.append("")
                i += r.size
                continue
            j = i
            while j < n and st[j] == DATA and (j == i or j not in regions):
                j += 1
            note = f"; data {r.kind}: {r.note}" if r is not None and r.note else "; data"
            refs = sorted(set(sum((v for (s2, o2), v in _csdata_refs(res).items()
                                   if s2 == seg and i <= o2 < j), [])), key=_addr_key)
            if refs:
                note += f" (read by {', '.join(refs[:4])}{' ...' if len(refs) > 4 else ''})"
            lines.append(note)
            lines.extend(_db_lines(seg, data, i, j, cs.sites))
            lines.append("")
            i = j
        else:
            j = i
            while j < n and st[j] == UNKNOWN:
                j += 1
            chunk = data[i:j]
            if is_padding(chunk) and not any(i <= o < j for o in cs.sites):
                lines.append(_line(addr(seg, i), _hexbytes(data, i, min(j - i, 8)),
                                   f"db {j - i} dup ({chunk[0]:02x}h)" if len(set(chunk)) == 1
                                   else "db " + ",".join(f"{c:02x}h" for c in chunk),
                                   ["alignment padding"]))
                lines.append("")
            else:
                lines.append(f"; undecoded {addr(seg, i)}..{addr(seg, j - 1)} ({j - i} bytes)")
                lines.extend(_db_lines(seg, data, i, j, cs.sites))
                lines.append("")
            i = j
    if len(data) > n:
        lines.append(f"; {addr(seg, n)}..{addr(seg, len(data) - 1)}: zero fill to minalloc")
    return "\n".join(lines) + "\n"


_CSDATA_CACHE: dict[int, dict] = {}


def _csdata_refs(res: AsmResult) -> dict[tuple[int, int], list[str]]:
    key = id(res)
    cached = _CSDATA_CACHE.get(key)
    if cached is None:
        refs: dict[tuple[int, int], list[str]] = defaultdict(list)
        for seg, insns in res.dis.insns.items():
            for off, insn in insns.items():
                m = insn.mem
                if m is not None and m.segreg == "cs" and m.absolute:
                    refs[(seg, m.disp)].append(addr(seg, off))
        _CSDATA_CACHE.clear()
        _CSDATA_CACHE[key] = cached = dict(refs)
    return cached


def _db_lines(seg: int, data: bytes, a: int, b: int, sites: dict[int, Site]) -> list[str]:
    out = []
    for o in range(a, b, 16):
        e = min(o + 16, b)
        chunk = data[o:e]
        notes = [f"{sites[x].addr} reloc {sites[x].label}" for x in range(o, e) if x in sites]
        txt = "".join(chr(c) if 0x20 <= c < 0x7F else "." for c in chunk)
        out.append(_line(addr(seg, o), "", "db " + ",".join(f"{c:02x}h" for c in chunk),
                         [f"|{txt}|"] + notes))
    return out


def _func_header(res: AsmResult, seg: int, off: int, callers) -> list[str]:
    dis = res.dis
    body, tails = res.bodies.get((seg, off), ([], []))
    insns = dis.insns[seg]
    rets = sorted({insns[o].mnem for o in body if insns[o].mnem in ("ret", "retf", "iret")})
    seeds = sorted(dis.functions[(seg, off)], key=SEED_ORDER.index)
    exp = res.exports.get((seg, off))
    head = (f"; ---- function {addr(seg, off)}  [{', '.join(seeds)}]"
            f"{'  export ' + ', '.join(exp) if exp else ''}"
            f"  {len(body)} insns  returns {'/'.join(rets) or 'none'}")
    out = ["", head]
    cl = sorted(set(callers.get((seg, off), [])), key=lambda c: _addr_key(c[0]))
    if cl:
        shown = ", ".join(f"{f} ({k})" for f, k in cl[:6])
        out.append(f"; called from {shown}{' (+%d more)' % (len(cl) - 6) if len(cl) > 6 else ''}")
    if tails:
        out.append(f"; continues into {', '.join(addr(seg, t) for t in tails)}")
    return out


def json_lines(obj: dict[str, Any]) -> bytes:
    """JSON with one line per entry of each top-level list or mapping (grep-friendly)."""
    def one(v: Any) -> str:
        return json.dumps(v, separators=(", ", ": "))

    parts = ["{"]
    items = list(obj.items())
    for i, (k, v) in enumerate(items):
        comma = "," if i < len(items) - 1 else ""
        if isinstance(v, dict) and v:
            sub = list(v.items())
            parts.append(f" {json.dumps(k)}: {{")
            parts.extend(f"  {json.dumps(k2)}: {one(v2)}{',' if j < len(sub) - 1 else ''}"
                         for j, (k2, v2) in enumerate(sub))
            parts.append(f" }}{comma}")
        elif isinstance(v, list) and v:
            parts.append(f" {json.dumps(k)}: [")
            parts.extend(f"  {one(v2)}{',' if j < len(v) - 1 else ''}" for j, v2 in enumerate(v))
            parts.append(f" ]{comma}")
        else:
            parts.append(f" {json.dumps(k)}: {one(v)}{comma}")
    parts.append("}")
    return ("\n".join(parts) + "\n").encode("utf-8")


def compact_index(functions_json: dict[str, Any]) -> dict[str, Any]:
    return {
        "module": functions_json["module"], "file": functions_json["file"],
        "tool": "isre asm",
        "fields": ["addr", "bytes", "seeds"],
        "functions": [[f["addr"], f["bytes"], "+".join(f["seeds"])]
                      for f in functions_json["functions"]],
    }


def write_outputs(res: AsmResult, out_dir: Path) -> tuple[list[Path], dict[str, Any]]:
    listings, fj, xj = build_outputs(res)
    out_dir = Path(out_dir)
    written = []
    files = {name: text.encode("utf-8") for name, text in listings.items()}
    files["functions.json"] = json_lines(fj)
    files["xrefs.json"] = json_lines(xj)
    for name, data in sorted(files.items()):
        if write_if_changed(out_dir / name, data):
            written.append(out_dir / name)
    return written, fj


def summary(fj: dict[str, Any]) -> str:
    rc = fj["relocation_check"]
    lines = [f"{fj['module']} ({fj['file']}): {len(fj['functions'])} functions, "
             f"first seeds {fj['first_seed_counts']}"]
    for s in fj["segments"]:
        lines.append(f"  seg{s['seg']:<3d} {s['bytes']:6d} bytes  decoded {s['decoded']:6d} "
                     f"({100 * s['coverage']:5.1f}%)  data {s['data']:5d}  undecoded "
                     f"{s['undecoded']:5d} ({s['undecoded_padding']} padding)  functions "
                     f"{s['functions']:4d}")
    lines.append(f"  relocation sites {rc['sites']}: in operands {rc['in_operands']}, "
                 f"in data {rc['in_data']}, undecoded {rc['undecoded']}, bad {rc['bad']}")
    lines.append(f"  switches {len(fj['switches'])}, unresolved indirect jumps "
                 f"{len(fj['unresolved_indirect'])}, rejected seeds {len(fj['rejected_seeds'])}, "
                 f"conflicts {len(fj['conflicts'])}")
    return "\n".join(lines)


# ------------------------------------------------------------------ show

class ShowError(ValueError):
    """An address that has no listing to show."""


def _off(a: str) -> int:
    return _addr_key(a)[1]


class FunctionIndex:
    """Address lookups over one module's functions.json (from an earlier run).

    A function's pieces are its body ranges, the ranges of the unreachable
    blocks inside it and the jump tables of its switches. Shared code (a tail
    that two entries reach) belongs to every function that reaches it.
    """

    def __init__(self, fj: dict[str, Any]):
        self.fj = fj
        self.by_addr = {f["addr"]: f for f in fj["functions"]}
        self._pieces: dict[str, list[tuple[int, int, str]]] = defaultdict(list)
        for f in fj["functions"]:
            self._pieces[f["addr"]] += [(_off(a), _off(b), "body") for a, b in f["ranges"]]
        for u in fj["unreachable_blocks"]:
            self._pieces[u["in_function"]] += [(_off(a), _off(b), "unreachable")
                                               for a, b in u["ranges"]]
        self._build()
        for d in fj["data"]:
            if d["kind"] == "jumptable" and d["ref"]:
                seg, off = _addr_key(d["ref"])
                for fa, _kind in self.at(seg, off):
                    a = _off(d["addr"])
                    self._pieces[fa].append((a, a + d["size"], "table"))
        self._build()

    def _build(self) -> None:
        spans: dict[int, list[tuple[int, int, str, str]]] = defaultdict(list)
        for fa, pieces in self._pieces.items():
            seg = self.by_addr[fa]["seg"]
            for a, b, kind in pieces:
                spans[seg].append((a, b, fa, kind))
        self._spans = {seg: sorted(v, key=lambda t: (t[0], t[1], _addr_key(t[2]), t[3]))
                       for seg, v in spans.items()}
        self._starts = {seg: [t[0] for t in v] for seg, v in self._spans.items()}
        self._maxlen = {seg: max(t[1] - t[0] for t in v) for seg, v in self._spans.items()}

    def pieces(self, func_addr: str) -> list[tuple[int, int, str]]:
        """(start, end, kind) of every piece of the function, in address order."""
        return sorted(set(self._pieces.get(func_addr, [])))

    def at(self, seg: int, off: int) -> list[tuple[str, str]]:
        """(function address, piece kind) of every piece holding seg:off."""
        spans = self._spans.get(seg)
        if not spans:
            return []
        hi = bisect_right(self._starts[seg], off)
        lo = bisect_left(self._starts[seg], off - self._maxlen[seg])
        found = {(fa, kind) for a, b, fa, kind in spans[lo:hi] if a <= off < b}
        return sorted(found, key=lambda t: (_addr_key(t[0]), t[1]))

    def overlapping(self, seg: int, a: int, b: int) -> list[str]:
        """Functions with a piece that overlaps [a, b)."""
        spans = self._spans.get(seg, [])
        hi = bisect_left(self._starts.get(seg, []), b)
        lo = bisect_left(self._starts.get(seg, []), a - self._maxlen.get(seg, 0))
        return sorted({fa for x, y, fa, _k in spans[lo:hi] if x < b and a < y}, key=_addr_key)

    def containing(self, seg: int, off: int) -> tuple[dict[str, Any] | None, list[str]]:
        """The function to show for seg:off and the other functions that share it.

        Among functions sharing the address, the one with the nearest entry
        at or below it wins. (None, []) when no piece holds the address
        (alignment padding, code-segment data, undecoded bytes).
        """
        hits = sorted({fa for fa, _k in self.at(seg, off)}, key=_addr_key)
        if not hits:
            return None, []
        below = [fa for fa in hits if _off(fa) <= off]
        best = below[-1] if below else hits[0]
        return self.by_addr[best], [fa for fa in hits if fa != best]


class Listing:
    """One segNN.asm listing with its address lines indexed."""

    def __init__(self, text: str):
        self.lines = text.splitlines()
        self.offs: list[int] = []
        self.idx: list[int] = []
        for i, ln in enumerate(self.lines):
            m = _ADDR_RE.match(ln)
            if m:
                self.offs.append(int(m.group(2), 16))
                self.idx.append(i)

    def span(self, a: int, b: int) -> tuple[int, int] | None:
        """First and last line index of the address lines in [a, b)."""
        i, j = bisect_left(self.offs, a), bisect_left(self.offs, b)
        return (self.idx[i], self.idx[j - 1]) if i < j else None

    def line_of(self, off: int) -> int | None:
        """Index of the last address line at or before ``off``."""
        i = bisect_right(self.offs, off)
        return self.idx[i - 1] if i else None

    def header(self, func_addr: str) -> int | None:
        head = f"; ---- function {func_addr} "
        return next((i for i, ln in enumerate(self.lines) if ln.startswith(head)), None)


_HEADER_TAIL = ("; called from ", "; continues into ")
_BLOCK_HEAD = ("; ---- unreachable code ", "; jump table ")


def function_lines(listing: Listing, index: FunctionIndex, func: dict[str, Any],
                   others: Sequence[str] = ()) -> list[tuple[str, int | None]]:
    """The listing of one function: header, then every piece in address order.

    Returns (line, offset of the address it shows or None). Lines between two
    pieces that belong elsewhere are replaced by one ``; ....`` line naming
    what was skipped.
    """
    lines = listing.lines
    seg = func["seg"]
    out: list[tuple[str, int | None]] = []
    h = listing.header(func["addr"])
    if h is not None:
        out.append((lines[h], None))
        k = h + 1
        while k < len(lines) and lines[k].startswith(_HEADER_TAIL):
            out.append((lines[k], None))
            k += 1
    if others:
        out.append((f"; shares code with function {', '.join(others)}", None))
    prev_line, prev_end = -1, None
    for a, b, _kind in index.pieces(func["addr"]):
        sp = listing.span(a, b)
        if sp is None:
            continue
        first, last = sp
        while first > 0 and lines[first - 1].startswith(_BLOCK_HEAD):
            first -= 1
        if prev_end == a:                  # adjacent: keep the blank and header lines between
            first = prev_line + 1
        first = max(first, prev_line + 1)
        if prev_end is not None and a > prev_end:
            who = [fa for fa in index.overlapping(seg, prev_end, a) if fa != func["addr"]]
            what = (f"function {', '.join(who[:3])}{' ...' if len(who) > 3 else ''}" if who
                    else "padding, data or undecoded bytes")
            out.append((f"; .... {addr(seg, prev_end)}..{addr(seg, a - 1)} skipped: {what}",
                        None))
        for i in range(first, last + 1):
            m = _ADDR_RE.match(lines[i])
            out.append((lines[i], int(m.group(2), 16) if m else None))
        prev_line = last
        prev_end = b if prev_end is None else max(prev_end, b)
    return out


def load_functions(listing_dir: Path) -> dict[str, Any]:
    path = Path(listing_dir) / "functions.json"
    if not path.is_file():
        raise ShowError(f"{path} is missing; run isre asm (or isre all) first")
    fj = json.loads(path.read_text())
    if fj["functions"] and "ranges" not in fj["functions"][0]:
        raise ShowError(f"{path} has no body ranges (an older isre asm wrote it); "
                        "run isre asm (or isre all) again")
    return fj


def load_listing(listing_dir: Path, fj: dict[str, Any], seg: int) -> Listing:
    path = Path(listing_dir) / f"seg{seg:02d}.asm"
    if not path.is_file():
        runs: list[list[int]] = []
        for n in sorted(s["seg"] for s in fj["segments"]):
            if runs and runs[-1][1] == n - 1:
                runs[-1][1] = n
            else:
                runs.append([n, n])
        codes = ", ".join(f"seg{a}" if a == b else f"seg{a}-seg{b}" for a, b in runs)
        what = "DGROUP, a data segment" if seg == fj.get("dgroup") else "not a code segment"
        raise ShowError(f"seg{seg} of {fj['module']} has no listing in {listing_dir} ({what}; "
                        f"code segments: {codes})")
    return Listing(path.read_text())


def show(listing_dir: Path, where: str, context: int = 24) -> str:
    """The listing of the function holding ``where`` (segN:0xOFF), or a window around it.

    Reads the outputs of an earlier run in ``listing_dir`` (one module). The
    function's shared tails, unreachable blocks and jump tables are included
    wherever they sit in the segment.
    """
    from isre.addr import AddrError, parse

    fj = load_functions(listing_dir)
    try:
        a = parse(where, dgroup_seg=fj.get("dgroup") or 1)
    except AddrError as e:
        raise ShowError(str(e)) from None
    listing = load_listing(listing_dir, fj, a.seg)
    index = FunctionIndex(fj)
    func, others = index.containing(a.seg, a.off)
    if func is not None:
        lines = [ln for ln, _o in function_lines(listing, index, func, others)]
        return "\n".join(lines).rstrip() + "\n"
    pos = listing.line_of(a.off) or 0
    lines = listing.lines
    return "\n".join(lines[max(0, pos - context):pos + context]) + "\n"


# ------------------------------------------------------------------ CLI

def main(argv) -> int:
    p = argparse.ArgumentParser(prog="isre asm", description=__doc__.split("\n")[0])
    p.add_argument("--work", help="work directory holding ne/ and unpacked/")
    p.add_argument("--module", action="append", help="module name under unpacked/ (repeatable)")
    p.add_argument("--ne", help="disassemble this OPTLOADER NE file instead of --work")
    p.add_argument("--out", help="output directory (default WORK/asm)")
    p.add_argument("--index-dir", help="also write functions-<module>.json here")
    p.add_argument("--specs", help="directory of Wine .spec files (default re/wine-specs)")
    p.add_argument("--no-fallback", action="store_true", help="no prologue or gap seeds")
    p.add_argument("--no-gap", action="store_true", help="no gap seeds")
    p.add_argument("--show", metavar="ADDR",
                   help="print the listing of the function holding ADDR from an earlier run")
    a = p.parse_args(list(argv))
    if a.show:
        if not (a.out or a.work) or (a.module and len(a.module) > 1):
            p.error("--show needs --work or --out and at most one --module")
        base = Path(a.out) if a.out else Path(a.work) / "asm"
        name = (a.module or ["inspace"])[0]
        d = base / name if (base / name / "functions.json").is_file() else base
        try:
            sys.stdout.write(show(d, a.show))
        except ShowError as e:
            print(f"isre asm: {e}", file=sys.stderr)
            return 2
        return 0
    specs = N.load_specs(Path(a.specs) if a.specs else N.default_spec_dir())
    images: list[tuple[ModuleImage, Path]] = []
    if a.ne:
        from isre import optloader
        from isre.ne import NEModule

        if not a.out:
            p.error("--ne needs --out")
        mod = NEModule.load(a.ne)
        img = load_ne(mod, optloader.unpack(mod))
        images.append((img, Path(a.out)))
    elif a.work:
        out = Path(a.out) if a.out else Path(a.work) / "asm"
        for name in a.module or DEFAULT_MODULES:
            images.append((load_work(Path(a.work), name), out / name))
    else:
        p.error("give --work or --ne")
    for img, out in images:
        res = analyse(img, specs, fallback=not a.no_fallback, gap=not a.no_gap)
        written, fj = write_outputs(res, out)
        print(summary(fj))
        print(f"  {len(written)} files written to {out}")
        if a.index_dir:
            path = Path(a.index_dir) / f"functions-{img.name}.json"
            data = json_lines(compact_index(fj))
            write_if_changed(path, data)
            print(f"  index {path} ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
