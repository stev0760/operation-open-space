# Nuked OPL3

- **Upstream:** https://github.com/nukeykt/Nuked-OPL3
- **Version:** commit `765ec962e473aeb767e4cba74ffdc8f588ffbfe8` (2026-08-24, "remove NEW bit checks for 4-op channels"); the source's own header says version 1.8.
- **Licence:** GNU LGPL 2.1 or later (`LICENSE`, the LGPL 2.1 text, copied from upstream unchanged; the "or later" grant is in the header of `opl3.c` and `opl3.h`). The port links it statically. Once the port's source is published with its binaries, a user can rebuild and relink the port against a modified Nuked OPL3, which meets LGPL 2.1 section 6; every binary distribution must then ship this LGPL text and point to that source. The repository has no public remote yet, so how a release meets this is [OPEN] for the release packaging (M7 s2b fixer).
- **Copied:** `opl3.c`, `opl3.h` and `LICENSE`, nothing else.
- **Changed:** nothing.

| File | SHA-256 |
|---|---|
| `opl3.c` | `59eb873fdb6d52bc7977a0fcbb97c1bae03dd71cc88e3acddbcc01b7121bfe03` |
| `opl3.h` | `a84266b8d71a4929f15f573afbe407fd24310b77757d855835dacabf68763679` |
| `LICENSE` | `20c17d8b8c48a600800dfd14f95d5cb9ff47066a9641ddeab48dc54aec96e331` |

- **Build:** `CMakeLists.txt` here (the port's own file) builds the C99 static library `nukedopl3`, only when the CMake option `IS_OPL3` is on (the default). The sources compile clean under the port's `-Wall -Wextra -Wpedantic` with GCC 16 and MinGW-w64 GCC 16 (x86_64 and i686) at `-O0` to `-O3`; the target adds `-Wno-error` (GCC and Clang) and sets `COMPILE_WARNING_AS_ERROR` off so that a newer compiler's warnings in this code do not fail the build.
- **Used by:** `src/audio/music_opl3.cpp`, the opl3 music synth (ADR 0011 decision 3).
