#!/usr/bin/env bash
# Scenario install: the original INSTALL.EXE in a fresh prefix, driven through its dialogs,
# then the installed game's title screen. Digest: install-0001.
#
# Usage: ORACLE_ID=9 oracle/scenarios/install.sh [OUTDIR]
#   OUTDIR defaults to work/oracle/install. Output: NN-*.png per dialog, 09-title.png of
#   the installed game, steps.txt, trace.log (the installer, TRACE=1), game-trace.log
#   (the game to its title), INSPACE.INI and system.ini as the installer left them,
#   files.txt (the installed tree under C:\SPACE).
# Environment (all optional; ORACLE_WINE and ORACLE_ID as in oracle/env.sh):
#   SRC=work/registered/zip   the install files; original/shipdisk when that is absent
#   NAME=Pilot ORG=Home       the personalization dialog's fields
#   FRESH=1                   0 keeps the prefix (an "installed before" question appears)
#   TRACE=1                   0 runs the installer untraced
#   WAIT_TITLE=18             seconds from the game's launch to its title shot
# The installer validates the install files ("VALID INSTALL DISK"), copies the game to
# C:\SPACE, the INI, screen saver and WAVEMIX to C:\windows, and after the personalization
# dialog registers the copy through ISPACE.DLL (SYSTEM.INI [386Enh] vidsp and 32BitSP,
# WIN.INI [windows] PDev). The game then starts as Plus Edition [ORACLE install-0001].
# Screen positions assume a 1024x768 display; the dialogs centre themselves.
set -u
OUT_DEFAULT=install
# shellcheck source=common.sh
source "$(dirname "${BASH_SOURCE[0]}")/common.sh" "$@"
SRC="${SRC:-$REPO/work/registered/zip}"
[ -d "$SRC" ] || SRC="$REPO/original/shipdisk"
NAME="${NAME:-Pilot}"; ORG="${ORG:-Home}"; FRESH="${FRESH:-1}"; WAIT_TITLE="${WAIT_TITLE:-18}"

# wait_windows NAME COUNT [SECS]: until COUNT visible windows are named exactly NAME.
wait_windows() {
    local i n
    for i in $(seq $(( ${3:-60} * 4 ))); do
        n=$(xdotool search --onlyvisible --name "^$1\$" 2>/dev/null | wc -l)
        [ "$n" -ge "$2" ] && return 0
        sleep 0.25
    done
    log "timeout: $2 windows named '$1' not seen in ${3:-60} s"; return 1
}
# message_box: the installer's splash is also named "IS Install", so a box makes two.
message_box() { wait_windows "IS Install" 2 "${1:-60}"; }

oracle_game_stop
if [ "$FRESH" = 1 ]; then rm -rf "$WINEPREFIX"; wineboot -u >/dev/null 2>&1; wineserver -w; fi
rm -f "$WINEPREFIX/dosdevices/z:"
oracle_xvfb "$DEPTH"
cd "$WINEPREFIX/drive_c" || exit 1
wine reg delete 'HKCU\Software\Wine\DllOverrides' /f >/dev/null 2>&1
oracle_crash_config
if [ "$TRACE" = 1 ]; then oracle_trace_config; else oracle_trace_clear; fi
mkdir -p "$WINEPREFIX/drive_c/temp"; cp "$SRC"/* "$WINEPREFIX/drive_c/temp/"
log "prefix $WINEPREFIX display $DISPLAY wine $ORACLE_WINE ($(wine --version)) src $SRC fresh $FRESH trace $TRACE"

touch "$OUT/.launch"
cd "$WINEPREFIX/drive_c/temp" || exit 1
if [ "$TRACE" = 1 ]; then WINEDEBUG="$ORACLE_TRACE_DEBUG" setsid -f wine INSTALL.EXE >"$OUT/trace.log" 2>&1
else setsid -f wine INSTALL.EXE >/dev/null 2>&1; fi
log "launch INSTALL.EXE"

message_box 90 || exit 1; sleep 1; shot 01-warning; key Return
# A fresh prefix still gets the "installed before" question; answer No either way.
for _ in $(seq 40); do
    if [ "$(xdotool search --onlyvisible --name '^IS Install$' 2>/dev/null | wc -l)" -ge 2 ]; then
        sleep 1; shot 02-installed-before; key alt+n; break; fi
    xdotool search --onlyvisible --name '^Inner Space New Valid Install$' >/dev/null 2>&1 && break
    sleep 0.25
done
wait_windows "Inner Space New Valid Install" 1 || exit 1; sleep 1; shot 03-directory; key Return
message_box || exit 1; sleep 1; shot 04-screen-saver; key alt+n
wait_windows "Game Difficulty and Speed" 1 || exit 1; sleep 1; shot 05-difficulty; key Return
wait_windows "Play Full Screen or in a Window" 1 || exit 1; sleep 1; shot 06-full-screen; key Return
wait_windows " Inner Space" 1 || exit 1; sleep 1
xdotool type --delay 80 "$NAME"; key Tab; xdotool type --delay 80 "$ORG"; log "typed name '$NAME' org '$ORG'"
shot 07-personalize
# The dialog takes OK only after a while (some seconds, longer when traced): retry.
for _ in $(seq 24); do key alt+o; wait_windows "README.TXT - Notepad" 1 5 2>/dev/null && break; done
wait_windows "README.TXT - Notepad" 1 1 || exit 1; sleep 2; shot 08-readme; log "installer done"
oracle_game_stop; log "stopped (installer and Notepad)"
cp "$WINEPREFIX/drive_c/windows/INSPACE.INI" "$WINEPREFIX/drive_c/windows/system.ini" "$OUT/"
find "$WINEPREFIX/drive_c/SPACE" "$WINEPREFIX/drive_c/windows" -newer "$OUT/.launch" -type f \
    -printf '%TT %10s %p\n' | sort >"$OUT/files.txt"
log "registration keys: $(grep -c 'vidsp\|32BitSP' "$WINEPREFIX/drive_c/windows/system.ini") in system.ini, $(grep -c '"PDev"' "$WINEPREFIX/user.reg") PDev in user.reg"

oracle_game_start "$OUT/game-trace.log" SPACE; log "launch INSPACE.EXE from C:\\SPACE"
sleep "$WAIT_TITLE"; shot 09-title
windows_log
scenario_finish
