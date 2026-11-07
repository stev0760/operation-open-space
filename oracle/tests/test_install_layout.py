"""install_layout.py: INI key editing and the game layout, on synthetic input."""
from __future__ import annotations

import install_layout as il

KEYS = {"Path": "C:\\INSPACE\\", "Calibrate": "713"}


def lines(text: str) -> list[str]:
    assert text.endswith("\r\n")
    assert "\n" not in text.replace("\r\n", "")
    return text[:-2].split("\r\n")


def test_section_present_replaces_and_appends():
    text = "[Inner Space]\r\nPath=D:\\OLD\\\r\nDetail=3\r\n\r\n[Other]\r\nPath=keep\r\n"
    out = lines(il.set_ini_keys(text, "Inner Space", KEYS))
    assert out == ["[Inner Space]", "Path=C:\\INSPACE\\", "Detail=3", "Calibrate=713", "",
                   "[Other]", "Path=keep"]


def test_section_missing_is_appended():
    out = lines(il.set_ini_keys("[Other]\nA=1\n", "Inner Space", KEYS))
    assert out == ["[Other]", "A=1", "[Inner Space]", "Path=C:\\INSPACE\\", "Calibrate=713"]


def test_empty_text():
    assert il.set_ini_keys("", "Inner Space", KEYS) == "[Inner Space]\r\nPath=C:\\INSPACE\\\r\nCalibrate=713\r\n"


def test_empty_section_at_end():
    out = lines(il.set_ini_keys("[Other]\r\nA=1\r\n[Inner Space]\r\n", "Inner Space", KEYS))
    assert out == ["[Other]", "A=1", "[Inner Space]", "Path=C:\\INSPACE\\", "Calibrate=713"]


def test_section_and_keys_match_case_insensitively():
    text = "[INNER space]\r\npath=X\r\nCALIBRATE = 9\r\n"
    out = lines(il.set_ini_keys(text, "Inner Space", KEYS))
    # The file's own spelling of section and key names stays; values change.
    assert out == ["[INNER space]", "path=C:\\INSPACE\\", "CALIBRATE=713"]


def test_value_with_equals_sign_and_unrelated_lines_survive():
    text = "; comment\r\n[Inner Space]\r\nCmd=a=b\r\n"
    out = lines(il.set_ini_keys(text, "Inner Space", {"Cmd": "x=y"}))
    assert out == ["; comment", "[Inner Space]", "Cmd=x=y"]


def test_regenerated_keys():
    keys = il.regenerated_keys("INSPACE", 713)
    assert keys["Path"] == keys["LocalPath"] == "C:\\INSPACE\\"
    assert keys["ShipPath"] == "C:\\INSPACE\\SHIPS\\" and keys["RacePath"] == "C:\\INSPACE\\GAMES\\"
    assert keys["Calibrate"] == "713"


def test_layout_from_synthetic_game(tmp_path, monkeypatch):
    ship = tmp_path / "game"
    ship.mkdir()
    (ship / "INSTALL.INI").write_text("[0]\nINSPACE.EXE=0\nINSPACE.INI=0\n[1]\nMASTER.SHP=1\n"
                                      "[2]\nBOOM.WAV=2\n[3]\nCTL3D.DLL=8\nSAVER.AD=9\n")
    (ship / "INSPACE.EX_").write_bytes(b"MZ exe")
    (ship / "INSPACE.INI").write_bytes(b"[Inner Space]\r\nDetail=3\r\n")
    (ship / "MASTER.SHP").write_bytes(b"ship")
    (ship / "boom.wav").write_bytes(b"RIFF")
    (ship / "CTL3D.DLL").write_bytes(b"MZ dll")
    prefix = tmp_path / "prefix"
    (prefix / "drive_c" / "windows").mkdir(parents=True)
    monkeypatch.setattr("sys.argv", ["install_layout.py", str(ship), str(prefix), "--with-ini"])
    assert il.main() == 0
    c = prefix / "drive_c"
    assert (c / "INSPACE" / "INSPACE.EXE").read_bytes() == b"MZ exe"
    assert (c / "INSPACE" / "SHIPS" / "MASTER.SHP").read_bytes() == b"ship"
    assert (c / "INSPACE" / "SOUNDS" / "BOOM.WAV").read_bytes() == b"RIFF"
    assert (c / "windows" / "CTL3D.DLL").read_bytes() == b"MZ dll"
    assert (c / "INSPACE" / "GAMES").is_dir()
    assert not list(c.rglob("SAVER.AD"))
    win_ini = (c / "windows" / "INSPACE.INI").read_bytes().decode("latin-1")
    assert win_ini.startswith("[Inner Space]\r\nDetail=3\r\nPath=C:\\INSPACE\\\r\n")
    assert "Calibrate=713\r\n" in win_ini
    # The game-directory copy stays as shipped.
    assert (c / "INSPACE" / "INSPACE.INI").read_bytes() == b"[Inner Space]\r\nDetail=3\r\n"
