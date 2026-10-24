#!/usr/bin/env python3
"""Build deterministic fixture drives for oracle filesystem-walk runs.

Usage:
  make_fixtures.py <work-dir> [--tree basic] [--map-prefix PREFIX] [--drive d]

Builds <work-dir>/fixtures/<tree>/: an 8.3-ASCII directory tree of small PE
executables, each carrying one or more distinct 16-colour 32x32 icons. The
icons and resource scripts are generated here; stub.c is compiled with
i686-w64-mingw32-gcc and the icons linked in with i686-w64-mingw32-windres.
Build intermediates go to <work-dir>/fixtures/_build/<tree>/, a manifest to
<work-dir>/fixtures/<tree>.manifest.txt and an icon contact sheet to
<work-dir>/fixtures/<tree>.icons.png (both outside the tree, so the game never
walks them). File and directory times are fixed, so two builds are identical.

--map-prefix links PREFIX/dosdevices/<drive>: to the tree and removes the
<drive>:: device link. Wine may still report the drive as a CD-ROM through
HKLM\\Software\\Wine\\Drives; oracle/scenarios/newgame.sh sets it to "hd".
"""
from __future__ import annotations

import argparse
import hashlib
import os
import pathlib
import shutil
import struct
import subprocess
import sys
import zlib

HERE = pathlib.Path(__file__).resolve().parent
FIXED_TIME = 965822400  # 2000-08-09 12:00:00 UTC, the v1.29.08 build date

# Windows default 16-colour palette, in DIB index order (R, G, B).
VGA16 = [(0, 0, 0), (128, 0, 0), (0, 128, 0), (128, 128, 0),
         (0, 0, 128), (128, 0, 128), (0, 128, 128), (192, 192, 192),
         (128, 128, 128), (255, 0, 0), (0, 255, 0), (255, 255, 0),
         (0, 0, 255), (255, 0, 255), (0, 255, 255), (255, 255, 255)]

# Icon n: background colour index, shape colour index, shape name.
ICON_STYLES = [(9, 15, "square"), (10, 0, "diamond"), (12, 15, "cross"),
               (11, 0, "circle"), (13, 15, "triangle"), (14, 0, "xmark"),
               (3, 15, "ring"), (5, 15, "bars"), (2, 15, "square"),
               (4, 15, "diamond")]
TRANSPARENT_BG = {6}  # icon 6 has a transparent background (AND mask set)

# Tree layout: relative path -> list of icon numbers; None marks an empty dir.
TREES: dict[str, dict[str, list[int] | None]] = {
    "basic": {
        "GAMES/ALPHA.EXE": [0],
        "GAMES/BRAVO.EXE": [1],
        "GAMES/CHARLIE.EXE": [2],
        "TOOLS/DELTA.EXE": [3],
        "TOOLS/ECHO.EXE": [4, 5, 6],
        "EMPTY": None,
        "DEEP/A/B/FOXTROT.EXE": [7],
    },
}


def icon_pixels(n: int) -> list[list[int]]:
    """32x32 palette indices, top row first. Value -1 means transparent."""
    bg, fg, shape = ICON_STYLES[n % len(ICON_STYLES)]
    back = -1 if n in TRANSPARENT_BG else bg
    px = [[back] * 32 for _ in range(32)]
    for y in range(32):
        for x in range(32):
            dx, dy = x - 15.5, y - 13.5
            on = {
                "square": abs(dx) <= 8 and abs(dy) <= 8,
                "diamond": abs(dx) + abs(dy) <= 11,
                "cross": abs(dx) <= 3 or abs(dy) <= 3,
                "circle": dx * dx + dy * dy <= 100,
                "triangle": 4 <= y <= 24 and abs(dx) <= (y - 4) * 0.6,
                "xmark": abs(abs(dx) - abs(dy)) <= 2.5 and abs(dx) <= 11,
                "ring": 49 <= dx * dx + dy * dy <= 121,
                "bars": x % 8 < 4 and 3 <= y <= 24,
            }[shape]
            if on and y <= 26:
                px[y][x] = fg
    # n+1 white-on-black dots along the bottom so the number can be read off a screenshot.
    for y in range(28, 32):
        for x in range(32):
            px[y][x] = 0
    for k in range(n + 1):
        x0 = 1 + 3 * k
        for y in (29, 30):
            for x in (x0, x0 + 1):
                if x < 32:
                    px[y][x] = 15
    if back != -1:  # 1-pixel black frame on opaque icons
        for i in range(32):
            px[0][i] = px[31][i] = px[i][0] = px[i][31] = 0
    return px


def ico_bytes(n: int) -> bytes:
    """A single-image .ico: 32x32, 4 bpp, standard palette, AND mask."""
    px = icon_pixels(n)
    xor = bytearray()
    andm = bytearray()
    for row in reversed(px):  # DIBs are bottom-up
        for x in range(0, 32, 2):
            a, b = (max(v, 0) for v in row[x:x + 2])
            xor.append((a << 4) | b)
        bits = 0
        for x in range(32):
            bits = (bits << 1) | (1 if row[x] == -1 else 0)
        andm += bits.to_bytes(4, "big")
    palette = b"".join(bytes((b, g, r, 0)) for r, g, b in VGA16)
    bih = struct.pack("<IiiHHIIiiII", 40, 32, 64, 1, 4, 0, len(xor) + len(andm), 0, 0, 16, 0)
    image = bih + palette + bytes(xor) + bytes(andm)
    header = struct.pack("<HHH", 0, 1, 1)
    entry = struct.pack("<BBBBHHII", 32, 32, 16, 0, 1, 4, len(image), 6 + 16)
    return header + entry + image


def png_bytes(rows: list[list[tuple[int, int, int]]]) -> bytes:
    """Minimal RGB PNG writer (no PIL in the venv)."""
    h, w = len(rows), len(rows[0])
    raw = b"".join(b"\x00" + bytes(c for px in row for c in px) for row in rows)

    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def contact_sheet(numbers: list[int], scale: int = 3) -> bytes:
    """Icons side by side, magenta-grey checker where transparent."""
    cell = 32 * scale + 8
    rows = [[(64, 64, 64)] * (cell * len(numbers)) for _ in range(cell)]
    for i, n in enumerate(numbers):
        px = icon_pixels(n)
        for y in range(32 * scale):
            for x in range(32 * scale):
                v = px[y // scale][x // scale]
                c = VGA16[v] if v >= 0 else ((160, 96, 160) if (x // 6 + y // 6) % 2 else (96, 96, 96))
                rows[4 + y][i * cell + 4 + x] = c
    return png_bytes(rows)


def run(cmd: list[str], cwd: pathlib.Path) -> None:
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if r.returncode:
        sys.exit(f"failed: {' '.join(cmd)}\n{r.stdout}{r.stderr}")


def build_exe(name: str, icons: list[int], build: pathlib.Path) -> bytes:
    stem = name.rsplit(".", 1)[0].lower()
    rc_lines = []
    for k, n in enumerate(icons, 1):
        ico = f"icon{n}.ico"
        (build / ico).write_bytes(ico_bytes(n))
        rc_lines.append(f'{k} ICON "{ico}"')
    (build / f"{stem}.rc").write_text("\n".join(rc_lines) + "\n")
    shutil.copy(HERE / "stub.c", build / "stub.c")
    run(["i686-w64-mingw32-windres", "-O", "coff", "-i", f"{stem}.rc", "-o", f"{stem}.res.o"], build)
    run(["i686-w64-mingw32-gcc", "-Os", "-s", "-mwindows", "-Wl,--no-insert-timestamp",
         "stub.c", f"{stem}.res.o", "-o", f"{stem}.exe"], build)
    return (build / f"{stem}.exe").read_bytes()


def build_tree(work: pathlib.Path, tree: str) -> pathlib.Path:
    layout = TREES[tree]
    root = work / "fixtures" / tree
    build = work / "fixtures" / "_build" / tree
    for d in (root, build):
        if d.exists():
            shutil.rmtree(d)
        d.mkdir(parents=True)
    manifest = [f"# fixture tree {tree}: path size sha256 icons"]
    for rel, icons in layout.items():
        dest = root / rel
        if icons is None:
            dest.mkdir(parents=True, exist_ok=True)
            manifest.append(f"{rel}\\ dir")
            continue
        dest.parent.mkdir(parents=True, exist_ok=True)
        data = build_exe(dest.name, icons, build)
        dest.write_bytes(data)
        desc = ", ".join(f"#{n} {ICON_STYLES[n][2]} bg={ICON_STYLES[n][0]}"
                         f"{' transparent' if n in TRANSPARENT_BG else ''}" for n in icons)
        manifest.append(f"{rel.replace('/', chr(92))} {len(data)} "
                        f"{hashlib.sha256(data).hexdigest()} [{desc}]")
    # Fixed times, deepest paths first so directory times stick.
    for p in sorted(root.rglob("*"), key=lambda q: -len(q.parts)):
        os.utime(p, (FIXED_TIME, FIXED_TIME))
    os.utime(root, (FIXED_TIME, FIXED_TIME))
    numbers = sorted({n for v in layout.values() if v for n in v})
    (work / "fixtures" / f"{tree}.manifest.txt").write_text("\n".join(manifest) + "\n")
    (work / "fixtures" / f"{tree}.icons.png").write_bytes(contact_sheet(numbers))
    return root


def map_drive(prefix: pathlib.Path, drive: str, target: pathlib.Path) -> None:
    dd = prefix / "dosdevices"
    link, dev = dd / f"{drive}:", dd / f"{drive}::"
    if link.is_symlink() or link.exists():
        link.unlink()
    if dev.is_symlink():
        dev.unlink()
    link.symlink_to(target.resolve())
    print(f"map   {drive.upper()}: -> {target.resolve()}")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("work", type=pathlib.Path, help="the repo's work/ directory")
    ap.add_argument("--tree", default="basic", choices=sorted(TREES))
    ap.add_argument("--map-prefix", type=pathlib.Path)
    ap.add_argument("--drive", default="d")
    a = ap.parse_args()
    root = build_tree(a.work, a.tree)
    print((a.work / "fixtures" / f"{a.tree}.manifest.txt").read_text(), end="")
    if a.map_prefix:
        map_drive(a.map_prefix, a.drive.lower(), root)
    return 0


if __name__ == "__main__":
    sys.exit(main())
