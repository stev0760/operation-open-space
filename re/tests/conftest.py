"""Shared fixtures. Paths are found relative to this file; the original game
files (``original/``, gitignored) are optional and tests needing them skip."""
from __future__ import annotations

from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]
ORIGINAL = REPO_ROOT / "original"
ARTIFACTS = REPO_ROOT / "re" / "artifacts"
GOLDEN = ARTIFACTS / "golden"


def require(path: Path) -> Path:
    if not path.exists():
        pytest.skip(f"{path.relative_to(REPO_ROOT)} is absent (original game files are local only)")
    return path


def read_sha256_list(path: Path) -> dict[str, str]:
    """Parse a sha256sum file into {path: hexdigest}."""
    out = {}
    for line in path.read_text().splitlines():
        if line.strip():
            digest, name = line.split(None, 1)
            out[name.strip().lstrip("*")] = digest
    return out


@pytest.fixture(scope="session")
def repo_root() -> Path:
    return REPO_ROOT


@pytest.fixture(scope="session")
def original_dir() -> Path:
    require(ORIGINAL / "game")
    require(ORIGINAL / "theworks")
    return ORIGINAL


@pytest.fixture(scope="session")
def game(original_dir) -> Path:
    return original_dir / "game"


@pytest.fixture(scope="session")
def theworks(original_dir) -> Path:
    return original_dir / "theworks"


@pytest.fixture(scope="session")
def inspace(game):
    from isre.ne import NEModule

    return NEModule.load(require(game / "INSPACE.ex_"))


@pytest.fixture(scope="session")
def inspace_unpacked(inspace):
    from isre import optloader

    return optloader.unpack(inspace)
