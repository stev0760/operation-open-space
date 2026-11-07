from __future__ import annotations

import importlib.util
import json
import os
import subprocess
import sys
import types
from pathlib import Path

import pytest
from conftest import GOLDEN, REPO_ROOT, read_sha256_list

from isre.cli import PLUGINS, build_parser, main
from isre.util import sha256


def _subcommands():
    parser = build_parser()
    action = next(a for a in parser._actions if a.dest == "cmd")
    return set(action.choices)


def test_subcommands_registered():
    cmds = _subcommands()
    assert {"szdd", "extract", "ne", "unpack", "relocs", "addr", "all"} <= cmds
    for name in PLUGINS:
        spec = importlib.util.find_spec(f"isre.{name}")
        has = spec is not None and "\ndef main(" in "\n" + Path(spec.origin).read_text()
        assert (name in cmds) == has, name


def test_plugin_dispatch_is_lazy(monkeypatch):
    calls = []
    fake = types.ModuleType("isre.hlp")
    fake.main = lambda argv: calls.append(argv) or 0
    monkeypatch.setitem(sys.modules, "isre.hlp", fake)
    assert main(["hlp", "dump", "--x"]) == 0
    assert calls == [["dump", "--x"]]
    monkeypatch.setitem(sys.modules, "isre.hlp", types.ModuleType("isre.hlp"))
    assert main(["hlp"]) == 2  # no main()


def test_help_exits_cleanly():
    with pytest.raises(SystemExit) as e:
        main(["--help"])
    assert e.value.code == 0


def test_module_entry_point():
    env = dict(os.environ, PYTHONPATH=str(REPO_ROOT / "re"))
    r = subprocess.run([sys.executable, "-m", "isre", "addr", "seg4:0x2ea2"],
                       capture_output=True, text=True, check=True, env=env)
    assert "1018:2ea2" in r.stdout


def test_missing_input_is_an_error(tmp_path, capsys):
    assert main(["ne", str(tmp_path / "missing.exe")]) == 2
    assert "missing.exe" in capsys.readouterr().err


def test_szdd_command(game, tmp_path):
    out = tmp_path / "WAVEMIX.INI"
    assert main(["szdd", str(game / "WAVEMIX.IN1"), str(out)]) == 0
    assert out.read_bytes().lstrip()[:1] == b"["


def test_ne_json(game, capsys):
    assert main(["ne", str(game / "INSPACE.ex_"), "--json"]) == 0
    d = json.loads(capsys.readouterr().out)
    assert d["modules"] == ["KERNEL", "USER", "GDI", "MMSYSTEM", "SHELL"]
    assert d["segments"][18]["kind"] == "DATA" and d["segments"][0]["kind"] == "CODE"
    assert d["optloader"]["packed"]


def test_all_pipeline_is_idempotent(original_dir, tmp_path, capsys):
    work = tmp_path / "work"
    assert main(["all", "--original", str(original_dir), "--work", str(work)]) == 0
    gold = read_sha256_list(GOLDEN / "inspace.sha256")
    up = work / "unpacked" / "inspace"
    for n in range(1, 20):
        assert sha256((up / f"seg{n:02d}.bin").read_bytes()) == gold[f"unpacked/seg{n:02d}.bin"]
    assert sha256((up / "relocs.json").read_bytes()) == gold["relocs.json"]
    for name in ("inspace", "ispace", "factory-1998a", "factory-1998b"):
        assert json.loads((work / "unpacked" / name / "unpack.json").read_text())["invariants_ok"], name
        assert (work / "ne" / f"{name}.json").is_file()
    ship = work / "extracted" / "game"
    for f in ("INSPACE.EXE", "FACTORY.EXE", "Eng_help.HLP", "WAVEMIX.INI", "MASTER.BOD", "MASTER.SHP"):
        assert (ship / f).is_file(), f
    assert len(list((work / "extracted" / "theworks").iterdir())) == len(
        [p for p in (original_dir / "theworks").iterdir() if p.is_file()])
    mtimes = {p: p.stat().st_mtime_ns for p in work.rglob("*") if p.is_file()}
    capsys.readouterr()
    assert main(["all", "--original", str(original_dir), "--work", str(work)]) == 0
    assert "0 files written" in capsys.readouterr().out
    assert mtimes == {p: p.stat().st_mtime_ns for p in work.rglob("*") if p.is_file()}
