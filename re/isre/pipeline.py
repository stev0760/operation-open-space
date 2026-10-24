"""``isre all``: regenerate the work tree from the original game files.

Layout written under ``<work>``:

    extracted/shipdisk/   every shipdisk file, SZDD expanded, names restored
    extracted/theworks/   every THEWORKS file
    ne/<name>.json        ``isre ne --json`` output per OPTLOADER module
    unpacked/<name>/      segNN.bin, relocs.json, unpack.json per module

Unchanged outputs are not rewritten, so a second run changes nothing.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable

from isre import optloader, szdd
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


@dataclass
class PipelineResult:
    extracted: dict[str, int] = field(default_factory=dict)
    written: int = 0
    modules: list[ModuleOutcome] = field(default_factory=list)

    @property
    def ok(self) -> bool:
        return all(m.ok for m in self.modules)


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
        res.modules.append(ModuleOutcome(name, f"{src}/{path.name}", up.ok, len(up.records), failed))
        status = "invariants ok" if up.ok else f"INVARIANTS FAIL in segments {failed}"
        log(f"unpack {name} ({src}/{path.name}): {len(up.segments)} segments, "
            f"{len(up.records)} relocation records, {status}")
    log(f"{res.written} files written")
    return res
