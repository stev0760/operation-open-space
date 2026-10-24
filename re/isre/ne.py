"""Win16 NE (New Executable) parser.

Header field offsets are relative to the NE header (``e_lfanew`` at MZ+0x3C):

    +00 'NE'           +02 linker major   +03 linker minor
    +04 entry table    +06 entry table length
    +08 file CRC (dword)
    +0C module flags   +0E autodata segment number
    +10 heap size      +12 stack size
    +14 CS:IP (dword: IP low word, CS high word)   +18 SS:SP (same)
    +1C segment count  +1E module reference count +20 nonresident names length
    +22 segment table  +24 resource table  +26 resident names
    +28 module refs    +2A imported names  (all NE-relative)
    +2C nonresident names (absolute file offset, dword)
    +30 movable entry count  +32 alignment shift  +34 resource segment count
    +36 target OS      +37 other flags
    +38 gangload area offset  +3A gangload length (sectors)
    +3C minimum code swap area  +3E expected Windows version (minor, major)

A segment is a data segment iff its flags have bit 0x0001 set; everything
else is code. The self-loading flag 0x0800 in the module flags hands segment
loading to the module's first segment; SLR OPTLOADER uses it, packs every
other segment and marks those with the (otherwise unused) ITERATED flag 0x0008.
"""
from __future__ import annotations

import struct
from dataclasses import asdict, dataclass, field
from typing import Any

from isre import szdd

# ---------------------------------------------------------------- constants

MODFLAG_NAMES = [
    (0x0001, "SINGLEDATA"),
    (0x0002, "MULTIPLEDATA"),
    (0x0004, "GLOBALINIT"),
    (0x0008, "PROTMODE"),
    (0x0010, "I86"),
    (0x0020, "I286"),
    (0x0040, "I386"),
    (0x0080, "I87"),
    (0x0800, "SELFLOAD"),
    (0x2000, "LINKERROR"),
    (0x8000, "LIBRARY"),
]
APPTYPE_NAMES = {0: None, 1: "NOTWINCOMPAT", 2: "WINCOMPAT", 3: "WINAPI"}
MODFLAG_SELFLOAD = 0x0800
MODFLAG_LIBRARY = 0x8000

SEG_DATA = 0x0001
SEG_ITERATED = 0x0008
SEG_MOVABLE = 0x0010
SEG_SHARED = 0x0020
SEG_PRELOAD = 0x0040
SEG_READONLY = 0x0080
SEG_RELOCINFO = 0x0100
SEG_DISCARDABLE = 0x1000
SEGFLAG_NAMES = [
    (SEG_DATA, "DATA"),
    (SEG_ITERATED, "ITERATED"),
    (SEG_MOVABLE, "MOVABLE"),
    (SEG_SHARED, "SHARED"),
    (SEG_PRELOAD, "PRELOAD"),
    (SEG_READONLY, "READONLY"),
    (SEG_RELOCINFO, "RELOCINFO"),
    (0x0200, "CONFORMING"),
    (SEG_DISCARDABLE, "DISCARDABLE"),
]

TARGET_OS = {0: "unknown", 1: "OS/2", 2: "Windows", 3: "European MS-DOS 4", 4: "Windows/386", 5: "BOSS"}

RES_TYPE_NAMES = {
    1: "CURSOR", 2: "BITMAP", 3: "ICON", 4: "MENU", 5: "DIALOG", 6: "STRING",
    7: "FONTDIR", 8: "FONT", 9: "ACCELERATOR", 10: "RCDATA", 11: "MESSAGETABLE",
    12: "GROUP_CURSOR", 14: "GROUP_ICON", 15: "NAMETABLE", 16: "VERSION",
    17: "DLGINCLUDE", 19: "PLUGPLAY", 20: "VXD", 21: "ANICURSOR", 22: "ANIICON",
    23: "HTML", 24: "MANIFEST",
}
RESFLAG_NAMES = [(0x0010, "MOVABLE"), (0x0020, "PURE"), (0x0040, "PRELOAD"), (0x1000, "DISCARDABLE")]

# Standard (non-OptLoader) per-segment relocation records.
RELOC_SRC_NAMES = {0: "LOBYTE", 2: "SEGMENT", 3: "FAR_ADDR", 5: "OFFSET", 11: "FAR48", 13: "OFFSET32"}
RELOC_TARGET_NAMES = {0: "INTERNALREF", 1: "IMPORTORDINAL", 2: "IMPORTNAME", 3: "OSFIXUP"}

OPTLOADER_SIGNATURE = b"OPTLOADER"


class NEError(ValueError):
    pass


def _flag_names(value: int, table: list[tuple[int, str]]) -> list[str]:
    return [name for bit, name in table if value & bit]


# ---------------------------------------------------------------- records

@dataclass
class Segment:
    n: int
    sector: int
    file_off: int
    file_len: int
    flags: int
    minalloc: int

    @property
    def is_data(self) -> bool:
        return bool(self.flags & SEG_DATA)

    @property
    def is_code(self) -> bool:
        return not self.is_data

    @property
    def iterated(self) -> bool:
        return bool(self.flags & SEG_ITERATED)

    @property
    def has_relocs(self) -> bool:
        return bool(self.flags & SEG_RELOCINFO)

    @property
    def has_file_data(self) -> bool:
        return self.sector != 0

    def to_json(self) -> dict[str, Any]:
        return {
            "n": self.n,
            "sector": self.sector,
            "file_off": self.file_off,
            "file_len": self.file_len,
            "flags": f"{self.flags:#06x}",
            "flag_names": _flag_names(self.flags, SEGFLAG_NAMES),
            "minalloc": self.minalloc,
            "kind": "DATA" if self.is_data else "CODE",
            "ghidra_selector": f"{ghidra_selector(self.n):04x}",
        }


@dataclass
class Resource:
    type_id: int | str
    type_name: str
    id: int | str
    file_off: int
    length: int
    raw_offset: int
    raw_length: int
    flags: int

    def to_json(self) -> dict[str, Any]:
        d = asdict(self)
        d["flags"] = f"{self.flags:#06x}"
        d["flag_names"] = _flag_names(self.flags, RESFLAG_NAMES)
        return d


@dataclass
class Entry:
    ordinal: int
    seg: int          # 1-based segment number, or 0xFE for a constant
    offset: int
    flags: int
    movable: bool

    @property
    def exported(self) -> bool:
        return bool(self.flags & 0x01)

    def to_json(self) -> dict[str, Any]:
        return {
            "ordinal": self.ordinal,
            "seg": self.seg,
            "offset": self.offset,
            "flags": self.flags,
            "exported": self.exported,
            "shared_data": bool(self.flags & 0x02),
            "movable": self.movable,
            "constant": self.seg == 0xFE,
        }


@dataclass
class StdReloc:
    seg: int
    src_type: int
    flags: int
    offset: int
    target: dict[str, Any] = field(default_factory=dict)

    def to_json(self) -> dict[str, Any]:
        return {
            "seg": self.seg,
            "src_type": RELOC_SRC_NAMES.get(self.src_type, f"?{self.src_type}"),
            "target_type": RELOC_TARGET_NAMES[self.flags & 3],
            "additive": bool(self.flags & 4),
            "offset": self.offset,
            **self.target,
        }


def ghidra_selector(seg_n: int) -> int:
    """Selector Ghidra's NE loader gives segment ``seg_n`` (1-based)."""
    return 0x1000 + (seg_n - 1) * 8


# ---------------------------------------------------------------- module

class NEModule:
    """A parsed NE module. Construct with ``NEModule(data)`` or ``NEModule.load(path)``."""

    def __init__(self, data: bytes, name: str = ""):
        self.data = bytes(data)
        self.name = name
        d = self.data
        if len(d) < 0x40 or d[:2] != b"MZ":
            raise NEError("no MZ header")
        (self.ne_off,) = struct.unpack_from("<I", d, 0x3C)
        if self.ne_off + 0x40 > len(d) or d[self.ne_off:self.ne_off + 2] != b"NE":
            raise NEError(f"no NE header at {self.ne_off:#x}")
        self.header = self._parse_header()
        h = self.header
        self.segments = self._parse_segments()
        self.resident_names = self._parse_names(self.ne_off + h["restab"], None)
        self.nonresident_names = (
            self._parse_names(h["nrestab_file"], h["nrestab_file"] + h["cbnrestab"])
            if h["nrestab_file"] and h["cbnrestab"] else []
        )
        self.imported_names = self._parse_imported_names()
        self.modules = self._parse_module_refs()
        self.entries = self._parse_entries()
        self.resources, self.res_align_shift = self._parse_resources()
        self.std_relocs = self._parse_std_relocs()

    @classmethod
    def load(cls, path, expand_szdd: bool = True) -> "NEModule":
        """Read an NE file, expanding it first when it is SZDD-compressed."""
        from pathlib import Path

        p = Path(path)
        data = p.read_bytes()
        if expand_szdd and szdd.is_szdd(data):
            data = szdd.expand(data)
        return cls(data, p.name)

    # ---- low-level readers

    def _w(self, ne_rel: int) -> int:
        return struct.unpack_from("<H", self.data, self.ne_off + ne_rel)[0]

    def _dw(self, ne_rel: int) -> int:
        return struct.unpack_from("<I", self.data, self.ne_off + ne_rel)[0]

    def _pstr(self, off: int) -> str:
        n = self.data[off]
        return self.data[off + 1:off + 1 + n].decode("latin-1")

    # ---- tables

    def _parse_header(self) -> dict[str, Any]:
        d, ne = self.data, self.ne_off
        h = {
            "linker_major": d[ne + 2],
            "linker_minor": d[ne + 3],
            "enttab": self._w(0x04),
            "cbenttab": self._w(0x06),
            "crc": self._dw(0x08),
            "flags": self._w(0x0C),
            "autodata": self._w(0x0E),
            "heap": self._w(0x10),
            "stack": self._w(0x12),
            "ip": self._w(0x14),
            "cs": self._w(0x16),
            "sp": self._w(0x18),
            "ss": self._w(0x1A),
            "cseg": self._w(0x1C),
            "cmod": self._w(0x1E),
            "cbnrestab": self._w(0x20),
            "segtab": self._w(0x22),
            "rsrctab": self._w(0x24),
            "restab": self._w(0x26),
            "modtab": self._w(0x28),
            "imptab": self._w(0x2A),
            "nrestab_file": self._dw(0x2C),
            "cmovent": self._w(0x30),
            "align_shift": self._w(0x32),
            "cres": self._w(0x34),
            "exetyp": d[ne + 0x36],
            "flagsothers": d[ne + 0x37],
            "gangload_off": self._w(0x38),
            "gangload_len": self._w(0x3A),
            "swaparea": self._w(0x3C),
            "expver": f"{d[ne + 0x3F]}.{d[ne + 0x3E]:02d}",
        }
        if h["align_shift"] == 0:
            h["align_shift"] = 9  # the NE spec's default when the field is 0
        return h

    def _parse_segments(self) -> list[Segment]:
        h = self.header
        base = self.ne_off + h["segtab"]
        segs = []
        for i in range(h["cseg"]):
            sector, ln, fl, ma = struct.unpack_from("<HHHH", self.data, base + i * 8)
            file_len = ln if ln or not sector else 0x10000
            segs.append(Segment(
                n=i + 1,
                sector=sector,
                file_off=sector << h["align_shift"],
                file_len=file_len,
                flags=fl,
                minalloc=ma if ma else 0x10000,
            ))
        return segs

    def _parse_names(self, start: int, end: int | None) -> list[dict[str, Any]]:
        """Resident/nonresident name tables: pascal string + ordinal word, 0-terminated."""
        out = []
        p = start
        d = self.data
        while p < len(d) and (end is None or p < end):
            n = d[p]
            if n == 0:
                break
            name = d[p + 1:p + 1 + n].decode("latin-1")
            (ordinal,) = struct.unpack_from("<H", d, p + 1 + n)
            out.append({"name": name, "ordinal": ordinal})
            p += 1 + n + 2
        return out

    def _imptab_end(self) -> int:
        """NE-relative end of the imported-names table: the next table after it."""
        h = self.header
        imp = h["imptab"]
        cands = [h[k] for k in ("enttab", "segtab", "rsrctab", "restab", "modtab") if h[k] > imp]
        nres = h["nrestab_file"] - self.ne_off
        if nres > imp:
            cands.append(nres)
        return min(cands) if cands else imp

    def _parse_imported_names(self) -> list[dict[str, Any]]:
        h = self.header
        base = self.ne_off + h["imptab"]
        end = self.ne_off + self._imptab_end()
        out = []
        p = base
        while p < end:
            n = self.data[p]
            if n:
                out.append({"offset": p - base, "name": self._pstr(p)})
            p += 1 + n
        return out

    def _parse_module_refs(self) -> list[str]:
        h = self.header
        mods = []
        for i in range(h["cmod"]):
            off = self._w(h["modtab"] + i * 2)
            mods.append(self._pstr(self.ne_off + h["imptab"] + off))
        return mods

    def _parse_entries(self) -> list[Entry]:
        h = self.header
        d = self.data
        p = self.ne_off + h["enttab"]
        end = p + h["cbenttab"]
        ordinal = 1
        out = []
        while p < end:
            count = d[p]
            if count == 0:
                break
            seg = d[p + 1]
            p += 2
            if seg == 0x00:            # unused ordinals, no entry data
                ordinal += count
                continue
            for _ in range(count):
                if seg == 0xFF:        # movable: flags, INT 3Fh, segment, offset
                    fl, int3f, sn, off = struct.unpack_from("<BHBH", d, p)
                    if int3f != 0x3FCD:
                        raise NEError(f"movable entry {ordinal} lacks INT 3Fh ({int3f:#06x})")
                    out.append(Entry(ordinal, sn, off, fl, True))
                    p += 6
                else:                  # fixed segment (or 0xFE constant): flags, offset
                    fl, off = struct.unpack_from("<BH", d, p)
                    out.append(Entry(ordinal, seg, off, fl, False))
                    p += 3
                ordinal += 1
        return out

    def _res_name(self, table: int, value: int) -> int | str:
        if value & 0x8000:
            return value & 0x7FFF
        return self._pstr(table + value)

    def _parse_resources(self) -> tuple[list[Resource], int]:
        h = self.header
        if h["rsrctab"] == h["restab"]:
            return [], 0
        table = self.ne_off + h["rsrctab"]
        d = self.data
        (shift,) = struct.unpack_from("<H", d, table)
        p = table + 2
        out = []
        while True:
            (type_raw,) = struct.unpack_from("<H", d, p)
            if type_raw == 0:
                break
            count, _reserved = struct.unpack_from("<HI", d, p + 2)
            type_id = self._res_name(table, type_raw)
            if isinstance(type_id, int):
                type_name = RES_TYPE_NAMES.get(type_id, str(type_id))
            else:
                type_name = type_id
            p += 8
            for _ in range(count):
                off, ln, fl, rid, _h, _u = struct.unpack_from("<HHHHHH", d, p)
                out.append(Resource(
                    type_id=type_id,
                    type_name=type_name,
                    id=self._res_name(table, rid),
                    file_off=off << shift,
                    length=ln << shift,
                    raw_offset=off,
                    raw_length=ln,
                    flags=fl,
                ))
                p += 12
        return out, shift

    def _parse_std_relocs(self) -> dict[int, list[StdReloc]]:
        """Standard NE relocation records that follow a segment's file data."""
        out: dict[int, list[StdReloc]] = {}
        d = self.data
        for s in self.segments:
            if not (s.has_relocs and s.has_file_data):
                continue
            p = s.file_off + s.file_len
            (count,) = struct.unpack_from("<H", d, p)
            p += 2
            recs = []
            for _ in range(count):
                src, fl, off, a, b = struct.unpack_from("<BBHHH", d, p)
                p += 8
                tt = fl & 3
                if tt == 0:
                    seg_b = a & 0xFF
                    target = ({"target_seg": "movable", "entry_ordinal": b} if seg_b == 0xFF
                              else {"target_seg": seg_b, "target_offset": b})
                elif tt == 1:
                    target = {"module_idx": a, "module": self._module_name(a), "ordinal": b}
                elif tt == 2:
                    target = {"module_idx": a, "module": self._module_name(a),
                              "name": self._pstr(self.ne_off + self.header["imptab"] + b)}
                else:
                    target = {"osfixup": a}
                recs.append(StdReloc(s.n, src, fl, off, target))
            out[s.n] = recs
        return out

    def _module_name(self, idx: int) -> str | None:
        return self.modules[idx - 1] if 1 <= idx <= len(self.modules) else None

    # ---- derived facts

    @property
    def flags(self) -> int:
        return self.header["flags"]

    @property
    def is_library(self) -> bool:
        return bool(self.flags & MODFLAG_LIBRARY)

    @property
    def self_loading(self) -> bool:
        return bool(self.flags & MODFLAG_SELFLOAD)

    @property
    def autodata(self) -> int:
        return self.header["autodata"]

    @property
    def module_name(self) -> str:
        return self.resident_names[0]["name"] if self.resident_names else ""

    @property
    def description(self) -> str:
        return self.nonresident_names[0]["name"] if self.nonresident_names else ""

    def segment(self, n: int) -> Segment:
        if not 1 <= n <= len(self.segments):
            raise NEError(f"segment {n} out of range 1..{len(self.segments)}")
        return self.segments[n - 1]

    def segment_file_bytes(self, n: int) -> bytes:
        s = self.segment(n)
        if not s.has_file_data:
            return b""
        return self.data[s.file_off:s.file_off + s.file_len]

    def optloader(self) -> dict[str, Any]:
        """Detect SLR OPTLOADER packing.

        Self-loading flag 0x0800, segment 1 starting with the self-load
        signature 'A0' and carrying the OPTLOADER copyright string.
        """
        info: dict[str, Any] = {"self_loading": self.self_loading, "packed": False}
        if not self.segments:
            return info
        seg1 = self.segment_file_bytes(1)
        info["selfload_signature"] = seg1[:2] == b"A0"
        info["optloader_string"] = OPTLOADER_SIGNATURE in seg1
        packed = (self.self_loading and info["selfload_signature"] and info["optloader_string"])
        info["packed"] = bool(packed)
        if packed:
            info["stub_seg"] = 1
            info["stub_len"] = len(seg1)
            info["packed_segments"] = [s.n for s in self.segments[1:] if s.iterated and s.has_file_data]
        return info

    def packed_segments(self) -> list[int]:
        info = self.optloader()
        return info.get("packed_segments", []) if info["packed"] else []

    def to_json(self) -> dict[str, Any]:
        h = dict(self.header)
        h["flags_hex"] = f"{h['flags']:#06x}"
        return {
            "file": self.name,
            "ne_offset": self.ne_off,
            "module_name": self.module_name,
            "description": self.description,
            "header": h,
            "module_flags": _flag_names(h["flags"], MODFLAG_NAMES),
            "app_type": APPTYPE_NAMES.get((h["flags"] >> 8) & 7, (h["flags"] >> 8) & 7),
            "target_os": TARGET_OS.get(h["exetyp"], h["exetyp"]),
            "optloader": self.optloader(),
            "segments": [s.to_json() for s in self.segments],
            "modules": self.modules,
            "imported_names": self.imported_names,
            "entries": [e.to_json() for e in self.entries],
            "resident_names": self.resident_names,
            "nonresident_names": self.nonresident_names,
            "resource_align_shift": self.res_align_shift,
            "resources": [r.to_json() for r in self.resources],
            "segment_relocations": {str(k): [r.to_json() for r in v] for k, v in self.std_relocs.items()},
        }

    def summary(self) -> str:
        h = self.header
        lines = [
            f"{self.name}: NE at {self.ne_off:#x}, linker {h['linker_major']}.{h['linker_minor']}, "
            f"target {TARGET_OS.get(h['exetyp'], h['exetyp'])}, expects Windows {h['expver']}",
            f"  module {self.module_name!r}  description {self.description!r}",
            f"  flags {h['flags']:#06x} {' '.join(_flag_names(h['flags'], MODFLAG_NAMES))}"
            f" apptype={APPTYPE_NAMES.get((h['flags'] >> 8) & 7)}",
            f"  autodata seg{h['autodata']}  CS:IP seg{h['cs']}:{h['ip']:#06x}  SS:SP seg{h['ss']}:{h['sp']:#06x}"
            f"  heap {h['heap']:#x} stack {h['stack']:#x}  align shift {h['align_shift']}",
        ]
        ol = self.optloader()
        if ol["packed"]:
            lines.append(f"  OPTLOADER packed: stub seg1 ({ol['stub_len']} bytes), packed segments "
                         f"{ol['packed_segments']}")
        elif ol["self_loading"]:
            lines.append("  self-loading (not recognised as OPTLOADER)")
        lines.append(f"  segments ({len(self.segments)}):")
        for s in self.segments:
            lines.append(
                f"    seg{s.n:<3d} {ghidra_selector(s.n):04x}  file {s.file_off:#08x} len {s.file_len:6d}"
                f"  min {s.minalloc:6d}  flags {s.flags:#06x} {'DATA' if s.is_data else 'CODE'}"
                f"  {' '.join(n for n in _flag_names(s.flags, SEGFLAG_NAMES) if n != 'DATA')}"
            )
        lines.append(f"  modules ({len(self.modules)}): {' '.join(self.modules)}")
        exported = [e for e in self.entries if e.exported]
        lines.append(f"  entries: {len(self.entries)} ({len(exported)} exported)")
        for n in self.resident_names[1:]:
            lines.append(f"    resident  {n['ordinal']:4d} {n['name']}")
        for n in self.nonresident_names[1:]:
            lines.append(f"    nonres    {n['ordinal']:4d} {n['name']}")
        lines.append(f"  resources ({len(self.resources)}), align shift {self.res_align_shift}:")
        for r in self.resources:
            rid = r.id if isinstance(r.id, int) else repr(r.id)
            lines.append(f"    {r.type_name:<12s} {str(rid):<12s} file {r.file_off:#08x} len {r.length}")
        for n, recs in self.std_relocs.items():
            lines.append(f"  seg{n} standard relocations: {len(recs)}")
        return "\n".join(lines)


def is_ne(data: bytes) -> bool:
    if len(data) < 0x40 or data[:2] != b"MZ":
        return False
    (off,) = struct.unpack_from("<I", data, 0x3C)
    return off + 2 <= len(data) and data[off:off + 2] == b"NE"
