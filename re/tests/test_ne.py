from __future__ import annotations

import json
import struct

import pytest

from isre import szdd
from isre.ne import NEError, NEModule, ghidra_selector, is_ne

# ---------------------------------------------------------------- synthetic module


def pstr(s: str) -> bytes:
    return bytes([len(s)]) + s.encode("latin-1")


def build_ne() -> bytes:
    """A minimal NE image exercising string resource types/names and entry bundles."""
    ne_off = 0x40
    segtab = struct.pack("<HHHH", 0, 0, 0x0001, 0x10)
    # resource table: string type "MYTYPE" with string id "MYRES"; STRING (6) with id 7
    rs_head_len = 2 + (8 + 12) * 2 + 2
    names = pstr("MYTYPE") + pstr("MYRES") + b"\x00"
    t_off, n_off = rs_head_len, rs_head_len + 7
    rsrc = struct.pack("<H", 4)
    rsrc += struct.pack("<HHI", t_off, 1, 0) + struct.pack("<HHHHHH", 0x10, 1, 0x30, n_off, 0, 0)
    rsrc += struct.pack("<HHI", 0x8006, 1, 0) + struct.pack("<HHHHHH", 0x11, 2, 0x1030, 0x8007, 0, 0)
    rsrc += struct.pack("<H", 0) + names
    restab = pstr("TEST") + struct.pack("<H", 0) + pstr("FOO") + struct.pack("<H", 1) + b"\x00"
    modtab = struct.pack("<H", 1)
    imptab = b"\x00" + pstr("KERNEL")
    enttab = (bytes([1, 0xFF, 0x03, 0xCD, 0x3F, 1]) + struct.pack("<H", 4)   # ordinal 1, movable
              + bytes([2, 0x00])                                             # ordinals 2-3 unused
              + bytes([1, 1, 0x01]) + struct.pack("<H", 8)                   # ordinal 4, fixed
              + b"\x00")
    o_seg = 0x40
    o_rsrc = o_seg + len(segtab)
    o_res = o_rsrc + len(rsrc)
    o_mod = o_res + len(restab)
    o_imp = o_mod + len(modtab)
    o_ent = o_imp + len(imptab)
    nres_file = ne_off + o_ent + len(enttab)
    nrestab = pstr("A test module") + struct.pack("<H", 0) + b"\x00"
    hdr = bytearray(0x40)
    hdr[0:2] = b"NE"
    hdr[2], hdr[3] = 5, 10
    struct.pack_into("<HH", hdr, 0x04, o_ent, len(enttab))
    struct.pack_into("<HH", hdr, 0x0C, 0x8001, 1)
    struct.pack_into("<HHHH", hdr, 0x1C, 1, 1, len(nrestab), o_seg)
    struct.pack_into("<HHHH", hdr, 0x24, o_rsrc, o_res, o_mod, o_imp)
    struct.pack_into("<IHHH", hdr, 0x2C, nres_file, 1, 4, 0)
    hdr[0x36] = 2
    mz = bytearray(0x40)
    mz[0:2] = b"MZ"
    struct.pack_into("<I", mz, 0x3C, ne_off)
    img = bytes(mz) + bytes(hdr) + segtab + rsrc + restab + modtab + imptab + enttab + nrestab
    return img + bytes(0x140 - len(img))


def test_synthetic_module():
    m = NEModule(build_ne(), "synthetic")
    assert m.module_name == "TEST" and m.description == "A test module"
    assert m.is_library and m.modules == ["KERNEL"]
    assert m.segments[0].is_data and m.segments[0].minalloc == 0x10
    r0, r1 = m.resources
    assert (r0.type_id, r0.type_name, r0.id) == ("MYTYPE", "MYTYPE", "MYRES")
    assert (r0.file_off, r0.length) == (0x100, 16)
    assert (r1.type_id, r1.type_name, r1.id) == (6, "STRING", 7)
    assert (r1.file_off, r1.length) == (0x110, 32)
    assert [(e.ordinal, e.seg, e.offset, e.movable) for e in m.entries] == [(1, 1, 4, True), (4, 1, 8, False)]
    assert m.imported_names == [{"offset": 1, "name": "KERNEL"}]
    json.dumps(m.to_json())
    assert "MYTYPE" in m.summary()


def test_rejects_non_ne():
    with pytest.raises(NEError):
        NEModule(b"MZ" + bytes(0x40))
    assert not is_ne(b"PK\x03\x04")


def test_ghidra_selector():
    assert ghidra_selector(1) == 0x1000
    assert ghidra_selector(4) == 0x1018
    assert ghidra_selector(19) == 0x1090


# ---------------------------------------------------------------- original files

def _ne_files(original_dir):
    out = []
    for sub in ("game", "theworks"):
        for p in sorted((original_dir / sub).iterdir()):
            if not p.is_file():
                continue
            data = p.read_bytes()
            if szdd.is_szdd(data):
                data = szdd.expand(data)
            if is_ne(data):
                out.append((f"{sub}/{p.name}", data))
    return out


def test_parses_every_ne_module(original_dir):
    mods = _ne_files(original_dir)
    assert len(mods) == 14
    for name, data in mods:
        m = NEModule(data, name)
        assert m.module_name, name
        for s in m.segments:
            if s.has_file_data:
                assert s.file_off + s.file_len <= len(data), (name, s.n)
        for r in m.resources:
            assert r.file_off + r.raw_length <= len(data), (name, r.type_name, r.id)
        for e in m.entries:
            assert e.seg == 0xFE or 1 <= e.seg <= len(m.segments), (name, e.ordinal)
        json.dumps(m.to_json())
        m.summary()


def test_inspace_resources(inspace):
    got = [(r.type_name, r.id, r.length) for r in inspace.resources]
    assert sorted(got, key=str) == sorted([
        ("ICON", 1, 752),
        ("ICON", 2, 304),
        ("GROUP_ICON", "GAME", 48),
        ("5500", 110, 32),
        ("5000", 143, 64),
        ("VERSION", 1, 576),
        ("STRING", 11, 32),
        ("STRING", 15, 32),
        ("STRING", 113, 32),
        ("STRING", 38, 32),
    ], key=str)
    by = {(r.type_name, r.id): r for r in inspace.resources}
    assert by[("5000", 143)].type_id == 5000 and by[("5500", 110)].type_id == 5500
    # file offsets are rnOffset << rscAlignShift and land on the resource data
    ver = by[("VERSION", 1)]
    assert inspace.data[ver.file_off + 4:ver.file_off + 19] == b"VS_VERSION_INFO"
    (cb,) = struct.unpack_from("<H", inspace.data, ver.file_off)
    assert cb <= ver.length
    grp = by[("GROUP_ICON", "GAME")]
    assert struct.unpack_from("<HHH", inspace.data, grp.file_off) == (0, 1, 2)
    assert all(r.file_off >= inspace.segments[-1].file_off + inspace.segments[-1].file_len
               for r in inspace.resources)


def test_inspace_header_and_modules(inspace):
    assert inspace.module_name == "INSPACE"
    assert inspace.modules == ["KERNEL", "USER", "GDI", "MMSYSTEM", "SHELL"]
    assert inspace.flags == 0x0B0A and inspace.self_loading and not inspace.is_library
    assert inspace.autodata == 19
    assert (inspace.header["cs"], inspace.header["ip"]) == (5, 0x1A)


def test_inspace_segments(inspace):
    segs = inspace.segments
    assert len(segs) == 19
    assert segs[0].flags == 0x0D50 and segs[0].is_code and segs[0].file_len == 2164
    assert segs[18].flags == 0x0C59 and segs[18].is_data
    assert [s.n for s in segs if s.is_data] == [19]
    assert [s.n for s in segs if not s.flags & 0x0040] == list(range(9, 19))  # not PRELOAD


def test_inspace_entries(inspace):
    ents = {e.ordinal: e for e in inspace.entries}
    assert len(ents) == 12 and 3 not in ents
    assert (ents[1].seg, ents[1].offset) == (2, 0x224)
    assert all(e.exported and not e.movable for e in ents.values())


def test_optloader_stub_relocations_are_standard(original_dir):
    expected = [
        ("SEGMENT", "INTERNALREF", 0x1A),
        ("FAR_ADDR", "IMPORTORDINAL", 0x28),
        ("OFFSET", "IMPORTORDINAL", 0x2C),
    ]
    for name, data in _ne_files(original_dir):
        m = NEModule(data, name)
        if not m.optloader()["packed"]:
            continue
        got = [(r.to_json()["src_type"], r.to_json()["target_type"], r.offset) for r in m.std_relocs[1]]
        assert got == expected, name
        assert [r.target.get("ordinal") for r in m.std_relocs[1]] == [None, 170, 114], name
        assert list(m.std_relocs) == [1], name


def test_optloader_detection(original_dir):
    mods = {name: NEModule(data, name) for name, data in _ne_files(original_dir)}
    packed = {n: m.optloader() for n, m in mods.items() if m.optloader()["packed"]}
    assert sorted(packed) == [
        "game/FACTORY.ex_", "game/INSPACE.ex_", "game/INSTALL.exe",
        "game/ISPACE.dll", "theworks/FACTORY.EXE", "theworks/SETUP.EXE",
    ]
    assert packed["game/INSPACE.ex_"]["packed_segments"] == list(range(2, 20))
    stubs = {mods[n].segment_file_bytes(1) for n in packed}
    assert len(stubs) == 1 and len(stubs.pop()) == 2164
    isp = mods["game/ISPACE.dll"]
    assert isp.flags == 0x8B09 and isp.is_library
    assert len(isp.segments) == 11 and len(packed["game/ISPACE.dll"]["packed_segments"]) == 10
    for n in ("game/FACTORY.ex_", "theworks/FACTORY.EXE"):
        assert len(mods[n].segments) == 6 and len(packed[n]["packed_segments"]) == 5
    assert not mods["game/INSPACE.sc1"].optloader()["packed"]
