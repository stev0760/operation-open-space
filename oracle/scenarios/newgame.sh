#!/usr/bin/env bash
# Scenario newgame: title -> ESC -> Restore Game (NEW GAME, OK) -> New Game dialog
# (records every combo list, picks a drive) -> OK -> desk intro (ESC, or left to run)
# -> Navigation window (visits every tree entry) -> Enter on D:\GAMES -> WAVE 1 MISSION
# BRIEFING -> ESC -> the first seconds of wave 1.
# Digests: newgame-0001 (stock Wine 11.0, which faults; see fault-0001), newgame-0002.
#
# Usage: ORACLE_ID=1 oracle/scenarios/newgame.sh [OUTDIR]
#   OUTDIR defaults to work/oracle/newgame. Screenshots NN-step.png, trace.log,
#   INSPACE.INI after the run, steps.txt (timestamped step log).
# Environment (all optional; ORACLE_WINE and ORACLE_ID as in oracle/env.sh):
#   DRIVE=D            drive letter to pick in the New Game DRIVE combo (C or D)
#   FIXTURE=basic      fixture tree mapped as D: (oracle/fixtures/make_fixtures.py)
#   TRACE=1            0 runs untraced
#   WAIT_BOOT=20       seconds from launch to the title screen
#   WAIT_STEP=2        seconds after each dialog action
#   WAIT_SKIP=3        seconds from New Game OK to the intro screenshot (and ESC)
#   SKIP_INTRO=1       press ESC to end the intro; 0 lets it run (INTRO_SHOTS shots)
#   WAIT_INTRO=4       seconds between intro and briefing screenshots
#   INTRO_SHOTS=8      screenshots of an unskipped intro; the game opens the
#                      Navigation window by itself 20-24 s after New Game OK
#   ENTER_SHOTS=2      briefing screenshots after Enter on D:\GAMES
#   WAVE_SHOTS=4       screenshots after ESC on the briefing (0: no ESC, the run ends on the briefing)
#   WAIT_WAVE=2        seconds between wave screenshots
#   KEEP=0             1 leaves the game running at the end
#   DEPTH=24           Xvfb depth when this script starts the display (8 = PseudoColor)
#   INI_SET=""         extra [Inner Space] keys for INSPACE.INI, e.g. "QuickIntro=1"
# A crash report from winedbg is appended to trace.log after the game stops
# (oracle/trace.sh, oracle_crash_collect); untraced runs write it to winedbg.txt.
# Assumes a 1024x768 display (oracle_xvfb default); the dialogs centre themselves,
# so the coordinates below are fixed for that size.
set -u
ORACLE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO="$(dirname "$ORACLE_DIR")"
# shellcheck source=../env.sh
source "$ORACLE_DIR/env.sh"
# shellcheck source=../trace.sh
source "$ORACLE_DIR/trace.sh"
PY="$REPO/re/.venv/bin/python"

OUT="$(realpath -m "${1:-$REPO/work/oracle/newgame}")"
DRIVE="${DRIVE:-D}"; FIXTURE="${FIXTURE:-basic}"; TRACE="${TRACE:-1}"
WAIT_BOOT="${WAIT_BOOT:-20}"; WAIT_STEP="${WAIT_STEP:-2}"
WAIT_INTRO="${WAIT_INTRO:-4}"; INTRO_SHOTS="${INTRO_SHOTS:-8}"; KEEP="${KEEP:-0}"
WAIT_SKIP="${WAIT_SKIP:-3}"; SKIP_INTRO="${SKIP_INTRO:-1}"; ENTER_SHOTS="${ENTER_SHOTS:-2}"
WAVE_SHOTS="${WAVE_SHOTS:-4}"; WAIT_WAVE="${WAIT_WAVE:-2}"
DEPTH="${DEPTH:-24}"; INI_SET="${INI_SET:-}"

mkdir -p "$OUT"; rm -f "$OUT"/*.png "$OUT"/trace.log "$OUT"/steps.txt "$OUT"/winedbg.txt
log() { printf '%s %s\n' "$(date +%T.%3N)" "$*" | tee -a "$OUT/steps.txt"; }
shot() { oracle_shot "$OUT/$1.png"; log "shot $1"; }
click() { xdotool mousemove "$1" "$2" click 1; }

# --- prefix: fresh game files and INI, fixture drive, no Z: ------------------
oracle_game_stop
oracle_xvfb "$DEPTH"
if [ ! -d "$WINEPREFIX/drive_c/windows" ]; then wineboot -i; wineserver -w; fi
"$PY" -I "$ORACLE_DIR/install_layout.py" "$REPO/original/shipdisk" "$WINEPREFIX" --with-ini >/dev/null || exit 1
cd "$WINEPREFIX/drive_c" || exit 1   # a Windows-visible cwd for wine reg
if [ -n "$INI_SET" ]; then
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
log "prefix $WINEPREFIX display $DISPLAY depth $(xdpyinfo -display "$DISPLAY" | awk '/depth of root/{print $5}') wine $ORACLE_WINE ($(wine --version)) drive $DRIVE fixture $FIXTURE trace $TRACE ini_set '$INI_SET'"

# --- run --------------------------------------------------------------------
if [ "$TRACE" = 1 ]; then oracle_game_start "$OUT/trace.log"; else oracle_game_start -; fi
log "launch"
sleep "$WAIT_BOOT"; shot 01-title
click 512 700; sleep 1; xdotool key Escape; log "key ESC"
sleep "$WAIT_STEP"; shot 02-restore
click 480 301; sleep 1; shot 03-restore-gamelist          # GAME combo dropped
click 480 321; sleep 0.5; log "pick NEW GAME"
click 622 300; log "click OK (Restore Game)"
sleep "$WAIT_STEP"; shot 04-newgame
click 559 372; sleep 1; shot 05-newgame-ships-sel           # SHIP list opens at the selection
# Scroll with the wheel and click TRANSFORMER again: a keyboard move changes the ship,
# and the demo accepts only TRANSFORMER (MessageBox on OK otherwise).
xdotool mousemove 490 450; for _ in $(seq 20); do xdotool click 4; done
sleep 0.5; shot 06-newgame-ships-top
for _ in $(seq 20); do xdotool click 5; done; sleep 0.5
click 480 530; sleep 0.5; shot 06b-newgame-ship-reselected
click 539 428; sleep 1; shot 07-newgame-play; xdotool key Escape; sleep 0.5
click 539 477; sleep 1; shot 08-newgame-zoom; xdotool key Escape; sleep 0.5
click 539 452; sleep 1; shot 09-newgame-drives
case "$DRIVE" in
    C|c) click 480 471 ;;
    *)   click 480 485 ;;
esac
log "pick drive $DRIVE"; sleep 0.5; shot 10-newgame-final
click 617 323; log "click OK (New Game)"
sleep "$WAIT_SKIP"; shot 11-intro
if [ "$SKIP_INTRO" = 1 ]; then
    xdotool key Escape; log "key ESC (skip intro)"   # ESC ends the desk intro
else
    for i in $(seq 1 "$INTRO_SHOTS"); do sleep "$WAIT_INTRO"; shot "$(printf 'intro-%02d' "$i")"; done
fi
sleep "$WAIT_STEP"; shot 12-navigation
log "windows: $(for w in $(xdotool search --name 'Inner Space' 2>/dev/null); do
    printf '[%s %s] ' "$(xdotool getwindowname "$w")" "$(xdotool getwindowgeometry "$w" | tail -2 | tr -s ' \n' ' ')"; done)"
# Navigation tree rows (screen y, 17 px apart) with deep and deep\a expanded:
# d:\ 67, deep 84, a 101, b 118, demon's gate 135, dodge city 152, empty 169,
# games 186, grand prix 203, lazy loop 220, sdi speedway 237, tools 254.
nav() { click 90 "$2"; sleep "$WAIT_STEP"; shot "$1"; }
nav 13-nav-games 153; nav 14-nav-tools 221                 # rows before expansion
xdotool mousemove 76 85 click --repeat 2 --delay 120 1; sleep "$WAIT_STEP"; shot 15-nav-deep-open
xdotool mousemove 92 101 click --repeat 2 --delay 120 1; sleep "$WAIT_STEP"; shot 16-nav-a-open
nav 17-nav-b 118; nav 18-nav-empty 169; nav 19-nav-demonsgate 135; nav 20-nav-dodgecity 152
nav 21-nav-grandprix 203; nav 22-nav-lazyloop 220; nav 23-nav-sdispeedway 237
nav 24-nav-games 186
click 525 78; log "click Enter (games)"
for i in $(seq 1 "$ENTER_SHOTS"); do sleep "$WAIT_INTRO"; shot "$(printf 'enter-%02d' "$i")"; done
if [ "$WAVE_SHOTS" -gt 0 ]; then
    xdotool key Escape; log "key ESC (start wave)"   # the briefing reads "PRESS 'ESC' TO START"
    for i in $(seq 1 "$WAVE_SHOTS"); do sleep "$WAIT_WAVE"; shot "$(printf 'wave-%02d' "$i")"; done
fi
cp "$WINEPREFIX/drive_c/windows/INSPACE.INI" "$OUT/INSPACE.INI"
if [ "$KEEP" != 1 ]; then
    oracle_game_stop; log "stopped"
    if [ "$TRACE" = 1 ]; then oracle_crash_collect "$OUT/trace.log"; else oracle_crash_collect "$OUT/winedbg.txt"; fi
fi
