#!/usr/bin/env bash
# Scenario wave: the shortest newgame.sh path to wave 1 (title -> ESC -> Restore Game NEW
# GAME -> New Game with drive D: -> ESC on the intro -> games -> Enter -> ESC on the
# briefing), then play inside the wave, one stage after the other (STAGES).
# Digest: wave-0001.
#
# Usage: ORACLE_ID=1 oracle/scenarios/wave.sh [OUTDIR]
#   OUTDIR defaults to work/oracle/wave. Screenshots NN-stage-step.png, trace.log,
#   INSPACE.INI after the run, files.txt (files written since launch), steps.txt
#   (timestamped step log), autopilot.log.
# Environment: the common variables of oracle/scenarios/common.sh (DRIVE, FIXTURE, TRACE,
# WAIT_BOOT, WAIT_STEP, KEEP, DEPTH, INI_SET), plus (all optional):
#   WAIT_SKIP=3        seconds from New Game OK to ESC on the intro
#   WAIT_BRIEF=8       seconds from Enter to ESC on the briefing
#   CAPTURE_SECS=150   time limit of the capture stage (oracle/scenarios/autopilot.py)
#   STAGES="fly target strategy capture weapons ambulance save perspective watch help abort exit"
#                      the stages to run, in this order; each is a stage_NAME function
#                      below. "none" stops in the wave (with KEEP=1 the game keeps running).
#   ORACLE_RELAY_PROFILE=wave  relay filter profile (oracle/trace.sh)
# Stages (screen positions for a 1024x768 display):
#   fly         Up, Left, Right, Down (brake) held for fractions of a second.
#   target      I twice: the target display steps through the icons.
#   strategy    F12 -> the Command Mode MessageBox -> No; F12 -> Yes -> Command mode;
#               F12 back to Action mode. The game border and the full status display
#               (score, fuel, damage) appear when the first MessageBox closes.
#   capture     autopilot.py flies into the nearest icons until the score changes; HUD
#               before and after.
#   weapons     Tab (weapon select), Z (fire weapon), Space held (gun).
#   ambulance   H, then the ambulance screen: option list scrolled, Sell.. and its OK,
#               the first option (the cheapest) added, OK, and the launch back into the wave.
#   save        Alt-S, a game name, Save, the demo's answer and its OK.
#   perspective P.
#   watch       Alt-I twice (icon watch off and on), W, Ctrl-W.
#   help        F1 (the game's own help window), its OK.
#   abort       Alt-X and Cancel, then ESC -> Save & Exit -> a game name -> Save -> the
#               demo's answer -> back to the intro and the title screen.
#   exit        Alt-X on the title screen -> Exit -> the demo's closing dialog -> Exit.
set -u
OUT_DEFAULT=wave
ORACLE_RELAY_PROFILE="${ORACLE_RELAY_PROFILE:-wave}"
# shellcheck source=common.sh
source "$(dirname "${BASH_SOURCE[0]}")/common.sh" "$@"
WAIT_SKIP="${WAIT_SKIP:-3}"; WAIT_BRIEF="${WAIT_BRIEF:-8}"; CAPTURE_SECS="${CAPTURE_SECS:-150}"
STAGES="${STAGES:-fly target strategy capture weapons ambulance save perspective watch help abort exit}"

N=0
step() { N=$((N + 1)); shot "$(printf '%02d' "$N")-$1"; }   # numbered screenshot

reach_wave() {
    scenario_launch
    sleep "$WAIT_BOOT"; step title
    click $XY_TITLE; sleep 1; key Escape
    sleep "$WAIT_STEP"; click $XY_RESTORE_GAME; sleep 1; click $XY_RESTORE_NEW_GAME; sleep 0.5
    click $XY_RESTORE_OK; log "Restore Game: NEW GAME, OK"
    sleep "$WAIT_STEP"; click $XY_NEWGAME_DRIVE; sleep 1
    case "$DRIVE" in C|c) click $XY_NEWGAME_DRIVE_C ;; *) click $XY_NEWGAME_DRIVE_D ;; esac
    sleep 0.5; step newgame; click $XY_NEWGAME_OK; log "New Game: drive $DRIVE, OK"
    sleep "$WAIT_SKIP"; key Escape   # ends the desk intro
    sleep "$WAIT_STEP"; click "$X_NAV_ROW" "$Y_NAV_GAMES_FIRST"; sleep "$WAIT_STEP"; step navigation
    click $XY_NAV_ENTER; log "click Enter (games)"
    sleep "$WAIT_BRIEF"; step briefing
    key Escape; log "wave start"   # the briefing reads "PRESS 'ESC' TO START"
    sleep 2; step wave
    windows_log
}

# wait_for X Y HEX SECS: poll the screen until pixel X,Y has colour #HEX (e.g. F5F5F5,
# the face of the game's dialogs); logs the outcome.
wait_for() {
    local i
    for i in $(seq $(( $4 * 4 ))); do
        if [ "$(import -display "$DISPLAY" -window root -crop "1x1+$1+$2" -depth 8 txt:- 2>/dev/null \
                | awk 'NR==2{print $3}')" = "#$3" ]; then
            log "pixel $1,$2 is #$3 after $(( (i - 1) / 4 )) s"; return 0
        fi
        sleep 0.25
    done
    log "pixel $1,$2 did not turn #$3 within $4 s"; return 1
}
hud() { step "$1"; convert "$OUT/$(printf '%02d' "$N")-$1.png" -crop 1024x88+0+680 +repage "$OUT/$(printf '%02d' "$N")-$1-hud.png"; }
game_running() {  # a game process of THIS prefix (other ORACLE_IDs may be running)
    local pid
    for pid in $(pgrep -f 'winevdm.exe.*INSPACE.EXE'); do
        tr '\0' '\n' < "/proc/$pid/environ" 2>/dev/null | grep -qxF "WINEPREFIX=$WINEPREFIX" && return 0
    done
    return 1
}
DIALOG_FACE=F5F5F5

stage_fly() {
    hold Up 1.0; step fly-up
    hold Left 0.3; step fly-left
    hold Right 0.3; step fly-right
    hold Down 0.6; step fly-brake
}

stage_target() {
    key i; sleep 0.5; hud target-i1
    key i; sleep 0.5; hud target-i2
}

stage_strategy() {
    key F12; sleep 1; step strategy-f12-box
    click 548 446; log "click No"; sleep 1; hud strategy-no
    key F12; sleep 1; click 478 446; log "click Yes"; sleep 2; hud strategy-command
    key F12; sleep 1; hud strategy-action
}

stage_capture() {
    hud capture-before
    : >"$OUT/autopilot.log"
    "$PY" -I "$ORACLE_DIR/scenarios/autopilot.py" --seconds "$CAPTURE_SECS" --log "$OUT/autopilot.log"
    log "autopilot exit $? ($(tail -1 "$OUT/autopilot.log"))"
    sleep 0.3; hud capture-after
    sleep 2; hud capture-after-2s
}

stage_weapons() {
    key Tab; sleep 0.6; hud weapons-tab
    key z; sleep 0.4; hud weapons-z
    xdotool keydown space; sleep 0.5; step weapons-gun; xdotool keyup space; log "hold space 0.5 s"
}

stage_ambulance() {
    key h; sleep 0.6; hud ambulance-called
    wait_for 310 145 "$DIALOG_FACE" 40; step ambulance-dialog
    for _ in $(seq 8); do click 463 283; sleep 0.15; done; sleep 0.4; step ambulance-list-2
    for _ in $(seq 8); do click 463 283; sleep 0.15; done; sleep 0.4; step ambulance-list-3
    click 512 258; sleep 1; step ambulance-sell; click 658 324; log "click OK (Sell)"; sleep 1
    for _ in $(seq 16); do click 463 171; sleep 0.1; done      # back to the top of the list
    click 360 171; sleep 0.3; click 512 177; log "add the first option"; sleep 0.6; step ambulance-add
    click 684 325; log "click OK (ambulance)"; sleep 2; step ambulance-ok
    sleep 8; hud ambulance-back
}

stage_save() {
    key alt+s; sleep 1; step save-dialog
    click 500 382; sleep 0.3; xdotool type --delay 80 ORACLE1; log "type ORACLE1"; sleep 0.3
    click 607 381; log "click Save"; sleep 1.5; step save-answer
    click 513 416; log "click OK"; sleep 1.5; hud save-after
}

stage_perspective() {
    key p; sleep 1; hud perspective
}

stage_watch() {
    key alt+i; sleep 1; step watch-icons-off
    key alt+i; sleep 1; step watch-icons-on
    key w; sleep 1; step watch-ship
    key ctrl+w; sleep 1; step watch-ship-off
}

stage_help() {
    key F1; sleep 3; step help
    log "WinHelp processes: $(pgrep -fa 'winhlp|winhelp' | tr '\n' ' ')"
    click 723 348; log "click OK (help)"; sleep 1; step help-closed
}

stage_abort() {
    key alt+x; sleep 1.5; step abort-altx; click 617 421; log "click Cancel"; sleep 1
    key Escape; sleep 1.5; step abort-esc
    click 407 421; log "click Save & Exit"; sleep 1.5; step abort-save-dialog
    click 500 382; sleep 0.3; xdotool type --delay 80 ORACLE2; log "type ORACLE2"; sleep 0.3
    click 607 381; log "click Save"; sleep 1.5; step abort-save-answer
    click 513 416; log "click OK"; sleep 3; step abort-intro
    sleep 20; step abort-title
}

stage_exit() {
    key alt+x; sleep 1.5; step exit-altx
    click 511 421; log "click Exit"; sleep 2; step exit-closing
    click 567 582; log "click Exit (closing dialog)"; sleep 4
    if game_running; then log "game still running"; step exit-left; else log "game exited"; fi
}

scenario_prefix
reach_wave
for s in $STAGES; do
    [ "$s" = none ] && break
    log "stage $s"; "stage_$s"
done
scenario_finish
