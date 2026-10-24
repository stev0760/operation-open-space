"""Shared fixtures for the oracle tooling tests.

Run from the repo root: ``re/.venv/bin/python -m pytest oracle/tests``.
Tests that need MinGW, Wine or a Wine source tree skip when those are absent."""
from __future__ import annotations

import shutil
import sys
from pathlib import Path

import pytest

ORACLE = Path(__file__).resolve().parents[1]
REPO_ROOT = ORACLE.parent
sys.path.insert(0, str(ORACLE))
sys.path.insert(0, str(ORACLE / "fixtures"))


def require_tools(*names: str) -> None:
    missing = [n for n in names if shutil.which(n) is None]
    if missing:
        pytest.skip(f"not on PATH: {', '.join(missing)}")


@pytest.fixture
def relay_log():
    """Build trace lines in the relay format: relay_log(tid, body, ...) with a
    timestamp that advances 10 ms per line; a body starting with '!' is written
    without the timestamp and thread prefix (wine's own messages, winedbg output)."""
    def make(*items: str, tid: str = "00ec", t0: float = 1000.0) -> list[str]:
        out = []
        for i, body in enumerate(items):
            if body.startswith("!"):
                out.append(body[1:] + "\n")
            else:
                out.append(f"{t0 + i * 0.01:.3f}:{tid}:{body}\n")
        return out
    return make
