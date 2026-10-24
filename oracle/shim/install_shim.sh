#!/usr/bin/env bash
# Build the CreateCompatibleBitmap(NULL) shim and install it into $WINEPREFIX as a
# native 32-bit version.dll (see version_shim.c), or remove it again.
# Usage (after sourcing oracle/env.sh):  oracle/shim/install_shim.sh [install|remove]
# Build output: work/oracle/shim/version.dll. Installed copy: drive_c/windows/syswow64.
set -eu
SHIM_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$SHIM_DIR/../.." && pwd)"
BUILD="$REPO/work/oracle/shim"
: "${WINEPREFIX:?source oracle/env.sh first}"
SYSWOW="$WINEPREFIX/drive_c/windows/syswow64"

case "${1:-install}" in
install)
    mkdir -p "$BUILD"
    i686-w64-mingw32-gcc -O2 -s -shared -Wl,--no-insert-timestamp -Wl,--enable-stdcall-fixup \
        -o "$BUILD/version.dll" "$SHIM_DIR/version_shim.c" "$SHIM_DIR/version_shim.def" -lgdi32
    [ -f "$SYSWOW/version.dll.wine" ] || cp "$SYSWOW/version.dll" "$SYSWOW/version.dll.wine"
    cp "$BUILD/version.dll" "$SYSWOW/version.dll"
    (cd "$WINEPREFIX/drive_c" && wine reg add 'HKCU\Software\Wine\DllOverrides' /v version /t REG_SZ /d native,builtin /f >/dev/null)
    echo "shim  installed: $SYSWOW/version.dll (version=native,builtin)"
    ;;
remove)
    [ -f "$SYSWOW/version.dll.wine" ] && mv "$SYSWOW/version.dll.wine" "$SYSWOW/version.dll"
    (cd "$WINEPREFIX/drive_c" && wine reg delete 'HKCU\Software\Wine\DllOverrides' /v version /f >/dev/null 2>&1) || true
    echo "shim  removed"
    ;;
esac
