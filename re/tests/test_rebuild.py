"""isre.rebuild (unpacked NE with standard relocations) and isre.neload (symbolic loader)."""
from __future__ import annotations

import struct
from collections import Counter

import pytest
from conftest import require

from isre import neload, optloader, rebuild
from isre.ne import NEModule
from isre.rebuild import StdRecord

# ---------------------------------------------------------------- synthetic NE


def _pstr(s: str) -> bytes:
    return bytes([len(s)]) + s.encode("latin-1")


def mini_ne(segs, relocs=None, modules=("KERNEL",), flags=0x030A) -> bytes:
    """A small NE file: segs = [(data, seg flags, minalloc)], relocs = {n: [StdRecord]}."""
    relocs = relocs or {}
    shift = 4
    segtab = 0x40
    rsrctab = segtab + 8 * len(segs)
    restab = rsrctab                                  # no resources
    resident = _pstr("MINI") + b"\0\0" + b"\0"
    modtab = restab + len(resident)
    imptab = modtab + 2 * len(modules)
    imp = b"\0"
    offs = []
    for m in modules:
        offs.append(len(imp))
        imp += _pstr(m)
    enttab = imptab + len(imp)
    ent = b"\0"
    nres = _pstr("mini test module") + b"\0\0" + b"\0"
    ne_off = 0x40
    nrestab = ne_off + enttab + len(ent)
    out = bytearray(b"MZ" + bytes(0x3A) + struct.pack("<I", ne_off))
    hdr = bytearray(0x40)
    hdr[0:2] = b"NE"
    struct.pack_into("<HH", hdr, 0x04, enttab, len(ent))
    struct.pack_into("<HHHH", hdr, 0x0C, flags, 0, 0, 0)
    struct.pack_into("<HHHH", hdr, 0x14, 0, 1, 0, 0)
    struct.pack_into("<HHH", hdr, 0x1C, len(segs), len(modules), len(nres))
    struct.pack_into("<HHHHH", hdr, 0x22, segtab, rsrctab, restab, modtab, imptab)
    struct.pack_into("<IHH", hdr, 0x2C, nrestab, 0, shift)
    hdr[0x36] = 2
    out += hdr
    out += bytes(8 * len(segs))
    out += resident + b"".join(struct.pack("<H", o) for o in offs) + imp + ent + nres
    for i, (data, sflags, minalloc) in enumerate(segs):
        n = i + 1
        pos = (len(out) + 15) & ~15
        out += bytes(pos - len(out))
        out += data
        recs = relocs.get(n, [])
        if recs:
            sflags |= 0x0100
            out += struct.pack("<H", len(recs)) + b"".join(r.pack() for r in recs)
        struct.pack_into("<HHHH", out, ne_off + segtab + i * 8, pos >> shift, len(data),
                         sflags, minalloc)
    return bytes(out)


def _words(*ws: int) -> bytes:
    return struct.pack(f"<{len(ws)}H", *ws)


def test_mini_ne_parses():
    mod = NEModule(mini_ne([(bytes(4), 0x0010, 8)]))
    assert mod.modules == ["KERNEL"] and mod.module_name == "MINI"
    assert mod.segment(1).file_len == 4 and not mod.self_loading


def test_standard_loader_chains_and_additive():
    # far-pointer chain 0x00 -> 0x08 -> end; selector chain 0x10 -> 0x14 -> end;
    # additive offset at 0x18 over base 5
    data = bytearray(0x20)
    data[0x00:0x04] = _words(0x0008, 0x1234)
    data[0x08:0x0C] = _words(0xFFFF, 0x0000)
    data[0x10:0x12] = _words(0x0014)
    data[0x14:0x16] = _words(0xFFFF)
    data[0x18:0x1A] = _words(5)
    recs = [StdRecord(3, 1, 0x00, 1, 91), StdRecord(2, 0, 0x10, 2, 0),
            StdRecord(5, 1 | 4, 0x18, 1, 7)]
    mod = NEModule(mini_ne([(bytes(data), 0x0010, 0x28), (bytes(4), 0x0010, 4)], {1: recs}))
    res = neload.load_standard(mod)
    assert not res.errors
    img = res.images[1]
    off, sel = ("IMP", "KERNEL", 91, "off"), ("IMP", "KERNEL", 91, "sel")
    for site in (0x00, 0x08):
        assert img[site:site + 4] == [(off, 0), (off, 1), (sel, 0), (sel, 1)]
    seg2 = ("SEG", 2, "sel")
    assert img[0x10:0x12] == img[0x14:0x16] == [(seg2, 0), (seg2, 1)]
    k7 = ("+", ("IMP", "KERNEL", 7, "off"), 5)
    assert img[0x18:0x1A] == [(k7, 0), (k7, 1)]
    assert len(img) == 0x28 and img[0x20:] == [0] * 8        # zero fill to minalloc
    assert res.patched_bytes == 4 + 4 + 2 + 2 + 2


def test_standard_loader_reports_loader_disagreements():
    data = bytearray(0x10)
    data[0:2] = _words(0xFFFF)
    data[4:8] = _words(0, 0x0042)                     # additive FAR_ADDR over selector 0x42
    recs = [StdRecord(5, 1, 0, 1, 1), StdRecord(5, 1, 0, 1, 2), StdRecord(3, 5, 4, 1, 3)]
    res = neload.load_standard(NEModule(mini_ne([(bytes(data), 0x0010, 0x10)], {1: recs})))
    assert any("patched twice" in e for e in res.errors)
    assert any("additive FAR_ADDR" in e for e in res.errors)


def test_optloader_loader_additive_selector_differs():
    img = bytearray(8)
    img[0:4] = _words(0xFFFB, 0x0003)
    rec = {"seg": 2, "kind": "import_ordinal", "target_type": "PTR16_A", "offset": 0,
           "module_idx": 1, "ordinal": 91, "module": "KERNEL"}
    s = optloader.SegmentResult(2, 0, 0, 0, 8, True, bytes(img), w0=1, decomp_len=8,
                                records=[rec])
    mod = NEModule(mini_ne([(bytes(2), 0x0010, 2), (bytes(8), 0x0010, 8)]))
    res = neload.LoadResult({}, {})
    seg = neload._Seg(2, s.image, 8, res)
    neload.apply_optloader(seg, mod, s)
    off, sel = ("IMP", "KERNEL", 91, "off"), ("IMP", "KERNEL", 91, "sel")
    assert seg.img[0:2] == [(("+", off, 0xFFFB), 0), (("+", off, 0xFFFB), 1)]
    assert seg.img[2:4] == [(("+", sel, 3), 0), (("+", sel, 3), 1)]   # OPTLOADER adds


@pytest.mark.parametrize("rec,expect", [
    ({"kind": "special_base", "offset": 0x24, "selector_idx": 7}, (2, 0, 0x24, 7, 0)),
    ({"kind": "import_ordinal", "target_type": "PTR16", "offset": 1, "module_idx": 2,
      "ordinal": 91}, (3, 1, 1, 2, 91)),
    ({"kind": "import_ordinal", "target_type": "PTR16_A", "offset": 1, "module_idx": 1,
      "ordinal": 91}, (3, 5, 1, 1, 91)),
    ({"kind": "import_ordinal", "target_type": "OFF16", "offset": 2, "module_idx": 1,
      "ordinal": 114}, (5, 1, 2, 1, 114)),
    ({"kind": "import_ordinal", "target_type": "SEG16_A", "offset": 2, "module_idx": 1,
      "ordinal": 114}, (2, 5, 2, 1, 114)),
    ({"kind": "import_name", "target_type": "PTR16", "offset": 3, "module_idx": 1,
      "imptab_off": 9}, (3, 2, 3, 1, 9)),
    ({"kind": "internal", "target_type": "PTR16", "offset": 4, "ref_seg_raw": 0xFF,
      "value": 12}, (3, 0, 4, 0xFF, 12)),
    ({"kind": "internal", "target_type": "OFF16_A", "offset": 4, "ref_seg_raw": 3,
      "value": 0x40}, (5, 4, 4, 3, 0x40)),
    ({"kind": "import_ordinal", "target_type": "BYTE_A", "offset": 5, "module_idx": 1,
      "ordinal": 1}, (0, 5, 5, 1, 1)),
])
def test_record_mapping(rec, expect):
    r = rebuild.to_standard(dict(rec, seg=2))
    assert (r.src_type, r.flags, r.offset, r.target1, r.target2) == expect
    assert struct.unpack("<BBHHH", r.pack()) == expect


@pytest.mark.parametrize("rec", [
    {"kind": "import_ordinal", "target_type": "BYTE", "offset": 0, "module_idx": 1, "ordinal": 1},
    {"kind": "osfloat", "target_type": "OFF16", "offset": 0, "n": 1},
])
def test_unsupported_records_are_refused(rec):
    with pytest.raises(rebuild.RebuildError):
        rebuild.to_standard(dict(rec, seg=2))


# ---------------------------------------------------------------- game modules

MODULES = [
    # (path under original/, segments, OPTLOADER records)
    ("shipdisk/INSPACE.ex_", 19, 930),
    ("shipdisk/ISPACE.dll", 11, 343),
    ("shipdisk/FACTORY.ex_", 6, 238),
    ("theworks/FACTORY.EXE", 6, 250),
    ("shipdisk/INSTALL.exe", 4, 116),
    ("theworks/SETUP.EXE", 4, 107),
]
_CACHE: dict = {}


def built(original_dir, rel):
    if rel not in _CACHE:
        mod = NEModule.load(require(original_dir / rel))
        up = optloader.unpack(mod)
        rb = rebuild.rebuild(mod, up)
        _CACHE[rel] = (mod, up, rb, NEModule(rb.data, "rebuilt"))
    return _CACHE[rel]


ids = [m[0] for m in MODULES]


@pytest.mark.parametrize("rel,nseg,nrec", MODULES, ids=ids)
def test_rebuilt_is_loader_equivalent(original_dir, rel, nseg, nrec):
    mod, up, rb, new = built(original_dir, rel)
    report = neload.check(new, mod, up)
    assert report.ok, report.summary()
    assert report.segments == nseg and report.patched_bytes > 0
    assert sum(len(s.records) for s in rb.segments) == nrec
    assert sum(len(v) for k, v in new.std_relocs.items() if k > 1) == nrec


@pytest.mark.parametrize("rel,nseg,nrec", MODULES, ids=ids)
def test_rebuilt_structure(original_dir, rel, nseg, nrec):
    mod, up, rb, new = built(original_dir, rel)
    assert not new.self_loading and new.flags == mod.flags & ~0x0800
    for k, v in mod.header.items():
        if k not in ("flags", "crc"):
            assert new.header[k] == v, k
    assert new.entries == mod.entries
    assert new.resident_names == mod.resident_names
    assert new.nonresident_names == mod.nonresident_names
    assert new.modules == mod.modules and new.imported_names == mod.imported_names
    assert new.std_relocs[1] == mod.std_relocs[1]       # stub keeps its own records
    for old, nw, u in zip(mod.segments, new.segments, up.segments):
        assert nw.minalloc == old.minalloc
        if not u.packed:
            assert (nw.flags, nw.file_len) == (old.flags, old.file_len)
            assert new.segment_file_bytes(nw.n) == mod.segment_file_bytes(old.n)
            continue
        assert nw.flags & ~0x0100 == old.flags & ~0x0008
        assert nw.has_relocs == bool(u.records)
        assert new.segment_file_bytes(nw.n) == u.image[:u.decomp_len]
        assert not any(u.image[u.decomp_len:])          # the rest is zero fill
    # the stub is not needed once the module stops self-loading
    assert new.header["cs"] != 1 and all(e.seg != 1 for e in new.entries)
    assert all(r.target.get("target_seg") != 1 for n, rs in new.std_relocs.items() if n > 1
               for r in rs)


@pytest.mark.parametrize("rel,nseg,nrec", MODULES, ids=ids)
def test_rebuilt_resources_match(original_dir, rel, nseg, nrec):
    mod, up, rb, new = built(original_dir, rel)
    assert len(new.resources) == len(mod.resources) > 0
    for a, b in zip(mod.resources, new.resources):
        assert (a.type_id, a.id, a.flags, a.raw_length) == (b.type_id, b.id, b.flags, b.raw_length)
        assert new.data[b.file_off:b.file_off + b.length] == mod.data[a.file_off:a.file_off + a.length]
        assert b.file_off >= max(s.file_off + s.file_len for s in new.segments)


def test_inspace_record_mapping(original_dir):
    mod, up, rb, new = built(original_dir, "shipdisk/INSPACE.ex_")
    assert rb.record_types == {"FAR_ADDR/IMPORTORDINAL": 722, "OFFSET/IMPORTORDINAL": 3,
                               "SEGMENT/INTERNALREF": 205}
    assert len(rb.data) == 547696
    # seg4:0x24 -> 0x6b -> 0xFFFF loads the seg7 selector
    rec = next(r for r in new.std_relocs[4] if r.offset == 0x24)
    assert rec.to_json()["src_type"] == "SEGMENT" and rec.target == {"target_seg": 7,
                                                                     "target_offset": 0}
    img = neload.load_standard(new).images[4]
    assert img[0x24] == img[0x6B] == (("SEG", 7, "sel"), 0)
    seg19 = new.segment(19)
    assert (seg19.file_len, seg19.minalloc, seg19.has_relocs) == (0x17B5, 41456, False)


def test_ispace_additive_record(original_dir):
    mod, up, rb, new = built(original_dir, "shipdisk/ISPACE.dll")
    adds = [(n, r) for n, rs in new.std_relocs.items() for r in rs if r.flags & 4]
    assert len(adds) == 1
    n, r = adds[0]
    assert (n, r.offset, r.src_type, r.target["module"], r.target["ordinal"]) == (3, 0x24, 3,
                                                                                 "KERNEL", 91)
    assert Counter(r.to_json()["src_type"] for rs in new.std_relocs.values() for r in rs) \
        == {"FAR_ADDR": 304, "OFFSET": 8, "SEGMENT": 34}


def test_check_detects_a_wrong_record(original_dir):
    mod, up, rb, new = built(original_dir, "shipdisk/INSPACE.ex_")
    data = bytearray(rb.data)
    s = new.segment(4)
    p = s.file_off + s.file_len + 2
    while struct.unpack_from("<H", data, p + 2)[0] != 0x24:
        p += 8
    data[p + 4] = 8                                     # seg7 -> seg8
    report = neload.check(NEModule(bytes(data), "bad"), mod, up)
    assert not report.ok
    assert any(d.startswith("seg4:0x0024") for d in report.differences)
    assert any(d.startswith("seg4:0x006b") for d in report.differences)


def test_rebuild_cli(original_dir, tmp_path, capsys):
    from isre.cli import main

    out = tmp_path / "ispace.dll"
    assert main(["rebuild", str(original_dir / "shipdisk/ISPACE.dll"), str(out)]) == 0
    assert "EQUIVALENT" in capsys.readouterr().out
    assert out.read_bytes() == built(original_dir, "shipdisk/ISPACE.dll")[2].data


def test_pipeline_writes_rebuilt(original_dir, tmp_path):
    from isre.pipeline import run_all

    res = run_all(original_dir, tmp_path, log=lambda s: None)
    assert res.ok
    names = {m.name: m for m in res.modules}
    assert names["inspace"].rebuilt == "rebuilt/inspace.exe"
    assert names["ispace"].rebuilt == "rebuilt/ispace.dll"
    assert (tmp_path / "rebuilt/inspace.exe").read_bytes() == \
        built(original_dir, "shipdisk/INSPACE.ex_")[2].data
    for f in ("factory-1998a.exe", "factory-1998b.exe"):
        assert NEModule.load(tmp_path / "rebuilt" / f).std_relocs
