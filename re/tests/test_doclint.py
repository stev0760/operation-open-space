"""Doc linter: the rules of CLAUDE.md that a machine can check.

Scope: ``docs/**/*.md``, ``oracle/digests/*.md`` and ``README.md``, plus the
feature checklist (``docs/spec/checklist.md``, or the draft
``work/drafts/checklist.md`` while no installed checklist exists).

Rules:
- banned words (case-insensitive): superseded, withdrawn, previously, now known;
- evidence tags are well formed: ``[MANUAL <topic>]``, ``[BYTES <text>]``,
  ``[ORACLE <id> ...]``, ``[REF <source>]``, ``[DESIGN]``, ``[OPEN]``;
  ``[ORACLE id]`` names an existing ``oracle/digests/<id>.md``; a ``[MANUAL]``
  topic is a topic title of one of the two help files; addresses inside
  ``[BYTES]`` use the canonical ``segN:0xOFF`` / ``DGROUP:0xOFF`` form, and
  those without a module name (INSPACE.EXE) or with a module this file can
  load resolve to a real segment and an offset below its minalloc;
- checklist rows have unique ``AREA-nn`` ids, an evidence tag and statuses
  from the fixed vocabularies;
- no ``/home/`` paths under ``docs/`` or ``oracle/``.

Resolution checks (manual topics, segment bounds) need ``original/``; without
it they are skipped and ``test_resolution_inputs_present`` reports the skip.

Violations already in the tree when this linter was written are listed in
``KNOWN`` with their counts. A file whose violations stay within its entry
reports xfail (visible with ``-rx``); any further violation fails. Remove an
entry once its file is fixed.
"""
from __future__ import annotations

import collections
import functools
import re
import warnings
from pathlib import Path
from typing import NamedTuple

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]
DOCS = REPO_ROOT / "docs"
ORACLE_DIR = REPO_ROOT / "oracle"
DIGESTS = ORACLE_DIR / "digests"
README = REPO_ROOT / "README.md"
CHECKLIST = DOCS / "spec" / "checklist.md"
CHECKLIST_DRAFT = REPO_ROOT / "work" / "drafts" / "checklist.md"
ORIGINAL = REPO_ROOT / "original"

# Modules whose segment tables resolve [BYTES] addresses: name -> path under original/.
# An address without a module name belongs to INSPACE.EXE.
MODULE_FILES = {
    "INSPACE.EXE": "shipdisk/INSPACE.ex_",
    "ISPACE.DLL": "shipdisk/ISPACE.dll",
    "FACTORY.EXE": "shipdisk/FACTORY.ex_",
    "THEWORKS/FACTORY.EXE": "theworks/FACTORY.EXE",
    "INSPACE.SCR": "shipdisk/INSPACE.sc1",
    "INRUN.AD": "shipdisk/INRUN.A1",
}
HELP_FILES = ("shipdisk/Eng_help.hl1", "theworks/ENG_HELP.HLP")

TAG_KINDS = ("MANUAL", "BYTES", "ORACLE", "REF", "DESIGN", "OPEN")
BANNED_RE = re.compile(r"\b(superseded|withdrawn|previously|now\s+known)\b", re.I)
TAG_RE = re.compile(r"(?<!!)\[(?P<kw>[A-Za-z]+)(?P<rest>[^\[\]\n]*)\](?![(\[])")
# Tokens that try to be addresses: segN:/DGROUP: forms, Ghidra SSSS:OFF and file: offsets.
ADDRISH_RE = re.compile(r"(?i)^((seg|dgroup)[^:\s]*|file|(0x)?[0-9a-f]{4}):")
ADDR_RE = re.compile(r"^(?:seg(?P<seg>[1-9]\d*)|(?P<dg>DGROUP)):0x(?P<off>[0-9a-f]+)"
                     r"(?:[-–]0x(?P<end>[0-9a-f]+))?$")
FENCE_RE = re.compile(r"^\s*(```|~~~)")

AREAS = ("KEY", "WIN", "NAV", "FLT", "WPN", "HAZ", "SHIP", "TEAM", "LAW", "ECO", "AI", "RACE",
         "DMN", "SET", "INI", "SND", "FAC", "SCR", "MODE", "SYS")
ID_RE = re.compile(r"^(?P<area>[A-Z]+)-\d{2,}$")
COLUMNS = ("ID", "Feature", "Evidence", "Spec", "RE", "Port", "Edition")
VOCAB = {
    "Spec": ("manual", "strings", "oracle", "specified"),
    "RE": ("none", "located", "understood", "verified"),
    "Port": ("todo", "wip", "done", "verified"),
    "Edition": ("demo", "registered", "unknown"),
}
STRING_ID_RE = re.compile(r"\[BYTES ENG_LANG\.DLL STRING (?P<block>\d+)\]\s*ids?\s+"
                          r"(?P<a>\d+)(?:\s*[-–]\s*(?P<b>\d+))?")

# Violations present when the linter landed: path -> {(rule, detail): count}.
KNOWN: dict[str, dict[tuple[str, str], int]] = {
    "oracle/digests/newgame-0001.md": {
        ("tag", "[OPEN meaning]: takes no text"): 1,
    },
}

# Paths under docs/ and oracle/ that hold regenerated or bulk output (gitignored).
SKIP_PARTS = {"__pycache__", "runs", ".pytest_cache"}
# The plan quotes the doc rules themselves (tag placeholders, banned words) and
# the host layout, so it is not linted.
UNLINTED = {"docs/PLAN.md"}


class Violation(NamedTuple):
    line: int
    rule: str
    detail: str

    def __str__(self) -> str:
        return f"line {self.line}: {self.rule}: {self.detail}"


# ---------------------------------------------------------------- resolution inputs

@functools.lru_cache(maxsize=None)
def segment_table(module: str) -> tuple[list[int], int] | None:
    """(minalloc per segment, autodata segment) of a module, or None when unavailable."""
    rel = MODULE_FILES.get(module.upper())
    if rel is None or not (ORIGINAL / rel).is_file():
        return None
    from isre.ne import NEModule

    m = NEModule.load(ORIGINAL / rel)
    return [s.minalloc for s in m.segments], m.autodata


@functools.lru_cache(maxsize=None)
def manual_titles() -> tuple[str, ...] | None:
    """Topic titles of both help files, longest first; None without original/."""
    paths = [ORIGINAL / p for p in HELP_FILES]
    if not all(p.is_file() for p in paths):
        return None
    from isre import hlp

    titles: set[str] = set()
    for p in paths:
        for t in hlp.open_help(p).topics:
            if t.title:
                titles.add(t.title.strip())
            else:
                # The untitled Race Records topic: its first line names it.
                first = t.plain().strip().splitlines()[0].strip() if t.plain().strip() else ""
                if first and len(first) <= 40:
                    titles.add(first)
    return tuple(sorted(titles, key=len, reverse=True))


# ---------------------------------------------------------------- the checks

def _line_of(text: str, pos: int) -> int:
    return text.count("\n", 0, pos) + 1


def _blank_fences(text: str) -> str:
    """Replace fenced code blocks with empty lines (keeps line numbers)."""
    out, inside = [], False
    for line in text.split("\n"):
        if FENCE_RE.match(line):
            inside = not inside
            out.append("")
        else:
            out.append("" if inside else line)
    return "\n".join(out)


def check_address(token: str, module: str | None, tables=segment_table) -> str | None:
    """Return a problem with one address token, or None."""
    m = ADDR_RE.match(token)
    if not m:
        return f"non-canonical address {token!r}"
    table = tables(module or "INSPACE.EXE")
    if table is None:
        return None
    minallocs, autodata = table
    seg = autodata if m.group("dg") else int(m.group("seg"))
    if seg > len(minallocs):
        return f"{token}: {module or 'INSPACE.EXE'} has {len(minallocs)} segments"
    for off in (m.group("off"), m.group("end")):
        if off is not None and int(off, 16) >= minallocs[seg - 1]:
            return (f"{token}: offset 0x{off} is outside seg{seg} "
                    f"(minalloc {minallocs[seg - 1]:#x})")
    return None


def check_tag(kw: str, rest: str, titles=manual_titles, tables=segment_table,
              digests: Path = DIGESTS) -> str | None:
    """Return a problem with one evidence tag, or None."""
    if kw != kw.upper():
        return "tag keyword must be upper case"
    text = rest.strip()
    if kw in ("DESIGN", "OPEN"):
        return "takes no text" if text else None
    if not text:
        return "needs text"
    if kw == "ORACLE":
        ident = text.split()[0]
        if not (digests / f"{ident}.md").is_file():
            return f"no digest oracle/digests/{ident}.md"
    elif kw == "MANUAL":
        known = titles()
        if known is not None and not any(text == t or text.startswith(t + " ") for t in known):
            return "names no topic of the help files"
    elif kw == "BYTES":
        tokens = [t.rstrip(",;") for t in text.split()]
        module = None
        if "." in tokens[0] or "/" in tokens[0]:
            module, tokens = tokens[0], tokens[1:]
        for tok in tokens:
            if ADDRISH_RE.match(tok):
                problem = check_address(tok, module, tables)
                if problem:
                    return problem
    return None


def iter_tags(text: str):
    """Yield (pos, kw, rest) for every evidence-tag candidate outside code fences."""
    for m in TAG_RE.finditer(_blank_fences(text)):
        if m.group("kw").upper() in TAG_KINDS:
            yield m.start(), m.group("kw"), m.group("rest")


def lint_text(text: str, titles=manual_titles, tables=segment_table,
              digests: Path = DIGESTS) -> list[Violation]:
    out = []
    for m in BANNED_RE.finditer(text):
        out.append(Violation(_line_of(text, m.start()), "banned-word",
                             re.sub(r"\s+", " ", m.group(1).lower())))
    for pos, kw, rest in iter_tags(text):
        problem = check_tag(kw, rest, titles, tables, digests)
        if problem:
            out.append(Violation(_line_of(text, pos), "tag", f"[{kw}{rest}]: {problem}"))
    for n, line in enumerate(text.split("\n"), 1):
        if "/home/" in line:
            out.append(Violation(n, "home-path", "/home/"))
    return out


# ---------------------------------------------------------------- the checklist

def checklist_path() -> Path | None:
    for p in (CHECKLIST, CHECKLIST_DRAFT):
        if p.is_file():
            return p
    return None


def _cells(line: str) -> list[str]:
    s = line.strip()
    if s.startswith("|"):
        s = s[1:]
    if s.endswith("|"):
        s = s[:-1]
    return [c.strip() for c in s.split("|")]


def parse_checklist(text: str) -> tuple[list[tuple[int, list[str]]], list[Violation]]:
    """Rows of every table whose header is COLUMNS, as (line, cells)."""
    rows, problems = [], []
    lines = text.split("\n")
    i = 0
    while i < len(lines):
        if lines[i].lstrip().startswith("|") and tuple(_cells(lines[i])) == COLUMNS:
            if i + 1 >= len(lines) or not re.fullmatch(r"\|?(\s*:?-{3,}:?\s*\|)+\s*", lines[i + 1].strip()):
                problems.append(Violation(i + 1, "checklist", "table header lacks a separator row"))
            i += 2
            while i < len(lines) and lines[i].lstrip().startswith("|"):
                rows.append((i + 1, _cells(lines[i])))
                i += 1
        else:
            i += 1
    return rows, problems


def lint_checklist(text: str, titles=manual_titles, tables=segment_table,
                   digests: Path = DIGESTS) -> list[Violation]:
    rows, out = parse_checklist(text)
    if not rows:
        return out + [Violation(1, "checklist", f"no table with header {' | '.join(COLUMNS)}")]
    seen: dict[str, int] = {}
    areas = set()
    for line, cells in rows:
        if len(cells) != len(COLUMNS):
            out.append(Violation(line, "checklist", f"{len(cells)} cells, expected {len(COLUMNS)}"))
            continue
        row = dict(zip(COLUMNS, cells))
        ident = row["ID"]
        m = ID_RE.match(ident)
        if not m or m.group("area") not in AREAS:
            out.append(Violation(line, "checklist", f"bad id {ident!r}"))
        else:
            areas.add(m.group("area"))
        if ident in seen:
            out.append(Violation(line, "checklist", f"duplicate id {ident} (first on line {seen[ident]})"))
        seen.setdefault(ident, line)
        if not row["Feature"]:
            out.append(Violation(line, "checklist", f"{ident}: empty feature"))
        for col, allowed in VOCAB.items():
            if row[col] not in allowed:
                out.append(Violation(line, "checklist", f"{ident}: {col} {row[col]!r} not in {allowed}"))
        tags = [(kw, rest) for _, kw, rest in iter_tags(row["Evidence"])]
        good = [(kw, rest) for kw, rest in tags if check_tag(kw, rest, titles, tables, digests) is None]
        if not good:
            out.append(Violation(line, "checklist", f"{ident}: no well-formed evidence tag"))
        kinds = {kw for kw, _ in good}
        need = {"manual": "MANUAL", "strings": "BYTES", "oracle": "ORACLE"}.get(row["Spec"])
        if need and need not in kinds:
            out.append(Violation(line, "checklist", f"{ident}: Spec {row['Spec']} without a [{need}] tag"))
        if row["RE"] != "none" and not any(
                kw == "BYTES" and any(ADDR_RE.match(t.rstrip(",;")) for t in rest.split())
                for kw, rest in good):
            out.append(Violation(line, "checklist", f"{ident}: RE {row['RE']} without a [BYTES] address"))
        for sm in STRING_ID_RE.finditer(row["Evidence"]):
            a = int(sm.group("a"))
            b = int(sm.group("b") or a)
            if not all((n >> 4) + 1 == int(sm.group("block")) for n in (a, b)):
                out.append(Violation(line, "checklist",
                                     f"{ident}: string id {a}-{b} is not in STRING block {sm.group('block')}"))
    for area in AREAS:
        if area not in areas:
            out.append(Violation(1, "checklist", f"no rows for area {area}"))
    return out


# ---------------------------------------------------------------- file discovery

def lint_files() -> list[Path]:
    files = sorted(DOCS.rglob("*.md")) + sorted(DIGESTS.glob("*.md"))
    if README.is_file():
        files.append(README)
    cl = checklist_path()
    if cl is not None and cl not in files:
        files.append(cl)
    return [p for p in files if not SKIP_PARTS.intersection(p.relative_to(REPO_ROOT).parts)
            and p.relative_to(REPO_ROOT).as_posix() not in UNLINTED]


def _rel(p: Path) -> str:
    return p.relative_to(REPO_ROOT).as_posix()


def _judge(rel: str, found: list[Violation]) -> None:
    """Pass, xfail (within KNOWN) or fail."""
    known = KNOWN.get(rel, {})
    counts = collections.Counter((v.rule, v.detail) for v in found)
    new = [v for v in found if counts[(v.rule, v.detail)] > known.get((v.rule, v.detail), 0)]
    if new:
        pytest.fail(f"{rel}:\n  " + "\n  ".join(map(str, found)), pytrace=False)
    if found:
        pytest.xfail(f"{rel}: {len(found)} known violation(s): " + "; ".join(map(str, found)))
    if known:
        warnings.warn(f"{rel} is clean; remove its KNOWN entry from test_doclint.py")


# ---------------------------------------------------------------- tests on the tree

@pytest.mark.parametrize("path", lint_files(), ids=_rel)
def test_doc(path: Path):
    _judge(_rel(path), lint_text(path.read_text(encoding="utf-8")))


def test_checklist():
    path = checklist_path()
    if path is None:
        pytest.skip("no docs/spec/checklist.md and no work/drafts/checklist.md")
    found = lint_checklist(path.read_text(encoding="utf-8"))
    if found:
        pytest.fail(f"{_rel(path)}:\n  " + "\n  ".join(map(str, found)), pytrace=False)


def test_no_home_paths_in_other_files():
    """Scripts, patches and data under docs/ and oracle/ carry no /home/ paths."""
    bad = []
    for root in (DOCS, ORACLE_DIR):
        for p in sorted(root.rglob("*")):
            rel = p.relative_to(REPO_ROOT)
            if not p.is_file() or p.suffix == ".md" or p.suffix in (".log", ".pyc"):
                continue
            if SKIP_PARTS.intersection(rel.parts):
                continue
            data = p.read_bytes()
            if b"\0" in data[:4096]:
                continue
            for n, line in enumerate(data.decode("utf-8", "replace").split("\n"), 1):
                if "/home/" in line:
                    bad.append(f"{rel.as_posix()}:{n}")
    assert not bad, "absolute /home/ paths: " + ", ".join(bad)


def test_resolution_inputs_present():
    """Reports whether the manual-topic and segment-bound checks ran."""
    missing = [n for n in ("INSPACE.EXE",) if segment_table(n) is None]
    if missing or manual_titles() is None:
        pytest.skip("original/ is absent: [MANUAL] topics and [BYTES] segment bounds were not resolved")
    assert len(segment_table("INSPACE.EXE")[0]) == 19 and segment_table("INSPACE.EXE")[1] == 19
    assert "Laws of Inner Space" in manual_titles() and "Customizing Ships" in manual_titles()


# ---------------------------------------------------------------- synthetic tests of the linter

def _fake_tables(module: str):
    return {"INSPACE.EXE": ([0x100, 0x200, 0x50], 3), "ISPACE.DLL": ([0x10], 1)}.get(module.upper())


def _fake_titles():
    return ("Laws of Inner Space", "Hazards", "Race Records")


@pytest.fixture
def fake_digests(tmp_path):
    (tmp_path / "run-0001.md").write_text("# run\n")
    return tmp_path


def _lint(text, digests):
    return [(v.rule, v.detail) for v in lint_text(text, _fake_titles, _fake_tables, digests)]


def test_banned_words(fake_digests):
    found = _lint("It was Superseded.\nWe now\n known this. Previously too.", fake_digests)
    assert [d for r, d in found if r == "banned-word"] == ["superseded", "now known", "previously"]
    assert _lint("The withdrawal was unprecedented.", fake_digests) == []


def test_well_formed_tags_pass(fake_digests):
    text = ("A [MANUAL Hazards] B [MANUAL Laws of Inner Space crimes list] C [ORACLE run-0001 step 5] "
            "D [REF Wine 11.0 dlls/user.exe16] E [DESIGN] F [OPEN] G [BYTES seg2:0x1ff] "
            "H [BYTES DGROUP:0x4f] I [BYTES ISPACE.DLL seg1:0xf] J [BYTES ENG_LANG.DLL STRING 146] "
            "K [BYTES NE header] L [BYTES seg1:0x10-0x20] [Inner Space] [Open the file](x.md)")
    assert _lint(text, fake_digests) == []


@pytest.mark.parametrize("tag, problem", [
    ("[OPEN meaning]", "takes no text"),
    ("[DESIGN choice]", "takes no text"),
    ("[BYTES]", "needs text"),
    ("[MANUAL]", "needs text"),
    ("[Manual Hazards]", "tag keyword must be upper case"),
    ("[MANUAL Hazard]", "names no topic of the help files"),
    ("[ORACLE run-0002]", "no digest oracle/digests/run-0002.md"),
    ("[BYTES seg4:0x10]", "INSPACE.EXE has 3 segments"),
    ("[BYTES seg2:0x200]", "outside seg2"),
    ("[BYTES DGROUP:0x50]", "outside seg3"),
    ("[BYTES ISPACE.DLL seg1:0x10]", "outside seg1"),
    ("[BYTES seg1:0x10-0x100]", "outside seg1"),
    ("[BYTES 1018:0x20]", "non-canonical address"),
    ("[BYTES file:0x4b860]", "non-canonical address"),
    ("[BYTES seg2:1f]", "non-canonical address"),
    ("[BYTES Seg2:0x1f]", "non-canonical address"),
    ("[BYTES dgroup:0x1f]", "non-canonical address"),
    ("[BYTES seg2:0x1F]", "non-canonical address"),
])
def test_malformed_tags(tag, problem, fake_digests):
    found = _lint(f"x {tag} y", fake_digests)
    if problem is None:
        assert found == []
    else:
        assert len(found) == 1 and found[0][0] == "tag" and problem in found[0][1], found


def test_fenced_code_is_not_tag_checked(fake_digests):
    text = "```\n[BYTES]\n[OPEN later]\n```\n[OPEN]\n"
    assert _lint(text, fake_digests) == []


def test_home_paths(fake_digests):
    assert ("home-path", "/home/") in _lint("see /home/user/x", fake_digests)


def test_unknown_module_addresses_are_only_syntax_checked(fake_digests):
    assert _lint("[BYTES FOO.DLL seg9:0xffff]", fake_digests) == []
    assert _lint("[BYTES FOO.DLL seg9:ffff]", fake_digests)


GOOD_CHECKLIST = """# Checklist

| ID | Feature | Evidence | Spec | RE | Port | Edition |
|---|---|---|---|---|---|---|
| KEY-01 | Thrust | [MANUAL Hazards] | manual | none | todo | demo |
| KEY-02 | Gun | [BYTES ENG_LANG.DLL STRING 12] id 187 | strings | none | todo | unknown |
| KEY-03 | Walk | [BYTES seg1:0x10] [ORACLE run-0001] | oracle | located | todo | registered |
"""


def _lint_cl(text, digests):
    return [v.detail for v in lint_checklist(text, _fake_titles, _fake_tables, digests)]


def test_checklist_good(fake_digests):
    found = _lint_cl(GOOD_CHECKLIST, fake_digests)
    assert found == [f"no rows for area {a}" for a in AREAS if a != "KEY"]


@pytest.mark.parametrize("row, problem", [
    ("| KEY-01 | Dup | [OPEN] | manual | none | todo | demo |", "duplicate id KEY-01"),
    ("| KEY-4 | Short id | [OPEN] | strings | none | todo | demo |", "bad id 'KEY-4'"),
    ("| FOO-04 | Bad area | [OPEN] | strings | none | todo | demo |", "bad id 'FOO-04'"),
    ("| KEY-04 | No tag | none | strings | none | todo | demo |", "no well-formed evidence tag"),
    ("| KEY-04 | Bad tag | [OPEN x] | strings | none | todo | demo |", "no well-formed evidence tag"),
    ("| KEY-04 | Status | [MANUAL Hazards] | manual | none | started | demo |", "Port 'started'"),
    ("| KEY-04 | Edition | [MANUAL Hazards] | manual | none | todo | full |", "Edition 'full'"),
    ("| KEY-04 | Spec | [OPEN] | manual | none | todo | demo |", "Spec manual without a [MANUAL] tag"),
    ("| KEY-04 | RE | [MANUAL Hazards] | manual | located | todo | demo |", "RE located without a [BYTES] address"),
    ("| KEY-04 | Cells | [OPEN] | manual | none | todo |", "6 cells, expected 7"),
    ("| KEY-04 | Block | [BYTES ENG_LANG.DLL STRING 12] id 192 | strings | none | todo | demo |",
     "string id 192-192 is not in STRING block 12"),
])
def test_checklist_problems(row, problem, fake_digests):
    found = _lint_cl(GOOD_CHECKLIST + row + "\n", fake_digests)
    assert any(problem in d for d in found), found


def test_judge_known_counts(monkeypatch):
    monkeypatch.setitem(KNOWN, "x.md", {("tag", "[BYTES]: needs text"): 2})
    v = Violation(1, "tag", "[BYTES]: needs text")
    with pytest.raises(pytest.xfail.Exception):
        _judge("x.md", [v, v])
    with pytest.raises(pytest.fail.Exception):
        _judge("x.md", [v, v, v])
    with pytest.warns(UserWarning):
        _judge("x.md", [])
