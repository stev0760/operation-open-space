"""Mechanical facts of the M5 stage 4 part-B readings that the reviewer confirmed (work/world/m5s4/review.md,
"Readings no verifier has checked", and the fixer's docs/re edits): the tether's holding cost, the witness
distance's bound, the arrest record's order, the gate's DGROUP:0x9324 compare, the Hall's seg15:0x28b2,
seg15:0x8604 and the launch step 5, the placement's DGROUP:0x9e5b store before seg14:0x0(-1) and seg16:0x3784's
single exit, seg8:0x9028 and the defenders' mover, seg13:0x8570's busy jumps, seg13:0x4ac4's dropper stores,
seg13:0x915c's grudge gate and duel words, the Strategy keys' mark, seg7:0x65aa's ship branch and the race winner
seg4:0x8d3a.

The byte facts are data-driven: ``"segN:0xOFF[-0xOFF]  hex bytes"``; a single address needs the bytes there, a range
needs them somewhere in it. Tests skip when ``work/`` or ``original/`` is absent.
"""
from __future__ import annotations

import struct

import pytest

from test_re_facts import (  # noqa: F401  (ins and dgroup are pytest fixtures)
    dgroup, ins, unpacked_seg,
)
from test_re_facts_m4s7 import _bytes_fact, _facts

FACTS = _facts("""
seg8:0x1f7e 83 7e ec 0e
seg8:0x1f92 26 8b 40 44
seg8:0x1f99 b9 38 ff
seg8:0x1f9f 05 08 00
seg8:0x1fa2 3d 03 00
seg8:0x1fb0 66 a1 68 1e
seg8:0x1fb9 66 f7 f1
seg8:0x1fc9 83 af ae 70 03
seg8:0x1fce 83 bf ae 70 00
seg8:0x1fd5 c7 46 ee e8 03
seg8:0x1fda 6b 46 d2 64
seg8:0x1fde 05 5e 01
seg8:0x1fe1 3b 46 ee
seg8:0x1fe4 7c 37
seg13:0xb8b0 3d 60 09
seg13:0xb8b3 7c 03
seg13:0xaae3 9a ec 06
seg13:0xaafe a0 63 a1
seg13:0xab01 a2 61 a1
seg13:0xab07 a2 62 a1
seg13:0xa9aa c4 bd f8 1d
seg12:0x2478 a1 24 93
seg12:0x2498 26 39 47 32
seg12:0x249e 26 69 5f 34 5a 01
seg12:0x24aa 83 bf 0a 97 07
seg12:0x24b1 c7 06 a4 93 01 00
seg12:0x2447 83 c4 04
seg12:0x2458 26 39 40 38
seg15:0x28c5 2d 03 00
seg15:0x28ce 2d 08 00
seg15:0x28e3 b9 04 00
seg15:0x28f1 ff 36 a8 5e
seg15:0x291b 68 a0 00 68 c9 00
seg15:0x2935 68 a0 00 68 c9 00
seg15:0x2959 81 7e fc fa 00
seg15:0x2969 f7 7e f4
seg15:0x296c 83 c2 03
seg15:0x2978 f7 7e f6
seg15:0x2983 b9 03 00
seg15:0x2989 0b d2
seg15:0x298b 75 03
seg15:0x2995 b9 04 00
seg15:0x29a3 51
seg15:0x29b5 6a 04
seg15:0x8611 a1 ae 3c 2b 06 aa 3c 2d 0f 00 a3 d2 92
seg15:0x8634 68 30 6e
seg15:0x865d 68 84 60
seg15:0x863a 05 19 00
seg15:0x8641 05 19 00
seg15:0x8648 2d 19 00
seg15:0x8653 2d 19 00
seg15:0x1193 83 3e f4 90 00
seg15:0x1198 75 64
seg15:0x11df 05 20 03
seg15:0x11ff e8 e4 f7
seg15:0x1202 6a 02
seg15:0x120e 05 b6 03
seg15:0x1214 a3 c4 3c
seg9:0x6867 a3 5b 9e
seg9:0x6888 6a ff
seg16:0x3b07 66 c7 06 5b 9e 01 00 00 00
seg16:0x3b10 5e 8d 66 fe 1f 5d 4d cb
seg8:0x8277-0x82c0 c7 06 52 94 01 00
seg8:0x8f52 26 83 78 18 00
seg8:0x8f57 0f 85 63 ff
seg8:0x8f72 39 84 ec 92
seg8:0x8f76 7d 0d
seg8:0x8fd6 0f 83 74 f4
seg13:0x8a54 eb 66
seg13:0x8a6e eb 4c
seg13:0x8a95 eb 25
seg13:0x8a9c eb 1e
seg13:0x8a9e 81 7e ee e8 03
seg13:0x8aa3 74 17
seg13:0x8af2 c7 46 ee e8 03
seg13:0x4af1 69 5e 06 5a 01
seg13:0x4af6 c7 87 c8 70 02 00
seg13:0x4afc 66 c7 87 c2 70 0b 00 00 00
seg13:0x91f9 83 3e 34 91 00
seg13:0x9200 83 3e bc 95 00
seg13:0x9205 75 28
seg13:0x9226 80 b8 52 71 04
seg13:0x9708 a3 04 8e
seg13:0x970b a3 00 8e
seg13:0xa1a5 a1 54 6e
seg13:0xa1ac a1 52 6e
seg13:0xa1f8 ff 36 52 6e
seg13:0xa1fc ff 36 54 6e
seg7:0x65cb a1 24 93
seg7:0x65d3 a1 46 96
seg7:0x65db ff 76 08 6a 01 6a 00
seg4:0x8d47 66 c7 46 fa 40 42 0f 00
seg4:0x8d75 26 80 7f 51 02
seg4:0x8d99 89 46 f6
seg4:0x8db0 8b 46 f6
seg11:0x6f3e 0f 84 5f 03
seg11:0x6f46 74 28
seg11:0x6f70 e9 2e 03
""")


@pytest.mark.parametrize("addr,hexs", FACTS, ids=[f"{a}:{h[:14]}" for a, h in FACTS])
def test_part_b_bytes(ins, addr, hexs):
    """The byte facts of the readings landed in law.md, pilots.md, shell.md and core.md: the tether cost
    (pool-14 holder, mass / -200 + 8 with a floor of 3, the frame dword modulo it, 3 fuel, 1000 at fuel <= 0, the
    100·[bp-0x2e] + 350 break), the witness compare with 0x960, B1's record after seg3:0x6ec, the gate's compare of
    +0x32 with DGROUP:0x9324, seg15:0x28b2's a, b, 250 iterations, the rops and the divisors, seg15:0x8604's
    limits, step 5's state-0 branch (800 ms) and its 950 ms, the placement's store, seg16:0x3784's exit, the
    defenders' mover, seg13:0x8570's busy jumps and the cloak test's 1000 compare, the dropper's stores, the
    grudge gate, the duel words' low halves, the mark of seg13:0xa186, seg7:0x65aa's ship branch, seg4:0x8d3a
    and the X key's two jumps to seg11:0x72a1."""
    assert _bytes_fact(addr, hexs)


def test_witness_distance_bound(dgroup):
    """law.md "The witness distance never fails in a wave": DGROUP:0x5a0 = 3200 cells, seg8:0x285a wraps each axis
    at half of it, and its result ((a² + b²)/e + e)/2 with e = max + min/2 is at most 2266 < 0x960."""
    assert struct.unpack_from("<H", dgroup, 0x5A0)[0] == 3200
    half = 3200 // 2

    def d(a: int, b: int) -> int:
        e = max(a, b) + min(a, b) // 2
        return ((a * a + b * b) // e + e) // 2 if e else 0

    best = max(d(a, b) for a in range(0, half + 1, 8) for b in range(0, a + 1, 8))
    assert best == d(half, half) == 2266
    assert best < 0x960


def test_dead_ship_marks_draws():
    """law.md "The ship in the Hall": seg15:0x28b2 draws four rand() per iteration over 250 iterations, 1000 in all."""
    assert 250 * 4 == 1000
    seg = unpacked_seg("inspace", 15)
    calls = [o for o in range(0x28B2, 0x2A00) if seg[o:o + 3] == bytes.fromhex("9a dc 00")]
    # seg5:0xdc (rand) is reached at seg15:0x28de, 0x2963, 0x2972, 0x297e and 0x2990; the PatBlt branch takes its
    # size draw at seg15:0x28de and the Ellipse branch at seg15:0x2990, one each per iteration
    assert calls == [0x28DE, 0x2963, 0x2972, 0x297E, 0x2990]


def test_seg16_3784_single_exit(ins):
    """law.md "The ship in the Hall": seg16:0x3784 has one return, after the dword store at seg16:0x3b07."""
    i, rets = ins.at("seg16:0x3784"), []
    while i.off <= 0x3B17:
        if i.mnem in ("retf", "ret"):
            rets.append(i.addr)
        i = ins.step(i, 1)
    assert rets == ["seg16:0x3b17"]


def test_placement_order(ins):
    """law.md "The ship in the Hall": the placement stores DGROUP:0x9e5b = 0 at seg9:0x6867 before it calls seg14:0x0
    with -1 at seg9:0x688a."""
    assert ins.at("seg9:0x6867").text.startswith("mov word ptr [0x9e5b]")
    assert ins.at("seg9:0x688a").text.startswith("call far seg14:0x0")
    assert ins.at("seg9:0x6888").text == "push -1"
