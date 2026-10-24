"""Tests for ``isre pack`` (analyst context packs).

The unit tests run everywhere. The pack tests read ``work/`` as written by
``isre all`` (and ``re/ghidra/run.sh`` for the C) and skip without it or
without the original game files.
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys
import time

import pytest
from conftest import REPO_ROOT, require

from isre import asm, pack

WORK = REPO_ROOT / "work"


# ---------------------------------------------------------------- unit


def test_ghidra_converter():
    conv = pack.ghidra_converter(19, 19)
    assert conv("FUN_1020_0364();") == "FUN_seg5_0x364();"
    assert conv("x = DAT_1090_9212;") == "x = DAT_DGROUP_0x9212;"
    assert conv("/* WARNING: Removing unreachable block (ram,0x102003b8) */") == \
        "/* WARNING: Removing unreachable block (ram,seg5:0x3b8) */"
    assert conv("/* WARNING: Could not recover jumptable at 0x10000765. */") == \
        "/* WARNING: Could not recover jumptable at seg1:0x765. */"
    assert conv("goto code_r0x1000028e;") == "goto code_r_seg1_0x28e;"
    assert conv("lstrcpy((void *)0x10909690,s_Inner_Space_1090_0082);") == \
        "lstrcpy((void *)0x10909690 /*DGROUP:0x9690*/,s_Inner_Space_DGROUP_0x82);"
    assert conv("switchD_1020:0abc") == "switchD_seg5:0xabc"
    assert conv("// inspace seg5:0x364  (Ghidra 1020:0364)  FUN_1020_0364") == \
        "// inspace seg5:0x364  FUN_seg5_0x364"
    # not selectors of this module: seg21 (past 19 segments), 0x1234 (not 0x1000 + 8k)
    for text in ("y = 0x10a00000;", "x = 0x12345678;", "CONCAT22(0x1090,local_86)"):
        assert conv(text) == text


def test_doc_mentions_carry_their_module():
    m = pack.mentions("It repoints the one additive FAR_ADDR import (ISPACE.DLL seg3:0x24, "
                      "KERNEL.91 + 0xfffb) at the thunk. [BYTES ISPACE.DLL seg3:0x23]")
    assert m == [pack.Mention("ispace", 3, 0x24, 0x24), pack.Mention("ispace", 3, 0x23, 0x23)]
    # a tag without a module name is INSPACE.EXE, whatever the prose before it named
    m = pack.mentions("For INSPACE, it labels g with the ISPACE.DLL entry address. "
                      "[BYTES seg10:0x67c0]")
    assert m == [pack.Mention("inspace", 10, 0x67C0, 0x67C0)]
    m = pack.mentions("A loop at ISPACE.DLL seg7:0x1481–0x14c2 runs four times; from "
                      "seg5:0x36e1/0x3707. [BYTES ISPACE.DLL seg7:0x1481]")
    M = pack.Mention
    assert m == [M("ispace", 7, 0x1481, 0x14C2), M("ispace", 5, 0x36E1, 0x36E1),
                 M("ispace", 5, 0x3707, 0x3707), M("ispace", 7, 0x1481, 0x1481)]
    assert pack.mentions("`[BYTES FACTORY.EXE seg4:0x5d0f]` `[BYTES DGROUP:0x1260]`") == \
        [M("factory-1998a", 4, 0x5D0F, 0x5D0F), M("inspace", None, 0x1260, 0x1260)]


def test_fit_listing_keeps_head_and_target():
    lines = [("; ---- function seg2:0x0  [export]", None)]
    lines += [(f"seg2:{o:#x}".ljust(14) + "90".ljust(27) + "nop" + " " * 30, o)
              for o in range(0, 4000, 2)]
    out = pack.fit_listing(lines, 6000, 0x500, "isre asm --show seg2:0x0", 2)
    text = "\n".join(out)
    assert len(text.encode()) <= 6000
    assert out[0].startswith("; ---- function seg2:0x0")
    assert re.search(r"^seg2:0x0 ", text, re.M) and re.search(r"^seg2:0x500 ", text, re.M)
    notes = [ln for ln in out if ln.startswith("; [pack:")]
    assert len(notes) == 2 and "seg2:0xf9e" in notes[-1]
    assert pack.fit_listing(lines[:10], 6000, 0, "", 2) == [t for t, _o in lines[:10]]


# ---------------------------------------------------------------- packs


@pytest.fixture(scope="module")
def work(original_dir):
    for name in ("inspace", "ispace"):
        require(WORK / "asm" / name / "functions.json")
    return WORK


def test_pack_findfirst(work):
    text = pack.build_pack(work, "inspace", "seg5:0x364")
    assert text.startswith("# INSPACE.EXE seg5:0x364\n")
    assert "findfirst" in text and "AH=4Eh" in text and "DOS3Call" in text
    # the shared tail is listed with the function
    assert re.search(r"^seg5:0x3b1 .*call far KERNEL\.102 DOS3Call +; AH=4Eh/4Fh findfirst",
                     text, re.M)
    assert "- shares code with: seg5:0x352" in text
    assert "`docs/re/pitfalls.md:" in text


def test_pack_names_the_ispace_ordinals(work):
    text = pack.build_pack(work, "inspace", "seg10:0x67c0")
    assert text.startswith("# INSPACE.EXE seg10:0x6656\n")
    assert re.search(r"^seg10:0x67c0 .*call far KERNEL\.95 LoadLibrary$", text, re.M)
    assert '- LoadLibrary seg10:0x67c0: its name ends in "ISPACE.DLL"' in text
    ne = json.loads((work / "ne" / "ispace.json").read_text())
    entries = {e["ordinal"]: asm.addr(e["seg"], e["offset"]) for e in ne["entries"]}
    found = re.findall(r"ordinal (\d+) -> far pointer at DGROUP:0x[0-9a-f]+ = ISPACE\.DLL "
                       r"(seg\d+:0x[0-9a-f]+)", text)
    assert len(found) == 23 and len({o for o, _a in found}) == 23
    assert all(entries[int(o)] == a for o, a in found)
    assert '- seg10:0x6782 LoadString(hInst word ptr [0x9212]): id 2117 (0x845)' in text


def test_pack_size_cap(work):
    fj = asm.load_functions(work / "asm" / "inspace")
    for f in sorted(fj["functions"], key=lambda f: (-f["bytes"], f["off"]))[:3]:
        text = pack.build_pack(work, "inspace", f["addr"])
        assert len(text.encode("utf-8")) <= pack.MAX_BYTES
        assert "; [pack: " in text and text.count("```") % 2 == 0
    text = pack.build_pack(work, "inspace", "seg10:0x67c0", cap=12000)
    assert len(text.encode("utf-8")) <= 12000 and "[pack: " in text


def test_pack_dgroup_string(work):
    text = pack.build_pack(work, "inspace", "DGROUP:0x1260")
    assert text.startswith("# INSPACE.EXE DGROUP:0x1260\n")
    assert "\n```\nC:\\*.*\n```\n" in text
    refs = json.loads((work / "asm" / "inspace" / "xrefs.json").read_text())["dgroup"]
    index = asm.FunctionIndex(asm.load_functions(work / "asm" / "inspace"))
    assert refs["DGROUP:0x1260"]
    for frm, how in refs["DGROUP:0x1260"]:
        func, _others = index.containing(*asm._addr_key(frm))
        assert f"- {frm} {how}, in function {func['addr']}" in text
    assert "BSS" in pack.build_pack(work, "inspace", "DGROUP:0x9212")


def test_pack_other_modules(work):
    text = pack.build_pack(work, "ispace", "seg3:0x23")
    assert "InitTask+0xfffb" in text and "`docs/re/ghidra.md:" in text
    if (work / "asm" / "factory-1998a" / "functions.json").is_file():
        text = pack.build_pack(work, "factory-1998a", "seg4:0x5d0f")
        assert text.startswith("# FACTORY.EXE seg4:0x5c9c\n") and "`docs/re/pitfalls.md:" in text


def test_pack_errors(work, capsys):
    assert pack.main(["seg30:0x0", "--work", str(work)]) == 2
    assert "seg30 of inspace has no listing" in capsys.readouterr().err
    assert pack.main(["somewhere", "--work", str(work)]) == 2
    assert pack.main(["DGROUP:0xfff0", "--work", str(work)]) == 2
    assert "outside DGROUP" in capsys.readouterr().err
    text = pack.build_pack(work, "inspace", "seg5:0x351")          # alignment padding
    assert "No function that `isre asm` decoded holds seg5:0x351" in text


def test_pack_cli_is_deterministic_and_fast(work, tmp_path):
    outs = []
    for seed in ("1", "2"):
        out = tmp_path / f"pack{seed}.md"
        start = time.monotonic()
        subprocess.run([sys.executable, "-I", "-m", "isre", "pack", "seg10:0x67c0", "--work",
                        str(work), "-o", str(out)], check=True, cwd=REPO_ROOT,
                       env=dict(os.environ, PYTHONHASHSEED=seed), capture_output=True)
        assert time.monotonic() - start < 5
        outs.append(out.read_bytes())
    assert outs[0] == outs[1]
