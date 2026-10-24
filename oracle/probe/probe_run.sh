#!/usr/bin/env bash
# Probe run (digest probe-0001): oracle/scenarios/newgame.sh with KEEP=1, and isprobe beside it.
#   title (launch + WAIT_TITLE s): isprobe find; dumps of both DGROUPs; watchers start
#   Navigation window ("shot 12-navigation"): dumps
#   wave 1 (scenario end, game kept running): dumps, then snapshot / Up for UP_SECS s / snapshot,
#   UP_PRESSES times, with a screenshot at each snapshot; then the watchers and the game stop.
# Usage: ORACLE_ID=3 oracle/probe/probe_run.sh [OUTDIR]
#   OUTDIR defaults to work/probe/run; dumps go to OUTDIR/../dumps (work/probe/dumps).
# Environment: ORACLE_ID, ORACLE_WINE (oracle/env.sh), WAIT_BOOT (scenario, default 40 here so
#   the title stays up while the probe runs), WAIT_TITLE=26, WATCH="0x9684 0x9320",
#   WATCH_LEN=4, WATCH_MS=50, UP_PRESSES=2, UP_SECS=1. Needs work/probe/isprobe.exe
#   (oracle/probe/build.sh).
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
OUT="$(realpath -m "${1:-$REPO/work/probe/run}")"
DUMPS="$(dirname "$OUT")/dumps"
PROBE="$REPO/work/probe/isprobe.exe"
export WAIT_BOOT="${WAIT_BOOT:-40}"
WAIT_TITLE="${WAIT_TITLE:-26}"; WATCH="${WATCH:-0x9684 0x9320}"; WATCH_LEN="${WATCH_LEN:-4}"
WATCH_MS="${WATCH_MS:-50}"; UP_PRESSES="${UP_PRESSES:-2}"; UP_SECS="${UP_SECS:-1}"
# shellcheck source=../env.sh
source "$REPO/oracle/env.sh"
# shellcheck source=../trace.sh
source "$REPO/oracle/trace.sh"
[ -f "$PROBE" ] || { echo "no $PROBE: run oracle/probe/build.sh" >&2; exit 1; }
mkdir -p "$OUT" "$DUMPS"; rm -f "$DUMPS"/*.bin "$OUT"/probe-*.txt "$OUT"/probe-*.png "$OUT"/steps.txt
plog() { printf '%s %s\n' "$(date +%T.%3N)" "$*" | tee -a "$OUT/probe-steps.txt"; }
# Start the probe by a relative path from the repo: with Z: removed (newgame.sh), Wine 11.0
# segfaults in get_full_path when the exe is given as an absolute Unix path.
probe() { (cd "$REPO" && wine work/probe/isprobe.exe "$@"); }
wait_step() {  # wait_step PATTERN TIMEOUT_S: wait for PATTERN in the scenario's steps.txt
    local i
    for i in $(seq $(( $2 * 10 ))); do
        grep -q "$1" "$OUT/steps.txt" 2>/dev/null && return 0
        sleep 0.1
    done
    plog "timeout waiting for '$1'"; return 1
}
dumps() {  # dumps PHASE: whole selector range of both DGROUPs
    probe ${SEL_INSPACE:+--sel $SEL_INSPACE} dump INSPACE DGROUP - all >"$DUMPS/$1-inspace.bin" 2>>"$OUT/probe-log.txt"
    probe ${SEL_ISPACE:+--sel $SEL_ISPACE} dump ISPACE DGROUP - all >"$DUMPS/$1-ispace.bin" 2>>"$OUT/probe-log.txt"
    plog "dumps $1 ($(stat -c %s "$DUMPS/$1-inspace.bin") + $(stat -c %s "$DUMPS/$1-ispace.bin") bytes)"
}

ORACLE_ID="$ORACLE_ID" KEEP=1 "$REPO/oracle/scenarios/newgame.sh" "$OUT" >"$OUT/probe-scenario.out" 2>&1 &
SCEN=$!
plog "scenario started (pid $SCEN, WAIT_BOOT=$WAIT_BOOT, wine $ORACLE_WINE)"
wait_step " launch" 60 || exit 1
sleep "$WAIT_TITLE"
oracle_shot "$OUT/probe-title.png"; plog "shot probe-title"
probe find >"$OUT/probe-find.txt" 2>&1; plog "find: $(tr '\n' ';' <"$OUT/probe-find.txt")"
SEL_INSPACE="$(awk '/^INSPACE.EXE +selector/ && / data/ {print $3; exit}' "$OUT/probe-find.txt")"
SEL_ISPACE="$(awk '/^ISPACE.DLL +selector/ && / data/ {print $3; exit}' "$OUT/probe-find.txt")"
plog "selectors INSPACE=${SEL_INSPACE:-?} ISPACE=${SEL_ISPACE:-?}"
dumps title
WPIDS=()
for a in $WATCH; do
    probe ${SEL_INSPACE:+--sel $SEL_INSPACE} watch INSPACE "DGROUP:$a" "$WATCH_LEN" "$WATCH_MS" 0 \
        >"$OUT/probe-watch-$a.txt" 2>&1 &
    WPIDS+=($!)
done
plog "watchers started: $WATCH (len $WATCH_LEN, $WATCH_MS ms)"

wait_step "shot 12-navigation" 120 && { dumps navigation; oracle_shot "$OUT/probe-navigation.png"; plog "shot probe-navigation"; }
wait "$SCEN"; plog "scenario finished (game kept running)"
oracle_shot "$OUT/probe-wave.png"; plog "shot probe-wave"
dumps wave
for i in $(seq 0 "$UP_PRESSES"); do
    if [ "$i" -gt 0 ]; then
        xdotool keydown Up; sleep "$UP_SECS"; xdotool keyup Up; plog "key Up held ${UP_SECS}s"
    fi
    dumps "move$i"; oracle_shot "$OUT/probe-move$i.png"; plog "shot probe-move$i"
done
kill "${WPIDS[@]}" 2>/dev/null; plog "watchers stopped"
cp "$WINEPREFIX/drive_c/windows/INSPACE.INI" "$OUT/INSPACE.INI"
oracle_game_stop; plog "game stopped"
oracle_crash_collect "$OUT/trace.log"
