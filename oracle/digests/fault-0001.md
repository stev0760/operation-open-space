# fault-0001: ISPACE.DLL loses its DS after GlobalFree under new-WoW64 Wine

- **Date:** 2026-10-24. **Host:** Wine 11.0 (Kron4ek `amd64-wow64`, new WoW64), Xvfb 1024×768×24, prefixes `innerspace3` and `innerspace5`, displays `:113` and `:115`.
- **Setup:** the same as [newgame-0001](newgame-0001.md): `--with-ini`, fixture `basic` as `D:`, no `Z:`.
- **Reproduce the fault:** `ORACLE_WINE=/opt/wine-11.0 ORACLE_ID=3 SKIP_INTRO=0 ORACLE_TRACE_DEBUG=+relay,+int21,+timestamp,+thunk,+seh,+global,+selector oracle/scenarios/newgame.sh work/fault/confirm-intro`.
- **Reproduce the fix:** `ORACLE_WINE=/opt/wine-11.0-patched ORACLE_ID=3 SKIP_INTRO=0 oracle/scenarios/newgame.sh work/fault/patched-intro`, then the same with `SKIP_INTRO=1` (output `work/fault/patched-skip`). See "Fix for the oracle" for how the patched copy is built.
- **Bulk output (gitignored):** `work/fault/`: traces and screenshots of each run, the patch, the build script, and the analysis scripts `disasm16.py`, `fixups.py`, `wrongds.py` and `poll_cpu.py`.

## Cause chain
1. ISPACE code accesses a global block through ES, frees the block with `GlobalFree`, and still holds the block's selector in ES at the call (see Windows behaviour). [ORACLE fault-0001]
2. `GlobalFree16` → `GLOBAL_FreeBlock` → `SELECTOR_FreeBlock` → `free_entries` clears the LDT entries. Nothing touches the caller's saved segment registers. [REF Wine 11.0 dlls/krnl386.exe16/global.c, selector.c]
3. The 16-bit relay return path pops DS, then ES, FS and GS from the STACK16FRAME (`popw %ds; popw %es; …; lret $12`). `popw %es` faults on the freed selector. [REF Wine 11.0 tools/winebuild/relay.c BuildCallFrom16Core]
4. `call16_handler` → `fix_selector` sets the stacked ES slot to 0 and returns `ExceptionContinueExecution`. [REF Wine 11.0 dlls/krnl386.exe16/wowthunk.c]
5. Under new WoW64 the caller's DS does not survive the exception round trip:
   - `get_thread_wow64_context` reports DS = ES = `ds64_sel` (0x2b), and `set_thread_wow64_context` ignores the DS/ES it is given. [REF Wine 11.0 dlls/ntdll/unix/signal_x86_64.c]
   - The resume path `.Lsyscall_32to64_return` loads DS and ES from `context->SegSs`. [REF Wine 11.0 dlls/wow64cpu/cpu.c]
   - The handler sees `ds=002b es=002b ss=03a7` for every such fault. [ORACLE fault-0001]
6. The resumed `popw %es` loads 0, but DS equals SS: 03a7, the INSPACE.EXE DGROUP. ISPACE.DLL runs on the task's stack, and its own DGROUP is 0427. [ORACLE fault-0001]
7. All later ISPACE data accesses read INSPACE DGROUP:
   - ISPACE.DLL seg5:0x23a4 (runs on the caller's DS) returns 0, so `GlobalLock(0)` returns NULL and the `rep movsw` faults.
   - Handles read from the wrong DGROUP reach `GlobalUnlock` and `GlobalFree` as junk (6c65, 7265, 4400).
   - The `CreateCompatibleBitmap` call at seg7:0x4db8 pushes the word at DGROUP:0x1a32 of the wrong DGROUP, which is 0 (a NULL DC). [ORACLE fault-0001]
- A 32-bit Wine saves and restores DS through the CONTEXT (`save_context`, `restore_context`), so there `fix_selector` keeps DS intact. [REF Wine 11.0 dlls/ntdll/unix/signal_i386.c]

## Evidence: intro fault (`work/fault/confirm-intro/`, unpatched)
- Block 0536 comes from a loop at ISPACE.DLL seg7:0x1481–0x14c2 that runs four times. [BYTES ISPACE.DLL seg7:0x1481]
  - Each pass calls `GlobalAlloc(0x42, n*n)`. The relay shows `GLOBALALLOC(0042,00012990)`, i.e. 76176 = 276×276 bytes.
  - The handle goes to ISPACE.DLL DGROUP:0xda8+2i, and the locked far pointer to DGROUP:0xec8+4i.
  - A fifth block follows: handle at DGROUP:0xe68, far pointer at DGROUP:0x1048.
- The blocks are over 64 KB, so each has two tiled selectors. `+selector` prints `SELECTOR_FreeBlock (0537,2)`. [ORACLE fault-0001]
- The loop at seg7:0x4cb3–0x4cee unlocks and frees the four handles. [BYTES ISPACE.DLL seg7:0x4ce3]
  - The GLOBALFREE(0536) that returns to seg7:0x4ce8 faults on `pop es` for 053f, the block's second tile (`+thunk`: "fixing up selector 53f").
  - The next call from that loop is `GLOBALUNLOCK(6c65)` with ds=03a7. Between seg7:0x4ce8 and seg7:0x4d45 no instruction loads DS. [ORACLE fault-0001]
- The seg6 path: [BYTES ISPACE.DLL seg6:0x63c7]
  - `les si,[bp-0x6a]` at seg6:0x63c7 leaves the block's selector (054f) in ES.
  - GLOBALFREE(054e), returning to seg6:0x638c, faults on `pop es`.
  - The far call at seg6:0x6432 to seg5:0x23a4 then runs on ds=03a7, and `rep movsw` at seg6:0x6480 faults.
- In the whole unpatched run:
  - `fix_selector` fires 18 times, every time on a `GLOBALFREE` or `FREERESOURCE` return.
  - 4 of those switch ISPACE code from DS 0427 to 03a7 (returns at seg7:0x4ce8 twice, seg8:0x1faf and seg6:0x638c).
  - 58 relay calls come from ISPACE code with ds=03a7. [ORACLE fault-0001]
- Which selector ES holds depends only on the code path, not on timing. This fits a fault that reproduces at depth 8 and 24 and with tracing off. [ORACLE fault-0001]

## Windows behaviour
- Win16 calling conventions preserve only BP, SI and DI (and DS by Windows convention), so a Windows API returns with ES undefined and never reloads the caller's stale ES. [REF The Old New Thing, "The history of calling conventions, part 1", 2004-01-02]
- A DPMI 1.0 host zeroes any segment register that holds a selector being freed. [REF DPMI 1.0 spec, Int 31h function 0001h]
- Whether the Windows 3.1/9x KERNEL `GlobalFree` also zeroes a stale ES in the caller's frame is [OPEN].
- **Verdict:** the game behaviour is latent and harmless on Windows and on 32-bit Wine. The fault is a Wine divergence that needs both the relay's ES reload (step 3) and new WoW64 losing DS across an exception (step 5). [ORACLE fault-0001]

## Fix for the oracle
- **No setting helps.**
  - `fix_selector`, the relay and wow64cpu read no configuration.
  - WoW64 loads `wow64cpu.dll` unless `HKLM\Software\Microsoft\Wow64\x86` names another CPU DLL, and Wine 11.0 x86_64 ships no other one. [REF Wine 11.0 dlls/wow64/syscall.c get_cpu_dll_name]
  - Old-WoW64 Wine and otvdm run the game without the patch: see [runtime-0001](runtime-0001.md).
- **krnl386.exe16 patch** (`oracle/wine/krnl386-clear-saved-selectors.patch`, 29 lines in `dlls/krnl386.exe16/selector.c`). [DESIGN]
  - `free_entries` first calls `clear_saved_selectors`, which walks the chain of STACK16FRAMEs (`frame32->frame16`).
  - In the current frame it zeroes each of DS, ES, FS and GS that falls inside the freed range. In outer frames it zeroes only ES, FS and GS.
  - The 16-bit return path then never loads a freed selector. This matches the DPMI 1.0 rule above.
- **Build** (`oracle/wine/build_patched.sh STOCK DEST`):
  - Out-of-tree `configure --enable-archs=i386,x86_64` with optional libraries disabled.
  - `make dlls/krnl386.exe16/i386-windows/krnl386.exe16`, then `i686-w64-mingw32-strip --strip-debug`.
  - Copy the stock install and replace only `lib/wine/i386-windows/krnl386.exe16`. `diff -rq` then lists that one file. Configure needs flex and bison.
- **Result with the patch:** [ORACLE fault-0001]
  - SKIP_INTRO=0 (`work/fault/patched-intro/`): the desk intro types through "PREPARE TO LAUNCH... GOOD LUCK, YOUR NAME!", launches the ship and opens the Navigation window by itself, 20–24 s after OK in the traced run.
  - The traced run shows zero `fix_selector` events, no ISPACE call with ds=03a7, and no fault.
  - Enter on D:\GAMES shows "WAVE 1 MISSION BRIEFING" (D:\GAMES\, 3 ICONS, ALPHA/BRAVO/CHARLIE.EXE).
  - SKIP_INTRO=1 (`work/fault/patched-skip/`) reaches the same briefing.
  - In a further untraced SKIP_INTRO=1 run with KEEP=1 (`work/fault/patched-keep/`), ESC on the briefing starts wave 1, with the ship, a rock, the icon column and the HUD (`wave-02.png`).
  - The call at seg7:0x4db8 passes real DCs (005a, 00ae) on ds=0427, so no NULL-DC call occurs.

## Silent exit during the volume-label probe
- systemd-coredump caught the crash in `newgame-died-at-label` (`winevdm.exe`, SIGSEGV, 06:32:57 = 25.1 s after launch = the label probe). The game thread died inside `segv_handler` → `setup_raise_exception`. [ORACLE fault-0001]
- The core shows a nested fault. [ORACLE fault-0001]
  - The interrupted context is an instruction fetch at EIP 0, CS 0x23 (flat 32-bit Wine code), SS 03a7, EBP 0x0115e388 (flat).
  - `setup_raise_exception` treats SS ≠ 0x2b as 16-bit and takes the TEB stack slot. That slot holds the 16-bit SS:SP while 32-bit code runs. It writes the exception frame at 0x03a7aec0, faults again, and the kernel kills the process. No message appears, and the process runs no DLL detach.
- The WoW64 CPU context in the core holds SegSs 03a7 next to a flat Esp 0x0115e138. [ORACLE fault-0001]
  - The full-state resume (`.Lsyscall_32to64_return`, taken when `WOW64_CPURESERVED_FLAG_RESET_STATE` is set) loads SS, DS and ES from that SegSs.
  - With a 16-bit SS the stack uses SP alone. The faulting SP is 0xe150 = 0xe138 + 4 + 0x14, which fits a syscall stub's `ret $0x14` that pops its return address (0) from INSPACE DGROUP:0xe138 instead of the flat stack.
- The flag is set when a suspend or context request reaches a thread inside a syscall (`signal_x86_64.c`), on NtContinue, and on user-APC delivery (`wow64/syscall.c`). Which one hit the game thread is [OPEN]. [REF Wine 11.0 dlls/ntdll/unix/signal_x86_64.c, dlls/wow64/syscall.c]
- SegSs 03a7 enters the saved context through the `fix_selector` NtContinue, which stores the 16-bit SS via CONTEXT_I386_CONTROL. [ORACLE fault-0001]
  - Polling the saved context through `/proc/PID/mem` (`work/fault/poll-*.txt`) shows SegSs alternating between 03a7 and 002b for a whole unpatched run.
  - With the patch it stays 002b throughout, gameplay included.
- The patch removes this source, so it also removes this crash path. Five patched scenario runs (`work/fault/patched-intro`, `patched-skip`, `patched-keep`, `rep-3`, `rep-5`) show no silent exit and no fault. At the unpatched rate of 1 in 5 that count is weak evidence by itself (0.8⁵ ≈ 0.33). [ORACLE fault-0001]

## Residual risks [OPEN]
- ISPACE.DLL contains `int 31h` (seg10:0x461e), `int 21h` AX=440Dh CX=0860h (seg10:0x6279) and `int 2Fh` AX=150Bh (seg10:0x62d5). [BYTES ISPACE.DLL seg10:0x461e]
  - If they run, Wine emulates them through the same exception path, which loses DS/ES and leaves SegSs stale.
  - They do not run in this scenario: `+seh` shows no other 16-bit exceptions.
- A fix in wow64cpu/ntdll would preserve DS/ES for a non-flat SS during exception resume and cover these cases too. The patch does not attempt that.
