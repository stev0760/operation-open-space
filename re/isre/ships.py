"""``isre ships``: dump MASTER.BOD (ship bodies) or MASTER.SHP (ships).

Usage:

    isre ships MASTER.BOD            aligned table of every record, then the tail
    isre ships MASTER.SHP --tsv      one tab-separated row per record, with a header
    isre ships MASTER.SH1 --json     records and tail as JSON

Both files are arrays of fixed-size records with no header: 44 bytes per
body, 65 per ship. The record count is file length // record size, as every
reader in the game counts it; the bytes after the last whole record (the
writer's block padding) are the tail. Layout and field meanings:
docs/re/formats.md, "MASTER.BOD: ship bodies" and "MASTER.SHP: ships". Field
names match src/formats/ships.h; ``byte_XX`` and ``word_XX`` name fields
whose meaning is [OPEN] by their offset.

The kind comes from the installed extension (.BOD/.SHP, or the distribution
names .BO1/.SH1, which are SZDD-compressed and expanded first) unless
``--kind`` names it. Text fields are cp1252; TSV escapes ``\\t \\r \\n \\\\``
like strings.tsv. ``write()`` rebuilds a file byte for byte, including bytes
after a string's NUL (kept in each record's ``stale``).
"""
from __future__ import annotations

import argparse
import json
import os
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Union

from isre import szdd
from isre.res_formats import decode_cp1252

BODY_SIZE = 0x2C
SHIP_SIZE = 0x41

# (name, offset, kind, size): kind is "str" (fixed cp1252 field), "u8", "i8" or "u16".
BODY_FIELDS = [
    ("name", 0x00, "str", 21),
    ("word_15", 0x15, "u16", 2),
    ("word_17", 0x17, "u16", 2),
    ("byte_19", 0x19, "i8", 1),
    ("byte_1a", 0x1A, "i8", 1),
    ("strength_base", 0x1B, "i8", 1),
    ("byte_1c", 0x1C, "i8", 1),
    ("byte_1d", 0x1D, "i8", 1),
    ("byte_1e", 0x1E, "u8", 1),
    ("byte_1f", 0x1F, "u8", 1),
    ("flags", 0x20, "u8", 1),
    ("bitmap_id", 0x21, "u16", 2),
    ("bitmap_file", 0x23, "str", 9),
]

SHIP_FIELDS = [
    ("fuel_tank", 0x00, "u8", 1),
    ("thruster", 0x01, "u8", 1),
    ("armor", 0x02, "u8", 1),
    ("team", 0x03, "u8", 1),
    ("to_friends", 0x04, "u8", 1),
    ("to_enemies", 0x05, "u8", 1),
    ("weapon", 0x06, "u8", 1),
    ("byte_07", 0x07, "u8", 1),
    ("byte_08", 0x08, "u8", 1),
    ("flags", 0x09, "u8", 1),
    ("name", 0x0A, "str", 21),
    ("hit_sound", 0x1F, "u16", 2),
    ("body", 0x21, "u16", 2),
    ("author", 0x23, "str", 21),
    ("sound_file", 0x38, "str", 9),
]

KINDS = {"bod": (BODY_SIZE, BODY_FIELDS), "shp": (SHIP_SIZE, SHIP_FIELDS)}

_FMT = {"u8": "<B", "i8": "<b", "u16": "<H"}

Value = Union[int, bytes]


@dataclass
class Record:
    """One record: field name -> int, or bytes (cp1252, up to the NUL) for strings."""
    values: dict[str, Value]
    stale: bytes  # the record's bytes after each string's NUL; every other byte is 0


@dataclass
class ShipFile:
    kind: str
    records: list[Record] = field(default_factory=list)
    tail: bytes = b""


def _check_kind(kind: str) -> tuple[int, list]:
    if kind not in KINDS:
        raise ValueError(f"unknown kind {kind!r}; expected one of {sorted(KINDS)}")
    return KINDS[kind]


def parse_record(kind: str, raw: bytes) -> Record:
    size, fields = _check_kind(kind)
    if len(raw) < size:
        raise ValueError(f"{kind} record needs {size} bytes, got {len(raw)}")
    stale = bytearray(size)
    values: dict[str, Value] = {}
    for name, off, typ, n in fields:
        if typ == "str":
            f = raw[off:off + n]
            k = f.find(b"\0")
            if k < 0:
                k = n
            stale[off + k + 1:off + n] = f[k + 1:]
            values[name] = bytes(f[:k])
        else:
            (values[name],) = struct.unpack_from(_FMT[typ], raw, off)
    return Record(values, bytes(stale))


def write_record(kind: str, rec: Record) -> bytes:
    size, fields = _check_kind(kind)
    out = bytearray(rec.stale) if rec.stale else bytearray(size)
    if len(out) != size:
        raise ValueError(f"{kind} record stale bytes must be {size} long")
    for name, off, typ, n in fields:
        v = rec.values[name]
        if typ == "str":
            if not isinstance(v, (bytes, bytearray)):
                raise ValueError(f"{name}: expected bytes")
            if len(v) > n or b"\0" in v:
                raise ValueError(f"{name}: {v!r} does not fit {n} bytes")
            out[off:off + len(v)] = v
            if len(v) < n:
                out[off + len(v)] = 0
        else:
            struct.pack_into(_FMT[typ], out, off, v)
    return bytes(out)


def parse(kind: str, data: bytes) -> ShipFile:
    size, _ = _check_kind(kind)
    count = len(data) // size
    recs = [parse_record(kind, data[i * size:(i + 1) * size]) for i in range(count)]
    return ShipFile(kind, recs, bytes(data[count * size:]))


def write(f: ShipFile) -> bytes:
    return b"".join(write_record(f.kind, r) for r in f.records) + f.tail


def kind_for(path: str | Path) -> str | None:
    """'bod' or 'shp' from the installed (or distribution) file extension."""
    ext = szdd.restored_name(Path(path).name).rpartition(".")[2].upper()
    return {"BOD": "bod", "SHP": "shp"}.get(ext)


def load(path: str | Path, kind: str | None = None) -> ShipFile:
    """Read a MASTER.BOD/SHP file, expanding SZDD input first."""
    kind = kind or kind_for(path)
    if kind is None:
        raise ValueError(f"{path}: cannot tell MASTER.BOD from MASTER.SHP; give --kind")
    data = Path(path).read_bytes()
    if szdd.is_szdd(data):
        data = szdd.expand(data)
    return parse(kind, data)


# ---------------------------------------------------------------- output

def _text(v: Value) -> str:
    return decode_cp1252(v) if isinstance(v, (bytes, bytearray)) else str(v)


def _cell(name: str, v: Value) -> str:
    if name == "flags":
        return f"{v:#04x}"
    s = _text(v)
    return s.replace("\\", "\\\\").replace("\t", "\\t").replace("\r", "\\r").replace("\n", "\\n")


def columns(kind: str) -> list[str]:
    return ["index"] + [name for name, *_ in _check_kind(kind)[1]]


def tsv(f: ShipFile) -> str:
    names = columns(f.kind)[1:]
    lines = ["\t".join(columns(f.kind))]
    for i, r in enumerate(f.records):
        lines.append("\t".join([str(i)] + [_cell(n, r.values[n]) for n in names]))
    return "\n".join(lines) + "\n"


def table(f: ShipFile) -> str:
    names = columns(f.kind)[1:]
    rows = [columns(f.kind)] + [[str(i)] + [_cell(n, r.values[n]) for n in names]
                                 for i, r in enumerate(f.records)]
    widths = [max(len(row[c]) for row in rows) for c in range(len(rows[0]))]
    out = ["  ".join(cell.ljust(w) for cell, w in zip(row, widths)).rstrip() for row in rows]
    zero = " (all zero)" if not any(f.tail) else ""
    stale = sum(1 for r in f.records if any(r.stale))
    out.append(f"{len(f.records)} records, {len(f.tail)} tail bytes{zero}, "
               f"{stale} records with bytes after a string's NUL")
    return "\n".join(out) + "\n"


def to_json(f: ShipFile) -> dict[str, Any]:
    recs = []
    for i, r in enumerate(f.records):
        d: dict[str, Any] = {"index": i}
        d.update({n: _text(v) if isinstance(v, (bytes, bytearray)) else v
                  for n, v in r.values.items()})
        if any(r.stale):
            d["stale"] = r.stale.hex()
        recs.append(d)
    return {"kind": f.kind, "record_size": KINDS[f.kind][0], "records": recs,
            "tail": f.tail.hex()}


def main(argv: list[str]) -> int:
    p = argparse.ArgumentParser(prog="isre ships", description="Dump MASTER.BOD or MASTER.SHP.")
    p.add_argument("file", help="MASTER.BOD or MASTER.SHP (SZDD-compressed input is expanded)")
    p.add_argument("--kind", choices=sorted(KINDS), help="file kind when the name does not say")
    out = p.add_mutually_exclusive_group()
    out.add_argument("--tsv", action="store_true", help="tab-separated rows with a header")
    out.add_argument("--json", action="store_true", help="JSON with records and tail")
    a = p.parse_args(argv)
    try:
        f = load(a.file, a.kind)
        if a.tsv:
            sys.stdout.write(tsv(f))
        elif a.json:
            print(json.dumps(to_json(f), indent=1, ensure_ascii=False))
        else:
            sys.stdout.write(table(f))
        return 0
    except BrokenPipeError:
        # output piped into head(1) or similar; keep the exit-time flush quiet
        os.dup2(os.open(os.devnull, os.O_WRONLY), sys.stdout.fileno())
        return 0
    except (ValueError, OSError) as e:
        print(f"isre ships: {e}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
