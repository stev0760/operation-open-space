"""Mechanical facts of the M4 stage 4 verification round in docs/re/{world,shell,sound,pitfalls}.md.

The listing model, the fixtures and the helpers come from test_re_facts.py and
test_re_facts_core.py; this file follows their conventions (references, writers,
``segN:0xOFF`` addresses) and skips when ``work/`` or ``original/`` is absent. Each
test names the doc file and the section it pins.
"""
from __future__ import annotations

import struct

from test_re_facts import (  # noqa: F401  (ins and dgroup are pytest fixtures)
    A, _calls, _eng, _ne, _texts, dgroup, ins, is_write,
)
from test_re_facts_core import _callers, _imm_pushes


# ------------------------------------------------------------------ shell.md Phase 11


def test_phase11_slot_latched_at_entry(ins):
    """shell.md Phase 11 Slot and No sheet, pitfalls.md A value latched at entry: seg17:0x20fa
    reads ship word +0x50 of DGROUP:0x9464 plus 0x28 into [bp - 0x12] at its entry, every DC
    read indexes DGROUP:0x39e8 with it, the Space path raises DGROUP:0x9464 later, and a zero
    DC returns 1 at seg17:0x22a4."""
    expect = {"seg17:0x2109": "mov ax, word ptr [0x9464]", "seg17:0x210f": "imul bx, ax, 0x15a",
              "seg17:0x2113": "mov ax, word ptr [bx + 0x70e0]", "seg17:0x2117": "add ax, 0x28",
              "seg17:0x211a": "mov word ptr [bp - 0x12], ax",
              "seg17:0x228c": "inc word ptr [0x9464]",
              "seg17:0x23f7": "mov bx, word ptr [bp - 0x12]",
              "seg17:0x23fc": "cmp word ptr [bx + 0x39e8], 0", "seg17:0x2401": "je seg17:0x22a4",
              "seg17:0x22a4": "mov ax, 1",
              "seg17:0x255e": "mov bx, word ptr [bp - 0x12]",
              "seg17:0x2563": "push word ptr [bx + 0x39e8]"}
    assert _texts(ins, expect) == expect
    # [bp - 0x12] has one writer in the routine: the entry latch.
    w = [i.addr for i in ins.func_insns("seg17:0x20fa")
         if i.mem is not None and i.mem.regs == ("bp",) and i.mem.disp == 0xFFEE and is_write(i)]
    assert w == ["seg17:0x211a"]


def test_phase11_stretch_call(ins):
    """shell.md Phase 11 Fly-in: SetStretchBltMode(DGROUP:0x13dc, 3), then the cdecl
    seg10:0x9962 with 13 words (add sp 0x1a), the ROP dword 0x00CC0020 pushed first and the
    source (gutter + (gutter + cell)k, gutter, cell, cell)."""
    expect = {"seg17:0x253a": "push word ptr [0x13dc]", "seg17:0x253e": "push 3",
              "seg17:0x2545": "push 0xcc", "seg17:0x2548": "push 0x20",
              "seg17:0x254a": "push word ptr [bp - 0xc]", "seg17:0x254d": "push word ptr [bp - 0xc]",
              "seg17:0x2553": "push ax", "seg17:0x2554": "add ax, word ptr [bp - 0xc]",
              "seg17:0x2557": "imul word ptr [bp - 6]", "seg17:0x255a": "add ax, word ptr [bp - 0xe]",
              "seg17:0x2576": "push word ptr [0x13dc]", "seg17:0x257a": "push 0",
              "seg17:0x2581": "add sp, 0x1a"}
    assert _texts(ins, expect) == expect
    assert all(_calls(ins, {"seg17:0x2540": "SetStretchBltMode", "seg17:0x257c": "seg10:0x9962"}).values())
    words, i = 0, ins.at("seg17:0x2545")
    while i.addr != "seg17:0x257c":
        if i.mnem == "push":
            words += 2 if i.raw[0] == 0x66 else 1
        i = ins.step(i, 1)
    assert words == 13 and 2 * words == 0x1A


def test_ship_page_sentence(ins, dgroup):
    """shell.md Phase 11 Sentence, world.md Ship rating: seg17:0x4068 sets ship word +0x26 to
    100 around seg13:0x33d6, compares +0xd8 signed with 32600, 25000 and 100 x the words
    DGROUP:0x96a down to DGROUP:0x956, picks 709 ... 722 and formats 3023 with
    (1, ship = player, ship != player, rank)."""
    expect = {"seg17:0x407b": "mov ax, word ptr [bx + 0x70b6]",
              "seg17:0x4082": "mov word ptr [bx + 0x70b6], 0x64",
              "seg17:0x4096": "mov word ptr [si + 0x70b6], ax",
              "seg17:0x409a": "cmp word ptr [si + 0x7168], 0x7f58",
              "seg17:0x40af": "cmp word ptr [bx + 0x7168], 0x61a8",
              "seg17:0x41e9": "push 1", "seg17:0x41eb": "push 0xbcf", "seg17:0x41f7": "add sp, 0xe"}
    assert _texts(ins, expect) == expect
    assert (0x7F58, 0x61A8, 0xBCF) == (32600, 25000, 3023)
    assert all(_calls(ins, {"seg17:0x408d": "seg13:0x33d6", "seg17:0x41f2": "seg10:0xa332"}).values())
    func = [i for i in ins.func_insns("seg17:0x4068") if A("seg17:0x409a")[1] <= i.off < A("seg17:0x41c6")[1]]
    # Every band compare is signed and the band words are read in descending address order.
    assert {i.mnem for i in func if i.mnem.startswith("j") and i.mnem != "jmp"} == {"jle"}
    bands = [i.mem.disp for i in func if i.mnem == "imul" and i.abs_ds]
    assert bands == list(range(0x96A, 0x954, -2))
    assert [struct.unpack_from("<h", dgroup, d)[0] for d in bands] == [143, 114, 97, 80, 70, 63, 55, 48, 40, 34, 28]
    ranks = [int(i.ops[1], 0) for i in func if i.text.startswith("mov word ptr [bp - 0x42], ")]
    assert ranks == list(range(709, 723))
    assert _eng(709) == "BEYOND REASON" and _eng(722) == "DEFENSELESS"
    assert _eng(3023) == "%<YOUR%>%<THIS%> SHIP IS %S."


# ------------------------------------------------------------------ shell.md Formatter


_DISPATCH = {"#": "seg10:0xa558", "%": "seg10:0xa512", "<": "seg10:0xa61a", ">": "seg10:0xa6d1",
             "?": "seg10:0xa713", "A": "seg10:0xa4c9", "D": "seg10:0xa754", "N": "seg10:0xa6e3",
             "R": "seg10:0xa7c4", "S": "seg10:0xa4c9", "V": "seg10:0xa4c9", "a": "seg10:0xa435",
             "d": "seg10:0xa558", "l": "seg10:0xa529", "n": "seg10:0xa6e3", "s": "seg10:0xa435",
             "v": "seg10:0xa435"}


def _dispatch(ins, ch: int) -> str:
    """Run seg10:0xa332's directive chain (seg10:0xa3cc-0xa42d) for the character ``ch``."""
    lo, hi = A("seg10:0xa3cc")[1], A("seg10:0xa42f")[1]
    al, zf, above, i = ch, False, False, ins.at("seg10:0xa3cc")
    while True:
        t, nxt = i.text, ins.step(i, 1)
        if t == "cwde":
            pass
        elif t == "cmp ax, 0x76":
            zf, above = al == 0x76, al > 0x76
        elif i.mnem == "sub" and i.ops[0] == "al":
            al = (al - int(i.ops[1], 0)) & 0xFF
            zf = al == 0
        elif t == "dec al":
            al = (al - 1) & 0xFF
            zf = al == 0
        elif i.mnem in ("je", "jne", "ja", "jmp"):
            taken = {"je": zf, "jne": not zf, "ja": above, "jmp": True}[i.mnem]
            if taken:
                target = i.ops[0]
                if not lo <= A(target)[1] < hi:
                    return target
                nxt = ins.at(target)
        else:
            raise AssertionError(t)
        if not lo <= nxt.off < hi:
            return nxt.addr
        i = nxt


def test_formatter_directive_dispatch(ins):
    """shell.md Formatter and Other directives: the subtraction chain sends each directive
    letter to its handler and every other character to seg10:0xa42f, which skips three."""
    got = {chr(c): _dispatch(ins, c) for c in range(0x20, 0x7F)}
    assert {c: a for c, a in got.items() if a != "seg10:0xa42f"} == _DISPATCH
    assert ins.at("seg10:0xa42f").text == "add word ptr [bp - 4], 3"


def test_formatter_groups_and_newline(ins):
    """shell.md Formatter, pitfalls.md A stack byte can hold the directive: %> sets the flag,
    skips two and clears the counter; %N's phase-11 skip also needs byte [bp - 0x6a] = 0, whose
    only writer stores the directive letter, which is N or n on that path."""
    expect = {"seg10:0xa3c5": "mov al, byte ptr [bp + si - 0x131]",
              "seg10:0xa3c9": "mov byte ptr [bp - 0x6a], al",
              "seg10:0xa6d1": "mov word ptr [bp - 0x138], 1", "seg10:0xa6d7": "add word ptr [bp - 4], 2",
              "seg10:0xa6db": "mov word ptr [bp - 0xa], 0",
              "seg10:0xa6ec": "cmp word ptr [0x93aa], 0xb", "seg10:0xa6f3": "cmp byte ptr [bp - 0x6a], 0",
              "seg10:0xa701": "mov byte ptr es:[bx + si], 0xd"}
    assert _texts(ins, expect) == expect
    w = [i.addr for i in ins.func_insns("seg10:0xa332")
         if i.mem is not None and i.mem.regs == ("bp",) and i.mem.disp == 0xFF96 and is_write(i)]
    assert w == ["seg10:0xa3c9"]
    assert [chr(c) for c in range(0x20, 0x7F) if _dispatch(ins, c) == "seg10:0xa6e3"] == ["N", "n"]
    assert _eng(797) == " AND"


def test_formatter_caret_cut(ins):
    """shell.md Other directives, pitfalls.md Subtract the buffer's base: %S loads into
    [bp - 0x68] and tests and cuts [bp + si - 0x6a] = buf[len - 2]; 46 ENG_LANG.DLL strings end
    in '^' plus a letter and none in a bare '^'."""
    from isre.res_formats import decode_string_block

    expect = {"seg10:0xa4d8": "lea ax, [bp - 0x68]", "seg10:0xa4dd": "push 0x50",
              "seg10:0xa4f5": "mov si, ax", "seg10:0xa4f7": "cmp byte ptr [bp + si - 0x6a], 0x5e",
              "seg10:0xa50b": "mov byte ptr [bp + si - 0x6a], 0"}
    assert _texts(ins, expect) == expect
    assert 0x6A - 0x68 == 2
    m = _ne("ENG_LANG.dl1")
    texts = [t for r in m.resources if r.type_id == 6
             for _, t in decode_string_block(r.id, m.data[r.file_off:r.file_off + r.length])]
    assert sum(len(t) >= 2 and t[-2] == "^" for t in texts) == 46
    assert not any(t.endswith("^") for t in texts)


# ------------------------------------------------------------------ shell.md State 5


def _reach(m, root: str) -> set[str]:
    """The functions that ``root`` reaches through direct calls and tail jumps."""
    seen, todo = set(), [root]
    while todo:
        f = todo.pop()
        if f in seen or f not in m.funcs:
            continue
        seen.add(f)
        todo.extend(t for t, _ in m.funcs[f]["calls"] + m.funcs[f]["tail_jumps"])
    return seen


def test_unregistered_death_clock(ins):
    """shell.md State 5, Its path: the nine functions that write DGROUP:0x1e50 are out of reach
    of the MessageBox routine seg16:0x68d6 and of seg6:0x5d58, which ends with DGROUP:0x1e7c =
    DGROUP:0x1e50 + 34000 after its seg6:0x5f7a(1)."""
    sites = sorted({i.addr for i in ins.writers(0x1E50, 0x1E51)}, key=A)
    assert sites == ["seg7:0x24", "seg9:0x685e", "seg10:0x7f60", "seg10:0x83ef", "seg10:0x8612",
                     "seg15:0x5814", "seg15:0x806c", "seg17:0x60ff", "seg17:0x640a"]
    owners = {f for a in sites for f in ins.owners(*A(a))}
    assert owners == {"seg7:0x0", "seg9:0x6624", "seg10:0x7f18", "seg10:0x82a2", "seg10:0x8568",
                      "seg15:0x5786", "seg15:0x7cda", "seg17:0x60d8", "seg17:0x6116"}
    for root in ("seg16:0x68d6", "seg6:0x5d58"):
        assert not _reach(ins, root) & owners, root
    assert {"seg6:0x5f7a", "seg6:0x6576", "seg6:0x885c"} <= _reach(ins, "seg6:0x5d58")
    expect = {"seg6:0x5f5e": "mov ax, word ptr [0x1e50]", "seg6:0x5f65": "add ax, 0x84d0",
              "seg6:0x5f68": "adc dx, 0", "seg6:0x5f6b": "mov word ptr [0x1e7c], ax"}
    assert _texts(ins, expect) == expect and 0x84D0 == 34000
    assert _calls(ins, {"seg6:0x5f5a": "seg6:0x5f7a"})["seg6:0x5f5a"]
    assert _callers(ins, "seg6:0x5d58") == ["seg7:0x1e1"]


# ------------------------------------------------------------------ world.md


def test_offworld_path(ins):
    """world.md Ships at wave start, pitfalls.md Check ES: the off-world path stores +0x88 from
    GetCurrentTime and writes vx = 0 and vy = -30 to the pool-14 object through DGROUP:0x1e30."""
    expect = {"seg9:0x6386": "les bx, ptr [0x1e30]",
              "seg9:0x638a": "mov word ptr es:[bx + si + 0xc], 0",
              "seg9:0x6390": "les bx, ptr [0x1e30]",
              "seg9:0x6394": "mov word ptr es:[bx + si + 0xe], 0xffe2",
              "seg9:0x637e": "mov word ptr [bx + 0x7118], ax",
              "seg9:0x6382": "mov word ptr [bx + 0x711a], dx"}
    assert _texts(ins, expect) == expect
    assert _calls(ins, {"seg9:0x634d": "GetCurrentTime"})["seg9:0x634d"]
    assert 0x7118 - 0x7090 == 0x88


def test_rating_quotient_and_clamp(ins):
    """world.md Ship rating: a = max(1, low word of seg5:0x110((m + t)·q, 86)) with m + t a
    16-bit add; the kill value keeps a negative product's low word and clamps above 0x7fbc."""
    expect = {"seg13:0x34ba": "add ax, word ptr [bp - 0x10]", "seg13:0x34bd": "imul word ptr [bp - 6]",
              "seg13:0x34c7": "cmp ax, 1", "seg13:0x34ca": "jge seg13:0x34cf",
              "seg13:0x365a": "imul cx", "seg13:0x365c": "or dx, dx", "seg13:0x365e": "jl seg13:0x366a",
              "seg13:0x3660": "jg seg13:0x3667", "seg13:0x3662": "cmp ax, 0x7fbc",
              "seg13:0x3665": "jbe seg13:0x366a", "seg13:0x3667": "mov ax, 0x7fbc",
              "seg13:0x366a": "mov word ptr [bx + 0x7168], ax"}
    assert _texts(ins, expect) == expect and 0x7FBC == 32700
    assert _calls(ins, {"seg13:0x34c2": "seg5:0x110"})["seg13:0x34c2"]
    assert _imm_pushes(ins, "seg13:0x34c2", 4)[2:] == [86, 0]


def test_briefing_sheet_path(ins):
    """world.md Ship sheets, Path: seg9:0x5cca raises DGROUP:0x91f8 around seg9:0x6624, which
    calls seg14:0x0(-1); seg14:0x0 calls seg16:0x3784(i, 1) on its flag or DGROUP:0x91f8 = 1;
    seg16:0x3784 calls seg16:0x3468(i, 3), which reaches seg14:0x11a8."""
    expect = {"seg9:0x5e58": "inc word ptr [0x91f8]", "seg9:0x5e60": "dec word ptr [0x91f8]",
              "seg9:0x6888": "push -1", "seg14:0xd1a": "cmp word ptr [bp + si - 0x54], 0",
              "seg14:0xd20": "cmp word ptr [0x91f8], 1", "seg14:0xd2d": "push 1",
              "seg16:0x38cc": "push 3", "seg16:0x38d5": "add sp, 4"}
    assert _texts(ins, expect) == expect
    assert all(_calls(ins, {"seg9:0x5e5d": "seg9:0x6624", "seg9:0x688a": "seg14:0x0",
                            "seg14:0xd32": "seg16:0x3784", "seg16:0x38d2": "seg16:0x3468",
                            "seg16:0x362b": "seg14:0x11a8"}).values())


# ------------------------------------------------------------------ sound.md


def test_descent_ignite(ins):
    """sound.md The descent: seg15:0x374c plays 0x20 when DGROUP:0x17fc = 0 and differs from
    DGROUP:0x17f8, then copies DGROUP:0x17fc into DGROUP:0x17f8."""
    expect = {"seg15:0x3988": "cmp word ptr [0x17fc], 0", "seg15:0x398d": "jne seg15:0x39a0",
              "seg15:0x398f": "mov ax, word ptr [0x17f8]", "seg15:0x3992": "cmp word ptr [0x17fc], ax",
              "seg15:0x3996": "je seg15:0x39a0", "seg15:0x3998": "push 0x20",
              "seg15:0x39a0": "mov ax, word ptr [0x17fc]", "seg15:0x39a3": "mov word ptr [0x17f8], ax"}
    assert _texts(ins, expect) == expect
    assert _calls(ins, {"seg15:0x399a": "seg10:0x744c"})["seg15:0x399a"]
    assert ins.owners(*A("seg15:0x3988")) == {"seg15:0x374c"}


def test_player_damage_bounce_is_dead(ins, dgroup):
    """sound.md Wave events: DGROUP:0xb0a is 0 in the file and its only writer stores 0, so
    seg8:0x45e0 takes the seg10:0x7944 hit sound and never the fixed 0x07."""
    assert [(i.addr, ins.stored_value(i)) for i in ins.writers(0xB0A, 0xB0B)] == [("seg8:0x4d43", 0)]
    assert dgroup[0xB0A:0xB0C] == b"\0\0"
    assert all(_calls(ins, {"seg8:0x4960": "seg10:0x754a", "seg8:0x49dc": "seg10:0x7944"}).values())


# ------------------------------------------------------------------ pitfalls.md


def test_credit_tests_use_the_ship_pool_word(ins):
    """pitfalls.md The ship pool is DGROUP:0x9324: seg8:0x5df2's credit tests load the word."""
    expect = {"seg8:0x60c3": "mov ax, word ptr [0x9324]", "seg8:0x60c6": "cmp word ptr [bp - 0x10], ax",
              "seg8:0x60ce": "mov ax, word ptr [0x9324]", "seg8:0x6159": "mov ax, word ptr [0x9324]",
              "seg8:0x6174": "cmp word ptr [bp - 0x10], ax"}
    assert _texts(ins, expect) == expect


def test_ship_field_from_record_base(ins):
    """pitfalls.md Check each struct field against the stride: the credit store at seg8:0x60fe
    writes [bx + 0x7134] with bx = 0x15a·ship, ship field +0xa4."""
    expect = {"seg8:0x60d9": "imul bx, word ptr [bp - 0x12], 0x15a",
              "seg8:0x60fe": "mov word ptr [bx + 0x7134], ax", "seg8:0x6102": "mov word ptr [bx + 0x7136], dx"}
    assert _texts(ins, expect) == expect
    assert 0x7134 - 0x7090 == 0xA4


def test_pascal_examples(ins):
    """pitfalls.md Calls are written in declaration order: seg8:0x1290 ends with retf 8 and
    gets the mass dword first and 25 last; seg13:0x2e60 ends with retf 6."""
    expect = {"seg8:0x131c": "retf 8", "seg13:0x2f2c": "retf 6",
              "seg8:0x5db5": "mov ax, word ptr es:[bx + si + 0x44]", "seg8:0x5dba": "push dx",
              "seg8:0x5dbb": "push ax", "seg8:0x5dbc": "push 0", "seg8:0x5dbe": "push 0x19"}
    assert _texts(ins, expect) == expect
    assert _calls(ins, {"seg8:0x5dc0": "seg8:0x1290"})["seg8:0x5dc0"]


def test_tank_respawns(ins):
    """pitfalls.md A routine's only call site: seg12:0x4f4e is called only from seg12:0x3ee5,
    and seg7:0xe78 calls the second respawn seg12:0x515e in kind-7 waves."""
    assert ins.call_sites("seg12:0x4f4e") == ["seg12:0x3ee5"]
    expect = {"seg12:0x3ea8": "cmp word ptr [0x9320], 6", "seg7:0xe71": "cmp word ptr [0x9320], 7",
              "seg7:0xe76": "jne seg7:0xe7d"}
    assert _texts(ins, expect) == expect
    assert _calls(ins, {"seg7:0xe78": "seg12:0x515e"})["seg7:0xe78"]
