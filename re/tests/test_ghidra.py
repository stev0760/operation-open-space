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
HEADER = ["address", "ghidra", "name", "size", "callers", "call_sites", "status", "file",
          "ranges"]
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


def ranges(row: dict[str, str]) -> list[tuple[int, int]]:
    """Body ranges (start, end exclusive) of an index row; all lie in the row's segment."""
    out = []
    for r in row["ranges"].split():
        assert ":" not in r, f"{row['address']}: body range {r} in another segment"
        lo, hi = r.split("-")
        out.append((int(lo, 16), int(hi, 16)))
    return out


def owner(rows: list[dict[str, str]], seg: int, off: int) -> dict[str, str] | None:
    """The index row whose body holds seg:off."""
    for r in rows:
        if seg_off(r["address"])[0] == seg and any(a <= off < b for a, b in ranges(r)):
            return r
    return None


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
        body = ranges(r)
        assert body == sorted(body) and all(a < b for a, b in body), r["address"]
        assert sum(b - a for a, b in body) == int(r["size"]), r["address"]
        assert any(a <= off < b for a, b in body), f"{r['address']}: entry outside its body"
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
    assert Counter(s for s, _ in found - scan) == {1: 7, 5: 55}
    assert len(found) == 942


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
    assert Counter(s for s, _ in found - scan) == {1: 7, 3: 41}
    assert len(found) == 293


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


def test_fatal_exit_imports():
    """KERNEL.1 FatalExit is pascal void FatalExit(word) and does not return; KERNEL.137
    FatalAppExit returns, so the RTL's FatalExit(0xff) fallback after it is decoded."""
    for module in ("inspace", "ispace"):
        imp = report(module, "setup")["imports"]
        assert imp["no_return"] == ["KERNEL.1 FatalExit"]
        assert imp["returning"] == ["KERNEL.137 FatalAppExit"]
        assert imp["signature_overrides"] == ["KERNEL.1 stub -> pascal void FatalExit(word)"]


@pytest.mark.parametrize("module,func,fallback", [
    ("inspace", "seg5:0xb61", (0xb9a, 0xba3)),   # FatalAppExit at seg5:0xb95 [BYTES seg5:0xb95]
    ("ispace", "seg3:0xa58", (0xa91, 0xa9a)),    # FatalAppExit at seg3:0xa8c
])
def test_fatal_app_exit_falls_through(module, func, fallback):
    rows = {r["address"]: r for r in _index_rows(module)}
    a, b = fallback
    assert any(lo <= a and b <= hi for lo, hi in ranges(rows[func])), rows[func]["ranges"]
    seg = seg_off(func)[0]
    assert owner(list(rows.values()), seg, a)["address"] == func
    c = require(WORK / "decomp" / module / rows[func]["file"]).read_text()
    assert "FatalAppExit(" in c and "FatalExit(0xff);" in c


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
    assert rep["instructions_outside_functions"] == 0


# ---------------------------------------------------------------- function bodies

# Sizes that a Ghidra defect once got wrong; isre asm agrees with each.
@pytest.mark.parametrize("module,func,size,body", [
    # 10-entry table at seg12:0x1678 (switch at seg12:0x1673) read with 160 targets
    ("inspace", "seg12:0x129e", 1219, "0x129e-0x1678 0x168c-0x1775"),
    # 10-entry table at seg12:0x1199 (switch at seg12:0x1194)
    ("inspace", "seg12:0xe5c", 1069, "0xe5c-0x1199 0x11ad-0x129d"),
    # 16-entry table at seg10:0x5803 (switch at seg10:0x57fe) left unresolved
    ("ispace", "seg10:0x5612", 648, "0x5612-0x5803 0x5823-0x58ba"),
    # epilogue seg5:0xdc3, reached only by the jmp at seg5:0x3db, was a function
    ("inspace", "seg5:0x364", 151, "0x364-0x3de 0xdb8-0xdd5"),
])
def test_function_size(module, func, size, body):
    rows = {r["address"]: r for r in _index_rows(module)}
    assert int(rows[func]["size"]) == size
    assert rows[func]["ranges"] == body


@pytest.mark.parametrize("module,funcs", [
    ("inspace", ["seg5:0xdc3"]),
    ("ispace", ["seg3:0x846", "seg3:0x98d"]),
])
def test_no_frame_tail_functions(module, funcs):
    """Epilogue fragments reached by jumps from one function are part of it."""
    found = {r["address"] for r in _index_rows(module)}
    assert found.isdisjoint(funcs)
    want = {"inspace": ["seg5:0xdc3 into seg5:0x364"],
            "ispace": ["seg3:0x846 into seg3:0x69c", "seg3:0x98d into seg3:0x8a6"]}[module]
    assert report(module, "finish")["frame_tails_removed"] == want


def test_switch_c_has_no_garbage():
    c = require(WORK / "decomp" / "inspace" / "seg12_0x129e.c").read_text()
    assert "halt_baddata" not in c and "caseD_" not in c
    for callee in ("StretchBlt(", "PatBlt(", "FUN_1030_2080(", "FUN_1030_1208("):
        assert callee in c


def isre_switches(module: str) -> list[dict]:
    return json.loads(require(WORK / "asm" / module / "functions.json").read_text())["switches"]


@pytest.mark.parametrize("module", ["inspace", "ispace"])
def test_switch_targets_in_owner(module):
    """Every case of every isre asm jump table lies in the function that owns the jump."""
    rows = _index_rows(module)
    sws = isre_switches(module)
    assert len(sws) == {"inspace": 42, "ispace": 18}[module]
    for sw in sws:
        seg, at = seg_off(sw["at"])
        own = owner(rows, seg, at)
        assert own is not None, f"{sw['at']}: jump in no function"
        for t in sw["targets"]:
            assert owner(rows, *seg_off(t)) is own, f"{sw['at']}: case {t} outside {own['address']}"


@pytest.mark.parametrize("module", ["inspace", "ispace"])
def test_switches_match_isre(module):
    """IsFinish bounds the same tables as isre asm, and repairs only the broken ones."""
    rep = report(module, "finish")
    mine = {s["at"]: s for s in rep["switches"]}
    theirs = {s["at"]: s for s in isre_switches(module)}
    assert set(mine) == set(theirs)
    for at, s in theirs.items():
        m = mine[at]
        assert (m["table"], m["count"], m["bound_at"]) == (s["table"], s["count"], s["bound_at"])
        assert m["good"] is True
    first = rep["switch_repair_before_seeds"]
    assert first["table_jumps_unbounded"] == []
    assert [r.split(" ")[0] for r in first["repaired"]] == {
        "inspace": ["seg12:0x1194", "seg12:0x1673"], "ispace": ["seg10:0x57fe"]}[module]
    assert rep["switch_repair_after_analysis"]["repaired"] == []


@pytest.mark.parametrize("module,rtl", [("inspace", 5), ("ispace", 3)])
def test_code_data_only_in_tables(module, rtl):
    """Besides jump tables, code segments hold only the seg1 stub's data and the
    __WINFLAGS word of the RTL segment."""
    assert report(module, "finish")["code_data_outside_tables"] == [
        "seg1:0x2c 2", "seg1:0x2e 2", "seg1:0x84d 16", f"seg{rtl}:0x10 2"]


@pytest.mark.parametrize("module,seeded", [
    ("inspace", ["gap seg5:0xb5c", "gap seg5:0x1da8"]),
    ("ispace", ["gap seg3:0x12", "gap seg3:0x2b", "gap seg3:0x34", "gap seg3:0x791",
                "gap seg3:0x99f", "gap seg3:0xa4e", "gap seg3:0xa9a"]),
])
def test_gap_seeds(module, seeded):
    rep = report(module, "finish")
    assert rep["gap_seeded"] == seeded
    # an isre gap entry that Ghidra decoded already (the FatalExit fallback after
    # FatalAppExit) counts as gap_inside_function
    assert set(rep["gap_seeds"]) <= {"gap_inside_function", "gap_seeded"}


@pytest.mark.parametrize("module", ["inspace", "ispace"])
def test_import_call_sites_in_functions(module):
    """Every far call to an import (9a + import FAR_ADDR site) lies in a function body."""
    from isre import asm

    img = asm.load_work(require(WORK), module)
    rows = _index_rows(module)
    calls = []
    for n, sites in asm.build_sites(img, {}).items():
        data = img.segs[n].data
        for so, st in sites.items():
            if st.kind == "import" and st.width == 4 and so > 0 and data[so - 1] == 0x9A:
                calls.append((n, so - 1))
    assert len(calls) == {"inspace": 5900, "ispace": 1959}[module]
    outside = [f"seg{n}:{o:#x}" for n, o in calls if owner(rows, n, o) is None]
    assert outside == []


# isre asm functions that Ghidra does not have, and the reason:
#   seg1:0x30, seg1:0x1e7  one-byte OPTLOADER self-load entries
ISRE_ONLY = {"inspace": {"seg1:0x30", "seg1:0x1e7"},
             "ispace": {"seg1:0x30", "seg1:0x1e7"}}
# Ghidra functions that isre asm folds into others: seg1 stub routines, and RTL
# fragments that several functions reach (isre lists them as shared tails).
GHIDRA_ONLY = {"inspace": {"seg1:0x4c1", "seg1:0x74f", "seg1:0x93d", "seg5:0xb61", "seg5:0xdd5",
                           "seg5:0x1bfe", "seg5:0x1ef4"},
               "ispace": {"seg1:0x4c1", "seg1:0x74f", "seg1:0x93d", "seg3:0x2e", "seg3:0x42",
                          "seg3:0x97b", "seg3:0xa1a", "seg3:0xa58"}}


@pytest.mark.parametrize("module", ["inspace", "ispace"])
def test_functions_vs_isre(module):
    """Outside seg1 and the RTL segment, Ghidra and isre asm find the same functions with
    the same sizes."""
    rtl = {"inspace": 5, "ispace": 3}[module]
    iso = {f["addr"]: f for f in json.loads(
        require(WORK / "asm" / module / "functions.json").read_text())["functions"]}
    gh = {r["address"]: r for r in _index_rows(module)}
    assert set(iso) - set(gh) <= ISRE_ONLY[module]
    assert set(gh) - set(iso) <= GHIDRA_ONLY[module]
    for a in set(iso) & set(gh):
        if seg_off(a)[0] not in (1, rtl):
            assert iso[a]["bytes"] == int(gh[a]["size"]), a


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
