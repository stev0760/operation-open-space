#!/usr/bin/env bash
# Ghidra headless pipeline for the rebuilt Inner Space NE modules.
#
#   GHIDRA_HOME=<ghidra install> JAVA_HOME=<jdk 21> re/ghidra/run.sh [inspace] [ispace]
#
# Inputs:  work/rebuilt/{inspace.exe,ispace.dll} and work/asm/<module>/functions.json
#          (from `isre all`), re/wine-specs/*.spec
# Outputs: work/ghidra/ (project InnerSpace, logs, <module>-{setup,finish,export}.json,
#          <module>-imports.tsv, inspace-join.tsv), work/decomp/<module>/ (C + index.tsv).
#          The export is written to work/decomp/.<module>.new and replaces
#          work/decomp/<module>/ only when every script succeeded.
# Environment: MAXMEM (JVM heap, default 3G), THREADS (decompiler threads, default 2),
#          MAXCPU (analysis cores, default 2), WORK (default <repo>/work)
set -euo pipefail

die() { echo "run.sh: $*" >&2; exit 2; }

[ -n "${GHIDRA_HOME:-}" ] || die "GHIDRA_HOME is not set (the Ghidra 12.1.x install directory)"
[ -n "${JAVA_HOME:-}" ] || die "JAVA_HOME is not set (a JDK 21 directory)"
[ -x "$GHIDRA_HOME/support/analyzeHeadless" ] || die "no support/analyzeHeadless under GHIDRA_HOME=$GHIDRA_HOME"
[ -x "$JAVA_HOME/bin/java" ] || die "no bin/java under JAVA_HOME=$JAVA_HOME"

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
WORK="${WORK:-$REPO/work}"
SPECS="$REPO/re/wine-specs"
PROJ="$WORK/ghidra"
export JAVA_HOME PATH="$JAVA_HOME/bin:$PATH"
export GHIDRA_HEADLESS_MAXMEM="${MAXMEM:-3G}"
THREADS="${THREADS:-2}"
MAXCPU="${MAXCPU:-2}"

modules=("$@")
[ ${#modules[@]} -gt 0 ] || modules=(inspace ispace)   # inspace first: ispace reads its join table

mkdir -p "$PROJ" "$WORK/decomp"

run_module() {
	local m="$1" file
	local -a finish=()
	case "$m" in
		inspace)
			file="$WORK/rebuilt/inspace.exe"
			finish=(rtl=seg5 rtl_start=seg5:0x1a
				join_dll="$WORK/rebuilt/ispace.dll" join_name=ISPACE join_loader=seg10:0x67c0) ;;
		ispace)
			file="$WORK/rebuilt/ispace.dll"
			finish=(join_tsv="$PROJ/inspace-join.tsv" join_importer=INSPACE.EXE) ;;
		*) die "unknown module '$m' (inspace, ispace)" ;;
	esac
	[ -f "$file" ] || die "$file is missing; run: re/.venv/bin/isre all --original original --work work"
	local gaps="$WORK/asm/$m/functions.json"
	[ -f "$gaps" ] || die "$gaps is missing; run: re/.venv/bin/isre all --original original --work work"
	local out="$WORK/decomp/$m" new="$WORK/decomp/.$m.new" old="$WORK/decomp/.$m.old"
	rm -rf "$new" "$old"
	rm -f "$PROJ/$m"-{setup,finish,export}.json "$PROJ/$m.log" "$PROJ/$m-script.log"
	local t0=$SECONDS
	"$GHIDRA_HOME/support/analyzeHeadless" "$PROJ" InnerSpace \
		-import "$file" -overwrite \
		-loader NeLoader -loader-ordinalLookup false \
		-max-cpu "$MAXCPU" \
		-scriptPath "$HERE" \
		-preScript IsSetup.java module="$m" ne="$file" specs="$SPECS" out="$PROJ" \
		-postScript IsFinish.java module="$m" out="$PROJ" gaps="$gaps" "${finish[@]}" \
		-postScript IsExport.java module="$m" dir="$new" report="$PROJ" \
			threads="$THREADS" \
		-log "$PROJ/$m.log" -scriptlog "$PROJ/$m-script.log" >/dev/null
	local dt=$((SECONDS - t0))
	if grep -E "(ERROR|Exception)" "$PROJ/$m-script.log" >/dev/null 2>&1; then
		grep -E -A3 "(ERROR|Exception)" "$PROJ/$m-script.log" | head -20 >&2
		die "$m: a script failed (see $PROJ/$m-script.log)"
	fi
	for r in setup finish export; do
		[ -f "$PROJ/$m-$r.json" ] || die "$m: $PROJ/$m-$r.json was not written (see $PROJ/$m.log)"
	done
	[ -f "$new/index.tsv" ] || die "$m: $new/index.tsv was not written (see $PROJ/$m.log)"
	if [ -d "$out" ]; then mv "$out" "$old"; fi
	mv "$new" "$out"
	rm -rf "$old"
	grep -E "Is(Setup|Finish|Export) " "$PROJ/$m-script.log" | sed 's/^.*> //; s/ (GhidraScript).*$//' || true
	echo "$m: ${dt}s"
}

for m in "${modules[@]}"; do
	run_module "$m"
done
