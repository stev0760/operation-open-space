#!/usr/bin/env bash
# Build Wine 11.0's krnl386.exe16 with krnl386-clear-saved-selectors.patch and install a
# Wine copy whose only difference from the stock install is that module (digest fault-0001).
# Usage: oracle/wine/build_patched.sh STOCK_WINE_DIR PATCHED_WINE_DIR
#   e.g. oracle/wine/build_patched.sh /opt/wine-11.0 /opt/wine-11.0-patched
#   STOCK_WINE_DIR is Kron4ek Wine 11.0 amd64-wow64 (install_kron4ek.sh amd64-wow64 /opt/wine-11.0).
# Environment:
#   WINE_WORK=work/wine          work directory (source, build tree, logs, stripped module)
#   WINE_SRC=$WINE_WORK/wine-src  a wine-11.0 checkout; cloned (tag wine-11.0) when absent
#   WINE_BUILD=$WINE_WORK/wine-build  out-of-tree build directory
# Needs: git, gcc, flex, bison, i686-w64-mingw32-gcc, x86_64-w64-mingw32-gcc; sudo when
# PATCHED_WINE_DIR's parent is not writable. PATCHED_WINE_DIR is created as a copy of
# STOCK_WINE_DIR when it does not exist; only lib/wine/i386-windows/krnl386.exe16 changes.
set -eu
STOCK="${1:?usage: build_patched.sh STOCK_WINE_DIR PATCHED_WINE_DIR}"
DEST="${2:?usage: build_patched.sh STOCK_WINE_DIR PATCHED_WINE_DIR}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
PATCH="$HERE/krnl386-clear-saved-selectors.patch"
WORK="$(realpath -m "${WINE_WORK:-$REPO/work/wine}")"
SRC="$(realpath -m "${WINE_SRC:-$WORK/wine-src}")"
BUILD="$(realpath -m "${WINE_BUILD:-$WORK/wine-build}")"
mkdir -p "$WORK"

[ -d "$SRC/.git" ] || git clone --depth 1 --branch wine-11.0 https://gitlab.winehq.org/wine/wine.git "$SRC"
if git -C "$SRC" apply --check -R "$PATCH" 2>/dev/null; then
    echo "patch already applied in $SRC"
elif git -C "$SRC" apply --check "$PATCH"; then
    git -C "$SRC" apply "$PATCH"
else
    echo "patch does not apply to $SRC" >&2; exit 1
fi

# Rebuilds give identical bytes: PE timestamps (linker and strip) come from the source tag,
# and source paths embedded through __FILE__ are made relative to the checkout.
SOURCE_DATE_EPOCH="$(git -C "$SRC" log -1 --format=%ct)"; export SOURCE_DATE_EPOCH
mkdir -p "$BUILD"
if [ ! -f "$BUILD/Makefile" ]; then
    (cd "$BUILD" && "$SRC/configure" CROSSCFLAGS="-g -O2 -ffile-prefix-map=$SRC/=" \
        --enable-archs=i386,x86_64 --disable-tests --without-x \
        --without-freetype --without-gstreamer --without-pulse --without-alsa --without-oss \
        --without-cups --without-dbus --without-gnutls --without-sane --without-usb --without-v4l2 \
        --without-wayland --without-vulkan --without-opengl --without-sdl --without-capi \
        --without-gphoto --without-krb5 --without-netapi --without-pcap --without-pcsclite \
        --without-udev --without-unwind --without-fontconfig --without-ffmpeg >"$WORK/configure.log")
fi
make -C "$BUILD" -j"$(nproc)" dlls/krnl386.exe16/i386-windows/krnl386.exe16 >"$WORK/build.log"
i686-w64-mingw32-strip --strip-debug -o "$WORK/krnl386.exe16" \
    "$BUILD/dlls/krnl386.exe16/i386-windows/krnl386.exe16"

SUDO=""; [ -w "$(dirname "$DEST")" ] || SUDO=sudo
[ -e "$DEST" ] || $SUDO cp -a "$STOCK" "$DEST"
$SUDO cp "$WORK/krnl386.exe16" "$DEST/lib/wine/i386-windows/krnl386.exe16"
sha256sum "$WORK/krnl386.exe16"
diff -rq "$STOCK" "$DEST" || true   # lists krnl386.exe16 only
