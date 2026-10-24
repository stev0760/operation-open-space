# Operation: Inner Space — Clean-Room Port

**Project:** Reimplementation of Operation: Inner Space v1.29.08 (Software Dynamics, 1994-2000)
**Codename:** InnerSpace-Port
**Repo:** `/home/cabal/Git/repos/innerspace-port/`
**Date codified:** 2026-10-16
**Status:** PLANNING — RE of original complete enough to begin

---

## 0. Why this project

Operation: Inner Space is a Windows 3.1 game where ships fly around your desktop
capturing icons. The signature mechanic: **the game world IS your filesystem**.
Every executable, shortcut, and folder with an icon becomes a capturable node in
real-time 3D-ish space. On a modern PC, `C:\Windows\System32` alone yields
thousands of icons — the folder is intentionally not completable, and the game
does not require capturing every icon to win.

The original is abandonware. It runs under Wine (ironically) but not on modern
Windows. We are doing a **clean-room reimplementation** informed by our full
reverse-engineering of the original binary.

## 1. Reverse-engineering status

**Authority note (important):** the Ghidra NE decompilation is **NOT authoritative**
for this binary and must not be used as the spec source:

- `ghidra-out/decompiled.c` (38 MB, packed bytes) — **VOID**
- `ghidra-out/decompiled-unpacked.c` (20.9 MB) — **DEMOTED, low reliability**.
  Ghidra's NE analysis of this program is materially damaged: it disassembles the
  DGROUP *data* segment as code (~1,700 bogus 1–8 byte functions out of 3,969),
  truncates real bodies (the WndProc decompiles to 332 lines but is recorded as a
  17-byte body with `call_count 0`), and renders OptLoader chain-link words as
  `func_0x…` call targets.

**Byte-level artifacts are authoritative for call structure:**
`relocs.json` (930 relocation records), `api-sites.json` / `api-sites.tsv`
(9,977 fixup sites), `api-imports.json` (186 ordinals → 183 named APIs),
`annotated-asm/segNN.asm` (17 code segments), `callgraph.json` (122 internal
function entries), raw `unpacked/segNN.bin`.

Validated invariants (all exact): relocation table fills each packed segment to the
byte (17/17, zero slack); decoded item counts == loader count word (18/18); import
records land on genuine `lcall` opcodes (722/722); raw `9a ?? ?? 00 00` pattern count
== import site count (5,900 == 5,900); 9,910/9,977 sites (99.3%) matched to
disassembled instructions.

### Verified structures

**Ship struct** — 112 bytes (0x70 stride), array base via far pointer `DGROUP:0x1e30`,
loop bound (count) `DGROUP:0x9316`, current-player index `DGROUP:0x9646`:

| Offset | Type | Meaning | Status |
|---|---|---|---|
| +0x00 | word | (paired word with +0x02) | unmapped |
| +0x02 | word | — | unmapped |
| +0x04 | **long (32-bit)** | **X coordinate** (paired with `+0x06` = Y); bounds-tested, `pushl`'d as ONE argument to the sprite-centring helper | **X — strongly evidenced, not fully verified.** Ship-specific accesses via `DGROUP:0x1e30` = **94**; the former "240 accesses" was a cross-struct conflation and is withdrawn |
| +0x18 | int | **active flag; 0 = inactive**, used as the loop gate | VERIFIED |
| +0x1a | word | — | unmapped (63 accesses) |
| +0x22 | int | counter/timer | partial |
| +0x26 | int | state machine value | partial |
| +0x28 | word | **mod-16 accumulator in [0,0xf]**, driven by a companion entity's `+0x02`, wraps to 0 above 15, clamps to 15 below 0 | VERIFIED (NOT a velocity timer) |
| +0x44 | — | — | unmapped (76 accesses) |
| +0x46 | — | — | unmapped |
| **+0x51** | **byte** | **owner / player index — values 1 and 2** | VERIFIED (earlier "+0x52 team, 0-7 of 8 teams" was **wrong**) |
| +0x54 | — | — | unmapped (48 accesses) |
| **+0x5e** | **far ptr** | **dispatch function pointer (behaviour handler)** | VERIFIED |
| +0x60 | word | segment half of the +0x5e pointer, written by relocation | VERIFIED |
| **+0x66** | **far ptr** | **second dispatch function pointer** | VERIFIED |

> **Behaviour is pointer-dispatched, not switch-dispatched.** State transitions
> replace the pointer (`movw $0x2652,%es:0x5e(...)`) and it is invoked with
> `lcall *%es:0x5e(%bx,%si)`. A `switch` on a state integer will not reproduce the
> original's transitions.

**Entity struct** — 346 bytes (0x15a stride), **direct DGROUP array at base
`DGROUP:0x70a0`** (not pointer-reached). Indexed with the *same index* as the ship
array, so slot `i` is a ship plus its companion entity. Fields range `+0x22` to
`+0x138` (`0x138 = 312 < 346`, so the stride fits exactly — this is what confirms
the base). Named so far: `+0x02` (signed increment driving ship `+0x28`),
`+0x22` (DWORD, init 12), `+0x28`/`+0x2c` (words, init 2), `+0xca` (signed, abs'd and
thresholded against `DGROUP:0x9ea0`), `+0x138` (byte, init `0xFE`).

**Also present:** a 52-byte (0x34 stride) entity struct with 126 accesses, and a
third array at `DGROUP:0x970A` with stride 0xad (173 bytes), indexed by a field of
the 0x15a struct.

### Verified globals (DGROUP, linear base 0x10000)
`0x1e30` ship far pointer · `0x1e58/0x1e5a` 32-bit score accumulator ·
`0x1e68/0x1e6a` 32-bit value subtracted from the per-player array ·
`0x238c` gate flag · `0x3a02` game DC · `0x5078/0x507a` 32-bit score accumulator ·
`0x88c0` 32-bit viewport scale divisor · `0x91fc` announcement/UI flag ·
`0x9316` **ship/entity count** · `0x94a6` extracted-icon counter ·
`0x94a8`/`0x94aa` scoring inputs · `0x94ba` cached icon bitmap ·
`0x9646` **current player's ship index** · `0x967c` incrementing counter ·
`0x9684` **game mode (1 or 2)** · `0x9e60` per-player 32-bit array (stride 4) ·
`0x9ea0` threshold · `0x9ea2`/`0x9ea4` loop bounds · `0x9eb5` flag.

> **SUPERSEDED — the table reading is CORRECT after all.** `DGROUP:0x1df8–0x1e34` is a
> **15-entry table of 4-byte far pointers**, and it is **indexed**:
> `mov idx,%si; shl $0x2,%si; les 0x1df8(%si),%si`. `0x1df8 + 15*4 = 0x1e34` exactly.
> **The entries are per-player ship arrays** — the capture code selects one by
> `4 × entity[+0x22]` (owner player) (`BREAKTHROUGH.md` §10a). The 32-bit accumulators
> live at `0x1e58+`, outside the table.
>
> **Historical (superseded) — this was stated WRONG, twice.** A revision of this file
> said "not a pure far-pointer table but a mixed global block holding far pointers *and*
> 32-bit scalars (`0x1e58/0x1e5a`, `0x1e68/0x1e6a` accumulated with carry) — do not walk
> it as a pointer array." **Withdrawn.** The 32-bit scalars sit at `0x1e58+`, **outside**
> `0x1df8–0x1e34`, so they never contradicted the table reading.
> **Walk it as a 4-byte pointer array.**

**Viewport/camera** — `DGROUP:0x1e34` is a far pointer to a *single* global object
(no index register). It holds the world-to-screen transform:
`(field +0x22 << 13) / DGROUP:0x88c0 - 250`, plus a lerp writing `+0x10`/`+0x12`
from `+0x22`/`+0x24`/`+0x28`. Feeds the port's `worldScale` / `worldOffset`.

### Other verified structure
- **WndProc**: `FUN_1008_0224 @ 1008:0224` (all window messages)
- **Game tick**: `FUN_1008_11f8()` driven by **`SetTimer`** — the original fails hard
  with *"All timers used. Can't run game."* if timers are unavailable
- **Input**: 196-case switch in selector 1010
- **Ship stats**: `MASTER.BOD` = 52 ships × 44-byte records (DECODED)
- **Single instance**: *"Another version of game already running."*
- **Internal call graph**: 122 cross-segment function entries (`callgraph.json`).
  Most-called: `seg6:0x0f1a`, `seg9:0x88ba`, `seg5:0x00dc` (7 callers each),
  `seg3:0x06ec` (6)

### Config surface (`INSPACE.INI`, read via `GetPrivateProfileInt/String`)
Asset subdirectories: **`SHIPS\`** (`ShipPath`), **`SOUNDS\`** (`SndPath`),
**`GAMES\`** (`RacePath`). Keys recovered: `Language` (`ENG`), `Path`, `LocalPath`,
`MainShip`, `ThePlayer` ("your name"), `Games`, `Dead`, `Size`, `Calibrate`,
`GameSpeed`, `FrameRateSet` (=36), `JBtn`, `JoyStick`, `Quick`, `Detail`, `Enhance`,
`MixChannels`, `QuickIntro`, `BadVideo`, `Braking`, `ShipAnim`, `Border`, `Menu`,
`Bright`, `Complex`, `Aux`, `Rate`, `Full`, `OnTop`, `Language`, `PopupMenu`,
`MainAccel`, `ShowLaunch`, `IconWnd`, `VScale`/`SScale`, `SmWnd`, `RadarLeft/Top`,
`WatchLeft/Top/Right`, `IconLeft/Top`, `SmLeft/Top`, `HelpLeft/Top`,
`ShowLeft/Top/Right/Bottom`.
Window classes: `GAME`, `IS_HELP`, `IS_WATCH`, `IS_ICON`, `IS_SMART`.
External modules referenced: `ISPACE.DLL`, `IGRAPHIC.DLL`, `CTL3D.DLL`,
`_LANG.DLL`, `_HELP.HLP`, `GRMSETUP.EXE`, `SYSTEM.INI`, `WIN.INI`.
Recovered message: *"Some game settings are missing. Will attempt to regenerate
INSPACE.INI."*

## 2. Target platform

| Property | Decision |
|---|---|
| **Language** | C++17 |
| **Rendering** | Win32 GDI + `AlphaBlend` (per-pixel alpha) |
| **Audio** | winmm — `midiOut*` for MIDI, `waveOut*` for WAV (replaces WAVEMIX.DLL) |
| **Desktop integration** | Win32 Shell APIs + COM (`IShellLink`) |
| **Overlay** | `WS_EX_LAYERED \| WS_EX_TRANSPARENT \| WS_EX_TOPMOST` full-screen |
| **Build** | CMake, dual-target: MinGW-w64 (Wine testing) + MSVC (shipping) |
| **Engine** | None — raw Win32 for exact control over transparent desktop overlay |
| **Target OS** | Windows 10/11 (native), Linux/macOS via Wine |

### Why C++ (not Rust):
1. Raw Win32 + GDI + winmm are C-native APIs — `#include <windows.h>` away
2. Desktop icon enumeration needs COM (`IShellLink`) — native in C++, verbose `unsafe` in Rust
3. Wine compatibility is the explicit goal — C++ Win32 is the gold standard on Wine
4. Ghidra decompilation is C — struct layouts map directly to C++ structs
5. GDI+ is incomplete on Wine; D3D is overkill for 2D sprites — GDI + `AlphaBlend` it is

### Why no game engine:
Transparent desktop overlays are awkward in Unity/Godot. Raw Win32 gives exact
control over layering, click-through, and always-on-top behavior.

## 3. Original game assets (loaded at runtime from game files)

The port loads assets from the **original game files** — no re-encoding needed:

| Asset | Source file | Format | Port handling |
|---|---|---|---|
| Ship sprites | `MASTER.SHP` (7,680 bytes) + THEWORKS `.DIB` files | DIB (BMP variant) | Load DIB → `HBITMAP` → blit with `AlphaBlend` |
| Ship stats | `MASTER.BOD` (2,304 bytes, 52×44-byte records) | Custom binary | Parse directly into `ShipDef` struct |
| MIDI music | `MAIN.MID`, `ANTHEM.MID`, `DEMON.MID`, `RACE.MID` | Standard MIDI | `midiOutShortMsg` via winmm |
| WAV SFX | THEWORKS/ (~45 WAV files: LASER, KABOOM, MISSILE, etc.) | Standard WAV | `waveOutWrite` via winmm |
| Config | `INSPACE.INI` | INI | `GetPrivateProfileInt/String` |
| Ctl3d | `CTL3D.DLL` | DLL | Skip — use native Windows 3D-look buttons instead |
| Language | `ENG_LANG.DLL` | DLL | Skip — hardcode English strings |

Asset search path: exe directory first, then `INSPACE_ASSETS` env var, then
`~/.innerspace/assets/`. Ships as a single folder with the original files.

## 4. Architecture

```
innerspace-port/
├── CMakeLists.txt
├── README.md
├── PLAN.md                    ← this file
├── src/
│   ├── main.cpp               ← WinMain, message loop, window creation
│   ├── game.h / game.cpp      ← Game state, tick loop (FUN_1008_11f8 equivalent)
│   ├── ship.h / ship.cpp      ← Ship struct, movement, AI, combat
│   ├── icon_node.h/.cpp       ← Filesystem icon node (capturable entity)
│   ├── icon_scanner.h/.cpp    ← Async filesystem walk + icon extraction
│   ├── renderer.h / renderer.cpp  ← GDI sprite rendering, AlphaBlend
│   ├── audio.h / audio.cpp    ← MIDI + WAV playback via winmm
│   ├── overlay.h / overlay.cpp ← Transparent layered window management
│   ├── wndproc.h / wndproc.cpp ← Window procedure (FUN_1008_0224 equivalent)
│   ├── input.h / input.cpp    ← Keyboard/joystick input (196-case switch equivalent)
│   ├── bod_parser.h / bod_parser.cpp  ← Parse MASTER.BOD ship stats
│   ├── shp_parser.h / shp_parser.cpp  ← Parse MASTER.SHP sprite data
│   ├── dib_loader.h / dib_loader.cpp  ← Load DIB files to HBITMAP
│   ├── config.h / config.cpp  ← INI config loading
│   ├── team.h                 ← Team definitions (8 teams from INI)
│   └── types.h                ← Common types (Vec2, Rect, ShipDef, etc.)
├── assets/                    ← (populated at runtime from original game files)
│   └── (MASTER.BOD, MASTER.SHP, *.DIB, *.MID, *.WAV, INSPACE.INI)
└── tests/
    └── test_bod_parser.cpp    ← Unit tests for asset parsers
```

## 5. Core data structures

### ShipDef (from MASTER.BOD, 44-byte record)
```cpp
struct ShipDef {
    char name[12];       // Ship name (e.g., "SKULL", "THUNDERBIRD")
    uint8_t _pad[8];     // Unknown 8 bytes
    uint16_t _unk16;     // 01 00
    uint16_t _unk18;     // 01 00
    uint8_t speed;       // off25: 17-31 (higher = faster)
    uint16_t mass;       // off26-27: 3847-14602 (hull/mass)
    uint8_t turnRate;    // off28: 18-64 (higher = tighter turns)
    uint8_t weaponClass; // off29: 3-11
    uint8_t armor;       // off30: 0-10
    uint8_t fuel;        // off31: 2-24
    uint8_t secondary;   // off32: 0-28
    uint8_t spriteIndex; // off33: 2-255 (sprite/shape index)
};
// 52 ships + 1 terminator record
```

### Ship (runtime, mirrors original 0x70=112 byte struct)
> **Instance count.** The original holds this struct behind **about fifteen** far
> pointers at `DGROUP:0x1df8`–`0x1e34` (4 bytes apart), all sharing one layout; the
> busiest is `0x1e30`. What the fifteen denote is **unresolved** (per-team / per-player
> / per-arena). Keep `Ship` instances in a collection rather than assuming one global
> array. See RE `BREAKTHROUGH.md` §5.1a.
```cpp
struct Ship {
    Vec2 position;       // +0x04 = X (word), +0x06 = Y (word)  [NOT +0x00]
                         // The pair is pushed to drawing helpers as ONE 32-bit
                         // argument (`pushl %es:0x4(%bx,%si)`, 104x), and x/y are
                         // also pushed individually as words (133x / 29x).
    Vec2 velocity;       // derived from speed/turnRate
    int active;          // +0x18: VERIFIED — 0 = inactive (used as the loop gate)
    int counter;         // +0x22: general-purpose timer
    int state;           // +0x26: state machine value
    int accumulator;     // +0x28: mod-16 accumulator in [0,0xf]  (see note)
    uint8_t owner;       // +0x51: VERIFIED — owner/player index, values 1 and 2
                         //        (former guess "0-7 of 8 teams" was wrong)
    uint8_t state2;      // +0x53: secondary state (==2 or ==0 checks)
    // --- VERIFIED FROM DISASSEMBLY (RE BREAKTHROUGH.md §5.1) ---
    // +0x5e, +0x62 and +0x66 are FAR FUNCTION POINTERS —
    // THREE of them (segment halves at +0x60, +0x64, +0x68). An earlier note
    // named only +0x5e and +0x66; see RE BREAKTHROUGH.md §5.1 for the 16
    // installed handler offsets, which ARE the behaviour set to port.
    // Behaviour transitions are performed by REPLACING these pointers, e.g.
    //     movw $0x2652,%es:0x5e(%bx,%si)   ; install handler offset
    //     lcall *%es:0x5e(%bx,%si)         ; dispatch
    // The segment halves (+0x60, +0x64) are written by relocation.
    BehaviourFn behaviour;   // +0x5e: per-ship update/handler
    BehaviourFn behaviour2;  // +0x66: second handler
    int field_74;        // +0x74: additional field
    int health;
    int fuel;
    int score;
    const ShipDef* def;  // pointer to static ship definition
    HBITMAP sprite;
    int spriteIndex;
};
```
> **Correction:** the field previously annotated as "velTimer +0x28 (cmp 0xf)" is a
> **mod-16 accumulator clamped to [0,0xf]**, driven by a companion entity's `+0x02`
> field and wrapping to 0 above 15 (RE `BREAKTHROUGH.md` §5.1, §6.2). It is not
> a timer.
>
> **Behaviour is pointer-dispatched, not switch-dispatched.** Phases 3 and 5 must
> model per-ship behaviour as a replaceable function pointer (or equivalent virtual
> dispatch), because that is how the original changes a ship's state. A `switch`
> on a state integer will not reproduce the original's transitions.

### Entity animation (VERIFIED — implement this)
The companion entity drives ship sprites through an explicit frame cycle: `+0x138`
is a frame counter in `0 .. DGROUP:0x9ea2-1` advanced one per tick; `+0x22` is the
frame delay (8); `+0x24` is the derived frame index (`2 * DGROUP:0x9318 + frame`); and
`+0xca` is a signed motion value **negated each time the cycle wraps**. Port this as a
real state machine, not as ad-hoc per-frame logic. See RE `BREAKTHROUGH.md` §5.2.

### IconNode — PORT-SIDE DESIGN, no direct original counterpart
> **Do not model this on the original's 346-byte (`0x15a`) struct.** An earlier
> version of this plan said IconNode "mirrors" that struct. It does not: ships and
> the 346-byte entities are **parallel arrays indexed by the same index**, so slot
> `i` is a ship plus its companion entity — not a desktop-icon record. What the
> original uses as a capturable node, and how capture transfers ownership, is
> **not yet established** (see the capture note in Phase 4). IconNode is therefore
> our own abstraction, constrained only by the verified facts about icon
> extraction (§6 note above) and the 32x32 cached bitmap.
```cpp
struct IconNode {
    std::wstring path;       // Filesystem path (exe, lnk, folder)
    std::wstring displayName; // Icon label
    Vec2 position;           // Screen position (virtual world coords)
    Vec2 velocity;           // Drift/float movement
    HICON icon;              // Extracted icon (cached)
    HBITMAP iconBitmap;      // Render-ready bitmap
    int captured;            // 0 = free, >0 = captured by team N
    int team;                // Owning team when captured
    int value;               // Currency value when captured
    IconType type;           // Exe, Shortcut, Folder, Dll, Other
    bool iconLoaded;         // Async icon extraction complete
};
```

### GameState
```cpp
struct GameState {
    std::vector<Ship> ships;
    std::vector<IconNode> nodes;
    std::vector<Projectile> projectiles;
    int playerTeam;
    int frameCount;
    int score;
    float worldScale;        // Zoom level
    Vec2 worldOffset;        // Viewport pan
    GameMode mode;           // Menu, Playing, Paused, GameOver
    int frameRate;           // From INI: FrameRateSet=36
    uint32_t tickTimer;      // SetTimer interval (1000/fps ms)
};
```

## 6. The filesystem walk (signature mechanic)

The original game walks the entire accessible filesystem. On a modern PC this
yields **tens of thousands** of icons (System32 alone has thousands). Design:

### Async enumeration
- Background thread walks the filesystem tree starting from drives/roots
- Emits `IconNode` entries incrementally (game world populates in real-time)
- Main thread never blocks on filesystem I/O

### Icon extraction (the bottleneck)
- `SHGetFileInfo(path, 0, &shfi, sizeof(shfi), SHGFI_ICON | SHGFI_SMALLICON)` per file
- Thread pool (4-8 workers) with a bounded work queue
- Icon cache: HICON → HBITMAP conversion cached, LRU eviction at ~2000 entries
- LRU cache prevents memory explosion on System32-scale enumeration

> **Original behaviour (now verified from the disassembly, see RE
> `BREAKTHROUGH.md` §6.1).** The original does **not** use
> `SHGetFileInfo`. It uses `SHELL!ExtractIcon` with a fallback ladder:
> 1. `ExtractIcon(hInst, path, 0xffff)` — the file's own icon
> 2. `ExtractIcon(hInst, path, 0)` — the first icon embedded in the file
> 3. `LoadIcon(hInst, 0x69/0x6a/0x6b)` — a built-in resource icon selected by a
>    type byte (== 0x0e picks 0x6a)
> Icons are rasterised **once into a single cached 32x32 compatible bitmap**
> (`CreateCompatibleDC` + `CreateCompatibleBitmap(32,32)`, `PatBlt(...,BLACKNESS)`,
> `DrawIcon`) — the DC and bitmap are not recreated per icon. Replicate the ladder
> and the caching, not just the 32x32 size. `SHGetFileInfo` remains acceptable for
> the port but must preserve the fallback and the size.

### Spatial partitioning
- Uniform grid (cell size = viewport size) or quadtree
- Only ships/nodes in visible cells update/render
- Critical for 50k+ entities on screen

### Entity types (from file extension)
| Extension | Type | Value | Notes |
|---|---|---|---|
| `.exe` | Executable | 10 | Primary currency |
| `.lnk` | Shortcut | 5 | Resolved via `IShellLink` to target |
| `.dll` | Library | 3 | System32 = thousands of these |
| `.sys`/`.drv` | Driver | 3 | System32 |
| `.ico` | Icon file | 5 | Direct icon |
| `.bat`/`.cmd` | Script | 2 | |
| Directory | Folder | 8 | Container — maybe spawns sub-nodes? |
| Other | Misc | 1 | |

### Config (INI)
```ini
[Scan]
Roots=C:\
Exclude=ganda;Recycle.Bin;Temp;Cache
MaxNodes=100000
IconSize=32
ScanDepth=unlimited
```

### Win condition
- Score threshold.
- **NOT** full filesystem capture — deliberately not completable.
- **NO FILESYSTEM BROWSER — and none is needed** (RE `BREAKTHROUGH.md` §10/icon-source).
  The original contains **no directory enumeration of any kind**: no `FindFirst`/
  `FindNext`/`DosFind`, no `DlgDirList`, no `GetOpenFileName`, no `DragAcceptFiles`, no
  INT 21h `DosFindFirst`/`DosFindNext`, and no wildcard expansion. The icon list is
  **config-driven by name**: an INI path string is read into a buffer, **tokenized on
  `\`**, indexed 0..9 in a scrolling window, and each name is passed to `ExtractIcon`
  to pull icons out of a **DLL**. A port reproduces this with a **config list**, not a
  directory scan. Do not build a filesystem walker.

> **Status:** the original's scoring *formula* is partially recovered (a match-reset
> routine computes score in two game modes and accumulates it into 32-bit globals).
> What is **not** established is how a capture feeds that score. "Capture N icons
> worth X points" is an assumption, not a finding.

## 7. Window messages (from WndProc FUN_1008_0224)

> **Reliability note.** Handler addresses in this table that appear as
> `func_0xNNNNNNNN` are *raw OptLoader chain-link words*, not real call targets —
> the decompiler renders unresolved far calls from whatever value sits in the
> instruction. Do not use them for porting. Use the verified map instead:
> `/home/cabal/Git/repos/innerspace-rea/api-calls.txt` (9,910 annotated call
> sites, address order) and `annotated-asm/segNN.asm`.

| Message | Value | Handler | Original function |
|---|---|---|---|
| WM_DESTROY | 0x0002 | Cleanup, PostQuitMessage | inline in WndProc |
| WM_CLOSE | 0x0010 | Confirm quit | inline in WndProc |
| WM_PAINT | 0x000F | Render frame | `FUN_1008_0970` |
| WM_TIMER | 0x0113 | **Game tick** | `FUN_1008_11f8` |
| WM_COMMAND | 0x0111 | Menu commands | `FUN_1008_13b2` |
| WM_KEYDOWN | 0x0100 | Ship controls | input switch (sel 1010) |
| WM_KEYUP | 0x0101 | Ship controls | input switch (sel 1010) |
| WM_MOUSEMOVE | 0x0200 | Aim/cursor | inline in WndProc |
| WM_LBUTTONDOWN | 0x0201 | Fire weapon | inline in WndProc |
| WM_LBUTTONUP | 0x0202 | — | inline in WndProc |
| WM_RBUTTONDOWN | 0x0204 | Special/alt-fire | `FUN_1008_134c` |
| WM_INITMENU | 0x0116 | Menu setup | **unknown** — `func_0x00327798` is a link-word artifact |
| WM_ERASEBKGND | 0x0014 | Suppress (we paint everything) | inline |
| WM_SIZE | 0x0005 | Viewport resize | inline |
| WM_ACTIVATE | 0x0006 | Pause on focus loss | inline |
| Custom 0x0042 | — | Sound event | **unknown** — `func_0x15037f18` is a link-word artifact |
| Custom 0x0032 | — | System event | inline |

## 8. Audio system

| Layer | API | Purpose |
|---|---|---|
| MIDI music | `midiOutOpen` / `midiOutShortMsg` / `midiOutLongMsg` | Background music (MAIN.MID, ANTHEM.MID, etc.) |
| WAV SFX | `waveOutOpen` / `waveOutWrite` | Sound effects (LASER, KABOOM, MISSILE, etc.) |
| Mixer | Software mixing in port | Replaces WAVEMIX.DLL (4+ channel mixing) |

## 9. Phase breakdown

### Phase 1: Project scaffold + Asset parsers (1 week)
- CMake project, MinGW-w64 + MSVC targets
- `bod_parser.cpp` — parse MASTER.BOD into `ShipDef[52]`
- `shp_parser.cpp` — parse MASTER.SHP sprite data
- `dib_loader.cpp` — load DIB → HBITMAP
- `config.cpp` — parse INSPACE.INI
- Unit tests for parsers
- **Milestone:** Console app that loads all assets and prints ship roster

### Phase 2: Overlay window + Renderer (1-2 weeks)
- `overlay.cpp` — transparent layered full-screen window
- `renderer.cpp` — GDI sprite blit with `AlphaBlend`
- Load first ship sprite, render it on the overlay
- Viewport pan/zoom
- **Milestone:** Transparent overlay window showing a ship sprite that moves with arrow keys

### Phase 3: Core game loop (2-3 weeks)
- `game.cpp` — game state, tick loop
- `ship.cpp` — ship movement (speed, turn rate, momentum)
- Ship spawning, team assignment
- Basic AI (seek nearest icon node, chase/flee)
- **Milestone:** Ships fly around, chase icon nodes, game is playable skeleton

### Phase 4: Icon capture (2-3 weeks) ⭐ signature feature

> **SOURCE MODEL (verified).** There is **no filesystem browser to port**. The original
> supplies icon names from configuration — an INI path string, tokenized on `\`, over an
> indexed 0..9 window — and extracts each icon from a **DLL** via `ExtractIcon`. Nothing
> in the binary enumerates a directory (see the note above and RE `BREAKTHROUGH.md`).
> Implement the source as a **config list + `ExtractIcon`**, not a filesystem walk.
- `icon_scanner.cpp` — async filesystem walk
- Icon extraction thread pool + LRU cache
- `icon_node.cpp` — icon node entity, capture mechanics
- Spatial grid for 50k+ entities
- **Milestone:** Ships fly over real desktop icons and are **observed** interacting
  with them.

> **UNBLOCKED (2026-10-16).** The capture mechanic **is now pinned** — see
> `BREAKTHROUGH.md` §10a and port `README.md` §How it works 5. Implement it as the
> per-icon state machine described there (`+0x51` owned, `+0x22`/`+0x24` owner
> player/slot, `+0x139` ±2 progress driven by `ship[+0x52]`, completion at `>= 5` via
> `seg9:0x88ba`). The reward arithmetic sub-probe (`<seg>:0x0d6c`) is the one open item.
>
> **Historical blocking risk (superseded).** Previously: the capture mechanic was **NOT
> pinned** in the original. Known:
> the accumulator at `1018:0000` (`ship[i].+0x28` mod-16 wrap/clamp driven by
> `entity[i].+0x02`), the entity reset/despawn path, and the parallel-array
> structure. **Not known: where ownership or score is transferred.** Do not implement
> capture from the assumed model — confirm it in the RE first (`PLAN.md` item 2 in the
> RE project). This is the largest single unknown in the port.

### Phase 5: Combat + 8 teams (2-3 weeks)
- Projectiles, weapons, damage
- 8 teams (AVENGERS, PREDATORS, FUZZY ONES, PIRATES, KNIGHTS, SPEED DEMONS, RENEGADES, ENFORCERS)
- Team AI behaviors (aggressive, defensive, etc.)
- Ship death/respawn
- **Milestone:** Full combat with 8 AI teams

### Phase 6: Audio (1 week)
- `audio.cpp` — MIDI playback, WAV SFX, software mixer
- **Milestone:** Music plays, SFX fire on events

### Phase 7: HUD + UI (1-2 weeks)
- Radar, score, fuel, shields, team indicator
- Menu system (New Game, Ship Select, Options, Quit)
- Ship Factory (custom ship editor — reads/writes BOD format)
- **Milestone:** Full UI

### Phase 8: Polish + Wine validation (2-4 weeks)
- Wine compatibility testing (WoW64 build)
- Performance profiling (50k+ entities)
- Save/load
- README, installer
- **Milestone:** Playable on Windows + Wine, original assets, original feel

**Total: ~4-7 months solo part-time**

## 10. Wine compatibility notes

- Target: Win32 API (not Win16) — runs natively on Windows, on Linux via Wine
- GDI `AlphaBlend` — fully supported on Wine
- `midiOut*` / `waveOut*` — supported on Wine (via ALSA/whatever backend)
- `SHGetFileInfo` / `IShellLink` COM — supported on Wine
- `WS_EX_LAYERED` — supported on Wine (since Wine 1.x)
- Build with MinGW-w64 for Wine testing, MSVC for shipping
- Use the WoW64 portable Wine build (see `reverse-engineering-toolchain` skill, `references/wine-gui-validation-rhel.md`)

## 11. What makes this "clean-room"

- We do NOT copy code from the decompilation. We use it as a **specification**.
- Data structures are re-derived from observed behavior, not copied.
- Asset parsers (BOD, SHP, DIB) read the **original data files** — this is data, not code.
- The original game's assets (sprites, MIDI, WAV, ship stats) are abandonware and loaded as-is.
- All game logic, rendering, audio, and UI code is written from scratch.

## 12. Success criteria

- [ ] Runs on Windows 10/11
- [ ] Runs on Linux via Wine
- [ ] Loads original game assets (MASTER.BOD, MASTER.SHP, DIB sprites, MIDI, WAV)
- [ ] Ships fly over the real desktop and capture real filesystem icons
- [ ] 8 teams (distinct AI behaviour is **assumed**, not yet verified)
- [ ] All 52 ship types with original stats
- [ ] Music + SFX play
- [ ] Ship Factory editor works
- [ ] Score-based win condition (not full-capture)
- [ ] Feels like the original

## 13. References

- RE project: `/home/cabal/Git/repos/innerspace-rea/`
  - `BREAKTHROUGH.md` — **the authoritative RE reference** (current-state, not a log).
    Read §0 (authority ranking) and §8 (retracted models) before proposing any new
    extraction method — several obvious approaches are already proven wrong.
  - `PLAN.md` — RE continuation plan
  - `OPTLOADER-ANALYSIS.md` — OptLoader format docs
  - **AUTHORITATIVE for call structure and API identity:**
    - `api-calls.txt` — 9,910 verified Windows API call sites, address order,
      each mapped to `MODULE!Function` with its import ordinal
    - `api-sites.json` / `api-sites.tsv` — 9,977 fixup sites (call sites + data slots)
    - `annotated-asm/segNN.asm` — annotated disassembly of all 17 code segments
    - `api-imports.json` — the 186 distinct imported APIs with site counts
  - **LOW RELIABILITY — do not use for control flow or function boundaries:**
    - `ghidra-out/decompiled-unpacked.c` — useful for *local* expression-level
      detail, but Ghidra disassembled the DGROUP data segment as code (~1,700
      bogus tiny functions), 2,019 of 3,969 recorded function bodies are under
      24 bytes, and real bodies are truncated (WndProc is recorded as 17 bytes
      with call_count 0). Far-call targets in it are link-word noise.
    - `functions-unpacked.tsv` — same defect
  - `relocs.json` (930 decoded relocation records), `wine-specs/` (ordinal→name ground truth)
  - `extracted/shipdisk/` — original game files
- Wine validation: `~/.hermes/skills/software-development/reverse-engineering-toolchain/references/wine-gui-validation-rhel.md`
- Original game: Operation: Inner Space v1.29.08, © 1994-2000 Software Dynamics, Inc.
