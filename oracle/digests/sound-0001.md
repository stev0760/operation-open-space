# sound-0001: the original's sound and music calls on the shell path, one channel and WaveMix

- **Date:** 2026-10-26. **Host:** Arch Linux, Wine 11.0 old WoW64 with the ExtractIcon patch (`~/opt/wine-11.0-oldwow-patched`, the `oracle/env.sh` default), Xvfb 1024×768×24, PulseAudio.
- **Prefix:** `work/wine/oracle-35` (`ORACLE_ID=35`, display `:145`), made fresh for each run by `scenario_prefix`: game layout in `C:\INSPACE`, `WAVEMIX.DLL` and `WAVEMIX.INI` in `C:\windows` (INSTALL.INI destination 8), the MIDI files and both FX DLLs in `C:\INSPACE\SOUNDS`, `SndPath=C:\INSPACE\SOUNDS\`, fixture `basic` as `D:`, no `Z:`. Unregistered. The registered prefix took no part.
- **Audio device:** Wine's PulseAudio driver with `PULSE_SINK` set to a null sink, so nothing was heard. `waveOutGetNumDevs` returns 4. There is one MIDI output, which plays to the ALSA "Midi Through" port. [ORACLE sound-0001]
- **Trace:** the default relay profile (`oracle/trace.sh`). The `sound-idle2` run also adds `+mci,+mcimidi`.
- **Runs:**

  | Set | `INI_SET` | Runs | Output (gitignored) |
  |---|---|---|---|
  | one channel | `SoundSystem=1 MixChannels=1` | `main`, `briefesc`, `title` of `oracle/scenarios/start.sh` | `work/oracle/sound-ss1/` |
  | WaveMix | `SoundSystem=3 MixChannels=4` | the same three | `work/oracle/sound-ss3/` |
  | idle | `SoundSystem=1 MixChannels=1` | launch, 220 s (230 s) at the title without input, Alt+X, Exit, #799 Exit | `work/oracle/sound-idle1/`, `work/oracle/sound-idle2/` |

- **Reproduce:** `EDITION=unreg ORACLE_ID=35 WINEPREFIX=$PWD/work/wine/oracle-35 PULSE_SINK=<null sink> RUNS="main briefesc title" INI_SET="SoundSystem=1 MixChannels=1" oracle/scenarios/start.sh work/oracle/sound-ss1`, and the same with `SoundSystem=3 MixChannels=4` into `work/oracle/sound-ss3`. The idle runs and the call listings come from scratch scripts kept in `work/oracle/sound-0001-tools/`: `idle.sh`, plus `sound_calls.py`, which writes `sound_calls.tsv` beside each trace. The scripts are not in the repository.
- **Times** count seconds from the trace's first line, which is within 0.1 s of `wine INSPACE.EXE`. Steps are matched to the trace with a 0.15 s tolerance, because `steps.txt` logs each key or click just after it is sent. The three runs of a set agree to within 0.4 s up to their first difference in input.

## Naming the sound of a call
- **One channel:** each `SNDPLAYSOUND(ptr, flags)` passes the locked image of a slot. The trace shows `FINDRESOURCE(module, id, 100)`, `LOADRESOURCE` and `LOCKRESOURCE`. Their results map the pointer's selector to a module and a resource id, and the slot is resource − 100 ([sound.md](../../docs/re/sound.md#slots)). The slot column below is exact. The id column maps a slot to the id that INSPACE passed. It is exact when only one id reaches that slot with those flags. [ORACLE sound-0001]
- **WaveMix:** `WaveMixPlay` is native code, so a play shows only as the device calls that WAVEMIX.DLL makes. Each first play of a deferred slot shows as an `MMIOOPEN` right after that slot's `LOCKRESOURCE`, which names the slot. Each later play shows only as one `WAVEOUTPAUSE` … `WAVEOUTRESTART` burst. [ORACLE sound-0001]
- `SNDPLAYSOUND(NULL, 0)` is the stop routine ISPACE.DLL seg9:0x1174. Ids 0x69 and 0x6e both reach it with one channel. So do the admission's "stop all" rules and the session set-up. A stop row therefore does not name its id.

## Start-up (all runs)
| t | Call | Ret | Meaning |
|---|---|---|---|
| 0.79 | `MCISENDCOMMAND(0, MCI_PAUSE 0x809, 0)` | 0x101 | the −1 of the state-0 entry seg15:0x57b5, sent to device 0 before any open; `MCIERR_INVALID_DEVICE_ID` |
| 0.80 | `WAVEOUTGETNUMDEVS` | 4 | device test ISPACE.DLL seg9:0x2416 |
| 0.85 | `WAVEOUTGETDEVCAPS(0, …, 0x30)`, `MIDIOUTGETDEVCAPS(0, …, 0x32)` | 0, 0 | |
| 0.86 | `MIDIOUTGETVOLUME(1, DGROUP:0x6b0)`, `WAVEOUTGETVOLUME(0, DGROUP:0x6ac)` | 5, 5 | saving the system volumes as SND_FX.DLL loads; 5 is `MMSYSERR_INVALHANDLE` |
| 0.87 | `WAVEOUTGETDEVCAPS` again; one channel: `SNDPLAYSOUND(NULL, 0)` ×3 | 0; 1 | session set-up |
| 2.79 | `MIDIOUTGETVOLUME(1)`, `WAVEOUTGETVOLUME(0)`, `MIDIOUTSETVOLUME(1, …)`, `WAVEOUTSETVOLUME(0, …)` | 5 each | command 4 at the first activation |

- **Values at 2.79.** With one channel both set calls pass 0xFFFFFFFF. With WaveMix they pass `MIDIOUTSETVOLUME(1, ffffffff)`, `WAVEOUTSETVOLUME(0, ffffffff)`, then `MIDIOUTSETVOLUME(1, f332f332)` (the 95 % music volume) and `WAVEOUTSETVOLUME(0, cccccccc)` (A = B = 80 % for the −6000 point). [ORACLE sound-0001]
- **Every volume call fails.** Every `WAVEOUTSETVOLUME`, `WAVEOUTGETVOLUME`, `MIDIOUTSETVOLUME` and `MIDIOUTGETVOLUME` in every run returns 5 (`MMSYSERR_INVALHANDLE`). So the oracle never applies the game's volume or pan, and every oracle recording plays at full volume, centred. [ORACLE sound-0001] Whether Windows 3.1 accepts device ids 0 and 1 here is [OPEN].
- **No Midi Mapper box.** No run shows a MessageBox with string 2219. In `sound-idle2`, Wine traces `MCI_SEQ_STATUS_PORT => 65535` (`MIDI_MAPPER`) at both opens. [ORACLE sound-0001]
- **WaveMix only:**
  - The session opens the device with `WAVEOUTOPEN(…, 0, format, hwnd, 0, 0x10000 CALLBACK_WINDOW)` and prepares six 0x20-byte headers.
  - It then preloads the 13 slots without bit 1. Each preload shows as a format query `WAVEOUTOPEN(…, flags 1 WAVE_FORMAT_QUERY)` and an `MMIOOPEN`/`MMIODESCEND`/`MMIOREAD`/`MMIOCLOSE` sequence. In order the slots are 0x00, 0x03, 0x06, 0x08, 0x11, 0x12, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x45. This is flags 0, 1 and 4 of the slot table, all from t 0.97 to 0.98.
  - At 2.89 the six headers are unprepared, the device is closed (`WAVEOUTCLOSE`) and then reopened. This is `WaveMixActivate` FALSE then TRUE around command 4.
  - [ORACLE sound-0001]

## One channel (`SoundSystem=1`, `MixChannels=1`)
`SNDPLAYSOUND` flags are 7 (`SND_ASYNC|SND_NODEFAULT|SND_MEMORY`) or 0x0F (adds `SND_LOOP`). Every call returns 1. No `WAVEOUT*` play call and no `WAVEOUTSETVOLUME` other than the one at 2.79 appear: STANDARD mode never reaches ordinal 8's volume code. [ORACLE sound-0001]

### Intro and title (main; briefesc and title within 0.2 s)
| t | Calls | Slot (resource) | Id, how sure |
|---|---|---|---|
| 6.38 | stop; `SNDPLAYSOUND(img, 7)` | 0x23 (SND_FX 135) | 0x23: certain. It is the intro routine seg15:0x5e22, either site seg15:0x5ee7 or seg15:0x637e. |
| 7.52 | `SNDPLAYSOUND(img, 0x0F)` | 0x14 (SND_FX 120), looped | 0x68: certain. Only 0x68 plays slot 0x14 with `SND_LOOP` (seg15:0x60a4). |
| 9.55–9.57 | stop ×4; `SNDPLAYSOUND(img, 7)` | 0x23 | 0x23: certain |
| 10.71–13.70 | stop ×464, about 155/s | — | a per-frame stop with no play; which id is [OPEN] (0x69 from seg10:0x744c, or the frame's 0x6e at seg15:0x5a23) |
| 13.70 | `MCI_CLOSE` dev 0 (0x101), then the open sequence below | — | music track 0 (anthem), intro phase 1, seg15:0x5bf5 |
| 14.70 | stop ×2 | — | |

### main run
| t | Step | Calls (ret 1 unless shown) | Slot | Id, how sure |
|---|---|---|---|---|
| 20.15 | ESC at the title (Restore Game) | stop ×4; `(img, 7)`; `MCI_PAUSE` dev 1 | 0x13 | 0x6e then 0x13 (dialog open): certain for 0x13. The pause is −2 (see Music). |
| 45.48 | New Game OK | `MCI_PLAY` 9, `MCI_PAUSE`; stop ×2; `WAVEOUTGETNUMDEVS`, `WAVEOUTGETDEVCAPS` | — | −3 after the dialog, then −1 or −2 for the desk scene |
| 46.60 | desk scene | stop; `(img, 7)` | 0x14 | 0x14: likely (typewriter reset seg15:0x84b0) |
| 47.09–58.98 | desk text | `(img, 7)` ×289, at 20–22 per 0.5 s, with no stops | 0x2b | 0x2b: certain (desk character tick seg15:0x1846, mode 0) |
| 59.01 | desk | stop ×2; `(img, 7)` | 0x4b (ENG_FX 175) | 0x4b "Commencing launch sequence": certain |
| 60.49 | desk | stop ×2; `(img, 7)` | 0x05 (SND_FX 105) | 0x05 SHIELD: certain (seg15:0x4dbf) |
| 61.72–62.85 | desk text | `(img, 7)` ×46 | 0x2b | 0x2b |
| 63.66 | desk | stop ×2; `(img, 7)` | 0x20 (SND_FX 132) | 0x20 IGNITE: certain. No 0x21 follows, so it is not sequence 0x6a. |
| 64.61 | desk | stop ×2; `(img, 7)` | 0x1c (SND_FX 128) | 0x1c MISSILE: certain |
| 65.99 | desk | `(img, 7)` with no stop | 0x4c (ENG_FX 176) | 0x4c "Penetrating inner space": certain (seg15:0x613, mode 0) |
| 69.27 | Navigation window | stop ×4; `(img, 7)` | 0x4d (ENG_FX 177) | 0x4d "Select destination": certain (seg6:0x2daa) |
| 72.52 | Navigation Enter | stop ×4; `(img, 0x0F)`; stop ×2 | 0x17 (SND_FX 123), looped | 0x6f: certain. Only 0x6f plays slot 0x17 with `SND_LOOP` (seg15:0x666f). The next stops end the loop after 20 ms. |
| 74.57 | briefing | stop ×3; `(img, 7)` | 0x14 | 0x14: likely (typer reset) |
| 75.05–78.64 | briefing page 1 | `(img, 7)` ×102 | 0x2b | 0x2b |
| 79.19–79.64 | briefing | stop ×3; `(img, 7)` ×3; `(img, 7)` | 0x13 ×3, then 0x14 | 0x13 page click, 0x14 typer reset |
| 80.15–222.12 | briefing pages | per page: (stop ×2; `(img, 7)`) repeated 90–278 times, then `(img, 7)` once or 3 times with stops between, then stop ×12–15; `(img, 7)` 1 to 3 times; `(img, 7)` | 0x2b; 0x15 (SND_FX 121); 0x13; 0x14 | 0x2b tick (seg9:0x8ff6), 0x15 AWAY at a page's end (page typer result 2), 0x13 page open, 0x14 typer reset: all certain from the slots |
| 223.11 | ESC, wave start | music: `MCI_CLOSE` dev 1, open of main.mid (track 1); `MM_MCINOTIFY` wParam 4 | — | seg9:0x65de track 1 |
| 223.13 | wave start | `(img, 7)` ×2 | 0x27 (SND_FX 139) | 0x27 HYPER: certain |
| 223.88 | wave start | `(img, 7)` | 0x32 (ENG_FX 150) | 0x65: certain. Only 0x65 (0x30 + rand() % 3) reaches slot 0x32. The voice is "Show no mercy". |
| 225.27 | Alt+X | stop ×4; `(img, 7)` | 0x0b (SND_FX 111) | 0x0b or 0x0c (ordinal 9 plays 0x0c as 0x0b): the SC_CLOSE draw |
| 225.72 | Alt+X, 0.45 s later | `(img, 7)` with no stop; `MCI_PAUSE` | 0x50 (ENG_FX 180) | 0x50 "Please stay": certain. The pause is −2 for the dialog. |
| 226.87 | Cancel (#244) | `MCI_PLAY` 9; `MM_MCINOTIFY` wParam 2 | — | −3 |
| 228.02 | Alt+X | stop ×4; `(img, 7)` | 0x0d (SND_FX 113) | 0x0d SCREECH: certain (the draw's third value) |
| 228.47 | 0.45 s later | `(img, 7)`; `MCI_PAUSE` | 0x50 | 0x50 |
| 229.60 | Exit (#244) | `MCI_PLAY`, `MCI_PAUSE`; stop; `MM_MCINOTIFY` 2 | — | −3, then a pause; the stop is the #799 init's 0x6e. 0x51 plays nothing because ENG_FX.DLL 181 is absent. |
| 231.79 | Exit (#799) | `MCI_PAUSE`; stop | — | |

- **Briefing totals** (72.5–223): 0x14 plays 18 times, once per typer reset. 0x15 plays in 21 separate seconds, once to three times at the end of a page. [ORACLE sound-0001]
- **Steps without sound.** The GAME combo, OK on Restore Game, the SHIP, PLAY, DRIVE and ZOOM LEVEL combos and the games row make no sound call. [ORACLE sound-0001]
- **Stop counts.** ESC and Alt+X each show four stops before their sound. The calls 0x6e and then id through seg10:0x744c give three: 0x69, 0x6e, 0x69. Where the fourth comes from is [OPEN]. [ORACLE sound-0001]
- The 0x0b → 0x50 gap is 0.45 s both times, the busy-wait of seg2:0xd78. [ORACLE sound-0001]

### briefesc run
It is the same as main up to the briefing, about 18 s earlier (New Game OK at 27.90). After the Enter come 0x6f at 54.85, 0x14 at 56.89 and 0x2b ×70 from 57.37 to 59.35. ESC at 59.39 starts the wave. That gives the music open of main.mid, then 0x27 ×2 and 0x32 at 60.17. Alt+X at 61.54 plays slot 0x0b, then 0x50 at 62.01. Exit and #799 Exit at 63.13 and 65.30 are the same as in main. [ORACLE sound-0001]

### title run
| t | Step | Calls | Slot | Id, how sure |
|---|---|---|---|---|
| 21.15 | ESC (Restore Game) | stop ×4; `(img, 7)`; `MCI_PAUSE` | 0x13 | 0x6e, 0x13 |
| 23.25 | Cancel | `MCI_PLAY` 9; `MM_MCINOTIFY` 2; stop ×3; `(img, 7)` | 0x13 | 0x13: the credits page opens |
| 24.33–29.23 | credits typer | `(img, 7)` ×34, plus 2 plays of slot 0x22 at 27.53 and 27.87 | 0x14, 0x22 (SND_FX 134) | 0x14 typing tick of seg15:0x7cda; 0x22 BOLT of the logo typer (seg15:0x7eee or seg15:0x800d) |
| 29.97 | R (Order #693) | stop; `MCI_PAUSE` | — | 0x6e, then 0x51 (silent); the pause is −2 |
| 32.19 | Register, empty code | — | — | no sound |
| 34.65 | Register, code 123 | stop; `(img, 7)`; `MESSAGEBOX("Sorry! Incorrect registration number.", "Inner Space", 0x10)` | 0x0d | 0x0d: certain |
| 37.77, 40.77 | Order Form.., More Info.. (Notepad takes focus) | stop; `MIDIOUTGETVOLUME(1, DGROUP:0x6a8)`; `WAVEOUTGETVOLUME(0, DGROUP:0x6a4)` | — | command 3. Its set calls do not appear, because the values equal the caches. |
| 39.77, 41.77 | Alt+F4 on Notepad (focus back) | `MIDIOUTGETVOLUME(1, DGROUP:0x6b0)`; `WAVEOUTGETVOLUME(0, DGROUP:0x6ac)` | — | command 4. There is no `MCI_PLAY`: the music stays paused. |
| 42.73 | Cancel (#693) | `MCI_PLAY`; stop; `(img, 7)` at 42.73 and 43.06; stop ×2; `MM_MCINOTIFY` 2 | 0x22 | −3; the credits resume |
| 50.93 / 51.39 | Alt+X | stop ×4; `(img, 7)` slot 0x0b; then `(img, 7)` slot 0x50; `MCI_PAUSE` | 0x0b, 0x50 | 0x0b/0x0c, then 0x50 |
| 52.66 | Save & Exit (disabled at the title) | — | — | no sound |
| 54.22 | Cancel (#244) | `MCI_PLAY`; `MM_MCINOTIFY` 2 | — | −3 |
| 55.37 / 55.82 | Alt+X | the same as 50.93, slot 0x0b again | 0x0b, 0x50 | |
| 56.95 | Exit (#244) | `MCI_PLAY`, `MCI_PAUSE`; stop; `MM_MCINOTIFY` 2 | — | |
| 59.15 | Order.. (#799) | stop ×2; `(img, 7)` | 0x13 | 0x13 (seg2:0x1046 or #799's Order path) |
| 63.91 | Register, code 123 | `(img, 7)`; `MESSAGEBOX` as above | 0x0d | 0x0d |
| 66.79–71.77 | Order Form.., More Info.. | as at 37.77–41.77 | | |
| 72.25 | Cancel (#693) | `MCI_PLAY`, `MCI_PAUSE`; stop | — | the game ends |

## WaveMix (`SoundSystem=3`, `MixChannels=4`)
The ids and the steps are the ones of the one-channel set. The first play of each deferred slot names it (`MMIOOPEN`). [ORACLE sound-0001]
- main: 0x14 at 7.42, 0x13 at 20.15, 0x2b at 47.12, 0x4b at 58.96, 0x05 at 60.43, 0x20 at 63.61, 0x1c at 64.57, 0x4c at 65.95, 0x4d at 69.23, 0x17 at 72.51, 0x14 again at 74.56 (purged and reloaded), 0x2b at 75.07, 0x13 at 79.35, 0x15 at 84.74, 0x32 at 224.00, 0x0b at 225.34 and 0x50 at 225.79.
- title: 0x22 at 27.71 and 0x0d at 34.60.

Per step of the main run, from the trace:

| Step (t) | `WAVEOUTPAUSE`/`RESTART` (≈ plays) | `WAVEOUTWRITE` | `MM_WOM_DONE` to NotifyClass by channel | `WAVEOUTSETVOLUME` values |
|---|---|---|---|---|
| intro (0.9–17.8) | 4 | 21 | ch0 2 | 225 calls: `cccccccc`, then a pan sweep `fd6f2666` → `fd6f451e` → `fae0451e` … in steps of about 0x28f, then `f332f332`, `cccccccc` and the mirrored sweep `147adc28` … |
| ESC at title (20.15) | 1 | 3 | ch0 1 | `cccccccc` |
| New Game OK, desk (45.5–70.2) | 296 | 332 | ch0 151, ch1 144 | `c7adc7ad`, `cccccccc` |
| Enter, briefing (72.5–218.2) | 3930 | 4041 | ch0 490, ch1 453, ch2 377, ch3 376 | 221 calls: a sweep from `c28ec28e` through `23d6dc28` … `deb73333`, repeated, then single values such as `ae13f5c1`, `e8f4f332` and `35c2e665` |
| ESC, wave start (223.2–225.1) | 3 | 13 | ch0 1, ch1 1 | `f332f332`, `cccccccc` |
| Alt+X (225.3–226.9) | 2 | 9 | ch0 2, ch1 1 | — |
| Exit (#799) (232.39) | 0 | 0 | — | `MIDIOUTSETVOLUME(1, ffffffff)`, `WAVEOUTSETVOLUME(0, ffffffff)`; 6 × `WAVEOUTUNPREPAREHEADER`, `WAVEOUTCLOSE` |

- **What a play costs.** Each `WaveMixPlay` gives `WAVEOUTPAUSE`, `WAVEOUTGETPOSITION`, `WAVEOUTRESET` (in most bursts), `WAVEOUTWRITE` of the remixed blocks and `WAVEOUTRESTART`. The device's own block completions go to WAVEMIX.DLL's window (handle 0x82, 4428 in main). Channel completions are posted to the `NotifyClass` procedure ISPACE.DLL seg9:0x2ac4 with wParam = channel. The briefing uses all four channels. [ORACLE sound-0001]
- **Polling.** `WAVEOUTGETPOSITION` runs about 3500 times a second in the desk scene: 87,869 calls in main and 122,496 in briefesc. [ORACLE sound-0001]
- **Focus.** On a loss of focus (Order Form.. or More Info.. starting Notepad) the session sends `MIDIOUTGETVOLUME(1)` and `WAVEOUTGETVOLUME(0)`. It then sends `MIDIOUTSETVOLUME(1, ffffffff)` and `WAVEOUTSETVOLUME(0, ffffffff)` (the saved system values) and `WAVEOUTCLOSE`. On regaining focus it sends `MIDIOUTSETVOLUME(1, f332f332)`, `WAVEOUTSETVOLUME(0, cccccccc)`, `WAVEOUTOPEN` (CALLBACK_WINDOW) and a stop. These are commands 3 and 4 with `WaveMixActivate`. [ORACLE sound-0001]
- **Volume values.** The volume dword is right << 16 | left. `cccccccc` is 80/80 % and `f332f332` 95/95 %. The sweeps are ordinal 8 at each priority play and frame. All of them return 5, as in Start-up. [ORACLE sound-0001]

## Music (both sets)
**MCI sequence as observed.** Device ids are the MCI ids; flags are `dwFlags`. [ORACLE sound-0001]

| Code | `mciSendCommand` sequence | Returns |
|---|---|---|
| −1 before any open | `MCI_PAUSE` dev 0, flags 0 | 0x101 |
| track (new file) | `MCI_CLOSE` dev 0 or 1, flags 2; `MCI_OPEN` 0x803 flags 0x2200 ("sequencer", the `.mid` path); `MCI_STATUS` 0x814 flags 0x100; `MCI_SET` 0x80d flags 0x400; `MCI_SEEK` 0x807 flags 0xa; `MCI_PLAY` 0x806 flags 9 | close 0x101 on dev 0, else all 0 |
| −2 (playing) | `MCI_PAUSE` dev 1, flags 0 | 0 |
| −3 (paused) | `MCI_PLAY` dev 1, flags 9, with no seek | 0 |

- **Files and positions** (`sound-idle2`, Wine's mcimidi trace): [ORACLE sound-0001]
  - Track 0 opens `C:\INSPACE\SOUNDS\anthem.mid`, seeks to 0 ms and plays from 0 to 181000.
  - The attract demo (a duel) opens `main.mid`, seeks to 287000 and plays from 287000 to 292000, which is track 7.
  - The resume after a pause plays from 287000 to 292000 again: Wine replays the `dwTo` request from the current position. Both values match the track table of [sound.md](../../docs/re/sound.md#music).
- **Codes in the scenario.** At the title the music plays track 0. Each dialog pauses it with −2 and resumes it with −3. A resume never shows a seek, so the paused state was 2 and not 4. New Game OK resumes and pauses again at the same instant. The music then stays paused through the desk scene, the Navigation window and the whole briefing. The wave start closes the device and opens main.mid. The exit path pauses, resumes and pauses. The game ends with the device still open: no `MCI_CLOSE` follows. [ORACLE sound-0001]
- **`MM_MCINOTIFY`.** The window procedure ISPACE.DLL seg9:0x2ac4 receives it with lParam = device 1. [ORACLE sound-0001]
  - wParam 2 (`MCI_NOTIFY_SUPERSEDED`) follows every −3 `MCI_PLAY` after a pause: the new play supersedes the pending notify of the earlier one.
  - wParam 4 (`MCI_NOTIFY_ABORTED`) follows every `MCI_CLOSE` of a playing or paused device.
  - **wParam 1 (`MCI_NOTIFY_SUCCESSFUL`) arrives** in `sound-idle1`, at 206.72, 10.02 s after the track-7 `MCI_PLAY` of the attract duel (196.70). In the same tick the game sends `MCI_SEEK` flags 9 (`MCI_TO|MCI_NOTIFY`). A second wParam 1 follows at once and is answered by `MCI_PLAY` flags 9. This is the 7 → 2 chaining of the notify window: a notify seek in state 3, then a play.
  - No other run reaches the end of a track. The title's anthem section (181 s) is paused at 162.8 s for the briefing of the attract cycle.
  - **Timing.** Track 7 is 5.0 s long, but the notify came 10.02 s after the play. In `sound-idle2`, with the extra `+mci,+mcimidi` channels, no notify came in the 25.6 s before the next pause. How long Wine's mciseq takes to reach a section that starts at 287 s is [OPEN].
- **Music volume:** `MIDIOUTSETVOLUME(1, …)` is called only at start-up (0xFFFFFFFF; with WaveMix also 0xF332F332), at focus changes with WaveMix, and at exit with WaveMix (0xFFFFFFFF). The calls return 5. [ORACLE sound-0001]

## INI
INSPACE.INI after the runs differs from before only in the keys of [start-0001](start-0001.md) and `Calibrate`. No sound key is written: `SoundSystem` was set explicitly and passes the capability clamp. [ORACLE sound-0001]

## Open
- Which id gives the 464 stops of the intro at 10.7–13.7 s, and the fourth stop before 0x13 and 0x0b. [OPEN]
- Whether Windows 3.1 accepts `waveOutSetVolume(0, …)` and `midiOutSetVolume(1, …)`, since Wine 11's 16-bit MMSYSTEM rejects both with `MMSYSERR_INVALHANDLE`. [OPEN]
- The time Wine's sequencer takes to notify the end of a section that starts deep in a file (track 7 at 287 s). [OPEN]
- WaveMix's mix format and block size. The relay shows the `WAVEOUTOPEN` format only as a pointer. [OPEN]
