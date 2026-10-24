"""WinHelp 3.x (.HLP) reader and Markdown extractor.

Usage::

    python re/isre/hlp.py FILE.HLP OUT_DIR     # standalone
    isre hlp FILE.HLP OUT_DIR                  # through the isre CLI
    isre hlp FILE.HLP                          # summary only

``FILE`` may be SZDD-compressed (the shipdisk's ``Eng_help.hl1``); it is
expanded in memory first. ``OUT_DIR`` receives ``index.md`` (|SYSTEM data,
topic list with context ids and help map ids, keyword index, bitmaps), one
``NN-slug.md`` per topic and every ``|bmN`` picture as ``bmN.bmp``.

Format reference: ``helpfile.txt`` of helpdeco (Manfred Winterhoff, GPL).
The facts this reader relies on, as checked against the two Inner Space
manuals:

* The file starts with a 16-byte header (magic 0x00035F3F, directory offset).
  Each internal file starts with a 9-byte FILEHEADER (reserved, used, flags).
  The directory and |CONTEXT, |TTLBTREE, |KWBTREE are B+ trees (magic
  0x293B, 38-byte header, pages after it).
* |SYSTEM: magic 0x036C, Minor 21 = HC31, Flags 0 = no LZ77. Minor > 16
  means typed SYSTEMREC records follow.
* |TOPIC is split into 4096-byte blocks, each with a 12-byte header. A
  TOPICPOS addresses the block data as block = (pos - 12) // 0x4000,
  offset = (pos - 12) % 0x4000. Records (TOPICLINKs) are walked through
  their NextBlock pointer, never sequentially: unused space inside |TOPIC is
  zero-filled. A record that runs past the end of a block's data continues
  at the start of the next block's data. The walk ends at the record whose
  NextBlock is 0 or -1; in both manuals that is an empty topic header, so
  |TOPIC holds one more topic header (and |TTLBTREE one more title, empty)
  than there are topics.
* A TOPICOFFSET (used by |CONTEXT, |CTXOMAP, |TTLBTREE, |KWDATA and the
  browse links) is block * 0x8000 + character count, where the count restarts
  at 0 with the first record that starts in that block and advances by the
  TopicLength of every display or table record.
* The picture size in the 0x86/0x87/0x88 commands is a compressed signed long.
* Hotspot jumps (0xE2/0xE3/0xE6/0xE7) carry the hash of the context id,
  resolved through |CONTEXT. Context-id strings are not stored in the file:
  they are recovered by hashing candidate names (titles, keywords, link
  texts); unrecovered ids are shown as ``#XXXXXXXX`` hashes.

Not supported (raises HlpError): Windows 3.0 (HC30) files, phrase or Hall
compressed text. LZ77-compressed topic blocks and pictures are supported but
untested on real files (neither Inner Space manual uses them).
"""
from __future__ import annotations

import argparse
import bisect
import datetime as _dt
import re
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Iterator, Sequence

HLP_MAGIC = 0x00035F3F
BTREE_MAGIC = 0x293B
SYSTEM_MAGIC = 0x036C
BTREE_HEADER_SIZE = 38
FILEHEADER_SIZE = 9
TOPICLINK_SIZE = 21
TOPICBLOCKHEADER_SIZE = 12

TL_TOPICHDR = 0x02
TL_DISPLAY = 0x20
TL_TABLE = 0x23

SZDD_MAGIC = b"SZDD\x88\xf0\x27\x33"


class HlpError(ValueError):
    pass


# --------------------------------------------------------------------------
# small binary helpers

def _u16(b: bytes, o: int) -> int:
    return struct.unpack_from("<H", b, o)[0]


def _i16(b: bytes, o: int) -> int:
    return struct.unpack_from("<h", b, o)[0]


def _u32(b: bytes, o: int) -> int:
    return struct.unpack_from("<I", b, o)[0]


def _i32(b: bytes, o: int) -> int:
    return struct.unpack_from("<i", b, o)[0]


def _cstr(b: bytes, o: int) -> tuple[bytes, int]:
    """NUL-terminated string at ``o``: (bytes without NUL, offset after NUL)."""
    end = b.find(b"\0", o)
    if end < 0:
        return b[o:], len(b)
    return b[o:end], end + 1


def _dec(s: bytes) -> str:
    return s.decode("cp1252", errors="replace")


# Compressed integers (helpfile.txt, "Data inside LinkData1 is sometimes
# stored as compressed shorts or longs"). Each returns (value, new offset).

def cu16(b: bytes, o: int) -> tuple[int, int]:
    """Compressed unsigned short: 1 byte if even, else 2 bytes; value >> 1."""
    if b[o] & 1:
        return _u16(b, o) >> 1, o + 2
    return b[o] >> 1, o + 1


def ci16(b: bytes, o: int) -> tuple[int, int]:
    """Compressed signed short."""
    if b[o] & 1:
        return (_u16(b, o) >> 1) - 0x4000, o + 2
    return (b[o] >> 1) - 0x40, o + 1


def cu32(b: bytes, o: int) -> tuple[int, int]:
    """Compressed unsigned long: 2 bytes if even, else 4 bytes; value >> 1."""
    if b[o] & 1:
        return _u32(b, o) >> 1, o + 4
    return _u16(b, o) >> 1, o + 2


def ci32(b: bytes, o: int) -> tuple[int, int]:
    """Compressed signed long."""
    if b[o] & 1:
        return (_u32(b, o) >> 1) - 0x40000000, o + 4
    return (_u16(b, o) >> 1) - 0x4000, o + 2


# --------------------------------------------------------------------------
# decompressors

def lz77_expand(data: bytes, limit: int | None = None) -> bytes:
    """WinHelp LZ77: flag byte, LSB first; set bit = 2-byte back reference
    (length = (w >> 12) + 3, distance = (w & 0xFFF) + 1), clear bit = literal."""
    out = bytearray()
    i, n = 0, len(data)
    while i < n:
        flags = data[i]
        i += 1
        for bit in range(8):
            if i >= n or (limit is not None and len(out) >= limit):
                break
            if flags & (1 << bit):
                if i + 1 >= n:
                    i = n
                    break
                w = data[i] | (data[i + 1] << 8)
                i += 2
                length = (w >> 12) + 3
                start = len(out) - ((w & 0x0FFF) + 1)
                if start < 0:
                    raise HlpError("LZ77 reference before start of data")
                for k in range(length):
                    out.append(out[start + k])
            else:
                out.append(data[i])
                i += 1
        if limit is not None and len(out) >= limit:
            break
    return bytes(out[:limit] if limit is not None else out)


def runlen_expand(data: bytes) -> bytes:
    """WinHelp picture RunLen: n & 0x80 -> copy n & 0x7F bytes, else repeat next byte n times."""
    out = bytearray()
    i, n = 0, len(data)
    while i < n:
        c = data[i]
        i += 1
        if c & 0x80:
            k = c & 0x7F
            out += data[i:i + k]
            i += k
        else:
            if i >= n:
                break
            out += bytes([data[i]]) * c
            i += 1
    return bytes(out)


def szdd_expand(data: bytes) -> bytes:
    """Expand an SZDD file, using isre.szdd when it is importable."""
    try:
        from isre import szdd as _szdd  # imported at run time only
        return _szdd.expand(data)
    except ImportError:
        pass
    if data[:8] != SZDD_MAGIC or len(data) < 14 or data[8] != 0x41:
        raise HlpError("not an SZDD (mode A) file")
    size = _u32(data, 10)
    src, out = data[14:], bytearray()
    win, pos, i = bytearray(b" " * 4096), 4096 - 16, 0
    while i < len(src) and len(out) < size:
        flags = src[i]
        i += 1
        for bit in range(8):
            if i >= len(src) or len(out) >= size:
                break
            if flags & (1 << bit):
                out.append(src[i])
                win[pos] = src[i]
                pos = (pos + 1) & 0xFFF
                i += 1
            else:
                if i + 1 >= len(src):
                    i = len(src)
                    break
                lo, hi = src[i], src[i + 1]
                i += 2
                off = lo | ((hi & 0xF0) << 4)
                for k in range((hi & 0x0F) + 3):
                    c = win[(off + k) & 0xFFF]
                    out.append(c)
                    win[pos] = c
                    pos = (pos + 1) & 0xFFF
    if len(out) != size:
        raise HlpError(f"SZDD stream ended after {len(out)} of {size} bytes")
    return bytes(out)


def load_bytes(path: str | Path) -> bytes:
    """Read a help file, expanding SZDD compression if present."""
    data = Path(path).read_bytes()
    if data[:8] == SZDD_MAGIC:
        data = szdd_expand(data)
    return data


# --------------------------------------------------------------------------
# context-id hash (helpfile.txt, |CONTEXT)

_HASH_TABLE = bytes.fromhex(
    "00d1d2d3d4d5d6d7d8d9dadbdcdddedf"
    "e0e1e2e3e4e5e6e7e8e9eaebecedeeef"
    "f00bf2f3f4f5f6f7f8f9fafbfcfd0cff"
    "0a0102030405060708090a0b0c0d0e0f"
    "101112131415161718191a1b1c1d1e1f"
    "202122232425262728292a0b0c0d0e0d"
    "101112131415161718191a1b1c1d1e1f"
    "202122232425262728292a2b2c2d2e2f"
    "505152535455565758595a5b5c5d5e5f"
    "606162636465666768696a6b6c6d6e6f"
    "707172737475767778797a7b7c7d7e7f"
    "808182830b85868788898a8b8c8d8e8f"
    "909192939495969798999a9b9c9d9e9f"
    "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf"
    "b0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
    "c0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
)
_HASH_SIGNED = [v - 256 if v >= 128 else v for v in _HASH_TABLE]


def context_hash(name: str) -> int:
    """WinHelp 3.1 context-id hash as an unsigned 32-bit value (empty -> 1)."""
    if not name:
        return 1
    h = 0
    for c in name.encode("cp1252", errors="replace"):
        h = (h * 43 + _HASH_SIGNED[c]) & 0xFFFFFFFF
    return h


_LEGAL_CTX = re.compile(r"[^0-9A-Za-z_.]")


# --------------------------------------------------------------------------
# data model

@dataclass
class SystemInfo:
    minor: int = 0
    major: int = 0
    gen_date: int = 0
    flags: int = 0
    title: str = ""
    copyright: str = ""
    citation: str = ""
    contents: int | None = None  # TOPICOFFSET
    config: list[str] = field(default_factory=list)
    windows: list[dict] = field(default_factory=list)
    icon: bytes | None = None
    other: dict[int, bytes] = field(default_factory=dict)

    @property
    def build_date(self) -> str:
        if not self.gen_date:
            return ""
        return _dt.datetime.fromtimestamp(self.gen_date, _dt.timezone.utc).strftime(
            "%Y-%m-%d %H:%M:%S UTC")


@dataclass
class Font:
    attributes: int
    half_points: int
    family: int
    face: str
    color: tuple[int, int, int]

    @property
    def bold(self) -> bool:
        return bool(self.attributes & 1)

    @property
    def italic(self) -> bool:
        return bool(self.attributes & 2)


@dataclass
class Link:
    kind: str            # "jump", "popup", "macro", "external"
    target: str = ""     # context name / macro text / external reference
    hash: int | None = None


@dataclass
class Run:
    """A piece of paragraph content: text, tab, line break or picture."""
    kind: str            # "text", "tab", "br", "image", "embed"
    text: str = ""
    font: int = -1
    link: Link | None = None
    align: str = ""      # for images: "c" (inline), "l", "r"


@dataclass
class Paragraph:
    runs: list[Run] = field(default_factory=list)
    nonscroll: bool = False
    left_indent: int = 0
    first_indent: int = 0
    tabs: list[int] = field(default_factory=list)
    align: str = ""      # "", "right", "center"

    def plain(self) -> str:
        out = []
        for r in self.runs:
            if r.kind == "text":
                out.append(r.text)
            elif r.kind == "tab":
                out.append("\t")
            elif r.kind == "br":
                out.append("\n")
        return "".join(out)


@dataclass
class TableRow:
    cells: list[list[Paragraph]] = field(default_factory=list)
    nonscroll: bool = False

    def plain(self) -> str:
        return "\t".join(" / ".join(p.plain() for p in c) for c in self.cells)


@dataclass
class Topic:
    index: int                 # 1-based, file order
    number: int                # TopicNum from the topic header
    offset: int                # TOPICOFFSET of the topic start
    pos: int                   # TOPICPOS of the topic header record
    title: str = ""
    macros: list[str] = field(default_factory=list)
    browse_prev: int = -1      # TOPICOFFSET
    browse_next: int = -1
    nonscroll_pos: int = -1    # TOPICPOS of the non-scrolling region, -1 if none
    scroll_pos: int = -1       # TOPICPOS of the scrolling region
    blocks: list = field(default_factory=list)   # Paragraph | TableRow
    contexts: list[str] = field(default_factory=list)
    map_ids: list[int] = field(default_factory=list)
    keywords: list[str] = field(default_factory=list)
    bitmaps: list[str] = field(default_factory=list)

    def plain(self) -> str:
        return "\n".join(b.plain() for b in self.blocks)

    def paragraphs(self) -> Iterator[Paragraph]:
        """Every paragraph in reading order, table cells included."""
        for b in self.blocks:
            if isinstance(b, Paragraph):
                yield b
            else:
                for cell in b.cells:
                    yield from cell

    def links(self) -> list[Link]:
        """Each hotspot once, in reading order."""
        out: list[Link] = []
        for para in self.paragraphs():
            for r in para.runs:
                if r.link is not None and not any(r.link is k for k in out):
                    out.append(r.link)
        return out


@dataclass
class Picture:
    name: str
    ptype: int            # 5 = DDB, 6 = DIB, 8 = metafile
    packing: int          # 0 none, 1 RunLen, 2 LZ77, 3 both
    width: int = 0
    height: int = 0
    bit_count: int = 0
    planes: int = 1
    xdpi: int = 0
    ydpi: int = 0
    colors_used: int = 0
    colors_important: int = 0
    palette: bytes = b""  # RGBQUAD entries
    bits: bytes = b""     # expanded pixel data
    hotspot_size: int = 0
    count: int = 1        # pictures in this |bm file (MRB)
    note: str = ""

    def to_bmp(self) -> bytes | None:
        """A Windows BMP for DIB/DDB pictures, None for metafiles."""
        if self.ptype not in (5, 6) or not self.width or not self.height:
            return None
        bpp = self.bit_count * max(self.planes, 1)
        dib_stride = ((self.width * bpp + 31) // 32) * 4
        bits = self.bits
        if self.ptype == 5:
            # DDB rows are padded to 16 bits; DIB rows to 32 bits.
            ddb_stride = ((self.width * bpp + 15) // 16) * 2
            rows = [bits[r * ddb_stride:(r + 1) * ddb_stride] for r in range(self.height)]
            rows.reverse()  # DDBs are top-down
            bits = b"".join(row.ljust(dib_stride, b"\0")[:dib_stride] for row in rows)
        size = dib_stride * self.height
        bits = bits[:size].ljust(size, b"\0")
        palette = self.palette
        if not palette and bpp <= 8:
            if bpp == 1:
                palette = b"\0\0\0\0\xff\xff\xff\0"
            else:
                palette = bytes(4 * (1 << bpp))
        ppm_x = round(self.xdpi * 10000 / 254) if self.xdpi else 0
        ppm_y = round(self.ydpi * 10000 / 254) if self.ydpi else 0
        info = struct.pack("<IiiHHIIiiII", 40, self.width, self.height, 1, bpp, 0, size,
                           ppm_x, ppm_y, len(palette) // 4 if bpp <= 8 else 0,
                           self.colors_important if self.colors_important > 1 else 0)
        off = 14 + len(info) + len(palette)
        head = struct.pack("<2sIHHI", b"BM", off + size, 0, 0, off)
        return head + info + palette + bits


@dataclass
class Keyword:
    text: str
    offsets: list[int]


# --------------------------------------------------------------------------
# reader

class HelpFile:
    """Parsed WinHelp 3.1 file. ``errors`` lists everything that did not parse."""

    def __init__(self, data: bytes, name: str = ""):
        self.data = data
        self.name = name
        self.errors: list[str] = []
        if len(data) < 16 or _u32(data, 0) != HLP_MAGIC:
            raise HlpError("not a WinHelp file (bad magic)")
        self.directory_start = _i32(data, 4)
        self.files: dict[str, int] = {}
        for fname, off in self._btree_leaves(self._raw_file(self.directory_start), self._leaf_dir):
            self.files[fname] = off
        self.system = self._parse_system()
        self.lz77 = self.system.minor > 16 and self.system.flags in (4, 8)
        self.block_size = 2048 if self.system.flags == 8 else 4096
        self.fonts = self._parse_fonts()
        self.contexts = self._parse_context()          # [(hash, topicoffset)]
        self.ctxomap = self._parse_ctxomap()           # [(mapid, topicoffset)]
        self.titles = self._parse_titles()             # {topicoffset: title}
        self.keywords = self._parse_keywords()         # [Keyword]
        self.pictures: dict[str, Picture] = {}
        for fname in self.files:
            if fname.startswith("|bm"):
                try:
                    self.pictures[fname[1:]] = self._parse_picture(fname[1:], self.file(fname))
                except (HlpError, struct.error, IndexError) as e:
                    self.errors.append(f"{fname}: {e}")
        self.topics: list[Topic] = []
        self.topic_headers = 0      # topic-header records, the terminal empty one included
        self.end_offset: int | None = None
        self.terminal_record: int | None = None
        self._parse_topics()
        self._topic_offsets = [t.offset for t in self.topics]
        self.context_names: dict[int, str] = {}
        self._recover_context_names()
        self._attach()

    # ---- internal files

    def _raw_file(self, off: int) -> bytes:
        if off < 0 or off + FILEHEADER_SIZE > len(self.data):
            raise HlpError(f"internal file header at {off:#x} is outside the file")
        reserved, used = _i32(self.data, off), _i32(self.data, off + 4)
        start = off + FILEHEADER_SIZE
        if used < 0 or start + used > len(self.data):
            raise HlpError(f"internal file at {off:#x} (size {used}) runs past end of file")
        return self.data[start:start + used]

    def file(self, name: str) -> bytes:
        if name not in self.files:
            raise HlpError(f"no internal file {name}")
        return self._raw_file(self.files[name])

    def has(self, name: str) -> bool:
        return name in self.files

    # ---- B+ trees

    @staticmethod
    def _leaf_dir(b: bytes, o: int):
        s, o = _cstr(b, o)
        return (_dec(s), _i32(b, o)), o + 4

    def _btree_leaves(self, b: bytes, parse: Callable) -> Iterator:
        if len(b) < BTREE_HEADER_SIZE or _u16(b, 0) != BTREE_MAGIC:
            raise HlpError("bad B+ tree magic")
        page_size = _u16(b, 4)
        root = _i16(b, 26)
        total_pages = _i16(b, 30)
        levels = _i16(b, 32)
        page = root
        for _ in range(levels - 1):     # index pages: follow PreviousPage
            base = BTREE_HEADER_SIZE + page * page_size
            page = _i16(b, base + 4)
        seen = set()
        while page != -1:
            if page in seen or not 0 <= page < total_pages:
                self.errors.append(f"B+ tree leaf chain broken at page {page}")
                return
            seen.add(page)
            base = BTREE_HEADER_SIZE + page * page_size
            n = _i16(b, base + 2)
            nxt = _i16(b, base + 6)
            o = base + 8
            for _ in range(n):
                entry, o = parse(b, o)
                yield entry
            page = nxt

    # ---- |SYSTEM

    def _parse_system(self) -> SystemInfo:
        b = self.file("|SYSTEM")
        if _u16(b, 0) != SYSTEM_MAGIC:
            raise HlpError("bad |SYSTEM magic")
        si = SystemInfo(minor=_u16(b, 2), major=_u16(b, 4), gen_date=_u32(b, 6), flags=_u16(b, 10))
        if si.minor <= 16:
            raise HlpError(f"Windows 3.0 help file (|SYSTEM minor {si.minor}) is not supported")
        o = 12
        while o + 4 <= len(b):
            rtype, size = _u16(b, o), _u16(b, o + 2)
            d = b[o + 4:o + 4 + size]
            o += 4 + size
            if rtype == 1:
                si.title = _dec(_cstr(d, 0)[0])
            elif rtype == 2:
                si.copyright = _dec(_cstr(d, 0)[0])
            elif rtype == 3:
                si.contents = _i32(d, 0)
            elif rtype == 4:
                si.config.append(_dec(_cstr(d, 0)[0]))
            elif rtype == 5:
                si.icon = d
            elif rtype == 6 and len(d) >= 90:
                flags = _u16(d, 0)
                si.windows.append({
                    "flags": flags,
                    "type": _dec(_cstr(d[2:12], 0)[0]),
                    "name": _dec(_cstr(d[12:21], 0)[0]),
                    "caption": _dec(_cstr(d[21:72], 0)[0]),
                    "x": _i16(d, 72), "y": _i16(d, 74), "width": _i16(d, 76),
                    "height": _i16(d, 78), "maximize": _i16(d, 80),
                    "rgb": _u32(d, 82), "rgb_nsr": _u32(d, 86),
                })
            elif rtype == 8:
                si.citation = _dec(_cstr(d, 0)[0])
            else:
                si.other[rtype] = d
        if si.flags not in (0, 4, 8):
            self.errors.append(f"|SYSTEM: unknown flags {si.flags:#x}")
        return si

    # ---- |FONT

    def _parse_fonts(self) -> list[Font]:
        if not self.has("|FONT"):
            return []
        b = self.file("|FONT")
        n_faces, n_desc, face_off, desc_off = struct.unpack_from("<4H", b, 0)
        if face_off >= 12:
            self.errors.append("|FONT: multimedia font descriptors are not supported")
            return []
        face_len = (desc_off - face_off) // n_faces if n_faces else 0
        faces = [_dec(_cstr(b[face_off + i * face_len:face_off + (i + 1) * face_len], 0)[0])
                 for i in range(n_faces)]
        fonts = []
        for i in range(n_desc):
            o = desc_off + i * 11
            attr, hp, fam = b[o], b[o + 1], b[o + 2]
            face = _u16(b, o + 3)
            fonts.append(Font(attr, hp, fam, faces[face] if face < len(faces) else "",
                              (b[o + 5], b[o + 6], b[o + 7])))
        return fonts

    # ---- |CONTEXT, |CTXOMAP, |TTLBTREE, |KWBTREE/|KWDATA

    def _parse_context(self) -> list[tuple[int, int]]:
        if not self.has("|CONTEXT"):
            return []

        def leaf(b, o):
            return (_u32(b, o), _i32(b, o + 4)), o + 8
        return list(self._btree_leaves(self.file("|CONTEXT"), leaf))

    def _parse_ctxomap(self) -> list[tuple[int, int]]:
        if not self.has("|CTXOMAP"):
            return []
        b = self.file("|CTXOMAP")
        n = _u16(b, 0)
        return [(_i32(b, 2 + 8 * i), _i32(b, 6 + 8 * i)) for i in range(n)]

    def _parse_titles(self) -> dict[int, str]:
        if not self.has("|TTLBTREE"):
            return {}

        def leaf(b, o):
            off = _i32(b, o)
            s, o = _cstr(b, o + 4)
            return (off, _dec(s)), o
        return dict(self._btree_leaves(self.file("|TTLBTREE"), leaf))

    def _parse_keywords(self) -> list[Keyword]:
        if not (self.has("|KWBTREE") and self.has("|KWDATA")):
            return []
        data = self.file("|KWDATA")

        def leaf(b, o):
            s, o = _cstr(b, o)
            return (_dec(s), _i16(b, o), _i32(b, o + 2)), o + 6
        out = []
        for text, count, off in self._btree_leaves(self.file("|KWBTREE"), leaf):
            out.append(Keyword(text, [_i32(data, off + 4 * k) for k in range(count)]))
        return out

    # ---- pictures

    def _parse_picture(self, name: str, b: bytes) -> Picture:
        magic, count = _u16(b, 0), _u16(b, 2)
        if magic not in (0x506C, 0x706C):
            raise HlpError(f"bad picture magic {magic:#x}")
        if count < 1:
            raise HlpError("picture file holds no pictures")
        base = _u32(b, 4)  # first (usually only) resolution
        ptype, packing = b[base], b[base + 1]
        pic = Picture(name, ptype, packing, count=count)
        o = base + 2
        if ptype in (5, 6):
            pic.xdpi, o = cu32(b, o)
            pic.ydpi, o = cu32(b, o)
            pic.planes, o = cu16(b, o)
            pic.bit_count, o = cu16(b, o)
            pic.width, o = cu32(b, o)
            pic.height, o = cu32(b, o)
            pic.colors_used, o = cu32(b, o)
            pic.colors_important, o = cu32(b, o)
            csize, o = cu32(b, o)
            pic.hotspot_size, o = cu32(b, o)
            coff, hoff = _u32(b, o), _u32(b, o + 4)
            o += 8
            if ptype == 6 and pic.bit_count <= 8:
                ncolors = pic.colors_used or (1 << pic.bit_count)
                pic.palette = b[o:o + 4 * ncolors]
            raw = b[base + coff:base + coff + csize]
        elif ptype == 8:
            _mm, o = cu16(b, o)
            pic.width, pic.height = _u16(b, o), _u16(b, o + 2)
            o += 4
            _dsize, o = cu32(b, o)
            csize, o = cu32(b, o)
            pic.hotspot_size, o = cu32(b, o)
            coff = _u32(b, o)
            raw = b[base + coff:base + coff + csize]
            pic.note = "metafile (not converted)"
        else:
            raise HlpError(f"unknown picture type {ptype}")
        if packing in (2, 3):
            raw = lz77_expand(raw)
        if packing in (1, 3):
            raw = runlen_expand(raw)
        pic.bits = raw
        if ptype in (5, 6):
            stride = (((pic.width * pic.bit_count * max(pic.planes, 1)) + 31) // 32) * 4
            if ptype == 5:
                stride = (((pic.width * pic.bit_count * max(pic.planes, 1)) + 15) // 16) * 2
            want = stride * pic.height
            if len(raw) != want:
                self.errors.append(f"|{name}: expanded {len(raw)} bytes, expected {want}")
        if count > 1:
            pic.note = (pic.note + "; " if pic.note else "") + \
                f"{count} resolutions, first extracted"
        return pic

    # ---- |TOPIC

    def _topic_blocks(self) -> tuple[list[bytes], list[tuple[int, int, int]]]:
        """Block data (decompressed) and each block's 12-byte header."""
        t = self.file("|TOPIC")
        blocks, headers = [], []
        for start in range(0, len(t), self.block_size):
            chunk = t[start:start + self.block_size]
            if len(chunk) < TOPICBLOCKHEADER_SIZE:
                break
            headers.append(struct.unpack_from("<iii", chunk, 0))
            body = chunk[TOPICBLOCKHEADER_SIZE:]
            if self.lz77:
                body = lz77_expand(body, 0x4000)
            blocks.append(body)
        return blocks, headers

    def _parse_topics(self) -> None:
        if self.system.minor <= 16:
            return
        blocks, headers = self._topic_blocks()
        starts = []
        acc = 0
        for blk in blocks:
            starts.append(acc)
            acc += len(blk)
        data = b"".join(blocks)
        ds = 0x4000

        def to_off(pos: int) -> int:
            blk, inb = divmod(pos - TOPICBLOCKHEADER_SIZE, ds)
            if pos < TOPICBLOCKHEADER_SIZE or blk >= len(blocks) or inb > len(blocks[blk]):
                raise HlpError(f"TOPICPOS {pos:#x} is outside |TOPIC")
            return starts[blk] + inb

        # TOPICBLOCKHEADER = (LastTopicLink, FirstTopicLink, LastTopicHeader)
        pos = headers[0][1] if headers else TOPICBLOCKHEADER_SIZE
        topic_offset = 0
        topic: Topic | None = None
        seen = set()
        while True:
            if pos in seen:
                self.errors.append(f"|TOPIC: record loop at {pos:#x}")
                break
            seen.add(pos)
            try:
                o = to_off(pos)
            except HlpError as e:
                self.errors.append(str(e))
                break
            if o + TOPICLINK_SIZE > len(data):
                self.errors.append(f"|TOPIC: truncated record at {pos:#x}")
                break
            size, len2, _prev, nxt, len1 = struct.unpack_from("<iiiii", data, o)
            rtype = data[o + 20]
            if rtype == TL_TOPICHDR:
                self.topic_headers += 1
            if nxt <= 0:
                # The last TOPICLINK carries no data. In both Inner Space
                # manuals it is an empty topic header (no title, no text)
                # whose TOPICOFFSET ends the last real topic.
                self.end_offset = topic_offset
                self.terminal_record = rtype
                break
            if len1 < TOPICLINK_SIZE or size < len1 or o + size > len(data):
                self.errors.append(f"|TOPIC: bad record sizes at {pos:#x}")
                break
            ld1 = data[o + TOPICLINK_SIZE:o + len1]
            ld2 = data[o + len1:o + size]
            if len2 > size - len1:
                raise HlpError("phrase-compressed topic text is not supported")
            ld2 = ld2[:len2]
            if rtype == TL_TOPICHDR:
                _bs, bback, bfor, num, nonscroll, scroll, _nxt = struct.unpack_from("<iiiiiii", ld1, 0)
                strings = ld2.split(b"\0")
                topic = Topic(index=len(self.topics) + 1, number=num, offset=topic_offset, pos=pos,
                              title=_dec(strings[0]) if strings else "",
                              macros=[_dec(s) for s in strings[1:] if s],
                              browse_prev=bback, browse_next=bfor,
                              nonscroll_pos=nonscroll, scroll_pos=scroll)
                self.topics.append(topic)
            elif rtype in (TL_DISPLAY, TL_TABLE):
                _, q = ci32(ld1, 0)
                length, _ = cu16(ld1, q)
                topic_offset += length
                if topic is None:
                    self.errors.append(f"|TOPIC: display record before first topic header at {pos:#x}")
                else:
                    ns, sc = topic.nonscroll_pos, topic.scroll_pos
                    nonscroll = ns != -1 and ns <= pos and (sc == -1 or pos < sc)
                    try:
                        self._parse_display(topic, rtype, ld1, ld2, nonscroll)
                    except (HlpError, struct.error, IndexError) as e:
                        self.errors.append(f"topic {topic.index} record {pos:#x}: {e}")
            else:
                self.errors.append(f"|TOPIC: unknown record type {rtype:#x} at {pos:#x}")
            # crossing into another block restarts the character count
            if (nxt - 12) // ds != (pos - 12) // ds:
                topic_offset = ((nxt - 12) // ds) * 0x8000
            pos = nxt

    def _parse_display(self, topic: Topic, rtype: int, ld1: bytes, ld2: bytes,
                       nonscroll: bool) -> None:
        """Parse one display (0x20) or table (0x23) record into topic.blocks."""
        _size, p = ci32(ld1, 0)
        _length, p = cu16(ld1, p)
        if rtype == TL_TABLE:
            ncols, ttype = ld1[p], ld1[p + 1]
            p += 2
            if ttype in (0, 2):
                p += 2
            p += 4 * ncols
        strings = ld2.split(b"\0")
        si = 0
        row: TableRow | None = TableRow(nonscroll=nonscroll) if rtype == TL_TABLE else None
        last_col = None
        link: Link | None = None
        font = -1
        end = len(ld1)
        while p < end:
            col = None
            if rtype == TL_TABLE:
                col = _i16(ld1, p)
                if col == -1:
                    break
                p += 5
            p += 4
            bits = _u16(ld1, p)
            p += 2
            para = Paragraph(nonscroll=nonscroll)
            if bits & 0x0400:
                para.align = "right"
            if bits & 0x0800:
                para.align = "center"
            if bits & 0x0001:
                _, p = ci32(ld1, p)
            vals = {}
            for flag, name in ((0x02, "sb"), (0x04, "sa"), (0x08, "sl"),
                               (0x10, "li"), (0x20, "ri"), (0x40, "fi")):
                if bits & flag:
                    vals[name], p = ci16(ld1, p)
            para.left_indent = vals.get("li", 0)
            para.first_indent = vals.get("fi", 0)
            if bits & 0x0100:
                p += 3
            if bits & 0x0200:
                ntabs, p = ci16(ld1, p)
                for _ in range(max(ntabs, 0)):
                    stop, p = cu16(ld1, p)
                    if stop & 0x4000:
                        _, p = cu16(ld1, p)
                    para.tabs.append(stop & 0x3FFF)
            # cell bookkeeping
            if row is not None:
                if col != last_col or not row.cells:
                    row.cells.append([])
                last_col = col

            def emit(pa: Paragraph) -> None:
                if row is not None:
                    row.cells[-1].append(pa)
                else:
                    topic.blocks.append(pa)

            cur = para
            while True:
                s = strings[si] if si < len(strings) else b""
                si += 1
                if s:
                    cur.runs.append(Run("text", _dec(s), font, link))
                if p >= end:
                    raise HlpError("character formatting ran past LinkData1")
                cmd = ld1[p]
                if cmd == 0xFF:
                    p += 1
                    break
                if cmd == 0x20:
                    p += 5
                elif cmd == 0x21:
                    p += 3
                elif cmd == 0x80:
                    font = _i16(ld1, p + 1)
                    p += 3
                elif cmd == 0x81:
                    cur.runs.append(Run("br"))
                    p += 1
                elif cmd == 0x82:
                    emit(cur)
                    cur = Paragraph(nonscroll=nonscroll, left_indent=para.left_indent,
                                    first_indent=para.first_indent, tabs=para.tabs, align=para.align)
                    p += 1
                elif cmd == 0x83:
                    cur.runs.append(Run("tab", font=font, link=link))
                    p += 1
                elif cmd in (0x86, 0x87, 0x88):
                    ptype = ld1[p + 1]
                    p += 2
                    psize, p = ci32(ld1, p)
                    if ptype == 0x22:
                        _nhot, p = cu16(ld1, p)
                    start = p
                    align = {0x86: "c", 0x87: "l", 0x88: "r"}[cmd]
                    if ptype in (3, 0x22):
                        embedded = _u16(ld1, p)
                        if embedded == 0:
                            name = f"bm{_u16(ld1, p + 2)}"
                            cur.runs.append(Run("image", name, font, link, align))
                            if name not in topic.bitmaps:
                                topic.bitmaps.append(name)
                        else:
                            cur.runs.append(Run("embed", "embedded picture (not extracted)",
                                                font, link, align))
                            self.errors.append(f"topic {topic.index}: embedded picture not extracted")
                    elif ptype == 5:
                        s5, _ = _cstr(ld1, p + 6)
                        cur.runs.append(Run("embed", _dec(s5), font, link, align))
                    else:
                        self.errors.append(f"topic {topic.index}: unknown picture type {ptype:#x}")
                    p = start + psize
                elif cmd == 0x89:
                    link = None
                    p += 1
                elif cmd == 0x8B:
                    cur.runs.append(Run("text", " ", font, link))
                    p += 1
                elif cmd == 0x8C:  # non-breaking hyphen
                    cur.runs.append(Run("text", "-", font, link))
                    p += 1
                elif cmd in (0xC8, 0xCC):
                    mlen = _i16(ld1, p + 1)
                    macro, _ = _cstr(ld1, p + 3)
                    link = Link("macro", _dec(macro))
                    p += 3 + mlen
                elif cmd in (0xE0, 0xE1):
                    link = Link("popup" if cmd == 0xE0 else "jump", f"TOPIC{_i32(ld1, p + 1)}")
                    p += 5
                elif cmd in (0xE2, 0xE3, 0xE6, 0xE7):
                    h = _u32(ld1, p + 1)
                    link = Link("popup" if cmd in (0xE2, 0xE6) else "jump", hash=h)
                    p += 5
                elif cmd in (0xEA, 0xEB, 0xEE, 0xEF):
                    sz = _i16(ld1, p + 1)
                    kind = ld1[p + 3]
                    h = _u32(ld1, p + 4)
                    ext = ""
                    if kind in (4, 6):
                        f1, q = _cstr(ld1, p + 8)
                        ext = "@" + _dec(f1)
                        if kind == 6:
                            w, _ = _cstr(ld1, q)
                            ext = ">" + _dec(f1) + "@" + _dec(w)
                    elif kind == 1:
                        ext = f">window{ld1[p + 8]}"
                    link = Link("external", ext, hash=h)
                    p += 3 + sz
                else:
                    raise HlpError(f"unknown formatting command {cmd:#x} at {p}")
            # a group that ends without 0x82 still closes its paragraph
            if cur.runs:
                emit(cur)
        if row is not None and row.cells:
            topic.blocks.append(row)

    # ---- context names, cross references

    def _recover_context_names(self) -> None:
        """Map context hashes to names by hashing candidate strings."""
        wanted = {h for h, _ in self.contexts}
        wanted |= {k.hash for t in self.topics for k in t.links() if k.hash is not None}
        candidates: list[str] = [t.title for t in self.topics]
        candidates += list(self.titles.values())
        candidates += [k.text for k in self.keywords]
        for t in self.topics:
            for para in t.paragraphs():
                text = "".join(r.text for r in para.runs if r.kind == "text" and r.link)
                if text:
                    candidates.append(text)
                first = para.plain().strip()
                if 0 < len(first) < 60:
                    candidates.append(first)
        names: dict[int, str] = {}
        for c in candidates:
            for variant in _name_variants(c):
                for prefix in ("", "idh_", "helpid_", "IDH_"):
                    h = context_hash(prefix + variant)
                    if h in wanted and h not in names:
                        names[h] = prefix + variant
        self.context_names = names

    def context_name(self, h: int) -> str:
        return self.context_names.get(h, f"#{h:08X}")

    def topic_at(self, offset: int) -> Topic | None:
        """Topic containing TOPICOFFSET ``offset`` (None for -1 or out of range)."""
        if offset is None or offset < 0 or not self.topics:
            return None
        if self.end_offset is not None and offset >= self.end_offset:
            return None
        i = bisect.bisect_right(self._topic_offsets, offset) - 1
        return self.topics[i] if i >= 0 else None

    def topic_for_hash(self, h: int) -> Topic | None:
        for hh, off in self.contexts:
            if hh == h:
                return self.topic_at(off)
        return None

    def _attach(self) -> None:
        for h, off in self.contexts:
            t = self.topic_at(off)
            if t is not None:
                t.contexts.append(self.context_name(h))
            else:
                self.errors.append(f"|CONTEXT {self.context_name(h)} points outside any topic")
        for mid, off in self.ctxomap:
            t = self.topic_at(off)
            if t is not None:
                t.map_ids.append(mid)
        for kw in self.keywords:
            for off in kw.offsets:
                t = self.topic_at(off)
                if t is not None and kw.text not in t.keywords:
                    t.keywords.append(kw.text)
        for t in self.topics:
            for link in t.links():
                if link.kind == "external":
                    link.target = self.context_name(link.hash) + link.target
                elif link.hash is not None:
                    link.target = self.context_name(link.hash)
            for name in t.bitmaps:
                if name not in self.pictures:
                    self.errors.append(f"topic {t.index}: |{name} is referenced but missing")


def _name_variants(s: str) -> list[str]:
    s = s.strip()
    if not s:
        return []
    out = []
    a = _LEGAL_CTX.sub("", s.replace(" ", "_"))
    out.append(a)
    out.append(_LEGAL_CTX.sub("", s))
    out.append(re.sub(r"_+", "_", _LEGAL_CTX.sub("_", s)).strip("_"))
    if ":" in s:
        out += _name_variants(s.split(":", 1)[1])
    return [v for v in dict.fromkeys(out) if v]


# --------------------------------------------------------------------------
# Markdown output

_SYMBOL_MAP = {0xB7: "\u2022", 0xAE: "\u2192", 0xAC: "\u2190", 0xAD: "\u2191", 0xAF: "\u2193",
               0xD7: "\u22c5", 0xB0: "\u00b0", 0xB4: "\u00d7", 0xA5: "\u221e"}
_WINGDINGS_MAP = {0x9F: "\u2022", 0x6C: "\u25cf", 0x6E: "\u25a0", 0xA7: "\u25aa",
                  0xD8: "\u27a2", 0xE8: "\u2794", 0xFC: "\u2714", 0x4A: "\u263a"}
_SEP = " \u00b7 "


def slugify(s: str) -> str:
    s = re.sub(r"[^0-9a-z]+", "-", s.lower()).strip("-")
    return s or "topic"


def _md_escape(s: str) -> str:
    """Escape Markdown syntax, leaving text greppable where Markdown allows it.

    Brackets, angle brackets and backslashes are escaped only where CommonMark
    would read them as syntax, so ``[Inner Space]``, ``<520`` and ``C:\\SPACE``
    stay as written.
    """
    s = re.sub(r"\\(?=[!-/:-@\[-`{-~])", r"\\\\", s)
    s = re.sub(r"([*`])", r"\\\1", s)
    s = re.sub(r"\[(?=[^\]]*\][(\[:])", r"\\[", s)
    s = re.sub(r"<(?=[A-Za-z/!?])", r"\\<", s)
    s = re.sub(r"^([>#+-])", r"\\\1", s)
    s = re.sub(r"(?<![0-9A-Za-z])_|_(?![0-9A-Za-z])", r"\\_", s)
    return s


class MarkdownWriter:
    def __init__(self, hf: HelpFile):
        self.hf = hf
        self.files = {t.index: f"{t.index:02d}-{slugify(self.display_title(t))}.md" for t in hf.topics}

    @staticmethod
    def display_title(t: Topic) -> str:
        """The topic title, or for an untitled topic its first line of text."""
        if t.title:
            return t.title
        for b in t.blocks:
            line = b.plain().strip().split("\n")[0].strip()
            if line:
                return line[:60]
        return "untitled"

    # ---- runs -> text

    def _font(self, i: int) -> Font | None:
        return self.hf.fonts[i] if 0 <= i < len(self.hf.fonts) else None

    def _run_text(self, r: Run) -> str:
        f = self._font(r.font)
        text = r.text
        if f and f.face.lower() in ("symbol", "wingdings"):
            table = _SYMBOL_MAP if f.face.lower() == "symbol" else _WINGDINGS_MAP
            text = "".join(table.get(ord(c) if ord(c) < 256 else 0,
                                     c if c.isalnum() or c == " " else f"[{f.face} {ord(c):#04x}]")
                           for c in r.text.encode("cp1252", errors="replace").decode("latin-1"))
        return text

    def _link_href(self, link: Link) -> tuple[str, str]:
        if link.kind == "macro":
            return "", link.target
        if link.kind == "external":
            return "", link.target
        t = self.hf.topic_for_hash(link.hash) if link.hash is not None else None
        title = ' "popup"' if link.kind == "popup" else ""
        if t is None:
            return f"#{link.target}", title + (' "unresolved"' if not title else "")
        return self.files[t.index], title

    def _segments(self, runs: list[Run]) -> list[tuple[str, bool, bool, Link | None]]:
        """Merge runs into (text, bold, italic, link) segments; tabs and breaks as text."""
        segs: list[list] = []
        for r in runs:
            f = self._font(r.font)
            bold = bool(f and f.bold)
            ital = bool(f and f.italic)
            if r.kind == "text":
                txt = self._run_text(r)
            elif r.kind == "tab":
                txt = "\t"
            elif r.kind == "br":
                txt = "\n"
            elif r.kind == "image":
                txt = f"\x00IMG{r.text}\x00"
            else:
                txt = f"\x00EMB{r.text}\x00"
            if segs and segs[-1][1] == bold and segs[-1][2] == ital and segs[-1][3] is r.link:
                segs[-1][0] += txt
            else:
                segs.append([txt, bold, ital, r.link])
        return [tuple(s) for s in segs]

    def _render_inline(self, runs: list[Run], plain_bold: bool, plain_ital: bool) -> str:
        out = []
        for text, bold, ital, link in self._segments(runs):
            body = self._inline_text(text)
            if link is not None:
                lead = body[:len(body) - len(body.lstrip())]
                trail = body[len(body.rstrip()):]
                core = body.strip()
                if not core:
                    out.append(body)
                    continue
                href, title = self._link_href(link)
                core = core.replace("[", "\\[").replace("]", "\\]")
                if href:
                    out.append(f"{lead}[{core}]({href}{title}){trail}")
                elif link.kind == "macro":
                    out.append(f"{lead}{core} (macro `{link.target}`){trail}")
                else:
                    out.append(f"{lead}{core} (external `{link.target}`){trail}")
                continue
            mark = ""
            if bold and not plain_bold:
                mark += "**"
            if ital and not plain_ital:
                mark += "*"
            if mark and body.strip() and "\x00" not in text:
                lead = body[:len(body) - len(body.lstrip())]
                trail = body[len(body.rstrip()):]
                out.append(f"{lead}{mark}{body.strip()}{mark[::-1]}{trail}")
            else:
                out.append(body)
        return "".join(out)

    def _inline_text(self, text: str) -> str:
        parts = text.split("\x00")
        out = []
        for i, part in enumerate(parts):
            if i % 2 == 0:
                out.append(_md_escape(part))
            elif part.startswith("IMG"):
                name = part[3:]
                out.append(f"![{name}]({name}.bmp)")
            else:
                out.append(f"`{{{part[3:]}}}`")
        return "".join(out)

    # ---- blocks

    def _para_style(self, para: Paragraph) -> tuple[int, bool, bool]:
        """(smallest half-points, all bold, all italic) over the paragraph's visible text."""
        sizes, bolds, itals = [], [], []
        for r in para.runs:
            if r.kind == "text" and r.text.strip():
                f = self._font(r.font)
                sizes.append(f.half_points if f else 0)
                bolds.append(bool(f and f.bold))
                itals.append(bool(f and f.italic))
        if not sizes:
            return 0, False, False
        return min(sizes), all(bolds), all(itals)

    def _body_size(self, topic: Topic) -> int:
        count: dict[int, int] = {}
        for para in topic.paragraphs():
            for r in para.runs:
                if r.kind == "text":
                    f = self._font(r.font)
                    hp = f.half_points if f else 0
                    count[hp] = count.get(hp, 0) + len(r.text.strip())
        return max(count, key=lambda k: count[k]) if count else 0

    def topic_markdown(self, t: Topic) -> str:
        hf = self.hf
        lines = [f"# {t.title or '(untitled topic) ' + self.display_title(t)}", ""]
        meta = [f"Topic {t.index} of {len(hf.topics)}"]
        if t.contexts:
            meta.append("context " + ", ".join(f"`{c}`" for c in t.contexts))
        if t.map_ids:
            meta.append("help map id " + ", ".join(str(m) for m in t.map_ids))
        lines.append("*" + _SEP.join(meta) + "*")
        if t.keywords:
            lines.append("")
            lines.append("*Keywords: " + "; ".join(_md_escape(k) for k in t.keywords) + "*")
        nav = []
        for label, off in (("previous", t.browse_prev), ("next", t.browse_next)):
            o = hf.topic_at(off) if off != -1 else None
            if o is not None and o is not t:
                nav.append(f"browse {label}: [{_md_escape(o.title)}]({self.files[o.index]})")
        if nav:
            lines.append("")
            lines.append("*" + _SEP.join(nav) + "*")
        if t.macros:
            lines.append("")
            lines.append("*Entry macros: " + "; ".join(f"`{m}`" for m in t.macros) + "*")
        lines.append("")
        body = self._blocks_markdown(t)
        lines += body
        text = "\n".join(lines).rstrip() + "\n"
        return re.sub(r"\n{3,}", "\n\n", text)

    def _blocks_markdown(self, t: Topic) -> list[str]:
        body_size = self._body_size(t)
        out: list[str] = []
        table: list[list[str]] = []

        def flush_table() -> None:
            if not table:
                return
            width = max(len(r) for r in table)
            out.append("")
            out.append("|" + " |" * width)
            out.append("|" + "---|" * width)
            for r in table:
                cells = [c.replace("|", "\\|") for c in r] + [""] * (width - len(r))
                out.append("| " + " | ".join(cells) + " |")
            out.append("")
            table.clear()

        for b in t.blocks:
            if isinstance(b, TableRow):
                flush_table()
                cells = []
                for cell in b.cells:
                    txt = "<br>".join(self._render_inline(p.runs, False, False).strip()
                                      for p in cell if p.runs)
                    cells.append(txt.replace("\t", " ").replace("\n", "<br>"))
                table.append(cells)
                continue
            para = b
            plain = para.plain()
            if not plain.strip() and not any(r.kind in ("image", "embed") for r in para.runs):
                flush_table()
                continue
            size, all_bold, all_ital = self._para_style(para)
            text = self._render_inline(para.runs, all_bold, all_ital)
            # heading: a short paragraph in a font larger than the body text
            if body_size and size > body_size and len(plain.strip()) < 60 and "\t" not in plain \
                    and not any(r.link for r in para.runs):
                flush_table()
                head = text.strip().replace("\n", " ")
                if not out and plain.strip().lower() == t.title.strip().lower():
                    continue  # the in-text copy of the topic title
                level = "##" if size >= 28 or para.nonscroll else "###"
                out += ["", f"{level} {head}", ""]
                continue
            lines = text.split("\n")
            tabbed = [ln for ln in lines if "\t" in ln.strip("\t ")]
            if tabbed:
                for ln in lines:
                    core = ln.strip()
                    if not core:
                        continue
                    cells = [c.strip() for c in re.split(r"\t+", ln.strip(" \t"))]
                    table.append(cells)
                continue
            flush_table()
            mark = ""
            if not any(r.link for r in para.runs):
                mark = ("**" if all_bold else "") + ("*" if all_ital else "")
            md_lines = []
            for ln in lines:
                lead_tabs = len(ln) - len(ln.lstrip("\t"))
                ln = ln.strip("\t ").rstrip()
                if mark and ln:
                    ln = f"{mark}{ln}{mark[::-1]}"
                if lead_tabs:
                    ln = "&emsp;" * lead_tabs + ln
                md_lines.append(ln)
            # Markdown has no paragraph alignment or indents; text is kept flush left.
            out += ["  \n".join(md_lines).strip(), ""]
        flush_table()
        return out

    # ---- index

    def index_markdown(self) -> str:
        hf, si = self.hf, self.hf.system
        L = [f"# {si.title or hf.name}", ""]
        L.append(f"Extracted from `{hf.name}` by `isre hlp`.")
        L.append("")
        L.append("## |SYSTEM")
        L.append("")
        L.append("| Field | Value |")
        L.append("|---|---|")
        L.append(f"| Format | WinHelp, minor {si.minor}, major {si.major} (minor 21 = HC31) |")
        L.append(f"| Flags | {si.flags:#x} ({'LZ77' if hf.lz77 else 'not compressed'}, "
                 f"{hf.block_size}-byte topic blocks) |")
        L.append(f"| Build date | {si.build_date or '(none)'} |")
        L.append(f"| Title | {_md_escape(si.title)} |")
        if si.copyright:
            L.append(f"| Copyright | {_md_escape(si.copyright)} |")
        if si.citation:
            L.append(f"| Citation | {_md_escape(si.citation)} |")
        if si.contents is not None:
            ct = hf.topic_at(si.contents)
            L.append(f"| Contents topic | {self._topic_ref(ct) if ct else hex(si.contents)} |")
        for m in si.config:
            L.append(f"| Config macro | `{m}` |")
        for w in si.windows:
            L.append(f"| Window | {_window_text(w)} |")
        for k, v in sorted(si.other.items()):
            L.append(f"| Record type {k} | {len(v)} bytes |")
        L.append(f"| Topics | {len(hf.topics)} ({hf.topic_headers} topic-header records; the last is "
                 f"the empty terminal record) |" if hf.topic_headers != len(hf.topics)
                 else f"| Topics | {len(hf.topics)} |")
        L.append(f"| Pictures | {len(hf.pictures)} |")
        L.append(f"| Parse errors | {len(hf.errors)} |")
        L.append("")
        L.append("## Topics")
        L.append("")
        L.append("| # | Topic | Context ids | Help map ids |")
        L.append("|---|---|---|---|")
        for t in hf.topics:
            ctx = ", ".join(f"`{c}`" for c in t.contexts)
            mids = ", ".join(str(m) for m in t.map_ids)
            L.append(f"| {t.index} | {self._topic_ref(t)} | {ctx} | {mids} |")
        L.append("")
        if hf.ctxomap:
            L.append("## Help context map (|CTXOMAP)")
            L.append("")
            L.append("Numeric ids an application passes to `WinHelp(..., HELP_CONTEXT, id)`.")
            L.append("")
            L.append("| Map id | Topic |")
            L.append("|---|---|")
            for mid, off in sorted(hf.ctxomap):
                t = hf.topic_at(off)
                L.append(f"| {mid} | {self._topic_ref(t) if t else f'(no topic: offset {off})'} |")
            L.append("")
        if hf.keywords:
            L.append("## Keyword index (|KWBTREE)")
            L.append("")
            for kw in hf.keywords:
                refs = []
                for off in kw.offsets:
                    t = hf.topic_at(off)
                    refs.append(self._topic_ref(t) if t else f"offset {off:#x}")
                L.append(f"- {_md_escape(kw.text)}: " + ", ".join(refs))
            L.append("")
        if hf.pictures:
            L.append("## Pictures")
            L.append("")
            L.append("| Name | Type | Size | Bits | Packing | Used in | Note |")
            L.append("|---|---|---|---|---|---|---|")
            for name in sorted(hf.pictures, key=_natural):
                p = hf.pictures[name]
                users = [self._topic_ref(t) for t in hf.topics if name in t.bitmaps]
                ptype = {5: "DDB", 6: "DIB", 8: "metafile"}.get(p.ptype, str(p.ptype))
                pack = {0: "none", 1: "RunLen", 2: "LZ77", 3: "LZ77+RunLen"}.get(p.packing, "?")
                link = f"[{name}.bmp]({name}.bmp)" if p.ptype in (5, 6) else name
                L.append(f"| {link} | {ptype} | {p.width}x{p.height} | {p.bit_count} | {pack} | "
                         f"{', '.join(users)} | {p.note} |")
            L.append("")
        L.append("## Internal files")
        L.append("")
        L.append("| Name | Offset | Size |")
        L.append("|---|---|---|")
        for name, off in self.hf.files.items():
            cell = name.replace("|", "\\|")  # GFM tables need \| even inside code spans
            L.append(f"| `{cell}` | {off:#x} | {len(self.hf._raw_file(off))} |")
        L.append("")
        if hf.errors:
            L.append("## Parse errors")
            L.append("")
            L += [f"- {_md_escape(e)}" for e in hf.errors]
            L.append("")
        return "\n".join(L).rstrip() + "\n"

    def _topic_ref(self, t: Topic | None) -> str:
        if t is None:
            return "(none)"
        return f"[{_md_escape(t.title or '(untitled)')}]({self.files[t.index]})"


def _window_text(w: dict) -> str:
    """A |SYSTEM window record, listing only the fields its flags mark valid."""
    f = w["flags"]
    parts = []
    for bit, key in ((0x01, "type"), (0x02, "name"), (0x04, "caption")):
        if f & bit:
            parts.append(f"{key} \"{_md_escape(w[key])}\"")
    for bit, key in ((0x08, "x"), (0x10, "y"), (0x20, "width"), (0x40, "height")):
        if f & bit:
            parts.append(f"{key} {w[key]}")
    if f & 0x80:
        parts.append(f"maximize {w['maximize']:#x}")
    if f & 0x100:
        parts.append(f"rgb {w['rgb']:#08x}")
    if f & 0x200:
        parts.append(f"rgb_nsr {w['rgb_nsr']:#08x}")
    if f & 0x400:
        parts.append("on top")
    if f & 0x800:
        parts.append("auto-size height")
    return f"flags {f:#x}: " + ", ".join(parts)


def _natural(s: str):
    return [int(x) if x.isdigit() else x for x in re.split(r"(\d+)", s)]


def _write(path: Path, data: bytes) -> bool:
    try:
        if path.is_file() and path.read_bytes() == data:
            return False
    except OSError:
        pass
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return True


def extract(hf: HelpFile, out_dir: str | Path) -> list[Path]:
    """Write index.md, the topic files and the bitmaps. Returns the paths written or kept."""
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    w = MarkdownWriter(hf)
    paths = []
    for t in hf.topics:
        p = out / w.files[t.index]
        _write(p, w.topic_markdown(t).encode("utf-8"))
        paths.append(p)
    for name, pic in sorted(hf.pictures.items()):
        bmp = pic.to_bmp()
        if bmp is not None:
            p = out / f"{name}.bmp"
        else:
            p = out / f"{name}.bin"
            bmp = pic.bits
        _write(p, bmp)
        paths.append(p)
    p = out / "index.md"
    _write(p, w.index_markdown().encode("utf-8"))
    paths.append(p)
    # Remove topic and picture files left by an earlier run (other files stay).
    expected = {q.name for q in paths}
    ours = re.compile(r"(\d{2,}-[0-9a-z-]+\.md|bm\d+\.(bmp|bin))$")
    for stale in out.iterdir():
        if stale.is_file() and ours.match(stale.name) and stale.name not in expected:
            stale.unlink()
    return paths


def open_help(path: str | Path) -> HelpFile:
    return HelpFile(load_bytes(path), Path(path).name)


def summary(hf: HelpFile) -> str:
    si = hf.system
    lines = [f"{hf.name}: {si.title!r}, WinHelp minor {si.minor}, flags {si.flags:#x}, "
             f"built {si.build_date}",
             f"  {len(hf.topics)} topics, {len(hf.pictures)} pictures, {len(hf.contexts)} context ids, "
             f"{len(hf.ctxomap)} map ids, {len(hf.keywords)} keywords, {len(hf.errors)} parse errors"]
    for t in hf.topics:
        lines.append(f"  {t.index:3d}  {t.title}")
    for e in hf.errors:
        lines.append(f"  error: {e}")
    return "\n".join(lines)


def main(argv: Sequence[str] | None = None) -> int:
    ap = argparse.ArgumentParser(prog="isre hlp", description="Extract a WinHelp 3.1 file to Markdown.")
    ap.add_argument("file", help="help file (.HLP, or SZDD-compressed .hl1)")
    ap.add_argument("out", nargs="?", help="output directory; omit to print a summary")
    a = ap.parse_args(list(sys.argv[1:] if argv is None else argv))
    try:
        hf = open_help(a.file)
    except (HlpError, OSError, struct.error) as e:
        print(f"isre hlp: {a.file}: {e}", file=sys.stderr)
        return 2
    if a.out:
        paths = extract(hf, a.out)
        print(f"{a.file} -> {a.out}: {len(hf.topics)} topics, {len(hf.pictures)} pictures, "
              f"{len(paths)} files, {len(hf.errors)} parse errors")
        for e in hf.errors:
            print(f"  error: {e}", file=sys.stderr)
    else:
        print(summary(hf))
    return 1 if hf.errors else 0


if __name__ == "__main__":
    sys.exit(main())
