# Sound and music

This page covers the sound system of the original: the sound modes and the device test, the INI settings, the WaveMix session, the 82 sound slots, how ordinal 9 plays an id, the position and volume arithmetic of ordinal 8, the INSPACE wrappers, the MIDI music, the Options and Audio Options dialogs and the sound-id catalogue. Addresses without a module name are in INSPACE.EXE, whose DGROUP is seg19; ISPACE.DLL addresses carry the module name. The ordinal table and calling conventions are in [ispace.md](ispace.md), the INI key list in [formats.md](formats.md#inspaceini-inner-space) and the WAV resources in [resources.md](resources.md#100-wav-files-snd_fxdll-46-eng_fxdll-32). [DESIGN]

## Modes and device test
The INI reader ISPACE.DLL seg9:0x22ca reads the sound keys ([Settings](#settings)), calls the volume routine ISPACE.DLL seg9:0xa9e and then tests the wave device. Ordinal 7 command 0 calls it, and so does Options OK when the sound system changed. [BYTES ISPACE.DLL seg9:0x22ca seg9:0x2416 seg9:0x47e seg9:0x2207]

1. `waveOutGetNumDevs()` = 0 sets the capability level ISPACE.DLL DGROUP:0x227e to 0 and changes nothing else; the MIDI test is skipped, so ISPACE.DLL DGROUP:0x227c stays unset. [BYTES ISPACE.DLL seg9:0x2419 seg9:0x2422 seg9:0x2425]
2. Otherwise it calls `waveOutGetDevCaps(0, caps, 0x30)` and reads `AllowSound` (default 0). `dwSupport` is the dword at caps + 0x2c. [BYTES ISPACE.DLL seg9:0x2431 seg9:0x2444 seg9:0x244c] [REF Win16 SDK mmsystem.h WAVEOUTCAPS]
3. **PC-speaker refusal.** When `dwSupport` has `WAVECAPS_SYNC` (0x10) and `AllowSound` is 0, the level becomes 0, `Music`, `SoundFX` and `VoiceFX` become 0 in memory (ISPACE.DLL DGROUP:0x221e, DGROUP:0x694, DGROUP:0x696), and the reader writes them to INSPACE.INI as "0" in the order `Music`, `SoundFX`, `VoiceFX`. The MIDI test is skipped on this path too. [BYTES ISPACE.DLL seg9:0x2455 seg9:0x245e seg9:0x2483 seg9:0x24a9 seg9:0x24cf seg9:0x24d4] The manual says the game refuses PC-speaker drivers and that `AllowSound=1` forces audio on. [MANUAL Troubleshooting] The shipped WAVEMIX.INI notes that most PC-speaker drivers are synchronous. [BYTES WAVEMIX.INI]
4. Otherwise `midiOutGetDevCaps(0, caps, 0x32)` sets ISPACE.DLL DGROUP:0x227c to 3 with `MIDICAPS_VOLUME` (1) and `MIDICAPS_LRVOLUME` (2), 2 with the first only and 1 without `MIDICAPS_VOLUME`. The wave level ISPACE.DLL DGROUP:0x227e becomes 3 with `WAVECAPS_VOLUME` (4) and `WAVECAPS_LRVOLUME` (8), 2 with `WAVECAPS_VOLUME` only, and 1 whenever `WAVECAPS_VOLUME` is clear, even with `WAVECAPS_LRVOLUME` set. [BYTES ISPACE.DLL seg9:0x24df seg9:0x24e7 seg9:0x24eb seg9:0x250a seg9:0x250e seg9:0x2514 seg9:0x251c seg9:0x2524]
5. `SoundSystem` (default −1) goes to ISPACE.DLL DGROUP:0x2280. A −1 is replaced by the capability level and written back to INSPACE.INI. The value is then clamped from above to the capability level in memory only (signed compare). [BYTES ISPACE.DLL seg9:0x2538 seg9:0x2540 seg9:0x254d seg9:0x2569 seg9:0x25b0 seg9:0x25b7 seg9:0x25bc]

The sound system ISPACE.DLL DGROUP:0x2280 takes the values of the manual's list. [MANUAL Game Settings]

| Value | Manual name | What the code does |
|---|---|---|
| 0 | NO SOUND | Ordinal 9 returns 0 at once, the slot loader returns −1, command 0 opens no session, commands 3 and 4 do nothing and music requests only store the track. |
| 1 | STANDARD SOUND | Ordinal 8 and its music twin ISPACE.DLL seg9:0x3bf4 return at once, so effects never set a volume or pan. |
| 2 | DYNAMIC SOUND | Ordinal 8 writes separate left and right volume words. |
| 3 | 3D DYNAMIC SOUND | The same code as 2: nothing on the effect path compares the mode with 2 or 3, so the two differ only in what the device does with the words. |

[BYTES ISPACE.DLL seg9:0xc50 seg9:0x25d4 seg9:0x497 seg9:0xafc seg9:0xbb0 seg9:0x393e seg9:0x779 seg9:0x3c01]

- In dialogs-0001 the Options dialog shows Audio.., the Master Volume bar and the sound combo disabled, which is the capability-0 state. [ORACLE dialogs-0001]
- The manual describes DYNAMIC as distance volume and 3D DYNAMIC as added left/right position. [MANUAL Game Settings] In the code both come from the same left and right words ([Position and volume](#position-and-volume)). [BYTES ISPACE.DLL seg9:0x779]

## Settings
ISPACE.DLL seg9:0x22ca reads section `[Inner Space]` of INSPACE.INI with `GetPrivateProfileInt`; [formats.md](formats.md#inspaceini-inner-space) holds the key list with every read and write site. [BYTES ISPACE.DLL seg9:0x22d8 seg9:0x22e2]

| Key | Default | ISPACE.DLL DGROUP | Role | Read at |
|---|---|---|---|---|
| `SoundFX` | 1 | 0x694 | effects from `SND_FX.DLL` on | [BYTES ISPACE.DLL seg9:0x22e5] |
| `VoiceFX` | 1 | 0x696 | voices from `<Language>_FX.DLL` on | [BYTES ISPACE.DLL seg9:0x22fb] |
| `Music` | 1 | 0x221e | music on | [BYTES ISPACE.DLL seg9:0x2311] |
| `SoundVolume` | 100 | 0x69e | master volume M | [BYTES ISPACE.DLL seg9:0x2327] |
| `FXMusic` | 50 | 0x6a2 | effects-versus-music balance | [BYTES ISPACE.DLL seg9:0x233d] |
| `StereoEffect` | 2 | 0x228c | distance scaling of ordinal 8 | [BYTES ISPACE.DLL seg9:0x2353] |
| `PurgeSound` | 1 | 0x698 | purge deferred slots at command 2 | [BYTES ISPACE.DLL seg9:0x2369] |
| `StereoSwap` | 0 | 0x69a | swap left and right | [BYTES ISPACE.DLL seg9:0x237f] |
| `AllowSound` | 0 | local | allow a synchronous device | [BYTES ISPACE.DLL seg9:0x2444] |
| `SoundSystem` | −1 | 0x2280 | sound mode | [BYTES ISPACE.DLL seg9:0x2538] |
| `MixChannels` | 4 | 0x219e | channel count, read in the session set-up | [BYTES ISPACE.DLL seg9:0x55d seg9:0x571 seg9:0x576] |
| `ForceVol` | 0 | — | read by command 4 | [BYTES ISPACE.DLL seg9:0xbde] |
| `SaverVolume` | 0 | 0x69e | read by command 10 in place of `SoundVolume` | [BYTES ISPACE.DLL seg9:0x6e seg9:0x73] |
| `SndPath` | "" | 0x21b0 | directory of the FX DLLs, the MIDI files and the id −1 WAV files | [BYTES ISPACE.DLL seg9:0x152] |

- The reader also reads non-sound keys (`JoyStick`, `4Btns`, `JMinX` … `JRangeY`, `Enhance`, `Detail`, `Quick`) and calls ISPACE.DLL seg9:0x35ba, which allocates the joystick's 8-byte block. [BYTES ISPACE.DLL seg9:0x239b seg9:0x257c seg9:0x2412]
- INSPACE reads `MixChannels` itself with default 1 into DGROUP:0x920a ([formats.md](formats.md#inspaceini-inner-space)). [BYTES seg6:0xc9c seg6:0xca7]
- **SndPath.** The `SndPath` buffer is 0x6d bytes. ISPACE.DLL seg9:0x162 appends `\` (ISPACE.DLL DGROUP:0x7b2) when the byte at ISPACE.DLL DGROUP:0x21af + length is not `\`. For an empty `SndPath` that byte is the high byte of the window word ISPACE.DLL DGROUP:0x21ae, so `\` is appended unless that byte is 0x5c, and the files resolve to `\main.mid` and so on at the root of the current drive. [BYTES ISPACE.DLL seg9:0x141 seg9:0x152 seg9:0x162 seg9:0x171]
- **FX DLL path.** The module loader ISPACE.DLL seg9:0xba opens module 0 as `SndPath` + `SND_FX.DLL` and module 1 as `SndPath` + `<Language>` + `_FX.DLL`, tests that path with `OpenFile(OF_EXIST)` and loads it with `LoadLibrary`. Every call reads `Path`, `LocalPath` and `SndPath` again before its loaded-module test, so each module load looks in the current `SndPath`. [BYTES ISPACE.DLL seg9:0xba seg9:0x152 seg9:0x17f seg9:0x26c seg9:0x283 seg9:0x286 seg9:0x2ca seg9:0x1fe seg9:0x340]

## Session
**Command 0.** Ordinal 7 command 0 (ISPACE.DLL seg9:0x448) starts the sound system: [BYTES ISPACE.DLL seg9:0x448]
1. A negative argument low word is negated and selects the reduced mode; a non-zero argument goes to ISPACE.DLL DGROUP:0x19c8/0x19ca as the two wrap periods (INSPACE passes 3200 and 3200, [ispace.md](ispace.md#ordinal-notes)). [BYTES ISPACE.DLL seg9:0x455 seg9:0x45b]
2. It calls the INI reader and loads modules 0 (`SND_FX.DLL`) and 1 (`<Language>_FX.DLL`) with ISPACE.DLL seg9:0xba. [BYTES ISPACE.DLL seg9:0x476 seg9:0x47e seg9:0x48a]
3. When ISPACE.DLL DGROUP:0x2280 ≠ 0 it calls the session set-up ISPACE.DLL seg9:0x4e0(0, reduced). [BYTES ISPACE.DLL seg9:0x497 seg9:0x4a4]
4. It preloads slots 0–0x51 with ISPACE.DLL seg9:0x25c6(id, 0), or only slot 0x23 in the reduced mode. [BYTES ISPACE.DLL seg9:0x4b0 seg9:0x4c2]

**Session set-up** ISPACE.DLL seg9:0x4e0(reopen, reduced):
1. ISPACE.DLL seg9:0x362e loads `WAVEMIX.DLL` when `<Windows directory>\WAVEMIX.DLL` exists ([ispace.md](ispace.md#what-ispacedll-owns)). Without a handle (ISPACE.DLL DGROUP:0x6b4 = 0) the channel count ISPACE.DLL DGROUP:0x219e is 1. Otherwise it is `MixChannels`, forced to 1 in the reduced mode. [BYTES ISPACE.DLL seg9:0x4ee seg9:0x4f8 seg9:0x571 seg9:0x57f]
2. With a `WAVEMIX.DLL` handle, `reopen` = 0 increments the reference count ISPACE.DLL DGROUP:0x219c and returns early when the count exceeds 1, so a second command 0 opens nothing. A non-zero `reopen` (from Audio OK, ISPACE.DLL seg9:0x3582) calls ISPACE.DLL seg9:0x624(1), sets the count to 1 only when it is 0, leaves it unchanged otherwise and rebuilds the session; it never increments the count. Without a handle the count is not touched. [BYTES ISPACE.DLL seg9:0x579 seg9:0x585 seg9:0x58e seg9:0x592 seg9:0x599 seg9:0x5a2 seg9:0x5ab seg9:0x5ad seg9:0x3582]
3. It creates the hidden `NotifyClass` window once into ISPACE.DLL DGROUP:0x21ae: `RegisterClass` in ISPACE.DLL seg9:0x2a60 with class style 0x4000 (`CS_GLOBALCLASS`) and window procedure ISPACE.DLL seg9:0x2ac4 (ordinal 31), class name at ISPACE.DLL DGROUP:0x9aa. It then stops all effects with ISPACE.DLL seg9:0x1174. [BYTES ISPACE.DLL seg9:0x534 seg9:0x53d seg9:0x2a70 seg9:0x2ab8]
4. With one channel the session handle ISPACE.DLL DGROUP:0x21aa stays 0 and every play goes through `sndPlaySound` ([the single-channel path](#playing)). [BYTES ISPACE.DLL seg9:0x547 seg9:0x1304]
5. Otherwise `WaveMixGetInfo` (with `wSize` 0x14 at ISPACE.DLL DGROUP:0x171c) must return 0. The MIXCONFIG at ISPACE.DLL DGROUP:0x213e is {`wSize` 10, `dwFlags` 1 = `WMIX_CONFIG_CHANNELS`, `wChannels` 1}, so the session is **mono** and `wSamplingRate` is not set. `WaveMixConfigureInit` returns the session into ISPACE.DLL DGROUP:0x21aa; a 0 result returns 0. [BYTES ISPACE.DLL seg9:0x5b9 seg9:0x5c3 seg9:0x5cf seg9:0x5d5 seg9:0x5e1 seg9:0x5eb] [REF Microsoft WaveMix 1.x wavemix.h]
6. `WaveMixOpenChannel(session, channel count, 2 = WMIX_OPENCOUNT)` follows. Only a 0 result is followed by `WaveMixActivate(session, TRUE)` and ISPACE.DLL DGROUP:0x2222 = 1; a non-zero result still returns 1, with the session open but inactive, so nothing plays. [BYTES ISPACE.DLL seg9:0x5fd seg9:0x5ff seg9:0x608 seg9:0x610 seg9:0x614]

- **Rate.** The session rate comes from WAVEMIX.INI: the shipped `[default]` section has `SamplesPerSec=11` (11,025 Hz) and `WaveBlocks=3`, and device-named sections override it, for example `[SB16 Wave Out]` with `SamplesPerSec=22`. [BYTES WAVEMIX.INI]
- **Single channel.** The single-channel path receives no completion message, so its channel record stays set until ISPACE.DLL seg9:0x1174. [BYTES ISPACE.DLL seg9:0x2ac4 seg9:0x11b6]
- **Channel records.** The 12-byte record of channel c at ISPACE.DLL DGROUP:0x1760 + 12c holds +0 id, +4 start time (dword), +8 q and +0xa the voice bit; a priority channel ISPACE.DLL DGROUP:0x21a2 of 8 means none. [BYTES ISPACE.DLL seg9:0x11b6 seg9:0x11ba seg9:0x11c2 seg9:0x11ca seg9:0xd9c]
- In wave-0001 (`SoundSystem=0`) the game never loads `WAVEMIX.DLL`. [ORACLE wave-0001]
- The manual offers 1–8 channels and says one channel plays one sound at a time. [MANUAL Game Settings]

## Slots
Ids 0x00–0x51 are 82 slots. The loader ISPACE.DLL seg9:0x25c6 first stores 2 into the flag byte ISPACE.DLL DGROUP:0x2224[id] and then dispatches on the id through the 82-entry table at ISPACE.DLL seg9:0x2660 (bound 0x51); ids 0x21 and 0x41 keep the preset 2. [BYTES ISPACE.DLL seg9:0x25fa seg9:0x2650 seg9:0x265b seg9:0x2660 seg9:0x2704]

| Flag | Ids |
|---|---|
| 0 | 0x00, 0x03, 0x06, 0x08, 0x11, 0x12, 0x23, 0x24, 0x25, 0x27, 0x45 |
| 1 | 0x26 |
| 2 | 0x01, 0x02, 0x04, 0x07, 0x13–0x22, 0x29, 0x2b, 0x41, 0x46 |
| 3 | 0x2a, 0x2c–0x40, 0x44, 0x47, 0x48, 0x49 |
| 4 | 0x28 |
| 6 | 0x05, 0x09–0x10, 0x42, 0x4a, 0x4e |
| 7 | 0x43, 0x4b, 0x4c, 0x4d, 0x4f, 0x50, 0x51 |

[BYTES ISPACE.DLL seg9:0x2603 seg9:0x260e seg9:0x2619 seg9:0x2624 seg9:0x262f seg9:0x263a seg9:0x2645]

- **Bit 0** picks the module k: 0 is `SND_FX.DLL`, 1 is `<Language>_FX.DLL` (`ENG_FX.DLL` for English), with handles at ISPACE.DLL DGROUP:0x68c + 2k. [BYTES ISPACE.DLL seg9:0x2707 seg9:0x271f]
- **Bit 1** defers loading to the first play (the command-0 pass skips the slot) and makes the slot purgeable at command 2 while `PurgeSound` ≠ 0. [BYTES ISPACE.DLL seg9:0x2729 seg9:0x2732 seg9:0x28e6 seg9:0x28f6]
- **Bit 2** forces that purge regardless of `PurgeSound`, but it is tested only after bit 1, so id 0x28 (flag 4) is never purged by command 2. [BYTES ISPACE.DLL seg9:0x28e6 seg9:0x2900]
- **Resource rule.** Slot s is resource type 100, id s + 100, of module k: `FindResource(module[k], MAKEINTRESOURCE(s + 100), MAKEINTRESOURCE(100))` on both the single-channel and the WaveMix path. ISPACE.DLL has no stored id-to-resource table. [BYTES ISPACE.DLL seg9:0x2771 seg9:0x2774 seg9:0x2780 seg9:0x27d7 seg9:0x27e3]
- **Present resources.** `SND_FX.DLL` holds 46 WAVs (100–111, 113–137, 139–141, 143, 166, 169, 170, 174, 178) and `ENG_FX.DLL` 32 (138, 144–164, 167, 168, 171–173, 175–177, 179, 180). Every one is PCM, 8-bit, mono, 11,025 Hz. [BYTES SND_FX.DLL 100] [BYTES ENG_FX.DLL 100]
- **Missing resources.** 112 (slot 0x0c), 142 (slot 0x2a), 165 (slot 0x41) and 181 (slot 0x51) are in neither DLL. Ordinal 9 plays slot 0x0b for id 0x0c; the other three slots load nothing and never play. [BYTES SND_FX.DLL 100] [BYTES ENG_FX.DLL 100] [BYTES ISPACE.DLL seg9:0xdb6]

**Loader** ISPACE.DLL seg9:0x25c6(id, now):
1. It returns −1 while the sound system is 0, and −3 when the slot is already loaded or the module handle is 0. [BYTES ISPACE.DLL seg9:0x25d4 seg9:0x25db seg9:0x25f1 seg9:0x2726]
2. It returns −4 when `now` = 0 and bit 1 is set, when bit 0 is set and `VoiceFX` = 0, or when bit 0 is clear and `SoundFX` = 0. [BYTES ISPACE.DLL seg9:0x2729 seg9:0x2732 seg9:0x2739 seg9:0x273f seg9:0x2750]
3. With one channel it keeps the `LoadResource` handle in ISPACE.DLL DGROUP:0x17d8[id] and the locked RIFF image in ISPACE.DLL DGROUP:0x187c[id]. [BYTES ISPACE.DLL seg9:0x2791 seg9:0x27a4]
4. With WaveMix it needs ISPACE.DLL DGROUP:0x21aa and DGROUP:0x2222 non-zero. It zeroes the 0x42-byte MMIOINFO at ISPACE.DLL DGROUP:0x215a, sets `fccIOProc` 'MEM ', `cchBuffer` = `GlobalSize` and `pchBuffer` = the locked image, calls `WaveMixOpenWave(session, &mmioinfo, module, 4 = WMIX_MEMORY)`, keeps the wave in ISPACE.DLL DGROUP:0x187c[id] and unlocks and frees the resource at once. [BYTES ISPACE.DLL seg9:0x2823 seg9:0x2849 seg9:0x2871 seg9:0x2890 seg9:0x28a8]
5. It returns 1 on success and 0 on failure; ISPACE.DLL decompresses nothing. [BYTES ISPACE.DLL seg9:0x25c6]

**Purge** ISPACE.DLL seg9:0x28d2(id, force) frees a loaded slot when `force` ≠ 0, or when bit 1 is set and either `PurgeSound` ≠ 0 or bit 2 is set. [BYTES ISPACE.DLL seg9:0x28dd seg9:0x28e6 seg9:0x28f6 seg9:0x2900]
- Command 2 (ISPACE.DLL seg9:0x73a) stops all effects and purges all 82 slots with `force` 0, so `PurgeSound` = 0 ("Keep Sounds in Memory") keeps the deferred slots without bit 2. [BYTES ISPACE.DLL seg9:0x748 seg9:0x756]
- Command 1 (ISPACE.DLL seg9:0x6a0) purges with `force` = (the `SND_FX.DLL` reference count ISPACE.DLL DGROUP:0x19d8 ≤ 1), releases modules 0 and 1, frees the block ISPACE.DLL DGROUP:0x2298 and sets the saved system volumes. [BYTES ISPACE.DLL seg9:0x6c9 seg9:0x6f0 seg9:0x70c seg9:0x717]
- The block ISPACE.DLL DGROUP:0x2298 is not a sound buffer: ISPACE.DLL seg9:0x35ba allocates it as `GlobalAlloc(0x42, 8)` with its far pointer in ISPACE.DLL DGROUP:0x19e4, and only joystick code uses it (ISPACE.DLL seg9:0x40da, seg9:0x4298, seg9:0x48f8, seg9:0x4d02). [BYTES ISPACE.DLL seg9:0x35cd seg9:0x35d3 seg9:0x35e5 seg9:0x416a seg9:0x4613 seg9:0x4935 seg9:0x4d16]

**"Compressed" (SND-09).** The Options dialog calls `LoadString(SND_FX.DLL, 2314)` and only when that succeeds puts ENG_LANG.DLL string 2314 "Compressed" into control 144; control 146 does the same for `<Language>_FX.DLL`, and a module that is not loaded shows 1148 "NONE". [BYTES ISPACE.DLL seg9:0x1b36 seg9:0x1b55 seg9:0x1b71 seg9:0x1b91 seg9:0x1bb5 seg9:0x1bf1] [BYTES ENG_LANG.DLL STRING 145] id 2314 [BYTES ENG_LANG.DLL STRING 72] id 1148
- The shipped FX DLLs have no string table, so a loaded module shows the template's "Normal". [BYTES SND_FX.DLL 100] [BYTES ENG_FX.DLL 100]
- Whether a build of the FX DLLs with string 2314 exists is [OPEN].

## Playing
Ordinal 9 (ISPACE.DLL seg9:0xc42) plays id `id` with position `p`, hold flag `q`, listener offsets `r`, `s`, play area, time `now` and file name `name` ([ispace.md](ispace.md#the-23-ordinals-inspace-uses)). A local loop word L (default 0) comes from the remap. [BYTES ISPACE.DLL seg9:0xc42 seg9:0xc6e]

1. It returns 0 while the sound system is 0, and for id −1 while `VoiceFX` = 0. [BYTES ISPACE.DLL seg9:0xc50 seg9:0xc5c seg9:0xc63]
2. It remaps the id (table below). [BYTES ISPACE.DLL seg9:0xc72]
3. With L = 0 it returns 0 when the effects volume ISPACE.DLL DGROUP:0x6a0 is 0 or when `now` is below the busy-until dword ISPACE.DLL DGROUP:0xcf2 (unsigned). [BYTES ISPACE.DLL seg9:0xcdb seg9:0xce5 seg9:0xcef seg9:0xcf6 seg9:0xd03]
4. For a slot id it tests the dword ISPACE.DLL DGROUP:0x187c[id] and, only when that is 0, calls the loader ISPACE.DLL seg9:0x25c6(id, 1), ignoring its result. It then tests DGROUP:0x187c[id] again and returns 0 when the slot is still empty; a `p.x` of −11111 (0xd499) then returns 0 without playing, which makes a preload. The file id −1 (its local file flag [bp − 6] ≠ 0) jumps straight to the admission at ISPACE.DLL seg9:0xe6a. [BYTES ISPACE.DLL seg9:0xe31 seg9:0xe35 seg9:0xe37 seg9:0xe45 seg9:0xe47 seg9:0xe53 seg9:0xe61 seg9:0xe63 seg9:0xe6a seg9:0xe81]
5. g = the admission ISPACE.DLL seg9:0x10ae(id, q, L); −1 returns 0, and g ≠ 0 calls ordinal 8 before the play. [BYTES ISPACE.DLL seg9:0xe75 seg9:0xe9d]
6. It sets busy-until to `now`, plus the hold of the remapped id when `q` ≠ 0. [BYTES ISPACE.DLL seg9:0xea6 seg9:0xead seg9:0xec8]
7. It calls the player ISPACE.DLL seg9:0x11f4; a 0 result returns 0 and leaves busy-until set. [BYTES ISPACE.DLL seg9:0x101b]
8. After a successful play it stores the 0x66 and 0x67 memories, calls ordinal 8 again when g ≠ 0, sets ISPACE.DLL DGROUP:0xcf6 = `now` + 1200 for id 0x1c with fewer than 2 channels, resets busy-until to `now` when `q` = 0 and returns 1 only when g ≠ 0. [BYTES ISPACE.DLL seg9:0x1037 seg9:0x1057 seg9:0x105a seg9:0x106d seg9:0x107a seg9:0x108d seg9:0x1099]

**Remaps.** [BYTES ISPACE.DLL seg9:0xc72 seg9:0xcca seg9:0xd0e seg9:0xd5a seg9:0xd7d seg9:0xd8f seg9:0xda4 seg9:0xdb6 seg9:0xdbe seg9:0xdcc seg9:0xdda seg9:0xde7 seg9:0xdf5 seg9:0x1037 seg9:0x1099]

| Id | Plays | Loop word L |
|---|---|---|
| −1 | the file `SndPath` + `name` + `.WAV`, booked as slot 0x11 | 0 |
| 0x0c | slot 0x0b | 0 |
| 0x26 | slot 0x23 while `VoiceFX` = 0, else 0x26 | 0 |
| 0x64 | nothing: returns 0 | — |
| 0x65 | 0x30 + rand() % 3 | 0 |
| 0x66 | 0x47 + v with v = rand() % 3, stepped to (v + 1) mod 3 when v equals ISPACE.DLL DGROUP:0xcfc; a successful play stores v there | 0 |
| 0x67 | 0x49 while ISPACE.DLL DGROUP:0xcfa = 0, else 0x2e; a successful play stores (DGROUP:0xcfa = 0) there, so the two alternate | 0 |
| 0x68 | slot 0x14 | 0x14 |
| 0x6f | slot 0x17 | 0x14 |
| 0x70 | slot 0x4a | 8 |
| 0x75 | slot 0x46 | 7 |
| 0x69, 0x6e | stop commands (below) | — |

- `rand()` is ISPACE.DLL's own ([rng.md](rng.md)). [BYTES ISPACE.DLL seg9:0xcca seg3:0x668]
- A non-zero loop word skips the volume and busy-until checks of step 3. [BYTES ISPACE.DLL seg9:0xcdb seg9:0xce2]
- **Stop ids.** Id 0x6e calls the stop routine ISPACE.DLL seg9:0x1174. Id 0x69 with more than one channel only resets the priority (ISPACE.DLL DGROUP:0x21a0 = 0, DGROUP:0x21a2 = 8) and with one channel calls ISPACE.DLL seg9:0x1174. Both then clear busy-until and return 1, behind the sound-off check of step 1. [BYTES ISPACE.DLL seg9:0xd8f seg9:0xd96 seg9:0xd9c seg9:0xda4 seg9:0xdaa seg9:0xdb0]
- **Stop routine.** ISPACE.DLL seg9:0x1174 resets the priority, calls `WaveMixFlushChannel(session, 0, 1 = WMIX_ALL)` when a session is open and active, zeroes the counters ISPACE.DLL DGROUP:0x21a8 and DGROUP:0x21a4, clears the records of channels 0 to count − 1 and, with one channel, calls `sndPlaySound(NULL, 0)`. [BYTES ISPACE.DLL seg9:0x1181 seg9:0x11a5 seg9:0x11ab seg9:0x11b6 seg9:0x11e8]

**Holds.** With `q` ≠ 0 the switch at ISPACE.DLL seg9:0xf93 (36 entries, ids 0x26–0x49) adds a hold to busy-until; every other id adds 0. [BYTES ISPACE.DLL seg9:0xf88 seg9:0xf93 seg9:0xf98]

| Id | Hold (ms) | Id | Hold (ms) | Id | Hold (ms) |
|---|---|---|---|---|---|
| 0x26 | 900 | 0x32 | 1100 | 0x3d | 350 |
| 0x2c | 730 | 0x33 | 1250 | 0x3e | 640 |
| 0x2d | 900 | 0x34 | 510 | 0x3f | 750 |
| 0x2e | 1000 | 0x35 | 300 | 0x40 | 520 |
| 0x2f | 900 | 0x36 | 700 | 0x47 | 660 |
| 0x30 | 600 | 0x37 | 540 | 0x48 | 960 |
| 0x31 | 1300 | 0x38 | 300 | 0x49 | 700 |

**Admission** ISPACE.DLL seg9:0x10ae(id, q, L) returns −1 (reject), 0 (play without pan and without priority) or 1 (play as the priority sound). With cur the priority id ISPACE.DLL DGROUP:0x21a0, cc its channel ISPACE.DLL DGROUP:0x21a2 and R the record of channel cc, the first matching rule decides: [BYTES ISPACE.DLL seg9:0x10bb seg9:0x10d3 seg9:0x10ec seg9:0x110c seg9:0x1114 seg9:0x1124 seg9:0x1131 seg9:0x1138 seg9:0x1145 seg9:0x114e seg9:0x1155 seg9:0x1163]
1. `q` = 0 while R's q ≠ 0: −1.
2. `q` ≠ 0, R's q = 0 and the held count ISPACE.DLL DGROUP:0x21a4 = 0: −1 for id 1 while the active count ISPACE.DLL DGROUP:0x21a8 ≠ 0, otherwise stop all and 1.
3. cur = 1: stop all and 1.
4. id 1: 0.
5. A voice id (flag bit 0): 1.
6. L ≠ 0 with one channel: 1.
7. id ≥ cur (signed): 1.
8. A voice cur: 0.
9. id 0x1a: 1 while cur < 0x20, else 0.
10. id 0x1c: 1.
11. Otherwise 0.

**Player** ISPACE.DLL seg9:0x11f4 picks a channel, plays and books the record: [BYTES ISPACE.DLL seg9:0x1207 seg9:0x1238 seg9:0x124d seg9:0x1282 seg9:0x1298 seg9:0x12b8 seg9:0x12c9 seg9:0x12ce]
1. Ids other than 0x2b, 0x27, 8 and 0x14 reuse the channel whose record has the same id and started less than 80 ms ago.
2. Otherwise it takes the first record with id 0.
3. Otherwise it takes the oldest record (start below `now` + 500, strictly older wins) that is not channel cc and not a voice record with q set.
4. Otherwise it takes channel 0.

- **Single channel.** `sndPlaySound(image, flags)` with flags 3 (`SND_ASYNC | SND_NODEFAULT`) for a file, 0x0f (adds `SND_MEMORY | SND_LOOP`) when L ≠ 0, and 7 (`SND_ASYNC | SND_NODEFAULT | SND_MEMORY`) otherwise. `SND_LOOP` ignores the count and loops until the next play or stop. [BYTES ISPACE.DLL seg9:0x1304 seg9:0x1311 seg9:0x131e seg9:0x1325 seg9:0x1333] [REF Win16 SDK mmsystem.h sndPlaySound]
- **WaveMix.** A file is first opened with `WaveMixOpenWave(session, name, 0, 0)` into ISPACE.DLL DGROUP:0x173c[channel]. The MIXPLAYPARAMS at ISPACE.DLL DGROUP:0x2148 are {`wSize` 0x12, session, channel, wave, notify window ISPACE.DLL DGROUP:0x21ae, `dwFlags` 5 = `WMIX_CLEARQUEUE | WMIX_HIPRIORITY`, `wLoops` L}, and `WaveMixPlay` returning 0 is success. Without an open, active session the play fails. [BYTES ISPACE.DLL seg9:0x1333 seg9:0x134c seg9:0x1367 seg9:0x1383 seg9:0x1395 seg9:0x13a4 seg9:0x13be] [REF Microsoft WaveMix 1.x wavemix.h]
- **Booking.** After a successful play the record becomes {id, `GetCurrentTime`, q, flag & 1}; a record that held id 0 increments ISPACE.DLL DGROUP:0x21a8, a newly held one increments DGROUP:0x21a4, and g ≠ 0 makes the sound the priority (ISPACE.DLL DGROUP:0x21a0 = id, DGROUP:0x21a2 = channel). [BYTES ISPACE.DLL seg9:0x13ef seg9:0x1404 seg9:0x140f seg9:0x143d]
- **Completion.** The `NotifyClass` procedure handles `MM_WOM_DONE` (0x3bd) with wParam = channel: the priority channel resets the priority (channel 8, id 0), a file wave on that channel is freed with `WaveMixFreeWave`, the record is cleared, DGROUP:0x21a4 is recounted over the open channels and DGROUP:0x21a8 decremented. [BYTES ISPACE.DLL seg9:0x2adf seg9:0x2b68 seg9:0x2b8c seg9:0x2b9a seg9:0x2ba8 seg9:0x2bcc seg9:0x2be6]

## Position and volume
**Ordinal 8** (ISPACE.DLL seg9:0x76c) takes the play area (w, h), the listener offsets (b, a) and the position p. It returns 0 at once while the sound system is at most 1. With a ≠ 0 it stores a, b, w and h in ISPACE.DLL DGROUP:0x2282, 0x2284, 0x19c4 and 0x19c6. It then refreshes the music volume through ISPACE.DLL seg9:0x38e2 ([Music](#music)). [BYTES ISPACE.DLL seg9:0x779 seg9:0x785 seg9:0x7a4]

1. A `p.x` of −6000 gives A = B = 80 and V = 100. [BYTES ISPACE.DLL seg9:0x7a9]
2. dx = p.x − half(w) − a, where half truncates toward zero. When |dx| > half(Px), with Px the wrap period ISPACE.DLL DGROUP:0x19c8, p.x moves by Px toward the listener and dx is recomputed. [BYTES ISPACE.DLL seg9:0x7c1 seg9:0x7e6 seg9:0x7f0]
3. d = |dx|, scaled by `StereoEffect`: 0 gives d / 4, 1 gives d / 2, 3 gives d + d / 2, and any other value keeps d. [BYTES ISPACE.DLL seg9:0x823 seg9:0x833 seg9:0x83f]
4. d < 300: A = d / 60 + 95, B = 10d / (−35) + 95. d < 600: A = (d − 300) / (−6) + 100, B = (d − 300) / (−30) + 10. d < 900: A = 10(d − 600) / (−85) + 50, B = 0. Otherwise A = 15, B = 0. [BYTES ISPACE.DLL seg9:0x86c seg9:0x879 seg9:0x8a2 seg9:0x8c0 seg9:0x8ca]
5. dy and e come from p.y, h, b and the period ISPACE.DLL DGROUP:0x19ca the same way, with the same scaling. e < 300: V = e / (−12) + 100. e < 600: V = 10(e − 300) / (−55) + 75. e < 900: V = (e − 600) / (−60) + 20. Otherwise V = 15. [BYTES ISPACE.DLL seg9:0x948 seg9:0x972 seg9:0x98d seg9:0x9a7 seg9:0x9ac]
6. dx < 0 gives left = A, right = B; otherwise left = B, right = A. Every division is signed and truncates toward zero. [BYTES ISPACE.DLL seg9:0x9c9]
7. R' = V · right / 100 and L' = V · left / 100 (unsigned). ISPACE.DLL DGROUP:0x21a6 = ISPACE.DLL seg2:0x850(L', R'), one Newton step of √(L'² + R'²) from max + min / 2 (1 when that start value is 0). Below 65 the priority channel's record loses its q. [BYTES ISPACE.DLL seg9:0x9ec seg9:0x9f7 seg9:0xa01 seg2:0x850]
8. Each side becomes min(x · E / 100, 100) · 65535 / 100 (unsigned 32-bit), with E the effects volume ISPACE.DLL DGROUP:0x6a0. `StereoSwap` ≠ 0 swaps the sides. ISPACE.DLL DGROUP:0x6a4 takes the left word and DGROUP:0x6a6 the right word. [BYTES ISPACE.DLL seg9:0xa0e seg9:0xa1c seg9:0xa25 seg9:0xa29 seg9:0xa5b seg9:0xa7a seg9:0xa84]
9. ISPACE.DLL seg9:0xb62 calls `waveOutSetVolume(0, right << 16 | left)` only when the dword differs from the cache ISPACE.DLL DGROUP:0xcee, which it updates first. Ordinal 8 returns 0 when the volume is unchanged or set, and 1 when the call fails. [BYTES ISPACE.DLL seg9:0xb6c seg9:0xb8e seg9:0xa8a]

- **One device-wide volume.** There is no per-channel volume and no sample scaling: all effect loudness and pan reach the output only through this one `waveOutSetVolume` of device 0, set by the priority sound at its play and at every ordinal 8 call. [BYTES ISPACE.DLL seg9:0xb8e seg9:0x13be]
- **Volumes.** ISPACE.DLL seg9:0xa9e computes, with M = ISPACE.DLL DGROUP:0x69e (`SoundVolume`, or `SaverVolume` after command 10) and F = `FXMusic`, the effects volume ISPACE.DLL DGROUP:0x6a0 = M when F ≤ 50, else M · 2 · (100 − F) / 100, and the music volume ISPACE.DLL DGROUP:0x2288 = M when F ≥ 50, else F · 2 · M / 100. The products are 16-bit `imul` results divided with `idiv`. [BYTES ISPACE.DLL seg9:0xaab seg9:0xab7 seg9:0xaca seg9:0xacd seg9:0xad9 seg9:0xae8]
  - Its callers are command 10, Master Volume, the INI reader and Audio's balance bar. [BYTES ISPACE.DLL seg9:0x77 seg9:0x1d9e seg9:0x2416 seg9:0x320c]
- **Focus lost.** Command 3 (ISPACE.DLL seg9:0xaf2) does nothing while the sound system is 0. Otherwise it stops the effects with ISPACE.DLL seg9:0x1174 (nothing resumes them), reads `midiOutGetVolume(1)` into ISPACE.DLL DGROUP:0x6a8 and `waveOutGetVolume(0)` into DGROUP:0x6a4, pauses playing music with ISPACE.DLL seg9:0x3a48(2), sets the MIDI volume to the saved system value ISPACE.DLL DGROUP:0x6b0 and the wave volume to DGROUP:0x6ac, and calls `WaveMixActivate(FALSE)` with ISPACE.DLL DGROUP:0x2222 = 0 when a session exists. [BYTES ISPACE.DLL seg9:0xafc seg9:0xb04 seg9:0xb0d seg9:0xb18 seg9:0xb1f seg9:0xb2c seg9:0xb3d seg9:0xb50]
- **Focus gained.** Command 4 (ISPACE.DLL seg9:0xba6) does nothing while the sound system is 0. Otherwise it reads the system volumes into ISPACE.DLL DGROUP:0x6b0 and DGROUP:0x6ac (`ForceVol` ≠ 0 replaces both with 0xFFFFFFFF), sets the game volumes from DGROUP:0x6a8 and DGROUP:0x6a4, calls `WaveMixActivate(TRUE)` with DGROUP:0x2222 = 1 when a session exists, and resumes the music with ISPACE.DLL seg9:0x3b5c when ISPACE.DLL DGROUP:0x229a is set. [BYTES ISPACE.DLL seg9:0xbb0 seg9:0xbc0 seg9:0xbcb seg9:0xbde seg9:0xbe7 seg9:0xc01 seg9:0xc12 seg9:0xc25 seg9:0xc36]
- Loading `SND_FX.DLL` the first time (ISPACE.DLL seg9:0xba(0)) also saves the system volumes. [BYTES ISPACE.DLL seg9:0x17f seg9:0x25c seg9:0x267]
- **Device ids.** Every `midiOutGetVolume` and `midiOutSetVolume` uses device 1, not `MIDI_MAPPER` and not 0; every wave volume call uses device 0. [BYTES ISPACE.DLL seg9:0xb07 seg9:0xb12 seg9:0xbba seg9:0xbc5 seg9:0x3e6e]
- In wave-0001 the abort reloads the FX DLLs around `MIDIOUTSETVOLUME` and `WAVEOUTSETVOLUME` with 0xffffffff. [ORACLE wave-0001]

## INSPACE wrappers
- **seg10:0x744c(n)** calls seg10:0x754a(0x69, 0, 0, 0) and then seg10:0x754a(n, 0, 0, 0) unless n = 0x69, so every such sound first resets the effect priority, or stops the single channel. [BYTES seg10:0x745f seg10:0x7465 seg10:0x7475]
- **Calls.** INSPACE calls seg10:0x744c 152 times, seg10:0x754a 114 times (four of them inside seg10) and seg10:0x7944 4 times; [re/artifacts/sounds.tsv](../../re/artifacts/sounds.tsv) lists them per id. [BYTES seg10:0x745f seg10:0x7475 seg10:0x7c05 seg10:0x7c1c]

**seg10:0x754a(id, a, b, mode)** calls ordinal 9 through DGROUP:0x3c5c with id, p, q = seg10:0x7896(mode, p), r = s = 1, the area (DGROUP:0x90ea, DGROUP:0x90ec, or DGROUP:0x88c0, DGROUP:0x88c2 when DGROUP:0x9130 ≠ 0), `now` = DGROUP:0x1e50 and the name DGROUP:0x7e4: [BYTES seg10:0x699f seg10:0x7801 seg10:0x7820 seg10:0x785e]
1. Id 0x6e forces mode 0, and DGROUP:0x920c = 3 plays nothing. [BYTES seg10:0x7558 seg10:0x755e seg10:0x7563]
2. Outside mode 5, ids 0x6a–0x76 go through the 13-entry switch at seg10:0x765d ([Sequences](#inspace-wrappers)). [BYTES seg10:0x756c seg10:0x7574 seg10:0x7658]
3. p comes from the mode: [BYTES seg10:0x776c seg10:0x76d9 seg10:0x76e3 seg10:0x76ed seg10:0x7701 seg10:0x7706 seg10:0x7719 seg10:0x7734 seg10:0x773d seg10:0x7745 seg10:0x775c]

   | Mode | p |
   |---|---|
   | 0 | (−6000, −6000) |
   | 1 | words +4 and +6 of record b (stride 0x70) of pool a (far pointer DGROUP:0x1df8 + 4a); nothing plays when a > DGROUP:0x9324 (signed) |
   | 2 | the words at DGROUP:0x60c8 (x) and DGROUP:0x60e0 (y) + 0x94·b + 12·row, with row 1 when the word DGROUP:0x611c + 0x94·b is 0, else row 0 |
   | 3 | the dword DGROUP:0x5144 |
   | 4 | (a, b) |
   | 5 | (−11111, b): ordinal 9 loads the slot and plays nothing |

4. Modes 1–3 pass p through seg8:0x29ea and, when DGROUP:0x9156 ≠ 1 and DGROUP:0x9130 = 0, move it to ship + ((2 · (p − ship)) >> DGROUP:0x9156) per axis (arithmetic shift). [BYTES seg10:0x778b seg10:0x77c1]
5. q from seg10:0x7896 is 1 for mode 0, mode 4 or DGROUP:0x9130 ≠ 0. Otherwise the signed distance seg8:0x285a(ship, p) decides: above 800 gives −1, and seg10:0x754a then plays nothing and clears the pending follow-up (DGROUP:0x3c88, DGROUP:0x9216, DGROUP:0x9292); 341–800 gives 0; up to 340 gives 1. The `PtInRect` loop over the DGROUP:0x9158 rectangles at DGROUP:0x23e0 runs only in the ≤ 340 case and never changes the result. [BYTES seg10:0x78a9 seg10:0x78b5 seg10:0x78bc seg10:0x78e5 seg10:0x78ed seg10:0x78f7 seg10:0x78fa seg10:0x7905 seg10:0x792b seg10:0x7795]
6. A successful play stores id, a, b and mode in DGROUP:0x921c, DGROUP:0x928c, DGROUP:0x928e and DGROUP:0x9290, p in DGROUP:0x3b98 and q in DGROUP:0x9226. [BYTES seg10:0x786f seg10:0x7b22]

- **Mode-5 preloads.** The wave set-up seg9:0x6624 preloads 0x4f, 0x28 and 0x42, and seg14:0x1b84 preloads 5 and the ship's positive hit sound (SHP +0x1f). A preload reaches the slot loader only when seg10:0x7896 does not return −1 and ordinal 9's volume and busy-until checks pass. [BYTES seg9:0x6914 seg9:0x6924 seg9:0x6934 seg14:0x1e2d seg14:0x1fb9] [BYTES ISPACE.DLL seg9:0xe63]

**Sequences.** The switch plays the first id now, stores the follow-up in DGROUP:0x9292, sets the deadline DGROUP:0x3c88 = DGROUP:0x1e50 + delay and saves the first id, a and b in DGROUP:0x9216, DGROUP:0x9218 and DGROUP:0x921a. Ids 0x6e, 0x6f, 0x70 and 0x75 and ids above 0x76 clear DGROUP:0x3c88 and pass through; ids below 0x6a leave it alone. [BYTES seg10:0x7582 seg10:0x7587 seg10:0x7597 seg10:0x759a seg10:0x7642 seg10:0x764d seg10:0x7653 seg10:0x765d seg10:0x7677]

| Id | First | Follow-up | Delay (ms) |
|---|---|---|---|
| 0x6a | 0x20 | 0x21 | 1000 |
| 0x6b | 0x39 | 0x3a | 420 |
| 0x6c | 0x38 | 0x39 | 385 |
| 0x6d | 0x35 | 0x39 | 385 |
| 0x71 | 0x24 | 0x25 | 660 |
| 0x72 | 0x1a | 0x1a | 200 |
| 0x73 | 0x42 | 0x43 | 700 |
| 0x74 | 0x42 | 0x66 | 400 |
| 0x76 | 0x23 | 0x25 | 400 |

[BYTES seg10:0x7594 seg10:0x75b6 seg10:0x75cd seg10:0x75eb seg10:0x7600 seg10:0x7617 seg10:0x762f]

**seg10:0x7b48**, called once per wave frame from seg7:0xcaf, does two things. [BYTES seg7:0xcaf]
1. Once `GetCurrentTime` passes DGROUP:0x3c88 it plays 0x69 and then DGROUP:0x9292 with the last-played a, b and mode (DGROUP:0x928c, DGROUP:0x928e, DGROUP:0x9290). It does so only when DGROUP:0x920a > 1, or the saved a = 0xe and b = the player DGROUP:0x9646, or the last-played id, a and b equal the sequence's first id, a and b, or the follow-up is 0x66, or the first id is ≥ 0x38; otherwise the follow-up is dropped. Either way it clears DGROUP:0x3c88 and DGROUP:0x9292. [BYTES seg10:0x7b98 seg10:0x7bb6 seg10:0x7c05 seg10:0x7c1c seg10:0x7c28]
2. It recomputes p for the last played source by DGROUP:0x9290: mode 0 gives −6000, mode 1 the pool record (returning when a > DGROUP:0x9324), mode 2 the table, mode 3 the dword DGROUP:0x5144; mode 4 returns without a call and mode 5 passes an uninitialised p. For mode ≠ 0 it applies seg8:0x29ea and the DGROUP:0x9156 wrap with no DGROUP:0x9130 test, then calls ordinal 8(p, a = 1, b = 1, area = dword DGROUP:0x90ea); the listener offsets it computes at entry are overwritten by 1. [BYTES seg10:0x7b7f seg10:0x7b95 seg10:0x7cbc seg10:0x7cc0 seg10:0x7cc3 seg10:0x7cc9 seg10:0x7d30 seg10:0x7d3f]

- Ordinal 9 also passes r = s = 1, so the listener is the play-area centre plus 1 px. [BYTES seg10:0x7820] Whether seg8:0x29ea returns view pixels, which would put the 3200 wrap on view-pixel offsets, is [OPEN].
- **seg10:0x7944** takes the same (id, a, b, mode) arguments with an id read from a table: seg8:0x49dc, seg8:0x54e1 and seg8:0x5574 pass the ship hit sound DGROUP:0x9700 + 0xad·n, and seg11:0x89fd passes DGROUP:0x2f6c + 0x50·n. [BYTES seg8:0x49dc seg8:0x54e1 seg8:0x5574 seg11:0x89fd] seg10:0x7944(id, a, b, mode, far name) (cdecl) is seg10:0x754a without its sequence logic: [BYTES seg10:0x7944]
  1. It returns at once when DGROUP:0x920c = 3. [BYTES seg10:0x7952 seg10:0x7957]
  2. p by mode: 1 returns when a > DGROUP:0x9324 (signed) and otherwise takes words +4/+6 (`x`, `y`) of record b of pool a; modes above 3 leave p uninitialised. [BYTES seg10:0x79c0 seg10:0x79c2 seg10:0x79c8 seg10:0x79da seg10:0x79de]
  3. The listener words it computes from the player are overwritten by 1 before ordinal 9, as in seg10:0x754a; it has no id-0x6e test and no sequence switch. [BYTES seg10:0x7a49 seg10:0x7a4c seg10:0x7a4f seg10:0x79ae]
  4. Only for a = DGROUP:0x9324, b = the player and the player's ship byte +0xec ≠ 0 does it first call ordinal 9 with id 0x69. [BYTES seg10:0x7ab8 seg10:0x7ac0 seg10:0x7acc seg10:0x7ad3 seg10:0x7aef]
  5. On a successful play it stores id, a, b and mode in DGROUP:0x921c, 0x928c, 0x928e and 0x9290, p before seg8:0x29ea in DGROUP:0x3b98, and q in DGROUP:0x9226, so seg10:0x7b48 keeps the sound on its source; for a launch that is the missile. [BYTES seg10:0x79ec seg10:0x7b15 seg10:0x7b1c seg10:0x7b22 seg10:0x7b28 seg10:0x7b2e seg10:0x7b35 seg10:0x7b3c]
  - The launcher's name argument DGROUP:0x1118 is an empty string. Whether ordinal 9 treats it as it treats seg10:0x754a's DGROUP:0x7e4 for a non-file id is [OPEN]. [BYTES DGROUP:0x1118 seg11:0x89fd]
- **seg10:0x7480(code, flag)** is the music wrapper: [BYTES seg10:0x748d seg10:0x7499 seg10:0x74b7 seg10:0x74c9 seg10:0x74cd seg10:0x74d5 seg10:0x74f0 seg10:0x7504 seg10:0x7512 seg10:0x7524 seg10:0x7531 seg10:0x7538]
  1. Code 9 or 10 while DGROUP:0x9220 is 9 or 10 does nothing.
  2. Code 10 while DGROUP:0x9220 = 1 becomes 9.
  3. Code −2 saves the byte of DGROUP:0x9220 in DGROUP:0x7e0[DGROUP:0x73e] and sets DGROUP:0x9222 = −2; any other code sets DGROUP:0x9222 = −3, and code −3 becomes that saved track when it differs from DGROUP:0x9220.
  4. A code ≥ 0 goes to DGROUP:0x9220, with 7 stored as 2, and when the music hold DGROUP:0x2384 is below DGROUP:0x1e50 the hold becomes DGROUP:0x1e50 + 9000.
  5. When the ISPACE.DLL handle DGROUP:0x9210 ≠ 0 it calls ordinal 7 command 11 with the dword (code, flag).

## Music
**Files and tracks.** The music is four MIDI files in `SndPath`, named by the strings `main`, `race`, `anthem` and `demon` (ISPACE.DLL DGROUP:0xaef, 0xaf4, 0xaf9, 0xb00) plus `.mid` (ISPACE.DLL DGROUP:0xb06). [BYTES ISPACE.DLL seg9:0x39a3 seg9:0x39a8 seg9:0x39c3 seg9:0x39d1]
- All four are SMF format 1 with division 480 and no SysEx: MAIN.MID 97,465 bytes and 15 tracks, RACE.MID 55,863 and 13, ANTHEM.MID 33,595 and 14, DEMON.MID 57,931 and 13. Their lengths to the last end-of-track under the tempo map are 295.52 s, 295.26 s, 200.80 s and 223.98 s. [BYTES MAIN.MID] [BYTES RACE.MID] [BYTES ANTHEM.MID] [BYTES DEMON.MID]
- Each file carries the sequencer-specific meta `FF 7F 03 00 00 41` (the Microsoft three-byte manufacturer id) exactly once, in track 0 at file offset 23. [BYTES MAIN.MID] [BYTES RACE.MID] [BYTES ANTHEM.MID] [BYTES DEMON.MID]
- Every track carries an `FF 04` instrument-name meta "soumdblaster"; the `FF 03` track names are part names and "04MAIN.MID" and so on. [BYTES MAIN.MID] [BYTES RACE.MID] [BYTES ANTHEM.MID] [BYTES DEMON.MID]
- Channels 1–10 hold the extended-level parts and 13–16 a base-level arrangement of the same music, the Microsoft dual-mode MPC layout: MAIN, ANTHEM and DEMON have drums on 10 and 16 (MAIN 1477 note-ons on each), and RACE uses channels 1–9 and 13–15 with no drum channel. [BYTES MAIN.MID] [BYTES RACE.MID] [BYTES ANTHEM.MID] [BYTES DEMON.MID] [REF Microsoft MPC authoring guidelines]

A track t is a section of one file: word ISPACE.DLL DGROUP:0xaa8[t] is the start and word DGROUP:0xac0[t] the length, both in units of 100 ms, and byte DGROUP:0xad8[t] the file (0 main, 1 race, 2 anthem, 3 demon). [BYTES ISPACE.DLL DGROUP:0xaa8 DGROUP:0xac0 DGROUP:0xad8 seg9:0x3979 seg9:0x3abb seg9:0x3b86]

| Track | File | Start | Length | Plays (s) | At its end |
|---|---|---|---|---|---|
| 0 | anthem | 0 | 1810 | 0–181 | loops |
| 1 | main | 45 | 1495 | 4.5–154, from 0 when entered from track 2 | loops |
| 2 | main | 1700 | 1170 | 170–287, from 155 when entered from track 1 | loops |
| 3 | race | 0 | 290 | 0–29 | loops |
| 4 | race | 340 | 2550 | 34–289 | loops |
| 5 | anthem | 2000 | 166 | 200–216.6 | stops |
| 6 | demon | 190 | 2030 | 19–222 | loops |
| 7 | main | 2870 | 50 | 287–292 | continues as track 2 (from 170) |
| 8 | main | 2920 | 90 | 292–301 | continues as track 2 |
| 9 | demon | 0 | 89 | 0–8.9 | loops |
| 10 | demon | 89 | 101 | 8.9–19 | loops |
| 11 | anthem | 1810 | 190 | 181–200 | stops |

[BYTES ISPACE.DLL DGROUP:0xaa8 seg9:0x2b36 seg9:0x3af9 seg9:0x3b0f]

- Tracks 5 and 8 ask `MCI_PLAY` for an end past their files (216.6 s against ANTHEM's 200.80 s, 301.0 s against MAIN's 295.52 s). Whether the sequencer plays to the file end or rejects the play, which closes the device at ISPACE.DLL seg9:0x3bd1, is [OPEN].

**Globals.** ISPACE.DLL DGROUP:0x22a6 is the MCI device id (0 at start, 0xFFFF after a close) and DGROUP:0x2292 the state: 0 closed, 1 playing, 2 paused, 3 seeked and waiting, 4 hard-paused. DGROUP:0xaa2 is the open file, DGROUP:0xaa4 the current track, DGROUP:0xaa6 the previous track or control code, and DGROUP:0x229a the resume-on-focus flag. [BYTES ISPACE.DLL seg9:0x3840 seg9:0x3a34 seg9:0x3a5e seg9:0x3a88 seg9:0x3bd6 seg9:0x3a6b] DGROUP:0xaa2, DGROUP:0xaa4 and DGROUP:0xaa6 start at −1 in the initialised data, and the MIDI volume cache dword DGROUP:0xd06/0xd08 lies in BSS and starts at 0. [BYTES ISPACE.DLL DGROUP:0xaa2 DGROUP:0xaa4 DGROUP:0xaa6 DGROUP:0xd06 seg9:0x3e46]

**Requests.** Ordinal 7 command 11 calls ISPACE.DLL seg9:0x3906 with the track from the argument's low word and the flag from its high word: [BYTES ISPACE.DLL seg9:0x7c seg9:0x3913 seg9:0x3923 seg9:0x3926 seg9:0x3952 seg9:0x3959 seg9:0x3968 seg9:0x393b seg9:0x393e seg9:0x3948 seg9:0x3972 seg9:0x397e seg9:0x3984 seg9:0x398c seg9:0x3a05]
1. A code ≤ −1 in state 4 returns.
2. −5 returns.
3. −3 calls the play routine ISPACE.DLL seg9:0x3b5c (resume a paused track or start a seeked one).
4. −2 stores −2 in DGROUP:0xaa6 and calls the pause ISPACE.DLL seg9:0x3a48(2).
5. −1 stores −1 in DGROUP:0xaa6 and calls ISPACE.DLL seg9:0x3a48(4).
6. Any other value (a track; INSPACE never passes −4 or ≤ −6) goes to DGROUP:0xaa4. It returns while the sound system or `Music` is 0. Otherwise it refreshes the music volume (ISPACE.DLL seg9:0x38e2); when the track's file is the open file and the state is not 0 it re-seeks with ISPACE.DLL seg9:0x3a94(flag), and otherwise it closes the device, stores the file in DGROUP:0xaa2 and opens `SndPath` + name + `.mid` with ISPACE.DLL seg9:0x37ea.

- So −1 is a hard pause that −1, −2 and −3 cannot undo; only a track number restarts the music. −2 pauses only a playing track, −3 does nothing in states 0, 1 and 4, and −5 does nothing at all. [BYTES ISPACE.DLL seg9:0x3913 seg9:0x3a48 seg9:0x3b5c]
- INSPACE's wrapper turns a −3 into the track saved at the last −2 when it differs from DGROUP:0x9220, so a caller's −3 can reach ISPACE.DLL as a track number and restart the music after a −1. [BYTES seg10:0x74db seg10:0x74f0]
- At game end the last `MCI_PAUSE` is the teardown's −1 at seg10:0x148, not command 3; [shell.md](shell.md#exit) has the path. [BYTES seg10:0x148] [ORACLE sound-0001]

**MCI sequence.** [REF Win16 SDK mmsystem.h MCI_OPEN MCI_STATUS MCI_SET MCI_SEEK MCI_PLAY MCI_PAUSE MCI_CLOSE]

| Step | Routine | `mciSendCommand` | Flags | On failure |
|---|---|---|---|---|
| open | ISPACE.DLL seg9:0x37ea | `MCI_OPEN` 0x803, device type "sequencer" (ISPACE.DLL DGROUP:0xae4), element = path | 0x2200 `MCI_OPEN_TYPE \| MCI_OPEN_ELEMENT` | returns the error, nothing opened |
| port | ISPACE.DLL seg9:0x37ea | `MCI_STATUS` 0x814, item 0x4003 `MCI_SEQ_STATUS_PORT` | 0x100 `MCI_STATUS_ITEM` | closes |
| time format | ISPACE.DLL seg9:0x37ea | `MCI_SET` 0x80d, `dwTimeFormat` 0 (milliseconds) | 0x400 `MCI_SET_TIME_FORMAT` | ignored |
| seek, wait | ISPACE.DLL seg9:0x3a94(1) | `MCI_SEEK` 0x807 to the start | 0xa `MCI_TO \| MCI_WAIT` | no play |
| seek, notify | ISPACE.DLL seg9:0x3a94(0) | `MCI_SEEK` 0x807 to the start | 9 `MCI_TO \| MCI_NOTIFY` | — |
| play | ISPACE.DLL seg9:0x3b5c | `MCI_PLAY` 0x806 to start + length, no `MCI_FROM` | 9 `MCI_TO \| MCI_NOTIFY` | closes |
| pause | ISPACE.DLL seg9:0x3a48 | `MCI_PAUSE` 0x809 | 0 | — |
| close | ISPACE.DLL seg9:0x3a16 | `MCI_CLOSE` 0x804, no parameters | 2 `MCI_WAIT` | — |

[BYTES ISPACE.DLL seg9:0x380c seg9:0x3833 seg9:0x3840 seg9:0x3851 seg9:0x3861 seg9:0x38ab seg9:0x38c0 seg9:0x38d3 seg9:0x3a2f seg9:0x3a34 seg9:0x3a80 seg9:0x3b2f seg9:0x3b4d seg9:0x3bc1 seg9:0x3bd1]

- **Open.** After the time format the open stores the notify window ISPACE.DLL DGROUP:0x21ae as the callback of the play parameters ISPACE.DLL DGROUP:0x1a18 and the pause parameters DGROUP:0x1a24, and seeks with flag 1, so a new file always plays at once. [BYTES ISPACE.DLL seg9:0x38c5 seg9:0x38cc seg9:0x38d3]
- **Seek.** ISPACE.DLL seg9:0x3a94 seeks to 100 · start[DGROUP:0xaa4] ms and sets state 3 before the seek, whatever its result. For a negative track it takes the word ISPACE.DLL DGROUP:0xaac instead, which is start[2] = 1700, so it seeks to 170 000 ms, and sets state 4. [BYTES ISPACE.DLL seg9:0x3aa8 seg9:0x3ada DGROUP:0xaac] The `MCI_SEEK` parameter block is ISPACE.DLL DGROUP:0x1a28 for both forms: `dwCallback` is the notify window ISPACE.DLL DGROUP:0x21ae with high word 0, and `dwTo` at DGROUP:0x1a2c is the target, multiplied by 100 in place through ISPACE.DLL seg3:0x6a6. [BYTES ISPACE.DLL seg9:0x3adf seg9:0x3ae2 seg9:0x3ae5 seg9:0x3b2f seg9:0x3b4d] It subtracts 4500 when track 1 follows track 2 and 15000 when track 2 follows track 1. With flag ≠ 0 a successful seek plays at once; with flag 0 the play waits for the seek's notify. [BYTES ISPACE.DLL seg9:0x3aa1 seg9:0x3ab3 seg9:0x3abb seg9:0x3acd seg9:0x3ad3 seg9:0x3af9 seg9:0x3b0f seg9:0x3b1a seg9:0x3b39]
- **Play.** ISPACE.DLL seg9:0x3b5c acts only in states 2 and 3. With `Music` or the sound system 0 it closes the device; otherwise it copies the track to DGROUP:0xaa6, plays to 100 · (start + length) ms from the current position and sets state 1. The copy comes before `MCI_PLAY`, so a play that fails and closes the device (ISPACE.DLL seg9:0x3a16, which does not write DGROUP:0xaa6) still updates the previous track that the 4500 and 15 000 ms adjustments read. [BYTES ISPACE.DLL seg9:0x3b6a seg9:0x3b78 seg9:0x3b86 seg9:0x3baa seg9:0x3bad seg9:0x3bc1 seg9:0x3bd1 seg9:0x3bd6 seg9:0x3be4]
- **Pause.** ISPACE.DLL seg9:0x3a48(code) clears DGROUP:0x229a. Code 4 always sends `MCI_PAUSE` and sets state 4. Code 2 acts only in state 1: it sets DGROUP:0x229a = 1, sends `MCI_PAUSE` and sets state 2. [BYTES ISPACE.DLL seg9:0x3a52 seg9:0x3a58 seg9:0x3a5e seg9:0x3a6b seg9:0x3a80 seg9:0x3a88]
- **Close.** ISPACE.DLL seg9:0x3a16 sets the device id to 0xFFFF and the state to 0. `MCI_STOP` is never sent, and no path closes the device at exit: command 1 makes no MCI call. [BYTES ISPACE.DLL seg9:0x3a2f seg9:0x3a34 seg9:0x6a0]
- **Pause without a device.** −1 always sends `MCI_PAUSE`: to device 0 before the first open and to 0xFFFF after any close. [BYTES ISPACE.DLL seg9:0x3a71 seg9:0x3a34]

**Notify window.** The `NotifyClass` procedure ISPACE.DLL seg9:0x2ac4 handles `MM_MCINOTIFY` (0x3b9) only for wParam 1 (`MCI_NOTIFY_SUCCESSFUL`). In state 1 tracks 7 and 8 become track 2, tracks 5 and 11 stop there, and every other track (and 7 or 8 as 2) calls ISPACE.DLL seg9:0x3a94(0), a notify seek back to its start, so each loop point has a short gap. In state 3 it calls ISPACE.DLL seg9:0x3b5c. Other states ignore it. [BYTES ISPACE.DLL seg9:0x2ad2 seg9:0x2b18 seg9:0x2b21 seg9:0x2b2e seg9:0x2b36 seg9:0x2b4a seg9:0x2b54 seg9:0x2b60]
- The window also passes `WM_TIMER` to ISPACE.DLL seg9:0x4d02 and maps the private message 0x84c to ISPACE.DLL seg9:0x3906(wParam, 1). [BYTES ISPACE.DLL seg9:0x2b0b] Its sender is [OPEN].

**Wait or notify.** The flag of seg10:0x7480 is ordinal 7's argument high word, pushed as the dword (code, flag). It matters only for a track in the already open file with state ≠ 0: 1 seeks with `MCI_WAIT` and plays at once, 0 seeks with `MCI_NOTIFY` and plays when the seek completes. Only the in-wave selectors seg13:0xae4e and seg13:0xbb72 pass 0. seg2:0x8df (−2) and seg2:0x4cf (wParam of message 0xa04) push no flag, so the high word is a stale stack word, which seg2:0x4cf uses when wParam is a track. Every other caller passes 1. [BYTES seg10:0x7538] [BYTES ISPACE.DLL seg9:0x7c seg9:0x3a05 seg9:0x38d3 seg9:0x3b1a] [BYTES seg13:0xae49 seg13:0xbb6d seg2:0x4cc seg2:0x8dd]

**Music off.** With `Music` = 0 or the sound system 0, track requests store the track in DGROUP:0xaa4 and return without opening, and a −3 in state 2 or 3 closes the device. [BYTES ISPACE.DLL seg9:0x393b seg9:0x3948 seg9:0x3be4] After Options, ISPACE.DLL seg9:0x220a calls ISPACE.DLL seg9:0x3906(DGROUP:0xaa4, 1) when `Music` was off as the dialog opened (snapshot ISPACE.DLL DGROUP:0x2220) and is now on, or when the sound system went from 0 to non-zero. [BYTES ISPACE.DLL seg9:0x1683 seg9:0x205d seg9:0x220a seg9:0x2225] That call stays inside ISPACE.DLL (far call at seg9:0x222b, not through seg10:0x7480), so INSPACE's DGROUP:0x9220, DGROUP:0x9222 and the music hold DGROUP:0x2384 do not change; INSPACE's own (−3, 1) at seg10:0x7f7e follows after the dialog. [BYTES ISPACE.DLL seg9:0x222b] [BYTES seg10:0x7f34 seg10:0x7f7e]

**Midi Mapper box (SND-07).** When the `MCI_STATUS` port's low word is not −1 (`MIDI_MAPPER`), the open loads string 2219 "Midi Mapper is not available. Continue?" from the language module ISPACE.DLL DGROUP:0x690 and calls `MessageBox(notify window, text, "", MB_YESNO)`. IDNO (7) closes the device and returns 0 without playing; any other answer continues. Nothing caches the answer, so the box comes back at every later open. [BYTES ISPACE.DLL seg9:0x3866 seg9:0x386c seg9:0x387b seg9:0x388b seg9:0x3890 seg9:0x3895 seg9:0x389b] [BYTES ENG_LANG.DLL STRING 139] id 2219
- The manual asks for a correctly configured MIDI mapper to hear the music. [MANUAL Troubleshooting]

**Music volume.** ISPACE.DLL seg9:0x38e2 calls the music twin of ordinal 8, ISPACE.DLL seg9:0x3bf4, with the −6000 point. The twin returns 0 at once unless the sound system is above 1. Otherwise both channels get v = min(100, ((100 · 95 / 100) · Mm mod 65536) / 100), with Mm the music volume ISPACE.DLL DGROUP:0x2288: each `mul` is unsigned 16-bit and its high word is cleared before the `div`, and the clamp to 100 is unsigned. v becomes v · 0xFFFF / 100 through a 16-bit `mul` and the unsigned long-divide helper ISPACE.DLL seg3:0x1d6, the same word in both halves. ISPACE.DLL DGROUP:0x6a8 = (v << 16) | v, and ISPACE.DLL seg9:0x3e46 calls `midiOutSetVolume(1, value)` only when the value differs from the cache ISPACE.DLL DGROUP:0xd06/0xd08, which it updates first. [BYTES ISPACE.DLL seg9:0x38ef seg9:0x3c01 seg9:0x3c0d seg9:0x3dbb seg9:0x3dc2 seg9:0x3dc8 seg9:0x3dcc seg9:0x3dd0 seg9:0x3dd7 seg9:0x3dd9 seg9:0x3ddd seg9:0x3de4 seg9:0x3e22 seg9:0x3e57 seg9:0x3e72 seg3:0x1d6]
- A negative Mm, which the 16-bit `imul` of ISPACE.DLL seg9:0xa9e gives when (2 · `FXMusic`) · M overflows, wraps as an unsigned word instead of clamping to 0. [BYTES ISPACE.DLL seg9:0xad9 seg9:0xae8 seg9:0x3dcc]
- With the default `SoundVolume` 100 and `FXMusic` 50 the value is 0xF332F332 (95 %), and sound-0001 shows `MIDIOUTSETVOLUME(1, f332f332)`. [BYTES ISPACE.DLL seg9:0x2321 seg9:0x2337 seg9:0x3c0d] [ORACLE sound-0001]
- ISPACE.DLL seg9:0x38e2 runs for every track request that passes the sound-system and `Music` checks, and from ordinal 8 on every effect play while the sound system is above 1. [BYTES ISPACE.DLL seg9:0x3972 seg9:0x7a4 seg9:0x779]
- On a system with a single MIDI output (device 0) device 1 does not exist, so these calls fail with `MMSYSERR_BADDEVICEID` and the music volume is never applied; what the oracle returns is in [Open questions](#open-questions). [BYTES ISPACE.DLL seg9:0x3e6e] [REF Win32 SDK midiOutSetVolume]
- **Separate devices.** Effects go to waveOut through WaveMix or `sndPlaySound` and music to the MCI sequencer, so both run at once. [BYTES ISPACE.DLL seg9:0x1333 seg9:0x13be seg9:0x3bc1]

**Adaptive wave music.** While the music hold DGROUP:0x2384 is below DGROUP:0x1e50 and the wave is not a duel (DGROUP:0x950), race (DGROUP:0x94e), Demon's Gate (DGROUP:0x92e8) or seg18 mode (DGROUP:0x9130), seg7:0xd5b calls the selector seg13:0xba04. [BYTES seg7:0xd34 seg7:0xd3f seg7:0xd5b]
1. It returns at once while track 9 or 10 plays (DGROUP:0x9220). [BYTES seg13:0xba1f]
2. It picks track 2 when a live ship (byte +0x717c ≠ 0 and the word at [DGROUP:0x1e30] + 0x70·i + 0x18 = 0) targets the player: +0x70c8 = 1, +0x70c2 = 0xe and +0x70c4 = the player DGROUP:0x9646, or +0x70e2 in {0x16, 0x17} with +0x70e4 = 0xe and +0x70e6 = the player. Otherwise it picks track 1. [BYTES seg13:0xba70 seg13:0xba92 seg13:0xbac2]
3. While track 2 plays it also keeps 2 when the player's record has +0x70c8 = 1 and +0x70c2 = 0xe, or when another live ship's +0x70d4 is non-zero and within 20 of seg5:0x2b4(DGROUP:0x1e68, 10). [BYTES seg13:0xbacf seg13:0xbb07 seg13:0xbb52]
4. On a change it issues (track, 0) and sets the hold to now + 6000 for track 2, or + 5000 or + 3000 otherwise; without a change the hold becomes now + 6000 for track 2 and now + 3000 otherwise. [BYTES seg13:0xbb65 seg13:0xbb72 seg13:0xbb7c seg13:0xbb92]

- **Event music.** seg13:0xadbe issues (track, 0) when seg13:0xae68 accepts the event and then sets the hold to now + 12000. Kinds 3, 0x13 and 0x14 give track 7. Kind 4 gives 8 when the ship is the player, 7 when the player's record targets that ship (+0x70c2 = 0xe, +0x70c4 = ship, +0x70c8 = 1), and otherwise nothing, with no call and no hold. Kinds 0x15, 0x16 and every other kind give 2. [BYTES seg13:0xadd3 seg13:0xade4 seg13:0xadf8 seg13:0xae28 seg13:0xae4e seg13:0xae53] The meaning of the event kinds and the test of seg13:0xae68 are [OPEN].

**Call sites.** INSPACE calls seg10:0x7480 at 36 sites; `flag —` means the call pushes no flag. [BYTES seg10:0x7480]

| Site | Code | Flag | Condition or context |
|---|---|---|---|
| seg2:0x4cf | wParam of message 0xa04 | — | WndProc |
| seg2:0x8df | −2 | — | help window seg2:0x88e |
| seg6:0x1dac | −1 | 1 | Analysing Drive |
| seg6:0x5e25 | 11, or 5 | 1 | state-5 entry seg6:0x5d58: 11 when DGROUP:0x94cc ≠ 0 or (score DGROUP:0x1e58 ≥ 200000 and rank DGROUP:0x93a2 ≤ 3) |
| seg9:0x65de | 3, 7 or 1 | 1 | wave start seg9:0x60bc: 3 for a race (DGROUP:0x94e), 7 for Demon's Gate (DGROUP:0x92e8) or a duel (DGROUP:0x950), else 1 |
| seg9:0x6bd4 | −1 | 1 | — |
| seg10:0x148 | −1 | 1 | — |
| seg10:0x7f34 | −2 | 1 | Options, before the dialog |
| seg10:0x7f7e | −3 | 1 | Options, after the dialog |
| seg10:0x82bb | −2 | 1 | dialogs |
| seg10:0x841d | −3 | 1 | dialogs |
| seg10:0x857f | −2 | 1 | Save Game |
| seg10:0x8679 | −3 | 1 | Save Game |
| seg11:0x476e | 4 | 1 | when DGROUP:0x186a = 0x11 |
| seg11:0x7aa2 | −1 | 1 | state-0 ESC flow |
| seg13:0xae4e | 2, 7 or 8 | 0 | event music seg13:0xadbe |
| seg13:0xbb72 | 1 or 2 | 0 | selector seg13:0xba04 |
| seg15:0x10dc | −1 | 1 | launch scene, unless state 9 |
| seg15:0x3058 | −2 | 1 | landing scene, unless state 9 |
| seg15:0x57b5 | −1 | 1 | state-0 entry |
| seg15:0x58ec | −5 | 1 | state-0 entry |
| seg15:0x5bf5 | 0 | 1 | intro phase 1, unless DGROUP:0x8f70 ≠ 0 and DGROUP:0x90f8 ≠ 0 |
| seg16:0x1e4b | 2 | 1 | in seg16:0xcfe |
| seg16:0x663c | −2 | 1 | unless DGROUP:0x9222 = −2 |
| seg17:0x6018 | −3 | 1 | help window close |
| seg18:0x54 | 6 | 1 | when DGROUP:0x13a0 = 1 after its decrement |
| seg18:0x129 | 0 | 1 | Inner Demon entry |
| seg18:0x1e0 | −3 | 1 | when DGROUP:0x9530 = 1 and DGROUP:0x9222 = −2 |
| seg18:0x490 | −3 | 1 | after DGROUP:0x73e = 0 |
| seg18:0x8d9 | 10 | 1 | when DGROUP:0x9660 = 0, after DGROUP:0x73e = 1 |
| seg18:0xacb | −1 | 1 | when DGROUP:0x9660 = 0 |
| seg18:0x4154 | −2 | 1 | — |
| seg18:0x532c | −2 | 1 | — |
| seg18:0x5f41 | −2 | 1 | followed by seg18:0x5f4d |
| seg18:0x5f4d | 10 | 1 | — |
| seg18:0x632b | −3 | 1 | — |

[BYTES seg2:0x4cf seg2:0x8df seg6:0x1dac seg6:0x5dfc seg6:0x5e25 seg9:0x65b1 seg9:0x65bf seg9:0x65cd seg9:0x65de seg9:0x6bd4 seg10:0x148 seg10:0x7f34 seg10:0x7f7e seg10:0x82bb seg10:0x841d seg10:0x857f seg10:0x8679 seg11:0x476e seg11:0x7aa2 seg13:0xae28 seg13:0xae4e seg13:0xbb72 seg15:0x10dc seg15:0x3058 seg15:0x57b5 seg15:0x58ec seg15:0x5bf5 seg16:0x1e4b seg16:0x663c seg17:0x6018 seg18:0x54 seg18:0x129 seg18:0x1ce seg18:0x1e0 seg18:0x490 seg18:0x8c9 seg18:0x8d9 seg18:0xacb seg18:0x4154 seg18:0x532c seg18:0x5f41 seg18:0x5f4d seg18:0x632b]

## Audio Options and Options
**Dialogs.** Ordinal 10 command 0 opens Options/Setup, ENG_LANG.DLL DIALOG 100 with procedure ordinal 11 (ISPACE.DLL seg9:0x15cc). Its button 139 opens Audio Options, DIALOG 999 (171 × 158 DLU at 73, 31), with ordinal 38 (ISPACE.DLL seg9:0x2f30). Button 132 opens Display (#029a, ordinal 37, ISPACE.DLL seg9:0x2bf8) and button 147 Joystick (#03cf, ordinal 42, ISPACE.DLL seg9:0x40da). [BYTES ISPACE.DLL seg9:0x1546 seg9:0x15ae seg9:0x1e14 seg9:0x1e26] [BYTES ENG_LANG.DLL DIALOG 100] [BYTES ENG_LANG.DLL DIALOG 999]
- Display touches no sound global or sound function. Joystick reads no sound global, but its button poll (ISPACE.DLL seg9:0x4262 → seg9:0x48f8 → seg9:0x4a55 → seg9:0x4f46) plays 0x6e and then a per-button sound through ISPACE.DLL seg9:0x298c each time a button goes down. [BYTES ISPACE.DLL seg9:0x2bf8 seg9:0x4262 seg9:0x4a55 seg9:0x4f46 seg9:0x298c]
- **Test-sound helper.** ISPACE.DLL seg9:0x298c(id) calls ordinal 9 directly with p = (−6000, −6000), q = 1, r = s = 0, the area (−6000, −6000), `now` = `GetCurrentTime` and the name ISPACE.DLL DGROUP:0x9a9. It bypasses INSPACE's seg10:0x754a, so the DGROUP:0x920c = 3 test and the last-played stores do not apply. [BYTES ISPACE.DLL seg9:0x2999 seg9:0x29a1 seg9:0x29a3 seg9:0x29a9 seg9:0x29b1 seg9:0x29b5]
- Audio Options opens no sub-dialog and computes a centre position that it never applies, so it opens at its template position. [BYTES ISPACE.DLL seg9:0x2f4d seg9:0x2f99]

| Dialog | Id | Control | Text | Feeds |
|---|---|---|---|---|
| 100 | 139 | push button | &Audio.. | opens DIALOG 999 |
| 100 | 111 | scroll bar | (labels 140 Master Volume, 112 Softer, 113 Louder) | `SoundVolume` |
| 100 | 118 | drop list | — | `SoundSystem` |
| 100 | 104 | group box | SOUND | — |
| 100 | 143, 144 | static | Sound FX:, Normal | 144 shows the `SND_FX.DLL` kind |
| 100 | 145, 146 | static | Voice FX:, Normal | 146 shows the `<Language>_FX.DLL` kind |
| 100 | 106 | push button | &Help | help |
| 999 | 102 | check box | &Sound FX | `SoundFX` |
| 999 | 103 | check box | &Voice Effects | `VoiceFX` |
| 999 | 101 | check box | &Music Soundtrack | `Music` |
| 999 | 1001 | scroll bar | (labels 1000 Effects vs. Music, 112 Effects, 113 Music) | `FXMusic` |
| 999 | 128 | drop list | (label 129 Mix Channels:) | `MixChannels` |
| 999 | 1008 | drop list | (label 1007 Stereo Effect:) | `StereoEffect` |
| 999 | 1003 | check box | &Keep Sounds in Memory (faster loading) | `PurgeSound` = not checked |
| 999 | 1004 | check box | &Swap left/right Channels | `StereoSwap` |
| 999 | 1, 2, 106 | push buttons | &OK, &Cancel, &Help | — |

[BYTES ENG_LANG.DLL DIALOG 100] [BYTES ENG_LANG.DLL DIALOG 999]

**Options init.** WM_INITDIALOG of ordinal 11 does the following for sound: [BYTES ISPACE.DLL seg9:0x1683 seg9:0x18ea seg9:0x1919 seg9:0x194b seg9:0x1981 seg9:0x1a0a seg9:0x19b7 seg9:0x19d0 seg9:0x1ad5 seg9:0x1af7]
1. It snapshots `Music` into ISPACE.DLL DGROUP:0x2220.
2. Combo 118 gets 2233 "No Sound" always, 2234 "Standard Sound" when the capability level is ≥ 1, 2235 "Dynamic Sound" when ≥ 2 and 2236 "3D Dynamic Sound" when ≥ 3; the selection is the sound system, so the index is the `SoundSystem` value. [BYTES ENG_LANG.DLL STRING 140] ids 2233–2236
3. A capability level below 2 disables bar 111; below 1 it also disables combo 118 and button 139.
4. Bar 111 gets the range 0–100 and the position `SoundVolume`.
5. Labels 144 and 146 get the module kind ([Slots](#slots), "Compressed").

- A separate gate disables controls 0x7f, 0x7d, 0x73, 0x7e, 0x77, 0x72, 0x82, 0x84, 0x89 and 0x8a when ISPACE.DLL DGROUP:0x2136 = 1. [BYTES ISPACE.DLL seg9:0x1824]
- **Placement.** The init centres the dialog: x = trunc(`SM_CXSCREEN`/2) + trunc((left − right)/2) from `GetWindowRect`, y likewise, then `MoveWindow(hDlg, x, y, w, h, TRUE)`. It focuses bar 107 and returns 0, so the focus stays there. [BYTES ISPACE.DLL seg9:0x15ef seg9:0x15f4 seg9:0x1632 seg9:0x1b21 seg9:0x1b31]
- **Other combos.** Every string comes from the language module ISPACE.DLL DGROUP:0x690 through ISPACE.DLL seg9:0x379e. [BYTES ISPACE.DLL seg9:0x168b seg9:0x16c7 seg9:0x170f seg9:0x174b seg9:0x1766 seg9:0x1773 seg9:0x17b1 seg9:0x17c2 seg9:0x17fe seg9:0x1814 seg9:0x2597 seg9:0x25ad seg9:0x379e]
  - Combo 126 (Play Detail) gets 2221–2224 "Highest" to "Low" and selection 3 − ISPACE.DLL DGROUP:0x228a. [BYTES ENG_LANG.DLL STRING 139] ids 2221–2223 [BYTES ENG_LANG.DLL STRING 140] id 2224
  - Combo 119 (Quickload) gets 2225–2227 "Never", "Mid-Wave", "Always" and selection ISPACE.DLL DGROUP:0x2286. [BYTES ENG_LANG.DLL STRING 140] ids 2225–2227
  - Combo 137 (Difficulty) gets 1035–1039 "Gentle" to "Extreme!" and selection INI `Complex` (default 2). [BYTES ENG_LANG.DLL STRING 65] ids 1035–1039
  - Combo 114 (Zoom) gets 2230–2232 "2x", "1x", "1/2x" and selection 2 − INI `Size` (default 1). [BYTES ENG_LANG.DLL STRING 140] ids 2230–2232
  - The init reads `Complex` and `Size` itself; DGROUP:0x228a and DGROUP:0x2286 hold INI `Detail` (default 3) and `Quick` (default 1), which the INI reader loads.
- **Labels.** Label 122 gets the version: "V0.00.00" (ISPACE.DLL DGROUP:0x883) with INSPACE.EXE string 167 "129" at positions 1, 3 and 4 and string 233 "08" at 6 and 7, cut at 8, so "V1.29.08". Label 123 gets INSPACE.EXE string 1800 "Inner Space", label 136 string 607 "Aug 9/2000" and label 142 ENG_LANG.DLL string 2309 "English". The INSPACE.EXE strings load through `GetWindowWord(GetParent(hDlg), GWW_HINSTANCE)`. [BYTES ISPACE.DLL seg9:0x1bf6 seg9:0x1c1d seg9:0x1c2c seg9:0x1c34 seg9:0x1c5c seg9:0x1cae seg9:0x1f38 DGROUP:0x883] [BYTES INSPACE.EXE STRING 11] id 167 [BYTES INSPACE.EXE STRING 15] id 233 [BYTES INSPACE.EXE STRING 113] id 1800 [BYTES INSPACE.EXE STRING 38] id 607 [BYTES ENG_LANG.DLL STRING 145] id 2309
- **Play Speed init.** Bar 107 gets the range 0–1000 and a position from INI `FrameRateSet` r (default 36): 1000 for r = 1200, (r − 4) · 500 / 32 for r < 36 and (r − 36) · 500 / 70 + 500 otherwise, in signed 32-bit arithmetic (both formulas give 500 at 36). A result whose low word is negative becomes 0; there is no upper clamp, so r = 107–1199 or above 1200 gives a position above 1000. The position goes to ISPACE.DLL DGROUP:0xcfe and DGROUP:0xd00. The rate of a position is the rule in [timing.md](timing.md#rate-settings). [BYTES ISPACE.DLL seg9:0x1a25 seg9:0x1a34 seg9:0x1a4a seg9:0x1a64 seg9:0x1a6b seg9:0x1a86 seg9:0x1aa4 seg9:0x1aaa seg9:0x1aad seg9:0x1ac4]
- **Play Speed bar.** WM_HSCROLL on bar 107 (jump table ISPACE.DLL seg9:0x1d06): codes 0 and 2 subtract 50, 1 and 3 add 50, 4 and 5 take nPos, 6 (`SB_TOP`) sets 1000 and 7 (`SB_BOTTOM`) sets 0; higher codes change nothing. The value is clamped to at most 1000 and then at least 0. Only when it differs from DGROUP:0xd00 does ISPACE.DLL seg9:0x1ea8 set the bar position and label 124 and DGROUP:0xd00 take the value. Label 124 reads "Play Speed  %s" (ISPACE.DLL DGROUP:0x874) with "Full Out" (DGROUP:0x85d) for rate 1200 and otherwise "%d frames/sec" (DGROUP:0x866) of the rate. [BYTES ISPACE.DLL seg9:0x1cce seg9:0x1d01 seg9:0x1d06 seg9:0x1d16 seg9:0x1d2a seg9:0x1d2d seg9:0x1d42 seg9:0x1ea8 DGROUP:0x85d DGROUP:0x866 DGROUP:0x874]
- **Normal Speed.** Button 110 "Normal &Speed" sets DGROUP:0xcfe = DGROUP:0xd00 = 500, calls ISPACE.DLL seg9:0x1ea8(500) and puts the focus back on bar 107. [BYTES ISPACE.DLL seg9:0x1e62 seg9:0x1e6d seg9:0x1e7c seg9:0x1e81] [BYTES ENG_LANG.DLL DIALOG 100]
- **Master Volume.** WM_HSCROLL on bar 111 moves the value by 1 for line and page steps and to nPos for the thumb (codes 4 and 5); `SB_TOP` and `SB_BOTTOM` do nothing and `SB_ENDSCROLL` plays the test sound 0x23 through ISPACE.DLL seg9:0x298c. The value is clamped to 0–100, ISPACE.DLL seg9:0xa9e recomputes both volumes and `SetScrollPos` runs only when the value differs from ISPACE.DLL DGROUP:0xd02. [BYTES ISPACE.DLL seg9:0x1d45 seg9:0x1d4a seg9:0x1d9e]
  - The handler calls no volume API. The next sound played through ordinal 8, the test sound included, applies both the wave volume and, while the sound system is above 1, the MIDI volume; the next track request also applies the MIDI volume. [BYTES ISPACE.DLL seg9:0x298c seg9:0x7a4 seg9:0xb62 seg9:0x3e46 seg9:0x3972]

**Options OK and Cancel** (ISPACE.DLL seg9:0x1fae):
1. OK (wParam ≠ 0) stores the combo's selection as the sound system and writes, in this order: `Complex` = the combo 137 index, `Braking` (control 148), `Enhance` (control 130, also into ISPACE.DLL DGROUP:0x2132), `SoundVolume`, `SoundSystem` (combo 118), `Quick`, `Detail` = 3 − the combo 126 index (also into DGROUP:0x228a), `Size` and `SetSize`, both 2 − the combo 114 index from one buffer, and `FrameRateSet` = the rate of the Play Speed position ([timing.md](timing.md#rate-settings)). [BYTES ISPACE.DLL seg9:0x1fdd seg9:0x1fe1 seg9:0x1ffd seg9:0x2002 seg9:0x2057 seg9:0x20b3 seg9:0x20d8 seg9:0x20fe seg9:0x2124 seg9:0x214a seg9:0x2170 seg9:0x2196 seg9:0x21bb seg9:0x21d0 seg9:0x21fb]
2. When the sound system changed, OK re-runs the INI reader ISPACE.DLL seg9:0x22ca, which re-reads every sound key and the capability level. [BYTES ISPACE.DLL seg9:0x2207]
3. Both OK and Cancel then call ISPACE.DLL seg9:0x3906(DGROUP:0xaa4, 1) in the two cases of [Music off](#music). On Cancel (ISPACE.DLL seg9:0x1fc1 → seg9:0x220a) the second test reads the local [bp − 0x34], which only OK writes. [BYTES ISPACE.DLL seg9:0x1fc1 seg9:0x220a seg9:0x221f seg9:0x222b]
4. Cancel restores nothing: `SoundVolume` moved on the bar stays in ISPACE.DLL DGROUP:0x69e, with both volumes recomputed, and only the INI write is skipped. [BYTES ISPACE.DLL seg9:0x1fc1 seg9:0x1d9e]
5. Neither path stops or restarts the sound engine with ordinal 7 commands 1 and 0. [BYTES ISPACE.DLL seg9:0x1fae]

**Audio init** (ordinal 38): [BYTES ISPACE.DLL seg9:0x2fa1 seg9:0x2fc8 seg9:0x2fdf seg9:0x3031 seg9:0x304c seg9:0x3082 seg9:0x308f seg9:0x30bc seg9:0x30d6 seg9:0x313c seg9:0x3169 seg9:0x31ae]
1. Combo 128 gets "1" to "n", with n = 8 when `WAVEMIX.DLL` is loaded or `OpenFile(<Windows directory>\WAVEMIX.DLL, OF_EXIST)` succeeds and n = 1 otherwise; the selection is the channel count − 1.
2. Combo 1008 gets 2413 "Minimal", 2414 "Less", 2415 "Normal" and 2416 "More", selected by `StereoEffect`. [BYTES ENG_LANG.DLL STRING 151] ids 2413–2415 [BYTES ENG_LANG.DLL STRING 152] id 2416
3. A capability level below 3 disables 1004 and 1008; below 2 it disables bar 1001; below 1 it disables 101, 102, 103, 128 and 1003.
4. Check boxes 102, 103, 101, 1003 and 1004 show `SoundFX`, `VoiceFX`, `Music`, `PurgeSound` = 0 and `StereoSwap`; bar 1001 gets the range 0–100 and the position `FXMusic`.

- **Balance bar.** WM_HSCROLL moves `FXMusic` by 1 for line and page steps and to nPos for the thumb, clamps it to 0–100, calls ISPACE.DLL seg9:0xa9e and sets the position when it differs from ISPACE.DLL DGROUP:0xd04; it plays no test sound and calls no volume API. [BYTES ISPACE.DLL seg9:0x31c1 seg9:0x31e2 seg9:0x320c]

**Audio OK** (ISPACE.DLL seg9:0x331a):
1. It sets the globals from the controls and writes `StereoEffect` (combo index), `FXMusic`, `Music`, `SoundFX`, `VoiceFX`, `MixChannels` (index + 1), `PurgeSound` (1 − Keep) and `StereoSwap` in this order. [BYTES ISPACE.DLL seg9:0x33d6 seg9:0x33fc seg9:0x3422 seg9:0x3448 seg9:0x346e seg9:0x3493 seg9:0x34b9 seg9:0x34df]
2. `SoundFX` off with module 0 loaded calls ISPACE.DLL seg9:0x3d8(0), and `VoiceFX` off with module 1 loaded calls seg9:0x3d8(1); `SoundFX` on with module 0 absent calls ISPACE.DLL seg9:0xba(0), and `VoiceFX` on with module 1 absent calls seg9:0xba(1). [BYTES ISPACE.DLL seg9:0x34e4 seg9:0x350a seg9:0x3524 seg9:0x353e]
3. When the channel count changed and the sound system is not 0 it unloads both modules, stores the new count, loads both modules and calls ISPACE.DLL seg9:0x4e0(1, 0), which closes and reopens the WaveMix session with `MixChannels` re-read from the INI. [BYTES ISPACE.DLL seg9:0x3547 seg9:0x3582]
4. When step 2 or 3 called a loader, it preloads slots 0–0x51 with ISPACE.DLL seg9:0x25c6(slot, 0). [BYTES ISPACE.DLL seg9:0x3599 seg9:0x35ae]
5. Cancel only ends the dialog, and `FXMusic` moved on the bar stays changed in memory. [BYTES ISPACE.DLL seg9:0x332d seg9:0x320c]

- **Module loader.** When `OpenFile(OF_EXIST)` fails, ISPACE.DLL seg9:0xba silently writes `SoundFX=0` for module 0 or `VoiceFX=0` for module 1; any other module shows "Can't Find " + path with `MB_ICONEXCLAMATION`. [BYTES ISPACE.DLL seg9:0x1fe seg9:0x219 seg9:0x2da seg9:0x332] [BYTES ENG_LANG.DLL STRING 127] id 2025
- **Help.** Help in both dialogs calls `WinHelp(<Path><Language>_HELP.HLP, HELP_CONTEXT, 15)`, the manual's Game Settings topic. [BYTES ISPACE.DLL seg9:0x1e55 seg9:0x3262 seg9:0x328e] [MANUAL Game Settings]

**Already in use (SYS-28).** Ordinal 10 command 0 (ISPACE.DLL seg9:0x1546) tests the Options window word ISPACE.DLL DGROUP:0x69c, which WM_INITDIALOG sets and seg9:0x15ba clears after `DialogBox`. When it is set the call shows `MessageBox(NULL, string 2220, "Inner Space", MB_OK | MB_ICONEXCLAMATION)` and returns 0; otherwise it plays 0x6e and then 0x13 and opens DIALOG 100. [BYTES ISPACE.DLL seg9:0x1553 seg9:0x155e seg9:0x157a seg9:0x158f seg9:0x1596 seg9:0x15ba seg9:0x15e5] [BYTES ENG_LANG.DLL STRING 139] id 2220
- **INSPACE side.** Menu 101 "Options.." (0x68) and the popup's "Game Options.." (0xc0, Alt+G) run seg10:0x7f18: DGROUP:0x592 = 2, sound 0x6e, music (−2, 1), ordinal 10 command 0, seg6:0xb3e (which re-reads `MixChannels` into DGROUP:0x920a), seg10:0x7f88, DGROUP:0x1e50 = now and DGROUP:0x3b94 = now + 500, then music (−3, 1). [BYTES seg10:0x7f22 seg10:0x7f2b seg10:0x7f34 seg10:0x7f4e seg10:0x7f52 seg10:0x7f7e seg6:0xcac]

## Catalogue
[re/artifacts/sounds.tsv](../../re/artifacts/sounds.tsv) lists every slot id 0x00–0x51 and every special id with its module, resource (id + 100), flag byte, THEWORKS twin (`same_as`), length in ms (frames · 1000 / 11025, truncated, or `absent`), transcript, hold, meaning and call sites. [BYTES ISPACE.DLL seg9:0x2660 seg9:0xf98] [BYTES SND_FX.DLL 100] [BYTES ENG_FX.DLL 100]
- The transcripts of the 32 voice WAVs come from whisper.cpp with the ggml-medium model, capitalised; ENG_FX.DLL 149 reads "Do try to come back in one piece." after its THEWORKS name DOTRY.WAV, where whisper heard "to try come back in one piece". [BYTES ENG_FX.DLL 100] [REF whisper.cpp ggml-medium]
- The `same_as` names come from equal sample data in the loose THEWORKS `.WAV` files ([resources.md](resources.md#100-wav-files-snd_fxdll-46-eng_fxdll-32)). [BYTES ENG_FX.DLL 100 156]
- A call-site entry is `C:site in function` for seg10:0x744c(id) and `P:site in function(a, b, mode)` for seg10:0x754a. [BYTES seg10:0x744c seg10:0x754a]
- Ids 0x00, 0x04, 0x0a, 0x0e, 0x0f, 0x1e, 0x26 and 0x2d have no call site with a constant id. [BYTES seg10:0x744c seg10:0x754a]

The special ids are handled by ordinal 9 or by seg10:0x754a, not by a slot. [BYTES ISPACE.DLL seg9:0xc72] [BYTES seg10:0x765d]

| Id | Handled by | Effect |
|---|---|---|
| −1 | ordinal 9 | plays `SndPath` + name + `.WAV`, booked as slot 0x11 |
| 0x64 | ordinal 9 | returns 0 |
| 0x65 | ordinal 9 | 0x30 + rand() % 3 |
| 0x66 | ordinal 9 | 0x47, 0x48 or 0x49, never its last pick twice |
| 0x67 | ordinal 9 | 0x49 and 0x2e alternately |
| 0x68 | ordinal 9 | slot 0x14, loop word 0x14 |
| 0x69 | ordinal 9 | reset the priority, or stop the single channel; clear busy-until |
| 0x6a | seg10:0x754a | 0x20, then 0x21 after 1000 ms |
| 0x6b | seg10:0x754a | 0x39, then 0x3a after 420 ms |
| 0x6c | seg10:0x754a | 0x38, then 0x39 after 385 ms |
| 0x6d | seg10:0x754a | 0x35, then 0x39 after 385 ms |
| 0x6e | ordinal 9 (mode 0 forced) | stop all; clear busy-until |
| 0x6f | ordinal 9 | slot 0x17, loop word 0x14 |
| 0x70 | ordinal 9 | slot 0x4a, loop word 8 |
| 0x71 | seg10:0x754a | 0x24, then 0x25 after 660 ms |
| 0x72 | seg10:0x754a | 0x1a, then 0x1a after 200 ms |
| 0x73 | seg10:0x754a | 0x42, then 0x43 after 700 ms |
| 0x74 | seg10:0x754a | 0x42, then 0x66 after 400 ms |
| 0x75 | ordinal 9 | slot 0x46, loop word 7 |
| 0x76 | seg10:0x754a | 0x23, then 0x25 after 400 ms |

- **Shell sounds.** 0x13 is the dialog and page open click (17 sites), 0x14 the typing tick of the credits and logo typer seg15:0x7cda and of the briefing typewriter reset seg15:0x84b0, 0x2b the desk-scene and briefing character tick, and 0x6e stops all sounds at screen changes (31 sites). [BYTES seg15:0x7e67 seg15:0x84d5 seg15:0x1846 seg9:0x8ff6 seg17:0x2f25 seg10:0x8438 seg10:0x85ee]
- **Briefing page click.** seg17:0x20fa plays seg10:0x754a(0x13, 100, trunc(DGROUP:0x90ec / 2), 4) at seg17:0x23e6, and at seg17:0x2c20 by falling through from seg17:0x2c11: a positional click at x = 100 and y = half the play-area height, which mode 4 passes through. The call at seg17:0x2c20 is shared: seg17:0x2af2 jumps to it with seg10:0x754a(0x15, trunc(DGROUP:0x6088 / 2) + 250, trunc(DGROUP:0x90ec / 2), 4) on the first count of DGROUP:0x18c6/0x18c8. [BYTES seg17:0x23d7 seg17:0x23e6 seg17:0x2ad9 seg17:0x2af2 seg17:0x2c11 seg17:0x2c20 seg10:0x7745]
- 0x4b "Commencing launch sequence" plays in the desk scene entry seg15:0xcb0 and the desk/launch frame seg15:0x10be, 0x4c "Penetrating inner space" in seg15:0x0, which the desk scene calls in phases 5–7, and 0x4d "Select destination" in the Navigation dialog seg6:0x23d4. [BYTES seg15:0xeed seg15:0x110a seg15:0x613 seg6:0x2daa]
- **Exit request.** SC_CLOSE (wParam & 0xfff0 = 0xF060, seg2:0xd78) plays 0x6e, then 0x0b + rand() % 3 (0x0c plays as 0x0b), waits 450 ms, plays 0x50 "Please stay" and opens the exit dialog seg10:0x82a2(5). [BYTES seg2:0xd88 seg2:0xdac seg2:0xdcf seg2:0xdff]
- 0x51 plays before the Order dialog (seg11:0x79e5) but is silent, because ENG_FX.DLL 181 is absent. [BYTES seg11:0x79e5] [BYTES ENG_FX.DLL 100]
- **Voices in messages.** The message dispatcher seg3:0x6ec (outer switch seg3:0x1351, code = case + 1) plays voices next to its strings, for example 0x33 for code 7 and 0x3b for codes 3, 81 and 117–119. Codes 59–63, 65–68, 87 and 88 pick rand() % 4: 0 plays 0x3d, 1 plays 0x3e, 2 plays 0x3f and 3 plays nothing, and then they show string code + 0x1eb; code 64 shows string 0x22b with no sound. [BYTES seg3:0x1351 seg3:0xcbd seg3:0xf61 seg3:0xf7c seg3:0xf59]
- **Wave events.** 0x45 KABOOM plays at destruction (seg12:0x274a, seg12:0x2b20, seg12:0xa26, seg16:0x8e4, seg8:0x3fbe, seg7:0x5090, seg6:0x78ea), 0x07 BOUNCE in seg12:0x3e98, and 0x27 HYPER at 12 sites. Hit sounds are data-driven: record +0x42, SHP word +0x1f (DGROUP:0x9700) and DGROUP:0x2f6c. [BYTES seg12:0x2b0d seg12:0x2da5 seg12:0xac7 seg16:0xaac seg8:0x4037 seg12:0x46a1 seg8:0x5509 seg14:0x1e18 seg14:0x1e2d]
  - The player-damage handler seg8:0x45e0 plays 0x07 only while DGROUP:0xb0a ≠ 0. That word is 0 in the file and in every probe dump, and its only writer seg8:0x4d43 stores 0, so the handler plays the design's hit sound DGROUP:0x9700 + 0xad·n through seg10:0x7944 instead ([core.md](core.md#damage-to-a-ship-seg80x45e0)). [BYTES seg8:0x494e seg8:0x4960 seg8:0x49bc seg8:0x49d8 seg8:0x49dc seg8:0x4d43 DGROUP:0xb0a] [ORACLE probe-0001]
- **Smoke, break-ups and icons.** The smoke start seg7:0x5b68 plays 0x01 PUFF, seg10:0x754a(1, pool, rec, 1), only with its third argument ≠ 0 and only after it has taken a record, so a full table is silent ([world.md](world.md#smoke-seg70x5b68-and-seg70x5e04)). The break-up seg7:0x2e02 plays 0x1a FLANGE for pools 1 and 8 and for a pool-6 record whose weapon's end kind is 6, 0x1f SHATTER for pool 4 and the other pool-6 records, both at seg7:0x2ec2, and nothing for another pool. The defender burst seg8:0x92a0 plays 0x16 DEFBOOM. The icon effects play 0x6a for a melt (seg7:0x4cef) and an ignite (seg7:0x4796) and 0x76 for a thaw from below −2 (seg7:0x4aed) and an extinguish (seg7:0x4c15) ([core.md](core.md#icon-effects)). [BYTES seg7:0x5c60-0x5c76 seg7:0x2e5c-0x2ec2 seg7:0x306e-0x3076 seg8:0x92c5 seg7:0x4cef seg7:0x4796 seg7:0x4aed seg7:0x4c15] core-0005 hears 0x01 one or two frames before each tail-smoke puff shows. [ORACLE core-0005]
- **A missile's end.** A rock contact calls, in one frame, the missile record's `hit_sound` (0x06 CLUNK from the wave template), the 0x45 of its `on_destroy` and the rock's 0x08, in that order; a fuse plays its 0x45 with no 0x06 and no 0x08 ([core.md](core.md#pool-6-mover-seg120x0)). [BYTES seg10:0x109c seg8:0x645d seg12:0x2b0d seg7:0x4060] The oracle's traces show the contact's three within 10 ms, 0x06 first. [ORACLE core-0005] [ORACLE core-0006]
- **The 800 award.** Its 0x73 shows in a trace as slot 0x42 CROWD and, 700 ms later, slot 0x43 "He shoots, he scores!", at the vanished object's position, never as 0x73 ([core.md](core.md#the-vanish-seg70x13b2-and-seg70x158e)). [BYTES seg7:0x1950 seg10:0x7605 seg10:0x7617]
- **The shield and the ambulance.** 5 SHIELD plays at every shield toggle, seg10:0x754a(5, DGROUP:0x9324, i, 1) at seg11:0x4c5e, and when the visit's pad starts to move (seg15:0x4c76, phases 17, 20, 27 and 29) ([core.md](core.md#the-shield-seg110x4b00)). 0x33 RESCUE is the dispatch reply (message code 7, seg3:0xcbd), 0x23 CAPTURED the pickup (seg16:0x46a), 0x2f LET_ME "Let me guide you in." the state 2 → 3 step (seg16:0x4c5), 0x08 SHORT a collision of the ambulance (seg16:0x84c) and 0x2b POP each character the visit's monitor types (seg16:0x3197 in seg16:0x2e3c) ([core.md](core.md#the-ambulance)). core-0003 hears 0x33 with string 110 in the same trace millisecond, 0x23 then 0x2f 0.75 s apart, and 0x2b once per typed character. [BYTES seg11:0x4c5e seg15:0x4dbf seg3:0xcbd seg16:0x46a seg16:0x4c5 seg16:0x84c seg16:0x3197] [ORACLE core-0003]
- **The descent.** Every call of the visit's descent seg15:0x374c, after its speed step ([core.md](core.md#the-visit-state-6-seg160xcfe)), plays 0x20 IGNITE through seg10:0x744c when the flag DGROUP:0x17fc is 0 and differs from DGROUP:0x17f8, and then copies DGROUP:0x17fc into DGROUP:0x17f8, so the sound marks each change of the flag to 0. [BYTES seg15:0x3914 seg15:0x397b seg15:0x3988 seg15:0x398d seg15:0x3992 seg15:0x3996 seg15:0x3998 seg15:0x399a seg15:0x39a0 seg15:0x39a3]
- **The port's shell ids.** The M2 port's wrappers log 0x05, 0x0b–0x0d, 0x13, 0x14, 0x15, 0x1c, 0x20, 0x22, 0x23, 0x2b, 0x4b, 0x4c, 0x4d, 0x50, 0x51, 0x68, 0x69, 0x6e and 0x6f. Four need care: 0x69 and 0x6e are commands with no resource, 0x51 is silent, and 0x0c plays resource 111. [BYTES ISPACE.DLL seg9:0xd8f seg9:0xda4 seg9:0xdb6] [BYTES seg11:0x79e5]

## Open questions
- WaveMix 1.x's mixing arithmetic (8-bit sum and clipping), block length, latency and resampling decide the exact mix; they need a reading of `WAVEMIX.DLL`. [OPEN]
- Whether WaveMix plays a wave L + 1 times for the loop word L of the MIXPLAYPARAMS (0 once, 0xFFFF forever), and whether a `WMIX_CLEARQUEUE` play posts `MM_WOM_DONE` (handled at ISPACE.DLL seg9:0x2adf) for the wave it replaces. ISPACE.DLL's bytes cannot settle either; a reading of `WAVEMIX.DLL` ordinal 7 `WaveMixPlay` and its mixing pump, or a WaveMix oracle run that plays 0x68 and counts the per-channel `MM_WOM_DONE` against the wave length, would. [OPEN] [BYTES ISPACE.DLL seg9:0x13a4 seg9:0x2adf]
- Whether a mode-5 preload ever reaches ordinal 9 outside seg18 mode depends on seg10:0x7896 returning −1 for its point. [OPEN]
- The initial game wave volume ISPACE.DLL DGROUP:0x6a4 in STANDARD mode is BSS 0 until command 3 or ordinal 8 writes it. [OPEN]
- `MixChannels` above 8 overruns the nine records at ISPACE.DLL DGROUP:0x1760, whose channel 8 is the no-priority value; the dialog offers 1–8. [OPEN]
- The callers of ordinal 7 command 9 (`WaveMixPump`) in INSPACE are not listed. [OPEN]
- What ISPACE.DLL DGROUP:0xcf6 = now + 1200 after slot 0x1c is for; no reader is known. [OPEN]
- Wine's mciseq accepts `MCI_PLAY` past the file end (tracks 5 and 8): seek and play return 0, no note sounds, and `MCI_NOTIFY_SUCCESSFUL` arrives within 150 ms, so the close path seg9:0x3bd1 is never taken there. [ORACLE sound-0002] Whether Windows' mciseq fails the play and closes the device instead is [OPEN].
- Whether the sequencer sounds notes that are held across an `MCI_SEEK` target: tracks 4 (RACE.MID from 34 000 ms), 11 (ANTHEM.MID from 181 000 ms) and 5 (ANTHEM.MID from 200 000 ms) start inside held notes, with the next note-on 764 ms, 1797 ms and never after the seek. Wine's mciseq strikes no held note: its skip to the target sends only controller and program messages, and the first note-on it sends is the file's next note, skipping the gap. [ORACLE sound-0002] Whether Windows' mciseq sounds the held notes is [OPEN].
- No oracle run shows `MCI_NOTIFY_SUCCESSFUL`, so the loops and the 7/8 → 2 chaining rest on the bytes alone. [OPEN]
- Which device MIDI device 1 is to the original's volume calls, and what the port does for music volume ([DESIGN] ADR 0011). [OPEN]
- Which level of the dual-mode files the author's Windows 3.x MIDI Mapper set-up sounded, the extended channels 1–10 or the base-level 13–16; playing all 16 doubles the parts (the port's synth backends play 1–10, [DESIGN] ADR 0011). [OPEN]
- The acceptance test of seg13:0xae68 and the event kinds 3, 4 and 0x13–0x16 of seg13:0xadbe; what DGROUP:0x186a = 0x11 (seg11:0x476e), the DGROUP:0x13a0 countdown (seg18:0x49) and DGROUP:0x9660 (seg18:0xac0) mean. [OPEN]
- The effect of the uninitialised [bp − 0x34] read on Options Cancel (ISPACE.DLL seg9:0x221f). [OPEN]
- The sender of the private message 0x84c to the `NotifyClass` window. [OPEN]
- Which other Win16 task can hold ISPACE.DLL DGROUP:0x69c, since DLL data is shared: whether INSPACE.SCR, INRUN.AD or FACTORY.EXE calls ordinal 10 command 0. [OPEN]
- After switching the sound system from 0 to non-zero in Options alone, ISPACE.DLL DGROUP:0x219e stays 0, so the loader takes the WaveMix branch without a session and loads nothing: the effects stay silent until a restart, while the music starts, and Audio Options then selects nothing in combo 128, so `CB_GETCURSEL` returns −1 and OK writes `MixChannels=0`, which silences the effects at the next start too. [ORACLE sound-0002]
- Which ids the Ship Factory's hit-sound names (ENG_LANG.DLL 610–624: CLUNK … SHATTER) stand for. [OPEN]
- Whether ids 0x00, 0x04, 0x0a, 0x0e, 0x0f, 0x1e, 0x26 and 0x2d are reachable through the data tables (record +0x42, SHP +0x1f, DGROUP:0x2f6c) or not at all. [OPEN]
- Names for the wave routines in seg7, seg8, seg12, seg13 and seg18 that hold sound calls, and the meaning of mode 2's a = 0x1e and of DGROUP:0x920c = 3. [OPEN]
- The start-reg traces show `WAVEOUTSETVOLUME(0, cccccccc)` (the −6000 case), a pan sweep from fd6f2666 and `MIDIOUTSETVOLUME(1, f332f332)`, and most MIDI volume calls returning 5 or 8; these need a digest before they carry an oracle tag. [OPEN]
- The oracle mixes effects through WaveMix, so its waveOut buffers do not show which id played ([pitfalls.md](pitfalls.md#tools)). [OPEN]
