# TinySoundFont

- **Upstream:** https://github.com/schellingb/TinySoundFont
- **Version:** commit `853a0a171759f1ddba0de1442133a75912bbeffa` (2026-07-19)
- **Licence:** MIT (`LICENSE`, copied from upstream unchanged).
- **Copied:** `tsf.h` and `LICENSE` only. `tml.h` (the upstream MIDI file loader) is not taken: the port has its own MIDI player (`src/audio/midi_player.*`).
- **Changed:** nothing. `tsf.h` SHA-256 `70d55963c98f60ebb81518eaa1f25d46888d5180eb5f5289fd6b74ffc177d197`.
- **Used by:** `src/audio/music_soundfont.cpp`, the one translation unit that defines `TSF_IMPLEMENTATION` (ADR 0011 decision 3, the `soundfont` music backend).
- **Warnings:** the header compiles clean under `-Wall -Wextra -Wpedantic -Werror` with GCC 16 and MinGW-w64 GCC 16 (x86_64 and i686), at `-O0` and `-O2`; the including file still wraps it in a diagnostic push/pop so a newer compiler's warnings stay out of the port's build.
