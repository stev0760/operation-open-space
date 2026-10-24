cmake_minimum_required(VERSION 3.20)
project(innerspace LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(IS_WERROR "Treat warnings as errors" ON)
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
