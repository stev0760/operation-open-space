"""Tests for ``isre asm`` (recursive-descent disassembler).

The synthetic tests run everywhere; the module tests need the original game
files and skip without them.
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys

import pytest
from conftest import ARTIFACTS, REPO_ROOT, require

from isre import asm, optloader, x86names
from isre.ne import NEModule
from isre.x86dis import DATA, F_CALLF, HEAD, CodeSeg, Disassembler, Site

# ---------------------------------------------------------------- synthetic


def test_parse_spec():
    text = ("1   stub FatalExit\n"
            "3   pascal GetVersion() GetVersion16\n"
            "102 pascal -register DOS3Call() DOS3Call\n"
            "113 equate __AHSHIFT 3\n"
            "# 200 pascal Commented()\n"
            "@   stdcall -ret16 Next(long) Next16\n")
    assert x86names.parse_spec(text) == {1: "FatalExit", 3: "GetVersion", 102: "DOS3Call",
                                         113: "__AHSHIFT", 114: "Next"}


def test_specs_name_the_game_imports():
    specs = x86names.load_specs(x86names.default_spec_dir())
    assert specs["KERNEL"][102] == "DOS3Call"
    assert specs["KERNEL"][91] == "InitTask"
    assert specs["SHELL"][34] == "ExtractIcon"
    assert specs["USER"][91] == "GetDlgItem"


def _seg(code: bytes, sites=()) -> dict[int, CodeSeg]:
    cs = CodeSeg(2, code, len(code), 0x0C58)
    for s in sites:
        cs.sites[s.off] = s
    return {2: cs}


def test_relocation_site_is_never_an_opcode():
    code = bytes.fromhex("9a00000000" "cb")         # chain link 0 decodes as add [bx+si],al
    site = Site(2, 1, 4, "import", "PTR16", 1, "KERNEL.102 DOS3Call", "KERNEL", 102)
    dis = Disassembler(_seg(code, [site]))
    insn, _ci, err = dis.decode(2, 0)
    assert insn is not None and insn.flow == F_CALLF and insn.op_start == 1
    insn, _ci, err = dis.decode(2, 1)                 # would start inside the site
    assert insn is None and "relocation site" in err
    dis.add_seed(2, 0, "export", 0)
    dis.add_seed(2, 2, "prologue", 3, True)           # inside the call: rejected
    dis.run()
    assert sorted(dis.insns[2]) == [0, 5]
    assert [r.reason for r in dis.rejected] == ["inside an instruction"]


def test_far_call_without_relocation_is_an_error():
    dis = Disassembler(_seg(bytes.fromhex("9a34120000cb")))
    dis.add_seed(2, 0, "prologue", 3, True)
    dis.run()
    assert not dis.insns[2]
    assert "far call without a relocation" in dis.rejected[0].reason


def test_borland_switch_table():
    code = bytes.fromhex(
        "48"            # 00 dec ax
        "3d0200"        # 01 cmp ax, 2
        "770f"          # 04 ja 0x15
        "03c0"          # 06 add ax, ax
        "93"            # 08 xchg bx, ax
        "2effa70e00"    # 09 jmp cs:[bx+0xe]
        "140015001600"  # 0e table: 0x14, 0x15, 0x16
        "90"            # 14 case 0
        "90"            # 15 case 1 / default
        "cb")           # 16 case 2
    dis = Disassembler(_seg(code))
    dis.add_seed(2, 0, "export", 0)
    dis.run()
    sw = dis.switches[(2, 9)]
    assert (sw.table, sw.count, sw.targets, sw.cmp_off) == (0x0E, 3, [0x14, 0x15, 0x16], 1)
    assert all(dis.state[2][b] == DATA for b in range(0x0E, 0x14))
    assert all(dis.state[2][b] == HEAD for b in (0x14, 0x15, 0x16))
    assert not dis.unresolved


def test_dos_function_merged_from_two_paths():
    code = bytes.fromhex(
        "b04e"   # 00 mov al, 0x4e
        "eb02"   # 02 jmp 6
        "b04f"   # 04 mov al, 0x4f
        "8ae0"   # 06 mov ah, al
        "cd21"   # 08 int 21h
        "cb")    # 0a retf
    dis = Disassembler(_seg(code))
    dis.add_seed(2, 0, "export", 0)
    dis.add_seed(2, 4, "export", 0)
    dis.run()
    assert dis.reg_values(2, 8, "ah") == ({0x4E, 0x4F}, False)


def _far_ptrs(code: bytes, site_off: int, seeds=(0,)) -> list:
    site = Site(2, site_off, 2, "selector", "SEG16", site_off, "seg3", ref_seg=3)
    dis = Disassembler(_seg(code, [site]))
    for o in seeds:
        dis.add_seed(2, o, "export", 0)
    dis.run()
    return [c.target for c in dis.calls if c.kind == "far_ptr"]


def test_far_pointer_pairs():
    # selector store, then the offset store: the following fall-through chain
    assert _far_ptrs(bytes.fromhex("c70602010000" "c70600013412" "cb"), 4) == [(3, 0x1234)]
    # offset store first: the preceding fall-through chain
    assert _far_ptrs(bytes.fromhex("c70600013412" "c70602010000" "cb"), 10) == [(3, 0x1234)]
    # a jmp ends the search: the matching store behind it is reached another way
    code = bytes.fromhex("c70602010000" "eb06" "c70600013412" "cb")
    assert _far_ptrs(code, 4, seeds=(0, 8)) == []


def test_listing_line_columns():
    nine = asm._line("seg3:0x9ca", "66 c7 06 0c 87 00 00 00 00", "mov dword ptr [0x870c], 0",
                     ["DGROUP:0x870c"])
    assert "00 mov dword" in nine
    assert asm.parse_line(nine) == (3, 0x9CA, bytes.fromhex("66c7060c8700000000"),
                                    "mov dword ptr [0x870c], 0", "DGROUP:0x870c")
    ten = asm._line("seg2:0x0", " ".join(["90"] * 10), "nop", [])
    assert asm.parse_line(ten) == (2, 0, bytes(10 * [0x90]), "nop", "")
    row = asm._line("seg3:0x10", "", "db ffh,ffh", ["|..|", "seg3:0x10 reloc X"])
    assert asm.parse_line(row) == (3, 0x10, b"", "db ffh,ffh", "|..|; seg3:0x10 reloc X")
    long = asm._line("seg2:0x0", "90", "x" * 60, ["c"])
    assert long.endswith("x ; c")
    assert asm.parse_line("; ---- function seg2:0x0") is None


def test_int21_exit_does_not_return():
    code = bytes.fromhex("b44c" "cd21" "ffff")      # mov ah,4ch; int 21h; garbage
    dis = Disassembler(_seg(code))
    dis.add_seed(2, 0, "export", 0)
    dis.run()
    assert sorted(dis.insns[2]) == [0, 2] and dis.insns[2][2].noreturn
    assert not dis.errors


# ---------------------------------------------------------------- modules


@pytest.fixture(scope="module")
def specs():
    return x86names.load_specs(x86names.default_spec_dir())


@pytest.fixture(scope="module")
def inspace_asm(inspace, inspace_unpacked, specs):
    return asm.analyse(asm.load_ne(inspace, inspace_unpacked, "inspace"), specs)


def _module_asm(original_dir, rel, name, specs):
    mod = NEModule.load(require(original_dir / rel))
    return asm.analyse(asm.load_ne(mod, optloader.unpack(mod), name), specs)


@pytest.fixture(scope="module")
def ispace_asm(original_dir, specs):
    return _module_asm(original_dir, "game/ISPACE.dll", "ispace", specs)


@pytest.fixture(scope="module")
def factory_asm(original_dir, specs):
    return _module_asm(original_dir, "game/FACTORY.ex_", "factory-1998a", specs)


@pytest.fixture(scope="module")
def inspace_outputs(inspace_asm):
    return asm.build_outputs(inspace_asm)


@pytest.fixture(scope="module")
def ispace_outputs(ispace_asm):
    return asm.build_outputs(ispace_asm)


def _insn(res, seg, off):
    insn = res.dis.insns[seg].get(off)
    assert insn is not None, f"seg{seg}:{off:#x} is not an instruction start"
    return insn


def _containing(res, seg, off):
    st = res.dis.state[seg]
    assert st[off] in (1, 2), f"seg{seg}:{off:#x} is not inside a decoded instruction"
    o = off
    while st[o] != HEAD:
        o -= 1
    return res.dis.insns[seg][o]


def test_findfirst_findnext(inspace_asm, inspace_outputs):
    res = inspace_asm
    # the linear sweep loses sync at seg5:0x351; both helpers start with the far prologue
    for entry, al in ((0x352, 0x4F), (0x364, 0x4E)):
        assert (5, entry) in res.dis.functions
        assert [_insn(res, 5, entry + d).mnem for d in (0, 2, 3, 4, 5, 7, 8)] == \
            ["mov", "nop", "inc", "push", "mov", "push", "mov"]
        mov_al = _insn(res, 5, entry + 11)
        assert (mov_al.mnem, mov_al.ops) == ("mov", f"al, {al:#x}")
    assert res.dis.state[5][0x351] == 0                       # padding byte, not code
    # the shared tail issues the search through DOS3Call with AH = AL = 4Eh/4Fh
    call = _insn(res, 5, 0x3B1)
    assert res.dis.dos_call_site(call)
    assert res.dis.reg_values(5, 0x3B1, "ah") == ({0x4E, 0x4F}, False)
    _listings, _fj, xj = inspace_outputs
    dos = {d["at"]: d for d in xj["dos"]}
    assert dos["seg5:0x3b1"]["names"] == ["findfirst", "findnext"]


def test_all_dos3call_sites_decoded(inspace_asm):
    res = inspace_asm
    sites = [s for seg in res.sites.values() for s in seg.values()
             if (s.module, s.ordinal) == ("KERNEL", 102)]
    assert len(sites) == 39 and {s.seg for s in sites} == {5}
    for s in sites:
        insn = _containing(res, s.seg, s.off)
        assert insn.flow == F_CALLF and insn.off == s.off - 1
        assert insn.off + insn.op_start <= s.off and s.off + s.width <= insn.end


def test_extract_icon_sites_decoded(inspace_asm):
    res = inspace_asm
    for seg, off in ((6, 0x442C), (6, 0x4469), (9, 0x0178), (9, 0x01A5), (9, 0x0251)):
        insn = _insn(res, seg, off)
        assert insn.flow == F_CALLF
        site = res.dis.far_site(insn)
        assert site.label == "SHELL.34 ExtractIcon" and site.off == off + 1


def test_borland_startup_entry(inspace_asm):
    res = inspace_asm
    assert res.img.cs_ip == (5, 0x1A)
    assert "cs_ip" in res.dis.functions[(5, 0x1A)]
    assert (_insn(res, 5, 0x1A).mnem, _insn(res, 5, 0x1A).ops) == ("xor", "bp, bp")
    assert (_insn(res, 5, 0x1C).mnem, _insn(res, 5, 0x1C).ops) == ("push", "bp")
    call = _insn(res, 5, 0x1D)
    assert call.flow == F_CALLF and res.dis.far_site(call).label == "KERNEL.91 InitTask"


def test_ispace_exports_decode(ispace_asm):
    res = ispace_asm
    assert len(res.img.entries) == 44
    for ordinal, seg, off, _name in res.img.entries:
        assert res.dis.state[seg][off] == HEAD, f"export {ordinal} seg{seg}:{off:#x}"
        assert "export" in res.dis.functions[(seg, off)]
        assert res.bodies[(seg, off)][0], f"export {ordinal} has no body"


@pytest.mark.parametrize("fixture", ["inspace_asm", "ispace_asm", "factory_asm"])
def test_relocation_sites_in_operands(request, fixture):
    """S2c: every relocation site lies in operand bytes, or in a listed data region."""
    res = request.getfixturevalue(fixture)
    rc = asm.relocation_check(res)
    assert rc["bad"] == 0 and rc["undecoded"] == 0
    assert rc["in_operands"] + rc["in_data"] == rc["sites"]
    # the stub's self-load table (3 selector chain sites, 2 import cells) and the
    # runtime library's __WINFLAGS word read through cs:[0x10]
    assert rc["in_data"] == 6
    assert {u["where"].split()[1] for u in rc["uncovered"]} == {"cs_data", "header"}
    assert {u["label"] for u in rc["uncovered"] if "cs_data" in u["where"]} == \
        {"KERNEL.178 __WINFLAGS"}
    assert all(u["site"].startswith("seg1:") for u in rc["uncovered"] if "header" in u["where"])


def test_inspace_code_fully_decoded(inspace_asm):
    """Outside the OPTLOADER stub only alignment padding stays undecoded."""
    res = inspace_asm
    for seg in range(2, 19):
        for a, b in res.dis.gaps(seg):
            chunk = res.dis.segs[seg].data[a:b]
            assert asm.is_padding(chunk) or (seg, a) == (5, 0), (seg, hex(a), chunk.hex())
    assert not res.dis.unresolved
    assert len(res.dis.switches) == 42
    assert (8, 0x12C6) in res.unreachable                      # a dead loop kept by the compiler


def test_legacy_inventories_are_covered(inspace_asm):
    res = inspace_asm
    legacy = json.loads((ARTIFACTS / "function-entries.json").read_text())
    entries = {(int(s), o) for s, offs in legacy.items() for o in offs}
    assert len(entries) == 880 and entries <= set(res.dis.functions)
    cg = json.loads((ARTIFACTS / "callgraph.json").read_text())["entries"]
    far = {((c.seg, c.off), c.target) for c in res.dis.calls if c.kind == "far"}
    for e in cg:
        assert (e["tseg"], e["toff"]) in res.dis.functions
        for caller in e["callers"]:
            assert ((caller[0], caller[1]), (e["tseg"], e["toff"])) in far


def test_listing_uses_canonical_addresses(inspace_outputs):
    listings, fj, _xj = inspace_outputs
    text = listings["seg05.asm"]
    head = text.splitlines()[:3]
    assert head[0].startswith("; INSPACE.ex_ (INSPACE) seg5  CODE  flags 0x0c58")
    assert head[1].startswith("; seg5:0x0..seg5:0x209d: 8350 bytes (OPTLOADER-packed, "
                              "5846 bytes on disk at file:0x8fb0")
    assert re.search(r"^seg5:0x1d +9a ff ff 00 00 +call far KERNEL\.91 InitTask$", text, re.M)
    assert "; ---- function seg5:0x352  [far_call]" in text
    assert fj["functions"][0]["addr"].startswith("seg")


def test_listing_bytes_column(factory_asm):
    """Every instruction line reads back with its bytes; 9-byte ones keep a space."""
    res = factory_asm
    listings, _fj, _xj = asm.build_outputs(res)
    nine = 0
    for name, text in listings.items():
        for line in text.splitlines():
            p = asm.parse_line(line)
            if p is None or not p[2] or p[3].startswith(("db ", "dw ")):
                continue
            seg, off, raw, txt, _c = p
            insn = res.dis.insns[seg][off]
            assert raw == res.dis.segs[seg].data[off:insn.end], line
            assert txt.startswith((insn.mnem, "call far", "jmp far")), line
            nine += insn.size == 9
    assert nine == 14


def test_additive_relocation_shows_addend(ispace_outputs):
    listings, _fj, _xj = ispace_outputs
    assert re.search(r"^seg3:0x23 +9a fb ff 00 00 +call far KERNEL\.91 InitTask\+0xfffb +; "
                     r"additive PTR16_A: target offset \+0xfffb \(-5\)$", listings["seg03.asm"],
                     re.M)


def _shown(outputs, seg, off):
    listings, fj, _xj = outputs
    index = asm.FunctionIndex(fj)
    func, others = index.containing(seg, off)
    lines = asm.function_lines(asm.Listing(listings[f"seg{seg:02d}.asm"]), index, func, others)
    return func["addr"], "\n".join(ln for ln, _o in lines)


def test_function_listing_has_tails_and_unreachable_code(inspace_outputs):
    # a jump to seg8:0x1316 passes the dead loop at seg8:0x12c6
    fa, text = _shown(inspace_outputs, 8, 0x1316)
    assert fa == "seg8:0x1290" and text.startswith("; ---- function seg8:0x1290 ")
    assert "; ---- unreachable code seg8:0x12c6 inside function seg8:0x1290" in text
    assert re.search(r"^seg8:0x131c +ca 08 00 +retf 8$", text, re.M)
    assert _shown(inspace_outputs, 8, 0x12D0)[0] == "seg8:0x1290"
    # findnext jumps into findfirst's body and shares its tail
    fa, text = _shown(inspace_outputs, 5, 0x352)
    assert "; .... seg5:0x364..seg5:0x373 skipped: function seg5:0x364" in text
    assert re.search(r"^seg5:0x3b1 .*DOS3Call +; AH=4Eh/4Fh findfirst/findnext$", text, re.M)
    fa, text = _shown(inspace_outputs, 5, 0x3B1)
    assert fa == "seg5:0x364" and "; shares code with function seg5:0x352" in text
    # a switch's jump table belongs to the function of its jmp
    listings, fj, _xj = inspace_outputs
    sw = fj["switches"][0]
    seg, at = asm._addr_key(sw["at"])
    _fa, text = _shown(inspace_outputs, seg, at)
    assert f"; jump table for {sw['at']}" in text


def test_cli_output_is_deterministic(original_dir, tmp_path):
    ne = require(original_dir / "game" / "ISPACE.dll")
    outs = []
    for seed in ("1", "2"):
        out = tmp_path / f"run{seed}"
        env = dict(os.environ, PYTHONHASHSEED=seed)
        subprocess.run([sys.executable, "-I", "-m", "isre", "asm", "--ne", str(ne), "--out",
                        str(out)], check=True, cwd=REPO_ROOT, env=env, capture_output=True)
        outs.append({p.name: p.read_bytes() for p in sorted(out.iterdir())})
    assert outs[0] == outs[1]
    assert {"functions.json", "xrefs.json", "seg05.asm"} <= set(outs[0])
    fj = json.loads(outs[0]["functions.json"])
    assert fj["relocation_check"]["bad"] == 0


def test_show_prints_function(original_dir, tmp_path):
    ne = require(original_dir / "game" / "ISPACE.dll")
    from isre.asm import main, show

    assert main(["--ne", str(ne), "--out", str(tmp_path)]) == 0
    text = show(tmp_path, "seg2:0x30")
    assert text.startswith("; ---- function seg2:0x26  [export]  export 43 WEP")
    assert "seg2:0x30" in text and "retf" in text
    # seg3:0x2e lies past the function at seg3:0x2b, in a tail of seg3:0x12
    text = show(tmp_path, "seg3:0x2e")
    assert text.startswith("; ---- function seg3:0x12 ")
    assert "; .... seg3:0x2a..seg3:0x2d skipped: function seg3:0x2b" in text
    assert re.search(r"^seg3:0x2e ", text, re.M) and re.search(r"^seg3:0x47 ", text, re.M)
    # padding between functions: a window around it
    assert "seg3:0x2a" in show(tmp_path, "seg3:0x2a")
    with pytest.raises(asm.ShowError, match="seg30 of .* has no listing"):
        show(tmp_path, "seg30:0x0")
    with pytest.raises(asm.ShowError, match="unrecognised address"):
        show(tmp_path, "nowhere")
    assert main(["--out", str(tmp_path), "--show", "seg30:0x0"]) == 2


def test_work_loader_matches_ne_loader(repo_root, inspace, inspace_unpacked):
    work = repo_root / "work"
    if not (work / "ne" / "inspace.json").is_file() or not (work / "unpacked" / "inspace").is_dir():
        pytest.skip("work/ has no isre all output")
    a = asm.load_work(work, "inspace")
    b = asm.load_ne(inspace, inspace_unpacked, "inspace")
    assert a.entries == b.entries and a.cs_ip == b.cs_ip and a.autodata == b.autodata
    assert a.records == b.records and a.std_relocs == b.std_relocs
    assert {n: (s.data, s.length, s.flags) for n, s in a.segs.items()} == \
        {n: (s.data, s.length, s.flags) for n, s in b.segs.items()}


def test_module_counts(inspace_asm, ispace_asm, factory_asm):
    assert len(inspace_asm.bodies) >= 930
    assert len(ispace_asm.bodies) >= 280
    assert len(factory_asm.bodies) >= 240
