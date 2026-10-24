"""Address conversion between the forms used around this project.

    canonical   segN:0xOFF        N = 1-based NE segment number (docs use this)
    ghidra      SSSS:OFF          selector 0x1000 + (N-1)*8, e.g. seg4 -> 1018
    dgroup      DGROUP:0xOFF      the NE autodata segment (seg19 for INSPACE.EXE)
    com         segN/com:0xOFF    rea's dos-com view of one segment: OFF + 0x100
    file        file:0xOFF        a file offset; it maps to an address only inside
                                  a segment stored raw (not OPTLOADER-packed)
"""
from __future__ import annotations

import re
from dataclasses import dataclass
from typing import Any

from isre.ne import NEModule, ghidra_selector

INSPACE_DGROUP = 19
COM_BASE = 0x100

_HEX = r"(?:0x)?([0-9a-f]+)"
_RE_CANON = re.compile(r"seg(\d+):" + _HEX + r"$", re.I)
_RE_GHIDRA = re.compile(r"(?:0x)?([0-9a-f]{4}):" + _HEX + r"$", re.I)
_RE_DGROUP = re.compile(r"dgroup:" + _HEX + r"$", re.I)
_RE_COM = re.compile(r"(?:seg(\d+)/)?com:" + _HEX + r"$", re.I)
_RE_FILE = re.compile(r"file:" + _HEX + r"$", re.I)


class AddrError(ValueError):
    pass


@dataclass(frozen=True)
class Address:
    seg: int
    off: int

    def __post_init__(self) -> None:
        if self.seg < 1:
            raise AddrError(f"segment number {self.seg} must be >= 1")
        if not 0 <= self.off <= 0xFFFF:
            raise AddrError(f"offset {self.off:#x} is outside 0..0xffff")

    @property
    def canonical(self) -> str:
        return f"seg{self.seg}:{self.off:#x}"

    @property
    def ghidra(self) -> str:
        return f"{ghidra_selector(self.seg):04x}:{self.off:04x}"

    @property
    def com(self) -> str | None:
        v = self.off + COM_BASE
        return f"seg{self.seg}/com:{v:#x}" if v <= 0xFFFF else None

    def dgroup(self, dgroup_seg: int) -> str | None:
        return f"DGROUP:{self.off:#x}" if self.seg == dgroup_seg else None


def parse(text: str, dgroup_seg: int = INSPACE_DGROUP, seg_context: int | None = None,
          module: NEModule | None = None) -> Address:
    """Parse any supported address form into an Address."""
    t = text.strip()
    m = _RE_CANON.match(t)
    if m:
        return Address(int(m.group(1)), int(m.group(2), 16))
    m = _RE_DGROUP.match(t)
    if m:
        return Address(dgroup_seg, int(m.group(1), 16))
    m = _RE_COM.match(t)
    if m:
        seg = int(m.group(1)) if m.group(1) else seg_context
        if seg is None:
            raise AddrError("a com: address needs a segment, e.g. seg4/com:0x2ea2 or --seg 4")
        v = int(m.group(2), 16)
        if v < COM_BASE:
            raise AddrError(f"com address {v:#x} is below the 0x100 base")
        return Address(seg, v - COM_BASE)
    m = _RE_FILE.match(t)
    if m:
        if module is None:
            raise AddrError("a file: offset needs --ne FILE")
        return from_file_offset(module, int(m.group(1), 16))
    m = _RE_GHIDRA.match(t)
    if m:
        sel = int(m.group(1), 16)
        if sel < 0x1000 or (sel - 0x1000) % 8:
            raise AddrError(f"selector {sel:04x} is not of the form 0x1000 + (N-1)*8")
        return Address((sel - 0x1000) // 8 + 1, int(m.group(2), 16))
    raise AddrError(f"unrecognised address {text!r}")


def from_file_offset(module: NEModule, file_off: int) -> Address:
    packed = set(module.packed_segments())
    for s in module.segments:
        if s.has_file_data and s.file_off <= file_off < s.file_off + s.file_len:
            if s.n in packed:
                raise AddrError(
                    f"file offset {file_off:#x} lies in OPTLOADER-packed seg{s.n} "
                    f"(file {s.file_off:#x}+{s.file_len:#x}); packed bytes have no address")
            return Address(s.n, file_off - s.file_off)
    for r in module.resources:
        if r.file_off <= file_off < r.file_off + r.length:
            raise AddrError(
                f"file offset {file_off:#x} lies in resource {r.type_name}/{r.id} "
                f"(+{file_off - r.file_off:#x}), not in a segment")
    raise AddrError(f"file offset {file_off:#x} is not inside any segment")


def describe(addr: Address, dgroup_seg: int = INSPACE_DGROUP,
             module: NEModule | None = None) -> dict[str, Any]:
    d: dict[str, Any] = {
        "canonical": addr.canonical,
        "ghidra": addr.ghidra,
        "dgroup": addr.dgroup(dgroup_seg),
        "com": addr.com,
    }
    if module is not None:
        if addr.seg > len(module.segments):
            raise AddrError(f"{module.name} has only {len(module.segments)} segments")
        s = module.segment(addr.seg)
        d["segment_kind"] = "DATA" if s.is_data else "CODE"
        d["minalloc"] = s.minalloc
        d["in_segment"] = addr.off < s.minalloc
        d["packed"] = addr.seg in module.packed_segments()
        d["file"] = (f"file:{s.file_off + addr.off:#x}"
                     if s.has_file_data and not d["packed"] and addr.off < s.file_len else None)
    return d
