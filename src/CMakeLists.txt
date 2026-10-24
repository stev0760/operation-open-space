# Portable readers for the original's file formats (no Win32 dependency).
add_library(isformats STATIC
  formats/bytes.cpp
  formats/ne.cpp
  formats/res16.cpp
  formats/dib.cpp)
target_include_directories(isformats PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
