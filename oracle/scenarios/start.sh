#!/usr/bin/env bash
# Scenario start: the whole shell path from the title to the wave and out, for one edition,
# with screenshots named by the time since launch, so a comparison runner can measure the
# port against it. Digest: start-0001.
#
# Usage: EDITION=unreg oracle/scenarios/start.sh [OUTDIR]     (ORACLE_ID defaults to 4)
#        EDITION=reg   oracle/scenarios/start.sh [OUTDIR]     (ORACLE_ID defaults to 9)
#   OUTDIR defaults to work/oracle/start-$EDITION. One subdirectory per run (RUNS), each
#   with NN-step-tSSS.SS.png (SSS.SS = seconds from `wine INSPACE.EXE`), trace.log
#   (default relay profile), INSPACE.INI.before and INSPACE.INI.after, files.txt (files
#   written since launch). steps.txt (all runs) logs every step with its wall-clock time
#   and its time since launch; the key and click lines align the trace (WM_KEYDOWN and
#   WM_LBUTTONDOWN in the CallTo16 lines) with the screenshots.
# Editions:
#   unreg  a fresh prefix from scenario_prefix (common.sh): the shipdisk layout in
#          C:\INSPACE, its INSPACE.INI, fixture tree `basic` as D:. Unregistered.
#   reg    the registered install in the existing prefix innerspace9, started from
#          C:\SPACE (as install.sh does). This scenario never runs scenario_prefix or
#          install_layout.py there. Before the first run it backs up C:\windows\INSPACE.INI
#          next to itself (INSPACE.INI.start-backup) and restores it after every run, so
#          every run starts from the same INI and the install is left as found. With
#          REG_FIXTURE=1 (default) the fixture tree `basic` is mapped as E: for the runs
#          (the prefix's D: is a CD device) and unmapped afterwards. The INI copies kept
#          in OUTDIR have the [Registration] section's values and ThePlayer replaced by
#          "<redacted>"; nothing of the registration goes to the output.
# Runs (RUNS, in this order; each launches the game afresh):
#   main         title -> ESC -> Restore Game (its GAME list) -> NEW GAME, OK -> New Game
#                (SHIP list paged with the wheel, PLAY, DRIVE, ZOOM LEVEL lists) -> drive
#                -> OK -> desk scene, shots every DESK_STEP s, back-to-back from
#                DESK_FAST s on, until the Navigation window shows -> games -> Enter ->
#                "LOADING GRAPHICS" and the briefing, shots every BRIEF_STEP s for
#                BRIEF_SECS s (ESC afterwards if it has not ended) -> wave start ->
#                Alt+X (#244) -> Cancel -> Alt+X -> Exit -> (#799 in the unregistered
#                game) Exit -> the game ends.
#   briefesc     the same path without the list shots; ESC BRIEF_ESC_AT s after Enter.
#   briefspace   the same; Space every SPACE_EVERY s for SPACE_SECS s, then ESC if needed.
#   title        from the title: ESC -> Restore Game -> Cancel (shots every 0.5 s for
#                TITLE_AFTER s) -> R (Order #693 in the unregistered game: Register with
#                an empty code, with code 123 -> MessageBox, Order Form.., More Info..,
#                Cancel; nothing in the registered game; shots for TITLE_AFTER s) ->
#                Alt+X -> #244 -> Save & Exit (disabled at the title) -> Cancel -> Alt+X
#                -> Exit -> (#799 -> Order.. -> Order, its buttons, Cancel) -> the game ends.
# Environment: the common variables of common.sh (TRACE, WAIT_BOOT, WAIT_STEP, KEEP,
# DEPTH, INI_SET, FIXTURE, DRIVE), plus (all optional):
#   EDITION=unreg  RUNS="main briefesc briefspace title"
#   DESK_STEP=0.5 DESK_FAST=17 DESK_CAP=60  BRIEF_STEP=0.5 BRIEF_SECS=150
#   BRIEF_ESC_AT=4  SPACE_EVERY=1 SPACE_SECS=40  TITLE_AFTER=6
#   REG_FIXTURE=1  REG_DRIVE=e
# Screen positions assume the 1024x768 display (oracle_xvfb); the dialogs centre
# themselves. With no window manager the game keeps the focus after a click.
set -u
EDITION="${EDITION:-unreg}"
case "$EDITION" in
    unreg) export ORACLE_ID="${ORACLE_ID:-4}" ;;
    reg)   export ORACLE_ID="${ORACLE_ID:-9}" ;;
    *) echo "start.sh: EDITION must be unreg or reg" >&2; exit 2 ;;
esac
OUT_DEFAULT="start-$EDITION"
# shellcheck source=common.sh
source "$(dirname "${BASH_SOURCE[0]}")/common.sh" "$@"
RUNS="${RUNS:-main briefesc briefspace title}"
DESK_STEP="${DESK_STEP:-0.5}"; DESK_FAST="${DESK_FAST:-17}"; DESK_CAP="${DESK_CAP:-60}"
BRIEF_STEP="${BRIEF_STEP:-0.5}"; BRIEF_SECS="${BRIEF_SECS:-150}"; BRIEF_ESC_AT="${BRIEF_ESC_AT:-4}"
SPACE_EVERY="${SPACE_EVERY:-1}"; SPACE_SECS="${SPACE_SECS:-40}"; TITLE_AFTER="${TITLE_AFTER:-6}"
REG_FIXTURE="${REG_FIXTURE:-1}"; REG_DRIVE="${REG_DRIVE:-e}"

# Positions beyond common.sh's.
XY_RESTORE_CANCEL="622 333"
XY_EXIT_SAVE="407 421"; XY_EXIT_EXIT="511 421"; XY_EXIT_CANCEL="617 421"     # #244
XY_799_EXIT="567 582"; XY_799_ORDER="666 582"                                 # #799
XY_ORDER_MOREINFO="690 232"; XY_ORDER_FORM="690 406"                          # #693
XY_ORDER_REGISTER="690 537"; XY_ORDER_CANCEL="690 567"; XY_ORDER_CODE="518 570"
XY_SHIP_SCROLLBAR="560 480"    # wheel over the dropped SHIP list's scroll bar: no hover selection
XY_NOTEPAD="300 300"
CROP_SHIP_LIST="150x232+425+382"
# The drive entry of the dropped DRIVE list: C: at y 471, the fixture drive below it at
# 485. The list holds only hard disks: the registered prefix's CD drive D: is not listed.
if [ "$EDITION" = reg ]; then PICK_DRIVE="$(echo "$REG_DRIVE" | tr a-z A-Z)"; else PICK_DRIVE="$DRIVE"; fi
case "$PICK_DRIVE" in C|c) XY_DRIVE_PICK="$XY_NEWGAME_DRIVE_C" ;; *) XY_DRIVE_PICK="$XY_NEWGAME_DRIVE_D" ;; esac

INI="$WINEPREFIX/drive_c/windows/INSPACE.INI"
GAME_DIR=INSPACE; [ "$EDITION" = reg ] && GAME_DIR=SPACE
RD=""; T0=0; N=0; STEPS0=0

elapsed() { awk -v a="$(date +%s.%N)" -v b="$T0" 'BEGIN { printf "%06.2f", a - b }'; }
later() { awk -v a="$1" -v b="$2" 'BEGIN { exit !(a + 0 >= b + 0) }'; }
# tshot NAME: numbered, timestamped screenshot in the run directory.
tshot() {
    local t; t="$(elapsed)"; N=$((N + 1))
    LAST="$RD/$(printf '%03d' "$N")-$1-t$t.png"; oracle_shot "$LAST"; log "shot $(basename "$LAST")"
}
step_log() { log "t=$(elapsed) $*"; }
tkey() { xdotool key "$@"; step_log "key $*"; }
tclick() { xdotool mousemove "$1" "$2" click 1; step_log "click $1,$2${3:+ ($3)}"; }
visible() { xdotool search --onlyvisible --name "$1" 2>/dev/null | head -1; }
windows_now() {
    step_log "windows: $(for w in $(xdotool search --onlyvisible --name '.' 2>/dev/null); do
        printf '[%s %s] ' "$(xdotool getwindowname "$w")" "$(xdotool getwindowgeometry "$w" | tail -2 | tr -s ' \n' ' ')"; done)"
}
game_running() {  # a game process of THIS prefix
    local pid
    for pid in $(pgrep -f 'INSPACE.EXE'); do
        tr '\0' '\n' < "/proc/$pid/environ" 2>/dev/null | grep -qxF "WINEPREFIX=$WINEPREFIX" && return 0
    done
    return 1
}
# wait_window REGEX SECS: until a visible window's name matches.
wait_window() {
    local i
    for i in $(seq $(( $2 * 4 ))); do visible "$1" >/dev/null && [ -n "$(visible "$1")" ] && return 0; sleep 0.25; done
    step_log "timeout: no window '$1' within $2 s"; return 1
}
# close_notepad: the Notepad that More Info.. or Order Form.. started.
close_notepad() {
    if wait_window 'Notepad$' 8; then
        sleep 1; tshot "$1"; windows_now
        xdotool mousemove $XY_NOTEPAD click 1; sleep 0.3; tkey alt+F4; sleep 1
    fi
}
# redact FILE: the registered INI without registration values or the player name.
redact() {
    awk 'BEGIN { s = "" }
         /^\[/ { s = tolower($0) }
         s == "[registration]" && /=/ { sub(/=.*/, "=<redacted>") }
         /^[Tt]he[Pp]layer=/ { sub(/=.*/, "=<redacted>") }
         { print }' "$1"
}
keep_ini() {  # keep_ini SUFFIX
    if [ "$EDITION" = reg ]; then redact "$INI" >"$RD/INSPACE.INI.$1"; else cp "$INI" "$RD/INSPACE.INI.$1"; fi
}

reg_prefix() {
    oracle_game_stop
    oracle_xvfb "$DEPTH"
    cd "$WINEPREFIX/drive_c" || exit 1
    [ -f "$INI.start-backup" ] || cp -p "$INI" "$INI.start-backup"
    cp -p "$INI.start-backup" "$INI"
    if [ "$REG_FIXTURE" = 1 ]; then
        "$PY" "$ORACLE_DIR/fixtures/make_fixtures.py" "$REPO/work/oracle-$ORACLE_ID" --tree "$FIXTURE" \
            --map-prefix "$WINEPREFIX" --drive "$REG_DRIVE" >/dev/null || exit 1
        wine reg add 'HKLM\Software\Wine\Drives' /v "$REG_DRIVE:" /t REG_SZ /d hd /f >/dev/null 2>&1
    fi
    oracle_crash_config
    if [ "$TRACE" = 1 ]; then oracle_trace_config; else oracle_trace_clear; fi
    log "registered prefix $WINEPREFIX display $DISPLAY wine $ORACLE_WINE ($(wine --version)) fixture ${REG_FIXTURE/1/$FIXTURE as $REG_DRIVE:} trace $TRACE"
}
reg_cleanup() {
    oracle_game_stop
    cp -p "$INI.start-backup" "$INI" && rm -f "$INI.start-backup"
    if [ "$REG_FIXTURE" = 1 ]; then
        rm -f "$WINEPREFIX/dosdevices/$REG_DRIVE:"
        (cd "$WINEPREFIX/drive_c" && wine reg delete 'HKLM\Software\Wine\Drives' /v "$REG_DRIVE:" /f >/dev/null 2>&1)
    fi
    (cd "$WINEPREFIX/drive_c" && oracle_trace_clear)
    log "registered prefix restored: INI from backup, $REG_DRIVE: unmapped, relay filter cleared"
}

start_run() {  # start_run NAME
    RD="$OUT/$1"; N=0; mkdir -p "$RD"; rm -f "$RD"/*.png "$RD"/trace.log "$RD"/INSPACE.INI.* "$RD"/steps.txt
    STEPS0="$(wc -l <"$OUT/steps.txt" 2>/dev/null || echo 0)"
    if [ "$EDITION" = reg ]; then reg_prefix; else scenario_prefix; fi
    keep_ini before
    touch "$OUT/.launch"
    if [ "$TRACE" = 1 ]; then oracle_game_start "$RD/trace.log" "$GAME_DIR"; else oracle_game_start - "$GAME_DIR"; fi
    T0="$(date +%s.%N)"; log "run $1: launch (t0 = $(date -d "@${T0%.*}" +%T).${T0#*.}) from C:\\$GAME_DIR"
    sleep "$WAIT_BOOT"; tshot title
    tclick $XY_TITLE "focus"
}

finish_run() {
    local i
    for i in $(seq 20); do game_running || break; sleep 0.5; done
    if game_running; then step_log "game still running"; windows_now; tshot left-running; else step_log "game ended"; fi
    keep_ini after
    find "$WINEPREFIX/drive_c" "$REPO/work/oracle-$ORACLE_ID" -newer "$OUT/.launch" -type f \
        ! -name 'INSPACE.INI.start-backup' -printf '%TT %10s %p\n' 2>/dev/null | sort >"$RD/files.txt"
    if [ "$KEEP" != 1 ]; then
        oracle_game_stop
        if [ "$TRACE" = 1 ]; then oracle_crash_collect "$RD/trace.log"; else oracle_crash_collect "$RD/winedbg.txt"; fi
    fi
    [ "$EDITION" = reg ] && cp -p "$INI.start-backup" "$INI"
    log "run $(basename "$RD") done"
    tail -n +"$((STEPS0 + 1))" "$OUT/steps.txt" >"$RD/steps.txt"
}

# list_pages: the dropped SHIP list, paged with the wheel over its scroll bar until it
# stops changing (5 wheel clicks = 15 rows of the 16 shown).
list_pages() {
    local p prev="" cur
    xdotool mousemove $XY_SHIP_SCROLLBAR
    for _ in $(seq 60); do xdotool click 4; done; sleep 0.5
    for p in $(seq 1 40); do
        tshot "newgame-ship-page$(printf '%02d' "$p")"
        cur="$(magick "$LAST" -crop "$CROP_SHIP_LIST" +repage -format '%#' info:)"
        [ "$cur" = "$prev" ] && { rm -f "$LAST"; N=$((N - 1)); break; }
        prev="$cur"
        for _ in 1 2 3 4 5; do xdotool click 5; done; sleep 0.4
    done
    step_log "SHIP list: $((p - 1)) pages"
}

to_newgame() {  # to_newgame LISTS(0/1): title -> ESC -> Restore Game -> NEW GAME -> New Game
    tkey Escape; sleep "$WAIT_STEP"; tshot restore; windows_now
    tclick $XY_RESTORE_GAME "GAME combo"; sleep 1; [ "$1" = 1 ] && tshot restore-gamelist
    tclick $XY_RESTORE_NEW_GAME "NEW GAME"; sleep 0.5
    tclick $XY_RESTORE_OK "OK (Restore Game)"; sleep "$WAIT_STEP"; tshot newgame; windows_now
    if [ "$1" = 1 ]; then
        tclick $XY_NEWGAME_SHIP "SHIP combo"; sleep 1; tshot newgame-ship-dropped
        list_pages
        tkey Escape; sleep 0.7; tshot newgame-ship-closed
        tclick $XY_NEWGAME_PLAY "PLAY combo"; sleep 1; tshot newgame-play; tkey Escape; sleep 0.5
        tclick $XY_NEWGAME_DRIVE "DRIVE combo"; sleep 1; tshot newgame-drive; tkey Escape; sleep 0.5
        tclick $XY_NEWGAME_ZOOM "ZOOM LEVEL combo"; sleep 1; tshot newgame-zoom; tkey Escape; sleep 0.5
    fi
    tclick $XY_NEWGAME_DRIVE "DRIVE combo"; sleep 1
    tclick $XY_DRIVE_PICK "drive $PICK_DRIVE:"; sleep 0.5; tshot newgame-final
    tclick $XY_NEWGAME_OK "OK (New Game)"
}

desk() {  # shots until the Navigation window shows
    local t0 t
    t0="$(elapsed)"
    while :; do
        t="$(awk -v a="$(elapsed)" -v b="$t0" 'BEGIN { print a - b }')"
        if [ -n "$(visible '^Inner Space Navigation$')" ]; then step_log "Navigation window seen ${t} s after OK"; break; fi
        later "$t" "$DESK_CAP" && { step_log "no Navigation window within $DESK_CAP s"; return 1; }
        tshot desk
        later "$t" "$DESK_FAST" || sleep "$DESK_STEP"
    done
    sleep 1; tshot navigation; windows_now
    tclick "$X_NAV_ROW" "$Y_NAV_GAMES_FIRST" "games"; sleep "$WAIT_STEP"; tshot navigation-games
    tclick $XY_NAV_ENTER "Enter"
}

# brief SECS [ACTION EVERY]: shots every BRIEF_STEP s for SECS s after Enter; ACTION=space
# presses Space every EVERY s.
brief() {
    local t0 t next_key=0
    t0="$(elapsed)"
    while :; do
        t="$(awk -v a="$(elapsed)" -v b="$t0" 'BEGIN { print a - b }')"
        later "$t" "$1" && break
        if [ "${2:-}" = space ] && later "$t" "$next_key"; then
            tkey space; next_key="$(awk -v n="$t" -v e="$3" 'BEGIN { print n + e }')"
        fi
        tshot brief; sleep "$BRIEF_STEP"
    done
}

wave_and_exit() {  # ESC into the wave, then the Alt+X path
    tkey Escape; sleep 0.3; tshot wave-start; sleep 0.7; tshot wave-1s; sleep 1; tshot wave-2s
    windows_now
    tkey alt+x; sleep 1.5; tshot exit-dialog; windows_now
    tclick $XY_EXIT_CANCEL "Cancel (#244)"; sleep 1; tshot exit-cancelled
    tkey alt+x; sleep 1.5; tshot exit-dialog-2
    tclick $XY_EXIT_EXIT "Exit (#244)"; sleep 2; tshot after-exit; windows_now
    after_exit Exit
}

# after_exit WHAT: the dialogs that follow Exit. WHAT=Exit leaves #799 by Exit, Order by
# Order.. (then the Order dialog's buttons); a remaining dialog of another kind is shot and
# left by Return (its default button).
after_exit() {
    local i
    if [ -n "$(visible '^Registration Benefits$')" ]; then
        if [ "$1" = Order ]; then
            tclick $XY_799_ORDER "Order.. (#799)"; sleep 2; tshot order-from-799; windows_now
            order_buttons
        else
            tclick $XY_799_EXIT "Exit (#799)"; sleep 2
        fi
    fi
    for i in 1 2 3; do
        game_running || return 0
        if [ -n "$(xdotool search --onlyvisible --name '.' 2>/dev/null)" ]; then
            windows_now; tshot "exit-left-$i"; tkey Return; sleep 2
        fi
    done
}

# order_buttons: the Order dialog #693, every button.
order_buttons() {
    tclick $XY_ORDER_REGISTER "Register, empty code"; sleep 1.5; tshot order-register-empty; windows_now
    tclick $XY_ORDER_CODE "Registration # edit"; xdotool type --delay 80 123; step_log "type 123"
    sleep 0.5; tshot order-code
    tclick $XY_ORDER_REGISTER "Register, code 123"; sleep 1.5; tshot order-register-wrong; windows_now
    tkey Return; sleep 1; tshot order-message-closed
    tclick $XY_ORDER_FORM "Order Form.."; close_notepad order-form-notepad
    tclick $XY_ORDER_MOREINFO "More Info.."; close_notepad order-moreinfo-notepad
    tshot order-before-cancel
    tclick $XY_ORDER_CANCEL "Cancel (#693)"; sleep 1.5; tshot order-cancelled; windows_now
}

run_main() {
    start_run main
    to_newgame 1
    desk || { finish_run; return; }
    brief "$BRIEF_SECS"
    wave_and_exit
    finish_run
}

run_briefesc() {
    start_run briefesc
    to_newgame 0
    desk || { finish_run; return; }
    brief "$BRIEF_ESC_AT"
    tkey Escape; step_log "ESC in the briefing"
    sleep 0.3; tshot wave-start; sleep 0.7; tshot wave-1s; sleep 1; tshot wave-2s
    tkey alt+x; sleep 1.5; tshot exit-dialog
    tclick $XY_EXIT_EXIT "Exit (#244)"; sleep 2; tshot after-exit; after_exit Exit
    finish_run
}

run_briefspace() {
    start_run briefspace
    to_newgame 0
    desk || { finish_run; return; }
    brief "$SPACE_SECS" space "$SPACE_EVERY"
    wave_and_exit
    finish_run
}

run_title() {
    local t
    start_run title
    sleep 1; tkey Escape; sleep "$WAIT_STEP"; tshot title-restore; windows_now
    tclick $XY_RESTORE_CANCEL "Cancel (Restore Game)"
    for t in $(seq 1 $(( TITLE_AFTER * 2 ))); do sleep 0.5; tshot title-after-cancel; done
    windows_now
    tkey r; sleep 2; tshot title-r; windows_now
    if [ -n "$(visible '^Order Inner Space Now!$')" ]; then order_buttons; fi
    for t in $(seq 1 $(( TITLE_AFTER * 2 ))); do sleep 0.5; tshot after-r; done
    tkey alt+x; sleep 1.5; tshot title-exit-dialog; windows_now
    tclick $XY_EXIT_SAVE "Save & Exit (#244)"; sleep 1.5; tshot title-save-exit; windows_now
    if [ -n "$(visible '^Save Game$')" ]; then step_log "Save Game opened"; tkey Escape; sleep 1; fi
    if [ -n "$(visible '^Inner Space$')" ]; then
        tclick $XY_EXIT_CANCEL "Cancel (#244)"; sleep 1; tshot title-exit-cancelled
    fi
    tkey alt+x; sleep 1.5; tshot title-exit-dialog-2
    tclick $XY_EXIT_EXIT "Exit (#244)"; sleep 2; tshot title-after-exit; windows_now
    after_exit Order
    finish_run
}

log "start.sh edition $EDITION runs '$RUNS' prefix $WINEPREFIX display $DISPLAY"
for run in $RUNS; do
    case "$run" in
        main|briefesc|briefspace|title) "run_$run" ;;
        *) log "unknown run $run (main, briefesc, briefspace, title)" ;;
    esac
done
[ "$EDITION" = reg ] && [ "$KEEP" != 1 ] && reg_cleanup
rm -f "$OUT/.launch"
log "done"
