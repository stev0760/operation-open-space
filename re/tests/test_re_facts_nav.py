"""Mechanical facts of docs/re/nav.md and of the IS_NAV items in docs/re/shell.md
(M3 stage 3 verification round).

The listing model, the fixtures and the helpers come from test_re_facts.py; this
file follows its conventions (references, writers, ``segN:0xOFF`` addresses) and
skips when ``work/`` or ``original/`` is absent.
"""
from __future__ import annotations

import re

from test_re_facts import (  # noqa: F401  (ins and dgroup are pytest fixtures)
    _cstr, _eng, _texts, dgroup, ins, is_write,
)

_PUSH_IMM = re.compile(r"push (0x[0-9a-f]+|\d+)$")


def _imm(i) -> int | None:
    m = _PUSH_IMM.fullmatch(i.text)
    return int(m.group(1), 0) if m else None


def _region(m, lo: str, hi: str):
    a, b = m.at(lo), m.at(hi)
    return [i for i in m.insns if i.seg == a.seg and a.off <= i.off <= b.off]


def test_nav_strings():
    """nav.md Race and duel boxes, shell.md item 11: the captions and combo strings 2143-2169."""
    assert {s: _eng(s) for s in range(2143, 2170)} == {
        2143: "&Enter", 2144: "&Help", 2145: "RACE", 2146: "DUEL",
        2147: "Solo", 2148: "2 Ships", 2149: "3 Ships", 2150: "4 Ships",
        2151: "1 Opponent", 2152: "2 Opponents", 2153: "3 Opponents",
        2154: "Normal", 2155: "Difficult", 2156: "Dangerous!",
        2157: "1 Lap", 2158: "5 Laps", 2159: "10 Laps",
        2160: "Fastest", 2161: "Faster", 2162: "Similar", 2163: "Same Ships", 2164: "Slower",
        2165: "Slowest", 2166: "Toughest", 2167: "Tougher", 2168: "Weaker", 2169: "Weakest"}


def _nav_controls(ins):
    """Walk seg6:0x23d4's control creation in order: per created control id, its style
    word, its caption id, the ids added with CB_ADDSTRING (0x403) and the WM_SETFONT lParam."""
    out: dict[int, dict] = {}
    body = _region(ins, "seg6:0x2562", "seg6:0x2d92")
    last_id = cur = loaded = None
    for n, i in enumerate(body):
        v = _imm(i)
        if v is not None:
            if 0x8C <= v <= 0x94 and "0x8e66" in body[n + 1].text:
                last_id = v
            elif 2145 <= v <= 2169:
                loaded = v
            elif v == 0x403 and cur is not None and loaded is not None:
                out[cur]["strings"].append(loaded)
                loaded = None
        elif "CreateWindow" in i.text:
            cur = last_id
            style = next(int(j.ops[0], 0) for j in reversed(body[max(0, n - 20):n])
                         if j.text in ("push 0x5000", "push 0x5021"))
            out[cur] = {"style_hi": style, "caption": loaded, "strings": [], "font": None}
            loaded = None
        elif i.text == "push word ptr [0x90fe]" and body[n - 1].text == "push 0x30":
            out[cur]["font"] = _imm(body[n + 2])
    return out


def test_nav_race_duel_controls(ins):
    """C011, C012: group boxes 0x8c/0x93 and the seven combos, their strings and WM_SETFONT."""
    c = _nav_controls(ins)
    assert list(c) == [0x8C, 0x93, 0x8D, 0x91, 0x8E, 0x92, 0x8F, 0x90, 0x94]
    assert {k: v["caption"] for k, v in c.items()} == {
        0x8C: 2145, 0x93: 2146, **{k: None for k in (0x8D, 0x91, 0x8E, 0x92, 0x8F, 0x90, 0x94)}}
    assert {k: v["strings"] for k, v in c.items() if k not in (0x8C, 0x93)} == {
        0x8D: [2147, 2148, 2149, 2150], 0x91: [2151, 2152, 2153],
        0x8E: [2154, 2155, 2156], 0x92: [2154, 2155, 2156], 0x8F: [2157, 2158, 2159],
        0x90: [2160, 2161, 2162, 2163, 2164, 2165], 0x94: [2166, 2167, 2162, 2163, 2168, 2169]}
    assert {k: v["style_hi"] for k, v in c.items()} == {
        **{k: 0x5000 for k in (0x8C, 0x93)},
        **{k: 0x5021 for k in (0x8D, 0x91, 0x8E, 0x92, 0x8F, 0x90, 0x94)}}
    assert {k: v["font"] for k, v in c.items()} == {
        **{k: 1 for k in (0x8C, 0x93)},
        **{k: 0 for k in (0x8D, 0x91, 0x8E, 0x92, 0x8F, 0x90, 0x94)}}
    body = " | ".join(i.text for i in ins.func_insns("seg6:0x23d4"))
    assert body.count("push 0x30 | push word ptr [0x90fe]") == 9


def test_nav_text_height_and_box_origin(ins, dgroup):
    """C013: DGROUP:0x94b8 = GetTextExtent("Test", 4) height; WM_SIZE y0 = client bottom - 0x9b."""
    expect = {"seg6:0x2642": "push word ptr [0x90fe]", "seg6:0x2692": "mov word ptr [0x94b8], ax",
              "seg6:0x211b": "mov ax, word ptr [bp - 0x32]", "seg6:0x211e": "sub ax, 0x9b",
              "seg6:0x2121": "mov word ptr [bp - 0x28], ax"}
    assert _texts(ins, expect) == expect
    assert "GetTextExtent" in ins.at("seg6:0x2658").text
    assert _cstr(dgroup, 0xD51) == b"Test"
    assert [i.addr for i in ins.writers(0x94B8, 0x94B9)] == ["seg6:0x2692"]


def test_nav_window_geometry(ins):
    """C010: IS_NAV WM_CREATE and WM_SIZE child rectangles (W, H the client right and bottom)."""
    expect = {
        # WM_CREATE seg6:0x23d4: Enter (W - 90, 15, 90, 26), Help (W - 90, 51, 90, 26)
        "seg6:0x2437": "push 0x85f", "seg6:0x2457": "sub ax, 0x5a", "seg6:0x245b": "push 0xf",
        "seg6:0x245d": "push 0x5a", "seg6:0x245f": "push 0x1a", "seg6:0x2465": "push 1",
        "seg6:0x2482": "push 0x860", "seg6:0x24a2": "sub ax, 0x5a", "seg6:0x24a6": "push 0x33",
        "seg6:0x24b0": "push 0x70",
        # listbox 0x65 (0, 0, 160, H - 26) and 0x6b (164, 0, W - 264, H - 26)
        "seg6:0x24db": "push 0xa0", "seg6:0x24e1": "sub ax, 0x1a", "seg6:0x24e9": "push 0x65",
        "seg6:0x252d": "push 0xa4", "seg6:0x2535": "sub ax, 0x108", "seg6:0x253c": "sub ax, 0x1a",
        "seg6:0x2544": "push 0x6b",
        # WM_SIZE seg6:0x2079: Enter (W - 105, 5), Help (W - 105, 40), 0x65 (0, 0, 160, H - 26),
        # 0x6b (164, 0, W - 284, H - 26)
        "seg6:0x2098": "push 1", "seg6:0x20a3": "sub ax, 0x69", "seg6:0x20a7": "push 5",
        "seg6:0x20a9": "push 0x5a", "seg6:0x20ab": "push 0x1a",
        "seg6:0x20b8": "push 0x70", "seg6:0x20c3": "sub ax, 0x69", "seg6:0x20c7": "push 0x28",
        "seg6:0x20d8": "push 0x65", "seg6:0x20e4": "push 0xa0", "seg6:0x20ea": "sub ax, 0x1a",
        "seg6:0x20f9": "push 0x6b", "seg6:0x2101": "push 0xa4", "seg6:0x2109": "sub ax, 0x11c",
        "seg6:0x2110": "sub ax, 0x1a"}
    assert _texts(ins, expect) == expect
    assert (0x5A, 0x69, 0x108, 0x11C, 0x1A, 0xA4, 0xA0) == (90, 105, 264, 284, 26, 164, 160)
    assert (_eng(0x85F), _eng(0x860)) == ("&Enter", "&Help")


def test_nav_focus_rect_shift(ins):
    """C006: both focus rectangles shift left by (third-to-last character - '0') * 16 - 6,
    the depth digit, for control 0x65 and an item other than 0."""
    for base in (0x4E04, 0x4EA8):
        d = 0x4E28 - 0x4E04 if base == 0x4E04 else 0x4ED4 - 0x4EA8
        o = base + d
        expect = {f"seg6:{o:#x}": "cmp word ptr es:[bx + 2], 0x65",
                  f"seg6:{o + 0x36:#x}": "mov al, byte ptr [bp + si - 0x71]",
                  f"seg6:{o + 0x3a:#x}": "sub ax, 0x30",
                  f"seg6:{o + 0x40:#x}": "shl ax, 4", f"seg6:{o + 0x43:#x}": "sub ax, 6"}
        assert _texts(ins, expect) == expect
        # LB_GETTEXT buffer at bp - 0x6e; si = lstrlen, so bp + si - 0x71 is buf[len - 3]
        assert any(i.text == "lea ax, [bp - 0x6e]" for i in _region(ins, f"seg6:{o:#x}",
                                                                   f"seg6:{o + 0x36:#x}"))
    assert ins.at("seg6:0x4e7a").text == "cmp word ptr es:[bx + 4], -1"
    assert "DrawFocusRect" in ins.at("seg6:0x4e91").text
    assert ins.at("seg6:0x4f26").text == "test byte ptr es:[bx + 8], 0x10"
    assert "DrawFocusRect" in ins.at("seg6:0x4f36").text


def test_nav_initial_selection_search(ins):
    """C003: later components use LB_FINDSTRINGEXACT from DGROUP:0x94b4, then LB_FINDSTRING
    with the failed result -1 as start index."""
    expect = {"seg6:0x3ed4": "push 0x423", "seg6:0x3ed7": "push word ptr [0x94b4]",
              "seg6:0x3ee8": "cmp ax, 0xffff", "seg6:0x3eeb": "jne seg6:0x3f02",
              "seg6:0x3ef1": "push 0x410", "seg6:0x3ef4": "push ax",
              "seg6:0x3f06": "push 0x407"}
    assert _texts(ins, expect) == expect


HANDOVER_READERS = {
    0x9EA0: ["seg4:0x1f78", "seg4:0x1fef", "seg4:0x2026", "seg4:0x204f", "seg4:0x207f",
             "seg4:0x249a", "seg4:0x251e", "seg4:0x25a8", "seg4:0x25d0", "seg4:0x261d",
             "seg4:0x2640", "seg4:0x2b10", "seg4:0x2b7f", "seg4:0x2bb2", "seg4:0x2c51",
             "seg4:0x2cd8", "seg4:0x2d5d", "seg4:0x2d6c", "seg4:0x2dc8", "seg4:0x2e0e",
             "seg9:0x289d", "seg9:0x417d", "seg9:0x71f4", "seg9:0x76ca", "seg9:0x7c88",
             "seg11:0x41f7", "seg11:0x42cf", "seg11:0x4432", "seg11:0x73c8", "seg17:0x7a7a",
             "seg17:0x7aad"],
    0x94A8: ["seg4:0x2eb1", "seg4:0x2ed4", "seg9:0x4120", "seg9:0x4211", "seg9:0x601d",
             "seg11:0x48b6"],
    0x94AA: ["seg4:0x2ea9", "seg4:0x2ee1", "seg9:0x3bdc", "seg14:0x2793", "seg14:0x2ad6",
             "seg14:0x348f"],
}
HANDOVER_WRITERS = {
    0x9EA0: ["seg6:0x32a6", "seg6:0x32b3", "seg6:0x343e", "seg9:0x3ae5", "seg9:0x3afd"],
    0x94A8: ["seg6:0x3267", "seg6:0x339a", "seg6:0x34cf", "seg6:0x3571", "seg9:0x3bbd",
             "seg9:0x3c0e", "seg9:0x3cba"],
    0x94AA: ["seg6:0x326a", "seg6:0x33e4", "seg6:0x34d5", "seg6:0x35bb", "seg9:0x3b2d",
             "seg9:0x3b64", "seg9:0x3bb7", "seg9:0x3c27", "seg9:0x3c6d", "seg9:0x3c86",
             "seg9:0x3cb1"],
}


def test_nav_handover_references(ins):
    """C014: the readers and writers of DGROUP:0x9ea0, DGROUP:0x94a8 and DGROUP:0x94aa."""
    for lo in HANDOVER_READERS:
        refs = ins.refs(lo, lo + 1)
        assert all(i.mem.disp == lo and i.mem.size == 2 for i in refs)
        assert [i.addr for i in refs if not is_write(i)] == HANDOVER_READERS[lo]
        assert [i.addr for i in refs if is_write(i)] == HANDOVER_WRITERS[lo]
    assert {w for lo in HANDOVER_WRITERS for w in HANDOVER_WRITERS[lo] if w.startswith("seg6:")} \
        <= {i.addr for i in ins.func_insns("seg6:0x301a")}
    assert {w for lo in HANDOVER_WRITERS for w in HANDOVER_WRITERS[lo] if w.startswith("seg9:")} \
        <= {i.addr for i in ins.func_insns("seg9:0x3202")}


def test_nav_listbox_messages(ins):
    """C007, C008, C009: WM_SETREDRAW, LB_RESETCONTENT and LB_SETCURSEL in the pane and tree."""
    # seg6:0x5318: WM_SETREDRAW (0xb) and LB_RESETCONTENT (0x405) to 0x6b, SetFocus to 0x65
    body = [i.text for i in ins.func_insns("seg6:0x5318")]
    assert "push 0x405" in body and "push 0xb" in body and "push 0x6b" in body
    assert any("SetFocus" in t for t in body)
    # seg6:0x5524 rewrites the completed row without LB_SETCURSEL (0x407)
    rewrite = [i.text for i in _region(ins, "seg6:0x59ca", "seg6:0x5a72")]
    assert "push 0x403" in rewrite and "push 0x402" in rewrite and "push 0x407" not in rewrite
    # seg6:0x511c: LB_SETCURSEL inside its WM_SETREDRAW pair, before the child deletions
    coll = [i.text for i in ins.func_insns("seg6:0x511c")]
    assert "push 0x407" in coll and coll.count("push 0xb") == 2
    assert coll.index("push 0x407") < len(coll) - 1 - coll[::-1].index("push 0x403")


def test_nav_find_results_untested(ins):
    """C004, C032: seg6:0x4748 goes to find-next after a failed find-first; seg9:0x0 tests
    DGROUP:0x9206 > 9 after each successful find and before seg9:0x156."""
    expect = {"seg6:0x47ab": "mov word ptr [bp - 0x32], ax", "seg6:0x47b0": "jne seg6:0x47cf",
              "seg6:0x47d3": "call far seg5:0x352",
              "seg9:0x58": "call far seg5:0x364", "seg9:0x60": "jmp seg9:0x9a",
              "seg9:0x62": "cmp word ptr [0x9206], 9", "seg9:0x67": "jg seg9:0xa2",
              "seg9:0x8e": "call seg9:0x156", "seg9:0x94": "call far seg5:0x352",
              "seg9:0x9e": "or ax, ax", "seg9:0xa0": "je seg9:0x62"}
    assert _texts(ins, expect) == expect


def test_nav_status_line_jumps(ins):
    """C001, C002: the three locked built-in branches load 2323 (0x913); the complete case
    jumps past the 3028 (0xbd4) call to the 3029 (0xbd5) append."""
    assert [i.addr for i in ins.func_insns("seg6:0x5524") if i.text == "push 0x913"] == [
        "seg6:0x5634", "seg6:0x573e", "seg6:0x581b"]
    expect = {"seg6:0x5a72": "jmp seg6:0x5abc", "seg6:0x5aab": "push 0xbd4",
              "seg6:0x5abc": "cmp word ptr [bp - 0xc], 0", "seg6:0x5b17": "push 0xbd5"}
    assert _texts(ins, expect) == expect
