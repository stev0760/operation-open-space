# Operation: Inner Space — Win32 port

A faithful port of **Operation: Inner Space v1.29.08** (Software Dynamics, Inc., 1994–2000) from Win16 to Win32. It targets Windows 10/11 natively and Linux/macOS through Wine.

In Inner Space your hard disk is the game world. Each directory is a wave you fly into from a Navigation window. Its programs' icons are there to capture or destroy, alongside rival teams, enforcers, viruses and an Inner Demon.

## Status
**Bootstrap.** No port code exists yet. Current work rebuilds the reverse-engineering pipeline, extracts the manual and assets, and gets the original running under Wine as a reference. See `docs/PLAN.md` and `docs/spec/checklist.md`.

## Layout
| Path | Contents |
|---|---|
| `docs/` | Plan, behaviour spec and checklist, RE reference, design decisions |
| `re/` | Reverse-engineering tools (`isre`), Ghidra scripts, small committed artifacts and tests |
| `oracle/` | Scripts that run the original under Wine for traces and screenshots |
| `src/`, `tools/`, `tests/`, `cmake/` | The C++17 port (from milestone M1) |
| `original/` | Your original game files. **Not committed**; `original.sha256` lists the expected set |
| `work/` | Regenerated outputs. Not committed |

## Original files
The port ships no original assets. To work on it, place the original distribution archives in `original/archives/` (`Shipdisk.zip`, `THEWORKS.zip`) and extract them to `original/shipdisk/` and `original/theworks/`. Then check them with `sha256sum -c original.sha256`.

## Legal
This is an independent, non-commercial reimplementation. Operation: Inner Space and its assets are © Software Dynamics, Inc.
