#!/usr/bin/env bash
# Build Wine 11.0 16-bit modules with oracle patches and install a Wine copy whose only
# difference from the stock install is those modules.
# Usage: oracle/wine/build_patched.sh STOCK_WINE_DIR PATCHED_WINE_DIR [PATCH...]
#   PATCH  a patch file (a bare name is looked up in oracle/wine/). Each patch changes one
#          module, dlls/MODULE/, and the copy gets that module's lib/wine/i386-windows/MODULE.
#          Default: krnl386-clear-saved-selectors.patch.
# The two oracle installs (see oracle/env.sh):
#   build_patched.sh /opt/wine-11.0 /opt/wine-11.0-patched
#       new WoW64 (install_kron4ek.sh amd64-wow64) with the krnl386 patch (digest fault-0001).
#   build_patched.sh /opt/wine-11.0-oldwow /opt/wine-11.0-oldwow-patched shell16-extracticon-count.patch
#       old WoW64 (install_kron4ek.sh amd64) with the shell.dll16 ExtractIcon count patch
#       (digest wave-0001); the oracle default runtime.
# Both Kron4ek builds ship byte-identical i386-windows krnl386.exe16 and shell.dll16, so one
# build tree serves both.
# Environment:
#   WINE_WORK=work/wine          work directory (source, build tree, logs, stripped modules)
#   WINE_SRC=$WINE_WORK/wine-src  a wine-11.0 checkout; cloned (tag wine-11.0) when absent
#   WINE_GIT=https://gitlab.winehq.org/wine/wine.git  where to clone it from (a local
#                                 wine repository with the wine-11.0 tag also works)
#   WINE_BUILD=$WINE_WORK/wine-build  out-of-tree build directory
# Needs: git, gcc, flex, bison, i686-w64-mingw32-gcc, x86_64-w64-mingw32-gcc; sudo when
# PATCHED_WINE_DIR's parent is not writable. PATCHED_WINE_DIR is created as a copy of
# STOCK_WINE_DIR when it does not exist; an existing one only gets the listed modules.
set -eu
STOCK="${1:?usage: build_patched.sh STOCK_WINE_DIR PATCHED_WINE_DIR [PATCH...]}"
DEST="${2:?usage: build_patched.sh STOCK_WINE_DIR PATCHED_WINE_DIR [PATCH...]}"
shift 2
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
[ $# -gt 0 ] || set -- krnl386-clear-saved-selectors.patch
WORK="$(realpath -m "${WINE_WORK:-$REPO/work/wine}")"
SRC="$(realpath -m "${WINE_SRC:-$WORK/wine-src}")"
BUILD="$(realpath -m "${WINE_BUILD:-$WORK/wine-build}")"
GIT_URL="${WINE_GIT:-https://gitlab.winehq.org/wine/wine.git}"
mkdir -p "$WORK"

# patch_module PATCH: the single dlls/MODULE that PATCH changes.
patch_module() {
    local mods
    mods="$(sed -n 's|^+++ b/dlls/\([^/]*\)/.*|\1|p' "$1" | sort -u)"
    [ "$(printf '%s\n' "$mods" | grep -c .)" = 1 ] || { echo "$1 must change exactly one dlls/ module" >&2; return 1; }
    printf '%s\n' "$mods"
}

[ -d "$SRC/.git" ] || git clone --depth 1 --branch wine-11.0 "$GIT_URL" "$SRC"
PATCHES=(); MODULES=()
for p in "$@"; do
    case "$p" in */*) ;; *) p="$HERE/$p" ;; esac
    p="$(realpath "$p")"
    mod="$(patch_module "$p")"
    if git -C "$SRC" apply --check -R "$p" 2>/dev/null; then
        echo "already applied in $SRC: $(basename "$p")"
    elif git -C "$SRC" apply --check "$p"; then
        git -C "$SRC" apply "$p"
    else
        echo "patch does not apply to $SRC: $p" >&2; exit 1
    fi
    PATCHES+=("$p"); MODULES+=("$mod")
done

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
SUDO=""; [ -w "$(dirname "$DEST")" ] || SUDO=sudo
[ -e "$DEST" ] || $SUDO cp -a "$STOCK" "$DEST"
for mod in "${MODULES[@]}"; do
    make -C "$BUILD" -j"$(nproc)" "dlls/$mod/i386-windows/$mod" >"$WORK/build-$mod.log"
    i686-w64-mingw32-strip --strip-debug -o "$WORK/$mod" "$BUILD/dlls/$mod/i386-windows/$mod"
    $SUDO cp "$WORK/$mod" "$DEST/lib/wine/i386-windows/$mod"
    sha256sum "$WORK/$mod"
done
diff -rq "$STOCK" "$DEST" || true   # lists the replaced modules only
