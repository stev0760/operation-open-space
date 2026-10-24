# RE pitfalls

These are rules for working on this binary. Each one exists because ignoring it has produced a wrong conclusion. Check this list before proposing a new extraction method.

## Packing and relocations
- **Packed segment layout:** `[word reloc count][compressed bitstream][relocation table]`. The bitstream starts at `fo+2`, and bit `1` means a literal. Settle decompressor parameters empirically against the golden hashes (`re/tests/test_unpack.py`), never by reading the loader. `[BYTES INSPACE.EXE segment table]`
- **The NE header's relocation counts are zeroed.** The real relocations sit inside each packed segment. `[BYTES INSPACE.EXE segment table]`
- **Relocation chains:** they can reach 159 sites, so never cap a chain walk. Special-base records chain through the segment word as well as import records: `seg4:0x24` → `seg4:0x6b` → `0xFFFF`. A `9a .. .. ff ff` call is the tail of such a chain, not a broken call. `[BYTES seg4:0x68]`
- **In a special-base far call (`9a oo oo ss ss`) the relocation site is the segment word.** The offset word holds the real target offset; only the segment words carry chain links. Taking chain heads alone finds 122 of the 553 internal far-call targets. `[BYTES seg4:0x24]`
- **seg1 of `INSPACE.EXE` is the OptLoader stub** (code, flags `0xd50`, 2,164 bytes). It is not DGROUP. **DGROUP is seg19** (NE autodata). Initialised data ends at `DGROUP:0x17b5`; everything above that is BSS filled at runtime. Tables that live in BSS can't be read from the file. `[BYTES INSPACE.EXE segment table]`
- **seg5 is the Borland runtime library** and starts with normal Borland startup at `seg5:0x1a` (`xor bp,bp; push bp; call KERNEL.InitTask`). Its assembly routines lack standard prologues, so prologue scans miss them. Disassembling the compressed bytes produces a fake "stub overwrote seg5" story. `[BYTES seg5:0x1a]`
- **Decode packed code up to minalloc, not the decompressed length.** OptLoader zero-fills the rest, and code can rely on it: the final `retf 0xa` of seg4 in the shipdisk FACTORY.EXE needs the zero-fill byte, and `theworks/FACTORY.EXE` repeats the pattern at seg4:0x634f. `[BYTES FACTORY.EXE seg4:0x5d0f]`
- **Segment preloading:** segments 9–18 are not PRELOAD (flags `0x1c18`). Don't assume all segments preload. `[BYTES INSPACE.EXE segment table]`

## Reading code
- **Win16 KERNEL/USER/GDI APIs use the Pascal convention.** Arguments are pushed left to right, so the first `push` is the first argument. Borland's own C functions are cdecl (right to left). Check the callee's module before assigning roles to pushed operands. `[REF Win16 SDK]` `[REF Wine 11.0 *.spec]`
- **Some DOS call numbers arrive in registers.** The Borland directory-search helpers load the function number into `AL` and share a tail that issues `mov %al,%ah` and DOS3Call: `seg5:0x352` is findnext (`mov $0x4f,%al`), `seg5:0x364` is findfirst (`mov $0x4e,%al`). So a search for `mov $0x4e00,%ax` immediates finds nothing even though the game enumerates directories. `[BYTES seg5:0x35d]` `[BYTES seg5:0x36f]` `[BYTES DGROUP:0x1260]` (`C:\*.*`)
- **KERNEL.1 FatalExit does not return; KERNEL.137 FatalAppExit can.** A decoder that falls through after FatalExit swallows the next prologue: the call at `seg5:0xb9e` is followed by one pad byte and then the function at `seg5:0xba4`. The runtime library expects FatalAppExit to return: the instructions after its call at `seg5:0xb95` set up `FatalExit(0xff)` as a fallback and nothing else reaches them. `[BYTES seg5:0xb9e]` `[BYTES seg5:0xb95]`
- **Stack locals are not struct fields.** Indices loaded from `-0xc(%bp)` or `-0xa(%bp)` are locals (loop counters, for example). Before naming a field, trace where the base and index registers come from. `[BYTES seg4:0x1003]`
- **Check each struct field against the stride.** An offset at or beyond the stride (for example `+0x74` in a `0x70`-stride struct) belongs to something else. `[DESIGN]`
- **Win16 `ExtractIcon(hInst, path, -1)` returns the number of icons** in the file, not an icon. `[REF Win16 SDK]`
- **Packed-file offsets are not addresses.** A string's offset in the packed `INSPACE.EXE` (for example the "Unregistered Copy" text) is a file position inside a resource, and code never references it. Convert addresses with `isre addr`. `[BYTES INSPACE.EXE 5000 143]`

## Tools
- **Ghidra decompilation is only as good as the import.** Without NE relocation records, Ghidra resolves no imports or far calls, and it disassembles DGROUP as code. Import the output of `isre rebuild`, never the packed file. `[REF Ghidra 12.1.2 NE loader]`
- **Linear-sweep disassembly loses sync** inside data embedded in code (for example `seg5:0x351` swallows the prologue at `0x352`). Use `isre asm`, which descends recursively from known entries. `[BYTES seg5:0x351]`
- **rea opens neither NE images nor Ghidra projects** (`unsupported NE executable in MZ image`, also for the rebuilt NE). It opens only a single segment loaded as `dos-com` (base `+0x100`; convert with `isre addr`), which loses relocations, assumes DS = CS and finds few functions. Use the Ghidra pipeline and `isre pack` instead. `[REF rea-agents 6.4.0 open_binary]`
- **The Hermes-era docs in `../innerspace-rea/` are unreliable.** Their claims marked VERIFIED include errors. Re-derive from bytes before carrying any claim forward. `[DESIGN]`
