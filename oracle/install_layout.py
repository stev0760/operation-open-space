#!/usr/bin/env python3
"""Lay out the original Game files in a Wine prefix without INSTALL.EXE (the
unregistered game; oracle/scenarios/install.sh runs the installer for the registered one).

Usage: install_layout.py <game-dir> <wineprefix> [--game-dir INSPACE]
                         [--with-ini] [--calibrate N]

Directory codes follow the [0]..[7] sections of INSTALL.INI, as INSTALL.EXE
resolves them (docs/re/install.md): 0 = game dir, 1 = SHIPS, 2 = SOUNDS,
8 = the Windows directory (GetWindowsDirectory, INSTALL.EXE seg2:0x1950),
9 = After Dark modules (skipped). INSPACE.INI goes to both the game dir and
C:\\WINDOWS, because the manual places it in the Windows directory.

--with-ini pre-writes into C:\\WINDOWS\\INSPACE.INI the [Inner Space] keys the
game regenerates on first launch [ORACLE smoke-0001], so runs start without
the "Some game settings are missing" MessageBox. Without it the shipped INI is
copied unchanged. The game measures Calibrate again and rewrites it on every
launch [ORACLE newgame-0001]; the pre-written 713 only fills the key.
"""
from __future__ import annotations

import argparse
import configparser
import pathlib
import shutil
import sys

_RE = pathlib.Path(__file__).resolve().parents[1] / "re"
sys.path.insert(0, str(_RE))
from isre.szdd import expand as expand_szdd  # noqa: E402

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


def regenerated_keys(game_dir: str, calibrate: int) -> dict[str, str]:
    """[Inner Space] keys as the game writes them for a C:\\<game_dir> install."""
    root = f"C:\\{game_dir}\\"
    return {"Path": root, "LocalPath": root,
            "ShipPath": root + "SHIPS\\", "SndPath": root + "SOUNDS\\",
            "RacePath": root + "GAMES\\", "Dead": "RETIRED", "Games": "GAMES",
            "SoundSystem": "0", "Calibrate": str(calibrate)}


def set_ini_keys(text: str, section: str, keys: dict[str, str]) -> str:
    """Set keys in one INI section, keeping every other line and CRLF endings."""
    lines = text.splitlines()
    out: list[str] = []
    todo = dict(keys)
    in_sec = False
    found = False

    def flush() -> None:
        # Insert remaining keys before any trailing blank lines of the section.
        tail: list[str] = []
        while out and not out[-1].strip():
            tail.insert(0, out.pop())
        out.extend(f"{k}={v}" for k, v in todo.items())
        out.extend(tail)
        todo.clear()

    for line in lines:
        st = line.strip()
        if st.startswith("[") and st.endswith("]"):
            if in_sec:
                flush()
            in_sec = st[1:-1].strip().lower() == section.lower()
            found = found or in_sec
            out.append(line)
            continue
        if in_sec and "=" in line:
            k = line.split("=", 1)[0].strip()
            match = next((x for x in todo if x.lower() == k.lower()), None)
            if match is not None:
                out.append(f"{k}={todo.pop(match)}")
                continue
        out.append(line)
    if in_sec:
        flush()
    if not found:
        out.append(f"[{section}]")
        out.extend(f"{k}={v}" for k, v in todo.items())
    return "\r\n".join(out) + "\r\n"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("game", type=pathlib.Path)
    ap.add_argument("prefix", type=pathlib.Path)
    ap.add_argument("--game-dir", default="INSPACE")
    ap.add_argument("--with-ini", action="store_true",
                    help="pre-write the [Inner Space] keys the game regenerates")
    ap.add_argument("--calibrate", type=int, default=713,
                    help="Calibrate value written by --with-ini (default 713)")
    a = ap.parse_args()

    c = a.prefix / "drive_c"
    game = c / a.game_dir
    dest = {"0": game, "1": game / "SHIPS", "2": game / "SOUNDS",
            "8": c / "windows", "9": None}
    ini = configparser.ConfigParser(strict=False)
    ini.optionxform = str
    ini.read(a.game / "INSTALL.INI")

    for sec in [s for s in ini.sections() if s.isdigit()]:
        for name, code in ini[sec].items():
            target = dest.get(code)
            if target is None:
                print(f"skip  {name} (code {code})")
                continue
            src = find_source(a.game, name)
            if src is None:
                print(f"miss  {name} (code {code})")
                continue
            target.mkdir(parents=True, exist_ok=True)
            (target / name).write_bytes(read_file(src))
            print(f"copy  {src.name} -> {target.relative_to(c)}/{name}")
    (game / "GAMES").mkdir(exist_ok=True)
    shutil.copy(game / "INSPACE.INI", c / "windows" / "INSPACE.INI")
    print("copy  INSPACE.INI -> windows/INSPACE.INI")
    if a.with_ini:
        win_ini = c / "windows" / "INSPACE.INI"
        text = win_ini.read_bytes().decode("latin-1")
        keys = regenerated_keys(a.game_dir, a.calibrate)
        win_ini.write_bytes(set_ini_keys(text, "Inner Space", keys).encode("latin-1"))
        print(f"ini   windows/INSPACE.INI [Inner Space] += {', '.join(keys)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
