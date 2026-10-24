#!/usr/bin/env bash
# watch_windows.sh LOGFILE: appends the named top-level X windows of $DISPLAY to LOGFILE whenever
# the list changes (polled every 0.2 s). Runs until killed.
prev=""
while true; do
    cur="$(xwininfo -root -tree 2>/dev/null | grep -E '^\s+0x[0-9a-f]+ "' \
        | grep -v -E '"Default IME"|"OleMainThreadWndName"' | sed 's/^ *//' | sort)"
    if [ "$cur" != "$prev" ]; then
        { printf '%s ---\n' "$(date +%T.%3N)"; printf '%s\n' "$cur"; } >>"$1"
        prev="$cur"
    fi
    sleep 0.2
done
