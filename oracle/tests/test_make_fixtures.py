"""make_fixtures.py: icon and PNG generation, deterministic tree builds, drive mapping."""
from __future__ import annotations

import hashlib
import os
import struct
import zlib

import make_fixtures as mf
from conftest import require_tools


def test_ico_bytes_layout_and_determinism():
    ico = mf.ico_bytes(0)
    assert ico == mf.ico_bytes(0)
    reserved, typ, count = struct.unpack_from("<HHH", ico)
    assert (reserved, typ, count) == (0, 1, 1)
    w, h, colours, _, planes, bpp, size, offset = struct.unpack_from("<BBBBHHII", ico, 6)
    assert (w, h, colours, planes, bpp, offset) == (32, 32, 16, 1, 4, 22)
    assert size == len(ico) - 22 == 40 + 64 + 32 * 16 + 32 * 4


def test_icons_differ_and_icon6_is_transparent():
    pixels = [mf.icon_pixels(n) for n in range(8)]
    assert len({str(p) for p in pixels}) == 8
    assert pixels[6][5][0] == -1 and pixels[0][5][0] == 0


def test_png_is_valid():
    png = mf.png_bytes([[(255, 0, 0), (0, 255, 0)]])
    assert png.startswith(b"\x89PNG\r\n\x1a\n")
    pos = 8
    while pos < len(png):
        n, = struct.unpack_from(">I", png, pos)
        tag, data = png[pos + 4:pos + 8], png[pos + 8:pos + 8 + n]
        crc, = struct.unpack_from(">I", png, pos + 8 + n)
        assert crc == zlib.crc32(tag + data)
        pos += 12 + n
    assert mf.contact_sheet([0, 1]) == mf.contact_sheet([0, 1])


def tree_state(root):
    return {str(p.relative_to(root)): (p.is_dir(), int(p.stat().st_mtime),
                                       hashlib.sha256(p.read_bytes()).hexdigest() if p.is_file() else None)
            for p in sorted(root.rglob("*"))}


def test_build_tree_is_deterministic(tmp_path):
    require_tools("i686-w64-mingw32-gcc", "i686-w64-mingw32-windres")
    a = mf.build_tree(tmp_path / "a", "basic")
    b = mf.build_tree(tmp_path / "b", "basic")
    assert tree_state(a) == tree_state(b)
    man_a = (tmp_path / "a" / "fixtures" / "basic.manifest.txt").read_text()
    assert man_a == (tmp_path / "b" / "fixtures" / "basic.manifest.txt").read_text()
    assert "GAMES\\ALPHA.EXE" in man_a and "EMPTY\\ dir" in man_a
    assert all(mtime == mf.FIXED_TIME for _, mtime, _ in tree_state(a).values())
    assert (a / "TOOLS" / "ECHO.EXE").read_bytes()[:2] == b"MZ"
    # A rebuild into the same work dir replaces the tree with identical content.
    assert tree_state(mf.build_tree(tmp_path / "a", "basic")) == tree_state(b)


def test_map_drive_replaces_links(tmp_path):
    prefix = tmp_path / "prefix"
    dd = prefix / "dosdevices"
    dd.mkdir(parents=True)
    target = tmp_path / "tree"
    target.mkdir()
    os.symlink("/nonexistent", dd / "d:")
    os.symlink("/dev/null", dd / "d::")
    mf.map_drive(prefix, "d", target)
    assert os.readlink(dd / "d:") == str(target.resolve())
    assert not (dd / "d::").is_symlink()
