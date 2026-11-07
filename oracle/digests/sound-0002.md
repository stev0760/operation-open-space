# sound-0002: Audio Options at capability 3, the capability gate, and Wine's sequencer after a seek

- **Date:** 2026-10-27. **Host:** Arch Linux, Wine 11.0 old WoW64 with the ExtractIcon patch (`~/opt/wine-11.0-oldwow-patched`, the `oracle/env.sh` default), Xvfb 1024×768×24, PulseAudio.
- **Prefix:** `work/wine/oracle-58` (`ORACLE_ID=58`, display `:168`), made fresh for each game run by `scenario_prefix` as in [sound-0001](sound-0001.md): game layout in `C:\INSPACE`, `WAVEMIX.DLL` and `WAVEMIX.INI` in `C:\windows`, the MIDI files and both FX DLLs in `C:\INSPACE\SOUNDS`, fixture `basic` as `D:`, no `Z:`. Unregistered. The registered prefix took no part.
- **Audio device:** Wine's PulseAudio driver with `PULSE_SINK` set to a null sink. `waveOutGetNumDevs` returns 4. The MIDI mapper plays to the ALSA "Midi Through" port. [ORACLE sound-0002]
- **Trace:** the default relay profile (`oracle/trace.sh`) for the game runs. The sequencer runs use `+mci,+mcimidi,+winmm,+timestamp` (t4 also `+midi`).
- **Control state:** a scratch Win32 tool, `ctldump.exe`, runs in the same prefix and wineserver and lists the child controls of the named dialog: id, class, text, enabled, `BM_GETCHECK`, the combo items with `CB_GETCURSEL`, and `SBM_GETPOS` with the scroll range. Its listings sit beside the screenshots as `NN-name.ctl.txt`. [DESIGN]
- **Runs:**

  | Run | `INI_SET` | Steps | Output (gitignored) |
  |---|---|---|---|
  | `ss3` | `SoundSystem=3 MixChannels=4` | title, Alt+G (Options), Audio.., balance bar right arrow ×3 and one trough click to the right, Voice Effects off, Music off, Keep on, Swap on, Mix Channels Up Up Return (4 → 2), Stereo Effect Down Return (Normal → More), OK; Master Volume left arrow ×2; Options OK; Alt+G, Audio.., balance left arrow, Sound FX off, Voice on, Mix Down Return, Cancel; Audio.. again, Cancel; Options Cancel | `work/audio/s3/oracle/ss3/` |
  | `ss0to3` | `SoundSystem=0 MixChannels=4` | Options, Audio.. and Cancel; sound combo End Return (3D Dynamic Sound), Options OK; ESC (Restore Game) and Cancel; Alt+G, Master Volume left arrow, Audio.. and Cancel, Options Cancel; Alt+G, Audio.., OK; ESC and Cancel | `work/audio/s3/oracle/ss0to3/` |
  | `mix0` | `SoundSystem=3 MixChannels=0` | Options, Audio.., OK; Options Cancel; ESC and Cancel | `work/audio/s3/oracle/mix0/` |
  | `cap/ss1`, `cap/ss2`, `cap/ss-1` | `SoundSystem=1`, `2`, `-1`, each with `MixChannels=4` | Options, Audio.. | `work/audio/s3/oracle/cap/` |
  | `cap/noaudio-ss3`, `cap/noaudio-ss-1` | `SoundSystem=3`, `-1`, with `HKCU\Software\Wine\Drivers` `Audio` = "" (no Wine audio driver) | Options | `work/audio/s3/oracle/cap/` |
  | `seq/t4`, `t11`, `t5`, `t8` | — | `mciplay.exe` (below) on RACE.MID 34000–289000 ms, ANTHEM.MID 181000–200000, ANTHEM.MID 200000–216600 and MAIN.MID 292000–301000 | `work/audio/s3/oracle/seq/` |

- **Sequencer probe.** `mciplay.exe FILE FROM TO SECONDS` is a scratch Win32 program run under the same Wine. It sends the game's MCI sequence of [sound.md](../../docs/re/sound.md#music): `MCI_OPEN` "sequencer" with the element (flags 0x2200), `MCI_STATUS` `MCI_SEQ_STATUS_PORT`, `MCI_SET` milliseconds, `MCI_SEEK` flags 0xa (`MCI_TO|MCI_WAIT`) and `MCI_PLAY` flags 9 (`MCI_TO|MCI_NOTIFY`). It logs `MM_MCINOTIFY` and polls position and mode once a second. The game's 16-bit `MCISENDCOMMAND` reaches the same `mciseq` driver through Wine's `mmsystem.dll16`, so the probe stands for tracks 4, 5, 8 and 11, which no scenario reaches. [DESIGN]
- **Reproduce:** `work/audio/s3/oracle/start58.sh <outdir> "<INI_SET>"` (it sources `oracle/scenarios/common.sh` with `ORACLE_ID=58`, `KEEP=1`; `NO_AUDIO=1` empties the Wine audio driver), then the steps above with the helpers of `env58.sh` (`click`, `key`, `shot`, `dump`); `capcheck.sh` does the `cap/` runs. Screen positions: Options client origin 377,196, Audio.. 462,230, Options OK 597,215 and Cancel 597,246; Audio Options client origin 487,246, OK 696,267, Cancel 696,303. The tools are `work/audio/s3/tools/ctldump.c`, `mciplay.c`, `seqscan.py` (the messages the player sends, split by its skip state) and `smfnotes.py` (held notes and next note-ons under the file's tempo map). The scripts are not in the repository.
- **Ticks.** Quoted trace lines keep Wine's `+timestamp` tick. Selector 0417 is ISPACE.DLL seg9, as in [sound-0001](sound-0001.md) and [newgame-0001](newgame-0001.md).

## Audio Options at capability 3 (`ss3`)
**Placement.** Audio Options opens with window rectangle 484,217 263×289 and client 257×257 at 487,246. That is its template position 73,31 DLU from the client origin 377,196 of Options, scaled by 6/4 and 13/8. Options is at 374,167 276×435. [ORACLE sound-0002] [BYTES ENG_LANG.DLL DIALOG 999]

**Initial state** (`03-audio.ctl.txt`), with the sound keys of the INI absent except `SoundSystem` and `MixChannels`: [ORACLE sound-0002]

| Id | Control | State |
|---|---|---|
| 102 | &Sound FX | checked |
| 103 | &Voice Effects | checked |
| 101 | &Music Soundtrack | checked |
| 1001 | balance bar | range 0–100, position 50 |
| 128 | Mix Channels combo | "1" … "8", selection 3 ("4") |
| 1008 | Stereo Effect combo | Minimal, Less, Normal, More; selection 2 |
| 1003 | &Keep Sounds in Memory (faster loading) | unchecked |
| 1004 | &Swap left/right Channels | unchecked |
| 1, 2, 106 | &OK, &Cancel, &Help | enabled |

Every control is enabled. The statics read Mix Channels:, Stereo Effect:, Effects vs. Music, Effects and Music.

**Init calls.** WM_INITDIALOG sends `WM_SETFONT` to each control and shows it. It then adds "1" … "8" to combo 128 and selects 3, adds strings 0x96d–0x970 to combo 1008 and selects 2. Next come five `CHECKDLGBUTTON` calls, then `SETSCROLLRANGE` and `SETSCROLLPOS`. There is no `ENABLEWINDOW`, no `SETFOCUS` and, with WAVEMIX.DLL loaded, no `OPENFILE`: [ORACLE sound-0002]
```
340457.002:00d8:Call USER.111: SENDMESSAGE(00ee,040e,0003,00000000) ret=0417:303f ds=0427
340457.002:00d8:Call USER.97: CHECKDLGBUTTON(00e6,0066,0001) ret=0417:3147 ds=0427
340457.002:00d8:Call USER.97: CHECKDLGBUTTON(00e6,0067,0001) ret=0417:3155 ds=0427
340457.002:00d8:Call USER.97: CHECKDLGBUTTON(00e6,0065,0001) ret=0417:3163 ds=0427
340457.002:00d8:Call USER.97: CHECKDLGBUTTON(00e6,03eb,0000) ret=0417:3178 ds=0427
340457.002:00d8:Call USER.97: CHECKDLGBUTTON(00e6,03ec,0000) ret=0417:3187 ds=0427
340457.002:00d8:Call USER.64: SETSCROLLRANGE(00ec,0002,0000,0064,0000) ret=0417:31a0 ds=0427
340457.002:00d8:Call USER.62: SETSCROLLPOS(00ec,0002,0032,0001) ret=0417:31b9 ds=0427
```

**Balance bar.** Each arrow click is `WM_HSCROLL` code 1 then code 8. A trough click is code 3 then 8. Both move `FXMusic` by 1, so 50 becomes 51, 52, 53 and then 54. Each move calls only `SETSCROLLPOS`, with no volume API and no sound. [ORACLE sound-0002]
```
340486.078:00d8:CallTo16(func=0417:2f30,00e6,0114,0003,00ec,0000) …
340486.078:00d8:Call USER.62: SETSCROLLPOS(00ec,0002,0036,0001) ret=0417:3231 ds=0427
340486.079:00d8:CallTo16(func=0417:2f30,00e6,0114,0008,00ec,0000) …
```

**Audio OK** writes eight keys in this order and ends the dialog with 1: [ORACLE sound-0002]
```
340505.111 WRITEPRIVATEPROFILESTRING("Inner Space","StereoEffect","3","INSPACE.INI") ret=0417:33db
340505.111 … "FXMusic","54" ret=0417:3401
340505.112 … "Music","0" ret=0417:3427
340505.112 … "SoundFX","1" ret=0417:344d
340505.112 … "VoiceFX","0" ret=0417:3473
340505.113 … "MixChannels","2" ret=0417:3498
340505.113 … "PurgeSound","0" ret=0417:34be
340505.113 … "StereoSwap","1" ret=0417:34e4
```
- Then come `FREELIBRARY(051f)`, `FREELIBRARY(050f)`, `LOADLIBRARY("C:\INSPACE\SOUNDS\SND_FX.DLL")` and `LOADLIBRARY("C:\INSPACE\SOUNDS\ENG_FX.DLL")`. Both modules load although `VoiceFX` is now 0. Loading SND_FX.DLL sends `MIDIOUTGETVOLUME(0001,…)` and `WAVEOUTGETVOLUME(0000,…)`, as at start-up. [ORACLE sound-0002]
- The channel change then closes the session and opens a new one. The calls are `GETPRIVATEPROFILEINT("MixChannels")`, `WAVEOUTRESET`, six `WAVEOUTUNPREPAREHEADER`, `WAVEOUTCLOSE`, `FREELIBRARY(054f)` (WAVEMIX.DLL), a fresh `LOADLIBRARY("WAVEMIX.DLL")` with its WAVEMIX.INI reads and `GETPROCADDRESS` calls, `SNDPLAYSOUND(NULL,0)`, `WAVEOUTOPEN(…,0x10000)` and six `WAVEOUTPREPAREHEADER`. The deferred slots are preloaded again (`FINDRESOURCE` … `MMIOCLOSE`), and the last call is `ENDDIALOG(00e6,0001)`. No MCI call is made. [ORACLE sound-0002]
- Music off stops nothing at once: no MCI call. On Options OK, the −3 from INSPACE closes the paused device: `MCISENDCOMMAND(0001,0804,00000002,…)` (`MCI_CLOSE`). [ORACLE sound-0002]

**Master Volume.** Each left-arrow click on bar 111 is code 0 then code 8. Code 0 sets the position (100 → 99 → 98). Code 8 plays the test sound 0x23, and its play through ordinal 8 sets the volumes. The second click sends only `MIDIOUTSETVOLUME`. Every set call returns 5. [ORACLE sound-0002]
```
340523.590:00d8:Call MMSYSTEM.212: MIDIOUTSETVOLUME(0001,f0a2f0a2) ret=0417:3e77 ds=0427
340523.590:00d8:Call MMSYSTEM.416: WAVEOUTSETVOLUME(0000,b851b851) ret=0417:0b93 ds=0427
340523.590:00d8:Call MMSYSTEM.409: WAVEOUTPAUSE(c000) ret=0547:1464 ds=054f
340524.294:00d8:Call MMSYSTEM.212: MIDIOUTSETVOLUME(0001,ee13ee13) ret=0417:3e77 ds=0427
340524.294:00d8:Call MMSYSTEM.409: WAVEOUTPAUSE(c000) ret=0547:0fc4 ds=054f
```

**Options OK** writes `Complex` 2, `Braking` 0, `Enhance` 1, `SoundVolume` 98, `SoundSystem` 3, `Quick` 1, `Detail` 3, `Size` 1, `SetSize` 1 and `FrameRateSet` 36, in this order, and ends with `ENDDIALOG(0088,0001)`. With the sound system unchanged there is no INI reader rerun. [ORACLE sound-0002]

**INSPACE.INI after OK** (`INSPACE.INI.after-ok`) holds the new keys appended in write order: `MixChannels=2` (in place), `StereoEffect=3`, `FXMusic=54`, `Music=0`, `SoundFX=1`, `VoiceFX=0`, `PurgeSound=0`, `StereoSwap=1`, `Complex=2`, `Braking=0`, `Enhance=1`, `SoundVolume=98`, `Quick=1`, `Detail=3`, `Size=1` and `SetSize=1`. Wine's profile writer drops the blank lines between sections. [ORACLE sound-0002]

**Cancel.**
- The reopened Audio Options shows the stored values: Voice and Music unchecked, bar 54, selection 1 ("2"), More, Keep and Swap checked. [ORACLE sound-0002]
- After the balance left arrow (54 → 53), Sound FX off, Voice on and Mix 2 → 3, Cancel makes one call, `ENDDIALOG(0108,0000)`. No profile write, no library call. [ORACLE sound-0002]
- Audio.. opened again shows bar 53, so `FXMusic` moved before a Cancel stays in memory. The check boxes and the combo show the stored values. [ORACLE sound-0002]
- Options Cancel makes only `ENDDIALOG(0088,0000)`. [ORACLE sound-0002]

## (a) SoundSystem 0 → 3 in Options, without a restart (`ss0to3`)
- **At start-up** with `SoundSystem=0` the game does not load WAVEMIX.DLL. Audio init therefore tests the file and finds it, which gives 8 entries. The selection is −1, and the combo shows nothing: [ORACLE sound-0002]
```
340840.894:00d8:Call KERNEL.74: OPENFILE(03a7b528 "C:\\windows\\WAVEMIX.DLL",03a7:b4a0,4000) ret=0417:2fe4 ds=0427
340840.895:00d8:Call USER.111: SENDMESSAGE(00e2,040e,ffff,00000000) ret=0417:303f ds=0427
340840.895:00d8:Ret  USER.111: SENDMESSAGE() retval=ffffffff ret=0417:303f ds=0427
```
  The selection is the channel count − 1 [BYTES ISPACE.DLL seg9:0x2fdf], so the count ISPACE.DLL DGROUP:0x219e is 0. `MixChannels=4` in the INI does not reach the dialog. [ORACLE sound-0002]
- **Options OK with 3D Dynamic Sound** writes `SoundSystem` 3 and reruns the INI reader. The reader reads the sound keys, then `WAVEOUTGETNUMDEVS`, `WAVEOUTGETDEVCAPS`, `AllowSound`, `MIDIOUTGETDEVCAPS` and `SoundSystem`. Then the music starts: `MIDIOUTSETVOLUME(0001,f332f332)`, `MCI_CLOSE` to device 0 (0x101), and `MCI_OPEN`, `MCI_STATUS`, `MCI_SET`, `MCI_SEEK` and `MCI_PLAY` on device 1, all returning 0. That is track 0, the anthem. No `LOADLIBRARY("WAVEMIX.DLL")`, no `WAVEOUTOPEN` and no `SNDPLAYSOUND` follow. [ORACLE sound-0002]
- **Effects stay silent.** From ESC (Restore Game) to Options Cancel, some 40 000 trace lines hold `MCI_PAUSE` and `MCI_PLAY` and no `SNDPLAYSOUND`, `WAVEOUT*`, `MMIOOPEN` or `FINDRESOURCE` of a sound module. That window covers the 0x6e and 0x13 of Restore Game, the Master Volume test sound 0x23 and the 0x6e and 0x13 of Options. The test sound also sets no volume. A second ESC and Cancel after Audio OK give 0 `SNDPLAYSOUND`, `WAVEOUTWRITE` or `WAVEOUTPAUSE` calls. [ORACLE sound-0002]
- **The selection stays −1** in every later Audio Options of the run (`SENDMESSAGE(00f4,040e,ffff,…)`, `SENDMESSAGE(00e4,040e,ffff,…)`), so DGROUP:0x219e stays 0. [ORACLE sound-0002]
- **Audio OK then writes `MixChannels` 0** (below). The music still plays: the effects alone are silent until a restart, and the restart then reads `MixChannels=0`. [ORACLE sound-0002]

## (b) Audio OK while combo 128 has no selection
- **After the 0 → 3 switch** (`ss0to3`), Audio OK writes the eight keys with `MixChannels` 0. No `FREELIBRARY`, no `LOADLIBRARY`, no session call and no `FINDRESOURCE` follow before `ENDDIALOG`: [ORACLE sound-0002]
```
340891.962:00d8:Call KERNEL.129: WRITEPRIVATEPROFILESTRING(04270674 "Inner Space",04270a66 "MixChannels",04271674 "0",04270680 "INSPACE.INI") ret=0417:3498 ds=0427
340891.963:00d8:Call USER.88: ENDDIALOG(00fc,0001) ret=0417:35b3 ds=0427
```
- **With `MixChannels=0` in the INI beforehand** (`mix0`), the start-up loads WAVEMIX.DLL. It reads `MixChannels` (0), opens the device with `WAVEOUTOPEN(…,00010000)` and loads waves on demand (`MMIOOPEN`). The whole run has no `WAVEOUTWRITE` and no `WAVEOUTPAUSE`, so no effect plays. Audio init selects −1 (`SENDMESSAGE(00ee,040e,ffff,…) ret=0417:303f`). Audio OK writes `MixChannels` 0 again, with no reopen. Restore Game afterwards plays nothing either. [ORACLE sound-0002]

## (c) What enables Audio.. in Options
The capability level that the device test finds enables Audio.., not `SoundSystem`. [ORACLE sound-0002]

| Run | `waveOutGetNumDevs` | Combo 118 items, selection | Audio.. (139), bar 111, combo 118 | `SoundSystem` in the INI after |
|---|---|---|---|---|
| `ss3` | 4 | No Sound … 3D Dynamic Sound, 3 | enabled | 3 |
| `ss0to3` (at start) | 4 | the four, 0 | enabled | 3 (written by OK) |
| `cap/ss1` | 4 | the four, 1 | enabled | 1 |
| `cap/ss2` | 4 | the four, 2 | enabled | 2 |
| `cap/ss-1` | 4 | the four, 3 | enabled | 3 (−1 resolved and written) |
| `cap/noaudio-ss3` | 0 | No Sound only, 0 | all three disabled (style 0x58…) | 3 (clamped in memory only) |
| `cap/noaudio-ss-1` | 0 | No Sound only, 0 | all three disabled | 0 |

- With 4 wave devices under PulseAudio the level is 3 for every `SoundSystem` value. With no Wine audio driver, `waveOutGetNumDevs` returns 0 and the level is 0. The Options dialog is then the one dialogs-0001 saw ([dialogs-0001](dialogs-0001.md#not-covered)). [ORACLE sound-0002] [ORACLE dialogs-0001]
- At level 3 Audio Options enables every control, at `SoundSystem` 0 as well. [ORACLE sound-0002]

## (d) Notes held across the seek target (Wine's sequencer)
**The skip sends no note-on.** In each probe, mciseq's player reads the file from its start with `doPlay=F` until the seek target and sends only the channel state of that stretch. No 0x9n message with a non-zero velocity is sent while skipping. [ORACLE sound-0002]

| Probe | Sent while skipping (status: count) | Notes held across the target (true tempo map) | First note-on sent |
|---|---|---|---|
| t4, RACE.MID from 34000 | 0xb0: 185, 0xc0: 9 | 4: ch2 and ch14 notes 71 and 76 from 0.2 s | `005A2490` (ch1 note 36) at +0.091 s |
| t11, ANTHEM.MID from 181000 | 0xb0: 267, 0xc0: 49 | 13, struck at 180.2 s (ch1 60, 65, 72; ch2 41; ch3 77, 84; ch6 77; ch13–15) | `007D4F90` (ch1 note 79) at +0.060 s |
| t5, ANTHEM.MID from 200000 | 0xb0: 277, 0xc0: 57, 0xd0: 290 | — | none |
| t8, MAIN.MID from 292000 | 0xb0: 577, 0xc0: 160 | — | none |

- The first note-on sent is the file's first note after the target: in t4, ch1 note 36 at 34763.9 ms, and in t11, ch1 note 79 at 182796.6 ms. The held notes stay silent until they are struck again. [ORACLE sound-0002]
- **Wine skips the gap before that note.** In t4 the file has 764 ms of no note-on after 34000 ms, yet the first note-on goes out 0.091 s after `MCI_PLAY`. The position reads 35608 one second after the play, and the following seconds advance in real time (36599, 37579, 38750, 39585). [ORACLE sound-0002]
```
341465.759:0024:trace:mcimidi:MIDI_mciPlay Playing from 34000 to 289000
341465.802:0110:trace:mcimidi:MIDI_player wmm->dwStatus=526, doPlay=F
341465.802:0110:trace:winmm:midiOutShortMsg (00008000, 000030C0)
```
- **Wine plays track 11 as one burst.** All 1324 messages of the 181–200 s section go to handle 8000 within 0.048 s (341529.450 to 341529.498), and `MM_MCINOTIFY` wParam 1 arrives 106 ms after the play. The position then reads 199130 in mode 525 (`MCI_MODE_STOP`). The ANTHEM tempo ramp shows as one `Setting tempo` line per step (BPM 140 down to 127 within the first lines of the play). [ORACLE sound-0002]

## (e) `MCI_PLAY` past the file end on Wine's sequencer (tracks 5 and 8)
- `MCI_STATUS_LENGTH` reports 374000 ms for RACE.MID, 226000 for ANTHEM.MID and 506000 for MAIN.MID. The true lengths under the tempo map are 295.26 s, 200.80 s and 295.52 s ([sound.md](../../docs/re/sound.md#music)). [ORACLE sound-0002]
- **Track 5** (ANTHEM.MID, seek 200000, play to 216600): `MCI_SEEK` and `MCI_PLAY` return 0. The player skips to the end of the file, sending 0xb0, 0xc0 and 0xd0 messages and no note-on. `MM_MCINOTIFY` wParam 1 (`MCI_NOTIFY_SUCCESSFUL`) arrives 87 ms after the play (`mciDriverNotify (0000004E, 0001, 0001)`), and the position then reads 199130. [ORACLE sound-0002]
- **Track 8** (MAIN.MID, seek 292000, play to 301000): the same. Both calls return 0, no note-on is sent, wParam 1 arrives after 143 ms, and the position reads 290909. [ORACLE sound-0002]
- So on Wine's mciseq a play past the end is accepted and completes at once with `MCI_NOTIFY_SUCCESSFUL`. The play never fails, so the game's close path ISPACE.DLL seg9:0x3bd1 is not taken. Windows' own mciseq is not covered. [ORACLE sound-0002] [OPEN]
- Wine's end of play for ANTHEM.MID (199130) and MAIN.MID (290909) lies before 200000 and 292000, so tracks 5 and 8 play nothing, and track 7 (MAIN.MID 287000–292000) has at most 3.9 s. [ORACLE sound-0002]

## Side observations
- **No audio driver.** With `Audio` = "" the MIDI mapper finds no port (`MIDIMAP_drvOpen No software synthesizer midi port found`, `midiOutOpen => 69`). The probe's play then stays in mode 526 (`MCI_MODE_PLAY`) at position 34000 for 45 s, with no MIDI message and no notify. [ORACLE sound-0002]
- **Two MIDI handles.** mciseq opens `midiOutOpen` twice (handles 8000 and 8001) and sends every message to both. [ORACLE sound-0002]

## Open
- Whether Windows 3.1's MCI sequencer sounds held notes after a seek, skips the silence before the first note-on, and accepts `MCI_PLAY` past the file end. The answers above are Wine's mciseq. [OPEN]
- Why Wine's mciseq reports lengths of 374000, 226000 and 506000 ms and ends ANTHEM.MID and MAIN.MID at 199130 and 290909. [OPEN]
