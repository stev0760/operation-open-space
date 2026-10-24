# Wine bug candidate: new WoW64 loses a Win16 caller's DS after `fix_selector`

Status: **not filed.** Reproduce on a second machine and on the latest Wine development release first (checklist at the end).

This page is self-contained so it can travel with a `git bundle` of this repo. The full analysis, with evidence, is [fault-0001](../../oracle/digests/fault-0001.md); the runtime comparison is [runtime-0001](../../oracle/digests/runtime-0001.md).

## Summary
On a new-WoW64 Wine build, a Win16 program loses its DS when a 16-bit API returns with a freed selector in a saved segment register. The relay return path pops the stale ES and faults. `fix_selector` zeroes the stacked ES and resumes through `NtContinue`. The WoW64 resume then reloads DS (and ES) from SS. The program continues with DS = SS, which is wrong whenever DS ≠ SS, for example in a DLL running on its caller's stack. Old-WoW64 Wine resumes with the correct DS, and the program runs. [ORACLE fault-0001] [ORACLE runtime-0001]

Trigger in Operation: Inner Space v1.29.08: ISPACE.DLL frees a 276×276-byte block (two tiled selectors) while ES still holds its second selector. It then runs on INSPACE.EXE's DGROUP and faults about 14 s into the intro: `Unhandled page fault on read access to FFFFFFFF at address 00006480`. [ORACLE fault-0001]

## Affected and unaffected builds
| Build | Result |
|---|---|
| Wine 11.0, Kron4ek `amd64-wow64` (new WoW64) | faults, every run [ORACLE fault-0001] |
| Same, with `krnl386-clear-saved-selectors.patch` | no fault, intro to wave 1 [ORACLE fault-0001] |
| Wine 11.0, Kron4ek `amd64` (old WoW64) | no fault, intro to wave 1 [ORACLE runtime-0001] |
| otvdm 0.9.0 inside Wine 11.0 new WoW64 | no fault (own 16-bit layer) [ORACLE runtime-0001] |
| 32-bit Wine | untested; `signal_i386.c` saves and restores DS through the CONTEXT, so expected unaffected [REF Wine 11.0 dlls/ntdll/unix/signal_i386.c] |
| Wine 11.x development releases | untested [OPEN] |

Tell the two WoW64 modes apart by the install tree: an old-WoW64 build has `lib/wine/i386-unix/`; a new-WoW64 build has only `i386-windows`, `x86_64-unix` and `x86_64-windows`.

## Mechanism
Wine 11.0 source paths; the analysis was made against tag `wine-11.0`.
1. The game accesses a global block through ES, then calls `GlobalFree` on it with ES still holding one of its selectors. Win16 does not preserve ES across API calls, so this is harmless on Windows. [REF The Old New Thing, "The history of calling conventions, part 1", 2004-01-02]
2. `GlobalFree16` → `GLOBAL_FreeBlock` → `SELECTOR_FreeBlock` → `free_entries` clears the LDT entries and leaves the caller's saved segment registers alone. [REF Wine 11.0 dlls/krnl386.exe16/global.c, selector.c]
3. The 16-bit return path pops DS, then ES, from the STACK16FRAME. `popw %es` faults on the freed selector. [REF Wine 11.0 tools/winebuild/relay.c BuildCallFrom16Core]
4. `call16_handler` → `fix_selector` sets the stacked ES slot to 0 and returns `ExceptionContinueExecution`. [REF Wine 11.0 dlls/krnl386.exe16/wowthunk.c]
5. New WoW64 does not carry the 16-bit DS through that round trip. [REF Wine 11.0 dlls/ntdll/unix/signal_x86_64.c, dlls/wow64cpu/cpu.c]
   - `get_thread_wow64_context` reports DS = ES = `ds64_sel` (0x2b); `set_thread_wow64_context` forces DS/ES to `ds64_sel` and sets `WOW64_CPURESERVED_FLAG_RESET_STATE`.
   - The full-state resume `.Lsyscall_32to64_return` loads DS and ES from `context->SegSs`.
   - The handler sees `ds=002b es=002b ss=03a7` at every such fault. [ORACLE fault-0001]
6. The `popw %es` re-executes and loads 0, and DS is now SS. Every later DS-relative access in the DLL reads the caller's DGROUP. [ORACLE fault-0001]

The same faults happen on old WoW64 (19 `fix_selector` events at the same sites), but the handler sees and restores the right DS (`ds=0427`). So the difference lies in the new-WoW64 context round trip, not in krnl386. [ORACLE fault-0001]

A related symptom: in about 1 run in 5, the game exits silently. The resume leaves a stale 16-bit `SegSs` in the saved WoW64 context. A later full-state resume then runs flat 32-bit code on the 16-bit stack, and `setup_raise_exception` faults inside the signal handler. [ORACLE fault-0001]

## What to bring to the other machine
- **This repo:** `git bundle create innerspace.bundle --all` here, then `git clone innerspace.bundle innerspace-port` there.
- **The original files:** `original/archives/Shipdisk.zip` (sha256 `3e560aa7…`, full value in `original.sha256`). Extract it to `original/shipdisk/` and check it with `sha256sum -c --ignore-missing original.sha256`. THEWORKS is not needed for this bug.
- **Python 3.9+ venv:** `python3 -m venv re/.venv && re/.venv/bin/pip install pytest capstone && re/.venv/bin/pip install -e re`. The layout script needs `isre` for the SZDD-compressed files.
- **Wine builds:** `oracle/wine/install_kron4ek.sh amd64-wow64 /opt/wine-11.0` (affected) and `oracle/wine/install_kron4ek.sh amd64 /opt/wine-11.0-oldwow` (control). Both check the release sha256. Old WoW64 needs the host's 32-bit X11, font and sound libraries; the script header lists the RHEL 9 package names.
- **Headless runs only:** Xvfb, xdotool, ImageMagick, and `i686-w64-mingw32-gcc` (the fixture drive's test programs).

## Reproduce interactively (desktop session)
```
export WINEPREFIX=$HOME/.wine-inspace-bug WINEDLLOVERRIDES=ctl3d=n
/opt/wine-11.0/bin/wineboot -i
re/.venv/bin/python -I oracle/install_layout.py original/shipdisk "$WINEPREFIX" --with-ini
cd "$WINEPREFIX/drive_c/INSPACE"
WINEDEBUG=+thunk /opt/wine-11.0/bin/wine INSPACE.EXE 2>&1 | tee ~/inspace-bug.log
```
1. Wait for the title screen ("OPERATION: limited inner space"), then press ESC. The Restore Game dialog opens on "NEW GAME, STARTING AT WAVE 1." Click OK.
2. In the New Game dialog, keep the defaults (ship TRANSFORMER, any drive) and click OK. The desk-scene intro starts typing.
3. **Expected on new WoW64:** about 14 s later, `wine: Unhandled page fault on read access to FFFFFFFF at address 00006480`. The log holds `fix_selector fixing up selector … for pop instruction` lines before it.
4. **Control:** repeat with `/opt/wine-11.0-oldwow/bin/wine` in a fresh prefix. The intro types through "PREPARE TO LAUNCH… GOOD LUCK, <NAME>!", and the game opens the Inner Space Navigation window by itself.

## Reproduce headless (scripted)
From the repo root (it uses its own prefixes and Xvfb displays from `:110`; see `oracle/env.sh`):
```
# affected: faults in the intro
ORACLE_WINE=/opt/wine-11.0 ORACLE_ID=3 SKIP_INTRO=0 \
  ORACLE_TRACE_DEBUG=+relay,+int21,+timestamp,+thunk,+seh,+global,+selector \
  oracle/scenarios/newgame.sh work/bug/stock
# control: runs into the Navigation window and wave 1
ORACLE_WINE=/opt/wine-11.0-oldwow ORACLE_ID=3 SKIP_INTRO=0 oracle/scenarios/newgame.sh work/bug/oldwow
```
Each run leaves screenshots, `trace.log` and `steps.txt` in its output directory. A crash appends winedbg's register dump and backtrace to `trace.log`.

## Signals to check in a trace
- `grep -a 'Unhandled page fault' trace.log`: the fault at `00006480` (intro) or `00006FBC` (Enter in the Navigation window).
- `grep -ac 'fixing up selector' trace.log`: 18–19 per run on both WoW64 modes (needs `+thunk`). The count alone does not separate them.
- The selector map from `re/.venv/bin/python -I oracle/relay_digest.py trace.log` names each module's segment selectors and DGROUP. On this host INSPACE.EXE's DGROUP is `03a7` and ISPACE.DLL's is `0427`, with ISPACE code selectors `03c7`–`041f`. Another machine can assign different values.
- The decisive sign: relay `Call` lines whose `ret=` lies in ISPACE.DLL code but whose `ds=` is INSPACE's DGROUP. On this host a new-WoW64 run has dozens (58 in the intro run of fault-0001) and an old-WoW64 run has none. The first appear right after a `GLOBALFREE` return:
  ```
  Call KERNEL.17: GLOBALFREE(0536) ret=0407:4ce8 ds=0427
  …
  Call KERNEL.18: GLOBALLOCK(0000) ret=03ff:6444 ds=03a7
  ```
  The code at ISPACE.DLL seg7:0x4ce8 does not load DS between those calls. [BYTES ISPACE.DLL seg7:0x4ce3]

## Workaround patch
`oracle/wine/krnl386-clear-saved-selectors.patch` (29 lines, `dlls/krnl386.exe16/selector.c`). Before freeing LDT entries, `free_entries` walks the STACK16FRAME chain. It zeroes saved DS, ES, FS and GS in the current frame, and saved ES, FS and GS in outer frames, wherever they hold a freed selector. The relay return then never pops a freed selector. This matches the DPMI 1.0 rule that freeing a selector zeroes segment registers holding it. [REF DPMI 1.0 spec, Int 31h function 0001h] Build it with `oracle/wine/build_patched.sh /opt/wine-11.0 /opt/wine-11.0-patched`.

The patch avoids the trigger; it does not fix the loss of DS. The same loss follows any other 16-bit exception resumed through `NtContinue`: Wine's emulated `int 21h`, `int 2Fh` and `int 31h` paths, for example. A proper fix probably belongs in wow64cpu/ntdll: preserve DS/ES across an exception resume when SS is a 16-bit selector. [OPEN]

## Before filing
- [ ] Reproduce on the primary workstation with the stock new-WoW64 build, interactively.
- [ ] Repeat with the latest Wine development release (11.20 segfaults in `wineboot` on this VM for an unrelated reason) and, if available, a distribution's new-WoW64 package.
- [ ] Confirm the control (old WoW64) passes on that machine.
- [ ] Decide on a test case. Upstream prefers a minimal program to a commercial game: a small Win16 executable that frees a selector held in ES inside a DLL running on its caller's stack would show the bug without the game files [OPEN].
- [ ] Search WineHQ Bugzilla for existing reports (keywords: WoW64, Win16, fix_selector, DS).
