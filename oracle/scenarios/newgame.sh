#!/usr/bin/env bash
# Scenario newgame: title -> ESC -> Restore Game (NEW GAME, OK) -> New Game dialog
# (records every combo list, picks a drive) -> OK -> desk intro (ESC, or left to run)
# -> Navigation window (visits every tree entry) -> Enter on D:\GAMES -> WAVE 1 MISSION
# BRIEFING -> ESC -> the first seconds of wave 1.
# Digests: newgame-0001 (stock Wine 11.0, which faults; see fault-0001), newgame-0002,
# wave-0001 (the ExtractIcon patch).
#
# Usage: ORACLE_ID=1 oracle/scenarios/newgame.sh [OUTDIR]
#   OUTDIR defaults to work/oracle/newgame. Screenshots NN-step.png, trace.log,
#   INSPACE.INI after the run, files.txt (files written since launch), steps.txt
#   (timestamped step log).
# Environment: the common variables of oracle/scenarios/common.sh (DRIVE, FIXTURE, TRACE,
# WAIT_BOOT, WAIT_STEP, KEEP, DEPTH, INI_SET), plus (all optional):
#   WAIT_SKIP=3        seconds from New Game OK to the intro screenshot (and ESC)
#   SKIP_INTRO=1       press ESC to end the intro; 0 lets it run (INTRO_SHOTS shots)
#   WAIT_INTRO=4       seconds between intro and briefing screenshots
#   INTRO_SHOTS=8      screenshots of an unskipped intro; the game opens the
#                      Navigation window by itself 20-24 s after New Game OK
#   ENTER_SHOTS=2      briefing screenshots after Enter on D:\GAMES
#   WAVE_SHOTS=4       screenshots after ESC on the briefing (0: no ESC, the run ends on the briefing)
#   WAIT_WAVE=2        seconds between wave screenshots
# A crash report from winedbg is appended to trace.log after the game stops
# (oracle/trace.sh, oracle_crash_collect); untraced runs write it to winedbg.txt.
set -u
OUT_DEFAULT=newgame
# shellcheck source=common.sh
source "$(dirname "${BASH_SOURCE[0]}")/common.sh" "$@"
WAIT_INTRO="${WAIT_INTRO:-4}"; INTRO_SHOTS="${INTRO_SHOTS:-8}"
WAIT_SKIP="${WAIT_SKIP:-3}"; SKIP_INTRO="${SKIP_INTRO:-1}"; ENTER_SHOTS="${ENTER_SHOTS:-2}"
WAVE_SHOTS="${WAVE_SHOTS:-4}"; WAIT_WAVE="${WAIT_WAVE:-2}"

scenario_prefix

# --- run --------------------------------------------------------------------
scenario_launch
sleep "$WAIT_BOOT"; shot 01-title
click $XY_TITLE; sleep 1; xdotool key Escape; log "key ESC"
sleep "$WAIT_STEP"; shot 02-restore
click $XY_RESTORE_GAME; sleep 1; shot 03-restore-gamelist          # GAME combo dropped
click $XY_RESTORE_NEW_GAME; sleep 0.5; log "pick NEW GAME"
click $XY_RESTORE_OK; log "click OK (Restore Game)"
sleep "$WAIT_STEP"; shot 04-newgame
click $XY_NEWGAME_SHIP; sleep 1; shot 05-newgame-ships-sel           # SHIP list opens at the selection
# Scroll with the wheel and click TRANSFORMER again: a keyboard move changes the ship,
# and the demo accepts only TRANSFORMER (MessageBox on OK otherwise).
xdotool mousemove $XY_NEWGAME_SHIP_LIST; for _ in $(seq 20); do xdotool click 4; done
sleep 0.5; shot 06-newgame-ships-top
for _ in $(seq 20); do xdotool click 5; done; sleep 0.5
click $XY_NEWGAME_TRANSFORMER; sleep 0.5; shot 06b-newgame-ship-reselected
click $XY_NEWGAME_PLAY; sleep 1; shot 07-newgame-play; xdotool key Escape; sleep 0.5
click $XY_NEWGAME_ZOOM; sleep 1; shot 08-newgame-zoom; xdotool key Escape; sleep 0.5
click $XY_NEWGAME_DRIVE; sleep 1; shot 09-newgame-drives
case "$DRIVE" in
    C|c) click $XY_NEWGAME_DRIVE_C ;;
    *)   click $XY_NEWGAME_DRIVE_D ;;
esac
log "pick drive $DRIVE"; sleep 0.5; shot 10-newgame-final
click $XY_NEWGAME_OK; log "click OK (New Game)"
sleep "$WAIT_SKIP"; shot 11-intro
if [ "$SKIP_INTRO" = 1 ]; then
    xdotool key Escape; log "key ESC (skip intro)"   # ESC ends the desk intro
else
    for i in $(seq 1 "$INTRO_SHOTS"); do sleep "$WAIT_INTRO"; shot "$(printf 'intro-%02d' "$i")"; done
fi
sleep "$WAIT_STEP"; shot 12-navigation
windows_log
# Navigation tree rows (screen y, 17 px apart) with deep and deep\a expanded:
# d:\ 67, deep 84, a 101, b 118, demon's gate 135, dodge city 152, empty 169,
# games 186, grand prix 203, lazy loop 220, sdi speedway 237, tools 254.
nav() { click "$X_NAV_ROW" "$2"; sleep "$WAIT_STEP"; shot "$1"; }
nav 13-nav-games "$Y_NAV_GAMES_FIRST"; nav 14-nav-tools 221                 # rows before expansion
xdotool mousemove 76 85 click --repeat 2 --delay 120 1; sleep "$WAIT_STEP"; shot 15-nav-deep-open
xdotool mousemove 92 101 click --repeat 2 --delay 120 1; sleep "$WAIT_STEP"; shot 16-nav-a-open
nav 17-nav-b 118; nav 18-nav-empty 169; nav 19-nav-demonsgate 135; nav 20-nav-dodgecity 152
nav 21-nav-grandprix 203; nav 22-nav-lazyloop 220; nav 23-nav-sdispeedway 237
nav 24-nav-games 186
click $XY_NAV_ENTER; log "click Enter (games)"
for i in $(seq 1 "$ENTER_SHOTS"); do sleep "$WAIT_INTRO"; shot "$(printf 'enter-%02d' "$i")"; done
if [ "$WAVE_SHOTS" -gt 0 ]; then
    xdotool key Escape; log "key ESC (start wave)"   # the briefing reads "PRESS 'ESC' TO START"
    for i in $(seq 1 "$WAVE_SHOTS"); do sleep "$WAIT_WAVE"; shot "$(printf 'wave-%02d' "$i")"; done
fi
scenario_finish
