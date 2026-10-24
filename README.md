# Operation: Inner Space — Clean-Room Port

A modern reimplementation of **Operation: Inner Space** (Software Dynamics, 1994–2000),
the Windows 3.1 game where ships fly around your desktop capturing icons.

The signature mechanic: **the game world IS your filesystem.** Executables, shortcuts,
and files with icons become capturable nodes. Ships compete to capture
them. On a modern PC `C:\Windows\System32` alone yields thousands of icons.

**Target platforms:** Windows 10/11 natively; Linux/macOS via Wine.
**Build status: nothing implemented yet** — `src/` is empty. This is a design plus a
spec derived from reverse-engineering the original.

> **Status of claims in this file, and how to read them.** The port is being built
> from RE of the original binary, but not everything about the original is known yet.
> This file marks each claim:
>
> - **[VERIFIED]** — established from the binary; see
>   `/home/cabal/Git/repos/innerspace-rea/BREAKTHROUGH.md`
> - **[DESIGN]** — our choice for the port; not a property of the original
> - **[UNVERIFIED]** — believed or assumed, **not** yet established. Do not treat these
>   as findings.
>
> The RE reference is authoritative over this file. Where they disagree, it wins.

## Stack — [DESIGN]

- **C++17** — Win32 GDI + `AlphaBlend` rendering, winmm audio, Shell/COM desktop
  integration
- **No game engine** — raw Win32 gives exact control over a transparent desktop
  overlay, which is awkward in Unity/Godot
- **CMake** — dual-target: MinGW-w64 (Wine testing) + MSVC (shipping)

## Assets — [VERIFIED]

The port loads the **original game files** at runtime; no re-encoding.

| Asset | Source | Format | Status |
|---|---|---|---|
| Ship stats | `MASTER.BOD` | custom binary, 52 × 44-byte records | format **decoded** |
| Ship sprites | `MASTER.SHP` + THEWORKS `.DIB` files | DIB (BMP variant) | `.SHP` format **NOT decoded** |
| Music | `MAIN.MID`, `ANTHEM.MID`, `DEMON.MID`, `RACE.MID` | standard MIDI | — |
| SFX | THEWORKS/ (~45 WAV files) | standard WAV | — |
| Config | `INSPACE.INI` | INI, via `GetPrivateProfile*` | keys recovered |

Original asset directories are configured in `INSPACE.INI` as **`SHIPS\`**
(`ShipPath`), **`SOUNDS\`** (`SndPath`) and **`GAMES\`** (`RacePath`) — **[VERIFIED]**.

Asset search path — **[DESIGN]**: exe directory, then `INSPACE_ASSETS`, then
`~/.innerspace/assets/`.

## Build — [DESIGN]

```bash
# MinGW-w64 (for Wine testing)
cmake -B build-mingw -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64.cmake
cmake --build build-mingw

# MSVC (for shipping on Windows)
cmake -B build-msvc -G "Visual Studio 17 2022" -A x64
cmake --build build-msvc --config Release
```

## Run — [DESIGN]

```bash
innerspace.exe          # Windows
wine innerspace.exe     # Wine (Linux/macOS)
```

## Controls — [UNVERIFIED — placeholder]

The original's input layer is a **196-case switch in selector 1010 (seg3)**, located
but **not transcribed**. The table below is a convenient placeholder, not the
original's mapping. Do not document it as fact or build user-facing docs from it.

| Input | Action |
|---|---|
| Arrow keys / WASD | Thrust / steer |
| Space | Fire |
| Right mouse | Alt-fire |
| M | Menu |
| P | Pause |
| Esc | Quit |

## How it works

1. **Overlay window** — **[DESIGN]**. A transparent, click-through, always-on-top
   full-screen window (`WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST`) over the
   desktop. The original instead used a normal window plus separate `IS_ICON`,
   `IS_WATCH`, `IS_SMART`, `IS_HELP` child windows — **[VERIFIED]** window classes.
   Whether to keep the original's multi-window model or collapse to one layered
   overlay is an open design question.

2. **Icon extraction** — **[VERIFIED]** as to the original's mechanism. It does **not**
   use `SHGetFileInfo`. The original's fallback ladder is:
   **the file's own icon** (`ExtractIcon(hInst, path, 0xffff)`) → **the first embedded
   icon** (`ExtractIcon(hInst, path, 0)`) → **a built-in resource icon**
   (`LoadIcon` with `0x69`/`0x6a`/`0x6b`, selected by a type byte; `0x0e` → `0x6a`).
   Icons are **32×32**, rasterised once into a cached compatible bitmap
   (`DGROUP:0x94ba`), not re-created per icon. A second routine lays multiple icons
   from one file into a horizontal strip at `x = 38n + 6`.

3. **Game tick** — **[VERIFIED]** that the original is `SetTimer`-driven and aborts
   with *"All timers used. Can't run game."* when timers are unavailable. The INI
   requests `FrameRateSet = 36`. **[DESIGN]**: the port should use a proper frame loop
   rather than inheriting that failure mode.

4. **Rendering** — **[DESIGN]**. GDI sprites blitted with `AlphaBlend`. The original
   used `BitBlt`/`StretchBlt`/`PatBlt` with palettes (`SelectPalette`,
   `RealizePalette`) — **[VERIFIED]** API usage. A "50k+ entities at 60 FPS" target is
   ours, not the original's.

5. **Capture — [VERIFIED FROM BYTES, RE `BREAKTHROUGH.md` §10a.]** An icon is an entity
   in the `0x70a0` array (stride `0x15a`). **`entity[i].+0x51` is the boolean owned
   flag** (`1` = claimed, `0` = free). `entity[i].+0x22` = **owner player index** and
   `+0x24` = **owner ship slot** — used to reach that player's ship array through the
   `0x1df8` table (`shl $0x2` then `les 0x1df8(%si)`) and read the ship's `+0x04` (X
   position) as a **32-bit pair**. Progress is `entity[i].+0x139`, moved **±2 per tick**,
   and **the direction is set by `ship[+0x52]`**: zero on the referenced ship
   **decrements**, non-zero **increments**. Completion requires game mode
   `DGROUP:0x9320 == 6`, `entity[+0x2c] == 2` and `entity[+0x139] >= 5`, then calls
   `seg9:0x88ba`. Capture state lives in `entity[+0x2a]` (values 2/3/4/7;
   `7 → 2` on normalisation). **Port requirement: this is a real per-icon state machine,
   not a proximity test.** One sub-probe remains — the 7-argument effect call at
   `<seg>:0x0d6c` for the exact reward arithmetic.

   > **Historical (superseded) note — kept so it is not re-derived.** The mechanic *was*
   > described here as unverified with a proximity model.
   project's central risk and is **not yet pinned**. What *is* established: ships and
   346-byte entities are **parallel arrays indexed by the same index**, so slot `i` is
   a ship plus its companion entity; `ship[i].+0x28` is a **mod-16 accumulator in
   [0,0xf]** driven by `entity[i].+0x02`, wrapping to 0 above 15 and clamping to 15
   below 0; and there is an entity reset/despawn path that clears several fields
   together. **Not established: where ownership or score is transferred.** The
   commonly assumed "ship touches a node → node disappears → score increases" model is
   *plausible but unconfirmed*. It must be confirmed before implementing capture.

6. **Audio** — **[DESIGN]**. MIDI via `midiOut*`, WAV via `waveOut*`, with a software
   mixer replacing the original's `WAVEMIX.DLL`. MMSYSTEM is a real dependency
   — **[VERIFIED]**.

## Data model — [VERIFIED] where noted

The original's structures, as recovered. Field offsets are bytes within the record.

**Ship** — 112 bytes (`0x70` stride). **There is not one ship array: there are about
**one array per player, stored as a table** at `DGROUP:0x1df8` … `0x1e34` (4-byte
entries, 15 of them), selected by `4 × player index` — see the capture mechanic below,
which resolves the index. Each entry is
far pointer to a stride-`0x70` struct with the **same field layout**. `DGROUP:0x1e30`
is the busiest (737 accesses). Ship count is `DGROUP:0x9316`; the current player's ship
index is `DGROUP:0x9646`. **Open:** what the fifteen pointers denote (per-team,
per-player, per-arena is unresolved) — so model the *layout* once and the *instances*
as a collection. Do not hard-code a single global array.

| Offset | Type | Meaning | Status |
|---|---|---|---|
| +0x18 | int | active flag; **0 = inactive** (loop gate) | VERIFIED |
| +0x28 | word | mod-16 accumulator in [0,0xf] | VERIFIED |
| +0x51 | byte | **owner / player index — values 1 and 2** | VERIFIED |
| +0x5e | far ptr | **behaviour dispatch pointer** | VERIFIED |
| +0x66 | far ptr | second dispatch pointer | VERIFIED |
| **+0x04** | word | **X coordinate** (the pair is pushed as a long 104×) | strongly evidenced |
| **+0x06** | word | **Y coordinate** | strongly evidenced |
| others | — | ~20 offsets unmapped | see RE reference §10 |

> **Behaviour is pointer-dispatched, not switch-dispatched.** The original changes a
> ship's behaviour by **replacing a function pointer** and invoking it via
> `lcall *%es:0x5e(%bx,%si)`. A `switch` on a state integer will **not** reproduce the
> original's transitions — model this as a replaceable behaviour function pointer.

**Entity** — 346 bytes (`0x15a` stride), a **direct** DGROUP array at base
`DGROUP:0x70a0`, indexed with the *same* index as the ship array. Verified fields:

| Offset | Type | Meaning |
|---|---|---|
| +0x02 | word | signed increment driving `ship[i].+0x28` |
| +0x22 | word | **frame delay** (8 during animation, 12 at init) |
| +0x24 | word | **derived sprite/frame index** = `2*DGROUP:0x9318 + frame` |
| +0xca | word | signed **motion direction**, negated on each animation wrap; `abs()` thresholded vs `DGROUP:0x9ea0` |
| +0x138 | byte | **animation frame counter**, `0 .. DGROUP:0x9ea2-1`, one per tick |

This is an **animation state machine**: frames advance one per tick and the motion
value reverses each cycle close (`BREAKTHROUGH.md` §5.2).

**Also present:** a 52-byte (`0x34`) entity array, plus arrays at `DGROUP:0x6e24`
(`0x7c`), `0x6b04` (`0x46`), `0x6b08` (`0x229`), `0x69f4` (`0x297`) and `0x970A`
(`0xad`).

## Teams

The original's `INSPACE.INI` names 8 teams — **AVENGERS, PREDATORS, FUZZY ONES,
PIRATES, KNIGHTS, SPEED DEMONS, RENEGADES, ENFORCERS** — **[VERIFIED]** as INI
content. **[UNVERIFIED]**: that each has distinct AI behaviour. Note `ship[i].+0x51`
holds an owner value of **1 or 2**, not a team index 0–7, so the relationship between
these team names and the ship owner field needs establishing before use.

## Ships

52 ship types in `MASTER.BOD`: SKULL, PIRATE, TARGA, THUNDERBIRD, JAGUAR, TALON,
CAVALIER, HEART, EAGLE, FALCON, BONUS BUNNY, TEDDY BEAR, MUSTANG, CANADA, DOLPHIN,
INTERCEPTOR, DRAGON, SAUCER, EXCALIBUR, ORCA, BERZERK, LACKEY, TIGER, RUNABOUT,
BALD EAGLE, VIPER, HOT ROD, BULLET, ROCKER, ROCKET, ENIGMA, DUCK, KOALA, TRANSFORMER,
TERMINATOR, BAT, PLATYPUS, BEAVER, BIPLANE, REAPER, IMPALA, STEALTH, EYE, ZEN, SHARK,
REBEL, F-18, F-16, MiG-29, TROUBLE, SPECTRUM.

## Spec completeness — estimate (2026-10-16)

**~45%** toward a spec sufficient to port the game (was ~35% before the capture mechanic
was decoded). Judgment-based, ±10 points, and it depends entirely on the fidelity target. Breakdown, so the number can be argued with:

| Area | Weight | Known | Note |
|---|---|---|---|
| Asset/data formats | 0.22 | ~65% | BOD decoded, **SHP not decoded at all** |
| Core runtime (structs, loop, timing) | 0.20 | ~65% | plumbing largely mapped |
| Signature mechanic (icon -> capture) | 0.25 | **~72%** | transfer **DECODED** (RE §10a); only the reward arithmetic (`<seg>:0x0d6c`) open |
| Rendering | 0.13 | ~48% | blit known, **sprite frame layout unknown** |
| Input | 0.05 | ~5% | mapping located, not transcribed |
| Audio | 0.05 | ~20% | API-level only |
| AI / teams / combat / UI-HUD | 0.10 | ~7% | **near-unknown** |

**The number that matters is the asymmetry, not the average.** Infrastructure is
~65-70% known — packing, the function inventory (880 procedures), struct *layouts*,
API identification, the tick, the icon ladder. **Behaviour is now ~30-35% known**
(raised by the capture decode), and behaviour is what a port must reproduce. Be wary of
the weighted **53.5%** figure a naive average produces: it is inflated by easy
infrastructure items (DIB/MIDI/WAV, function inventory) and it averages away the fact
that **every opponent's AI remains near-unknown**. The signature mechanic is no longer
in that list — it was decoded 2026-10-16.

**Contrast:** only ~14 of 880 functions have been meaningfully decoded (1.6%). Function
count is the wrong denominator — most functions are incidental — but it shows how
little *code* has actually been read versus *structure* enumerated.

**What moves it fastest, in order:** (1) ~~the capture transfer~~ **DONE** — decoded in RE
`BREAKTHROUGH.md` §10a; the new first mover is the reward arithmetic (`<seg>:0x0d6c`);
(2) `MASTER.SHP`;
(3) the input mapping; (4) team AI. Items 1-3 are each days of work with the current
toolchain; item 4 is the open-ended one.

**How those gaps get closed is planned in `innerspace-rea/ATTACK-PLAN.md`** — per-target
probes and success criteria, not just a wish list.

## Known gaps blocking completion

Stated plainly so they are not mistaken for done work:

1. ~~**Capture mechanic** — unconfirmed~~ **RESOLVED** (see §How it works 5). The one
   remaining sub-item is the reward arithmetic in the `<seg>:0x0d6c` effect call.

   **Estimate note:** the capture component moved ~44% -> ~72%, so the recomputed
   weighted average is **53.5%** (was 46.5%). Applying the same
   infrastructure-inflation caveat as before, the behaviour-weighted headline moves
   from ~35% to **~45% +/- 10** — that discount is judgement, not measurement.
2. **`MASTER.SHP` format** — sprite count, frame layout, DIB pairing unknown.
3. **~18 ship field offsets** unmapped. The old "worst gap `+0x04`" is resolved — it
   is the X coordinate (with `+0x06` = Y).
4. **Input mapping** — not transcribed.
5. **Registration/nag** check — xref not traced.

## Clean-room notice

A **clean-room reimplementation**. All new code is written from scratch, informed by
reverse-engineering of the original binary (see
`/home/cabal/Git/repos/innerspace-rea/BREAKTHROUGH.md`). The original's data files
(sprites, MIDI, WAV, ship stats) are loaded as-is. No original code is copied.

## Original game

**Operation: Inner Space** v1.29.08 (Build Aug 9, 2000)
© 1994–2000 Software Dynamics, Inc.
Windows 3.1 strategic action game — "Inner Space — An Adventure in Windows!"

## License

Clean-room reimplementation of abandonware. Original game assets remain the property
of their respective copyright holders and are used here for archival/preservation
purposes. All new code in this repository is provided as-is for the abandonware
community.
