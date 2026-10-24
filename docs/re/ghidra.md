# Ghidra pipeline

`re/ghidra/run.sh` imports the rebuilt NE modules (`work/rebuilt/inspace.exe`, `work/rebuilt/ispace.dll`, written by `isre all`) into Ghidra 12.1 headless, analyses them and writes one decompiled C file per function. `re/tests/test_ghidra.py` checks the outputs. [DESIGN]

## Running
```
GHIDRA_HOME=<ghidra 12.1 install> JAVA_HOME=<jdk 21> re/ghidra/run.sh [inspace] [ispace]
```
- With no arguments it runs `inspace` and then `ispace`, which reads the join table that the `inspace` run writes. [DESIGN]
- The driver stops with exit code 2 if `GHIDRA_HOME` or `JAVA_HOME` is unset, or if the rebuilt module or `work/asm/<module>/functions.json` is missing. [DESIGN]
- Settings come from the environment: `MAXMEM` (JVM heap, default `3G`), `MAXCPU` (analysis cores, default 2), `THREADS` (decompiler threads, default 2) and `WORK` (default `<repo>/work`). [DESIGN]
- Each module is a fresh import (`-overwrite`) into the project `work/ghidra/InnerSpace`. The importer is the `NeLoader` with its ordinal lookup off, so import names come only from `re/wine-specs`. [DESIGN]
- The export goes to `work/decomp/.<module>.new`. It replaces `work/decomp/<module>/` only after every script has succeeded. [DESIGN]

## Outputs
| Path | Content |
|---|---|
| `work/decomp/<module>/segNN_0xOFF.c` | decompiled C for one function, with a header giving the canonical address, Ghidra address, name, size, callers and convention |
| `work/decomp/<module>/index.tsv` | `address` (`segN:0xOFF`), `ghidra`, `name`, `size` (bytes in the body), `callers` (distinct calling functions), `call_sites`, `status`, `file`, `ranges` (the body as end-exclusive offset ranges in the function's segment, such as `0x364-0x3de 0xdb8-0xdd5`) |
| `work/ghidra/<module>-setup.json` | segment and relocation checks, import naming and signatures, seeds |
| `work/ghidra/<module>-finish.json` | jump-table checks and repairs, frame and gap seeds, removed epilogue functions, data-segment cleanup, conventions, RTL and join results, function counts, every bounded jump table, data in code outside the tables |
| `work/ghidra/<module>-export.json` | function count, failures, decompile time |
| `work/ghidra/<module>-imports.tsv` | every imported ordinal with its spec name and kind |
| `work/ghidra/inspace-join.tsv` | INSPACE `GetProcAddress` ordinals joined to the ISPACE.DLL entry table |
| `re/artifacts/ghidra-functions-<module>.tsv` | committed copy of `index.tsv` |

Ghidra loads segment N at selector `0x1000 + (N-1)*8`. The scripts write canonical addresses, and `isre addr` converts others by hand. [REF Ghidra 12.1.2 NE loader]

## Scripts
- **`IsSetup.java`** (pre-analysis):
  - It checks the import. Every NE segment must be a memory block of minalloc bytes at its selector, and every NE relocation record must have an applied site. Every non-additive import FAR_ADDR site must point to a thunk of an external `Ordinal_N`. [DESIGN]
  - It sets DS on all code to the DGROUP selector, and SS too for an application, since an application runs with SS = DGROUP. DGROUP is the NE autodata segment: INSPACE seg19, ISPACE.DLL seg11. [BYTES NE header]
  - It renames each external from the matching `*16.spec` (KERNEL → `krnl386.exe16.spec`). Pascal entries get `__stdcall16far` with custom stack storage. The first argument then sits at the highest offset, and the callee pops the bytes. `-ret16` returns AX, otherwise DX:AX. `DOS3Call` takes AX, BX, CX and DX. [REF Wine 11.0 *.spec] [DESIGN]
  - It gives KERNEL.1 the signature `pascal void FatalExit(word)`, which the spec's `1 stub FatalExit` lacks, and marks it non-returning. In INSPACE, `push 0xff` precedes the call at seg5:0xb9e, and one pad byte and a far prologue at seg5:0xba4 follow it. [REF Win16 SDK] [REF Wine 11.0 krnl386.exe16.spec] [BYTES seg5:0xb9e]
  - It leaves KERNEL.137 `FatalAppExit` returning. In both modules the Borland RTL follows the FatalAppExit call with `mov ax,0xff; push ax; call far FatalExit`, which only the fall-through reaches. The call is at INSPACE seg5:0xb95 and ISPACE.DLL seg3:0xa8c, the fallback at seg5:0xb9a and ISPACE.DLL seg3:0xa91. [BYTES seg5:0xb95] [BYTES ISPACE.DLL seg3:0xa8c]
  - It writes each equate import's value (`__AHSHIFT` 3, `__AHINCR` 8, `__WINFLAGS` 0x413) into its OFFSET sites, because Ghidra writes a thunk offset there. [REF Wine 11.0 krnl386.exe16.spec]
  - It repoints the one additive FAR_ADDR import (ISPACE.DLL seg3:0x24, KERNEL.91 + 0xfffb) at the KERNEL.91 thunk and puts the addend in a comment. Ghidra adds the whole 32-bit placeholder, which carries into the selector. [BYTES ISPACE.DLL seg3:0x23]
  - It seeds functions in this order. First come the 10-byte far prologues: `mov ax,ss`, `mov ax,ds` or `push ds; pop ax`, then `nop; inc bp; push bp; mov bp,sp; push ds; mov ds,ax`. Then come the program entry, the entry table (named `<MODULE>_Ordinal_N`), and the targets of far calls carried by internal SEGMENT relocations. [DESIGN]
  - It turns off the Aggressive Instruction Finder. [DESIGN]
- **`IsFinish.java`** (post-analysis), in this order:
  - It bounds every `jmp cs:[reg+table]` the way `isre asm` does. It walks back from the jump to the `cmp reg,N` and the unsigned conditional jump that guard the index, then compares Ghidra's flows with the table. [DESIGN]
  - A table jump whose flows differ from its table gets rebuilt. IsFinish clears the owning function from its entry up to the next function and decodes it again with the bounded tables. It then clears two kinds of debris: 2-byte data outside every table that nothing references, and code that only a bogus target reached. Last, it recomputes the body, deletes Ghidra's `switchD_` labels and writes a jump-table override, because the decompiler recovers tables on its own and repeats the wrong range. [DESIGN]
  - Three tables need the rebuild. The jumps at INSPACE seg12:0x1194 and seg12:0x1673 have 10-entry tables, and Ghidra gives each 156 targets outside its table (`switch_repair_before_seeds` in the finish report). It also defines table entries as data over case code. The jump at ISPACE.DLL seg10:0x57fe has a 16-entry table, and Ghidra gives it no targets. [BYTES seg12:0x1673] [BYTES seg12:0x1194] [BYTES ISPACE.DLL seg10:0x57fe]
  - It seeds `inc bp; push bp; mov bp,sp` and `push bp; mov bp,sp` frames, then the `gap` entries of `work/asm/<module>/functions.json`. IsFinish places a seed only in bytes that analysis left undefined and that no fall-through reaches. The gap seeds are RTL code that nothing references: INSPACE seg5:0xb5c and seg5:0x1da8, and ISPACE.DLL seg3:0x12, 0x2b, 0x34, 0x791, 0x99f, 0xa4e and 0xa9a. The isre gap entry inside the FatalExit fallback (seg5:0xb9a, ISPACE.DLL seg3:0xa91) already lies in a decoded function and stays there. [DESIGN]
  - It re-runs analysis and checks the jump tables again. [DESIGN]
  - It removes a function that only jumps from one other function reach and that returns through that function's frame: the fragment pops BP without pushing it. It then recomputes the owner's body. Ghidra's shared-return analysis makes such epilogues functions and turns the jump into a call, which drops the owner's AX result. The removed functions are INSPACE seg5:0xdc3 (only the jmp at seg5:0x3db reaches it), ISPACE.DLL seg3:0x846 and ISPACE.DLL seg3:0x98d. An epilogue that several functions jump to stays a function: INSPACE seg5:0xdd5, ISPACE.DLL seg3:0x97b. [BYTES seg5:0xdc3] [DESIGN]
  - It removes any function or instruction in a non-code block, and gives `__cdecl16far` to far-frame or far-called functions that still have no convention. It reports a problem if analysis made FatalAppExit non-returning or FatalExit returning. [DESIGN]
  - It moves INSPACE seg5 into the namespace `BorlandRTL` and names seg5:0x1a `BorlandRTL::startup`. [BYTES seg5:0x1a]
  - For INSPACE, it finds the `push [h]; push 0; push N; call GetProcAddress; mov [g],ax; mov [g+2],dx` sequences whose handle `h` is the one stored after the LoadLibrary call at seg10:0x67c0. It labels `g` as `pISPACE_Ordinal_N` and comments `g`, the call site and every indirect call through `g` with the ISPACE.DLL entry address. [BYTES seg10:0x67c0]
  - For ISPACE, it adds the join to the plate comment of each exported function. [DESIGN]
- **`IsExport.java`** (post-analysis): it decompiles every function in a code segment with the parallel decompiler and writes the C files and the index. [DESIGN]

## Results
| | INSPACE | ISPACE.DLL |
|---|---|---|
| NE relocation records applied | 933 of 933 | 346 of 346 |
| non-additive FAR_ADDR import sites reaching an external ordinal | 5,901 of 5,901 | 1,959 of 1,959 |
| import far calls (`9a` at an import site) inside a function | 5,900 of 5,900 | 1,959 of 1,959 |
| distinct imported ordinals (outside seg1) | 186, all named from the specs | 153, all named |
| jump tables bounded, flows matching | 42 of 42 | 18 of 18 |
| functions in code segments | 942 | 293 |
| of which far-prologue entries | 880 | 245 |
| other functions | seg1: 7, seg5: 55 | seg1: 7, seg3: 41 |

- The FAR_ADDR site counts include the seg1 `AllocCStoDSAlias` pointer cell at seg1:0x28, which is not a call. The ISPACE.DLL far-call count includes the additive call at seg3:0x23. `test_import_call_sites_in_functions` asserts the far-call row. [BYTES seg1 relocations] [DESIGN]
- Every one of the 880 entries in `re/artifacts/function-entries.json` is an INSPACE function. The other INSPACE functions sit in seg1, the OPTLOADER stub that the prologue scan skipped, and in seg5, where Borland RTL routines lack the standard prologue. Every ISPACE.DLL exported-prologue match is a function too. `test_inspace_functions_vs_prologue_scan` and `test_ispace_functions_vs_prologue_scan` assert the counts in the table. [DESIGN]
- Outside seg1 and the RTL segment (INSPACE seg5, ISPACE.DLL seg3), the functions and their sizes equal those in `work/asm/<module>/functions.json`. `test_functions_vs_isre` asserts this and lists the remaining differences. Ghidra keeps RTL fragments that several functions reach as functions of their own, and `isre asm` adds them to each caller as a shared tail. [DESIGN]
- Every case of every jump table lies in the function that owns the jump (`test_switch_targets_in_owner`). Besides the tables, code segments hold only the seg1 stub's data and the `__WINFLAGS` word at seg5:0x10 (ISPACE.DLL seg3:0x10). [DESIGN]
- Only the seg1 stub of each module imports KERNEL.170 (`AllocCStoDSAlias`). [BYTES seg1 relocations]
- Two runs on the same inputs write identical `index.tsv` files. `test_committed_index` compares the committed copy with `work/`. [DESIGN]

## Limits
- Indirect calls through the `pISPACE_Ordinal_N` pointers have no prototype, so the decompiler guesses their arguments. [DESIGN]
- The far prologue's `inc bp` and `push ds` show up as `unaff_BP + 1` and a DGROUP constant local. [DESIGN]
- With SS = DGROUP, far pointers to locals appear as `CONCAT22(0x1090, local_x)` in INSPACE. [DESIGN]
- Code that tests `__WINFLAGS` (for example INSPACE seg5:0x376, `test cs:[0x10],1`, where seg5:0x10 is the OFFSET site of KERNEL.178) folds to its protected-mode branch. The patched value 0x413 has bit 0 set, so the decompiler reports the real-mode `int 21h` blocks as unreachable. [BYTES seg5:0x10] [BYTES seg5:0x376]
- For the same reason the decompiled ISPACE.DLL seg3:0x12 lacks the additive call at seg3:0x23. The `jns` at seg3:0x1c on `__WINFLAGS` (0x413, bit 15 clear) always skips it. [BYTES ISPACE.DLL seg3:0x17]
- At the overridden tables of INSPACE seg12:0x1194 and seg12:0x1673 the decompiler cannot match the switch variable. It labels each case with the case's linear address (`case 0x105813df` is seg12:0x13df) and shows the table load as a DGROUP expression. [DESIGN]
- Code that no reference, prologue or isre gap entry reaches is not decoded. [DESIGN]
- `-register` imports other than `DOS3Call` (for example `InitTask`) have no register parameters. [DESIGN]
- The pipeline decompiles the seg1 stub like other code, although nothing calls it in a module without SELFLOAD. [BYTES INSPACE.EXE entry table]
