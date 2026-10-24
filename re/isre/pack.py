"""``isre pack``: an analyst context pack (one markdown file) for one address.

Usage::

    isre pack ADDR [--module inspace|ispace|factory-1998a] [--work WORK] [-o FILE]

ADDR is canonical: ``segN:0xOFF`` for code, ``DGROUP:0xOFF`` for the
module's autodata segment.

For a code address the pack covers the function that holds it, as ``isre asm``
decoded it (shared tails, unreachable blocks and jump tables included):

* the function's entry, extent, size, seeds, callers (each with the function
  that holds the call) and callees (internal as ``segN:0xOFF``, imports by name);
* lines of ``docs/re/*.md`` and ``oracle/digests/*.md`` that mention an
  address inside the function or a DGROUP address it references;
* GetProcAddress lookups with an immediate ordinal, and LoadString string ids
  passed as immediates (directly, or through a helper whose id is a stack
  argument), with the text from ENG_LANG.DLL's string table;
* the DGROUP addresses it references, with the initialised bytes or string,
  or ``BSS`` at or above the initialised length;
* the annotated listing from ``WORK/asm``;
* the Ghidra C from ``WORK/decomp`` with Ghidra selector addresses converted
  to ``segN:0xOFF`` (``DGROUP:0xOFF`` for the autodata segment).

For a DGROUP address the pack shows the bytes, the string there, every code
reference (from ``xrefs.json``, with the function that holds it), the
references to nearby addresses, instructions whose immediate equals the
offset, and the notes that mention it.

A pack is at most ``--max-bytes`` (30,000) bytes. Long listings keep the
function head and a window around ADDR; long C keeps its start. Every cut is
marked in the pack. Inputs come from ``isre all`` and ``re/ghidra/run.sh``;
missing optional inputs (Ghidra C, string tables) are reported in the pack.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from bisect import bisect_right
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from isre import asm
from isre.addr import AddrError, parse
from isre.asm import FunctionIndex, Listing, ShowError, addr

MAX_BYTES = 30_000
REPO_ROOT = Path(__file__).resolve().parents[2]
NOTE_GLOBS = ("docs/re/*.md", "oracle/digests/*.md")

# Per-section caps (bytes); the listing and the C share what is left.
CAP_HEADER, CAP_NOTES, CAP_DYN, CAP_STRINGS, CAP_DGROUP = 3500, 3500, 3000, 3000, 4500

MODULE_ALIASES = {"INSPACE.EXE": "inspace", "INSPACE": "inspace", "ISPACE.DLL": "ispace",
                  "ISPACE": "ispace", "FACTORY.EXE": "factory-1998a", "FACTORY": "factory-1998a",
                  "THEWORKS/FACTORY.EXE": "factory-1998b"}

LOADSTRING = "USER.176 LoadString"
GETPROCADDRESS = "KERNEL.50 GetProcAddress"
LOADLIBRARY = "KERNEL.95 LoadLibrary"


class PackError(ValueError):
    pass


# ------------------------------------------------------------------ module data

class Module:
    """The generated outputs of one module under WORK."""

    def __init__(self, work: Path, name: str):
        self.work = Path(work)
        self.name = name
        self.asm_dir = self.work / "asm" / name
        try:
            self.fj = asm.load_functions(self.asm_dir)
        except ShowError as e:
            raise PackError(str(e)) from None
        self.file = self.fj["file"].upper()
        self.dgroup = self.fj["dgroup"]
        self.init_len = self.fj["dgroup_init_len"]
        self.index = FunctionIndex(self.fj)
        self.code_segs = {s["seg"] for s in self.fj["segments"]}
        self._xj: dict[str, Any] | None = None
        self._listings: dict[int, Listing] = {}
        self._dg: bytes | None = None
        self._ne: dict[str, Any] | None = None

    @property
    def xj(self) -> dict[str, Any]:
        if self._xj is None:
            path = self.asm_dir / "xrefs.json"
            if not path.is_file():
                raise PackError(f"{path} is missing; run isre all first")
            self._xj = json.loads(path.read_text())
        return self._xj

    @property
    def ne(self) -> dict[str, Any]:
        if self._ne is None:
            path = self.work / "ne" / f"{self.name}.json"
            self._ne = json.loads(path.read_text()) if path.is_file() else {}
        return self._ne

    @property
    def nsegs(self) -> int:
        return len(self.ne.get("segments", [])) or max(self.code_segs | {self.dgroup})

    @property
    def dg(self) -> bytes:
        if self._dg is None:
            path = self.work / "unpacked" / self.name / f"seg{self.dgroup:02d}.bin"
            self._dg = path.read_bytes() if path.is_file() else b""
        return self._dg

    def listing(self, seg: int) -> Listing:
        if seg not in self._listings:
            try:
                self._listings[seg] = asm.load_listing(self.asm_dir, self.fj, seg)
            except ShowError as e:
                raise PackError(str(e)) from None
        return self._listings[seg]

    def func_of(self, a: str) -> str | None:
        seg, off = asm._addr_key(a)
        f, _o = self.index.containing(seg, off)
        return f["addr"] if f else None


# ------------------------------------------------------------------ text helpers

def _clip(lines: list[str], cap: int, what: str) -> list[str]:
    """Lines up to ``cap`` bytes; a cut is marked with the number of lines left out."""
    out, used = [], 0
    for i, ln in enumerate(lines):
        if used + len(ln) + 1 > cap - 80:
            out.append(f"- [pack: {len(lines) - i} more {what} left out to keep the size cap]")
            return out
        out.append(ln)
        used += len(ln) + 1
    return out


def _size(lines: list[str]) -> int:
    return sum(len(ln.encode("utf-8")) + 1 for ln in lines)


def _short(items: list[str], n: int) -> str:
    return ", ".join(items[:n]) + (f" (+{len(items) - n} more)" if len(items) > n else "")


def dg_string(data: bytes, init_len: int, off: int) -> tuple[int, str] | None:
    """(start, text) of the printable NUL-terminated string holding ``off``."""
    if off >= min(init_len, len(data)):
        return None
    start = off
    while start > 0 and data[start - 1] in asm.PRINTABLE and off - start < 256:
        start -= 1
    end = data.find(b"\0", off, min(init_len, off + 512))
    if end < 0 or end - start < 2 or not all(c in asm.PRINTABLE for c in data[start:end]):
        return None
    return start, data[start:end].decode("ascii")


def dg_desc(mod: Module, off: int) -> str:
    """Initialised bytes and string at DGROUP:off, or BSS."""
    if off >= mod.init_len:
        return "BSS"
    if off >= len(mod.dg):
        return "outside DGROUP"
    raw = mod.dg[off:min(off + 8, mod.init_len)]
    txt = "bytes " + " ".join(f"{b:02x}" for b in raw)
    s = dg_string(mod.dg, mod.init_len, off)
    if s:
        start, text = s
        where = "" if start == off else f" (inside the string at DGROUP:{start:#x})"
        txt += f", string {json.dumps(text[off - start:][:80])}{where}"
    return txt


# ------------------------------------------------------------------ notes in docs

_TAG_RE = re.compile(r"\[(?:BYTES|ORACLE|REF|MANUAL)[^\[\]\n]*\]")
_TOKEN_RE = re.compile(
    r"(?P<mod>(?<![A-Za-z0-9_.])(?:[A-Za-z]+/)?[A-Z][A-Z0-9_]*\.(?:EXE|DLL|SCR|AD|exe|dll)"
    r"(?![A-Za-z0-9_])|(?<![A-Za-z0-9_.])(?:INSPACE|ISPACE|FACTORY)(?![A-Za-z0-9_.]))"
    r"|(?P<addr>(?<![A-Za-z0-9])(?:seg(?P<seg>\d+)|DGROUP):0x(?P<off>[0-9a-f]+)"
    r"(?P<tail>(?:\s*[-–]\s*0x[0-9a-f]+)|(?:/0x[0-9a-f]+)*))")


@dataclass(frozen=True)
class Mention:
    module: str | None
    seg: int | None            # None for DGROUP
    lo: int
    hi: int                    # inclusive


def mentions(line: str) -> list[Mention]:
    """Addresses a doc line mentions, each with the module it belongs to.

    Inside an evidence tag the module is the tag's own module name (INSPACE.EXE
    when it names none); in prose it is the nearest module name before the
    address on the line (INSPACE.EXE when there is none).
    """
    tags = [(m.start(), m.end()) for m in _TAG_RE.finditer(line)]
    prose_mod = "inspace"
    tag_mod: dict[int, str | None] = {}
    out = []
    for m in _TOKEN_RE.finditer(line):
        tag = next((i for i, (a, b) in enumerate(tags) if a <= m.start() < b), None)
        if m.group("mod"):
            mod = MODULE_ALIASES.get(m.group("mod").upper())
            if tag is None:
                prose_mod = mod
            else:
                tag_mod[tag] = mod
            continue
        module = prose_mod if tag is None else tag_mod.get(tag, "inspace")
        seg = int(m.group("seg")) if m.group("seg") else None
        lo = int(m.group("off"), 16)
        tail = m.group("tail") or ""
        rng = re.match(r"\s*[-–]\s*0x([0-9a-f]+)", tail)
        if rng:
            out.append(Mention(module, seg, lo, max(lo, int(rng.group(1), 16))))
            continue
        out.append(Mention(module, seg, lo, lo))
        for alt in re.findall(r"/0x([0-9a-f]+)", tail):
            out.append(Mention(module, seg, int(alt, 16), int(alt, 16)))
    return out


def doc_notes(module: str, roots: list[Path], code: list[tuple[int, int, int]],
              dgroup: set[int], dgroup_ranges: list[tuple[int, int]] = ()) -> list[str]:
    """Doc lines that mention an address in ``code`` ((seg, start, end) pieces) or
    in ``dgroup`` (offsets) / ``dgroup_ranges`` ([start, end) offsets)."""
    out = []
    for root in roots:
        for pattern in NOTE_GLOBS:
            for path in sorted(root.glob(pattern)):
                rel = path.relative_to(root).as_posix()
                for no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
                    if ":0x" not in line:
                        continue
                    hits = []
                    for mt in mentions(line):
                        if mt.module != module:
                            continue
                        if mt.seg is None:
                            if any(mt.lo <= d <= mt.hi for d in dgroup) or \
                                    any(mt.lo < b and a <= mt.hi for a, b in dgroup_ranges):
                                hits.append(f"DGROUP:{mt.lo:#x}")
                        elif any(mt.seg == s and mt.lo < b and a <= mt.hi for s, a, b in code):
                            hits.append(addr(mt.seg, mt.lo))
                    if hits:
                        text = line.strip()
                        text = text if len(text) <= 700 else text[:700] + " ..."
                        out.append(f"- `{rel}:{no}` ({', '.join(sorted(set(hits)))}): {text}")
    return out


# ------------------------------------------------------------------ listing scans

@dataclass
class Ins:
    off: int
    text: str
    comment: str


def _instructions(lines: list[tuple[str, int | None]]) -> list[Ins]:
    out = []
    for ln, off in lines:
        if off is None:
            continue
        p = asm.parse_line(ln)
        if p and p[2] and not p[3].startswith(("db ", "dw ")):
            out.append(Ins(p[1], p[3], p[4]))
    return out


_STOP = ("call", "jmp", "ret", "retf", "iret", "loop", "int ")


_DWORD = re.compile(r"push (?:dword ptr |e[abcd]x$|e[sd]i$|ebp$)")


def _pushes_before(ins: list[Ins], i: int, n: int) -> list[str] | None:
    """Operands of the ``n`` stack words pushed before ins[i], nearest first, or None.

    A 32-bit push fills two words (both listed as its operand). A ``push cs``
    right before a near call to a far function belongs to the call.
    """
    out: list[str] = []
    j = i - 1
    if ins[i].text.startswith("call seg") and j >= 0 and ins[j].text == "push cs":
        j -= 1
    while j >= 0 and len(out) < n and i - j <= 24:
        t = ins[j].text
        if t.startswith(_STOP):
            return None
        if t.startswith("push "):
            out += [t[5:]] * (2 if _DWORD.match(t) else 1)
        j -= 1
    return out[:n] if len(out) >= n else None


def _imm(op: str) -> int | None:
    m = re.fullmatch(r"0x[0-9a-f]+|\d+", op)
    return int(op, 0) if m else None


def _strings_table(work: Path) -> dict[int, str] | None:
    path = work / "assets" / "eng_lang.dll" / "strings.json"
    if not path.is_file():
        return None
    return {s["id"]: s["text"] for s in json.loads(path.read_text())}


def _string_line(at: str, how: str, sid: int, table: dict[int, str] | None) -> str:
    if table is None:
        text = "(ENG_LANG.DLL string table not generated)"
    elif sid in table:
        text = json.dumps(table[sid][:160])
    else:
        text = "(not in ENG_LANG.DLL's string table)"
    return f"- {at} {how}: id {sid} ({sid:#x}) {text}"


def _helper_arg(mod: Module, func: dict[str, Any]) -> int | None:
    """For a function that passes one of its own stack arguments as LoadString's
    id: how many pushes before a call to it that argument is (0 = the last)."""
    if LOADSTRING not in func["imports"]:
        return None
    lines = asm.function_lines(mod.listing(func["seg"]), mod.index, func)
    ins = _instructions(lines)
    base = 6 if "retf" in func["returns"] else 4       # ret IP (+ CS) and the saved BP
    for i, x in enumerate(ins):
        if x.text.endswith(LOADSTRING):
            p = _pushes_before(ins, i, 4)
            m = re.fullmatch(r"word ptr \[bp \+ (0x[0-9a-f]+|\d+)\]", p[3]) if p else None
            if m and (int(m.group(1), 0) - base) % 2 == 0 and int(m.group(1), 0) >= base:
                return (int(m.group(1), 0) - base) // 2
    return None


def string_ids(mod: Module, func: dict[str, Any], ins: list[Ins],
               table: dict[int, str] | None) -> list[str]:
    out = []
    helpers: dict[str, int | None] = {}
    for i, x in enumerate(ins):
        at = addr(func["seg"], x.off)
        if x.text.endswith(LOADSTRING):
            p = _pushes_before(ins, i, 5)
            sid = _imm(p[3]) if p else None
            if sid is not None:
                out.append(_string_line(at, f"LoadString(hInst {p[4]})", sid, table))
            continue
        m = re.fullmatch(r"call (?:far )?(seg\d+:0x[0-9a-f]+)", x.text)
        if not m or m.group(1) not in mod.index.by_addr:
            continue
        target = m.group(1)
        if target not in helpers:
            helpers[target] = _helper_arg(mod, mod.index.by_addr[target])
        k = helpers[target]
        if k is None:
            continue
        p = _pushes_before(ins, i, k + 1)
        sid = _imm(p[k]) if p else None
        if sid is not None:
            out.append(_string_line(at, f"via helper {target} (its stack word {k + 1} from "
                                        "the top is LoadString's id)", sid, table))
    return out


_DLL_STR = re.compile(r'-> DGROUP:0x[0-9a-f]+ "([^"]*\.(?:DLL|dll))"')
_STORE_AX = re.compile(r"mov word ptr \[(0x[0-9a-f]+)\], ax$")
_STORE_DX = re.compile(r"mov word ptr \[(0x[0-9a-f]+)\], dx$")


def _entry_table(work: Path, dll: str) -> tuple[str, dict[int, tuple[int, int, str]]] | None:
    """(module file, ordinal -> (seg, offset, name)) of the work module named ``dll``."""
    stem = dll.upper().rsplit(".", 1)[0].lstrip("_")
    for path in sorted((work / "ne").glob("*.json")):
        ne = json.loads(path.read_text())
        if ne.get("module_name", "").upper() != stem:
            continue
        names = {n["ordinal"]: n["name"] for n in ne["resident_names"] + ne["nonresident_names"]
                 if n["ordinal"]}
        entries = {e["ordinal"]: (e["seg"], e["offset"], names.get(e["ordinal"], ""))
                   for e in ne["entries"] if not e["constant"]}
        return ne["file"].upper(), entries
    return None


def dynamic_imports(mod: Module, func: dict[str, Any], ins: list[Ins]) -> list[str]:
    """GetProcAddress lookups with a constant ordinal or name."""
    seg = func["seg"]
    out = []
    last_dll: str | None = None
    last_lib: tuple[str, str | None] | None = None          # (LoadLibrary site, dll guess)
    handles: dict[str, tuple[str, str | None]] = {}       # DGROUP word -> LoadLibrary
    tables: dict[str, Any] = {}
    libs_seen: list[str] = []
    for i, x in enumerate(ins):
        m = _DLL_STR.search(x.comment)
        if m:
            last_dll = m.group(1)
        if x.text.endswith(LOADLIBRARY):
            last_lib = (addr(seg, x.off), last_dll)
            nxt = ins[i + 1] if i + 1 < len(ins) else None
            sm = _STORE_AX.match(nxt.text) if nxt else None
            if sm:
                handles[sm.group(1)] = last_lib
            continue
        if not x.text.endswith(GETPROCADDRESS):
            continue
        p = _pushes_before(ins, i, 3)
        if p is None:
            continue
        off_op, seg_op, handle = p
        if seg_op == "0" and _imm(off_op) is not None:
            what, ordinal = f"ordinal {_imm(off_op)}", _imm(off_op)
        elif seg_op == "ds" and _imm(off_op) is not None:
            s = dg_string(mod.dg, mod.init_len, _imm(off_op))
            what, ordinal = f"name {json.dumps(s[1]) if s else hex(_imm(off_op))}", None
        else:
            continue
        lib = last_lib if handle == "ax" else None
        hm = re.fullmatch(r"word ptr \[(0x[0-9a-f]+)\]", handle)
        if hm:
            lib = handles.get(hm.group(1))
        hdesc = f"handle of LoadLibrary {lib[0]}" if lib else f"handle {handle}"
        if lib and lib[0] not in libs_seen:
            libs_seen.append(lib[0])
            out.append(f"- LoadLibrary {lib[0]}: "
                       + (f"its name ends in {json.dumps(lib[1])} (the nearest .DLL string pushed "
                          "before the call; a guess)" if lib[1] else "module name not found"))
        line = f"- {addr(seg, x.off)} {hdesc}, {what}"
        stores = [ins[j].text for j in range(i + 1, min(i + 3, len(ins)))]
        sa = _STORE_AX.match(stores[0]) if stores else None
        if sa:
            sd = _STORE_DX.match(stores[1]) if len(stores) > 1 else None
            line += (f" -> far pointer at DGROUP:{int(sa.group(1), 16):#x}"
                     if sd and int(sd.group(1), 16) == int(sa.group(1), 16) + 2
                     else f" -> AX stored at DGROUP:{int(sa.group(1), 16):#x}")
        if ordinal is not None and lib and lib[1]:
            if lib[1] not in tables:
                tables[lib[1]] = _entry_table(mod.work, lib[1])
            et = tables[lib[1]]
            if et and ordinal in et[1]:
                es, eo, name = et[1][ordinal]
                line += f" = {et[0]} {addr(es, eo)}" + (f" {name}" if name else "")
            elif et:
                line += f"; {et[0]} has no entry {ordinal}"
        out.append(line)
    return out


def dgroup_refs(mod: Module, func: dict[str, Any], ins: list[Ins],
                pieces: list[tuple[int, int, str]]) -> dict[int, list[tuple[str, str]]]:
    """DGROUP offset -> [(from, how)] for the references the function makes."""
    seg = func["seg"]
    starts = [a for a, _b, _k in pieces]

    def inside(a: str) -> bool:
        s, o = asm._addr_key(a)
        i = bisect_right(starts, o) - 1
        return s == seg and i >= 0 and o < pieces[i][1]

    refs: dict[int, set[tuple[str, str]]] = {}
    for key, lst in mod.xj["dgroup"].items():
        off = int(key.split(":")[1], 16)
        for frm, how in lst:
            if inside(frm):
                refs.setdefault(off, set()).add((frm, how))
    for i in range(1, len(ins)):
        if ins[i - 1].text == "push ds":
            v = _imm(ins[i].text[5:]) if ins[i].text.startswith("push ") else None
            if v is not None and v >= 0x10:
                refs.setdefault(v, set()).add((addr(seg, ins[i].off), "ds:imm"))
    return {k: sorted(v, key=lambda t: (asm._addr_key(t[0]), t[1]))
            for k, v in sorted(refs.items())}


# ------------------------------------------------------------------ Ghidra C

_G_COLON = re.compile(r"(?<![0-9A-Za-z])(1[0-9a-f]{3}):([0-9a-f]{4})(?![0-9a-f])")
_G_UNDER = re.compile(r"(?<=_)(1[0-9a-f]{3})_([0-9a-f]{4})(?![0-9A-Za-z])")
_G_LINEAR = re.compile(r"(code_r|ram,|at |)0x(1[0-9a-f]{3})([0-9a-f]{4})(?![0-9a-f])")
_G_HEADER = re.compile(r"\s*\(Ghidra [0-9a-f]{4}:[0-9a-f]{4}\)")


def ghidra_converter(nsegs: int, dgroup: int):
    """A function that rewrites Ghidra selector addresses in C text."""
    def name(sel_hex: str) -> str | None:
        sel = int(sel_hex, 16)
        n = (sel - 0x1000) // 8 + 1
        if sel < 0x1000 or (sel - 0x1000) % 8 or n > nsegs:
            return None
        return "DGROUP" if n == dgroup else f"seg{n}"

    def colon(m: re.Match) -> str:
        nm = name(m.group(1))
        return f"{nm}:{int(m.group(2), 16):#x}" if nm else m.group(0)

    def under(m: re.Match) -> str:
        nm = name(m.group(1))
        return f"{nm}_{int(m.group(2), 16):#x}" if nm else m.group(0)

    def linear(m: re.Match) -> str:
        nm = name(m.group(2))
        if not nm:
            return m.group(0)
        off = int(m.group(3), 16)
        if m.group(1) == "code_r":
            return f"code_r_{nm}_{off:#x}"
        if m.group(1):
            return f"{m.group(1)}{nm}:{off:#x}"
        return f"{m.group(0)} /*{nm}:{off:#x}*/"

    def convert(text: str) -> str:
        text = _G_HEADER.sub("", text)
        text = _G_LINEAR.sub(linear, text)
        text = _G_COLON.sub(colon, text)
        return _G_UNDER.sub(under, text)
    return convert


def ghidra_c(mod: Module, func: dict[str, Any], pieces: list[tuple[int, int, str]]
             ) -> tuple[list[str], str, str]:
    """(C lines, note, file names) for the Ghidra functions of this function."""
    ddir = mod.work / "decomp" / mod.name
    index = ddir / "index.tsv"
    if not index.is_file():
        return [], (f"No Ghidra output for {mod.name} (decomp/{mod.name}/index.tsv is "
                    "absent)."), ""
    rows = {}
    for ln in index.read_text().splitlines()[1:]:
        cols = ln.split("\t")
        if len(cols) >= 8:
            rows[cols[0]] = cols
    picked = []
    if func["addr"] in rows:
        picked = [func["addr"]]
    else:
        seg = func["seg"]
        for a in sorted(rows, key=asm._addr_key):
            s, o = asm._addr_key(a)
            if s == seg and any(x <= o < y for x, y, _k in pieces):
                picked.append(a)
    if not picked:
        return [], (f"Ghidra has no function that starts inside {func['addr']} "
                    f"(decomp/{mod.name}/index.tsv)."), ""
    convert = ghidra_converter(mod.nsegs, mod.dgroup)
    out = []
    for a in picked:
        cols = rows[a]
        path = ddir / cols[7]
        if not path.is_file():
            out.append(f"// {a}: {cols[7]} is listed in index.tsv but missing")
            continue
        out.extend(convert(path.read_text()).rstrip("\n").splitlines())
        out.append("")
    files = ", ".join(f"decomp/{mod.name}/{rows[a][7]}" for a in picked)
    note = (f"From {files}; Ghidra selectors 0x1000+(N-1)*8 are rewritten as segN "
            f"(selector {0x1000 + (mod.dgroup - 1) * 8:#x} is DGROUP, seg{mod.dgroup}).")
    if picked != [func["addr"]]:
        note += f" Ghidra splits {func['addr']} differently: functions {', '.join(picked)}."
    return (out[:-1] if out and out[-1] == "" else out), note, files


# ------------------------------------------------------------------ fitting

def fit_listing(lines: list[tuple[str, int | None]], budget: int, target: int, hint: str,
                seg: int) -> list[str]:
    """The listing cut to ``budget`` bytes: header, function head, window around target."""
    sizes = [len(t.encode("utf-8")) + 1 for t, _o in lines]
    if sum(sizes) <= budget:
        return [t for t, _o in lines]
    n = len(lines)
    h = 0
    while h < n and lines[h][1] is None:
        h += 1
    t = h
    for i in range(h, n):
        o = lines[i][1]
        if o is not None and o <= target:
            t = i
    room = budget - 400                               # room for the two cut notes
    keep = set(range(h))
    used = sum(sizes[:h])
    if t > h:                                         # the entry, up to a quarter of the room
        quarter = used + max(0, room - used) // 4
        i = h
        while i < t and used + sizes[i] <= quarter:
            keep.add(i)
            used += sizes[i]
            i += 1
    lo, hi = t, t
    grew = True
    while grew:
        grew = False
        for _ in range(2):                            # two lines after for each line before
            if hi < n and used + sizes[hi] <= room:
                if hi not in keep:
                    keep.add(hi)
                    used += sizes[hi]
                hi += 1
                grew = True
        if lo > 0 and lo - 1 not in keep and used + sizes[lo - 1] <= room:
            lo -= 1
            keep.add(lo)
            used += sizes[lo]
            grew = True
    out: list[str] = []
    gap: list[int] = []

    def flush() -> None:
        offs = [lines[g][1] for g in gap if lines[g][1] is not None]
        span = f" ({addr(seg, offs[0])}..{addr(seg, offs[-1])})" if offs else ""
        out.append(f"; [pack: {len(gap)} lines{span} left out to keep the size cap; "
                   f"full listing: {hint}]")
        gap.clear()

    for i in range(n):
        if i in keep:
            if gap:
                flush()
            out.append(lines[i][0])
        else:
            gap.append(i)
    if gap:
        flush()
    return out


def fit_c(lines: list[str], budget: int, path_note: str) -> list[str]:
    if _size(lines) <= budget:
        return lines
    out, used = [], 0
    for i, ln in enumerate(lines):
        if used + len(ln.encode("utf-8")) + 1 > budget - 200:
            out.append(f"// [pack: {len(lines) - i} more lines left out to keep the size cap; "
                       f"full text: {path_note}]")
            break
        out.append(ln)
        used += len(ln.encode("utf-8")) + 1
    return out


# ------------------------------------------------------------------ packs

def function_pack(mod: Module, seg: int, off: int, roots: list[Path], cap: int) -> str:
    listing = mod.listing(seg)
    func, others = mod.index.containing(seg, off)
    req = addr(seg, off)
    if func is None:
        pos = listing.line_of(off) or 0
        window = listing.lines[max(0, pos - 30):pos + 30]
        notes = doc_notes(mod.name, roots, [(seg, off, off + 1)], set())
        out = [f"# {mod.file} {req}", "",
               f"No function that `isre asm` decoded holds {req} (padding, data or undecoded "
               "bytes). The listing around it:", "", "```asm", *window, "```", ""]
        if notes:
            out += ["## Notes in docs", "", *_clip(notes, CAP_NOTES, "lines"), ""]
        return _cap("\n".join(out), cap)
    fa = func["addr"]
    pieces = mod.index.pieces(fa)
    lines = asm.function_lines(listing, mod.index, func, others)
    ins = _instructions(lines)

    # header
    unre = [u for u in mod.fj["unreachable_blocks"] if u["in_function"] == fa]
    sw = [s for s in mod.fj["switches"]
          if any(x <= asm._addr_key(s["at"])[1] < y for x, y, k in pieces if k != "table")
          and asm._addr_key(s["at"])[0] == seg]
    ranges = [f"{a}..{addr(seg, asm._addr_key(b)[1] - 1)}" for a, b in func["ranges"]]
    callers = [f"{f} {k} (in {mod.func_of(f) or '?'})" for f, k in func["callers"]]
    internal = [f"{t} {k}" for t, k in func["calls"]]
    where = (f"{req} is the entry of function {fa}" if off == func["off"]
             else f"{req} is inside function {fa}")
    hdr = [f"# {mod.file} {fa}", "",
           f"Context pack for {mod.file} {req}: {where}. Addresses are {mod.file} "
           f"`segN:0xOFF`; DGROUP is seg{mod.dgroup}. Generated by `isre pack` from the work "
           f"directory (paths below are relative to it).", "",
           f"## Function {fa}", "",
           f"- spans {addr(seg, min(a for a, _b, _k in pieces))}.."
           f"{addr(seg, max(b for _a, b, _k in pieces) - 1)}; "
           f"{func['insns']} instructions, {func['bytes']} bytes"
           + (f" in {len(ranges)} ranges: {_short(ranges, 8)}" if len(ranges) > 1 else ""),
           f"- seeds (how `isre asm` found it): {', '.join(func['seeds'])}",
           f"- returns: {'/'.join(func['returns']) or 'none'}"]
    if func["export"]:
        hdr.append(f"- export: {', '.join(func['export'])}")
    sharing = sorted({o for a, b, _k in pieces for o in mod.index.overlapping(seg, a, b)} - {fa},
                     key=asm._addr_key)
    if sharing:
        hdr.append(f"- shares code with: {', '.join(sharing)}")
    if unre:
        hdr.append("- unreachable blocks: " + ", ".join(
            f"{u['addr']} ({u['insns']} insns)" for u in unre))
    if sw:
        hdr.append("- switches: " + ", ".join(
            f"{s['at']} ({s['count']} entries, table {s['table']})" for s in sw))
    if func["tail_jumps"]:
        hdr.append(f"- continues into: {', '.join(func['tail_jumps'])}")
    hdr.append(f"- callers ({len(callers)}): {_short(callers, 30) or 'none found'}")
    hdr.append(f"- calls internal ({len(internal)}): {_short(internal, 40) or 'none'}")
    hdr.append(f"- calls imports ({len(func['imports'])}): "
               f"{_short(func['imports'], 40) or 'none'}")
    hdr = _clip(hdr, CAP_HEADER, "header lines")

    # references
    refs = dgroup_refs(mod, func, ins, pieces)
    code = [(seg, a, b) for a, b, _k in pieces]
    notes = doc_notes(mod.name, roots, code, set(refs))
    dyn = dynamic_imports(mod, func, ins)
    table = _strings_table(mod.work)
    sids = string_ids(mod, func, ins, table)
    dg_lines = []
    for k, lst in refs.items():
        hows: dict[str, list[str]] = {}
        for f, h in lst:
            hows.setdefault(f, []).append(h)
        froms = _short([f"{f} {'/'.join(h)}" for f, h in hows.items()], 4)
        dg_lines.append(f"- DGROUP:{k:#x} {dg_desc(mod, k)}: {froms}")

    small: list[str] = []
    small += ["", "## Notes in docs", ""]
    small += _clip(notes, CAP_NOTES, "lines") if notes else \
        ["No line of docs/re/*.md or oracle/digests/*.md mentions an address of this function."]
    if dyn:
        small += ["", "## GetProcAddress lookups", "", *_clip(dyn, CAP_DYN, "lookups")]
    if sids:
        small += ["", "## String ids (ENG_LANG.DLL string table)", "",
                  "The hInst operand is shown as pushed; the texts come from ENG_LANG.DLL.", "",
                  *_clip(sids, CAP_STRINGS, "ids")]
    small += ["", f"## DGROUP references ({len(dg_lines)})", ""]
    if dg_lines:
        small += [f"Initialised data ends at DGROUP:{mod.init_len:#x}; BSS lies above. How: mem "
                  "(absolute operand), lea, indexed (displacement of a [reg+disp] operand), imm "
                  "(immediate pointing at a string), ds:imm (push ds; push imm far pointer).", ""]
        small += _clip(dg_lines, CAP_DGROUP, "references")
    else:
        small.append("None.")

    # listing and C share the rest
    c_lines, c_note, c_files = ghidra_c(mod, func, pieces)
    hint = f"isre asm --work WORK --module {mod.name} --show {fa}"
    lst_head = ["", "## Listing", "",
                f"From asm/{mod.name}/seg{seg:02d}.asm: every piece of the function in "
                "address order; `; ....` lines name what lies between pieces.", "", "```asm"]
    c_head = ["", "## Ghidra C", "", c_note]
    fixed = _size(hdr) + _size(small) + _size(lst_head) + _size(c_head) + 64
    rem = max(2000, cap - fixed)
    full_l = sum(len(t.encode("utf-8")) + 1 for t, _o in lines)
    full_c = _size(c_lines) + (16 if c_lines else 0)
    if full_l + full_c <= rem:
        lb, cb = full_l, full_c
    elif full_l <= rem * 0.6:
        lb, cb = full_l, rem - full_l
    elif full_c <= rem * 0.4:
        lb, cb = rem - full_c, full_c
    else:
        lb, cb = int(rem * 0.6), rem - int(rem * 0.6)
    body = fit_listing(lines, lb, off, hint, seg)
    out = hdr + small + lst_head + body + ["```"] + c_head
    if c_lines:
        out += ["", "```c", *fit_c(c_lines, cb - 16, c_files), "```"]
    return _cap("\n".join(out) + "\n", cap)


def _cap(text: str, cap: int) -> str:
    """Hard cut at a line boundary when a pack still exceeds ``cap`` bytes."""
    if len(text.encode("utf-8")) <= cap:
        return text
    cut = text.encode("utf-8")[:cap - 120].decode("utf-8", "ignore")
    cut = cut[:cut.rfind("\n") + 1]
    if cut.count("```") % 2:
        cut += "```\n"
    return cut + "\n[pack: cut here to keep the size cap]\n"


def dgroup_pack(mod: Module, off: int, roots: list[Path], cap: int) -> str:
    req = f"DGROUP:{off:#x}"
    if not mod.dg:
        raise PackError(f"DGROUP bytes are missing (unpacked/{mod.name}/seg{mod.dgroup:02d}.bin)")
    if off >= max(len(mod.dg), mod.init_len):
        raise PackError(f"{req} is outside DGROUP (seg{mod.dgroup}, {len(mod.dg):#x} bytes)")
    init = off < mod.init_len
    out = [f"# {mod.file} {req}", "",
           f"Context pack for {mod.file} {req}. DGROUP is seg{mod.dgroup}; its initialised data "
           f"ends at DGROUP:{mod.init_len:#x}, so this address is "
           + ("initialised." if init else "BSS (zero at load, written at run time).")
           + " Generated by `isre pack` from the work directory.", ""]
    s = dg_string(mod.dg, mod.init_len, off)
    lo = off
    hi = off + 16
    if s:
        lo, hi = s[0], s[0] + len(s[1]) + 1
    if init:
        out += ["## Bytes", "", "```"]
        start = off
        for row in range(start, min(start + 64, mod.init_len), 16):
            chunk = mod.dg[row:min(row + 16, mod.init_len)]
            txt = "".join(chr(c) if 0x20 <= c < 0x7F else "." for c in chunk)
            out.append(f"DGROUP:{row:#06x}  {' '.join(f'{b:02x}' for b in chunk):<47s}  |{txt}|")
        out += ["```", ""]
        out += ["## String", ""]
        if s:
            where = "" if s[0] == off else f" ({req} is inside it, at +{off - s[0]:#x})"
            out.append(f"The NUL-terminated string at DGROUP:{s[0]:#x}{where}, "
                       f"{len(s[1])} characters:")
            out += ["", "```", s[1], "```", ""]
        else:
            out += ["No printable NUL-terminated string at this address.", ""]
    exact, near = [], []
    for key, lst in mod.xj["dgroup"].items():
        k = int(key.split(":")[1], 16)
        if k == off:
            exact += [(f, h, k) for f, h in lst]
        elif lo - 32 <= k < hi + 32:
            near += [(f, h, k) for f, h in lst]
    out += [f"## Code references ({len(exact)})", ""]
    rows = [f"- {f} {h}, in function {mod.func_of(f) or '(none)'}"
            for f, h, _k in sorted(exact, key=lambda t: (asm._addr_key(t[0]), t[1]))]
    out += _clip(rows, 6000, "references") if rows else \
        ["None in xrefs.json (which records absolute memory operands, indexed operands with a "
         "displacement of 0x100 or more, and immediates that point at a string)."]
    out += ["", f"## References to nearby addresses (DGROUP:{max(0, lo - 32):#x}.."
                f"DGROUP:{hi + 31:#x})", ""]
    rows = [f"- DGROUP:{k:#x} {f} {h}, in function {mod.func_of(f) or '(none)'}"
            for f, h, k in sorted(near, key=lambda t: (t[2], asm._addr_key(t[0]), t[1]))]
    out += _clip(rows, 5000, "references") if rows else ["None."]
    # immediates with the same value anywhere in the code
    seen = {f for f, _h, _k in exact}
    pat = re.compile(rf"(?:push |, ){off:#x}(?![0-9a-f])")
    imms = []
    for seg in sorted(mod.code_segs):
        path = mod.asm_dir / f"seg{seg:02d}.asm"
        if not path.is_file():
            continue
        text = path.read_text()
        for m in pat.finditer(text):
            a = text.rfind("\n", 0, m.start()) + 1
            b = text.find("\n", m.end())
            ln = text[a:b if b >= 0 else len(text)]
            p = asm.parse_line(ln)
            if not p or not p[2] or pat.search(p[3]) is None:
                continue
            at = addr(seg, p[1])
            if at in seen or "ptr [" in p[3].split(",")[-1]:
                continue
            imms.append(f"- {at} `{p[3]}`, in function {mod.func_of(at) or '(none)'}")
    out += ["", f"## Instructions with the immediate {off:#x} ({len(imms)})", "",
            "The value matches; it may be a constant rather than a pointer.", ""]
    out += _clip(imms, 5000, "instructions") if imms else ["None."]
    notes = doc_notes(mod.name, roots, [], {off}, [(lo, hi)])
    out += ["", "## Notes in docs", ""]
    out += _clip(notes, CAP_NOTES, "lines") if notes else \
        ["No line of docs/re/*.md or oracle/digests/*.md mentions this address."]
    return _cap("\n".join(out) + "\n", cap)


def build_pack(work: Path, module: str, where: str, roots: list[Path] | None = None,
               cap: int = MAX_BYTES) -> str:
    """The markdown pack for ``where`` in ``module``."""
    mod = Module(Path(work), module)
    try:
        a = parse(where, dgroup_seg=mod.dgroup)
    except AddrError as e:
        raise PackError(str(e)) from None
    roots = [REPO_ROOT] if roots is None else roots
    if a.seg == mod.dgroup:
        return dgroup_pack(mod, a.off, roots, cap)
    if a.seg not in mod.code_segs:
        mod.listing(a.seg)                       # raises with the list of code segments
    return function_pack(mod, a.seg, a.off, roots, cap)


def main(argv) -> int:
    p = argparse.ArgumentParser(prog="isre pack", description=__doc__.split("\n")[0])
    p.add_argument("addr", help="segN:0xOFF or DGROUP:0xOFF")
    p.add_argument("--module", default="inspace",
                   help="module under WORK/asm (inspace, ispace, factory-1998a)")
    p.add_argument("--work", default="work", help="work directory written by isre all")
    p.add_argument("-o", "--output", help="write the pack here instead of stdout")
    p.add_argument("--notes-root", action="append",
                   help="directory holding docs/re and oracle/digests (default: the repository)")
    p.add_argument("--max-bytes", type=int, default=MAX_BYTES, help="size cap (default 30000)")
    a = p.parse_args(list(argv))
    try:
        text = build_pack(Path(a.work), a.module, a.addr,
                          [Path(r) for r in a.notes_root] if a.notes_root else None, a.max_bytes)
    except (PackError, OSError) as e:
        print(f"isre pack: {e}", file=sys.stderr)
        return 2
    if a.output:
        Path(a.output).parent.mkdir(parents=True, exist_ok=True)
        Path(a.output).write_text(text, encoding="utf-8")
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
