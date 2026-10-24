# runtime-0001: the original under Wine 11.0 old WoW64 and under otvdm

- **Date:** 2026-10-24. **Host:** Rocky 9.8, Xvfb 1024×768×24, display `:114` (`ORACLE_ID=4`), fixture `basic` as `D:`, `--with-ini`.
- **Baseline:** Wine 11.0 new WoW64 (`/opt/wine-11.0`, Kron4ek `amd64-wow64`), digest [newgame-0001](newgame-0001.md), screenshots `work/oracle/newgame/`.
- **Bulk output (gitignored):** `work/runtime/`: `oldwow-{intro,skip,noshim}/`, `otvdm-{intro2,skip,notrace}/` (screenshots, `trace.log`, `digest.md`, `INSPACE.INI`), montages `m-*.png`, and the scenario copy `newgame-rt.sh`. That copy adds `RUNTIME=otvdm` and keeps its fixture under `work/runtime/`. With `RUNTIME=wine` it runs the same steps as `oracle/scenarios/newgame.sh`.

## Wine 11.0 old WoW64 (Kron4ek `amd64`: 64-bit plus 32-bit Unix libraries)
- **Source:** https://github.com/Kron4ek/Wine-Builds/releases/tag/11.0, asset `wine-11.0-amd64.tar.xz`, sha256 `632f2c8e9150841c26d277000e76b82c425cd4564bfb050780705b9d37b2567f`. It matches the release's `sha256sums.txt`. `wine --version`: `wine-11.0`.
- **Install:** `oracle/wine/install_kron4ek.sh amd64 /opt/wine-11.0-oldwow` (sha256 check, extraction, SELinux labels; its header lists the i686 host packages). With those packages, `ldd` on `lib/wine/i386-unix/*.so` lists only optional libraries as missing (gphoto2, sane, gstreamer, libav*, OpenCL, capi20).
- **Run** (this digest's runs used `work/runtime/newgame-rt.sh` with the same variables): `WINEPREFIX=~/.local/share/wineprefixes/innerspace-rt-oldwow ORACLE_ID=4 ORACLE_WINE=/opt/wine-11.0-oldwow SKIP_INTRO=0 oracle/scenarios/newgame.sh OUTDIR`
- **Results:**
  - Title screen, Restore Game and New Game dialogs: screenshots 04–10 are pixel-identical to the baseline. Title and Restore Game differ only in the logo animation frame. [ORACLE runtime-0001]
  - **Intro (`SKIP_INTRO=0`):** the desk scene runs to "PREPARE TO LAUNCH.. GOOD LUCK, YOUR NAME!" and the ship launch, then the game opens the Navigation window by itself, 20–24 s after New Game OK. No fault (`oldwow-intro`, `oldwow-noshim`). [ORACLE runtime-0001]
  - **Enter on `games`:** the game shows "WAVE 1 MISSION BRIEFING" (`D:\GAMES`, 3 ICONS, ALPHA.EXE, BRAVO.EXE, CHARLIE.EXE). ESC starts wave 1: ship, rocks, gate, a HUD with "CURRENT TARGET (MANUAL)", BRAVO.EXE and "WEAPON SYSTEM". The HUD clock keeps real time (0:03 at 4 s after ESC, 0:31 at 32 s), and Up moves the ship. No fault (`oldwow-skip`). [ORACLE runtime-0001]
  - **DS at the lead sites:** every GLOBALFREE returning to ISPACE.DLL seg7:0x4ce8 (4) and seg6:0x638c (2) has `ds=0427`. No relay line returns into an ISPACE code selector (03c7–041f) with `ds=03a7`. The baseline trace has 74 such lines. No `GLOBALLOCK(0000)`, and no `CREATECOMPATIBLEBITMAP(0000,…)` (the baseline makes 1 or 2 per run). [ORACLE runtime-0001]
  - **Tracing:** the same `+relay,+int21,+file,+timestamp` output and the same selectors as the baseline (INSPACE.EXE seg1=02ef … DGROUP 03a7, ISPACE.DLL seg1=03c7 … DGROUP 0427). `relay_digest.py` works unchanged. The newgame scenario writes 70 MB of trace. [ORACLE runtime-0001]
  - Both builds ship a byte-identical `krnl386.exe16` (sha256 `a03e56673f516bead8c0d9086aea1ca37c1d18a204ef20f5a173dea0b91947aa`). The layer that moves DS on new WoW64 is its exception resume path, per [fault-0001](fault-0001.md). [ORACLE runtime-0001]
  - **Navigation tree:** the walk and the "+" folder overlays match the fault-free run in [newgame-0002](newgame-0002.md); otvdm agrees. [ORACLE runtime-0001]

## otvdm 0.9.0 inside Wine 11.0 new WoW64
- **Source:** https://github.com/otya128/winevdm/releases/tag/v0.9.0 (latest stable), asset `otvdm-v0.9.0.zip`, sha256 `842b11aed5fa81f3e1d4272e0ee7d37f1a5a8f936de825309dda672835e16fd4`. The release publishes no checksums.
- **Install:** fresh prefix `innerspace-rt-otvdm` (`ORACLE_WINE=/opt/wine-11.0`). Unzip `otvdm-v0.9.0/` to `drive_c/otvdm`, then run `RUNTIME=otvdm work/runtime/newgame-rt.sh OUTDIR`. The script does three things:
  - It sets `WINEDLLOVERRIDES` to `n` for every file in `C:\otvdm\dll` except `gvm.dll`, `haxmvm.dll` and `whpxvm.dll`. Without this, Wine resolves otvdm's delay-loaded `krnl386.exe16` to its own builtin, and `otvdm.exe` dies with exception 0xc06d007f before opening a window. [ORACLE runtime-0001]
  - It copies `INSPACE.INI` into `C:\otvdm\WINDOWS`, plus `WAVEMIX.DLL`, `WAVEMIX.INI`, `CTL3D.DLL` and `INSPACE.SCR` into `C:\otvdm\WINDOWS\SYSTEM`. otvdm redirects the Windows directory there, and the game opens `C:\otvdm\WINDOWS\INSPACE.INI`. [ORACLE runtime-0001]
  - It starts the game with `wine C:\otvdm\otvdm.exe INSPACE.EXE` from `C:\INSPACE`.
- **Results:**
  - Title, dialogs, the full intro, the Navigation window, the briefing and wave 1 all run without a fault, at `SKIP_INTRO=0` and `SKIP_INTRO=1`. GLOBALFREE returns at seg7:0x4ce8 and seg6:0x638c keep ISPACE's DGROUP, there is no `GLOBALLOCK(0000)`, and there is no NULL-DC CreateCompatibleBitmap. [ORACLE runtime-0001]
  - **Speed:** otvdm runs 16-bit code on its software CPU emulator (`vm86.dll`). The hypervisor back-ends need Windows drivers [REF otvdm 0.9.0 otvdm.ini]. Untraced, `Calibrate` is 7116 (Wine: 13965–14906) and the wave clock keeps real time. With `+relay` on, `Calibrate` is 1294–1324 and the wave clock runs at half speed (0:12 after 24 s). [ORACLE runtime-0001]
  - **Tracing:** otvdm has its own 16-bit relay. It reads the same `WINEDEBUG` value and the same `RelayInclude`/`RelayExclude` keys. Its lines have the baseline format without timestamps, e.g. `0128:Call KERNEL.170: ALLOCCSTODSALIAS(11ff) ret=11ff:0168 ds=0000`. `relay_digest.py` parses them. [ORACLE runtime-0001]
  - **Selectors:** INSPACE.EXE: seg1=11ff, seg2=12cf … seg18=134f, DGROUP 1357. ISPACE.DLL: seg1=137f, seg2=13e7 … seg10=1427, DGROUP 142f. [ORACLE runtime-0001]
  - **Keep `+file` off:** otvdm writes its debug output one byte per `WriteFile`, so Wine's `+file` channel produced 2 GB of trace in 2 minutes. The game also ran so slowly that the title screen did not appear within the 20 s boot wait. [ORACLE runtime-0001]
  - `+int21` under otvdm is untested [OPEN].
- **Differences from Wine (both WoW64 modes):**
  - `SHELL.EXTRACTICON(…, 0xffff)` returns the icon count (ALPHA.EXE 1, ECHO.EXE 3). Wine returns 0 for all 25 such calls in the scenario (see [newgame-0001](newgame-0001.md), Wine divergences). [ORACLE runtime-0001]
    - The status line for `tools` reads "There are 4 icons in 2 files." (Wine: "There are 2 icons in 2 files.").
    - The briefing and the HUD draw the fixtures' own icons (red square, green diamond, blue cross). Wine draws a generic green "D:>" box for each.
  - **Dialogs are wider:** the game places Restore Game with `MOVEWINDOW` at 325,252, size 374×264 (Wine: 352,252, size 321×264), and the New Game controls sit farther apart. Glyph rasterisation also differs slightly. Which runtime matches Windows is [OPEN]. [ORACLE runtime-0001]
  - A screenshot between the intro and the Navigation window shows "ANALYSING DRIVE D:UNKN" in the game's green display font. No Wine screenshot has caught it. Whether Wine only shows it more briefly is [OPEN]. [ORACLE runtime-0001]

## Recommendation
- **Default oracle runtime: Wine 11.0 old WoW64 (`/opt/wine-11.0-oldwow`).** [DESIGN]
  - It passes both fault points.
  - It keeps the baseline's relay format, timestamps and selectors, and its dialog screenshots are pixel-identical to the baseline, so existing digests and `relay_digest.py` stay valid.
  - It runs at full speed while tracing.
  - `oracle/env.sh` defaults to its copy with the ExtractIcon count patch, `/opt/wine-11.0-oldwow-patched` ([wave-0001](wave-0001.md)).
- **otvdm: cross-check runtime only.** Use it where Wine's 16-bit layer departs from Win16 (ExtractIcon count). Its 16-bit layer derives from Wine's 16-bit code, so agreement between the two does not prove Windows behaviour. It needs the launch, DLL-override and WINDIR steps above. Its selectors differ, and its tracing slows the game. [DESIGN]
- **Wine 11.0 new WoW64:** keep it only for reproducing the DS fault. [DESIGN]
- Both candidates pass, so this run did not plan a DOSBox-X + Windows 3.1 setup. [DESIGN]

## Not covered
- Repeat runs for the early-exit case in newgame-0001 [OPEN].
