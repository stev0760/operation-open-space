"""Symbolic NE loader: what a module's segments hold after relocation.

Two loaders build the same kind of image, one list entry per segment byte
up to the segment's minalloc (the autodata heap and stack that both loaders
add above minalloc are zero and not modelled):

* ``load_standard`` applies an NE file's standard per-segment relocation
  records the way Windows does [REF Microsoft KB Q65122 "Executable-File
  Header Format", PER SEGMENT DATA] and Wine 11.0 does
  [REF Wine dlls/krnl386.exe16/ne_segment.c apply_relocations]. Segment
  bytes are the file data, zero-filled up to minalloc (Wine allocates with
  GMEM_ZEROINIT).
* ``load_optloader`` applies the OPTLOADER records of a packed module to its
  unpacked images the way ``re/optlink-src/nappload.asm`` PROCESS_RELOCS
  does, using the chains of ``optloader.SegmentResult.chains``. The stub
  segment, which Windows loads itself, gets the input's standard records.

Unpatched bytes stay ints. A patched byte becomes ``(word, i)``: byte ``i``
(0 low, 1 high) of a symbolic word. Words are

    ('IMP', module, ordinal, 'off' | 'sel')   import by ordinal
    ('IMPN', module, name, 'off' | 'sel')     import by name
    ('SEG', n, 'sel')                         selector of segment n
    ('+', word, base)                         additive record over base != 0

and an internal target's offset is a plain int. Equal images mean every
patched site gets the same value and every other byte is the same.

Rules the two loaders differ on are reported as errors instead of modelled:
a site patched twice, a chain that leaves the segment, and an additive
selector over a nonzero word (OPTLOADER adds it, Wine stores it).
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Union

from isre import optloader
from isre.ne import NEModule, StdReloc

Word = Union[int, tuple]
Image = dict[int, list]


@dataclass
class LoadResult:
    images: Image
    sites: dict[int, set[int]]                    # patched byte offsets per segment
    errors: list[str] = field(default_factory=list)
    skipped: list[str] = field(default_factory=list)   # OSFIXUP / osfloat records

    @property
    def patched_bytes(self) -> int:
        return sum(len(v) for v in self.sites.values())


# ------------------------------------------------------------------ helpers

class _Seg:
    """One segment image being relocated."""

    def __init__(self, n: int, data: bytes, size: int, res: LoadResult):
        self.n = n
        self.img: list = list(data[:size]) + [0] * max(0, size - len(data))
        self.res = res
        self.sites = res.sites.setdefault(n, set())

    def err(self, msg: str) -> None:
        self.res.errors.append(f"seg{self.n}: {msg}")

    def fits(self, pos: int, width: int) -> bool:
        if pos < 0 or pos + width > len(self.img):
            self.err(f"site {pos:#x} (+{width}) lies outside the {len(self.img)}-byte segment")
            return False
        return True

    def clean(self, pos: int, width: int) -> bool:
        if any(p in self.sites for p in range(pos, pos + width)):
            self.err(f"site {pos:#x} is patched twice")
            return False
        return True

    def word(self, pos: int) -> int:
        lo, hi = self.img[pos], self.img[pos + 1]
        if not (isinstance(lo, int) and isinstance(hi, int)):
            self.err(f"word at {pos:#x} was already patched when read")
            return 0xFFFF
        return lo | (hi << 8)

    def put_word(self, pos: int, w: Word) -> None:
        if isinstance(w, int):
            self.img[pos], self.img[pos + 1] = w & 0xFF, (w >> 8) & 0xFF
        else:
            self.img[pos], self.img[pos + 1] = (w, 0), (w, 1)
        self.sites.update((pos, pos + 1))

    def put_byte(self, pos: int, w: Word) -> None:
        self.img[pos] = (w & 0xFF) if isinstance(w, int) else (w, 0)
        self.sites.add(pos)


def add(w: Word, base: int) -> Word:
    """``w`` added to the word ``base`` already at the site."""
    if isinstance(w, int):
        return (w + base) & 0xFFFF
    return w if base == 0 else ("+", w, base)


def _entry_target(mod: NEModule, ordinal: int) -> tuple[Word, Word]:
    for e in mod.entries:
        if e.ordinal == ordinal:
            if e.seg == 0xFE:
                return 0, e.offset           # constant entry
            return ("SEG", e.seg, "sel"), e.offset
    raise KeyError(f"entry ordinal {ordinal} is not in the entry table")


def _imp(mod: NEModule, module_idx: int, ordinal: int) -> tuple[Word, Word]:
    name = mod.modules[module_idx - 1] if 1 <= module_idx <= len(mod.modules) else f"?{module_idx}"
    return ("IMP", name, ordinal, "sel"), ("IMP", name, ordinal, "off")


def _impn(mod: NEModule, module_idx: int, name: str) -> tuple[Word, Word]:
    m = mod.modules[module_idx - 1] if 1 <= module_idx <= len(mod.modules) else f"?{module_idx}"
    return ("IMPN", m, name, "sel"), ("IMPN", m, name, "off")


# ------------------------------------------------------------------ standard loader

STD_WIDTH = {0: 1, 2: 2, 3: 4, 5: 2}


def _std_target(mod: NEModule, r: StdReloc) -> tuple[Word, Word] | None:
    tt = r.flags & 3
    t = r.target
    if tt == 0:
        if t["target_seg"] == "movable":
            return _entry_target(mod, t["entry_ordinal"])
        return ("SEG", t["target_seg"], "sel"), t["target_offset"]
    if tt == 1:
        return _imp(mod, t["module_idx"], t["ordinal"])
    if tt == 2:
        return _impn(mod, t["module_idx"], t["name"])
    return None                               # OSFIXUP: Wine ignores it


def apply_standard(seg: _Seg, mod: NEModule, recs: list[StdReloc]) -> None:
    for r in recs:
        target = _std_target(mod, r)
        if target is None:
            seg.res.skipped.append(f"seg{seg.n}:{r.offset:#x} OSFIXUP {r.target}")
            continue
        sel, off = target
        width = STD_WIDTH.get(r.src_type)
        if width is None:
            seg.err(f"record at {r.offset:#x}: source type {r.src_type} is not modelled")
            continue
        pos = r.offset
        if r.flags & 4:                       # additive: one site
            if not (seg.fits(pos, width) and seg.clean(pos, width)):
                continue
            if r.src_type == 0:
                seg.put_byte(pos, add(off, seg.img[pos]))
            elif r.src_type == 5:
                seg.put_word(pos, add(off, seg.word(pos)))
            elif r.src_type == 3:
                if seg.word(pos + 2):
                    seg.err(f"additive FAR_ADDR at {pos:#x} over selector word "
                            f"{seg.word(pos + 2):#06x}")
                    continue
                seg.put_word(pos, add(off, seg.word(pos)))
                seg.put_word(pos + 2, sel)
            else:
                if seg.word(pos):
                    seg.err(f"additive SEGMENT at {pos:#x} over {seg.word(pos):#06x}")
                    continue
                seg.put_word(pos, sel)
            continue
        steps = 0
        while True:                           # non-additive: 0xFFFF-terminated chain
            if not (seg.fits(pos, max(width, 2)) and seg.clean(pos, width)):
                break
            link = seg.word(pos)
            if r.src_type == 0:
                seg.put_byte(pos, off)
            elif r.src_type == 2:
                seg.put_word(pos, sel)
            elif r.src_type == 3:
                seg.put_word(pos, off)
                seg.put_word(pos + 2, sel)
            else:
                seg.put_word(pos, off)
            steps += 1
            if link == 0xFFFF:
                break
            if link == pos or steps > len(seg.img):
                seg.err(f"chain from {r.offset:#x} does not end")
                break
            pos = link


def load_standard(mod: NEModule) -> LoadResult:
    """Relocate every segment of a non-self-loading NE file."""
    if mod.self_loading:
        raise ValueError(f"{mod.name}: self-loading module; use load_optloader")
    res = LoadResult({}, {})
    for s in mod.segments:
        seg = _Seg(s.n, mod.segment_file_bytes(s.n), s.minalloc, res)
        if s.has_file_data and len(mod.segment_file_bytes(s.n)) > s.minalloc:
            seg.err(f"file data ({s.file_len}) exceeds minalloc ({s.minalloc})")
        apply_standard(seg, mod, mod.std_relocs.get(s.n, []))
        res.images[s.n] = seg.img
    return res


# ------------------------------------------------------------------ OPTLOADER

def _opt_target(mod: NEModule, rec: dict[str, Any]) -> tuple[Word, Word]:
    kind = rec["kind"]
    if kind == "import_ordinal":
        return _imp(mod, rec["module_idx"], rec["ordinal"])
    if kind == "import_name":
        return _impn(mod, rec["module_idx"], rec["name"])
    if kind == "internal":
        if rec["ref_seg_raw"] == 0xFF:        # RELOC_SRC_INT_MOVABLE: GETENTRY(self, ordinal)
            return _entry_target(mod, rec["value"])
        return ("SEG", rec["ref_seg_raw"], "sel"), rec["value"]
    if kind == "special_base":                # selector of the segment, SEG16 chain
        return ("SEG", rec["selector_idx"], "sel"), 0
    raise ValueError(kind)


def apply_optloader(seg: _Seg, mod: NEModule, s: optloader.SegmentResult) -> None:
    chains, errs = s.chains()
    for e in errs:
        seg.err(e)
    for rec, sites in zip(s.records, chains):
        if rec["kind"] == "osfloat":          # OSTYPE_ABORT with a coprocessor present
            seg.res.skipped.append(f"seg{seg.n}:{rec['offset']:#x} osfloat {rec['n']}")
            continue
        sel, off = _opt_target(mod, rec)
        tt = "SEG16" if rec["kind"] == "special_base" else rec["target_type"]
        for pos in sites:
            width = 4 if tt.startswith("PTR16") else 1 if tt.startswith("BYTE") else 2
            if not (seg.fits(pos, width) and seg.clean(pos, width)):
                continue
            if tt == "BYTE":
                seg.put_byte(pos, off)
            elif tt == "BYTE_A":
                seg.put_byte(pos, add(off, seg.img[pos]))
            elif tt == "SEG16":
                seg.put_word(pos, sel)
            elif tt == "SEG16_A":
                seg.put_word(pos, add(sel, seg.word(pos)))
            elif tt == "OFF16":
                seg.put_word(pos, off)
            elif tt == "OFF16_A":
                seg.put_word(pos, add(off, seg.word(pos)))
            elif tt == "PTR16":
                seg.put_word(pos, off)
                seg.put_word(pos + 2, sel)
            else:                             # PTR16_A
                base_off, base_sel = seg.word(pos), seg.word(pos + 2)
                seg.put_word(pos, add(off, base_off))
                seg.put_word(pos + 2, add(sel, base_sel))


def load_optloader(mod: NEModule, up: optloader.UnpackResult | None = None) -> LoadResult:
    """Relocate an OPTLOADER module's unpacked segments as OPTLOADER does."""
    if up is None:
        up = optloader.unpack(mod)
    res = LoadResult({}, {})
    for s in up.segments:
        seg = _Seg(s.n, s.image, s.minalloc, res)
        if s.packed:
            apply_optloader(seg, mod, s)
        else:                                 # stub: loaded by Windows from its own records
            apply_standard(seg, mod, mod.std_relocs.get(s.n, []))
        res.images[s.n] = seg.img
    return res


# ------------------------------------------------------------------ comparison

@dataclass
class CheckReport:
    module: str
    segments: int
    patched_bytes: int
    compared_bytes: int
    differences: list[str]
    errors: list[str]
    skipped: list[str]

    @property
    def ok(self) -> bool:
        return not self.differences and not self.errors

    def summary(self) -> str:
        state = "EQUIVALENT" if self.ok else "DIFFERENT"
        lines = [f"  loader check {self.module}: {state}: {self.segments} segments, "
                 f"{self.compared_bytes} bytes compared, {self.patched_bytes} patched"]
        lines += [f"    error: {e}" for e in self.errors[:20]]
        lines += [f"    diff:  {d}" for d in self.differences[:20]]
        if self.skipped:
            lines.append(f"    {len(self.skipped)} OS fixup records skipped by both loaders")
        return "\n".join(lines)


def compare(a: LoadResult, b: LoadResult, limit: int = 50) -> list[str]:
    diffs: list[str] = []
    for n in sorted(set(a.images) | set(b.images)):
        ia, ib = a.images.get(n), b.images.get(n)
        if ia is None or ib is None:
            diffs.append(f"seg{n} present in only one image")
            continue
        if len(ia) != len(ib):
            diffs.append(f"seg{n}: {len(ia)} bytes vs {len(ib)} bytes")
        if ia == ib:
            continue
        for i, (x, y) in enumerate(zip(ia, ib)):
            if x != y:
                diffs.append(f"seg{n}:{i:#06x}: {x!r} != {y!r}")
                if len(diffs) >= limit:
                    return diffs
    return diffs


def check(rebuilt: NEModule, original: NEModule,
          up: optloader.UnpackResult | None = None) -> CheckReport:
    """Compare a rebuilt module, loaded the standard way, with the OPTLOADER load."""
    std = load_standard(rebuilt)
    opt = load_optloader(original, up)
    diffs = compare(std, opt)
    if std.sites != opt.sites:
        diffs.append("patched-site sets differ")
    errors = [f"standard: {e}" for e in std.errors] + [f"optloader: {e}" for e in opt.errors]
    return CheckReport(
        module=original.name, segments=len(std.images), patched_bytes=std.patched_bytes,
        compared_bytes=sum(len(v) for v in std.images.values()), differences=diffs,
        errors=errors, skipped=std.skipped + opt.skipped)
