# Frame timing

INSPACE paces its frames from a busy message loop, not from a window timer. Each pass of the loop samples the clocks and runs the per-pass work of the current screen state. A frame runs when a pass counter reaches the state's threshold. In some states a millisecond gate also forces or skips frames. Two adaptive rules rescale the thresholds once per frame: the wave's rule aims at `FrameRateSet` frames per second, and the rule of the other screens aims at fixed targets, mostly 72 per second. The game clock counts wave frames at a fixed 36 per game second. The port's design choice is [ADR 0001](../decisions/0001-timestep.md).

## Message loop and idle step
- seg2:0x0 runs the message loop. It calls `PeekMessage(PM_REMOVE)` at seg2:0xfa while one of these holds: [BYTES seg2:0xc9] [BYTES seg2:0xfa]
  - The game is not paused (DGROUP:0x8f68 non-zero).
  - A modeless dialog is open (DGROUP:0xd1e non-zero with DGROUP:0x95bc zero).
  - DGROUP:0x9460 is non-zero.
- When the queue is empty and DGROUP:0x8f66 is set, the loop calls the idle step seg2:0x1c08. When none of the conditions holds, it blocks in `GetMessage` at seg2:0x184. [BYTES seg2:0x171] [BYTES seg2:0x184]
- The oracle counts about 65 000 loop passes per second on the title screen. [ORACLE newgame-0001]
- The idle step works in one of two ways:
  - **State change:** when DGROUP:0x13de is set, it calls seg2:0x1d58 instead of a frame. That routine tears the old state down, takes the new state from DGROUP:0x968e and calls seg2:0x1ac8. seg2:0x1ac8 recomputes the play area and reseeds `rand()` ([rng.md](rng.md)). [BYTES seg2:0x1c64] [BYTES seg2:0x1db9] [BYTES seg2:0x1b6b]
  - **Pass:** otherwise it takes a DC of the main window DGROUP:0x16, or of the dialog DGROUP:0x9460, into DGROUP:0x13dc and selects the palette DGROUP:0x920e into it. It calls seg7:0x0, or seg17:0x60d8 while DGROUP:0x9460 is set. It then restores the palette and releases the DC. [BYTES seg2:0x1c79] [BYTES seg2:0x1cdb] [BYTES seg2:0x1ce2] [BYTES seg2:0x1d3f]

## Per-pass work in seg7:0x0
- While DGROUP:0x1a is set, seg7:0x0 returns at once and the pass does nothing. [BYTES seg7:0x10]
- Otherwise `GetCurrentTime` goes to DGROUP:0x1e50 on every pass. Wall-clock deadlines in the game compare against this sample. [BYTES seg7:0x1f]
- Every 5000 ms of that clock (deadline DGROUP:0x1e54), while DGROUP:0x9132 is clear, the game reads `GetFreeSystemResources(GFSR_GDIRESOURCES)`. [BYTES seg7:0x39] [BYTES seg7:0x42] [BYTES seg7:0xa7]
  - At 10 % or less it shows "RESOURCE ALERT" once (ENG_LANG strings 2541 and 3043). [BYTES ENG_LANG.DLL STRING 159] [BYTES ENG_LANG.DLL STRING 191]
  - Above 40 % the warning re-arms (DGROUP:0x58e). [BYTES seg7:0x9c]
- When DGROUP:0x912e is set, `timeGetTime` goes to DGROUP:0x2214. [BYTES seg7:0xb7]
  - At start-up, seg6:0x1348 sets DGROUP:0x912e to 1 when `timeGetDevCaps` succeeds with a minimum period below 3 ms. [BYTES seg6:0x1348] [BYTES seg6:0x1357]
- The function then switches on the screen state DGROUP:0x90f4 through a 10-entry table at seg7:0x49c. [BYTES seg7:0x497]
- The oracle sees both clock calls on every pass (return addresses seg7:0x24 and seg7:0xbc). DGROUP:0x1e50, DGROUP:0x2214 and DGROUP:0x2218 track Wine's tick count in every phase. [ORACLE newgame-0001] [ORACLE probe-0001]

## The frame gate
Each state keeps a pass counter in DGROUP:0x9144 and a threshold N. The gated states add a millisecond gate with intervals F and S. DGROUP:0x2218 holds the `timeGetTime` sample of the last frame, and DGROUP:0x2214 the current one. One pass of a gated state does this: [BYTES seg7:0x55e] [BYTES seg7:0x58b] [BYTES seg7:0x9ce]

```
skip = false
if fine_timer:                        # DGROUP:0x912e
    if last + F < now: counter = N    # force a frame
    elif last + S > now: skip = true  # too early
counter += 1
if not skip and counter >= N:
    last = now; counter = 0; run frame
```

- With integer milliseconds, a gated frame never comes less than S ms after the last one. Once more than F ms have passed, the next pass runs it. [BYTES seg7:0x13b] [BYTES seg7:0x153]
- Ungated states run their frame when the counter reaches N. [BYTES seg7:0x3d0]
- Before each frame of states 0 (once DGROUP:0x1212 is set), 2, 3, 6, 8 and 9, and of the dialog pass seg17:0x60d8, seg7:0x2206 rescales DGROUP:0x9004 ([Adaptive screen threshold](#adaptive-screen-threshold)). DGROUP:0x8f76 adapts in the wave frame ([Adaptive wave threshold](#adaptive-wave-threshold)). Only seg6:0xa6c writes DGROUP:0x9002, so state 5 is the only state whose threshold never adapts: it runs a frame every DGROUP:0x9002 passes, at whatever rate the passes come. [BYTES seg7:0x2b2] [BYTES seg7:0x3e0] [BYTES seg17:0x60ef] [BYTES seg6:0xaa8]

| State | Frame function | Threshold N | Gate (F, S) and its condition |
|---|---|---|---|
| 0 | seg15:0x5962 | DGROUP:0x9004 | 14, 13 ms, when DGROUP:0x912e and DGROUP:0x1212 are set |
| 1, 7 | wave frame from seg7:0x9ce | DGROUP:0x8f76 | DGROUP:0x5c4, DGROUP:0x5c8, when DGROUP:0x912e is set |
| 2 | seg9:0x6dec | DGROUP:0x9004 | none |
| 3 | seg17:0x0 | DGROUP:0x9004 | none |
| 4 | none | none | waits for the deadline DGROUP:0x1e78, then switches to state 0 |
| 5 | seg6:0x5f7a | DGROUP:0x9002 | none. At the deadline DGROUP:0x1e7c it switches to state 4 |
| 6 | seg16:0xcfe | DGROUP:0x9004 | DGROUP:0x5c4, DGROUP:0x5c8, when DGROUP:0x952c is set, DGROUP:0x9530 = 1 and DGROUP:0x912e is set |
| 8 | seg16:0x6928 | DGROUP:0x9004 | none |
| 9 | seg16:0x7210 | DGROUP:0x9004 | none |

[BYTES seg7:0x264] [BYTES seg7:0x2c3] [BYTES seg7:0x3e9] [BYTES seg7:0x35c] [BYTES seg7:0x3f1] [BYTES seg7:0x42c] [BYTES seg7:0x487] [BYTES seg7:0x13b] [BYTES seg7:0x18f] [BYTES seg7:0x1b7] [BYTES seg7:0x202]

- State 0 runs the intro and holds the Calibrate measurement. State 1 runs the wave. [BYTES seg15:0x5f68] [BYTES seg7:0xb32] State 7 shares the wave's table entry, but the idle step never reaches seg7:0x0 in state 7: it runs the dialog pass seg17:0x60d8 while DGROUP:0x9460 is set, which holds throughout state 7 ([globals.md](globals.md#game-state-globals)). [BYTES seg2:0x1cdb] States 2, 3, 5, 6, 8 and 9 are the wave debriefing, the wave briefing, the high-score table, the ambulance visit, the Hall of Justice and the game-completion ending ([shell.md](shell.md#screen-states)). [BYTES seg9:0x6ec4 seg9:0x8a9b seg6:0x6322 seg16:0xd9b seg16:0x6ba2 seg16:0x73dc]
- Many states also check wall-clock deadlines on every pass, before the counter. Examples are DGROUP:0x1e70 and DGROUP:0x1e74 in state 2, DGROUP:0x5ac in state 3 and DGROUP:0x1e84 in state 9. [BYTES seg7:0x37b] [BYTES seg7:0x39f] [BYTES seg7:0x2d4] [BYTES seg7:0x1cd]
- **Wave passes:** after the gate's counter step (seg7:0x552–0x58b) and before the frame test (seg7:0x925, seg7:0x92d), every wave pass checks wall-clock deadlines, selects the clip region (seg7:0x84a) and polls the keyboard through seg11:0x4db0. [BYTES seg7:0x58b] [BYTES seg7:0x84a] [BYTES seg7:0x881] [BYTES seg7:0x8ec] [BYTES seg7:0x92d]
  - The poll runs only while DGROUP:0x1a is clear and field +0x18 of the DGROUP:0x1e30 record of index DGROUP:0x9646 (stride 0x70) is 0. [BYTES seg7:0x868] [BYTES seg7:0x87a]
  - The oracle counts 94 600 such passes in 8.2 s of wave. [ORACLE newgame-0002]
  - While DGROUP:0x9130 is set, the wave pass skips that work and the wave frame calls seg16:0x64fa instead of the world update. [BYTES seg7:0x594] [BYTES seg7:0xb6b]
- **Wave frame:** when a wave frame is due, the game first runs seg8:0x3aec (the [shot-slot update](core.md#shot-update-seg80x3aec)) in a loop of 5 × (DGROUP:0x594 + DGROUP:0x598) calls. It then runs seg8:0x3fbe (only while DGROUP:0x9430 > 0) and seg7:0x375a, and continues at seg7:0x9ce ([world.md](world.md#update-loop)). [BYTES seg7:0x942] [BYTES seg7:0x993]
- With the default rate of 36 (F = 28, S = 27) and a loop much faster than the frame rate, wave frames come 27, 28 or 29 ms apart. The adaptive threshold below keeps the average near 36 frames per second. [BYTES seg6:0xb9a] [BYTES seg7:0xac2]

## Rate settings
seg6:0xb3e reads the timing keys from the `[Inner Space]` section of INSPACE.INI. Its only callers are seg6:0xa6c, at the end of the Calibrate measurement, and seg10:0x7f18, after the Options dialog. Until the first call the initialised data holds a rate of 36, F = 27 and S = 26. [BYTES seg6:0xae7] [BYTES seg10:0x7f52] [BYTES DGROUP:0x59c] [BYTES DGROUP:0x5c4]
- **FrameRateSet** (default 36) goes to DGROUP:0x59c, the target frames per second. From it: [BYTES seg6:0xb82] [BYTES seg6:0xb9a] [BYTES seg6:0xbd8]
  - DGROUP:0x59e = rate / 4, truncated toward zero: the target frames per 250 ms.
  - F (dword DGROUP:0x5c4) = 1000 / rate, plus 1 when (10000 / rate) mod 10 > 4. S (dword DGROUP:0x5c8) = F − 1. All divisions truncate.

  | FrameRateSet | F ms | S ms | Frames per 250 ms |
  |---|---|---|---|
  | 4 (minimum) | 250 | 249 | 1 |
  | 20 | 50 | 49 | 5 |
  | 36 | 28 | 27 | 9 |
  | 54 | 19 | 18 | 13 |
  | 72 | 14 | 13 | 18 |
  | 104 | 10 | 9 | 26 |
  | 1200 (maximum) | 1 | 0 | 300 |

- The installer's Game Speed choices are Slow (20 frames/second), Normal (36), Fast (54) and Red Hot! (72). The shipped INSPACE.INI sets `FrameRateSet=36`. [BYTES INSTALL.EXE DIALOG 1922] [BYTES INSPACE.INI]
- The Options dialog's Play Speed scrollbar maps position p (0–1000) to a rate. p ≤ 500 gives p·32/500 + 4, so 4–36. p > 500 gives (p − 500)·70/500 + 36. Any result above 104, and p = 1000, gives 1200, shown as "Full Out". The dialog's closing code writes `FrameRateSet`. [BYTES ISPACE.DLL seg9:0x2242] [BYTES ISPACE.DLL seg9:0x1ee6] [BYTES ISPACE.DLL seg9:0x21fb]
- **GameSpeed** (default 20000) goes to DGROUP:0x8ffa. The value 20000 means "use Calibrate", and DGROUP:0x8ffa then takes DGROUP:0x8ffc. [BYTES seg6:0xb5c] [BYTES seg6:0xb6c]
  - The effective value sets the starting wave threshold. INSPACE also passes it to the Options dialog, which never reads it, and seg15:0x7470 compares it with 800. [BYTES seg6:0xdc8] [BYTES ISPACE.DLL seg9:0x1586] [BYTES seg15:0x7470]
- At the end of seg6:0xb3e, seg6:0xdaa sets the starting wave threshold DGROUP:0x8f76 and saves it in DGROUP:0x8f78. seg2:0xa74 restores it from there. [BYTES seg6:0xdaa] [BYTES seg6:0xeae] [BYTES seg2:0xb54]
  - Calibrate above 600 gives GameSpeed / 28, above 250 gives GameSpeed / 80, otherwise 0.
  - By the same Calibrate bands (above 600, 500 and 250, else), it sets DGROUP:0x1df4 to 120, 130, 140 or 150. What DGROUP:0x1df4 controls is [OPEN].
- No frame depends on `WM_TIMER`, so the coarse resolution of window timers does not limit the frame rate. [BYTES seg2:0x11f8] [BYTES seg7:0x0]

## Calibrate
- The intro measures it on every launch. At the start of state 0, while DGROUP:0x1212 is clear, seg15:0x58fb stores `GetCurrentTime` in DGROUP:0x120e and clears the count DGROUP:0x8ffa. [BYTES seg15:0x58fb] [BYTES seg15:0x5907]
- Every state-0 frame within 1000 ms of DGROUP:0x120e adds 1 to the count. DGROUP:0x9004 is 0 before the first measurement, and the 14/13 ms gate needs DGROUP:0x1212. So a state-0 frame runs on every loop pass, and the count is intro passes per second. [BYTES seg15:0x5f68] [BYTES seg15:0x6038] [BYTES seg7:0x25d]
- When the 1000 ms are over: [BYTES seg15:0x5f98] [BYTES seg15:0x5fcf] [BYTES seg15:0x5ff4]
  - The game reads the INI `Calibrate`, default 0. If the count is non-zero but below half that value and DGROUP:0x1214 is set, the count restarts once.
  - Unless DGROUP:0x90f8 is clear and DGROUP:0x90ee is at least 8, the measurement restarts while `GetCurrentTime` is within 2000 ms of DGROUP:0x1d3c. DGROUP:0x1d3c holds the time the message loop started. [BYTES seg2:0xa5]
  - A count of 0 restarts the measurement.
- Otherwise Calibrate = 23 × count / 10, computed in 32 bits by RTL seg5:0x110 and stored in DGROUP:0x8ffa. seg15:0x6009 sets DGROUP:0x1212, which turns on state 0's 14/13 ms gate. [BYTES seg15:0x6022] [BYTES seg15:0x6009]
- seg6:0xa6c then derives the following values, calls seg6:0xb3e and writes `Calibrate` back to INSPACE.INI: [BYTES seg6:0xa7c] [BYTES seg6:0xa8a] [BYTES seg6:0xac2] [BYTES seg6:0xb31]
  - DGROUP:0x8ffc = Calibrate.
  - The starting thresholds: DGROUP:0x9004 = Calibrate / 50 above 1200, Calibrate / 150 above 850, else 0. DGROUP:0x9002 = DGROUP:0x9004. From the next frame on, seg7:0x2206 rescales DGROUP:0x9004 ([Adaptive screen threshold](#adaptive-screen-threshold)), while DGROUP:0x9002 keeps its value.
  - DGROUP:0x9006 = Calibrate / 38 above 360, else 7, then clamped to 7–10.
- The INI `Calibrate` value is read only for the restart check. Each launch measures afresh. [BYTES seg15:0x5f98]
- The oracle measures 13 340–14 940 under Wine, 7116 under otvdm and 1294–1324 under otvdm with relay tracing. It sees the value rewritten 1.5 s after each launch. [ORACLE newgame-0001] [ORACLE runtime-0001] [ORACLE probe-0001]
- The manual's Calibrate bands are advice for choosing the Detail setting by hand: above 1300, 800–1300, 520–800 and below 520. `Detail` itself comes from the INI (default 3, DGROUP:0x9208). [MANUAL Optimizing for Your Computer] [BYTES seg6:0xc7b]
- seg15 and seg16 compare DGROUP:0x8ffc with further constants: 300, 800, 850, 900, 1300, 1400, 1500, 1600 and 2000. Examples are seg15:0x394f (1300) and seg15:0x7cef (800). What those branches change is [OPEN]. [BYTES seg15:0x394f] [BYTES seg15:0x7cef] [BYTES seg16:0x272b]
- Outside the `DEMO` command line, skipping the intro never skips the measurement: no key reaches the state-0 handler until seg15:0x6009 sets DGROUP:0x1212 on an accepted measurement, after which seg6:0xa6c has read `FrameRateSet` through seg6:0xb3e ([shell.md](shell.md#intro-and-title-screen)). [BYTES seg2:0x74a seg15:0x6009 seg15:0x6030 seg6:0xae7]

## Adaptive wave threshold
Each wave frame adjusts DGROUP:0x8f76 so that the frames meet the target rate (seg7:0x9dc–0xaf7). [BYTES seg7:0x9ed] [BYTES seg7:0xa40] [BYTES seg7:0xac2]
- Above a rate of 1000 (maximum speed), DGROUP:0x8f76 becomes 0 and neither adjustment runs. A frame then runs on every pass.
- **Every 250 ms** of DGROUP:0x1e50 (deadline DGROUP:0x1e64), the game compares the frames of the window (DGROUP:0x914c) with DGROUP:0x59e. When they differ by more than 1, DGROUP:0x8f76 becomes DGROUP:0x8f76 × frames / DGROUP:0x59e. A zero threshold first becomes 1 when there were too many frames.
- **Every 1000 ms** (deadline DGROUP:0x1e8c), the game compares the frames of the last second (DGROUP:0x586) with DGROUP:0x59c. When they lie strictly within 9 of it, DGROUP:0x8f76 becomes DGROUP:0x8f76 × frames / DGROUP:0x59c, with the same zero rule. The count goes to DGROUP:0x59a for the `B` display, and the game posts `WM_PAINT` to the targeting display DGROUP:0xa7c while it shows the speed (DGROUP:0xa76 = 4). [BYTES seg7:0xadc] [BYTES seg7:0xaf2]
- Both products keep only their low 16 bits: `imul` leaves the 32-bit product in DX:AX, and the `cwd` before `idiv` overwrites DX with the sign of AX. So a product above 32767 wraps, and the new threshold can come out negative. A negative threshold lets a frame run on every pass that the gate does not skip. [BYTES seg7:0xa44] [BYTES seg7:0xac6]
- While DGROUP:0x592 is non-zero, neither adjustment runs, and each second decrements it. Pausing and opening the Options dialog set it to 2. [BYTES seg2:0x9d1] [BYTES seg10:0x7f22] [BYTES seg7:0xad5]
- seg7:0x9dc also counts DGROUP:0x9142 through 0–54. Its readers are seg7:0x317f, seg7:0x3193, seg7:0x32f4, seg7:0x3e63, seg12:0xb2d and seg12:0x5417. What they use it for is [OPEN]. [BYTES seg7:0x9e0] [BYTES seg7:0x317f] [BYTES seg12:0xb2d]

## Adaptive screen threshold
seg7:0x2206 runs before each frame of states 0 (once DGROUP:0x1212 is set), 2, 3, 6, 8 and 9, and before each frame of the dialog pass seg17:0x60d8 ([The frame gate](#the-frame-gate)). It rescales DGROUP:0x9004 by the same 250 ms and 1000 ms rules as the wave, against fixed targets instead of `FrameRateSet`. [BYTES seg7:0x2262] [BYTES seg7:0x22ac] [BYTES seg7:0x231b]
- The targets, per 250 ms and per second: [BYTES seg7:0x2228] [BYTES seg7:0x2256] [BYTES seg7:0x2262]

  | Condition | Per 250 ms | Per second |
  |---|---|---|
  | State 6 with DGROUP:0x952c set and DGROUP:0x9530 = 1 | field +0x2a of the record at the far pointer DGROUP:0x6da4, divided by 4 | that field |
  | Otherwise state 6 with DGROUP:0x914a = 1 and DGROUP:0x92a2 = 0 | 8 | 32 |
  | Every other case | 18 | 72 |

- **Every 250 ms** (deadline DGROUP:0x1e64), when the frames of the window (DGROUP:0x914c) differ from the target by more than 1, DGROUP:0x9004 becomes DGROUP:0x9004 × frames / target. A zero threshold first becomes 1 when there were too many frames. [BYTES seg7:0x227f] [BYTES seg7:0x22ac]
- **Every 1000 ms** (deadline DGROUP:0x1e8c), when the frames of the last second (DGROUP:0x586) lie strictly within 9 of the target, DGROUP:0x9004 becomes DGROUP:0x9004 × frames / target, with the same zero rule. The count goes to DGROUP:0x59a. [BYTES seg7:0x22e9] [BYTES seg7:0x231b] [BYTES seg7:0x232c]
- The products wrap above 32767 as in the wave (`imul`, then `cwd`). [BYTES seg7:0x22a8] [BYTES seg7:0x2317]
- The routine shares the counters and deadlines DGROUP:0x914c, DGROUP:0x1e64, DGROUP:0x586 and DGROUP:0x1e8c with the wave's rules. It does not skip its adjustments while DGROUP:0x592 is non-zero, but it still decrements DGROUP:0x592 each second. [BYTES seg7:0x226c] [BYTES seg7:0x2325]
- Code at seg15:0x3558–0x357d and seg16:0x6443–0x6461 also changes DGROUP:0x9004. When DGROUP:0x90ee is at least 8 and DGROUP:0x93c is set, the seg15 code multiplies it by 4 when Calibrate is above 2000 and by 2 when it is above 1500. The seg16 code doubles it, or sets it to 250 when it is 0, and raises it to at least 250. When these run is [OPEN]. [BYTES seg15:0x356e] [BYTES seg15:0x357d] [BYTES seg16:0x6450] [BYTES seg16:0x6461]
- In state 0 with the fine timer (DGROUP:0x912e), the 14/13 ms gate keeps frames 13–15 ms apart, about 67–77 per second, around the target of 72. In states 2, 3, 8 and 9 and the dialog pass, no gate applies, so the threshold alone sets the rate. Their actual rates are [OPEN]. [BYTES seg7:0x264] [BYTES seg7:0x27d]
- In the probe's title dump (state 0), DGROUP:0x9004 holds −490, a wrapped value, and DGROUP:0x59a holds 76. Calibrate (DGROUP:0x8ffc) is 14 940 there, so the starting threshold was 298. [ORACLE probe-0001]

## Game time
- Each wave frame adds 1 to the dwords DGROUP:0x1e68 and DGROUP:0x5a8. seg16:0x1f11 also increments DGROUP:0x1e68. [BYTES seg7:0xb2d] [BYTES seg7:0xb32] [BYTES seg16:0x1f11]
- The HUD wave clock shows DGROUP:0x1e68 / 36 seconds, through RTL seg5:0x2b4, then splits it into minutes and seconds by 60. The divisor is the constant 36, not DGROUP:0x59c. [BYTES seg11:0x1f0e] [BYTES seg11:0x1f21]
  - The probe's DGROUP:0x1e68 values 0x12e, 0x15a and 0x187 match HUD clocks of 0:08, 0:09 and 0:10. [ORACLE probe-0001]
- Other rules count wave frames as well. One example: when DGROUP:0x1e68 mod 300 is 0, the frame calls seg13:0xb93a. [BYTES seg7:0xd60] A survey of the other readers of DGROUP:0x1e68 and DGROUP:0x5a8 is [OPEN].
- So the wave clock and the frame-counted rules run at FrameRateSet / 36 of real time when the machine reaches the set rate, and slower when it does not. The manual states this: "Inner Space is calibrated for 36 frames/second. If you run Inner Space faster or slower than 36 frames/second, time speeds up or slows down in the game". [MANUAL Game Settings]
  - At 36 frames per second under Wine and untraced otvdm, the clock keeps real time. Under otvdm with relay tracing it runs at half speed. [ORACLE newgame-0002] [ORACLE runtime-0001]
- Other deadlines use the wall clock DGROUP:0x1e50 and do not scale with the frame rate:

  | Site | Deadline | Purpose |
  |---|---|---|
  | seg7:0xdd7 | now + 1500 ms into the player's record, DGROUP:0x71e6 + 0x15a · DGROUP:0x9646 | [OPEN] |
  | seg7:0x8a3 | DGROUP:0x1e6c = now + 10 000 ms | [OPEN] |
  | seg7:0x8fb | DGROUP:0x5a4 = now + 50 ms | [OPEN] |
  | seg7:0x914 | DGROUP:0x5b8 = now + 10 000 ms, then seg7:0x2128 | The decay of the violence score DGROUP:0x9432 ([globals.md](globals.md#global-table)) |
  | seg7:0xb4d | DGROUP:0x2388 = now + 50 ms | Joystick poll, when INI `JoyStick` (DGROUP:0x8ff4) is set |
  | seg7:0xc2b | DGROUP:0x17b8 = now + 280 ms | Targeting display refresh |
  | seg10:0x7524 | DGROUP:0x2384 = now + 9000 ms on a music change | Checked at seg7:0xd38 |
  | seg10:0x7e35 | DGROUP:0x870c = now + 5000 ms | End of the speed display |

  [BYTES seg7:0xdd7] [BYTES seg7:0x8a3] [BYTES seg7:0x8fb] [BYTES seg7:0x914] [BYTES seg7:0xb4d] [BYTES seg7:0xc2b] [BYTES seg10:0x7524] [BYTES seg10:0x7e35]
  - The probe sees tick values at DGROUP:0x71e6. [ORACLE probe-0001]
  - Which gameplay rules run on frame counts and which on the wall clock needs a per-rule survey. [OPEN]

## Speed keys
- The key handler seg11:0x6b64 treats Alt as held when `GetKeyState(VK_MENU)` is down or the message parameter is 0x99b. It treats Ctrl as held when `GetKeyState(VK_CONTROL)` is down or the parameter is 0x99e. [BYTES seg11:0x6cfd] [BYTES seg11:0x6cde]
- Alt with `VK_ADD` or `VK_OEM_PLUS` calls seg10:0x7e1c(+1). Alt with `VK_SUBTRACT` or `VK_OEM_MINUS` calls seg10:0x7e1c(−1). Ctrl with the same keys calls seg10:0x9a2a(±1) instead. [BYTES seg11:0x6ed4] [BYTES seg11:0x6e45] [BYTES seg11:0x71f7] [BYTES seg11:0x7207]
  - The manual names these keys: Alt+ and Alt− for speed, Ctrl+ and Ctrl− for scaling. [MANUAL Game Settings]
- Without a modifier, + and − step DGROUP:0xa6c by 8, up to 500, and move the HUD scrollbar. The value is the radar's scale: the radar callbacks map positions with it ([structs.md](structs.md#object-record-0x70-bytes)), and the radar display zooms [MANUAL General]. [BYTES seg11:0x7215] [BYTES seg11:0x7242] [BYTES seg11:0x392d]
- seg10:0x7e1c adds 2 × d to DGROUP:0x59c. [BYTES seg10:0x7e5b] [BYTES seg10:0x7e86] [BYTES seg10:0x7f0b]
  - Stepping down from above 1000 gives 104.
  - The result is clamped to at least 4, and anything above 104 becomes 1200 (maximum).
  - It recomputes DGROUP:0x59e, F and S by the rules above and writes `FrameRateSet` to INSPACE.INI.
  - It shows the rate on the targeting display (DGROUP:0xa76 = 4) for 5 s. [BYTES seg10:0x7e3f]
- The `B` key sets DGROUP:0xa76 to 4 as well. Whether or not DGROUP:0xa76 already was 4, it then posts `WM_PAINT` to the targeting display window DGROUP:0xa7c when that window exists. [BYTES seg11:0x71d4] [BYTES seg11:0x71d9] [BYTES seg11:0x71b6] [BYTES seg11:0x71cc] The manual says "press 'B' and the Targeting Display will show the game speed", from a minimum of 4 frames/second up to "as fast as your computer can go full out". [MANUAL Game Settings]
- After the Options dialog closes, seg10:0x7f18 re-reads the INI through seg6:0xb3e and resets DGROUP:0x1e50. [BYTES seg10:0x7f52] [BYTES seg10:0x7f60]

## Window timers
- At start-up seg6:0x1397 calls `SetTimer(DGROUP:0x90fc, 1, 1000, NULL)` and kills timer 1 at once. A failure shows "All timers used. Can't run game." (DGROUP:0x4ea). [BYTES seg6:0x1397] [BYTES seg6:0x13b1] [ORACLE newgame-0001]
- seg6:0x9f8 sets the only lasting timer of INSPACE, `SetTimer(main, 1, 1000 ms)` at seg6:0xa0f. A failure shows ENG_LANG string 2116 "Too many clocks or timers!" and posts `WM_DESTROY`. seg6:0x1a38 kills the timer at exit. [BYTES seg6:0xa0f] [BYTES seg6:0xa18] [BYTES ENG_LANG.DLL STRING 133] [BYTES seg6:0x1a38] [ORACLE wave-0001]
- ISPACE.DLL's only timer belongs to its Joystick dialog ([ispace.md](ispace.md)). [BYTES ISPACE.DLL seg9:0x46d4]
- The timer's `WM_TIMER` goes to seg2:0x11f8, which does no frame work: [BYTES seg2:0x813] [BYTES seg2:0x11f8]
  - **One-shot:** WM_CREATE sets DGROUP:0x1c. The first tick after that clears DGROUP:0x1c and DGROUP:0x8f6a, posts the private messages 0x8e2 (when DGROUP:0x8f6e is set) and 0x914 to the main window, sets DGROUP:0x1e and skips the rest of the handler. [BYTES seg2:0x398] [BYTES seg2:0x1211] [BYTES seg2:0x1224] [BYTES seg2:0x1236] [BYTES seg2:0x123b] Nothing writes DGROUP:0x8f6e and WndProc has no case for 0x8e2, which reaches DefWindowProc; the 0x914 handler calls `ShowWindow(main, SW_SHOWNORMAL)` and `SetActiveWindow(main)` ([shell.md](shell.md#start-up)). [BYTES seg2:0x1214 seg2:0x326 seg2:0x329 seg2:0x445 seg2:0x7df seg2:0x7ee]
  - **Cursor nudge:** while DGROUP:0x95bc is clear, the nudge runs at most every 20 s (DGROUP:0x1d44). It also waits until 10 s have passed since DGROUP:0x1d3c. It sets the cursor to x − 1, x or x + 1 of the position saved in DGROUP:0x1d48 at start-up, in a 3-step cycle. [BYTES seg2:0x1244] [BYTES seg2:0x1258] [BYTES seg2:0x12b1] [BYTES seg2:0x52] Its purpose is [OPEN].
    - The writers of DGROUP:0x95bc are seg4:0x4cc, seg4:0x5d8, seg8:0x142, seg8:0x179, seg13:0x1925, seg13:0x1941, seg15:0x5974 and seg15:0x5a64. DGROUP:0x95bc = 0 marks the demo, in which the computer pilot plays ([shell.md](shell.md#intro-and-title-screen)). [BYTES seg15:0x5a64 seg10:0x59 seg10:0x6c] [BYTES seg4:0x4cc] [BYTES seg8:0x142] [BYTES seg13:0x1925] [BYTES seg15:0x5974]
  - The focus watch, pause and resume below run only while DGROUP:0x8f66 is set. [BYTES seg2:0x12b6]
  - **Focus watch:** it compares the task of the active window with its own. In another task's window it calls ISPACE ordinal 7 command 3 (seg10:0x8686). Back in its own task and not iconic, it calls command 4 (seg10:0x86a8). DGROUP:0x8e5c holds which of the two ran last: seg10:0x8686 clears it and seg10:0x86a8 sets it. [BYTES seg2:0x12e7] [BYTES seg2:0x1302]
  - **Pause:** when the active window is none of the main window, DGROUP:0x934c and DGROUP:0xd1e, and the game is not paused, seg2:0x9c4 pauses. It sets DGROUP:0x592 to 2, clears DGROUP:0x8f68 and sets the caption to "Inner Space - PAUSED.." (string 2044). [BYTES seg2:0x131f] [BYTES seg2:0x1327] [BYTES seg2:0x9d7] [BYTES ENG_LANG.DLL STRING 128]
  - **Resume:** back in one of those windows while paused and not iconic, seg2:0xa06 clears the down state of Shift, Ctrl, Alt, Caps Lock and Space in the keyboard state. It restores the caption "Inner Space" (string 2337) and sets DGROUP:0x8f68. [BYTES seg2:0x132c] [BYTES seg2:0xa1f] [BYTES seg2:0xa66] [BYTES ENG_LANG.DLL STRING 147]
  - While the game is paused and no modeless dialog is open, the loop blocks in `GetMessage` and no frames run. [BYTES seg2:0xe3]
