from __future__ import annotations

import struct

import pytest

from isre import szdd


def make_szdd(stream: bytes, size: int, missing: int = 0) -> bytes:
    return szdd.SZDD_MAGIC + b"A" + bytes([missing]) + struct.pack("<I", size) + stream


# ---------------------------------------------------------------- synthetic

def test_literals():
    assert szdd.expand(make_szdd(b"\xff" + b"ABCDEFGH", 8)) == b"ABCDEFGH"


def test_match_copies_from_window_start():
    # 3 literals, then a match at window position 0xFF0 (the start position), length 3
    stream = bytes([0b0111]) + b"abc" + bytes([0xF0, 0xF0])
    assert szdd.expand(make_szdd(stream, 6)) == b"abcabc"


def test_window_starts_as_spaces():
    stream = bytes([0b0000]) + bytes([0x00, 0x02])  # offset 0, length 5
    assert szdd.expand(make_szdd(stream, 5)) == b"     "


def test_overlapping_match():
    stream = bytes([0b01]) + b"x" + bytes([0xF0, 0xF3])  # offset 0xFF0, length 6
    assert szdd.expand(make_szdd(stream, 7)) == b"x" * 7


def test_short_stream_raises():
    with pytest.raises(szdd.SzddError):
        szdd.expand(make_szdd(b"\xff" + b"ABC", 8))


def test_not_szdd_raises():
    with pytest.raises(szdd.SzddError):
        szdd.expand(b"MZ" + bytes(40))


def test_restored_names():
    assert szdd.restored_name("MAIN.MI1") == "MAIN.MID"
    assert szdd.restored_name("ENG_LANG.dl1") == "ENG_LANG.DLL"
    assert szdd.restored_name("Eng_help.hl1") == "Eng_help.HLP"
    assert szdd.restored_name("INSPACE.sc1") == "INSPACE.SCR"
    assert szdd.restored_name("INRUN.A1") == "INRUN.AD"
    assert szdd.restored_name("INSPACE.ex_") == "INSPACE.EXE"
    assert szdd.restored_name("README.TXT") == "README.TXT"
    hdr = szdd.SzddHeader(0x41, ord("X"), 1)
    assert szdd.restored_name("FOO.EX_", hdr) == "FOO.EXE"  # table wins
    assert szdd.restored_name("FOO.TX_", hdr) == "FOO.TXX"


# ---------------------------------------------------------------- original files

def _szdd_files(original_dir):
    return [p for sub in ("game", "theworks") for p in sorted((original_dir / sub).iterdir())
            if p.is_file() and szdd.is_szdd(p.read_bytes()[:8])]


def test_every_szdd_file_expands_to_declared_size(original_dir):
    files = _szdd_files(original_dir)
    assert len(files) == 14
    for p in files:
        data = p.read_bytes()
        hdr = szdd.parse_header(data)
        assert len(szdd.expand(data)) == hdr.size, p.name


def test_expanded_content_matches_restored_type(game):
    seen = set()
    for p in _szdd_files(game.parent):
        out = szdd.expand(p.read_bytes())
        ext = szdd.restored_name(p.name).rsplit(".", 1)[1]
        seen.add(ext)
        if ext == "MID":
            assert out[:4] == b"MThd", p.name
        elif ext in ("DLL", "SCR", "AD"):
            (ne,) = struct.unpack_from("<I", out, 0x3C)
            assert out[:2] == b"MZ" and out[ne:ne + 2] == b"NE", p.name
        elif ext == "HLP":
            assert out[:4] == b"\x3f\x5f\x03\x00", p.name
        elif ext == "INI":
            assert out.lstrip()[:1] == b"[", p.name
        elif ext == "BOD":
            assert len(out) == 52 * 44 + 16, p.name
        elif ext == "SHP":
            assert len(out) == 118 * 65 + 10, p.name
    assert seen == {"MID", "DLL", "SCR", "AD", "HLP", "INI", "BOD", "SHP"}


def test_restored_names_are_listed_in_install_ini(game):
    ini = (game / "INSTALL.INI").read_text("latin-1")
    listed = {line.split("=", 1)[0].strip().upper() for line in ini.splitlines() if "=" in line}
    renamed = [p.name for p in sorted(game.iterdir()) if szdd.restored_name(p.name) != p.name]
    assert len(renamed) == 16  # 14 SZDD files + INSPACE.ex_ + FACTORY.ex_
    for name in renamed:
        assert szdd.restored_name(name).upper() in listed, name


def test_extract_dir(game, tmp_path):
    files = szdd.extract_dir(game, tmp_path)
    src_names = sorted(p.name for p in game.iterdir() if p.is_file())
    assert sorted(f.source for f in files) == src_names
    assert sum(f.szdd for f in files) == 14
    assert (tmp_path / "INSPACE.EXE").read_bytes() == (game / "INSPACE.ex_").read_bytes()
    assert (tmp_path / "FACTORY.EXE").read_bytes() == (game / "FACTORY.ex_").read_bytes()
    for out_name, src_name in (("Eng_help.HLP", "Eng_help.hl1"), ("WAVEMIX.INI", "WAVEMIX.IN1")):
        src = (game / src_name).read_bytes()
        assert (tmp_path / out_name).read_bytes() == szdd.expand(src)
    again = szdd.extract_dir(game, tmp_path)
    assert not any(f.written for f in again)
