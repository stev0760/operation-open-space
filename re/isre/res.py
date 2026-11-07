"""``isre res``: dump the resources of Win16 NE modules.

Usage:

    isre res FILE                      list FILE's resources and what each decodes to
    isre res FILE -o DIR               write FILE's resources to DIR
    isre res --extracted E --out O     every NE module in E/game and E/theworks
                                       to O/<module>/ (``generate()``, for ``isre all``)

Layout of one module directory:

    index.json          every resource: type, id, size, file offset, output, facts
    strings.tsv/.json   STRINGTABLE, one row per string (tabs/newlines escaped in .tsv)
    dialogs.json/.rc    DIALOG templates
    menus.json/.rc      MENU templates
    accelerators.json/.rc
    version.json        VERSION (VS_VERSION_INFO)
    <TYPE>/<id>.<ext>   BITMAP .bmp, ICON/GROUP_ICON .ico, CURSOR/GROUP_CURSOR .cur;
                        RCDATA and custom types sniffed to .wav/.bmp/.txt, else .bin

Module directory names are the lower-cased file name; modules from theworks
get a ``theworks-`` prefix. Unchanged files are not rewritten. A file the
previous run listed in index.json and this run no longer produces is removed;
nothing else in the directory is touched.
"""
from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable

from isre import res_formats as F
from isre import res_rc as RC
from isre.ne import NEModule, Resource, is_ne
from isre.util import sha256, write_if_changed

SOURCES = ["game", "theworks"]

RT_CURSOR, RT_BITMAP, RT_ICON, RT_MENU, RT_DIALOG, RT_STRING = 1, 2, 3, 4, 5, 6
RT_ACCELERATOR, RT_GROUP_CURSOR, RT_GROUP_ICON, RT_VERSION = 9, 12, 14, 16


def jbytes(obj: Any) -> bytes:
    return (json.dumps(obj, indent=1, ensure_ascii=False) + "\n").encode("utf-8")


def module_key(source: str, filename: str) -> str:
    key = filename.lower()
    return key if source == "game" else f"{source}-{key}"


def _safe(s: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.-]", "_", s) or "_"


def _id_hex(rid: int | str) -> str | None:
    return f"{rid:#06x}" if isinstance(rid, int) else None


@dataclass
class ModuleDump:
    key: str
    source: str
    module_name: str
    index: dict[str, Any]
    outputs: dict[str, bytes]
    errors: list[str] = field(default_factory=list)
    written: int = 0
    removed: int = 0

    @property
    def counts(self) -> dict[str, int]:
        return self.index["counts"]

    @property
    def kinds(self) -> dict[str, int]:
        return self.index["kinds"]


@dataclass
class ResResult:
    modules: list[ModuleDump] = field(default_factory=list)

    @property
    def ok(self) -> bool:
        return not any(m.errors for m in self.modules)

    @property
    def written(self) -> int:
        return sum(m.written for m in self.modules)


class _Builder:
    def __init__(self, mod: NEModule):
        self.mod = mod
        self.outputs: dict[str, bytes] = {}
        self.errors: list[str] = []
        self.align = 1 << mod.res_align_shift if mod.res_align_shift else 1
        self.images = {RT_ICON: {}, RT_CURSOR: {}}  # type: dict[int, dict[Any, bytes]]

    def data(self, r: Resource) -> bytes:
        end = r.file_off + r.length
        if end > len(self.mod.data):
            raise ValueError(f"resource {r.type_name}/{r.id} ends at {end:#x}, past the file end")
        return self.mod.data[r.file_off:end]

    def path(self, r: Resource, ext: str) -> str:
        tdir = _safe(r.type_name if isinstance(r.type_id, int) else str(r.type_id))
        base = f"{tdir}/{_safe(str(r.id))}"
        p = base + ext
        n = 2
        while p in self.outputs:
            p = f"{base}_{n}{ext}"
            n += 1
        return p


def build_module(mod: NEModule, key: str = "", source: str = "") -> ModuleDump:
    """Decode every resource of ``mod`` into output files held in memory."""
    b = _Builder(mod)
    strings: list[tuple[int, str]] = []
    dialogs: list[dict[str, Any]] = []
    menus: list[dict[str, Any]] = []
    accels: list[dict[str, Any]] = []
    versions: list[dict[str, Any]] = []
    entries: list[dict[str, Any]] = []
    counts: dict[str, int] = {}
    kinds: dict[str, int] = {}

    for r in mod.resources:
        if r.type_id in (RT_ICON, RT_CURSOR):
            try:
                b.images[r.type_id][r.id] = b.data(r)
            except ValueError:
                pass

    for r in mod.resources:
        counts[r.type_name] = counts.get(r.type_name, 0) + 1
        e: dict[str, Any] = {
            "type": r.type_name, "type_id": r.type_id, "id": r.id, "id_hex": _id_hex(r.id),
            "size": r.length, "file_offset": r.file_off, "flags": f"{r.flags:#06x}",
        }
        try:
            data = b.data(r)
            kind, out, info = _decode(b, r, data, strings, dialogs, menus, accels, versions)
        except (ValueError, struct.error, IndexError) as ex:
            msg = f"{r.type_name}/{r.id}: {ex}"
            b.errors.append(msg)
            data = mod.data[r.file_off:r.file_off + r.length]
            out = b.path(r, ".bin")
            b.outputs[out] = data
            kind, info = "undecoded", {"error": str(ex), "hex_preview": data[:32].hex(" ")}
        e["kind"] = kind
        e["output"] = out
        e.update(info)
        kinds[kind] = kinds.get(kind, 0) + 1
        entries.append(e)

    if strings:
        strings.sort()
        tsv = ["id\ttext"] + [f"{sid}\t{F.escape_tsv(t)}" for sid, t in strings]
        b.outputs["strings.tsv"] = ("\n".join(tsv) + "\n").encode("utf-8")
        b.outputs["strings.json"] = jbytes([{"id": sid, "text": t} for sid, t in strings])
    if dialogs:
        b.outputs["dialogs.json"] = jbytes(dialogs)
        b.outputs["dialogs.rc"] = "\n".join(RC.dialog_rc(d["id"], d) for d in dialogs).encode("utf-8")
    if menus:
        b.outputs["menus.json"] = jbytes(menus)
        b.outputs["menus.rc"] = "\n".join(RC.menu_rc(m["id"], m) for m in menus).encode("utf-8")
    if accels:
        b.outputs["accelerators.json"] = jbytes(accels)
        b.outputs["accelerators.rc"] = "\n".join(
            RC.accelerators_rc(a["id"], a["entries"]) for a in accels).encode("utf-8")
    if versions:
        b.outputs["version.json"] = jbytes(versions)

    index = {
        "module": key,
        "source": source,
        "module_name": mod.module_name,
        "description": mod.description,
        "resource_align_shift": mod.res_align_shift,
        "counts": counts,
        "kinds": kinds,
        "strings": len(strings),
        "errors": b.errors,
        "resources": entries,
        "files": sorted(set(b.outputs) | {"index.json"}),
    }
    b.outputs["index.json"] = jbytes(index)
    return ModuleDump(key, source, mod.module_name, index, b.outputs, b.errors)


def _decode(b: _Builder, r: Resource, data: bytes, strings, dialogs, menus, accels, versions):
    """Decode one resource; returns (kind, output path, index facts)."""
    t = r.type_id
    if t == RT_STRING:
        block = F.decode_string_block(r.id, data)
        strings.extend(block)
        info: dict[str, Any] = {"strings": len(block)}
        if block:
            info["first_id"], info["last_id"] = block[0][0], block[-1][0]
        return "strings", "strings.tsv", info
    if t == RT_DIALOG:
        d = F.parse_dialog(data)
        dialogs.append({"id": r.id, "id_hex": _id_hex(r.id), **d})
        return "dialog", "dialogs.json", {"caption": d["caption"], "controls": len(d["controls"])}
    if t == RT_MENU:
        m = F.parse_menu(data)
        menus.append({"id": r.id, "id_hex": _id_hex(r.id), **m})
        return "menu", "menus.json", {"items": len(m["items"])}
    if t == RT_ACCELERATOR:
        a = F.parse_accelerators(data)
        accels.append({"id": r.id, "id_hex": _id_hex(r.id), "entries": a})
        return "accelerators", "accelerators.json", {"entries": len(a)}
    if t == RT_VERSION:
        v = F.parse_version(data)
        versions.append({"id": r.id, **v})
        fv = (v["fixed"] or {}).get("file_version")
        return "version", "version.json", {"file_version": fv}
    if t == RT_BITMAP:
        bmp, info = F.dib_to_bmp(data)
        p = b.path(r, ".bmp")
        b.outputs[p] = bmp
        return "dib", p, {"data_size": len(bmp), **F._bmp_summary(info)}
    if t == RT_ICON:
        ico, info = F.single_icon(data)
        p = b.path(r, ".ico")
        b.outputs[p] = ico
        return "icon", p, {"data_size": len(ico), **info}
    if t == RT_CURSOR:
        cur, info = F.single_cursor(data)
        p = b.path(r, ".cur")
        b.outputs[p] = cur
        return "cursor", p, {"data_size": len(cur), **info}
    if t in (RT_GROUP_ICON, RT_GROUP_CURSOR):
        g = F.parse_group(data)
        lookup = b.images[RT_ICON if g["kind"] == "icon" else RT_CURSOR]
        blob = F.build_group_file(g, lookup.get)
        ext = ".ico" if g["kind"] == "icon" else ".cur"
        p = b.path(r, ext)
        b.outputs[p] = blob
        return "group_" + g["kind"], p, {"data_size": len(blob), "members": g["entries"]}
    kind, ext, payload, info = F.sniff(data, b.align)
    p = b.path(r, ext)
    b.outputs[p] = payload
    return kind, p, {"data_size": len(payload), **info}


def _previous_files(out_dir: Path) -> list[str]:
    """Files the last run listed in ``out_dir``/index.json (relative, inside out_dir only)."""
    try:
        old = json.loads((out_dir / "index.json").read_text(encoding="utf-8"))
        files = old.get("files", [])
    except (OSError, ValueError, AttributeError):
        return []
    out = []
    for rel in files:
        if isinstance(rel, str) and rel and not rel.startswith(("/", "\\")) and ".." not in rel.split("/"):
            out.append(rel)
    return out


def write_module(dump: ModuleDump, out_dir: Path) -> ModuleDump:
    """Write a module's outputs under ``out_dir``.

    Files that the previous run's index.json listed and this run no longer
    produces are removed; nothing else in ``out_dir`` is touched.
    """
    out_dir = Path(out_dir)
    stale = [rel for rel in _previous_files(out_dir) if rel not in dump.outputs]
    for rel, blob in sorted(dump.outputs.items()):
        if write_if_changed(out_dir / rel, blob):
            dump.written += 1
    for rel in stale:
        p = out_dir / rel
        if p.is_file():
            p.unlink()
            dump.removed += 1
            parent = p.parent
            while parent != out_dir and parent.is_dir() and not any(parent.iterdir()):
                parent.rmdir()
                parent = parent.parent
    return dump


def ne_modules(extracted_dir: Path) -> list[tuple[str, str, Path]]:
    """(module key, source-relative name, path) for every NE file under game/ and theworks/."""
    out = []
    for src in SOURCES:
        d = Path(extracted_dir) / src
        if not d.is_dir():
            continue
        for p in sorted(d.iterdir()):
            if not p.is_file():
                continue
            with p.open("rb") as fh:
                head = fh.read(0x40)
                if head[:2] != b"MZ":
                    continue
                fh.seek(0)
                if not is_ne(fh.read()):
                    continue
            out.append((module_key(src, p.name), f"{src}/{p.name}", p))
    return out


LOOSE_SUFFIXES = (".wav", ".dib", ".bmp")


def content_key(blob: bytes) -> str:
    """Identity of an asset's content: the samples for a WAV, else the whole file."""
    if blob[:4] == b"RIFF" and blob[8:12] == b"WAVE":
        try:
            return "pcm:" + sha256(F.wav_samples(blob))
        except (ValueError, struct.error):
            pass
    return "file:" + sha256(blob)


def loose_assets(extracted_dir: Path) -> dict[str, list[str]]:
    """content_key -> ["theworks/POP.WAV", ...] for the loose WAV/DIB/BMP files."""
    out: dict[str, list[str]] = {}
    for src in SOURCES:
        d = Path(extracted_dir) / src
        if not d.is_dir():
            continue
        for p in sorted(d.iterdir()):
            if p.is_file() and p.suffix.lower() in LOOSE_SUFFIXES:
                out.setdefault(content_key(p.read_bytes()), []).append(f"{src}/{p.name}")
    return out


def annotate_same_as(dump: ModuleDump, loose: dict[str, list[str]]) -> int:
    """Add ``same_as`` to index entries whose output matches a loose file; returns the count."""
    n = 0
    for e in dump.index["resources"]:
        if e["kind"] not in ("wav", "bmp-file", "dib"):
            continue
        names = loose.get(content_key(dump.outputs[e["output"]]))
        if names:
            e["same_as"] = names
            n += 1
    dump.outputs["index.json"] = jbytes(dump.index)
    return n


def generate(extracted_dir: Path, out_dir: Path, log: Callable[[str], None] = print) -> ResResult:
    """Dump every NE module in ``extracted_dir``/{game,theworks} to ``out_dir``/<module>/.

    Entries whose WAV samples or bitmap file equal a loose file of the
    distribution get ``same_as`` naming that file.
    """
    out_dir = Path(out_dir)
    res = ResResult()
    summary = []
    loose = loose_assets(Path(extracted_dir))
    for key, source, path in ne_modules(Path(extracted_dir)):
        dump = build_module(NEModule.load(path), key, source)
        annotate_same_as(dump, loose)
        write_module(dump, out_dir / key)
        res.modules.append(dump)
        summary.append({"module": key, "source": source, "module_name": dump.module_name,
                        "counts": dump.counts, "kinds": dump.kinds,
                        "strings": dump.index["strings"], "errors": dump.errors})
        state = f"{len(dump.errors)} ERRORS" if dump.errors else "ok"
        log(f"res {key}: {sum(dump.counts.values())} resources, {dump.index['strings']} strings, "
            f"{dump.written} written, {dump.removed} removed, {state}")
        for err in dump.errors:
            log(f"  error: {err}")
    extra = int(write_if_changed(out_dir / "modules.json", jbytes(summary)))
    log(f"res: {len(res.modules)} modules, {res.written + extra} files written")
    return res


def _listing(dump: ModuleDump) -> str:
    idx = dump.index
    lines = [f"{dump.source}: module {idx['module_name']!r}, align shift {idx['resource_align_shift']}, "
             f"{len(idx['resources'])} resources, {idx['strings']} strings"]
    for e in idx["resources"]:
        lines.append(f"  {e['type']:<12s} {str(e['id']):<12s} off {e['file_offset']:#08x} "
                     f"len {e['size']:6d}  {e['kind']:<12s} {e['output']}")
    for k, v in idx["counts"].items():
        lines.append(f"  count {k}: {v}")
    for err in dump.errors:
        lines.append(f"  error: {err}")
    return "\n".join(lines)


def main(argv: list[str]) -> int:
    p = argparse.ArgumentParser(prog="isre res", description="Dump Win16 NE resources.")
    p.add_argument("files", nargs="*", help="NE files (SZDD-compressed input is expanded first)")
    p.add_argument("-o", "--out", help="output directory (one file: the module directory itself)")
    p.add_argument("--extracted", help="extracted tree holding game/ and theworks/")
    p.add_argument("--json", action="store_true", help="print index.json instead of a listing")
    a = p.parse_args(argv)
    try:
        if a.extracted:
            if not a.out or a.files:
                p.error("--extracted needs --out and no FILE arguments")
            return 0 if generate(Path(a.extracted), Path(a.out)).ok else 1
        if not a.files:
            p.error("give FILE arguments or --extracted")
        rc = 0
        for f in a.files:
            mod = NEModule.load(f)
            name = Path(f).name
            dump = build_module(mod, name.lower(), name)
            if a.out:
                target = Path(a.out) if len(a.files) == 1 else Path(a.out) / name.lower()
                write_module(dump, target)
                print(f"{name}: {len(dump.index['resources'])} resources -> {target} "
                      f"({dump.written} written, {dump.removed} removed)")
            elif a.json:
                print(json.dumps(dump.index, indent=1, ensure_ascii=False))
            else:
                print(_listing(dump))
            if dump.errors:
                rc = 1
        return rc
    except BrokenPipeError:
        sys.stderr.close()
        return 0
    except (ValueError, OSError) as e:
        print(f"isre res: {e}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
