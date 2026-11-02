"""Mechanical facts of docs/re/core.md and of the M4 stage 0, M5 stage 1-2 and M3 stage 4
readings in docs/re/{structs,globals,formats,ispace,pitfalls,world}.md.

The listing model, the fixtures and the helpers come from test_re_facts.py; this
file follows its conventions (references, writers, ``segN:0xOFF`` addresses) and
skips when ``work/`` or ``original/`` is absent. Each test names the doc file and
the section it pins.
"""
from __future__ import annotations

import struct
from collections import defaultdict

from test_re_facts import (  # noqa: F401  (ins, isp and dgroup are pytest fixtures)
    A, _IMM_RE, _calls, _cstr, _eng, _texts, _trunc, dgroup, ins, is_write, isp, s16,
    unpacked_seg,
)


def _imm_pushes(m, site: str, n: int) -> list[int]:
    """The immediates of the ``n`` pushes before the call at ``site``, nearest first
    (the first cdecl argument first). A non-immediate push gives None; the ``push cs`` of
    a near call within the segment is skipped."""
    out, i = [], m.at(site)
    while len(out) < n:
        i = m.step(i, -1)
        if i.mnem == "push" and i.ops[0] != "cs":
            op = i.ops[0]
            out.append(int(op, 0) & 0xFFFFFFFF if _IMM_RE.fullmatch(op) else None)
    return out


def _colorref(m, call: str) -> int:
    """The COLORREF of the CreateSolidBrush call at ``call``: one 32-bit push (0x66 prefix)
    or two word pushes, high word first."""
    p = m.step(m.at(call), -1)
    assert p.mnem == "push", p.text
    if p.raw[0] == 0x66:
        return int(p.ops[0], 0) & 0xFFFFFFFF
    q = m.step(p, -1)
    assert q.mnem == "push", q.text
    return ((int(q.ops[0], 0) & 0xFFFF) << 16) | (int(p.ops[0], 0) & 0xFFFF)


def _brushes(m, sites: dict[str, int]) -> dict[int, int]:
    """{DGROUP word: COLORREF} for the brush stores that follow the calls at ``sites``."""
    out = {}
    for call, disp in sites.items():
        assert "CreateSolidBrush" in m.at(call).text
        st = m.step(m.at(call), 1)
        assert st.text == f"mov word ptr [{disp:#x}], ax", st.text
        out[disp] = _colorref(m, call)
    return out


def _callers(m, func: str) -> list[str]:
    return sorted({a for a, k in m.funcs[func]["callers"]}, key=A)


# ------------------------------------------------------------------ M4 stage 0: collisions and damage


MOVER_SITES = {
    # core.md Who tests whom: the store of DGROUP:0x943a before each mover's seg8:0x50aa call
    "seg13:0xb7d": ("seg13:0x0", None), "seg7:0x3f2c": ("seg7:0x3bae", None),
    "seg12:0x5459": ("seg12:0x532c", 9), "seg12:0x62c8": ("seg12:0x5636", 8),
    "seg12:0x1cc1": ("seg12:0x1776", 8), "seg12:0x3d8b": ("seg12:0x390e", 8),
    "seg8:0x864b": ("seg8:0x8256", 2), "seg8:0x8e2d": ("seg8:0x8256", 2),
    "seg7:0x3151": ("seg7:0x309c", 0), "seg7:0x339a": ("seg7:0x32d0", 1),
    "seg7:0x43c9": ("seg7:0x40c4", None), "seg12:0x332": ("seg12:0x0", 6),
    "seg12:0x3307": ("seg12:0x3148", 10), "seg12:0x6c91": ("seg12:0x6b1e", 11),
    "seg12:0xb6d": ("seg12:0xae4", 4), "seg16:0x7e1": ("seg16:0xa2", 7),
    "seg18:0x3ce0": ("seg18:0x37d8", 15), "seg18:0x4945": ("seg18:0x403e", 20),
}


def test_collision_movers_register_their_pools(ins):
    """core.md Who tests whom: each mover stores its pool in DGROUP:0x943a and calls
    seg8:0x50aa; 18 call sites, 16 of them in a normal wave and 2 in seg18. The pool-8
    site seg12:0x62c8 lies in seg12:0x5636, the kind-2 mover that seg12:0x633e calls."""
    for site, (func, pool) in MOVER_SITES.items():
        i = ins.at(site)
        assert i.mnem == "mov" and i.abs_ds and i.mem.disp == 0x943A, (site, i.text)
        assert ins.owners(i.seg, i.off) == {func}, site
        assert ins.stored_value(i) == pool, site
    assert {i.ops[1] for i in map(ins.at, ("seg13:0xb7d", "seg7:0x3f2c", "seg7:0x43c9"))} == {"cx"}
    calls = ins.call_sites("seg8:0x50aa")
    assert len(calls) == 18 and sum(c.startswith("seg18:") for c in calls) == 2
    assert {next(iter(ins.owners(*A(c)))) for c in calls} == {f for f, _ in MOVER_SITES.values()}
    assert _callers(ins, "seg12:0x5636") == ["seg12:0x63b0"]
    assert ins.owners(*A("seg12:0x63b0")) == {"seg12:0x633e"}


def test_pair_finder_search_box(ins):
    """core.md Pair test: box B uses h = (70 << Size) >> 1 and q = (15 << Size) >> 1, and
    seg8:0x6afa is called from the race-track path and the visibility loop only."""
    expect = {"seg8:0x525c": "mov cl, byte ptr [0x9156]", "seg8:0x5260": "mov ax, 0x46",
              "seg8:0x5263": "shl ax, cl", "seg8:0x5265": "sar ax, 1",
              "seg8:0x5283": "mov si, 0xf", "seg8:0x5286": "shl si, cl", "seg8:0x5288": "sar si, 1"}
    assert _texts(ins, expect) == expect
    assert (0x46, 0xF) == (70, 15)
    assert _callers(ins, "seg8:0x6afa") == ["seg8:0x5218", "seg8:0x52f8"]


def test_pair_role_flag_has_four_writers(ins):
    """pitfalls.md seg8:0x6afa's party roles, core.md Roles A and B: [bp - 0xc] is written
    only by 1, 0, 1, 0 in this order and read at seg8:0x6d5c; the prologue leaves it."""
    func = ins.func_insns("seg8:0x6afa")
    w = [(i.addr, int(i.ops[1], 0)) for i in func
         if i.mem is not None and i.mem.regs == ("bp",) and i.mem.disp == 0xFFF4 and is_write(i)]
    assert w == [("seg8:0x6d0e", 1), ("seg8:0x6d23", 0), ("seg8:0x6d3d", 1), ("seg8:0x6d57", 0)]
    assert ins.at("seg8:0x6b04").text == "sub sp, 0x16"
    assert ins.at("seg8:0x6d5c").text == "cmp word ptr [bp - 0xc], 0"


def test_ship_damage_scale_and_mode_factor(ins):
    """core.md Damage to a ship steps 3 and 8: d·7/8, the player's (DGROUP:0x9000 + 6)/8,
    then 4d/3 with byte +0xea, 3d (at least 2) cloaked, else 2d (at least 1)."""
    expect = {"seg8:0x468a": "imul ax, word ptr [bp + 0xa], 7", "seg8:0x4694": "sar ax, 3",
              "seg8:0x46a5": "mov ax, word ptr [0x9000]", "seg8:0x46a8": "add ax, 6",
              "seg8:0x496d": "cmp byte ptr [bx + 0x717a], 0", "seg8:0x4977": "shl ax, 2",
              "seg8:0x497a": "mov cx, 3", "seg8:0x497e": "idiv cx",
              "seg8:0x4987": "cmp byte ptr [bx + 0x7178], 0",
              "seg8:0x498e": "imul ax, word ptr [bp + 0xa], 3", "seg8:0x4997": "mov ax, 2",
              "seg8:0x499f": "add ax, ax", "seg8:0x49a6": "mov ax, 1"}
    assert _texts(ins, expect) == expect
    assert (0x717A - 0x7090, 0x7178 - 0x7090) == (0xEA, 0xE8)


def test_lamp_message_cases(ins):
    """core.md Lamp messages: the jump table at seg3:0x1356 (code − 1) sends codes 10-12 to
    seg3:0xd2d (string 134, %S = code + 0x75, the %< %> part on W > 700) and 82, 83 to
    seg3:0xd86 (string 517, %S = code + 0x2e), 84 to seg3:0xd95."""
    sw = ins.switches["seg3:0x1351"]
    assert (sw["table"], ins.at("seg3:0x1346").text, ins.at("seg3:0x1347").text) == \
        ("seg3:0x1356", "dec ax", "cmp ax, 0xbf")
    t = sw["targets"]
    assert [t[c - 1] for c in (10, 11, 12, 82, 83, 84)] == \
        ["seg3:0xd2d"] * 3 + ["seg3:0xd86"] * 2 + ["seg3:0xd95"]
    expect = {"seg3:0xd2d": "push 0x6e", "seg3:0xd35": "cmp word ptr [bp + 0xc], 0xa",
              "seg3:0xd3b": "push 0x2d", "seg3:0xd3f": "push 0x2c",
              "seg3:0xd47": "cmp word ptr [0x90ea], 0x2bc", "seg3:0xd6e": "add ax, 0x75",
              "seg3:0xd74": "push 0x86", "seg3:0xd78": "push 0x8db0",
              "seg3:0xd89": "add ax, 0x2e", "seg3:0xd8f": "push 0x205"}
    assert _texts(ins, expect) == expect
    assert (0x2BC, 0x86, 0x205) == (700, 134, 517)
    assert [_eng(10 + 0x75), _eng(11 + 0x75), _eng(12 + 0x75)] == ["ENGINE", "GUN", "THRUSTER"]
    assert [_eng(82 + 0x2E), _eng(83 + 0x2E)] == ["GUN", "THRUSTER"]
    assert _eng(134) == "DANGER: %S %?. REPAIR OR ABORT%< MISSION%>."
    assert (_eng(135), _eng(519)) == ("TROUBLE", "FAILURE")
    assert _eng(517) == "%S SYSTEM OK REPAIRS COMPLETE"
    assert _eng(138) == "Repairs Complete, %s. Try To Avoid Trouble."


def test_taunt_cases(ins):
    """core.md Wings torn off, Taunts: codes 0x19-0x1e show strings 373, 375, 383, 377, 379
    and 381."""
    t = ins.switches["seg3:0x1351"]["targets"]
    got = {}
    for code in range(0x19, 0x1F):
        i = ins.at(t[code - 1])
        while not (i.mnem == "push" and i.ops[0].startswith("0x1")):
            i = ins.step(i, 1)
        got[code] = int(i.ops[0], 0)
    assert list(got.values()) == [373, 375, 383, 377, 379, 381]
    assert _eng(373) == "Oohh, That Smarts. You Owe Me A Wing!!"
    assert _eng(383) == "Would You Look At That.. Now Our Wings Match!"


def test_wing_tear_timer(ins):
    """core.md Wings torn off, structs.md ship +0x156: seg8:0x2cd8 sets dword +0x156 = now − 10000."""
    expect = {"seg8:0x2cee": "mov cx, word ptr [0x1e50]", "seg8:0x2cf6": "sub cx, 0x2710",
              "seg8:0x2d01": "mov word ptr [bx + 0x71e6], cx"}
    assert _texts(ins, expect) == expect
    assert (0x2710, 0x71E6 - 0x7090) == (10000, 0x156)


def test_ship_death_constants(ins):
    """core.md Death and the explosion: rand() % 60 − 12 offsets, +0x84 = now + 6000, and the
    death deadline DGROUP:0x1e80 = now + 8000; the callers of seg16:0x8e4."""
    expect = {"seg16:0x95d": "call far seg5:0xdc", "seg16:0x962": "mov cx, 0x3c",
              "seg16:0x968": "sub dx, 0xc", "seg16:0x96e": "call far seg5:0xdc",
              "seg16:0xa26": "add ecx, 0x1770", "seg16:0xa2d": "mov dword ptr [bx + 0x7114], ecx",
              "seg16:0xae0": "add eax, 0x1f40", "seg16:0xae6": "mov dword ptr [0x1e80], eax"}
    assert _texts(ins, expect) == expect
    assert (0x3C, 0xC, 0x1770, 0x1F40, 0x7114 - 0x7090) == (60, 12, 6000, 8000, 0x84)
    assert _callers(ins, "seg16:0x8e4") == ["seg8:0x4cc9", "seg11:0x6f50", "seg12:0x4568",
                                            "seg14:0x2130"]


def test_fragment_counts(ins):
    """core.md Fragments, structs.md Effect table: blast_size 1 makes 1 fragment, else 30, 60,
    100 or 130 % for Detail 0-3, at least 2, then (35 − DGROUP:0x9430)·n/25 above 10."""
    sw = {"seg12:0x2829": "je seg12:0x2836", "seg12:0x282c": "je seg12:0x288a",
          "seg12:0x282f": "je seg12:0x289f", "seg12:0x2832": "je seg12:0x28b3"}
    expect = {"seg12:0x2816": "cmp word ptr es:[bx + si + 0x48], 1",
              "seg12:0x281d": "mov word ptr [bp - 8], 1",
              "seg12:0x2844": "imul ax, word ptr es:[bx + si + 0x48], 0x1e",
              "seg12:0x2849": "mov cx, 0x64",
              "seg12:0x2898": "imul ax, word ptr es:[bx + si + 0x48], 0x3c",
              "seg12:0x28ad": "mov ax, word ptr es:[bx + si + 0x48]",
              "seg12:0x28c1": "imul ax, word ptr es:[bx + si + 0x48], 0x82",
              "seg12:0x2855": "cmp ax, 2", "seg12:0x2860": "cmp word ptr [0x9430], 0xa",
              "seg12:0x2867": "mov ax, 0x23", "seg12:0x2871": "mov cx, 0x19", **sw}
    assert _texts(ins, expect) == expect
    assert ins.at("seg12:0x28b1").text == "jmp seg12:0x284f"        # Detail 2 skips the /100
    assert (0x1E, 0x3C, 0x82, 0x23, 0x19) == (30, 60, 130, 35, 25)


def test_rock_damage_constants(ins):
    """core.md Rocks (seg7:0x5090), structs.md object +0x6a: d = 3d/2, no heat from a mass-200
    pool-5 object, heat rises by d/4 to at most 9, and +0x6a = now + 3000."""
    expect = {"seg7:0x50c5": "imul ax, word ptr [bp + 0xa], 3", "seg7:0x50cc": "sar ax, 1",
              "seg7:0x50e7": "mov ax, word ptr [0x944c]", "seg7:0x50ea": "cmp word ptr [0x9328], ax",
              "seg7:0x50fe": "cmp word ptr es:[bx + si + 0x44], 0xc8", "seg7:0x5104": "je seg7:0x5135",
              "seg7:0x510f": "sar ax, 2", "seg7:0x5129": "cmp cx, 9",
              "seg7:0x513c": "add ax, 0xbb8", "seg7:0x5150": "mov word ptr es:[bx + di + 0x6a], ax",
              "seg7:0x5163": "add word ptr es:[bx + di + 0x1e], cx"}
    assert _texts(ins, expect) == expect
    assert (0xC8, 0xBB8) == (200, 3000)


def test_auto_repair_tick(ins):
    """core.md Auto-repair, globals.md DGROUP:0x5014, DGROUP:0x1858: every 181 frames; the
    countdown reloads to max(1, 9 − mass/75); repair needs design word DGROUP:0x971e and
    fuel > 90, skips health ≥ 82 and lamp 3 at 0 or 50; +0x8c = max(27, 4·(75 − gun))/27."""
    expect = {"seg11:0x1d92": "cmp dword ptr [0x5014], eax", "seg11:0x1d97": "jae seg11:0x1eed",
              "seg11:0x1d9b": "add ax, 0xb4", "seg11:0x1da1": "mov word ptr [0x5014], ax",
              "seg11:0x1e6f": "dec word ptr [bx + 0x1858]", "seg11:0x1e7d": "mov cx, 0xffb5",
              "seg11:0x1e87": "add ax, 9", "seg11:0x1e8a": "cmp ax, 1",
              "seg11:0x1e97": "mov word ptr [bx + 0x1858], ax",
              "seg11:0x1eab": "cmp word ptr [bx + 0x1858], 0", "seg11:0x1eb0": "jle seg11:0x1e75",
              "seg11:0x1ebd": "cmp word ptr [bx - 0x68e2], 0",
              "seg11:0x1edc": "cmp word ptr [bx + 0x70ae], 0x5a",
              "seg11:0x1df7": "cmp word ptr [bx + si - 0x690c], -1",
              "seg11:0x1e08": "cmp word ptr [bx + 0x70b6], 0x52",
              "seg11:0x1e15": "cmp word ptr [bx + 0x70b6], 0",
              "seg11:0x1e1c": "cmp word ptr [bx + 0x70b6], 0x32",
              "seg11:0x1e44": "mov ax, 0x4b", "seg11:0x1e50": "shl ax, 2",
              "seg11:0x1e53": "cmp ax, 0x1b", "seg11:0x1e5b": "mov cx, 0x1b"}
    assert _texts(ins, expect) == expect
    assert (0xB4 + 1, s16(0xFFB5), 0x5A, 0x52, 0x32, 0x4B, 0x1B) == (181, -75, 90, 82, 50, 75, 27)
    assert (0x10000 - 0x68E2, 0x10000 - 0x690C) == (0x971E, 0x96F4)
    # the tick's store, and the wave set-up seg9:0x6624's reset to 0 with DGROUP:0x1e68
    assert [w.addr for w in ins.writers(0x5014, 0x5017)] == ["seg9:0x668d", "seg11:0x1da1", "seg11:0x1da4"]
    assert ins.at("seg9:0x668d").text == "mov dword ptr [0x5014], eax" and ins.in_func(ins.at("seg9:0x668d"), "seg9:0x6624")
    assert _callers(ins, "seg11:0x1d7c") == ["seg11:0x1d71", "seg18:0x1c5"]


def test_shot_slot_constants(ins, dgroup):
    """structs.md Shot slots, globals.md DGROUP:0x594: seg11:0x4e0a stores 7, DGROUP:0x598
    holds 2, the 9 slots of 0x94 bytes end at DGROUP:0x65cc; one shot per 5 frames."""
    expect = {"seg11:0x4e19": "mov word ptr [0x594], 7",
              "seg11:0x4e6f": "mov eax, dword ptr [bx + 0x7130]", "seg11:0x4e74": "add eax, 5",
              "seg11:0x4e7a": "cmp eax, dword ptr [0x1e68]"}
    assert _texts(ins, expect) == expect
    assert struct.unpack_from("<h", dgroup, 0x598)[0] == 2
    assert 0x6098 + (7 + 2) * 0x94 == 0x65CC and 0x7130 - 0x7090 == 0xA0


def test_shock_constants(ins):
    """core.md The shock (seg11:0x9daa), globals.md DGROUP:0x238c, DGROUP:0x2408: a ship user
    needs fuel ≥ 250; DGROUP:0x2408 = now + 2000, DGROUP:0x238c = 9 and DGROUP:0x238e = 6."""
    expect = {"seg11:0x9db8": "cmp word ptr [0x238c], 0",
              "seg11:0x9dce": "cmp word ptr [bx + 0x70ae], 0xfa", "seg11:0x9dd4": "jl seg11:0x9fd3",
              "seg11:0x9ddc": "add eax, 0x7d0", "seg11:0x9de2": "mov dword ptr [0x2408], eax",
              "seg11:0x9de6": "mov dword ptr [0x238c], 0x60009"}
    assert _texts(ins, expect) == expect
    assert (0xFA, 0x7D0, 0x60009 & 0xFFFF, 0x60009 >> 16) == (250, 2000, 9, 6)


# ------------------------------------------------------------------ M4 stage 0: capture and scoring


def test_icon_value_formula(ins):
    """core.md Capturing an icon, Value; globals.md DGROUP:0x9604: (damage × −90)/210 + 100,
    a negative value increments DGROUP:0x9498 and becomes 50; resources and +0xd2 gain."""
    expect = {"seg13:0x1ae7": "imul ax, word ptr es:[bx + si + 0x1e], -0x5a",
              "seg13:0x1aec": "mov cx, 0xd2", "seg13:0x1af0": "idiv cx", "seg13:0x1af2": "add ax, 0x64",
              "seg13:0x1af5": "mov word ptr [0x9604], ax", "seg13:0x1afa": "jge seg13:0x1b06",
              "seg13:0x1afc": "inc word ptr [0x9498]", "seg13:0x1b00": "mov word ptr [0x9604], 0x32",
              "seg13:0x1b0e": "add word ptr [bx + 0x715e], ax", "seg13:0x1b12": "inc word ptr [bx + 0x7162]"}
    assert _texts(ins, expect) == expect
    assert (0xD2, 0x64, 0x32) == (210, 100, 50)
    assert _trunc(211 * -90, 210) + 100 == 10               # core.md: at least 10 for damage ≤ 211


def test_capture_awards(ins):
    """core.md Capturing an icon, Score and Teammate; globals.md Scoring: 1000 for a teammate,
    (DGROUP:0x9602 + 4) × 1000 for the player, to the score and to the rating."""
    expect = {"seg13:0x1b4a": "add dword ptr [0x1e58], 0x3e8", "seg13:0x1b53": "add dword ptr [0x5078], 0x3e8",
              "seg13:0x1c1f": "mov ax, word ptr [0x9646]", "seg13:0x1c25": "jne seg13:0x1c42",
              "seg13:0x1c27": "mov ax, word ptr [0x9602]", "seg13:0x1c2a": "add ax, 4",
              "seg13:0x1c2d": "imul ax, ax, 0x3e8", "seg13:0x1c32": "add word ptr [0x1e58], ax",
              "seg13:0x1c3a": "add word ptr [0x5078], ax"}
    assert _texts(ins, expect) == expect
    assert ins.at("seg13:0x1b42").text == "call far seg3:0x6ec"
    assert _eng(1062) == "I've Captured An Icon. Score 1 For The %s."


def test_no_wave_end_score(ins):
    """core.md Scoring, No wave-end bonus: no addition to DGROUP:0x1e58 lies in seg9:0x6bc2,
    seg9:0x6cc4, seg9:0x2828 or seg9:0x6fee."""
    adds = [i for i in ins.writers(0x1E58, 0x1E5B) if i.mem.disp == 0x1E58 and i.mnem == "add"]
    assert {i.addr for i in adds} == {"seg4:0x2ef8", "seg7:0x196a", "seg8:0x6785", "seg8:0x67a6",
                                      "seg8:0x7971", "seg8:0x7c3b", "seg8:0x92cd", "seg13:0x1b4a",
                                      "seg13:0x1c32", "seg16:0x7228"}
    for f in ("seg9:0x6bc2", "seg9:0x6cc4", "seg9:0x2828", "seg9:0x6fee"):
        assert not [i for i in ins.writers(0x1E58, 0x1E5B) if ins.in_func(i, f)], f


def test_capture_sounds_and_messages(ins):
    """core.md Capturing an icon, Sound and Message: 0x23 without defenders (or kind 6), 0x26
    below 2 defenders, else 0x66; strings 574, 575, 2530 and 12530."""
    expect = {"seg13:0x1bba": "cmp word ptr [0x9602], 0",
              "seg13:0x1bcf": "cmp byte ptr es:[bx + si + 0x54], 0",
              "seg13:0x1bd6": "cmp word ptr [0x9320], 6", "seg13:0x1be5": "push 0x23",
              "seg13:0x1be9": "cmp word ptr [0x9602], 2"}
    assert _texts(ins, expect) == expect
    sound = [i for i in ins.insns if i.seg == 13 and 0x1BBA <= i.off <= 0x1C06 and i.mnem == "push"
             and i.ops[0] in ("0x23", "0x26", "0x66")]
    assert [i.ops[0] for i in sound] == ["0x23", "0x26", "0x66"]
    assert _eng(574) == "Good Icon Capture! %s Resource Gained."
    assert _eng(575) == "Good Icon Capture! %# Acquired."
    assert (_eng(2530), _eng(12530)) == ("Defender", "Defenders")


def test_virused_capture(ins):
    """core.md Capturing an icon, Virused icon: capturable 2, resources − 100 floored at 0,
    message code 0x48 (sound 0x67, string 652), +0x94 = now + 15000, DGROUP:0x8690 = now +
    7500, the nearest search from 20000; strings 1700/1701."""
    expect = {"seg13:0x18d7": "cmp byte ptr es:[bx + si + 0x53], 2", "seg13:0x18e9": "sub ax, 0x64",
              "seg13:0x18ec": "jns seg13:0x18f0", "seg13:0x18ee": "xor ax, ax",
              "seg13:0x18f4": "push 0x48", "seg13:0x19c7": "add ax, 0x3a98",
              "seg13:0x19d2": "mov word ptr [bx + 0x7124], ax", "seg13:0x19de": "add eax, 0x1d4c",
              "seg13:0x19e4": "mov dword ptr [0x8690], eax",
              "seg13:0x19ed": "mov word ptr [bp - 0x16], 0x4e20"}
    assert _texts(ins, expect) == expect
    assert (0x3A98, 0x1D4C, 0x4E20, 0x7124 - 0x7090) == (15000, 7500, 20000, 0x94)
    assert ins.switches["seg3:0x1351"]["targets"][0x48 - 1] == "seg3:0xfdd"
    case = {"seg3:0xfdd": "push 0x67", "seg3:0xfe7": "push 0x28c"}
    assert _texts(ins, case) == case
    assert _eng(0x28C) == "You're In Trouble! You Captured A Virused Icon!"
    assert (_eng(1700), _eng(1701)) == ("Out Of Control. That", "Icon Didn't Taste So Good!")


def test_fuel_pickup(ins):
    """core.md Other touches, Pool 4: fuel = min(tank, fuel + 250), seg13:0x4eec(ship, 30),
    sound 0x71 and pickup byte +0x51 = 10."""
    expect = {"seg13:0x1c8a": "mov ax, word ptr [bx - 0x6910]", "seg13:0x1c94": "add cx, 0xfa",
              "seg13:0x1c9e": "mov word ptr [bx + 0x70ae], ax", "seg13:0x1ca2": "push 0x1e",
              "seg13:0x1ca6": "call seg13:0x4eec", "seg13:0x1cb5": "push 0x71",
              "seg13:0x1cb7": "call far seg10:0x754a",
              "seg13:0x1cc3": "mov byte ptr es:[bx + di + 0x51], 0xa"}
    assert _texts(ins, expect) == expect
    assert 0x10000 - 0x6910 == 0x96F0                         # the design tank
    assert {"seg13:0x18c3", "seg13:0x1ca6", "seg11:0x528a", "seg11:0xacea"} <= \
        set(_callers(ins, "seg13:0x4eec"))


def test_icon_damage_cap(ins):
    """core.md Shooting an icon: seg7:0x233c skips while DGROUP:0x57a8 + 0x23·rec ≠ 0, adds
    3 × amount capped at 211 and shatters above 208."""
    expect = {"seg7:0x234b": "imul bx, word ptr [bp + 8], 0x23", "seg7:0x234f": "cmp word ptr [bx + 0x57a8], 0",
              "seg7:0x2375": "imul cx, word ptr [bp + 0xa], 3", "seg7:0x237d": "cmp cx, 0xd3",
              "seg7:0x2383": "mov cx, 0xd3", "seg7:0x23a5": "mov word ptr es:[bx + di + 0x1d2], cx",
              "seg7:0x24da": "cmp word ptr es:[bx + 0x1e], 0xd0"}
    assert _texts(ins, expect) == expect
    assert (0xD3, 0xD0) == (211, 208)
    assert _callers(ins, "seg7:0x5ae8") == ["seg7:0x2758", "seg7:0x5062", "seg9:0x6c22",
                                            "seg12:0x24e2", "seg13:0x18ac"]


def test_object_name_strings():
    """core.md Target validity, Names; structs.md Pool roles, Names: the ids and texts."""
    names = {152: "Icon ^N", 153: "Rock^N", 154: "Defender^N", 170: "Virus^N", 347: "Ambulance^N",
             585: "Exit Gate^N", 892: "Icon Fragment^N"}
    assert {s: _eng(s) for s in names} == names
    assert _eng(152)[:-2] == "Icon "                         # the last 2 characters cut
    assert [(s >> 4) + 1 for s in (152, 170, 347, 585, 892)] == [10, 11, 22, 37, 56]


# ------------------------------------------------------------------ M4 stage 0: weapons


def _weapon_stores(ins) -> dict[int, list]:
    vals = defaultdict(list)
    for i in ins.func_insns("seg10:0x1fb2"):
        if i.abs_ds and is_write(i) and 0x2F00 <= i.mem.disp < 0x3900:
            vals[i.mem.disp].append(ins.stored_value(i))
    return vals


def W(w: int, o: int) -> int:
    return 0x2F54 + 0x50 * w + o


def test_weapon_table_extent(ins):
    """structs.md Weapon records, globals.md DGROUP:0x2f54: seg10:0x1fb2 fills 792 word fields
    of 28 blocks of 0x50 bytes from DGROUP:0x2f54 up to DGROUP:0x380b; name id 186 + w."""
    vals = _weapon_stores(ins)
    assert len(vals) == 792
    assert min(vals) == 0x2F54 and max(vals) + 1 == 0x380B
    assert 0x2F54 + 28 * 0x50 == 0x3814 and 28 * 0x50 == 2240
    assert ins.at("seg10:0x1fbc").text == "mov word ptr [0x2f54], 0xba"
    assert [vals[W(w, 0)] for w in range(27)] == [[186 + w] for w in range(27)]
    assert (_eng(186), _eng(194), _eng(212)) == ("NO WEAPON", "RADAR-GUIDED MISSILES", "INNER FORCE")
    assert [(s >> 4) + 1 for s in (186, 191, 192, 207, 208, 212)] == [12, 12, 13, 13, 14, 14]


def test_weapon_8_record(ins):
    """structs.md Weapon records: weapon 8's 23, 1, −1, 0x172 at −8 … −2, 10 and 120 at
    +0x0c/+0x0e, 40 at +0x20 and sound 0x1c at +0x18; core.md Pool-6 mover, +0x3a = 1."""
    vals = _weapon_stores(ins)
    got = {o: vals[W(8, o)] for o in (-8, -6, -4, -2, 0x0C, 0x0E, 0x20, 0x18, 0x3A)}
    assert got == {-8: [23], -6: [1], -4: [0xFFFF], -2: [0x172], 0x0C: [10], 0x0E: [120],
                   0x20: [40], 0x18: [0x1C], 0x3A: [1]}
    sites = {"seg10:0x22d8": W(8, -8), "seg10:0x22f0": W(8, -6), "seg10:0x22f3": W(8, -2),
             "seg10:0x22ba": W(8, 0x0C), "seg10:0x22c0": W(8, 0x0E), "seg10:0x22cc": W(8, 0x20),
             "seg10:0x2308": W(8, 0x18)}
    assert {a: ins.at(a).mem.disp for a in sites} == sites


def test_weapon_shop_list(ins):
    """core.md The options dialog: with DGROUP:0x920c ≠ 3 the weapons whose +0x36 is −1 are
    4, 5, 7, 8, 10, 11, 14, 15, 16, 17 and 21 at 650 … 650; 22-24 and 26 cost 0."""
    vals = _weapon_stores(ins)
    listed = [w for w in range(1, 22) if 0xFFFF in vals[W(w, 0x36)]]
    assert listed == [4, 5, 7, 8, 10, 11, 14, 15, 16, 17, 21]
    assert [vals[W(w, 0x34)] for w in listed] == \
        [[p] for p in (650, 700, 800, 750, 450, 450, 900, 750, 500, 600, 650)]
    assert [vals[W(w, 0x34)] for w in (22, 23, 24, 26)] == [[0]] * 4


def test_weapon_system_switch(ins):
    """core.md The weapon system (seg11:0xac86): the 23-entry switch on w − 1 at seg11:0xadba."""
    sw = ins.switches["seg11:0xadb5"]
    assert (sw["table"], sw["count"], ins.at(sw["bound_at"]).text) == ("seg11:0xadba", 23, "cmp ax, 0x16")
    t = sw["targets"]
    case = {w: t[w - 1] for w in (1, 2, 3, 4, 5, 13, 15, 23)}
    assert case == {1: "seg11:0xad5a", 2: "seg11:0xad6b", 3: "seg11:0xad82", 4: "seg11:0xad4c",
                    5: "seg11:0xad38", 13: "seg11:0xad9e", 15: "seg11:0xad7e", 23: "seg11:0xad90"}
    assert {t[w - 1] for w in range(1, 24) if w not in case} == {"seg11:0xade8"}
    calls = {"seg11:0xad63": "seg11:0x8a42", "seg11:0xad76": "seg11:0x947a", "seg11:0xad8b": "seg11:0x9fdc",
             "seg11:0xad44": "seg13:0x1318", "seg11:0xada7": "seg11:0x9078", "seg11:0xad99": "seg11:0x9daa"}
    assert all(_calls(ins, calls).values())
    expect = {"seg11:0xad40": "push 0x1e", "seg11:0xad42": "push 5", "seg11:0xad56": "push 4",
              "seg11:0xad6b": "push 2", "seg11:0xad7e": "push 0xf"}
    assert _texts(ins, expect) == expect


# ------------------------------------------------------------------ M4 stage 0: fuel, the ambulance, messages


def test_ship_fuel_set_up(ins):
    """core.md Fuel, The words: without a roster record fuel = 40 × SHP byte +0 (8-bit imul)
    and resources = 100; a new game's roster record 0 gets +0x02 = 15 and +0x04 = 0."""
    expect = {"seg14:0x1b25": "mov al, 0x28", "seg14:0x1b27": "imul byte ptr es:[bx + si]",
              "seg14:0x1b2f": "mov word ptr [bx + 0x70ae], ax",
              "seg14:0x1b48": "mov word ptr [bx + 0x715e], 0x64",
              "seg14:0x3cc": "mov word ptr es:[si + 2], 0xf", "seg14:0x3d8": "mov word ptr es:[si + 4], 0"}
    assert _texts(ins, expect) == expect
    assert _callers(ins, "seg9:0x6db0") == ["seg9:0x6909", "seg9:0x6cf6"]


def test_fuel_consumption(ins):
    """core.md Consumption: thrust/−40 and 5·cloaked; cloaked subtracts 3 more and calls the
    cloak toggle and seg3:0x332e(0x04090418) below 120."""
    expect = {"seg11:0x1d43": "mov al, 5", "seg11:0x1d45": "imul byte ptr [bx + 0x7178]",
              "seg11:0x1d4b": "mov ax, word ptr [bx + 0x709c]", "seg11:0x1d4f": "mov dx, 0xffd8",
              "seg11:0x1c83": "sub word ptr [bx + 0x70ae], 3",
              "seg11:0x1c88": "cmp word ptr [bx + 0x70ae], 0x78",
              "seg11:0x1c92": "call far seg11:0x4c72", "seg11:0x1c98": "push 0x4090418",
              "seg11:0x1c9e": "call far seg3:0x332e"}
    assert _texts(ins, expect) == expect
    assert (s16(0xFFD8), 0x78) == (-40, 120)


def test_ambulance_call_and_dispatch(ins):
    """core.md Calling it (H): the reply codes 7 (string 110), 8 (348) and 132 (1136) with
    "no credit" below −300 and the refusal below −350; dispatch distance 1400."""
    t = ins.switches["seg3:0x1351"]["targets"]
    assert [t[c - 1] for c in (7, 8, 132, 9)] == ["seg3:0xcbb", "seg3:0xcde", "seg3:0xcce", "seg3:0xcee"]
    expect = {"seg3:0xcbb": "push 0x33", "seg3:0xcc4": "push 0x380c", "seg3:0xcc9": "push 0x6e",
              "seg3:0xcde": "push 0x2c", "seg3:0xce8": "push 0x15c", "seg3:0xcce": "push 0x2c",
              "seg3:0xcd8": "push 0x470", "seg3:0xcee": "push 0x2c", "seg3:0xcf8": "push 0x79",
              "seg11:0x6fcc": "cmp ax, 0xfed4", "seg11:0x6fd1": "push 0x84", "seg11:0x6fd6": "push 8",
              "seg16:0xbb3": "cmp ax, 0xfea2", "seg16:0xbb8": "cmp cx, 0xc8",
              "seg16:0xbef": "mov word ptr [0x9e4b], 0x578", "seg16:0xbf8": "mov word ptr [0x952e], ax",
              "seg16:0xbfb": "mov word ptr [0x9e49], 0"}
    assert _texts(ins, expect) == expect
    assert (s16(0xFED4), s16(0xFEA2), 0x578, 0x84) == (-300, -350, 1400, 132)
    assert _eng(110) == "Hold Your Horses, %s. Rescue Unit Dispatched. "
    assert _eng(348) == "Sorry! Ambulance Service Is Busy At This Time."
    assert _eng(1136) == "Sorry! You Have No Credit. Help Is Not Coming."
    assert _eng(121) == "FUEL LEVEL LOW. SEEK FUEL ASAP"
    assert "seg11:0x6fb0" in _callers(ins, "seg16:0xb4e")


def test_message_end_times(ins):
    """core.md Messages (seg3:0x6ec), globals.md DGROUP:0x870c: sender −1 loads string 2213;
    the end time is now + 7000, or in a race 2500 (pool 7) and 5000."""
    expect = {"seg3:0x77d": "cmp word ptr [bp + 8], -1", "seg3:0x787": "push 0x8a5",
              "seg3:0x7f3": "cmp word ptr [0x94e], 0", "seg3:0x7fa": "cmp word ptr [bp + 8], 7",
              "seg3:0x807": "add ax, 0x9c4", "seg3:0x813": "add ax, 0x1388", "seg3:0x81f": "add ax, 0x1b58",
              "seg3:0x825": "mov word ptr [0x870c], ax"}
    assert _texts(ins, expect) == expect
    assert (0x8A5, 0x9C4, 0x1388, 0x1B58) == (2213, 2500, 5000, 7000)
    assert _eng(2213) == "NO MESSAGE"


# ------------------------------------------------------------------ M4 stage 0: saved games (formats.md)


def test_save_writer_gates(ins):
    """formats.md Names and triggers, Writing: kind ≠ 0 needs a score of at least 1000; the
    trial shows string 2324 only for a manual save; OpenFile 0x1002, GlobalAlloc(0x42, 0x1a9),
    _lwrite chunks of 0x7fff."""
    expect = {"seg6:0x886b": "cmp word ptr [bp + 8], 0", "seg6:0x8871": "cmp word ptr [0x1e5a], 0",
              "seg6:0x8878": "jl seg6:0x8882", "seg6:0x887a": "cmp word ptr [0x1e58], 0x3e8",
              "seg6:0x8887": "cmp word ptr [0x920c], 0", "seg6:0x888e": "cmp word ptr [0x95bc], 0",
              "seg6:0x8895": "cmp word ptr [bp + 6], 0", "seg6:0x889b": "cmp word ptr [bp + 8], 0",
              "seg6:0x88a3": "push 0x914", "seg6:0x8a75": "push 0x1002",
              "seg6:0x8a86": "push 0x42", "seg6:0x8a8a": "push 0x1a9",
              "seg6:0x9e91": "cmp word ptr [bp + 6], 0x7fff", "seg6:0x9ea1": "push 0x7fff"}
    assert _texts(ins, expect) == expect
    assert all(_calls(ins, {"seg6:0x88aa": "seg16:0x68d6", "seg6:0x8a78": "OpenFile",
                            "seg6:0x8a8d": "GlobalAlloc", "seg6:0x9ea4": "_lwrite"}).values())
    assert (0x3E8, 0x914, 0x1002) == (1000, 2324, 0x1000 | 0x0002)       # OF_CREATE | OF_READWRITE
    assert _eng(2324) == "Game saving is a handy feature of the full version of Inner Space."
    assert _eng(2325) == "The restore game feature only works in the full version of the game."
    assert [(s >> 4) + 1 for s in (2324, 2325)] == [146, 146]


def test_save_call_sites(ins):
    """formats.md Names and triggers: the six call sites of seg6:0x885c(named, kind)."""
    assert _callers(ins, "seg6:0x885c") == ["seg6:0x5e39", "seg6:0x9e56", "seg7:0x687",
                                            "seg9:0x31f0", "seg10:0x8657", "seg16:0x7235"]
    args = {s: _imm_pushes(ins, s, 2) for s in ("seg6:0x5e39", "seg6:0x9e56", "seg7:0x687",
                                                 "seg9:0x31f0", "seg10:0x8657", "seg16:0x7235")}
    assert args == {"seg6:0x5e39": [1, 1], "seg6:0x9e56": [1, 0], "seg7:0x687": [0, 0],
                    "seg9:0x31f0": [0, 0], "seg10:0x8657": [1, 0], "seg16:0x7235": [1, 2]}
    expect = {"seg9:0x31d0": "cmp byte ptr [bx + 0x717c], 0", "seg9:0x31d7": "cmp word ptr [0x91f8], 0",
              "seg9:0x31dc": "jle seg9:0x31f8", "seg9:0x31de": "cmp word ptr [0x95bc], 0",
              "seg9:0x31e5": "cmp word ptr [0x9516], 0",
              "seg16:0x7228": "add dword ptr [0x1e58], 0xc350"}
    assert _texts(ins, expect) == expect
    assert 0xC350 == 50000
    assert _callers(ins, "seg6:0x7042") == ["seg6:0x88b5"]
    assert _callers(ins, "seg6:0x8776") == ["seg6:0x89dc"]
    assert _callers(ins, "seg6:0x930a") == ["seg11:0x7a8d", "seg15:0x5b91"]


def test_save_header_shape_credit_and_counts(ins):
    """formats.md Header: +0x15e = 100 … 103 by the system mean (> 70, > 45, > 27) over 3 or 4
    words by design word DGROUP:0x96fa; +0x162 = credit capped at 0x4fff6 in place, / 10;
    +0x15a/+0x15c from ship words +0xd2/+0xd0."""
    expect = {"seg6:0x913d": "cmp word ptr [bx - 0x6906], -1", "seg6:0x9144": "mov word ptr [bp - 8], 3",
              "seg6:0x914b": "mov word ptr [bp - 8], 4", "seg6:0x9164": "mov ax, word ptr [bx + 0x70b6]",
              "seg6:0x917d": "cmp ax, 0x46", "seg6:0x9185": "mov word ptr es:[bx + 0x15e], 0x64",
              "seg6:0x918e": "cmp ax, 0x2d", "seg6:0x9196": "mov word ptr es:[bx + 0x15e], 0x65",
              "seg6:0x919f": "cmp ax, 0x1b", "seg6:0x91a7": "mov word ptr es:[bx + 0x15e], 0x66",
              "seg6:0x91b3": "mov word ptr es:[bx + 0x15e], 0x67",
              "seg6:0x8fed": "cmp dx, 4", "seg6:0x8ff4": "cmp ax, 0xfff6", "seg6:0x8ff9": "mov dx, 4",
              "seg6:0x8fff": "mov word ptr [si + 0x716c], ax", "seg6:0x8fdd": "push 0xa",
              "seg6:0x9009": "call far seg5:0x110", "seg6:0x9011": "mov word ptr es:[bx + 0x162], ax",
              "seg6:0x8fd2": "mov ax, word ptr [si + 0x7162]",
              "seg6:0x8fd6": "mov word ptr es:[bx + 0x15a], ax", "seg6:0x8fc7": "mov word ptr es:[bx + 0x15c], ax"}
    assert _texts(ins, expect) == expect
    assert (0x10000 - 0x6906, 0x46, 0x2D, 0x1B) == (0x96FA, 70, 45, 27)
    assert [_eng(311 + k) for k in range(4)] == ["GOOD", "FAIR", "BAD", "SORRY"]
    assert _eng(3002) == "YOUR SHIP HAS %d.%d ICONS OF CREDIT AND IS IN %A SHAPE. "
    assert [(s >> 4) + 1 for s in (311, 3000, 3002)] == [20, 188, 188]


# ------------------------------------------------------------------ M4 stage 0: design words (formats.md, structs.md)


DESIGN_FLAG_TESTS = {
    # site: (bit test, design word it sets) -- formats.md MASTER.SHP and MASTER.BOD flag rows
    "seg14:0x1eb4": ("test byte ptr es:[bx + 9], 0x10", 0x9714),
    "seg14:0x1ed6": ("test byte ptr es:[bx + si + 9], 1", 0x9716),
    "seg14:0x1efd": ("test byte ptr es:[bx + si + 9], 0x20", 0x9716),
    "seg14:0x1fd6": ("test byte ptr es:[bx + si + 9], 8", 0x971C),
    "seg14:0x1ffd": ("test byte ptr es:[bx + si + 9], 4", 0x971E),
    "seg14:0x1f41": ("test byte ptr es:[bx + si + 0x20], 1", 0x9718),
    "seg14:0x1f9f": ("test byte ptr es:[bx + si + 0x20], 2", 0x9779),
    "seg14:0x1d55": ("test byte ptr es:[bx + di + 0x20], 8", 0x971A),
}


def test_design_flag_masks(ins):
    """formats.md MASTER.SHP +0x09 and MASTER.BOD flags, structs.md Design records: the bit
    tests of seg14:0x1b84 and the design words they set (0x01 clears the flame row and
    Body Thrust words, 0x20 ORs 0x20 into the flame row)."""
    assert {s: ins.at(s).text for s in DESIGN_FLAG_TESTS} == {s: t for s, (t, _) in DESIGN_FLAG_TESTS.items()}
    vals = {"seg14:0x1ee2": "mov word ptr [bx - 0x68ea], 0", "seg14:0x1eef": "mov word ptr [bx - 0x68ea], 1",
            "seg14:0x1f09": "or byte ptr [bx - 0x68ea], 0x20", "seg14:0x1f4d": "mov word ptr [bx - 0x68e8], 0",
            "seg14:0x1f5a": "mov word ptr [bx - 0x68e8], 1", "seg14:0x1fab": "mov word ptr [bx - 0x6887], 1",
            "seg14:0x1fe2": "mov word ptr [bx - 0x68e4], 1", "seg14:0x1fef": "mov word ptr [bx - 0x68e4], 0",
            "seg14:0x2009": "mov word ptr [bx - 0x68e2], 1", "seg14:0x1d5c": "mov word ptr [si - 0x68e6], 1",
            "seg14:0x1ebb": "mov word ptr [si - 0x68ec], 1"}
    assert _texts(ins, vals) == vals
    assert [0x10000 - d for d in (0x68EC, 0x68EA, 0x68E8, 0x68E6, 0x68E4, 0x68E2, 0x6887)] == \
        [0x9714, 0x9716, 0x9718, 0x971A, 0x971C, 0x971E, 0x9779]
    assert [d - 0x96D2 for d in (0x9714, 0x9716, 0x9718, 0x971A, 0x971C, 0x971E, 0x9779)] == \
        [0x42, 0x44, 0x46, 0x48, 0x4A, 0x4C, 0xA7]


def test_design_words_copied_to_ship_bytes(ins):
    """structs.md ship +0xe4, +0xe7, +0xe9 and formats.md: seg14 copies design +0x46, +0x4a and
    +0xa7 to ship bytes +0xe4, +0xe7 and +0xe9."""
    expect = {"seg14:0x21da": "mov al, byte ptr [si - 0x68e8]", "seg14:0x21de": "mov byte ptr [di + 0x7174], al",
              "seg14:0x21ea": "mov al, byte ptr [si - 0x68e4]", "seg14:0x21ee": "mov byte ptr [di + 0x7177], al",
              "seg14:0x21f2": "mov al, byte ptr [si - 0x6887]", "seg14:0x21f6": "mov byte ptr [di + 0x7179], al"}
    assert _texts(ins, expect) == expect
    assert [d - 0x7090 for d in (0x7174, 0x7177, 0x7179)] == [0xE4, 0xE7, 0xE9]


# ------------------------------------------------------------------ M5 stages 1-2: ordinals 2 and 18 (ispace.md)


def test_ordinal_18_tables(isp):
    """ispace.md 18, Rotation: T1 = ISPACE.DLL DGROUP:0xf6, T2 = DGROUP:0x108; ordinal 2's S =
    DGROUP:0x11a and C = DGROUP:0x13a (±512·sin/cos)."""
    d = unpacked_seg("ispace", 11)
    h = lambda o, n: list(struct.unpack_from(f"<{n}h", d, o))
    assert h(0xF6, 5) == [0, 196, 362, 473, 512]
    assert h(0x108, 5) == [-512, -473, -362, -196, 0]
    assert h(0x11A, 16) == [0, 196, 362, 473, 512, 473, 362, 196, 0, -196, -362, -473, -512, -473, -362, -196]
    assert h(0x13A, 16) == [-512, -473, -362, -196, 0, 196, 362, 473, 512, 473, 362, 196, 0, -196, -362, -473]
    import math
    assert h(0x11A, 16) == [round(512 * math.sin(2 * math.pi * d / 16)) for d in range(16)]


def test_ordinal_18_entry_offsets_and_load(isp):
    """ispace.md 18, Rotation and Load: the (offx, offy) stack words (0, 0), (−1, 2), (1, 1),
    (1, −1), (0, −1); graphic resource 0x118 = 280 with schemes 12, 12; `retf 0xc`."""
    words = {}
    val = None
    for i in isp.func_insns("seg6:0x5e76")[:30]:
        if i.mnem == "mov" and i.ops[0] in ("ax", "cx") and _IMM_RE.fullmatch(i.ops[1]):
            val = s16(int(i.ops[1], 0) & 0xFFFF)
        elif i.mnem == "xor" and i.ops == ("ax", "ax"):
            val = 0
        elif i.mnem == "mov" and i.mem is not None and i.mem.regs == ("bp",):
            src = i.ops[1]
            words[s16(i.mem.disp)] = s16(int(src, 0) & 0xFFFF) if _IMM_RE.fullmatch(src) else val
    pairs = [(words[-0x15A + 4 * d], words[-0x158 + 4 * d]) for d in range(5)]
    assert pairs == [(0, 0), (-1, 2), (1, 1), (1, -1), (0, -1)]
    expect = {"seg6:0x5f38": "push 0x118", "seg6:0x5f3c": "push 0x15e", "seg6:0x5f41": "push 0xc",
              "seg6:0x5f43": "push 0xc", "seg6:0x5ebd": "cmp word ptr [0x1e9a], 8"}
    assert _texts(isp, expect) == expect
    assert 0x118 == 280
    rets = {i.text for i in isp.func_insns("seg6:0x5e76") if i.mnem == "retf"}
    assert rets == {"retf 0xc"}


def test_ordinal_18_ramp_records(isp):
    """ispace.md 18, Lower rows: the ramp record of scheme s is the word pair DGROUP:0x1398 /
    DGROUP:0x139a at 4·(19·mode + DGROUP:0x1dea[25·mode + s]); lo and span are trunc(/4), and
    the lower value is (v − lo1)·span0 / span1 + lo0, 0 for v = 0."""
    expect = {"seg6:0x700d": "imul bx, word ptr [bp - 0x124], 0x32",
              "seg6:0x7012": "imul si, word ptr [bp - 0x124], 0x13",
              "seg6:0x7019": "add si, word ptr [bx + 0x1dec]", "seg6:0x701d": "shl si, 2",
              "seg6:0x7022": "mov ax, word ptr [si + 0x139a]", "seg6:0x7026": "sub ax, word ptr [si + 0x1398]",
              "seg6:0x702d": "and dx, 3", "seg6:0x7032": "sar ax, 2",
              "seg6:0x7035": "mov word ptr [bp - 0x11e], ax",
              "seg6:0x7039": "mov bx, word ptr [bx + 0x1dea]", "seg6:0x7053": "mov word ptr [bp - 0x120], ax",
              "seg6:0x7064": "mov word ptr [bp - 0x11a], ax", "seg6:0x7075": "mov word ptr [bp - 0x11c], ax",
              "seg6:0x71c4": "cmp word ptr [bp - 0xc6], 0", "seg6:0x71cf": "sub ax, word ptr [bp - 0x11a]",
              "seg6:0x71d3": "imul word ptr [bp - 0x120]", "seg6:0x71d8": "idiv word ptr [bp - 0x11e]",
              "seg6:0x71dc": "add ax, word ptr [bp - 0x11c]", "seg6:0x71e6": "mov word ptr [bp - 0x122], 0"}
    assert _texts(isp, expect) == expect
    assert (0x32 // 2, 0x13, 0x1DEC - 0x1DEA) == (25, 19, 2)     # scheme 1 is the next word


def test_ordinal_18_wrapper(ins):
    """ispace.md 18, Arguments; world.md The thrust flame item 1: seg10:0x7032 pushes
    (DGROUP:0x920e, 1, 1, arg 1, 5, arg 3) through DGROUP:0x3c3c, and seg10:0x5c42 calls
    it as seg10:0x7032(0, 0x10, 0)."""
    expect = {"seg10:0x7041": "push word ptr [0x920e]", "seg10:0x7045": "push 1", "seg10:0x7047": "push 1",
              "seg10:0x7049": "push word ptr [bp + 6]", "seg10:0x704c": "push 5",
              "seg10:0x704e": "push word ptr [bp + 0xa]", "seg10:0x7051": "lcall [0x3c3c]",
              "seg10:0x5c79": "push 0", "seg10:0x5c7b": "push 0x10", "seg10:0x5c7d": "push 0",
              "seg10:0x5c80": "call seg10:0x7032", "seg10:0x5c86": "mov word ptr [0x3c60], ax"}
    assert _texts(ins, expect) == expect
    assert _callers(ins, "seg10:0x7032") == ["seg10:0x5c80", "seg15:0x21ca", "seg15:0x2be1"]


def test_ordinal_2_gap_and_directions(isp):
    """ispace.md 2, The renderer: a gap of 99 becomes 5 for an odd h and 6 for an even h; the
    direction count DGROUP:0x1ccc is 1 for w2 = −100, 4 for w5 ≠ 0 and 16 otherwise."""
    expect = {"seg6:0x2ae": "mov word ptr [0x1c72], ax", "seg6:0x2b1": "cmp word ptr [bp + 0x14], 0x63",
              "seg6:0x2b7": "test al, 1", "seg6:0x2bb": "mov word ptr [bp + 0x14], 5",
              "seg6:0x2c2": "mov word ptr [bp + 0x14], 6", "seg6:0x2f9": "cmp word ptr [bp + 0x14], -0x64",
              "seg6:0x2ff": "mov word ptr [0x1ccc], 1", "seg6:0x305": "mov word ptr [bp + 0x14], 0",
              "seg6:0x314": "cmp word ptr [bp + 0xe], 0", "seg6:0x31a": "mov word ptr [0x1ccc], 4",
              "seg6:0x322": "mov word ptr [0x1ccc], 0x10"}
    assert _texts(isp, expect) == expect


def test_ordinal_41_time_out(isp, ins):
    """ispace.md Game port, Time-out; world.md joystick Read: ordinal 41 returns 0xa7 after a
    MessageBeep when the read returns 0, and INSPACE seg11:0x72ce then clears DGROUP:0x8ff4."""
    expect = {"seg9:0x3ebf": "or ax, ax", "seg9:0x3ec1": "jne seg9:0x3ecf",
              "seg9:0x3ec9": "mov ax, 0xa7", "seg9:0x4079": "xor ax, ax",
              "seg9:0x408a": "cmp word ptr [0x22a4], 3"}
    assert _texts(isp, expect) == expect
    assert "MessageBeep" in isp.at("seg9:0x3ec4").text
    inspace = {"seg11:0x72c2": "cmp byte ptr [bx + 0x7172], 0", "seg11:0x72ce": "cmp ax, 0xa7",
               "seg11:0x72d3": "mov word ptr [0x8ff4], 0"}
    assert _texts(ins, inspace) == inspace


def test_ship_sheet_call(ins):
    """ispace.md Ship sheets: seg14:0x11a8 calls seg10:0x6ee6 with mode 0/2 by byte +0xe4 and
    1 for +0xe9, design bytes DGROUP:0x96ec/0x96ea, (0, 1), words DGROUP:0x977b/0x977d, 99,
    1, DS:0x9756 + 0xad·n, word DGROUP:0x9708; the gap after is 5 for an odd size, else 6."""
    expect = {"seg14:0x1245": "cmp byte ptr [bx + 0x7174], 0", "seg14:0x124c": "mov byte ptr [bp - 0x12], 0",
              "seg14:0x1252": "mov byte ptr [bp - 0x12], 2", "seg14:0x125a": "cmp byte ptr [bx + 0x7179], 0",
              "seg14:0x1261": "mov byte ptr [bp - 0x12], 1", "seg14:0x1265": "cmp word ptr [0x90f4], 1",
              "seg14:0x126c": "cmp dword ptr [0x1e68], 0", "seg14:0x1274": "cmp word ptr [0x931e], 1",
              "seg14:0x1282": "cmp word ptr [0x931e], 2",
              "seg14:0x12a8": "mov al, byte ptr [bx - 0x6914]", "seg14:0x12ad": "mov al, byte ptr [bx - 0x6916]",
              "seg14:0x12b2": "push 0", "seg14:0x12b4": "push 1", "seg14:0x12b6": "push dword ptr [bx - 0x6885]",
              "seg14:0x12bb": "push 0x63", "seg14:0x12bd": "push 1", "seg14:0x12c1": "add bx, 0x9756",
              "seg14:0x12c9": "push word ptr [bx - 0x68f8]", "seg14:0x12d0": "call far seg10:0x6ee6",
              "seg14:0x12d8": "call far seg10:0x86ca", "seg14:0x12e0": "test al, 1",
              "seg14:0x12e4": "mov word ptr [bp - 0xc], 5", "seg14:0x12eb": "mov word ptr [bp - 0xc], 6"}
    assert _texts(ins, expect) == expect
    assert [0x10000 - d for d in (0x6914, 0x6916, 0x6885, 0x68F8)] == [0x96EC, 0x96EA, 0x977B, 0x9708]


# ------------------------------------------------------------------ M5 stages 1-2: the thrust flame (world.md)


def _anchor(k: int, size: int, d2a: int, d04: int) -> tuple[int, int]:
    """world.md The thrust flame item 2: the anchor of heading k (shifts are sar)."""
    cx = [0, -98, -181, -236, -256][k]
    cy = [-256, -236, -181, -98, 0][k]
    half = (d2a << size) >> 1
    r = 2 * (d04 + 1) - (1 if k in (0, 3, 4) else 0) - (1 if k == 0 else 0)
    wx, wy = [(9, 9), (7, 9), (9, 9), (9, 10 if d2a > 0x1D else 9), (8 if size >= 2 else 6, 9)][k]
    sh = 2 - size
    return ((_trunc(cx * r, 256) >> sh) - (wx >> sh) + half // 2,
            half // 2 - (_trunc(cy * r, 256) >> sh) - (wy >> sh))


def test_flame_anchor_tables(ins, dgroup):
    """world.md The thrust flame item 2: Cx = DGROUP:0x6d6, Cy = DGROUP:0x6e0, and the
    TRANSFORMER's anchors at Size 1 with design +0x2a = 32 or 33 and +0x04 = 11; seg10:0x16ba
    has the one caller seg10:0x5e30, in the strip seg10:0x5d3c."""
    assert list(struct.unpack_from("<5h", dgroup, 0x6D6)) == [0, -98, -181, -236, -256]
    assert list(struct.unpack_from("<5h", dgroup, 0x6E0)) == [-256, -236, -181, -98, 0]
    for d2a in (32, 33):
        assert [_anchor(k, 1, d2a, 11) for k in range(5)] == [(12, 23), (8, 23), (4, 20), (1, 15), (1, 12)]
    assert _callers(ins, "seg10:0x16ba") == ["seg10:0x5e30"]
    assert ins.owners(*A("seg10:0x5e30")) == {"seg10:0x5d3c"}
    assert ins.at("seg10:0x5e23").text == "cmp byte ptr [bx + 0x7174], 0"


def test_flame_cell_size(ins):
    """world.md The thrust flame: f = (9 << Size) >> 1, or Size·6 + 3 while DGROUP:0x9130 ≠ 0,
    in the paste seg11:0xb09c and the strip seg10:0x5d3c."""
    expect = {"seg11:0xb12b": "cmp word ptr [0x9130], 0", "seg11:0xb132": "imul ax, word ptr [0x9156], 6",
              "seg11:0xb137": "add ax, 3", "seg11:0xb13c": "mov cl, byte ptr [0x9156]",
              "seg11:0xb140": "mov ax, 9", "seg11:0xb143": "shl ax, cl", "seg11:0xb145": "sar ax, 1",
              "seg10:0x5d5c": "imul ax, word ptr [0x9156], 6", "seg10:0x5d6e": "mov cl, byte ptr [0x9156]"}
    assert _texts(ins, expect) == expect
    assert [(9 << s) >> 1 for s in range(3)] == [4, 9, 18]


def test_flame_paste_and_callers(ins):
    """world.md The thrust flame items 4 and 5, structs.md object +0x2f: seg11:0xb09c (retf 4)
    stores thrust word +0x0c / 20 in +0x2f when on ≠ 0, else 0; the ten call sites."""
    expect = {"seg11:0xb0db": "cmp word ptr [bp + 6], 0", "seg11:0xb0e1": "mov ax, word ptr [bx + 0x709c]",
              "seg11:0xb0e5": "mov cx, 0x14", "seg11:0xb0eb": "mov byte ptr es:[si + 0x2f], al",
              "seg11:0xb100": "mov byte ptr es:[bx + si + 0x2f], 0",
              "seg11:0xb10a": "cmp word ptr [bx + 0x70a0], 0", "seg11:0xb111": "cmp word ptr [bx + 0x70a0], 7",
              "seg11:0xb302": "retf 4"}
    assert _texts(ins, expect) == expect
    assert _callers(ins, "seg11:0xb09c") == [
        "seg9:0x2e69", "seg11:0x4c50", "seg11:0x6039", "seg11:0x641f", "seg11:0x68f3", "seg13:0x3d6",
        "seg17:0x22fb", "seg17:0x25f2", "seg17:0x2a32", "seg17:0x2b3e"]
    assert 0x709C - 0x7090 == 0x0C and 0x70A0 - 0x7090 == 0x10


def test_flame_strip_and_sheet_callers(ins):
    """world.md Ship sheets and The thrust flame: the set-up seg14:0x0 calls the strip
    seg10:0x5d3c at seg14:0xc7e; the flame cells seg10:0x5c42 run from seg10:0x4b."""
    assert "seg14:0xc7e" in _callers(ins, "seg10:0x5d3c")
    assert ins.owners(*A("seg14:0xc7e")) == {"seg14:0x0"}
    assert "seg10:0x4b" in _callers(ins, "seg10:0x5c42")
    assert ins.owners(*A("seg10:0x4b")) == {"seg10:0x0"}
    assert _callers(ins, "seg14:0x11a8") == ["seg10:0x9e10", "seg16:0x362b", "seg16:0x5ff7"]


def test_entry_time_swap(ins):
    """world.md Entry-time swap: seg8:0x4d94 (retf 8) swaps two dwords through far pointers
    and has the one caller seg9:0x64a7."""
    assert _callers(ins, "seg8:0x4d94") == ["seg9:0x64a7"]
    assert {i.text for i in ins.func_insns("seg8:0x4d94") if i.mnem == "retf"} == {"retf 8"}
    expect = {"seg8:0x4da1": "les bx, ptr [bp + 0xa]", "seg8:0x4db1": "les bx, ptr [bp + 6]"}
    assert _texts(ins, expect) == expect


# ------------------------------------------------------------------ M3 stage 4: gates, stars, links, clip (world.md)


def test_gate_post_fields(ins):
    """world.md Post fields: +0x24 = (6 << Size) >> 1, +0x2f = 2, +0x54 = 1, DGROUP:0x3d80 +
    0x34·k = −1, +0x44 = 3500, +0x22 = (34 << Size) >> 1, +0x20 = 272, hit_sound 8,
    radar_brush 1, radar_draw offset 0x39d8, on_damage offset 0x5090, kind word 3."""
    expect = {"seg9:0x11b0": "mov ax, 6", "seg9:0x11b3": "shl ax, cl", "seg9:0x11b5": "sar ax, 1",
              "seg9:0x11b7": "mov word ptr es:[bx + si + 0x24], ax",
              "seg9:0x11bf": "mov byte ptr es:[bx + si + 0x2f], 2", "seg9:0x11c8": "mov byte ptr es:[bx + si + 0x54], 1",
              "seg9:0x11db": "mov word ptr [bx + 0x3d80], 0xffff",
              "seg9:0x11e7": "mov word ptr es:[bx + si + 0x44], 0xdac", "seg9:0x11f5": "mov dx, 0x22",
              "seg9:0x11fc": "mov word ptr es:[bx + si + 0x22], dx",
              "seg9:0x1204": "mov word ptr es:[bx + si + 0x20], 0x110",
              "seg9:0x120e": "mov word ptr es:[bx + si + 0x42], 8",
              "seg9:0x1218": "mov word ptr es:[bx + si + 0x46], 1",
              "seg9:0x1222": "mov word ptr es:[bx + si + 0x62], 0x39d8",
              "seg9:0x1232": "mov word ptr es:[bx + si + 0x5e], 0x5090",
              "seg9:0x1240": "mov word ptr [bx + 0x3d6c], 3"}
    assert _texts(ins, expect) == expect
    assert (0xDAC, 0x22, 0x110) == (3500, 34, 272)


def test_gate_test_constants(ins):
    """world.md Gate test seg12:0x1f7a: the duel check, pool 7 and mass 0xdac excluded, a ship
    only after +0x88 + 300 (player) or + 3000, and byte +0x2e ≠ 1; its one caller seg12:0x1dca."""
    func = {i.text for i in ins.func_insns("seg12:0x1f7a")}
    for t in ("cmp word ptr [0x9320], 7", "add ax, 0xbb8", "cmp byte ptr [bx + 0x507c], 7",
              "cmp word ptr es:[bx + si + 0x44], 0xdac", "cmp byte ptr es:[bx + si + 0x2e], 1",
              "add ax, 0x12c", "mov ax, word ptr [bx + 0x7118]"):
        assert t in func, t
    assert (0x12C, 0xBB8, 0x7118 - 0x7090) == (300, 3000, 0x88)
    assert _callers(ins, "seg12:0x1f7a") == ["seg12:0x1dca"]
    expect = {"seg12:0x63ca": "call seg12:0x1776", "seg12:0x6403": "call seg12:0x390e",
              "seg12:0x63b0": "call seg12:0x5636"}
    assert _texts(ins, expect) == expect


def test_gate_and_beam_brushes(ins):
    """world.md Gate images, Brushes: seg10:0x4b2e's beam brushes DGROUP:0x3b8a … 0x3b92 and
    heat brushes DGROUP:0x3b76 … 0x3b86, all plain RGB."""
    beam = _brushes(ins, {"seg10:0x4d48": 0x3B8A, "seg10:0x4d56": 0x3B8C, "seg10:0x4d64": 0x3B8E,
                          "seg10:0x4d72": 0x3B90, "seg10:0x4d7f": 0x3B92})
    assert list(beam.values()) == [0x007F0000, 0x00C47F00, 0x00FFC400, 0x00FFFF00, 0x00FFFFFF]
    assert [(d - 0x3B88) // 2 for d in beam] == [1, 2, 3, 4, 5]
    heat = _brushes(ins, {"seg10:0x4b6c": 0x3B86, "seg10:0x4b79": 0x3B84, "seg10:0x4b86": 0x3B82,
                          "seg10:0x4b93": 0x3B80, "seg10:0x4ba0": 0x3B7E, "seg10:0x4bad": 0x3B7C,
                          "seg10:0x4bba": 0x3B7A})
    assert {(d - 0x3B74) // 2: v for d, v in heat.items()} == {
        9: 0x000072FF, 8: 0x000060FF, 7: 0x000040EA, 6: 0x000030E0, 5: 0x000020D0, 4: 0x001020C0,
        3: 0x002020B0}
    b = beam[0x3B90]                                       # beam index 4 is the oracle's 00ffff
    assert "%02x%02x%02x" % (b & 0xFF, (b >> 8) & 0xFF, (b >> 16) & 0xFF) == "00ffff"


def test_star_brushes_and_counts(ins):
    """world.md Stars, Colours and Count; core.md The icon after capture: brushes 10-14 are
    PALETTERGB 0x02FFFF00 … 0x02826400; Detail 0-3 give 4, 7, 13, 18 stars and the dissolve
    step 3, 2, 1, 1."""
    stars = _brushes(ins, {"seg10:0x4fe0": 0x3B60, "seg10:0x4fee": 0x3B62, "seg10:0x4ffc": 0x3B64,
                           "seg10:0x500a": 0x3B66})
    assert list(stars.values()) == [0x02FFFF00, 0x02E6D700, 0x02CDAF00, 0x02A08200]
    assert ins.at("seg10:0x5012").text == "push 0x2826400"
    expect = {"seg6:0xe36": "mov ax, word ptr [0x9208]", "seg6:0xe3b": "mov word ptr [0x9154], 4",
              "seg6:0xe41": "mov word ptr [0x8ffe], 3", "seg6:0xe49": "mov word ptr [0x9154], 7",
              "seg6:0xe4f": "mov word ptr [0x8ffe], 2", "seg6:0xe57": "mov word ptr [0x9154], 0xd",
              "seg6:0xe5d": "mov word ptr [0x8ffe], 1", "seg6:0xe65": "mov word ptr [0x9154], 0x12",
              "seg6:0xe6b": "jmp seg6:0xe5d", "seg6:0xe6f": "je seg6:0xe3b", "seg6:0xe72": "je seg6:0xe49",
              "seg6:0xe75": "je seg6:0xe57"}
    assert _texts(ins, expect) == expect
    assert _callers(ins, "seg8:0x158a") == ["seg7:0xb87"]


def test_gate_image_slots(ins, dgroup):
    """world.md Gate images, Slots and Sources: c = 34, g = 6 written to pool-8 record 0 +0x22
    and +0x24; slot 2 takes BW_BLOCKS and BW_MBLOCKS, the exit sign OUT_SIGN; three callers."""
    expect = {"seg10:0x3eb1": "cmp word ptr [0x39ea], 0", "seg10:0x40ca": "mov word ptr es:[bx + 0x24], 6",
              "seg10:0x40db": "mov ax, 0x22", "seg10:0x40de": "mov word ptr es:[bx + 0x22], ax",
              "seg10:0x3ec3": "push 0x74b", "seg10:0x3ed1": "push 0x755"}
    assert _texts(ins, expect) == expect
    assert [_cstr(dgroup, o) for o in (0x74B, 0x755, 0x760)] == [b"BW_BLOCKS", b"BW_MBLOCKS", b"OUT_SIGN"]
    c, g = 0x22, 6
    assert (4 * c + 5 * g, 2 * g + c) == (166, 46)
    assert _callers(ins, "seg10:0x3ea2") == ["seg9:0x26e5", "seg10:0x958b", "seg17:0xf17"]


def test_link_line_slots(ins):
    """world.md Link lines (seg12:0x3778): DGROUP:0x9300 has the one writer seg9:0x2508; the
    two callers seg7:0xbed and seg7:0x19d6; the slots of 0x20 bytes start at DGROUP:0x65cc."""
    w = ins.writers(0x9300, 0x9301)
    assert [i.addr for i in w] == ["seg9:0x2508"]
    assert _callers(ins, "seg12:0x3778") == ["seg7:0xbed", "seg7:0x19d6"]
    assert 0x65D0 - 0x65CC == 4 and 0x65DC - 0x65CC == 0x10


def test_play_rectangle(ins):
    """world.md Clip region: seg2:0xca4 with Border on sets left = 78 − (58 − W % 58)/2, top
    78, right W − left, bottom H − 20 (+20 more with PlayMode ≠ 0); (68, 78, 956, 663) at
    1024 × 683."""
    expect = {"seg2:0xcb1": "cmp word ptr [0xa78], 0", "seg2:0xcbb": "mov cx, 0x3a",
              "seg2:0xcca": "sub ax, 0x4e", "seg2:0xcd2": "mov word ptr [bp - 4], 0x4e",
              "seg2:0xce8": "sub ax, 0x28", "seg2:0xcee": "mov ax, 0x14",
              "seg2:0xd1b": "cmp word ptr [0x9134], 0", "seg2:0xd22": "add word ptr [bp - 8], 0x14",
              "seg2:0xd3d": "push 0x1d30"}
    assert _texts(ins, expect) == expect
    W_, H_ = 1024, 683
    off = _trunc(0x3A - W_ % 0x3A, 2)
    left = 0x4E - off
    assert (left, 0x4E, W_ - left, H_ - 0x14) == (68, 78, 956, 663)
