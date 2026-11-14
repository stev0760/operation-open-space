"""Mechanical facts of the M5 stage 3 analyses (work/analysis/m5/s3-{roster,tick,contact}.md with the verifiers'
corrections): the wave's computer ships (seg14:0x0 and its pickers, the call-in seg13:0xae68, the entry buffer
seg12:0x2556, perspective shifting seg11:0xaece and the pan seg8:0x0), the tick's ship calls and the pilot
(seg4:0x78, seg4:0xbde, seg4:0x7e2a, seg4:0x83ae), the crime routine seg13:0xb6f2 and the arrest executor
seg13:0xa6bc, and the contact, death and defender code (seg16:0x85e, seg8:0x45e0's divisors, seg4:0x7636, seg3:0x6ec's
messages).

The listing model, the fixtures and the helpers come from test_re_facts.py and test_re_facts_m4s7.py. The byte
facts are data-driven: ``"segN:0xOFF[-0xOFF]  hex bytes"``; a single address needs the bytes there, a range needs
them somewhere in it. Tests skip when ``work/`` or ``original/`` is absent.
"""
from __future__ import annotations

import struct

import pytest

from test_re_facts import (  # noqa: F401  (ins, isp, dgroup and master are pytest fixtures)
    A, _eng, dgroup, ins, isp, master, unpacked_seg,
)
from test_re_facts_m4s7 import _bytes_fact, _facts


def _shp_name(shp: bytes, k: int) -> str:
    return shp[k * 65 + 10:k * 65 + 31].split(b"\0")[0].decode()


# ------------------------------------------------------------------ ROSTER: byte facts

ROSTER_BYTES = _facts("""
seg14:0x3e9 a3 0a 97
seg14:0x47e 89 87 0a 97
seg14:0x14ca 89 87 0a 97
seg9:0xdbf 89 87 0a 97
seg13:0xaed2 66 c7 46 f2 07 00 16 00
seg13:0xaee8 66 c7 46 f2 07 00 17 00
seg13:0xaf0c 66 c7 46 f2 04 00 01 00
seg13:0xb000 66 c7 46 f2 03 00 04 00
seg13:0xb04d 66 c7 46 f2 07 00 14 00
seg13:0xaecd c7 46 f6 01 00
seg13:0xaee3 c7 46 f6 00 00
seg13:0xaf07 c7 46 f6 ff ff
seg13:0xaffb c7 46 f6 ff ff
seg13:0xb048 c7 46 f6 02 00
seg14:0x2561 c7 46 ee 64 00
seg14:0x2569 c7 46 ee 14 00
seg14:0x2580 c7 46 ee 32 00
seg14:0x2325 c7 46 ec 18 fc
seg14:0x329f c7 06 b2 95 f0 d8
seg14:0x8ce c7 06 b2 95 48 f4
seg8:0xc8 3d 10 00
seg8:0x186 6b 46 fc 0c
seg8:0x196 6b 46 fa 0c
seg8:0x18e c1 e0 03
seg11:0x6dcd c7 06 32 10 01 00
seg12:0x268f 68 cc 00 6a 20
seg14:0x4115 b9 24 00
seg14:0x412b 3c 16
seg14:0x4134 b0 16
seg14:0x41f8 83 7e fc 0b
""")


@pytest.mark.parametrize("addr,hexs", ROSTER_BYTES, ids=[f"{a}:{h[:14]}" for a, h in ROSTER_BYTES])
def test_roster_bytes(ins, addr, hexs):
    """s3-roster.md Facts for tests: the design team stores, the call-in requests, the picker scores, the pan and the latch."""
    assert _bytes_fact(addr, hexs)


def test_roster_dgroup_data(dgroup):
    """s3-roster.md Facts for tests: the weapon values, the name glue and the MainShip strings."""
    assert dgroup[0x1332:0x1332 + 27] == bytes.fromhex("00 08 0a 08 04 05 0a 06 07 06 04 03 04 06 09 07 03 06 05 06 05 06 08 08 07 05 09")
    assert dgroup[0x9EC:0x9EE] == b" \0" and dgroup[0x48B:0x48D] == b"-\0"
    for off, text in ((0x11AE, b"Ship1\0"), (0x11B5, b"Ship2\0"), (0x11BB, b"EAGLE\0"), (0x11C1, b"MainShip\0"),
                      (0x11CA, b"TRANSFORMER\0")):
        assert dgroup[off:off + len(text)] == text, hex(off)


def test_team7_ships(master):
    """s3-roster.md 'Teams': the six team-7 designs and their class bytes (SHP +0x05)."""
    shp = master[1]
    team7 = [k for k in range(118) if shp[k * 65 + 3] == 7]
    assert team7 == [22, 35, 42, 47, 77, 85]
    assert [(_shp_name(shp, k), shp[k * 65 + 5]) for k in team7] == [
        ("INTERCEPTOR", 1), ("CONSTABLE", 3), ("LETHAL WEAPON", 1), ("PATROLMAN", 2), ("TERMINATOR", 0), ("ELIMINATOR", 0)]


def test_picker_kind2_choice(master):
    """s3-roster.md 1.5: the shipped PATROLMAN (+0x05 = 2) and CONSTABLE (+0x05 = 3) are the Patrol picks of kind 2."""
    shp = master[1]
    assert [k for k in range(118) if shp[k * 65 + 3] == 7 and shp[k * 65 + 5] > 1] == [35, 47]


# ------------------------------------------------------------------ TICK: byte facts (s3-tick.md Facts for tests)

TICK_BYTES = _facts("""
seg13:0xb761 11 b7 18 b7 1f b7 26 b7 2d b7 3e b7 73 b7 45 b7 4c b7
seg13:0xb711 c7 46 f4 45 00
seg13:0xb718 c7 46 f4 46 00
seg13:0xb71f c7 46 f4 5d 00
seg13:0xb726 c7 46 f4 65 00
seg13:0xb737 c7 46 f4 72 00
seg13:0xb73e c7 46 f4 86 00
seg13:0xb745 c7 46 f4 b9 00
seg13:0xb74c c7 46 f4 c0 00
seg13:0xb8b0 3d 60 09
seg13:0xb912 81 3e 48 96 e8 03
seg13:0xb92c c7 06 48 96 e9 03
seg13:0xaacb 3d fa 00
seg13:0xaad9 6a 3d
seg13:0xab0c 6a 57
seg13:0xabc8 80 b8 48 71 0f
seg13:0xabd5 9a 4c 58
seg13:0xabe1 66 c7 87 96 70 0c 00 00 00
seg13:0xa890 66 81 c1 c8 32 00 00
seg13:0xa6d2 05 ee 02
seg3:0x13de 9d 0f
seg3:0x1430 6f 11
seg3:0x14b0 cd 12
seg4:0x8322 b9 2c 01
seg3:0xf9d 6a 02 68 3a 02
seg3:0xfad 6a 02 68 3c 02
seg3:0xfb5 6a 02 68 c0 03
seg3:0xfbd 6a 02 68 c1 03
seg3:0xfc5 6a 02 68 c2 03
seg3:0xfcd 6a 02 68 c3 03
seg3:0xfa5 6a 02 68 c4 03
seg3:0x116f 6a 30
seg3:0x12cd 6a 02 68 94 07
seg3:0xf84 8b 46 0c 05 eb 01
seg4:0x7fcc b9 d0 02
seg4:0x8224 b9 2c 01
seg4:0x8031 66 05 58 1b 00 00
seg4:0x827c 66 05 58 1b 00 00
seg4:0x7eb2 81 bf 54 38 96 00
seg4:0x8050 83 bf 64 38 ce
seg4:0x82fc 8b 87 94 38
seg4:0x845e 3d 5e 01
seg4:0x8468 b9 a0 0f
seg4:0x8476 81 ea d0 07
seg13:0x6784 66 05 10 27 00 00
seg13:0x67cc 05 88 13
seg11:0x5ce9 c7 87 6a 71 78 00
seg11:0x5cf6 c7 87 6a 71 5a 00
seg11:0x6369 3d 90 01
seg4:0xab6 66 05 38 4a 00 00
seg4:0xa83 81 7e ee 90 01
seg4:0xbba 81 7e ee bc 02
seg4:0xbb2 81 bf ae 70 96 00
seg4:0x59b7 ca 06 00
seg4:0xbda ca 02 00
seg11:0x63b0 ca 04 00
seg13:0xadba ca 02 00
seg13:0xb96c 83 7e fa 0a
seg13:0xb950 83 7e fa 09
""")


@pytest.mark.parametrize("addr,hexs", TICK_BYTES, ids=[f"{a}:{h[:14]}" for a, h in TICK_BYTES])
def test_tick_bytes(ins, addr, hexs):
    """s3-tick.md Facts for tests F1-F23 and F27: the crime switch, the witness range, the call-in requests, the cuff
    range, the message pushes, the call-in chances and delays, the guess of seg4:0x83ae, the speed caps and the Pascal widths."""
    assert _bytes_fact(addr, hexs)


def test_tick_strings():
    """s3-tick.md F25 and F26: the crime names (STRING 69, ids 1098-1107) and the Enforcer lines (STRING 35, ids 550-558)."""
    assert [_eng(i) for i in range(1098, 1108)] == [
        " Murdering An Enforcer.", " Destroying An Icon", " Removing Evidence", " Attacking The Defenseless",
        " Taking An Enforcer's Doughnut", " Firing Upon An Officer", " Destroying Doughnut Dispensors",
        " Resisting Arrest.", " Unsportsmanlike Race Conduct", " Being An Idiot."]
    assert _eng(550) == "Do Not Provoke Me! Or There Will Be Trouble."
    assert _eng(551) == "The Interceptor Will Make You Abide By The Law!!!"
    assert _eng(552) == "Follow Me Out. And Don't Try Anything!"
    assert _eng(553) == "Alright Tough Guy, I'll Resort To Force."
    assert _eng(554) == "I Must Pursue Someone Else. Try To Be A Better Citizen."
    assert _eng(558) == "You Know, A Guy Could Get Arrested For That!"
    assert _eng(570) == "Stop That! You Just Broke a Good Icon!"
    assert _eng(572) == "That Was Evidence! It's Mine To Take."
    assert _eng(578) == "You Will Pay For That Next Wave, Tough Guy!"
    assert _eng(701) == "You're Loaded With Stuff. Watch Out for Thieves!"
    assert _eng(705) == "Hang In There, You Could Get Help Anytime Now."
    assert _eng(707) == "We Do Have Laws Here. Do I Have To Send The Patrol?"
    assert _eng(730) == "Abide By The Law, The Patrolman Is Entering."
    assert _eng(1940) == "Hey! I Have An Outstanding Warrant For Your Arrest!"


# ------------------------------------------------------------------ CONTACT: byte facts (s3-contact.md Facts for tests)

CONTACT_BYTES = _facts("""
seg16:0x85e 8c d0 90 45 55 8b ec 1e 8e d8 33 c0 8d 66 fe 1f 5d 4d cb
seg12:0xe36 26 ff 5f 66
seg12:0x6ba2 26 ff 58 66
seg7:0x26b1 26 ff 5c 66
seg13:0x4a1d 26 ff 58 66
seg3:0x10ad 68 22 04
seg3:0x10b5 68 1a 04
seg3:0x10bd 68 91 05
seg3:0x10d2 68 1c 04
seg3:0x10f5 68 1e 04
seg3:0x124f 68 92 05
seg3:0xdb3 68 75 01
seg3:0xdbb 68 77 01
seg3:0xdcb 68 7d 01
seg3:0xddb 68 7b 01
seg3:0xde3 68 7f 01
seg3:0xdeb 68 79 01
seg3:0xf9f 68 3a 02
seg3:0xfaf 68 3c 02
seg3:0xfb7 68 c0 03
seg3:0xfbf 68 c1 03
seg3:0xfc7 68 c2 03
seg3:0xfcf 68 c3 03
seg3:0xfa7 68 c4 03
seg3:0x1179 68 c3 02
seg3:0xda9 6a 3f
seg3:0xdd1 6a 3f
seg3:0x1245 6a 44
seg14:0x1cb2 b3 04
seg14:0x1cc0 b1 05
seg14:0x1cb8 89 84 dc 96
seg14:0x1cc6 89 84 de 96
seg14:0x1cd2 89 84 e0 96
seg14:0x1cde 89 84 da 96
seg14:0x1cea 89 84 e4 96
seg14:0x1cf6 89 84 e6 96
seg14:0x1d02 89 84 e8 96
seg14:0x1d0e 89 84 e2 96
seg14:0x1d16 88 84 78 97
seg8:0x47d1 8b 46 08
seg8:0x47da 6a 01
seg8:0x47dc ff 76 0a
seg8:0x47df ff b7 14 61
seg8:0x47e3 50
seg4:0x764d 66 05 88 13 00 00
seg4:0x767a 83 bc 0a 97 07
seg4:0x76a3 80 b8 87 71 03
seg4:0x76b6 66 c7 46 f4 07 00 ff ff
seg4:0x76e8 05 05 00
seg4:0x76eb 3d 0c 00
seg4:0x76f0 05 64 00
seg4:0x77a9 80 b8 7e 71 05
seg4:0x77b0 83 bc c8 70 04
seg4:0x77ca 83 bc 0a 97 03
seg4:0x77db 83 3e 20 93 07
seg4:0x779b 83 7e fa 09
seg4:0x785b 83 7e fa 03
seg4:0x78db 68 89 00
seg4:0x790f 6a 63
seg4:0x791b 6a 5f
seg4:0x7957 6a 60
seg4:0x7973 6a 61
seg4:0x7992 6a 62
seg4:0x5e81 b8 64 00
seg4:0x5e10 66 05 32 00 00 00
seg4:0x5e3f 66 05 32 00 00 00
seg4:0x5e5f 66 c7 46 e6 0a 00 00 00
seg4:0x5e6f 66 b9 64 00 00 00
seg8:0x2ddc 3d 5f 00
seg8:0x2df5 3d 5f 00
seg8:0x2dcb 6a 1e
seg8:0x2de1 6a 1c
seg8:0x2de5 6a 19
seg8:0x2dfa 6a 1d
seg8:0x2dfe 6a 1a
seg8:0x2e57 6a 1b
seg4:0x7d73 83 7e 0a 1e
seg4:0x7d8e 3d 06 00
seg4:0x7da9 3d fa ff
seg4:0x7dbb ff 06 80 96
seg4:0x7dd3 ff 06 82 96
seg4:0x7def c7 46 fa 03 00
seg8:0x7d1c 6a 0a
seg8:0x7d41 83 f9 14
seg8:0x7d46 83 46 fa 03
seg8:0x7d53 83 7e fa 06
seg8:0x7d4d 83 7e fc 08
seg13:0x26cb 03 94 24 38
seg13:0x26be 8b 94 f2 70
seg13:0x26e0 6b c0 14
seg13:0x26f7 6b c0 14
seg13:0x2704 b8 d4 fe
seg8:0x86bd 68 b4 00
seg8:0x8e87 68 b4 00
seg8:0x8e67 6a 37
seg8:0x8619 6a 01
seg8:0x861b 6a 0c
seg8:0x861d 6a 0f
seg8:0x9092 83 7e 08 05
seg8:0x90e5 83 7e 08 0f
seg8:0x90eb c7 87 f6 52 00 00
seg13:0x4ad2 6a 00
seg16:0xb37 6a 01
seg13:0x4af6 c7 87 c8 70 02 00
seg13:0x4afc 66 c7 87 c2 70 0b 00 00 00
seg7:0x13a2 c7 06 8a 05 00 00
seg7:0x6025 c7 06 8a 05 01 00
seg12:0x4f3e c7 06 8a 05 01 00
seg7:0xd84 83 3e 88 05 00
""")


@pytest.mark.parametrize("addr,hexs", CONTACT_BYTES, ids=[f"{a}:{h[:14]}" for a, h in CONTACT_BYTES])
def test_contact_bytes(ins, addr, hexs):
    """s3-contact.md Facts for tests: the ambulance's empty on_damage, the +0x66 calls, the message pushes, the shot
    divisor stores, the death routine, the strength test, the assist score, the relation read and the defender literals."""
    assert _bytes_fact(addr, hexs)


def test_message_jump_table():
    """s3-contact.md Facts for tests: the word at seg3:0x1356 + 2*(code - 1) for the messages the ships send."""
    seg = unpacked_seg("inspace", 3)
    exp = {0x19: 0x0DA9, 0x1A: 0x0DB9, 0x1B: 0x0DE1, 0x1C: 0x0DE9, 0x1D: 0x0DD1, 0x1E: 0x0DC1, 0x44: 0x0F61,
           0x45: 0x0F9D, 0x46: 0x0FAD, 0x5D: 0x0FB5, 0x5F: 0x10B3, 0x60: 0x10C3, 0x61: 0x10D8, 0x62: 0x10E7,
           0x63: 0x10AB, 0x65: 0x0FBD, 0x6E: 0x116F, 0x72: 0x14D6, 0x86: 0x0FC5, 0x89: 0x10BB, 0x8A: 0x1245,
           0xB9: 0x0FCD, 0xC0: 0x0FA5}
    for code, target in exp.items():
        assert struct.unpack_from("<H", seg, 0x1356 + 2 * (code - 1))[0] == target, hex(code)


def test_crime_switch_table():
    """s3-contact.md and s3-tick.md F1: seg13:0xb6f2's switch table at seg13:0xb761 and its bound 8 at seg13:0xb754."""
    seg = unpacked_seg("inspace", 13)
    assert struct.unpack_from("<9H", seg, 0xB761) == (0xB711, 0xB718, 0xB71F, 0xB726, 0xB72D, 0xB73E, 0xB773, 0xB745, 0xB74C)
    assert seg[0xB754:0xB757] == bytes.fromhex("3d0800")


def test_contact_strings():
    """s3-contact.md Facts for tests: the wing taunts, the death lines and the fleeing texts."""
    assert _eng(373) == "Oohh, That Smarts. You Owe Me A Wing!!"
    assert _eng(375) == "There Goes The Other One. Must Be Wing Night Tonight."
    assert _eng(377) == "You Ripped Off My Wing And I'm Still Tougher!!"
    assert _eng(379) == "I May Have No Wings, But You're Still Weaker!"
    assert _eng(381) == "I Still Have More Wings, You Lousy %D"
    assert _eng(383) == "Would You Look At That.. Now Our Wings Match!"
    assert _eng(960) == "You're Charged With Using Excessive Force."
    assert _eng(961) == "Rule #1 ...Donut Mess With My Doughnuts!!"
    assert _eng(962) == "Donut Deploying Devices Are Not To Be Disturbed!"
    assert _eng(963) == "The Prize Is For The Winner You Loser!"
    assert _eng(964) == "You're Guilty Of Reckless Thrusting!!"
    assert _eng(1050) == "This Ship Is A Write Off. I Hope My Allies Get You!"
    assert _eng(1052) == "Flame Out..Arrrgghh! Get %D!"
    assert _eng(1054) == "I'm Outta Here, For Good. I Was Killed By %D!"
    assert _eng(1058) == "I Can't Believe You Did That. I'm Your Ally!"
    assert _eng(1425) == "Silly Me. I Killed Myself."
    assert _eng(1426) == "That's Enough, You Win. I'm Breaking Off Attack."
    assert (_eng(482), _eng(484)) == ("Running Alright?", "Fleeing From %D")


def test_defender_picture_name(dgroup):
    """s3-contact.md 6.2: seg10:0x464a pushes DGROUP:0x769, the bitmap name DEF."""
    assert dgroup[0x769:0x76D] == b"DEF\0"


@pytest.mark.xfail(strict=True, reason="s3-tick.md F15 gives seg4:0x8325 for the block-3 chance; the `mov cx, 300` is at "
                                       "seg4:0x8322 (b9 2c 01), pinned in test_tick_bytes")
def test_tick_f15_block3_address(ins):
    assert _bytes_fact("seg4:0x8325", "b9 2c 01")


def test_player_index_writers(ins):
    """s3-roster-verify.md claim 61: the stores to the player index DGROUP:0x9646 are the shift's four (seg11:0xaf23,
    seg11:0xaf29, seg11:0xafa6, seg11:0xaff5) and the ship set-up's three (seg14:0x243, seg14:0x42c, seg14:0x447)."""
    writes = sorted((i.addr for i in ins.refs(0x9646, 0x9647)
                     if i.seg != 19 and (str(i.text).startswith("mov word ptr [0x9646]") or str(i.text).startswith("inc word ptr [0x9646]"))),
                    key=A)
    assert writes == ["seg11:0xaf23", "seg11:0xaf29", "seg11:0xafa6", "seg11:0xaff5", "seg14:0x243", "seg14:0x42c", "seg14:0x447"]


def test_shift_latch_references(ins):
    """s3-roster.md 4.1: DGROUP:0x1032 has three code references, the pass's test (seg7:0x7d5), the key's store
    (seg11:0x6dcd) and the shift's clear (seg11:0xaef1)."""
    assert sorted((i.addr for i in ins.refs(0x1032, 0x1033) if i.seg != 19), key=A) == ["seg7:0x7d5", "seg11:0x6dcd", "seg11:0xaef1"]


def test_warrant_references(ins):
    """s3-roster.md 1.3 and s3-tick.md 3.7: DGROUP:0x9346 (the arrest warrant) has 15 code references."""
    assert len([i for i in ins.refs(0x9346, 0x9347) if i.seg != 19]) == 15
