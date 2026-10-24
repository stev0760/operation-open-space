# Source after oracle/env.sh: relay tracing limited to the game's 16-bit API use.
# Usage:
#   ORACLE_ID=1 source oracle/env.sh; source oracle/trace.sh
#   oracle_trace_config          # once per prefix: writes the relay filter to the registry
#   oracle_crash_config          # once per prefix: winedbg writes crash reports to C:\winedbg.txt
#   oracle_game_start LOG        # starts INSPACE.EXE in the background, trace to LOG
#   oracle_game_stop             # wineserver -k for this prefix only
#   oracle_crash_collect LOG     # after the stop: appends C:\winedbg.txt to LOG
#   oracle_trace_clear           # removes the relay filter from the registry
#
# Filter (HKCU\Software\Wine\Debug, read by both the 32/64-bit relay in ntdll and
# the 16-bit relay in krnl386.exe16):
#   RelayInclude = every builtin 16-bit module as MODULE.* (see ORACLE_RELAY_INCLUDE).
#     16-bit modules are named KERNEL, USER, ... while the PE DLLs are kernel32,
#     user32, ..., so no 32-bit export matches and the PE relay stays silent.
#     Calls into the game's own NE DLLs (ISPACE, IGRAPHIC, CTL3D, WAVEMIX) are
#     native code and are never relayed.
#   RelayExclude (ORACLE_RELAY_EXCLUDE) = the calls of two loops plus a few
#     bookkeeping calls. relay_digest.py prints the exclude list a log was made with, as
#     far as Wine's load_list line shows it (some 300 characters); the scenarios log the
#     whole list in steps.txt.
#     - The idle/frame-pacing loop, ~65k iterations per second (10 MB/s): PEEKMESSAGE,
#       GETDC, SELECTPALETTE, GETCURRENTTIME, TIMEGETTIME, RELEASEDC.
#     - The wave frame loop, ~11.5k iterations per second (6.7 MB/s, digest
#       newgame-0002): GETASYNCKEYSTATE (VK_SHIFT, VK_SPACE and the arrow keys),
#       SELECTCLIPRGN and PTINRECT. Keys still show as WM_KEYDOWN/WM_CHAR in the
#       CallTo16 lines of the window procedures. To trace the key polling, set
#       ORACLE_RELAY_EXCLUDE to the list below without USER.GETASYNCKEYSTATE.
#     ORACLE_RELAY_PROFILE=wave (oracle/scenarios/wave.sh) also excludes the drawing
#     calls of the wave's full status display (ORACLE_RELAY_EXCLUDE_WAVE): some 20 000
#     SETPIXEL per second plus PATBLT, SELECTOBJECT, BITBLT, SETRECT, INTERSECTRECT,
#     FRAMERECT, GETPIXEL, the GETWINDOWTASK/GETACTIVEWINDOW focus checks of the frame
#     loop and the GETKEYSTATE polling of the ambulance dialog, 5-8 MB/s together
#     (digest wave-0001). TEXTOUT, resources, windows, dialogs, files, INI and sound
#     calls stay in the trace. A set ORACLE_RELAY_EXCLUDE overrides both profiles.
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
# The game's stdout and stderr both go to LOG, including Wine's one-line
# "wine: Unhandled ..." message on a crash. With the default profile a trace grows by
# some 0.3 MB/s up to Enter, 0.6 MB/s in the wave until the full status display is
# drawn and 5-8 MB/s after that; the wave profile keeps the wave at 0.05-0.2 MB/s
# (digest wave-0001).
# Crash reports: Wine starts the AeDebug debugger, winedbg --auto, on an unhandled
# exception. winedbg is a console program started by a process without a console,
# so it gets a console of its own and its report never reaches LOG.
# oracle_crash_config therefore runs it through cmd with its output redirected
# to C:\winedbg.txt (both registry views, as the game runs in a 32-bit process),
# and oracle_crash_collect appends that file to LOG after the game has stopped.

ORACLE_RELAY_INCLUDE="${ORACLE_RELAY_INCLUDE:-KERNEL.*;USER.*;GDI.*;MMSYSTEM.*;SHELL.*;KEYBOARD.*;SOUND.*;SYSTEM.*;DISPLAY.*;MOUSE.*;WIN87EM.*;TOOLHELP.*;COMMDLG.*;LZEXPAND.*;DDEML.*;VER.*;WING.*;WINSOCK.*}"
ORACLE_RELAY_PROFILE="${ORACLE_RELAY_PROFILE:-default}"
ORACLE_RELAY_EXCLUDE_WAVE="GDI.SETPIXEL;GDI.GETPIXEL;GDI.PATBLT;GDI.SELECTOBJECT;GDI.BITBLT;USER.SETRECT;USER.INTERSECTRECT;USER.FRAMERECT;USER.GETWINDOWTASK;USER.GETACTIVEWINDOW;USER.GETKEYSTATE"
if [ -z "${ORACLE_RELAY_EXCLUDE:-}" ]; then
    ORACLE_RELAY_EXCLUDE="USER.PEEKMESSAGE;USER.GETDC;USER.RELEASEDC;USER.SELECTPALETTE;USER.GETCURRENTTIME;MMSYSTEM.TIMEGETTIME;KERNEL.GETCURRENTTASK;KERNEL.LOCKSEGMENT;KERNEL.UNLOCKSEGMENT;KERNEL.GLOBALHANDLE;USER.GETFREESYSTEMRESOURCES;USER.GETASYNCKEYSTATE;GDI.SELECTCLIPRGN;USER.PTINRECT"
    case "$ORACLE_RELAY_PROFILE" in
        default) ;;
        wave) ORACLE_RELAY_EXCLUDE="$ORACLE_RELAY_EXCLUDE;$ORACLE_RELAY_EXCLUDE_WAVE" ;;
        *) echo "oracle/trace.sh: unknown ORACLE_RELAY_PROFILE $ORACLE_RELAY_PROFILE (default, wave)" >&2 ;;
    esac
fi
ORACLE_TRACE_DEBUG="${ORACLE_TRACE_DEBUG:-+relay,+int21,+file,+timestamp}"
ORACLE_CRASH_FILE='C:\winedbg.txt'

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

oracle_crash_config() {  # winedbg --auto output to C:\winedbg.txt, no crash dialog
    local view
    for view in 32 64; do
        wine reg add 'HKLM\Software\Microsoft\Windows NT\CurrentVersion\AeDebug' /v Debugger /t REG_SZ \
            /d "cmd /c winedbg --auto %ld %ld >$ORACLE_CRASH_FILE 2>&1" /f /reg:$view >/dev/null
        wine reg add 'HKLM\Software\Microsoft\Windows NT\CurrentVersion\AeDebug' /v Auto /t REG_SZ \
            /d 1 /f /reg:$view >/dev/null
    done
    wine reg add 'HKCU\Software\Wine\WineDbg' /v ShowCrashDialog /t REG_DWORD /d 0 /f >/dev/null
    rm -f "$WINEPREFIX/drive_c/winedbg.txt"
    wineserver -w
}

oracle_crash_collect() {  # oracle_crash_collect LOG  — appends and removes C:\winedbg.txt
    local f="$WINEPREFIX/drive_c/winedbg.txt"
    [ -f "$f" ] || return 0
    { echo "oracle: winedbg --auto output follows"; tr -d '\r' <"$f"; } >>"$1"
    rm -f "$f"
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
