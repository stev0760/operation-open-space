# ADR 0006: Window and scaling

- **Status:** Proposed. [DESIGN]
- **Scope:** the main window's modes and size, how the play area maps to pixels, and how the port scales on large or high-DPI displays. The palette is ADR 0007. [DESIGN]

## Context
- **The original's play area follows its client rectangle.** The play width and height come from `GetClientRect`, bounded by the screen size, and the visible world extent is the play size shifted by the scale level: one pixel is 16, 8 or 4 world units at `Size` 0, 1 or 2. Nothing letterboxes or stretches the play area when the client size changes ([shell.md](../re/shell.md#window-geometry)). [BYTES seg2:0xb15] [BYTES seg2:0x1b07] [BYTES seg6:0xb07]
- **It has two modes.** `Full` = 1 is a `WS_POPUP` window of the screen size, topmost when `OnTop` is set. `Full` = 0 is an overlapped, resizable window whose rectangle `ShowLeft` to `ShowBottom` defaults to 4/5 of the screen, centred, with a 600 × 400 minimum. A double click outside the HUD hotspots, SC_RESTORE and the boss key switch between them ([shell.md](../re/shell.md#window-geometry)). [BYTES seg6:0x0] [BYTES seg2:0xfc4] [BYTES seg2:0x1046] [BYTES seg2:0x3eb]
- **The HUD is laid out from the play size:** an 85-pixel strip at the bottom, panels and scrollbar placed from the width ([shell.md](../re/shell.md#window-geometry)). [BYTES seg11:0xafe] [BYTES seg2:0xbfc]
- **The period screen was 640 × 480 to 1024 × 768.** The oracle runs at 1024 × 768. A 2560 × 1440 or 3840 × 2160 display shows the same sprites at the same pixel size, so the ship and the HUD text shrink and the view widens. [ORACLE newgame-0001] [DESIGN]
- **Windows scales DPI-unaware windows by bitmap stretching,** which blurs pixel art, and lays out dialogs and menus itself when a process declares per-monitor DPI awareness. [REF Win32 SDK, High DPI Desktop Application Development]

## Decision
1. **The original's model stays.** The port keeps both modes, the INI keys `Full`, `OnTop`, `ShowLeft` to `ShowBottom` and `Size`, the 600 × 400 minimum, the mode switches and the client-rectangle-following play area and HUD layout, as [shell.md](../re/shell.md#window-geometry) describes them. [DESIGN]
2. **Logical pixels.** The game draws into an off-screen 8-bit surface of the logical size, which is the client size divided by an integer factor. The surface is presented with nearest-neighbour scaling by that factor, so every game pixel is a crisp block. The play area and the HUD are computed in logical pixels, so a larger factor shows a smaller world view, as a smaller screen did. [DESIGN]
3. **The factor** comes from the port's own key `Scale` in a `[Port]` section of the INI: 0 (the default) means automatic, the largest integer that keeps the logical height at or above 600 pixels, and never below 1. The values 1 to 4 fix it. A logical size below the 600 × 400 minimum lowers the factor. [DESIGN]
4. **Per-monitor DPI awareness (v2).** The process declares it in its manifest. Windows then scales dialogs, menus and the caption by the system DPI, and the port scales only its own surface. [DESIGN] [REF Win32 SDK, DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2]
5. **The Icon Watch, Ship Watch and `IS_SMART` windows** draw through the same surface mechanism at the same factor. Their `1/2x Scale` menu items keep the original's meaning. [DESIGN]

## Alternatives
- **No scaling, DPI-unaware:** Windows stretches the window with bilinear filtering, which blurs every sprite, and the 85-pixel HUD becomes unreadable at 4K without it. [DESIGN]
- **Fractional scaling of the surface:** blurs or ripples pixel art. Rejected. [DESIGN]
- **A fixed logical size (for example 1024 × 768) with a letterbox:** changes the original's behaviour, where the view widens with the window. Rejected. [DESIGN]

## Consequences
- The renderer of M2 draws into a DIB section at the logical size and presents it per frame; ADR 0007 defines its pixel format. [DESIGN]
- Mouse coordinates divide by the factor before the HUD hotspot tests. [DESIGN]
- `INI-` rows for `Full`, `OnTop`, `Show*` and `Size` port unchanged, and formats.md gains the `[Port]` section when M2 implements it. [DESIGN]
