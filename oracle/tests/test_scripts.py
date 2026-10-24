"""Shell scripts: syntax, oracle/env.sh defaults, and the krnl386 patch against a Wine checkout."""
from __future__ import annotations

import subprocess

import pytest

from conftest import ORACLE, REPO_ROOT, require_tools

SCRIPTS = ["env.sh", "trace.sh", "scenarios/newgame.sh", "wine/build_patched.sh", "wine/install_kron4ek.sh"]
PATCH = ORACLE / "wine" / "krnl386-clear-saved-selectors.patch"


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
    assert e["ORACLE_WINE"] == "/opt/wine-11.0-oldwow"
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


def wine_checkouts():
    for rel in ("work/wine/wine-src", "work/tools/wine-src"):
        src = REPO_ROOT / rel
        if (src / ".git").exists() and (src / "dlls" / "krnl386.exe16" / "selector.c").exists():
            yield src


def test_krnl386_patch_matches_a_wine_checkout():
    require_tools("git")
    srcs = list(wine_checkouts())
    if not srcs:
        pytest.skip("no wine-11.0 checkout under work/ (oracle/wine/build_patched.sh clones one)")
    for src in srcs:
        applied = subprocess.run(["git", "-C", str(src), "apply", "--check", "-R", str(PATCH)],
                                 capture_output=True).returncode == 0
        applies = subprocess.run(["git", "-C", str(src), "apply", "--check", str(PATCH)],
                                 capture_output=True).returncode == 0
        assert applied or applies, f"{PATCH.name} neither applies to nor is applied in {src}"
