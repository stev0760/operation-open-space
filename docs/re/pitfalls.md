# RE pitfalls

These are rules for working on this binary. Each one exists because ignoring it has produced a wrong conclusion. Check this list before proposing a new extraction method.

## Packing and relocations
- **Packed segment layout:** `[word reloc count][compressed bitstream][relocation table]`. The bitstream starts at `fo+2`, and bit `1` means a literal. Settle decompressor parameters empirically against the golden hashes, never by reading the loader. `[BYTES]` `re/artifacts/golden/inspace.sha256`
- **The NE header's relocation counts are zeroed.** The real relocations sit inside each packed segment. `[BYTES]`
- **Relocation chains:** they can reach 159 sites, so never cap a chain walk. Special-base records chain through the segment word as well as import records: `seg4:0x24` → `seg4:0x6b` → `0xFFFF`. A `9a .. .. ff ff` call is the tail of such a chain, not a broken call. `[BYTES seg4:0x68]`
- **seg1 of `INSPACE.EXE` is the OptLoader stub** (code, flags `0xd50`, 2,164 bytes). It is not DGROUP. **DGROUP is seg19** (NE autodata). Initialised data ends at `DGROUP:0x17b5`; everything above that is BSS filled at runtime. Tables that live in BSS can't be read from the file. `[BYTES]`
- **seg5 is the Borland runtime library** and starts with normal Borland startup at `seg5:0x1a` (`xor bp,bp; push bp; call KERNEL.InitTask`). Its assembly routines lack standard prologues, so prologue scans miss them. Disassembling the compressed bytes produces a fake "stub overwrote seg5" story. `[BYTES seg5:0x1a]`
- **Segment preloading:** segments 9–18 are not PRELOAD (flags `0x1c18`). Don't assume all segments preload. `[BYTES]`

## Reading code
- **Win16 KERNEL/USER/GDI APIs use the Pascal convention.** Arguments are pushed left to right, so the first `push` is the first argument. Borland's own C functions are cdecl (right to left). Check the callee's module before assigning roles to pushed operands. `[BYTES]`
- **Some DOS call numbers arrive in registers.** The Borland findfirst/findnext helper loads the function number into `AL` (`mov $0x4e,%al` / `mov $0x4f,%al`) and issues `mov %al,%ah; int 21h` through DOS3Call. So a search for `mov $0x4e00,%ax` immediates finds nothing even though the game enumerates directories. `[BYTES seg5:0x352]` `[BYTES seg5:0x364]` `[BYTES DGROUP:0x1260]` (`C:\*.*`)
- **Stack locals are not struct fields.** Indices loaded from `-0xc(%bp)` or `-0xa(%bp)` are locals (loop counters, for example). Before naming a field, trace where the base and index registers come from. `[BYTES seg4:0x1003]`
- **Check each struct field against the stride.** An offset at or beyond the stride (for example `+0x74` in a `0x70`-stride struct) belongs to something else. `[BYTES]`
- **Win16 `ExtractIcon(hInst, path, -1)` returns the number of icons** in the file, not an icon. `[REF Win16 SDK]`
- **Packed-file offsets are not addresses.** A string's offset in the packed `INSPACE.EXE` (for example the registration text) is a file position inside a resource, and code never references it. Convert addresses with `isre addr`. `[BYTES]`

## Tools
- **Ghidra decompilation is only as good as the import.** Without NE relocation records, Ghidra resolves no imports or far calls, and it disassembles DGROUP as code. Rebuild the NE with real relocation records before trusting any decompiler output. `[BYTES]`
- **Linear-sweep disassembly loses sync** inside data embedded in code (for example `seg5:0x351` swallows the prologue at `0x352`). Use recursive descent seeded from known entries. `[BYTES seg5:0x351]`
- **rea can't open NE images directly.** Use it on Ghidra projects or on single segments loaded as `dos-com`, where the base is `+0x100`; convert with `isre addr`. `[DESIGN]`
- **The Hermes-era docs in `../innerspace-rea/` are unreliable.** Their claims marked VERIFIED include errors. Re-derive from bytes before carrying any claim forward. `[DESIGN]`
