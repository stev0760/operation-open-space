"""Mechanical facts of the M4 stage 6 readings in docs/re/{ispace,shell,core,timing,formats,globals,
structs}.md and docs/decisions/0001, 0007 and 0016: the landing pad, scene 105, the device texts,
ordinal 12's ramp builder and test vectors, the visit's jump table, the fast flag and the call
counts of the incoming jet, the descent and the rise.

The listing model, the fixtures and the helpers come from test_re_facts.py; this file follows its
conventions (references, writers, ``segN:0xOFF`` addresses) and skips when ``work/`` or
``original/`` is absent. Each test names the doc file and the section it pins.
"""
from __future__ import annotations

import struct
import zlib
from collections import Counter

from test_re_facts import (  # noqa: F401  (ins, isp and dgroup are pytest fixtures)
    A, REPO_ROOT, _calls, _desk_slots, _emulate_5846, _launch_calls, _resource, _texts, dgroup, ins,
    is_write, isp, unpacked_seg,
)

DOCS = REPO_ROOT / "docs"


def _doc(name: str) -> str:
    return (DOCS / "re" / name).read_text()


def _td(a: int, b: int) -> int:
    """Signed division truncating toward zero (idiv, ISPACE.DLL seg3:0x130)."""
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b >= 0) else -q


def _w16(v: int) -> int:
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


def _half(v: int) -> int:
    """`cwd; sub ax, dx; sar ax, 1`: a halving toward zero."""
    return _td(v, 2)


def _refs(m, lo: int, hi: int) -> tuple[list[str], list[str]]:
    r = m.refs(lo, hi)
    return (sorted({i.addr for i in r if is_write(i)}, key=A),
            sorted({i.addr for i in r if not is_write(i)}, key=A))


# ------------------------------------------------------------------ shell.md Ctrl skip, globals.md


def test_fast_flag_writers_and_readers(ins):
    """shell.md Screen states, Ctrl skip, and globals.md DGROUP:0x928: seven writers, eight readers,
    all of them in seg15's landing code."""
    w, r = _refs(ins, 0x928, 0x929)
    assert w == ["seg16:0xd5e", "seg16:0xd6b", "seg16:0x1bd9", "seg16:0x6948", "seg16:0x6955",
                 "seg16:0x6a7d", "seg18:0x114"]
    assert r == ["seg15:0x1dab", "seg15:0x3192", "seg15:0x35f8", "seg15:0x3876", "seg15:0x38cc",
                 "seg15:0x3900", "seg15:0x397b", "seg15:0x3cb9"]
    for a in r:
        assert "seg15:0x" + a.split(":0x")[1] in _doc("shell.md")


def test_first_landing_flag(ins, dgroup):
    """timing.md Adaptive screen threshold and globals.md DGROUP:0x93c: 1 in the image, one writer."""
    assert struct.unpack_from("<H", dgroup, 0x93C)[0] == 1
    assert _refs(ins, 0x93C, 0x93D) == (["seg15:0x3581"], ["seg15:0x355f"])


# ------------------------------------------------------------------ shell.md item 7, the pad


def test_pad_geometry(ins):
    """shell.md Game-start dialogs item 7, The pad's geometry; structs.md The pad polygon; globals.md
    DGROUP:0x3c8c … 0x3ce6: seg15:0x482e's constants and its values at play height 683, client 768."""
    expect = {"seg15:0x4840": "shl ax, 2", "seg15:0x4843": "mov cx, 3",
              "seg15:0x484b": "sub ax, 0x44c", "seg15:0x484e": "mov dx, 0x16",
              "seg15:0x4858": "add ax, 0x8b", "seg15:0x485b": "mov word ptr [0x3ce4], ax",
              "seg15:0x485e": "mov dx, 0x24", "seg15:0x486a": "add ax, 0xa4",
              "seg15:0x486d": "mov word ptr [0x3ce6], ax", "seg15:0x4870": "mov word ptr [0x92dc], 0xc8",
              "seg15:0x4890": "mov word ptr [0x3c8c], ax", "seg15:0x4898": "mov word ptr [0x3c90], di",
              "seg15:0x489c": "mov di, 1", "seg15:0x48a7": "mov word ptr [0x3c94], bx",
              "seg15:0x48af": "mov cx, 0x1e", "seg15:0x48bc": "add ax, 0x50",
              "seg15:0x48bf": "mov word ptr [0x3c96], ax", "seg15:0x48c2": "mov word ptr [0x3c9a], ax",
              "seg15:0x48c8": "imul ax, ax, 0x64", "seg15:0x48f2": "mov word ptr [0x3c9c], ax",
              "seg15:0x48f7": "mov word ptr [0x3c98], dx", "seg15:0x48fb": "mov word ptr [0x3c9e], dx",
              "seg15:0x4a91": "push 0x3c8c"}
    assert _texts(ins, expect) == expect
    h, client = 683, 768
    q = _td(_w16(h << 2), 3)
    k = q - 1100
    a, pw, pc = _td(k, 22) + 139, _td(k, 36) + 164, _td(k, 30) + 80
    pts = [(_half(pw) - _half(a), 1), (_half(a) + _half(pw), 1), (pw, pc), (0, pc)]
    x = _half(q) - _half(pw)
    m1 = _td(_w16(100 * (1 - pc)), pts[0][0])
    m2 = _td(_w16(100 * (1 - pc)), pts[1][0] - pw)
    vx = _w16(_td(x * (m1 - m2) - pw * m2, m1 - m2))
    vy = _w16(_td(m1 * _w16(vx - x), 100)) + client - 52
    assert (q, a, pw, pc, x, m1, m2, vx, vy) == (910, 131, 159, 74, 376, -521, 486, 452, 321)
    assert pts == [(14, 1), (144, 1), (159, 74), (0, 74)]
    assert ("q 910, A 131, pw 159, pc 74, P0 (14, 1), P1 (144, 1), m1 −521, m2 486, X 376, "
            "DGROUP:0x3ca0 = 452 and DGROUP:0x3ca2 = 321") in _doc("shell.md")
    assert "(14, 1), (144, 1), (159, 74), (0, 74)" in _doc("structs.md")


def test_pad_globals_readers(ins):
    """globals.md DGROUP:0x3ca0, 0x3ca2, 0x3ce4, 0x92dc: who reads the pad's extra stores."""
    assert _refs(ins, 0x3CA0, 0x3CA1) == (["seg15:0x4950"], [])
    assert _refs(ins, 0x3CA2, 0x3CA3) == (["seg15:0x4975"], ["seg15:0x45a5", "seg15:0x45d6", "seg15:0x4792",
                                                              "seg15:0x47bc", "seg15:0x47e4", "seg15:0x480a"])
    assert _refs(ins, 0x3CE4, 0x3CE5) == (["seg15:0x485b"], [])
    assert _refs(ins, 0x92DC, 0x92DD) == (["seg15:0x4870"], [])
    assert _refs(ins, 0x3C9C, 0x3C9F) == (["seg15:0x48f2", "seg15:0x48fb"], [])
    assert [i.addr for i in ins.insns if i.seg == 15 and 0x4A91 < i.off < 0x4AA6 and "Polygon" in i.text] \
        == ["seg15:0x4a96"]


def _pad_greys(pc: int) -> tuple[list[int], list[int]]:
    step, val, bands = _td(0x6A4, pc), 0x1C2, []
    y = 0
    while pc - 2 > y:
        bands.append(_td(val, 10))
        val += step
        val = 0x9F6 if val > 0x9F6 else val + step
        y += 2
    post, val = [], 0x1C2
    for _ in range(0, 0x12, 2):
        post.append(_td(val, 10))
        val = min(val + 0xE3, 0x9F6)
    return bands, post


def test_pad_bands_and_post(ins):
    """shell.md item 7, The drawer seg15:0x4a2e: the band rule and the post's greys at pc = 74."""
    expect = {"seg15:0x4aa6": "mov ax, 0x6a4", "seg15:0x4aaa": "idiv word ptr [0x3c9a]",
              "seg15:0x4ab1": "mov word ptr [bp - 0x10], 0x1c2", "seg15:0x4abd": "mov ax, 0x9f6",
              "seg15:0x4ac3": "add word ptr [bp - 0xc], 2", "seg15:0x4b09": "push 2",
              "seg15:0x4b13": "push 0xc0", "seg15:0x4b16": "push 0xca",
              "seg15:0x4b36": "cmp word ptr [bp - 0x10], 0x9f6", "seg15:0x4b43": "add ax, word ptr [bp - 0xe]",
              "seg15:0x4bad": "mov word ptr [bp - 0xe], 0xe3", "seg15:0x4bb2": "mov word ptr [bp - 0x10], 0x1c2"}
    assert _texts(ins, expect) == expect
    assert all(_calls(ins, {"seg15:0x4aee": "CreateSolidBrush", "seg15:0x4b19": "BitBlt"}).values())
    bands, post = _pad_greys(74)
    assert len(bands) == 36 and bands[:2] == [45, 49] and bands[-1] == 199
    assert bands == [_td(450 + 44 * i, 10) for i in range(36)]
    assert post == [45, 67, 90, 113, 135, 158, 181, 203, 226]
    assert "45, 67, 90, 113, 135, 158, 181, 203, 226" in _doc("shell.md")


def test_pad_dc_and_back_buffer_sites(ins):
    """globals.md DGROUP:0x92b0 … 0x92b6 and DGROUP:0x9580: writers of the ship squares, the pad DC
    and the visit's back buffer."""
    assert _refs(ins, 0x92B0, 0x92B1)[0] == ["seg15:0x216d", "seg15:0x2b97"]
    assert _refs(ins, 0x92B2, 0x92B3) == (["seg15:0x2183", "seg15:0x2bcd"], ["seg15:0xb06", "seg15:0x1cb5"])
    assert _refs(ins, 0x92B4, 0x92B5) == (["seg15:0x2191", "seg15:0x2c9d"], ["seg15:0x2c8d", "seg15:0x2c94"])
    assert _refs(ins, 0x92B6, 0x92B7)[0] == ["seg15:0x1579", "seg15:0x49b9", "seg15:0x49c5"]
    assert _refs(ins, 0x9580, 0x9581)[0] == ["seg15:0x37a4", "seg16:0x27d3"]
    assert _refs(ins, 0x9584, 0x9585)[0] == ["seg15:0x37b8", "seg16:0x27d0"]


# ------------------------------------------------------------------ core.md The visit, timing.md


def test_visit_jump_table(ins):
    """core.md The visit, Phases: the 65-word table at seg16:0x1e6e and the rows it closes."""
    expect = {"seg16:0x1e5f": "cmp ax, 0x40", "seg16:0x1e62": "ja seg16:0x1ef0",
              "seg16:0x1e69": "jmp word ptr cs:[bx + 0x1e6e]"}
    assert _texts(ins, expect) == expect
    tab = struct.unpack_from("<65H", unpacked_seg("inspace", 16), 0x1E6E)
    rows = {0x170E: [3, 7, 12, 14, 32, 37], 0x1430: [5, 9, 16, 23, 34, 39, 42, 55, 57, 62],
            0x17F5: [19], 0x1831: [40], 0x1852: [41], 0x1935: [48], 0x19FF: [49, 52], 0x1956: [50],
            0x1A15: [53], 0x1A55: [54], 0x1B00: [59], 0x1B55: [61], 0x1B7F: [63], 0x1B87: [64]}
    for target, phases in rows.items():
        assert sorted(p for p, t in enumerate(tab) if t == target) == phases, hex(target)
    assert ins.at("seg16:0x1430").text == "push 1"
    assert ins.at("seg16:0x17f5").text == "inc word ptr [0x914a]"


def test_state6_tick_and_record_target(ins):
    """timing.md Adaptive screen threshold and structs.md The left monitor's record: one seg16:0xcfe
    call per state-6 frame, the record of 0x2e bytes and its target 0x24 divided by 4."""
    assert "seg7:0x18f" in ins.call_sites("seg16:0xcfe")
    assert "seg16:0xcfe" in ins.at("seg7:0x18f").text
    expect = {"seg16:0x61bc": "push 0x2e", "seg16:0x61b8": "push 0x42",
              "seg16:0x62e1": "mov word ptr es:[bx + 0x2a], 0x24",
              "seg7:0x222c": "mov ax, word ptr es:[bx + 0x2a]", "seg7:0x2234": "and dx, 3",
              "seg7:0x2239": "sar ax, 2"}
    assert _texts(ins, expect) == expect
    assert all(_calls(ins, {"seg16:0x61be": "GlobalAlloc"}).values())
    assert (0x24, _td(0x24, 4)) == (36, 9)
    # the 1000 ms rule's window and the wrap: 64 · 512 is the first product past 32767
    assert (72 - 8) * 512 == 32768 and (72 - 8) * 511 < 32768


def _jet_calls(y0: int, ye: int) -> tuple[int, int]:
    """shell.md The landing scene, seg15:0x3caa: (all calls, phase-0 calls) from the first call on."""
    y10, phase, step, speed, n, n0 = 10 * y0, 0, _td(y0 - ye, 16) or 1, 150, 0, 0
    while True:
        n += 1
        if phase == 0:
            n0 += 1
            idx = max(0, min(15, _td(_td(y10, -10) + y0 - 55, step)))
            speed = max(150 - 4 * idx, 9)
            y10 += _td(2 * speed, -3)
            if y10 < 10 * ye:
                y10, speed, phase = y10 - 20, 200, 1
            continue
        speed = max(speed - 5, 50)
        y10 += _td(2 * speed, 20)
        if y10 > 10 * (ye + 44):
            return n, n0


def _descent_calls(cal: int, ye: int = 574, y0: int = 274) -> tuple[int, int]:
    """core.md The descent, steps 1-5 with the fast flag clear: (calls, call of the 1 → 0 change)."""
    pos, flag, prev, speed, n, change = 10 * y0, 0, 0, 150, 0, None
    while True:
        n += 1
        if (ye - 10) * 10 < pos:
            flag = 1
        elif (ye - 60) * 10 < pos:
            flag = 0
        elif (y0 + 60) * 10 < pos:
            flag = 1
        if prev == 1 and flag == 0 and change is None:
            change = n
        prev = flag
        speed = max(50, min(120, speed - (3 if cal < 1300 else 5)))
        d = (5 if flag == 0 else 3) if cal < 1300 else (6 if flag == 0 else 4)
        pos += _td(2 * speed, d)
        if 10 * ye < pos:
            return n, change


def test_landing_call_counts(ins):
    """shell.md The landing scene and items 9, core.md The descent, ADR 0001 decision 8: the jet's
    134 calls at 1024 × 768, the descent's 134 and 96 and the state-6 rise's 122 and 57."""
    expect = {"seg15:0x3d36": "and dx, 0xf", "seg15:0x3d3b": "sar ax, 4",
              "seg15:0x3d41": "mov word ptr [0x1808], 0x96", "seg15:0x42b1": "mov cx, 0xfffd",
              "seg15:0x42cc": "sub ax, 0x14", "seg15:0x42d2": "mov word ptr [0x1808], 0xc8",
              "seg15:0x4524": "mov cx, 0x14", "seg15:0x4531": "add ax, 0x2c",
              "seg15:0x3f5d": "sub ax, 0x96", "seg15:0x3f62": "cmp ax, 9",
              "seg15:0x3bd4": "imul ax, word ptr [0x92d6], 0xa"}
    assert _texts(ins, expect) == expect
    assert _jet_calls(758, 161) == (134, 74)
    assert _descent_calls(14000) == (134, 100)
    assert _descent_calls(1000) == (96, 70)
    assert (_launch_calls(574, 14000, -10), _launch_calls(574, 1000, -10)) == (122, 57)
    core = _doc("core.md")
    assert "134 calls, the flag's 1 → 0 change (0x20, IGNITE) at call 100" in core
    assert "96 calls with the change at call 70" in core
    assert "122 calls at Calibrate ≥ 1400 and 57 below" in _doc("shell.md")
    assert "74 in phase 0, then 60 with frame 15" in _doc("shell.md")


# ------------------------------------------------------------------ ispace.md ordinal 32, scene 105


def test_scene_105_projection_and_slots():
    """ispace.md ordinal 32, Scene 105, and shell.md The landing scene step 5: the constants and
    slots 0 and 1 at 1024 × 768, so DGROUP:0x92de = 629 and DGROUP:0x92d6 = 161."""
    consts, slots = _desk_slots(_resource("IGRAPHIC.DL1", 6600, 105), 1024, 768)
    assert consts == {"d28": 1024, "d2c": 512, "d2e": 154, "d4c": 819, "c6c": 629, "d48": 927, "d44": 927}
    assert slots[0] == (1, -255, 618, 1281, 629)
    assert slots[1] == (3, 419, 201, 457, 238)
    assert (slots[0][4], slots[1][2] - 40, 768 - 10) == (629, 161, 758)
    assert "slot 0 = (−255, 618, 1281, 629)" in _doc("ispace.md")


def test_scene_105_header_and_levels(isp):
    """formats.md Header colours and ispace.md ordinal 32, Level: four colours, the overlapping
    divisor, the sign-extended brightness and the box's −10 transform."""
    d = _resource("IGRAPHIC.DL1", 6600, 105)
    w = struct.unpack_from("<54h", d, 0)
    n = w[0x24 // 2]
    cols = [w[3 * i:3 * i + 3] for i in range(n)]
    divs = [w[0x26 // 2 + i] for i in range(n)]
    darks = [w[0x2C // 2 + 3 * i:0x2C // 2 + 3 * i + 3] for i in range(n)]
    assert n == 4
    assert cols == [(255, 255, 255), (0, 255, 0), (255, 235, 80), (0, 120, 240)]
    assert divs == [4, 4, 8, 8] and divs[3] == darks[0][0]
    assert darks == [(8, 12, 12), (0, 32, 0), (60, 54, 19), (0, 0, 56)]
    expect = {"seg8:0x171c": "mov al, byte ptr es:[bx + 0x20]", "seg8:0x1720": "cwde",
              "seg8:0x174e": "imul cx", "seg8:0x1750": "mov cx, 0x32", "seg8:0x1753": "cdq",
              "seg8:0x1754": "idiv cx", "seg8:0x28f4": "imul ax, word ptr [bp + 0x2a], 0xa",
              "seg8:0x28f8": "neg ax", "seg8:0x30a7": "imul ax, word ptr [bp + 0x2a], 5",
              "seg8:0x30b1": "sar ax, 2", "seg8:0x2d6c": "imul ax, word ptr [bp + 0x2a], 3",
              "seg8:0x2d76": "sar ax, 2"}
    assert _texts(isp, expect) == expect      # the listing prints CBW as cwde and CWD as cdq
    assert struct.unpack_from("<H", unpacked_seg("ispace", 8), 0x1720)[0] & 0xFF == 0x98
    body = d[0x80:]
    recs = [body[i * 0x26:(i + 1) * 0x26] for i in range(len(body) // 0x26)]
    assert len(recs) == 89
    neg = [i for i, r in enumerate(recs) if r[0x1E] and r[0x20] >= 128]
    assert neg == [2, 4]
    for i in neg:
        r = recs[i]
        peak = max(range(3), key=lambda j: (cols[r[0x1D]][j], -j))
        level = _td(_w16(cols[r[0x1D]][peak] * (r[0x20] - 256)), 50)
        assert level == -81 and _w16(-10 * level) == 810
    assert "−81" in _doc("ispace.md") and "810" in _doc("ispace.md")


def test_scene_105_text_records():
    """ispace.md ordinal 32, Texts and Device texts; formats.md the record bytes +0x21 … +0x25:
    the string words, size factors and flags of the labelled records."""
    body = _resource("IGRAPHIC.DL1", 6600, 105)[0x80:]

    def rec(i):
        r = body[i * 0x26:(i + 1) * 0x26]
        return struct.unpack_from("<h", r, 0x19)[0], r[0x1E], list(r[0x21:0x26])

    assert rec(63) == (-13, 1, [12, 0, 75, 0, 1])
    assert rec(66) == (-11, 1, [0, 0, 9, 1, 0])
    assert rec(75) == (-12, 1, [0, 0, 9, 1, 0])
    assert rec(56) == (-10, 1, [0, 0, 10, 1, 0])
    assert rec(9) == (1950, 1, [100, 0, 20, 1, 1])
    assert rec(40) == (1951, 1, [12, 0, 50, 0, 1])
    pins = [i for i in range(89) if body[i * 0x26 + 0x1E] == 1 and body[i * 0x26 + 0x22]]
    assert pins == [44, 45, 60, 61, 87]
    crosses = [i for i in range(89) if body[i * 0x26 + 0x1E] == 3 and body[i * 0x26 + 0x21] == 1]
    assert crosses == [31, 32, 43, 59]


def test_device_text_sources(isp):
    """ispace.md ordinal 32, Device texts; formats.md WIN.INI, SYSTEM.INI and other files;
    win16-translation.md Profiles: the INI strings, defaults and buffer sizes, and the CPU class."""
    dg = unpacked_seg("ispace", 11)

    def s(o):
        return dg[o:dg.index(b"\0", o)].decode()

    strings = {0x31: "METAL", 0x37: "", 0x38: "HIGH", 0x3D: "VOLTAGE", 0x45: "\\system.ini",
               0x51: "Video Card", 0x5C: "display.drv", 0x68: "Boot.description", 0x79: "", 0x7A: "Wave",
               0x7F: "drivers", 0x87: "\\control.ini", 0x94: "Sound Card", 0x9F: "drivers.desc",
               0xAC: "Sound Blaster", 0xBA: "\\protocol.ini", 0xC8: "", 0xC9: "netcard",
               0xD1: "network.setup", 0xDF: "\\network.inf", 0xEC: "", 0xED: "netcard", 0x66E: "Arial"}
    assert {o: s(o) for o in strings} == strings
    expect = {"seg8:0x5cb5": "push 0x45", "seg8:0x5cbe": "push 0x68", "seg8:0x5cc2": "push 0x5c",
              "seg8:0x5cc6": "push 0x51", "seg8:0x5cca": "push 0x1b34", "seg8:0x5ccd": "push 0x5a",
              "seg8:0x5f59": "test al, 2", "seg8:0x5f65": "test al, 4",
              "seg8:0x5f6f": "test byte ptr [bp - 0x10b], 4", "seg8:0x5f7d": "test byte ptr [bp - 0x10c], 8",
              "seg8:0x5f5d": "mov word ptr [0x1c70], 2", "seg8:0x5f69": "mov word ptr [0x1c70], 3",
              "seg8:0x5f84": "mov word ptr [0x1c70], 4", "seg8:0x5f91": "mov word ptr [0x1c70], 0",
              "seg8:0x5f76": "mov byte ptr [0x22a8], 1", "seg8:0x5f8a": "mov byte ptr [0x22a8], 0",
              "seg8:0x5f9c": "imul ax, word ptr [0x1c70], 0x64", "seg8:0x5fa1": "add ax, 0x56",
              "seg8:0x5f99": "push 0xd1c"}
    assert _texts(isp, expect) == expect
    sites = ("seg8:0x5cd5", "seg8:0x5d30", "seg8:0x5d6b", "seg8:0x5e51", "seg8:0x5eb2")
    assert all(_calls(isp, {a: "GetPrivateProfileString" for a in sites}).values())
    assert all(_calls(isp, {"seg8:0x5f4c": "GetWinFlags", "seg8:0x5fa5": "seg3:0x236"}).values())
    assert [100 * c + 86 for c in (4, 3, 2, 0)] == [486, 386, 286, 86]
    assert isp.call_sites("seg8:0x5c86") == ["seg8:0x23b"]


def test_renderer_flags(isp):
    """globals.md ISPACE.DLL globals of the renderers: the writers and readers of the flags."""
    def rw(a):
        r = isp.refs(a, a)
        return (sorted({i.addr for i in r if is_write(i)}, key=A),
                sorted({i.addr for i in r if not is_write(i)}, key=A))

    assert rw(0x1EAA) == (["seg5:0x2ec2", "seg5:0x2fde", "seg5:0x310c"], ["seg5:0x3125", "seg6:0xd4e", "seg7:0x1541"])
    assert rw(0x1EAC) == (["seg5:0x2fe4", "seg5:0x3100", "seg5:0x3118"], ["seg5:0x32d0", "seg6:0xd65", "seg7:0x154b"])
    assert rw(0x22A8) == (["seg8:0x5f47", "seg8:0x5f76", "seg8:0x5f8a"], ["seg8:0x169b"])
    assert rw(0x22A9) == (["seg8:0x5e32", "seg8:0x5e5d", "seg8:0x5ebe"], [])
    assert rw(0x22AA) == (["seg8:0x5dd5", "seg8:0x5de8", "seg8:0x5e05"], ["seg8:0x17af"])
    assert rw(0x22AB) == (["seg8:0x5dda", "seg8:0x5e11"], ["seg8:0x17a8"])


# ------------------------------------------------------------------ ispace.md ordinal 12


def test_ramp_builder_bases_and_rows(isp):
    """ispace.md ordinal 12, The ramp builder seg7:0x4ee8: the bases of DGROUP:0x178/0x17c/0x180 and
    the scheme rows its switch at seg7:0x4fc8 picks."""
    dg = unpacked_seg("ispace", 11)
    assert struct.unpack_from("<6H", dg, 0x178) == (740, 40, 852, 332, 920, 448)
    assert _w16(0) == 0 and [b // 4 for b in (740, 852, 920, 40, 332, 448)] == [185, 213, 230, 10, 83, 112]
    assert isp.at("seg7:0x4fc0").text == "cmp ax, 0xd"
    assert isp.at("seg7:0x4fc8").text == "jmp word ptr cs:[bx + 0x4fcd]"
    targets = struct.unpack_from("<14H", unpacked_seg("ispace", 7), 0x4FCD)
    rows = []
    for t in targets:
        i = isp.at(f"seg7:{t:#x}")
        assert i.text.startswith("mov word ptr [bp - 0x12], "), i.text
        rows.append(int(i.ops[1], 0))
    assert rows == [8, 4, 0, 15, 10, 1, 3, 5, 7, 17, 13, 14, 6, 2]
    text = ", ".join(f"{c} → {r}" for c, r in enumerate(rows))
    assert text in _doc("ispace.md")


def _dib4(data: bytes):
    assert data[:2] == b"BM"                  # seg5:0x1b8e requires the file header
    off = struct.unpack_from("<I", data, 10)[0]
    hs, w, h, _p, bpp = struct.unpack_from("<IiiHH", data, 14)
    assert bpp == 4
    tab = [tuple(data[14 + hs + 4 * k:14 + hs + 4 * k + 3][::-1]) for k in range(16)]
    stride = ((w * 4 + 31) // 32) * 4
    bits = data[off:off + stride * abs(h)]
    return w, h, tab, stride, bits


def _newton(x: int, q: int) -> int:
    for _ in range(3):
        x = max(x, 2)
        x += _td(q - x * x, 2 * x)
    return _w16(x)


def _bright(r: int, g: int, b: int, b_rule) -> int:
    r1, g1, b1 = r - (r >> 2), g, b_rule(b)
    return _newton(_td(r1 + g1 + b1, 3), _td(r1 * r1 + g1 * g1 + b1 * b1, 3))


def _bcol(c):  # seg7:0x12d8's colour brightness, B' = (b >> 1) + (b >> 3), no +5
    return _bright(*c, lambda b: (b >> 1) + (b >> 3))


def _bent(c):  # seg7:0x4ee8's and seg5:0x6b72's entry brightness, B' = b >> 1
    return _bright(*c, lambda b: b >> 1)


def _smooth(data: bytes, b1: int, b2: int, mem: bytearray) -> dict:
    """ispace.md ordinal 12 as documented, at depth ≥ 8 on a kind-0 palette with w1 … w5 = −2, −2,
    2, 0, 1, for graphics whose colour 15 is black and whose flags force both scheme bytes to 100."""
    w, h, tab, stride, bits = _dib4(data)
    t10 = lambda r: [tuple(mem[0x117C + 30 * r + 3 * k:0x117C + 30 * r + 3 * k + 3]) for k in range(10)]  # noqa: E731
    t5 = lambda r: [tuple(mem[0x1CDC + 15 * r + 3 * k:0x1CDC + 15 * r + 3 * k + 3]) for k in range(5)]  # noqa: E731
    if b1 not in (12, 0xFF):
        tab[0:10] = t10(b1)
    if b2 not in (12, 0xFF):
        tab[10:15] = t5(b2)
    for k in range(10, 15):
        if tab[k] in tab[0:10]:
            r, g, b = tab[k]
            tab[k] = ((r + 4 if r < 0xFB else r - 4) & 0xFF, (g + 4 if g < 0xFB else g - 4) & 0xFF, b)
    pal = [(0, 0, 0)] * 256

    def ramp(first, last, steps, pos):
        for k in range(first, last):
            c0, c1 = tab[k], tab[k + 1]
            d = [_td(c1[j] - c0[j], steps) for j in range(3)]
            cur = list(c0)
            for _ in range(steps):
                pal[pos] = tuple(v & 0xFF for v in cur)
                pos += 1
                cur = [cur[j] + d[j] for j in range(3)]
        pal[pos] = tab[last]

    ramp(0, 9, 3, 185)
    ramp(10, 14, 4, 213)
    lv = [_bcol(tab[k]) for k in range(15)] + [0]
    lo, hi = [185, 213], [212, 229]
    t1, t0 = [_bent(pal[185]), _bent(pal[213])], [_bent(pal[212]), _bent(pal[229])]
    s = 2 * h
    planes = [bytearray(s * s) for _ in range(4)]
    cb = bytearray(s * s)

    def nib(y, x):
        v = bits[y * stride + (x >> 1)]
        return v >> 4 if x & 1 == 0 else v & 0xF

    for ps in (2, 3):
        layer, pl = ps - 2, planes[ps]

        def add(i, v):
            pl[i] = (pl[i] + v) & 0xFF

        for y in range(h):
            for x in range(h):
                v, p = nib(y, x), 2 * x + 2 * y * s
                block = (p, p + 1, p + s, p + s + 1)
                if v == 15:
                    for q in block:
                        cb[q] = 100
                    continue
                half, qu = (lv[v] >> 1) & 0xFF, (lv[v] >> 2) & 0xFF
                if (1 if v >= 10 else 0) != layer or half == 0:
                    continue
                other = (lambda n: n < 10 or n == 15) if layer else (lambda n: n >= 10)
                cnt = [0, 0, 0, 0]
                spread = {"u": 1, "d": 1, "l": 1, "r": 1}
                for side, ok, yy, xx, hit in (("u", y > 0, y - 1, x, (0, 1)), ("d", y < h - 1, y + 1, x, (2, 3)),
                                              ("l", x > 0, y, x - 1, (0, 2)), ("r", x < h - 1, y, x + 1, (1, 3))):
                    if ok and other(nib(yy, xx)):
                        spread[side] = 0
                        if nib(yy, xx) != 15:
                            for j in hit:
                                cnt[j] += 1
                for q in block:
                    cb[q] = layer & 0xFF          # group 0: 2 · group + layer
                    add(q, half)
                for q, c in zip(block, cnt):
                    if c:
                        add(q, (qu << (c - 1)) & 0xFF)
                if y > 0 and spread["u"]:
                    add(p - s, qu), add(p - s + 1, qu)
                if y < h - 1 and spread["d"]:
                    add(p + 2 * s, qu), add(p + 2 * s + 1, qu)
                if x > 0 and spread["l"]:
                    add(p - 1, qu), add(p + s - 1, qu)
                if x < h - 1 and spread["r"]:
                    add(p + 2, qu), add(p + s + 2, qu)
    for k in (0, 1):
        src, dst = planes[k + 2], planes[k]
        for yy in range(1, s - 1):
            for xx in range(1, s - 1):
                i = yy * s + xx
                v = src[i]
                if not v:
                    continue
                acc = dst[i] + (v >> 1) + (v >> 2)
                for n in (i - s - 1, i - s + 1, i + s - 1, i + s + 1):
                    acc += (src[n] if src[n] > 20 else v) >> 4
                dst[i] = acc & 0xFF

    def cls(i):
        return 2 if cb[i] == 100 else cb[i] & 1

    fixes = 0
    for y in range(2, h - 2):
        for x in range(2, h - 2):
            p = 2 * x + 2 * y * s
            c0, cl, cu, cr, cd = cls(p), cls(p - 2), cls(p - 2 * s), cls(p + 2), cls(p + 2 * s)

            def fix(dst, frm, lay):
                nonlocal fixes
                planes[1][dst] = 0
                planes[0][dst] = 0
                if c0 == 2 or lay != 2:
                    planes[lay][dst] = planes[lay][frm]
                    cb[dst] = (cb[frm] + 101) & 0xFF
                    fixes += 1

            if cl != c0:
                if cu == cl and (c0 != 0 or (cb[p + 2 * s - 2] & 1) == cl):
                    fix(p, p - s, cl)
                if cd == cl and (c0 != 0 or (cb[p - 2 * s - 2] & 1) == cl):
                    fix(p + s, p + 2 * s, cl)
            if cr != c0:
                if cu == cr and (c0 != 0 or (cb[p + 2 * s + 2] & 1) == cr):
                    fix(p + 1, p - s + 1, cr)
                if cd == cr and (c0 != 0 or (cb[p - 2 * s + 2] & 1) == cr):
                    fix(p + s + 1, p + 2 * s + 1, cr)

    def nearest(v, lo_, hi_, p):
        d0 = abs(_bent(pal[p]) - v)
        if d0 < 3:
            return p
        step = 1 if _bent(pal[p]) > v else -1
        e = p + step
        while (e <= hi_) if step > 0 else (e >= lo_):
            d = abs(_bent(pal[e]) - v)
            if d > d0:
                return e - step
            if d < 3:
                return e
            d0, e = d, e + step
        return hi_ if step > 0 else lo_

    out = bytearray(s * s)
    for e in range(s * s):
        a0, a1 = planes[0][e], planes[1][e]
        if a0 > 2 and a1 < a0:
            lay, v = 0, a0
        elif a1 > 2:
            lay, v = 1, a1
        else:
            continue
        if 2 * v > t0[lay]:
            pv = _w16(hi[lay] - _td((v - t0[lay]) * (hi[lay] - lo[lay]), t1[lay] - t0[lay]))
            pv = lo[lay] if pv < lo[lay] else min(pv, hi[lay])
            out[e] = nearest(v, lo[lay], hi[lay], pv) & 0xFF
        else:
            out[e] = 0xF5
    top_down = b"".join(bytes(out[(s - 1 - r) * s:(s - r) * s]) for r in range(s))
    cnt = Counter(top_down)
    return {"side": s, "L": lv[:15], "T1": t1, "T0": t0, "fixes": fixes, "crc": zlib.crc32(top_down),
            "zeros": cnt[0], "f5": cnt[0xF5], "ramp": sum(v for k, v in cnt.items() if k not in (0, 0xF5)),
            "values": len(cnt)}


def test_ordinal12_test_vectors(isp):
    """ispace.md ordinal 12, Test vectors: graphic 300 (b1 = 1, b2 = 12) and the TRANSFORMER's body
    IGRAPHIC.DLL #4000 60 (b1 = 0, b2 = 10) through the documented rule, their CRC-32 and counts."""
    mem = _emulate_5846(isp)
    title = _smooth(_resource("ISPACE.dll", 4000, 300), 1, 12, mem)
    assert title["L"] == [178, 162, 147, 131, 116, 100, 85, 70, 55, 41, 173, 149, 127, 92, 68]
    assert (title["T1"], title["T0"], title["fixes"]) == ([176, 167], [41, 66], 466)
    assert (title["side"], title["zeros"], title["f5"], title["ramp"], title["values"]) == \
        (276, 54338, 30, 21808, 44)
    assert title["crc"] == 0x6B8677A8
    ship = _smooth(_resource("IGRAPHIC.DL1", 4000, 60), 0, 10, mem)
    assert ship["L"] == [205, 186, 166, 145, 126, 106, 85, 65, 46, 26, 171, 138, 105, 73, 44]
    assert (ship["T1"], ship["T0"], ship["fixes"]) == ([198, 171], [24, 44], 79)
    assert (ship["side"], ship["zeros"], ship["f5"], ship["ramp"]) == (66, 2622, 0, 1734)
    assert ship["crc"] == 0x3E518112
    text = _doc("ispace.md")
    for frag in ("0x6b8677a8", "0x3e518112", "466 staircase fixes", "79 fixes", "54 338 zeros",
                 "30 × 0xf5", "21 808 ramp pixels", "2622 zeros, 1734 ramp pixels"):
        assert frag in text, frag


# ------------------------------------------------------------------ the verifier corrections (review M5)


def test_pad16_state6_palette_call_order(ins):
    """shell.md The landing scene, the descent's set-up (s6-pad-verify.md claim 16, CORRECTED): in
    state 6 seg15:0x3590 calls seg10:0x6e76(-1, 1), the greys through seg10:0x6d7a, seg10:0x6e4c,
    seg10:0x6da6(1, 0), seg10:0x6e76(0, 2), the greys, seg10:0x6e4c, the pad seg15:0x482e,
    seg10:0x6e76, seg10:0x6dd4 and seg10:0x6e76, in that order."""
    order = [("seg15:0x3627", "seg10:0x6e76"), ("seg15:0x363d", "seg10:0x6d7a"),
             ("seg15:0x3650", "seg10:0x6e4c"), ("seg15:0x3659", "seg10:0x6da6"),
             ("seg15:0x3665", "seg10:0x6e76"), ("seg15:0x367b", "seg10:0x6d7a"),
             ("seg15:0x368e", "seg10:0x6e4c"), ("seg15:0x3694", "seg15:0x482e"),
             ("seg15:0x369b", "seg10:0x6e76"), ("seg15:0x36a5", "seg10:0x6dd4"),
             ("seg15:0x36b1", "seg10:0x6e76")]
    assert all(_calls(ins, dict(order)).values())
    assert [A(a) for a, _ in order] == sorted(A(a) for a, _ in order)
    expect = {"seg15:0x3619": "cmp word ptr [0x90f4], 6", "seg15:0x3623": "push 1", "seg15:0x3625": "push -1",
              "seg15:0x3655": "push 0", "seg15:0x3657": "push 1", "seg15:0x3661": "push 2",
              "seg15:0x3663": "push 0"}
    assert _texts(ins, expect) == expect


def test_pad30_wing_cuts_test_the_wings_flag(ins):
    """shell.md seg15:0x2a08's common part, the cuts (s6-pad-verify.md claim 30, CORRECTED): each cut
    tests its roster wing byte and then the wings flag [bp-0x42] (ability 3)."""
    expect = {"seg15:0x2ef2": "cmp byte ptr es:[bx + si + 0x72], 0", "seg15:0x2ef9": "cmp word ptr [bp - 0x42], 0",
              "seg15:0x2f3e": "cmp byte ptr es:[bx + si + 0x73], 0", "seg15:0x2f45": "cmp word ptr [bp - 0x42], 0",
              "seg15:0x2c0b": "cmp word ptr [bx - 0x6906], 3"}
    assert _texts(ins, expect) == expect


def test_pad32_body_image_pushes_mov_al(ins):
    """shell.md seg15:0x20ca (s6-pad-verify.md claim 32, CORRECTED): the scheme bytes DGROUP:0x96ec and
    DGROUP:0x96ea + d are pushed as AX after `mov al`, then 1, 0, 2, -2, -2, the far name
    DS:0x9756 + d and the id DGROUP:0x9708 + d for seg10:0x73a6."""
    expect = {"seg15:0x2143": "mov al, byte ptr [bx - 0x6914]", "seg15:0x2147": "push ax",
              "seg15:0x2148": "mov al, byte ptr [bx - 0x6916]", "seg15:0x214c": "push ax",
              "seg15:0x214d": "push 1", "seg15:0x214f": "push 0", "seg15:0x2151": "push 2",
              "seg15:0x2153": "push -2", "seg15:0x2155": "push -2", "seg15:0x2159": "add bx, 0x9756",
              "seg15:0x2161": "push word ptr [bx - 0x68f8]"}
    assert _texts(ins, expect) == expect
    assert _calls(ins, {"seg15:0x2165": "seg10:0x73a6"})["seg15:0x2165"]


def test_scene32_34_left_and_width_adjust(isp):
    """ispace.md ordinal 32, the front face (s6-scene-verify.md claims 32 and 34, CORRECTED): the
    adjustment [bp+0x24] is added to left' [bp-0x32] and taken from the 32-bit SX'' [bp-0x12]."""
    expect = {"seg8:0x34b8": "mov ax, word ptr [bp + 0x24]", "seg8:0x34bb": "add word ptr [bp - 0x32], ax",
              "seg8:0x34be": "cdq", "seg8:0x34bf": "sub word ptr [bp - 0x12], ax",
              "seg8:0x34c2": "sbb word ptr [bp - 0x10], dx"}
    assert _texts(isp, expect) == expect


def test_scene58_landing_blacks_before_the_palette_save(ins):
    """shell.md The landing scene, step 1 (s6-scene-verify.md claim 58, CORRECTED): with DGROUP:0x93a6
    = 0 the PatBlt BLACKNESS of W x DGROUP:0x10 comes before seg10:0x6da6(0, 1)."""
    expect = {"seg15:0x3079": "cmp word ptr [0x93a6], 0", "seg15:0x3088": "push word ptr [0x90ea]",
              "seg15:0x308c": "push word ptr [0x10]", "seg15:0x3090": "push 0", "seg15:0x3092": "push 0x42",
              "seg15:0x3099": "push 1", "seg15:0x309b": "push 0"}
    assert _texts(ins, expect) == expect
    assert all(_calls(ins, {"seg15:0x3094": "PatBlt", "seg15:0x309d": "seg10:0x6da6"}).values())


def test_pacing18_substep2_raises_the_phase(ins):
    """core.md The descent, sub-step 2 (s6-pacing-verify.md claim 18, CORRECTED): in state 6 it raises
    DGROUP:0x914a inside phase 1's call."""
    expect = {"seg15:0x370a": "inc word ptr [0x92a2]", "seg15:0x3710": "cmp word ptr [0x90f4], 6",
              "seg15:0x3717": "inc word ptr [0x914a]"}
    assert _texts(ins, expect) == expect


def test_pacing55_phase64_demon_branch(ins):
    """core.md phase 64 (s6-pacing-verify.md claim 55, CORRECTED): with DGROUP:0x93a6 = 1 phase 64
    calls seg10:0xa90c, clears DGROUP:0x92be and calls seg16:0x7210(1), then leaves through
    seg16:0x11e8."""
    expect = {"seg16:0x1bdf": "cmp word ptr [0x93a6], 1", "seg16:0x1beb": "mov word ptr [0x92be], 0",
              "seg16:0x1bf1": "push 1", "seg16:0x1bf8": "jmp seg16:0x11e8"}
    assert _texts(ins, expect) == expect
    assert all(_calls(ins, {"seg16:0x1be6": "seg10:0xa90c", "seg16:0x1bf3": "seg16:0x7210"}).values())


def test_pacing75_left_monitor_early_return(ins):
    """core.md The left monitor, its end (s6-pacing-verify.md claim 75, CORRECTED): with DGROUP:0x13dc
    = 0 and DGROUP:0x9530 = 2 seg16:0x64fa calls seg18:0x0 and returns before the DGROUP:0x958a
    store."""
    expect = {"seg16:0x6507": "cmp word ptr [0x13dc], 0", "seg16:0x650e": "cmp word ptr [0x9530], 2",
              "seg16:0x651a": "jmp seg16:0x68ce"}
    assert _texts(ins, expect) == expect
    assert _calls(ins, {"seg16:0x6515": "seg18:0x0"})["seg16:0x6515"]
    assert "seg16:0x6507-0x651a" in _doc("core.md")


def test_fixed_set_remap_table(isp):
    """palette.h fixed_set_remap, shell.md Graphic colour tables (review H4): seg6:0x7fc8's switch maps
    nibble v to (15, 5, 1, 2, 9, 6, 10, 13, 14, 4, 0, 3, 8, 7, 11, 12)[v], which under the fixed
    16-colour set gives each index its VGA colour."""
    targets = ["seg6:0x80bc", "seg6:0x80c4", "seg6:0x80cc", "seg6:0x80d4", "seg6:0x80dc", "seg6:0x80e3",
               "seg6:0x80ea", "seg6:0x80f1", "seg6:0x80f8", "seg6:0x80ff", "seg6:0x8106", "seg6:0x810d",
               "seg6:0x8114", "seg6:0x811b", "seg6:0x8122", "seg6:0x8129"]
    m = [15, 5, 1, 2, 9, 6, 10, 13, 14, 4, 0, 3, 8, 7, 11, 12]
    expect = {a: f"mov word ptr [bp - 0x46], {hex(v) if v > 9 else v}" for a, v in zip(targets, m)}
    assert _texts(isp, expect) == expect
    table = struct.unpack_from("<16H", unpacked_seg("ispace", 6), 0x813D)
    assert [A(f"seg6:{hex(t)}") for t in table] == [A(a) for a in targets]
    fixed = [0x00FF00, 0x008000, 0x808000, 0xFFFF00, 0xFF0000, 0x800000, 0x800080, 0xFF00FF, 0x0000FF,
             0x000080, 0x008080, 0x00FFFF, 0xFFFFFF, 0xC0C0C0, 0x808080, 0x000000]
    vga = [0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080, 0x008080, 0xC0C0C0, 0x808080,
           0xFF0000, 0x00FF00, 0xFFFF00, 0x0000FF, 0xFF00FF, 0x00FFFF, 0xFFFFFF]
    assert [fixed[m[v]] for v in range(16)] == vga
    calls = {"seg5:0x57c": "seg6:0x7fc8", "seg6:0x200": "seg6:0x7fc8", "seg6:0x563b": "seg6:0x7fc8",
             "seg7:0x13b8": "seg6:0x7fc8"}
    assert all(_calls(isp, calls).values())
