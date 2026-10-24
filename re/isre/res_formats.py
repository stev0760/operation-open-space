"""Decoders for Win16 NE resource formats (not the Win32 PE layouts).

Every function takes the raw resource bytes as stored in the NE file. NE
resource lengths are rounded up to the resource alignment unit, so the data
usually carries trailing padding; decoders find the real end themselves.

Formats, as Windows 3.1 defines them [REF Win16 SDK; Wine user.exe16/gdi.exe16]:

    STRINGTABLE  16 strings per block, each a length byte + cp1252 text;
                 block id = (string id >> 4) + 1.
    DIALOG       DLGTEMPLATE: style DWORD, item count BYTE, x y cx cy WORDs,
                 menu (0 | 0xFF+WORD | ASCIIZ), class ASCIIZ, caption ASCIIZ,
                 point size WORD + face ASCIIZ when DS_SETFONT; then items:
                 x y cx cy id WORDs, style DWORD, class (0x80..0x85 | ASCIIZ),
                 text (0xFF+WORD | ASCIIZ), extra-byte count BYTE + bytes.
    MENU         header (version WORD, offset WORD), then items: flags WORD,
                 id WORD unless MF_POPUP, ASCIIZ text; MF_END (0x80) closes a level.
    ACCELERATOR  5-byte entries: flags BYTE, key WORD, command WORD; flag 0x80
                 marks the last entry.
    VERSION      VS_VERSION_INFO nodes without wType: length WORD, value length
                 WORD, ASCIIZ key, DWORD padding, value, DWORD padding, children.
    BITMAP       a packed DIB (BITMAPINFOHEADER or BITMAPCOREHEADER, palette, bits).
    ICON/CURSOR  a packed DIB with an AND mask (a cursor starts with the hotspot);
                 GROUP_ICON / GROUP_CURSOR are directories naming them by id.
"""
from __future__ import annotations

import struct
from typing import Any, Callable

# ---------------------------------------------------------------- text

# The five byte values cp1252 leaves undefined map to the C1 controls, as
# Windows' MultiByteToWideChar does.
_CP1252_UNDEFINED = {0x81, 0x8D, 0x8F, 0x90, 0x9D}


def decode_cp1252(b: bytes) -> str:
    if not any(x in _CP1252_UNDEFINED for x in b):
        return b.decode("cp1252")
    return "".join(chr(x) if x in _CP1252_UNDEFINED else bytes([x]).decode("cp1252") for x in b)


def _asciiz(d: bytes, p: int) -> tuple[bytes, int]:
    """Return the NUL-terminated bytes at ``p`` and the offset after the NUL."""
    end = d.find(b"\0", p)
    if end < 0:
        raise ValueError(f"unterminated string at {p:#x}")
    return d[p:end], end + 1


def _printable(b: bytes) -> bool:
    return all(x >= 0x20 or x in (0x09, 0x0A, 0x0D) for x in b) and 0x7F not in b


def text_payload(data: bytes, align: int) -> bytes | None:
    """The text before the first NUL when ``data`` is a NUL-terminated text resource.

    Bytes after the terminator are tolerated only inside the last alignment
    unit, where the linker leaves stale memory as padding.
    """
    nul = data.find(b"\0")
    text = data if nul < 0 else data[:nul]
    if not text or not _printable(text):
        return None
    if nul >= 0 and len(data) - nul > max(align, 1):
        return None
    return text


def escape_tsv(s: str) -> str:
    return s.replace("\\", "\\\\").replace("\t", "\\t").replace("\r", "\\r").replace("\n", "\\n")


# ---------------------------------------------------------------- string tables

def decode_string_block(block_id: int, data: bytes) -> list[tuple[int, str]]:
    """Decode one STRINGTABLE block into (string id, text); empty slots are absent."""
    if not isinstance(block_id, int) or block_id < 1:
        raise ValueError(f"string block id must be a positive integer, got {block_id!r}")
    out = []
    p = 0
    base = (block_id - 1) << 4
    for i in range(16):
        if p >= len(data):
            raise ValueError(f"string block {block_id} truncated at slot {i}")
        n = data[p]
        if p + 1 + n > len(data):
            raise ValueError(f"string block {block_id} slot {i} overruns the resource")
        if n:
            out.append((base + i, decode_cp1252(data[p + 1:p + 1 + n])))
        p += 1 + n
    return out


# ---------------------------------------------------------------- style names

def _bits(value: int, table: list[tuple[int, str]]) -> list[str]:
    return [name for bit, name in table if value & bit == bit and bit]


WS_COMMON = [
    (0x80000000, "WS_POPUP"), (0x40000000, "WS_CHILD"), (0x20000000, "WS_MINIMIZE"),
    (0x10000000, "WS_VISIBLE"), (0x08000000, "WS_DISABLED"), (0x04000000, "WS_CLIPSIBLINGS"),
    (0x02000000, "WS_CLIPCHILDREN"), (0x01000000, "WS_MAXIMIZE"),
]
WS_FRAME = [(0x00800000, "WS_BORDER"), (0x00400000, "WS_DLGFRAME")]
WS_REST = [
    (0x00200000, "WS_VSCROLL"), (0x00100000, "WS_HSCROLL"), (0x00080000, "WS_SYSMENU"),
    (0x00040000, "WS_THICKFRAME"),
]
WS_CONTROL = [(0x00020000, "WS_GROUP"), (0x00010000, "WS_TABSTOP")]
WS_TOPLEVEL = [(0x00020000, "WS_MINIMIZEBOX"), (0x00010000, "WS_MAXIMIZEBOX")]
DS_NAMES = [
    (0x01, "DS_ABSALIGN"), (0x02, "DS_SYSMODAL"), (0x20, "DS_LOCALEDIT"),
    (0x40, "DS_SETFONT"), (0x80, "DS_MODALFRAME"), (0x100, "DS_NOIDLEMSG"),
]
DS_SETFONT = 0x40

BS_TYPES = {0: "BS_PUSHBUTTON", 1: "BS_DEFPUSHBUTTON", 2: "BS_CHECKBOX", 3: "BS_AUTOCHECKBOX",
            4: "BS_RADIOBUTTON", 5: "BS_3STATE", 6: "BS_AUTO3STATE", 7: "BS_GROUPBOX",
            8: "BS_USERBUTTON", 9: "BS_AUTORADIOBUTTON", 11: "BS_OWNERDRAW"}
SS_TYPES = {0: "SS_LEFT", 1: "SS_CENTER", 2: "SS_RIGHT", 3: "SS_ICON", 4: "SS_BLACKRECT",
            5: "SS_GRAYRECT", 6: "SS_WHITERECT", 7: "SS_BLACKFRAME", 8: "SS_GRAYFRAME",
            9: "SS_WHITEFRAME", 10: "SS_USERITEM", 11: "SS_SIMPLE", 12: "SS_LEFTNOWORDWRAP"}
ES_TYPES = {0: "ES_LEFT", 1: "ES_CENTER", 2: "ES_RIGHT"}
ES_BITS = [(0x4, "ES_MULTILINE"), (0x8, "ES_UPPERCASE"), (0x10, "ES_LOWERCASE"),
           (0x20, "ES_PASSWORD"), (0x40, "ES_AUTOVSCROLL"), (0x80, "ES_AUTOHSCROLL"),
           (0x100, "ES_NOHIDESEL"), (0x400, "ES_OEMCONVERT"), (0x800, "ES_READONLY"),
           (0x1000, "ES_WANTRETURN")]
LBS_BITS = [(0x1, "LBS_NOTIFY"), (0x2, "LBS_SORT"), (0x4, "LBS_NOREDRAW"),
            (0x8, "LBS_MULTIPLESEL"), (0x10, "LBS_OWNERDRAWFIXED"), (0x20, "LBS_OWNERDRAWVARIABLE"),
            (0x40, "LBS_HASSTRINGS"), (0x80, "LBS_USETABSTOPS"), (0x100, "LBS_NOINTEGRALHEIGHT"),
            (0x200, "LBS_MULTICOLUMN"), (0x400, "LBS_WANTKEYBOARDINPUT"),
            (0x800, "LBS_EXTENDEDSEL"), (0x1000, "LBS_DISABLENOSCROLL")]
CBS_TYPES = {0: None, 1: "CBS_SIMPLE", 2: "CBS_DROPDOWN", 3: "CBS_DROPDOWNLIST"}
CBS_BITS = [(0x10, "CBS_OWNERDRAWFIXED"), (0x20, "CBS_OWNERDRAWVARIABLE"),
            (0x40, "CBS_AUTOHSCROLL"), (0x80, "CBS_OEMCONVERT"), (0x100, "CBS_SORT"),
            (0x200, "CBS_HASSTRINGS"), (0x400, "CBS_NOINTEGRALHEIGHT"),
            (0x800, "CBS_DISABLENOSCROLL")]
SBS_TYPES = {0: "SBS_HORZ", 1: "SBS_VERT"}

CONTROL_ATOMS = {0x80: "BUTTON", 0x81: "EDIT", 0x82: "STATIC", 0x83: "LISTBOX",
                 0x84: "SCROLLBAR", 0x85: "COMBOBOX"}


def window_style_names(style: int, control: bool) -> list[str]:
    names = _bits(style, WS_COMMON)
    if style & 0x00C00000 == 0x00C00000:
        names.append("WS_CAPTION")
    else:
        names += _bits(style, WS_FRAME)
    names += _bits(style, WS_REST)
    names += _bits(style, WS_CONTROL if control else WS_TOPLEVEL)
    return names


def control_style_names(cls: str, style: int) -> list[str]:
    low = style & 0xFFFF
    names: list[str] = []
    c = cls.upper()
    if c == "BUTTON":
        names.append(BS_TYPES.get(low & 0x0F, f"BS_{low & 0x0F:#x}"))
        if low & 0x20:
            names.append("BS_LEFTTEXT")
    elif c == "STATIC":
        names.append(SS_TYPES.get(low & 0x1F, f"SS_{low & 0x1F:#x}"))
        if low & 0x80:
            names.append("SS_NOPREFIX")
    elif c == "EDIT":
        names.append(ES_TYPES.get(low & 0x03, "ES_3"))
        names += _bits(low, ES_BITS)
    elif c == "LISTBOX":
        names += _bits(low, LBS_BITS)
    elif c == "COMBOBOX":
        t = CBS_TYPES.get(low & 0x03)
        if t:
            names.append(t)
        names += _bits(low, CBS_BITS)
    elif c == "SCROLLBAR":
        names.append(SBS_TYPES.get(low & 0x01))
    elif low:
        names.append(f"{low:#06x}")
    return names + window_style_names(style & 0xFFFF0000, control=True)


# ---------------------------------------------------------------- dialogs

def _name_or_ordinal(d: bytes, p: int) -> tuple[Any, int]:
    """Menu name / control text: 0xFF + WORD ordinal, else an ASCIIZ string."""
    if d[p] == 0xFF:
        return {"ordinal": struct.unpack_from("<H", d, p + 1)[0]}, p + 3
    s, p = _asciiz(d, p)
    return decode_cp1252(s), p


def parse_dialog(data: bytes) -> dict[str, Any]:
    d = data
    style, count, x, y, cx, cy = struct.unpack_from("<IBhhhh", d, 0)
    p = 13
    if d[p] == 0:
        menu: Any = None
        p += 1
    else:
        menu, p = _name_or_ordinal(d, p)
    cls, p = _asciiz(d, p)
    caption, p = _asciiz(d, p)
    out: dict[str, Any] = {
        "style": f"{style:#010x}",
        "style_names": _bits(style & 0xFFFF, DS_NAMES) + window_style_names(style, control=False),
        "x": x, "y": y, "cx": cx, "cy": cy,
        "menu": menu,
        "class": decode_cp1252(cls) or None,
        "caption": decode_cp1252(caption),
        "font": None,
    }
    if style & DS_SETFONT:
        (pt,) = struct.unpack_from("<H", d, p)
        face, p = _asciiz(d, p + 2)
        out["font"] = {"point_size": pt, "face": decode_cp1252(face)}
    items = []
    for i in range(count):
        if p + 14 > len(d):
            raise ValueError(f"dialog item {i} truncated")
        ix, iy, icx, icy, cid, cstyle = struct.unpack_from("<hhhhHI", d, p)
        p += 14
        if d[p] & 0x80:
            atom = d[p]
            ccls = CONTROL_ATOMS.get(atom, f"#{atom:#04x}")
            p += 1
        else:
            atom = None
            raw, p = _asciiz(d, p)
            ccls = decode_cp1252(raw)
        text, p = _name_or_ordinal(d, p)
        nextra = d[p]
        extra = d[p + 1:p + 1 + nextra]
        p += 1 + nextra
        items.append({
            "id": cid,
            "class": ccls,
            "class_atom": f"{atom:#04x}" if atom is not None else None,
            "text": text,
            "x": ix, "y": iy, "cx": icx, "cy": icy,
            "style": f"{cstyle:#010x}",
            "style_names": control_style_names(ccls, cstyle),
            "extra": extra.hex() if extra else None,
        })
    out["controls"] = items
    out["template_size"] = p
    return out


# ---------------------------------------------------------------- menus

MF_GRAYED, MF_DISABLED, MF_BITMAP, MF_CHECKED = 0x01, 0x02, 0x04, 0x08
MF_POPUP, MF_MENUBARBREAK, MF_MENUBREAK, MF_END = 0x10, 0x20, 0x40, 0x80
MF_OWNERDRAW, MF_HELP = 0x100, 0x4000
MENU_FLAG_NAMES = [(MF_GRAYED, "GRAYED"), (MF_DISABLED, "INACTIVE"), (MF_BITMAP, "BITMAP"),
                   (MF_CHECKED, "CHECKED"), (MF_POPUP, "POPUP"), (MF_MENUBARBREAK, "MENUBARBREAK"),
                   (MF_MENUBREAK, "MENUBREAK"), (MF_OWNERDRAW, "OWNERDRAW"), (MF_HELP, "HELP")]


def _parse_menu_items(d: bytes, p: int, depth: int) -> tuple[list[dict[str, Any]], int]:
    if depth > 16:
        raise ValueError("menu nesting deeper than 16 levels")
    items = []
    while True:
        if p + 2 > len(d):
            raise ValueError(f"menu truncated at {p:#x}")
        (flags,) = struct.unpack_from("<H", d, p)
        p += 2
        item: dict[str, Any] = {"flags": f"{flags & ~MF_END:#06x}",
                                "flag_names": _bits(flags & ~MF_END, MENU_FLAG_NAMES)}
        if not flags & MF_POPUP:
            (item["id"],) = struct.unpack_from("<H", d, p)
            p += 2
        raw, p = _asciiz(d, p)
        item["text"] = decode_cp1252(raw)
        if flags & MF_POPUP:
            item["items"], p = _parse_menu_items(d, p, depth + 1)
        elif not raw and item["id"] == 0 and not flags & ~MF_END:
            item["separator"] = True
        items.append(item)
        if flags & MF_END:
            return items, p


def parse_menu(data: bytes) -> dict[str, Any]:
    version, offset = struct.unpack_from("<HH", data, 0)
    items, end = _parse_menu_items(data, 4 + offset, 0)
    return {"version": version, "header_offset": offset, "items": items, "template_size": end}


# ---------------------------------------------------------------- accelerators

FVIRTKEY, FNOINVERT, FSHIFT, FCONTROL, FALT, ACCEL_LAST = 0x01, 0x02, 0x04, 0x08, 0x10, 0x80
ACCEL_FLAG_NAMES = [(FVIRTKEY, "VIRTKEY"), (FNOINVERT, "NOINVERT"), (FSHIFT, "SHIFT"),
                    (FCONTROL, "CONTROL"), (FALT, "ALT")]

VK_NAMES = {
    0x01: "VK_LBUTTON", 0x02: "VK_RBUTTON", 0x03: "VK_CANCEL", 0x04: "VK_MBUTTON",
    0x08: "VK_BACK", 0x09: "VK_TAB", 0x0C: "VK_CLEAR", 0x0D: "VK_RETURN", 0x10: "VK_SHIFT",
    0x11: "VK_CONTROL", 0x12: "VK_MENU", 0x13: "VK_PAUSE", 0x14: "VK_CAPITAL", 0x1B: "VK_ESCAPE",
    0x20: "VK_SPACE", 0x21: "VK_PRIOR", 0x22: "VK_NEXT", 0x23: "VK_END", 0x24: "VK_HOME",
    0x25: "VK_LEFT", 0x26: "VK_UP", 0x27: "VK_RIGHT", 0x28: "VK_DOWN", 0x29: "VK_SELECT",
    0x2A: "VK_PRINT", 0x2B: "VK_EXECUTE", 0x2C: "VK_SNAPSHOT", 0x2D: "VK_INSERT",
    0x2E: "VK_DELETE", 0x2F: "VK_HELP", 0x6A: "VK_MULTIPLY", 0x6B: "VK_ADD",
    0x6C: "VK_SEPARATOR", 0x6D: "VK_SUBTRACT", 0x6E: "VK_DECIMAL", 0x6F: "VK_DIVIDE",
    0x90: "VK_NUMLOCK", 0x91: "VK_SCROLL",
}
VK_NAMES.update({0x60 + i: f"VK_NUMPAD{i}" for i in range(10)})
VK_NAMES.update({0x70 + i: f"VK_F{i + 1}" for i in range(24)})


def accel_key_name(key: int, flags: int) -> str:
    if flags & FVIRTKEY:
        if 0x30 <= key <= 0x39 or 0x41 <= key <= 0x5A:
            return f'"{chr(key)}"'
        return VK_NAMES.get(key, f"{key:#04x}")
    if 1 <= key <= 26:
        return f'"^{chr(key + 0x40)}"'
    if 0x20 <= key < 0x7F:
        return '"\\""' if key == 0x22 else f'"{chr(key)}"'
    return f"{key}"


def parse_accelerators(data: bytes) -> list[dict[str, Any]]:
    out = []
    p = 0
    while p + 5 <= len(data):
        flags, key, cmd = struct.unpack_from("<BHH", data, p)
        p += 5
        out.append({
            "flags": f"{flags & ~ACCEL_LAST:#04x}",
            "flag_names": _bits(flags, ACCEL_FLAG_NAMES),
            "key": key,
            "key_name": accel_key_name(key, flags),
            "cmd": cmd,
        })
        if flags & ACCEL_LAST:
            return out
    raise ValueError("accelerator table has no entry flagged 0x80")


# ---------------------------------------------------------------- version

FIXED_FIELDS = ["signature", "struc_version", "file_version_ms", "file_version_ls",
                "product_version_ms", "product_version_ls", "file_flags_mask", "file_flags",
                "file_os", "file_type", "file_subtype", "file_date_ms", "file_date_ls"]


def _align4(p: int) -> int:
    return (p + 3) & ~3


def _version_node(d: bytes, p: int, depth: int) -> tuple[dict[str, Any], int]:
    if depth > 8:
        raise ValueError("VERSION nesting deeper than 8 levels")
    length, vlen = struct.unpack_from("<HH", d, p)
    if length < 4 or p + length > len(d):
        raise ValueError(f"VERSION node at {p:#x} has bad length {length}")
    end = p + length
    key, q = _asciiz(d, p + 4)
    q = _align4(q)
    node: dict[str, Any] = {"key": decode_cp1252(key), "value": d[q:q + vlen], "children": []}
    q = _align4(q + vlen)
    while q + 4 <= end:
        child, q = _version_node(d, q, depth + 1)
        node["children"].append(child)
        q = _align4(q)
    return node, end


def parse_version(data: bytes) -> dict[str, Any]:
    root, end = _version_node(data, 0, 0)
    if root["key"] != "VS_VERSION_INFO":
        raise ValueError(f"VERSION root key is {root['key']!r}")
    out: dict[str, Any] = {"key": root["key"], "fixed": None, "string_file_info": {},
                           "var_file_info": {}}
    v = root["value"]
    if len(v) >= 52:
        f = dict(zip(FIXED_FIELDS, struct.unpack_from("<13I", v, 0)))
        if f["signature"] != 0xFEEF04BD:
            raise ValueError(f"VS_FIXEDFILEINFO signature {f['signature']:#x}")
        fixed = {k: f"{val:#010x}" for k, val in f.items()}
        fixed["file_version"] = (f"{f['file_version_ms'] >> 16}.{f['file_version_ms'] & 0xFFFF}."
                                 f"{f['file_version_ls'] >> 16}.{f['file_version_ls'] & 0xFFFF}")
        fixed["product_version"] = (f"{f['product_version_ms'] >> 16}.{f['product_version_ms'] & 0xFFFF}."
                                    f"{f['product_version_ls'] >> 16}.{f['product_version_ls'] & 0xFFFF}")
        out["fixed"] = fixed
    for child in root["children"]:
        if child["key"] == "StringFileInfo":
            for table in child["children"]:
                strings = {}
                for s in table["children"]:
                    strings[s["key"]] = decode_cp1252(s["value"].split(b"\0", 1)[0])
                out["string_file_info"][table["key"]] = strings
        elif child["key"] == "VarFileInfo":
            for var in child["children"]:
                words = struct.unpack_from(f"<{len(var['value']) // 2}H", var["value"])
                out["var_file_info"][var["key"]] = [f"{w:#06x}" for w in words]
    out["size"] = end
    return out


# ---------------------------------------------------------------- DIBs

def dib_header(data: bytes, off: int = 0) -> dict[str, Any]:
    """Parse a BITMAPINFOHEADER or BITMAPCOREHEADER at ``off``."""
    (hsize,) = struct.unpack_from("<I", data, off)
    if hsize == 12:
        w, h, planes, bpp = struct.unpack_from("<HHHH", data, off + 4)
        comp, size_image, clr_used = 0, 0, 0
        entry = 3
    elif hsize in (40, 52, 56, 108, 124):
        w, h, planes, bpp, comp, size_image, _xp, _yp, clr_used, _imp = struct.unpack_from(
            "<iiHHIIiiII", data, off + 4)
        entry = 4
    else:
        raise ValueError(f"unknown DIB header size {hsize}")
    if planes != 1 or bpp not in (1, 4, 8, 16, 24, 32) or w <= 0 or h == 0:
        raise ValueError(f"implausible DIB: {w}x{h} planes {planes} bpp {bpp}")
    ncolors = clr_used if clr_used else (1 << bpp if bpp <= 8 else 0)
    if comp == 3 and hsize == 40:
        ncolors += 3  # BI_BITFIELDS masks follow the header
    stride = ((w * bpp + 31) // 32) * 4
    rows = abs(h)
    computed = stride * rows if comp in (0, 3) else size_image
    pal_off = off + hsize
    palette = []
    for i in range(ncolors):
        q = pal_off + i * entry
        if q + 3 > len(data):
            raise ValueError("DIB palette overruns the data")
        b, g, r = data[q], data[q + 1], data[q + 2]
        palette.append(f"{r:02x}{g:02x}{b:02x}")
    return {"header_size": hsize, "width": w, "height": h, "planes": planes, "bpp": bpp,
            "compression": comp, "size_image": size_image, "colors": ncolors,
            "palette_entry_size": entry, "palette": palette, "stride": stride,
            "bits_offset": hsize + ncolors * entry, "bits_size": computed}


def pixel_rows(data: bytes, info: dict[str, Any], bits_off: int) -> list[list[int]]:
    """Palette indices of an uncompressed 1/4/8-bpp DIB, top row first."""
    w, h, bpp, stride = info["width"], info["height"], info["bpp"], info["stride"]
    if bpp not in (1, 4, 8) or info["compression"] != 0:
        raise ValueError("pixel_rows handles uncompressed 1/4/8-bpp DIBs only")
    rows = []
    per = 8 // bpp
    mask = (1 << bpp) - 1
    for r in range(abs(h)):
        row = data[bits_off + r * stride:bits_off + (r + 1) * stride]
        px = []
        for x in range(w):
            byte = row[x // per]
            shift = 8 - bpp * (x % per + 1)
            px.append((byte >> shift) & mask)
        rows.append(px)
    if h > 0:
        rows.reverse()
    return rows


def dib_to_bmp(data: bytes) -> tuple[bytes, dict[str, Any]]:
    """Wrap a resource DIB (RT_BITMAP) in a BITMAPFILEHEADER."""
    info = dib_header(data)
    size = info["bits_offset"] + info["bits_size"]
    if size > len(data):
        raise ValueError(f"DIB needs {size} bytes, resource holds {len(data)}")
    body = data[:size]
    head = b"BM" + struct.pack("<IHHI", 14 + size, 0, 0, 14 + info["bits_offset"])
    return head + body, info


def bmp_file_info(data: bytes) -> tuple[bytes, dict[str, Any]]:
    """Validate a file-format DIB ('BM' + BITMAPFILEHEADER) and trim it to bfSize."""
    if data[:2] != b"BM":
        raise ValueError("no BM signature")
    bf_size, _r1, _r2, off_bits = struct.unpack_from("<IHHI", data, 2)
    info = dib_header(data, 14)
    if off_bits != 14 + info["bits_offset"]:
        raise ValueError(f"bfOffBits {off_bits} != {14 + info['bits_offset']}")
    need = off_bits + info["bits_size"]
    if bf_size < need or bf_size > len(data):
        raise ValueError(f"bfSize {bf_size} outside {need}..{len(data)}")
    info["bf_size"] = bf_size
    return data[:bf_size], info


# ---------------------------------------------------------------- icons and cursors

def icon_image_size(data: bytes, off: int = 0) -> tuple[int, dict[str, Any]]:
    """Exact byte size of an icon/cursor image (header, palette, XOR and AND masks)."""
    info = dib_header(data, off)
    h = abs(info["height"]) // 2
    xor = info["stride"] * h
    and_stride = ((info["width"] + 31) // 32) * 4
    return info["bits_offset"] + xor + and_stride * h, info


def single_icon(data: bytes) -> tuple[bytes, dict[str, Any]]:
    """An .ico file holding one RT_ICON image."""
    size, info = icon_image_size(data)
    if size > len(data):
        raise ValueError(f"icon needs {size} bytes, resource holds {len(data)}")
    w, h = info["width"], abs(info["height"]) // 2
    colors = info["colors"] if info["bpp"] < 8 else 0
    entry = struct.pack("<BBBBHHII", w & 0xFF, h & 0xFF, colors & 0xFF, 0, 1, info["bpp"], size, 22)
    return struct.pack("<HHH", 0, 1, 1) + entry + data[:size], _image_info(info, h)


def single_cursor(data: bytes) -> tuple[bytes, dict[str, Any]]:
    """A .cur file holding one RT_CURSOR image (hotspot WORDs + DIB)."""
    hx, hy = struct.unpack_from("<HH", data, 0)
    size, info = icon_image_size(data, 4)
    if 4 + size > len(data):
        raise ValueError(f"cursor needs {4 + size} bytes, resource holds {len(data)}")
    w, h = info["width"], abs(info["height"]) // 2
    entry = struct.pack("<BBBBHHII", w & 0xFF, h & 0xFF, 0, 0, hx, hy, size, 22)
    out = _image_info(info, h)
    out["hotspot"] = [hx, hy]
    return struct.pack("<HHH", 0, 2, 1) + entry + data[4:4 + size], out


def _image_info(info: dict[str, Any], h: int) -> dict[str, Any]:
    return {"width": info["width"], "height": h, "bpp": info["bpp"], "colors": info["colors"]}


def parse_group(data: bytes) -> dict[str, Any]:
    """GROUP_ICON / GROUP_CURSOR directory."""
    reserved, kind, count = struct.unpack_from("<HHH", data, 0)
    if reserved != 0 or kind not in (1, 2):
        raise ValueError(f"bad group header {reserved} {kind}")
    entries = []
    for i in range(count):
        p = 6 + i * 14
        if kind == 1:
            w, h, colors, _res, planes, bpp, nbytes, rid = struct.unpack_from("<BBBBHHIH", data, p)
            entries.append({"width": w, "height": h, "colors": colors, "planes": planes,
                            "bpp": bpp, "bytes": nbytes, "id": rid})
        else:
            w, h, planes, bpp, nbytes, rid = struct.unpack_from("<HHHHIH", data, p)
            entries.append({"width": w, "height": h // 2, "planes": planes, "bpp": bpp,
                            "bytes": nbytes, "id": rid})
    return {"kind": "icon" if kind == 1 else "cursor", "entries": entries}


def build_group_file(group: dict[str, Any], image: Callable[[int], bytes | None]) -> bytes:
    """Assemble an .ico/.cur from a parsed group and a lookup of member images by id."""
    is_icon = group["kind"] == "icon"
    parts = []
    for e in group["entries"]:
        img = image(e["id"])
        if img is None:
            raise ValueError(f"group member {e['id']} is missing")
        if is_icon:
            size, _ = icon_image_size(img)
            parts.append((e, img[:size], 0, 0))
        else:
            hx, hy = struct.unpack_from("<HH", img, 0)
            size, _ = icon_image_size(img, 4)
            parts.append((e, img[4:4 + size], hx, hy))
    head = struct.pack("<HHH", 0, 1 if is_icon else 2, len(parts))
    off = 6 + 16 * len(parts)
    dirs, blobs = [], []
    for e, blob, hx, hy in parts:
        if is_icon:
            dirs.append(struct.pack("<BBBBHHII", e["width"] & 0xFF, e["height"] & 0xFF,
                                    e["colors"] & 0xFF, 0, e["planes"], e["bpp"], len(blob), off))
        else:
            dirs.append(struct.pack("<BBBBHHII", e["width"] & 0xFF, e["height"] & 0xFF, 0, 0,
                                    hx, hy, len(blob), off))
        blobs.append(blob)
        off += len(blob)
    return head + b"".join(dirs) + b"".join(blobs)


# ---------------------------------------------------------------- WAV

def wav_samples(data: bytes) -> bytes:
    """The payload of a RIFF WAVE file's data chunk."""
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError("not RIFF WAVE")
    end = min(len(data), 8 + struct.unpack_from("<I", data, 4)[0])
    p = 12
    while p + 8 <= end:
        cid, clen = data[p:p + 4], struct.unpack_from("<I", data, p + 4)[0]
        if cid == b"data":
            return data[p + 8:p + 8 + clen]
        p += 8 + clen + (clen & 1)
    raise ValueError("WAVE lacks a data chunk")


def wav_info(data: bytes) -> tuple[bytes, dict[str, Any]]:
    """Validate a RIFF WAVE file, trim it to its RIFF size, return the fmt facts."""
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError("not RIFF WAVE")
    (riff,) = struct.unpack_from("<I", data, 4)
    end = 8 + riff
    if end > len(data):
        raise ValueError(f"RIFF size {riff} overruns the resource ({len(data)} bytes)")
    p = 12
    fmt = None
    data_len = None
    while p + 8 <= end:
        cid, clen = data[p:p + 4], struct.unpack_from("<I", data, p + 4)[0]
        if p + 8 + clen > end:
            raise ValueError(f"chunk {cid!r} overruns the RIFF")
        if cid == b"fmt ":
            tag, ch, rate, brate, align, bits = struct.unpack_from("<HHIIHH", data, p + 8)
            fmt = {"format_tag": tag, "channels": ch, "rate": rate, "byte_rate": brate,
                   "block_align": align, "bits": bits}
        elif cid == b"data":
            data_len = clen
        p += 8 + clen + (clen & 1)
    if fmt is None or data_len is None:
        raise ValueError("WAVE lacks fmt or data chunk")
    frames = data_len // fmt["block_align"] if fmt["block_align"] else 0
    info = dict(fmt)
    info["data_bytes"] = data_len
    info["frames"] = frames
    info["duration_ms"] = round(frames * 1000 / fmt["rate"]) if fmt["rate"] else 0
    return data[:end], info


# ---------------------------------------------------------------- sniffing

def sniff(data: bytes, align: int) -> tuple[str, str, bytes, dict[str, Any]]:
    """Classify a custom or RCDATA resource: (kind, extension, payload, info)."""
    if data[:4] == b"RIFF" and data[8:12] == b"WAVE":
        try:
            payload, info = wav_info(data)
            return "wav", ".wav", payload, info
        except (ValueError, struct.error):
            pass
    if data[:2] == b"BM":
        try:
            payload, info = bmp_file_info(data)
            return "bmp-file", ".bmp", payload, _bmp_summary(info)
        except (ValueError, struct.error):
            pass
    if len(data) >= 16 and struct.unpack_from("<I", data, 0)[0] in (12, 40):
        try:
            payload, info = dib_to_bmp(data)
            return "dib", ".bmp", payload, _bmp_summary(info)
        except (ValueError, struct.error):
            pass
    text = text_payload(data, align)
    if text is not None:
        s = decode_cp1252(text)
        return "text", ".txt", s.encode("utf-8"), {"chars": len(s)}
    nz = len(data.rstrip(b"\0"))
    info = {"hex_preview": data[:32].hex(" "), "nonzero_length": nz}
    return ("zeros" if nz == 0 else "binary"), ".bin", data, info


def _bmp_summary(info: dict[str, Any]) -> dict[str, Any]:
    return {"width": info["width"], "height": info["height"], "bpp": info["bpp"],
            "compression": info["compression"], "colors": info["colors"]}
