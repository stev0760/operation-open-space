#!/usr/bin/env bash
# dialogs_orig.sh OUTDIR: fresh oracle prefix (ORACLE_ID=9) on display :152, the original
# started traced and left running (KEEP), plus a watcher that logs every change of the
# named top-level X windows to OUTDIR/windows.log. The dialogs are then opened by hand
# with xdotool (steps in OUTDIR/steps.txt).
set -u
export ORACLE_ID=9
OUT_DEFAULT=dialogs
# shellcheck source=/dev/null
source "$(cd "$(dirname "$0")/../.." && pwd)/oracle/scenarios/common.sh" "$@"
export DISPLAY=:152
scenario_prefix
scenario_launch
setsid -f "$REPO/oracle/scenarios/watch_windows.sh" "$OUT/windows.log" >/dev/null 2>&1
log "watcher started on $DISPLAY"
