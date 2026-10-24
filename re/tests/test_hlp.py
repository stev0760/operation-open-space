"""Tests for isre.hlp, the WinHelp 3.1 extractor.

The synthetic tests always run. The tests on the two Inner Space manuals skip
when ``original/`` is absent; the helpdeco cross-check also skips when no
helpdeco binary is built under ``work/tools/helpdeco/``.
"""
from __future__ import annotations

import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path

import pytest

from isre import hlp

REPO_ROOT = Path(__file__).resolve().parents[2]
ORIGINAL = REPO_ROOT / "original"
HLP_1998 = ORIGINAL / "theworks" / "ENG_HELP.HLP"
HLP_2000 = ORIGINAL / "shipdisk" / "Eng_help.hl1"
HELPDECO = REPO_ROOT / "work" / "tools" / "helpdeco" / "gcc" / "helpdeco"


def _require(path: Path) -> Path:
    if not path.is_file():
        pytest.skip(f"{path.relative_to(REPO_ROOT)} is absent (original game files are local only)")
    return path


@pytest.fixture(scope="module")
def help1998() -> hlp.HelpFile:
    return hlp.open_help(_require(HLP_1998))


@pytest.fixture(scope="module")
def help2000() -> hlp.HelpFile:
    return hlp.open_help(_require(HLP_2000))


def _topic(hf: hlp.HelpFile, context: str) -> hlp.Topic:
    for t in hf.topics:
        if context in t.contexts:
            return t
    raise AssertionError(f"no topic with context {context}")


# ---------------------------------------------------------------- synthetic

def test_compressed_integers():
    assert hlp.cu16(bytes([0x0A]), 0) == (5, 1)
    assert hlp.cu16(struct.pack("<H", (300 << 1) | 1), 0) == (300, 2)
    assert hlp.ci16(bytes([0x80]), 0) == (0, 1)              # 0x80 >> 1 = 64, minus 64
    assert hlp.ci16(bytes([0x00]), 0) == (-64, 1)
    assert hlp.cu32(struct.pack("<H", 0x0008), 0) == (4, 2)
    assert hlp.cu32(struct.pack("<I", (70000 << 1) | 1), 0) == (70000, 4)
    assert hlp.ci32(struct.pack("<H", 0x8000), 0) == (0, 2)  # 0x4000 - 0x4000
    assert hlp.ci32(struct.pack("<H", 0x0000), 0) == (-0x4000, 2)
    assert hlp.ci32(struct.pack("<I", ((0x40000000 - 5) << 1) | 1), 0) == (-5, 4)


def test_lz77_literals_and_back_reference():
    # flags 0b0100: 2 literals, then a reference (distance 2, length 3), then literal 'z'
    ref = ((3 - 3) << 12) | (2 - 1)
    data = bytes([0b0100]) + b"ab" + struct.pack("<H", ref) + b"z"
    assert hlp.lz77_expand(data) == b"ababaz"


def test_runlen():
    assert hlp.runlen_expand(bytes([0x83]) + b"xyz" + bytes([0x04]) + b"q") == b"xyzqqqq"


def test_context_hash():
    assert hlp.context_hash("") == 1
    # hash = hash * 43 + table[c], 32-bit wrap-around
    assert hlp.context_hash("A") == 0x11
    assert hlp.context_hash("AB") == (0x11 * 43 + 0x12) & 0xFFFFFFFF
    assert hlp.context_hash("ab") == hlp.context_hash("AB")  # case-insensitive table


def test_markdown_escape_keeps_ini_text_greppable():
    assert hlp._md_escape("[Inner Space] section") == "[Inner Space] section"
    assert hlp._md_escape("C:\\SPACE\\SHIPS") == "C:\\SPACE\\SHIPS"
    assert hlp._md_escape("Sound_On") == "Sound_On"
    assert hlp._md_escape("<520") == "<520"
    assert hlp._md_escape("a*b") == "a\\*b"
    assert hlp._md_escape("[x](y)") == "\\[x](y)"


def _bare_reader() -> hlp.HelpFile:
    hf = object.__new__(hlp.HelpFile)
    hf.errors = []
    return hf


def _record_head(text_len: int) -> bytes:
    """TopicSize (compressed signed long) and TopicLength (compressed unsigned short)."""
    return struct.pack("<H", (text_len + 0x4000) << 1) + bytes([text_len << 1])


def test_display_record_runs():
    # one paragraph: "Go " [jump "here"] TAB "x" BR "y", then end of paragraph
    para_info = bytes(4) + struct.pack("<H", 0)
    fmt = (bytes([0xE3]) + struct.pack("<I", 0x1234) + bytes([0x89, 0x83, 0x81, 0x82, 0xFF]))
    ld1 = _record_head(12) + para_info + fmt
    ld2 = b"Go \0here\0\0x\0y\0\0"
    topic = hlp.Topic(index=1, number=0, offset=0, pos=12)
    _bare_reader()._parse_display(topic, hlp.TL_DISPLAY, ld1, ld2, False)
    (para,) = topic.blocks
    assert [r.kind for r in para.runs] == ["text", "text", "tab", "text", "br", "text"]
    assert para.plain() == "Go here\tx\ny"
    assert para.runs[1].link.hash == 0x1234 and para.runs[1].link.kind == "jump"
    assert para.runs[0].link is None and para.runs[3].link is None


def test_table_record_cells():
    cols = struct.pack("<4h", 0, 100, 10, 100)          # (gap, width) per column
    head = _record_head(4) + bytes([2, 1]) + cols       # 2 columns, TableType 1
    cell = bytes(5) + bytes(4) + struct.pack("<H", 0) + bytes([0x82, 0xFF])
    ld1 = head + struct.pack("<h", 0) + cell[2:] + struct.pack("<h", 1) + cell[2:] + struct.pack("<h", -1)
    ld2 = b"A\0\0B\0\0"
    topic = hlp.Topic(index=1, number=0, offset=0, pos=12)
    _bare_reader()._parse_display(topic, hlp.TL_TABLE, ld1, ld2, False)
    (row,) = topic.blocks
    assert isinstance(row, hlp.TableRow)
    assert [[p.plain() for p in c] for c in row.cells] == [["A"], ["B"]]


def test_rejects_non_help_file():
    with pytest.raises(hlp.HlpError):
        hlp.HelpFile(b"\0" * 64)


# ---------------------------------------------------------------- manuals

def test_system_records(help1998, help2000):
    for hf in (help1998, help2000):
        assert (hf.system.minor, hf.system.major, hf.system.flags) == (21, 1, 0)
        assert not hf.lz77 and hf.block_size == 4096
        assert hf.system.config == ["BrowseButtons()"]
    assert help1998.system.title == "Enhanced Inner Space Help"
    assert help1998.system.build_date.startswith("1998-08-31")
    assert help2000.system.title == "Basic Inner Space Help"
    assert help2000.system.build_date.startswith("2000-01-13")
    assert "1994-2000 Software Dynamics" in help2000.system.copyright


def test_topic_counts(help1998, help2000):
    # |TOPIC holds 41 / 38 topic-header records. The last one is the empty
    # terminal record (no title, no text, NextBlock -1), so 40 / 37 topics
    # carry content; helpdeco also writes 40 / 37 topics.
    assert help1998.topic_headers == 41
    assert help2000.topic_headers == 38
    assert len(help1998.topics) == 40
    assert len(help2000.topics) == 37
    assert help1998.terminal_record == help2000.terminal_record == hlp.TL_TOPICHDR
    # |TTLBTREE lists the terminal record too, with an empty title
    assert len(help1998.titles) == 41 and len(help2000.titles) == 38


def test_no_parse_errors(help1998, help2000):
    assert help1998.errors == []
    assert help2000.errors == []


def test_titles_match_ttlbtree(help1998, help2000):
    for hf in (help1998, help2000):
        for t in hf.topics:
            assert hf.titles[t.offset] == t.title


def test_contexts_and_keywords_land_on_topic_starts(help1998, help2000):
    for hf in (help1998, help2000):
        starts = {t.offset for t in hf.topics}
        assert all(off in starts for _, off in hf.contexts)
        assert all(off in starts for kw in hf.keywords for off in kw.offsets)


def test_context_names_recovered(help1998, help2000):
    assert len(help1998.contexts) == 39
    assert len(help2000.contexts) == 36
    for hf in (help1998, help2000):
        names = [hf.context_name(h) for h, _ in hf.contexts]
        assert not [n for n in names if n.startswith("#")]
        assert all(hlp.context_hash(hf.context_name(h)) == h for h, _ in hf.contexts)
        assert hf.topics[0].contexts == ["idh_Contents"]


def test_untitled_race_records_topic(help1998, help2000):
    # Topic 14 has no title and no context id; it holds the RacePath INI key.
    for hf in (help1998, help2000):
        t = hf.topics[13]
        assert t.title == "" and t.contexts == []
        assert "RacePath=(path)" in t.plain()


def test_help_context_map(help1998, help2000):
    m98 = dict(help1998.ctxomap)
    m00 = dict(help2000.ctxomap)
    assert sorted(m98) == sorted(m00) == list(range(1, 41))
    assert m98[19] == -1 and m00[19] == -1   # map id 19 points at no topic
    for mid in (38, 39, 40):                 # Ship Factory topics, dropped in 2000
        assert help1998.topic_at(m98[mid]) is not None
        assert m00[mid] == -1
    assert help1998.topic_at(m98[1]).title == "Contents"
    assert help2000.topic_at(m00[29]).title == "Laws of Inner Space"


def test_known_strings(help1998, help2000):
    for hf in (help1998, help2000):
        assert "Resisting Arrest" in _topic(hf, "Laws_of_Inner_Space").plain()
        assert "Navigation window" in _topic(hf, "General").plain()
        assert "Calibrate=xxxx" in _topic(hf, "Optimizing_for_Your_Computer").plain()
        assert "36 frames/second" in _topic(hf, "Game_Settings").plain()
        assert '"AllowSound=1"' in _topic(hf, "Troubleshooting").plain()
        assert '"Lunch=1300"' in _topic(hf, "Special_Times_of_Day").plain()
    assert "BitmapEditor" in _topic(help1998, "Editing_Ship_Bitmaps").plain()


def test_pictures(help1998, help2000):
    assert sorted(help1998.pictures) == ["bm0", "bm1", "bm2", "bm3", "bm4"]
    assert help2000.pictures == {}
    sizes = {n: (p.width, p.height, p.bit_count) for n, p in help1998.pictures.items()}
    assert sizes == {"bm0": (450, 333, 4), "bm1": (429, 119, 8), "bm2": (452, 236, 4),
                     "bm3": (241, 90, 4), "bm4": (190, 231, 4)}
    for p in help1998.pictures.values():
        assert (p.ptype, p.packing) == (6, 1)   # RunLen-packed DIBs
        bmp = p.to_bmp()
        assert bmp[:2] == b"BM" and struct.unpack_from("<I", bmp, 2)[0] == len(bmp)
        w, h = struct.unpack_from("<ii", bmp, 18)
        assert (w, h) == (p.width, p.height)
    used = {n for t in help1998.topics for n in t.bitmaps}
    assert used == set(help1998.pictures)


def test_szdd_fallback_matches_isre_szdd(monkeypatch):
    raw = _require(HLP_2000).read_bytes()
    assert raw[:4] == b"SZDD"
    expected = hlp.szdd_expand(raw)
    import isre
    monkeypatch.setitem(sys.modules, "isre.szdd", None)   # force the inline expander
    monkeypatch.delattr(isre, "szdd", raising=False)
    assert hlp.szdd_expand(raw) == expected


def test_extract_writes_markdown_and_bitmaps(tmp_path, help1998):
    hlp.extract(help1998, tmp_path)
    names = sorted(p.name for p in tmp_path.iterdir())
    assert "index.md" in names
    assert len([n for n in names if re.match(r"\d\d-.*\.md$", n)]) == 40
    assert [n for n in names if n.endswith(".bmp")] == [f"bm{i}.bmp" for i in range(5)]
    index = (tmp_path / "index.md").read_text(encoding="utf-8")
    assert "| 19 | (no topic: offset -1) |" in index
    laws = (tmp_path / "34-laws-of-inner-space.md").read_text(encoding="utf-8")
    assert laws.startswith("# Laws of Inner Space\n")
    assert "Resisting Arrest" in laws
    contents = (tmp_path / "01-contents.md").read_text(encoding="utf-8")
    assert "[Laws of Inner Space](34-laws-of-inner-space.md)" in contents
    factory = (tmp_path / "16-ship-factory.md").read_text(encoding="utf-8")
    assert "![bm0](bm0.bmp)" in factory


def test_main_cli(tmp_path, capsys):
    _require(HLP_2000)
    assert hlp.main([str(HLP_2000), str(tmp_path / "out")]) == 0
    assert (tmp_path / "out" / "index.md").is_file()
    assert hlp.main([str(HLP_2000)]) == 0
    assert "37 topics" in capsys.readouterr().out
    assert hlp.main([str(tmp_path / "missing.hlp")]) == 2


# ---------------------------------------------------------------- helpdeco

_RTF_SKIP = ("footnote", "up", "v", "fonttbl", "colortbl", "stylesheet", "*", "field")


def _rtf_topics(text: str) -> list[str]:
    """Plain text per topic from helpdeco's RTF (topics end with \\page)."""
    out: list[str] = []
    cur: list[str] = []
    skip = [False]
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "{":
            skip.append(skip[-1])
            m = re.match(r"\\([a-z]+|\*)", text[i + 1:])
            if m and m.group(1) in _RTF_SKIP:
                skip[-1] = True
            i += 1
        elif c == "}":
            skip.pop()
            i += 1
        elif c == "\\":
            m = re.match(r"\\([a-z]+)(-?\d+)? ?", text[i:])
            if m:
                i += m.end()
                if not skip[-1]:
                    word = m.group(1)
                    if word in ("par", "line"):
                        cur.append("\n")
                    elif word == "tab":
                        cur.append("\t")
                    elif word == "page":
                        out.append("".join(cur))
                        cur = []
                continue
            nxt = text[i + 1]
            if nxt == "'":
                if not skip[-1]:
                    cur.append(bytes([int(text[i + 2:i + 4], 16)]).decode("cp1252", "replace"))
                i += 4
                continue
            if not skip[-1]:
                cur.append(" " if nxt == "~" else nxt if nxt in "{}\\" else "")
            i += 2
        else:
            if c not in "\r\n" and not skip[-1]:
                cur.append(c)
            i += 1
    if "".join(cur).strip():
        out.append("".join(cur))
    return out


def _norm(s: str) -> str:
    s = re.sub(r"\{(bm[clr]t?|ew[clr]) [^}]*\}", " ", s)
    return re.sub(r"\s+", " ", s).strip()


@pytest.mark.parametrize("path", [HLP_1998, HLP_2000], ids=["1998", "2000"])
def test_text_matches_helpdeco(tmp_path, path):
    if not HELPDECO.is_file():
        pytest.skip("helpdeco is not built under work/tools/helpdeco/gcc/")
    hf = hlp.open_help(_require(path))
    src = tmp_path / "MANUAL.HLP"
    src.write_bytes(hlp.load_bytes(path))
    subprocess.run([str(HELPDECO), src.name], cwd=tmp_path, capture_output=True, timeout=120)
    rtf = tmp_path / "MANUAL.rtf"
    assert rtf.is_file()
    text = rtf.read_text(encoding="latin-1")
    topics = _rtf_topics(text)
    assert len(topics) == len(hf.topics)
    for t, r in zip(hf.topics, topics):
        assert _norm(t.plain()) == _norm(r), f"topic {t.index} {t.title!r}"
    titles = re.findall(r"\{\\up \$\}\{\\footnote\\pard\\plain\{\\up \$\} ([^}]*)\}", text)
    assert titles == [t.title for t in hf.topics if t.title]
    shutil.rmtree(tmp_path, ignore_errors=True)
