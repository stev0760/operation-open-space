# Documents

A map of this directory. Each topic has one file; the others link to it. [DESIGN]

## Where to start
- [`PLAN.md`](PLAN.md): the milestones, each stage's results and its open items.
- [`spec/checklist.md`](spec/checklist.md): every feature of the original with its evidence and its status in the port.
- [`re/pitfalls.md`](re/pitfalls.md): the traps in reading this game's code.

## Reverse-engineering reference (`re/`)
What the original's code does, each claim tagged with its evidence and confirmed by an independent verifier. [DESIGN]

| File | Topic |
|---|---|
| [`shell.md`](re/shell.md) | Start-up, screens, intro, dialogs and exit |
| [`nav.md`](re/nav.md) | The Navigation window, the directory tree and the filesystem walk |
| [`world.md`](re/world.md) | The wave: set-up, world, flight and drawing |
| [`core.md`](re/core.md) | Collisions, damage, capture, the wave end, weapons and the economy |
| [`sound.md`](re/sound.md) | Sound and music |
| [`ispace.md`](re/ispace.md) | ISPACE.DLL: palette, bitmaps, sound and dialogs |
| [`palette-tables.md`](re/palette-tables.md) | ISPACE.DLL's colour tables |
| [`install.md`](re/install.md) | INSTALL.EXE, the sold installer |
| [`formats.md`](re/formats.md) | Data-file formats and the INI keys |
| [`resources.md`](re/resources.md) | NE resources |
| [`structs.md`](re/structs.md), [`globals.md`](re/globals.md) | Data structures, globals and the main loop |
| [`timing.md`](re/timing.md), [`rng.md`](re/rng.md) | Frame timing and random numbers |
| [`win16-translation.md`](re/win16-translation.md) | How Win16 calls map to Win32 |
| [`oracle.md`](re/oracle.md) | Running the original under Wine as the reference |
| [`ghidra.md`](re/ghidra.md) | The Ghidra pipeline |

## Decision records (`decisions/`)
Where the port differs from the original, and why. [DESIGN]

| ADR | Topic |
|---|---|
| [0001](decisions/0001-timestep.md) | Timestep |
| [0002](decisions/0002-asset-source.md) | Asset source |
| [0003](decisions/0003-text-and-paths.md) | Text and paths |
| [0004](decisions/0004-ini-location.md) | INI location |
| [0005](decisions/0005-help.md) | Help |
| [0006](decisions/0006-window-scaling.md) | Window and scaling |
| [0007](decisions/0007-palette.md) | Palette |
| [0008](decisions/0008-registration.md) | Registration and the installer |
| [0009](decisions/0009-cpu-paced-animation.md) | CPU-paced animation |
| [0010](decisions/0010-single-instance.md) | Single instance |
| [0011](decisions/0011-audio.md) | Audio |
| [0012](decisions/0012-filesystem-walk.md) | Filesystem walk and icons |
| [0013](decisions/0013-wave-drawing-and-input.md) | Wave drawing and input |
| [0014](decisions/0014-saved-games.md) | Saved games |
| [0015](decisions/0015-collision-orientation.md) | The collision pair's orientation word |
| [0016](decisions/0016-undefined-reads.md) | Reads the original leaves undefined |
| [0017](decisions/0017-gdi-raster-reference.md) | Wine's GDI raster as the reference for polygon masks |

## Elsewhere
- [`spec/`](spec/README.md): the behaviour spec's plan and the checklist.
- [`upstream/`](upstream/): Wine bugs found on the way.
- [`../oracle/digests/`](../oracle/digests/): what each scripted run of the original recorded.
