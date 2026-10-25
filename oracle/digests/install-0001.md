# install-0001: the original installer under the oracle Wine, and the registered game

- **Date:** 2026-10-25. **Host:** Arch Linux, Xvfb 1024×768×24 on `:119`.
- **Runtime:** the default `~/opt/wine-11.0-oldwow-patched` (Wine 11.0, old WoW64), fresh prefix `innerspace9` (`ORACLE_ID=9`), no `Z:`, `WINEDLLOVERRIDES=ctl3d=n`, relay trace with the default profile.
- **Install files:** the user's purchased download `thegame.zip` (25 files, from the author's customer page). Every file is byte-identical to `original/shipdisk` except README.TXT, whose copyright year reads 2010. The purchased copy is therefore the v1.29.08 shipdisk build; its registration happens at install time, below.
- **Reproduce:** `ORACLE_ID=9 NAME=<name> ORG=<org> oracle/scenarios/install.sh`. It copies the install files to `C:\temp`, runs `INSTALL.EXE` through its dialogs with `xdotool`, stops it, then starts the installed game from `C:\SPACE` and shoots its title. Output in `work/oracle/install/`: `01-warning.png` … `08-readme.png`, `09-title.png`, `steps.txt`, `trace.log` (installer), `game-trace.log`, `INSPACE.INI`, `system.ini`, `files.txt`.
- **Bulk output (gitignored):** `work/oracle/install/`, and the first traced runs in `work/registered/install-try/`. The registration values themselves stay out of git (`work/registered/NOTES.md`).

## The installer runs
- `INSTALL.EXE` completes under the old-WoW64 runtime. The page fault that [smoke-0001](smoke-0001.md) records was under the new-WoW64 runtime. [ORACLE install-0001] [ORACLE smoke-0001]
- The installer busy-waits on `GetCurrentTime` between its screens. An unfiltered `+relay` makes it crawl (the first attempt wrote 700 MB of relay lines in a minute without reaching its first dialog); the default relay profile excludes that call and the installer runs at normal speed. [ORACLE install-0001]
- `winevdm` reports repeated handled `EXCEPTION_ACCESS_VIOLATION`s at the same krnl386 address during the splash. They are Wine's own selector faults, caught by its handler; nothing is lost. [ORACLE install-0001]

## Dialog sequence (window names as `xdotool` lists them)
| Step | Window | Content | Answer given |
|---|---|---|---|
| 1 | `IS Install` message box | "Make sure any running programs, (especially email programs) are exited before continuing." | OK |
| 2 | `IS Install` message box, not always shown | "Inner Space has been installed before. Should old preferences be kept? (Answer No if installing to fix a problem)" | No |
| 3 | splash | "OPERATION: inner space™ PLUS EDITION", "V1.29.08 INSTALLER", "VALID INSTALL DISK, READY FOR NEW INSTALL", the spinning S | |
| 4 | `Inner Space New Valid Install` | directory `C:\SPACE`; components Inner Space 771 KB, Ship Factory 37 KB, Sound Effects 316 KB, Voice Effects 336 KB, Music Soundtrack 239 KB, Screen Savers 21 KB; required 1766 KB | OK |
| 5 | `IS Install` message box | "Do you wish Inner Space to become the active screen saver?" | No |
| 6 | `Game Difficulty and Speed` | Gentle / Easier / Normal / Harder / Extreme!; Slow 20, Normal 36, Fast 54, Red Hot! 72 frames/second | OK (Normal, Normal) |
| 7 | `Play Full Screen or in a Window` | Full Screen / Resizeable Window | OK (Full Screen) |
| 8 | ` Inner Space` (leading space) | "Please personalize your copy of Inner Space:" Name, Organization, OK | name and organization, OK (Alt+O, accepted only after a few seconds) |
| 9 | `README.TXT - Notepad` | the installer opens README.TXT and exits | |

Step 2 appeared in two of four fresh prefixes; what the installer tests for is [OPEN]. Each `IS Install` box is a second window with that name next to the splash. [ORACLE install-0001]

## What the installer writes
- **Files** (LZEXPAND, the SZDD sources expand): `C:\SPACE`: INSPACE.EXE, ISPACE.DLL, ENG_LANG.DLL, IGRAPHIC.DLL, FACTORY.EXE, ENG_HELP.HLP, README.TXT, ORDER.TXT and BACKUP.EXE, a byte-for-byte copy of INSTALL.EXE. `C:\SPACE\SHIPS`: MASTER.BOD, MASTER.SHP. `C:\SPACE\SOUNDS`: MAIN.MID, RACE.MID, DEMON.MID, ANTHEM.MID, ENG_FX.DLL, SND_FX.DLL. `C:\windows`: INSPACE.INI (with INSPACE.BAK), INSPACE.SCR, INRUN.AD, WAVEMIX.DLL, WAVEMIX.INI, CTL3D.DLL. Empty `C:\SPACE\GAMES` and `C:\SPACE\RETIRED`. SECURE.DAT is created empty in the source directory and copied nowhere. [ORACLE install-0001]
- **INSPACE.INI [Inner Space]**, in this order: `LocalPath`, `Path`, `ShipPath`, `SndPath` (first to the Windows directory and the temp directory, then to `C:\SPACE\`, `C:\SPACE\SHIPS\`, `C:\SPACE\SOUNDS\`), `Games=GAMES`, `RacePath`, `Dead=RETIRED`, `SoundSystem=3`, `Complex=2` and `FrameRateSet=36` (the difficulty dialog), `Full=1` (the window dialog), `ThePlayer=<name>` and `Detail=3` (after personalization). SYSTEM.INI `[drivers] MSACM.imaadpcm=imaadpcm.acm`. [ORACLE install-0001]
- **Registration, after the personalization dialog.** The writes come from ISPACE.DLL (the relay shows its data segment), as [formats.md](../../docs/re/formats.md#resources-rewritten-in-place) describes from the bytes:
  1. It opens `C:\SPACE\INSPACE.EXE` for writing, walks its NE resource table, writes 10 bytes into `#5500 110` (file offset `0x4b840`) and 50 bytes into `#5000 143` (file offset `0x4b860`, over "Unregistered Copy   ILLEGAL"), and restores the file's DOS date and time (`int 21h` 5700h/5701h). No other game file changes: ISPACE.DLL, ENG_LANG.DLL, IGRAPHIC.DLL and FACTORY.EXE stay identical to the shipdisk. [ORACLE install-0001] [BYTES ISPACE.DLL seg10:0x4c68]
  2. It writes SYSTEM.INI `[386Enh] vidsp` and `32BitSP` and WIN.INI `[windows] PDev` (Wine keeps WIN.INI `[windows]` in the registry; the game reads it with `GetProfileInt` all the same). The values differ between two installs on the same day, so they carry a random component. [ORACLE install-0001]
  3. Earlier, during the splash, ISPACE.DLL reads `C:\temp\INSTALL.EXE` itself twice ("VALID INSTALL DISK"). [ORACLE install-0001]
- An install stopped before the personalization dialog (prefix `innerspace7`, first attempt) leaves the game unregistered: it starts as "limited inner space™ / Standard Edition" with the trial-day line and the Press 'R' box. [ORACLE install-0001]

## The registered game
- Start-up reads WIN.INI `[Inner Space] CDDrive` (empty), SYSTEM.INI `vidsp` and `32BitSP`, and WIN.INI `PDev`, then the same keys again at the state-0 entry and at the title, as [shell.md](../../docs/re/shell.md#intro-and-title-screen) describes. [ORACLE install-0001]
- Title (`09-title.png`): "OPERATION:", "inner space™", " Plus Edition - V1.29.08 Aug 9/2000", the 256-colour hint, the copyright, then "Reg User: <name> - <org>" in place of the trial-day line; no Press 'R' box; "ESC to play - Alt-X to Exit". The main window is created with the title "Inner Space". [ORACLE install-0001]
- The intro and title layout is otherwise the one of [intro-0001](intro-0001.md).

## Open
- The layouts of the 10-byte `#5500 110` record, the 50-byte `#5000 143` record and the three INI values, and how the game validates them (seg10:0x885e, ordinal 24). Neither trace shows an `int 21h` 440Dh media-ID call, so under Wine the values do not visibly bind to a drive serial. [OPEN]
- What makes the installer ask "installed before" in a fresh prefix. [OPEN]
