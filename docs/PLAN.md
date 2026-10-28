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
| M2 Shell | **done** | Stage 4 done: the shell leftovers (ship preview and Order logo pixel-exact, the launch's frame table and step rule, the state-1 HUD band), a whole-codebase review and the acceptance comparison `m2-accept-0001` (both editions: every INI write and file write matches; dialogs and flow match; the remaining screen differences are [DESIGN] or [OPEN] items listed below). Stage 0 done: the RE round on start-up, exit, screen states, intro, dialogs, geometry and palette (`docs/re/shell.md`, 200 claims, 171 confirmed by independent verifiers, 44 fact tests). Stage 1 done: `src/shell/` (scheduler, state machine, command tables, geometry, window keys), `src/app/` (`inspace.exe`, both window modes, loop, timer, exit with INI write-back) and `src/settings/` (IniFile, GameSettings, regeneration), reviewed and fixed; runs headless under Wine (`work/shell/s1/notes.md`). Stage 2a done: the RE round on the logo strip, title picture, bitmap font, colour tables, title palette and credits (`docs/re/shell.md`, `docs/re/ispace.md`, `docs/re/palette-tables.md`; 87 claims confirmed, 11 fact tests). Stage 3d done: `src/install/` (the port installs from the original install files per ADR 0008: the installer's own dialogs, SZDD expansion, the INI keys, the personalization and registration records into its fresh copy; a sold-set install starts as Plus Edition), `src/desk/` (the desk and launch scene through the ordinal 32 renderer; three of five slots pixel-exact), the briefing's remaining pages and the load-screen greeting, the palette-message emulation (ADR 0007), the DRIVE list and Order serial rules; compared against install-0001, newgame-0002 and start-0001, reviewed and fixed. Stage 3a done: the RE round on the edition check, the install-time records, the dialog procedures and the briefing (111 claims, 78 confirmed, 33 corrected). Stage 3b done: `src/shell/edition` (the read-only edition check per ADR 0008; the registered install reads "Plus Edition / Reg User"), `src/settings/ini_location` (ADR 0004), `src/dialogs/` (Restore Game, New Game, New Demo, Exit, Benefits, Important Information, Plus Pack, Order, Save Game and the state-0 flow around them), `src/nav/` (the Navigation window as a stub), `src/briefing/` (state 3 and the hand-off to state 1), `src/formats/hlp` and `src/help/` (ADR 0005), compared with the registered and unregistered oracle (`oracle/digests/start-0001.md`) and fixed. See the stage 3 deviations below. Stage 2b done: `src/gfx/` (8-bit index surface, software palette and ported palette manager per ADR 0007, isolated GDI presenter with integer scaling and the DPI manifest per ADR 0006, bitmap font, graphic loading) and `src/intro/` (state 0 phases 0–5), reviewed, compared with the oracle (`oracle/digests/intro-0001.md`, `work/shell/s2/compare/report.md`) and fixed. See the stage 2 deviations below. ADRs 0004–0007 accepted; 0004 and 0005 wait for stage 3. |
| M7 Audio | **in progress** | Stage 3 done: 22 code-comment claims of the M7 implementers verified independently (15 confirmed or corrected into `docs/re/sound.md` and `shell.md`, fact tests in `re/tests/test_re_facts.py`); the exit-time music pause traced to the teardown's seg10:0x148, not to command 3, and ported; a whole-audio review (no HIGH, five MEDIUM fixed: missing custom WAV, empty `SndPath` for MIDI, `SoundSystem` −1 write-back, `waveOut` failure path, command 2 from the Navigation close); the first Audio Options oracle capture at WaveMix (`oracle/digests/sound-0002.md`: every control state and INI write matches the port after the Mix Channels selection fix; the 0→3 switch, `MixChannels=0`, held notes and play past the end on Wine answered). See the stage 3 open items below. Stage 2b done: the `opl3` backend (`src/audio/opl3_bank.*`, `opl3_driver.*`, `music_opl3.*`): Nuked OPL3 (`third_party/nuked-opl3/`, LGPL-2.1-or-later) under a port-written General MIDI voice driver with the Fat Man 2-operator bank (`third_party/fatman-2op/`, MIT), `IS_OPL3` on by default and set by the presets, the Wine default order `opl3`, `soundfont`, `mci`; the dual-mode channel rule (channels 1–10) for both synths; reviewed and fixed; headless title and main runs play the anthem bit-exact through `opl3` with the stage 2 command sequence and effects log; twelve-track renders under `work/audio/s2b/fix/render/`. See the stage 2b open items below. Stage 2 done: the port's own MIDI file player (`src/audio/midi_file.*`, `midi_player.*`: SMF 0/1, tempo map, silent section seeks, one end notify, pause and resume, software volume), the synth contract (`synth.h`), the `soundfont` backend over TinySoundFont (`third_party/tsf/`, MIT) rendering into the mixer, the `[Port] Music` and `[Port] SoundFont` keys with the Wine default (ADR 0011 decision 3); reviewed and fixed; a headless title run plays the anthem bit-exact in the dump at the formula's volume, pauses and resumes at the dialogs as the oracle's MCI calls do, and the `mci` call log is unchanged from stage 1; twelve-track reference renders under `work/audio/s2/render/`. Stage 1 done: `src/audio/` (the portable mixer and the effects engine with every rule of sound.md as pure functions, the `waveOut` output thread with the `IS_AUDIO_DUMP` render, the `mci` music backend with the track tables and notify handling, the `is::audio` contract), `src/dialogs/audio_options.*` (the Options and Audio Options dialogs with their fills, bars and INI writes; the already-in-use box), every sound and music call site rewired, focus pause and resume; reviewed and fixed; compared against `oracle/digests/sound-0001.md` (the ids match step by step on the title, dialogs, desk and exit path at one channel and at four; every audible play is heard at its time in the dump and nothing else is; pans match ordinal 8 at 1024×768; the MCI sequence matches command for command). See the stage 1 open items below. Stage 0 done: the RE round on the sound engine, the music, the sound catalogue and the Audio options (`docs/re/sound.md`, `re/artifacts/sounds.tsv`; 88 claims, 59 confirmed, 29 corrected, 0 refuted; the 32 voice lines transcribed with whisper.cpp). ADR 0011 accepted. |

**M2 stage 2 deviations and gaps** (details in the source comments and `work/shell/s2/compare/report.md`):
- **Timing matches the oracle phase for phase:** logo steps, "presents.. ", the black second, the 105-frame dissolve, the credits, both text pages and the key page have the oracle's durations. The bitmap-font texts, the copyright, the Press 'R' box, the SD_LOGO letters and the logo cells are pixel-exact in shape.
- **Not ported in stage 2:** the title's 45-second scripted scene (`seg15:0x7792`; the port's title runs its 51 500 ms deadline, about 2.5 s longer than the oracle), the demo start after the key page (`seg15:0x5a64`; the port restarts state 0 at the logo) and the desk and launch scene (stage 3d). Sounds and music are logged once per id.
- **Colours follow the 8-bit model of ADR 0007,** so they differ from the oracle's 24-bpp captures by design: plain-RGB text lands on the nearest static colour (the page heading, the key rows), PALETTERGB on the nearest palette entry, brushes are not dithered, and text on the 8-bit surface is not antialiased. Open: the credits logo (IGRAPHIC #4000 340) comes out purple where the oracle shows blues, and the logo ring greys and gold are off by a few units; whether the original writes a 4-bpp graphic's table into palette entries or matches it is an RE question for the next RE round (`docs/re/shell.md` Open questions, ordinal 1 mode 3 against mode 0), as is ordinal 12's smoothed title emblem (a plain 2x stand-in with 15 colours against the oracle's 43).
- **Renderer design points** [DESIGN]: every index bitmap carries one fixed, duplicate-free colour table (the 20 statics plus a synthetic ramp), so only the presenter's lookup table carries the palette and a blit between index bitmaps copies indices; colours reach GDI as `DIBINDEX(n)` through one resolver; command 0 publishes all 256 entries at once; the presenter skips unchanged frames; WM_ERASEBKGND in state 0 blacks the logical cover of the update region.
- **Doc corrections waiting for a verifier** (the code follows the bytes, marked [OPEN] in the source): the entry-15 test of `ISPACE.DLL seg5:0x26a2` (red counts only when DGROUP:0x188 ≠ 0; green and blue always), ordinal 14's product truncated to a signed word (`ISPACE.DLL seg5:0x5186`), the parity test before sound 0x14 in the credits typing (`seg15:0x7e55`), and the phase-4 PatBlt address (`seg15:0x6a27`, height H/2 + 0x95, not `seg15:0x6956`).
- **Not verified on real Windows:** text rendering and mono-to-index blits on the 8-bpp DIB section, and per-monitor DPI changes.

**M2 close-out, what stays open for later milestones** (`oracle/digests/m2-accept-0001.md`):
- **Windows-only checks** that Wine cannot give: the timer resolution (ADR 0001 decision 11), DPI scaling of run-time fonts and children (ADR 0006 decision 4), bold and Arial metrics, the caption while another program is active. The M9 Windows matrix covers them.
- **The wave is black:** the HUD band and scrollbar are drawn at state-1 entry; the IS_INFO panels, the wave objects and the state-1 frame are M3. DEMO and PLAY (and the second instance's forwarding, ADR 0010) are M3.
- **Title:** the emblem is a stand-in and the 45-second scripted scene is M3, so the title runs about 4 s longer than the oracle's.
- **"Analysing Drive"** was not seen in any port shot; whether the stub Navigation window erases it within a frame is unconfirmed.
- **Desk:** the launch pad drawer, the rise-strip mask and the ship's smoothing and flame (ordinal 12's 2x picture) stay approximate; the strip faces and the monitor neck shading likewise.
- **Briefing:** rock, virus and ship images other than the S logo are placeholders; the statistics page's numbers wait for M3.
- **8-bit colours:** the depth-8 oracle capture matched the title text and the briefing's title bar exactly; the greeting's olive lines are the nearest-static-colour rule of ADR 0007.

**M2 stage 3 deviations and gaps** (details in the source comments and the agents' notes under `work/shell/s3/`; the port's comparison runs are `work/shell/s3/compare/`):
- **Edition:** the check runs read-only on the NT branch from the asset directory's `INSPACE.EXE` and the `[Registration]` keys (ADR 0008); no CD edition; a missing `#5000 143` gives an empty name. Portable mode is off in the asset directory (ADR 0004 decision 2).
- **Dialogs:** the LevClass ship previews and the Order logo match the oracle to the pixel (stage 4); the Order serial reads "xxxxx" for a zero `IGRAPHIC.DLL #5500 110` record; Register is disabled (ADR 0008 decision 3); the save writer (`seg6:0x885c`, `seg6:0x99b6`) is logged (M4); the Win32 3D client edge is removed from the edits so they keep the Win16 1-pixel border.
- **Navigation:** a stub (`src/nav/`): the window, Enter, Help and the INI keys; the tree walk `seg6:0x37ca` and the other children are M3. Enter hands only the drive root to the briefing (`DGROUP:0x38c8`'s writer is undocumented).
- **Installer (stage 3d):** dialogs centred and bordered as the original's; the set is copied to `<INI dir>\INSTALL\` first; destination-8 files go to the game directory; a target holding game modules is refused (ADR 0008). Not reproduced: the splash picture and its text, the install sounds and the percent format, the DeInstall/ReInstall and backup paths (install.md Open questions). The 64-bit free-space figure replaces the 16-bit one.
- **Desk scene (stage 3d):** slots 0, 1 and 3 match newgame-0002 to the pixel except the #4000 290 logo's palette colours; slot 2's strip faces and the monitor neck are approximations (`ISPACE.DLL seg8:0x1fe6`, `seg8:0x3874`, `seg8:0x5632` [OPEN]); the launch pad and ship sprite are placeholders (`seg15:0x9e6`, `seg15:0x2a08`, `seg15:0x3bfa` [OPEN]) and the port's launch ends about 1.3 s earlier than the oracle's, with the ship vanishing at the monitor neck. Needs frame-level oracle shots before a fix.
- **Briefing (stage 3d):** the whole cycle runs (icons, ambulance, reality check) with the pages' order and holds matching start-0001; the typers and frame counts run at ADR 0009's rates (cycle 27.1 s against 24.8 s). Rock, virus and ship images other than the S logo are placeholders [OPEN]; the statistics page's numbers wait for M3; Space during the ambulance growth is unconfirmed. State 1 stays black (M3).
- **Help:** WinHelp becomes an HTML export under the INI directory, opened with ShellExecute (ADR 0005); macro links and external jumps render as plain spans; `IS_HELP` topic 4 for F1 in the wave waits for the HUD (M8).
- **Palette:** the port emulates the original's `WM_QUERYNEWPALETTE` handler on every activation of the main window (ADR 0007 decision 5, `src/gfx/palette_message`); the blackouts around New Game and after Cancel match start-0001.
- **Wine-only differences to check on Windows:** bold and Arial metrics under Win32 Wine (labels up to 7 px narrower), and the caption while Notepad is active (the port's PAUSED is the Win16 GetActiveWindow result).
- **Verifier queue for 3c:** D's reading of `seg9:0x27b3` (no LOADING GRAPHICS from `seg9:0x5cca`), B's Navigation details from `seg6:0x1f22`, `seg6:0x23d4` and `seg6:0x2ebc`, the `IGRAPHIC.DLL #5500 110` serial layout (count at +4, dword at +6), and `SetCurrentDirectory` as the counterpart of `_chdrive` in the DRIVE list (`ISPACE.DLL seg10:0x2104`).

**Phase 0 deviations:**
- **Wine 11.0 stable** (Kron4ek `amd64-wow64`) instead of 11.20. On this host 11.20 segfaults in `wineboot` with a bad address-space limit.
- **Oracle runtime is old-WoW64 Wine 11.0** (Kron4ek `amd64`). New WoW64 loses ISPACE.DLL's DS after `GlobalFree` and faults (`oracle/digests/fault-0001.md`); old WoW64 runs the game into wave 1 (`oracle/digests/runtime-0001.md`).
- **Installer:** `INSTALL.EXE` crashes under new-WoW64 Wine 11.0 and completes under the old-WoW64 oracle runtime (`oracle/scenarios/install.sh`, `install-0001`). `oracle/install_layout.py` lays out the unregistered game without it.
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
  - **Decision of 2026-10-25 (ADR 0008):** the port installs the game from the original install files itself, so that no legacy Windows is needed to register; the sold download registers and the free download starts a trial, as the author's installers do.
  - **The user bought a copy on 2026-10-25.** The download is the v1.29.08 shipdisk build, byte for byte; the registration is the installer's personalization step, which rewrites two INSPACE.EXE resources and writes three WIN.INI/SYSTEM.INI values (`oracle/digests/install-0001.md`). The oracle now observes the registered game through `oracle/scenarios/install.sh` (prefix `innerspace9`), using the user's own install and never a patch. The written values and the rewritten INSPACE.EXE stay local like `original/` and are never committed; the port reads the user's installed files and INI and never writes them.

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

## Target repo layout (the repository root)
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
| M3 | World (detailed plan below), **in progress** | Filesystem walk per the original's rules, with a modern-safety ADR. Navigation, wave generation, ExtractIconEx at 32×32, rocks and gates, flight model, status panel, radar. |
| M4 | Core loop | Capture, fuel, damage and auto-repair, weapons, ambulance economy, scoring, waves, save/load (format ADR). |
| M5 | Ships & AI | Teams and attitudes, commands, defenders, viruses, hazards, enforcers, laws and Hall of Justice, Strategy mode, perspective shift. |
| M6 | Specials | Races and duels, Inner Demon and Noble Weapons, demo, Special Times of Day. |
| M7 | Audio | Own mixer replacing WAVEMIX (detailed plan below); music through MCI, a SoundFont synth or OPL3 (ADR 0011). |
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
- **ADR 0009 CPU-paced animation:** accepted; stage 3d (the briefing's typers and frame counts at their designed rates, not the oracle host's).
- **ADR 0008 Registration and the installer:** accepted; stage 3 (the read-only edition check and the `[Registration]` keys) and a new installer stage (the port installs the game from the original install files, which is how a copy becomes registered).

**Stages** (one Workflow run per stage; parallel implementers own disjoint files):
0. **RE round** (done): `docs/re/shell.md`; checklist RE statuses updated.
1. **Skeleton** (2 implementers, 1 reviewer): `src/shell/`: WinMain, the seven window classes with their styles, the start-up order and failure messages of shell.md, the main window in both modes with the Show rectangle rules, the message loop with the frame scheduler of ADR 0001, the state machine (10 states, frame hooks, requests through the state-change step), the 1000 ms timer with the one-shot, focus watch, pause and resume, WM_SYSCOMMAND and WM_COMMAND tables (menus 101/102/POPUPMENU and MAINACCEL from the asset store, commands posted as key events), the exit sequence and INI write-back, the private messages. `src/settings/`: the INI layer over the W profile functions with every key, default and regeneration rule of formats.md, behind a path. Portable pieces (scheduler, state machine, settings parsing) carry doctest tests on the `host` preset; the window runs under Wine in a headless check that records the class, style, size and title.
2. **Renderer, intro and title**, in two runs:
   - **2a, RE round** (done; 4 analysts, 4 verifiers, 1 doc writer): the facts the renderer and the intro still lack: the ordinal 2 id −1000 logo strip and ordinals 1, 3 and 12 as the title uses them; ordinal 33 (the bitmap font) completely; the run-time colour tables ISPACE.DLL DGROUP:0x117c and DGROUP:0x1cdc and the exact title palette; the credits phase and the title and logo font fields. Landed in `docs/re/shell.md` and `docs/re/ispace.md`, pinned in `re/tests/test_re_facts.py`.
   - **2b, implementation** (done; 2 implementers in sequence, 1 reviewer, 1 oracle runner, 1 comparison runner, 1 fixer): `src/gfx/`: the 8-bit index surface, the software palette and the ported palette manager (ADR 0007), the isolated GDI presenter with integer scaling (ADR 0006), the bitmap font (ordinal 33) and the graphic loading the intro needs. `src/intro/`: state 0 phases 0–5 per shell.md (logo, black second, title with its texts, picture reveal and trial line, credits, the two text pages, Basic Key Commands), the state-0 key handler for Space and ESC (ESC opens the game-start flow of stage 3b) and the repaint rules. Oracle comparison: screenshots of each phase against `smoke-0001` and `depth8-0002`.
   - **Left to stage 3** with the dialogs: the desk and launch scene (phase 6, `seg15:0xcb0`, `seg15:0x10be`, the typing `seg15:0x1584` and the scene-100 render through ordinal 32) and "Analysing Drive", which need their own RE round, and the title's 45-second scripted scene (wave objects, M3), which stage 2 documents as a gap.
3. **Dialogs, briefing, exit and help**, in three runs:
   - **3a, RE round** (done): the registration check and the install-time records (`docs/re/shell.md`, `docs/re/formats.md`), the dialog procedures and their fills, the briefing's 19 phases, the desk scene's structure (a procedural render of IGRAPHIC.DLL #6600 100 by ordinal 32; its drawers wait for 3c), and the stage-2 leftovers (the credits-logo purple is the faithful 8-bpp result; four shell.md corrections).
   - **3b, implementation** (done; 4 implementers, 1 oracle runner, 1 comparison runner, 1 reviewer, 1 fixer): below.
   - **3c, RE round** (done; 5 analysts, 5 verifiers, 1 doc writer; 109 claims, 71 confirmed, 38 corrected, 0 refuted; `docs/re/install.md` is new): `INSTALL.EXE` (its install-set validation, the `INSTALL.INI` groups, its dialogs, the ordinal 24 command it issues after personalization; the shareware `SETUP.EXE` as the contrast, which writes the trial start instead of the `#5500 110` record), ordinal 32's drawers and projection for the desk scene, the LevClass ship preview (`ISPACE.DLL seg5:0x0`, `seg10:0x309c`), and the briefing phases 4-9 and 11.
   - **3d, the installer, the desk scene and the 3c fixes** (done; 4 implementers, 1 comparison runner, 1 reviewer, 1 fixer; ADR 0009 for the CPU-paced loops): the port's install step per ADR 0008 (`src/install/`: validation, `INSTALL.INI`, the dialogs 300/1922/1774/135, SZDD expansion, the INI keys, the registration records with the NT-branch values into the port's own copy); the desk scene's ordinal 32 renderer (projection and slots per ispace.md; the strip-face and cylinder shades stay approximate until their formulas are read); the load screen's greeting in place of LOADING GRAPHICS; the palette-message emulation of ADR 0007 decision 5; the briefing's missing phases (slide-ins, phase 7, phase 11, icon labels, CRT flash) with the port's own typewriter rate as a [DESIGN] deviation; the DRIVE list's `Unknown_HD` label and `_chdrive` rule; the Plus weapon table where M3 reaches it. Still to read first: the LevClass preview and the Order logo (ISPACE.DLL seg10:0x309c, seg5:0x0, seg10:0x2f4), the launch leftovers, the first state-1 frames.
   The 3b implementation: the registration check (seg10:0x885e and ordinal 24, now verifiable against the registered oracle: the Plus title with "Reg User", no trial line), the ordinal 10 dialogs the shell reaches (Restore Game, New Game, New Demo, Exit, Order, Benefits, Important Information, Plus Pack) with their procedures' logic and INI writes; the Navigation window as a stub that M3 fills (creation, Enter, the Help button); state 3 briefing and the transition to state 1; states 4 and 5 (GAME OVER, high scores) as far as the shell reaches them; the exit dialogs and `PlusCount`; help per ADR 0005 (`src/formats/hlp`, HTML export, context map).
4. **Verify** (done; `oracle/digests/m2-accept-0001.md`; ADRs 0001, 0004, 0006 amended, ADR 0010 new): independent review of `src/`; an oracle digest comparing the port's run from launch to the briefing and to exit with the original's (screens, window geometry, INI writes); fixes; the orchestrator updates the checklist Port statuses and commits.

**Verification:**
- `ctest` green on `host`, `mingw-x64` and `mingw-i686`; `re/tests` and `oracle/tests` green.
- The port's window class, style, size and mode switches match shell.md and the oracle.
- Title and intro screenshots match the oracle's phase for phase; the demo-day line and greeting follow the rules of shell.md.
- A run from launch through New Game to the briefing and Exit writes the same INI keys as the oracle's run.
- Checklist: the WIN, MODE, SYS and INI rows that shell.md covers move to Port `done` or `verified`.

**Out of scope:** the Navigation walk (M3), the wave (M3, M4), sound (M7), the Online Help window and watch windows (M8), MSVC.

## M7 — Audio (detailed plan)
**Goal:** the port plays every sound effect and the music as the original does, through its own mixer and the music backends of ADR 0011, with the Audio and Options dialogs working. M7 is done when a run from launch through the briefing to exit plays the same ids at the same moments as the oracle's trace (the port's sound log against the original's ordinal 9 calls), the volumes follow sound.md's formulas, and the music plays through `mci` on Windows and through `soundfont` under Wine.

**Source of truth:** `docs/re/sound.md`, with `re/artifacts/sounds.tsv` for the ids and `docs/re/ispace.md` for the ordinal map.

**Stages** (one Workflow run per stage):
0. **RE round** (done): the engine (modes, device test, session, the 82-slot flag table, the player, ordinal 8's volumes and pans, the wrappers), the music (twelve tracks as sections of four files, the MCI sequence, notifications, loops and chaining, control codes, the Midi Mapper box, volume), the catalogue (every id with module, resource, length, transcript, meaning and call sites) and the Audio Options and Options dialogs.
1. **Mixer, effects and MCI music** (done; implementers, reviewer, oracle comparison, fixer): `src/audio/`: the portable mixer (channels, queues, priorities, holds, loops, variants, the volume and pan formulas as pure functions) and its `waveOut` output; the engine's commands 0-11 behind the port's existing sound log calls (every `seg10:0x744c`, `seg10:0x754a`, `seg10:0x7b48` and `seg10:0x7480` site plays); the `mci` music backend with the track tables and the notify handling; focus pause and resume; the custom-WAV path; the Audio Options and Options dialogs with their INI writes. Comparison: the port's id log against the oracle's traced ordinal 9 calls through the shell path, and a recorded render of the title, desk and briefing sounds listened to by the user.
2. **Synth backends** (done; implementers, reviewer, render-and-listen runner, fixer): the MIDI file player, `soundfont` through TinySoundFont, the `[Port] Music` and `[Port] SoundFont` keys, the render-to-file check of each track's first seconds. `opl3` moved to stage 2b.
2b. **OPL3** (done; 2 implementers, reviewer, render-and-listen runner, fixer): Nuked OPL3 vendored (LGPL-2.1-or-later), a General MIDI voice driver written for the port (2-operator voices, channel allocation, drum channel, the GM controller subset the files use), the Fat Man 2-operator bank embedded under its MIT licence, `IS_OPL3` on by default; the twelve-track render check; listening files under `work/audio/s2b/listen/`.
3. **Verify** (done; claim collector, 2 verifiers, doc writer, reviewer, oracle runner, fixer): independent review; verifiers for the [BYTES] claims the M7 implementers made; the oracle capture of Audio Options at capability 3 (`sound-0002`); the checklist rows updated.

**Stage 3 open items** (each tagged at its site in the code or in ADR 0011):
- Docs owed to a verifier before they land in `docs/re/`: INSPACE calls ordinal 7 command 2 only through seg10:0x7db8, from seg6:0x2fd1 (Navigation close, when DGROUP:0x49e = 0) and seg9:0x6cd1 (seg9:0x6cc4, the wave start, M3); Audio OK compares the new count with DGROUP:0x219e [BYTES ISPACE.DLL seg9:0x3547 seg9:0x354a].
- `src/desk/desk.cpp` plays 0x4c before the 1.43 s hold on a reading shell.md refutes (start-0001 ran with `SoundSystem=0`, so 0x4c never played there); where 0x4c sounds in that interval needs a `SoundSystem` ≥ 1 run with frame captures (shell.md [OPEN]).
- Windows' own mciseq: held notes across a seek and `MCI_PLAY` past the end (Wine answered in `sound-0002`); the mapper volume question; the SoundFont and OPL3 synths on a real `waveOut` thread. All wait for a real Windows run.
- Wine's mciseq reports wrong file lengths and plays track 11 in a 48 ms burst, so oracle recordings of tracks 5, 7, 8 and 11 do not reflect the game (`sound-0002`); the sound-0001 track-7 notify timing question stays open.
- WaveMix's loop-word and `WMIX_CLEARQUEUE` semantics ([OPEN] in sound.md; the port follows the header).
- Test gaps the reviewer listed: admission rules 2–6 and 8–10, ordinal 8's loudness rule, commands 3 and 4, the seg10:0x754a modes 1–5, the SND-12 effects-volume-0 loop exception, Options Cancel and the in-use box.
- The music volume is computed from Settings in `config_from` rather than from the engine; pass the engine's value once command 10 (`SaverVolume`) gets a caller (M8).
- A port-own master volume (below, stage 2b) is a small follow-on with its own ADR.

**Stage 2b open items** (each tagged at its site in the code or in ADR 0011):
- The title mix with the 0x22 bursts over the OPL3 music peaks within 0.1 dB of full scale; a louder burst would clip in the mixer's sum (ADR 0011 decision 1). The levels themselves are set (OPL3 at unity, SoundFont at −16 dB, judged by ear).
- Which level of the dual-mode files the author's 1994 set-up sounded ([OPEN] in sound.md); the synths play channels 1–10, `mci` plays whatever the system sequencer does.
- The Windows 9x volume model (bank volume model 4) is not followed; the driver's 40·log10 law is a GM convention.
- Release packaging for the statically linked LGPL Nuked OPL3 (the LGPL text and source access with every binary).
- Nuked OPL3 resamples with linear interpolation and no low-pass filter, so 11025 Hz output carries aliasing; not measured.
- A port-own master volume for the whole output stream (the original left the device volume to Windows' mixer; the port scales music and effects in software, so the Audio Options volume and FX/Music balance are the only controls under Wine): a modernisation for a later stage, with an ADR first (user request, 2026-10-27).

**Stage 2 open items** (each tagged at its site in the code or in ADR 0011):
- The comparison script reports the bubbles loop 0x6f as 5.7 s against 2.0 s expected after the load-screen change; probably the neighbouring plays merging into its window, not checked.

**Stage 1 open items** (each tagged at its site in the code):
- The briefing's positional sound sites at seg17:0x600 (0x13 at seg17:0x88f, 0x15 at seg17:0xab7 and seg17:0xb9c): the oracle plays 0x13 three times on alternate page opens and 0x15 four times at some page ends; the port plays one of each, and its page cycle differs (briefing module).
- The wave start's sounds (0x27 ×2, 0x65) and the track-1 open at seg9:0x65de, the World source for modes 1 to 3, `seg10:0x7944` (hit sounds), `seg10:0x7d58` (position only, intro steps 3–4), command 9 and command 10 (`SaverVolume`, M8): M3/M4.
- The intro's sounds come 0.4 s before the oracle's and the 0x6e burst runs past the music open (intro timing, not audio).
- Register's 0x0d (ADR 0008 decision 3); `INSTALL.EXE`'s own 0x4d still only logs; the Display and Joystick sub-dialogs log a todo; the gate on ISPACE.DLL DGROUP:0x2136 that disables ten Options controls [OPEN ISPACE.DLL seg9:0x1824].
- The linear volume words (ADR 0011 decision 2).
- WaveMix's own arithmetic, its `wLoops` meaning (taken as L + 1 plays), completions on replaced or flushed waves, and Wine's late `MCI_NOTIFY_SUCCESSFUL` ([OPEN] in sound.md and sound-0001).
- Code quality: the `is::sound` wrappers live in `src/dialogs/audio_options.h`; the track-table test repeats sound.md's constants instead of reading ISPACE.DLL DGROUP:0xaa8.

**Out of scope:** the wave's own sound events (M4 plays them through the same engine), the screensaver's `SaverVolume` path (M8), Ship Factory hit-sound names (M8).

## M3 — World (detailed plan)
**Goal:** the port fills the Navigation window from the player's own disk as the original does, builds the wave from the chosen directory (icons as objects, rocks, gates), flies the player's ship with the original's physics and controls, and draws the wave: the scrolling play area, the sprites, the status panels, the target and weapon displays and the radar. M3 is done when a run from Navigation Enter into wave 1 matches the oracle's `wave-0001` screens step for step (HUD, sprites, headings and positions within the frame tolerance of ADR 0001) and the Navigation window lists the same files, icons and counts for the fixture drive.

**Source of truth:** `docs/re/world.md` (written by stage 0) for the Navigation walk, the wave set-up, the flight model and the drawing; `docs/re/structs.md` for the object pools and records; `docs/re/globals.md` for the tick; `oracle/digests/wave-0001.md` and `newgame-0002.md` for the running original.

**ADRs:** ADR 0012 (accepted): the filesystem walk and the icons (Win32 find calls where the original uses DOS ones, untouched network and no-media drives, the 100-byte path bound, reparse points never entered, 8.3 aliases, the port's own icon reader at 32×32 on black). ADR 0013 (accepted): the wave's drawing and input (the surface is the window, self-erasing draws kept, no second buffer; `IS_INFO` panels drawn on the surface, the scrollbar a real control; a key latch with the original's masks at the polled sites; the 0 … 8 cycle and the 16-bit widths kept exactly).

**Stages** (one Workflow run per stage):
0. **RE round** (done; 5 analysts, 5 verifiers, 3 doc writers, 1 doc reviewer; 207 claims, 150 confirmed, 56 corrected, 1 unclear, 0 refuted; `docs/re/nav.md`, `docs/re/world.md`, 31 fact tests): the Navigation window (the tree walk seg6:0x37ca, the directory and file lists, the icon count and the status line, the race and duel boxes, Enter's hand-over to the wave set-up, the demo's auto-navigation seg6:0x1e68, "Analysing Drive"); the wave set-up and the world (seg9:0x60bc, the object pools filled from the directory's files, rocks and gates, world extent and coordinates, the entry and exit gates, the world update seg8:0x3aec, seg8:0x3fbe and seg7:0x375a); the flight model and input (the player's ship record, thrust, turn and drift, the state-1 input router seg11:0x6aa8, the view scrolling, pause); the wave's drawing (the play-area frame and border, sprites, the HUD repaint seg11:0x15b3, the status panels, the `IS_INFO` target and weapon displays, the scrollbar); the radar and the icons (ExtractIcon at index −1 and 32×32, the icon column, the direction mark, the radar screen's rules). Out of the round: capture, fuel, damage and scoring (M4), other ships' behaviour (M5), races and duels beyond the Navigation boxes (M6).
1. **Navigation and the filesystem walk** (done; 4 implementers, reviewer, oracle comparison, fixer): `src/nav/` (fs, tree, files, boxes, enter, pilot, the window's owner-drawn lists, preview and status) and `src/formats/pe` + `icon_source` per nav.md and ADR 0012. On the fixture drive the port's Navigation window matches `newgame-0002` shots 12 to 24 with 0 differing pixels in the tree, file pane, buttons, preview and boxes (the status band differs only by font smoothing and the icon counts of decision 5), the 112 filesystem calls match one for one apart from the 9 `ExtractIcon −1` counts, and the INI after Enter matches apart from `Calibrate`.
   **Stage 1 open items:** NAV-17 (demo directory limit) is not implemented; NAV-19's records live in memory only (`Host::records` unset, lost at exit) until the saved game lands; the briefing's `icon_files` enumeration in `src/briefing/hooks.cpp` still runs outside the walker's bounds and log (stage 2); `NewGame::system_drives` has no log sink, so the seg10:0x2104 drive lines are not in the game log; plain `[DESIGN]` tags without an ADR remain at tree.cpp (LB_SETCURSEL −1), nav.cpp (row −1 as 0, WM_NCDESTROY), fs.cpp, enter.cpp (cut_suffix), pilot.cpp:47 and boxes_win32.cpp:47; the oracle should be rerun on the patched runtime for a count-for-count reference. **nav.md corrections owed to a verifier** (the implementers' readings, not yet in docs): the locked built-in status is the first string + separator + 2323 and the complete status is 765 + ". " + 3029; LB_FINDSTRING(−1) for later path components; seg6:0x4748's find-next after a failed find-first; the WM_DRAWITEM dispatch; WM_SETREDRAW in seg6:0x5318 and seg6:0x511c; the combo strings 2145–2169; WM_SETFONT with lParam 0 (a correction to shell.md item 11); DGROUP:0x94b8; the `%?` directive of seg10:0xa332 (idx = flag > 1 ? 1 : flag, so files.h can move from [OPEN] to [BYTES]); kMinTrack 560×350 from seg6:0x1f22 (nav_rules.h).
2. **Wave set-up, flight and drawing** (implementers, reviewer, oracle comparison, fixer): `src/world/` (pools, records, generation, the update loop, the flight model), `src/wave/` or the renderer's wave frame (sprites, HUD, panels, displays, radar), the state-1 input; compared against `wave-0001` from the wave start through the flying and targeting steps.
3. **Verify:** independent review, verifiers for the implementers' [BYTES] claims, the checklist NAV, WAVE and HUD rows.

**Out of scope:** capture, the ambulance economy, weapons and damage (M4), ship AI (M5), races and duels (M6), the wave's sounds (M7's engine plays them when M4 wires the sites), MSVC.

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
