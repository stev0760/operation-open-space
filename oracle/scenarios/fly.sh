#!/usr/bin/env bash
# fly.sh: a user-flown recording session of the original (registered game, patched oracle Wine).
#
# Usage: oracle/scenarios/fly.sh RUN-ID [KEY=VALUE ...]
#   Everything lands in work/oracle/RUN-ID/: trace.log (relay profile wave), watch-*.txt (isprobe
#   watchers every 20 ms), steps.txt, timeline.txt (merged, wall clock), vid-fly.mkv (lossless, 60 fps,
#   wall-clock timestamps, with vid-fly.start), INSPACE.INI, files.txt, files/ (saves and .ded written
#   during the run), the first menu pictures. Text files carry <redacted> for the pilot's name.
# Arguments (also environment variables):
#   SHIP=TRANSFORMER   the MainShip of the INI (the New Game dialog starts on it)
#   INI_SET="K=V ..."  more [Inner Space] keys
#   WATCH_FILE=PATH    extra watch specs, one NAME:DGROUPOFFSET:LEN:MS per line (# comments)
#   DISPLAY_MODE=nested|desktop|headless   where the game is shown (default nested):
#       nested    a rootful Xwayland window on the desktop (the nested X server, here :ORACLE_ID+110)
#                 holds the game; the video grabs that X server's root. The user's keyboard, mouse
#                 and speakers reach the game; no other window of the desktop is captured.
#       desktop   the game on the desktop's own X display (DESKTOP_DISPLAY, default $DISPLAY at
#                 start); the video grabs the game window by its id. No scripted menu clicks.
#       headless  Xvfb :ORACLE_ID+110, nothing shown; a null PulseAudio sink. For tests: the session
#                 drives itself with xdotool (SCRIPT=PATH, a bash file run in the wave, sourced with
#                 log/key/hold/shot available; the session then ends with Alt-X ... see FLY.md).
#   AUTOSTART=1        click through the menus into wave 1 (nested and headless; 0 leaves the menus
#                      to the user; desktop forces 0)
#   REG_SRC=work/wine/m4s8-oracle-reg  the registered prefix to copy from, once
#   ORACLE_ID=95       display, fixture and default prefix name
#   MAX_SECS=7200      the session stops after this many seconds even when the game still runs
# The script never prints a registration value.
set -u
FLY_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$FLY_DIR/../.." && pwd)"
RUN_ID="${1:?usage: oracle/scenarios/fly.sh RUN-ID [SHIP=name] [INI_SET=...] [WATCH_FILE=path] [DISPLAY_MODE=nested|desktop|headless]}"
shift
for a in "$@"; do case "$a" in [A-Z_]*=*) export "${a%%=*}=${a#*=}" ;; *) echo "fly: unknown argument $a" >&2; exit 2 ;; esac; done
# The desktop's display and Wayland socket are needed before oracle/env.sh hides them.
DESKTOP_DISPLAY="${DESKTOP_DISPLAY:-${DISPLAY:-:0}}"
SAVED_WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-wayland-0}"
export ORACLE_ID="${ORACLE_ID:-95}"
DISPLAY_MODE="${DISPLAY_MODE:-nested}"
SHIP="${SHIP:-TRANSFORMER}"; export SHIP
export WINEPREFIX="${WINEPREFIX:-$REPO/work/wine/fly-oracle-reg}"
export TRACE=1 ORACLE_RELAY_PROFILE="${ORACLE_RELAY_PROFILE:-wave}"
OUT_DEFAULT=fly
# shellcheck source=common.sh
source "$FLY_DIR/common.sh" "$REPO/work/oracle/$RUN_ID"
# shellcheck source=fly_lib.sh
source "$FLY_DIR/fly_lib.sh"
GAME_DISPLAY="$DISPLAY"
AUTOSTART="${AUTOSTART:-1}"; MAX_SECS="${MAX_SECS:-7200}"
REG_SRC="${REG_SRC:-$REPO/work/wine/m4s8-oracle-reg}"
rm -f "$OUT"/watch-*.txt "$OUT"/vid-fly.* "$OUT"/.n "$OUT"/.wpids "$OUT"/timeline.txt
STARTED_XSERVER=""
cleanup() {
    trap - EXIT INT TERM
    [ -f "$OUT/.wpids" ] && watch_stop
    [ -f "$OUT/.rec-fly" ] && rec_off fly
    oracle_game_stop
    [ -n "$STARTED_XSERVER" ] && kill "$STARTED_XSERVER" 2>/dev/null
    [ "${STARTED_XVFB:-0}" = 1 ] && pkill -f "^Xvfb $DISPLAY " 2>/dev/null
    sink_off
    log "stopped"
}
trap cleanup EXIT INT TERM

case "$DISPLAY_MODE" in
    headless)
        xdpyinfo -display "$DISPLAY" >/dev/null 2>&1 || STARTED_XVFB=1
        oracle_xvfb 24; sink_on "fly$ORACLE_ID" ;;
    nested)
        if ! xdpyinfo -display "$DISPLAY" >/dev/null 2>&1; then
            # A rootful Xwayland is a window of the desktop that holds a private X server.
            WAYLAND_DISPLAY="$SAVED_WAYLAND_DISPLAY" setsid -f Xwayland "$DISPLAY" -geometry 1024x768 -decorate -nolisten tcp -noreset >"$OUT/xserver.log" 2>&1
            for _ in $(seq 100); do xdpyinfo -display "$DISPLAY" >/dev/null 2>&1 && break; sleep 0.1; done
            xdpyinfo -display "$DISPLAY" >/dev/null 2>&1 || { echo "fly: the nested X server did not start (see $OUT/xserver.log); try DISPLAY_MODE=desktop" >&2; exit 1; }
            STARTED_XSERVER="$(pgrep -n -f "Xwayland $DISPLAY " || true)"
        fi ;;
    desktop) DISPLAY="$DESKTOP_DISPLAY"; export DISPLAY; GAME_DISPLAY="$DISPLAY"; AUTOSTART=0 ;;
    *) echo "fly: DISPLAY_MODE must be nested, desktop or headless" >&2; exit 2 ;;
esac
log "display mode $DISPLAY_MODE on $DISPLAY, run $RUN_ID, autostart $AUTOSTART"
python3 -c 'import time; print(time.time() - time.monotonic())' >"$OUT/clock-offset.txt"
fly_prefix "$REG_SRC" "$WINEPREFIX" || exit 1
fly_setup_game
cp "$WINEPREFIX/drive_c/windows/INSPACE.INI" "$OUT/INSPACE.INI.before"

touch "$OUT/.launch"
oracle_game_start "$OUT/trace.log" SPACE
log "launch from C:\\SPACE"
T0=$SECONDS

if [ "$AUTOSTART" = 1 ]; then
    echo "fly: the game starts and clicks itself to wave 1 (about 45 s). Hands off the mouse until 'YOUR TURN'."
    fly_reach_wave
else
    for _ in $(seq 60); do [ -n "$(xdotool search --name 'Inner Space' 2>/dev/null)" ] && break; sleep 1; done
    sleep 3
fi
watch_start "${WATCH_FILE:-}"
WIN=""; GEOM="1024x768"
if [ "$DISPLAY_MODE" = desktop ]; then
    WIN="$(xdotool search --name 'Inner Space' | head -1)"
    GEOM="$(xdotool getwindowgeometry "$WIN" | awk '/Geometry/{print $2}')"
    log "game window $WIN $GEOM"
fi
rec_on fly 60 "$WIN" "$GEOM"
shot flystart
echo "fly: YOUR TURN. Recording. Quit with the game's own exit (Escape, then Exit; or Alt-X on the title screen)."
log "YOUR TURN"
if [ -n "${SCRIPT:-}" ]; then   # headless test: scripted keys stand in for the user
    # shellcheck disable=SC1090
    source "$SCRIPT"
fi
# Wait for the game to end, or for MAX_SECS.
( sleep "$MAX_SECS"; touch "$OUT/.maxed" ) & MAXPID=$!
fly_wait_exit &
WAITPID=$!
while kill -0 "$WAITPID" 2>/dev/null; do
    [ -f "$OUT/.maxed" ] && { log "MAX_SECS reached"; kill "$WAITPID" 2>/dev/null; break; }
    sleep 1
done
kill "$MAXPID" 2>/dev/null; rm -f "$OUT/.maxed"
log "game ended after $((SECONDS - T0)) s"
sleep 1
watch_stop; rm -f "$OUT/.wpids"
rec_off fly
oracle_game_stop
fly_collect
log "done"
echo "fly: done. Files in $OUT (timeline.txt, trace.log, watch-*.txt, vid-fly.mkv)."
