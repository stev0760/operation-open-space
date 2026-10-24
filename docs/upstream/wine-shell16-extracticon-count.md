# Wine bug candidate: Win16 `ExtractIcon(…, 0xFFFF)` returns 0 instead of the icon count

Status: **not filed.** Reproduce on a second machine and on the latest Wine development release first. The same transfer steps as [the DS-loss page](wine-wow64-ds-loss.md) apply.

## Summary
In Win16, `ExtractIcon(hInst, lpszExeFileName, nIconIndex)` with `nIconIndex` = -1 (0xFFFF, since UINT is 16-bit) returns the number of icons in the file. [REF Win16 SDK ExtractIcon] Wine's 16-bit `ExtractIcon16` declares the index as `UINT16` and passes it zero-extended (65535) to the 32-bit `ExtractIconA`/`ExtractIconW`. Those count icons only for `(UINT)-1`, so the call returns NULL and the 16-bit function returns 0. [REF Wine 11.0 dlls/shell.dll16/shell.c, dlls/shell32/shell32_main.c] Both WoW64 modes are affected; the module is the same file in both Kron4ek 11.0 builds. [ORACLE wave-0001]

## Effect in Operation: Inner Space
The game counts each program's icons with the 0xFFFF query when the player selects a directory in the Navigation window. [ORACLE newgame-0001] Under stock Wine it gets 0 for every program:
- the status line under-counts ("There are 2 icons in 2 files." for a directory whose programs hold 4 icons);
- the wave briefing, the icon column and the target display draw a generic green "D:>" box instead of each program's own icon.
With the patch, all 32 counts in the scripted scenario match otvdm (1 per test program, 3 for a program with three icons), and the game draws the programs' icons. [ORACLE wave-0001] [ORACLE runtime-0001]

## Reproduce
- Interactive: set up the game as on the DS-loss page, but with an old-WoW64 build (so the intro does not fault). Choose a drive in New Game, open a directory of programs with known icon counts in the Navigation window, and compare the status line with the real counts.
- Relay check: with `WINEDEBUG=+relay`, the lines `Call SHELL.34: EXTRACTICON(…,ffff)` are followed by `Ret SHELL.34: EXTRACTICON() retval=0000`.
- Scripted: `ORACLE_WINE=/opt/wine-11.0-oldwow ORACLE_ID=1 oracle/scenarios/newgame.sh work/bug/icons`, then `grep -a 'SHELL.34' work/bug/icons/trace.log | grep -a Ret`. Repeat with `ORACLE_WINE=/opt/wine-11.0-oldwow-patched` for the corrected counts.

## Patch
`oracle/wine/shell16-extracticon-count.patch` (4 added lines in `dlls/shell.dll16/shell.c`): when the index is 0xFFFF, `ExtractIcon16` calls the 32-bit function with `~0u` and returns its count without icon conversion. Build: `oracle/wine/build_patched.sh /opt/wine-11.0-oldwow /opt/wine-11.0-oldwow-patched shell16-extracticon-count.patch`. Compiled stock and patched, only `_ExtractIcon16@12` differs. [ORACLE wave-0001]

## Before filing
- [ ] Reproduce on the primary workstation; confirm against the latest development release.
- [ ] A minimal Win16 test program (call `ExtractIcon(hInst, "x.exe", (UINT)-1)` on a file with a known icon count) would show it without the game [OPEN].
- [ ] Check whether other Win16 shell entry points with the same index semantics (`ExtractAssociatedIcon`, `ExtractIconEx` thunks) share the bug.
- [ ] Search WineHQ Bugzilla (keywords: ExtractIcon16, shell.dll16, icon count).
