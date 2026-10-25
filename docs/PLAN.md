# Inner Space Port — Project Overhaul & Porting Plan

## Status
| Phase | State | Notes |
|---|---|---|
| 0 Bootstrap + oracle smoke | **done** | See the Phase 0 deviations below. Smoke test: `oracle/digests/smoke-0001.md`. |
| 1 RE pipeline | **done** | `isre rebuild` (loader-equivalent, runs under Wine), `isre asm` (verified), `isre pack`, Ghidra pipeline with C export (`docs/re/ghidra.md`; S4 verifier's findings fixed). Tool guide: `re/README.md`. |
| 2 Knowledge base | **done** | `isre hlp`, `isre res` (`docs/re/resources.md`), checklist (636 rows, 243 with RE addresses) with the doc linter, S3 `docs/re/formats.md`, S4 `docs/re/structs.md` and `globals.md` (Appendix B items settled), each with two independent verification rounds; `re/tests/test_re_facts.py` pins their mechanical facts. `docs/re/win16-translation.md` (pinned by `re/tests/test_win16_facts.py`). |
| 3 Oracle | **done** | Scripted runs play wave 1 (`wave-0001`); read-only memory probe (`probe-0001`); `docs/re/oracle.md` answers the phase's questions, with the rest listed as open questions. Two Wine bugs found and patched locally (`docs/upstream/`, not filed). |
| Early spikes | **done** | `docs/re/ispace.md` (ISPACE.DLL owns palette, bitmaps, sound, dialogs), `docs/re/rng.md` (Microsoft C `rand()`), `docs/re/timing.md` and ADR 0001 (accepted). |
| M1 Foundations | **done** | Readers for every original format (`src/formats/`), the asset store (`src/assets/`), `isview` v1, ADRs 0001–0003. Each stage independently reviewed, plus a whole-codebase review; converted dialogs match the original's client size (`oracle/digests/dialogs-0001.md`). Open: `isview` on a real Windows machine. |
| M2 Shell | **in progress** | Stage 0 done: the RE round on start-up, exit, screen states, intro, dialogs, geometry and palette (`docs/re/shell.md`, 200 claims, 171 confirmed by independent verifiers, 44 fact tests). Stage 1 done: `src/shell/` (scheduler, state machine, command tables, geometry, window keys), `src/app/` (`inspace.exe`, both window modes, loop, timer, exit with INI write-back) and `src/settings/` (IniFile, GameSettings, regeneration), reviewed and fixed; runs headless under Wine (`work/shell/s1/notes.md`). ADRs 0004–0007 are Proposed and gate stages 2 and 3. |

**Phase 0 deviations:**
- **Wine 11.0 stable** (Kron4ek `amd64-wow64`) instead of 11.20. On this host 11.20 segfaults in `wineboot` with a bad address-space limit.
- **Oracle runtime is old-WoW64 Wine 11.0** (Kron4ek `amd64`). New WoW64 loses ISPACE.DLL's DS after `GlobalFree` and faults (`oracle/digests/fault-0001.md`); old WoW64 runs the game into wave 1 (`oracle/digests/runtime-0001.md`).
- **No original installer:** `INSTALL.EXE` crashes under Wine 11.0, so `oracle/install_layout.py` replaces it.
- **Displays:** oracle displays start at `:110`, clear of the desktop's `:0` and its Xwayland displays.
- **Deferred:** helpdeco is built in Phase 2a; doctest is vendored at M1; FluidSynth only if needed.

## Context

The goal is a **faithful Win32 port of Operation: Inner Space v1.29.08** (SDi, Win16 NE). It should run natively on Windows 10/11 and under Wine. The port reads assets from the user's own original install. Earlier work by Hermes with open-weights models left two problems:

- **`innerspace-port/`** has only `README.md` and `PLAN.md`. The design in them is wrong: a desktop overlay, "no filesystem walker", and an invented feature set.
- **`innerspace-rea/`** (78 MB, no git) has real byte-level groundwork, buried in about 50 one-off scripts, `.class` files, 57 MB of void or demoted Ghidra output, and docs full of retractions.

This session's research ran 4 explorer agents and a plan review. The full findings are in **Appendix A** (workspace triage) and **Appendix B** (facts and refutations). The headlines:

- **Wrong claims in the old docs:** the game does walk the filesystem (seg5 findfirst/findnext); DGROUP is seg19, not seg1; and the capture state machine and icon fallback ladder are misread.
- **The manual is the behaviour spec.** `ENG_HELP.HLP` is WinHelp 3.1, uncompressed. It says directories on disk are "waves" entered from a Navigation window, and the game runs in its own window, full-screen by default, with a status panel. It documents a large feature set: teams, laws, about 20 weapons, 12 hazards, races, Inner Demon and more. The full key map is known.
- **Assets:** ship and UI art are in `IGRAPHIC.DLL`, strings, dialogs and menus in `ENG_LANG.dll`, sounds in `SND_FX.DLL` / `eng_fx.dll`. `MASTER.SHP` is decoded as 118×65-byte named ships pointing at the 52 BOD bodies.
- **ISPACE.dll** is a packed second engine: 162 KB of code, loaded with LoadLibrary, 23 ordinals used. It probably holds rendering (DIBs, palettes), sound, options and save/load. Nobody has analysed it.
- **Pipeline gap:** no script regenerates the unpacked segments. `rebuild_ne.py` writes no relocations and a broken resource table, which is the root of the Ghidra failure.

**User decisions:**
- One monorepo.
- Original files kept local only, with a committed SHA-256 manifest.
- The original window model, scaled.
- A rewrite guided by decompilation, checked against the original running under Wine.
- Subagent orchestration.
- Passwordless sudo, so install directly.
- **No registration code.** The oracle observes only the unregistered build. Registered-only features are specified purely from static RE of their code paths, and the checklist marks them as such: Factory, save/restore, Inner Demon and Noble Weapons, 100+ ships, powerups. The registration check is not patched.
  - Later, the user buys a registered copy of the game (it is still sold). From then on the oracle also observes registered-only features, using the user's own registration and never a patch. The registration code stays local like `original/` and is never committed. If the copy sold is not v1.29.08, the pipeline processes it as a separate build, and the port's addresses stay those of v1.29.08 unless a decision record changes the target.

**Scope of this approval:** Phases 0–3, which deliver the repo, the RE pipeline, the knowledge base and the oracle. Each port milestone M1–M9 gets its own plan later.

## Guiding principles (go into `CLAUDE.md`)
1. **Evidence tags on every claim:** `[MANUAL topic]`, `[BYTES segN:0xOFF]`, `[ORACLE id]`, `[DESIGN]` or `[OPEN]`.
   - Verification is tiered. Mechanical facts (counts, hashes, offsets) are proven by tests. Interpretive claims need a verifier agent that sees only the claim and its addresses, not the analyst's reasoning.
2. **Docs state current truth; git holds history.** No narrative of what was superseded.
   - Dead ends are kept as **present-tense rules** in `docs/re/pitfalls.md`, e.g. "relocation chains reach 159 sites; never cap", "Win16 APIs push args left-to-right".
3. **One canonical address form: `segN:0xOFF`.** `isre addr` converts to and from Ghidra `10xx:`, file offsets and DGROUP. Each topic has one source of truth and one writer per doc per workflow run. Only the orchestrator edits `checklist.md`.
4. **A doc linter (pytest) enforces the rules:**
   - Every claim is tagged.
   - `[BYTES]` addresses resolve to a real segment and offset.
   - `[ORACLE id]` resolves to a committed text digest in `oracle/digests/`.
   - Checklist rows have stable IDs (`WPN-07`) and fixed status values.
   - Banned words: superseded, withdrawn, previously, now known.
5. **Every artifact regenerates with one command** from `original/`. No absolute paths in tools; every tool has tests.
6. **Progress is measured by `docs/spec/checklist.md`,** not by percentages.
7. **Fidelity first.** Modernisations become ADRs in `docs/decisions/`.

## Target repo layout (`the repository root`)
```
CLAUDE.md README.md .gitignore CMakeLists.txt CMakePresets.json
docs/spec/       checklist.md (master feature ledger, stable IDs) + per-subsystem stubs filled per milestone
docs/re/         binary-map, structs, globals, functions, resources, formats, oracle, pitfalls, win16-translation
docs/decisions/  ADRs (timing, window/scaling, filesystem rules, asset loading, save format, help)
re/isre/         Python package + CLI (ne, szdd, unpack, relocs, rebuild, asm, addr, hlp, res, pack)
re/ghidra/       headless import/analysis/export scripts (Ghidra 12.1.2 @ /opt/ghidra, JDK 21)
re/artifacts/    small committed outputs + golden SHA256 manifests (segments, relocs.json)
re/wine-specs/   Win16 ordinal specs (record source commit);  re/tests/  pytest (goldens, parsers, doc linter)
oracle/          Wine launch/scenario scripts, fixture directory trees (8.3 ASCII, old icons), mem-probe helper, digests/
src/ tools/ tests/ cmake/    C++17 port (empty until M1, apart from the toolchain file and probe helper)
original/  GITIGNORED (+ committed original.sha256)      work/  GITIGNORED (unpacked, asm, Ghidra projects, decomp C, manual, assets, traces)
```
`innerspace-rea/` stays where it is and is made read-only with `chmod -R a-w`. Renaming it would break its hardcoded paths and the `~/ghidra-projects` project. Nothing is deleted until you say so. Only the KEEP items from Appendix A are copied into the new repo.

## Orchestration model
I orchestrate. Each phase stage runs as a **Workflow** script (≤10 agents) with these roles:
- **Analyst** (read-only): works from generated **context packs** (`isre pack segN:0xOFF`: annotated disassembly, Ghidra C, xrefs, struct and global notes, ≤30 KB), never the bulk dumps.
- **Verifier** (adversarial and independent).
- **Implementer:** uses git worktrees when tasks run in parallel; its work must build and pass tests.
- **Oracle runner:** uses its own `WINEPREFIX` and `DISPLAY`, because of the game's single-instance check.
- **Doc writer:** passes the linter and `prose-lint`.

Cheaper models handle refactors, tests and lint. I merge, update the checklist and commit at the end of each stage.

## Phase 0 — Bootstrap + oracle smoke test (me, sequential)
1. **Install:**
   - **Wine:** Kron4ek **11.20 `amd64-wow64`**, checksum-verified, into `/opt/wine-11.20`, symlinked as `/opt/wine`.
     - Wine 11's new WoW64 mode runs 16-bit apps. The kernel has `MODIFY_LDT`, `X86_16BIT` and `IA32_EMULATION`.
     - SELinux: `semanage fcontext -a -t textrel_shlib_t '/opt/wine[^/]*/lib/wine(/.*)?'`, then `restorecon -R`.
     - Fallbacks, in order: the Kron4ek `amd64` multilib build plus `glibc.i686`; otvdm 0.9 inside Wine; DOSBox-X with Win3.1.
   - **Cross-compiler:** `dnf install mingw32-gcc-c++ mingw64-gcc-c++ mingw{32,64}-winpthreads-static` (GCC 14.2.1 from CRB), linked with `-static`. Alternative: llvm-mingw 20250402; newer releases need a GLIBCXX the host doesn't have.
   - **Python:** `unar`, and a venv at `re/.venv` with `pytest` and `capstone`. Build `helpdeco` from source to cross-check the HLP extractor. Vendor the `doctest` header.
   - **Display:** Xvfb, `xdotool` and ImageMagick are already present. FluidSynth is **deferred**: a null MIDI device comes first, and FluidSynth is added only if missing MIDI changes code paths.
2. **Create the repo:** `git init` with `.gitignore`, `CLAUDE.md`, a stub `README.md`, and Appendices A and B committed as `re/TRIAGE.md` and `docs/re/pitfalls.md` seeds.
3. **Populate `original/`:** unar and SZDD from the two archives, after checking the archive SHA-256 values (`3e560aa7…`, `4e4af35d…`). Commit `original.sha256`.
   - **Record the golden hashes now,** from the archive: `unpacked/segNN.bin` (seg19 zero-padded to 41,456 bytes, seg1 raw at 2,164) and `relocs.json`.
4. **Copy and lock the old workspace:** copy the KEEP tools and artifacts into `re/`, then `chmod -R a-w innerspace-rea`.
5. **Oracle smoke test (the biggest risk, so it comes first):**
   - Create a fresh prefix and run the real **`INSTALL.exe`** (Win16) under Xvfb. Snapshot the prefix diff.
   - Run the packed `INSPACE.exe` to the title screen and take a screenshot.
   - Confirm that `+relay` logs 16-bit KERNEL/USER calls.
   - On failure, work through the fallback ladder before going further.
6. **Save memories:** passwordless sudo / "just do it", the repo layout, and the evidence-tag rule.

## Phase 1 — RE pipeline (Workflow "re-pipeline", ≤9 agents)
| Stage | Shape | Work |
|---|---|---|
| S1 gate | 1 implementer, sequential | See S1 below. |
| S2 | 4 implementers in parallel worktrees | (a) the relocation emitter in `rebuild`; (b) recursive-descent disassembly; (c) the other modules; (d) the Ghidra headless scripts. See S2 below. |
| S3 | 1, sequential | Ghidra run and per-function C export to `work/decomp/<module>/`. |
| S4 | 3 verifiers in parallel | See S4 below. |

**S1 (gate):**
- Build `isre ne` with resource offsets and names fixed and seg19 marked as data.
- Build `isre unpack`: the missing writer around `slr_relocs.dpmi_unquik_trace`, starting at fo+2, zero-padded to minalloc, seg1 raw.
- Build `isre relocs`, `isre addr` and `isre szdd`.
- Goldens must match.

**S2:**
- **(a) Relocation emitter in `rebuild`:**
  - Emit the 930 chain heads as standard NE records. Special-base chains link through the segment word, for example seg4@0x24 → 0x6b → 0xFFFF.
  - Keep import segment placeholders at `0000`, and rebuild the resource table.
  - **Loader-equivalence test:** a small NE loader applying our records must reproduce the OptLoader-relocated images.
- **(b) Recursive-descent disassembly:** replaces the linear sweep, seeded from exports, prologues and chain heads, and fixes the header offsets.
- **(c) Other modules:** run `isre` on **ISPACE.dll** (343 records: 310 import-ordinal, 33 special-base, one of them additive), **FACTORY ×2**, INSTALL and SETUP. Use the same invariants as INSPACE: zero slack, counts match, every record lands on an opcode. INSPACE.SCR is not OptLoader-packed.
- **(d) Ghidra headless scripts:**
  - Set the DS/SS context to the DGROUP selector.
  - Seed functions from exports, prologues and chain heads, with the aggressive instruction finder off.
  - Apply Win16 names and the Pascal convention from `wine-specs`.
  - Label seg5 as Borland RTL.
  - Join INSPACE `GetProcAddress` ordinals to the ISPACE exports.

**S4 (verifiers):**
- A byte check of 20 functions spread across segments and modules: call targets, function bounds, no functions in DGROUP.
- A runnable-NE smoke test: the rebuilt unpacked exe reaches the title screen under Wine. That proves unpack plus relocations end to end and gives a patchable binary for oracle hooks.
- A rea MCP check on the Ghidra project.

## Phase 2 — Knowledge base (Workflow "kb"; S1 starts after Phase 1 S1)
- **S1 (2 agents in parallel):**
  - **`isre hlp`:** walk `|TOPIC` by next-pointer, handle signed picture sizes, and cross-check against `helpdeco`. Extract both help files to `work/manual/`. The 2000 file governs v1.29.08; the 1998 file covers the Factory topics removed in 2000.
  - **`isre res`:** dump ENG_LANG strings, dialogs, menus and accelerators; IGRAPHIC bitmaps; WAV DLLs; ISPACE resources to `work/assets/`. Write `docs/re/resources.md`.
- **S2 (1 doc agent):** build `docs/spec/checklist.md` from the manual and ENG_LANG strings. Stable IDs cover the key map, weapons, hazards, laws, settings, windows and modes. Columns: spec, RE and port status. Subsystem spec files start as stubs.
- **S3 (analyst + verifier, after Phase 1 S3):** write `docs/re/formats.md`.
  - BOD: corrected layout.
  - SHP: 118×65, checked against FACTORY's BOD/SHP I/O routines only.
  - Also: `#6600` and the ISPACE tables, save files (`.sav`/`.don`/`.ded`), race records, and all INI keys.
- **S4 (2–3 verifiers, split by topic):** re-check the Appendix B "needs re-verification" items. Write `docs/re/` structs and globals with only the confirmed facts. Seed `pitfalls.md` and `win16-translation.md`:
  - 16-bit wrap, idiv/sar.
  - Fixed point (no FPU fixups).
  - `__AHINCR` huge pointers.
  - Message differences, dialog units, CTL3D.

## Phase 3 — Oracle (Workflow "oracle", ≤4 agents; runs **alongside Phase 1** once Phase 0 step 5 passes)
- **Fixtures:** committed 8.3-ASCII directory trees with old 16-colour icons, because Wine returns host readdir order. Run Xvfb at **8-bit PseudoColor and 24-bit**, since the palette, XOR/ROP and `BadVideo` paths differ.
- **Scenarios** in `oracle/`: intro → New Game → Navigation → enter a fixture directory → fly and capture → save (if the unregistered build allows it, LASTGAME becomes a free state comparator).
- **Each run produces:** screenshots, a filtered `+relay` trace (dir/file I/O, timers, GDI, MMSYSTEM) and a committed text digest per run.
- **Memory probe:** a small Win32 helper (ReadProcessMemory, finds DGROUP by a signature string) instead of `winedbg`. Address-level probes wait for Phase 1 S3.
- **Deliverable:** `docs/re/oracle.md`, answering each question with digests:
  - How the filesystem walk behaves: roots, filters, order, caps.
  - Window and client geometry, and whether the world size depends on the client rect.
  - Palette and depth.
  - Unregistered limits.
  - Which timer and clock APIs the game calls.

## Early spikes (folded into the phases above)
- **RNG:** the Borland rand constant 0x015A4E35 is in no INSPACE segment, so the RNG is custom or in ISPACE. Find it and its seed; deterministic comparison needs both. Phase 1 S4 / Phase 2 S4.
- **Timing ADR, from static RE:**
  - Win3.x/9x SetTimer was quantised to 55 ms, so `FrameRateSet=36` probably ran at about 18 Hz on period hardware.
  - Wine times accurately, so measuring the oracle misleads.
  - Find how timeGetTime/GetCurrentTime, GameSpeed, Calibrate and WM_TIMER coalescing are used.
  - M2's timestep is decided only after this.
- **Rendering ownership:** INSPACE imports no CreatePalette, CreateDIBitmap or StretchDIBits, so confirm that ISPACE owns DIB, palette and sound. Check the 23-ordinal contract between the 2000 EXE and the 2002 DLL.

## Later milestones (each planned separately)
| # | Milestone | Content |
|---|---|---|
| M1 | Foundations (detailed plan below) | CMake presets for mingw i686/x86_64 under Wine (MSVC later). C++ readers: NE resources, SZDD, BOD/SHP, DIB with palette and transparency. 16→32-bit DLGTEMPLATE/MENU converter, so the original dialogs come from ENG_LANG. doctest. `isview` asset viewer. |
| M2 | Shell | Game window and intro, Restore/New Game, accelerators, port-local INI, timestep per ADR, integer/DPI scaling, help (HLP→HTML). |
| M3 | World | Filesystem walk per the original's rules, with a modern-safety ADR. Navigation, wave generation, ExtractIconEx at 32×32, rocks and gates, flight model, status panel, radar. |
| M4 | Core loop | Capture, fuel, damage and auto-repair, weapons, ambulance economy, scoring, waves, save/load (format ADR). |
| M5 | Ships & AI | Teams and attitudes, commands, defenders, viruses, hazards, enforcers, laws and Hall of Justice, Strategy mode, perspective shift. |
| M6 | Specials | Races and duels, Inner Demon and Noble Weapons, demo, Special Times of Day. |
| M7 | Audio | WAV mixer replacing WAVEMIX; MIDI through MCI or midiOut. |
| M8 | Extras | Icon and Ship Watch, dynamic help, Ship Factory, screensaver. |
| M9 | Release | Wine + Win10/11 matrix, performance, packaging, bring-your-own-copy setup. |

## M1 — Foundations (detailed plan)
**Goal:** every original asset the port needs is readable through tested C++ readers, the original dialogs, menus and accelerators run as Win32 templates, and `isview` shows all of it. No gameplay. M1 is done when M2 can start the game shell without writing a new reader.

**Already delivered by the proof of concept** (`8d939e2`, `4f9c3b0`):
- CMake presets `mingw-x64`, `mingw-i686` (static; CTest runs the tests under Wine) and `host` (native).
- doctest.
- `src/formats/`:
  - NE resource tables;
  - Win16 STRING, DIALOG, MENU and ACCELERATOR decoders with conversion to Win32 templates;
  - DIBs.
- Golden tests: all 1,446 strings equal `isre`'s output.
- `isview` v0.
- The `poc_wave` sandbox.

**Decisions first** (ADRs the user approves before the work that depends on them):
- **ADR 0001 Timestep:** **accepted** (`docs/decisions/0001-timestep.md`).
- **ADR 0002 Asset source:** **accepted** (`docs/decisions/0002-asset-source.md`). An installed or distribution directory, the port's own directory first; versions identified by file hashes; read-only.
- **ADR 0003 Text and paths:** **accepted** (`docs/decisions/0003-text-and-paths.md`). W APIs only; game text stays cp1252 and converts at the Win32 call; paths are `std::filesystem::path`; the original's files keep cp1252.

**Stages** (one Workflow run per stage; parallel implementers own disjoint files):
1. **Readers** (2 implementers in parallel):
   - (a) SZDD expansion; MASTER.BOD and MASTER.SHP with every field in `docs/re/formats.md`; the VERSION resource. With unit and golden tests, plus an `isre ships --json` reference dump if the goldens need one.
   - (b) ICON and GROUP_ICON; WAV resources from SND_FX.DLL and ENG_FX.DLL; the records that code loads as data (IGRAPHIC #6600, ISPACE #5500, #5600 and #5000 150). With tests.
2. **Asset store** (1 implementer), `src/assets/`:
   - opens a game or distribution directory, with case-insensitive lookup and transparent SZDD;
   - checks the version table and picks the language DLL from the `Language` key (default ENG);
   - gives lazy typed access to strings, dialog templates, menus, accelerators, sprites, bitmaps, icons, WAVs and ships.
   - The dialog font mapping (Helv 8 pt) stays a parameter until M2's window and scaling ADR.
3. **isview v1** (1 implementer):
   - a resource tree for all 14 modules, with string search;
   - dialogs, menus and accelerators;
   - bitmaps and sprites with zoom, palette and frame-grid views; icons;
   - WAV playback from memory;
   - a ship browser (SHP list, BOD sprite, every field);
   - `--dump <module>` JSON for golden comparison with `isre res`.
4. **Verify:**
   - independent code review of `src/` and `tools/` (bounds, undefined behaviour, error paths);
   - golden cross-check of every reader against `work/assets` and `formats.md`;
   - screenshots of the converted dialogs against the oracle's, recorded in a digest.

   Then fixes, and the orchestrator merges and commits.

**Verification:**
- `ctest` passes on `mingw-x64`, `mingw-i686` and `host`, with the golden tests running, not skipped.
- Every resource of the game's modules decodes, and the counts equal `docs/re/resources.md`.
- MASTER.BOD and MASTER.SHP give 52 and 118 records, with field values equal to the reference dump.
- Converted dialogs have the original's pixel size under Wine.
- Optional: `isview` on the user's Windows 10/11 machine.

**Out of scope:** the game window, INI and timing (M2); the filesystem walk (M3); audio playback (M7); MSVC.

**Carried into M2:** the original loads SND_FX.DLL and `<Language>_FX.DLL` from the INI's `SndPath`, which a regenerated INI sets to `Path` + `SOUNDS\`. A flat distribution keeps them in the game directory. M2's INI ADR decides how the port maps `SndPath` (the asset store takes it through `set_sound_dir`).

## M2 — Shell (detailed plan)
**Goal:** the port starts, shows the intro and title, lets the player open Restore Game and New Game, reaches the mission briefing and exits, with the original's window modes, timing, palette behaviour and INI. The wave itself stays a black play area with the HUD frame until M3 and M4. M2 is done when a run from launch to the briefing and back to exit matches the oracle's screens and INI writes.

**Source of truth:** `docs/re/shell.md` (start-up and exit sequences, the state table and transitions, the intro phases, the ordinal 10 command map, geometry, palette, help), with `globals.md`, `timing.md` and `formats.md` for what they own.

**Decisions first** (ADRs the user approves before the stage that depends on them):
- **ADR 0004 INI location:** accepted; stage 3 implements the default path (stage 1 shipped with `--ini <path>` required).
- **ADR 0005 Help:** accepted; stage 3.
- **ADR 0006 Window and scaling:** accepted; stage 2 (the renderer's logical size and presentation).
- **ADR 0007 Palette:** accepted; stage 2 (the 8-bit surface and the software palette).

**Stages** (one Workflow run per stage; parallel implementers own disjoint files):
0. **RE round** (done): `docs/re/shell.md`; checklist RE statuses updated.
1. **Skeleton** (2 implementers, 1 reviewer): `src/shell/`: WinMain, the seven window classes with their styles, the start-up order and failure messages of shell.md, the main window in both modes with the Show rectangle rules, the message loop with the frame scheduler of ADR 0001, the state machine (10 states, frame hooks, requests through the state-change step), the 1000 ms timer with the one-shot, focus watch, pause and resume, WM_SYSCOMMAND and WM_COMMAND tables (menus 101/102/POPUPMENU and MAINACCEL from the asset store, commands posted as key events), the exit sequence and INI write-back, the private messages. `src/settings/`: the INI layer over the W profile functions with every key, default and regeneration rule of formats.md, behind a path. Portable pieces (scheduler, state machine, settings parsing) carry doctest tests on the `host` preset; the window runs under Wine in a headless check that records the class, style, size and title.
2. **Renderer, intro and title** (2 implementers, 1 reviewer): `src/gfx/`: the 8-bit surface, the software palette and the ported palette manager (ADR 0007), presentation with integer scaling (ADR 0006), the bitmap-font and frame helpers that the title needs (ISPACE ordinals 33–35). The state 0 phases: logo, title with the demo-day line, desk scene with its greeting, launch scene, demo attract, per shell.md's phase table, timings and key handling. Oracle comparison: screenshots of each phase against `smoke-0001`, `newgame-0001` and `depth8-0002`.
3. **Dialogs, briefing, exit and help** (2 implementers, 1 reviewer): the ordinal 10 dialogs the shell reaches (Restore Game, New Game, New Demo, Exit, Order, Benefits, Important Information, Plus Pack) with their procedures' logic and INI writes; the Navigation window as a stub that M3 fills (creation, Enter, the Help button); state 3 briefing and the transition to state 1; states 4 and 5 (GAME OVER, high scores) as far as the shell reaches them; the exit dialogs and `PlusCount`; help per ADR 0005 (`src/formats/hlp`, HTML export, context map).
4. **Verify:** independent review of `src/`; an oracle digest comparing the port's run from launch to the briefing and to exit with the original's (screens, window geometry, INI writes); fixes; the orchestrator updates the checklist Port statuses and commits.

**Verification:**
- `ctest` green on `host`, `mingw-x64` and `mingw-i686`; `re/tests` and `oracle/tests` green.
- The port's window class, style, size and mode switches match shell.md and the oracle.
- Title and intro screenshots match the oracle's phase for phase; the demo-day line and greeting follow the rules of shell.md.
- A run from launch through New Game to the briefing and Exit writes the same INI keys as the oracle's run.
- Checklist: the WIN, MODE, SYS and INI rows that shell.md covers move to Port `done` or `verified`.

**Out of scope:** the Navigation walk (M3), the wave (M3, M4), sound (M7), the Online Help window and watch windows (M8), MSVC.

## Verification (Phases 0–3)
- **Phase 0:** the original reaches its title screen under Wine 11.20, with a screenshot and a relay trace showing 16-bit calls. `original.sha256` and the goldens are committed.
- **`pytest re/tests`:**
  - Segment and relocation goldens match.
  - The loader-equivalence test passes.
  - ISPACE and FACTORY invariants hold.
  - The doc linter passes.
  - `grep -r /home/ re/ docs/` is empty.
- **Ghidra:** no functions in DGROUP, 183 API names resolve, and the 20-function verifier sample is CONFIRMED. The rebuilt unpacked NE reaches the title screen.
- **Knowledge base:** `isre hlp` extracts 40 topics (1998) and 37 (2000), plus one empty terminator header each, with 0 errors and agrees with helpdeco. `isre res` counts match: 1,446 strings, 22 dialogs, 113+15 bitmaps, 46+32 WAVs. Every checklist row has an ID and a `[MANUAL]` tag.
- **Oracle:** `docs/re/oracle.md` answers each question with digest references.
- **Hygiene:** `git status` is clean after `isre all`.

---
## Appendix A — Workspace triage (`innerspace-rea/`)
- **KEEP-TOOL:**
  - szdd, ne_parse, slr_relocs, rebuild_ne, name_imports, annotate, annotated_asm, callgraph, prologue_scan, struct_fields, dump_asm, redirect_entry, attr_fields, field_by_func, globals_by_func, find_callers, resolve_call, ne_tables.
  - Checks: validate_relocs, validate_entries, test_decomp.
  - **Known bugs to fix:** rebuild_ne (no relocs, bad resource table); ne_parse (`file_off` 0, garbage names, seg19 marked code); annotated_asm (header shows the sector number, linear sweep loses sync at seg5:0x351); prologue_scan (hardcodes seg1).
  - 16 scripts hardcode `PROJ=/home/cabal/Git/repos/innerspace-rea`.
- **KEEP-ARTIFACT:**
  - Inputs: Shipdisk.rar (actually a ZIP) and THEWORKS.rar; wine-specs/ (record the commit); optlink-src/nappload.asm (record source and licence).
  - Small outputs: relocs.json, api-imports.json, function-entries.json, callgraph.json.
- **REGENERABLE:** extracted/, unpacked/, INSPACE-unpacked.exe, api-sites.*, annotated-asm/, api-calls*.txt, struct-fields.*, ne-*.json, import-set.json, probes/*.bin.
- **ARCHIVE** (stays in the locked old directory): all 6 .md docs, one-off probes (coverage, diagnose, probe_links, inspect_relocs, data_slots, icon_source, entries_for, rank_targets, find_collision, check_docs, dump_dgroup, ne_summary), and the Ghidra .java glue.
- **NOT COPIED (junk):** *.class, __pycache__, slr_decompress*.py and slr_unpack.py (broken), struct_map/field_semantics + outputs, array_base, probe_bytes/probe_calls, r2-functions*, functions-unpacked.tsv, prologue-funcs.json, ghidra-out/ (57 MB; decompiled.c is VOID).

## Appendix B — Facts and refutations to carry forward
- **Byte-verified (keep):**
  - Packed segment = `[count][bitstream][relocs]`, stream at fo+2, bit 1 = literal; all 18 reproduce.
  - 930 relocs (725 ordinal + 205 special-base), chains end at 0xFFFF; 186 ordinals.
  - DOS3Call ×39, all in seg5 (the Borland RTL). ExtractIcon ×5 at seg6:0x442c/0x4469 and seg9:0x0178/0x01a5/0x0251. GetDlgItem ×141.
  - 880 far procedures.
  - DGROUP = seg19, initialised data to 0x17b5; INI key table at 0x401.
  - Entity array at 0x70a0, stride 0x15a. Animation bytes at seg4:0x216a–0x21a7.
  - Entry point seg5:0x1a is the normal Borland startup (InitTask).
  - Findfirst/findnext at seg5:0x352/0x364; `C:\*.*` at DGROUP:0x1260.
  - INSPACE loads `<Lang>_LANG.DLL` (seg10:0x6714), ISPACE.DLL with 23 ordinals (seg10:0x67c0) and CTL3D (seg2:0x19e0).
  - BOD: `01 00 01 00` at offset 21; word at offset 33 = IGRAPHIC #4000 ID.
  - SHP: 118×65 records + 10 pad bytes; the BOD index is at offset 33.
- **Needs re-verification:**
  - Ship +0x04/+0x06 = X/Y, +0x18 = active, +0x51 = owner.
  - 0x1df8 table = per-player ship arrays.
  - 0x1e34 = camera.
  - WndProc, tick and command-handler roles; the "196-case input switch".
  - The scoring formulas.
  - 0x9684 vs 0x9320 as the "game mode" global.
- **Refuted (record as pitfalls, not history):**
  - "No directory enumeration": the code scan looked for `mov $0x4e00,%ax`, not `mov $0x4e,%al`.
  - "seg1 = DGROUP".
  - "seg5 head overwritten by the stub".
  - §10a "entity +0x22 = owner": those are stack-local loop indices; +0x22 is the frame delay.
  - 0x1df8 "15 entries".
  - Ship field +0x74 (outside a 0x70 struct).
  - `ExtractIcon(...,-1)` "= own icon": it returns the count.
  - The nag string "@0x4b860" is a packed-file offset inside resource #5000/143.
  - The pointer tables "in seg01".
  - "All segments preload": segments 9–18 are not.
