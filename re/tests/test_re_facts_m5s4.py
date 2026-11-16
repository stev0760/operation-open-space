"""Mechanical facts of the M5 stage 4 analyses (work/analysis/m5/s4-{crime,hall,commands}.md with the verifiers'
corrections, and s4-owed-verify.md): the cheat keys of seg11:0x72e0, the crime sites and the crime routine
seg13:0xb6f2, the arrest record, the warrant seg13:0xb93a, the Hall of Justice (seg16:0x6928 and seg16:0x6afe with
its fine and its text), the player's commands (seg13:0x81c2 ... 0xa01c, the replies of seg3:0x6ec), the reactions
seg13:0x7ba2, and the readings of stage 3 that the owed verification checked.

The listing model, the fixtures and the helpers come from test_re_facts.py, test_re_facts_m4s7.py and
test_re_facts_m5s3.py. The byte facts are data-driven: ``"segN:0xOFF[-0xOFF]  hex bytes"``; a single address needs the
bytes there, a range needs them somewhere in it. The arithmetic of the Hall (fine, severity, ordinals, officer number,
fixed-point text) is restated in this file from the analysis and checked against the numbers that the oracle runs
printed (oracle/digests/ships-0003.md, ships-0004.md). Tests skip when ``work/`` or ``original/`` is absent.
"""
from __future__ import annotations

import struct

import pytest

from test_re_facts import (  # noqa: F401  (ins, isp, dgroup and master are pytest fixtures)
    A, _eng, dgroup, ins, isp, master, unpacked_seg,
)
from test_re_facts_m4s7 import _bytes_fact, _facts


def _s16(v: int) -> int:
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def _idiv(a: int, b: int) -> int:
    """Signed division truncating toward zero (the `cwd; idiv` forms)."""
    q = abs(a) // abs(b)
    return q if (a < 0) == (b < 0) else -q


# ------------------------------------------------------------------ CHEAT KEYS and the crime sites: byte facts

LAW_BYTES = _facts("""
seg11:0x7288 2d 00 01
seg11:0x728f 48
seg11:0x7294 2d 9a 08
seg11:0x7299 2d 03 00
seg11:0x6ee7 83 3e 0c 92 02
seg11:0x6eee 83 3e 0c 92 ff
seg11:0x6ef7 83 3e 1c 95 01
seg11:0x6efc 1b c0
seg11:0x733b c7 06 4a 10 01 00
seg11:0x7341 9a dc 00 4c 69
seg11:0x7346 b9 0a 00
seg11:0x7349 99
seg11:0x734a f7 f9
seg11:0x7351 9a f2 b6 ac 6d
seg11:0x73a9 c7 06 48 10 00 00
seg11:0x731f 83 3e 16 93 08
seg11:0x7328 6a 6d
seg11:0x732a 6a 16
seg11:0x7330 9a be ad 54 73
seg11:0x7335 83 c4 06
seg11:0x73e0 83 3e 16 93 08
seg11:0x73e7 6a 6a
seg11:0x73e9 6a 04
seg11:0x73ee 83 3e 16 93 08
seg11:0x73f5 6a 69
seg11:0x73f7 6a 03
seg11:0x73fc 83 3e e8 92 00
seg11:0x7406 c7 06 18 13 01 00
seg11:0x7464 83 3e 4e 10 01
seg11:0x746d a3 4e 10
seg11:0x7473 69 1e 46 96 5a 01
seg11:0x7479 83 87 5e 71 64
seg11:0x7496 26 c6 40 33 06
seg11:0x72fa 26 c6 40 32 06
seg11:0x7316 c7 06 3e 91 02 00
seg11:0x7391 83 3e 4c 10 01
seg11:0x739a a3 4c 10
seg11:0x73b2 83 3e 20 93 06
seg11:0x73b9 9a a2 2d 77 42
seg11:0x73d0 83 3e 20 93 07
seg11:0x73d7 c7 06 8a 66 ff ff
seg11:0x735c 6a 16
seg11:0x736a 6a 17
seg11:0x7362 9a 64 6d 73 73
seg13:0xb705 83 3e 20 93 07
seg13:0xaaf4 66 a3 47 a1
seg13:0xaafb a3 4b a1
seg13:0xab01 a2 61 a1
seg13:0xab07 a2 62 a1
seg13:0xab85 66 c7 87 e2 70 02 00 0e 00
seg13:0xa9f7 69 5e 06 ad 00
seg7:0xd60 66 a1 68 1e
seg7:0xd64 66 b9 2c 01 00 00
seg7:0xd6d 66 f7 f1
seg7:0xd78 9a 3a b9 ec 0d
seg3:0x1346 48
seg3:0x1347 3d bf 00
seg3:0x134a 0f 87 88 01
seg4:0x7669 83 7e 08 0e
seg4:0x766d 75 23
seg4:0x767a 83 bc 0a 97 07
seg4:0x7692 69 5e 06 5a 01
seg4:0x7697 83 bf c2 70 0e
seg4:0x76a3 80 b8 87 71 03
seg4:0x7790 8a 87 4d a1
seg4:0x7794 88 87 57 a1
seg9:0x6c62 c7 06 f4 90 08 00
seg9:0x6c6a a3 9c 92
seg9:0x6c6e 9a 28 69 0f 3a
seg9:0x6c74 c7 06 46 93 ff ff
seg7:0x652 83 3e a4 93 00
seg7:0x659 c7 06 f4 90 08 00
seg7:0x65f c7 06 9c 92 00 00
seg7:0x665 6a 01
seg7:0x667 e9 4d fb
seg3:0x2d40 a0 63 a1
seg3:0x2d44 05 4a 04
seg3:0x2d0e 83 bf e2 70 16
seg13:0x4b8e c6 06 63 a1 05
seg13:0x4b93 fe 06 52 a1
seg3:0x6fa a1 46 96
seg3:0x6fd 39 46 06
seg3:0x700 0f 85 36 0e
seg13:0xb94e eb 5c
seg13:0xb950 83 7e fa 09
seg13:0xb96c 83 7e fa 0a
seg13:0xb978 7d 2f
""")


@pytest.mark.parametrize("addr,hexs", LAW_BYTES, ids=[f"{a}:{h[:14]}" for a, h in LAW_BYTES])
def test_law_bytes(ins, addr, hexs):
    """s4-crime.md claims 1-16, 18, 44, 46, 58 and the facts of seg3:0x6ec's bound: the key dispatch, the DELETE
    toggle (`cmp x, 1; sbb`), each key's gate and call, F6's draw, the arrest-record stores, the 300-frame test,
    seg3:0x6ec's first-argument filter, the Terminator copy and its team-7 / pool-14 killer tests (claim 68, corrected),
    the state-8 entry of seg9:0x6bc2 and the demo's seg7:0x652, the crime-5 pack drop, the arrest caption 1098 + the
    last crime, and the warrant's loop bounds (c = 0 ... 8)."""
    assert _bytes_fact(addr, hexs)


def test_cheat_key_table(ins):
    """s4-crime.md 1.2 and facts: the compare chain at seg11:0x749e maps '1' ... '9' and F3 ... F9 to the routines
    of the per-key table (VK 0x31 ... 0x39, 0x72 ... 0x78)."""
    exp = {0x31: "seg11:0x73b2", 0x32: "seg11:0x73d0", 0x33: "seg11:0x73e0", 0x34: "seg11:0x73ee", 0x35: "seg11:0x73fc",
           0x36: "seg11:0x7433", 0x37: "seg11:0x7464", 0x38: "seg11:0x7473", 0x39: "seg11:0x748d", 0x72: "seg11:0x72f1",
           0x73: "seg11:0x7316", 0x74: "seg11:0x731f", 0x75: "seg11:0x733b", 0x76: "seg11:0x735c", 0x77: "seg11:0x7391"}
    # `sub al, 0x31` makes the key base 0x31 for the first `je`; `sub al, 0x39` after '9' makes it 0x72
    got = {}
    base, i = 0, ins.at("seg11:0x74a7")
    while i.off != 0x74fb:
        if i.mnem == "sub" and i.ops[0] == "al":
            base += int(i.ops[1], 0)
        elif i.mnem == "dec" and i.ops[0] == "al":
            base += 1
        elif i.mnem == "je" and i.ops[0].startswith("seg11"):
            got[base] = i.ops[0]
        i = ins.step(i, 1)
    assert got == exp
    assert ins.at("seg11:0x749e").text == "cmp ax, 0x78" and ins.at("seg11:0x74a1").text.startswith("je seg11:0x73a9")


def test_key_up_chain(ins):
    """s4-crime.md facts: the key-up chain at seg11:0x6d22 sends F12 to seg11:0x6d6c, Enter and 'Z' to seg11:0x6e11,
    CAPS LOCK to seg11:0x6dd6, 'P' to seg11:0x6dc4 and the digits '1' ... '9' and F3 ... F9 to seg11:0x6d58."""
    assert ins.at("seg11:0x6d25").text == "cmp ax, 0x7b" and ins.at("seg11:0x6d28").text == "je seg11:0x6d6c"
    base, got, i = 0, {}, ins.at("seg11:0x6d2e")
    while i.off != 0x6d58:
        if i.mnem == "sub" and i.ops[0] == "al":
            base += int(i.ops[1], 0)
        elif i.mnem in ("je", "jbe") and i.ops[0].startswith("seg11"):
            got[base] = i.ops[0]
        i = ins.step(i, 1)
    assert got == {0x0D: "seg11:0x6e11", 0x14: "seg11:0x6dd6", 0x39: "seg11:0x6d58", 0x50: "seg11:0x6dc4", 0x5A: "seg11:0x6e11"}
    assert ins.at("seg11:0x6d56").text == "ja seg11:0x6d5f"       # above F9 (0x78) leaves; F3 ... F9 fall to seg11:0x6d58
    assert ins.at("seg11:0x6d58").text == "cmp word ptr [0x951c], 0"


def test_crime_callers_and_single_callers(ins):
    """s4-crime.md facts (counts): seg13:0xb6f2 has 12 callers, seg13:0xa6bc, seg13:0xb93a and seg13:0x4ac4 one each."""
    assert ins.call_sites("seg13:0xb6f2") == [
        "seg4:0x5fd8", "seg4:0x61ac", "seg7:0x4b95", "seg8:0x5eeb", "seg8:0x653b", "seg8:0x6559", "seg8:0x7761",
        "seg8:0x779b", "seg11:0x7351", "seg13:0x1879", "seg13:0x1f90", "seg13:0x320e"]
    assert ins.call_sites("seg13:0xa6bc") == ["seg13:0x693a"]
    assert ins.call_sites("seg13:0xb93a") == ["seg7:0xd78"]
    assert ins.call_sites("seg13:0x4ac4") == ["seg11:0x7043"]


def test_dg1048_references(ins):
    """s4-crime.md claim 16 and 69: DGROUP:0x1048 has one writer (F9's seg11:0x73a9) and three readers; its image
    value is 1."""
    refs = sorted(i.addr for i in ins.refs(0x1048, 0x1049))
    assert refs == ["seg11:0x73a9", "seg7:0x886", "seg8:0x9479", "seg8:0x9579"]
    assert [i.addr for i in ins.writers(0x1048, 0x1049)] == ["seg11:0x73a9"]


def test_cheat_words_image(dgroup):
    """s4-crime.md claim 69 and facts: DGROUP:0x1048 = 1, DGROUP:0x104a, 0x104c and 0x104e = 0 in the image."""
    assert dgroup[0x1048:0x1050] == bytes.fromhex("01 00 00 00 00 00 00 00")


def test_arrest_record_references(ins):
    """s4-hall.md claim 48: no instruction reads DGROUP:0xa147 ... 0xa14c; its four stores are seg4:0x777c, seg4:0x7783,
    seg13:0xaaf4 and seg13:0xaafb."""
    assert sorted(i.addr for i in ins.refs(0xA147, 0xA14C)) == ["seg13:0xaaf4", "seg13:0xaafb", "seg4:0x777c", "seg4:0x7783"]
    assert all(i in ins.writers(0xA147, 0xA14C) for i in ins.refs(0xA147, 0xA14C))


def test_dga161_store_is_a_byte(ins):
    """s4-hall-verify.md claim 49: seg9:0xd33 stores the byte DGROUP:0xa161 = 0xff (`c6 06 61 a1 ff`), not a dword."""
    assert _bytes_fact("seg9:0xd33", "c6 06 61 a1 ff")


# ------------------------------------------------------------------ MESSAGES (seg3:0x6ec) and strings

def test_message_table_s4():
    """s4-crime.md and s4-commands.md facts: the word at seg3:0x1356 + 2·(code − 1) for the law's and the commands'
    messages; codes 0x8c ... 0xa3 share seg3:0x128f except 0x8d (seg3:0x1255)."""
    seg = unpacked_seg("inspace", 3)
    exp = {0x0D: 0x0CFD, 0x3B: 0x0F61, 0x3C: 0x0F61, 0x3D: 0x0F61, 0x3E: 0x0F61, 0x3F: 0x0F61, 0x43: 0x0F61, 0x44: 0x0F61,
           0x57: 0x0F61, 0x58: 0x0F61, 0x45: 0x0F9D, 0x46: 0x0FAD, 0x5C: 0x0F95, 0x5D: 0x0FB5, 0x65: 0x0FBD, 0x69: 0x1147,
           0x6A: 0x1137, 0x6D: 0x117F, 0x6E: 0x116F, 0x72: 0x14D6, 0x86: 0x0FC5, 0xAE: 0x12CD, 0xB5: 0x0E7B, 0xB7: 0x131B,
           0xB9: 0x0FCD, 0xC0: 0x0FA5,
           0x1: 0x841, 0x2: 0x9CA, 0x3: 0xB80, 0x5: 0xC19, 0x6: 0xC29, 0xE: 0xC31, 0xF: 0xC3D, 0x10: 0xC64, 0x11: 0xC74,
           0x12: 0xC7E, 0x13: 0xAE0, 0x14: 0xC54, 0x15: 0xC96, 0x16: 0xC86, 0x1F: 0xDA1, 0x23: 0xE83, 0x24: 0xE9B,
           0x25: 0xEB3, 0x26: 0xED3, 0x27: 0xEE3, 0x31: 0xF21, 0x32: 0xF19, 0x34: 0x9CA, 0x35: 0xF41, 0x36: 0xF39,
           0x38: 0xF49, 0x40: 0xF59, 0x47: 0xF29, 0x4B: 0x1009, 0x4C: 0x1011, 0x4D: 0x1019, 0x4E: 0x1021, 0x5A: 0x1068,
           0x64: 0xBFE, 0x74: 0x11C8, 0x82: 0x1235, 0x8B: 0x1281, 0xBC: 0x12ED, 0xBD: 0x12FB}
    for code in range(0x8C, 0xA4):
        exp[code] = 0x128F
    exp[0x8D] = 0x1255
    for code, target in exp.items():
        assert struct.unpack_from("<H", seg, 0x1356 + 2 * (code - 1))[0] == target, hex(code)
    assert _bytes_fact("seg3:0x1294", "05 af 06")           # codes 0x8c ... 0xa3 load string code + 1711


def test_law_strings():
    """s4-crime.md facts: the warning, Enforcer and Hall-of-Justice strings of the law."""
    exp = {350: "Be More Careful, %s. Avoid Dangerous Hazards.", 426: "I'm Heading For The Exit. See You Later...",
           508: "Arresting %D For ", 550: "Do Not Provoke Me! Or There Will Be Trouble.",
           551: "The Interceptor Will Make You Abide By The Law!!!", 552: "Follow Me Out. And Don't Try Anything!",
           553: "Alright Tough Guy, I'll Resort To Force.", 554: "I Must Pursue Someone Else. Try To Be A Better Citizen.",
           555: "We Don't Take Orders, We Enforce Order!", 556: "Looking For Trouble? Let Me Do My Job!",
           557: "I Wish I Had More Backup. I'll Get You!", 558: "You Know, A Guy Could Get Arrested For That!",
           570: "Stop That! You Just Broke a Good Icon!", 572: "That Was Evidence! It's Mine To Take.",
           576: "If You Try That Again, You'll Be Terminated!", 578: "You Will Pay For That Next Wave, Tough Guy!",
           707: "We Do Have Laws Here. Do I Have To Send The Patrol?", 730: "Abide By The Law, The Patrolman Is Entering.",
           860: "WE'LL BE WATCHING YOU..", 861: "OBEY THE LAW OR YOU'RE GOING TO GET IT!",
           862: "FLY CAREFULLY AND BE A GOOD CITIZEN..", 863: "IF YOU IGNORE THE LAW, WE'LL TAKE YOU DOWN!",
           950: "You're Not Supposed To Bully. Now Stop It!", 960: "You're Charged With Using Excessive Force.",
           961: "Rule #1 ...Donut Mess With My Doughnuts!!", 962: "Donut Deploying Devices Are Not To Be Disturbed!",
           963: "The Prize Is For The Winner You Loser!", 964: "You're Guilty Of Reckless Thrusting!!",
           1097: "AFTER WITNESSING YOUR DISGRACEFUL,", 1108: "(It's Been A Slow Day)",
           1940: "Hey! I Have An Outstanding Warrant For Your Arrest!", 208: "ENLIGHTENMENT", 209: "UNCERTAINTY",
           212: "INNER FORCE"}
    for sid, text in exp.items():
        assert _eng(sid) == text, sid
    assert [_eng(i) for i in range(1098, 1108)] == [
        " Murdering An Enforcer.", " Destroying An Icon", " Removing Evidence", " Attacking The Defenseless",
        " Taking An Enforcer's Doughnut", " Firing Upon An Officer", " Destroying Doughnut Dispensors",
        " Resisting Arrest.", " Unsportsmanlike Race Conduct", " Being An Idiot."]


def test_absent_law_strings():
    """s4-crime.md facts: the strings 559, 571, 573, 577 and 579 are absent from ENG_LANG.DLL, so message codes 0x44 and
    0x58 load no string."""
    for sid in (559, 571, 573, 577, 579):
        with pytest.raises(KeyError):
            _eng(sid)


def test_team7_attitude_row():
    """s4-crime.md facts and claim 66: row 7 of the team table (ISPACE.DLL #5600 135, words from offset 0x70) is
    0, 0, 0, −200, 200, −200, −250, 250 for the teams 0 ... 7."""
    from conftest import REPO_ROOT
    p = REPO_ROOT / "work" / "assets" / "ispace.dll" / "5600" / "135.bin"
    if not p.is_file():
        pytest.skip("work/assets absent")
    data = p.read_bytes()
    assert struct.unpack_from("<8h", data, 0x70) == (0, 0, 0, -200, 200, -200, -250, 250)


# ------------------------------------------------------------------ THE HALL: byte facts, tables, strings

HALL_BYTES = _facts("""
seg16:0x69b4 81 c1 dc 05
seg16:0x6a39 81 c1 dc 05
seg16:0x69e8 05 a0 0f
seg16:0x6a9b 3d d0 07
seg15:0xef8 05 0e 06
seg15:0x1872 05 6c 07
seg15:0x167a 05 28 00
seg15:0x1605 05 f4 01
seg15:0x3284 6a 6e
seg15:0x31fd 6a 69
seg15:0x903 6a 73
seg16:0x6b12 6a 42 6a 00 68 d0 07
seg16:0x6b0c c7 06 d4 92 00 00
seg16:0x6fea 6a 02 6a 64
seg16:0x7073 6a 02 6a 64
seg17:0x8ee5 05 bc 03
seg16:0x6f67 89 86 2e ff
seg16:0x6f2a 69 1e 46 96 5a 01
seg16:0x7139 9a dc 00 ab 40
seg16:0x713e b9 04 00
""")


@pytest.mark.parametrize("addr,hexs", HALL_BYTES, ids=[f"{a}:{h[:14]}" for a, h in HALL_BYTES])
def test_hall_bytes(ins, addr, hexs):
    """s4-hall.md facts: the Hall's waits (1500, 4000, 2000, 1550, 1900, 40 and 500 ms), the scene ids 110, 105 and 115,
    the 2000-byte text block, the sentence's scale and digits, the 956 of the officer number, the one writer of conf
    and the Hall's only `rand()` % 4."""
    assert _bytes_fact(addr, hexs)


def test_hall_phase_table():
    """s4-hall.md facts: the 11 words at seg16:0x6ae0, cases 3 and 7 sharing 0x6a14, and the bound `cmp ax, 0xa; ja`."""
    seg = unpacked_seg("inspace", 16)
    assert struct.unpack_from("<11H", seg, 0x6AE0) == (0x697A, 0x698B, 0x6993, 0x6A14, 0x6999, 0x69C7, 0x69DB, 0x6A14,
                                                       0x6A1E, 0x6A4F, 0x6A57)


def test_hall_weights_and_literals(dgroup):
    """s4-hall.md facts: the crime weights at DGROUP:0x116a and the literals of the text."""
    assert struct.unpack_from("<10h", dgroup, 0x116A) == (700, 400, 550, 550, 500, 300, 300, 450, 500, 0)
    lits = {0x48D: b"-\0", 0x48F: b".\0", 0x491: b"-\0", 0x493: b".\0", 0x8E8: b":\0", 0x8EA: b"0\0", 0x8EC: b"PM\0",
            0x8EF: b"AM\0", 0x8F2: b" \0", 0x8F4: b"/\0", 0x8F6: b"/\0", 0x9EE: b". \0", 0x9F2: b", \0", 0xA56: b" \0",
            0xA58: b" \0", 0xCED: b" \0", 0x117E: b" (\0", 0x1181: b")\0", 0x1183: b"\r\n\r\n\0", 0x1188: b"\r\n \r\n\0",
            0x118E: b"\r\n\r\n\0", 0x1193: b"\r\n\r\n\0", 0x1198: b"\r\n \r\n\0"}
    for off, text in lits.items():
        assert dgroup[off:off + len(text)] == text, hex(off)


def test_hall_strings():
    """s4-hall.md facts: the string ids of the Hall's text (ENG_LANG.DLL)."""
    assert _eng(908).startswith("THIS IS THE HALL OF JUSTICE, %s.")
    assert _eng(909).startswith("THIS IS YOUR %s ARREST IN %#, %s.")
    assert _eng(912) == "AFTER OBSERVING YOU" and _eng(913) == " ARRESTED YOU AT "
    assert _eng(914) == "YOU ARE CHARGED WITH "
    assert _eng(915).startswith("AFTER DUE CONSIDERATION, YOU HAVE BEEN FOUND GUILTY AS CHARGED")
    assert _eng(916) == "STAY OUT OF TROUBLE"
    assert _eng(234) == "%< AND %>%<, %>%# OF %S"
    assert _eng(1158).startswith("YOUR CREDIT IS NOW %s ICONS.")
    assert _eng(725).startswith("WE ARE ASSESSING YOU A FINE OF %s ICONS")
    assert _eng(726).startswith("WE ARE CONFISCATING YOUR %S")
    assert _eng(630).startswith("WE ARE LOOKING AT %S IDIOT HERE! (HE HE HE)")
    assert [_eng(i) for i in (632, 633, 634, 635)] == ["AN ", "A REAL ", "A SUPREME ", "A SUPER SUPREME "]
    assert _eng(636).startswith("FURTHER IDIOCY WILL BE DEALT WITH MORE SEVERELY..")
    assert _eng(637).startswith("DON'T DO IT AGAIN!")
    assert _eng(788) == "Officer" and _eng(795) == "AND"
    assert [_eng(i) for i in (2095, 2096, 2097, 2098)] == ["ST", "ND", "RD", "TH"]
    assert _eng(2518) == "WAVE" and _eng(12518) == "WAVES" and _eng(2519) == "COUNT" and _eng(12519) == "COUNTS"
    assert _eng(186) == "NO WEAPON" and _eng(201) == "HANDCUFFS" and _eng(212) == "INNER FORCE"
    with pytest.raises(KeyError):
        _eng(213)                                               # conf = 27 prints nothing
    for sid in (789, 798, 799, 878, 884, 917, 918, 919, 248, 2514):
        assert _eng(sid)


# ------------------------------------------------------------------ THE HALL: arithmetic restated and checked with the oracle

WEIGHTS = (700, 400, 550, 550, 500, 300, 300, 450, 500, 0)


def hall_severity(diffs: dict) -> int:
    """seg16:0x6d50-0x6e79: Σ w + (w/5)(d − 1) for d > 1; the extra is a 16-bit product."""
    s = 0
    for c, d in diffs.items():
        s += WEIGHTS[c]
        if d > 1:
            s += _s16(_idiv(WEIGHTS[c], 5) * (d - 1))
    return s


def hall_fine(served: int, severity: int, credit: int, slots: int):
    """seg16:0x6ebf-0x6f94: returns (fine, confiscated)."""
    s = severity
    if served == 0 and s > 0:
        s = max(_idiv(s, 3), 100)
        if credit > 0:
            s = min(credit, s)
    if credit + 100 < s and s != 0:
        if slots > 1:
            return 0, True
        s = _idiv(s, 2)
    return s, False


def officer_number(i: int, c: int) -> int:
    """seg17:0x8e60: the number printed for a ship without a roster record (16-bit products)."""
    x = _s16((2 * i + 3) * (c + 2))
    x = _s16(x * (i + 6))
    x = _s16(x * (i + 7))
    x = _s16(2 * x)
    return _idiv(x, c + 3) + 956


def ordinal(n: int) -> str:
    """seg17:0x7ba6: n followed by ST, ND, RD or TH."""
    r = n % 10 if n >= 0 else -((-n) % 10)
    if r == 1 and n != 11:
        return f"{n}ST"
    if r == 2 and n != 12:
        return f"{n}ND"
    if r == 3 and n != 13:
        return f"{n}RD"
    return f"{n}TH"


def fixed(v: int, div: int = 100, digits: int = 2) -> str:
    """seg6:0x1010 / seg6:0xf54: quotient, '.', remainder without padding, cut at `digits` characters."""
    sign = ""
    if v < 0:
        sign, v = "-", -v
    return f"{sign}{v // div}.{str(v % div)[:digits]}"


def test_officer_numbers():
    """s4-hall.md claim 46 and facts: the officer number; the oracle printed 2171, 2771, 3530 and 4466 for the class-1
    ships 3, 4, 5 and 6 [ORACLE ships-0003] [ORACLE ships-0004]."""
    assert [officer_number(i, 1) for i in (3, 4, 5, 6)] == [2171, 2771, 3530, 4466]
    assert officer_number(1, 1) == 1376 and officer_number(4, 3) == 2972


def test_ordinals_and_fixed_point():
    """s4-hall.md claim 45 and facts: the ordinals and the fixed-point text (no zero padding)."""
    assert [ordinal(n) for n in (1, 2, 3, 4, 11, 12, 13, 21, 111)] == ["1ST", "2ND", "3RD", "4TH", "11TH", "12TH", "13TH", "21ST", "111ST"]
    assert (fixed(150), fixed(105), fixed(100), fixed(-150), fixed(0)) == ("1.50", "1.5", "1.0", "-1.50", "0.0")
    assert fixed(1205) == "12.5"


@pytest.mark.parametrize("served,diffs,credit,slots,fine,conf", [
    (0, {2: 1}, 150, 2, 150, False),            # ships-0003 k1: S 550, /3 = 183, capped at the credit
    (0, {7: 1}, 150, 2, 150, False),            # ships-0003 k5: S 450 -> 150
    (0, {2: 1}, 1000, 2, 183, False),
    (0, {0: 3}, 50, 2, 50, False),              # S 700 + 140 + 140 = 980 -> 326 -> credit 50
    (1, {1: 1}, -200, 1, 200, False),           # ships-0004: crime 1 fined 2.00 once the missiles are gone
    (1, {1: 1}, -200, 2, 0, True),              # a second weapon slot: the confiscation
    (1, {2: 1}, -720, 1, 275, False),           # crime 2: 550 / 2
    (1, {3: 1}, -250, 1, 275, False),
    (1, {4: 1}, 0, 1, 250, False),
    (1, {5: 1}, -990, 1, 150, False),
    (1, {8: 1}, 50, 1, 250, False),
    (1, {4: 2, 9: 1}, 150, 1, 300, False),      # m1: 600 / 2
    (1, {3: 1, 4: 1, 8: 3}, -150, 1, 875, False),   # m2: 550 + 500 + 700 = 1750 / 2
    (0, {0: 1, 5: 1}, 150, 2, 150, False),      # the warrant arrest of a6 g2: S 1000 / 3 capped at the credit
    (1, {9: 1}, 150, 1, 0, False),              # crime 9 weighs 0: no fine
])
def test_hall_fine(served, diffs, credit, slots, fine, conf):
    """s4-hall.md claims 31-35 and 'The punishment': the fine and the confiscation, checked against the numbers of
    [ORACLE ships-0003] and [ORACLE ships-0004]."""
    assert hall_fine(served, hall_severity(diffs), credit, slots) == (fine, conf)


# ------------------------------------------------------------------ THE COMMANDS: byte facts, strings, thresholds

CMD_BYTES = _facts("""
seg13:0x820b 05 88 13
seg13:0x85b6 05 10 27
seg13:0x8efa 05 88 13
seg13:0x918b 81 c1 88 13
seg13:0x82ae 66 81 c1 98 3a 00 00
seg13:0x8ce3 66 81 c1 10 27 00 00
seg13:0x8cf8 66 81 c1 98 3a 00 00
seg13:0x8f75 66 05 20 4e 00 00
seg13:0x959c 66 05 40 9c 00 00
seg13:0x95ab 66 05 40 1f 00 00
seg13:0x9e6d 66 81 c1 a8 61 00 00
seg13:0xa10a 66 81 c2 a8 61 00 00
seg13:0xa641 66 05 bc 02 00 00
seg13:0x5217 05 f4 01
seg13:0x8478 81 7e f8 6a ff
seg13:0x87d6 81 7e e8 96 00
seg13:0x8ac4 81 7e ec 90 00
seg13:0x8acb 3d 9e 00
seg13:0xa677 66 83 3e 58 88 0b
seg13:0x58b4 05 36 00
seg13:0x6427 6a 32
seg13:0x6429 68 96 00
seg13:0x6400 6b c0 2e
seg13:0x8105 8b b7 c4 70
seg13:0x8110 56
seg13:0x8111 ff 76 06
seg13:0x8821 83 7e f6 0e
seg13:0x884f 83 7e f6 0e
seg13:0xa647 66 a3 5c 88
seg3:0x2cc7 89 46 f4
seg3:0x2fe4 ff 36 12 92
seg4:0x64fa 8a 87 ed 70
seg4:0x654e 8a 84 ed 70
seg7:0x383d 6b f1 16
seg7:0x385e 8b 8d a0 7b
seg7:0x3872 8b 85 a2 7b
seg7:0x3922 ff b4 28 5b
""")


@pytest.mark.parametrize("addr,hexs", CMD_BYTES, ids=[f"{a}:{h[:14]}" for a, h in CMD_BYTES])
def test_command_bytes(ins, addr, hexs):
    """s4-commands.md facts, s4-commands-verify.md claims 54 and 68, s4-owed-verify.md R9, R13 and R15: the commands'
    times (5000, 10000, 15000, 20000, 25000, 40000, 8000 and 700 ms), thresholds, window sizes, the two-argument call
    of seg13:0x2688 at seg13:0x8110, the literal record 14, the deferred reply's compare with 11, the allies byte
    (+0x5d, `[bx + 0x70ed]`) of seg4:0x5e8e, the particle burst's entry table and muzzle base."""
    assert _bytes_fact(addr, hexs)


def test_dg885c_references(ins):
    """s4-owed-verify.md R13: the code references to DGROUP:0x885c are seg13:0xa647 (in seg13:0xa5c2) and three in
    seg13:0xa65a."""
    assert sorted(i.addr for i in ins.refs(0x885C, 0x885D)) == ["seg13:0xa647", "seg13:0xa664", "seg13:0xa670", "seg13:0xa6ac"]


def test_command_strings():
    """s4-commands.md facts: the replies of the commands (ENG_LANG.DLL) and the refusals 1850 ... 1874."""
    exp = {329: "You'll Have To Fend For Yourself", 331: "No One Can Help %D.", 332: "Sorry, %s. I Will Not Fight You.",
           334: "You Are Not Worth The Trouble of Destroying!", 336: "Very Well, %s, I Accept Your Challenge!",
           338: "I Love A Good Fight! Prepare To Die, Fool!", 340: "Busy Now, I'll Get You Later...",
           356: "I've Had Enough of You. Let's Fight It Out!", 358: "Look, Friend... Do You Want To Rumble?",
           385: "I Love A Good Duel!, Who Will Come Out Alive?", 398: "I Can't Capture %D Should I Attack It?",
           400: "What Use Have I Got For %D?", 401: "Can't Capture That. %D Isn't Capturable.",
           404: "I'm Too Busy Right Now. You Go Ahead And Get It.", 406: "It's Nice To Share, But No Teammates Are Around.",
           430: "You Do Not Have An Ally Willing To Leave.", 432: "I'm Staying. I've Got Some Business To Take Care Of.",
           436: "You'll Have To Get Your Own Fuel Buddy..", 438: "I'm Busy. I'll Get Gas When I Need Some.",
           448: "I Can't Find A Tank Of Gas Anywhere!", 555: "We Don't Take Orders, We Enforce Order!",
           650: "Leave Me Be! Can't You See You've Done Enough?", 657: "If I Die, I Die. But You'll Still Suffer!",
           659: "I Can't Take You Now, But Your Day Will Come.", 661: "You're Crazy To Take Me On! But That's Your Problem!",
           663: "If You Were Stronger, I Would Fight You!", 685: "Don't Worry! I'll Be Alright.",
           1408: "We Are Already In a Duel!", 1419: "I Don't Want Any More Duels. So I'll Kill You Now!!",
           1850: "Sorry, But I Can't Help. I'm Too Low On %D.", 1874: "Try That Command When There's Someone Around.",
           1948: "You're Gonna Get It When I Get %D!", 2000: "I Don't Accept. I'm Here To Help %D."}
    for sid, text in exp.items():
        assert _eng(sid) == text, sid
    assert _eng(3020).startswith("Please help me %s. I'm in trouble!")
    assert [_eng(i) for i in range(2700, 2702)] == ["THE ", "THE "]
    assert _eng(2720) == "A " and _eng(2721) == "AN " and _eng(2724) == "SOME " and _eng(2760) == "FOR"
    assert _eng(790) == "DOESN'T LIKE" and _eng(791) == "IS NEUTRAL TO" and _eng(792) == "LIKES" and _eng(793) == "IS ON"
    assert _eng(794) == "HE %S YOUR TEAM %? HE %S YOU.%N%N" and _eng(921) == "THIS IS YOUR FIRST ENCOUNTER WITH %D."
    assert _eng(922) == " TWICE" and _eng(923) == " ONCE BEFORE" and _eng(2041) == "REALITY CHECK" and _eng(2042) == "TEAMMATE"
    assert _eng(2046) == "YOU INTERACTED WITH HIM IN THE PAST" and _eng(2048) == "BUT" and _eng(2531) == "%D %D"
    assert _eng(3034) == "YOU HAVE MET %D %<%?%>%<%d TIMES%>."
    for sid in (18, 21, 24):
        with pytest.raises((KeyError, AssertionError)):
            _eng(sid)                                           # the strings below id 110 are absent (string 21)


def test_watch_window_literals(dgroup):
    """s4-commands.md facts: the window class names, title and separators."""
    lits = {0x1376: b"\0", 0x1377: b"\0", 0x1378: b" - \0", 0x137C: b" - \0", 0x1380: b"Ship - Pilot\0", 0x138D: b"IS_WATCH\0",
            0x1396: b"\0", 0x1397: b"IS_SMART\0", 0x13C4: b" (\0", 0x13CB: b"!\0", 0x13CD: b"\0", 0x13CE: b".\0",
            0x13D0: b".\0", 0x13D2: b" \0", 0x13D4: b" \0", 0xB80: b"1/2x Scale\0"}
    for off, text in lits.items():
        assert dgroup[off:off + len(text)] == text, hex(off)


def test_key_decode_chain(ins):
    """s4-commands.md facts: the letter chain at seg11:0x6e42 reaches A 0x7065, B 0x71d4, C 0x7019, D 0x6f20, E 0x70a5,
    F 0x6feb, G 0x6fff, H 0x6f7a, I 0x716c, M 0x71a9, R 0x718b, S 0x7033, T 0x7132, U 0x7183, V 0x7187, W 0x7127, X 0x6f3a,
    Y 0x717f, Z and Enter 0x6f58, TAB 0x71e3 through its `sub al` / `dec al` steps."""
    base, got, i = 0, {}, ins.at("seg11:0x6e42")
    for _ in range(140):
        if i.mnem == "sub" and i.ops[0] == "al":
            base += int(i.ops[1], 0)
        elif i.mnem == "dec" and i.ops[0] == "al":
            base += 1
        elif i.mnem in ("je", "jbe") and i.ops[0].startswith("seg11"):
            got.setdefault(base, i.ops[0])
        if i.off >= 0x6EE4:
            break
        i = ins.step(i, 1)
    letters = {ord("A"): 0x7065, ord("C"): 0x7019, ord("D"): 0x6F20, ord("E"): 0x70A5, ord("F"): 0x6FEB, ord("G"): 0x6FFF,
               ord("H"): 0x6F7A, ord("S"): 0x7033, ord("W"): 0x7127, ord("X"): 0x6F3A}
    for vk, target in letters.items():
        assert got.get(vk) == f"seg11:{target:#x}", (chr(vk), got.get(vk))


# ------------------------------------------------------------------ MESSAGE CASES: the string pushes

COMMAND_PUSHES = [(0xC2B, 0x149), (0xC23, 0x14B), (0xC37, 0x14C), (0xC42, 0x14E), (0xC5E, 0x14E), (0xC6E, 0x154),
                  (0xC79, 0x150), (0xC80, 0x152), (0xCA0, 0x166), (0xC90, 0x164), (0xE95, 0x18E), (0xEAD, 0x190),
                  (0xEC5, 0x191), (0xEDD, 0x194), (0xEE5, 0x196), (0xF23, 0x1B0), (0xF1B, 0x1AE), (0xF43, 0x1B6),
                  (0xF3B, 0x1B4), (0xF4B, 0x1C0), (0xF5B, 0x22B), (0xF2B, 0x28A), (0x100B, 0x291), (0x1013, 0x293),
                  (0x101B, 0x295), (0x1023, 0x297), (0x1072, 0x2AD), (0xC04, 0x148), (0x11CA, 0x580), (0x1237, 0x58B),
                  (0x1289, 0x73A), (0x127B, 0x73C), (0x12F5, 0x79C), (0x1305, 0x7D0), (0xA93, 0xBCC), (0xA4C, 0xBCB),
                  (0xA5C, 0x12E), (0xA80, 0x12F), (0xAA3, 0x184), (0xAAA, 0x1AC), (0xAB2, 0x1BA)]


@pytest.mark.parametrize("off,sid", COMMAND_PUSHES, ids=[f"seg3:{o:#x}:{s}" for o, s in COMMAND_PUSHES])
def test_command_reply_pushes(off, sid):
    """s4-commands.md facts: the `push <string id>` of each reply case of seg3:0x6ec sits at the address given."""
    seg = unpacked_seg("inspace", 3)
    assert seg[off:off + 3] == bytes([0x68, sid & 0xFF, sid >> 8])


LAW_PUSHES = [(0x131B, 0x1AA, 24), (0xE7B, 0x798, 8), (0x1147, 0x2BB, 24), (0x1137, 0x2BF, 24), (0xCFD, 0x15E, 48)]


@pytest.mark.parametrize("off,sid,span", LAW_PUSHES, ids=[f"seg3:{o:#x}:{s}" for o, s, _ in LAW_PUSHES])
def test_law_reply_pushes(off, sid, span):
    """s4-crime.md facts: case 0xb7 pushes string 426, 0xb5 1944, 0x69 699, 0x6a 703 and 0x0d 350 within its case."""
    seg = unpacked_seg("inspace", 3)
    assert bytes([0x68, sid & 0xFF, sid >> 8]) in seg[off:off + span]
