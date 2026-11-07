"""tools/gen_known_files.py: the committed table is the generator's output, and the
generator's rules hold on synthetic manifests."""
from __future__ import annotations

import hashlib
import importlib.util
from pathlib import Path

import pytest

from conftest import REPO_ROOT, require

GENERATOR = REPO_ROOT / "tools" / "gen_known_files.py"
TABLE = REPO_ROOT / "src" / "assets" / "known_files_table.cpp"


def load_generator():
    spec = importlib.util.spec_from_file_location("gen_known_files", GENERATOR)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_committed_table_is_current(tmp_path: Path):
    manifest = require(REPO_ROOT / "original.sha256")
    require(REPO_ROOT / "original" / "game")
    require(REPO_ROOT / "original" / "theworks")
    gen = load_generator()
    out = tmp_path / "known_files_table.cpp"
    assert gen.main([str(manifest), str(out)]) == 0
    assert out.read_bytes() == TABLE.read_bytes(), "re-run tools/gen_known_files.py"


def write_manifest(root: Path, files: dict[str, bytes]) -> Path:
    lines = []
    for rel, data in files.items():
        path = root / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        lines.append(f"{hashlib.sha256(data).hexdigest()}  {rel}")
    manifest = root / "original.sha256"
    manifest.write_text("\n".join(lines) + "\n", encoding="ascii")
    return manifest


def test_rows_sorted_archives_skipped_and_deterministic(tmp_path: Path):
    gen = load_generator()
    manifest = write_manifest(tmp_path, {
        "original/theworks/B.DIB": b"b",
        "original/game/README.TXT": b"readme",
        "original/game/a.dll": b"a",
        "original/archives/X.ZIP": b"zip",
    })
    rows = gen.build_rows(manifest)
    assert [(r["package"], r["name"]) for r in rows] == [
        ("original/game", "a.dll"),
        ("original/game", "README.TXT"),
        ("original/theworks", "B.DIB"),
    ]
    first = gen.render(rows, manifest.name)
    assert first == gen.render(gen.build_rows(manifest), manifest.name)
    assert str(tmp_path) not in first


def test_unknown_directory_and_hash_mismatch_are_errors(tmp_path: Path):
    gen = load_generator()
    manifest = write_manifest(tmp_path, {"original/other/A.TXT": b"a"})
    with pytest.raises(gen.GenError, match="no package"):
        gen.build_rows(manifest)
    manifest = write_manifest(tmp_path, {"original/game/A.TXT": b"a"})
    (tmp_path / "original" / "game" / "A.TXT").write_bytes(b"changed")
    with pytest.raises(gen.GenError, match="does not match"):
        gen.build_rows(manifest)


def test_names_restoring_to_one_installed_name_are_an_error(tmp_path: Path):
    gen = load_generator()
    manifest = write_manifest(tmp_path, {
        "original/game/A.BOD": b"plain",
        "original/game/A.BO1": b"renamed",
    })
    with pytest.raises(gen.GenError, match="both restore to"):
        gen.build_rows(manifest)
