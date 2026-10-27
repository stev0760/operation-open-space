# Nuked OPL3 (LGPL-2.1-or-later), the chip emulator under the opl3 music synth (ADR 0011
# decision 3). The upstream sources compile unchanged; the warnings they raise under the port's
# flags are relaxed on this target only.
add_library(nukedopl3 STATIC opl3.c)
set_target_properties(nukedopl3 PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_include_directories(nukedopl3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# CMAKE_COMPILE_WARNING_AS_ERROR would append -Werror after the options; the property keeps it
# off on this target (CMake 3.24+, ignored before) (M7 s2b fixer). -Wno-error is the GCC and
# Clang spelling; MSVC gets none until that build exists.
set_target_properties(nukedopl3 PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
target_compile_options(nukedopl3 PRIVATE $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wno-error>)  # [DESIGN] clean today under -Wall -Wextra -Wpedantic (GCC 16, MinGW-w64 GCC 16, -O0..-O3); a newer compiler warns without failing the build
