# ADR 0004: INI location

- **Status:** Accepted. [DESIGN]
- **Scope:** where the port keeps `INSPACE.INI`, how it resolves the path keys, and which keys it writes. The meaning of each key stays in [formats.md](../re/formats.md#ini-files). [DESIGN]

## Context
- **The original names `INSPACE.INI` without a path,** so the profile functions use the file in the Windows directory. INSPACE.EXE, ISPACE.DLL and both Factory builds read and write that one file ([formats.md](../re/formats.md#files-and-sections)). [BYTES seg6:0x6ef] [BYTES ISPACE.DLL seg9:0x6e] [REF Win16 SDK GetPrivateProfileString]
- **The game writes the file while it runs.** Each launch rewrites `Calibrate`. The speed keys and the Options dialog write `FrameRateSet`. New Game OK writes `Restore`, `Drive`, `MainShip` and `PlayMode`. Enter in Navigation writes `NavLeft` to `NavBottom`, and exit writes the window positions ([formats.md](../re/formats.md#inspaceini-inner-space), [timing.md](../re/timing.md#rate-settings)). [BYTES seg6:0xb31] [BYTES seg10:0x7f0b] [BYTES seg6:0x33e] [ORACLE newgame-0001] [ORACLE wave-0001]
- **An empty `Path` regenerates the file** from the program's directory: `Path` and `LocalPath` become that directory, `ShipPath`, `SndPath` and `RacePath` its `SHIPS\`, `SOUNDS\` and `GAMES\` subdirectories ([formats.md](../re/formats.md#files-and-sections)). [BYTES seg6:0x7e9] [ORACLE smoke-0001]
- **The Windows directory is not writable on Windows 10 and 11** without elevation. A legacy process that writes there is redirected to `%LOCALAPPDATA%\VirtualStore`, and a 64-bit process is not redirected at all. [REF Win32 SDK, Registry and File Virtualization]
- **The asset directory can be read-only** (ADR 0002): a distribution directory, or a game directory the user keeps pristine. A flat distribution has no `SOUNDS\` or `SHIPS\` subdirectory ([PLAN.md](../PLAN.md#m1--foundations-detailed-plan), "Carried into M2"). [DESIGN]
- **The INI is cp1252** (ADR 0003, decision 4). [DESIGN]

## Decision
1. **One file, the original's layout.** The port keeps one `INSPACE.INI` with the original's section, key names, value formats and defaults. The settings code and the Options dialogs then port unchanged, and formats.md stays the reference for every key. [DESIGN]
2. **Location:** `%APPDATA%\Inner Space\INSPACE.INI` (`FOLDERID_RoamingAppData`). Under Wine that is inside the prefix. [DESIGN]
   - **Portable mode:** when an `INSPACE.INI` exists in the directory that holds the port's executable, the port uses that file instead. [DESIGN]
     - Portable mode is off when the executable's directory is the asset directory (ADR 0002 decision 1): the `INSPACE.INI` there is the game's own file, which the port never writes (ADR 0002 decision 5). The game contains such a file. [DESIGN]
   - **Override:** the command-line option `--ini <path>` names the file to use and wins over both, unless the file lies in the asset directory, which ADR 0002 decision 5 keeps read-only; such an override is refused with a log line and the usual rule applies. [DESIGN]
3. **First run.** When the chosen file does not exist and `<Windows directory>\INSPACE.INI` does, the port copies it first, so a prefix or machine that ran the original keeps its settings. Then the original's rule applies: an empty `Path` regenerates the keys, with the same message. [DESIGN] [BYTES seg6:0x7e9]
4. **Path keys resolve against the asset directory,** not the executable's directory. Regeneration sets `Path` to the asset directory (ADR 0002), `ShipPath` to its `SHIPS\`, `SndPath` to its `SOUNDS\` and `RacePath` to its `GAMES\`. When a named subdirectory does not exist, the asset store reads the ships or sounds from the asset directory itself, which covers a flat distribution. [DESIGN]
5. **`LocalPath` defaults to the INI's own directory,** so saved games (`LocalPath` + `Games`) and retired games (`LocalPath` + `Dead`) land under `%APPDATA%\Inner Space\` by default, and the asset directory stays read-only unless the user points `LocalPath` at it. ADR 0002 decision 5 names the only writes into the game directory. [DESIGN]
6. **The port writes the same keys at the same events** as the original. `Calibrate` receives the constant of ADR 0001, decision 9. A Factory build that reads the same file then sees a valid value. [DESIGN]
7. **No `GRMSETUP.EXE`.** Where the original starts `GRMSETUP.EXE` when the INI is missing, the port regenerates the file instead ([formats.md](../re/formats.md#files-and-sections)). [DESIGN] [BYTES seg6:0x791]

8. **Atomic saves.** The port writes the whole file at every save (there is no per-key profile write into a port-owned path), so it writes a temporary file beside the INI and replaces the INI with `MoveFileExW(MOVEFILE_REPLACE_EXISTING)`, and a crash mid-write leaves the previous file intact. [DESIGN] [REF Win32 SDK, MoveFileEx]

## Alternatives
- **The Windows directory, as the original:** needs elevation or relies on virtualization, which a 64-bit process does not get. [DESIGN]
- **The registry:** the Factory and the original read the INI, so a user who runs them under Wine would lose shared settings. formats.md would also stop describing the port's store. [DESIGN]
- **Always next to the executable:** fails under `Program Files`. Kept only as the portable mode. [DESIGN]
- **`%LOCALAPPDATA%`:** the file holds no machine-specific value once `Calibrate` is a constant, so the roaming folder is the conventional place. [DESIGN]

## Consequences
- M2 implements a settings layer over `GetPrivateProfileStringW` and `WritePrivateProfileStringW` with an explicit path, and creates the file itself so it stays ANSI (cp1252). [DESIGN]
- The asset store takes the resolved `ShipPath` and `SndPath` (its `set_sound_dir`), with the fallback of decision 4. [DESIGN]
- Checklist rows SYS-09 and SYS-33 describe the behaviour this ADR ports. [DESIGN]
