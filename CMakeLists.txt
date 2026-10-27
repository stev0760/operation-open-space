cmake_minimum_required(VERSION 3.20)
project(innerspace LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(IS_WERROR "Treat warnings as errors" ON)
# The opl3 music synth (ADR 0011 decision 3). OFF: libADLMIDI, the candidate, is GPL-3.0-or-later
# in its core (src/adlmidi*.cpp, include/adlmidi.h at tag v1.6.2.1), outside the port's
# third-party licences (MIT, BSD, zlib, public domain, LGPL), and its Windows 3.x AdLib bank is
# unlicensed (grey zone). No OPL3 synth is vendored, so the option cannot be turned on yet;
# src/audio/music_opl3.cpp's make_opl3 returns nullptr.
option(IS_OPL3 "Build the opl3 music synth (needs a vendored OPL3 synth; none yet)" OFF)
if(IS_OPL3)
  message(FATAL_ERROR "IS_OPL3: no OPL3 synth is vendored (see the comment above the option)")
endif()
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
add_subdirectory(src)
add_subdirectory(tests)
if(WIN32)
  add_subdirectory(tools)
endif()
