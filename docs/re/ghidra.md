# Ghidra pipeline

`re/ghidra/run.sh` imports the rebuilt NE modules (`work/rebuilt/inspace.exe`, `work/rebuilt/ispace.dll`, written by `isre all`) into Ghidra 12.1 headless, analyses them and writes one decompiled C file per function. `re/tests/test_ghidra.py` checks the outputs. [DESIGN]

## Running
```
GHIDRA_HOME=<ghidra 12.1 install> JAVA_HOME=<jdk 21> re/ghidra/run.sh [inspace] [ispace]
```
- With no arguments it runs `inspace` and then `ispace`, which reads the join table that the `inspace` run writes. [DESIGN]
- The driver stops with exit code 2 if `GHIDRA_HOME` or `JAVA_HOME` is unset or the rebuilt module is missing. [DESIGN]
- Settings come from the environment: `MAXMEM` (JVM heap, default `3G`), `MAXCPU` (analysis cores, default 2), `THREADS` (decompiler threads, default 2) and `WORK` (default `<repo>/work`). [DESIGN]
- Each module is a fresh import (`-overwrite`) into the project `work/ghidra/InnerSpace`. The importer is the `NeLoader` with its ordinal lookup off, so import names come only from `re/wine-specs`. [DESIGN]

## Outputs
| Path | Content |
|---|---|
| `work/decomp/<module>/segNN_0xOFF.c` | decompiled C for one function, with a header giving the canonical address, Ghidra address, name, size, callers and convention |
| `work/decomp/<module>/index.tsv` | `address` (`segN:0xOFF`), `ghidra`, `name`, `size` (bytes in the body), `callers` (distinct calling functions), `call_sites`, `status`, `file` |
| `work/ghidra/<module>-setup.json` | segment and relocation checks, import naming, seeds |
| `work/ghidra/<module>-finish.json` | frame seeds, data-segment cleanup, conventions, RTL and join results, function counts |
| `work/ghidra/<module>-export.json` | function count, failures, decompile time |
| `work/ghidra/<module>-imports.tsv` | every imported ordinal with its spec name and kind |
| `work/ghidra/inspace-join.tsv` | INSPACE `GetProcAddress` ordinals joined to the ISPACE.DLL entry table |
| `re/artifacts/ghidra-functions-<module>.tsv` | committed copy of `index.tsv` |

Ghidra loads segment N at selector `0x1000 + (N-1)*8`. The scripts write canonical addresses, and `isre addr` converts others by hand. [REF Ghidra 12.1.2 NE loader]

## Scripts
- **`IsSetup.java`** (pre-analysis):
  - It checks the import. Every NE segment must be a memory block of minalloc bytes at its selector, and every NE relocation record must have an applied site. Every import FAR_ADDR site must point to a thunk of an external `Ordinal_N`. [DESIGN]
  - It sets DS on all code to the DGROUP selector, and SS too for an application, since an application runs with SS = DGROUP. DGROUP is the NE autodata segment: INSPACE seg19, ISPACE.DLL seg11. [BYTES NE header]
  - It renames each external from the matching `*16.spec` (KERNEL → `krnl386.exe16.spec`). Pascal entries get `__stdcall16far` with custom stack storage. The first argument then sits at the highest offset, and the callee pops the bytes. `-ret16` returns AX, otherwise DX:AX. `DOS3Call` takes AX, BX, CX and DX. [REF Wine 11.0 *.spec] [DESIGN]
  - It marks KERNEL.1 `FatalExit` non-returning. In INSPACE, one pad byte and a far prologue at seg5:0xba4 follow the call at seg5:0xb9e. [BYTES seg5:0xb9e]
  - It writes each equate import's value (`__AHSHIFT` 3, `__AHINCR` 8, `__WINFLAGS` 0x413) into its OFFSET sites, because Ghidra writes a thunk offset there. [REF Wine 11.0 krnl386.exe16.spec]
  - It repoints the one additive FAR_ADDR import (ISPACE.DLL seg3:0x24, KERNEL.91 + 0xfffb) at the KERNEL.91 thunk and puts the addend in a comment. Ghidra adds the whole 32-bit placeholder, which carries into the selector. [BYTES ISPACE.DLL seg3:0x23]
  - It seeds functions in this order. First come the 10-byte far prologues: `mov ax,ss`, `mov ax,ds` or `push ds; pop ax`, then `nop; inc bp; push bp; mov bp,sp; push ds; mov ds,ax`. Then come the program entry, the entry table (named `<MODULE>_Ordinal_N`), and the targets of far calls carried by internal SEGMENT relocations. [DESIGN]
  - It turns off the Aggressive Instruction Finder. [DESIGN]
- **`IsFinish.java`** (post-analysis):
  - It seeds `inc bp; push bp; mov bp,sp` and `push bp; mov bp,sp` frames only in bytes that analysis left undefined and that no fall-through reaches, then re-runs analysis. [DESIGN]
  - It removes any function or instruction in a non-code block, and gives `__cdecl16far` to far-frame or far-called functions that still have no convention. [DESIGN]
  - It moves INSPACE seg5 into the namespace `BorlandRTL` and names seg5:0x1a `BorlandRTL::startup`. [BYTES seg5:0x1a]
  - For INSPACE, it finds the `push [h]; push 0; push N; call GetProcAddress; mov [g],ax; mov [g+2],dx` sequences whose handle `h` is the one stored after the LoadLibrary call at seg10:0x67c0. It labels `g` as `pISPACE_Ordinal_N` and comments `g`, the call site and every indirect call through `g` with the ISPACE.DLL entry address. [BYTES seg10:0x67c0]
  - For ISPACE, it adds the join to the plate comment of each exported function. [DESIGN]
- **`IsExport.java`** (post-analysis): it decompiles every function in a code segment with the parallel decompiler and writes the C files and the index. [DESIGN]

## Results
| | INSPACE | ISPACE.DLL |
|---|---|---|
| NE relocation records applied | 933 of 933 | 346 of 346 |
| import call sites reaching an external ordinal | 5,901 of 5,901 | 1,959 of 1,959 |
| distinct imported ordinals (outside seg1) | 186, all named from the specs | 153, all named |
| functions in code segments | 939 | 284 |
| of which far-prologue entries | 880 | 245 |
| other functions | seg1: 7, seg5: 52 | seg1: 7, seg3: 32 |

- Every one of the 880 entries in `re/artifacts/function-entries.json` is an INSPACE function. The other INSPACE functions sit in seg1, the OPTLOADER stub that the prologue scan skipped, and in seg5, where Borland RTL routines lack the standard prologue. Every ISPACE.DLL exported-prologue match is a function too. `test_inspace_functions_vs_prologue_scan` and `test_ispace_functions_vs_prologue_scan` assert the counts in the table. [DESIGN]
- Only the seg1 stub of each module imports KERNEL.170 (`AllocCStoDSAlias`). [BYTES seg1 relocations]
- Two runs on the same inputs write identical `index.tsv` files. `test_committed_index` compares the committed copy with `work/`. [DESIGN]

## Limits
- Indirect calls through the `pISPACE_Ordinal_N` pointers have no prototype, so the decompiler guesses their arguments. [DESIGN]
- The far prologue's `inc bp` and `push ds` show up as `unaff_BP + 1` and a DGROUP constant local. [DESIGN]
- With SS = DGROUP, far pointers to locals appear as `CONCAT22(0x1090, local_x)` in INSPACE. [DESIGN]
- Code that tests `__WINFLAGS` (for example INSPACE seg5:0x376, `test cs:[0x10],1`, where seg5:0x10 is the OFFSET site of KERNEL.178) folds to its protected-mode branch. The patched value 0x413 has bit 0 set, so the decompiler reports the real-mode `int 21h` blocks as unreachable. [BYTES seg5:0x10] [BYTES seg5:0x376]
- `-register` imports other than `DOS3Call` (for example `InitTask`) have no register parameters. [DESIGN]
- The pipeline decompiles the seg1 stub like other code, although nothing calls it in a module without SELFLOAD. [BYTES INSPACE.EXE entry table]
