#!/usr/bin/env python3
"""Recover the game's rand() seed and draw count from probe dumps.

Usage: rngcheck.py DUMPDIR [PHASE ...]

DUMPDIR holds <phase>-inspace.bin files written by oracle/probe (the full INSPACE.EXE
DGROUP). For each phase the tool reads the rand() state at DGROUP:0x144c, steps the
generator backwards until the state fits in 16 bits (a value srand() can store), and
prints that seed, the number of draws since it, the GetCurrentTime sample at
DGROUP:0x1e50, the last tick at or before it whose low word equals the seed, and the
wave frame counter DGROUP:0x1e68. It then counts the forward steps between the states
of consecutive phases. The generator is docs/re/rng.md's: state = state * 214013 +
2531011 mod 2**32, srand(s) stores s with a zero high word.
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

MUL, ADD, MOD = 214013, 2531011, 1 << 32
INV = pow(MUL, -1, MOD)
STATE, TICK, FRAMES = 0x144C, 0x1E50, 0x1E68
PHASES = ("title", "navigation", "wave", "move0", "move1", "move2")


def step(x: int) -> int:
    return (x * MUL + ADD) % MOD


def unstep(x: int) -> int:
    return ((x - ADD) * INV) % MOD


def seed_of(state: int, limit: int = 10**7) -> tuple[int, int] | None:
    """(seed, draws) for the nearest earlier state below 0x10000, or None."""
    x = state
    for n in range(limit + 1):
        if x < 0x10000:
            return x, n
        x = unstep(x)
    return None


def steps_between(a: int, b: int, limit: int = 10**6) -> int | None:
    x = a
    for n in range(limit + 1):
        if x == b:
            return n
        x = step(x)
    return None


def seed_tick(tick: int, seed: int) -> int:
    """The last tick at or before `tick` whose low word is `seed`."""
    t = (tick & ~0xFFFF) | seed
    return t - 0x10000 if t > tick else t


def main(argv: list[str]) -> int:
    if not argv:
        print(__doc__.strip().splitlines()[2], file=sys.stderr)
        return 2
    d = Path(argv[0])
    phases = argv[1:] or [p for p in PHASES if (d / f"{p}-inspace.bin").exists()]
    states = []
    print("| Dump | DGROUP:0x144c | Seed | Draws | DGROUP:0x1e50 | Seed tick | ms since | DGROUP:0x1e68 |")
    print("|---|---|---|---|---|---|---|---|")
    for p in phases:
        b = (d / f"{p}-inspace.bin").read_bytes()
        s = struct.unpack_from("<I", b, STATE)[0]
        t = struct.unpack_from("<I", b, TICK)[0]
        f = struct.unpack_from("<H", b, FRAMES)[0]
        r = seed_of(s)
        if r is None:
            print(f"| {p} | 0x{s:08x} | none | | 0x{t:08x} | | | {f} |")
        else:
            st = seed_tick(t, r[0])
            print(f"| {p} | 0x{s:08x} | 0x{r[0]:04x} | {r[1]} | 0x{t:08x} | 0x{st:08x} | {t - st} | {f} |")
        states.append((p, s))
    for (pa, a), (pb, b) in zip(states, states[1:]):
        n = steps_between(a, b)
        print(f"{pa} -> {pb}: {'not reached' if n is None else f'{n} draws'}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
