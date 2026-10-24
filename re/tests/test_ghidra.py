"""Ghidra headless pipeline (re/ghidra): driver guards and the outputs it writes.

The output checks read work/decomp/<module>/index.tsv and work/ghidra/<module>-*.json,
written by `re/ghidra/run.sh`; they skip when those are absent. The committed
re/artifacts/ghidra-functions-<module>.tsv copies are checked whenever they exist.
"""
from __future__ import annotations

import json
import os
import re
import subprocess
from collections import Counter
from pathlib import Path

import pytest
from conftest import ARTIFACTS, REPO_ROOT, require

GHIDRA_DIR = REPO_ROOT / "re" / "ghidra"
WORK = REPO_ROOT / "work"
DGROUP = {"inspace": 19, "ispace": 11}
HEADER = ["address", "ghidra", "name", "size", "callers", "call_sites", "status", "file"]
CANON = re.compile(r"^seg([1-9][0-9]*):0x([0-9a-f]+)$")
# GetProcAddress ordinals of ISPACE.DLL used by INSPACE's loader [BYTES seg10:0x67c0]
ISPACE_ORDINALS = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 15, 17, 18, 24, 29, 32, 33, 34,
                   35, 36, 41]


def read_index(path: Path) -> list[dict[str, str]]:
    lines = path.read_text(encoding="utf-8").splitlines()
    assert lines[0].split("\t") == HEADER
    return [dict(zip(HEADER, ln.split("\t"))) for ln in lines[1:]]


def seg_off(addr: str) -> tuple[int, int]:
    m = CANON.match(addr)
    assert m, f"not a canonical segN:0xOFF address: {addr!r}"
    return int(m.group(1)), int(m.group(2), 16)


def work_index(module: str) -> list[dict[str, str]]:
    return read_index(require(WORK / "decomp" / module / "index.tsv"))


def report(module: str, kind: str) -> dict:
    return json.loads(require(WORK / "ghidra" / f"{module}-{kind}.json").read_text())


def prologue_entries() -> set[tuple[int, int]]:
    data = json.loads((ARTIFACTS / "function-entries.json").read_text())
    return {(int(s), o) for s, offs in data.items() for o in offs}


# ---------------------------------------------------------------- driver and hygiene


def _run_sh(env: dict[str, str]) -> subprocess.CompletedProcess:
    return subprocess.run(["bash", str(GHIDRA_DIR / "run.sh")], env=env, cwd=REPO_ROOT,
                          capture_output=True, text=True, timeout=60)


def test_run_sh_needs_ghidra_home():
    r = _run_sh({"PATH": os.environ.get("PATH", "/usr/bin:/bin")})
    assert r.returncode == 2
    assert "GHIDRA_HOME is not set" in r.stderr


def test_run_sh_needs_java_home(tmp_path):
    r = _run_sh({"PATH": os.environ.get("PATH", "/usr/bin:/bin"), "GHIDRA_HOME": str(tmp_path)})
    assert r.returncode == 2
    assert "JAVA_HOME is not set" in r.stderr


def test_no_absolute_paths():
    files = sorted(GHIDRA_DIR.iterdir()) + [REPO_ROOT / "docs" / "re" / "ghidra.md"]
    files += sorted(ARTIFACTS.glob("ghidra-functions-*.tsv"))
    for f in files:
        if f.is_file():
            assert "/home/" not in f.read_text(encoding="utf-8", errors="replace"), f


# ---------------------------------------------------------------- index format


def check_index(module: str, rows: list[dict[str, str]]) -> None:
    from isre.addr import Address

    seen = set()
    last = (0, -1)
    for r in rows:
        seg, off = seg_off(r["address"])
        assert seg != DGROUP[module], f"{module}: function inside DGROUP at {r['address']}"
        assert (seg, off) > last, f"{module}: index not sorted or duplicated at {r['address']}"
        last = (seg, off)
        assert r["ghidra"] == Address(seg, off).ghidra
        assert r["file"] == f"seg{seg:02d}_0x{off:x}.c"
        assert int(r["size"]) > 0 and int(r["callers"]) >= 0 and int(r["call_sites"]) >= 0
        assert r["status"] in ("ok", "failed")
        seen.add((seg, off))
    assert len(seen) == len(rows)


@pytest.mark.parametrize("module", ["inspace", "ispace"])
def test_work_index(module):
    rows = work_index(module)
    check_index(module, rows)
    out = WORK / "decomp" / module
    files = {p.name for p in out.glob("*.c")}
    assert files == {r["file"] for r in rows}
    assert all(r["status"] == "ok" for r in rows), [r["address"] for r in rows
                                                    if r["status"] != "ok"]
    exp = report(module, "export")
    assert exp["functions"] == len(rows) and exp["failed"] == 0


@pytest.mark.parametrize("module", ["inspace", "ispace"])
def test_committed_index(module):
    path = ARTIFACTS / f"ghidra-functions-{module}.tsv"
    if not path.exists():
        pytest.skip(f"{path.name} is not committed")
    rows = read_index(path)
    check_index(module, rows)
    work = WORK / "decomp" / module / "index.tsv"
    if work.exists():
        assert path.read_text(encoding="utf-8") == work.read_text(encoding="utf-8"), \
            f"{path.name} differs from {work.relative_to(REPO_ROOT)}; copy the new index"


def _index_rows(module: str) -> list[dict[str, str]]:
    path = WORK / "decomp" / module / "index.tsv"
    if not path.exists():
        path = require(ARTIFACTS / f"ghidra-functions-{module}.tsv")
    return read_index(path)


def test_inspace_functions_vs_prologue_scan():
    """Every one of the 880 far-prologue entries is a function. The extra functions sit
    in seg1 (the OPTLOADER stub, which the scan skipped) and seg5 (Borland RTL routines
    written without the standard prologue)."""
    found = {seg_off(r["address"]) for r in _index_rows("inspace")}
    scan = prologue_entries()
    assert len(scan) == 880
    assert scan - found == set()
    assert Counter(s for s, _ in found - scan) == {1: 7, 5: 52}
    assert len(found) == 939


def test_ispace_functions_vs_prologue_scan():
    """ISPACE.DLL uses the exported far prologue (mov ax,ds; nop; inc bp; push bp;
    mov bp,sp; push ds; mov ds,ax). Every match is a function; the extras sit in seg1
    (OPTLOADER stub) and seg3."""
    prologue = bytes.fromhex("8cd89045558bec1e8ed8")
    scan = set()
    for path in sorted(require(WORK / "unpacked" / "ispace").glob("seg*.bin")):
        n = int(path.stem[3:])
        data = path.read_bytes()
        i = data.find(prologue)
        while i >= 0:
            scan.add((n, i))
            i = data.find(prologue, i + 1)
    found = {seg_off(r["address"]) for r in _index_rows("ispace")}
    assert len(scan) == 245
    assert scan - found == set()
    assert Counter(s for s, _ in found - scan) == {1: 7, 3: 32}


def test_inspace_rtl_namespace():
    rows = _index_rows("inspace")
    seg5 = [r for r in rows if seg_off(r["address"])[0] == 5]
    assert seg5 and all(r["name"].startswith("BorlandRTL::") for r in seg5)
    assert {r["address"]: r["name"] for r in seg5}["seg5:0x1a"] == "BorlandRTL::startup"
    assert not any(r["name"].startswith("BorlandRTL::") for r in rows if r not in seg5)


def test_ispace_exports_are_functions():
    from isre.ne import NEModule

    dll = NEModule.load(require(WORK / "rebuilt" / "ispace.dll"))
    found = {seg_off(r["address"]): r["name"] for r in _index_rows("ispace")}
    for e in dll.to_json()["entries"]:
        key = (e["seg"], e["offset"])
        assert key in found, f"ISPACE ordinal {e['ordinal']} at seg{key[0]}:{key[1]:#x}"


# ---------------------------------------------------------------- reports


@pytest.mark.parametrize("module", ["inspace", "ispace"])
def test_setup_report(module):
    rep = report(module, "setup")
    assert rep["segments_loaded"] == rep["segments"]
    assert rep["ne_relocation_records_applied"] == rep["ne_relocation_records"]
    assert rep["import_far_sites_to_external_ordinal"] == rep["import_far_sites"] > 0
    assert rep["dgroup_seg"] == DGROUP[module]
    assert rep["imports"]["unresolved"] == []
    assert rep["problems"] == []
    assert rep["analysis_options"]["Aggressive Instruction Finder"] == "false"
    want = ["DS"] if module == "ispace" else ["DS", "SS"]
    assert rep["context_registers"] == want


def test_inspace_import_counts():
    imp = report("inspace", "setup")["imports"]
    assert imp["distinct_ordinals_outside_seg1"] == 186
    assert imp["seg1_only"] == ["KERNEL.170"]


def test_inspace_import_names_match_api_imports():
    """Names from the Wine specs agree with re/artifacts/api-imports.json."""
    tsv = require(WORK / "ghidra" / "inspace-imports.tsv").read_text().splitlines()
    names = {}
    for ln in tsv[1:]:
        mod, ordinal, name = ln.split("\t")[:3]
        names[(mod, int(ordinal))] = name
    api = json.loads((ARTIFACTS / "api-imports.json").read_text())["imports"]
    assert len(api) == 186
    for i in api:
        assert names[(i["module"], i["ordinal"])] == i["name"], i


@pytest.mark.parametrize("module", ["inspace", "ispace"])
def test_finish_report(module):
    rep = report(module, "finish")
    assert rep["problems"] == []
    assert rep["functions"] == len(work_index(module))


def test_inspace_join():
    from isre.ne import NEModule

    rep = report("inspace", "finish")["join"]
    assert rep["handle"] == "DGROUP:0x9210"
    assert rep["ordinals"] == ISPACE_ORDINALS
    entries = {e["ordinal"]: (e["seg"], e["offset"])
               for e in NEModule.load(require(WORK / "rebuilt" / "ispace.dll"))
               .to_json()["entries"]}
    rows = (WORK / "ghidra" / "inspace-join.tsv").read_text().splitlines()[1:]
    assert len(rows) == len(ISPACE_ORDINALS)
    for ln in rows:
        ordinal, dll_addr, _, glob, gname = ln.split("\t")[:5]
        assert seg_off(dll_addr) == entries[int(ordinal)]
        assert glob.startswith("DGROUP:0x") and gname == f"pISPACE_Ordinal_{ordinal}"
