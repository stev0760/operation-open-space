# MinGW-w64 cross build for 32-bit Windows; tests run under Wine.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)
set(CMAKE_C_COMPILER i686-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER i686-w64-mingw32-g++)
set(CMAKE_RC_COMPILER i686-w64-mingw32-windres)
include(${CMAKE_CURRENT_LIST_DIR}/wine-emulator.cmake)
