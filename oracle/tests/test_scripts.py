"""Oracle scripts: shell syntax, oracle/env.sh and oracle/trace.sh settings, the Wine patches
against a Wine checkout, and the screen analysis of oracle/scenarios/autopilot.py."""
from __future__ import annotations

import math
import subprocess
import sys

import pytest

from conftest import ORACLE, REPO_ROOT, require_tools

sys.path.insert(0, str(ORACLE / "scenarios"))
import autopilot  # noqa: E402

SCRIPTS = ["env.sh", "trace.sh", "scenarios/common.sh", "scenarios/newgame.sh", "scenarios/wave.sh",
           "scenarios/intro.sh", "scenarios/install.sh", "scenarios/start.sh",
           "wine/build_patched.sh", "wine/install_kron4ek.sh"]
PATCHES = {"krnl386-clear-saved-selectors.patch": "dlls/krnl386.exe16/selector.c",
           "shell16-extracticon-count.patch": "dlls/shell.dll16/shell.c"}


@pytest.mark.parametrize("script", SCRIPTS)
def test_shell_syntax(script):
    require_tools("bash")
    subprocess.run(["bash", "-n", str(ORACLE / script)], check=True)


def env_after_sourcing(home, **env) -> dict[str, str]:
    require_tools("bash")
    cmd = 'source "$1"; printf "%s\\n" "$ORACLE_WINE" "$WINEPREFIX" "$DISPLAY" "$WINEDLLOVERRIDES"'
    base = {"PATH": "/usr/bin:/bin", "HOME": str(home)}
    out = subprocess.run(["bash", "-c", cmd, "bash", str(ORACLE / "env.sh")], env={**base, **env},
                         capture_output=True, text=True, check=True).stdout.splitlines()
    return dict(zip(["ORACLE_WINE", "WINEPREFIX", "DISPLAY", "WINEDLLOVERRIDES"], out))


def test_env_defaults(tmp_path):
    e = env_after_sourcing(tmp_path)
    assert e["ORACLE_WINE"] == f"{tmp_path}/opt/wine-11.0-oldwow-patched"
    assert e["WINEPREFIX"] == f"{tmp_path}/.local/share/wineprefixes/innerspace"
    assert e["DISPLAY"] == ":110"
    assert e["WINEDLLOVERRIDES"].startswith("ctl3d=n")


def test_env_per_runner_and_runtime_override(tmp_path):
    e = env_after_sourcing(tmp_path, ORACLE_ID="3", ORACLE_WINE="/opt/wine-11.0")
    assert e["ORACLE_WINE"] == "/opt/wine-11.0"
    assert e["WINEPREFIX"].endswith("/innerspace3")
    assert e["DISPLAY"] == ":113"


def test_scenario_has_no_shim_option():
    text = (ORACLE / "scenarios" / "newgame.sh").read_text()
    assert "SHIM" not in text and not (ORACLE / "shim").exists()


def relay_exclude(**env) -> list[str]:
    require_tools("bash")
    cmd = 'source "$1"; printf "%s" "$ORACLE_RELAY_EXCLUDE"'
    out = subprocess.run(["bash", "-c", cmd, "bash", str(ORACLE / "trace.sh")],
                         env={"PATH": "/usr/bin:/bin", **env}, capture_output=True, text=True, check=True)
    return out.stdout.split(";")


def test_relay_exclude_profiles():
    frame_loop = {"USER.GETASYNCKEYSTATE", "GDI.SELECTCLIPRGN", "USER.PTINRECT", "USER.PEEKMESSAGE"}
    default, wave = relay_exclude(), relay_exclude(ORACLE_RELAY_PROFILE="wave")
    assert frame_loop <= set(default) and frame_loop <= set(wave)
    assert "GDI.SETPIXEL" not in default and {"GDI.SETPIXEL", "USER.GETWINDOWTASK"} <= set(wave)
    assert "GDI.TEXTOUT" not in wave and "USER.CREATEWINDOW" not in wave
    assert relay_exclude(ORACLE_RELAY_EXCLUDE="USER.PEEKMESSAGE", ORACLE_RELAY_PROFILE="wave") == ["USER.PEEKMESSAGE"]


def test_patches_change_one_module_each():
    for name, path in PATCHES.items():
        changed = {line[6:].strip() for line in (ORACLE / "wine" / name).read_text().splitlines()
                   if line.startswith("+++ b/")}
        assert changed == {path}, name


def wine_checkouts(path: str):
    for rel in ("work/wine/wine-src", "work/tools/wine-src"):
        src = REPO_ROOT / rel
        if (src / ".git").exists() and (src / path).exists():
            yield src


@pytest.mark.parametrize("name", sorted(PATCHES))
def test_patch_matches_a_wine_checkout(name):
    require_tools("git")
    patch = ORACLE / "wine" / name
    srcs = list(wine_checkouts(PATCHES[name]))
    if not srcs:
        pytest.skip("no wine-11.0 checkout under work/ (oracle/wine/build_patched.sh clones one)")
    for src in srcs:
        applied = subprocess.run(["git", "-C", str(src), "apply", "--check", "-R", str(patch)],
                                 capture_output=True).returncode == 0
        applies = subprocess.run(["git", "-C", str(src), "apply", "--check", str(patch)],
                                 capture_output=True).returncode == 0
        assert applied or applies, f"{name} neither applies to nor is applied in {src}"


# --- autopilot.py on synthetic screens ------------------------------------------------

def screen(paint) -> bytes:
    """A black 1024x768 RGB screen; paint(set_pixel) draws on it."""
    buf = bytearray(autopilot.W * autopilot.H * 3)

    def put(x, y, c):
        i = (y * autopilot.W + x) * 3
        buf[i:i + 3] = bytes(c)
    paint(put)
    return bytes(buf)


def rect(put, x0, y0, w, h, c):
    for y in range(y0, y0 + h):
        for x in range(x0, x0 + w):
            put(x, y, c)


def ship(put, nose_dx, nose_dy):
    """A grey 21x21 hull centred on the ship position with a green cockpit towards the nose."""
    sx, sy = autopilot.SHIP
    rect(put, sx - 10, sy - 10, 21, 21, (192, 192, 192))
    rect(put, sx - 2 + nose_dx, sy - 2 + nose_dy, 5, 5, (0, 160, 0))


@pytest.mark.parametrize("nose,expected", [((0, -6), 90), ((6, 0), 0), ((-6, 0), 180), ((0, 6), -90)])
def test_heading_from_cockpit(nose, expected):
    h = autopilot.heading(screen(lambda put: ship(put, *nose)))
    assert abs(autopilot.angle_diff(h, expected)) < 1


def test_heading_without_ship():
    assert autopilot.heading(screen(lambda put: None)) is None


def test_angle_diff_folds():
    assert autopilot.angle_diff(170, -170) == -20
    assert autopilot.angle_diff(-170, 170) == 20
    assert autopilot.angle_diff(90, 90) == 0


def test_blobs_group_neighbours():
    pts = [(0, 0), (1, 0), (1, 1), (10, 10), (11, 10)]
    got = sorted(autopilot.blobs(pts, 1), key=lambda b: b[2])
    assert [b[2] for b in got] == [2, 3]
    assert math.isclose(got[0][0], 10.5) and math.isclose(got[1][1], 1 / 3)


def test_radar_and_column_and_target():
    def paint(put):
        ship(put, 0, -6)
        # radar: viewport rectangle centred on 510,725 and two icon dots
        for x in range(494, 527):
            put(x, 717, autopilot.RADAR_VIEW)
            put(x, 733, autopilot.RADAR_VIEW)
        rect(put, 520, 700, 2, 2, autopilot.RADAR_ICON)
        rect(put, 480, 760, 2, 2, autopilot.RADAR_ICON)
        # icon column: two faces
        rect(put, 986, 40, 30, 30, (255, 0, 0))
        rect(put, 986, 85, 30, 30, (0, 0, 255))
    raw = screen(paint)
    centre, dots = autopilot.radar(raw)
    assert centre == (510.0, 725.0) and len(dots) == 2
    assert autopilot.column_count(raw) == 2
    dx, dy, src = autopilot.target(raw)
    assert src == "radar" and (dx, dy) == (10.5 * 32, -24.5 * 32)


def test_target_prefers_an_icon_on_screen():
    def paint(put):
        ship(put, 0, -6)
        rect(put, 200, 100, 30, 30, (0, 255, 0))
    dx, dy, src = autopilot.target(screen(paint))
    assert src == "screen" and abs(dx - (214.5 - autopilot.SHIP[0])) < 2 and abs(dy - (114.5 - autopilot.SHIP[1])) < 2


def test_score_digits_follow_the_score_panel():
    blank = screen(lambda put: None)
    lit = screen(lambda put: rect(put, 100, 740, 8, 12, (255, 128, 0)))
    elsewhere = screen(lambda put: rect(put, 300, 300, 8, 12, (255, 128, 0)))
    assert autopilot.score_digits(blank) != autopilot.score_digits(lit)
    assert autopilot.score_digits(blank) == autopilot.score_digits(elsewhere)
