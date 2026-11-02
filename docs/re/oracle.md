# Oracle

The oracle is the original v1.29.08 demo running under Wine. Scripts in `oracle/` drive it, and screenshots, a filtered relay trace and a read-only memory probe observe it. Each run that answers a question gets a digest in `oracle/digests/`. This page answers the standing questions in short and links to the digest section with the detail. [DESIGN]

## Digests
| Id | Runtime | Covers |
|---|---|---|
| [smoke-0001](../../oracle/digests/smoke-0001.md) | stock Wine 11.0, new WoW64 | title screen, INI regeneration, Restore Game |
| [newgame-0001](../../oracle/digests/newgame-0001.md) | stock Wine 11.0, new WoW64 (faults) | New Game, Navigation window, filesystem walk, start-up API facts |
| [fault-0001](../../oracle/digests/fault-0001.md) | stock and krnl386-patched new WoW64 | the DS fault, its cause and the patch |
| [depth8-0001](../../oracle/digests/depth8-0001.md) | stock Wine 11.0, new WoW64, depth 8 | device answers, title and dialog colours |
| [runtime-0001](../../oracle/digests/runtime-0001.md) | old WoW64, otvdm 0.9.0 | runtime comparison, otvdm differences |
| [newgame-0002](../../oracle/digests/newgame-0002.md) | old WoW64 | Navigation walk, Enter, briefing, first seconds of wave 1 |
| [probe-0001](../../oracle/digests/probe-0001.md) | old WoW64 | DGROUP memory at the title, in Navigation and in wave 1 |
| [wave-0001](../../oracle/digests/wave-0001.md) | old WoW64 + ExtractIcon patch | the patch, relay profiles, play inside wave 1 |
| [depth8-0002](../../oracle/digests/depth8-0002.md) | old WoW64 + ExtractIcon patch, depth 8 | palette calls and colours from title to wave 1 |
| [dialogs-0001](../../oracle/digests/dialogs-0001.md) | old WoW64 + ExtractIcon patch; port under new WoW64 | the port's 22 converted dialogs against the 13 the original opened: equal client sizes |
| [core-0001](../../oracle/digests/core-0001.md) | old WoW64 + ExtractIcon patch, `SoundSystem=1` | rock hits, lamp values and auto-repair from a memory watch, lamp messages, wing loss, the player's death to GAME OVER, three exits through the gate, the debriefing and Navigation |
| [core-0002](../../oracle/digests/core-0002.md) | old WoW64 + ExtractIcon patch, `SoundSystem=1` | icon captures with their values, words, sound, dissolve and message timing; the gun's rate, slot words, trail colours and ricochets; kills by ramming heated rocks; an icon cracked by fire and shattered by a rock; missile launches, targets, lifetimes and the weapon display's count; HUD crops of the score panel and weapon display |
| [win98-0001](../../oracle/digests/win98-0001.md) | Windows 98, CRT (a photograph) | the full status display during play, the icon column, the Ship Watch window, the weapon picture |

## Running the oracle
Usage lives in each script's header: `oracle/env.sh`, `oracle/trace.sh`, `oracle/scenarios/common.sh`, `newgame.sh`, `wave.sh`, `oracle/relay_digest.py`, `oracle/probe/isprobe.c` and `oracle/probe/probe_run.sh`. [DESIGN]
- `ORACLE_WINE` picks the runtime. The default, `~/opt/wine-11.0-oldwow-patched`, is Wine 11.0 in old-WoW64 mode, which runs the original into wave 1 without a fault [ORACLE runtime-0001] [ORACLE newgame-0002], plus `oracle/wine/shell16-extracticon-count.patch` [ORACLE wave-0001].
  - `~/opt/wine-11.0-oldwow` (stock old WoW64) matches the digests made on it. `~/opt/wine-11.0-patched` is new WoW64 with the krnl386 patch, and stock `~/opt/wine-11.0` serves only to reproduce the DS fault. [ORACLE fault-0001]
  - otvdm 0.9.0 inside Wine cross-checks Wine's 16-bit layer. Its set-up, its slow traced runs and the need to keep `+file` off are in [runtime-0001, otvdm](../../oracle/digests/runtime-0001.md#otvdm-090-inside-wine-110-new-wow64). [ORACLE runtime-0001]
  - Wine 11.20 does not run on this host, because its `wineboot` segfaults. [ORACLE smoke-0001]
  - The script `oracle/wine/install_kron4ek.sh` installs a Kron4ek build. The script `oracle/wine/build_patched.sh` builds a copy that differs from the stock install in one module. [ORACLE fault-0001] [ORACLE wave-0001]
- `ORACLE_ID=N` selects the prefix `~/.local/share/wineprefixes/innerspaceN` (`innerspace` for 0) and the display `:110+N`. Parallel runs use distinct ids, and `oracle_game_stop` runs `wineserver -k` for its own prefix only. [DESIGN]
- `oracle/install_layout.py` lays out the unregistered shipdisk without the installer. The original `INSTALL.EXE` crashes under new-WoW64 Wine and completes under the old-WoW64 runtime, where `oracle/scenarios/install.sh` drives it and produces the registered game [ORACLE install-0001]. `--with-ini` pre-writes the keys that the game otherwise regenerates behind a MessageBox. [ORACLE smoke-0001]
- The shared `common.sh` gives each run fresh game files and INI, the fixture tree `basic` as `D:`, no `Z:` and the relay filter. The fixture generator is `oracle/fixtures/make_fixtures.py`. [DESIGN]
- The `newgame.sh` scenario drives title, New Game, Navigation, briefing and the first seconds of wave 1. The `wave.sh` scenario plays stages inside wave 1 with `autopilot.py`. Click positions assume a 1024×768 display. [DESIGN]
- The tracer `trace.sh` relays the 16-bit system modules with `+relay,+int21,+file,+timestamp`. The default exclude list drops the idle loop and the wave frame loop. The profile `ORACLE_RELAY_PROFILE=wave` also drops the status display's drawing calls. It keeps the wave at 0.05–0.2 MB/s instead of 5–8 MB/s ([wave-0001, Relay filter](../../oracle/digests/wave-0001.md#relay-filter)). [ORACLE wave-0001]
- `relay_digest.py` turns a trace into a markdown digest: selector map, call counts, file and INI operations, timers, windows, dialogs and faults. Wine's `load_list` line shows only some 300 characters of the exclude list, so the scenarios log the whole list in `steps.txt`. [ORACLE wave-0001]
- `isprobe` reads both DGROUPs of the running game and never writes. A signature scan and the LDT selectors find the same bases. Start it by a path relative to the repo: with `Z:` removed, Wine segfaults on an absolute Unix path to the exe. [ORACLE probe-0001]

## Filesystem walk
The patterns below come from runtimes where ISPACE.DLL keeps its own DS. Stock new WoW64 issues three malformed patterns ([newgame-0002, Navigation window walk](../../oracle/digests/newgame-0002.md#navigation-window-walk)). [ORACLE newgame-0002]
- Drive list: the New Game dialog probes C: to Z: (int21 0x0E/0x19), never A: or B:. Each drive it accepts gets a volume-label search `X:\*.*` attr 0x08 and a combo entry `<L>: <label>`, or `Unknown_HD` without a label. [ORACLE newgame-0001] The scenarios remove `Z:`, so the host root stays out of the list. [DESIGN]
- Root: the Navigation tree starts at the chosen drive (`d:\`) and mixes its real directories with five built-in levels, sorted and in lower case ([newgame-0001, Navigation window](../../oracle/digests/newgame-0001.md#navigation-window)). [ORACLE newgame-0001]
- Filters: directories by `*.*` attr 0x15, apps by `*.exe` and then `*.dll`. The Navigation window and the briefing count each exe's icons with `ExtractIcon(…, 0xFFFF)`, and the built-in levels query the pseudo-paths `S\`, `D\` and `R\`. Saved games are `C:\INSPACE\GAMES\*.SAV`. [ORACLE newgame-0001] [ORACLE newgame-0002]
- Laziness: opening the window probes root entries up to the first one holding an exe. The game reads each root entry only up to its first subdirectory, which decides the "+" overlay. Expanding a directory probes one level ahead. [ORACLE newgame-0002] Selecting a directory lists its files and extracts the first exe's icon only. [ORACLE newgame-0001]
- Order: Wine hands entries over in name order. The root search returns no `.` and `..`, and subdirectory searches return them first. Both listboxes sort, so the tree order does not depend on it. [ORACLE newgame-0001]
- Caps: the `basic` tree reaches no depth or count limit. [ORACLE newgame-0001]
- Enter runs a label probe, and the briefing lists `D:\GAMES\*.exe` and `*.dll` with attr 0x01 ([newgame-0002, Wave briefing](../../oracle/digests/newgame-0002.md#wave-briefing-728-s)). [ORACLE newgame-0002]
- Writes: a run from title to exit writes only `C:\WINDOWS\INSPACE.INI`. [ORACLE wave-0001]

## Window and client geometry
- The main window is class `Inner Space`, created as "Limited Inner Space", `WS_POPUP` at 0,0 with the size from `GetSystemMetrics` (1024×768 here). [ORACLE newgame-0001] Its title reads "Inner Space - PAUSED.." while a dialog is up. [ORACLE wave-0001]
- Navigation is `IS_NAV` at 30,30, size 560×350 (client 552×316). Enter stores the rectangle as `NavLeft` … `NavBottom`. [ORACLE newgame-0001]
- The positions and sizes of Restore Game and New Game are in [newgame-0001](../../oracle/digests/newgame-0001.md#restore-game-dialog). Those of the ambulance, Sell.., save, exit and demo dialogs and the `IS_HELP` window are in [wave-0001](../../oracle/digests/wave-0001.md#ambulance-h). Without a window manager, the screenshots show no caption or frame. [ORACLE newgame-0001] [ORACLE wave-0001]
- The wave HUD is a scrollbar, two `IS_INFO` panels and the `IS_ICON` column ([newgame-0002, Wave 1](../../oracle/digests/newgame-0002.md#wave-1-esc-790-s)). [ORACLE newgame-0002] The game recreates them with the same geometry after every dialog. The ship stays at the screen centre while the view scrolls. [ORACLE wave-0001]
- Client rect: every digest uses a 1024×768 screen. The bytes make the play area, the HUD layout and the visible world extent follow the client rectangle, with no letterbox or stretch ([shell.md](shell.md#window-geometry)). [BYTES seg2:0xb15 seg2:0x1b07 seg8:0x14 seg11:0xcbe seg2:0x9b0] Some INI defaults follow the screen too: `IconLeft` defaults to the screen width − 48, 976 at 1024 pixels ([formats.md](formats.md#inspaceini-inner-space)). [ORACLE newgame-0001] Whether 3200 × 3200 is the world size is [OPEN] ([ispace.md](ispace.md#ordinal-notes)).

## Palette and colour depth
Depth 8 (PseudoColor) against depth 24: [ORACLE depth8-0001] [ORACLE depth8-0002]
- `GetDeviceCaps` gives BITSPIXEL 8 or 32 and SIZEPALETTE 256 or 0, and NUMRESERVED 20 at both ([depth8-0001, Device answers](../../oracle/digests/depth8-0001.md#device-answers)).
- At depth 8 the title drops "For best results, change Windows to 256 color mode".
- Both depths make the same `CreatePalette` and `AnimatePalette(hpal, 10, 236, …)` calls. Depth 8 adds 54 `GetDIBits`/`RealizePalette` pairs (ISPACE.DLL seg5:0x229a, seg5:0x227c) and raises the `RealizePalette` count at seg5:0x20f4 from 27 to 81 ([depth8-0002, Palette calls](../../oracle/digests/depth8-0002.md#palette-calls-whole-run-depth-8-against-depth-24)).
- The call `GetSystemPaletteEntries` asks for entries 0–9 and 246–255 at depth 8 and gets 10 each time. At depth 24 it asks for 0–9 and 6–15 and gets 0. The digest depth8-0002 derives the range from ISPACE.DLL seg5:0x3649, and the bytes confirm it: ordinal 4 command 0 copies system entries 0 … NUMRESERVED/2 − 1 and SIZEPALETTE − NUMRESERVED/2 … into palette entries 0–9 and 246–255 at both depths ([shell.md](shell.md#palette)). [BYTES ISPACE.DLL seg5:0x3649 seg5:0x3651 seg5:0x36dc seg5:0x36e6 seg5:0x3702]
- At depth 8 the dialogs get wrong system colours, and the title, briefing bars, ship sprite, radar dots and dim HUD text lose theirs. The fixture icons keep their colours. Whether Windows at 256 colours does the same is [OPEN].
- Both depths reach wave 1 on the default runtime. [ORACLE depth8-0002]

## Unregistered limits seen at run time
- The title screen shows "You are on day N of your demo trial period." and "Press 'R' to order the full game package". [ORACLE smoke-0001]
- New Game accepts only the TRANSFORMER ship and answers any other ship with a MessageBox about the full package. [ORACLE newgame-0001]
- In Navigation, demon's gate, dodge city (duel), grand prix and sdi speedway (races) refuse entry without the full version. Lazy loop (race) is open. [ORACLE newgame-0001]
- Saving (Alt-S, or Save & Exit) shows "Game saving is a handy feature of the full version of Inner Space." and writes no file. The game still writes `Restore=<name>` to INSPACE.INI. [ORACLE wave-0001]
- The ambulance's left monitor reads "ORDER FULL GAME TO SEE WHAT APPEARS HERE....". [ORACLE wave-0001]
- Exit from the title screen shows the demo dialog `#031f` with Exit and Order... [ORACLE wave-0001]

## Timers and clocks
- Window timers: a start-up `SetTimer(NULL, 1, 1000)` and `KillTimer` (INSPACE.EXE seg6:0x139c, seg6:0x13b6), then `SetTimer(main, 1, 1000 ms)` (INSPACE.EXE seg6:0x0a14), killed at exit (INSPACE.EXE seg6:0x1a3d). That is the only window timer. [ORACLE newgame-0001] [ORACLE wave-0001]
- No `SetTimer`, `KillTimer` or `timeSetEvent` runs from Enter to the exit. [ORACLE newgame-0002] [ORACLE wave-0001] Before its first MessageBox the game calls `timeGetDevCaps` once. [ORACLE smoke-0001]
- Idle loop: `PeekMessage(PM_REMOVE)` (INSPACE.EXE seg2:0x00ff), `GetDC`, `SelectPalette`, `GetCurrentTime` (INSPACE.EXE seg7:0x0024), `timeGetTime` (INSPACE.EXE seg7:0x00bc), `SelectPalette` and `ReleaseDC`, some 65 000 times per second on the title. [ORACLE newgame-0001]
- Wave frame loop: `SelectClipRgn` (INSPACE.EXE seg7:0x084f) and `GetAsyncKeyState` for VK_SHIFT and VK_SPACE, some 11 500 times per second. The game polls the arrow keys less often. [ORACLE newgame-0002]
- The game measures and rewrites `Calibrate` 1.5 s after each launch [ORACLE newgame-0001]. The value depends on host speed and load: the digests record 13 340–14 940 under Wine on this host, 7116 under otvdm and 1294–1324 under traced otvdm. [ORACLE runtime-0001] [ORACLE probe-0001]. What the game derives from it is in [timing.md](timing.md#calibrate).
- The HUD clock keeps real time under Wine [ORACLE newgame-0002] and under untraced otvdm. Under traced otvdm it runs at half speed. [ORACLE runtime-0001]
- INSPACE.EXE DGROUP:0x1e50, DGROUP:0x2214 and DGROUP:0x2218 follow Wine's millisecond tick in every phase. [ORACLE probe-0001]

## Wine divergences
| Divergence | Oracle handling |
|---|---|
| New WoW64 loses ISPACE.DLL's DS after a `GlobalFree` that leaves the freed selector in ES. The game faults in the intro and on Enter, and in 1 of 5 runs exits silently ([fault-0001](../../oracle/digests/fault-0001.md#cause-chain)). [ORACLE fault-0001] | The default runtime is old WoW64. New WoW64 runs only with the krnl386 patch. [ORACLE runtime-0001] [ORACLE fault-0001] |
| `ExtractIcon(…, 0xFFFF)` returns 0, because Wine's `ExtractIcon16` takes a 16-bit index. The game then counts no icons and draws a generic "D:>" box. [ORACLE runtime-0001] [ORACLE wave-0001] | The shell.dll16 patch in the default runtime returns the count, and all 32 calls of `newgame.sh` match otvdm. newgame-0001, newgame-0002, depth8-0001 and probe-0001 come from runtimes without it. [ORACLE wave-0001] |
| `CreateCompatibleBitmap(NULL, …)` returns NULL [ORACLE newgame-0001], where Windows returns a 1-bpp bitmap [REF ReactOS win32ss/gdi/ntgdi/bitmaps.c NtGdiCreateCompatibleBitmap]. | The game passes NULL only while its DS is wrong. On the default runtime it passes a real DC. [ORACLE fault-0001] [ORACLE runtime-0001] |
| Dialogs are narrower than under otvdm: Restore Game is 321×264 at 352,252, against 374×264 at 325,252. [ORACLE runtime-0001] | Digests record Wine's geometry, and the click positions are Wine's. Which runtime matches Windows is [OPEN]. |
| Directory entries arrive in name order. [ORACLE newgame-0001] | The fixtures are fixed trees, and the sorted listboxes hide the order. Whether the order matters elsewhere is [OPEN]. |

## Open questions
- [OPEN] Which runtime's dialog geometry matches Windows, and whether Windows shows the "ANALYSING DRIVE" screen that only otvdm caught [ORACLE runtime-0001].
- [OPEN] Whether Windows at 256 colours shows the depth-8 colour losses [ORACLE depth8-0002].
- [OPEN] Depth and count caps of the walk, which need a larger fixture tree, and whether walk order matters outside the sorted listboxes [ORACLE newgame-0001].
- [OPEN] Why the full status display is missing at wave start and comes and goes around dialogs, and what Windows shows there [ORACLE wave-0001].
- [OPEN] What removed the icon that dissolved without a capture in `wave-0001-run2`, and which input issues the Command-mode capture, since Q calls the ambulance [ORACLE wave-0001].
- [OPEN] The end of a wave, the exit gate, other ships, LASTGAME and runs with sound [ORACLE wave-0001].
- [OPEN] Why traced otvdm halves the HUD clock [ORACLE runtime-0001]. [timing.md](timing.md) explains the frame pacing and the clock samples.
- [OPEN] Which value of DGROUP:0x9214 the newgame-0002 run had [ORACLE newgame-0002]. That word selects the intro's closing line: 0 gives "GOOD LUCK", 3 "TO LUNCH... ENJOY YOUR FOOD", and 1, 2 and 4 "ENJOY YOUR COFFEE", "TEA" or "FOOD" ([shell.md](shell.md#intro-and-title-screen)). [BYTES seg15:0x56ef seg15:0x5700 seg15:0x572b seg15:0x5737 seg15:0x575d]
- [OPEN] Whether the Windows KERNEL `GlobalFree` zeroes a stale ES in the caller's frame [ORACLE fault-0001].
- [OPEN] `+int21` tracing under otvdm [ORACLE runtime-0001].
