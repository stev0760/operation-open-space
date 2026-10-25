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
if(WIN32)
  file(GLOB IS_APP_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/app/*.cpp)
  if(IS_APP_SOURCES AND TARGET isshell)
    add_executable(inspace WIN32 ${IS_APP_SOURCES})
    target_link_libraries(inspace PRIVATE isshell gdi32 user32 shell32 winmm)
    target_link_options(inspace PRIVATE -municode)
  endif()
endif()
