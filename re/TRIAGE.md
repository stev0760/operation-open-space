# Triage of the Hermes-era RE workspace

The source is `../innerspace-rea/` (read-only archive). This file records what was carried into this repo and which open claims still need verification. Traps found during triage are in `docs/re/pitfalls.md`.

## Files
| Class | Items | Where now |
|---|---|---|
| Kept tools | szdd, ne_parse, slr_relocs, rebuild_ne, name_imports, annotate, annotated_asm, callgraph, prologue_scan, struct_fields, dump_asm, redirect_entry, attr_fields, field_by_func, globals_by_func, find_callers, resolve_call, ne_tables; checks validate_relocs, validate_entries, test_decomp | `re/legacy/` (to be refactored into `re/isre`) |
| Kept inputs | `Shipdisk.rar`, `THEWORKS.rar` (both ZIP archives) | `original/archives/*.zip` (gitignored) |
| Kept references | `wine-specs/*.spec`, `optlink-src/nappload.asm` | `re/wine-specs/`, `re/optlink-src/` |
| Kept outputs | `relocs.json`, `api-imports.json`, `function-entries.json`, `callgraph.json` | `re/artifacts/` |
| Golden hashes | `unpacked/seg*.bin`, `relocs.json`, legacy `INSPACE-unpacked.exe` | `re/artifacts/golden/` |
| Regenerable, not copied | extracted/, unpacked/, INSPACE-unpacked.exe, api-sites.*, annotated-asm/, api-calls*.txt, struct-fields.*, ne-*.json, import-set.json, probes/*.bin | regenerate into `work/` |
| Archived, not copied | the six .md docs, one-off probe scripts, Ghidra .java glue | stays in `../innerspace-rea/` |
| Junk, not copied | *.class, __pycache__, slr_decompress*.py, slr_unpack.py (broken), struct_map / field_semantics and their outputs, array_base, probe_bytes, probe_calls, r2-functions*, functions-unpacked.tsv, prologue-funcs.json, ghidra-out/ (57 MB; `decompiled.c` was made from packed bytes) | stays in `../innerspace-rea/` |

## Known defects in the kept tools
- **No unpack writer.** No script writes `unpacked/segNN.bin` correctly (`slr_unpack.py` writes 0 bytes). The decompressor `slr_relocs.dpmi_unquik_trace` reproduces all 19 segments byte for byte once wrapped: start at `fo+2`, zero-pad to minalloc, copy seg1 raw.
- **`rebuild_ne`:** writes no relocation records, and its resource table still points into segment code.
- **`ne_parse`:** returns resources with `file_off` 0 and garbage names, and marks seg19 (flags `0xc59`) as code.
- **`annotated_asm`:** prints the sector number as the file offset in headers, and its linear sweep loses sync (`seg5:0x351`).
- **`prologue_scan`:** hardcodes seg1 as the data segment.
- **Hardcoded paths:** 16 scripts hardcode `PROJ=/home/cabal/Git/repos/innerspace-rea`. Others read the current directory or default to `/tmp`.

## Byte-level facts carried forward
- **Packing:** segment format per `docs/re/pitfalls.md`; all 18 packed segments reproduce the goldens.
- **Relocations:** 930 records (725 import-ordinal + 205 special-base), chains end at `0xFFFF`, 186 distinct import ordinals, 5,900 API call sites.
- **API call sites:**
  - `DOS3Call` ×39, all in seg5.
  - `ExtractIcon` ×5 at `seg6:0x442c`, `seg6:0x4469`, `seg9:0x0178`, `seg9:0x01a5`, `seg9:0x0251`.
  - `GetDlgItem` ×141.
- **Procedures:** 880 far procedures by prologue scan.
- **DGROUP:** seg19; initialised data to `DGROUP:0x17b5`; INI key table at `DGROUP:0x401`; `*.exe` at `DGROUP:0x71c`; `C:\*.*` at `DGROUP:0x1260`.
- **Entity array:** at `DGROUP:0x70a0`, stride `0x15a`. Animation code at `seg4:0x216a`–`0x21a7`.
- **Dynamic loading:**
  - `<Language>_LANG.DLL` at `seg10:0x6714` (handle `DGROUP:0x9212`).
  - `ISPACE.DLL` at `seg10:0x67c0` (handle `DGROUP:0x9210`), with GetProcAddress by ordinal ×23: ordinals 1–10, 12, 14, 15, 17, 18, 24, 29, 32–36 and 41.
  - `\CTL3D.DLL` at `seg2:0x19e0`.
- **Data files:**
  - `MASTER.BOD`: 52×44-byte records plus 16 zero bytes. `01 00 01 00` sits at offset 21. The word at offset 33 is the IGRAPHIC `#4000` resource ID of the ship body.
  - `MASTER.SHP`: 118×65-byte records plus 10 pad bytes. Name at offset 10, class at 31, body (BOD index) at 33, author at 35, sound base name at 56.

## Interpretations that need verification before use
- Ship struct (stride `0x70`): `+0x04`/`+0x06` = X/Y, `+0x18` = active flag, `+0x51` = owner (values 1/2), `+0x5e`/`+0x62`/`+0x66` = behaviour function pointers.
- `DGROUP:0x1df8` = table of per-player ship-array far pointers. The entry count is unresolved.
- `DGROUP:0x1e34` = camera/viewport object (it is also used with an index).
- Roles of the WndProc (`seg2:0x0224`, export ordinal 1), the game tick, the command handler, and a "196-case input switch".
- The scoring formulas, and the accumulators at `DGROUP:0x1e58` and `DGROUP:0x5078`.
- The game-mode global: `DGROUP:0x9684` (values 1/2) vs `DGROUP:0x9320` (compared with 6).
- Everything in the Hermes capture-mechanic model. Its field attributions don't match the bytes.
