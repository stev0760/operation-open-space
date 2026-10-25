# Source this file: sets up Wine + a headless X display for oracle runs.
# Usage: ORACLE_ID=1 source oracle/env.sh   (each parallel runner uses its own ORACLE_ID)
#
# Runtime (ORACLE_WINE, the Wine install whose bin/wine runs the game). The runtimes live
# under ~/opt on the workstation, installed without root; each name below is a directory there:
#   wine-11.0-oldwow-patched  default. Wine 11.0 in old-WoW64 mode (Kron4ek "amd64"
#                           build, with 32-bit Unix libraries) plus
#                           oracle/wine/shell16-extracticon-count.patch: the Win16 icon
#                           count query ExtractIcon(…, 0xFFFF) returns the count, so the
#                           Navigation status line, the briefing and the HUD show the
#                           fixtures' own icons as under otvdm [ORACLE wave-0001]. The
#                           original plays the desk intro, the Navigation window, the
#                           wave briefing and wave 1 without a fault [ORACLE runtime-0001]
#                           [ORACLE newgame-0002]. Build: oracle/wine/build_patched.sh
#                           ~/opt/wine-11.0-oldwow ~/opt/wine-11.0-oldwow-patched
#                           shell16-extracticon-count.patch.
#   wine-11.0-oldwow   the same without the patch. ExtractIcon(…, 0xFFFF) returns 0,
#                           so the game counts no icons per file and draws a generic green
#                           "D:>" box for each icon [ORACLE runtime-0001]. Use it to compare
#                           with digests made on it (newgame-0002, runtime-0001).
#                           Install: oracle/wine/install_kron4ek.sh amd64 ~/opt/wine-11.0-oldwow.
#   wine-11.0-patched  Wine 11.0 new WoW64 with oracle/wine/krnl386-clear-saved-selectors.patch.
#                           Same game result as old WoW64 [ORACLE fault-0001], without the
#                           ExtractIcon patch. Build: oracle/wine/build_patched.sh.
#   wine-11.0          stock Wine 11.0 new WoW64 (~/opt/wine links to it). ISPACE.DLL loses
#                           its DS after GlobalFree and the game faults in the intro and on
#                           Enter in the Navigation window [ORACLE fault-0001]. Use it only to
#                           reproduce that fault.
# A prefix works with each of them: Wine updates it when the runtime changes.

ORACLE_ID="${ORACLE_ID:-0}"
ORACLE_WINE="${ORACLE_WINE:-$HOME/opt/wine-11.0-oldwow-patched}"
[ -x "$ORACLE_WINE/bin/wine" ] || echo "oracle/env.sh: no Wine at $ORACLE_WINE (see oracle/wine/)" >&2
export PATH="$ORACLE_WINE/bin:$PATH"
export WINEPREFIX="${WINEPREFIX:-$HOME/.local/share/wineprefixes/innerspace${ORACLE_ID/#0/}}"
export WINEDEBUG="${WINEDEBUG:--all}"
# Use the game's own CTL3D; skip Mono/Gecko install prompts.
export WINEDLLOVERRIDES="${WINEDLLOVERRIDES:-ctl3d=n;mscoree=;mshtml=;winegstreamer=}"
export DISPLAY=":$((110 + ORACLE_ID))"  # above :0 and the desktop's Xwayland displays
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
