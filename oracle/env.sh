# Source this file: sets up Wine + a headless X display for oracle runs.
# Usage: ORACLE_ID=1 source oracle/env.sh   (each parallel runner uses its own ORACLE_ID)

ORACLE_ID="${ORACLE_ID:-0}"
ORACLE_WINE="${ORACLE_WINE:-/opt/wine}"   # another runtime: ORACLE_WINE=/opt/wine-<name>
export PATH="$ORACLE_WINE/bin:$PATH"
export WINEPREFIX="${WINEPREFIX:-$HOME/.local/share/wineprefixes/innerspace${ORACLE_ID/#0/}}"
export WINEDEBUG="${WINEDEBUG:--all}"
# Use the game's own CTL3D; skip Mono/Gecko install prompts.
export WINEDLLOVERRIDES="${WINEDLLOVERRIDES:-ctl3d=n;mscoree=;mshtml=;winegstreamer=}"
export DISPLAY=":$((110 + ORACLE_ID))"  # :99 is taken by an unrelated Xvfb
unset WAYLAND_DISPLAY

oracle_xvfb() {  # oracle_xvfb [depth]  — start Xvfb on $DISPLAY if not running
    local depth="${1:-24}"
    if ! xdpyinfo -display "$DISPLAY" >/dev/null 2>&1; then
        setsid -f Xvfb "$DISPLAY" -screen 0 "1024x768x${depth}" -nolisten tcp >/dev/null 2>&1
        for _ in $(seq 50); do xdpyinfo -display "$DISPLAY" >/dev/null 2>&1 && break; sleep 0.1; done
    fi
}

oracle_shot() {  # oracle_shot out.png
    import -display "$DISPLAY" -window root "$1"
}
