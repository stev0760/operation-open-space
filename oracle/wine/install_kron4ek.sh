#!/usr/bin/env bash
# Install a Kron4ek Wine 11.0 build as an oracle runtime (see oracle/env.sh).
# Usage: oracle/wine/install_kron4ek.sh VARIANT DEST [TARBALL]
#   VARIANT amd64        old WoW64 (64-bit and 32-bit Unix libraries); the oracle default,
#                        installed as /opt/wine-11.0-oldwow.
#   VARIANT amd64-wow64  new WoW64 (64-bit Unix libraries only); stock Wine 11.0,
#                        installed as /opt/wine-11.0, and the base of build_patched.sh.
#   TARBALL  an already downloaded wine-11.0-VARIANT.tar.xz; otherwise the script downloads
#            it from the Kron4ek/Wine-Builds release 11.0 into work/wine/dl/.
# The tarball must match the sha256 in this script (from the release's sha256sums.txt).
# DEST must not exist. The script uses sudo when it cannot write DEST's parent.
# Old WoW64 needs the 32-bit X11, font and sound libraries on the host; on Rocky/RHEL 9:
#   sudo dnf install glibc.i686 libX11.i686 libXext.i686 libXrender.i686 libXrandr.i686 \
#     libXi.i686 libXcursor.i686 libXinerama.i686 libXcomposite.i686 libXxf86vm.i686 \
#     freetype.i686 fontconfig.i686 libgcc.i686 mesa-libGL.i686 alsa-lib.i686 \
#     pulseaudio-libs.i686 gnutls.i686 libxkbcommon.i686
# The script lists the libraries ntdll.so cannot find after the install.
set -eu
VARIANT="${1:?usage: install_kron4ek.sh amd64|amd64-wow64 DEST [TARBALL]}"
DEST="${2:?usage: install_kron4ek.sh amd64|amd64-wow64 DEST [TARBALL]}"
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
case "$VARIANT" in
    amd64)       SHA=632f2c8e9150841c26d277000e76b82c425cd4564bfb050780705b9d37b2567f ;;
    amd64-wow64) SHA=39574efa1132c3ca0d5c77dd2eddbe4a49cca0d6cc2c290ff4924493a1c40314 ;;
    *) echo "unknown variant $VARIANT (amd64 or amd64-wow64)" >&2; exit 2 ;;
esac
NAME="wine-11.0-$VARIANT.tar.xz"
TARBALL="${3:-$REPO/work/wine/dl/$NAME}"
if [ ! -f "$TARBALL" ]; then
    mkdir -p "$(dirname "$TARBALL")"
    curl -fL --retry 3 -o "$TARBALL.part" "https://github.com/Kron4ek/Wine-Builds/releases/download/11.0/$NAME"
    mv "$TARBALL.part" "$TARBALL"
fi
echo "$SHA  $TARBALL" | sha256sum -c --quiet - || { echo "sha256 mismatch: $TARBALL" >&2; exit 1; }
[ ! -e "$DEST" ] || { echo "$DEST exists" >&2; exit 1; }
SUDO=""; [ -w "$(dirname "$DEST")" ] || SUDO=sudo
$SUDO mkdir -p "$DEST"
$SUDO tar -xJf "$TARBALL" -C "$DEST" --strip-components=1 --no-same-owner
# SELinux: Wine's libraries need the textrel_shlib_t label, which the fcontext rule
# '/opt/wine[^/]*/lib/wine(/.*)?' gives them (docs/PLAN.md); restorecon applies it.
# DEST must be on a filesystem mounted with exec.
if command -v restorecon >/dev/null && command -v selinuxenabled >/dev/null && selinuxenabled; then
    $SUDO restorecon -R "$DEST"
fi
"$DEST/bin/wine" --version
for so in "$DEST"/lib/wine/*-unix/ntdll.so; do
    missing="$(ldd "$so" 2>/dev/null | awk '/not found/{print $1}' | tr '\n' ' ')"
    [ -z "$missing" ] || echo "missing for $so: $missing (see the dnf line in this script)" >&2
done
