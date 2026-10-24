# ADR 0001: Timestep

- **Status:** Accepted. [DESIGN]
- **Scope:** the game loop of milestone M2 and every later milestone that adds frame logic. [DESIGN]
- **Facts:** [docs/re/timing.md](../re/timing.md) holds the original's timing, with its evidence. Each Context bullet links to the section it rests on and keeps only the conclusion the decision needs.

## Context
- **Frames come from a busy loop** ([Message loop and idle step](../re/timing.md#message-loop-and-idle-step), [The frame gate](../re/timing.md#the-frame-gate)). A pass counter, a threshold and, with the fine timer, a millisecond gate decide when a frame runs. No frame depends on `WM_TIMER`.
- **The wave targets `FrameRateSet`** ([Adaptive wave threshold](../re/timing.md#adaptive-wave-threshold), [Rate settings](../re/timing.md#rate-settings)). At the default of 36, wave frames come 27–29 ms apart and average 36 per second.
- **Game time counts frames** ([Game time](../re/timing.md#game-time)). It runs at `FrameRateSet` / 36 of real time when the machine keeps up, and slower when it does not.
- **Some deadlines follow the wall clock** ([Game time](../re/timing.md#game-time)). They compare against the `GetCurrentTime` sample of the current pass and do not scale with the rate.
- **The speed controls set `FrameRateSet`** ([Rate settings](../re/timing.md#rate-settings), [Speed keys](../re/timing.md#speed-keys)). It takes even values from 4 to 104, or 1200 for the maximum, at which a frame runs on every pass.
- **Calibrate measures the loop** ([Calibrate](../re/timing.md#calibrate)). It sets the starting pass thresholds and picks branches that compare DGROUP:0x8ffc with constants up to 2000.
- **The other screens aim at fixed rates** ([Adaptive screen threshold](../re/timing.md#adaptive-screen-threshold)). seg7:0x2206 steers states 0, 2, 3, 6, 8 and 9 and the dialog pass toward 72 frames per second, or in state 6 toward 32 per second or a record field. State 0's 14/13 ms gate holds its frames 13–15 ms apart. State 5's threshold never adapts, so its frames follow the pass rate.

## Decision
1. **Discrete frames.** The port runs each screen state as a sequence of frames, and each frame runs that state's frame logic once. A monotonic high-resolution clock schedules the frames, and the thread sleeps between them instead of spinning. [DESIGN]
2. **Wave rate.** In the wave states, frame k is due at t₀ + k · 1000 / R ms, with R the current `FrameRateSet`. The same applies to state 6 while its gate condition holds. A change of R restarts the schedule from the current frame. [DESIGN]
3. **No catch-up.** A late port runs one frame and restarts the schedule from it, without running missed frames. The original never runs more than one frame per pass, so a slow machine slows game time instead of catching up. [DESIGN] [BYTES seg7:0x9ce] [ORACLE runtime-0001]
4. **Game time is the frame count.** The port keeps the frame counters (the equivalents of DGROUP:0x1e68 and DGROUP:0x5a8) and divides by 36 for the HUD clock. Every rule that counts frames in the original counts frames in the port. [DESIGN] [BYTES seg11:0x1f0e]
5. **Wall clock per pass.** The port keeps a 32-bit millisecond tick from the same monotonic clock, the equivalent of `GetCurrentTime`. It samples the tick into the equivalent of DGROUP:0x1e50 once per loop pass. A pass runs before every frame and after every batch of window messages. Wall-clock deadlines compare against that sample. [DESIGN] [BYTES seg7:0x24]
6. **Speed controls unchanged.** The port keeps the INI key `FrameRateSet`, the Play Speed mapping, Alt+ and Alt−, the `B` display and the 5 s speed display. At the maximum (above 1000) it runs frames back to back without waiting, as the original does. [DESIGN] [BYTES seg10:0x7e1c] [BYTES ISPACE.DLL seg9:0x2242]
7. **No pass machinery.** The port does not emulate the pass counter DGROUP:0x9144 or the thresholds DGROUP:0x8f76, DGROUP:0x9004 and DGROUP:0x9002. Nor does it emulate the 250 ms and 1000 ms threshold adjustments of the wave and of seg7:0x2206, the 16-bit wrap of their products, or the millisecond gates. It keeps the frames-per-second count behind the `B` display (DGROUP:0x59a), which both adjustments write. [DESIGN] [BYTES seg7:0xadc] [BYTES seg7:0x232c]
8. **Other screens.** State 0 runs every 13 ms, the lower bound of its gate: seg7:0x2206 steers it toward 72 frames per second, and the 14/13 ms gate bounds it to between 1000/15 and 1000/13 per second. The probe's title count of 76 per second sits at the upper rate bound ([Adaptive screen threshold](../re/timing.md#adaptive-screen-threshold)). States 2, 3, 8 and 9, the dialog pass and state 6 outside its gate condition run at seg7:0x2206's targets: 72 Hz, or 32 Hz in state 6 while DGROUP:0x914a = 1 and DGROUP:0x92a2 = 0. Inside its gate condition, state 6 follows decision 2, because the gate holds its frames S to F + 1 ms apart whatever seg7:0x2206 aims at. These rates are provisional until the oracle measures them. State 5 is the only state whose threshold never adapts, so its original rate depends on the machine. The port runs it at 72 Hz as well until the oracle measures it. [DESIGN] [BYTES seg7:0x2262] [BYTES seg7:0x27d] [BYTES seg6:0xaa8] [OPEN]
9. **Calibrate is a constant.** The port does not measure Calibrate. It uses a constant above 2000, provisionally 14 000 (inside the range Wine measures), so every comparison against DGROUP:0x8ffc takes its fast-machine branch. The effective `GameSpeed` follows the original rule: the INI value, or Calibrate when the INI holds 20000. seg15:0x7470 still compares it with 800. [DESIGN] [ORACLE runtime-0001] [BYTES seg6:0xb6c] [BYTES seg15:0x7470]
10. **One window timer.** The port keeps a 1000 ms timer on the main window for the focus watch, pause and resume, and the one-shot messages after creation. Whether it keeps the cursor nudge is left to the window ADR. [DESIGN] [BYTES seg6:0xa0f] [OPEN]

## Alternatives
| Alternative | Verdict |
|---|---|
| Emulate the pass loop literally: counter, adaptive thresholds, ms gates, Calibrate | Rejected. Its purpose is to approximate a target rate (R in the wave, mostly 72 per second elsewhere) on hardware of unknown speed, and the busy loop burns a core. In the wave its average equals decision 2's rate, and its 27/28/29 ms spacing is a quantisation side effect. [DESIGN] |
| Fixed 36 Hz simulation, with `FrameRateSet` changing only the render rate | Rejected. In the original, game time and frame-counted rules scale with `FrameRateSet`. [MANUAL Game Settings] [BYTES seg7:0xb32] |
| Variable timestep that scales movement by elapsed time | Rejected. The original's logic is integer and per frame, so scaling breaks bit-exact comparison and every rule that counts frames. [DESIGN] |
| Fixed step with interpolated rendering at the display refresh rate | Deferred. It is a modernisation that needs its own ADR, and the original draws inside its frame functions. [DESIGN] |
| Pacing by `SetTimer`/`WM_TIMER` | Rejected. The original does not pace this way, and window-timer resolution cannot hold 27.78 ms intervals. [BYTES seg2:0x11f8] [REF Win32 SetTimer] |
| Catch-up: run several frames after a stall to keep game time in step with real time | Rejected. The original never catches up. [BYTES seg7:0x9ce] |

## Consequences
- On a machine that holds R, game time matches the original exactly: R / 36 of real time, with the same frame-counted rules. [DESIGN]
- Frames are evenly spaced (27.78 ms at 36), where the original mixes 27, 28 and 29 ms. Frame logic does not see the difference. Wall-clock deadlines move by up to one frame relative to the frame count. [DESIGN]
- The original runs per-pass work, such as the keyboard poll seg11:0x4db0, about 11 500 times per second in the wave. The port runs it once per pass, so input must be latched between passes to keep short presses. Whether seg11:0x4db0 itself latches is [OPEN]. [BYTES seg7:0x881] [ORACLE newgame-0002]
- Deterministic tests become possible by injecting the frame schedule, the millisecond tick and the `rand()` seeds ([rng.md](../re/rng.md)). [DESIGN]
- The port sleeps between frames, so it no longer occupies a whole core. [DESIGN]
- At the maximum speed, a modern machine may run thousands of frames per second, which makes game time race. A cap would be a modernisation with its own ADR. [DESIGN]
- The rates of states 2, 3, 5, 8 and 9, of the dialog pass and of state 6 outside its gate condition stay provisional until measured. [OPEN]

## Open questions
- [OPEN] The frame rates on the oracle of states 2, 3, 5, 8 and 9, of the dialog pass and of state 6 outside its gate condition, and whether their animations count frames or follow the wall clock.
- [OPEN] Whether skipping the intro skips the Calibrate measurement, and so the first reading of `FrameRateSet`.
- [OPEN] Whether the keyboard poll seg11:0x4db0 latches key presses between frames.
- [OPEN] A per-rule survey of frame-counted and wall-clock rules.
- [OPEN] The constant for Calibrate, if a Calibrate branch turns out to change gameplay rather than detail.
