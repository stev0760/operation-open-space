# newgame-0002: Enter on D:\GAMES, the wave 1 briefing and the first seconds of wave 1

- **Date:** 2026-10-24. **Host:** Wine 11.0 old WoW64 (`/opt/wine-11.0-oldwow`, the oracle default, see [runtime-0001](runtime-0001.md)), Xvfb 1024×768×24, prefix `innerspace1` (created fresh by this run), display `:111`.
- **Setup:** `install_layout.py --with-ini`, fixture `basic` as `D:`, no `Z:`, no registry DLL overrides (only `WINEDLLOVERRIDES` from `oracle/env.sh`).
- **Reproduce:** `ORACLE_ID=1 oracle/scenarios/newgame.sh work/oracle/newgame-0002`, then `re/.venv/bin/python -I oracle/relay_digest.py work/oracle/newgame-0002/trace.log --from 'WRITEPRIVATEPROFILESTRING\(.*"NavLeft"'`.
- **Bulk output (gitignored):** `work/oracle/newgame-0002/`: 31 screenshots, `trace.log` (78 MB), `digest.md` (whole run), `digest-enter.md` (from Enter on), `steps.txt`, `INSPACE.INI`. Further runs of the same scenario: `work/oracle/newgame-0002-intro/` (`SKIP_INTRO=0 TRACE=0`), `work/oracle/depth8-oldwow/` (`ORACLE_ID=2 DEPTH=8`), and `work/oracle/patched-rebuild/` (`ORACLE_ID=2 SKIP_INTRO=0` on new WoW64 with the krnl386 module built by `oracle/wine/build_patched.sh`).
- **Times** are seconds from the first trace line. `steps.txt` puts the Enter click at 70.9 s and ESC on the briefing at 79.1 s after launch.

## Up to Enter
- Title screen, Restore Game, New Game, intro skip and Navigation window make the same calls with the same selectors, window geometry and texts as [newgame-0001](newgame-0001.md). [ORACLE newgame-0002]
- ISPACE.DLL keeps its own DGROUP 0427 throughout. The GLOBALFREE returns to ISPACE.DLL seg6:0x638c and seg7:0x4ce8, where stock Wine 11.0 switches DS ([fault-0001](fault-0001.md)), carry `ds=0427`. No relay line returns into ISPACE code (03c7–041f) with `ds=03a7`, and the trace has no `GLOBALLOCK(0000)` and no `CREATECOMPATIBLEBITMAP(0000,…)`. [ORACLE newgame-0002]
- Left to run (`newgame-0002-intro`, untraced), the desk intro types through "PREPARE TO LAUNCH...". The game then opens the Navigation window by itself, between 19.5 and 23.6 s after New Game OK (screenshots `intro-04`, `intro-05`). That run also reaches the briefing and wave 1. [ORACLE newgame-0002]
- The line after "PREPARE TO LAUNCH..." varies between runs: "ENJOY YOUR FOOD, YOUR NAME!" in `newgame-0002-intro`, "GOOD LUCK, YOUR NAME!" in `patched-rebuild` and in the runs of fault-0001 and runtime-0001. What selects it is [OPEN]. [ORACLE newgame-0002]
- `patched-rebuild` gives the same result as the default runtime. It has no fault, and ISPACE.DLL code always runs on `ds=0427`. The Navigation window opens between 19.5 and 23.6 s after OK, and ESC on the briefing starts wave 1. [ORACLE newgame-0002]

## Navigation window walk
Three search patterns differ from [newgame-0001](newgame-0001.md). With the krnl386 patch alone (`patched-rebuild` and the patched runs of fault-0001), new WoW64 issues the same patterns as old WoW64. The newgame-0001 patterns are therefore part of the stock Wine 11.0 fault. [ORACLE newgame-0002]
- **Opening the window:** after the label probe, `D:\*.exe` attr 0 and one call of `D:\*.*` attr 0x15, the game enumerates `D:\*.*` attr 0x15 again and probes `<entry>\*.exe` after each entry. `D:\DEEP\*.exe` and `D:\EMPTY\*.exe` find nothing, `D:\GAMES\*.exe` finds ALPHA.EXE, and the enumeration stops there. Stock Wine 11.0 issues `D:\` attr 0x15 (which fails) and then only `D:\DEEP\*.exe`. [ORACLE newgame-0002]
- **Root enumeration:** for each root entry the game issues `D:\<NAME>\*.*` attr 0x15 and reads entries until it meets a subdirectory or the end. DEEP stops at A, and EMPTY, GAMES and TOOLS run to the end. Stock Wine 11.0 issues `D:\<NAME>*.*`, which matches the directory itself. [ORACLE newgame-0002]
- **Expanding `deep`, then `a`:** each child probe is two `D:\DEEP\A\*.*` attr 0x15 searches. Stock Wine 11.0 issues one `D:\DEEP\A\*.*` and one `D:\DEEP\A\`, which fails. [ORACLE newgame-0002]
- The tree draws the "+" folder overlay on `d:\` and `deep`, the two entries with subdirectories. `patched-rebuild` draws the same. Stock Wine 11.0 draws it on `deep`, `empty`, `games` and `tools` (screenshots `12-navigation` of both digests, and runtime-0001). [ORACLE newgame-0002]

## Enter (70.8 s)
- The game writes `NavLeft=30`, `NavRight=560`, `NavTop=30` and `NavBottom=350`, destroys the `IS_NAV` window (INSPACE.EXE seg6:0x2ff1) and probes the `D:` volume label (`FINDFIRST "D:\*.*"` attr 0x08). [ORACLE newgame-0002]
- It loads graphics: [ORACLE newgame-0002]
  - ISPACE.DLL `#5500` 100.
  - IGRAPHIC.DLL `#4000` 280, 170, 175, 200, 50, 210, 820, 72, 70, 180, 690, 691 and 692 (`FINDRESOURCE` returning to ISPACE.DLL seg5:0x1c5e).
  - IGRAPHIC.DLL bitmaps `ROCKS`, `BW_BLOCKS`, `BW_MBLOCKS` and `OUT_SIGN` (`LOADBITMAP` returning to ISPACE.DLL seg5:0x0646).
- It loads the ENG_LANG.DLL strings 311 "GOOD", 639 "Morning", 645 "Welcome to Inner Space", 936 "Entering Drive ", 937 ":" and 899 "LOADING GRAPHICS". [ORACLE newgame-0002] [BYTES ENG_LANG.DLL STRING]
- The trace then has no line for 1.9 s (70.9–72.8 s). The relay filter in `oracle/trace.sh` hides the idle-loop calls. [ORACLE newgame-0002]

## Wave briefing (72.8 s)
- **Window set-up:** `GETVERSION`, two 8×8 monochrome pattern brushes, `LOADMENU` of ENG_LANG.DLL menu 101, `SHOWWINDOW(main, SW_SHOW)` and `SETCURSORPOS(512, 768)`. [ORACLE newgame-0002]
- **Files:** `OPENFILE` of `C:\INSPACE\SHIPS\MASTER.BOD` (one `_LREAD` of 0x8f0 bytes) and `MASTER.SHP` (0x1df6 bytes), with the directory read from `ShipPath`. INI reads: `ThePlayer`, `Complex` (default 0), `ShipPath` and `MainShip` (default `EAGLE`). [ORACLE newgame-0002]
- **Icons of the target directory:** [ORACLE newgame-0002]
  - `FINDFIRST "D:\GAMES\*.exe"` attr 0x01 delivers ALPHA.EXE, BRAVO.EXE and CHARLIE.EXE.
  - Each exe gets `SHELL.EXTRACTICON(…, 0xffff)` (0 under Wine, see newgame-0001, Wine divergences) and `EXTRACTICON(…, 0)`, which returns an icon.
  - `FINDFIRST "D:\GAMES\*.dll"` attr 0x01 finds nothing.
- **Graphics:** IGRAPHIC.DLL `#4000` 250, 240, 45, 260, 60 and 370, and bitmaps `METAL`, `DIGITAL`, `RADSHIP` and `SYSTEMS`. [ORACLE newgame-0002]
- **Text:** [ORACLE newgame-0002]
  - Strings 2537 "WAVE %d MISSION BRIEFING", 2268 "PRESS 'ESC' TO START", 2316 "%#" and 12515 "ICONS". [BYTES ENG_LANG.DLL STRING]
  - ENG_LANG.DLL `#5000` 101 at 72.9 s, which is the text the screen types: "ENTER INNER SPACE, CAPTURE OR DESTROY ALL ICONS BEFORE LEAVING THROUGH GATE. AVOID ROCKS AND CAPTURE GAS CANS IF FUEL RUNS LOW.....". [BYTES ENG_LANG.DLL #5000 101]
  - ENG_LANG.DLL `#5000` 300 at 78.9 s, "IF YOU CAPTURE AN ICON, YOU GET ITS RESOURCES, …". [BYTES ENG_LANG.DLL #5000 300]
- **Screen** (`enter-01`, `enter-02`): a framed box "WAVE 1 MISSION BRIEFING" with "PRESS 'ESC' TO START" and a status bar "D:\GAMES\", "3 ICONS". Inside are three "D:>" icon boxes labelled ALPHA.EXE, BRAVO.EXE and CHARLIE.EXE. Below them the briefing text types out character by character. [ORACLE newgame-0002]

## Wave 1 (ESC, 79.0 s)
- **Windows** (children of the main window 006c, in screen coordinates because the main window sits at 0,0): [ORACLE newgame-0002]

  | Class | Position | Size | Style | Id | Created at |
  |---|---|---|---|---|---|
  | `scrollbar` | 562,683 | 14×85 | 0x50000001 | 0xc8 | INSPACE.EXE seg11:0x0cde |
  | `IS_INFO` | 147,688 | 273×76 | 0x50000000 | 0xd2 | INSPACE.EXE seg11:0x0d53 |
  | `IS_INFO` | 673,688 | 343×76 | 0x50000000 | 0xdc | INSPACE.EXE seg11:0x0da4 |
  | `IS_ICON` | 976,0 | 50×150 | 0x10cc0000 | 0 | INSPACE.EXE seg7:0x649b |

  - The scrollbar gets `SETSCROLLRANGE(SB_CTL, 100, 500)` and `SETSCROLLPOS(480)`.
  - Within 0.1 s the game destroys the scrollbar and both `IS_INFO` windows and creates them again with the same geometry.
- **Icon column menu:** `GETSYSTEMMENU(IS_ICON)`, then `APPENDMENU` of a separator and of item 0xf0, `MODIFYMENU` of item 0xf0 and `DRAWMENUBAR`. [ORACLE newgame-0002] The item's text pointer is INSPACE.EXE `DGROUP:0xb80`, which holds "1/2x Scale" in the file. [BYTES DGROUP:0xb80]
- **Strings whose text the HUD shows:** 152 "Icon ^N", 930 "CURRENT ", 931 "TARGET  (", 933 "MANUAL", 934 "WEAPON SYSTEM" and 194 "RADAR-GUIDED MISSILES". [ORACLE newgame-0002] [BYTES ENG_LANG.DLL STRING]
- **Other strings loaded:** 2063 "SCORE", 362 "Gate Block^N", 2726 "A ", 1082 "You Are ", 470 "Just Wandering.", 153 "Rock^N" and 474 "Approaching %s". [ORACLE newgame-0002] [BYTES ENG_LANG.DLL STRING]
- **Screen** (`wave-01` … `wave-04`, 2 s apart): [ORACLE newgame-0002]
  - The ship sits at the centre, with rocks and, in `wave-01`, a gate of two red posts.
  - The icon column at the top right holds three "D:>" boxes.
  - The HUD reads "CURRENT TARGET (MANUAL)", "BRAVO.EXE", "Icon 1", "WEAPON SYSTEM" and "20 - RADAR-GUIDED MISSILES".
  - The HUD clock reads 0 01, 0 03, 0 05 and 0 07 in the four shots, so it keeps real time.
- **Frame loop:** in the 8.2 s of wave the game makes 94 600 iterations of `SELECTCLIPRGN` (INSPACE.EXE seg7:0x084f), `GETASYNCKEYSTATE(VK_SHIFT)` (seg11:0x4de3) and `GETASYNCKEYSTATE(VK_SPACE)` (seg11:0x4def), about 11 500 per second. The game polls the arrow keys less often: VK_UP and VK_DOWN 157 times (seg11:0x5fb9, 0x5f81), VK_LEFT and VK_RIGHT 82 times. `PTINRECT` runs 26 595 times. The trace grows by 6.7 MB/s during the wave. [ORACLE newgame-0002]
- **Timers:** no `SETTIMER`, `KILLTIMER` or `TIMESETEVENT` call occurs from Enter to the end of the run. The only window timer is `SetTimer(main, 1, 1000 ms)` from start-up (INSPACE.EXE seg6:0x0a14). [ORACLE newgame-0002]
- No MMSYSTEM call is relayed after Enter. `SoundSystem` is 0, and the filter hides `TIMEGETTIME`. [ORACLE newgame-0002]

## Depth 8
- At 1024×768×8 (`depth8-oldwow`) the scenario also runs to wave 1 without a fault. The device answers and the system-colour damage match [depth8-0001](depth8-0001.md). [ORACLE newgame-0002]

## Not covered
- Input during the wave (ship control, capture, firing) and the end of a wave [OPEN].
- The meaning of `Complex` and of the `#5500`, `#4000` and `#6600` records [OPEN].
