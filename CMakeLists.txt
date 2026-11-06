cmake_minimum_required(VERSION 3.20)
project(open_space LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(IS_WERROR "Treat warnings as errors" ON)
# The opl3 music synth (ADR 0011 decision 3): the Nuked OPL3 chip emulator (third_party/nuked-opl3,
# LGPL-2.1-or-later, built as the C library nukedopl3) under the port's own General MIDI voice
# driver, with the Fat Man 2-operator bank (third_party/fatman-2op, MIT). OFF builds the stub
# make_opl3 (returns nullptr) and does not compile nukedopl3.
# CMakePresets.json sets it ON in every preset, so a build directory whose cache still holds the
# stage 2 OFF picks it up on its next preset configure; -DIS_OPL3=OFF on the command line wins
# (M7 s2b fixer).
option(IS_OPL3 "Build the opl3 music synth (Nuked OPL3 + the Fat Man bank)" ON)
set(IS_ORIGINAL_DIR "${CMAKE_SOURCE_DIR}/work/extracted/shipdisk" CACHE PATH
    "Expanded original game files, read by the golden tests (skipped when absent)")
set(IS_ASSETS_DIR "${CMAKE_SOURCE_DIR}/work/assets" CACHE PATH
    "isre res output, the reference for the golden tests (skipped when absent)")

add_compile_options(-Wall -Wextra -Wpedantic $<$<BOOL:${IS_WERROR}>:-Werror>)
if(WIN32)
  add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0601)
  add_link_options(-static -static-libgcc -static-libstdc++)
endif()

enable_testing()
if(IS_OPL3)
  enable_language(C)
  add_subdirectory(third_party/nuked-opl3)
endif()
add_subdirectory(src)
add_subdirectory(tests)
if(WIN32)
  add_subdirectory(tools)
endif()
