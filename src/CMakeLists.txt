# Portable readers for the original's file formats (no Win32 dependency). Every
# formats/*.cpp belongs to the library, so parallel work never edits this file.
file(GLOB IS_FORMATS_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/formats/*.cpp)
add_library(isformats STATIC ${IS_FORMATS_SOURCES})
target_include_directories(isformats PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
