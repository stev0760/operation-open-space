"""Small file helpers shared by the isre tools."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def write_if_changed(path: Path, data: bytes) -> bool:
    """Write ``data`` to ``path`` unless the file already holds exactly it.

    Returns True when the file was (re)written. Keeping unchanged files
    untouched makes repeated pipeline runs idempotent, mtimes included.
    """
    path = Path(path)
    try:
        if path.is_file() and path.stat().st_size == len(data) and path.read_bytes() == data:
            return False
    except OSError:
        pass
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return True


def json_bytes(obj: Any, indent: int = 1) -> bytes:
    """Serialise ``obj`` the way the legacy scripts did (json.dump, indent=1)."""
    return json.dumps(obj, indent=indent).encode("utf-8")


def find_file(directory: Path, name: str) -> Path | None:
    """Find ``name`` in ``directory`` case-insensitively (FAT names vary in case)."""
    directory = Path(directory)
    exact = directory / name
    if exact.is_file():
        return exact
    low = name.lower()
    for p in sorted(directory.iterdir()):
        if p.is_file() and p.name.lower() == low:
            return p
    return None
