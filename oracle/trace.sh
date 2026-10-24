# Source after oracle/env.sh: relay tracing limited to the game's 16-bit API use.
# Usage:
#   ORACLE_ID=1 source oracle/env.sh; source oracle/trace.sh
#   oracle_trace_config          # once per prefix: writes the relay filter to the registry
#   oracle_game_start LOG        # starts INSPACE.EXE in the background, trace to LOG
#   oracle_game_stop             # wineserver -k for this prefix only
#   oracle_trace_clear           # removes the relay filter from the registry
#
# Filter (HKCU\Software\Wine\Debug, read by both the 32/64-bit relay in ntdll and
# the 16-bit relay in krnl386.exe16):
#   RelayInclude = every builtin 16-bit module as MODULE.* (see ORACLE_RELAY_INCLUDE).
#     16-bit modules are named KERNEL, USER, ... while the PE DLLs are kernel32,
#     user32, ..., so no 32-bit export matches and the PE relay stays silent.
#     Calls into the game's own NE DLLs (ISPACE, IGRAPHIC, CTL3D, WAVEMIX) are
#     native code and are never relayed.
#   RelayExclude = the idle/frame-pacing loop, which otherwise logs ~65k
#     iterations per second (10 MB/s): PEEKMESSAGE, GETDC, SELECTPALETTE,
#     GETCURRENTTIME, TIMEGETTIME, RELEASEDC (ORACLE_RELAY_EXCLUDE), plus a few
#     bookkeeping calls. relay_digest.py prints the exclude list it was given.
# WINEDEBUG = +relay,+int21,+file,+timestamp (ORACLE_TRACE_DEBUG):
#   +relay      16-bit API calls with arguments and return values, plus CallTo16
#               lines (window procedures, timer callbacks).
#   +int21      DOS3CALL services decoded by Wine, with path strings
#               (open, findfirst/findnext, chdir, mkdir, getcwd ...).
#   +file       kernelbase/ntdll file traces; the one relay_digest.py uses is
#               FindNextFileW "returning NAME (SHORT)", which names each entry
#               the int21 FINDFIRST/FINDNEXT emulation hands to the game.
#   +timestamp  a seconds.milliseconds tick count on every line, for timer
#               intervals and step timing.
# The game's stdout and stderr both go to LOG, so oracle/shim messages and a
# winedbg crash dump land there too. The newgame scenario (72 s) writes about 20 MB.

ORACLE_RELAY_INCLUDE="${ORACLE_RELAY_INCLUDE:-KERNEL.*;USER.*;GDI.*;MMSYSTEM.*;SHELL.*;KEYBOARD.*;SOUND.*;SYSTEM.*;DISPLAY.*;MOUSE.*;WIN87EM.*;TOOLHELP.*;COMMDLG.*;LZEXPAND.*;DDEML.*;VER.*;WING.*;WINSOCK.*}"
ORACLE_RELAY_EXCLUDE="${ORACLE_RELAY_EXCLUDE:-USER.PEEKMESSAGE;USER.GETDC;USER.RELEASEDC;USER.SELECTPALETTE;USER.GETCURRENTTIME;MMSYSTEM.TIMEGETTIME;KERNEL.GETCURRENTTASK;KERNEL.LOCKSEGMENT;KERNEL.UNLOCKSEGMENT;KERNEL.GLOBALHANDLE;USER.GETFREESYSTEMRESOURCES}"
ORACLE_TRACE_DEBUG="${ORACLE_TRACE_DEBUG:-+relay,+int21,+file,+timestamp}"

oracle_trace_config() {  # writes RelayInclude/RelayExclude for $WINEPREFIX
    wine reg add 'HKCU\Software\Wine\Debug' /v RelayInclude /t REG_SZ /d "$ORACLE_RELAY_INCLUDE" /f >/dev/null
    wine reg add 'HKCU\Software\Wine\Debug' /v RelayExclude /t REG_SZ /d "$ORACLE_RELAY_EXCLUDE" /f >/dev/null
    wineserver -w
}

oracle_trace_clear() {
    wine reg delete 'HKCU\Software\Wine\Debug' /v RelayInclude /f >/dev/null 2>&1
    wine reg delete 'HKCU\Software\Wine\Debug' /v RelayExclude /f >/dev/null 2>&1
    wineserver -w
}

oracle_game_start() {  # oracle_game_start LOG [game-dir-under-drive_c]  — runs untraced if LOG is "-"
    local log="$1" dir="${2:-INSPACE}"
    [ "$log" = "-" ] || log="$(realpath -m "$log")"   # the game starts from its own directory
    if [ "$log" = "-" ]; then
        (cd "$WINEPREFIX/drive_c/$dir" && setsid -f wine INSPACE.EXE >/dev/null 2>&1)
    else
        (cd "$WINEPREFIX/drive_c/$dir" && WINEDEBUG="$ORACLE_TRACE_DEBUG" setsid -f wine INSPACE.EXE >"$log" 2>&1)
    fi
}

oracle_game_stop() {
    wineserver -k 2>/dev/null
    wineserver -w 2>/dev/null
}
