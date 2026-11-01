# Operation: Inner Space — Win32 port

A faithful port of **Operation: Inner Space v1.29.08** (Software Dynamics, Inc., 1994–2000) from Win16 to Win32. It targets Windows 10/11 natively and Linux/macOS through Wine.

In Inner Space your hard disk is the game world. Each directory is a wave you fly into from a Navigation window. Its programs' icons are there to capture or destroy, alongside rival teams, enforcers, viruses and an Inner Demon.

## Status
**Milestones M1 (foundations), M2 (shell) and M7 (audio) are done; M3 (world) has its stages 0 to 4 done, and M5 (ships) its stages 1 and 2.** The port starts, installs from the original install files (a directory or the `.zip`), shows the intro and title, runs the dialogs, the Navigation window and the briefing, plays sound and music through its own mixer and synths, and reaches wave 1 with the player's own ship drawn from its body bitmap with its thrust flame, the gate, rocks, stars, a radar and the full status display; the rest of the core loop (capture, damage, weapons, other ships) are the next milestones. `docs/PLAN.md` holds the milestone table with each stage's results and open items, and `docs/spec/checklist.md` the per-feature status. One command regenerates the unpacked code, disassembly, decompiled C, manual and assets from the original files (`re/README.md`); the original runs under Wine as a scripted reference, the oracle (`docs/re/oracle.md`), and `docs/re/` holds the verified knowledge base.

## Layout
| Path | Contents |
|---|---|
| `docs/` | Plan, behaviour spec and checklist, RE reference, design decisions |
| `re/` | Reverse-engineering tools (`isre`), Ghidra scripts, small committed artifacts and tests |
| `oracle/` | Scripts that run the original under Wine for traces and screenshots |
| `src/`, `tools/`, `tests/`, `cmake/` | The C++17 port: format readers in `src/formats/`, the `isview` asset viewer in `tools/` |
| `third_party/` | Vendored dependencies (doctest) |
| `original/` | Your original game files. **Not committed**; `original.sha256` lists the expected set |
| `work/` | Regenerated outputs. Not committed |

## Original files
The port ships no original assets. To work on it, place the original distribution archives in `original/archives/` (`Shipdisk.zip`, `THEWORKS.zip`) and extract them to `original/shipdisk/` and `original/theworks/`. Then check them with `sha256sum -c original.sha256`.

## Legal
This is an independent, non-commercial reimplementation. Operation: Inner Space and its assets are © Software Dynamics, Inc.
