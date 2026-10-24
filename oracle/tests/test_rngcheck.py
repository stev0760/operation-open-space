"""rngcheck.py on synthetic generator runs, and on the probe dumps when present."""
from __future__ import annotations

import struct

import pytest

import rngcheck as rc
from conftest import REPO_ROOT

DUMPS = REPO_ROOT / "work" / "probe" / "dumps"


def run(seed: int, n: int) -> int:
    x = seed
    for _ in range(n):
        x = rc.step(x)
    return x


def test_unstep_inverts_step():
    for x in (0, 1, 0x3375, 0xFFFFFFFF, 0x25DA5EF6):
        assert rc.unstep(rc.step(x)) == x


def test_first_outputs_match_msvc_rand():
    # srand(1): the Microsoft C runtime's rand() gives 41, 18467, 6334
    x, out = 1, []
    for _ in range(3):
        x = rc.step(x)
        out.append((x >> 16) & 0x7FFF)
    assert out == [41, 18467, 6334]


def test_seed_of_recovers_seed_and_count():
    assert rc.seed_of(run(0x3375, 891)) == (0x3375, 891)
    assert rc.seed_of(0x1234) == (0x1234, 0)


def test_steps_between():
    a = run(0xB411, 2512)
    assert rc.steps_between(a, run(0xB411, 2522)) == 10
    assert rc.steps_between(a, a) == 0


def test_seed_tick():
    assert rc.seed_tick(0x4799D4D2, 0xB411) == 0x4799B411
    assert rc.seed_tick(0x4799D4D2, 0xE000) == 0x4798E000


def test_main_on_synthetic_dumps(tmp_path, capsys):
    for name, seed, n, tick in (("wave", 0xB411, 2512, 0x4799D4D2), ("move0", 0xB411, 2522, 0x4799D552)):
        b = bytearray(0x10000)
        struct.pack_into("<I", b, rc.STATE, run(seed, n))
        struct.pack_into("<I", b, rc.TICK, tick)
        (tmp_path / f"{name}-inspace.bin").write_bytes(bytes(b))
    assert rc.main([str(tmp_path)]) == 0
    out = capsys.readouterr().out
    assert "| wave | " in out and "| 0xb411 | 2512 | 0x4799d4d2 | 0x4799b411 | 8385 |" in out
    assert "wave -> move0: 10 draws" in out


@pytest.mark.skipif(not (DUMPS / "wave-inspace.bin").exists(), reason="no probe dumps in work/probe/dumps")
def test_probe_0001_dumps(capsys):
    assert rc.main([str(DUMPS)]) == 0
    out = capsys.readouterr().out
    assert "| title | 0x25da5ef6 | 0x3375 | 891 |" in out
    assert "| wave | 0xe43e04a1 | 0xb411 | 2512 | 0x4799d4d2 | 0x4799b411 | 8385 | 298 |" in out
    assert "move0 -> move1: 105 draws" in out and "move1 -> move2: 172 draws" in out
