# ADR 0003: Text and paths

- **Status:** Accepted. [DESIGN]
- **Scope:** every string the port reads, shows or writes, and every file path, from the M1 asset store on. The filesystem walk's own rules (8.3 names, long names, what the walk enters) belong to M3's walk ADR. [DESIGN]

## Context
- **The original's text is cp1252.** Its strings, dialogs and menus are byte strings in `ENG_LANG.DLL` ([resources.md](../re/resources.md#win16-layouts)). Its INI keys and saved games hold byte strings too ([formats.md](../re/formats.md#conventions)). [DESIGN]
- **The original's paths are short ANSI paths.** It walks drives with the DOS findfirst/findnext calls ([pitfalls.md](../re/pitfalls.md#reading-code)), and builds paths in fixed-size buffers ([formats.md](../re/formats.md#file-locations)). [DESIGN]
- **Modern Windows paths are UTF-16.** A user's game directory, or a directory the game walks, can hold names that no ANSI code page represents. The ANSI APIs replace those characters, and such a path then names nothing. [REF Win32 SDK, Unicode in the Windows API]
- **The port's readers already work in cp1252 bytes and convert at the edge.** `src/formats/` keeps resource text as cp1252 `std::string` and converts it with `cp1252_to_utf16`. [DESIGN]

## Decision
1. **Win32 calls use the W functions only.** The build defines `UNICODE` and `_UNICODE`, as it does today. [DESIGN]
2. **Game text stays cp1252 inside the port's data structures.** It converts to UTF-16 only at the point where it reaches a Win32 call: drawing, dialog templates, window titles. Game rules that count or compare characters see the same bytes as the original. [DESIGN]
3. **Paths are `std::filesystem::path`, which holds UTF-16 on Windows.** No path passes through a narrow string. [DESIGN]
4. **Files the original also reads keep its encoding.** This covers INSPACE.INI, saved games, MASTER.BOD and MASTER.SHP, which hold cp1252. A text the user types that cp1252 cannot hold (a pilot name, for example) is refused with a message. It is never stored lossily. [DESIGN]
5. **The port's own files are UTF-8.** These are its log and any file the original does not have. [DESIGN]

## Alternatives
- **UTF-8 throughout, with the process code page set to UTF-8:** this needs Windows 10 1903 or later, and the game files would still be cp1252. [DESIGN]
- **Converting the game text to UTF-16 at load:** this doubles the conversion points where game rules read text, and moves those rules away from the original's byte semantics. [DESIGN]

## Consequences
- The asset store returns cp1252 text, and the shell (M2) converts it for display. [DESIGN]
- M3's walk ADR decides which names the walk shows and enters; this ADR only guarantees that every path stays representable. [DESIGN]
