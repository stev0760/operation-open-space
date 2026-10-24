#!/usr/bin/env python3
"""Fly the player's ship into an icon, steering from screenshots (oracle scenario helper).

Usage: autopilot.py [--seconds N] [--log FILE]
  Runs against $DISPLAY (a 1024x768 oracle display with the game in a wave) and sends
  keys with xdotool. Each cycle grabs the screen (ImageMagick import), reads
  - the ship heading from the sprite at the screen centre: the green cockpit lies
    towards the nose,
  - the icons: blocks of the fixture icon colours on screen, else the blue icon dots
    of the radar,
  then brakes, turns towards the nearest icon and thrusts. It stops when the SCORE
  digits change (a capture scores) or after N seconds; exit status 0 after a score
  change, 1 otherwise. An icon that leaves the icon column (the IS_ICON window at the
  top right) without a score change is logged, and the flight goes on. The SCORE panel
  is drawn once the full status display is up (wave.sh runs its strategy stage first).
Screen geometry (wave-0001): the ship sprite is centred at 528,357; the radar is the
child window at 466..556 x 688..766, where icons are (0,128,255) dots, the screen
viewport is a (0,96,0) rectangle and the ship sits at the rectangle's centre.
"""
from __future__ import annotations

import argparse
import math
import os
import subprocess
import sys
import time

W, H = 1024, 768
SHIP = (528, 357)
RADAR = (464, 686, 560, 766)           # x0, y0, x1, y1 (exclusive)
PLAY = (0, 0, 975, 670)                # play area without the icon column and the HUD
COLUMN = (978, 30, 1024, 300)          # the icon column's icon faces
SCORE = (60, 728, 140, 760)            # the SCORE digits of the full status display
RADAR_ICON = (0, 128, 255)
RADAR_VIEW = (0, 96, 0)
ICON_COLOURS = [(255, 0, 0), (0, 255, 0), (0, 0, 255)]   # oracle/fixtures icon faces
TURN_DEG_PER_S = 384.0                 # TURN RATE 64 RPM on the help page of the TRANSFORMER


def grab() -> bytes:
    return subprocess.run(["import", "-display", os.environ["DISPLAY"], "-window", "root",
                           "-depth", "8", "rgb:-"], capture_output=True, check=True).stdout


def pixel(raw: bytes, x: int, y: int) -> tuple[int, int, int]:
    i = (y * W + x) * 3
    return raw[i], raw[i + 1], raw[i + 2]


def heading(raw: bytes) -> float | None:
    """Ship heading in degrees (0 = right, 90 = up), from the green cockpit's offset."""
    ax = ay = an = gx = gy = gn = 0
    for y in range(SHIP[1] - 22, SHIP[1] + 22):
        for x in range(SHIP[0] - 22, SHIP[0] + 22):
            r, g, b = pixel(raw, x, y)
            if r + g + b > 90:
                ax, ay, an = ax + x, ay + y, an + 1
            if g > 90 and r < 80 and b < 80:
                gx, gy, gn = gx + x, gy + y, gn + 1
    if an < 30 or gn < 5:
        return None
    dx, dy = gx / gn - ax / an, gy / gn - ay / an
    return math.degrees(math.atan2(-dy, dx))


def blobs(points: list[tuple[int, int]], gap: int) -> list[tuple[float, float, int]]:
    """Group points closer than gap (single link); returns (cx, cy, count) per group."""
    groups: list[list[tuple[int, int]]] = []
    for p in points:
        near = [g for g in groups if any(abs(p[0] - q[0]) <= gap and abs(p[1] - q[1]) <= gap for q in g)]
        merged = [p]
        for g in near:
            merged.extend(g)
            groups.remove(g)
        groups.append(merged)
    return [(sum(x for x, _ in g) / len(g), sum(y for _, y in g) / len(g), len(g)) for g in groups]


def radar(raw: bytes) -> tuple[tuple[float, float] | None, list[tuple[float, float, int]]]:
    """The ship's radar position (viewport centre) and the icon dots."""
    view, icons = [], []
    x0, y0, x1, y1 = RADAR
    for y in range(y0, y1):
        for x in range(x0, x1):
            c = pixel(raw, x, y)
            if c == RADAR_VIEW:
                view.append((x, y))
            elif c == RADAR_ICON:
                icons.append((x, y))
    centre = None
    if view:
        xs, ys = [x for x, _ in view], [y for _, y in view]
        centre = ((min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2)
    return centre, blobs(icons, 1)


def icon_faces(raw: bytes, area: tuple[int, int, int, int], skip_ship: bool) -> list[tuple[float, float, int]]:
    """Fixture icon faces in area (blocks of at least 40 pixels sampled on a 2-pixel grid)."""
    pts = []
    x0, y0, x1, y1 = area
    for y in range(y0, y1, 2):
        for x in range(x0, x1, 2):
            if skip_ship and abs(x - SHIP[0]) < 24 and abs(y - SHIP[1]) < 24:
                continue
            if pixel(raw, x, y) in ICON_COLOURS:
                pts.append((x, y))
    return [b for b in blobs(pts, 4) if b[2] >= 40]


def column_count(raw: bytes) -> int:
    return len(icon_faces(raw, COLUMN, False))


def score_digits(raw: bytes) -> bytes:
    x0, y0, x1, y1 = SCORE
    return b"".join(raw[(y * W + x0) * 3:(y * W + x1) * 3] for y in range(y0, y1))


def target(raw: bytes) -> tuple[float, float, str] | None:
    """Vector from the ship to the nearest icon in screen pixels, and its source."""
    on_screen = icon_faces(raw, PLAY, True)
    if on_screen:
        x, y, _ = min(on_screen, key=lambda b: (b[0] - SHIP[0]) ** 2 + (b[1] - SHIP[1]) ** 2)
        return x - SHIP[0], y - SHIP[1], "screen"
    centre, dots = radar(raw)
    if centre and dots:
        x, y, _ = min(dots, key=lambda b: (b[0] - centre[0]) ** 2 + (b[1] - centre[1]) ** 2)
        return (x - centre[0]) * 32, (y - centre[1]) * 32, "radar"
    return None


def angle_diff(a: float, b: float) -> float:
    """a - b folded into -180..180."""
    return (a - b + 180) % 360 - 180


def key_for(seconds: float, keysym: str) -> None:
    subprocess.run(["xdotool", "keydown", keysym], check=True)
    time.sleep(seconds)
    subprocess.run(["xdotool", "keyup", keysym], check=True)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--seconds", type=float, default=90)
    ap.add_argument("--log", type=argparse.FileType("a"), default=sys.stdout)
    a = ap.parse_args(argv)

    def say(text: str) -> None:
        a.log.write(f"autopilot: {text}\n")
        a.log.flush()
    start = time.monotonic()
    raw = grab()
    icons, score = column_count(raw), score_digits(raw)
    say(f"{icons} icons in the icon column")
    while time.monotonic() - start < a.seconds:
        raw = grab()
        n = column_count(raw)
        if score_digits(raw) != score:
            say(f"score changed after {time.monotonic() - start:.1f} s; {n} icons in the column")
            return 0
        if n < icons:
            say(f"{n} icons in the column after {time.monotonic() - start:.1f} s, score unchanged")
            icons = n
        h, t = heading(raw), target(raw)
        if h is None or t is None:
            say(f"heading {h} target {t}")
            time.sleep(0.5)
            continue
        dx, dy, src = t
        dist = math.hypot(dx, dy)
        err = angle_diff(math.degrees(math.atan2(-dy, dx)), h)
        say(f"{time.monotonic() - start:5.1f} s heading {h:4.0f} target {src} {dx:5.0f},{dy:5.0f} error {err:4.0f}")
        key_for(0.25, "Down")                                  # brake: kill the drift
        if abs(err) > 12:
            key_for(max(0.02, abs(err) / TURN_DEG_PER_S), "Left" if err > 0 else "Right")
        else:
            key_for(0.35 if dist > 300 else 0.15, "Up")
            time.sleep(0.4 if dist > 300 else 0.25)
    say("no score change")
    return 1


if __name__ == "__main__":
    sys.exit(main())
