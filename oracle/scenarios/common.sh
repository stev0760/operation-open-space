# Shared parts of the oracle scenarios (newgame.sh, wave.sh). A scenario sets OUT_DEFAULT
# (its output directory under work/oracle/) and sources this file:
#   OUT_DEFAULT=newgame; source "$(dirname "${BASH_SOURCE[0]}")/common.sh" "$@"
# The first scenario argument, when given, replaces work/oracle/$OUT_DEFAULT as OUT.
# This file sources oracle/env.sh and oracle/trace.sh and reads the common environment
# (all optional; ORACLE_WINE and ORACLE_ID as in oracle/env.sh):
#   DRIVE=D            drive letter to pick in the New Game DRIVE combo (C or D)
#   FIXTURE=basic      fixture tree mapped as D: (oracle/fixtures/make_fixtures.py)
#   TRACE=1            0 runs untraced
#   WAIT_BOOT=20       seconds from launch to the title screen
#   WAIT_STEP=2        seconds after each dialog action
#   KEEP=0             1 leaves the game running at the end
#   DEPTH=24           Xvfb depth when the scenario starts the display (8 = PseudoColor)
#   INI_SET=""         extra [Inner Space] keys for INSPACE.INI, e.g. "QuickIntro=1"
# Functions:
#   log TEXT           timestamped line to stdout and $OUT/steps.txt
#   shot NAME          screenshot $OUT/NAME.png
#   click X Y          left click at screen X,Y (no window manager: the game gets focus)
#   key KEYSYM...      xdotool key, logged
#   hold KEYSYM SECS   key held down for SECS seconds, logged
#   windows_log        logs every X window named "Inner Space*" with its geometry
#   scenario_prefix    fresh game files and INI, fixture drive as D:, no Z:, relay filter
#   scenario_launch    starts the game, traced into $OUT/trace.log when TRACE=1
#   scenario_finish    copies INSPACE.INI to $OUT, lists the files written since launch
#                      (files.txt), stops the game unless KEEP=1, collects winedbg output
# Screen positions (XY_*) assume a 1024x768 display (oracle_xvfb default). The dialogs
# centre themselves, so the positions are fixed for that size.
# shellcheck shell=bash
ORACLE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="$(dirname "$ORACLE_DIR")"
# shellcheck source=../env.sh
source "$ORACLE_DIR/env.sh"
# shellcheck source=../trace.sh
source "$ORACLE_DIR/trace.sh"
PY="$REPO/re/.venv/bin/python"

OUT="$(realpath -m "${1:-$REPO/work/oracle/${OUT_DEFAULT:?}}")"
DRIVE="${DRIVE:-D}"; FIXTURE="${FIXTURE:-basic}"; TRACE="${TRACE:-1}"
WAIT_BOOT="${WAIT_BOOT:-20}"; WAIT_STEP="${WAIT_STEP:-2}"; KEEP="${KEEP:-0}"
DEPTH="${DEPTH:-24}"; INI_SET="${INI_SET:-}"

# Title screen: a click gives the game focus, ESC opens Restore Game.
XY_TITLE="512 700"
# Restore Game: GAME combo, its NEW GAME entry when dropped, OK.
XY_RESTORE_GAME="480 301"; XY_RESTORE_NEW_GAME="480 321"; XY_RESTORE_OK="622 300"
# New Game: SHIP combo, the TRANSFORMER entry after scrolling back down, PLAY, DRIVE and
# ZOOM LEVEL combos, the C: and D: entries of the dropped DRIVE combo, OK.
XY_NEWGAME_SHIP="559 372"; XY_NEWGAME_SHIP_LIST="490 450"; XY_NEWGAME_TRANSFORMER="480 530"
XY_NEWGAME_PLAY="539 428"; XY_NEWGAME_DRIVE="539 452"; XY_NEWGAME_ZOOM="539 477"
XY_NEWGAME_DRIVE_C="480 471"; XY_NEWGAME_DRIVE_D="480 485"; XY_NEWGAME_OK="617 323"
# Navigation window: x of the tree rows, the games row before any expansion, Enter.
X_NAV_ROW=90; Y_NAV_GAMES_FIRST=153; XY_NAV_ENTER="525 78"

mkdir -p "$OUT"; rm -f "$OUT"/*.png "$OUT"/trace.log "$OUT"/steps.txt "$OUT"/winedbg.txt
log() { printf '%s %s\n' "$(date +%T.%3N)" "$*" | tee -a "$OUT/steps.txt"; }
shot() { oracle_shot "$OUT/$1.png"; log "shot $1"; }
click() { xdotool mousemove "$1" "$2" click 1; }
key() { xdotool key "$@"; log "key $*"; }
hold() { xdotool keydown "$1"; sleep "$2"; xdotool keyup "$1"; log "hold $1 $2 s"; }
windows_log() {
    log "windows: $(for w in $(xdotool search --name 'Inner Space' 2>/dev/null); do
        printf '[%s %s] ' "$(xdotool getwindowname "$w")" "$(xdotool getwindowgeometry "$w" | tail -2 | tr -s ' \n' ' ')"; done)"
}

scenario_prefix() {
    oracle_game_stop
    oracle_xvfb "$DEPTH"
    if [ ! -d "$WINEPREFIX/drive_c/windows" ]; then wineboot -i; wineserver -w; fi
    "$PY" -I "$ORACLE_DIR/install_layout.py" "$REPO/original/game" "$WINEPREFIX" --with-ini >/dev/null || exit 1
    cd "$WINEPREFIX/drive_c" || exit 1   # a Windows-visible cwd for wine reg
    if [ -n "$INI_SET" ]; then
        # shellcheck disable=SC2086  # INI_SET is a list of KEY=VALUE words
        "$PY" -I - "$ORACLE_DIR" "$WINEPREFIX/drive_c/windows/INSPACE.INI" $INI_SET <<'PYEOF' || exit 1
import pathlib, sys
sys.path.insert(0, sys.argv[1])
from install_layout import set_ini_keys
ini = pathlib.Path(sys.argv[2])
keys = dict(kv.split("=", 1) for kv in sys.argv[3:])
ini.write_bytes(set_ini_keys(ini.read_bytes().decode("latin-1"), "Inner Space", keys).encode("latin-1"))
PYEOF
    fi
    rm -rf "$WINEPREFIX/drive_c/INSPACE/GAMES/"* "$WINEPREFIX/drive_c/INSPACE/RETIRED"
    # Each ORACLE_ID gets its own fixture copy, because the generator rebuilds the tree.
    "$PY" "$ORACLE_DIR/fixtures/make_fixtures.py" "$REPO/work/oracle-$ORACLE_ID" --tree "$FIXTURE" --map-prefix "$WINEPREFIX" >/dev/null || exit 1
    wine reg add 'HKLM\Software\Wine\Drives' /v d: /t REG_SZ /d hd /f >/dev/null 2>&1
    rm -f "$WINEPREFIX/dosdevices/z:"   # keep the host root out of the drive list
    # The prefix carries no registry DLL overrides: WINEDLLOVERRIDES (oracle/env.sh) sets them all.
    wine reg delete 'HKCU\Software\Wine\DllOverrides' /f >/dev/null 2>&1
    oracle_crash_config
    if [ "$TRACE" = 1 ]; then oracle_trace_config; else oracle_trace_clear; fi
    # Wine's load_list trace line shows only the first ~300 characters of the list.
    [ "$TRACE" = 1 ] && log "relay profile $ORACLE_RELAY_PROFILE, RelayExclude $ORACLE_RELAY_EXCLUDE"
    log "prefix $WINEPREFIX display $DISPLAY depth $(xdpyinfo -display "$DISPLAY" | awk '/depth of root/{print $5}') wine $ORACLE_WINE ($(wine --version)) drive $DRIVE fixture $FIXTURE trace $TRACE ini_set '$INI_SET'"
}

scenario_launch() {
    touch "$OUT/.launch"
    if [ "$TRACE" = 1 ]; then oracle_game_start "$OUT/trace.log"; else oracle_game_start -; fi
    log "launch"
}

scenario_finish() {
    cp "$WINEPREFIX/drive_c/windows/INSPACE.INI" "$OUT/INSPACE.INI"
    # Files written since launch, on C: and on the fixture drive D:.
    find "$WINEPREFIX/drive_c" "$REPO/work/oracle-$ORACLE_ID" -newer "$OUT/.launch" -type f \
        -printf '%TT %10s %p\n' 2>/dev/null | sort >"$OUT/files.txt"
    if [ "$KEEP" != 1 ]; then
        oracle_game_stop; log "stopped"
        if [ "$TRACE" = 1 ]; then oracle_crash_collect "$OUT/trace.log"; else oracle_crash_collect "$OUT/winedbg.txt"; fi
    fi
}
