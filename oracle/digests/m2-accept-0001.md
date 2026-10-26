# m2-accept-0001: the M2 acceptance run, port against oracle, both editions

- **Date:** 2026-10-26. **Host:** Arch Linux. The port (`build/mingw-x64/src/inspace.exe`, built this day) runs under the stock `~/opt/wine` (Wine 11.0 new WoW64) on Xvfb 1024×768×24, as in stages 3b and 3d.
- **Question:** M2 is done when "a run from launch to the briefing and back to exit matches the oracle's screens and INI writes" ([PLAN](../../docs/PLAN.md#m2--shell-detailed-plan)). This digest measures that for both editions against [start-0001](start-0001.md) (main, briefesc, briefspace, title), [intro-0001](intro-0001.md) (intro phases), [newgame-0002](newgame-0002.md) (desk scene and launch) and [install-0001](install-0001.md) (the install step).
- **Unregistered port:** prefix `work/wine/ws-s4-u` (`ORACLE_ID=31`, `:141`). `C:\INSPACE` comes from `oracle/install_layout.py --with-ini`, fixture `basic` is `D:` (`hd`), and there is no `Z:`. The start INI is byte-identical to start-0001's `INSPACE.INI.before`.
- **Registered port:** prefix `work/wine/ws-s4-r` (`ORACLE_ID=32`, `:142`), installed fresh by the port, `inspace.exe --install S:\ --to C:\SPACE --ini C:\windows\INSPACE.INI`. `S:` is `original/shipdisk` and is unmapped after the install. The name and organisation are made up. `D:` is a CD device and fixture `basic` is `E:`, as in the registered oracle prefix. The start INI is the one the install step wrote. The prefix `innerspace9` was neither run nor read; its oracle captures are start-0001's. Its registered name appears nowhere in this digest. Composites whose oracle half shows it were deleted after counting.
- **Oracle at depth 8:** prefix `work/wine/ws-s4-o8` (`ORACLE_ID=33`, `:143`, Xvfb 1024×768×**8**), oracle runtime `~/opt/wine-11.0-oldwow-patched`, `EDITION=unreg RUNS=main BRIEF_SECS=12 TRACE=0 DEPTH=8 oracle/scenarios/start.sh`. The port ran the same path on an 8-bit Xvfb (`:145`).
- **Drivers:** `work/shell/s4/compare/scripts/` (gitignored), which extend the stage 3b/3d drivers:
  - `run.sh` repeats `oracle/scenarios/start.sh` step for step, and `all.sh` runs the four runs.
  - `install.sh` is the stage 3d install driver. `intro.sh` is the stage 2 watch protocol.
  - `batch.sh` and `sbs.sh` count pixels, as do `deskpairs.py`, `pages.py`, `blackout.py` and `dlgcmp.py`. The new scripts are `iniwrites.py`, `files.py` and `shift.py`.
  - The game-running check matches only the driver's own `WINEPREFIX`. `work/shell/s4/compare/README.txt` lists every output.
- **Pixel counts:** pixels whose largest channel differs by more than 4 % (10/255), oracle shot against the port shot of the same step. For a dialog the count covers the dialog's rectangle, otherwise the full screen. A count of 0 is pixel-exact.

## Install step (against install-0001)
| Step | Window | Port position and size | Differing px | Where |
|---|---|---|---|---|
| 01 | `IS Install` "Make sure any running programs…" | 357,356 309×82, same as oracle | 0 | |
| 03 | `Inner Space New Valid Install` (dialog 300) | 269,298 487×226, same | 606 (601 without `--to`) | the directory edit's text and the "Disk Space Available" value |
| 04 | `IS Install` screen saver question | 358,356 308×82 | 0 | |
| 05 | `Game Difficulty and Speed` | 310,267 404×261 | 0 | |
| 06 | `Play Full Screen or in a Window` | 310,318 404×158 | 0 | |
| 07 | ` Inner Space` personalization | 353,338 319×119 | 472 | the Name and Organization edits, which hold different names |
| 08 | `Inner Space` "Inner Space is now Validated and Installed!" | 342,360 340×75 | — | the oracle driver's second Alt+O closed it unshot. The text is dialog 100's, INSTALL.EXE seg2:0x1673 ([install.md](../../docs/re/install.md#dialogs)) |
| 08b | `README.TXT - Notepad` | 4,30 721×512 | — | |
| 09 | the installed game's title | | see Title | "Plus Edition", gold, "Reg User: " + name + " - " + organisation |

- **Dialog 300** shows `C:\SPACE\` in both. With and without `--to`, the port's edit text sits 1 px up and 1 px left (defect D1). Available reads "999934 KB" in the oracle (the Win16 figure) and "45277580 KB" in the port. The port's eight digits wrap "KB" onto a second, clipped line (D4). [ORACLE m2-accept-0001]
- **Files** (`install/files.txt`, `space-files.txt`). [ORACLE m2-accept-0001]
  - **Both:** `C:\SPACE` INSPACE.EXE, ISPACE.DLL, ENG_LANG.DLL, IGRAPHIC.DLL, FACTORY.EXE, ENG_HELP.HLP, README.TXT, ORDER.TXT and BACKUP.EXE; `SHIPS\` MASTER.BOD and MASTER.SHP; `SOUNDS\` SND_FX.DLL, ENG_FX.DLL and the four MIDs; empty `GAMES` and `RETIRED`.
  - **Oracle only, in `C:\windows`:** WAVEMIX.DLL, WAVEMIX.INI, CTL3D.DLL, INSPACE.SCR and INRUN.AD, plus `system.ini`. The port puts these five files in `C:\SPACE` and writes no SYSTEM.INI ([DESIGN] ADR 0008 decision 1).
  - **Port only:** `C:\windows\INSTALL\SET\`, the copy of the set with SECURE.DAT in it, where the oracle writes SECURE.DAT into its source `C:\temp` ([DESIGN] ADR 0008). `original/shipdisk` is unchanged: its listing before and after the run is identical.
- **INSPACE.INI [Inner Space] after the install** (values of `ThePlayer` and of the registration masked). [ORACLE m2-accept-0001]

| Key | Oracle | Port | Covered by |
|---|---|---|---|
| `MainShip`, `ThePlayer`, `PlayMode`, `FrameRateSet=36`, `Language=ENG`, `Path`, `ShipPath`, `SndPath`, `Games`, `RacePath`, `Dead`, `Complex=2`, `Full=1`, `Detail=3`, [Ship Factory], [Teams] | same values | same values | |
| `LocalPath` | `C:\SPACE\` | `C:\windows\` (the INI's directory) | [DESIGN] ADR 0004 decision 5 (`src/install/install.cpp`) |
| `SoundSystem` | 3 | not written | [OPEN] ordinal 4 command 0, INSTALL.EXE seg2:0x4098 |
| `Calibrate` | present (the oracle INI was copied after the game's first start) | written at the first start, 14000 | [DESIGN] ADR 0001 decision 9 |
| `vidsp`, `32BitSP`, `PDev` | SYSTEM.INI [386Enh], WIN.INI [windows] | INSPACE.INI [Registration] | [DESIGN] ADR 0008 decision 1 |
| [Port] `Installed=C:\SPACE\` | — | written | [DESIGN] ADR 0008 decision 1 |

- **Splash and sounds:** the port shows no splash (logged [OPEN], INSTALL.EXE seg2:0x2624) and plays no install sounds. Both are listed in the PLAN's stage 3d gaps. [ORACLE m2-accept-0001]

## Intro (against intro-0001, watch protocol, no input)
Phase runs from `classify.py`, in seconds from launch, with shots every 0.5 s to 30 s and every 2 s after. [ORACLE m2-accept-0001]

| Phase | Oracle (intro-0001) | Port |
|---|---|---|
| black | 0.01–0.54 | 0.01–0.56 |
| logo cell | 1.04–7.03 | 1.05–7.03 |
| logo sweep | 7.52–9.04 | 7.52–9.01 |
| SD_LOGO letters | 9.53–10.51 | 9.51–10.00 |
| "presents.. " | 11.05–13.55 | 10.56–13.03 |
| black second | 14.04–14.55 | 13.52–14.01 |
| title | 15.05–64.01 | 14.50–68.01 |
| credits | 68.05–98.03 | 72.04–100.01 |
| text page 1 | 100.04–122.03 | 102.03–124.04 |
| text page 2 | 124.03–150.02 | 126.05–152.01 |
| key commands | 152.02–162.01 | 154.04–162.02 |
| after the key page | logo cell 164, black 166, other 168–172, title 174 | logo cell 164–170, sweep 172, letters 174 |

- Up to the title the phases agree within one shot interval. The port's title lasts about 4 s longer: it ends in (68.0, 70.0] against (64.0, 66.0]. The PLAN's stage 2 note gives about 2.5 s. [OPEN] the scripted title scene, seg15:0x7792. [ORACLE m2-accept-0001]
- After the key page the oracle takes the demo-start path. The port restarts state 0 at the logo ([DESIGN] in `src/intro/intro.cpp`; not ported, seg15:0x5a64). [ORACLE m2-accept-0001]

## The shell path (against start-0001)
### Per-step pixel counts, `main` runs
| Step | Region | Unregistered | Registered | Main cause |
|---|---|---|---|---|
| title | full | 30 084 | 26 270 | see Title below |
| Restore Game | 315×260 at 355,281 | 3 749 | 3 795 | the title behind the dialog, and the description edit's text 1 px up-left (D1) |
| New Game, and each of its list steps | 297×190 / 297×330 at 364,302 | 1 926 | 1 704 | the black ship preview, and the PILOT edit's text 1 px up-left (D1); registered: the PILOT name |
| SHIP list pages | 150×232 at 425,382 | 22 per page, 4 pages | 16 per page, 8 pages | page counts match (61 and 112 ships) |
| New Game DRIVE list | 297×330 | 1 926 | 1 704 | rows "C: Unknown_HD", "D:" / "E: Unknown_HD", as the oracle shows |
| desk scene | full | see Desk | | |
| Navigation, Navigation after `games` | 560×350 at 30,30 | 94 188, 94 000 | 94 036, 93 848 | the stub window |
| load screen (2 s after Enter) | full | the port shows the greeting; the oracle stays black | lines 1/2/3: 2 775 / 2 136 / 0 | see Briefing |
| briefing | regions | see Briefing | | |
| wave start, +1 s, +2 s | full | 10 698, 10 935, 11 747 | 14 911, 13 914, 12 254 | state 1 is black in the port |
| exit dialog #244, again after Cancel | 339×93 at 343,351 | 0, 0 | 0, 0 | |
| after Cancel (wave) | full | 12 537 | 8 985 | state 1 is black |
| after Exit | 416×414 at 304,190 | 24 672 (#799) | 0 (no window in either) | #799's edit text 1 px up-left (D1) |

`briefesc` and `briefspace` give the same counts for the steps they share. The window positions match the oracle's in every step, with offset 0,0 in every `rclick` line of `steps.txt`. [ORACLE m2-accept-0001]

### Per-step pixel counts, `title` runs
| Step | Unregistered | Registered |
|---|---|---|
| Restore Game from the title | 3 749 | 3 795 |
| 0.5 s after Cancel (credits logo) | 666 | 755 |
| R: Order #693 (unregistered) / credits (registered) | 14 415 | 4 048 |
| Order with code 123 and Register | 19 026: Register is disabled, so the port opens no MessageBox ([DESIGN] ADR 0008 decision 3) | — |
| Order Form.., More Info.. (Notepad) | 700 each | — |
| **after Order Cancel** | **23 620: the oracle shows text page 1, the port is still in the credits (D2)** | — |
| 6 s after R | 25 140 (the same, D2) | 3 034 |
| #244 at the title, Save & Exit (greyed, no effect) | 0, 0 | 0, 0 |
| **after #244 Cancel** | **20 353: the same, the credits go on (D2)** | 3 857 |
| after Exit | 24 672 (#799, D1) | 0 |

### Title
- **Unregistered, 30 084 px by band** (24 bpp): lines 1–3 7 349, emblem 8 153, hint and copyright 4 655, trial line and R box 6 032, "ESC to play" line 3 895. [ORACLE m2-accept-0001]
- **Causes:**
  - The 24-bpp oracle antialiases its text. The port's text is not antialiased ([DESIGN] ADR 0007).
  - The emblem is the [OPEN] ordinal 12 stand-in.
  - The port draws no "256 color mode" hint ([DESIGN] ADR 0007 decision 5).
  - The trial day reads 12088 in the oracle and 12089 in the port: the captures are a day apart.
- **At depth 8** both sides lose the same colours, and the text then matches to the pixel (see Depth 8). The remaining 24-bpp text differences are therefore antialiasing, not metrics. [ORACLE m2-accept-0001]
- **Registered:** the port's title has "inner space™", "Plus Edition - V1.29.08 Aug 9/2000", gold text and "Reg User: <made-up name> - <made-up org>", with no hint, no trial line and no R box. Apart from the name line, the causes are the same as unregistered. [ORACLE m2-accept-0001]

### Desk scene and launch (against start-0001 `main`; newgame-0002's scene)
`deskpairs.py` pairs each oracle shot with the port shot nearest in time after New Game OK. "Outside" means outside the monitor text box. [ORACLE m2-accept-0001]

| After OK | Unregistered: differing px outside the text box | Registered |
|---|---|---|
| +0 to +1.2 s (desk, white monitor) | 24 179 | 24 179 |
| +1.2 to +15.7 s (typing) | 24 179–33 808 | 24 179–26 586 |
| +15.7 to +21.0 s (ship rises and flies) | 37 115–43 765 | 37 115–45 907 |
| port black from | +22.04 | +21.74 |
| oracle black / "ANALYSING DRIVE" from | +23.15 | +22.94 |
| Navigation window | port +22.51, +22.45, +22.46; oracle +23.20, +23.21, +23.14 | port +22.18, +22.14, +22.14; oracle +23.47, +23.37, +23.41 |

- The 24 179 px baseline is the desk geometry of slots 2 and 4 and the monitor neck. The ship and pad are placeholders. Both are [OPEN] in the PLAN's stage 3d note (`ISPACE.DLL seg8:0x1fe6`, `seg8:0x3874`, `seg8:0x5632`, `seg15:0x9e6`, `seg15:0x2a08`, `seg15:0x3bfa`). [ORACLE m2-accept-0001]
- **The launch ends 1.1–1.2 s early** in both editions, and the Navigation window opens 0.7–1.3 s early. The PLAN's stage 3d note says about 1.3 s ([OPEN]). [ORACLE m2-accept-0001]
- **"Analysing Drive"** shows in no port shot. The port is black for about 0.4 s, and the next shot already shows the Navigation window being drawn. The traced oracles show the text in the shot before the Navigation window, and so does the untraced depth-8 oracle (`oracle8`, +23.18, 142 green px) (D3). [ORACLE m2-accept-0001]

### Briefing
- **Load screen, unregistered:** the port draws the green greeting for the 2 s; the oracle stays black. Covered by [DESIGN] in `src/briefing/hooks.cpp` (Wine does not refresh during the busy-wait). [ORACLE m2-accept-0001]
- **Load screen, registered:** three lines at y 294, 329 and 384 in both. Line 3, "ENTERING DRIVE E:UNKNOWN HD" in (0,255,0), is pixel-exact. Lines 1 and 2 use 0x0000FF80. The oracle draws them in (128,255,0); the port draws them in (128,128,0), the nearest static colour. That is [DESIGN] ADR 0007: plain RGB maps to the nearest static colour. Line 1 also differs in its words: the time of day and the name. [ORACLE m2-accept-0001] [BYTES seg15:0x6679 seg15:0x6772]
- **Box:** the briefing box appears in the shot at +2.20 s after Enter in both editions, port and oracle. [ORACLE m2-accept-0001]
- **Lower panel:** pixel-exact at 24 bpp (0 px, `main-gamesroot` against `main` at +11.65, +20 and +27 s) and at 8 bpp. The typing runs about 1.12× faster in the port: about 610 yellow pixels per 0.55 s against about 545. The text is complete at +5.49 s, against (+6.64, +7.20] in the oracle. [DESIGN] ADR 0009. [ORACLE m2-accept-0001]
- **Title bar and status bar** (24 bpp): 2 726 and 763 px, from the antialiased oracle text. At 8 bpp the title bar is pixel-exact. [ORACLE m2-accept-0001]
- **Upper panel, icons page** (`main-gamesroot`, D: = `fixture\GAMES`): 616 px. The three icons, their dot rows and their labels are in place. In the `main` runs the port hands the drive root `D:\` to the briefing, so the status bar reads "D:\" and the icons page is skipped ([OPEN], PLAN stage 3b, `DGROUP:0x38c8`'s writer). [ORACLE m2-accept-0001]
- **Cycle** (`pages.py`; seconds after Enter): [ORACLE m2-accept-0001]

| | First page | Icons page starts | Cycle | Holds: icons / ambulance / reality check / blank |
|---|---|---|---|---|
| Oracle, unregistered | I +7.75 | 7.75, 32.29, 57.93, 84.12, 110.33 | 24.5–26.2 s | 7.3–7.8 / 8.4–8.9 / 5.0–5.6 / 1.7–2.8 |
| Oracle, registered | I +7.18 | 7.18, 31.72, 57.38, 81.90, 106.43, 131.53 | 24.5–25.7 s | similar |
| Port (`main-gamesroot`) | I +6.60 | 6.60, 33.69, 60.77, 87.86, 114.96, 142.05 | 27.09 s | 7.2–7.7 / 9.5–10.0 / 5.0–5.5 / 1.7–2.7 |
| Port, `main` (drive root, no icons page) | A +7.70 | — | about 19 s (ambulance and reality check) | — |

- The port's longer cycle comes from the ambulance page. It matches the PLAN's 27.1 s against 24.8 s ([DESIGN] ADR 0009). [ORACLE m2-accept-0001]
- **The briefing does not end by itself:** 150 s in both, as in the oracle. **ESC** starts state 1 within 0.02 s in the port's log; the oracle's shot 0.3 s later already shows the wave. **Space** advances the upper panel and never starts the wave (40 s of Space). The first Space, during the load, leaves the lower text complete when the box appears: 4 070 yellow px at +2.25 against +2.27 in the oracle. Both editions behave the same. [ORACLE m2-accept-0001]

### Wave and exit
- State 1 is black in the port. The oracle draws the gate, rocks, the icon column and the HUD. The port logs "unimplemented: wave frame and the HUD (seg11:0xafe)". The PLAN's stage 3d note says "State 1 stays black (M3)". Its M2 goal sentence says "a black play area with the HUD frame". [ORACLE m2-accept-0001]
- #244, Cancel, Exit, #799 (unregistered) and the end match. The registered game ends within 2.2 s of Exit without #799. [ORACLE m2-accept-0001]

## INI writes (every run, both editions)
`iniwrites.py` lists the keys whose value changed from `INSPACE.INI.before` to `INSPACE.INI.after`. [ORACLE m2-accept-0001]

| Run | Keys written, same value in port and oracle | Differ |
|---|---|---|
| main, briefesc, briefspace | `Restore=NEW GAME`, `MainShip` (TRANSFORMER / SKY TALON), `Drive` (D / E), `Size=1`, `SetSize=1`, `PlayMode=0`, `NavLeft=30`, `NavRight=560`, `NavTop=30`, `NavBottom=350`, `Full=1`, `Menu=0`, `IconWnd=1`, `VScale=0`, `SmWnd=0`, `SScale=0`; `ThePlayer` unchanged | `Calibrate`: oracle 27 850–29 334, port 14 000 ([DESIGN] ADR 0001 decision 9) |
| title, registered | the exit keys | `Calibrate` |
| title, unregistered | the exit keys | `Calibrate`; the port also writes [Registration] `Serial` and `SerialCount` when Order opens ([DESIGN] ADR 0008 decision 3) |

- **Raw file:** apart from `Calibrate`, the port's file keeps the blank lines between sections, which the Wine profile API drops from the oracle's file (Wine artefact). The registered port INI also keeps its install-time differences (`LocalPath`, no `SoundSystem`, [Registration], [Port], as in the install table). [ORACLE m2-accept-0001]
- **Files written** (`files.py`): in all eight runs, both port and oracle write only `C:\WINDOWS\INSPACE.INI`. Nothing is written under the game directory or the fixture drive. [ORACLE m2-accept-0001]

## Depth 8 (oracle and port on a 1024×768×8 PseudoColor Xvfb)
| Shot | Region | Oracle 8 against port 8 | Oracle 8 against oracle 24 | Port 8 against port 24 |
|---|---|---|---|---|
| title (t 20) | full | 11 988 | 51 305 | 40 585 |
| | lines 1–3 / trial and R / ESC line | 6 / 0 / 0 | | |
| | emblem | 11 982 | | |
| briefing (+11.7 s after Enter) | full | 18 329 | 26 824 | 28 726 |
| | title bar / lower panel / outside | 0 / 0 / 0 | | |
| | status bar / upper panel | 743 / 17 586 (D:\ against D:\GAMES\; ambulance against icons page) | | |

- At depth 8 the oracle shows the colour losses of [depth8-0001](depth8-0001.md) and [depth8-0002](depth8-0002.md). The title text is grey-white instead of purple, and the copyright, hint and R box are missing. The emblem is green, and the briefing's title and status bars are black with green outlined text. [ORACLE m2-accept-0001]
- The port, which draws one fixed 8-bit surface and never calls `RealizePalette` (ADR 0007), shows the same losses on the same display. Its text then matches the oracle's to the pixel. These colours are therefore the 8-bit X display under Wine, not the game: an 8-bit Wine oracle is no reference for the port's colours (Wine artefact). It does confirm the glyph shapes, the title-text and briefing layout, and the missing hint at depth 8. [ORACLE m2-accept-0001]
- The port's own colours change with the display depth (40 585 px on the title), and so do the oracle's. At depth 8 the registered greeting's lines 1 and 2 vanish, and only line 3's green shows. [ORACLE m2-accept-0001]

## Deviations
### Covered by [DESIGN]
- No "256 color mode" hint, text not antialiased, plain RGB on the nearest static colour (the greeting's lines 1 and 2 olive) — ADR 0007.
- Briefing typer about 1.12× fast, cycle 27.1 s against 24.5–26.2 s — ADR 0009.
- `Calibrate=14000` — ADR 0001 decision 9.
- The installer's destination-8/9 files in the game directory, the set copied to `<INI dir>\INSTALL\SET`, no SYSTEM.INI or WIN.INI writes, the three values in [Registration], [Port] `Installed`, `LocalPath` = the INI directory, the 64-bit free-space figure — ADR 0008, ADR 0004 and source comments.
- Register disabled (no MessageBox for a wrong code), [Registration] `Serial`/`SerialCount` — ADR 0008 decision 3.
- Unregistered load screen shows the greeting — `src/briefing/hooks.cpp`.
- After the key page the port restarts at the logo instead of the demo start — `src/intro/intro.cpp` (seg15:0x5a64).

### Covered by [OPEN]
- Title emblem stand-in (ordinal 12). Scripted title scene not ported (seg15:0x7792): the title runs about 4 s long here, against the PLAN's 2.5 s.
- New Game ship preview and Order logo black (`ISPACE.DLL seg10:0x2aac`, `seg10:0x2f4`).
- Navigation stub (94 000 px), drive root handed to the briefing so the `main` runs have no icons page (`DGROUP:0x38c8`).
- Desk slots 2 and 4, the monitor neck, the pad and ship placeholders (24 179–45 907 px), and the launch ending 1.1–1.2 s early.
- State 1 black, no HUD (seg11:0xafe; the PLAN's stage 3d note "State 1 stays black (M3)").
- Installer: no splash, no sounds, no `SoundSystem=3` (INSTALL.EXE seg2:0x2624, seg2:0x4098).

### Wine artefacts and capture conditions
- Antialiased 24-bpp text in the oracle. The INI's blank lines dropped by Wine's profile API. The depth-8 colour losses, which port and oracle share. The trial day differs (12088 against 12089) because the captures are a day apart. The greeting's time of day and the desk's weekday follow the clock.
- Notes for the orchestrator:
  - The PLAN's stage 3b line "the Restore description edit 330 is empty" no longer holds: the port shows "NEW GAME, STARTING AT WAVE 1." in it.
  - The New Game DRIVE list now matches the oracle once the prefix registers `d:` as `hd`. The stage 3d template prefix had `d:` and `e:` as `cdrom`.

### Defects (for the fixer)
- **D1. Edit text is drawn 1 px up and 1 px left in every edit control.** Affected: the PILOT edit of New Game, the Restore Game description (330), the Registration Benefits #799 text, and the installer's directory edit and personalization edits.
  - **Evidence** (`shift.py`): shifting the port by (−1, −1) drops the #799 edit box from 24 459 to 620 differing px, the Restore description from 750 to 387, and PILOT from 234 to 71. The borders themselves match. [ORACLE m2-accept-0001]
  - **Hypothesis:** `win16_edit_borders` (`src/install/install_win32.cpp:96` and the dialogs' copy in `src/dialogs/dialogs.cpp`) takes `WS_EX_CLIENTEDGE` off and adds `WS_BORDER` after the edit exists. It then sends `SWP_FRAMECHANGED | SWP_NOSIZE`. The client area grows from a 2-px inset to a 1-px inset, which moves the client origin 1 px up-left. Because the size does not change, the edit gets no `WM_SIZE` and keeps the formatting rectangle it computed for the client edge, so the text moves with the client origin.
  - **To test:** re-send `WM_SETFONT` (or `EM_SETRECTNP` from the new client rectangle) after the frame change, or create the controls without the client edge (dialog template style), and re-measure.
  - The original's templates are ENG_LANG.DLL DIALOG 267, 321 and 799, and INSTALL.EXE's dialogs 300 and the personalization dialog.
- **D2. A zero-result dialog during the credits does not end the credits.** After Order Cancel (command 8) or #244 Cancel (command 7) in phase 3, the oracle shows text page 1 within 0.5 s. The port's log has "command 8 returned 0", and its credits go on: entries at 42.6 s and 47.3 s, "Music Composed By" still at 55 s. [ORACLE m2-accept-0001]
  - At the title (phase 2) the same rule works: Restore Cancel moves to the credits at once in both.
  - **Hypothesis:** `Intro::after_dialog` sets `deadline_ = now_` (seg10:0x8550). The palette-message emulation then repaints on reactivation (ADR 0007 decision 5). `Intro::repaint` (`src/intro/intro.cpp`, seg11:0x15fc–0x1661) first assigns `now_ = now`, a fresh tick, and only then tests `deadline_ != now_`. The test therefore fails, and phases 3–5 get now + 15 000 ms.
  - In the original the test compares the deadline DGROUP:0x6e28 with DGROUP:0x1e50. seg10:0x82a2 has just set both to the same value (seg10:0x8429, seg10:0x8550), so the repaint keeps "end now".
  - **Fix direction:** compare against the stored `now_` from after_dialog, not the repaint's tick, and only then refresh it. Check the order in seg11:0x15fc.
- **D3. "Analysing Drive …" never shows.** In the port the desk ends, the screen stays black for about 0.4 s, and the Navigation window follows with no green text in any shot (shots 0.05 s apart, 4 runs). The traced oracles show it in the last shot before the Navigation window, and so does the untraced depth-8 oracle (+23.18 s). [ORACLE m2-accept-0001]
  - **Hypothesis:** `Intro::analysing` (seg6:0x1d9a) presents the text, and `open_navigation` (seg6:0x1c72) follows in the same tick. The stub window appears at once, without the original's tree walk. Its activation, or the palette emulation's black erase, clears the main window before a frame shows the text.
  - Low confidence: it may be visible for less than 0.05 s. Check with a frame log or a 10 ms shot loop.
- **D4. The installer's "Disk Space Available" value overflows its static (control 131).** Eight digits put "KB" on a clipped second line (dialog 300; the source comment marks the 64-bit figure [DESIGN] with no ADR). The oracle's Win16 figure is six digits.
  - **Fix direction:** decide in ADR 0008 whether to cap the figure, for example at the DOS ceiling as INSTALL.EXE seg2:0xc44 sees it, or to widen the format.

## Verdict
- **Matches:** the install step's dialogs (pixel-exact except D1/D4 and the names), files and INI keys; the dialog geometry and flow; the exit path; every INI write and the written-file list; the briefing's lower panel and its ESC/Space behaviour.
- **Covered deviations:** the remaining screen differences are [DESIGN]- or [OPEN]-covered (title rendering, desk placeholders, Navigation stub, black state 1, briefing cycle length).
- **Defects:** D1–D4 above.
- **For the orchestrator:** whether the M2 goal's "HUD frame" still applies, since the PLAN's stage 3d note defers state 1 to M3.

## Reproduce
- `work/shell/s4/compare/scripts/install.sh`, then `all.sh unreg` and `all.sh reg` (in parallel), and `intro.sh watch`.
- `PORT_ID=35 DEPTH=8 PORT_PREFIX=<ws-s4-i> OUTNAME=main-depth8 run.sh unreg main 12`, and the `main-gamesroot` run with D: on `fixture\GAMES`.
- The 8-bit oracle: `ORACLE_ID=33 WINEPREFIX=<ws-s4-o8> DEPTH=8 TRACE=0 EDITION=unreg RUNS=main BRIEF_SECS=12 oracle/scenarios/start.sh work/shell/s4/compare/oracle8`.
- Analysis: `batch.sh <edition> <run>`, `deskpairs.py`, `pages.py`, `iniwrites.py` and `files.py`, as listed in `work/shell/s4/compare/README.txt`.
- About 12 minutes of runs in parallel.
