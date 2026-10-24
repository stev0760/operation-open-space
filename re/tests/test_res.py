"""Tests for ``isre res``, the Win16 NE resource dumper.

The synthetic tests build small resources and NE modules and always run. The
tests on the game modules extract ``original/`` into a temporary directory,
dump every module and check the counts that docs/re/resources.md states; they
skip when ``original/`` is absent.
"""
from __future__ import annotations

import collections
import json
import struct
from pathlib import Path

import pytest

from isre import res
from isre import res_formats as F
from isre import res_rc as RC
from isre import szdd
from isre.ne import NEModule

REPO_ROOT = Path(__file__).resolve().parents[2]
ORIGINAL = REPO_ROOT / "original"


# ================================================================ synthetic builders

def string_block(slots: dict[int, bytes]) -> bytes:
    return b"".join(bytes([len(slots.get(i, b""))]) + slots.get(i, b"") for i in range(16))


def dib(w: int, h: int, bpp: int, icon: bool = False, fill: int = 0) -> bytes:
    ncol = 1 << bpp
    stride = ((w * bpp + 31) // 32) * 4
    rows = h
    head = struct.pack("<IiiHHIIiiII", 40, w, h * 2 if icon else h, 1, bpp, 0, 0, 0, 0, 0, 0)
    pal = b"".join(bytes([i, i, i, 0]) for i in range(ncol))
    bits = bytes([fill]) * (stride * rows)
    if icon:
        bits += b"\0" * (((w + 31) // 32) * 4 * h)
    return head + pal + bits


def wav(samples: bytes, rate: int = 11025) -> bytes:
    fmt = struct.pack("<HHIIHH", 1, 1, rate, rate, 1, 8)
    body = b"WAVE" + b"fmt " + struct.pack("<I", len(fmt)) + fmt
    body += b"data" + struct.pack("<I", len(samples)) + samples + (b"\0" if len(samples) & 1 else b"")
    return b"RIFF" + struct.pack("<I", len(body)) + body


def dialog_template() -> bytes:
    style = 0x90C000C0  # DS_SETFONT | DS_MODALFRAME | WS_POPUP | WS_VISIBLE | WS_CAPTION
    d = struct.pack("<IBhhhh", style, 3, 10, 20, 200, 100)
    d += b"\0"                       # no menu
    d += b"\0"                       # default class
    d += b"Test Dialog\0"
    d += struct.pack("<H", 8) + b"Helv\0"
    # item 1: OK button, class atom 0x80
    d += struct.pack("<hhhhHI", 5, 6, 40, 14, 1, 0x50010001) + b"\x80" + b"&OK\0" + b"\0"
    # item 2: custom class string, ordinal text, two extra bytes
    d += struct.pack("<hhhhHI", 50, 6, 32, 32, 0xFFFF, 0x50000003) + b"LevClass\0"
    d += b"\xff" + struct.pack("<H", 7) + b"\x02\xab\xcd"
    # item 3: drop-down list combobox
    d += struct.pack("<hhhhHI", 5, 30, 90, 60, 324, 0x50210103) + b"\x85" + b"\0" + b"\0"
    return d


def menu_template() -> bytes:
    d = struct.pack("<HH", 0, 0)
    d += struct.pack("<H", 0x10) + b"&Game\0"                     # popup
    d += struct.pack("<HH", 0, 101) + b"&New\tF2\0"
    d += struct.pack("<HH", 0, 0) + b"\0"                         # separator
    d += struct.pack("<HH", 0x80 | 0x08, 102) + b"E&xit\0"        # last, checked
    d += struct.pack("<H", 0x80 | 0x10) + b"&Help\0"              # last popup
    d += struct.pack("<HH", 0x80 | 0x01, 201) + b"&About\0"       # last, grayed
    return d


def accel_table() -> bytes:
    return (struct.pack("<BHH", 0x10, ord("G"), 192)              # "G", ALT
            + struct.pack("<BHH", 0x01, 0x70, 240)                # VK_F1, VIRTKEY
            + struct.pack("<BHH", 0x80 | 0x05, 0x09, 510))        # VK_TAB, VIRTKEY, SHIFT, last


def _vnode(key: str, value: bytes, children: list[bytes]) -> bytes:
    body = bytearray(b"\0\0\0\0" + key.encode("latin-1") + b"\0")
    body += b"\0" * (-len(body) % 4)
    body += value
    for c in children:
        body += b"\0" * (-len(body) % 4)
        body += c
    struct.pack_into("<HH", body, 0, len(body), len(value))
    return bytes(body)


def version_blob() -> bytes:
    fixed = struct.pack("<13I", 0xFEEF04BD, 0x10000, 0x0001001D, 0x00080000, 0x0001001D,
                        0x00080000, 0x3F, 0, 0x1, 0x1, 0, 0, 0)
    sfi = _vnode("StringFileInfo", b"", [_vnode("040904E4", b"", [
        _vnode("FileVersion", b"1.29.08\0", []),
        _vnode("ProductName", b"Test \xa9\0", []),
    ])])
    vfi = _vnode("VarFileInfo", b"", [_vnode("Translation", struct.pack("<HH", 0x409, 0x4E4), [])])
    return _vnode("VS_VERSION_INFO", fixed, [sfi, vfi])


def group_icon(entries: list[tuple[int, int, int, int]]) -> bytes:
    """entries: (width, height, bpp, id); image sizes come from dib(icon=True)."""
    d = struct.pack("<HHH", 0, 1, len(entries))
    for w, h, bpp, rid in entries:
        size = len(dib(w, h, bpp, icon=True))
        d += struct.pack("<BBBBHHIH", w, h, 16 if bpp == 4 else 0, 0, 1, bpp, size, rid)
    return d


def group_cursor(w: int, h: int, rid: int) -> bytes:
    size = len(dib(w, h, 1, icon=True))
    return struct.pack("<HHH", 0, 2, 1) + struct.pack("<HHHHIH", w, h * 2, 1, 1, size, rid)


def make_ne(resources: list[tuple], shift: int = 4, module: str = "TESTMOD") -> bytes:
    """A minimal Win16 NE library holding ``resources`` = [(type, id, data)], int or str."""
    types: list[list] = []
    for t, i, d in resources:
        for entry in types:
            if entry[0] == t:
                entry[1].append((i, d))
                break
        else:
            types.append([t, [(i, d)]])
    table_len = 2 + len(types) * 8 + len(resources) * 12 + 2
    names = bytearray()
    name_off: dict[str, int] = {}

    def ref(v):
        if isinstance(v, int):
            return v | 0x8000
        if v not in name_off:
            name_off[v] = table_len + len(names)
            names.extend(bytes([len(v)]) + v.encode("latin-1"))
        return name_off[v]

    for t, items in types:
        ref(t)
        for i, _ in items:
            ref(i)
    names.append(0)
    restab = bytes([len(module)]) + module.encode() + b"\0\0" + b"\0"
    rsrctab = 0x40
    restab_off = rsrctab + table_len + len(names)
    modtab = imptab = restab_off + len(restab)
    enttab = imptab + 1
    tables_end = 0x40 + enttab + 1
    unit = 1 << shift
    cur = (tables_end + unit - 1) // unit * unit
    data_start = cur
    rsrc = bytearray(struct.pack("<H", shift))
    blobs = bytearray()
    for t, items in types:
        rsrc += struct.pack("<HHI", ref(t), len(items), 0)
        for i, d in items:
            padded = d + b"\xee" * (-len(d) % unit)   # stale-memory padding
            rsrc += struct.pack("<HHHHHH", cur >> shift, len(padded) >> shift, 0x1030, ref(i), 0, 0)
            blobs += padded
            cur += len(padded)
    rsrc += b"\0\0" + names
    hdr = bytearray(0x40)
    hdr[0:2] = b"NE"
    struct.pack_into("<HH", hdr, 0x04, enttab, 1)
    struct.pack_into("<H", hdr, 0x0C, 0x8001)
    struct.pack_into("<HHHHH", hdr, 0x22, 0x40, rsrctab, restab_off, modtab, imptab)
    struct.pack_into("<H", hdr, 0x32, shift)
    hdr[0x36] = 2
    mz = bytearray(0x40)
    mz[0:2] = b"MZ"
    struct.pack_into("<I", mz, 0x3C, 0x40)
    body = bytes(mz + hdr + rsrc + restab) + b"\0\0"
    assert len(body) == tables_end
    return body + b"\0" * (data_start - len(body)) + bytes(blobs)


def synthetic_resources() -> list[tuple]:
    return [
        (6, 1, string_block({1: b"Hello", 2: b"Tab\there\r\nnext", 15: b"x\x81\xa9"})),
        (6, 3, string_block({0: b"thirty-two"})),
        (5, 0x0141, dialog_template()),
        (4, "MAINMENU", menu_template()),
        (9, "MAINACCEL", accel_table()),
        (16, 1, version_blob()),
        (2, "LOGO", dib(5, 3, 4)),
        (3, 1, dib(32, 32, 4, icon=True)),
        (3, 2, dib(16, 16, 4, icon=True)),
        (14, "GAME", group_icon([(32, 32, 4, 1), (16, 16, 4, 2)])),
        (1, 7, struct.pack("<HH", 3, 4) + dib(32, 32, 1, icon=True)),
        (12, 8, group_cursor(32, 32, 7)),
        (100, 100, wav(b"\x80" * 21)),
        (4000, 290, b"BM" + struct.pack("<IHHI", 14 + len(dib(6, 2, 4)), 0, 0, 14 + 40 + 64)
         + dib(6, 2, 4, fill=0xFF)),
        (5000, 101, b"ENTER INNER SPACE.\0"),
        (5500, 110, b"\0" * 32),
        (5600, 135, bytes(range(1, 40))),
        ("MYTYPE", "BLOB", b"\x01\x02\x03\x00\x05"),
        (10, "DLGINCLUDE", b"C:\\X\\DEMODLG.H\0"),
    ]


# ================================================================ synthetic tests

def test_string_block_ids_and_cp1252():
    block = F.decode_string_block(3, string_block({0: b"a", 5: b"\x93q\x94", 15: b"\x81"}))
    assert block == [(32, "a"), (37, "\u201cq\u201d"), (47, "\x81")]
    assert F.decode_string_block(1, string_block({})) == []
    with pytest.raises(ValueError):
        F.decode_string_block(1, b"\x05ab")


def test_escape_tsv():
    assert F.escape_tsv("a\tb\r\nc\\d") == "a\\tb\\r\\nc\\\\d"


def test_dialog_template():
    d = F.parse_dialog(dialog_template())
    assert (d["x"], d["y"], d["cx"], d["cy"]) == (10, 20, 200, 100)
    assert d["caption"] == "Test Dialog" and d["menu"] is None and d["class"] is None
    assert d["font"] == {"point_size": 8, "face": "Helv"}
    assert "DS_SETFONT" in d["style_names"] and "WS_CAPTION" in d["style_names"]
    ok, lev, combo = d["controls"]
    assert ok["class"] == "BUTTON" and ok["text"] == "&OK" and ok["id"] == 1
    assert "BS_DEFPUSHBUTTON" in ok["style_names"] and "WS_TABSTOP" in ok["style_names"]
    assert lev["class"] == "LevClass" and lev["text"] == {"ordinal": 7} and lev["extra"] == "abcd"
    assert lev["class_atom"] is None and lev["id"] == 0xFFFF
    assert combo["class"] == "COMBOBOX" and "CBS_DROPDOWNLIST" in combo["style_names"]
    assert "CBS_SORT" in combo["style_names"]
    assert d["template_size"] == len(dialog_template())
    rc = RC.dialog_rc(0x0141, d)
    assert rc.startswith("321 DIALOG 10, 20, 200, 100  // #0141")
    assert 'CONTROL 7, -1, "LevClass", 0x50000003, 50, 6, 32, 32' in rc


def test_menu_template():
    m = F.parse_menu(menu_template())
    game, helpm = m["items"]
    assert game["text"] == "&Game" and "id" not in game
    assert [i.get("id") for i in game["items"]] == [101, 0, 102]
    assert game["items"][1].get("separator") is True
    assert game["items"][2]["flag_names"] == ["CHECKED"]
    assert helpm["items"][0]["flag_names"] == ["GRAYED"]
    assert m["template_size"] == len(menu_template())
    rc = RC.menu_rc("MAINMENU", m)
    assert 'MENUITEM "&New\\tF2", 101' in rc and "MENUITEM SEPARATOR" in rc
    assert 'MENUITEM "E&xit", 102, CHECKED' in rc


def test_accelerators():
    a = F.parse_accelerators(accel_table() + b"\xee" * 7)
    assert [(e["key_name"], e["cmd"], e["flag_names"]) for e in a] == [
        ('"G"', 192, ["ALT"]), ("VK_F1", 240, ["VIRTKEY"]), ("VK_TAB", 510, ["VIRTKEY", "SHIFT"])]
    with pytest.raises(ValueError):
        F.parse_accelerators(struct.pack("<BHH", 0x10, 65, 1))


def test_version_win16_layout():
    v = F.parse_version(version_blob() + b"\0" * 12)
    assert v["fixed"]["file_version"] == "1.29.8.0"
    assert v["string_file_info"] == {"040904E4": {"FileVersion": "1.29.08", "ProductName": "Test \u00a9"}}
    assert v["var_file_info"] == {"Translation": ["0x0409", "0x04e4"]}


def test_dib_to_bmp_and_pixels():
    raw = dib(5, 3, 4, fill=0x1F)
    bmp, info = F.dib_to_bmp(raw + b"\xee" * 9)
    assert bmp[:2] == b"BM" and len(bmp) == 14 + len(raw)
    assert struct.unpack_from("<I", bmp, 2)[0] == len(bmp)
    assert struct.unpack_from("<I", bmp, 10)[0] == 14 + 40 + 64
    assert (info["width"], info["height"], info["bpp"], info["colors"]) == (5, 3, 4, 16)
    rows = F.pixel_rows(bmp, F.dib_header(bmp, 14), 14 + info["bits_offset"])
    assert rows == [[1, 15, 1, 15, 1]] * 3
    trimmed, binfo = F.bmp_file_info(bmp + b"\0" * 5)
    assert trimmed == bmp and binfo["bf_size"] == len(bmp)


def test_icons_and_cursors():
    ico, info = F.single_icon(dib(32, 32, 4, icon=True) + b"\xee" * 3)
    assert struct.unpack_from("<HHH", ico, 0) == (0, 1, 1)
    assert (info["width"], info["height"], info["bpp"]) == (32, 32, 4)
    assert struct.unpack_from("<I", ico, 6 + 8)[0] == len(ico) - 22
    images = {1: dib(32, 32, 4, icon=True), 2: dib(16, 16, 4, icon=True)}
    g = F.parse_group(group_icon([(32, 32, 4, 1), (16, 16, 4, 2)]))
    blob = F.build_group_file(g, images.get)
    assert struct.unpack_from("<HHH", blob, 0) == (0, 1, 2)
    off2 = struct.unpack_from("<I", blob, 6 + 16 + 12)[0]
    assert blob[off2:] == images[2]
    cur_img = struct.pack("<HH", 3, 4) + dib(32, 32, 1, icon=True)
    cur, cinfo = F.single_cursor(cur_img)
    assert struct.unpack_from("<HHH", cur, 0) == (0, 2, 1) and cinfo["hotspot"] == [3, 4]
    gc = F.parse_group(group_cursor(32, 32, 7))
    assert gc["kind"] == "cursor" and gc["entries"][0]["height"] == 32
    cblob = F.build_group_file(gc, {7: cur_img}.get)
    assert struct.unpack_from("<HH", cblob, 6 + 4) == (3, 4)
    with pytest.raises(ValueError):
        F.build_group_file(g, {}.get)


def test_wav_info():
    w = wav(b"\x80" * 21)
    trimmed, info = F.wav_info(w + b"\xee" * 6)
    assert trimmed == w
    assert (info["rate"], info["channels"], info["bits"], info["frames"]) == (11025, 1, 8, 21)
    assert F.wav_samples(w) == b"\x80" * 21


def test_sniff():
    assert F.sniff(wav(b"\x80\x81"), 16)[0] == "wav"
    kind, ext, payload, _ = F.sniff(b"Inner Space!\0ta\0", 16)
    assert (kind, ext, payload) == ("text", ".txt", b"Inner Space!")
    assert F.sniff(b"Inner Space!\0" + b"x" * 20, 16)[0] == "binary"   # data after the NUL
    assert F.sniff(b"\0" * 32, 16)[:2] == ("zeros", ".bin")
    kind, ext, _, info = F.sniff(bytes([1, 14, 12, 0x6D]) + b"\0" * 12, 16)
    assert (kind, ext) == ("binary", ".bin") and info["hex_preview"].startswith("01 0e 0c 6d")
    assert F.sniff(dib(4, 4, 1), 16)[0] == "dib"


def test_build_module_synthetic(tmp_path):
    mod = NEModule(make_ne(synthetic_resources()), "TEST.DLL")
    dump = res.build_module(mod, "test.dll", "shipdisk/TEST.DLL")
    assert dump.errors == []
    idx = dump.index
    assert idx["strings"] == 4
    assert idx["kinds"] == {"strings": 2, "dialog": 1, "menu": 1, "accelerators": 1, "version": 1,
                            "dib": 1, "icon": 2, "group_icon": 1, "cursor": 1, "group_cursor": 1,
                            "wav": 1, "bmp-file": 1, "text": 2, "zeros": 1, "binary": 2}
    out = dump.outputs
    assert out["strings.tsv"].decode() == ("id\ttext\n1\tHello\n2\tTab\\there\\r\\nnext\n"
                                           "15\tx\x81\u00a9\n32\tthirty-two\n")
    assert json.loads(out["dialogs.json"])[0]["id_hex"] == "0x0141"
    assert set(out) >= {"menus.rc", "accelerators.rc", "version.json", "BITMAP/LOGO.bmp",
                        "GROUP_ICON/GAME.ico", "GROUP_CURSOR/8.cur", "100/100.wav", "4000/290.bmp",
                        "5000/101.txt", "5500/110.bin", "MYTYPE/BLOB.bin", "RCDATA/DLGINCLUDE.txt"}
    assert out["5000/101.txt"] == b"ENTER INNER SPACE."
    entry = {(e["type"], e["id"]): e for e in idx["resources"]}
    assert entry[("4000", 290)]["width"] == 6 and entry[("100", 100)]["rate"] == 11025
    assert entry[("MYTYPE", "BLOB")]["hex_preview"].startswith("01 02 03 00 05")
    assert idx["files"] == sorted(out)


def test_undecodable_resource_becomes_bin():
    mod = NEModule(make_ne([(5, 1, b"\x00\x00\x00\x00\x05")]), "BAD.DLL")
    dump = res.build_module(mod, "bad.dll")
    assert len(dump.errors) == 1 and dump.index["kinds"] == {"undecoded": 1}
    assert "DIALOG/1.bin" in dump.outputs


def test_write_module_prunes_only_its_own_files(tmp_path):
    out = tmp_path / "m"
    out.mkdir()
    (out / "keep.txt").write_text("not ours")
    full = make_ne(synthetic_resources())
    d1 = res.write_module(res.build_module(NEModule(full, "T.DLL"), "t.dll"), out)
    assert d1.written == len(d1.outputs) and (out / "100" / "100.wav").is_file()
    d2 = res.write_module(res.build_module(NEModule(full, "T.DLL"), "t.dll"), out)
    assert d2.written == 0 and d2.removed == 0
    small = make_ne(synthetic_resources()[:2])
    d3 = res.write_module(res.build_module(NEModule(small, "T.DLL"), "t.dll"), out)
    assert d3.removed == len(d1.outputs) - len(d3.outputs)
    assert not (out / "100").exists() and (out / "keep.txt").read_text() == "not ours"
    assert sorted(p.name for p in out.iterdir()) == ["index.json", "keep.txt", "strings.json",
                                                      "strings.tsv"]


def test_cli_listing_and_dump(tmp_path, capsys):
    f = tmp_path / "TEST.DLL"
    f.write_bytes(make_ne(synthetic_resources()))
    assert res.main([str(f)]) == 0
    assert "DIALOG" in capsys.readouterr().out
    assert res.main([str(f), "-o", str(tmp_path / "out")]) == 0
    assert (tmp_path / "out" / "index.json").is_file()


def test_module_key():
    assert res.module_key("shipdisk", "ENG_LANG.DLL") == "eng_lang.dll"
    assert res.module_key("theworks", "FACTORY.EXE") == "theworks-factory.exe"


# ================================================================ game modules

EXPECTED_COUNTS = {
    "ctl3d.dll": {"BITMAP": 1, "VERSION": 1},
    "eng_lang.dll": {"5000": 57, "MENU": 4, "ACCELERATOR": 2, "RCDATA": 1, "DIALOG": 22, "STRING": 147},
    "factory.exe": {"ICON": 1, "GROUP_ICON": 1, "VERSION": 1, "STRING": 4},
    "igraphic.dll": {"5500": 1, "4000": 113, "BITMAP": 15, "6600": 5},
    "inrun.ad": {"GROUP_ICON": 1, "ICON": 1, "1000": 4, "2000": 3},
    "inspace.exe": {"ICON": 2, "GROUP_ICON": 1, "5500": 1, "5000": 1, "VERSION": 1, "STRING": 4},
    "inspace.scr": {"ICON": 1, "GROUP_ICON": 1, "RCDATA": 1, "DIALOG": 4, "STRING": 2},
    "install.exe": {"ICON": 1, "GROUP_ICON": 1, "5500": 2, "RCDATA": 1, "DIALOG": 9},
    "ispace.dll": {"5000": 1, "5500": 1, "5600": 1, "ICON": 3, "GROUP_ICON": 3, "BITMAP": 1,
                   "4000": 3, "STRING": 1},
    "snd_fx.dll": {"100": 46},
    "wavemix.dll": {"VERSION": 1},
    "eng_fx.dll": {"100": 32},
    "theworks-factory.exe": {"ICON": 1, "GROUP_ICON": 1, "RCDATA": 1, "DIALOG": 1, "MENU": 1,
                             "VERSION": 1, "STRING": 5},
    "theworks-setup.exe": {"ICON": 1, "GROUP_ICON": 1, "RCDATA": 1, "DIALOG": 4, "STRING": 4},
}
EXPECTED_STRINGS = {"eng_lang.dll": 1446, "factory.exe": 4, "inspace.exe": 4, "inspace.scr": 10,
                    "ispace.dll": 1, "theworks-factory.exe": 10, "theworks-setup.exe": 27}
EXPECTED_SNIFFED = {   # kinds of RCDATA and custom-type resources
    "eng_lang.dll": {"text": 58},
    "igraphic.dll": {"zeros": 1, "bmp-file": 113, "binary": 5},
    "inrun.ad": {"binary": 4, "text": 3},
    "inspace.exe": {"zeros": 1, "text": 1},
    "inspace.scr": {"text": 1},
    "install.exe": {"zeros": 2, "text": 1},
    "ispace.dll": {"binary": 3, "bmp-file": 3},
    "snd_fx.dll": {"wav": 46},
    "eng_fx.dll": {"wav": 32},
    "theworks-factory.exe": {"text": 1},
    "theworks-setup.exe": {"text": 1},
}
ENG_LANG_DIALOGS = {
    432: "Inner Space Demo Options", 321: "New Game", 100: "Inner Space Options/Setup",
    300: "Import Ship Body", 169: "Ship Behaviour", 139: "Ship Handling", 101: "Inner Space",
    194: "Ambulance Options (To Buy)", 378: "Ship Options (To Sell)", 267: "Restore Game",
    167: "Save Game", 286: "New Ship Body", 589: "New Demo", 244: "Inner Space", 404: "Search",
    975: "Joystick Configuration", 693: "Order Inner Space Now!", 666: "Display Options",
    999: "Audio Options", 988: "Important Information!", 799: "Registration Benefits",
    1872: "Inner Space Plus Pack!",
}
OTHER_DIALOGS = {
    "inspace.scr": {2003: "Operation: Inner Space", 2000: "Change Password",
                    2001: "<name of screensaver>", 2002: "<name of screen saver>"},
    "install.exe": {135: " Inner Space", 300: "Inner Space New Valid Install", 100: "Inner Space",
                    1922: "Game Difficulty and Speed", 1774: "Play Full Screen or in a Window",
                    891: "Inner Space", 200: "Installing...", 175: "Inner Space", 333: "Master Disk"},
    "theworks-factory.exe": {963: "Import Ship Body"},
    "theworks-setup.exe": {100: "Inner Space", 300: "Inner Space Add-on Install",
                           200: "Installing...", "STARTDLG": "Inner Space"},
}
DEMO_MESSAGE = ("With Limited Inner Space, you can only use the Transformer ship. "
                "If you order the full game package, all ships are available.")


@pytest.fixture(scope="module")
def assets(tmp_path_factory):
    for src in ("shipdisk", "theworks"):
        if not (ORIGINAL / src).is_dir():
            pytest.skip("original/ is absent (original game files are local only)")
    root = tmp_path_factory.mktemp("res")
    for src in ("shipdisk", "theworks"):
        szdd.extract_dir(ORIGINAL / src, root / "extracted" / src)
    result = res.generate(root / "extracted", root / "assets", log=lambda s: None)
    return root, result


def _index(assets, key: str) -> dict:
    return json.loads((assets[0] / "assets" / key / "index.json").read_text(encoding="utf-8"))


def _json(assets, key: str, name: str):
    return json.loads((assets[0] / "assets" / key / name).read_text(encoding="utf-8"))


def test_inventory_counts(assets):
    root, result = assets
    assert result.ok, [m.errors for m in result.modules]
    assert {m.key: m.counts for m in result.modules} == EXPECTED_COUNTS
    assert {m.key: m.index["strings"] for m in result.modules if m.index["strings"]} == EXPECTED_STRINGS
    std = {"STRING", "DIALOG", "MENU", "ACCELERATOR", "VERSION", "BITMAP", "ICON", "GROUP_ICON"}
    sniffed = {}
    for m in result.modules:
        c = collections.Counter(e["kind"] for e in m.index["resources"] if e["type"] not in std)
        if c:
            sniffed[m.key] = dict(c)
    assert sniffed == EXPECTED_SNIFFED
    summary = json.loads((root / "assets" / "modules.json").read_text(encoding="utf-8"))
    assert [s["module"] for s in summary] == list(EXPECTED_COUNTS)


def test_eng_lang(assets):
    dialogs = _json(assets, "eng_lang.dll", "dialogs.json")
    assert {d["id"]: d["caption"] for d in dialogs} == ENG_LANG_DIALOGS
    menus = _json(assets, "eng_lang.dll", "menus.json")
    assert {m["id"]: [i["text"] for i in m["items"]] for m in menus} == {
        "POPUPMENU": ["FOO-Dummy Popup"],
        110: ["&Factory", "&Ship", "&Body", "He&lp"],
        101: ["Ga&me", "Ship &Cmds", "De&fenders", "Wa&tch", "He&lp"],
        102: ["Ga&me", "St&rategy", "Ship &Cmds", "De&fenders", "Wa&tch", "He&lp"],
    }
    accels = {a["id"]: a["entries"] for a in _json(assets, "eng_lang.dll", "accelerators.json")}
    assert sorted(accels) == ["FACTACCEL", "MAINACCEL"]
    assert (len(accels["MAINACCEL"]), len(accels["FACTACCEL"])) == (26, 29)
    assert {"key_name": "VK_F1", "cmd": 240} in [
        {"key_name": e["key_name"], "cmd": e["cmd"]} for e in accels["MAINACCEL"]]
    for table in accels.values():   # Alt+G and Alt+X carry the POPUPMENU command ids
        alt = {e["key_name"]: e["cmd"] for e in table if e["flag_names"] == ["ALT"]}
        assert (alt['"G"'], alt['"g"'], alt['"X"'], alt['"x"']) == (192, 192, 176, 176)
    popup = {m["id"]: m for m in menus}["POPUPMENU"]["items"][0]["items"]
    assert [(i.get("id"), i["text"]) for i in popup] == [
        (208, "Options Menu"), (0, ""), (192, "&Game Options.."), (176, "E&xit Game")]
    idx = _index(assets, "eng_lang.dll")
    texts = [e for e in idx["resources"] if e["type"] == "5000"]
    assert len(texts) == 57 and all(e["kind"] == "text" for e in texts)
    assert sorted(e["id"] for e in texts) == (
        list(range(101, 106)) + list(range(110, 139)) + list(range(180, 184)) + [200, 210]
        + list(range(300, 314)) + [320, 321, 330])
    strings = _json(assets, "eng_lang.dll", "strings.json")
    assert (strings[0]["id"], strings[-1]["id"]) == (110, 12546)
    root = assets[0] / "assets"
    rcdata = [(k, e["id"], e["kind"]) for k in EXPECTED_COUNTS for e in _index(assets, k)["resources"]
              if e["type"] == "RCDATA"]
    assert len(rcdata) == 5 and all(rid == "DLGINCLUDE" and kind == "text" for _, rid, kind in rcdata)
    imports = {k: [c["text"] for c in {d["id"]: d for d in _json(assets, k, "dialogs.json")}[i]["controls"]]
               for k, i in (("eng_lang.dll", 300), ("theworks-factory.exe", 963))}
    assert "&Looping" in imports["theworks-factory.exe"] and "&Looping" not in imports["eng_lang.dll"]
    assert (root / "eng_lang.dll" / "RCDATA" / "DLGINCLUDE.txt").read_bytes() == b"C:\\INTEXT\\DEMODLG.H"
    shifts = {k: _index(assets, k)["resource_align_shift"] for k in EXPECTED_COUNTS}
    assert {k for k, v in shifts.items() if v != 4} == {"wavemix.dll"} and shifts["wavemix.dll"] == 9


def test_known_strings(assets):
    strings = {s["id"]: s["text"] for s in _json(assets, "eng_lang.dll", "strings.json")}
    assert len(strings) == 1446
    assert strings[2326] + strings[2327] == DEMO_MESSAGE
    assert strings[2338] == "Limited Inner Space"
    assert strings[1149] == "NEW GAME, STARTING AT WAVE 1."
    assert strings[2140] == "Inner Space Navigation"
    assert strings[3032] == "IT'S %<%S %S%>%<%S%>, %s!%NI HOPE YOU'RE READY TO PLAY..%N%N"
    assert (strings[597], strings[639]) == ("Saturday", "Morning")
    tsv = (assets[0] / "assets" / "eng_lang.dll" / "strings.tsv").read_text(encoding="utf-8")
    assert "2326\tWith Limited Inner Space, you can only use the Transformer ship.\n" in tsv
    assert len(tsv.splitlines()) == 1 + 1446
    intro = (assets[0] / "assets" / "eng_lang.dll" / "5000" / "210.txt").read_text(encoding="utf-8")
    assert intro.startswith("YOUR COMPUTER HAS BEEN INVADED!")
    ver = {s["id"]: s["text"] for s in _json(assets, "inspace.exe", "strings.json")}
    assert ver == {167: "129", 233: "08", 607: "Aug 9/2000", 1800: "Inner Space"}


def test_new_game_dialog(assets):
    """Template #0141 holds the fields the oracle saw (oracle/digests/newgame-0001.md)."""
    d = {x["id"]: x for x in _json(assets, "eng_lang.dll", "dialogs.json")}[0x0141]
    assert d["caption"] == "New Game"
    assert (d["cx"], d["cy"], d["font"]) == (198, 117, {"point_size": 8, "face": "Helv"})
    labels = [c for c in d["controls"] if c["class"] == "STATIC" and c["text"]]
    assert [c["text"] for c in labels] == ["PILOT:", "SHIP:", "TEAM:", "PLAY:", "DRIVE:", "ZOOM LEVEL:"]
    assert all("SS_RIGHT" in c["style_names"] for c in labels)
    assert "BS_DEFPUSHBUTTON" in {c["id"]: c for c in d["controls"]}[1]["style_names"]
    by_id = {c["id"]: c for c in d["controls"]}
    assert by_id[322]["class"] == "EDIT"                                   # pilot name
    assert by_id[324]["class"] == "COMBOBOX" and "CBS_SORT" in by_id[324]["style_names"]
    assert all("CBS_DROPDOWNLIST" in by_id[i]["style_names"] for i in (324, 325, 330, 332))
    assert by_id[326]["class"] == "STATIC" and by_id[326]["text"] == ""   # team follows the ship
    assert [by_id[1]["text"], by_id[333]["text"]] == ["&OK", "&Cancel"]
    assert all(c["class"] != "LevClass" for c in d["controls"])          # preview made at run time
    restore = {x["id"]: x for x in _json(assets, "eng_lang.dll", "dialogs.json")}[0x010B]
    assert restore["caption"] == "Restore Game"


def test_other_dialogs_and_versions(assets):
    for key, want in OTHER_DIALOGS.items():
        assert {d["id"]: d["caption"] for d in _json(assets, key, "dialogs.json")} == want
    versions = {k: _json(assets, k, "version.json")[0]["string_file_info"]["040904E4"]["FileVersion"]
                for k in ("inspace.exe", "factory.exe", "theworks-factory.exe", "ctl3d.dll",
                          "wavemix.dll")}
    assert versions == {"inspace.exe": "1.29.08", "factory.exe": "1.29.02",
                        "theworks-factory.exe": "1.49.00", "ctl3d.dll": "2.05", "wavemix.dll": "1.70"}


def test_every_bitmap_decodes(assets):
    root = assets[0] / "assets"
    n = 0
    for bmp in sorted(root.glob("*/**/*.bmp")):
        b = bmp.read_bytes()
        trimmed, info = F.bmp_file_info(b)
        assert trimmed == b and info["bf_size"] == len(b)
        assert 1 <= info["width"] <= 1024 and 1 <= abs(info["height"]) <= 768
        assert info["bpp"] in (1, 4) and info["compression"] == 0
        assert len(info["palette"]) == info["colors"] == 1 << info["bpp"]
        rows = F.pixel_rows(b, F.dib_header(b, 14), 14 + info["bits_offset"])
        assert len(rows) == abs(info["height"]) and max(max(r) for r in rows) < info["colors"]
        n += 1
    assert n == 1 + 128 + 4   # CTL3D, IGRAPHIC, ISPACE


def test_igraphic_4000(assets):
    idx = _index(assets, "igraphic.dll")
    names = sorted(e["id"] for e in idx["resources"] if e["type"] == "BITMAP")
    assert names == ["ALPHABET", "BW_BLOCKS", "BW_MBLOCKS", "CORNER", "CRACKS", "DEF", "DIGITAL",
                     "FILES", "FLDR", "METAL", "OUT_SIGN", "PIECE", "RADSHIP", "ROCKS", "SYSTEMS"]
    dibs = [e for e in idx["resources"] if e["type"] == "4000"]
    assert len(dibs) == 113
    pal15_black = corners15 = strips = 0
    for e in dibs:
        assert (e["kind"], e["bpp"], e["colors"], e["compression"]) == ("bmp-file", 4, 16, 0)
        b = (assets[0] / "assets" / "igraphic.dll" / e["output"]).read_bytes()
        info = F.dib_header(b, 14)
        rows = F.pixel_rows(b, info, 14 + info["bits_offset"])
        pal15_black += info["palette"][15] == "000000"
        corners15 += rows[0][0] == rows[0][-1] == rows[-1][0] == rows[-1][-1] == 15
        strips += info["width"] % info["height"] == 0 and info["width"] // info["height"] >= 2
    assert (pal15_black, corners15, strips) == (110, 90, 20)
    same = [e for e in dibs if "same_as" in e]
    assert len(same) == 53 and all(s.startswith("theworks/") and s.endswith(".DIB")
                                   for e in same for s in e["same_as"])
    by_id = {e["id"]: e for e in dibs}
    assert by_id[60]["same_as"] == ["theworks/XFORM.DIB"] and (by_id[60]["width"], by_id[60]["height"]) == (132, 33)
    assert (by_id[170]["width"], by_id[170]["height"]) == (248, 52)
    named = [e for e in idx["resources"] if e["type"] == "BITMAP"]
    assert collections.Counter(e["bpp"] for e in named) == {1: 10, 4: 5}
    ispace = {e["id"]: e for e in _index(assets, "ispace.dll")["resources"] if e["type"] == "4000"}
    assert (ispace[221]["width"], ispace[221]["height"]) == (360, 40)


def test_every_wav_parses(assets):
    for key, n, lo, hi, same in (("snd_fx.dll", 46, 100, 178, 45), ("eng_fx.dll", 32, 138, 180, 25)):
        entries = _index(assets, key)["resources"]
        assert len(entries) == n
        assert (min(e["id"] for e in entries), max(e["id"] for e in entries)) == (lo, hi)
        assert sum("same_as" in e for e in entries) == same
        for e in entries:
            blob = (assets[0] / "assets" / key / e["output"]).read_bytes()
            trimmed, info = F.wav_info(blob)
            assert trimmed == blob
            assert (info["format_tag"], info["rate"], info["channels"], info["bits"]) == (1, 11025, 1, 8)
            assert info["frames"] == info["data_bytes"] > 0
    eng = {e["id"]: e for e in _index(assets, "eng_fx.dll")["resources"]}
    assert eng[156]["same_as"] == ["theworks/ATTACK.WAV"]


def test_icons(assets):
    root = assets[0] / "assets"
    icos = sorted(root.glob("*/GROUP_ICON/*.ico"))
    assert len(icos) == 10   # ISPACE 3, seven other modules 1 each
    for ico in icos:
        b = ico.read_bytes()
        reserved, kind, count = struct.unpack_from("<HHH", b, 0)
        assert (reserved, kind) == (0, 1) and count >= 1
        for i in range(count):
            w, h, _c, _r, planes, bpp, size, off = struct.unpack_from("<BBBBHHII", b, 6 + 16 * i)
            assert (w, h) in ((32, 32), (16, 16)) and bpp == 4 and off + size <= len(b)
            assert F.dib_header(b, off)["width"] == w


def test_custom_data(assets):
    ispace = {(e["type"], e["id"]): e for e in _index(assets, "ispace.dll")["resources"]}
    perm = (assets[0] / "assets" / "ispace.dll" / ispace[("5000", 150)]["output"]).read_bytes()
    body = perm.rstrip(b"\0")
    assert len(body) == len(set(body)) == 88 and 0 not in body
    assert ispace[("5500", 100)]["size"] == 288
    assert ispace[("5600", 135)]["size"] == 128
    words = struct.unpack("<64h", (assets[0] / "assets" / "ispace.dll" / "5600" / "135.bin").read_bytes())
    assert max(abs(w) for w in words) == 250
    unreg = (assets[0] / "assets" / "inspace.exe" / "5000" / "143.txt").read_text(encoding="utf-8")
    assert unreg.rstrip(" ") == "Unregistered Copy   ILLEGAL" and len(unreg) == 50
    six = [e for e in _index(assets, "igraphic.dll")["resources"] if e["type"] == "6600"]
    assert [(e["id"], e["size"]) for e in six] == [(100, 2496), (105, 3520), (110, 2432),
                                                   (115, 1056), (120, 1088)]
    zeros = {(k, e["id"], e["size"]) for k in EXPECTED_COUNTS for e in _index(assets, k)["resources"]
             if e["kind"] == "zeros"}
    assert zeros == {("igraphic.dll", 110, 32), ("inspace.exe", 110, 32), ("install.exe", 110, 32),
                     ("install.exe", 120, 128)}
    inrun = (assets[0] / "assets" / "inrun.ad" / "2000" / "10.txt").read_text(encoding="utf-8")
    assert inrun.startswith("Inner Space (tm) Starter by Bill Stewart.")
    labels = [(assets[0] / "assets" / "inrun.ad" / "1000" / f"{i}.bin").read_bytes()[2:].split(b"\0")[0]
              for i in range(1, 5)]
    assert labels == [b"Allow Game Play", b"Condition:", b"Configure..", b"Sound:"]
    starts = {(assets[0] / "assets" / "igraphic.dll" / e["output"]).read_bytes()[:6] for e in six}
    assert starts == {bytes.fromhex("ff00ff00ff00")}


def test_determinism(assets, tmp_path):
    root, _ = assets
    again = res.generate(root / "extracted", tmp_path / "assets", log=lambda s: None)
    a = {p.relative_to(root / "assets"): p.read_bytes() for p in (root / "assets").rglob("*") if p.is_file()}
    b = {p.relative_to(tmp_path / "assets"): p.read_bytes() for p in (tmp_path / "assets").rglob("*")
         if p.is_file()}
    assert a == b
    assert again.written == len(a) - 1   # modules.json is counted separately
    rerun = res.generate(root / "extracted", tmp_path / "assets", log=lambda s: None)
    assert rerun.written == 0 and sum(m.removed for m in rerun.modules) == 0
