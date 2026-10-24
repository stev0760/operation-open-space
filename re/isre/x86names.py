"""Names for ``isre asm`` annotations: Win16 import ordinals and DOS services.

Import names come from Wine's Win16 ``.spec`` files (``re/wine-specs/``),
which list each module's exports by ordinal [REF Wine dlls/*/*.spec]. A spec
line is ``ORDINAL TYPE [-FLAGS] NAME(ARGS) IMPL``; ``stub``, ``equate`` and
``extern`` lines carry the name right after the type.

DOS function names follow the INT 21h function list [REF Ralf Brown's
Interrupt List, INT 21]. Only the functions a Win16 C runtime issues are
named; any other number is printed bare.
"""
from __future__ import annotations

import re
from pathlib import Path

# Win16 module name -> Wine spec file name.
MODULE_SPECS = {
    "KERNEL": "krnl386.exe16.spec",
    "USER": "user.exe16.spec",
    "GDI": "gdi.exe16.spec",
    "MMSYSTEM": "mmsystem.dll16.spec",
    "SHELL": "shell.dll16.spec",
    "COMPOBJ": "compobj.dll16.spec",
}

_NAME_RE = re.compile(r"[A-Za-z_?@$][A-Za-z0-9_?@$]*")


def parse_spec(text: str) -> dict[int, str]:
    """Parse one Wine ``.spec`` file into {ordinal: name}."""
    out: dict[int, str] = {}
    last = 0
    for raw in text.splitlines():
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        toks = line.split()
        if toks[0] == "@":
            ordinal = last + 1
        elif toks[0].isdigit():
            ordinal = int(toks[0])
        else:
            continue
        last = max(last, ordinal)
        rest = [t for t in toks[1:] if not t.startswith("-")]
        if len(rest) < 2:
            continue
        m = _NAME_RE.match(rest[1])
        if m:
            out[ordinal] = m.group(0)
    return out


def load_specs(spec_dir: Path | None) -> dict[str, dict[int, str]]:
    """{module: {ordinal: name}} for every spec file present in ``spec_dir``."""
    tables: dict[str, dict[int, str]] = {}
    if spec_dir is None:
        return tables
    for module, fname in MODULE_SPECS.items():
        p = Path(spec_dir) / fname
        if p.is_file():
            tables[module] = parse_spec(p.read_text(encoding="latin-1"))
    return tables


def default_spec_dir() -> Path:
    """``re/wine-specs`` next to the isre package."""
    return Path(__file__).resolve().parents[1] / "wine-specs"


# INT 21h AH values [REF Ralf Brown's Interrupt List, INT 21].
DOS_FUNCTIONS = {
    0x00: "terminate program", 0x01: "read char with echo", 0x02: "write char",
    0x06: "direct console I/O", 0x07: "direct char input", 0x08: "char input",
    0x09: "write string", 0x0A: "buffered input", 0x0B: "input status",
    0x0C: "flush buffer and read", 0x0D: "disk reset", 0x0E: "select drive",
    0x19: "get current drive", 0x1A: "set DTA", 0x1C: "get drive data",
    0x25: "set interrupt vector", 0x2A: "get date", 0x2B: "set date",
    0x2C: "get time", 0x2D: "set time", 0x2F: "get DTA", 0x30: "get DOS version",
    0x33: "Ctrl-Break flag", 0x35: "get interrupt vector", 0x36: "get disk free space",
    0x38: "country info", 0x39: "mkdir", 0x3A: "rmdir", 0x3B: "chdir",
    0x3C: "create file", 0x3D: "open file", 0x3E: "close file", 0x3F: "read file",
    0x40: "write file", 0x41: "delete file", 0x42: "lseek", 0x43: "get/set attributes",
    0x44: "IOCTL", 0x45: "dup handle", 0x46: "dup2 handle", 0x47: "get current directory",
    0x48: "allocate memory", 0x49: "free memory", 0x4A: "resize memory",
    0x4B: "exec", 0x4C: "terminate with return code", 0x4D: "get return code",
    0x4E: "findfirst", 0x4F: "findnext", 0x54: "get verify flag", 0x56: "rename file",
    0x57: "get/set file time", 0x58: "allocation strategy", 0x59: "extended error",
    0x5A: "create temp file", 0x5B: "create new file", 0x5C: "lock/unlock region",
    0x62: "get PSP", 0x67: "set handle count", 0x68: "commit file", 0x6C: "extended open",
}

# DOS functions that leave AL (and AH) unchanged, so a backward search for the
# value of AL may pass through them [REF Ralf Brown's Interrupt List, INT 21].
DOS_PRESERVES_AX = frozenset({0x1A, 0x25, 0x2F})

# AH values after which control never returns.
DOS_NORETURN = frozenset({0x00, 0x4C})

# DOS functions that take a subfunction or argument in AL.
DOS_AL_INPUT = frozenset({0x0C, 0x25, 0x33, 0x35, 0x3D, 0x42, 0x43, 0x44, 0x4B, 0x4C, 0x57,
                          0x58, 0x5C, 0x5E, 0x5F})

# Imports that do not return to their caller.
IMPORT_NORETURN = frozenset({("KERNEL", 1)})  # FatalExit; FatalAppExit (KERNEL.137) can return


def dos_name(ah: int) -> str:
    return DOS_FUNCTIONS.get(ah, "")
