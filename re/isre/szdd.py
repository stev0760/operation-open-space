"""SZDD ("Microsoft Compress", COMPRESS.EXE -Z) expansion and distribution-disk extraction.

Header layout:

    0..7   magic  53 5A 44 44 88 F0 27 33  ("SZDD" + 4 bytes)
    8      compression mode, 'A' (0x41) = LZSS
    9      the filename character that COMPRESS replaced with '_' (0 if none)
    10..13 expanded size, little-endian uint32
    14..   LZSS stream

LZSS: a 4096-byte window filled with spaces, write position starting at
4096-16. Each flag byte controls 8 codes, least-significant bit first; a set
bit is a literal byte, a clear bit a 2-byte match ``lo, hi`` with window
position ``lo | (hi & 0xF0) << 4`` and length ``(hi & 0x0F) + 3``.
"""
from __future__ import annotations

import struct
from dataclasses import dataclass
from pathlib import Path

from isre.util import write_if_changed

SZDD_MAGIC = b"SZDD\x88\xf0\x27\x33"
HEADER_SIZE = 14

# Distribution names -> installed names. The shipdisk stores most files with
# the last extension character replaced by '1' (and two executables as
# '.ex_', which are *not* SZDD-compressed). INSTALL.INI on the same disk
# lists the installed names, e.g. MAIN.MID, ENG_HELP.HLP, INSPACE.SCR and
# INRUN.AD, which fixes the two short extensions: .sc1 -> .SCR, .A1 -> .AD.
# tests/test_szdd.py checks this table against INSTALL.INI.
RESTORED_EXTENSIONS = {
    ".mi1": ".MID",
    ".dl1": ".DLL",
    ".hl1": ".HLP",
    ".bo1": ".BOD",
    ".sh1": ".SHP",
    ".in1": ".INI",
    ".a1": ".AD",
    ".sc1": ".SCR",
    ".ex_": ".EXE",
}


class SzddError(ValueError):
    pass


@dataclass(frozen=True)
class SzddHeader:
    mode: int
    missing_char: int
    size: int


def is_szdd(data: bytes) -> bool:
    return data[:8] == SZDD_MAGIC


def parse_header(data: bytes) -> SzddHeader:
    if len(data) < HEADER_SIZE or not is_szdd(data):
        raise SzddError("not an SZDD file")
    mode, missing = data[8], data[9]
    (size,) = struct.unpack_from("<I", data, 10)
    return SzddHeader(mode, missing, size)


def expand(data: bytes) -> bytes:
    """Expand an SZDD file. Raises SzddError on a bad header or a short stream."""
    hdr = parse_header(data)
    if hdr.mode != 0x41:
        raise SzddError(f"unsupported SZDD mode {hdr.mode:#x}")
    size = hdr.size
    src = data[HEADER_SIZE:]
    n = len(src)
    out = bytearray()
    win = bytearray(b"\x20" * 4096)
    pos = 4096 - 16
    i = 0
    while i < n and len(out) < size:
        flags = src[i]
        i += 1
        for bit in range(8):
            if len(out) >= size or i >= n:
                break
            if flags & (1 << bit):
                c = src[i]
                i += 1
                out.append(c)
                win[pos] = c
                pos = (pos + 1) & 0xFFF
            else:
                if i + 1 >= n:
                    i = n
                    break
                lo, hi = src[i], src[i + 1]
                i += 2
                off = lo | ((hi & 0xF0) << 4)
                for _ in range((hi & 0x0F) + 3):
                    if len(out) >= size:
                        break
                    c = win[off & 0xFFF]
                    out.append(c)
                    win[pos] = c
                    pos = (pos + 1) & 0xFFF
                    off += 1
    if len(out) != size:
        raise SzddError(f"stream ended after {len(out)} of {size} bytes")
    return bytes(out)


def restored_name(name: str, header: SzddHeader | None = None) -> str:
    """Installed name for a distribution file name.

    Known distribution extensions map through RESTORED_EXTENSIONS (the base
    name keeps its case). Otherwise a trailing '_' is replaced by the SZDD
    header's missing character when there is one.
    """
    stem, dot, ext = name.rpartition(".")
    if dot:
        new_ext = RESTORED_EXTENSIONS.get("." + ext.lower())
        if new_ext:
            return stem + new_ext
    if header is not None and name.endswith("_") and 0x20 < header.missing_char < 0x7F:
        return name[:-1] + chr(header.missing_char)
    return name


@dataclass
class ExtractedFile:
    source: str
    target: str
    szdd: bool
    size: int
    written: bool


def extract_dir(src_dir: Path, out_dir: Path) -> list[ExtractedFile]:
    """Copy every regular file of ``src_dir`` to ``out_dir``.

    SZDD files are expanded; distribution names are restored. Files whose
    content is already in place are left untouched.
    """
    src_dir, out_dir = Path(src_dir), Path(out_dir)
    if not src_dir.is_dir():
        raise FileNotFoundError(f"{src_dir} is not a directory")
    out_dir.mkdir(parents=True, exist_ok=True)
    results: list[ExtractedFile] = []
    targets: dict[str, str] = {}
    for p in sorted(src_dir.iterdir(), key=lambda q: q.name):
        if not p.is_file():
            continue
        data = p.read_bytes()
        header = None
        if is_szdd(data):
            header = parse_header(data)
            data = expand(data)
        target = restored_name(p.name, header)
        clash = targets.get(target.lower())
        if clash is not None:
            raise SzddError(f"{p.name} and {clash} both restore to {target}")
        targets[target.lower()] = p.name
        written = write_if_changed(out_dir / target, data)
        results.append(ExtractedFile(p.name, target, header is not None, len(data), written))
    return results
