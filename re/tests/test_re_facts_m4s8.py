"""Mechanical facts of the M4 stage 8 analyses (work/analysis/m4/s8-{size,hits}.md with the verifiers' corrections
and s8-owed-verify.md): the Size rescale seg10:0x9a2a, the image rescale seg10:0x6308, the scaling primitive
seg10:0x9962, seg10:0x7f88 and the Size keys; the launched weapon's contact chain (seg8:0x5df2, seg7:0x2652,
seg12:0x274a, seg7:0x2e02, seg8:0x5ab0); and the facts stage 7 owes (the eleven DGROUP:0x9498 sites, the per-frame
clamp seg7:0x1084-0x10b7, the sounds table row 0x19).

The listing model, the fixtures and the helpers come from test_re_facts.py and test_re_facts_m4s5.py. The byte
facts are data-driven: ``"segN:0xOFF[-0xOFF]  hex bytes"``. Tests skip when ``work/`` or ``original/`` is absent.
"""
from __future__ import annotations

import re
import struct

import pytest

from test_re_facts import (  # noqa: F401  (ins, isp, dgroup and master are pytest fixtures)
    A, SOUNDS_TSV, _eng, dgroup, ins, isp, master, unpacked_seg, unpacked_segs,
)
from test_re_facts_m4s5 import _disp_stores
from test_re_facts_m4s7 import _bytes_fact, _facts, _td


def s16(v: int) -> int:
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def _sar(v: int, n: int) -> int:
    return s16(v) >> n


def _dim(w: int, size: int) -> int:
    """seg10:0x6308's new extent: lo16(w << Size) sar 1 (`shl`, then `sar 1`)."""
    return _sar((w << size) & 0xFFFF, 1)


def _refs(m, lo: int) -> list[str]:
    return sorted({i.addr for i in m.refs(lo, lo + 1) if i.seg != 19}, key=A)


# ------------------------------------------------------------------ SIZE: byte facts (s8-size.md section 9)

SIZE_BYTES = _facts("""
seg10:0x9a39 83 3e f4 90 01
seg10:0x9a47 c7 06 3c 10 01 00
seg10:0x9a63 3d 02 00
seg10:0x9a89 6a 65
seg10:0x9a92 68 fe 90
seg10:0x9add 40 a3 56 91
seg10:0x9af1 83 7e fc 3a
seg10:0x9af7 83 7e fc 20
seg10:0x9b16 6a fe
seg10:0x9b28 c7 46 fc 0e 00
seg10:0x9b88 2d 04 00 f7 d8
seg10:0x9b90 6a 01 6a 22
seg10:0x9c40 b8 0c 00
seg10:0x9c4b 26 c7 47 24 04 00
seg10:0x9cd7 c7 46 fc 30 00
seg10:0x9dbd 05 28 00
seg10:0x9e46 c7 46 fc 17 00
seg10:0x9e6e 83 7e fc 1c
seg10:0x9e5b 6b 5e fc 06
seg10:0x9e80 83 bf 80 3d 05
seg10:0x9f09 c1 e0 03
seg10:0x9f56 c7 87 6c 3d 01 00
seg10:0x9f83 6b 46 fc 26 05 06 00 50 6a 06 6a 20 6a 20
seg10:0xa001 d3 f8 03 c0 99 2b c2 d1 f8
seg10:0xa02c b9 04 00
seg10:0xa0df 68 78 09
seg10:0xa101 6a 42
seg10:0x6334 83 7e 0c 02
seg10:0x6359 83 7e 06 09
seg10:0x635f 83 7e 06 08
seg10:0x638f c7 06 40 07 01 00
seg10:0x63bf c7 06 40 07 01 00
seg10:0x63c8 6a 0e
seg10:0x6546 6a 03
seg10:0x654d 6a 02
seg10:0x6554 68 cc 00 6a 20
seg10:0x9977 6a 02 6a 02
seg10:0x999b 83 f9 08
seg10:0x99ad 83 3e 42 07 00
seg10:0x7fa0 68 f3 07 6a 01
seg10:0x544 83 7e fc 3a
seg10:0x5c64 b8 09 00
seg10:0x5c56 6b 06 56 91 06 05 03 00
seg10:0x5c79 6a 00 6a 10 6a 00
seg11:0x6cdc 6a 11
seg11:0x6cd5 81 7e 0c 9e 09
seg11:0x6e45 3d bd 00
seg11:0x6ed2 2c 11
seg11:0x6ed8 2c 02
seg11:0x6ede 2c 4e
seg9:0x23f0 68 b7 09 6a 01
seg6:0x1bb1 6a 42
seg6:0x1bb5 68 60 04
seg9:0x2617 6a 42
""")


@pytest.mark.parametrize("addr,hexs", SIZE_BYTES, ids=[f"{a}:{h[:14]}" for a, h in SIZE_BYTES])
def test_size_bytes(ins, addr, hexs):
    """s8-size.md Facts for tests: the instruction bytes of the rescale, the image rescale, the keys and the load."""
    assert _bytes_fact(addr, hexs)


def test_size_dgroup_data(dgroup):
    """s8-size.md Facts for tests: the strings and the two words at DGROUP:0x740."""
    assert dgroup[0x8F8:0x908] == b"Loading Ship # \0"
    assert dgroup[0x908:0x90D] == b"Size\0" and dgroup[0x90D:0x915] == b"SetSize\0"
    assert dgroup[0x7F3:0x7F8] == b"Size\0"
    assert struct.unpack_from("<hh", dgroup, 0x740) == (0, 1)


def test_size_strings():
    """ENG_LANG.DLL STRING 142 id 2269 and STRING 140 ids 2230-2232 (the Zoom combo order 2x, 1x, 1/2x)."""
    assert _eng(2269) == "Resizing graphics...Please hold on."
    assert [_eng(i) for i in (2230, 2231, 2232)] == ["2x", "1x", "1/2x"]


def test_quick_word_references(ins):
    """s8-size-verify.md claim 3: DGROUP:0x931e has 13 code references, all direct, one store in seg6:0xb3e and
    three in seg10:0x9a2a, and only `cmp` reads at the three sheet `sel` tests."""
    refs = [i for i in ins.refs(0x931E, 0x931F) if i.seg != 19]
    assert len(refs) == 13 and all(i.abs_ds for i in refs)
    inside = sorted(i.addr for i in refs if ins.in_func(i, "seg10:0x9a2a"))
    assert inside == ["seg10:0x9a50", "seg10:0x9a56", "seg10:0xa10b"]
    reads = {i.addr: i.mnem for i in refs if i.addr not in ("seg6:0xc65",) and not ins.in_func(i, "seg10:0x9a2a")}
    assert len(reads) == 9 and set(reads.values()) == {"cmp"}
    assert ins.at("seg6:0xc65").mnem == "mov"


def test_size_flag_words(ins):
    """s8-size-verify.md claims 6, 45 and O7 and s8-size.md 1.2: the reference sets of DGROUP:0x103a, 0x92e2, 0x742,
    0x959e and 0x103c."""
    assert _refs(ins, 0x103A) == ["seg9:0x274e", "seg9:0x2755", "seg9:0x6992", "seg10:0x9ab6"]
    assert _refs(ins, 0x92E2) == ["seg6:0x95ed", "seg9:0x23b4", "seg9:0x23e4", "seg9:0x2418", "seg10:0x803f"]
    assert len(_refs(ins, 0x742)) == 2 and "seg10:0x99ad" in _refs(ins, 0x742)
    r = _refs(ins, 0x959E)
    assert len(r) == 12 and r[:4] == ["seg16:0x492", "seg16:0xe54", "seg16:0x2059", "seg16:0x2615"]
    assert all(a.startswith("seg18:") for a in r[4:])


def test_design_word_readers(ins):
    """s8-size-verify.md claim 25: design words +0x00 and +0x02 (DGROUP:0x96d2, 0x96d4) are read only at seg10:0x147b
    and seg10:0x163b and written at seg10:0xa015, seg10:0xa019, seg14:0xc50, seg14:0xc54, seg16:0x60e7, seg16:0x60eb;
    the callers of seg10:0x1460 and seg10:0x1620 are seg14:0xcdc, seg14:0xce6, seg16:0x610f and seg16:0x6118."""
    refs = {i.addr for i in ins.insns if i.mem is not None and (i.mem.disp & 0xFFFF) in (0x96D2, 0x96D4)
            and i.seg != 19}
    assert refs == {"seg10:0x147b", "seg10:0x163b", "seg10:0xa015", "seg10:0xa019", "seg14:0xc50", "seg14:0xc54",
                    "seg16:0x60e7", "seg16:0x60eb"}
    assert sorted(ins.call_sites("seg10:0x1460") + ins.call_sites("seg10:0x1620"), key=A) == sorted(
        ["seg14:0xcdc", "seg14:0xce6", "seg16:0x610f", "seg16:0x6118"], key=A)


def test_rescale_callers_and_key_sites(ins):
    """s8-size.md section 1 and 3: the callers of seg10:0x9a2a (seg11:0x7207 and the three in seg10:0x7f88) and of
    seg10:0x7f88 (seg7:0xb7a, seg10:0x7f58, seg10:0x8409); seg10:0x522 frees slots 0 .. 0x39."""
    assert sorted(ins.call_sites("seg10:0x9a2a"), key=A) == sorted(
        ["seg11:0x7207", "seg10:0x7ff4", "seg10:0x8008", "seg10:0x800f"], key=A)
    assert sorted(ins.call_sites("seg10:0x7f88"), key=A) == sorted(["seg7:0xb7a", "seg10:0x7f58", "seg10:0x8409"],
                                                                   key=A)


def test_rescale_arithmetic():
    """s8-size.md 1.4, 2.1 and 1.7: lo16(w << Size) sar 1 for the images (869 x 53 rocks), the ship pool's odd-side
    rule, the pool-3 gaps and the design words (2 * (side sar Size)) / 2 for the 33-px TRANSFORMER."""
    assert (_dim(869, 2), _dim(53, 2)) == (1738, 106) and (_dim(869, 0), _dim(53, 0)) == (434, 26)
    assert _dim(33, 1) == 33 and _dim(33, 2) == 66 and _dim(33, 0) == 16
    side, gap = 33, 5            # the TRANSFORMER at Size 0: an odd side is made even before halving
    side, gap = side + 1, gap - 1
    assert (_dim(side, 0), _dim(gap, 0)) == (17, 2) and (_dim(33, 2), _dim(5, 2)) == (66, 10)
    assert _dim(22, 2) == 44 and _dim(2, 2) == 4           # small rocks 22/2 -> 44/4 (the gap is also forced to 4)
    assert _dim(22, 0) == 11 and 22 < (12 << 2)            # the pool-3 threshold 12 << newSize
    def design(side_, size):
        if size == 0 and side_ & 1:
            side_ += 1
        return _td(s16(2 * (side_ >> size)), 2)
    assert design(33, 2) == 8 and design(33, 0) == 34 and design(33, 1) == 16


def test_flame_cell_size():
    """s8-size.md 1.3: seg10:0x5c42's f = (9 << Size) sar 1, 18 at Size 2 and 4 at Size 0."""
    assert [(9 << s) >> 1 for s in (0, 1, 2)] == [4, 9, 18]


# ------------------------------------------------------------------ stage 7 owes (s8-owed-verify.md)

def test_error_counter_sites_all_eleven():
    """s8-owed-verify.md item 4: a byte search of the unpacked code segments 2-18 for `98 94` finds exactly eleven
    hits, each the operand of `inc word ptr [0x9498]` (ff 06 98 94)."""
    hits, incs = [], []
    for seg, data in unpacked_segs("inspace").items():
        if not 2 <= seg <= 18:
            continue
        for m in re.finditer(b"\x98\x94", data):
            hits.append((seg, m.start()))
            if data[m.start() - 2:m.start() + 2] == b"\xff\x06\x98\x94":
                incs.append(f"seg{seg}:{m.start() - 2:#x}")
    assert len(hits) == 11
    assert sorted(incs, key=A) == sorted(["seg3:0x149", "seg3:0x3029", "seg3:0x31b1", "seg8:0x8f78", "seg11:0x2514",
                                          "seg11:0x8a31", "seg11:0xa273", "seg12:0x1384", "seg12:0x2767",
                                          "seg13:0x1afc", "seg14:0x3f62"], key=A)


def test_error_counter_instructions(ins):
    for a in ["seg3:0x149", "seg3:0x3029", "seg3:0x31b1", "seg8:0x8f78", "seg11:0x2514", "seg11:0x8a31",
              "seg11:0xa273", "seg12:0x1384", "seg12:0x2767", "seg13:0x1afc", "seg14:0x3f62"]:
        assert ins.at(a).raw.hex() == "ff069894", a


CLAMP = _facts("""
seg7:0x1084 c7 46 fc 00 00
seg7:0x1090 83 bf 06 94 00
seg7:0x1095 7c 0a
seg7:0x109b 39 87 ec 92
seg7:0x109f 7d 0d
seg7:0x10a6 8b 87 ec 92
seg7:0x10aa 89 87 06 94
seg7:0x10b7 7e d2
seg7:0x20f6 26 83 7f a8 00
seg7:0x20fd 26 80 7f e1 00
seg13:0x1693 6a ff
seg13:0x1697 9a 80 20 af 18
seg7:0x472b 9a 80 20 3b 3b
""")


@pytest.mark.parametrize("addr,hexs", CLAMP, ids=[a for a, _ in CLAMP])
def test_clamp_and_recount_bytes(ins, addr, hexs):
    """s8-owed-verify.md items 1 and 2: the per-frame clamp (pools 0 .. DGROUP:0x9324, in-use DGROUP:0x9406 + 2i
    negative or above the allocated DGROUP:0x92ec + 2i stores the allocated count), seg7:0x2080's live test (`state`
    +0x18 = 0 or byte +0x51 <> 0) and the launch and ball-end recounts."""
    assert _bytes_fact(addr, hexs)


def test_in_use_recount_call_sites(ins):
    """seg7:0x2080 is called with (5, -1) at seg13:0x1697, from the ball's end at seg7:0x472b and from seg7:0x59b2,
    seg12:0x3135 and seg8:0x67cc as the analysis lists them."""
    sites = ins.call_sites("seg7:0x2080")
    for a in ("seg13:0x1697", "seg7:0x472b", "seg7:0x59b2", "seg12:0x3135", "seg8:0x67cc"):
        assert a in sites, a


def test_retarget_gate_bytes(ins):
    """s8-owed-verify.md FIRE DEMONS: seg12:0xa6-0xab `cwd; or dx, dx; jne` after the idiv by 13, age > 8 at
    seg12:0xad."""
    assert ins.at("seg12:0xad").raw.hex() == "83fe08"
    assert ins.at("seg12:0xab").mnem == "jne"


def test_tab_clear_bytes(ins):
    """s8-owed-verify.md the Tab rule: seg11:0xae04 reads the old slot (ship word +0x18), seg11:0xae61 stores slot 0
    for a slot count of at most 1."""
    assert ins.at("seg11:0xae04").raw.hex() == "8b87a870" and ins.at("seg11:0xae61").raw.hex() == "c787a8700000"


def test_tick_branch_bytes(ins):
    """s8-owed-verify.md item 3 and 9: the frame's 1500 ms tick: `cmp [0x951a], 0` at seg7:0xdce, `add ax, 0x5dc` at
    seg7:0xdd7, seg13:0x6716 at seg7:0xde9."""
    assert ins.at("seg7:0xdce").raw.hex() == "833e1a9500" and ins.at("seg7:0xdd7").raw.hex() == "05dc05"
    assert ins.at("seg7:0xde9").raw.hex() == "9a1667b30e"


def test_sounds_row_0x19_has_the_snowball_launch():
    """re/artifacts/sounds.tsv row 0x19 lists seg13:0x1318's launch call (the `push 0x19` at seg13:0x1687 jumps into
    the shared call at seg13:0x1676)."""
    rows = {l.split("\t")[0]: l.split("\t") for l in SOUNDS_TSV.read_text().splitlines()[1:]}
    r = rows["0x19"]
    assert "P:seg13:0x1676 in seg13:0x1318(5,[bp - 4],1)" in r[9] and "seg13:0x1687" in r[8]
    assert "P:seg12:0x3111 in seg12:0x2db6(" in r[9]


def test_snowball_sound_push_bytes(ins):
    assert ins.at("seg13:0x1687").raw.hex() == "6a19" and ins.at("seg13:0x1674").raw.hex() == "6a1b"
    assert ins.at("seg13:0x1689").mnem == "jmp"


# ------------------------------------------------------------------ HITS (s8-hits.md Facts for tests)

HITS_BYTES = _facts("""
seg12:0x2a6f c7 87 9a 1e 04 00
seg12:0x28ce c7 87 9a 1e 01 00
seg12:0x291b c7 84 92 1e 0a 00
seg12:0x2af5 8b 84 72 2f
seg12:0x2afd c7 87 e4 66 ff ff
seg12:0x2b0b 6a 45
seg8:0x4238 b9 06 00
seg8:0x4242 83 bf 9a 1e 01
seg8:0x42d0 83 bf 90 1e 05
seg8:0x42d7 83 bf 9a 1e 06
seg8:0x5aec 83 3e 18 13 06
seg8:0x5af6 a1 26 93
seg8:0x5ce3 81 7e fc 06 ff
seg8:0x5d5e 83 7e f2 64
seg8:0x5d7d 3d 07 00
seg8:0x5d85 05 08 00
seg8:0x5d88 6b c0 0f
seg8:0x5d9c 69 c1 58 02
seg8:0x5b22 6b 46 ec 0f
seg8:0x5b2c 83 7e f6 02
seg8:0x5b49 26 80 41 2f 02
seg8:0x5b4e 81 7e f2 96 00
seg8:0x5b5d 26 80 40 2f 02
seg8:0x5b70 26 80 78 2f 09
seg8:0x5bd4 ba 64 00
seg8:0x5be6 b9 0a 00
seg8:0x5c0c 26 69 41 48 fa 00
seg8:0x6023 83 7e 0a 06
seg8:0x6040 83 bf 78 2f 06
seg8:0x6060 83 bf 78 2f 06
seg11:0x80e1 83 bf 78 2f 04
seg11:0x80e8 83 bf 78 2f 06
seg11:0x80f7 26 c7 40 66 02 2e
seg11:0x810d 26 c7 40 66 4a 27
seg13:0x1e9c 83 bf 96 2f 00
seg13:0x1ec6 66 05 b0 04 00 00
seg13:0x1edf 83 c1 14
seg13:0x1f1d 6a 23
seg13:0x1f30 6a 71
seg7:0x2e9e 83 bf 78 2f 06
seg7:0x2ec0 6a 1f
seg7:0x300e 83 bf 9a 1e 05
seg12:0x382 26 80 78 53 00
seg12:0x39b 38 a7 7a 71
seg12:0x3b1 83 bf 78 2f 06
seg12:0x3d8 83 bc 78 2f 06
seg12:0x407 fe 80 3e 71
seg12:0x41d 6a 06
seg12:0xa50 c7 87 9a 1e 03 00
seg12:0xad3 c7 87 fa 66 00 00
seg8:0x1962 8a c1
seg8:0x1964 8a e2
""")


@pytest.mark.parametrize("addr,hexs", HITS_BYTES, ids=[f"{a}:{h[:14]}" for a, h in HITS_BYTES])
def test_hits_bytes(ins, addr, hexs):
    """s8-hits.md Facts for tests: the effect-entry stores, the blast constants, the dispatcher's pool-6 and kind-6
    tests, the launch's `on_destroy` choice, the touch routine's doughnut words and the dead routine's lock store."""
    assert _bytes_fact(addr, hexs)


def test_hits_kill_tables(dgroup):
    """s8-hits.md Facts for tests: the kill tables DGROUP:0xb0e (21 words), DGROUP:0xb38 (21 words) and the strip
    offsets DGROUP:0xb70."""
    p = list(struct.unpack_from("<21h", dgroup, 0xB0E))
    assert p[:14] == [1000, 600, 1200, 900, 200, 0, 1000, 0, 3400, 500, 500, 0, 0, 5000] and p[14:] == [0] * 7
    assert p[6] == 1000 and struct.unpack_from("<h", dgroup, 0xB1A)[0] == 1000
    w = list(struct.unpack_from("<21h", dgroup, 0xB38))
    assert w[:15] == [1, 1, 1, 2, 0, 0, 0, 0, 4, 3, 0, 0, 0, 0, 8] and w[15:] == [0] * 6
    assert struct.unpack_from("<h", dgroup, 0xB44)[0] == 0
    assert list(struct.unpack_from("<8h", dgroup, 0xB70)) == [0, 10, 26, 48, 76, 110, 150, 196]
    assert struct.unpack_from("<h", dgroup, 0xB0C)[0] == 1


def test_lock_word_writers(ins):
    """s8-hits-verify.md claim 18: the stores to [... + 0x66fa] are exactly seven."""
    assert _disp_stores(ins, 0x66FA) == sorted(["seg11:0x7f4b", "seg11:0x7f5d", "seg12:0x4a4", "seg12:0x5e9",
                                                "seg12:0x7a1", "seg12:0x8e9", "seg12:0xad3"], key=A)


def test_star_colour_word_writers(ins):
    """claim 14: the stores to [... + 0x2f04] are five, each of a word whose low byte is `cl` = 0xff, and no
    instruction addresses DGROUP:0x2f28 directly."""
    assert _disp_stores(ins, 0x2F04) == sorted(["seg8:0x1453", "seg8:0x14b4", "seg8:0x1707", "seg8:0x1781",
                                                "seg8:0x1966"], key=A)
    assert [i.addr for i in ins.insns if i.mem is not None and (i.mem.disp & 0xFFFF) == 0x2F28] == []


def test_blast_flag_references(ins):
    """claim 13: DGROUP:0xb0c has two readers, seg8:0x616a and seg8:0x6589, and the blast's stores."""
    refs = sorted({i.addr for i in ins.refs(0xB0C, 0xB0D) if i.seg != 19}, key=A)
    assert "seg8:0x616a" in refs and "seg8:0x6589" in refs and "seg8:0x5b88" in refs and "seg8:0x5bb8" in refs


def test_dead_routines_have_no_callers(ins):
    """claim 21: seg12:0xa26 and seg12:0x2b20 have no caller, and no instruction holds 0xa26 or 0x2b20 as an
    immediate except `push 0xa26` at seg9:0x7728."""
    assert ins.call_sites("seg12:0xa26") == [] and ins.call_sites("seg12:0x2b20") == []
    for imm, ok in (("0xa26", {"seg9:0x7728"}), ("0x2b20", set())):
        found = {i.addr for i in ins.insns if imm in i.ops}
        assert found == ok, (imm, found)


def test_blast_numbers():
    """s8-hits.md blast numbers: B from W+0x22 and R = (min(B, 7) + 8) * 15."""
    bs = [7, 7, 9, 6, 6, 8, 6, 6, 6, 1, 4, 7, 5, 3, 20]
    assert [(min(b, 7) + 8) * 15 for b in bs] == [225, 225, 225, 210, 210, 225, 210, 210, 210, 135, 180, 225, 195,
                                                  165, 225]


def test_blast_amount_arithmetic():
    """claim 11 and B4: seg8:0x1290(mass, 25) = (mass/25 + 25) >> 1 and the 16-bit amount ((bs*600/d)*15)/max(1, S)."""
    S = lambda m: ((m // 25) + 25) >> 1
    assert S(20) == 12 and S(350) == 19
    bs, d, mass = 8, 100, 20
    a = _td(s16(bs * 600), d)
    assert _td(s16(a * 15), max(1, S(mass))) == _td(s16(_td(s16(bs * 600), d) * 15), 12)
