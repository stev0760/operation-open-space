#!/usr/bin/env bash
# Builds the read-only memory probe: oracle/probe/isprobe.c -> OUTDIR/isprobe.exe
# (32-bit Win32 console program, static, for the Wine prefix that runs the game).
# Usage: oracle/probe/build.sh [OUTDIR]     OUTDIR defaults to work/probe
# Environment: CC (default i686-w64-mingw32-gcc).
set -eu
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
OUT="${1:-$REPO/work/probe}"
CC="${CC:-i686-w64-mingw32-gcc}"
mkdir -p "$OUT"
"$CC" -static -O2 -std=c99 -Wall -Wextra -o "$OUT/isprobe.exe" "$HERE/isprobe.c"
echo "$OUT/isprobe.exe"
