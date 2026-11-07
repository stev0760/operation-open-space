"""Command-line entry point: ``python -m isre <command>`` or ``isre <command>``."""
from __future__ import annotations

import argparse
import importlib
import importlib.util
import json
import re
import sys
from pathlib import Path
from typing import Sequence

from isre import __version__


# Subcommands implemented as isre/<name>.py modules with main(argv) -> int.
# Each is registered only when its module exists and is imported only when run,
# so a module can be added without editing this file.
PLUGINS = {
    "hlp": "WinHelp 3.1 extractor (isre.hlp)",
    "res": "NE resource dumper (isre.res)",
    "rebuild": "write an unpacked NE with standard relocation records (isre.rebuild)",
    "asm": "recursive-descent disassembler (isre.asm)",
    "pack": "analyst context pack for one address (isre.pack)",
    "ships": "MASTER.BOD / MASTER.SHP record dumper (isre.ships)",
}


def _plugin_available(name: str) -> bool:
    """True when isre/<name>.py exists and defines a top-level main(); not imported here."""
    try:
        spec = importlib.util.find_spec(f"isre.{name}")
    except (ImportError, ValueError):
        return False
    if spec is None or not spec.origin:
        return False
    try:
        src = Path(spec.origin).read_text(encoding="utf-8", errors="replace")
    except OSError:
        return False
    return re.search(r"^def main\(", src, re.M) is not None


def _run_plugin(name: str, argv: list[str]) -> int:
    """Delegate to isre.<name>.main(argv), imported only when asked for."""
    try:
        mod = importlib.import_module(f"isre.{name}")
    except ImportError as e:
        print(f"isre {name}: module isre.{name} is not available ({e})", file=sys.stderr)
        return 2
    entry = getattr(mod, "main", None)
    if not callable(entry):
        print(f"isre {name}: isre.{name} has no main(argv)", file=sys.stderr)
        return 2
    rc = entry(argv)
    return int(rc) if isinstance(rc, int) else 0


def cmd_szdd(a: argparse.Namespace) -> int:
    from isre import szdd

    data = Path(a.input).read_bytes()
    out = szdd.expand(data)
    Path(a.output).parent.mkdir(parents=True, exist_ok=True)
    Path(a.output).write_bytes(out)
    print(f"{a.input} -> {a.output}: {len(out)} bytes")
    return 0


def cmd_extract(a: argparse.Namespace) -> int:
    from isre import szdd

    files = szdd.extract_dir(Path(a.src), Path(a.out))
    for f in files:
        tag = "szdd" if f.szdd else "copy"
        mark = "" if f.written else "  (unchanged)"
        print(f"{tag}  {f.source:<16s} -> {f.target:<16s} {f.size:8d}{mark}")
    print(f"{len(files)} files, {sum(f.szdd for f in files)} expanded, "
          f"{sum(f.written for f in files)} written")
    return 0


def cmd_ne(a: argparse.Namespace) -> int:
    from isre.ne import NEModule
    from isre.util import json_bytes, write_if_changed

    mod = NEModule.load(a.file)
    if a.output:
        write_if_changed(Path(a.output), json_bytes(mod.to_json()))
        print(f"wrote {a.output}")
    elif a.json:
        print(json.dumps(mod.to_json(), indent=1))
    else:
        print(mod.summary())
    return 0


def _unpack(path: str):
    from isre import optloader
    from isre.ne import NEModule

    return optloader.unpack(NEModule.load(path))


def _report(result) -> None:
    for s in result.segments:
        if not s.packed:
            print(f"  seg{s.n:<3d} raw      {len(s.image):6d} bytes")
            continue
        state = "ok" if s.invariants_ok else "FAIL " + "; ".join(s.errors or ["invariant"])
        print(f"  seg{s.n:<3d} packed   {s.decomp_len:6d} -> {len(s.image):6d} bytes, "
              f"relocs {len(s.records):4d}/{s.w0:<4d} slack {s.slack}  {state}")
    print(f"  {len(result.records)} relocation records; invariants "
          f"{'ok' if result.ok else 'FAILED'}")


def cmd_unpack(a: argparse.Namespace) -> int:
    from isre import optloader

    result = _unpack(a.file)
    written = optloader.write_unpacked(result, Path(a.out))
    _report(result)
    print(f"  {len(written)} files written to {a.out}")
    return 0 if result.ok else 1


def cmd_relocs(a: argparse.Namespace) -> int:
    from isre import optloader

    result = _unpack(a.file)
    optloader.write_relocs(result, Path(a.output))
    _report(result)
    print(f"  wrote {a.output}")
    return 0 if result.ok else 1


def cmd_addr(a: argparse.Namespace) -> int:
    from isre import addr as A
    from isre.ne import NEModule

    module = NEModule.load(a.ne) if a.ne else None
    dgroup = a.dgroup or (module.autodata if module else A.INSPACE_DGROUP)
    try:
        ad = A.parse(a.address, dgroup_seg=dgroup, seg_context=a.seg, module=module)
        info = A.describe(ad, dgroup_seg=dgroup, module=module)
    except A.AddrError as e:
        print(f"isre addr: {e}", file=sys.stderr)
        return 2
    if a.json:
        print(json.dumps(info, indent=1))
        return 0
    for k, v in info.items():
        if v is not None:
            print(f"{k:<13s} {v}")
    return 0


def cmd_all(a: argparse.Namespace) -> int:
    from isre.pipeline import run_all

    res = run_all(Path(a.original), Path(a.work))
    return 0 if res.ok else 1


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="isre", description="Inner Space reverse-engineering tools.")
    p.add_argument("--version", action="version", version=f"isre {__version__}")
    sub = p.add_subparsers(dest="cmd", metavar="<command>")
    sub.required = True

    s = sub.add_parser("szdd", help="expand one SZDD-compressed file")
    s.add_argument("input")
    s.add_argument("output")
    s.set_defaults(func=cmd_szdd)

    s = sub.add_parser("extract", help="copy a distribution directory, expanding SZDD and restoring names")
    s.add_argument("src", help="game or theworks directory")
    s.add_argument("out", help="output directory")
    s.set_defaults(func=cmd_extract)

    s = sub.add_parser("ne", help="dump an NE module's header and tables")
    s.add_argument("file", help="NE file (SZDD-compressed input is expanded first)")
    s.add_argument("--json", action="store_true", help="print JSON instead of a summary")
    s.add_argument("-o", "--output", help="write the JSON to this file instead of stdout")
    s.set_defaults(func=cmd_ne)

    s = sub.add_parser("unpack", help="unpack an OPTLOADER module to segNN.bin + relocs.json")
    s.add_argument("file")
    s.add_argument("out", help="output directory")
    s.set_defaults(func=cmd_unpack)

    s = sub.add_parser("relocs", help="decode an OPTLOADER module's relocations to JSON")
    s.add_argument("file")
    s.add_argument("output", help="output .json")
    s.set_defaults(func=cmd_relocs)

    s = sub.add_parser("addr", help="convert an address between segN:0xOFF, Ghidra, DGROUP and com forms")
    s.add_argument("address", help="segN:0xOFF | SSSS:OFF | DGROUP:0xOFF | segN/com:0xOFF | file:0xOFF")
    s.add_argument("--ne", help="NE file: sets DGROUP to its autodata segment, enables file: offsets")
    s.add_argument("--seg", type=int, help="segment number for a bare com:0xOFF address")
    s.add_argument("--dgroup", type=int, help="DGROUP segment number (default: --ne autodata, else 19)")
    s.add_argument("--json", action="store_true")
    s.set_defaults(func=cmd_addr)

    s = sub.add_parser("all", help="regenerate the work directory: extracted, ne, unpacked, rebuilt, assets, asm")
    s.add_argument("--original", required=True, help="directory holding game/ and theworks/")
    s.add_argument("--work", required=True, help="output work directory")
    s.set_defaults(func=cmd_all)

    for name, desc in PLUGINS.items():
        if _plugin_available(name):
            sub.add_parser(name, help=f"{desc}; arguments pass through", add_help=False)
    return p


def main(argv: Sequence[str] | None = None) -> int:
    args = list(sys.argv[1:] if argv is None else argv)
    if args and args[0] in PLUGINS:
        return _run_plugin(args[0], args[1:])
    parser = build_parser()
    a = parser.parse_args(args)
    try:
        return a.func(a)
    except BrokenPipeError:
        sys.stderr.close()  # output piped into head(1) or similar
        return 0
    except (ValueError, OSError) as e:
        print(f"isre {a.cmd}: {e}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
