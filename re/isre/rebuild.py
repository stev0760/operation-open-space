"""``isre rebuild``: write an OPTLOADER module as a plain NE with standard relocations.

The output keeps the input's segment numbering, header fields and tables, and
replaces each packed segment with its decompressed bytes followed by a
standard per-segment relocation table [REF Microsoft KB Q65122
"Executable-File Header Format", PER SEGMENT DATA]. Windows (or Wine) then
loads every segment itself, so the self-loading flag 0x0800 is cleared.

Layout of the output file::

    0 .. first segment   MZ stub, NE header and every NE table, copied
                         unchanged (they all lie before the first segment
                         in the OPTLOADER modules), with the module flags,
                         segment table and resource table patched
    segments 1..n        each at a (1 << align shift) boundary: file data,
                         then (RELOCINFO) a word count and 8-byte records
    resources            each at a (1 << resource shift) boundary, in
                         resource-table order, bytes copied unchanged
    nonresident names    appended here only if the input kept them after
                         the first segment (none of the game's modules do)

Segment table entries:

* packed segment: file length = decompressed length, minalloc unchanged
  (the loader zero-fills minalloc beyond the file data, as OPTLOADER's
  LOADAPPSEG does), ITERATED 0x0008 cleared, RELOCINFO 0x0100 set when the
  segment has relocation records;
* segment 1 (the OPTLOADER stub): flags, file bytes and its own three
  standard relocation records copied unchanged. Nothing refers to it once
  the module is not self-loading: CS:IP, the entry table and every
  relocation target name other segments.

OPTLOADER record -> standard record (``to_standard``):

    OPTLOADER target   NE source type      OPTLOADER kind      NE target type
    BYTE    / _A       0 LOBYTE            import_ordinal      1 IMPORTORDINAL
    SEG16   / _A       2 SEGMENT           import_name         2 IMPORTNAME
    PTR16   / _A       3 FAR_ADDR          internal            0 INTERNALREF
    OFF16   / _A       5 OFFSET            special_base        0 INTERNALREF,
    (_A sets the ADDITIVE flag 0x04)                           SEGMENT, offset 0

Both formats chain non-additive records through the site words to 0xFFFF
and patch one site for additive records (nappload.asm RELOC_INT_OFF16 /
RELOC_INT_PTR16; Q65122; Wine ``apply_relocations``), so chain heads map
one to one and the site words stay as the linker wrote them.
A special-base record (byte segment number, word chain head) writes the
selector of that segment into every chain site (nappload.asm, PROCESS_RELOCS
label 3$, SEG_AL_SELECTOR_ESBX); it becomes INTERNALREF SEGMENT with the
segment number in the fixed-segment form, see ``to_standard``.
"""
from __future__ import annotations

import argparse
import struct
import sys
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from isre import optloader
from isre.ne import (MODFLAG_SELFLOAD, SEG_ITERATED, SEG_RELOCINFO, NEModule,
                     RELOC_SRC_NAMES, RELOC_TARGET_NAMES)

# OPTLOADER target type (without the _A suffix) -> NE relocation source type.
SRC_TYPE = {"BYTE": 0, "SEG16": 2, "PTR16": 3, "OFF16": 5}
TGT_INTERNALREF = 0
TGT_IMPORTORDINAL = 1
TGT_IMPORTNAME = 2
TGT_OSFIXUP = 3
FLAG_ADDITIVE = 4
SEG_MOVABLE_REF = 0xFF


class RebuildError(ValueError):
    pass


@dataclass(frozen=True)
class StdRecord:
    """One 8-byte NE relocation record."""
    src_type: int
    flags: int
    offset: int
    target1: int
    target2: int

    def pack(self) -> bytes:
        return struct.pack("<BBHHH", self.src_type, self.flags, self.offset,
                           self.target1, self.target2)

    def describe(self) -> str:
        add = "+ADD" if self.flags & FLAG_ADDITIVE else ""
        return (f"{RELOC_SRC_NAMES.get(self.src_type, self.src_type)}/"
                f"{RELOC_TARGET_NAMES[self.flags & 3]}{add}")


def to_standard(rec: dict[str, Any]) -> StdRecord:
    """Map one decoded OPTLOADER record (``optloader.decode_packets``) to NE.

    Special-base and fixed internal targets use INTERNALREF's fixed-segment
    form (segment number, offset) even though every segment of these modules
    is MOVABLE, for which Q65122 names the 0xFF + entry-ordinal form. The
    fixed form is the one these modules already rely on: the module is
    PROTMODE (flags 0x0008), its entry table uses fixed bundles that name
    MOVABLE segments, and the stub's own standard record at seg1:0x1a is
    INTERNALREF SEGMENT to seg1, which is MOVABLE. Wine resolves the fixed
    form through the segment's selector whatever its flags (ne_segment.c,
    NE_RELTYPE_INTERNAL). The 0xFF form would need an entry-table entry in
    each target segment, and most targets have none.
    """
    kind = rec["kind"]
    off = rec["offset"]
    if kind == "special_base":
        return StdRecord(SRC_TYPE["SEG16"], TGT_INTERNALREF, off, rec["selector_idx"], 0)
    tt = rec["target_type"]
    additive = tt.endswith("_A")
    base = tt[:-2] if additive else tt
    if base == "BYTE" and not additive:
        # OPTLOADER patches one byte (RELOC_INT_BYTE, no chain); Wine walks a
        # chain for a non-additive LOBYTE record. No module has one.
        raise RebuildError(f"seg{rec['seg']}:{off:#x}: non-additive BYTE record has no "
                           "equivalent standard record")
    src = SRC_TYPE[base]
    add = FLAG_ADDITIVE if additive else 0
    if kind == "import_ordinal":
        return StdRecord(src, TGT_IMPORTORDINAL | add, off, rec["module_idx"], rec["ordinal"])
    if kind == "import_name":
        return StdRecord(src, TGT_IMPORTNAME | add, off, rec["module_idx"], rec["imptab_off"])
    if kind == "internal":
        # ref_seg_raw 0xFF: value is an entry ordinal (RELOC_SRC_INT_MOVABLE -> GETENTRY)
        return StdRecord(src, TGT_INTERNALREF | add, off, rec["ref_seg_raw"] & 0xFF, rec["value"])
    # osfloat: OPTLOADER stops at these when a coprocessor is present
    # (OSTYPE_ABORT); no module has one, so no OSFIXUP mapping is offered.
    raise RebuildError(f"seg{rec['seg']}:{off:#x}: {kind} records are not supported")


# ------------------------------------------------------------------ layout

@dataclass
class RebuiltSegment:
    n: int
    file_off: int
    file_len: int
    flags: int
    minalloc: int
    records: list[StdRecord] = field(default_factory=list)
    copied_relocs: int = 0      # standard records copied from the input (seg1)


@dataclass
class RebuildResult:
    name: str
    data: bytes
    align_shift: int
    segments: list[RebuiltSegment]
    resources: int
    record_types: dict[str, int]

    def summary(self) -> str:
        lines = [f"{self.name}: {len(self.data)} bytes, {len(self.segments)} segments, "
                 f"align shift {self.align_shift}, {self.resources} resources"]
        for s in self.segments:
            nrec = len(s.records) or s.copied_relocs
            src = "copied" if s.copied_relocs else "converted"
            lines.append(f"  seg{s.n:<3d} file {s.file_off:#08x} len {s.file_len:6d} min "
                         f"{s.minalloc:6d} flags {s.flags:#06x}  relocs {nrec:4d}"
                         f"{' ' + src if nrec else ''}")
        for k, v in sorted(self.record_types.items()):
            lines.append(f"  {v:5d} x {k}")
        return "\n".join(lines)


def _align(pos: int, shift: int) -> int:
    a = 1 << shift
    return (pos + a - 1) & ~(a - 1)


def _resource_entry_positions(mod: NEModule) -> list[int]:
    """File offsets of every 12-byte resource entry, in table order."""
    h = mod.header
    if h["rsrctab"] == h["restab"]:
        return []
    d = mod.data
    p = mod.ne_off + h["rsrctab"] + 2
    out = []
    while True:
        (type_raw,) = struct.unpack_from("<H", d, p)
        if type_raw == 0:
            break
        (count,) = struct.unpack_from("<H", d, p + 2)
        p += 8
        for _ in range(count):
            out.append(p)
            p += 12
    return out


def _std_reloc_bytes(mod: NEModule, n: int) -> bytes:
    """The input's own standard relocation table for segment n (count word included)."""
    s = mod.segment(n)
    if not (s.has_relocs and s.has_file_data):
        return b""
    p = s.file_off + s.file_len
    (count,) = struct.unpack_from("<H", mod.data, p)
    return mod.data[p:p + 2 + 8 * count]


def _layout(mod: NEModule, up: optloader.UnpackResult, shift: int
            ) -> tuple[bytearray, list[RebuiltSegment], dict[int, int], int]:
    """Place segments and resources; return (body, segments, res offsets, nrestab)."""
    h = mod.header
    data_segs = [s for s in mod.segments if s.has_file_data]
    head_end = min([s.file_off for s in data_segs] + [r.file_off for r in mod.resources]
                   + [len(mod.data)])
    tables_end = mod.ne_off + max(h["enttab"] + h["cbenttab"], mod._imptab_end(), 0x40)
    if tables_end > head_end:
        raise RebuildError(f"NE tables end at {tables_end:#x}, after the first segment "
                           f"at {head_end:#x}")
    out = bytearray(mod.data[:head_end])
    segs: list[RebuiltSegment] = []
    for s, u in zip(mod.segments, up.segments):
        if u.packed:
            body = u.image[:u.decomp_len]
            recs = [to_standard(r) for r in u.records]
            relocs = (struct.pack("<H", len(recs)) + b"".join(r.pack() for r in recs)
                      if recs else b"")
            flags = s.flags & ~SEG_ITERATED
            flags = flags | SEG_RELOCINFO if recs else flags & ~SEG_RELOCINFO
            copied = 0
        else:
            body = mod.segment_file_bytes(s.n)
            relocs = _std_reloc_bytes(mod, s.n)
            recs = []
            flags = s.flags
            copied = (len(relocs) - 2) // 8 if relocs else 0
        if len(body) > 0x10000:
            raise RebuildError(f"seg{s.n} holds {len(body)} bytes, more than 64K")
        if not body:
            if relocs:
                raise RebuildError(f"seg{s.n} has relocations but no file data")
            segs.append(RebuiltSegment(s.n, 0, 0, flags, s.minalloc, recs, copied))
            continue
        pos = _align(len(out), shift)
        out.extend(bytes(pos - len(out)))
        out.extend(body)
        out.extend(relocs)
        segs.append(RebuiltSegment(s.n, pos, len(body), flags, s.minalloc, recs, copied))

    rshift = mod.res_align_shift
    res_new: dict[int, int] = {}            # input raw offset -> output raw offset
    for r in mod.resources:
        if r.raw_offset in res_new:
            continue
        pos = _align(len(out), rshift)
        out.extend(bytes(pos - len(out)))
        blob = mod.data[r.file_off:r.file_off + r.length]
        out.extend(blob + bytes(r.length - len(blob)))
        res_new[r.raw_offset] = pos >> rshift
        if res_new[r.raw_offset] > 0xFFFF:
            raise RebuildError(f"resource {r.type_name} {r.id} lands past the reach of "
                               f"resource shift {rshift}")

    nrestab = h["nrestab_file"]
    if nrestab and h["cbnrestab"] and nrestab + h["cbnrestab"] > head_end:
        blob = mod.data[nrestab:nrestab + h["cbnrestab"]]
        nrestab = len(out)
        out.extend(blob)
    return out, segs, res_new, nrestab


def rebuild(mod: NEModule, up: optloader.UnpackResult | None = None) -> RebuildResult:
    """Build the unpacked NE file for an OPTLOADER module."""
    if up is None:
        up = optloader.unpack(mod)
    if not up.ok:
        bad = [s.n for s in up.segments if not s.invariants_ok]
        raise RebuildError(f"{mod.name}: unpack invariants fail in segments {bad}")
    shift = mod.header["align_shift"]
    while True:
        out, segs, res_new, nrestab = _layout(mod, up, shift)
        if all((s.file_off >> shift) <= 0xFFFF for s in segs):
            break
        shift += 1                          # sector numbers overflow: coarser sectors
        if shift > 15:
            raise RebuildError("segments do not fit the 16-bit sector numbers")

    ne = mod.ne_off
    struct.pack_into("<H", out, ne + 0x0C, mod.flags & ~MODFLAG_SELFLOAD)
    struct.pack_into("<I", out, ne + 0x08, 0)           # file CRC: not checked by loaders
    struct.pack_into("<H", out, ne + 0x32, shift)
    struct.pack_into("<I", out, ne + 0x2C, nrestab)
    segtab = ne + mod.header["segtab"]
    for s in segs:
        if s.file_off % (1 << shift):
            raise RebuildError(f"seg{s.n} is not aligned")
        struct.pack_into("<HHHH", out, segtab + (s.n - 1) * 8, s.file_off >> shift,
                         s.file_len & 0xFFFF, s.flags, s.minalloc & 0xFFFF)
    for pos in _resource_entry_positions(mod):
        (raw,) = struct.unpack_from("<H", out, pos)
        struct.pack_into("<H", out, pos, res_new[raw])

    types = Counter(r.describe() for s in segs for r in s.records)
    return RebuildResult(mod.name, bytes(out), shift, segs, len(mod.resources), dict(types))


def rebuild_file(src: Path, dst: Path | None = None) -> tuple[RebuildResult, NEModule,
                                                              optloader.UnpackResult]:
    mod = NEModule.load(src)
    up = optloader.unpack(mod)
    res = rebuild(mod, up)
    if dst is not None:
        Path(dst).parent.mkdir(parents=True, exist_ok=True)
        Path(dst).write_bytes(res.data)
    return res, mod, up


# ------------------------------------------------------------------ CLI

def main(argv: list[str]) -> int:
    p = argparse.ArgumentParser(
        prog="isre rebuild",
        description="Write an OPTLOADER-packed NE module as an unpacked NE with standard "
                    "relocation records, then check it with the symbolic loader.")
    p.add_argument("input", help="OPTLOADER NE file (SZDD input is expanded first)")
    p.add_argument("output", help="output NE file")
    p.add_argument("--no-check", action="store_true",
                   help="skip the loader-equivalence check (isre.neload)")
    a = p.parse_args(argv)
    try:
        res, mod, up = rebuild_file(Path(a.input), Path(a.output))
    except (OSError, ValueError) as e:
        print(f"isre rebuild: {e}", file=sys.stderr)
        return 2
    print(res.summary())
    print(f"  wrote {a.output}")
    if a.no_check:
        return 0
    from isre import neload

    report = neload.check(NEModule(res.data, Path(a.output).name), mod, up)
    print(report.summary())
    return 0 if report.ok else 1
