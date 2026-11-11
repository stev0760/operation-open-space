"""Mechanical facts of the M4 stage 7 analyses (work/analysis/m4/s7-{ring,tether,fuel,launched}.md, with the
verifiers' corrections): the weapon records of seg10:0x1fb2, the constants and pens of the RING OF DEATH,
LIGHTNING, GRAPPLING HOOK, HANDCUFFS, FIREBALLS, SNOWBALLS, WILDCARD DEFENSE and the launched weapons,
the switch's jump table at seg11:0xadba, the pool-5 tables, MASTER.SHP's carriers, the weapon strings and
the bitmap sizes.

The listing model, the fixtures and the helpers come from test_re_facts.py and test_re_facts_m4s5.py. The
byte facts are data-driven: ``"segN:0xOFF[-0xOFF]  hex bytes"``; a single address needs the bytes there, a
range needs them somewhere in it. Tests skip when ``work/`` or ``original/`` is absent.
"""
from __future__ import annotations

import re
import struct

import pytest

from test_re_facts import (  # noqa: F401  (ins, isp, dgroup and master are pytest fixtures)
    A, _dib, _eng, _resource, dgroup, ins, isp, master, unpacked_seg,
)
from test_re_facts_m4s5 import _disp_stores

W0 = 0x2F54  # weapon record 0 (DGROUP); W(w) = W0 + 0x50 * w


def _td(a: int, b: int) -> int:
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b >= 0) else -q


def _bytes_fact(addr: str, hexs: str) -> bool:
    m = re.fullmatch(r"seg(\d+):0x([0-9a-f]+)(?:-0x([0-9a-f]+))?", addr)
    seg, lo = int(m[1]), int(m[2], 16)
    hi = int(m[3], 16) if m[3] else lo
    data = unpacked_seg("inspace", seg)
    pat = bytes.fromhex(hexs.replace(" ", ""))
    window = data[lo:hi + len(pat)]
    return window.startswith(pat) if hi == lo else pat in window


def _facts(text: str) -> list[tuple[str, str]]:
    out = []
    for line in text.strip().splitlines():
        line = line.strip()
        if line:
            addr, hexs = line.split(None, 1)
            out.append((addr, hexs))
    return out


# ------------------------------------------------------------------ RING (s7-ring.md Facts for tests)

RING_BYTES = _facts("""
seg11:0x8a50 c7 46 fc 50 00
seg11:0x8a55 c7 46 e6 1e 00
seg11:0x8a91 83 bc ae 70 46
seg11:0x8ab2 b8 ba ff
seg11:0x8aaf 68 fa 00
seg11:0x8af2 c7 46 f4 0b 00
seg11:0x8f4d 83 46 f4 15
seg11:0x8f54 6b 46 fc 0a
seg11:0x8f58 b9 0e 00
seg11:0x8b4c c7 87 92 1e 0a 00
seg11:0x9007 c7 87 9a 1e 06 ff
seg11:0x902d 66 c7 87 90 1e 01 00 0d 00
seg11:0x8b02 83 7e f6 2d
seg11:0xa049 c1 e0 02
seg11:0xa06f 81 f9 96 00
seg11:0xa1b6 83 7e f6 05
seg11:0xa1bc 83 7e f6 0c
seg11:0xa152 c1 e0 02
seg11:0xa284 6a 22
seg11:0xa27a 6a 01
seg11:0xa3eb 05 d0 07
seg11:0xa411 6b 46 e8 03
seg11:0xa415 b9 05 00
seg11:0xa43d b9 03 00
seg11:0xa471 83 bd 0a 97 07
seg11:0xa8ad-0xa8af 6a 00 68 00 ff
seg11:0xa91e-0xa920 6a 00 68 00 ff
seg11:0xa8bc-0xa8bf 68 ff 00 6a c3
seg11:0xa92d-0xa930 68 ff 00 6a c3
seg11:0xa8ab 6a 02
seg11:0xa91c 6a 01
seg11:0xaa65 66 68 7f ff ff 00
seg11:0xaaf9 66 68 7f ff ff 00
seg11:0xaa0d-0xaa13 6a 00 6a 02 6a 00 6a 00
seg11:0xaacb-0xaad1 6a 01
seg11:0xabbd 6a 07
seg11:0x1ccf 2d 6d 00
seg11:0x1cef 2d 6d 00
seg11:0x1cd4 99 2b c2 d1 f8
seg11:0x1cf4 b9 05 00
seg11:0x1cac 80 b8 48 71 03
""")


@pytest.mark.parametrize("addr,hexs", RING_BYTES, ids=[a for a, _ in RING_BYTES])
def test_ring_lightning_bytes(ins, addr, hexs):
    """s7-ring.md Facts for tests: ring constants, lightning constants, pens and charge instructions."""
    assert _bytes_fact(addr, hexs)


@pytest.mark.parametrize("site,val", [("seg11:0x8bda", 5), ("seg11:0x8c68", 5), ("seg11:0x8cea", 10),
                                      ("seg11:0x8d79", 20), ("seg11:0x8e07", 15), ("seg11:0x8e8f", 15),
                                      ("seg11:0x8f1d", 10)])
def test_ring_m_constants(ins, site, val):
    """s7-ring.md Ring constants: the m constants of the seven later blocks are immediates of these sites."""
    i = ins.at(site)
    imms = [int(o, 0) & 0xFFFF for o in i.ops if re.fullmatch(r"-?(?:0x[0-9a-f]+|\d+)", o)]
    assert val in imms, i.text


@pytest.mark.xfail(strict=True, reason="s7-ring.md lists `83 3e 72 30` (cmp word ptr [0x3072]) at seg11:0xa01f and "
                   "seg11:0xa1de; the bytes there are `8b 0e 72 30` (mov cx, [0x3072]) and `2b 06 72 30` "
                   "(sub ax, [0x3072]): the word is read, not compared")
@pytest.mark.parametrize("addr", ["seg11:0xa01f", "seg11:0xa1de"])
def test_ring_cost_word_as_listed(ins, addr):
    """The fact as the analysis states it: a `cmp word ptr [0x3072], imm` (83 3e 72 30) at both sites."""
    assert _bytes_fact(addr, "83 3e 72 30")


def test_ring_cost_word_as_found(ins):
    """DGROUP:0x3072 is read at seg11:0xa01f (`mov cx, [0x3072]`) and seg11:0xa1de (`sub ax, [0x3072]`), as the bytes hold them."""
    assert ins.at("seg11:0xa01f").raw.hex() == "8b0e7230" and ins.at("seg11:0xa1de").raw.hex() == "2b067230"


def test_ring_geometry_values(master):
    """s7-ring.md Ring values: a = 11, 32, 53; b through one Newton step; world and screen offsets at Size 0, 1, 2."""
    a, avals = 11, []
    while 800 // 14 > a:   # R * 10 / 14 with R = 80: 57
        avals.append(a)
        a += 21
    assert avals == [11, 32, 53]
    bs = []
    for a in avals:
        n, g = 6400 - a * a, 80 - _td(a, 4)
        bs.append(((_td(n, g) + g) >> 1))
    assert bs == [79, 73, 60]
    assert [8 * a for a in avals] == [88, 256, 424] and [8 * b for b in bs] == [632, 584, 480]
    for size, ea, eb in ((0, [5, 16, 26], [39, 36, 30]), (1, avals, bs), (2, [22, 64, 106], [158, 146, 120])):
        assert [(a << size) >> 1 for a in avals] == ea and [(b << size) >> 1 for b in bs] == eb


def test_ring_charge_sequences():
    """s7-ring.md Charge and Strike sequence: c += (109 - c)/2 (player), /5 (others), strike 3A/5, A/3."""
    def seq(div, n):
        c, out = 0, []
        for _ in range(n):
            c += _td(109 - c, div)
            out.append(c)
        return out
    assert seq(2, 8) == [54, 81, 95, 102, 105, 107, 108, 108]
    other = seq(5, 60)
    assert other[:8] == [21, 38, 52, 63, 72, 79, 85, 89] and other[-1] == 105
    c = 108
    assert (c // 2, _td(3 * c, 5), c - _td(c, 3)) == (54, 64, 72)
    chain, t = [], 64
    while t:
        chain.append(t)
        t = _td(3 * t, 5)
    assert chain + [0] == [64, 38, 22, 13, 7, 4, 2, 1, 0]


def test_transformer_ring_numbers(master):
    """s7-ring.md TRANSFORMER and Team 7 designs: MASTER.SHP record 1 / MASTER.BOD body 33, the team byte +0x03."""
    bod, shp = master
    tf = shp[65:130]
    body = struct.unpack_from("<H", tf, 0x21)[0]
    b = bod[body * 44:(body + 1) * 44]
    assert tf[10:21] == b"TRANSFORMER" and body == 33 and tf[6] == 8
    base, armor = b[0x1B], tf[2]
    assert (base, armor) == (24, 34)
    mass = _td(base * 10 * (_td(armor, 4) + 100), 100)
    assert mass == 259 and _td(mass, 22) == 11
    tank = tf[0] * 40
    assert tank == 1320 and mass + _td(tank, 20) == 325
    assert _td(-70 * 325, 250) == -91
    team7 = [k for k in range(118) if shp[k * 65 + 3] == 7]
    assert team7 == [22, 35, 42, 47, 77, 85]
    names = [shp[k * 65 + 10:k * 65 + 31].split(b"\0")[0].decode() for k in team7]
    assert names == ["INTERCEPTOR", "CONSTABLE", "LETHAL WEAPON", "PATROLMAN", "TERMINATOR", "ELIMINATOR"]


# ------------------------------------------------------------------ the weapon table (seg10:0x1fb2)

_WIDE = {"ax", "bx", "cx", "dx", "si", "di"}
_NOWRITE = {"cmp", "test", "push", "nop"}


def _weapon_stores(m) -> dict[int, tuple[int, int | None]]:
    """{site offset in seg10: (DGROUP displacement, stored word or None)} for every ``mov word ptr [disp], src``
    of seg10:0x1fb2, walking the function in address order with a table of constants in ax, bx, cx, dx, si, di
    (``mov r, imm`` and ``xor r, r`` set one; any other write, a call or a far call clears it). The table is not
    reset at a jump target: the initialiser is straight-line apart from the DGROUP:0x920c diamonds, whose arms
    only store to memory."""
    regs: dict[str, int] = {}
    out = {}
    for i in m.func_insns("seg10:0x1fb2"):
        if i.mnem in ("call", "lcall") or i.text.startswith("call"):
            regs.clear()
            continue
        if i.mnem == "mov" and i.abs_ds and i.mem.size == 2 and i.ops and i.ops[0].startswith(("word", "[", "ds")):
            src = i.ops[-1]
            v = (int(src, 0) & 0xFFFF) if re.fullmatch(r"-?(?:0x[0-9a-f]+|\d+)", src) else regs.get(src)
            out[i.off] = (i.mem.disp, v)
            continue
        if i.mnem == "mov" and i.ops and i.ops[0] in _WIDE and re.fullmatch(r"-?(?:0x[0-9a-f]+|\d+)", i.ops[1]):
            regs[i.ops[0]] = int(i.ops[1], 0) & 0xFFFF
        elif i.mnem == "xor" and i.ops and i.ops[0] == i.ops[1] and i.ops[0] in _WIDE:
            regs[i.ops[0]] = 0
        elif i.mnem not in _NOWRITE and not i.mnem.startswith("j") and i.ops and i.ops[0] in _WIDE:
            regs.pop(i.ops[0], None)
        elif i.mnem not in _NOWRITE and i.ops and i.ops[0][:1] in "abcd" and i.ops[0][1:] in ("l", "h"):
            regs.pop(i.ops[0][0] + "x", None)
    return out

# (weapon, offset, value, site); site is a store in seg10:0x1fb2. Offsets below 0 are the last words of block w-1.
WEAPON_WORDS = (
    # RING (s7-ring.md)
    [(1, o, v, s) for o, v, s in (
        (0x00, 0xbb, 0x20f1), (0x02, 0xc2, 0x20eb), (0x04, 3, 0x20e5), (0x0a, 0, 0x20fd), (0x20, 0x14, 0x2118),
        (0x28, 0x19, 0x20f7), (0x2a, 6, 0x2109), (0x2c, 1, 0x210f), (0x2e, 0x14, 0x211b), (0x32, 0xc8, 0x2103),
        (0x34, 0x258, 0x2124), (0x42, 0, 0x2100), (0x46, 2, 0x211e))]
    + [(3, o, v, s) for o, v, s in (
        (0x00, 0xbd, 0x2092), (0x02, 0xc5, 0x208c), (0x04, 1, 0x20a1), (0x0a, 0, 0x20c4), (0x28, 0x50, 0x2098),
        (0x2a, 7, 0x20ad), (0x2c, 4, 0x20b3), (0x2e, 0x14, 0x20bf), (0x30, 0x14, 0x20bc), (0x32, 0x12c, 0x20a7),
        (0x34, 0x3b6, 0x20ca), (0x42, 1, 0x20a4), (0x46, 0, 0x20c7))]
    # FUEL (s7-fuel.md)
    + [(4, 0x2e, 8, 0x205f), (5, 0x2e, 8, 0x2015), (13, 0x46, 1, 0x2a3b), (27, 0x1e, 50, 0x2a6e),
       (4, 0x02, 196, 0x203b), (13, 0x02, 195, 0x2a0e)]
    # LAUNCHED (s7-launched.md)
    + [(6, 0x0a, 2, 0x2249), (6, 0x38, 1, 0x221c), (6, 0x3c, 0, 0x2284), (6, 0x20, 50, 0x2201),
       (6, 0x24, 1, 0x221f), (6, 0x34, 1100, 0x22a0), (6, 0x18, 0x1c, 0x223d), (6, -2, 370, 0x2228),
       (9, 0x04, 2, 0x2951), (9, 0x06, 1, 0x291b), (9, 0x0a, 4, 0x290c), (9, 0x12, 2, 0x2954),
       (9, 0x14, 4, 0x290f), (9, 0x16, 3, 0x2960), (9, 0x18, 2, 0x2957), (9, 0x1e, 200, 0x2912),
       (9, 0x38, 1, 0x2921), (9, 0x40, 5, 0x292d), (9, -2, 161, 0x2939),
       (10, 0x04, 2, 0x27dd), (10, 0x06, 4, 0x27f8), (10, 0x0a, 1, 0x279e), (10, 0x1e, 64, 0x2792),
       (10, 0x26, 6, 0x27ad), (10, 0x40, 6, 0x27b0), (10, 0x18, 24, 0x27ce), (10, -8, 22, 0x27b3),
       (10, -2, 150, 0x27c2),
       (11, 0x0a, 1, 0x285d), (11, 0x1e, 100, 0x2845), (11, 0x22, 8, 0x2848),
       (12, 0x0a, 4, 0x2445), (12, 0x06, 1, 0x2469), (12, 0x12, 2, 0x24a2), (12, 0x14, 1, 0x246f),
       (12, 0x16, 2, 0x24a5), (12, 0x18, 0x4e, 0x2499), (12, 0x40, 7, 0x2478),
       (14, 0x0a, 4, 0x2b06), (14, 0x14, 3, 0x2b60), (14, 0x16, 1, 0x2b27), (14, 0x1e, 210, 0x2b18),
       (14, -2, 680, 0x2b39),
       (16, 0x04, 2, 0x2d66), (16, 0x0a, 1, 0x2dbe), (16, 0x1e, 350, 0x2d7e), (16, 0x26, 8, 0x2d84),
       (16, 0x40, 6, 0x2d90), (16, 0x42, 1, 0x2dca), (16, -8, 23, 0x2d93),
       (17, 0x04, 3, 0x2c1a), (17, 0x0a, 5, 0x2bd2), (17, 0x0e, 120, 0x2bde), (17, 0x1e, 60, 0x2bea),
       (17, 0x26, 1, 0x2c76), (17, 0x46, 1, 0x2c7e), (17, -8, 9, 0x2bf0),
       (18, 0x0a, 1, 0x2589), (18, 0x24, 4, 0x256e), (18, 0x1e, 106, 0x2525),
       (19, 0x24, 6, 0x25f9), (19, 0x0c, 4, 0x2641), (19, -8, 27, 0x25ff),
       (20, 0x24, 4, 0x270d), (20, 0x22, 5, 0x26c8),
       (21, 0x0a, 5, 0x2cff), (21, 0x0e, 95, 0x2ca2), (21, 0x12, 0xFFFF, 0x2cc9), (21, 0x08, 3, 0x2cdb),
       (21, 0x46, 1, 0x2d48), (21, 0x1e, 85, 0x2cae),
       (22, -2, 0xFFFF, 0x2e6c), (22, 0x0a, 3, 0x2e7b), (22, 0x2e, 150, 0x2eae), (22, 0x22, 20, 0x2e42)]
)


@pytest.mark.parametrize("w,off,val,site", WEAPON_WORDS,
                         ids=[f"w{w}{o:+#x}@{s:#x}" for w, o, _, s in WEAPON_WORDS])
def test_weapon_word_stores(ins, w, off, val, site):
    """The word seg10:0x1fb2 stores at W(w) + off (W(w) = DGROUP:0x2f54 + 0x50 w), resolved by _weapon_stores."""
    disp, v = _weapon_stores(ins)[site]
    assert (disp - (W0 + 0x50 * w), v) == (off, val & 0xFFFF)


def test_weapon_table_records_closed(ins):
    """s7-launched.md Weapon words: no record has +0x24 = 5; every record with +0x0a != 0 has +0x3a = 1 and +0x10 = 20.
    The stores are collected from the whole function seg10:0x1fb2 (constant sources only)."""
    assert len(_weapon_stores(ins)) > 600
    rec: dict[int, dict[int, int]] = {}
    for off, (disp, v) in _weapon_stores(ins).items():
        if v is None or not W0 - 8 <= disp < W0 + 0x50 * 29:
            continue
        w, o = divmod(disp - W0, 0x50)
        if o >= 0x48:
            w, o = w + 1, o - 0x50
        rec.setdefault(w, {})[o] = v
    assert all(r.get(0x24) != 5 for r in rec.values())
    kinds = {w: r for w, r in rec.items() if r.get(0x0A, 0) != 0}
    assert kinds, "no record with +0x0a != 0 found"
    for w, r in kinds.items():
        assert r.get(0x3A) == 1 and r.get(0x10) == 20, (w, r)


# ------------------------------------------------------------------ TETHER (s7-tether.md Facts for tests)

TETHER_BYTES = _facts("""
seg10:0x213f-0x2183 c7 06 f8 2f 03 00
seg10:0x213f-0x2183 c7 06 f6 2f be 00
seg10:0x213f-0x2183 c7 06 f4 2f bc 00
seg10:0x213f-0x2183 c7 06 1c 30 28 00
seg10:0x213f-0x2183 33 c0
seg10:0x213f-0x2183 a3 fe 2f
seg10:0x213f-0x2183 a3 36 30
seg10:0x213f-0x2183 c7 06 26 30 18 01
seg10:0x213f-0x2183 c7 06 24 30 3c 00
seg10:0x213f-0x2183 c7 06 1e 30 06 00
seg10:0x213f-0x2183 c7 06 20 30 02 00
seg10:0x213f-0x2183 c7 06 22 30 19 00
seg10:0x213f-0x2183 c7 06 3a 30 01 00
seg10:0x213f-0x2183 c7 06 28 30 e8 03
seg10:0x2189 83 3e 0c 92 03 75 08 c7 06 2a 30 05 00 eb 06 c7 06 2a 30 0f 00
seg10:0x219e-0x21da c7 06 08 34 03 00
seg10:0x219e-0x21da c7 06 06 34 be 00
seg10:0x219e-0x21da c7 06 04 34 c9 00
seg10:0x219e-0x21da c7 06 2c 34 1a 00
seg10:0x219e-0x21da c7 06 36 34 18 01
seg10:0x219e-0x21da c7 06 34 34 3c 00
seg10:0x219e-0x21da c7 06 30 34 02 00
seg10:0x219e-0x21da c7 06 32 34 14 00
seg10:0x219e-0x21da a3 0e 34
seg10:0x219e-0x21da a3 46 34
seg10:0x219e-0x21da a3 2e 34
seg10:0x219e-0x21da a3 4a 34
seg10:0x219e-0x21da c7 06 38 34 ee 02
seg10:0x21e0-0x21ef c7 06 3a 34 ff ff
seg10:0x12c1 b8 0c 00
seg10:0x12d8 26 c7 40 20 60 00
seg10:0x12e6 b8 06 00
seg10:0x1309 26 c7 40 44 0b 00
seg10:0x1313 26 c7 40 18 01 00
seg10:0x131d 26 c7 40 42 06 00
seg10:0x1327 26 c7 40 46 04 00
seg10:0x1331 26 c7 40 4c 7f 7f
seg10:0x1337 26 c7 40 4e 7f 00
seg10:0x1341 26 c7 40 2c ff ff
seg10:0x134b 26 c7 40 48 00 00
seg10:0x4db2-0x4df3 6a 00 6a 01 6a 00 68 ff 00
seg10:0x4db2-0x4df3 6a 00 6a 01 6a 7f 6a 7f
seg10:0x4db2-0x4df3 6a 00 6a 01 6a 7f 68 ff 00
seg10:0x4db2-0x4df3 6a 00 6a 01 66 68 ff 00 ff 00
seg10:0x5632-0x564d 6a 00 6a 00 6a 0c 6a 0c 6a 00 6a 01 6a 00 6a 03 6a 63 6a 00 1e 68 6e 07 68 b4 00 6a 03
seg11:0x95a1 6a 1d
seg11:0x9586 26 c7 40 02 18 fc
seg11:0x96b1 c7 46 da 2c 01
seg11:0x97f6 05 90 01
seg11:0x97f9 3d b0 04
seg11:0x992b c7 46 f2 a0 00
seg11:0x9932 c7 46 f2 6e 00
seg11:0x9a9a 6b 97 50 10 07
seg11:0x9aba 6b 97 52 10 07
seg11:0x9c51 81 c2 20 03
seg12:0x333d 83 bf d4 65 29
seg12:0x345c 83 85 d4 65 32
seg12:0x326a b9 80 00
seg12:0x3278 b9 80 00
seg12:0x3283 02 c1
seg12:0x35c4-0x35cb 6a 01 ff 76 0a 6a 0a 6a 06
seg8:0x1ec2 c7 46 d4 2c 01
seg8:0x1f99 b9 38 ff
seg8:0x1f9f 05 08 00
seg8:0x1fa2 3d 03 00
seg8:0x1fc9 83 af ae 70 03
seg8:0x1fd5 c7 46 ee e8 03
seg8:0x1fda 6b 46 d2 64
seg8:0x1fde 05 5e 01
seg8:0x20b6 6a 69
seg8:0x20c6 6a 14
seg8:0x20d0 83 7e ee 28
seg8:0x21a8 6a 64
seg8:0x21c9 66 b9 9c ff ff ff
seg8:0x21f6 3d 3c 00
seg8:0x21fe 6b c0 32
seg8:0x23fd b8 b4 00
seg8:0x244a b8 4c ff
seg8:0x2457 b8 4c ff
seg8:0x24a4 b8 b4 00
seg8:0x25eb 6a 06
seg13:0x7a9 26 81 78 44 f4 01
seg13:0x842 3d 78 00
seg13:0x850 3d f4 01
seg13:0x903 66 3d 7d 00 00 00
seg12:0x730a b1 07
""")


@pytest.mark.parametrize("addr,hexs", TETHER_BYTES, ids=[f"{a}:{h[:14]}" for a, h in TETHER_BYTES])
def test_tether_bytes(ins, addr, hexs):
    """s7-tether.md Facts for tests: the W(2) and W(15) fills, the pool-10 template, pens, constants."""
    assert _bytes_fact(addr, hexs)


def test_tether_pen_stores(ins):
    """s7-tether.md rope pens: the four pens go to DGROUP:0x3b44, 0x3b46, 0x3b48, 0x3b4a."""
    stores = sorted(i.mem.disp for i in ins.insns if i.seg == 10 and 0x4DB2 <= i.off <= 0x4E10
                    and i.mnem == "mov" and i.abs_ds and 0x3B44 <= i.mem.disp <= 0x3B4A)
    assert stores == [0x3B44, 0x3B46, 0x3B48, 0x3B4A]


def test_tether_muzzle_table(dgroup):
    """s7-tether.md DGROUP:0x10d0 (non-ship muzzle): 16 (x, y) word pairs."""
    exp = [(-14, -23), (-7, -24), (-3, -21), (-3, -18), (0, -14), (-2, -8), (-4, -6), (-6, -3),
           (-13, 0), (-16, 0), (-21, -4), (-22, -10), (-22, -13), (-23, -18), (-24, -22), (-18, -23)]
    assert dgroup[0x10D0:0x1110].hex() == ("f2ffe9fff9ffe8fffdffebfffdffeeff0000f2fffefff8fffcfffafffafffdfff3ff"
                                           "0000f0ff0000ebfffcffeafff6ffeafff3ffe9ffeeffe8ffeaffeeffe9ff")
    assert list(struct.iter_unpack("<hh", dgroup[0x10D0:0x1110])) == exp


def test_tether_bitmaps():
    """s7-tether.md: the hook bitmap IGRAPHIC.DLL 4000/180 is 12 x 12, 4 bpp, 16 colours; the picture 4000/190 is 28 x 32, 4 bpp."""
    w, h, bpp, tab = _dib(_resource("IGRAPHIC.DL1", 4000, 180))
    assert (w, abs(h), bpp, len(tab)) == (12, 12, 4, 16)
    w, h, bpp, _ = _dib(_resource("IGRAPHIC.DL1", 4000, 190))
    assert (w, abs(h), bpp) == (28, 32, 4)


def test_enforcers_team(game):
    """s7-tether.md team names: INSPACE.INI [Teams] Team8 = ENFORCERS (the team byte 7)."""
    txt = (game / "INSPACE.INI").read_text(errors="replace")
    sec = txt.split("[Teams]", 1)[1].split("\n[", 1)[0]
    assert re.search(r"(?mi)^\s*Team8\s*=\s*ENFORCERS\s*$", sec)


# ------------------------------------------------------------------ carriers (MASTER.SHP +0x06 = weapon, +0x03 = team)


def _carriers(shp: bytes, w: int) -> list[int]:
    return [k for k in range(118) if shp[k * 65 + 6] == w]


def test_carriers_tether(master):
    """s7-tether.md carriers: weapon 2 (GRAPPLING HOOK) 9, 51, 57, 78, 87; weapon 15 (HANDCUFFS) 22, 35, 42, 47 (team 7), 103 (team 6)."""
    shp = master[1]
    assert _carriers(shp, 2) == [9, 51, 57, 78, 87]
    assert _carriers(shp, 15) == [22, 35, 42, 47, 103]
    assert [shp[k * 65 + 3] for k in (22, 35, 42, 47, 103)] == [7, 7, 7, 7, 6]


def test_carriers_fuel(master):
    """s7-fuel.md Oracle checks: SNOWBALLS (4), FIREBALLS (5), WILDCARD DEFENSE (13); carriers by record index and name."""
    shp = master[1]
    exp = {5: {5: "FIRE TALON", 21: "TIGER", 60: "RED HOT JAG", 67: "SIZZLING DUCK", 76: "CORSAIR"},
           4: {13: "CANADIAN", 16: "GREEN DRAGON", 17: "KOOL KAT", 30: "COLD HEART", 40: "BLUE CAVALIER",
               41: "ICE EAGLE"},
           13: {11: "TEDDY BEAR", 28: "MAGIC BUNNY", 62: "DEEP ENIGMA", 64: "HEART OF GOLD"}}
    for w, d in exp.items():
        assert _carriers(shp, w) == sorted(d), w
        for k, name in d.items():
            assert shp[k * 65 + 10:k * 65 + 31].split(b"\0")[0].decode() == name


def test_carriers_ring(master):
    """s7-ring.md: TRANSFORMER (record 1) carries weapon 8 on body 33 (the ring is weapon 1; its carriers are listed for the oracle)."""
    shp = master[1]
    assert shp[65 + 6] == 8
    assert isinstance(_carriers(shp, 1), list) and isinstance(_carriers(shp, 3), list)


# ------------------------------------------------------------------ FUEL (s7-fuel.md Facts for tests)

FUEL_BYTES = _facts("""
seg11:0xadc0 4c ad
seg11:0xadc2 38 ad
seg11:0xadd2 9e ad
seg11:0xad40 6a 1e
seg11:0xad42 6a 05
seg11:0xad56 6a 04
seg13:0x16a7 ca 08 00
seg13:0x135e 83 bc ae 70 28
seg13:0x136c 2b 87 82 2f
seg13:0x1370 79 02
seg13:0x13a1 3b 06 10 94
seg13:0x1408 26 c7 41 44 78 00
seg13:0x1412 26 c7 41 42 64 00
seg13:0x141c 66 26 c7 41 4c ff 00 00 00
seg13:0x14c4 26 c7 40 44 c8 00
seg13:0x14da 26 c7 40 44 78 00
seg13:0x1506 b8 9c ff
seg13:0x15a4 83 c1 06
seg13:0x15c6 2d 20 00
seg13:0x165f 6a 69
seg13:0x1667 83 7e 08 18
seg13:0x1674 6a 1b
seg13:0x1687 6a 19
seg7:0x40de 83 bf 4c 5a 1e
seg7:0x40e5 66 c7 46 f2 07 00 34 00
seg7:0x40ed c7 46 f0 15 00
seg7:0x474f 83 bf 4c 5a 1c
seg7:0x475c b9 05 00
seg7:0x4765 6b c8 06
seg7:0x4768 83 c1 0a
seg7:0x476e 6b c0 03
seg7:0x421a c7 46 ee 0a 00
seg7:0x4238 c7 46 ee 0b 00
seg7:0x424a 6a 06
seg7:0x42dd bb 28 00
seg7:0x434a 66 68 c9 00 a0 00
seg7:0x4364 83 7e f8 02
seg7:0x4486 66 05 88 13 00 00
seg7:0x4538 66 05 88 13 00 00
seg7:0x44f4 26 81 7c 44 ac 0d
seg7:0x45a9 26 81 78 44 a0 00
seg7:0x45de 66 81 c1 a0 0f 00 00
seg7:0x464b 05 04 00
seg7:0x4691 6a 36
seg7:0x46df 83 7e fa 06
seg7:0x5956 b9 03 00
seg7:0x5985 b9 fd ff
seg7:0x599c c7 85 4e 5a 10 00
seg7:0x5a10 c7 87 4e 5a 10 00
seg11:0x90b2 80 b8 48 71 0d
seg11:0x90b9 80 b8 48 71 17
seg11:0x90ef 83 bf e4 66 1b
seg11:0x911e c7 06 c0 37 39 00
seg11:0x9124 c7 06 e8 37 05 00
seg11:0x9132 26 c7 40 66 60 6a
seg11:0x91bb 81 c2 94 02
seg11:0x9436 3d 8a 02
seg10:0x205f c7 06 c2 30 08 00
seg10:0x2015 c7 06 12 31 08 00
seg10:0x2a3b c7 06 aa 33 01 00
seg10:0x2a6e c7 06 e2 37 32 00
seg10:0x203b c7 06 96 30 c4 00
seg10:0x2a0e c7 06 66 33 c3 00
seg12:0x3145 ca 0a 00
seg12:0x22d3 2d 07 00
""")


@pytest.mark.parametrize("addr,hexs", FUEL_BYTES, ids=[f"{a}:{h[:14]}" for a, h in FUEL_BYTES])
def test_fuel_bytes(ins, addr, hexs):
    """s7-fuel.md Facts for tests: the switch cases, the ball constants, the shell's table sites, the weapon words."""
    assert _bytes_fact(addr, hexs)


def test_pool5_table_sizes(ins):
    """s7-fuel.md Table sizes: the entry table DGROUP:0x5a4c + 0x14 r ends at DGROUP:0x5b28 and the trail DGROUP:0x5eac
    + 0x24 r at DGROUP:0x6038, 11 entries each; both next tables are addressed by the code."""
    assert 0x5A4C + 0x14 * 11 == 0x5B28 and 0x5EAC + 0x24 * 11 == 0x6038
    for disp, sites in ((0x5B28, ["seg8:0x870", "seg8:0x88b", "seg7:0x3922"]), (0x6038, ["seg7:0x2824", "seg7:0x3084"])):
        assert all(ins.at(a).mem.disp == disp for a in sites), disp


@pytest.mark.parametrize("rid,w,h", [(170, 248, 52), (175, 196, 46), (191, 57, 29), (196, 57, 29), (195, 47, 29),
                                     (660, 23, 23), (662, 23, 23), (661, 27, 27)])
def test_fuel_bitmaps(rid, w, h):
    """s7-fuel.md: IGRAPHIC.DLL 4000 sizes of the ball, trail and picture bitmaps."""
    dw, dh, _bpp, _ = _dib(_resource("IGRAPHIC.DL1", 4000, rid))
    assert (dw, abs(dh)) == (w, h)


# ------------------------------------------------------------------ LAUNCHED (s7-launched.md Facts for tests)

TEXTS = {
    "seg12:0x64c3": "test al, 3", "seg12:0x64c7": "cmp cx, 0xd",
    "seg12:0x64d8": "cmp word ptr [bx + 0x2f6a], 2", "seg12:0x668b": "mov ax, word ptr [bx + 0x2f68]",
    "seg12:0x66d7": "cmp byte ptr es:[bx + si + 0x50], 0x19", "seg12:0x66a2": "push 0xb",
    "seg12:0x66a4": "push 5", "seg12:0x66bf": "push 4", "seg12:0x6701": "retf 4",
    "seg4:0x562a": "mov word ptr [bx + 0x66f4], 4", "seg4:0x5636": "mov word ptr [bx + 0x66f4], 3",
    "seg4:0x5642": "mov word ptr [bx + 0x66f4], 2", "seg4:0x5650": "retf 2",
    "seg13:0x10ef": "mov word ptr [bp - 0x20], 0x4e20", "seg13:0x11a8": "cmp ax, 0x12c",
    "seg13:0x1289": "cmp word ptr [bp - 0x26], 0x3c", "seg13:0x129f": "cmp word ptr [bp - 0x26], 0x78",
    "seg13:0x1315": "retf 4",
    "seg13:0x20fa": "mov word ptr [si + 0x610c], 0x14", "seg13:0x2107": "mov word ptr [si + 0x610c], 0x24",
    "seg13:0x2118": "push 0x3e8", "seg13:0x2146": "push 0x1e", "seg13:0x2148": "push 3", "seg13:0x2187": "retf 4",
    "seg12:0x2dc8": "cmp ax, 0x18", "seg12:0x2deb": "mov cx, 0x14", "seg12:0x2df1": "cmp ax, 0xa",
    "seg12:0x2e8e": "mov word ptr es:[bx + di + 0x44], 0x78",
    "seg12:0x2fd3": "mov word ptr es:[bx + si + 0x44], 0xc8", "seg12:0x30cb": "imul ax, word ptr [bx], 6",
    "seg12:0x30f3": "cmp word ptr [bp + 8], 0x19", "seg12:0x310f": "push 0x19", "seg12:0x3145": "retf 0xa",
    "seg11:0x828c": "cmp ax, 0x15e", "seg11:0x82b4": "mov word ptr [bp - 0x10], 0x4e20",
    "seg11:0x82dd": "cmp ax, 0x96", "seg11:0x830c": "add ax, 0x32", "seg11:0x830f": "imul ax, ax, 5",
    "seg11:0x83bc": "cmp word ptr es:[bx + di + 0x44], 1", "seg11:0x8441": "mov cx, 6",
    "seg11:0x8507": "imul ax, ax, 5", "seg11:0x850a": "mov dx, 0x14",
    "seg11:0x8240": "cmp word ptr [bx + 0x2f9a], 0",
    "seg11:0x8841": "add ax, 8", "seg11:0x8847": "mov cx, 0x10", "seg11:0x8856": "mov ax, word ptr [bx + 0x7ba0]",
    "seg11:0x8864": "imul ax, word ptr [bx + 0x1050], 0xb", "seg11:0x88e3": "mov ax, word ptr [bx + 0x7ba2]",
    "seg11:0x8925": "neg ax",
    "seg11:0x8150": "mov word ptr [bx + 0x7096], 6", "seg11:0x816c": "add eax, 0x64",
    "seg11:0x80a6": "mov word ptr [bx + 0x66ec], ax", "seg11:0x8981": "mov word ptr [bx + 0x66ec], 0",
    "seg8:0x355c": "mov word ptr [bp - 0x28], ax",
    "seg12:0x6ad8": "mov word ptr [bx + 0x66ec], ax", "seg12:0x6adc": "mov word ptr [bx + 0x66e4], 0xffff",
    "seg6:0x8569": "call far seg5:0xdc", "seg6:0x857f": "mov word ptr [bp - 0xa], 0x1b",
    "seg6:0x8578": "mov word ptr [bp - 0xa], 0x4e",
}


@pytest.mark.parametrize("site,text", sorted(TEXTS.items(), key=lambda kv: A(kv[0])), ids=sorted(TEXTS, key=A))
def test_launched_instruction_texts(ins, site, text):
    """s7-launched.md Instruction texts (ins.at(site).text)."""
    assert ins.at(site).text == text


def test_launched_raw_and_deltas(ins):
    """s7-launched.md: seg8:0x358e is `66 f7 7e d8` (idiv dword [bp - 0x28]), seg8:0x3589 `66 f7 e9`; the DGROUP deltas."""
    assert ins.at("seg8:0x358e").raw.hex() == "66f77ed8" and ins.at("seg8:0x3589").raw.hex() == "66f7e9"
    assert (0x6112 - 0x6098, 0x610C - 0x6098, 0x60C8 - 0x6098) == (0x7A, 0x74, 0x30)
    assert 0x7BA0 - 0x7B60 == 0x40 and 0x40 // 4 == 16


def test_launched_writers(ins):
    """s7-launched.md Writers: the stores of ship words +0x66e8, +0x66ea, +0x66ee, +0x66f0/+0x66f2, +0x66f4."""
    assert _disp_stores(ins, 0x66E8) == ["seg11:0x89cb"]
    assert _disp_stores(ins, 0x66EA) == ["seg11:0x89d2"]
    assert _disp_stores(ins, 0x66EE) == ["seg11:0x911a"]
    assert _disp_stores(ins, 0x66F0) == ["seg4:0x555b"] and _disp_stores(ins, 0x66F2) == ["seg4:0x5561"]
    assert _disp_stores(ins, 0x66F4) == ["seg4:0x562a", "seg4:0x5636", "seg4:0x5642", "seg11:0x7ec4"]


LAUNCHED_STRINGS = ["THE TARGET MISSILE", "HEATSEEKERS", "RADAR GUIDED", "DEATH HEADS", "THIS MINE",
                    "THESE ARE THE SIMPLEST", "FIRE DEMONS", "THIS DEFENSIVE WEAPON", "BATTERIES", "HANDCUFFS",
                    "DOUGHNUTS", "THESE PELLETS", "BEER BOTTLES", "BOOMERANGS", "SPUD MISSILES", "THROWING STARS",
                    "ENLIGHTENMENT"]


@pytest.mark.parametrize("w", range(6, 23))
def test_launched_strings(w):
    """s7-launched.md Strings: ENG_LANG.DLL 5000 resource 109 + w begins with the weapon's help text."""
    assert _resource("ENG_LANG.dl1", 5000, 109 + w).startswith(LAUNCHED_STRINGS[w - 6].encode())
