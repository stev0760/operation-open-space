# ADR 0007: Palette

- **Status:** Accepted. [DESIGN]
- **Scope:** how the port reproduces the original's 256-colour palette, its animation and fades, on displays that have no hardware palette. [DESIGN]

## Context
- **The original is a palette program.** ISPACE.DLL creates one 256-entry logical palette: entries 0–9 and 246–255 copy the system colours, entries 10–245 are the game's and are animated with `AnimatePalette(hpal, 10, 236, …)`, the only such call in either module ([shell.md](../re/shell.md#palette)). [BYTES ISPACE.DLL seg5:0x3596] [BYTES ISPACE.DLL seg5:0x441a] [ORACLE depth8-0002]
- **Pixels are indices.** Every 8-bit image becomes a device bitmap through `CreateDIBitmap` with `DIB_PAL_COLORS` and the indices 0–255, so a pixel value is an index into the game palette, and a change of the palette recolours every image at once. Colour schemes, colour allocation (commands 6 and 7) and the intro fades (ordinal 14) all work by rewriting entries and animating ([shell.md](../re/shell.md#palette)). [BYTES ISPACE.DLL seg5:0x2080] [BYTES ISPACE.DLL seg5:0x50cc]
- **At 8 bits per pixel the game chooses `PALETTEINDEX` colours and at higher depths `PALETTERGB`,** and the title asks for "256 color mode" unless the depth is exactly 8. The manual recommends 256 colours ([shell.md](../re/shell.md#window-geometry)). [BYTES ISPACE.DLL seg8:0x27f0] [BYTES seg15:0x71e8] [MANUAL Ship Factory]
- **At depth 24 under Wine the title, briefing bars, sprite, radar dots and dim HUD text lose their colours,** because `AnimatePalette` changes nothing on a true-colour display. On Windows 10 and 11 every display is true colour, and `WM_QUERYNEWPALETTE` and `WM_PALETTECHANGED` never arrive. [ORACLE depth8-0002] [REF Win32 SDK, Palettes]
- **`UpdateColors`** is called once, on the palette-changed path. It has no true-colour equivalent. [BYTES seg2:0x5a2]

## Decision
1. **The port behaves as a 256-colour display.** It keeps one 256-entry software palette with the original's layout: entries 0–9 and 246–255 hold the 20 static colours of the Windows VGA palette, entries 10–245 are the game's and start black with `PC_RESERVED` semantics. [DESIGN]
2. **Drawing goes into an 8-bit DIB section** at the logical size (ADR 0006), through the same GDI calls the original makes (`BitBlt`, `StretchBlt`, `PatBlt`, text), with the game's images as 8-bit DIB sections whose pixels are indices. The colour table of the drawing surface is the palette's indices (`DIB_PAL_COLORS`), so colours written as `PALETTEINDEX(n)` land as `n`. [DESIGN]
3. **Presentation converts indices to colours** once per frame with the current palette as a 256-entry lookup table into a 32-bit surface, which ADR 0006 scales to the window. `AnimatePalette`, the schemes, allocation and fades become writes to the table and cost nothing else. [DESIGN]
4. **The palette manager is ported as it is:** ordinal 4's commands 0–8 and ordinal 14, with their entry ranges, slot tables and the RGB order of ISPACE.DLL #5500 100, from [shell.md](../re/shell.md#palette). Command 0 reads the port's fixed device answers: `BITSPIXEL` 8, `SIZEPALETTE` 256, `NUMRESERVED` 20. [DESIGN]
5. **No palette messages, no `UpdateColors`.** The port handles neither `WM_QUERYNEWPALETTE` nor `WM_PALETTECHANGED` and never calls `RealizePalette` or `UpdateColors`; nothing else can change its colours. The title never shows the "256 color mode" hint. [DESIGN]
6. **Dialogs and menus** are Win32 controls and keep the system's true-colour drawing, as they did on an 8-bit display with the static colours. [DESIGN]

## Alternatives
- **Converting the images to 32-bit at load:** loses palette animation, schemes, fades and colour allocation, all of which the original does every frame or on every scene change. Rejected. [DESIGN]
- **Emulating the depth-24 path (`PALETTERGB`):** reproduces the colour losses the oracle saw at depth 24, which were never the intended look. Rejected. [DESIGN]
- **GDI's own 8-bit DIB section without a presentation step:** Windows converts it with the DIB's colour table at blit time, so animating the palette would mean rewriting the colour table through `SetDIBColorTable` each frame, which is the same lookup in another form but offers no control over scaling. Possible fallback. [DESIGN]

## Consequences
- M2 ports the palette manager into `src/gfx/palette` with tests that pin every command's entry ranges against shell.md. [DESIGN]
- **The presenter is one isolated step** (index surface + palette + window in, pixels on screen out), implemented with GDI (a 32-bit DIB section and `StretchBlt`). A Direct3D or OpenGL presenter can be added later for vsync or borderless full screen without touching the palette or the surface code. GDI stays the only renderer until a need is measured. [DESIGN]
- The sprite readers of M1 (`src/formats/dib`) already give 8-bit pixels and the 256-colour tables the lookup needs. [DESIGN]
- Checklist rows SYS-24, SYS-25, WIN-04 and WIN-55 describe the behaviour this ADR replaces or ports. [DESIGN]
