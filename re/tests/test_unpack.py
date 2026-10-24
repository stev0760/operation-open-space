from __future__ import annotations

import json
from collections import Counter, defaultdict

import pytest
from conftest import ARTIFACTS, GOLDEN, read_sha256_list, require

from isre import optloader
from isre.ne import NEModule
from isre.util import json_bytes, sha256

# ---------------------------------------------------------------- INSPACE goldens


def test_segments_match_golden(inspace_unpacked):
    gold = read_sha256_list(GOLDEN / "inspace.sha256")
    segs = inspace_unpacked.segments
    assert len(segs) == 19
    for s in segs:
        assert sha256(s.image) == gold[f"unpacked/seg{s.n:02d}.bin"], f"seg{s.n}"


def test_segment_sizes(inspace, inspace_unpacked):
    segs = inspace_unpacked.segments
    assert segs[0].image == inspace.segment_file_bytes(1)  # loader stub, raw
    for s in segs[1:]:
        assert len(s.image) == s.minalloc
    dgroup = segs[18]
    assert dgroup.decomp_len == 0x17B5  # initialised DGROUP data; the rest is BSS
    assert not any(dgroup.image[0x17B5:])


def test_relocs_match_artifact(inspace_unpacked):
    art = json.loads((ARTIFACTS / "relocs.json").read_text())
    got = json.loads(json_bytes(inspace_unpacked.relocs_json()))
    assert got["modules"] == art["modules"]
    assert got["relocs"] == art["relocs"]
    assert got["report"] == art["report"]
    recs = inspace_unpacked.records
    assert len(recs) == 930
    assert Counter(r["kind"] for r in recs) == {"import_ordinal": 725, "special_base": 205}
    assert len({(r["module"], r["ordinal"]) for r in recs if r["kind"] == "import_ordinal"}) == 186


def test_relocs_json_bytes_match_golden(inspace_unpacked):
    gold = read_sha256_list(GOLDEN / "inspace.sha256")
    assert sha256(json_bytes(inspace_unpacked.relocs_json())) == gold["relocs.json"]


def _import_sites(result):
    sites = defaultdict(list)
    for s in result.segments:
        chains, errs = s.chains()
        assert not errs
        for rec, chain in zip(s.records, chains):
            if rec["kind"] == "import_ordinal":
                sites[(rec["module"], rec["ordinal"])].extend((s.n, x) for x in chain)
    return sites


def test_relocation_chains(inspace_unpacked):
    sites = _import_sites(inspace_unpacked)
    ptr16 = sum(len(c) for s in inspace_unpacked.segments
                for rec, c in zip(s.records, s.chains()[0]) if rec["target_type"] == "PTR16")
    assert ptr16 == 5900  # far-call import sites
    longest = max(len(c) for s in inspace_unpacked.segments for c in s.chains()[0])
    assert longest == 159
    # special-base chains link through the segment word: seg4:0x24 -> 0x6b -> 0xFFFF
    seg4 = inspace_unpacked.segments[3]
    heads = {rec["offset"]: c for rec, c in zip(seg4.records, seg4.chains()[0])
             if rec["kind"] == "special_base"}
    assert heads[0x24] == [0x24, 0x6B]
    # API site counts (call instruction = site - 1)
    dos3call = sites[("KERNEL", 102)]
    assert len(dos3call) == 39 and {n for n, _ in dos3call} == {5}
    extract_icon = sorted(f"seg{n}:{x - 1:#06x}" for n, x in sites[("SHELL", 34)])
    assert extract_icon == ["seg6:0x442c", "seg6:0x4469", "seg9:0x0178", "seg9:0x01a5", "seg9:0x0251"]
    assert len(sites[("USER", 91)]) == 141  # GetDlgItem


def test_unpack_cli_writes_outputs(shipdisk, tmp_path):
    from isre.cli import main

    out = tmp_path / "inspace"
    assert main(["unpack", str(shipdisk / "INSPACE.ex_"), str(out)]) == 0
    gold = read_sha256_list(GOLDEN / "inspace.sha256")
    for n in range(1, 20):
        assert sha256((out / f"seg{n:02d}.bin").read_bytes()) == gold[f"unpacked/seg{n:02d}.bin"]
    assert sha256((out / "relocs.json").read_bytes()) == gold["relocs.json"]
    manifest = json.loads((out / "unpack.json").read_text())
    assert manifest["invariants_ok"] and manifest["reloc_records"] == 930
    rel = tmp_path / "r.json"
    assert main(["relocs", str(shipdisk / "INSPACE.ex_"), str(rel)]) == 0
    assert rel.read_bytes() == (out / "relocs.json").read_bytes()


# ---------------------------------------------------------------- other modules

OTHER_MODULES = [
    # (path under original/, segments, packed segments)
    ("shipdisk/ISPACE.dll", 11, 10),
    ("shipdisk/FACTORY.ex_", 6, 5),
    ("theworks/FACTORY.EXE", 6, 5),
    ("shipdisk/INSTALL.exe", 4, 3),
    ("theworks/SETUP.EXE", 4, 3),
]


@pytest.mark.parametrize("rel,nseg,npacked", OTHER_MODULES, ids=[m[0] for m in OTHER_MODULES])
def test_other_module_invariants(original_dir, rel, nseg, npacked):
    mod = NEModule.load(require(original_dir / rel))
    res = optloader.unpack(mod)
    assert len(res.segments) == nseg
    packed = [s for s in res.segments if s.packed]
    assert len(packed) == npacked
    for s in packed:
        assert not s.errors, (s.n, s.errors)
        assert s.slack == 0, s.n                      # relocations end exactly at the segment end
        assert len(s.records) == s.w0, s.n            # decoded items == word0 count
        assert s.decomp_len <= s.minalloc, s.n
        assert not s.chains()[1], (s.n, s.chains()[1])
        assert len(s.image) == s.minalloc
    assert res.ok


def test_ispace_relocation_kinds(original_dir):
    res = optloader.unpack(NEModule.load(require(original_dir / "shipdisk/ISPACE.dll")))
    assert Counter(r["kind"] for r in res.records) == {"import_ordinal": 310, "special_base": 33}
    assert Counter(r["target_type"] for r in res.records)["PTR16_A"] == 1


def test_unpack_rejects_unpacked_module(shipdisk):
    from isre import szdd

    mod = NEModule(szdd.expand((shipdisk / "INSPACE.sc1").read_bytes()), "INSPACE.SCR")
    with pytest.raises(optloader.OptLoaderError):
        optloader.unpack(mod)
