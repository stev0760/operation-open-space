#!/usr/bin/env bash
# Scenario intro: the state-0 attract cycle of the original (SD logo, the black second,
# title, credits, the two text pages, Basic Key Commands, demo start) without input, then
# a second run that steps through the phases with Space.
# Digest: intro-0001.
#
# Usage: ORACLE_ID=3 oracle/scenarios/intro.sh [OUTDIR]
#   OUTDIR defaults to work/oracle/intro. Two runs, each from launch:
#   watch/  no input. tSSS.SS.png every WATCH_FAST s for the first WATCH_FAST_SECS s (the
#           logo steps are short), then every WATCH_SLOW s up to WATCH_SECS s, or until
#           the demo start (the green "(Starting Demo)" text, or the Navigation window
#           the demo opens) has been seen, plus WATCH_TAIL more shots.
#           SSS.SS is the elapsed time from launch when the capture started.
#   step/   Space on each phase: NN-phase-what-tSSS.SS.png. The run waits for the logo,
#           the black second and the title, shoots the title when its picture is
#           complete (screen unchanged between two polls), then for each later phase
#           waits for the phase to appear (the screen differs from the last shot; for
#           the first text page, its white "press ESC" line is drawn), shoots its first
#           draw and its complete draw (screen unchanged between two polls; for the
#           credits, the first name line drawn and unchanged), and presses Space
#           STEP_DELAY s after the complete draw. No Space during the logo: input reaches
#           the state-0 handler only after Calibrate (shell.md, Input).
#   steps.txt (timestamped step log, both runs), watch/INSPACE.INI, step/INSPACE.INI,
#   watch/files.txt, step/files.txt, trace.log when TRACE=1 (the last run's).
# Environment: the common variables of oracle/scenarios/common.sh (TRACE, KEEP, DEPTH,
# INI_SET, FIXTURE; WAIT_BOOT and WAIT_STEP are unused here), plus (all optional):
#   TRACE=0            this scenario runs untraced by default: the relay slows the frames
#                      and the timeline is the point of the capture
#   RUNS="watch step"  which runs, in this order
#   WATCH_FAST=0.5 WATCH_FAST_SECS=30 WATCH_SLOW=2 WATCH_SECS=300 WATCH_TAIL=5
#   POLL=0.25          seconds between polls in the step run
#   STEP_DELAY=1       seconds from a phase's complete draw to Space
#   STEP_TIMEOUT=120   seconds a step-run wait may take before the run gives up on it
#   DEMO_SHOTS=5       step run: shots 1 s apart after Space on Basic Key Commands
# Tools: ImageMagick 7 (magick, import) and xdotool.
# Screen facts used (1024x768, the full-screen window; W/2 = 512, H/2 = 384): the credits
# name line has its top at H/2 + 100 (shell.md, Credits), the demo-start text is drawn in
# colour 0x0000FF00 at (W/2, H/2 - 40) and (W/2, H/2) (shell.md, Demo start).
set -u
OUT_DEFAULT=intro
TRACE="${TRACE:-0}"
# shellcheck source=common.sh
source "$(dirname "${BASH_SOURCE[0]}")/common.sh" "$@"
RUNS="${RUNS:-watch step}"
WATCH_FAST="${WATCH_FAST:-0.5}"; WATCH_FAST_SECS="${WATCH_FAST_SECS:-30}"
WATCH_SLOW="${WATCH_SLOW:-2}"; WATCH_SECS="${WATCH_SECS:-300}"; WATCH_TAIL="${WATCH_TAIL:-5}"
POLL="${POLL:-0.25}"; STEP_DELAY="${STEP_DELAY:-1}"; STEP_TIMEOUT="${STEP_TIMEOUT:-120}"
DEMO_SHOTS="${DEMO_SHOTS:-5}"
TMP="$OUT/.poll"

T0=0
elapsed() { awk -v a="$(date +%s.%N)" -v b="$T0" 'BEGIN { printf "%06.2f", a - b }'; }
later() { awk -v a="$1" -v b="$2" 'BEGIN { exit !(a + 0 >= b + 0) }'; }   # later A B: A >= B
# tshot DIR/NAME: screenshot named with the elapsed time since launch.
tshot() { local t; t="$(elapsed)"; shot "${1}t$t"; LAST="$OUT/${1}t$t.png"; }
grab() { oracle_shot "$TMP/$1.png"; }
# Pixels brighter than 2 % grey in FILE [CROP geometry].
lit() { magick "$1" ${2:+-crop "$2"} -colorspace gray -threshold 2% -format '%[fx:round(mean*w*h)]' info:; }
# Pixels that differ between two files.
diffpx() { magick compare -metric AE "$1" "$2" null: 2>&1 | awk '{ print int($1) }'; }
# Pixels near pure green (the demo-start text) in the centre band.
# Light-grey (192, 192, 192) pixels: the Navigation window the demo opens.
grey() { magick "$1" -fill black -opaque white -fuzz 3% -fill white -opaque '#C0C0C0' -fill black +opaque white -format '%[fx:round(mean*w*h)]' info:; }
demo_started() { [ "$(green "$1")" -gt 200 ] || [ "$(grey "$1")" -gt 20000 ]; }
green() { magick "$1" -crop 640x90+192+330 +repage -fx 'g > 0.75 && r < 0.3 && b < 0.3' \
          -format '%[fx:round(mean*w*h)]' info:; }

start_run() {  # start_run NAME
    mkdir -p "$OUT/$1" "$TMP"; rm -f "$OUT/$1"/*.png
    scenario_prefix
    scenario_launch
    T0="$(date +%s.%N)"; log "run $1: t0 = launch"
}

finish_run() {  # finish_run NAME
    scenario_finish
    mv -f "$OUT/INSPACE.INI" "$OUT/$1/INSPACE.INI"; mv -f "$OUT/files.txt" "$OUT/$1/files.txt"
}

run_watch() {
    local next=0 t tail=-1 interval
    start_run watch
    while :; do
        t="$(elapsed)"
        later "$t" "$WATCH_SECS" && { log "watch: cap $WATCH_SECS s reached"; break; }
        if later "$t" "$next"; then
            tshot "watch/"
            if [ "$tail" -lt 0 ] && demo_started "$LAST"; then
                tail="$WATCH_TAIL"; log "watch: demo start seen at t=$t"
            elif [ "$tail" -gt 0 ]; then
                tail=$((tail - 1))
            fi
            [ "$tail" = 0 ] && break
            if later "$t" "$WATCH_FAST_SECS"; then interval="$WATCH_SLOW"; else interval="$WATCH_FAST"; fi
            next="$(awk -v n="$next" -v i="$interval" -v t="$t" 'BEGIN { n += i; while (n <= t) n += i; print n }')"
        fi
        sleep 0.05
    done
    finish_run watch
}

# await WHAT TEST...: poll until the shell test TEST (run on $TMP/cur.png, previous poll in
# $TMP/prev.png) succeeds. Returns 1 after STEP_TIMEOUT s.
await() {
    local what="$1" t_end; shift
    t_end="$(awk -v t="$(elapsed)" -v d="$STEP_TIMEOUT" 'BEGIN { print t + d }')"
    grab prev
    while :; do
        sleep "$POLL"; grab cur
        if "$@"; then log "step: $what at t=$(elapsed)"; return 0; fi
        later "$(elapsed)" "$t_end" && { log "step: TIMEOUT waiting for $what"; return 1; }
        mv -f "$TMP/cur.png" "$TMP/prev.png"
    done
}
is_lit() { [ "$(lit "$TMP/cur.png")" -gt 0 ]; }
is_black() { [ "$(lit "$TMP/cur.png")" -eq 0 ]; }
is_stable() { [ "$(lit "$TMP/cur.png")" -gt 0 ] && [ "$(diffpx "$TMP/prev.png" "$TMP/cur.png")" -eq 0 ]; }
differs_from() { [ "$(diffpx "$1" "$TMP/cur.png")" -gt 1000 ]; }
is_changed() { differs_from "$REF"; }
# Credits: the name line (top at H/2 + 100) is drawn and unchanged between two polls.
name_done() {
    [ "$(lit "$TMP/cur.png" 1024x40+0+484)" -gt 50 ] &&
    magick "$TMP/prev.png" "$TMP/cur.png" -crop 1024x40+0+484 +repage -metric AE -compare \
        -format '%[distortion]' info: | awk '{ exit !($1 == 0) }'
}
# Text page 1: the white "press ESC to play" line (bitmap font 0, top at H/2 + 173) is drawn.
press_esc_line() { [ "$(lit "$TMP/cur.png" 1024x20+0+557)" -gt 200 ]; }
# space: Space STEP_DELAY s after the last shot, which becomes the reference for is_changed.
space() { REF="$LAST"; sleep "$STEP_DELAY"; xdotool key space; log "key space at t=$(elapsed)"; }
# appear WHAT TEST...: wait for TEST, then (if the screen is black at that moment) for a lit screen.
appear() {
    local what="$1"; shift
    await "$what" "$@" || return 1
    if is_black; then await "$what (lit)" is_lit || return 1; fi
}

run_step() {
    local REF=""
    start_run step
    await "logo" is_lit && tshot step/00-logo-first-
    await "black second" is_black && tshot step/01-black-
    await "title" is_lit && tshot step/02-title-first-
    await "title complete" is_stable && tshot step/02-title-full-
    space
    appear "credits" is_changed && tshot step/03-credits-first-
    await "credits entry 0" name_done && tshot step/03-credits-full-
    space
    appear "text page 1" press_esc_line && tshot step/04-text1-first-
    await "text page 1 complete" is_stable && tshot step/04-text1-full-
    space
    appear "text page 2" is_changed && tshot step/05-text2-first-
    await "text page 2 complete" is_stable && tshot step/05-text2-full-
    space
    appear "key commands" is_changed && tshot step/06-keys-first-
    await "key commands complete" is_stable && tshot step/06-keys-full-
    space
    for i in $(seq 1 "$DEMO_SHOTS"); do sleep 1; tshot "step/$(printf '07-demo-%02d-' "$i")"; done
    finish_run step
}

for run in $RUNS; do
    case "$run" in
        watch) run_watch ;;
        step) run_step ;;
        *) log "unknown run $run (watch, step)" ;;
    esac
done
rm -rf "$TMP"
