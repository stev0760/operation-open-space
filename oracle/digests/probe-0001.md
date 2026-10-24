# probe-0001: DGROUP memory of the running original at the title, in the Navigation window and in wave 1

- **Date:** 2026-10-24. **Host:** Wine 11.0 old WoW64 (`/opt/wine-11.0-oldwow`, without the ExtractIcon patch), Xvfb 1024×768×24, prefix `innerspace3`, display `:113`.
- **Setup:** the newgame scenario of [newgame-0002](newgame-0002.md) (`--with-ini`, fixture `basic` as `D:`, no `Z:`), traced with the relay filter of `oracle/trace.sh` as it stood for this run, which also excludes `GETASYNCKEYSTATE`, `SELECTCLIPRGN` and `PTINRECT`. `WAIT_BOOT=40` keeps the title up while the probe runs.
- **Reproduce:** `oracle/probe/build.sh`, then `ORACLE_ID=3 ORACLE_WINE=/opt/wine-11.0-oldwow oracle/probe/probe_run.sh work/probe/run`. Usage of the probe is in the header of `oracle/probe/isprobe.c`.
- **Bulk output (gitignored):** `work/probe/run/` (scenario screenshots, `trace.log` 31 MB, `digest.md`, `steps.txt`, `probe-steps.txt`, `probe-find.txt`, `probe-watch-*.txt`, `probe-*.png`) and `work/probe/dumps/` (12 dumps, `<phase>-inspace.bin` and `<phase>-ispace.bin`).
- No fault occurs, and the game reaches wave 1 while the probe reads its memory. [ORACLE probe-0001]

## How the probe finds the DGROUPs under Wine
- `isprobe` (32-bit Win32 console program) runs in the game's prefix. FindWindow on class `Inner Space` and GetWindowThreadProcessId give the game's process and its 16-bit task thread. The probe opens them with `PROCESS_VM_READ | PROCESS_QUERY_INFORMATION` and `THREAD_QUERY_INFORMATION` and never writes. [ORACLE probe-0001]
- **Scan route:** VirtualQueryEx walks the address space and ReadProcessMemory reads every committed region. The probe searches each module's first anchor and checks two more anchors at their offsets from the implied base. [ORACLE probe-0001]
  - The anchors are text runs of 48–64 bytes from the initialised DGROUP images. Each is unique in its image and absent from the other image. `oracle/tests/test_probe.py` checks them against `work/unpacked/`.
  - INSPACE.EXE anchors: `DGROUP:0x4a0`, `0x2d6`, `0x12d3` [BYTES DGROUP:0x4a0]. ISPACE.DLL anchors: `DGROUP:0x6b6`, `0x7a`, `0x1b2` [BYTES ISPACE.DLL DGROUP:0x6b6].
  - At the title the walk covers 512 regions up to 0x7fff0000 and reads 62.7 MB in about 0.1 s per module. Each first anchor occurs once in the process, and all three anchors match at that base. [ORACLE probe-0001]
- **Selector route:** GetThreadSelectorEntry on the game thread works across processes under old WoW64. Wine answers it from the LDT copy that the target publishes through its PEB [REF Wine 11.0 dlls/ntdll/unix/virtual.c ldt_get_entry]. A walk over all 8191 LDT selectors takes about 0.7 s. [ORACLE probe-0001]
- **Result:** both routes agree. [ORACLE probe-0001]

  | Module | Linear base | LDT selector with that base | Limit |
  |---|---|---|---|
  | INSPACE.EXE | 0x00cb4950 | 03a7 (data) | 0xffff |
  | ISPACE.DLL | 0x00cc63c0 | 0427 (data) | 0x26bf |

  - The selectors match the relay's selector map ([newgame-0001](newgame-0001.md)).
  - No other selector maps either base.
  - Both bases stay the same in all 12 dumps, and the watchers never see the INSPACE selector's base move.
- After the title scan the run uses `--sel`. ReadProcessMemory and GetThreadSelectorEntry go through wineserver (`process_vm_readv`) without stopping the game. A cross-process VirtualQueryEx is a system APC that the game process answers itself [REF Wine 11.0 server/ptrace.c, dlls/ntdll/unix/virtual.c NtQueryVirtualMemory].
- `wine` given the probe as an absolute Unix path segfaults in `get_full_path` before the program starts when no drive maps the path (`Z:` removed). A path relative to the repo works. `probe_run.sh` starts the probe that way. [ORACLE probe-0001]

## Dump moments
Each dump covers the whole selector range: 65 536 bytes of INSPACE.EXE DGROUP and 9 920 bytes of ISPACE.DLL DGROUP. Times are wall clock, from `probe-steps.txt`. [ORACLE probe-0001]

| Phase | Time | Screen |
|---|---|---|
| title | 09:17:42.6 | title screen, 28 s after launch (`probe-title.png`) |
| navigation | 09:18:19.5 | between the scenario's `12-navigation` (`deep` selected, 19.43) and `probe-navigation.png` (`games` selected, 19.63) |
| wave | 09:19:02.6 | wave 1, 8.5 s after ESC on the briefing |
| move0 | 09:19:02.7 | wave 1, HUD clock 0 08 |
| move1 | 09:19:04.0 | after Up held for 1 s, HUD clock 0 09 |
| move2 | 09:19:05.2 | after Up held for 1 s more, HUD clock 0 10 |

Between `probe-move0`, `-move1` and `-move2` the rocks and fuel cans shift around the ship, which stays at the screen centre. [ORACLE probe-0001]

## Handles and the ISPACE.DLL pointer block
- INSPACE.EXE `DGROUP:0x9210` holds 0x0426 and `DGROUP:0x9212` holds 0x02bf in all six dumps. 0x0426 is the return value of `LOADLIBRARY("C:\INSPACE\ISPACE.DLL")` in the trace, and 0x02bf that of `LOADLIBRARY("C:\INSPACE\ENG_LANG.DLL")`. [ORACLE probe-0001]
- `DGROUP:0x3c28`–`0x3c87` holds 24 dwords. 23 of them equal the 23 results of `GETPROCADDRESS(0426, #n)` in the trace, whose return addresses are INSPACE.EXE seg10:0x67ec–0x6ad8. No result is missing. [ORACLE probe-0001]
  - Each of the 23 far pointers is the ISPACE.DLL entry-table address of its ordinal. [BYTES ISPACE.DLL entry table] [ORACLE probe-0001] The selectors map to segments as in the selector map: 03f7 = seg5, 03ff = seg6, 040f = seg8, 0417 = seg9, 041f = seg10, 03df = seg2.
  - Ordinals in slot order: 1, 3, 2, 12, 17, 18, 6, 15, 36, 5, 4, 7, 8, 9, —, 10, 24, 29, 34, 33, 32, 14, 35, 41.
  - The slot without a pointer, `DGROUP:0x3c60`, is 0 at the title and in the Navigation window and 0x000000a3 from the wave dump on. [ORACLE probe-0001]
  - The 23 pointer slots are identical in all six dumps.

## Candidates DGROUP:0x9684 and DGROUP:0x9320
- Both dwords read 0x00000000 in all six dumps. [ORACLE probe-0001]
- Two watchers polled them every 50 ms from 09:17:42.6 to 09:19:05.3. That span covers the title, Restore Game, New Game, the intro skip, the Navigation tour, Enter, the briefing, ESC and both Up presses. Neither reports a change. [ORACLE probe-0001]
- A change shorter than one poll would go unseen. What the two words hold is [OPEN].

## Changes between phases
- INSPACE.EXE, bytes that differ below minalloc (0xa1f0) / above it: title → navigation 317 / 1803, navigation → wave 2613 / 1418, wave → move2 272 / 345. [ORACLE probe-0001]
- The initialised part (`DGROUP:0x0`–`0x17b4`) differs from the file in 64 bytes at the title. All six anchors stay intact in every dump. [ORACLE probe-0001]
- Navigation dump: the DTA at `DGROUP:0x69f8` ([newgame-0001](newgame-0001.md)) holds the name `TOOLS` at `DGROUP:0x6a16`. `DGROUP:0x6a24` holds the Navigation status line, "There are 3 icons in 3 files.You have not been here before." A dump taken while the line was being rewritten held it in mixed case ("iN 3 FILES.YOU HAVE …"). [ORACLE probe-0001]
- Navigation → wave: 884 of the 2613 bytes lie in `DGROUP:0x2f04`–`0x3b92`, which holds 133 non-zero bytes in the Navigation window and 987 in the wave. [ORACLE probe-0001]
- ISPACE.DLL: 377 bytes differ from title to navigation and 22 from navigation to wave. **No byte differs** between the wave dump and move2 (2.6 s, two Up presses). [ORACLE probe-0001]

## Bytes from DGROUP:0x70a0 on a 0x15a grid
The probe read this region in 0x15a-byte blocks from `0x70a0`. The ship records lie 0x10 lower, at `DGROUP:0x7090 + k·0x15a` for k = 0…7 ([structs.md](../../docs/re/structs.md)); the offsets below are relative to `0x7090`. [ORACLE probe-0001]
- `0x70a0`–`0x71f9` is all zero at the title and in the Navigation window. It holds 57 non-zero bytes in the wave and move0 dumps and 61 in move1 and move2.
- Words in it that change across the four wave dumps (wave, move0, move1, move2):

  | Offset from 0x7090 | Address | Values |
  |---|---|---|
  | +0x016 | `DGROUP:0x70a6` | 001b 0012 000c 001b |
  | +0x01e | `DGROUP:0x70ae` | 0528 0528 0523 051e |
  | +0x034 | `DGROUP:0x70c4` | 001e 001e 001e 000c |
  | +0x090 | `DGROUP:0x7120` | 00000000 00000000 4799da2f 4799dea9 (dword) |
  | +0x156 | `DGROUP:0x71e6` | 4799da89 4799da89 4799da89 4799e06e (dword) |

- `DGROUP:0x70ae` drops by 5 for each second of Up. Whether it is fuel or another quantity is [OPEN].
- `0x71fa`–`0x7a15` is zero in all six dumps.
- The 0x15a-byte blocks at `0x7a16`, `0x7b70`, `0x84e6` and `0x8640` are zero before the wave and fill at wave entry (10, 120, 16 and 44 non-zero bytes). They do not change across the four wave dumps.
- From `0x8d02` on, the grid overlaps bytes that are already non-zero at the title.

## Table at DGROUP:0x1df8
Raw contents of `DGROUP:0x1df8`–`0x23ef`. The range holds 60 non-zero bytes at the title and in the Navigation window, and 283–295 in the four wave dumps. [ORACLE probe-0001]
- At wave entry the word at `0x1e90 + 0x14·k` (k = 0…44, last `0x2200`) changes from 0x0000 to 0xffff. Words `0x221c`–`0x223e` become 0xffff, `0x2240` becomes 0x0001, and the words at `0x226c`, `0x22c6`, `0x2320` and `0x237a` (0x5a apart) become 0xffff.
- `DGROUP:0x1e5c`–`0x1e62` become 0x0202, 0x0157, 0x021f, 0x0174 (514, 343, 543, 372) at wave entry and stay so. In `probe-move0.png` the ship's non-black pixels span x 513–543, y 345–367. Whether these words are the ship's screen box is [OPEN].
- `DGROUP:0x1e68` reads 0x012a, 0x012e, 0x015a and 0x0187 in the four wave dumps.
- `DGROUP:0x2398`–`0x23ee` (44 words) are zero before the wave.
  - In the wave the 36 words up to `0x23de` hold values from 0x0047 to 0x03bc, and most change between consecutive wave dumps.
  - The 8 words `0x23e0`–`0x23ee` hold larger values (0x0894, 0x2d44, 0x6658, 0xfda8 among them).

## rand() state and timing words
Values read from the dumps at the addresses that [rng.md](../../docs/re/rng.md) and [timing.md](../../docs/re/timing.md) name. `oracle/rngcheck.py work/probe/dumps` prints the first table. [ORACLE probe-0001]

| Dump | DGROUP:0x144c | Seed | Draws since the seed | DGROUP:0x1e50 | Last tick with the seed as low word | ms before the dump's sample | DGROUP:0x1e68 |
|---|---|---|---|---|---|---|---|
| title | 0x25da5ef6 | 0x3375 | 891 | 0x47989c25 | 0x47983375 | 26 800 | 0 |
| navigation | 0x25da5ef6 | 0x3375 | 891 | 0x47992c9f | 0x47983375 | 63 786 | 0 |
| wave | 0xe43e04a1 | 0xb411 | 2512 | 0x4799d4d2 | 0x4799b411 | 8 385 | 298 |
| move0 | 0x1dae3fcb | 0xb411 | 2522 | 0x4799d552 | 0x4799b411 | 8 513 | 302 |
| move1 | 0x732657ca | 0xb411 | 2627 | 0x4799da0c | 0x4799b411 | 9 723 | 346 |
| move2 | 0x507159d6 | 0xb411 | 2799 | 0x4799dee8 | 0x4799b411 | 10 967 | 391 |

- "Seed" is the first state below 0x10000 that stepping the generator backwards from DGROUP:0x144c reaches. Stepping forwards, the wave state reaches the move0 state after 10 draws, the move1 state after 105 more and the move2 state after 172 more. The Navigation state does not reach the wave state within 10⁶ draws.
- DGROUP:0x2214 equals DGROUP:0x1e50 in every dump, except move2 (0x4799dee9).
- ISPACE.DLL DGROUP:0xb8c holds 0x00000001 in all six dumps.
- DGROUP:0x90f4 holds 0, 0, 1, 1, 1, 1 (title to move2).

| Dump | DGROUP:0x8ffc | DGROUP:0x9002 | DGROUP:0x9004 | DGROUP:0x8f76 | DGROUP:0x59a |
|---|---|---|---|---|---|
| title | 14 940 | 298 | −490 | 533 | 76 |
| navigation | 14 940 | 298 | 140 | 533 | 78 |
| wave | 14 940 | 298 | 410 | 559 | 35 |
| move0 | 14 940 | 298 | 410 | 559 | 35 |
| move1 | 14 940 | 298 | 410 | 574 | 37 |
| move2 | 14 940 | 298 | 410 | 589 | 37 |

## Clock values
- Several dwords have a high word of 0x4798 or 0x4799. That is the range of the trace's `+timestamp` clock, Wine's millisecond tick count. The last trace line is at 1201266.567 s = 0x4799df87 ms. [ORACLE probe-0001]
- For `DGROUP:0x1e50`, `0x2214` and `0x2218`, the tick value minus the dump's wall-clock time of day is the same offset, within 40 ms, in all six dumps from the title on. These three follow the current tick in every phase. [ORACLE probe-0001]
- `DGROUP:0x7120` is 0 until the first Up press. Its two values (move1, move2) are 1146 ms apart, and `probe-steps.txt` logs the two Up releases 1215 ms apart. [ORACLE probe-0001]
- What each of these clock values records is [OPEN].

## Not covered
- Dumps between the title and the Navigation window (dialogs, intro) and after the wave [OPEN].
- Meanings of the fields above [OPEN]. They need code reading and an independent verifier before they move to `docs/`.
