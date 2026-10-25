#!/bin/bash
# dialogs_port.sh: open each ENG_LANG.DLL dialog in isview under Wine on :151, record the X window
# geometry of the dialog (found by its caption) and capture it.
# Usage: oracle/scenarios/dialogs_port.sh ISVIEW_EXE [ID...]   (PORT_WINE overrides the Wine)
# Output: work/oracle/dialogs/port/<id>.png, port/geometry.tsv (id, caption, X window line).
set -u
repo="$(cd "$(dirname "$0")/../.." && pwd)"
exe="$1"; shift
out="$repo/work/oracle/dialogs/port"
mkdir -p "$out"
export DISPLAY=:151 WINEPREFIX="$repo/work/wine/isview" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml="
ids=("$@")
if [ ${#ids[@]} -eq 0 ]; then
    mapfile -t ids < <("$repo/re/.venv/bin/python" -I -c '
import json, sys
for d in json.load(open(sys.argv[1])): print(d["id"])' "$repo/work/assets/eng_lang.dll/dialogs.json")
fi
caption_of() {
    "$repo/re/.venv/bin/python" -I -c '
import json, sys
for d in json.load(open(sys.argv[1])):
    if d["id"] == int(sys.argv[2]): print(d["caption"])' "$repo/work/assets/eng_lang.dll/dialogs.json" "$1"
}
for id in "${ids[@]}"; do
    cap="$(caption_of "$id")"
    "${PORT_WINE:-$HOME/opt/wine/bin/wine}" "$exe" "Z:$repo/work/extracted/shipdisk" --dialog "$id" >/dev/null 2>&1 &
    pid=$!
    line=""
    for _ in $(seq 1 80); do
        sleep 0.25
        line="$(xwininfo -root -tree 2>/dev/null | grep -F "\"$cap\": (\"isview.exe\"" | head -1)"
        [ -n "$line" ] && break
    done
    sleep 1.5
    line="$(xwininfo -root -tree 2>/dev/null | grep -F "\"$cap\": (\"isview.exe\"" | head -1)"
    if [ -n "$line" ]; then
        win="$(awk '{print $1}' <<<"$line")"
        geom="$(grep -oE '[0-9]+x[0-9]+\+-?[0-9]+\+-?[0-9]+' <<<"$line" | head -1)"
        import -display "$DISPLAY" -window "$win" "$out/$id.png"
        printf '%s\t%s\t%s\n' "$id" "$cap" "$geom" | tee -a "$out/geometry.tsv"
    else
        printf '%s\t%s\tNOT FOUND\n' "$id" "$cap" | tee -a "$out/geometry.tsv"
        import -display "$DISPLAY" -window root "$out/$id-root.png"
    fi
    kill "$pid" 2>/dev/null
    for _ in $(seq 1 40); do kill -0 "$pid" 2>/dev/null || break; sleep 0.25; done
    sleep 0.5
done
