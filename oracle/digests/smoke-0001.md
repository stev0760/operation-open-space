# smoke-0001: original reaches the title screen and Restore Game dialog

- **Date:** 2026-10-23
- **Host:** Rocky 9.8, Wine 11.0 (Kron4ek `amd64-wow64`, new WoW64), Xvfb 1024×768×24
- **Prefix:** `~/.local/share/wineprefixes/innerspace`, laid out by `oracle/install_layout.py` from `original/game`
- **Bulk output (gitignored):** `work/oracle/smoke/` (screenshots 01–07, `relay.log`, 85 MB)

## Observations
1. **No settings on first launch.** With `C:\WINDOWS\INSPACE.INI` lacking the `[Inner Space]` keys `Path` and `LocalPath`, the game shows a MessageBox: "Some game settings are missing. Will attempt to regenerate INSPACE.INI. For best results, reinstall from installer."
2. **INI regeneration.** After OK, the game writes these keys to `C:\WINDOWS\INSPACE.INI`:
   - `Path=C:\INSPACE\`, `LocalPath=C:\INSPACE\`
   - `ShipPath=C:\INSPACE\SHIPS\`, `SndPath=C:\INSPACE\SOUNDS\`, `RacePath=C:\INSPACE\GAMES\`
   - `Dead=RETIRED`, `Games=GAMES`, `SoundSystem=0`, `Calibrate=713`, `MainShip=TRANSFORMER`
   The paths come from the module's location (two `GetModuleFileName` calls precede the INI reads).
3. **Startup window.** A full-screen window (1024×768 on this display) titled "Inner Space" shows the spinning SDi logo, then the title screen: "OPERATION: limited inner space™ / Standard Edition - V1.29.08 Aug 9/2000". The title screen also shows:
   - "For best results, change Windows to 256 color mode"
   - "You are on day 12086 of your demo trial period."
   - "Press 'R' to order the full game package"
   - "ESC to play - Alt-X to Exit"
4. **Restore Game dialog.** ESC opens it (315×232; fields GAME / PILOT / SHIP / SCORE, OK / Cancel, preview box) with "NEW GAME, STARTING AT WAVE 1." The main window title changes to "Inner Space - PAUSED..".
5. **Relay trace.** `+relay` logs 16-bit calls in the form `Call KERNEL.128: GETPRIVATEPROFILESTRING(...)`. Before the first MessageBox the game made one `SETTIMER` and one `MMSYSTEM.604 TIMEGETDEVCAPS` call. It made 39 `DOS3CALL`s, matching the 39 static call sites in seg5.

## Not working
- **The original `INSTALL.EXE` crashes under new-WoW64 Wine** after its first dialog: "Unhandled page fault on read access to FFFFFFFF at address 0000633D". Under the old-WoW64 oracle runtime it completes ([install-0001](install-0001.md)). The manual layout above lays out the unregistered game without it.
- **Wine 11.20 (Kron4ek `amd64-wow64`) segfaults in `wineboot`** on this host. It computes the host address-space limit as `0xffffffffffff0000` and crashes in `create_view`. Wine 11.0 computes `0x7fffffff0000` and works.
