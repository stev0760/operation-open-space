# Portable readers for the original's file formats (no Win32 dependency). Every
# formats/*.cpp belongs to the library, so parallel work never edits this file.
file(GLOB IS_FORMATS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/formats/*.cpp)
add_library(isformats STATIC ${IS_FORMATS_SOURCES})
target_include_directories(isformats PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# The asset store over the user's original files (ADR 0002); portable as well.
file(GLOB IS_ASSETS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/*.cpp)
if(IS_ASSETS_SOURCES)
  add_library(isassets STATIC ${IS_ASSETS_SOURCES})
  target_link_libraries(isassets PUBLIC isformats)
endif()

# INSPACE.INI for the port (ADR 0004); portable.
file(GLOB IS_SETTINGS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/settings/*.cpp)
if(IS_SETTINGS_SOURCES)
  add_library(issettings STATIC ${IS_SETTINGS_SOURCES})
  target_link_libraries(issettings PUBLIC isformats)
endif()

# The shell (M2): the portable parts of the game loop, state machine, command tables and
# geometry live in shell/*.cpp and are tested on the host. The Win32 program is app/*.cpp.
file(GLOB IS_SHELL_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shell/*.cpp)
if(IS_SHELL_SOURCES)
  add_library(isshell STATIC ${IS_SHELL_SOURCES})
  target_link_libraries(isshell PUBLIC isformats)
  foreach(_dep isassets issettings)
    if(TARGET ${_dep})
      target_link_libraries(isshell PUBLIC ${_dep})
    endif()
  endforeach()
endif()
# The renderer (ADR 0006, ADR 0007): palette, surface, images, font and presenter. The portable
# parts build and are tested on the host; the GDI parts are guarded by _WIN32.
file(GLOB IS_GFX_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/gfx/*.cpp)
if(IS_GFX_SOURCES AND TARGET isshell)
  add_library(isgfx STATIC ${IS_GFX_SOURCES})
  target_link_libraries(isgfx PUBLIC isformats isshell)
  if(TARGET isassets)
    target_link_libraries(isgfx PUBLIC isassets)
  endif()
  if(WIN32)
    target_link_libraries(isgfx PUBLIC gdi32)
  endif()
endif()
# The intro (state 0, stage 2): the phase logic, texts and bolt are portable and tested on the
# host; the GDI canvas is guarded by _WIN32.
file(GLOB IS_INTRO_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/intro/*.cpp)
if(IS_INTRO_SOURCES AND TARGET isgfx)
  add_library(isintro STATIC ${IS_INTRO_SOURCES})
  target_link_libraries(isintro PUBLIC isshell isgfx isformats)
endif()
# The parts that hook into the program from outside src/app (stage 3): the dialogs, the help
# window and the briefing. Each directory is a library of its *.cpp, so parallel work never
# edits this file; src/app/app.h declares their hook points (install_*_hooks).
set(IS_PART_LIBS)
foreach(_part audio dialogs help briefing nav install desk)
  file(GLOB _part_sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${_part}/*.cpp)
  if(_part_sources AND TARGET isgfx)
    add_library(is${_part} STATIC ${_part_sources})
    target_link_libraries(is${_part} PUBLIC isshell isgfx isformats)
    foreach(_dep isassets issettings isintro isaudio)
      if(TARGET ${_dep} AND NOT _dep STREQUAL is${_part})  # isaudio: built first (audio), linked by the parts
        target_link_libraries(is${_part} PUBLIC ${_dep})
      endif()
    endforeach()
    if(WIN32)
      target_link_libraries(is${_part} PUBLIC gdi32 user32 comdlg32 shell32 winmm)
    endif()
    list(APPEND IS_PART_LIBS is${_part})
  endif()
endforeach()
# The game-start flow hands Navigation Enter's directory to the briefing (src/dialogs/hooks.cpp).
if(TARGET isdialogs AND TARGET isnav AND TARGET isbriefing)
  target_link_libraries(isdialogs PUBLIC isnav isbriefing)
endif()
if(WIN32)
  file(GLOB IS_APP_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/app/*.cpp)
  if(IS_APP_SOURCES AND TARGET isshell)
    # The application manifest (per-monitor DPI awareness v2, ADR 0006 decision 4) goes in
    # through app/inspace.rc.
    enable_language(RC)
    add_executable(inspace WIN32 ${IS_APP_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/app/inspace.rc)
    target_link_libraries(inspace PRIVATE ${IS_PART_LIBS} isshell gdi32 user32 shell32 ole32 winmm)
    if(TARGET isgfx)
      target_link_libraries(inspace PRIVATE isgfx)
    endif()
    if(TARGET isintro)
      target_link_libraries(inspace PRIVATE isintro)
    endif()
    if(TARGET isaudio)
      target_link_libraries(inspace PRIVATE isaudio)
    endif()
    target_link_options(inspace PRIVATE -municode)
  endif()
endif()
