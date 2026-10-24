# Runs cross-built test executables under Wine. Win32 programs need no Win16 support,
# so any Wine 11 build works; IS_WINE overrides the choice.
find_program(IS_WINE NAMES wine HINTS /opt/wine/bin /opt/wine-11.0/bin)
if(IS_WINE)
  set(CMAKE_CROSSCOMPILING_EMULATOR "${IS_WINE}")
endif()
