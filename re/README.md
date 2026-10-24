# RE tools

`re/isre/` is the Python package and CLI that turns the original game files into everything under `work/`. RE facts live in `docs/re/`; this page covers only how to run the tools.

## Setup
```
python3.9 -m venv re/.venv
re/.venv/bin/pip install pytest capstone
re/.venv/bin/pip install -e re
```
Run Python with `-I` whenever it reads the original files.

## Regenerate everything
```
re/.venv/bin/isre all --original original --work work
```
A second run writes nothing. Outputs:

| Directory | Content | Command |
|---|---|---|
| `work/extracted/shipdisk/`, `theworks/` | distribution files, SZDD expanded, names restored | `isre extract` |
| `work/ne/<m>.json` | NE header and tables | `isre ne` |
| `work/unpacked/<m>/` | `segNN.bin`, `relocs.json` per OPTLOADER module | `isre unpack` |
| `work/rebuilt/<m>.exe` / `.dll` | unpacked NE with standard relocation records, checked by the symbolic loader | `isre rebuild` |
| `work/assets/<module>/` | resources: strings, dialogs, menus, bitmaps, WAVs | `isre res` |
| `work/asm/<m>/` | listings `segNN.asm`, `functions.json`, `xrefs.json` for `inspace`, `ispace`, `factory-1998a` | `isre asm` |

The help files are extracted separately: `isre hlp` (see `isre hlp --help`) into `work/manual/2000` and `work/manual/1998`. The Ghidra pipeline (`re/ghidra/run.sh`, which writes `work/ghidra/` and `work/decomp/<m>/`) is described in [docs/re/ghidra.md](../docs/re/ghidra.md).

## Reading code
- **One function:** `isre asm --work work --module inspace --show seg7:0x0` prints the whole function holding the address, with its shared tails, unreachable blocks and jump tables.
- **Analyst context pack:** `isre pack seg10:0x67c0 [--module ispace]` writes one markdown file of at most 30 KB. It holds the function's header (callers, callees, seeds), its listing, the Ghidra C with addresses converted to `segN:0xOFF`, the DGROUP references, the string ids it loads and every line of `docs/re/` and `oracle/digests/` that cites an address inside it. `isre pack DGROUP:0x1260` gives a data pack: bytes, string, references.
- **Addresses:** `isre addr` converts between `segN:0xOFF`, Ghidra `10xx:OFF`, DGROUP offsets, file offsets and rea `dos-com` addresses.

## Listing format
- `; ---- function segN:0xOFF  [seeds]  N insns  returns …` opens a function, and `; ---- unreachable code` marks a block nothing reaches.
- An instruction line has the address, the bytes (27 characters), the instruction and, from column 73, a comment: `; <- …` lists the sites that jump or call here, `DGROUP:0x…` names a data reference, and relocations name their import or target.
- `; ....` in `--show` output stands for a stretch of other functions skipped between two pieces of the shown one.
- `isre.asm.parse_line` reads a listing line back into its fields.

## Tests
`re/.venv/bin/python -m pytest re/tests`, then, as a separate run, `re/.venv/bin/python -m pytest oracle/tests`. Tests that need `original/` or `work/` skip when those are absent.
