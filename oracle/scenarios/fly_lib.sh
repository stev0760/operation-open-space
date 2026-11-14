# Shared parts of the user-flown recording session (fly.sh) and of the scripted recordings
# that reuse its machinery (watchers, video, sink, redaction). Source it after oracle/scenarios/common.sh
# (which defines OUT, REPO, ORACLE_DIR, PY, log, shot, click, key, hold and sources oracle/env.sh).
# shellcheck shell=bash

probe() { (cd "$REPO" && exec wine work/probe/isprobe.exe "$@"); }

# Watchers: NAME:OFFSET:LEN:MS, DGROUP offsets of INSPACE.EXE. isprobe prints at most 64 bytes
# of a span. The player's ship record is DGROUP:0x7090 (0x15a bytes, structs.md), ship i is
# 0x15a * i further. Pools: allocated counts DGROUP:0x92ec + 2k, in-use counts DGROUP:0x9406 + 2k.
fly_default_watches() {
    local w="fuel:0x70ae:2:20 systems:0x70b6:8:20 score:0x1e58:4:20 wave:0x91f8:2:20 state:0x90f4:2:20"
    w="$w pools:0x9406:0x2a:20 alloc:0x92ec:0x2a:20 shipcnt:0x9316:2:20 shippool:0x9324:2:20 playeridx:0x9646:2:20"
    w="$w arrest:0x93a4:2:20 slot:0x70a8:2:20 credit:0x716c:4:20 amb:0x9e41:0x20:50 mis:0x66e0:0x38:50"
    local k i
    for k in 0 1 2 3 4 5; do   # the player's record
        w="$w rec$k:$(printf '0x%x' $((0x7090 + 0x40 * k))):$([ $k = 5 ] && echo 0x1a || echo 0x40):20"
    done
    for i in 1 2 3 4 5 6 7; do   # the other ships: +0x40 .. +0x80 (design, owner words, +0x52 ... +0x5e) and +0xe0 .. +0xf0 (alive +0xec)
        w="$w s${i}b:$(printf '0x%x' $((0x7090 + 0x15a * i + 0x40))):0x40:20 s${i}c:$(printf '0x%x' $((0x7090 + 0x15a * i + 0xe0))):0x10:20"
    done
    echo "$w"
}
# watch_start [SPECFILE]: the default watchers plus the lines NAME:OFF:LEN:MS of SPECFILE (comments with #)
watch_start() {
    local specs w name off len ms sel
    specs="$(fly_default_watches)"
    if [ -n "${1:-}" ] && [ -f "$1" ]; then specs="$specs $(grep -v '^[[:space:]]*#' "$1" | tr '\n' ' ')"; fi
    probe find >"$OUT/probe-find.txt" 2>&1
    sel="$(awk '/^INSPACE.EXE +selector/ && / data/ {print $3; exit}' "$OUT/probe-find.txt")"
    log "probe find: selector ${sel:-?}"
    WPIDS=()
    for w in $specs; do
        IFS=: read -r name off len ms <<<"$w"
        probe ${sel:+--sel "$sel"} watch INSPACE "DGROUP:$off" "$len" "${ms:-20}" 0 >>"$OUT/watch-$name.txt" 2>&1 &
        WPIDS+=($!)
    done
    echo "${WPIDS[*]}" >"$OUT/.wpids"
    log "watchers started (${#WPIDS[@]}): $specs"
}
watch_stop() { kill $(cat "$OUT/.wpids" 2>/dev/null) 2>/dev/null; log "watchers stopped"; }

# Lossless video with wall-clock timestamps (the PTS are the epoch time in microseconds).
# rec_on NAME [FPS [WINDOW_ID WxH]]: the whole display, or one window.
rec_on() {
    local fps="${2:-60}" src=(-video_size 1024x768 -i "$DISPLAY")
    [ -n "${3:-}" ] && src=(-window_id "$3" -video_size "${4:-1024x768}" -i "$DISPLAY")
    date +%s.%N >"$OUT/vid-$1.start"
    ffmpeg -nostdin -hide_banner -loglevel error -f x11grab -framerate "$fps" \
        -use_wallclock_as_timestamps 1 "${src[@]}" -c:v ffv1 -level 3 -fps_mode passthrough -copyts \
        "$OUT/vid-$1.mkv" >"$OUT/vid-$1.log" 2>&1 &
    echo $! >"$OUT/.rec-$1"; sleep 0.3; log "rec $1 on at $fps fps (pid $(cat "$OUT/.rec-$1"))"
}
rec_off() {
    local p; p="$(cat "$OUT/.rec-$1" 2>/dev/null)"
    [ -n "$p" ] && kill -INT "$p" 2>/dev/null
    for _ in $(seq 100); do kill -0 "$p" 2>/dev/null || break; sleep 0.1; done
    rm -f "$OUT/.rec-$1"; log "rec $1 off"
}

# Audio: headless runs use a null sink of their own, the user's runs the default sink.
sink_on() {
    SINK_MODULE="$(pactl load-module module-null-sink sink_name="$1" 2>/dev/null)" || SINK_MODULE=""
    [ -n "$SINK_MODULE" ] && export PULSE_SINK="$1"
    log "null sink $1 module ${SINK_MODULE:-none}"
}
sink_off() { [ -n "${SINK_MODULE:-}" ] && pactl unload-module "$SINK_MODULE" 2>/dev/null; SINK_MODULE=""; }

# The registered prefix of this runner: a copy of an existing registered prefix, made once.
# fly_prefix SRC DST: cp -a when DST is missing. Registration values are never read or printed.
fly_prefix() {
    if [ ! -d "$2/drive_c/SPACE" ]; then
        [ -d "$1/drive_c/SPACE" ] || { echo "fly: no registered prefix at $1 (REG_SRC=)" >&2; return 1; }
        log "copying the registered prefix $1 -> $2"; mkdir -p "$(dirname "$2")"; cp -a "$1" "$2"
    fi
}
# fly_setup_game: INI keys (SHIP, INI_SET words KEY=VALUE), fixture as E:, drive map, trace config.
fly_setup_game() {
    oracle_game_stop
    cd "$WINEPREFIX/drive_c" || exit 1
    "$PY" -I - "$ORACLE_DIR" "$WINEPREFIX/drive_c/windows/INSPACE.INI" "$SHIP" ${INI_SET:-} <<'PYEOF' || exit 1
import pathlib, sys
sys.path.insert(0, sys.argv[1])
from install_layout import set_ini_keys
ini = pathlib.Path(sys.argv[2])
keys = {"SoundSystem": "1", "MixChannels": "1", "Music": "0", "MainShip": sys.argv[3]}
keys.update(dict(kv.split("=", 1) for kv in sys.argv[4:]))
ini.write_bytes(set_ini_keys(ini.read_bytes().decode("latin-1"), "Inner Space", keys).encode("latin-1"))
PYEOF
    "$PY" "$ORACLE_DIR/fixtures/make_fixtures.py" "$REPO/work/oracle-$ORACLE_ID" --tree "$FIXTURE" \
        --map-prefix "$WINEPREFIX" --drive e >/dev/null || exit 1
    wine reg add 'HKLM\Software\Wine\Drives' /v e: /t REG_SZ /d hd /f >/dev/null 2>&1
    wine reg delete 'HKCU\Software\Wine\DllOverrides' /f >/dev/null 2>&1
    oracle_crash_config; oracle_trace_config
    log "registered copy $WINEPREFIX display $DISPLAY ship '$SHIP' fixture $FIXTURE as E: ini_set '${INI_SET:-}'"
}

# Menus to the first frame of wave 1 and the full status display (1024x768 display, no window manager).
fly_reach_wave() {
    sleep "${WAIT_BOOT:-20}"; shot title
    click $XY_TITLE; sleep 1; key Escape
    sleep "$WAIT_STEP"; click $XY_RESTORE_GAME; sleep 1; click $XY_RESTORE_NEW_GAME; sleep 0.5
    click $XY_RESTORE_OK; log "Restore Game: NEW GAME, OK"
    sleep "$WAIT_STEP"; click $XY_NEWGAME_DRIVE; sleep 1; click $XY_NEWGAME_DRIVE_D
    sleep 0.5; shot newgame; click $XY_NEWGAME_OK; log "New Game: drive entry 2, OK"
    sleep "${WAIT_SKIP:-3}"; key Escape
    sleep "$WAIT_STEP"; click "$X_NAV_ROW" "$Y_NAV_GAMES_FIRST"; sleep "$WAIT_STEP"; shot navigation
    click $XY_NAV_ENTER; log "click Enter (games)"
    sleep "${WAIT_BRIEF:-8}"; shot briefing
    key Escape; log "wave start"
    sleep 2; shot wave
    key F12; sleep 1; shot f12-box; click 548 446; log "click No"; sleep 1; shot hud
}

# Is the game of this prefix running? (winevdm of INSPACE.EXE whose environment names $WINEPREFIX;
# other runners' games are other prefixes and do not count.)
fly_game_running() {
    local pid
    for pid in $(pgrep -f 'winevdm.*INSPACE.EXE'); do
        tr '\0' '\n' <"/proc/$pid/environ" 2>/dev/null | grep -qx "WINEPREFIX=$WINEPREFIX" && return 0
    done
    return 1
}
# The game has ended: no game process of this prefix for 5 polls in a row after it was seen once.
fly_wait_exit() {
    local seen=1 gone=0 waited=0
    while :; do
        if fly_game_running; then [ "$seen" = 0 ] && log "game process seen"; seen=1; gone=0
        else gone=$((gone + 1)); [ "$seen" = 1 ] && [ "$gone" = 1 ] && log "game process gone"; fi
        [ "$seen" = 1 ] && [ "$gone" -ge 5 ] && return 0
        waited=$((waited + 1)); [ "$seen" = 0 ] && [ "$waited" -ge 240 ] && return 1
        sleep 1
    done
}

# fly_collect: after the game has stopped: INI, files written since launch, copies of them
# (saves, .ded), the trace's crash report, the timeline and the redaction of the text files.
fly_collect() {
    cp "$WINEPREFIX/drive_c/windows/INSPACE.INI" "$OUT/INSPACE.INI"
    find "$WINEPREFIX/drive_c" "$REPO/work/oracle-$ORACLE_ID" -newer "$OUT/.launch" -type f \
        -printf '%TT %10s %p\n' 2>/dev/null | sort >"$OUT/files.txt"
    mkdir -p "$OUT/files"
    find "$WINEPREFIX/drive_c/SPACE" -newer "$OUT/.launch" -type f \( -iname '*.sav' -o -iname '*.ded' -o -iname '*.ini' \) \
        -exec cp -p {} "$OUT/files/" \; 2>/dev/null
    oracle_crash_collect "$OUT/trace.log"
    "$PY" -I "$ORACLE_DIR/scenarios/fly_timeline.py" "$OUT" >"$OUT/timeline.txt" 2>"$OUT/timeline.err"
    "$PY" -I "$ORACLE_DIR/scenarios/fly_redact.py" "$WINEPREFIX" "$OUT"
}
