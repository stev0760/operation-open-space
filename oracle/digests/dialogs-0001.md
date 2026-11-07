# dialogs-0001: the port's converted dialogs against the original's

- **Date:** 2026-10-24. **Host:** Rocky 9.8, Xvfb 1024×768×24 on two displays.
- **Port side:** `isview.exe` from the `mingw-x64` preset (Debug), run as `isview.exe Z:<repo>/work/extracted/game --dialog N` for each of the 22 ENG_LANG.DLL dialogs. isview builds each template with the asset store's `dialog_template` and opens it modeless with `CreateDialogIndirectParamW`. Runtime: stock Wine 11.0 new WoW64 (`/opt/wine`), prefix `work/wine/isview`, display `:151`, `WINEDEBUG=-all`, `WINEDLLOVERRIDES="mscoree,mshtml="`.
- **Original side:** the default runtime `/opt/wine-11.0-oldwow-patched`, prefix `innerspace9` (`ORACLE_ID=9`), display `:152`. `scenario_prefix` of `oracle/scenarios/common.sh` prepares the prefix: `--with-ini`, fixture `basic` as `D:`, no `Z:`, `SoundSystem=0`. The game runs traced with the default relay profile. The dialogs were opened by hand with `xdotool`.
- **Measure:** on each side the dialog is the X window that `xwininfo -root -tree` lists under its caption. The size is that window's geometry, and `import -window` captures it. Without a window manager the X window covers the client area only [ORACLE newgame-0001]. The original's crops come from root screenshots at that geometry. A watcher polled the named top-level X windows every 0.2 s and logged each change (`windows.log`). [DESIGN]
- **Reproduce:**
  - Port: build isview, then run `oracle/scenarios/dialogs_port.sh <build>/tools/isview.exe`.
  - Original: run `oracle/scenarios/dialogs_orig.sh work/oracle/dialogs/orig-1`, which sources `common.sh` with `ORACLE_ID=9`, moves to `DISPLAY=:152` and starts `oracle/scenarios/watch_windows.sh`. Then repeat the `xdotool` steps logged in `orig-1/steps.txt`.
  - Trace digest: `re/.venv/bin/python -I oracle/relay_digest.py orig-1/trace.log` (the bulk copy is gzipped).
- **Bulk output (gitignored):** `work/oracle/dialogs/`:
  - `port/`: 22 captures `<id>.png` and `geometry.tsv`.
  - `orig-1/`: title, Options, Order, exit, Restore Game, New Game, then wave 1 with Save, the ambulance, Sell, help Search and the exit path. It holds 22 screenshots, the crops `<id>.png`, `windows.log`, `steps.txt`, `trace.log.gz` (204 MB unpacked) and `digest.md`.
  - `orig-2/`: the sound controls, then the screen saver's settings with `432.png` and the traced `scr-trace.log`.
  - `orig-3/`: Options with `SoundSystem=-1`.
  - `side/`: 14 side-by-side images, original left and port right.
  - `diff.tsv`: sizes and differing pixels. `setfont.txt`: fonts and icons per dialog from the trace.

## Sizes
Template sizes are in dialog units, and pixel sizes are client areas. The last column is the share of differing pixels between the two captures (ImageMagick 6.9.13 `compare -metric AE`, no fuzz). [ORACLE dialogs-0001] [BYTES ENG_LANG.DLL DIALOG]

| Id | Caption | Template | Port | Original | Port − original | Differing pixels |
|---|---|---|---|---|---|---|
| 100 #0064 | Inner Space Options/Setup | 180×248 | 270×403 | 270×403 | 0×0 | 5.5 % |
| 101 #0065 | Inner Space | 185×20 | 324×35 | not observed | | |
| 139 #008b | Ship Handling | 228×128 | 342×208 | not observed | | |
| 167 #00a7 | Save Game | 180×43 | 270×70 | 270×70 | 0×0 | 3.2 % |
| 169 #00a9 | Ship Behaviour | 229×97 | 344×158 | not observed | | |
| 194 #00c2 | Ambulance Options (To Buy) | 284×135 | 426×219 | 426×219 | 0×0 | 5.0 % |
| 244 #00f4 | Inner Space | 226×57 | 339×93 | 339×93 | 0×0 | 4.1 % title, 4.2 % wave |
| 267 #010b | Restore Game | 210×143 | 315×232 | 315×232 | 0×0 | 14.2 % |
| 286 #011e | New Ship Body | 208×77 | 312×125 | not observed | | |
| 300 #012c | Import Ship Body | 239×122 | 359×198 | not observed | | |
| 321 #0141 | New Game | 198×117 | 297×190 | 297×190 | 0×0 | 17.4 % |
| 378 #017a | Ship Options (To Sell) | 243×124 | 365×202 | 365×202 | 0×0 | 3.0 % |
| 404 #0194 | Search | 186×135 | 279×219 | 279×219 | 0×0 | 9.5 % |
| 432 #01b0 | Inner Space Demo Options | 233×172 | 350×280 | 350×280 | 0×0 | 20.2 % |
| 589 #024d | New Demo | 198×105 | 297×171 | not observed | | |
| 666 #029a | Display Options | 194×100 | 291×163 | 291×163 | 0×0 | 5.6 % |
| 693 #02b5 | Order Inner Space Now! | 302×235 | 453×382 | 453×382 | 0×0 | 11.2 % |
| 799 #031f | Registration Benefits | 277×255 | 416×414 | 416×414 | 0×0 | 10.6 % |
| 975 #03cf | Joystick Configuration | 187×227 | 281×369 | 281×369 | 0×0 | 25.6 % |
| 988 #03dc | Important Information! | 310×247 | 465×401 | not observed | | |
| 999 #03e7 | Audio Options | 171×158 | 257×257 | not observed | | |
| 1872 #0750 | Inner Space Plus Pack! | 344×272 | 516×442 | not observed | | |

- All 13 observed dialogs have the same client size in the port and in the original. [ORACLE dialogs-0001]
- The port sizes of the 21 templates in 8 pt Helv equal the template size scaled by 6/4 horizontally and 13/8 vertically, rounded to the nearest pixel. #101, the only template in 9 pt Arial, scales by 7/4 and 14/8. The font substitution behind these factors is in [win16-translation.md, Templates](../../docs/re/win16-translation.md#templates). [ORACLE dialogs-0001] [BYTES ENG_LANG.DLL DIALOG]
- The crops of #267 and #321 are pixel-identical to the same regions of the screenshots of newgame-0001, newgame-0002 and wave-0001's newgame run, and those of #167 and #799 to wave-0001's reference run. Their positions are the ones those digests give. [ORACLE dialogs-0001] [ORACLE newgame-0001] [ORACLE newgame-0002] [ORACLE wave-0001]
- The differing pixels come from what the original's procedures add (below) and from text rendering (see Text). [ORACLE dialogs-0001]

## Placement in the original
- Each of the 12 traced dialogs that the game moves with `MoveWindow` gets a window rectangle 6×32 larger than its client area. [ORACLE dialogs-0001]
- An X window first appears at its template position, measured from the owner's client area, before the move. New Game appears first at 93,29, Joystick Configuration at 598,245 over Options, Ship Options at 376,223 over the ambulance dialog, and Demo Options at 199,110. The watcher catches only some of these first positions. [ORACLE dialogs-0001]
- The dialogs that newgame-0001 and wave-0001 do not place: [ORACLE dialogs-0001]

| Id | DIALOGBOX return address | MoveWindow (window) | Client at |
|---|---|---|---|
| 100 | ISPACE.DLL seg9:0x15b3 | 374,167 276×435, ISPACE.DLL seg9:0x1b26 | 377,196 |
| 666 | ISPACE.DLL seg9:0x1e2b | none | 487,245 |
| 975 | ISPACE.DLL seg9:0x1e2b | 369,184 287×401, ISPACE.DLL seg9:0x4708 | 372,213 |
| 693 | ISPACE.DLL seg10:0x4351 | 283,177 459×414, ISPACE.DLL seg10:0x02ec | 286,206 |
| 404 | INSPACE.EXE seg17:0x594f | 170,109 285×251, INSPACE.EXE seg17:0x8613 | 173,138 |
| 432 | INSPACE.SCR process | 334,228 356×312 | 337,257 |

- The window rectangles of #100, #975, #693, #244, #267, #321, #167, #799 and #432 have their centre at the screen centre (512,384), within half a pixel. The centres of #194 and #378 lie at x 512–512.5 and y 234–234.5, and that of #404 at 312.5,234.5. #666 stays at its template position, 110,49 from the client origin of Options. [ORACLE dialogs-0001]
- isview opens its dialogs at their template positions, measured from its own window, so this digest compares sizes and not positions. [DESIGN]

## What the original's procedures add
The template gives only the controls and their template texts. In the original the following appear in addition. Return addresses use the selector map of newgame-0001, which this run repeats. [ORACLE dialogs-0001]
- **Fonts.** The game sends `WM_SETFONT` to the controls of every dialog it opened. [ORACLE dialogs-0001]
  - In each dialog that ISPACE.DLL opens, every control gets one message. It comes from ISPACE.DLL seg10:0x60c5 or seg10:0x60dd, and in #975 from ISPACE.DLL seg9:0x438d–seg9:0x4505. #693 and #799 get 6 and 1 more from other places in ISPACE.DLL seg10.
  - These fonts are `CreateFont(14, 6, …, "Arial")` at weights 0 and 730, from ISPACE.DLL seg9:0x29ed and seg9:0x2a14.
  - #194 and #378 send 10 messages each with `CreateFont(14, 0, …, "Arial")`, which INSPACE.EXE seg6:0x198f creates at start-up.
  - #404 uses `CreateFont(12, 6, …)` and `CreateFont(14, 7, …)` at weight 730, both "Arial".
- **Icons.** In #100, #244 and #799 the game calls `LoadIcon(INSPACE.EXE, "GAME")` and `SendDlgItemMessage(dialog, 591, 0x0400, icon)`, and the icon shows at the SS_ICON control 591. The port leaves that place empty, and ENG_LANG.DLL holds no ICON resource ([resources.md, Inventory](../../docs/re/resources.md#inventory)). [ORACLE dialogs-0001]
- **#267 Restore Game:** [ORACLE dialogs-0001]
  - The GAME combo holds NEW GAME, selected.
  - PILOT, SHIP and SCORE read "?" where the template has "AA", "A", "A" [BYTES ENG_LANG.DLL DIALOG 267].
  - The description edit reads "NEW GAME, STARTING AT WAVE 1.".
  - A black 96×96 `LevClass` child sits at 4,119, created at ISPACE.DLL seg10:0x167d.
  - `MoveWindow` gives the GAME combo a height of 44 at its template position and width.
- **#321 New Game:** [ORACLE dialogs-0001]
  - The fields read "Your Name", TRANSFORMER (selected), AVENGERS, ACTION, "C: Unknown_HD" and 1x.
  - A 96×96 `LevClass` child at 193,87 shows the ship, created at ISPACE.DLL seg10:0x22ec.
  - `MoveWindow` gives the PLAY and DRIVE combos a height of 58 at their template positions and widths.
- **#100 Options/Setup:** [ORACLE dialogs-0001]
  - The combos read No Sound, Mid-Wave, Highest, 1x and Normal, and Enhanced Graphics is checked.
  - Audio.., the Master Volume scroll bar and the sound combo are disabled.
  - Static texts replace the template's: "Play Speed  36 frames/sec" for "Play Speed", "English" for "x", "V1.29.08" for "V 0.899" and "Aug 9/2000" for "Nov 1/94" [BYTES ENG_LANG.DLL DIALOG 100].
- **#666 Display Options:** Game Border, Animated Ship Watches and Always Ask for Save Name are checked, and Skip Sequences reads Skip None. [ORACLE dialogs-0001]
- **#975 Joystick Configuration:** [ORACLE dialogs-0001]
  - The edit reads "There is no joystick attached. Please check the cable connection.".
  - Calibrate, Button 1–4 and the four combos are disabled. The combos read Fire Gun, Fire Weapon, Change Weapon and Turbo Thrust.
  - A black 160×160 `LevClass` child at 40,88 (ISPACE.DLL seg9:0x46c5) covers the check box of "4 Buttons", whose label shows grey.
  - `MoveWindow` (ISPACE.DLL seg9:0x44ec) gives the four combos a height of 128 at their template positions and widths.
- **#693 Order Inner Space Now!:** [ORACLE dialogs-0001]
  - Texts: "You are on day 12087 of your demo trial period." for "You are on day xx of your trial.", Serial # 10197298 for "xxxxx", and "If today is not 10/24/126, …" for the `%d/%d/%d` [BYTES ENG_LANG.DLL DIALOG 693].
  - The static "or 1-800-307-7223" does not show.
  - A 146×114 `LevClass` child at 298,70 (ISPACE.DLL seg10:0x02cb) shows the logo over the right end of the long order text.
  - More Info.. starts `NOTEPAD C:\INSPACE\README.TXT`.
- **#244 exit:** the text reads "Are you sure you want to exit Inner Space?" after Alt-X. Save & Exit is disabled on the title screen and enabled in the wave. [ORACLE dialogs-0001] The text after ESC is in [wave-0001](wave-0001.md#abort-and-exit). [ORACLE wave-0001]
- **#167 Save Game:** the game creates an editable combo box of style 0x50210102 (CBS_DROPDOWN, CBS_SORT) at 65,10, 140×30, from ISPACE.DLL seg10:0x0c10. The template has no control there [BYTES ENG_LANG.DLL DIALOG 167]. [ORACLE dialogs-0001]
- **#194 Ambulance and #378 Sell:** [ORACLE dialogs-0001]
  - The owner-drawn Options lists hold the priced entries of [wave-0001](wave-0001.md#ambulance-h).
  - Credit, Total and Remaining show "1.50 ICONS", "0.0 ICONS" and "1.50 ICONS", cut off at the edit's width. Remove is disabled.
  - The labels render wider than in the port. "Total Cost:" shows only "Total", and "Remaining:" shows as "emaining:".
- **#404 Search:** the list holds the help subtopics, with "TRANSFORMER - Your Name - SPACE SHIPS" selected. [ORACLE dialogs-0001]
- **#799 Registration Benefits:** the edit holds the text of ENG_LANG.DLL #5000 330 [ORACLE wave-0001], and the icon shows. [ORACLE dialogs-0001]
- **#432 Demo Options:** [ORACLE dialogs-0001]
  - The screen saver opens it: `INSPACE.SCR /c` shows the saver's own settings window, its template #07d3 [BYTES INSPACE.SCR DIALOG 2003]. Its Configure.. button loads ISPACE.DLL, ENG_LANG.DLL and IGRAPHIC.DLL and calls `DIALOGBOX` with ENG_LANG.DLL's instance and template #01b0 (`scr-trace.log`).
  - Random Demo is selected. The Racing combos read Lazy Loop, Normal, "? LAPS" and "1 SHIP", Zoom reads "?", and SHIP #1 and SHIP #2 read ".RANDOM".
  - Two black 96×96 `LevClass` children sit at 76,147 and 248,147.
  - The radio buttons show no mnemonic underline. The port underlines their D, G, R and B.

## Text
- Ink widths differ, and ink heights match for every label below. The table gives widths in pixels, counting pixels darker than 50 % grey. [ORACLE dialogs-0001]

| Dialog, label | Original | Port |
|---|---|---|
| #100 "Master Volume" | 72 | 69 |
| #666 "Skip Sequences:" | 81 | 79 |
| #321 "PILOT:" (right-aligned) | 31 | 32 |
| #267 "GAME:" (right-aligned) | 31 | 32 |
| #194 "Options" | 41 | 35 |

- Long statics wrap at other words. In #799 the original breaks the first line after "We worked" and the port after "hard to". #693's long order text breaks differently too. [ORACLE dialogs-0001]
- How far the port should match the original's fonts is [OPEN].

## Not covered
- #999 Audio Options: Audio.. and the sound combo stay disabled with `SoundSystem=0` and with `SoundSystem=-1` (`orig-3`), so the dialog did not open. [ORACLE dialogs-0001] The capability level from the device test enables them, not `SoundSystem`: this host had no wave device (`sound-0002`).
- #101: orig-1's trace has no `CREATEDIALOG` call, and no window with its caption and size appeared. [ORACLE dialogs-0001]
- #139, #169, #286 and #300 share their captions with items of the Factory menu (MENU 110) [BYTES ENG_LANG.DLL MENU 110]. FACTORY.EXE was not run. [OPEN]
- #589, #988 and #1872: their triggers were not reached. [OPEN]
