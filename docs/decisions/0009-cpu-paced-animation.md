# ADR 0009: CPU-paced animation

- **Status:** Accepted. [DESIGN]
- **Scope:** the rate the port gives to animations that the original paces by its frame loop rather than by the clock. The mechanisms themselves stay in [shell.md](../re/shell.md#screen-states) and [timing.md](../re/timing.md). [DESIGN]

## Context
- **The original paces some loops by the CPU.** The state-3 pass has no millisecond gate: it counts passes against the adaptive threshold DGROUP:0x9004, whose 16-bit rescaling toward 72 frames per second can wrap, after which every pass is a frame. The briefing's typers advance two or ten characters per step behind a 50 ms gate re-armed at each step, so on a fast CPU a step is one frame. The slide-ins and the ship flight are frame counts, not durations. [BYTES seg7:0x326-0x361 seg7:0x2206 seg17:0x1b3e seg17:0x20fa] [ORACLE start-0001]
- **The game was designed for a 486.** The user reports that on faster machines the briefing text types far too fast and the intro's spinning logo spins too fast before it slows and opens, and that a true 486 shows the intended pace. The Wine oracle on a modern CPU shows the same fast, bursty timing (190 ambulance frames in under 0.56 s), so "matches the oracle" is the wrong target for these loops. [ORACLE start-0001] [DESIGN]
- **One loop is paced by the disk.** The load screen's greeting starts the looping bubbles 0x6f, and the world load seg9:0x22b4(0) that follows ends with the stop-all 0x6e, so the bubbles play for as long as the disk read takes: seconds in 1994, 20 ms in the oracle. The 2000 ms window that follows is silent. [BYTES seg15:0x666f seg10:0xad seg9:0x276a seg10:0xb7] [ORACLE sound-0001]
- **Clock-gated timing is a different matter.** Waits, holds and deadlines taken from `GetCurrentTime` or `timeGetTime` run the same on every CPU, and the port matches them exactly ([ADR 0001](0001-timestep.md)). [DESIGN]

## Decision
1. **A CPU-paced or disk-paced loop gets the rate its own mechanism names, not the rate a fast machine reaches.** Where the bytes gate a step (the typers' 50 ms gate), the port runs one step per gate: the briefing types two characters every 50 ms and the page typer ten. [DESIGN] [BYTES seg9:0x8d18 seg17:0x2c5a]
2. **The state-3 frame cadence is fixed at 72 frames per second,** the target the threshold rescales toward, so frame-counted animations (the slide-ins, the ambulance's 190 growth frames, the ship's fly-in) take their frame count at 13.9 ms per frame. [DESIGN] [BYTES seg7:0x2206] [REF timing.md, adaptive screen threshold]
3. **Each such loop is listed here with its mechanism and the port's rate,** and the oracle comparison records both the oracle's and the port's measured rate instead of counting the difference as a defect. [DESIGN]

| Loop | Mechanism | Port's rate |
|---|---|---|
| Briefing lower text and pages | 2 or 10 characters per step, 50 ms gate re-armed per step | one step per 50 ms (55.6 ms at 72 Hz) |
| Briefing slide-ins, ambulance, ship flight | frame counts 150, 190, 112, 102 and the fly-in's 30 | 72 frames per second |
| Load-screen bubbles 0x6f | looped until the disk load's stop-all | the stop-all plays after the 2000 ms window, so the bubbles last the load screen |
| Desk launch (seg15:0x0, seg15:0x9e6) | frame-paced steps, no time base | the state-0 cadence; to be set when the launch leftovers are read [OPEN] |
| Intro logo spin | reported by the user as CPU-fast; its pacing in the bytes is not yet tied to this ADR [OPEN] | unchanged until read |

4. **No ADR amendment is needed to add a row:** a new loop gets a row, a source comment naming this ADR, and a line in the stage's comparison. [DESIGN]

## Alternatives
- **Matching the Wine oracle's measured rates:** reproduces a CPU artefact, and the number changes with the host. [DESIGN]
- **Emulating a 486's pass rate:** needs a model of the original's per-pass cost that the bytes do not give. [DESIGN]

## Consequences
- The briefing cycle takes about 27 s in the port against 25-27 s in the oracle, with the ambulance phase longer (11.4 s against 9.0 s) and the typing slower (55.6 ms against 64 ms per step on the oracle host). [ORACLE start-0001] [DESIGN]
- Checklist rows that describe these loops carry the port's rate as a [DESIGN] note, not an oracle mismatch. [DESIGN]
