# ADR 0010: Single instance

- **Status:** Accepted. [DESIGN]
- **Scope:** how the port refuses a second copy of itself, where the original relies on `hPrevInstance`. [DESIGN]

## Context
- **The original tests `hPrevInstance`** at start-up step 1 and shows "Another version of Inner Space is already running!" (string 2114 is never loaded; the text is DGROUP:0x4a0) before it creates anything ([shell.md](../re/shell.md#start-up)). Win32 passes `hPrevInstance` as NULL always. [BYTES seg6:0x12c4] [REF Win32 SDK, WinMain]
- **A window search is the wrong test.** `FindWindow` on the class `Inner Space` would also match the original game running in the same session, and a second instance started with DEMO or PLAY must still reach the first one's handlers (shell.md "DEMO and PLAY", M3). [DESIGN]

## Decision
1. **A named mutex,** `Local\InnerSpacePort.SingleInstance`, created at the start of `WinMain` before the install step and before the INI and asset resolution; `ERROR_ALREADY_EXISTS` shows the original's message with the original's caption and exits with the original's code. [DESIGN] [BYTES seg6:0x12c4 DGROUP:0x4a0]
2. **DEMO and PLAY forwarding** to the running instance (shell.md step 2) is M3's; until then a second instance started with those arguments exits with the message like any other. [OPEN]

## Alternatives
- **`FindWindow` on the class:** matches the original; see Context. [DESIGN]
- **No check:** two instances would share the INI and the saved games. [DESIGN]

## Consequences
- Checklist row SYS-17 describes the behaviour this ADR ports. [DESIGN]
