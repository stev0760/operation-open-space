# start-0001: the shell path from the title to the wave and out, both editions

- **Date:** 2026-10-25. **Host:** Arch Linux, Wine 11.0 old WoW64 with the ExtractIcon patch (`~/opt/wine-11.0-oldwow-patched`, the `oracle/env.sh` default), Xvfb 1024×768×24.
- **Unregistered:** prefix `innerspace4` (`ORACLE_ID=4`, display `:114`), made fresh for each run by `scenario_prefix`: game layout in `C:\INSPACE`, its INSPACE.INI, fixture `basic` as `D:`, `SoundSystem=0`.
- **Registered:** the existing install in prefix `innerspace9` (`ORACLE_ID=9`, display `:119`), started from `C:\SPACE` after `oracle_trace_config`. The scenario does not run `scenario_prefix` or `install_layout.py` there. For each run it maps fixture `basic` as `E:`, because that prefix's `D:` is a CD device. Its INSPACE.INI has `SoundSystem=3`, `MainShip=SKY TALON` and the player name in `ThePlayer`. The scenario backs that INI up and restores it after each run. At the end it unmaps `E:` and clears the relay filter. The INI after the scenario is byte-identical to the INI before it.
- **Trace:** the default relay profile (`oracle/trace.sh`), unchanged excludes.
- **Reproduce:** `EDITION=unreg oracle/scenarios/start.sh` and `EDITION=reg oracle/scenarios/start.sh`. Each takes about 9 minutes. Both editions can run in parallel.
- **Bulk output (gitignored):** `work/oracle/start-unreg/` and `work/oracle/start-reg/`. Each holds one directory per run: `main`, `briefesc`, `briefspace` and `title`.
  - Every run directory holds `NNN-step-tSSS.SS.png`, where SSS.SS counts seconds from `wine INSPACE.EXE`.
  - It also holds `steps.txt` (each key and click with its wall-clock time and its t), `trace.log` (18–146 MB), `INSPACE.INI.before`, `INSPACE.INI.after` and `files.txt`.
  - The registered INI copies show `ThePlayer=<redacted>`.
  - `work/oracle/start-explore/` holds the first hand-driven pass. `start-reg-main.out` holds a registered run that picked drive `C:` (see "What did not work").
- **Times:** t is seconds since launch. The first trace line falls within 0.1 s of t = 0, so trace time − first trace time ≈ t. Steps log their t just after the key or click. "(a, b]" gives the bounds set by two consecutive shots.
- In this digest, "the registered name" and "the organisation" stand for values of the user's install. Their values are not recorded here.

## Title (t ≈ 20)
- **Unregistered** (`main/001`): the title as described in [intro-0001](intro-0001.md), with purple text, "limited inner space™", "Standard Edition - V1.29.08 Aug 9/2000", the trial-day line and the " Press 'R' to order the full game package " box. [ORACLE start-0001]
- **Registered** (`main/001`): [ORACLE start-0001]
  - Every title text is gold instead of purple.
  - Line 2 reads "inner space™", without "limited". Line 3 reads "Plus Edition - V1.29.08 Aug 9/2000".
  - In place of the trial line and the R box, one line reads "Reg User: " + the registered name + " - " + the organisation. It is gold, centred, with its baseline about at the trial line's.
  - The hint box, the copyright line and "ESC to play    -    Alt-X to Exit" are unchanged in place.
- While any game dialog is open, the main window's caption is "Inner Space - PAUSED..". This holds in both editions, at the title and in the wave (`windows:` lines in `steps.txt`). [ORACLE start-0001]

## Restore Game #267 (ESC at the title)
- ESC calls `DIALOGBOX(#010b)` at once (trace 20.15 s, ESC at t = 20.18). The X window sits at 355,281, 315×232. [ORACLE start-0001]
- The contents are identical in both editions (`main/002`, `main/003`). [ORACLE start-0001]
  - The GAME combo lists one entry, NEW GAME, which is selected. Neither prefix has a `.SAV` in its GAMES directory.
  - PILOT, SHIP and SCORE read "?". The description reads "NEW GAME, STARTING AT WAVE 1.", and a black 96×96 box sits at the lower left.
  - The buttons are OK (622,300) and Cancel (622,333).
- **OK:** the dialog returns 1 and the game writes `Restore= NEW GAME`, with a leading space (ISPACE.DLL seg10:0x1d9b return). The INI file then holds `Restore=NEW GAME`, because the profile API trims the space. New Game #321 (`#0141`) opens in the same tick. [ORACLE start-0001]
- **Cancel** (`title` run): the dialog returns 0, and the attract cycle advances to its next phase at once. [ORACLE start-0001]
  - From the title, the credits logo shows in the first shot 0.5 s after Cancel (`title/003`, t 23.88), in both editions. "Directed By" / "Bill Stewart" follows by t 24.96, and "Programming By" by t 29.92.
  - Cancelling the Order dialog in the credits (unregistered, below) ends the credits the same way. Text page 1 shows 0.5 s later (`title/024`).
  - This matches shell.md: a zero result of commands other than 5 and 6 sets the phase deadline to now in phases below 4.

## New Game #321
- The X window sits at 364,302, 297×190. The title is no longer drawn behind it: the screen around the dialog is black. [ORACLE start-0001]

| Field | Unregistered | Registered |
|---|---|---|
| PILOT edit | "Your Name" | the INI's `ThePlayer` (the player name) |
| SHIP combo (selected) | TRANSFORMER | SKY TALON (the INI's `MainShip`) |
| SHIP list | 61 ships, AQUATIC BULLET … WINTER DUCK | 112 ships, AQUATIC BULLET … ZENMASTER |
| TEAM static | AVENGERS | AVENGERS |
| PLAY combo | ACTION (selected), COMMAND | same |
| DRIVE combo | "C: Unknown_HD" (selected), "D: Unknown_HD" | "C: Unknown_HD" (selected), "E: Unknown_HD"; the CD drive D: is not listed |
| ZOOM LEVEL combo | 2x, 1x (selected), 1/2x | same |
| Ship picture | the TRANSFORMER sprite, 96×96, black | the SKY TALON sprite |

- **Ship lists in order.** Each list is alphabetical. The dropped list shows 16 rows and opens scrolled so that the selection is visible (`newgame-ship-dropped`). [ORACLE start-0001]
  - Unregistered: AQUATIC BULLET, AQUATIC XFORM, BALD EAGLE, BAT OUT OF HELL, BIPLANE, BLASTER, BLUE SHARK, BRONZE SWORD, BULLET, CANADIAN, CLASSIC XFORM, COLD HEART, CORSAIR, COSMIC ROCKER, COSMIC TALON, COVERT FIGHTER, CUDDLY BEAR, DESERT EAGLE, DESTROYER, DOUBLEWING, DUCK, ECO TALON, EXCALIBUR, FIRE TALON, FLYING SAUCER, FRUIT BAT, GOLDEN VIPER, HEART, HEART OF GOLD, HENCHMAN, HORNET F18, IMPALA, INTREPID, KOOL KAT, LACKEY, MAPLE LEAF, MUSTANG, NEMESIS, NIGHT BULLET, NIGHT VIPER, PLATYPUS, POCKET ROCKET, REBEL, ROCKER, ROCKET, ROGUE, RUBBER DUCK, SCOUNDREL, SHARK, SIZZLING DUCK, SKY TALON, STEALTH, STEED, TEDDY BEAR, TIGER, TRANSFORMER, TROUBLE, UFO, VAMPIRE BAT, VIPER, WINTER DUCK.
  - The registered list adds ARCTIC ORCA, BEAVER, BERZERKER, BIRD OF PREY, BLUE CAVALIER, BRASS DOLPHIN, BUNNY, CAVALIER, CRAZY, DARK THUNDER, DEEP ENIGMA, DOLPHIN, DRAGON, DREAD PIRATE, EAGLE, ENIGMA, EVIL EYE, FALCON, FIREBIRD, FULCRUM, GOLDEN EAGLE, GREEN DRAGON, GRIM REAPER, HAWKEYE, HOT ORCA, HOT ROD, HOT SPECTRUM, ICE EAGLE, JAGUAR, KHAKI ENIGMA, KILLER WHALE, KOALA, LUNATIC, MAGIC BUNNY, METAL TARGA, MIDNIGHT DRAGON, MIG 29, NAVY DOLPHIN, NINJA, OCHRE TBIRD, PACIFIC TARGA, RED HOT JAG, SAMURAI, SCREAMING ROD, SPECTRAL FORCE, SPORTS CAR, STRIKE FALCON, THUNDERBIRD, WARBIRD, WILD CAT and ZENMASTER.
  - The registered list keeps every unregistered name. It is read from `newgame-ship-page01`…`08`, 15 rows apart, the last page partial.
- **Wheel and hover.** In the dropped SHIP list the selection follows the mouse pointer while the wheel scrolls under it. The pointer was over the items in `start-explore` (`g-after-esc.png`): after scrolling, ESC closed the list on the hovered SIZZLING DUCK, while the ship picture still showed TRANSFORMER. The scenario therefore wheels over the list's scroll bar (560,480). With the pointer there, the selection stays (`newgame-ship-closed`). [ORACLE start-0001]
- **OK** returns 1. The game writes `MainShip`, `Drive` (the letter only: `D` or `E`), `Size=1`, `SetSize=1`, `ThePlayer` (the edit text) and `PlayMode=0`, in that order, from ISPACE.DLL seg10:0x25a4–0x26cd. [ORACLE start-0001]

## Desk scene (New Game OK → Navigation window)
Shots every 0.5 s, back-to-back (0.06 s) from 17 s after OK. The times are seconds after the OK click.

| Phase | Unregistered (`main`) | Registered (`main`) |
|---|---|---|
| desk, chair and empty white monitor, fully drawn | first shot (+0.0) | same |
| green monitor typing starts | (+1.1, +1.7] | same |
| ship on the stool, rising | (+15.7, +16.3] | similar |
| ship leaves the stool and flies to the monitor | ≈ +18.8 to +19.4 | similar |
| last desk frame | +23.07 | +22.84 (`briefesc`) |
| black screen | not caught | +22.90 to +23.28 (`briefesc` shots 126–133) |
| "ANALYSING DRIVE X:UNKNOWN_HD..." in green | +23.13, partly drawn ("ANALYS") | +23.33 (`briefesc`), complete, "E:UNKNOWN HD..." |
| Navigation window | +23.20 (`main`), +23.21 (`briefesc`), +23.14 (`briefspace`) | +23.47, +23.37, +23.41 |

- **Monitor text** (trace, `LSTRCPY`/`LSTRCAT` arguments): "IT'S Sunday Night, Your Name!\r\nI HOPE YOU'RE READY TO PLAY..\r\n\r\nYOUR COMPUTER HAS BEEN INVADED!\r\n\r\nYOUR MISSION: ENTER INNER SPACE, RESTORE ORDER AS YOU SEE FIT....\r\n\r\nWHEN ALL ICONS HAVE BEEN CAPTURED OR DESTROYED, YOU CAN RETURN FROM INNER SPACE...\r\n \r\nPREPARE TO LAUNCH...\r\nGOOD LUCK, YOUR NAME! ". The screen shows it in capitals. [ORACLE start-0001]
  - The registered game uses the player name and adds nothing else.
  - The closing line comes from the template "%N %NPREPARE %<TO LAUNCH%>%<TO LUNCH%>...%N%<GOOD LUCK%>%<ENJOY YOUR %S%>, %s! ". All six runs chose TO LAUNCH and GOOD LUCK. "Sunday Night" is the weekday and the time of day at run time.
- **Analysing Drive** appears only after the desk is cleared to black. The text is drawn left to right, so a shot can catch it partly drawn (`main/139`, "ANALYS"). It stays until the Navigation window opens. The trace builds the string with `LSTRCAT("Analysing Drive ", "D:Unknown_HD")` + "..." and draws it with `STRETCHBLT` from the bitmap-font sheet. The Navigation window is created 1 ms after the string in the trace. Under the default relay the unregistered screen shows the text for less than 0.1 s, and the registered one for 0.1–0.5 s. [ORACLE start-0001]
- No input reaches the desk in these runs; for ESC on the desk see [newgame-0002](newgame-0002.md). [ORACLE start-0001]

## Navigation window → Enter
- The window sits at 34,60, 552×316, in both editions. On `D:` (`E:`) it lists the fixture tree as in newgame-0002. `games` is the row at y 153, and Enter is at 525,78 (`main/navigation`, `navigation-games`). [ORACLE start-0001]
- **Enter** writes `NavLeft=30`, `NavRight=560`, `NavTop=30` and `NavBottom=350`, then destroys `IS_NAV`. [ORACLE start-0001]
- **"LOADING GRAPHICS":** the trace loads strings 311 "GOOD", 645 "Welcome to Inner Space", 936 "Entering Drive ", 937 ":" and 899 "LOADING GRAPHICS" + "..." within 4 ms of Enter, and draws them as bitmap-font cells. [ORACLE start-0001]
  - **Unregistered:** every shot from 0.01 s to 2.2 s after Enter is black, with no pixel above 2 % grey (`main/143`–`146`, the same in `briefesc` and `briefspace`).
  - **Registered:** the shots from 0.01 s to 2.2 s after Enter show three green bitmap-font lines centred near the middle of the screen: "GOOD EVENING, " + the player name + "!", "WELCOME TO INNER SPACE" and "ENTERING DRIVE E:UNKNOWN HD".
  - "LOADING GRAPHICS..." shows in no shot of either edition.
- The briefing box appears 2.2 s after Enter in both editions (unregistered (+2.17, +2.21], registered (+2.19, +2.20]), which fits the 2000 ms busy-wait of seg10:0x0. [ORACLE start-0001]

## Briefing (state 3)
- **Box and labels:** as in newgame-0002. The title bar reads "WAVE 1 MISSION BRIEFING" and "PRESS 'ESC' TO START", and the status bar reads "D:\GAMES\" ("E:\GAMES\") and "3 ICONS". [ORACLE start-0001]
- **Lower panel:** yellow capitals type "ENTER INNER SPACE, CAPTURE OR DESTROY ALL ICONS BEFORE LEAVING THROUGH GATE. AVOID ROCKS AND CAPTURE GAS CANS IF FUEL RUNS LOW....." with a yellow block cursor. Typing runs from the box's appearance to (+4.4, +5.0] after it, in both editions. The cursor then blinks: the panel's lit pixel count alternates between 19 878 and 20 058. [ORACLE start-0001]
- **Upper panel:** four pages cycle in cyan. [ORACLE start-0001]
  1. **Icons.** The three fixture icons (red ALPHA.EXE, green BRAVO.EXE, blue CHARLIE.EXE, each with a dot row under it, labels in green) appear one by one. The text "IF YOU CAPTURE AN ICON, YOU GET ITS RESOURCES, WHICH CAN BE EXCHANGED FOR SERVICES IN THE AMBULANCE. CAPTURE ICONS BY FLYING OVER THEM WITH YOUR SHIP. DESTROY ICONS IF CAPTURE IS DIFFICULT, THE ICON HAS BEEN INFECTED BY A VIRUS, OR YOU REALLY DON'T LIKE THE ICON'S PROGRAM!" then types above them. Last, the icons are erased and the text stays.
  2. **Ambulance.** The S logo at the left. To its right types "ALL SHIPS CAN MAKE USE OF THE INNER SPACE AMBULANCE SERVICE. THE AMBULANCE REPAIRS YOUR SHIP AND PROVIDES NEW WEAPONS IN EXCHANGE FOR ICON RESOURCES. USE YOUR SHIP'S AUTO REPAIR SYSTEM TO AVOID THE COST OF AMBULANCE SERVICES." Then the logo is erased.
  3. **Reality check.** "REALITY CHECK." / "(this is you)" at the top left, the player's ship sprite in the middle and "YOU HAVE BEEN LOYAL TO ALLIES AND UNFRIENDLY TO ENEMIES." at the lower left. The right column types "WAVE 1", "D:\GAMES\", "SCORE : 0", "CAPTURED : 0 ICONS", "DESTROYED : 0 ICONS", "CREDIT : 1.50 ICONS", "RESOURCES : 0.0 ICONS" and "YOUR SHIP IS RESPECTABLE.".
  4. **Blank,** about 1–2 s.
- **Cycle timing:** the first page starts (+5.0, +5.5] after the box (unregistered (79.33 − 74.34), registered (83.48 − 78.50)). A cycle lasts about 25–27 s (blank at 102.19 → icons at 104.42 unregistered, 106.36 → 108.02 registered). [ORACLE start-0001]
- **The briefing does not end by itself.** The `main` runs shot it for 150 s, more than five cycles, in both editions. ESC then started the wave. [ORACLE start-0001]
- **ESC** (`briefesc`, 4.4 s after Enter, during the typing): the shot 0.3 s later already shows the wave, in both editions. [ORACLE start-0001]
- **Space** (`briefspace`, every 1.13 s from Enter on): [ORACLE start-0001]
  - The first Space came during the 2 s load. When the box appeared, the lower text was already complete (unregistered (56.20, 56.77], 19 878 pixels at once).
  - Each later Space advances the upper panel to the next page or step. Pages change at almost every shot instead of every 5–7 s.
  - Space never starts the wave: 40 s of Space left the briefing running until ESC. The two editions behave the same.

## Wave start (state 1)
- **ESC → first shot** (`wave-start`, 0.3 s): ship at the centre, a gate of two red posts with a cyan beam, rocks, stars, and the icon column at the top right with the three fixture icons. [ORACLE start-0001]
  - HUD: "CURRENT TARGET  (MANUAL)", "BRAVO.EXE", "Icon 1", the green BRAVO icon, the radar and "WEAPON SYSTEM".
  - The weapon line reads "20 - RADAR-GUIDED MISSILES" (unregistered, TRANSFORMER) or "20 - HEATSEEKER MISSILES" (registered, SKY TALON).
  - The clock appears at "0 01" by 1 s.
- The window list holds the main window "Inner Space" only. [ORACLE start-0001]

## Exit dialog #244 and the end
- **Alt+X in the wave:** `DIALOGBOX(#00f4)`, X window at 343,351, 339×93 (`exit-dialog`). [ORACLE start-0001]
  - It holds the GAME icon and "Are you sure you want to exit Inner Space?".
  - Its buttons are Save & Exit (407,421, enabled in the wave), Exit (511,421) and Cancel (617,421, default).
  - The init disables button 593 and enables it again (`ENABLEWINDOW(…, 0)` then `(…, 1)`, ISPACE.DLL seg10:0x11c2 and seg10:0x11fd).
  - The editions do not differ.
- **At the title** (`title-exit-dialog`): the same text, with Save & Exit greyed (only `ENABLEWINDOW(…, 0)`). Clicking it does nothing: no Save Game dialog opens, and the dialog stays. [ORACLE start-0001]
- **Cancel** returns 0. The wave goes on, and the clock keeps counting. [ORACLE start-0001]
- **Exit** returns 1. [ORACLE start-0001]
  - **Unregistered:** Registration Benefits #799 (`#031f`) opens at 304,190, 416×414. It holds the GAME icon and the bold text "We hope you enjoy your trial version of Inner Space. We worked hard to create it. Here are some of the benefits of ordering the game. Press 'R' to order.". The edit holds the #5000 330 text, which starts "EXPERIENCE THE REAL INNER SPACE". The buttons are Exit (567,582, default) and Order.. (666,582).
    - Its Exit returns 0, and the main window is destroyed at once (INSPACE.EXE seg2:0x0e49). The game is gone within 2 s.
    - Its Order.. returns 1 and opens Order #693 (`#02b5`, return ISPACE.DLL seg10:0x4351) directly. Cancel there returns 0 and destroys the main window (`title` run).
  - **Registered:** no #799, no #1872. The main window is destroyed 2 s after the Exit click, and the trace has no `PlusCount` read-back write. The exit writes are `Full=1`, `Menu=0`, `IconWnd=1`, `VScale=0`, `SmWnd=0` and `SScale=0`, as in the unregistered game.

## 'R' and the Order dialog #693
- **Unregistered** (`title` run, R pressed in the credits): `DIALOGBOX(#02b5)` at 286,206, 453×382, as in [dialogs-0001](dialogs-0001.md). [ORACLE start-0001]
  - It reads "You are on day 12088 of your demo trial period." The serial number differs per prefix.
  - The buttons are More Info.. (690,232), Order Form.. (690,406), Register (690,537) and Cancel (690,567). The Registration # edit is at 518,570.
  - **Register with an empty code:** the button is greyed and the click does nothing.
  - **Typing** enables Register: one `ENABLEWINDOW(Register, 1)` per typed character, from ISPACE.DLL seg10:0x077d.
  - **Register with "123":** `MessageBox(owner, "Sorry! Incorrect registration number.", "Inner Space", MB_ICONHAND)`, an X window at 383,356, 257×82 (`order-register-wrong`). Return closes it, and the Order dialog stays.
  - **Order Form..:** `WinExec("NOTEPAD C:\INSPACE\ORDER.TXT", SW_SHOW)`. Notepad opens at 4,30, 721×512, and the Order dialog stays.
  - **More Info..:** `WinExec("NOTEPAD C:\INSPACE\README.TXT", SW_SHOW)`, the same way. The path is `Path` + the file name.
  - **Cancel** returns 0. In the credits, the credits then end at once and text page 1 shows (`title/024`).
- **Registered** (`title/015`, R in the credits): no dialog, no `DIALOGBOX` call, and the credits go on ("Programming By" → "Music Composed By" → "Voice Effects"). [ORACLE start-0001]

## INI writes and files
In order, per run; values identical in both editions except where shown. [ORACLE start-0001]

| When | Keys |
|---|---|
| start-up (t ≈ 1.9) | `Calibrate` (unregistered 27850–29334, registered 29037–29306 in these runs) |
| Restore Game OK | `Restore= NEW GAME` |
| New Game OK | `MainShip` (TRANSFORMER / SKY TALON), `Drive` (D / E), `Size=1`, `SetSize=1`, `ThePlayer` ("Your Name" / the player name), `PlayMode=0` |
| Navigation Enter | `NavLeft=30`, `NavRight=560`, `NavTop=30`, `NavBottom=350` |
| exit (after #799 in the unregistered game) | `Full=1`, `Menu=0`, `IconWnd=1`, `VScale=0`, `SmWnd=0`, `SScale=0` |

- The `title` runs write only `Calibrate` and the exit keys. [ORACLE start-0001]
- **Files:** every run writes only `C:\WINDOWS\INSPACE.INI` (`files.txt`), in both editions. Nothing is written under the game directory or the fixture drive. [ORACLE start-0001]
- `Calibrate` is positive on this host in every run here (27 850–29 334). In intro-0001 it wrapped negative. The value varies between runs. [ORACLE start-0001]

## Differences between the editions
- **Title:** gold text, "inner space™", "Plus Edition", and the "Reg User: <registered name> - <organisation>" line in place of the trial line and the R box. [ORACLE start-0001]
- **R** does nothing in the registered game. [ORACLE start-0001]
- **New Game:** PILOT and SHIP come from the INI. The SHIP list holds 112 ships instead of 61. [ORACLE start-0001]
- **Enter:** the registered game shows the green "GOOD EVENING, <name>! / WELCOME TO INNER SPACE / ENTERING DRIVE E:UNKNOWN HD" screen for the 2 s load. The unregistered screen stays black. [ORACLE start-0001]
- **Wave:** the weapon line follows the ship. [ORACLE start-0001]
- **Exit** goes straight to the end in the registered game. The unregistered game shows #799 first. [ORACLE start-0001]
- The registered timings run 0.2–0.3 s longer from OK to the Navigation window (23.37–23.47 s against 23.14–23.21 s). That install has `SoundSystem=3`. [ORACLE start-0001]
- Restore Game, the PLAY, DRIVE and ZOOM lists, the desk text, the briefing pages and their timing, ESC and Space in the briefing, and #244 are the same. [ORACLE start-0001]

## What did not work
- **"LOADING GRAPHICS..."** shows in no shot. In the unregistered game the whole 2 s after Enter is black, although the trace loads and lays out the same strings as the registered game. Why the unregistered screen stays black is [OPEN].
- **Analysing Drive** is visible in the unregistered game for less than one shot interval (0.06 s). Its full text there is known only from the trace.
- **First registered attempt** (`work/oracle/start-reg-main.out`): the drive pick clicked the third row (y 499), which does not exist, because the registered DRIVE list holds only C: and E:. The game analysed `C:\` and opened the Navigation window on the prefix's C: tree. Enter on the selected `prog~fp2` folder (status line "No apps in this directory. Select another directory to enter.") did nothing. ESC and Alt+X then reached no dialog while the Navigation window had the focus. The scenario now picks the second row.
- **Briefing end:** the brief expected the briefing to end by itself. It did not within 150 s, in either edition. Whether it ever ends without input is [OPEN].
- `oracle/tests/test_scripts.py` does not list `scenarios/start.sh`, so pytest does not check its syntax. `bash -n oracle/scenarios/start.sh` passes by hand.
