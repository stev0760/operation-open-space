"""Unpacker for NE modules packed by SLR Systems OPTLOADER (1993).

Reference: ``re/optlink-src/nappload.asm`` (the loader's source), procedures
DECOMPRESS, DPMI_UNQUIK and PROCESS_RELOCS. Decompressor parameters are
settled by the golden hashes in ``re/artifacts/golden/inspace.sha256``.

Packed segment layout on disk (``fo`` = segment file offset, ``len`` = its
file length from the segment table)::

    fo+0      word   relocation item count (total over all packets)
    fo+2      ...    compressed bitstream (DPMI_UNQUIK); bit 1 = literal byte
    ...       ...    relocation packets, ending exactly at fo+len

Relocation packets (PROCESS_RELOCS)::

    word w: count = w >> 8, al = w & 0xFF
    al == 0xF0          "special base": count x {byte segment, word offset}
    else target = al & 7    (BYTE SEG16 PTR16 OFF16 BYTE_A SEG16_A PTR16_A OFF16_A)
         source = (al >> 3) & 3
         0 INTERNAL: byte seg (0xFF = movable), count x {word offset, word value}
         1 ORDINAL:  word module, count x {word offset, word ordinal}
         2 NAME:     word module, count x {word offset, word imported-names offset}
         3 OSFLOAT:  word type,   count x {word offset}

The non-additive targets walk a chain through the patched words, ending at
0xFFFF; the decoded records are chain heads, so the unpacked images still
hold the chain links.

The unpacked image of a packed segment is the decompressed bytes zero-padded
to the segment's minalloc; segments that are not packed (the loader stub in
segment 1) are copied raw at their file length.
"""
from __future__ import annotations

import struct
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from isre.ne import NEModule
from isre.util import json_bytes, sha256, write_if_changed

TARG_NAMES = ["BYTE", "SEG16", "PTR16", "OFF16", "BYTE_A", "SEG16_A", "PTR16_A", "OFF16_A"]
SRC_NAMES = ["INTERNAL", "ORDINAL", "NAME", "OSFLOAT"]


class OptLoaderError(ValueError):
    pass


# ------------------------------------------------------------- DPMI_UNQUIK

@dataclass
class UnquikResult:
    data: bytes
    consumed: int          # bytes of the source used, end-of-stream marker included
    terminated: bool       # reached the 0x82..0xFF end-of-stream code
    overread: int          # bytes read past the end of the source (read as 0)
    bad_backrefs: int      # match bytes copied from before the output start (as 0)


def unquik(src: bytes, max_out: int = 0x10000) -> UnquikResult:
    """Port of OPTLOADER's DPMI_UNQUIK.

    ``src`` starts at the first bitstream word (segment file offset + 2).
    Bits come from 16-bit words, most significant bit first, interleaved in
    the same byte stream as the literal and offset bytes. The carry of the
    16th shift of a word is the bit used; the next word is loaded right after.
    """
    n = len(src)
    out = bytearray()
    st = {"si": 0, "bp": 0, "dx": 0, "over": 0}

    def load_word() -> None:
        si = st["si"]
        if si + 1 < n:
            st["bp"] = src[si] | (src[si + 1] << 8)
        else:
            st["bp"] = src[si] if si < n else 0
            st["over"] += min(2, si + 2 - n)
        st["si"] = si + 2
        st["dx"] = 16

    def bit() -> int:
        bp = st["bp"]
        carry = (bp >> 15) & 1
        st["bp"] = (bp << 1) & 0xFFFF
        st["dx"] -= 1
        if st["dx"] == 0:
            load_word()
        return carry

    def byte() -> int:
        si = st["si"]
        st["si"] = si + 1
        if si < n:
            return src[si]
        st["over"] += 1
        return 0

    bad = 0

    def copy(bh: int, bl: int, length: int) -> None:
        # 27$: SI = DI + NOT(BH:BL), i.e. DI - (BH:BL) - 1; REP MOVSB (overlapping)
        nonlocal bad
        pos = (((~((bh << 8) | bl)) & 0xFFFF) + len(out)) & 0xFFFF
        for _ in range(length):
            if pos < len(out):
                out.append(out[pos])
            else:
                out.append(0)
                bad += 1
            pos = (pos + 1) & 0xFFFF

    def bits(count: int) -> int:
        v = 0
        for _ in range(count):
            v = (v << 1) | bit()
        return v

    def offset_high() -> int:
        # 5$: high byte of the match offset, a prefix code.
        if not bit():                               # 0
            if not bit():                           # 00 -> 0
                return 0
            if not bit():                           # 010 -> 1
                return 1
            return (bits(1) + 2) & 0xFF             # 011x -> 2..3
        if not bit():                               # 10
            if not bit():
                return (bits(2) + 4) & 0xFF         # 100xx -> 4..7
            return (bits(3) + 8) & 0xFF             # 101xxx -> 8..15
        if not bit():                               # 110xxxx -> 0x10..0x1F
            return (bits(4) + 0x10) & 0xFF
        if not bit():                               # 1110xxxx -> 0x20..0x2F
            return (bits(4) + 0x20) & 0xFF
        if not bit():                               # 11110xxxx -> 0x30..0x3F
            return (bits(4) + 0x30) & 0xFF
        return (bits(6) + 0x40) & 0xFF              # 11111xxxxxx -> 0x40..0x7F

    load_word()
    terminated = False
    while len(out) < max_out:
        if bit():                                   # 1: literal byte
            out.append(byte())
            continue
        if not bit():                               # 00x: length 2 or 3
            length = 2 + bit()
            if length == 2:                         # length 2: one offset byte, high = 0
                copy(0, byte(), 2)
                continue
        else:                                       # 01...
            if not bit():                           # 010x: 4..5
                length = 4 + bit()
            elif not bit():                         # 0110x: 6..7
                length = 6 + bit()
            elif not bit():                         # 01110xx: 8..11
                length = 8 + bits(2)
            elif not bit():                         # 011110xxx: 12..19
                length = 12 + bits(3)
            else:                                   # 011111 + byte
                cl = byte()
                if cl < 0x81:
                    length = cl
                elif cl == 0x81:                    # no-op code
                    continue
                else:                               # end of stream
                    terminated = True
                    break
        bh = offset_high()
        copy(bh, byte(), length)
    return UnquikResult(bytes(out), st["si"], terminated, st["over"], bad)


# ------------------------------------------------------------- relocations

def decode_packets(buf: bytes, start: int, total: int, seg_n: int
                   ) -> tuple[list[dict[str, Any]], int, list[str]]:
    """Decode OPTLOADER relocation packets.

    ``buf`` holds the segment's on-disk bytes, ``start`` the offset of the
    first packet and ``total`` the item count from the segment's first word.
    Returns (records, bytes consumed, errors). Record shapes follow the
    legacy ``slr_relocs.py`` output (re/artifacts/relocs.json).
    """
    p = start
    remaining = total
    recs: list[dict[str, Any]] = []
    errs: list[str] = []

    def need(k: int) -> bool:
        if p + k > len(buf):
            errs.append(f"packet data runs past the segment end at {p:#x}")
            return False
        return True

    while remaining > 0:
        if not need(2):
            break
        (w,) = struct.unpack_from("<H", buf, p)
        p += 2
        count, al = w >> 8, w & 0xFF
        if count == 0:
            errs.append(f"zero-count packet at {p - 2:#x}")
            break
        if count > remaining:
            errs.append(f"packet at {p - 2:#x} holds {count} items, only {remaining} remain")
        remaining -= count

        if al == 0xF0:
            if not need(3 * count):
                break
            for _ in range(count):
                sel = buf[p]
                (off,) = struct.unpack_from("<H", buf, p + 1)
                p += 3
                recs.append({"seg": seg_n, "kind": "special_base",
                             "target_type": "SEG16_from_idx", "offset": off,
                             "selector_idx": sel})
            continue

        ttype = TARG_NAMES[al & 7]
        stype = (al >> 3) & 3
        if al & 0xE0:
            errs.append(f"packet type byte {al:#04x} at {p - 2:#x} has unknown high bits")
        if stype == 0:                                  # INTERNAL
            if not need(1 + 4 * count):
                break
            ref = buf[p]
            p += 1
            for _ in range(count):
                off, val = struct.unpack_from("<HH", buf, p)
                p += 4
                recs.append({"seg": seg_n, "kind": "internal", "target_type": ttype,
                             "offset": off, "ref_seg_raw": ref, "value": val})
        elif stype in (1, 2):                           # ORDINAL / NAME
            if not need(2 + 4 * count):
                break
            (mod,) = struct.unpack_from("<H", buf, p)
            p += 2
            for _ in range(count):
                off, val = struct.unpack_from("<HH", buf, p)
                p += 4
                if stype == 1:
                    recs.append({"seg": seg_n, "kind": "import_ordinal", "target_type": ttype,
                                 "offset": off, "module_idx": mod, "ordinal": val})
                else:
                    recs.append({"seg": seg_n, "kind": "import_name", "target_type": ttype,
                                 "offset": off, "module_idx": mod, "imptab_off": val})
        else:                                           # OSFLOAT
            if not need(2 + 2 * count):
                break
            (ftype,) = struct.unpack_from("<H", buf, p)
            p += 2
            for _ in range(count):
                (off,) = struct.unpack_from("<H", buf, p)
                p += 2
                recs.append({"seg": seg_n, "kind": "osfloat", "target_type": ttype,
                             "offset": off, "n": ftype})
    return recs, p - start, errs


# ------------------------------------------------------------- unpacking

@dataclass
class SegmentResult:
    n: int
    file_off: int
    file_len: int
    flags: int
    minalloc: int
    packed: bool
    image: bytes
    w0: int = 0
    decomp_len: int = 0
    reloc_start: int = 0
    reloc_bytes_used: int = 0
    records: list[dict[str, Any]] = field(default_factory=list)
    errors: list[str] = field(default_factory=list)

    @property
    def slack(self) -> int:
        return self.file_len - (self.reloc_start + self.reloc_bytes_used)

    def chains(self) -> tuple[list[list[int]], list[str]]:
        """Walk every record's fixup chain through the unpacked image.

        Returns one list of patched offsets per record, plus errors. Additive,
        BYTE and OSFLOAT records patch a single site (OSFLOAT adds once, per
        RELOC_SRC_OSFLOAT in nappload.asm); the others follow the word at
        each site to the next one until 0xFFFF, as RELOC_INT_OFF16 and
        RELOC_INT_PTR16 do. No site may be visited twice within a segment.
        """
        out: list[list[int]] = []
        errs: list[str] = []
        seen: set[int] = set()
        img = self.image
        for rec in self.records:
            tt = rec["target_type"]
            width = 4 if tt.startswith("PTR16") else (1 if tt.startswith("BYTE") else 2)
            pos = rec["offset"]
            sites: list[int] = []
            while True:
                if pos + width > len(img):
                    errs.append(f"seg{self.n} chain from {rec['offset']:#x} leaves the segment at {pos:#x}")
                    break
                if pos in seen:
                    errs.append(f"seg{self.n} site {pos:#x} is patched twice")
                    break
                seen.add(pos)
                sites.append(pos)
                if tt.endswith("_A") or tt == "BYTE" or rec["kind"] == "osfloat":
                    break
                (link,) = struct.unpack_from("<H", img, pos)
                if link == 0xFFFF:
                    break
                if len(sites) >= 0x10000:
                    errs.append(f"seg{self.n} chain from {rec['offset']:#x} does not end")
                    break
                pos = link
            out.append(sites)
        return out, errs

    @property
    def invariants_ok(self) -> bool:
        if not self.packed:
            return True
        return (not self.errors and self.slack == 0 and len(self.records) == self.w0
                and self.decomp_len <= self.minalloc and not self.chains()[1])

    def legacy_report(self) -> dict[str, Any]:
        """Per-segment report entry in the legacy slr_relocs.py layout."""
        if not self.packed:
            return {"seg": self.n, "nsiter": False, "items": 0}
        if self.w0 == 0:
            return {"seg": self.n, "nsiter": True, "items": 0, "decomp": self.decomp_len,
                    "minalloc": self.minalloc, "reloc_start": self.reloc_start,
                    "avail": self.file_len - self.reloc_start}
        return {
            "seg": self.n, "nsiter": True, "items": self.w0, "decoded": len(self.records),
            "decomp": self.decomp_len, "minalloc": self.minalloc,
            "reloc_start": self.reloc_start,
            "reloc_bytes_avail": self.file_len - self.reloc_start,
            "reloc_bytes_used": self.reloc_bytes_used,
            "errs": self.errors,
            "kinds": dict(Counter(r["kind"] for r in self.records)),
        }

    def manifest(self) -> dict[str, Any]:
        d: dict[str, Any] = {
            "seg": self.n, "file_off": self.file_off, "file_len": self.file_len,
            "flags": f"{self.flags:#06x}", "minalloc": self.minalloc, "packed": self.packed,
            "image_len": len(self.image), "sha256": sha256(self.image),
        }
        if self.packed:
            chains, chain_errs = self.chains()
            d.update({
                "reloc_items_word0": self.w0, "reloc_records": len(self.records),
                "decomp_len": self.decomp_len, "reloc_start": self.reloc_start,
                "reloc_bytes_used": self.reloc_bytes_used, "slack": self.slack,
                "chain_sites": sum(len(c) for c in chains), "chain_errors": chain_errs,
                "errors": self.errors, "invariants_ok": self.invariants_ok,
            })
        return d


@dataclass
class UnpackResult:
    name: str
    modules: dict[int, str]
    segments: list[SegmentResult]

    @property
    def records(self) -> list[dict[str, Any]]:
        return [r for s in self.segments for r in s.records]

    @property
    def ok(self) -> bool:
        return all(s.invariants_ok for s in self.segments)

    def relocs_json(self) -> dict[str, Any]:
        """relocs.json content, byte-compatible with the legacy slr_relocs.py output."""
        return {"modules": self.modules,
                "report": [s.legacy_report() for s in self.segments],
                "relocs": self.records}

    def manifest(self) -> dict[str, Any]:
        recs = self.records
        return {
            "module": self.name,
            "invariants_ok": self.ok,
            "reloc_records": len(recs),
            "reloc_kinds": dict(Counter(r["kind"] for r in recs)),
            "segments": [s.manifest() for s in self.segments],
        }


def unpack(mod: NEModule) -> UnpackResult:
    """Decompress every segment and decode the relocations of an OPTLOADER module."""
    info = mod.optloader()
    if not info["packed"]:
        raise OptLoaderError(f"{mod.name or 'module'} is not OPTLOADER-packed")
    packed = set(info["packed_segments"])
    d = mod.data
    imptab = mod.ne_off + mod.header["imptab"]
    modules = {i + 1: name for i, name in enumerate(mod.modules)}
    segs: list[SegmentResult] = []
    for s in mod.segments:
        raw = mod.segment_file_bytes(s.n)
        if s.n not in packed:
            image = raw if s.has_file_data else bytes(s.minalloc)
            segs.append(SegmentResult(s.n, s.file_off, s.file_len, s.flags, s.minalloc, False, image))
            continue
        if len(raw) != s.file_len:
            raise OptLoaderError(f"seg{s.n} runs past the end of the file")
        (w0,) = struct.unpack_from("<H", raw, 0)
        res = unquik(raw[2:], max_out=s.minalloc + 4096)
        errors = []
        if not res.terminated:
            errors.append("bitstream did not reach its end-of-stream code")
        if res.overread:
            errors.append(f"bitstream read {res.overread} bytes past the segment end")
        if res.bad_backrefs:
            errors.append(f"{res.bad_backrefs} match bytes referenced data before the segment start")
        reloc_start = 2 + res.consumed
        recs, used, rerrs = decode_packets(raw, reloc_start, w0, s.n) if w0 else ([], 0, [])
        errors.extend(rerrs)
        for r in recs:
            if "module_idx" in r:
                r["module"] = modules.get(r["module_idx"], "?%d" % r["module_idx"])
            if r["kind"] == "import_name":
                q = imptab + r["imptab_off"]
                r["name"] = d[q + 1:q + 1 + d[q]].decode("latin-1")
        image = res.data + bytes(max(0, s.minalloc - len(res.data)))
        segs.append(SegmentResult(
            s.n, s.file_off, s.file_len, s.flags, s.minalloc, True, image,
            w0=w0, decomp_len=len(res.data), reloc_start=reloc_start,
            reloc_bytes_used=used, records=recs, errors=errors,
        ))
    return UnpackResult(mod.name, modules, segs)


def write_unpacked(result: UnpackResult, out_dir: Path) -> list[Path]:
    """Write segNN.bin, relocs.json and unpack.json; return the paths written."""
    out_dir = Path(out_dir)
    written = []
    files = {f"seg{s.n:02d}.bin": s.image for s in result.segments}
    files["relocs.json"] = json_bytes(result.relocs_json())
    files["unpack.json"] = json_bytes(result.manifest(), indent=1)
    for name, data in files.items():
        if write_if_changed(out_dir / name, data):
            written.append(out_dir / name)
    return written


def write_relocs(result: UnpackResult, out_json: Path) -> bool:
    return write_if_changed(Path(out_json), json_bytes(result.relocs_json()))
