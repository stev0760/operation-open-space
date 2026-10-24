"""``isre all``: regenerate the work tree from the original game files.

Layout written under ``<work>``:

    extracted/shipdisk/   every shipdisk file, SZDD expanded, names restored
    extracted/theworks/   every THEWORKS file
    ne/<name>.json        ``isre ne --json`` output per OPTLOADER module
    unpacked/<name>/      segNN.bin, relocs.json, unpack.json per module
    rebuilt/<name>.<ext>  unpacked NE with standard relocation records
                          (``isre rebuild``), checked by the symbolic loader
    assets/<module>/      every NE module's resources (``isre res``)
    asm/<name>/           recursive-descent listings, functions.json, xrefs.json
                          (``isre asm``) for inspace, ispace, factory-1998a

Unchanged outputs are not rewritten, so a second run changes nothing.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable

from isre import asm, neload, optloader, rebuild, szdd, x86names
from isre import res as resdump
from isre.ne import NEModule
from isre.util import find_file, json_bytes, write_if_changed

# (output name, extracted subdirectory, installed file name)
MODULES = [
    ("inspace", "shipdisk", "INSPACE.EXE"),
    ("ispace", "shipdisk", "ISPACE.DLL"),
    ("factory-1998a", "shipdisk", "FACTORY.EXE"),
    ("factory-1998b", "theworks", "FACTORY.EXE"),
]
SOURCES = ["shipdisk", "theworks"]


@dataclass
class ModuleOutcome:
    name: str
    source: str
    ok: bool
    records: int
    failed_segments: list[int] = field(default_factory=list)
    rebuilt: str = ""                          # work-relative path of the rebuilt NE
    rebuild_errors: list[str] = field(default_factory=list)


@dataclass
class PipelineResult:
    extracted: dict[str, int] = field(default_factory=dict)
    written: int = 0
    modules: list[ModuleOutcome] = field(default_factory=list)
    assets_ok: bool = True

    @property
    def ok(self) -> bool:
        return self.assets_ok and all(m.ok for m in self.modules)


def run_all(original: Path, work: Path, log: Callable[[str], None] = print) -> PipelineResult:
    original, work = Path(original), Path(work)
    res = PipelineResult()
    for src in SOURCES:
        files = szdd.extract_dir(original / src, work / "extracted" / src)
        res.extracted[src] = len(files)
        n_written = sum(f.written for f in files)
        res.written += n_written
        n_szdd = sum(f.szdd for f in files)
        log(f"extract {src}: {len(files)} files ({n_szdd} SZDD expanded), {n_written} written")

    for name, src, filename in MODULES:
        path = find_file(work / "extracted" / src, filename)
        if path is None:
            raise FileNotFoundError(f"{filename} not found in {work / 'extracted' / src}")
        mod = NEModule.load(path)
        if write_if_changed(work / "ne" / f"{name}.json", json_bytes(mod.to_json())):
            res.written += 1
        up = optloader.unpack(mod)
        res.written += len(optloader.write_unpacked(up, work / "unpacked" / name))
        failed = [s.n for s in up.segments if not s.invariants_ok]
        outcome = ModuleOutcome(name, f"{src}/{path.name}", up.ok, len(up.records), failed)
        res.modules.append(outcome)
        status = "invariants ok" if up.ok else f"INVARIANTS FAIL in segments {failed}"
        log(f"unpack {name} ({src}/{path.name}): {len(up.segments)} segments, "
            f"{len(up.records)} relocation records, {status}")
        if up.ok:
            res.written += _rebuild(mod, up, name, Path(filename).suffix.lower(), work,
                                    outcome, log)
    assets = resdump.generate(work / "extracted", work / "assets", log)
    res.written += assets.written
    res.assets_ok = assets.ok

    specs = x86names.load_specs(x86names.default_spec_dir())
    for name in asm.DEFAULT_MODULES:
        out = asm.analyse(asm.load_work(work, name), specs)
        written, fj = asm.write_outputs(out, work / "asm" / name)
        res.written += len(written)
        log(f"asm {name}: {len(fj['functions'])} functions, {len(written)} written")
    log(f"{res.written} files written")
    return res


def _rebuild(mod: NEModule, up: optloader.UnpackResult, name: str, ext: str, work: Path,
             outcome: ModuleOutcome, log: Callable[[str], None]) -> int:
    """Write rebuilt/<name><ext> and check it against the OPTLOADER load."""
    rel = f"rebuilt/{name}{ext}"
    try:
        rb = rebuild.rebuild(mod, up)
        report = neload.check(NEModule(rb.data, f"{name}{ext}"), mod, up)
    except ValueError as e:
        outcome.rebuild_errors.append(str(e))
        outcome.ok = False
        log(f"rebuild {name}: FAILED: {e}")
        return 0
    outcome.rebuilt = rel
    if not report.ok:
        outcome.rebuild_errors.extend(report.errors + report.differences)
        outcome.ok = False
    written = int(write_if_changed(work / rel, rb.data))
    state = "loader-equivalent" if report.ok else "NOT loader-equivalent"
    log(f"rebuild {name} -> {rel}: {len(rb.data)} bytes, "
        f"{sum(len(s.records) for s in rb.segments)} standard records, {state}")
    return written
