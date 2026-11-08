"""Mechanical facts of the M4 stage 5 readings in docs/re/{core,world,ispace,structs,globals,
rng,sound,formats,timing,pitfalls}.md, re/artifacts/sounds.tsv and docs/decisions/0016.

The listing model, the fixtures and the helpers come from test_re_facts.py and
test_re_facts_core.py; this file follows their conventions (references, writers,
``segN:0xOFF`` addresses) and skips when ``work/`` or ``original/`` is absent. Each test
names the doc file and the section it pins.
"""
from __future__ import annotations

import csv
import struct

from test_re_facts import (  # noqa: F401  (ins, isp, dgroup and master are pytest fixtures)
    A, SOUNDS_TSV, _calls, _dib, _eng, _resource, _texts, dgroup, ins, is_write, isp, master, s16,
    unpacked_seg,
)
from test_re_facts_core import _brushes, _callers, _imm_pushes

_FLOW = ("jmp", "call", "lcall", "ret", "retf", "iret", "loop")


def _push_order(m, call: str, n: int) -> list:
    """The immediates of the ``n`` pushes before ``call`` in push order (None for a non-immediate)."""
    return list(reversed(_imm_pushes(m, call, n)))


def _reg_value(m, i, reg: str):
    """The constant in ``reg`` at instruction ``i``: the nearest earlier ``mov reg, imm`` or
    ``xor reg, reg`` in straight-line code (no flow instruction between), else None."""
    cur = i
    while True:
        cur = m.step(cur, -1)
        if cur.mnem.startswith("j") or cur.mnem in _FLOW:
            return None
        if cur.ops and cur.ops[0] == reg:
            if cur.mnem == "mov" and cur.ops[1].lstrip("-").replace("0x", "").isalnum():
                try:
                    return int(cur.ops[1], 0) & 0xFFFF
                except ValueError:
                    return None
            if cur.mnem == "xor" and cur.ops[1] == reg:
                return 0
            return None


def _store(m, site: str) -> tuple[int, int]:
    """(DGROUP displacement, stored word) of ``mov word ptr [disp], src`` at ``site``."""
    i = m.at(site)
    assert i.mnem == "mov" and i.abs_ds, i.text
    src = i.ops[1]
    try:
        v = int(src, 0) & 0xFFFF
    except ValueError:
        v = _reg_value(m, i, src)
    return i.mem.disp, v


def _writers(m, lo: int, hi: int | None = None) -> list[str]:
    return sorted({i.addr for i in m.writers(lo, lo if hi is None else hi)}, key=A)


def _disp_stores(m, disp: int) -> list[str]:
    """Every ``mov`` store whose memory displacement is ``disp``, indexed or not."""
    return sorted({i.addr for i in m.insns if i.mem is not None and (i.mem.disp & 0xFFFF) == disp
                   and i.mnem == "mov" and is_write(i)}, key=A)


# ------------------------------------------------------------------ core.md Wings torn off


def test_wing_tear_cut_flag_and_ordinal_15_calls(ins):
    """core.md Wings torn off: `cut` [bp−4] = 1 for side 0; the two ordinal-15 wrapper calls;
    seg10:0x7370 pushes DGROUP:0x13dc, args 1-5, DGROUP:0x920e, args 6-7 and calls DGROUP:0x3c44."""
    expect = {"seg8:0x2cea": "mov dword ptr [bp - 6], eax", "seg8:0x2d0e": "mov byte ptr [bx + 0x713c], al",
              "seg8:0x2d12": "mov word ptr [bp - 4], 1", "seg8:0x2d1e": "mov byte ptr [bx + 0x713d], 0",
              "seg8:0x30d4": "call far seg10:0x7370", "seg8:0x347d": "call far seg10:0x7370",
              "seg10:0x737d": "push word ptr [0x13dc]", "seg10:0x7381": "push word ptr [bp + 6]",
              "seg10:0x738d": "push word ptr [bp + 0xe]", "seg10:0x7390": "push word ptr [0x920e]",
              "seg10:0x7394": "push word ptr [bp + 0x10]", "seg10:0x7397": "push word ptr [bp + 0x12]",
              "seg10:0x739a": "lcall [0x3c44]"}
    assert _texts(ins, expect) == expect
    assert ins.call_sites("seg10:0x7370") == ["seg8:0x30d4", "seg8:0x347d"]


def test_band_half_width_writers(ins):
    """core.md Wings torn off, structs.md Design records +0x3c: three stores of design word
    +0x3c (DGROUP:0x970e + 0xad·n), at seg8:0x2d8d, seg14:0x208b and seg18:0x3307."""
    sites = sorted({i.addr for i in ins.insns
                    if i.mem is not None and i.mem.disp in (0x970E, (0x970E - 0x10000)) and is_write(i)
                    and i.mnem == "mov"}, key=A)
    assert sites == ["seg8:0x2d8d", "seg14:0x208b", "seg18:0x3307"]


def test_debris_offset_table(dgroup):
    """core.md Wings torn off, Placement: the 16 word pairs at DGROUP:0x66e (128 × the heading's
    unit vector, clockwise from up)."""
    t = struct.unpack_from("<32h", dgroup, 0x66E)
    pairs = list(zip(t[0::2], t[1::2]))
    assert pairs == [(0, -128), (48, -110), (91, -91), (110, -48), (128, 0), (110, 48), (91, 91),
                     (48, 110), (0, 128), (-48, 110), (-91, 91), (-110, 48), (-128, 0), (-110, -48),
                     (-91, -91), (-48, -110)]


def test_debris_strip_fields(ins):
    """structs.md Other wave tables, Debris: the stores of +0x10, +0x14, +0x18, +0x12 and +0x16
    (DGROUP:0x916c … 0x9174) in seg8:0x2cd8 and the extent +0x1e at seg8:0x2f3b."""
    want = {"seg8:0x3133": 0x916C, "seg8:0x3141": 0x9170, "seg8:0x315b": 0x9174, "seg8:0x3361": 0x916E,
            "seg8:0x3372": 0x9170, "seg8:0x338d": 0x9172}
    for site, disp in want.items():
        i = ins.at(site)
        assert i.mnem == "mov" and i.mem is not None and (i.mem.disp & 0xFFFF) == disp, (site, i.text)
    assert ins.at("seg8:0x33f4").text == "mov word ptr [bp - 0xc], 0x100"


def test_wing_tear_palette_word(ins):
    """ADR 0016 Context: seg8:0x2cd8's [bp − 0x1a] has one writer, seg8:0x310c, and seg8:0x33a1
    tests it."""
    w = [i.addr for i in ins.func_insns("seg8:0x2cd8")
         if i.mem is not None and i.mem.regs == ("bp",) and i.mem.disp == 0xFFE6 and is_write(i)]
    assert w == ["seg8:0x310c"]
    assert ins.at("seg8:0x33a1").text == "cmp word ptr [bp - 0x1a], 0"


# ------------------------------------------------------------------ ispace.md ordinal 15


def test_ordinal_15_mask(isp):
    """ispace.md 15, The mask: CreateBitmap(450, 90, 1, 1, 0), the BLACKNESS and two WHITENESS
    PatBlts, and the three Polygons' points."""
    assert _push_order(isp, "seg5:0x3731", 6) == [450, 90, 1, 1, 0, 0]
    assert _push_order(isp, "seg5:0x3762", 6) == [0, 0, 450, 90, 0, 0x42]
    assert _push_order(isp, "seg5:0x3778", 6) == [45, 0, 46, 90, 0xFF, 0x62]
    assert _push_order(isp, "seg5:0x378f", 6) == [360, 45, 90, 46, 0xFF, 0x62]
    assert isp.at("seg5:0x371a").text == "cmp word ptr [0x186], 0"
    # replay the word stores into the point array at [bp - 0x26] before each Polygon call
    mem, reg, polys = {}, {}, []
    i = isp.at("seg5:0x37bc")
    while i.addr != "seg5:0x3856":
        t = i
        if t.mnem == "mov" and t.ops[0] in ("ax", "cx", "dx"):
            reg[t.ops[0]] = int(t.ops[1], 0)
        elif t.mnem == "xor" and t.ops[0] == t.ops[1]:
            reg[t.ops[0]] = 0
        elif t.mnem == "mov" and t.mem is not None and t.mem.regs == ("bp",):
            src = t.ops[1]
            mem[s16(t.mem.disp)] = reg[src] if src in reg else int(src, 0)
        elif "Polygon" in t.text:
            n = _imm_pushes(isp, t.addr, 1)[0]
            polys.append([(mem[-0x26 + 4 * k], mem[-0x24 + 4 * k]) for k in range(n)])
        i = isp.step(i, 1)
    assert polys == [[(180, 89), (269, 89), (269, 0)], [(112, 89), (179, 89), (179, 0), (157, 0)],
                     [(270, 68), (270, 89), (359, 89), (359, 21)]]


def test_ordinal_15_tables_and_rops(isp):
    """ispace.md 15, Windows and The cut: ISPACE.DLL DGROUP:0x1a and DGROUP:0x10, `retf 0x12`,
    DSna (0x220326) at the four debris blits and SRCAND at the eight sheet blits."""
    d = unpacked_seg("ispace", 11)
    assert struct.unpack_from("<5h", d, 0x1A) == (-128, -118, -91, -50, 0)
    assert struct.unpack_from("<5h", d, 0x10) == (0, 50, 91, 118, 128)
    assert isp.at("seg5:0x17bf").text == "retf 0x12"

    def rop(site):
        lo, hi = _imm_pushes(isp, site, 2)
        return ((hi & 0xFFFF) << 16) | (lo & 0xFFFF)
    for site in ("seg5:0x133e", "seg5:0x1372", "seg5:0x15b4", "seg5:0x15fd"):
        assert rop(site) == 0x00220326, site
    for site in ("seg5:0x140b", "seg5:0x144d", "seg5:0x1483", "seg5:0x14fe", "seg5:0x165a",
                 "seg5:0x168f", "seg5:0x16cb", "seg5:0x175c"):
        assert rop(site) == 0x008800C6, site
    assert D5 == ([-5, -5, -4, -2, 0], [0, 1, 3, 4, 5])


def _band(d: int) -> tuple[list[int], list[int]]:
    """A[k], B[k] of ordinal 15 for band half-width d: lo16(T·d) sar 7."""
    def f(t):
        return s16((t * d) & 0xFFFF) >> 7
    return [f(t) for t in (-128, -118, -91, -50, 0)], [f(t) for t in (0, 50, 91, 118, 128)]


D5 = _band(5)


# ------------------------------------------------------------------ ispace.md ordinal 17, core.md The shield


def test_ordinal_17_arguments(ins, isp):
    """ispace.md 17, Arguments: `retf 0x1c`; [bp + 0x14] and [bp + 0x12] never read; the
    wrapper's 13 pushes; seg11:0x4b00's last two pushes 1 and 0x63; one caller."""
    assert isp.at("seg6:0x5e72").text == "retf 0x1c"
    func = isp.func_insns("seg6:0x5538")
    assert not [i.addr for i in func if i.mem is not None and i.mem.regs == ("bp",)
                and i.mem.disp in (0x12, 0x13, 0x14, 0x15)]
    assert not [i.addr for i in func if i.mnem == "lea" and ("bp + 0x12" in i.text or "bp + 0x14" in i.text)]
    pushes, i = 0, ins.at("seg10:0x7335")
    while i.addr != "seg10:0x7365":
        pushes += i.mnem == "push"
        i = ins.step(i, 1)
    assert pushes >= 13
    assert ins.at("seg10:0x7365").text == "lcall [0x3c38]"
    assert (ins.at("seg11:0x4be4").text, ins.at("seg11:0x4be6").text) == ("push 0x63", "push 1")
    assert ins.call_sites("seg10:0x7328") == ["seg11:0x4c32"]


def test_wing_repair_early_return(ins):
    """core.md Purchases, seg16:0x3468: for k = 3 the early return needs DGROUP:0x3a38[ship] ≠ 0
    (`je` continues at seg16:0x34ac on 0)."""
    expect = {"seg16:0x3495": "cmp word ptr [bp + 8], 3", "seg16:0x34a0": "cmp word ptr [bx + 0x3a38], 0",
              "seg16:0x34a5": "je seg16:0x34ac", "seg16:0x34a7": "xor ax, ax"}
    assert _texts(ins, expect) == expect


def test_ship_words_20_22_writers(ins):
    """structs.md Ship records +0x20/+0x22: the eight stores at DGROUP:0x70b0/0x70b2."""
    assert _disp_stores(ins, 0x70B0) == ["seg8:0x4b9b", "seg11:0x4b97", "seg11:0x4bd5", "seg16:0x35f7"]
    assert _disp_stores(ins, 0x70B2) == ["seg8:0x4b9f", "seg11:0x4b9b", "seg11:0x4bd9", "seg16:0x35fb"]


def test_design_word_2a_and_barrel_offset_writers(ins):
    """structs.md Design records +0x2a and +0x68 … +0x6e: the writers of DGROUP:0x96fc + 0xad·n
    and of DGROUP:0x973a … 0x9740 (indexed stores)."""
    assert _disp_stores(ins, 0x96FC) == ["seg10:0x9d1d", "seg10:0x9e3a", "seg14:0xba9", "seg14:0xd94",
                                         "seg16:0x6012", "seg18:0x3098"]
    offs = {d: _disp_stores(ins, d) for d in (0x973A, 0x973C, 0x973E, 0x9740)}
    assert offs == {0x973A: ["seg14:0x13b2", "seg18:0x3330"], 0x973C: ["seg14:0x13ca", "seg18:0x3348"],
                    0x973E: ["seg14:0x13d8", "seg18:0x3352"], 0x9740: ["seg14:0x13e0", "seg18:0x335a"]}


def test_barrel_offset_vectors(master, dgroup):
    """core.md The barrel offsets and formats.md MASTER.BOD +0x1e/+0x1f: TALON (body 5) 1, 16 and
    TRANSFORMER (body 33) 4, 6; SHP record 59 SKY TALON weapon 7 body 5, record 1 weapon 8 body
    33; the design words and the SKY TALON's heading-3 entries from the sine bytes DGROUP:0xf00."""
    bod, shp = master
    recs = [bod[k * 44:(k + 1) * 44] for k in range(52)]
    assert recs[5][:5] == b"TALON" and (recs[5][0x1E], recs[5][0x1F]) == (1, 16)
    assert recs[33][:11] == b"TRANSFORMER" and (recs[33][0x1E], recs[33][0x1F]) == (4, 6)
    sky, tf = shp[59 * 65:60 * 65], shp[65:130]
    assert sky[10:19] == b"SKY TALON" and (sky[6], struct.unpack_from("<H", sky, 0x21)[0]) == (7, 5)
    assert tf[10:21] == b"TRANSFORMER" and (tf[6], struct.unpack_from("<H", tf, 0x21)[0]) == (8, 33)

    def words(b1e, b1f, side=33, size=1):
        c = side // 2
        w68 = s16(b1e) - c - size
        return w68, s16(b1f) - c, -(w68 + size), s16(b1f) - c
    assert words(1, 16) == (-16, 0, 15, 0) and words(4, 6) == (-13, -10, 12, -10)
    sin = lambda a: dgroup[0xF00 + a]
    cos = lambda a: dgroup[0xF00 + 90 - a]

    def t128(v):
        return int(v / 128)  # toward zero

    def entry(ox, oy, a, s=33):
        lo = lambda v: s16(v & 0xFFFF)
        return (s // 2 + t128(lo(lo(ox * cos(a)) - lo(oy * sin(a)))),
                s // 2 + t128(lo(lo(oy * cos(a)) + lo(ox * sin(a)))))
    assert (sin(68), cos(68)) == (119, 49)
    assert entry(-16, 0, 68) == (10, 2) and entry(15, 0, 68) == (21, 29)
    assert (21 - 10, 29 - 2) == (11, 27)


# ------------------------------------------------------------------ world.md Effect frame, Pieces


def test_effect_brushes(ins):
    """world.md Effect frame, The brushes: the 20 CreateSolidBrush COLORREFs of seg10:0x4b2e."""
    calls = ["seg10:0x4c11", "seg10:0x4c1f", "seg10:0x4c2d", "seg10:0x4c3b", "seg10:0x4c48", "seg10:0x4c56",
             "seg10:0x4c64", "seg10:0x4c71", "seg10:0x4c7e", "seg10:0x4c8b", "seg10:0x4c98", "seg10:0x4ca6",
             "seg10:0x4cb4", "seg10:0x4cc2", "seg10:0x4cd0", "seg10:0x4cdd", "seg10:0x4cea", "seg10:0x4cf7",
             "seg10:0x4d04", "seg10:0x4d11"]
    got = _brushes(ins, {c: 0x576C + 2 * n for n, c in enumerate(calls)})
    assert [got[0x576C + 2 * n] for n in range(20)] == [
        0x808080, 0xA0A0A0, 0xC0C0C0, 0xE0E0E0, 0xFFFFFF, 0xC0C0C0, 0xA0DFA0, 0x60E060, 0x40FF40, 0x00FF00,
        0x800000, 0x8A2800, 0xA05000, 0xC4A000, 0xFFC400, 0x0000A0, 0x0000C4, 0x0020DF, 0x0060FF, 0x0080FF]


def test_colour_offset(ins, dgroup):
    """world.md The colour offset, globals.md DGROUP:0xb6a: 0 and 1 in the image, the two
    writers, the 60 ms test against DGROUP:0x579c and the bounds 3 and 1."""
    assert struct.unpack_from("<hh", dgroup, 0xB6A) == (0, 1)
    assert _writers(ins, 0xB6A, 0xB6D) == ["seg8:0x82e2", "seg8:0x82f4"]
    assert _writers(ins, 0x579C, 0x579F) == ["seg8:0x82db"]
    expect = {"seg8:0x82d0": "cmp eax, dword ptr [0x579c]", "seg8:0x82e6": "cmp word ptr [0xb6a], 3",
              "seg8:0x82ed": "cmp word ptr [0xb6a], 1", "seg8:0x82f4": "neg word ptr [0xb6c]"}
    assert _texts(ins, expect) == expect
    seq, v, d = [], 0, 1
    for _ in range(9):
        v += d
        if v > 3 or v < 1:
            d = -d
        seq.append(v)
    assert seq == [1, 2, 3, 4, 3, 2, 1, 0, 1]


def test_piece_record_index_and_types(ins):
    """structs.md Effect pieces and Effect table +0x0a: the 0x1c-byte piece index and the three
    constant type stores (7, 7, 6)."""
    expect = {"seg8:0x430e": "imul ax, word ptr [bp + 6], 5", "seg8:0x4312": "add ax, word ptr [bp - 4]",
              "seg8:0x4315": "imul ax, ax, 5", "seg8:0x4318": "add ax, word ptr [bp - 6]",
              "seg8:0x431b": "imul bx, ax, 0x1c", "seg8:0x431e": "push word ptr [bx + 0x2414]",
              "seg7:0x2f73": "mov word ptr [bx + 0x1e9a], 6"}
    assert 0x2414 + 4 * 25 * 0x1C == 0x2F04
    assert _texts(ins, expect) == expect
    assert "0x20007" in ins.at("seg8:0x93a9").text
    assert ins.at("seg8:0x8126").text.endswith(", 7")


# ------------------------------------------------------------------ world.md Smoke


def test_smoke_table_and_constants(ins):
    """world.md Smoke, structs.md Smoke records: the placement's −1 loop over 20 records of 0x14,
    the start's thresholds 25, 45, 65, 90, the mover's T table, its end at 0x37 in the same call as
    the count, and the strip bitmap 270 × 22 in DGROUP:0x3a00."""
    expect = {"seg9:0x674c": "imul bx, word ptr [bp - 4], 0x14", "seg9:0x6750": "mov word ptr [bx + 0x58cc], 0xffff",
              "seg9:0x6759": "cmp word ptr [bp - 4], 0x14",
              "seg7:0x5b89": "mov word ptr [bp - 0xc], 0x19", "seg7:0x5ba8": "mov word ptr [bp - 0xc], 0x2d",
              "seg7:0x5baf": "mov word ptr [bp - 0xc], 0x41", "seg7:0x5bb6": "mov word ptr [bp - 0xc], 0x5a",
              "seg7:0x5b8e": "cmp word ptr [0x9208], 3", "seg7:0x5ba3": "jle seg7:0x5bbd",
              "seg7:0x5e13": "mov dword ptr [bp - 0x18], 0x120006", "seg7:0x5e1b": "mov dword ptr [bp - 0x14], 0x3c0024",
              "seg7:0x5e25": "mov word ptr [bp - 0x1c], ax", "seg7:0x5e28": "mov word ptr [bp - 0x1a], ax",
              "seg7:0x602f": "inc word ptr [bx + 0x58cc]", "seg7:0x6037": "cmp word ptr [bx + 0x58cc], 0x37",
              "seg7:0x6046": "mov ax, 6", "seg7:0x605d": "mov ax, 0x18",
              "seg10:0x525a": "push 0x10e", "seg10:0x525d": "mov ax, 0x16", "seg10:0x5275": "mov word ptr [0x3a00], ax"}
    assert _texts(ins, expect) == expect
    # T = {0, 0, 6, 18, 36, 60}; [bp − 0x10] (T[6]) is written in seg7:0x5e04 only by the seg18 block
    w = [i.addr for i in ins.func_insns("seg7:0x5e04")
         if i.mem is not None and i.mem.regs == ("bp",) and i.mem.disp == 0xFFF0 and is_write(i)]
    assert w == ["seg7:0x5f17", "seg7:0x5f69"] and all(A("seg7:0x5ee6")[1] <= A(a)[1] < A("seg7:0x5f77")[1] for a in w)
    assert (ins.at("seg7:0x5edd").text, ins.at("seg7:0x5ee2").text) == ("cmp word ptr [0x9130], 0", "je seg7:0x5f79")
    assert [q * 30 + t for q, t in enumerate((0, 0, 6, 18, 36, 60))] == [0, 30, 66, 108, 156, 210]


def test_smoke_counters(ins):
    """globals.md DGROUP:0x9456 and DGROUP:0x58a: their writers and readers."""
    assert _writers(ins, 0x9456) == ["seg7:0x5c76", "seg7:0x5df9", "seg7:0x608b", "seg9:0x670a"]
    assert sorted({i.addr for i in ins.refs(0x9456, 0x9456)} - set(_writers(ins, 0x9456)), key=A) == \
        ["seg7:0xd0a", "seg18:0x332"]
    assert _writers(ins, 0x58A) == ["seg7:0x13a2", "seg7:0x6025", "seg12:0x4f3e"]


def test_smoke_graphic():
    """world.md Smoke, The strip: IGRAPHIC.DLL #4000 200 is 60 × 90 at 4 bpp, entry 7 (192, 0, 0)
    used only in bands 0 and 1 (rows 0 … 21 of the top-down picture)."""
    data = _resource("IGRAPHIC.DL1", 4000, 200)
    w, h, bpp, table = _dib(data)
    assert (w, h, bpp, table[7]) == (60, 90, 4, "c00000")
    o = 14 if data[:2] == b"BM" else 0
    bits = data[o + 40 + 64:]
    stride = ((w * 4 + 31) // 32) * 4
    red_rows = set()
    for row in range(h):
        line = bits[(h - 1 - row) * stride:(h - row) * stride]
        for x in range(w):
            v = line[x // 2] >> 4 if x % 2 == 0 else line[x // 2] & 15
            if v == 7:
                red_rows.add(row)
    assert red_rows and max(red_rows) < 10 + 12 and [sum(2 * (m + 5) for m in range(k)) for k in range(7)] == \
        [0, 10, 22, 36, 52, 70, 90]


# ------------------------------------------------------------------ core.md Icon effects, M-3


def test_icon_effect_constants(ins):
    """core.md Icon effects: the melt's 80 and its heat test, the ignite's 0x59 and 0x6a, the
    graphic ids 230, 360 and 500 pushed before seg10:0x6c02 (pitfalls.md), seg9:0x24d3's dword."""
    expect = {"seg7:0x4cd5": "cmp byte ptr es:[bx + si + 0x2f], 4", "seg7:0x4cda": "jle seg7:0x4cf7",
              "seg7:0x4ce0": "mov word ptr [bx + 0x57a8], 0x50", "seg7:0x4ced": "push 0x6a",
              "seg7:0x4794": "push 0x6a", "seg7:0x47b4": "mov word ptr [bx + 0x57a8], 0x59",
              "seg7:0x47c0": "push 0xe6", "seg13:0x5e02": "push 0x1f4",
              "seg9:0x24d3": "mov dword ptr [0x92ec], 0x20008", "seg9:0x24b5": "mov word ptr [0x9406], 8"}
    assert _texts(ins, expect) == expect
    assert ins.at("seg7:0x255f").text.startswith("push 0x168")
    assert all(_calls(ins, {"seg7:0x47c3": "seg10:0x6c02", "seg13:0x5e05": "seg10:0x6c02"}).values())
    assert (0x50 - 6, 0x59) == (74, 89)


def test_briefing_crack_loop(ins):
    """core.md The briefing's crack loop: seg9:0x5cca's seg9:0x22b4, seg9:0xb2a, seg10:0x506c and
    the loop seg7:0x233c(0, k, 0) for k < 8; DGROUP:0x94b0's two writers."""
    assert all(_calls(ins, {"seg9:0x5d3c": "seg9:0x22b4", "seg9:0x5d66": "seg9:0xb2a",
                            "seg9:0x5d6b": "seg10:0x506c", "seg9:0x5d7c": "seg7:0x233c"}).values())
    expect = {"seg9:0x5d75": "push 0", "seg9:0x5d77": "push word ptr [bp - 4]", "seg9:0x5d7a": "push 0",
              "seg9:0x5d87": "cmp word ptr [bp - 4], 8", "seg9:0x5d8b": "jl seg9:0x5d75"}
    assert _texts(ins, expect) == expect
    assert _writers(ins, 0x94B0) == ["seg6:0x36b6", "seg6:0x36ed"]


def test_kind_3_pod(ins):
    """core.md Other touches, A kind-3 pod: the kind tests 2 and 1 and the jump past the
    hand-over."""
    expect = {"seg13:0x1d7a": "cmp word ptr [bx - 0x7aa0], 2", "seg13:0x1dbf": "cmp word ptr [bx - 0x7aa0], 1",
              "seg13:0x1dc6": "jmp seg13:0x1e8d"}
    assert _texts(ins, expect) == expect
    assert (-0x7AA0) & 0xFFFF == 0x8560


# ------------------------------------------------------------------ globals.md DGROUP:0x9432, 0x9332, 0x5b8


def test_violence_decay(ins, dgroup):
    """globals.md DGROUP:0x9432 and DGROUP:0x5b8, timing.md: the 10-s timer's writers and the
    decay's divisors, floors and 15000 ms test."""
    assert struct.unpack_from("<I", dgroup, 0x5B8)[0] == 0
    assert _writers(ins, 0x5B8, 0x5BB) == ["seg7:0x91a", "seg7:0x91d", "seg18:0x250", "seg18:0x253"]
    expect = {"seg7:0x213f": "mov cx, 0x32", "seg7:0x2176": "mov cx, 0x12", "seg7:0x217c": "cmp ax, 4",
              "seg7:0x21a4": "mov cx, 0xe", "seg7:0x21aa": "cmp ax, 4", "seg7:0x21b7": "mov cx, 0x28",
              "seg7:0x21bd": "cmp ax, 3", "seg7:0x21e5": "mov cx, 0x24", "seg7:0x21eb": "cmp ax, 2",
              "seg7:0x2194": "add eax, 0x3a98", "seg7:0x219f": "jae seg7:0x21ff",
              "seg7:0x215c": "cmp word ptr [bx + 0x70c2], 0xe", "seg7:0x216c": "cmp word ptr [bx + 0x70c8], 1"}
    assert _texts(ins, expect) == expect

    def step(a, q, m):
        d = max(int(a / q), m)
        return max(a - d, 0)
    a = 98
    for _ in range(21):
        a = step(a, 50, 2)
    assert a == 56 and 56 * 9 // 10 == 50 and step(step(50, 40, 3), 36, 2) == 45


def test_violence_score_writers(ins):
    """globals.md DGROUP:0x9432 and DGROUP:0x9332: the writer lists and DGROUP:0x9332's readers."""
    assert _writers(ins, 0x9432) == [
        "seg4:0x629e", "seg6:0x966a", "seg7:0x2184", "seg7:0x218a", "seg7:0x21c5", "seg7:0x21cb",
        "seg7:0x21f3", "seg7:0x21f9", "seg8:0x6221", "seg8:0x62b5", "seg8:0x62d8", "seg8:0x62ff",
        "seg8:0x67e9", "seg8:0x7662", "seg8:0x768b", "seg8:0x7a19", "seg9:0xc97", "seg9:0x24b2",
        "seg11:0x5281", "seg11:0xacdf", "seg13:0x8c2c", "seg13:0x8c33"]
    w = _writers(ins, 0x9332)
    assert w == ["seg9:0x2a1e", "seg9:0x2a2e", "seg9:0x2a3e", "seg9:0x2a46"]
    readers = sorted({i.addr for i in ins.refs(0x9332, 0x9332)} - set(w), key=A)
    assert readers == ["seg9:0x2a67", "seg9:0x2aa6", "seg9:0x2f00", "seg9:0x3900", "seg9:0x6d7f",
                       "seg14:0x2544", "seg14:0x2caf", "seg14:0x2cba", "seg14:0x2db8", "seg14:0x2dc5",
                       "seg14:0x2e07", "seg14:0x2e19", "seg14:0x3647", "seg14:0x3652", "seg14:0x38b6",
                       "seg14:0x38c1", "seg14:0x3903", "seg14:0x3915", "seg14:0x392f"]


def test_generator_class_draw(ins):
    """rng.md seg9:0x3900: one rand() % 6 only while DGROUP:0x9332 < 2, a result below 4 lowering
    DGROUP:0x9336."""
    expect = {"seg9:0x3900": "cmp word ptr [0x9332], 2", "seg9:0x3905": "jge seg9:0x391b",
              "seg9:0x3907": "call far seg5:0xdc", "seg9:0x390c": "mov cx, 6", "seg9:0x3912": "cmp dx, 4",
              "seg9:0x3915": "jge seg9:0x391b", "seg9:0x3917": "dec word ptr [0x9336]"}
    assert _texts(ins, expect) == expect


def test_shp_block_handle(ins):
    """globals.md DGROUP:0x95a6: its two writers and its readers."""
    assert _writers(ins, 0x95A6) == ["seg14:0x3fa1", "seg14:0x40a1"]
    readers = sorted({i.addr for i in ins.refs(0x95A6, 0x95A6) if i.mnem == "cmp"}, key=A)
    assert readers == ["seg8:0x2d23", "seg9:0x2ed6", "seg14:0x1380", "seg14:0x3ee9", "seg14:0x3fb1"]


# ------------------------------------------------------------------ core.md weapons


def test_weapon_7_words(ins):
    """structs.md Weapon 7 and core.md The SKY TALON's Z: seg10:0x1fb2's stores of W(7) =
    DGROUP:0x3184 + n."""
    want = {"seg10:0x23a1": (-8, 23), "seg10:0x23bc": (-6, 1), "seg10:0x23d1": (-4, 0xFFFF),
            "seg10:0x23bf": (-2, 370), "seg10:0x23c5": (0x00, 193), "seg10:0x23ce": (0x02, 0xFFFF),
            "seg10:0x23aa": (0x04, 1), "seg10:0x23e3": (0x06, 2), "seg10:0x23ad": (0x08, 1),
            "seg10:0x2389": (0x0a, 4), "seg10:0x238f": (0x0e, 100), "seg10:0x2395": (0x10, 20),
            "seg10:0x23d4": (0x18, 0x1C), "seg10:0x239b": (0x1e, 200), "seg10:0x23b0": (0x24, 1),
            "seg10:0x243c": (0x34, 800), "seg10:0x23b3": (0x3a, 1), "seg10:0x23b6": (0x3c, 1),
            "seg10:0x23b9": (0x3e, 1)}
    w7 = 0x2F54 + 0x50 * 7
    for site, (off, val) in want.items():
        disp, v = _store(ins, site)
        assert (disp - w7, v) == (off, val), site
    for site, off in {"seg10:0x2400": 0x0C, "seg10:0x2418": 0x14, "seg10:0x2424": 0x20, "seg10:0x2404": 0x22,
                      "seg10:0x2434": 0x2E, "seg10:0x2442": 0x36, "seg10:0x2438": 0x46}.items():
        assert ins.at(site).mem.disp - w7 == off, site
    assert _eng(193) == "HEATSEEKER MISSILES" and _eng(186 + 8) == "RADAR-GUIDED MISSILES"


def test_animation_4_weapons(ins):
    """structs.md Animation 4, core.md seg10:0x4412: W(10), W(16), W(17) +0x06 and +0x26 sites;
    DGROUP:0x3450 = W(16) − 4 with −1 at seg10:0x2da8; the gap rule, GetObject's 14 bytes and
    [bp − 0xa]'s one writer."""
    for w, s6, s26 in ((10, "seg10:0x27f8", "seg10:0x27ad"), (16, "seg10:0x2ddd", "seg10:0x2d84"),
                       (17, "seg10:0x2bcc", "seg10:0x2c76")):
        base = 0x2F54 + 0x50 * w
        assert ins.at(s6).mem.disp - base == 0x06 and ins.at(s26).mem.disp - base == 0x26, w
    assert ins.at("seg10:0x2d84").text == "mov word ptr [0x347a], 8"
    assert ins.at("seg10:0x2bcc").text == "mov word ptr [0x34aa], 4"
    assert 0x2F54 + 0x50 * 16 - 4 == 0x3450
    assert ins.at("seg10:0x2da8").text == "mov word ptr [0x3450], 0xffff"
    expect = {"seg12:0x138c": "mov word ptr [bx + 0x66e4], 0x10", "seg12:0x1392": "mov ax, word ptr [0x3450]",
              "seg10:0x4423": "push 3", "seg10:0x4433": "push 0xe", "seg10:0x4445": "test al, 1",
              "seg10:0x4453": "mov word ptr es:[bx + 0x24], 5", "seg10:0x4465": "mov word ptr es:[bx + 0x24], 6",
              "seg10:0x4602": "cmp word ptr [bp - 0xa], 0"}
    assert _texts(ins, expect) == expect
    w = [i.addr for i in ins.func_insns("seg10:0x4412")
         if i.mem is not None and i.mem.regs == ("bp",) and i.mem.disp == 0xFFF6 and is_write(i)]
    assert w == ["seg10:0x44fb"]
    assert ins.call_sites("seg10:0x4412") == ["seg10:0x545e"]


def test_load_weapon_slot(ins):
    """core.md Loading a weapon: the search from slot 1, the ten-slot bound, the count 0x14 and
    the ten callers."""
    expect = {"seg12:0x6d72": "mov word ptr [bp - 4], 1", "seg12:0x6d89": "jle seg12:0x6d9c",
              "seg12:0x6da1": "cmp byte ptr [bx + 0x715b], 9", "seg12:0x6da6": "jg seg12:0x6de2",
              "seg12:0x6dad": "inc byte ptr [bx + 0x715b]", "seg12:0x6dbf": "mov byte ptr [bx + si + 0x7148], al",
              "seg12:0x6dc6": "mov byte ptr [bx + 0x713e], 0x14"}
    assert _texts(ins, expect) == expect
    assert ins.call_sites("seg12:0x6d64") == [
        "seg7:0xed3", "seg11:0x7362", "seg11:0x7370", "seg11:0x737e", "seg11:0x9f26", "seg13:0x1e4e",
        "seg14:0x1764", "seg16:0x39e2", "seg16:0x4021", "seg18:0x5251"]
    assert all(_calls(ins, {"seg12:0x6ddc": "seg10:0x533a"}).values())


def test_award_window_and_message(ins):
    """core.md The 800 award and Message 0x70: the unsigned /10 of the push and the search, the
    bound 20, case 111's pushes, strings 1064 and 688-691."""
    expect = {"seg8:0x823b": "call far seg5:0x2b4", "seg8:0x8240": "mov word ptr [0x937a], ax",
              "seg7:0x190a": "call far seg5:0x2b4", "seg7:0x191c": "cdq", "seg7:0x1923": "jne seg7:0x1973",
              "seg7:0x1925": "cmp cx, 0x14", "seg7:0x1928": "jae seg7:0x1973",
              "seg3:0x108a": "push 0x380c", "seg3:0x108d": "push 0x2b0", "seg3:0x1090": "push 4",
              "seg3:0x1092": "push 2", "seg3:0x1094": "push 0x428", "seg3:0x1097": "jmp seg3:0x915",
              "seg3:0x919": "call far seg10:0xa332"}
    assert _texts(ins, expect) == expect
    assert ins.switches["seg3:0x1351"]["targets"][0x70 - 1] == "seg3:0x1089"
    assert (0x2B0, 0x428) == (688, 1064)
    assert _eng(1064) == "He Shoots! He Scores! %R %s!!"
    assert [_eng(k) for k in range(688, 692)] == ["Well Done, ", "Way To Go, ", "That's The Ticket, ", "Right On, "]

    def found(p, t):
        d = t // 10 - p // 10
        return 0 <= d < 20
    assert [found(*v) for v in ((0, 190), (0, 199), (0, 200), (9, 199), (9, 200), (5, 196))] == \
        [True, True, False, True, False, True]
    assert all(found(p, p + d) for p in range(10) for d in range(191))
    assert not any(found(p, p + d) for p in range(10) for d in range(200, 260))


def test_seam_word_frames(ins):
    """core.md Shot hit test, Its value: the frames that put seg8:0x6f3c's [bp − 0x1e] at T − 44
    and seg8:0x29ea's CX push there."""
    expect = {"seg8:0x3692": "sub sp, 0x30", "seg8:0x6f46": "sub sp, 0x34", "seg8:0x29f4": "sub sp, 0x16",
              "seg8:0x2add": "push bx", "seg8:0x2ade": "push cx", "seg8:0x2adf": "call far seg9:0x88dc",
              "seg8:0x2b4a": "call far seg5:0x346", "seg8:0x2b4f": "jmp seg8:0x2b9e",
              "seg8:0x36bf": "lea ax, [bp - 0x1e]", "seg8:0x72ab": "cmp word ptr [bp - 0x1e], 0"}
    assert _texts(ins, expect) == expect
    T = 0
    bp_6f3c = T - 8 - 2 - 2 - 2          # four word pushes, push cs, near call, push bp
    assert bp_6f3c - 0x1E == T - 44
    bp_29ea = T - 6 - 4 - 2              # three word pushes, far call, push bp
    sp_29ea = bp_29ea - 2 - 0x16 - 2 - 2  # push ds, sub sp 0x16, push di, push si
    assert sp_29ea == T - 40 and sp_29ea - 4 == T - 44   # push bx, push cx
    assert -0x36 + 0x1E == -24 and (-0x36) - (-0x1E) == -24 and T + 0x36 - 0x1E == 24


def test_break_up_sounds(ins):
    """re/artifacts/sounds.tsv rows 0x1a and 0x1f, sound.md Smoke, break-ups and icons: seg7:0x2e02
    pushes 0x1a at seg7:0x3074 and jumps to the shared call at seg7:0x2ec2."""
    expect = {"seg7:0x3074": "push 0x1a", "seg7:0x3076": "jmp seg7:0x2ec2", "seg7:0x2ec0": "push 0x1f",
              "seg7:0x2ec2": "call far seg10:0x754a", "seg7:0x2e5c": "cmp word ptr [bp + 6], 8",
              "seg7:0x2e79": "jmp seg7:0x306e", "seg7:0x2e9e": "cmp word ptr [bx + 0x2f78], 6",
              "seg7:0x2ea5": "jmp seg7:0x306e", "seg7:0x2ea8": "cmp word ptr [bp + 6], 4",
              "seg7:0x2eac": "jne seg7:0x2eca"}
    assert _texts(ins, expect) == expect
    rows = {r["id"]: r for r in csv.DictReader(SOUNDS_TSV.open(newline=""), delimiter="\t")}
    assert "P:seg7:0x2ec2 in seg7:0x2e02(" in rows["0x1a"]["call_sites"]
    assert "P:seg7:0x2ec2 in seg7:0x2e02(" in rows["0x1f"]["call_sites"]


def test_dgroup_3450_and_template(ins):
    """ADR 0016 Context and core.md Contact comes first: the pool-6 template's +0x42 = 6 store in
    seg10:0x108a's loop."""
    i = ins.at("seg10:0x109c")
    assert "0x42" in i.text and i.text.endswith(", 6"), i.text


def test_smoke_and_spark_callers(ins):
    """world.md Smoke, Who starts smoke, and core.md The spark: the call sites of seg7:0x5b68,
    seg7:0x5d6a and seg8:0x804a; no instruction reads ship words +0x20/+0x22 by displacement."""
    assert ins.call_sites("seg7:0x5b68") == [
        "seg7:0x3c75", "seg7:0x3ff7", "seg7:0x4630", "seg7:0x4ac6", "seg7:0x4bf0", "seg7:0x4fef", "seg7:0x504d",
        "seg12:0x1f5", "seg12:0x1972", "seg12:0x3a46", "seg12:0x5833", "seg12:0x6aef", "seg13:0x6dd",
        "seg18:0x48b7"]
    assert ins.call_sites("seg7:0x5d6a") == ["seg12:0x3b5f", "seg13:0x45c0"]
    assert ins.call_sites("seg8:0x804a") == ["seg8:0x3aa7", "seg8:0x76d0"]
    refs = [i for i in ins.insns if i.mem is not None and (i.mem.disp & 0xFFFF) in (0x70B0, 0x70B2)]
    assert len(refs) == 8 and all(is_write(i) for i in refs)


def test_placement_shp_bracket(ins):
    """ispace.md Ship sheets and core.md Loading a weapon: the placement brackets seg14:0x0 with
    seg14:0x3eda and seg14:0x4074; the placement's seg12:0x6d64(ship, 13) under DGROUP:0x104e."""
    assert all(_calls(ins, {"seg9:0x6883": "seg14:0x3eda", "seg9:0x688a": "seg14:0x0",
                            "seg9:0x6890": "seg14:0x4074"}).values())
    expect = {"seg14:0x1758": "cmp word ptr [0x104e], 0", "seg14:0x175f": "push 0xd",
              "seg14:0x1764": "call far seg12:0x6d64"}
    assert _texts(ins, expect) == expect


def test_overlay_and_debris_puff_callers(ins):
    """core.md Icon effects, the overlay's callers; world.md Smoke: seg7:0x5d6a never reads its
    third argument [bp + 0xa]."""
    assert ins.call_sites("seg7:0x2512") == ["seg7:0x4c3c", "seg9:0x94ef", "seg9:0x9540", "seg11:0xa3db"]
    assert not [i.addr for i in ins.func_insns("seg7:0x5d6a")
                if i.mem is not None and i.mem.regs == ("bp",) and i.mem.disp in (0xA, 0xB)]


# ------------------------------------------------------------------ the corrected facts (M4 s5 fixer)


def test_pod_resources_word_readers(ins):
    """core.md Captures, a kind-3 pod (icons claim 55, CORRECTED): the pod word DGROUP:0x8562 +
    0x34·rec has two indexed readers, seg13:0x1dc9 and the fill's seg13:0x4c7b, and seg3:0x23e9
    reads record 0's word itself."""
    def reads(indexed: bool) -> list[str]:
        return sorted({i.addr for i in ins.insns if i.mem is not None and (i.mem.disp & 0xFFFF) == 0x8562
                       and i.mem.size == 2 and bool(i.mem.regs) == indexed and i.mem.ds
                       and not is_write(i)}, key=A)
    assert reads(True) == ["seg13:0x1dc9", "seg13:0x4c7b"]
    assert reads(False) == ["seg3:0x23e9"]
    assert _texts(ins, {"seg13:0x1dc9": "mov ax, word ptr [bx - 0x7a9e]"}) == \
        {"seg13:0x1dc9": "mov ax, word ptr [bx - 0x7a9e]"}


def test_award_history_bytes_sign_extended(ins):
    """core.md The 800 award, The window (weapons claim 24, CORRECTED): the history's pool and
    record bytes DGROUP:0x938a + k and DGROUP:0x9392 + k are sign-extended (`cbw`, listed as
    `cwde`) before the word compares with the object's +0x06 pair."""
    expect = {"seg7:0x192d": "mov al, byte ptr [bx - 0x6c76]", "seg7:0x1931": "cwde",
              "seg7:0x1936": "cmp word ptr [si + 0x226c], ax", "seg7:0x193a": "jne seg7:0x18f8",
              "seg7:0x193c": "mov al, byte ptr [bx - 0x6c6e]", "seg7:0x1940": "cwde",
              "seg7:0x1941": "cmp word ptr [si + 0x226e], ax", "seg7:0x1945": "jne seg7:0x18f8"}
    assert _texts(ins, expect) == expect
    assert ins.at("seg7:0x1931").raw == b"\x98" and ins.at("seg7:0x1940").raw == b"\x98"
    assert ((0x10000 - 0x6C76) & 0xFFFF, (0x10000 - 0x6C6E) & 0xFFFF) == (0x938A, 0x9392)


def test_double_guns_wingless_store(ins):
    """core.md Purchases, seg16:0x3ece DOUBLE GUNS (weapons claim 41, CORRECTED): with both wings
    0 and lamp row 3 (design +0x28) = 3 it stores +0x24 = 1 and byte +0xe0 = 1."""
    expect = {"seg16:0x3f6b": "cmp word ptr [si - 0x6906], 3", "seg16:0x3f70": "jne seg16:0x3f7d",
              "seg16:0x3f72": "mov word ptr [bx + 0x70b4], 1", "seg16:0x3f78": "mov byte ptr [bx + 0x7170], 1",
              "seg16:0x3f7d": "mov ax, 0xfa"}
    assert _texts(ins, expect) == expect
    assert (0x70B4 - 0x7090, 0x7170 - 0x7090, (0x10000 - 0x6906) & 0xFFFF - 0x96D2) == (0x24, 0xE0, 0x28)


def test_design_half_side_store(ins):
    """formats.md MASTER.BOD +0x19 and core.md The muzzle tables: seg14:0x0 writes s =
    trunc([bp − 0x24] / 2) to design words +0x00 and +0x02 (DGROUP:0x96d2 / 0x96d4 + 0xad·n), the
    radii seg10:0x1460 (+0x00 + 3) and seg10:0x1620 (+0x02) read."""
    expect = {"seg14:0xc42": "mov ax, word ptr [bp - 0x24]", "seg14:0xc45": "cdq", "seg14:0xc46": "sub ax, dx",
              "seg14:0xc48": "sar ax, 1", "seg14:0xc50": "mov word ptr [bx - 0x692e], ax",
              "seg14:0xc54": "mov word ptr [bx - 0x692c], ax"}
    assert _texts(ins, expect) == expect
    assert ins.at("seg14:0xc45").raw == b"\x99"  # cwd in a 16-bit segment
    assert ((0x10000 - 0x692E) & 0xFFFF, (0x10000 - 0x692C) & 0xFFFF) == (0x96D2, 0x96D4)
    # The other writers: the Size rescale (seg10) and the visit's new body (seg16:0x6012's path).
    assert _disp_stores(ins, 0x96D2) == ["seg10:0xa015", "seg14:0xc50", "seg16:0x60e7"]
    assert _disp_stores(ins, 0x96D4) == ["seg10:0xa019", "seg14:0xc54", "seg16:0x60eb"]


def test_ship_byte_e3_writer(ins):
    """core.md Which row and world.md Ship flight fields: the only writer of ship byte +0xe3
    (DGROUP:0x7173 + 0x15a·i) stores design byte +0x44 (DGROUP:0x9716 + 0xad·n) AND 1, and design
    +0x44 is 0 for SHP +9 bit 0x01, else 1."""
    expect = {"seg14:0x1ed6": "test byte ptr es:[bx + si + 9], 1", "seg14:0x1edb": "je seg14:0x1eea",
              "seg14:0x1ee2": "mov word ptr [bx - 0x68ea], 0", "seg14:0x1eef": "mov word ptr [bx - 0x68ea], 1",
              "seg14:0x21d0": "mov al, byte ptr [si - 0x68ea]", "seg14:0x21d4": "and al, 1",
              "seg14:0x21d6": "mov byte ptr [di + 0x7173], al"}
    assert _texts(ins, expect) == expect
    assert _disp_stores(ins, 0x7173) == ["seg14:0x21d6"]
