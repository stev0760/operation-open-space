#!/usr/bin/env python3
"""Lay out the original Shipdisk files in a Wine prefix without INSTALL.EXE.

Usage: install_layout.py <shipdisk-dir> <wineprefix> [--game-dir INSPACE]

Directory codes follow the [0]..[7] sections of INSTALL.INI. Their meaning is
inferred [OPEN]: 0 = game dir, 1 = SHIPS, 2 = SOUNDS, 8 = Windows system dir,
9 = After Dark modules (skipped). INSPACE.INI goes to both the game dir and
C:\\WINDOWS, because the manual places it in the Windows directory.
"""
from __future__ import annotations

import argparse
import configparser
import pathlib
import shutil
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "re" / "legacy"))
from szdd import expand_szdd  # noqa: E402

# Last character of a SZDD-compressed name -> expanded extension.
EXPANDED = {".MI1": ".MID", ".DL1": ".DLL", ".HL1": ".HLP", ".BO1": ".BOD",
            ".SH1": ".SHP", ".IN1": ".INI", ".A1": ".AD", ".SC1": ".SCR"}
PLAIN_RENAMES = {".EX_": ".EXE"}


def find_source(src: pathlib.Path, name: str) -> pathlib.Path | None:
    stem, ext = name.rsplit(".", 1)
    for f in src.iterdir():
        fs, _, fe = f.name.rpartition(".")
        if fs.upper() != stem.upper():
            continue
        fe = "." + fe.upper()
        if fe == "." + ext.upper() or EXPANDED.get(fe) == "." + ext.upper() \
                or PLAIN_RENAMES.get(fe) == "." + ext.upper():
            return f
    return None


def read_file(f: pathlib.Path) -> bytes:
    data = f.read_bytes()
    return expand_szdd(data) if data[:4] == b"SZDD" else data


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("shipdisk", type=pathlib.Path)
    ap.add_argument("prefix", type=pathlib.Path)
    ap.add_argument("--game-dir", default="INSPACE")
    a = ap.parse_args()

    c = a.prefix / "drive_c"
    game = c / a.game_dir
    dest = {"0": game, "1": game / "SHIPS", "2": game / "SOUNDS",
            "8": c / "windows" / "system", "9": None}
    ini = configparser.ConfigParser(strict=False)
    ini.optionxform = str
    ini.read(a.shipdisk / "INSTALL.INI")

    for sec in [s for s in ini.sections() if s.isdigit()]:
        for name, code in ini[sec].items():
            target = dest.get(code)
            if target is None:
                print(f"skip  {name} (code {code})")
                continue
            src = find_source(a.shipdisk, name)
            if src is None:
                print(f"miss  {name} (code {code})")
                continue
            target.mkdir(parents=True, exist_ok=True)
            (target / name).write_bytes(read_file(src))
            print(f"copy  {src.name} -> {target.relative_to(c)}/{name}")
    (game / "GAMES").mkdir(exist_ok=True)
    shutil.copy(game / "INSPACE.INI", c / "windows" / "INSPACE.INI")
    print("copy  INSPACE.INI -> windows/INSPACE.INI")
    return 0


if __name__ == "__main__":
    sys.exit(main())
