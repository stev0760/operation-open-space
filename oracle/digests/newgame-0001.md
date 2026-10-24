# newgame-0001: New Game dialogs, Navigation window and filesystem walk on a fixture drive

- **Date:** 2026-10-24. **Host:** Wine 11.0 (new WoW64), Xvfb 1024×768×24, prefix `innerspace1`, display `:111`.
- **Setup:** `install_layout.py --with-ini`, fixture tree `basic` (`oracle/fixtures/make_fixtures.py`) as `D:`, no `Z:`, oracle shim installed (`oracle/shim/`).
- **Reproduce:** `ORACLE_ID=1 oracle/scenarios/newgame.sh`, then `re/.venv/bin/python -I oracle/relay_digest.py work/oracle/newgame/trace.log`.
- **Bulk output (gitignored):** `work/oracle/newgame/`: 31 screenshots, `trace.log` (20 MB), `digest.md`, `steps.txt`, `INSPACE.INI`.
- **Trace filter:** `oracle/trace.sh` (`WINEDEBUG=+relay,+int21,+file,+timestamp`, RelayInclude = 16-bit modules only).

Fixture `basic` as `D:\`: `GAMES\ALPHA.EXE BRAVO.EXE CHARLIE.EXE`, `TOOLS\DELTA.EXE ECHO.EXE` (ECHO has 3 icons), `EMPTY\`, `DEEP\A\B\FOXTROT.EXE`. Each exe is a 16 KB PE with distinct 16-colour 32×32 icons.

## Selector map
Both `INSPACE.EXE` and `ISPACE.DLL` load through their own OPTLOADER segment 1. The k-th `KERNEL.MYALLOC` of a module allocates segment k+1. For INSPACE.EXE the flags arguments are the NE segment flags | 0x4000, in segment-table order (0x4c58 ×4, 0x5c58 ×3, 0x5c18 ×10). [ORACLE newgame-0001]
- INSPACE.EXE: seg1=02ef, seg2=031f … seg18=039f (step 8), DGROUP seg19 = 03a7.
- ISPACE.DLL: seg1=03c7, seg2=03df, seg3=03e7 … seg10=041f, DGROUP seg11 = 0427.
- The bytes confirm the map: the relay return `03ff:6444` follows the far call at ISPACE.DLL seg6:0x643f, and the fault IP 0x6480 is the `rep movsw` at seg6:0x6480 (`work/unpacked/ispace/seg06.bin`). [ORACLE newgame-0001] [BYTES ISPACE.DLL seg6:0x643f]

## Restore Game dialog
- It is a `DIALOGBOX` from ISPACE.DLL seg10:0x438b. Its template is `#010b`, in instance `02bf` (ENG_LANG.DLL). [ORACLE newgame-0001]
- `MOVEWINDOW` places the window at 352,252, size 321×264. The visible client area is 315×232. Without a window manager the caption and frame are not drawn. [ORACLE newgame-0001]
- The dialog lists saved games with `FINDFIRST "C:\INSPACE\GAMES\*.SAV"` (attr 0x01, RacePath). With none found, the GAME combo holds the single entry `NEW GAME`. PILOT/SHIP/SCORE show `?`, and the description box says "NEW GAME, STARTING AT WAVE 1." [ORACLE newgame-0001]
- OK writes `[Inner Space] Restore= NEW GAME`, with a leading space. [ORACLE newgame-0001]

## New Game dialog
- It is a `DIALOGBOX` from ISPACE.DLL seg10:0x442b, template `#0141` in ENG_LANG.DLL. The window is at 361,273, size 303×222; the visible client is 297×190 at 364,302. [ORACLE newgame-0001]
- Opening it reads `C:\INSPACE\SHIPS\MASTER.BOD` (0x8f0 bytes) and `MASTER.SHP` (0x1df6 bytes). [ORACLE newgame-0001]
- Fields, top to bottom: [ORACLE newgame-0001]
  - **PILOT:** edit, from `ThePlayer` (default `AutoPilot`), here "Your Name".
  - **SHIP:** drop-down list of 61 ships, alphabetical, `AQUATIC BULLET` … `WINTER DUCK`. The game reads `MainShip` (default `EAGLE`) but selects `TRANSFORMER`.
  - **TEAM:** static text that follows the ship (TRANSFORMER → AVENGERS, AQUATIC BULLET → SPEED DEMONS).
  - **PLAY:** `ACTION`, `COMMAND` (from `PlayMode`).
  - **DRIVE:** one entry per accepted drive, `"<L>: " + label`. An unlabelled drive shows `Unknown_HD`. Here the entries are `C: Unknown_HD` and `D: Unknown_HD`; the initial choice comes from `Drive` (default `C`).
  - **ZOOM LEVEL:** `2x`, `1x`, `1/2x` (initial `1x`).
  - OK, Cancel, and a 96×96 `LevClass` ship preview child window.
- **Drive list:** int21 0x0E select + 0x19 get-current for C:, D:, … Z:, then the original drive is restored. A: and B: are never probed. Each accepted drive gets a volume-label search, `FINDFIRST "X:\*.*"` attr 0x08. [ORACLE newgame-0001]
- **Demo limit:** OK with any ship other than TRANSFORMER shows the MessageBox "With Limited Inner Space, you can only use the Transformer ship. If you order the full game package, all ships are available." (caption "Inner Space", flags 0x30). [ORACLE newgame-0001]
- OK writes `MainShip=TRANSFORMER`, `Drive=D`, `Size=1`, `SetSize=1`, `ThePlayer=Your Name` and `PlayMode=0`. [ORACLE newgame-0001]

## Intro
- After OK the game shows a desk scene. Its monitor types: "IT'S SATURDAY MORNING, YOUR NAME! / I HOPE YOU'RE READY TO PLAY.. / YOUR COMPUTER HAS BEEN INVADED! / YOUR MISSION: ENTER INNER SPACE, RESTORE ORDER AS YOU SEE FIT.... / WHEN ALL ICONS HAVE BEEN CAPTURED OR DESTROYED, …". The weekday matches the host date (Saturday 2026-10-24); the name is the pilot's. [ORACLE newgame-0001]
- ESC ends the intro and opens the Navigation window. `QuickIntro=1` does not shorten the desk scene. [ORACLE newgame-0001]
- Left to run, the intro faults in ISPACE.DLL (see Faults). [ORACLE newgame-0001]

## Navigation window
- `CREATEWINDOW` class `IS_NAV`, title "Inner Space Navigation", style 0x10cc0000 (visible, caption, sysmenu, thick frame), at 30,30, size 560×350, parent = main window. The X client area is 552×316 at screen 34,60. Leaving it through Enter writes `NavLeft=30 NavRight=560 NavTop=30 NavBottom=350`. [ORACLE newgame-0001]
- Children, after the layout `MOVEWINDOW`s at INSPACE seg6:0x20b4–0x22a1 (client coordinates): [ORACLE newgame-0001]
  - directory listbox 0,0 160×290, style 0x50b30153 (sort, owner-draw fixed, has-strings);
  - file listbox 164,0 268×290, style 0x50a30b53 (sort, owner-draw fixed, has-strings, multicolumn, extended-select);
  - `&Enter` 447,5 90×26 and `&Help` 447,40 90×26;
  - `RACE` and `DUEL` group boxes, 442,161 102×122;
  - seven drop-down combos, 455,179…254, 80–83 wide;
  - a 64×64 icon preview and a status line along the bottom edge.
- **Directory tree:** the root `d:\` has a green folder. Below it, sorted alphabetically and in lower case, the real directories mix with five built-in levels: `deep`, `demon's gate`, `dodge city - duel`, `empty`, `games`, `grand prix - race`, `lazy loop - race`, `sdi speedway - race`, `tools`. Real directories have yellow folders; locked levels have red ones. Double-click expands a directory (`deep` → `a` → `b`). [ORACLE newgame-0001]
- **Status texts** (exact, including the missing space after the period): [ORACLE newgame-0001]
  - `games`: "There are 3 icons in 3 files.You have not been here before."; `b`: "There is 1 icon in 1 file.You have not been here before."
  - `tools`: "There are 2 icons in 2 files.…" (see Wine notes; ECHO.EXE carries 3 icons).
  - `empty`, `deep`, `a`: "No apps in this directory. Select another directory to enter."
  - `demon's gate` (SDi logo icon): "Gateway to the ultimate challenge! To enter, you must pay for the full game version."
  - `dodge city - duel` (crossed swords; DUEL box: `1 Opponent`, `Normal`, `Similar`): "This is the arena for inner space duels. To enter, you must pay for the full game version."
  - `grand prix - race` and `sdi speedway - race` (chequered flag; RACE box: `2 Ships`, `Normal`, `5 Laps`, `Similar`): "This is an inner space race track. To enter, you must pay for the full game version."
  - `lazy loop - race`: the same RACE box, with "This is an inner space race track. Enter here for a thrill!"
- The file pane lists exe names in lower case. The preview shows the first exe's icon at 2× (ALPHA.EXE's red square). [ORACLE newgame-0001]

## Filesystem walk (int21 FINDFIRST/FINDNEXT via DOS3CALL, entries named by Wine's FindNextFileW)
- **Opening the window** probes the root: label (`D:\*.*` attr 0x08), `D:\*.exe` attr 0, and `D:\*.*` attr 0x15 for one call only (first entry `DEEP`). `D:\` attr 0x15 fails. Then `D:\DEEP\*.exe` runs once. [ORACLE newgame-0001]
- **Root enumeration:** `D:\*.*` attr 0x15 runs on DTA 03A7:69F8 and delivers `DEEP, EMPTY, GAMES, TOOLS`, then no more files. For each name the game issues one `FINDFIRST "D:\<NAME>*.*"` attr 0x15 on a second DTA (03A7:B3F6). That pattern has no backslash, so it matches the directory entry itself. It then builds the list text `<NAME>` plus three flag digits (`DEEP111`) [OPEN meaning]. Subdirectories are not walked yet. [ORACLE newgame-0001]
- **Selecting a directory** enumerates `<dir>\*.exe`, calls `SHELL.EXTRACTICON(hInst, path, 0xffff)` (count query) for every exe, and calls `EXTRACTICON(path, 0)` for the first exe only (the preview). Then it runs `<dir>\*.dll` and `EXTRACTICON("\", 0xffff)`. Built-in levels call `EXTRACTICON("S\" | "D\" | "R\", 0xffff)`. [ORACLE newgame-0001]
- **Expanding a directory** enumerates `<dir>\*.*` attr 0x15, which yields `.`, `..` and the subdirectories (files too where present). For each child it probes `<child>\*.exe`, `<child>\*.*` attr 0x15 and `<child>\` attr 0x15, one level ahead. [ORACLE newgame-0001]
- The root search returns no `.`/`..`; subdirectory searches return them first. [ORACLE newgame-0001]
- **Enter on `games`** runs `D:\GAMES\*.exe` once (ALPHA.EXE) and a label probe, then faults (see Faults). [ORACLE newgame-0001]
- No walk limits show up with this tree; depth or count limits are [OPEN] (needs a larger fixture).

## Other API facts
- **Main window:** class `Inner Space`, title "Limited Inner Space", style WS_POPUP, at 0,0, 1024×768 (`GetSystemMetrics` 0/1, INI `Full` default 1). [ORACLE newgame-0001]
- **Timers:** `SetTimer(NULL,1,1000)` then `KillTimer` at INSPACE seg6:0x139c/0x13b6 (a probe); `SetTimer(main,1,1000 ms)` at seg6:0x0a14. That is the only window timer. [ORACLE newgame-0001]
- **Frame pacing** (relay return addresses): an idle loop runs `PEEKMESSAGE(PM_REMOVE)` @seg2:0x00ff, `GETDC` @seg2:0x1c7e, `SELECTPALETTE`, `GETCURRENTTIME` @seg7:0x0024, `TIMEGETTIME` @seg7:0x00bc, `SELECTPALETTE` and `RELEASEDC`. It runs about 65 000 times per second on the title screen and is excluded from the default filter. [ORACLE newgame-0001]
- `GetDeviceCaps` at 24-bit: HORZRES 1024, VERTRES 768, BITSPIXEL 32, PLANES 1, CAPS1(94) 0, SIZEPALETTE 0, NUMRESERVED 20. [ORACLE newgame-0001]
- `Calibrate` is measured again and rewritten 1.5 s after every launch (13340, 14117, 14572 on three runs). [ORACLE newgame-0001]
- The game reads 93 distinct `[Inner Space]` keys of INSPACE.INI; the defaults are listed in `work/oracle/newgame/digest.md`. Examples: `GameSpeed` 20000, `FrameRateSet` 36, `Detail` 3, `ShowLaunch` 2, `WatchRight` 280, `IconLeft` 976. [ORACLE newgame-0001]

## Faults (block play under Wine)
- **Intro fault** (desk scene, about 14 s after OK; with `SKIP_INTRO=0`, with and without the shim, at depth 24 and depth 8): `Unhandled page fault … at address 00006480`. Traces: `work/oracle/explore/ng-shim1/` and `ng-d2/`. [ORACLE newgame-0001]
  - ISPACE.DLL seg6:0x6432 calls seg5:0x23a4, which returns handle 0. `GLOBALLOCK(0)` returns 0:0, and `rep movsw` at seg6:0x6480 copies a 40-byte header through that pointer. [ORACLE newgame-0001] [BYTES ISPACE.DLL seg6:0x6432]
  - seg5:0x23a4 returns 0 when its 4th argument is none of the three words at ISPACE DGROUP:0x1cd6. Those words are zero in the file, filled at run time [OPEN], and pushed as `SelectPalette` handles elsewhere in seg5. [BYTES ISPACE.DLL seg5:0x23a4]
  - The root cause is [OPEN]; needs RE.
- **Wave-entry fault:** Enter on `games` reaches the same allocator from seg6:0x6f64 and faults at seg6:0x6fbc. [ORACLE newgame-0001]
- **Early exit:** in 1 of 5 full scenario runs the game ended without a fault message during New Game dialog set-up. The 16-bit thread stops inside the `D:` volume-label probe (`FINDFIRST "D:\*.*"` attr 0x08), and the process tears down 1.3 s later. Trace: `work/oracle/explore/newgame-died-at-label/`. Cause [OPEN].

## Wine divergences seen
- `CreateCompatibleBitmap(NULL, cx, cy)` returns NULL in Wine 11.0: win32u passes the NULL DC's PLANES=0 to `NtGdiCreateBitmap`. Windows returns a 1-bpp bitmap [REF ReactOS win32ss/gdi/ntgdi/bitmaps.c NtGdiCreateCompatibleBitmap]. The game makes these calls for 276×276 and 66×66 bitmaps (returning to ISPACE seg7:0x4db8). `oracle/shim` restores the Windows result. The faults above happen either way. [ORACLE newgame-0001]
- `SHELL.ExtractIcon(…, 0xffff)` returns 0 under Wine: `ExtractIcon16` takes a `UINT16` index and passes 65535 to `ExtractIconA` [REF Wine 11.0 dlls/shell.dll16/shell.c]. Win16 defines −1 as "count icons". The icon counts in the status line may therefore differ from Windows; ECHO.EXE's count would be 3 [OPEN].
- Wine delivers directory entries in name order: the fixture's directories were created as GAMES, TOOLS, EMPTY, DEEP and come back as DEEP, EMPTY, GAMES, TOOLS. [ORACLE newgame-0001] Both listboxes sort, so the tree order does not depend on it. [OPEN] whether walk order matters elsewhere, since a FAT volume returns directory-slot order.
