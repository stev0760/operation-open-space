# ADR 0002: Asset source

- **Status:** Accepted. [DESIGN]
- **Scope:** the asset store of milestone M1 and every later milestone that loads the original's files. [DESIGN]

## Context
- **The port ships no original files.** The user supplies their own copy, and the port reads it at run time. [DESIGN]
- **The original reads its files from its own directory.** It loads `ISPACE.DLL` from the game directory and the language DLL named by the INI key `Language` ([ispace.md](../re/ispace.md#loading-and-unloading)). It finds the ship files through the INI key `ShipPath` ([formats.md](../re/formats.md#file-locations)). [DESIGN]
- **The distribution differs from an installed copy.** Most shipdisk files are SZDD-compressed under changed names (`ENG_LANG.dl1`, `INSPACE.ex_`), and the installer expands and renames them. Under Wine 11 the installer crashes, so a user with only the distribution files cannot install them that way. `isre extract` restores the names from an extension table and the SZDD header's stored character (`re/isre/szdd.py`). [DESIGN]
- **There is more than one version.** The shipdisk v1.29.08, the THEWORKS add-on and, later, the user's registered copy differ ([PLAN](../PLAN.md)). [DESIGN]

## Decision
1. **Where:** the port looks in two places. First, the directory that holds its executable. Second, a directory named on the command line or in the port's own INI, which M2 defines. The first directory that holds the game's modules wins. [DESIGN]
2. **Which forms:** an installed directory (expanded files under their installed names), or a distribution directory (the files as shipped). For a distribution directory the asset store expands SZDD in memory and restores the names as `isre extract` does. Archives (`.zip`) are not read. [DESIGN]
3. **Names:** lookup ignores case, because the files mix cases (`ISPACE.dll`, `eng_fx.dll`). [DESIGN]
4. **Versions:** a table of known SHA-256 values per file and version identifies the copy. The table is generated from `original.sha256` and gains the registered copy's files later.
   - An unknown file or version is reported in `isview` and the port's log, but still used.
   - This keeps ships edited with the Ship Factory loadable.

   [DESIGN]
5. **Read-only:** the port writes into the game directory only where the original writes: the Ship Factory's MASTER.BOD and MASTER.SHP (M8) and saved games under `LocalPath` (M4), as their ADRs decide. It never copies, converts or uploads the files. [DESIGN]
6. **Language:** the language module is `<Language>_LANG.DLL`, with the `Language` key's value and ENG by default, as in the original. [DESIGN]

## Alternatives
- **An installed directory only:** simpler, but a user with only the distribution files would need the original installer, which fails under Wine 11. [DESIGN]
- **Reading the archives directly:** needs a ZIP reader, and the archive a seller provides can differ from the one at hand. [DESIGN]
- **Converting the assets at setup:** duplicates the user's files and loses byte fidelity. [DESIGN]

## Consequences
- The asset store (M1 stage 2) implements SZDD expansion, the name restoration and the version table. `isview` and the port share it. [DESIGN]
- The registered copy adds rows to the version table and needs no other change. [DESIGN]
