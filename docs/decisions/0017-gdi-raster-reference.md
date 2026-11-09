# ADR 0017: Wine's GDI raster as the reference for polygon masks

- **Status:** Proposed (2026-11-09). [DESIGN]
- **Scope:** the port's pixel tables and emulations of GDI polygon and region fills: the wing cut masks of ISPACE.DLL ordinal 15 (`src/gfx/wing_cut`) and the landing pad's trapezoid mask and bands of seg15:0x482e and seg15:0x4a2e (`src/gfx/pad`). [DESIGN]

## Context
- The original draws these shapes with GDI `Polygon`, `CreatePolygonRgn` and pen lines on 1-bpp or 8-bpp memory bitmaps, and the pixels that result are GDI's edge rule, not the game's arithmetic. [BYTES ISPACE.DLL seg5:0x11a4] [BYTES seg15:0x482e seg15:0x4a2e]
- Every oracle digest comes from the original under Wine, so the pixels the comparisons hold the port to are Wine's raster. A plain rounding rule for the pad's slanted edges failed at many play heights against those pixels ([pad.cpp](../../src/gfx/pad.cpp)). [ORACLE core-0003] [ORACLE core-0007] [ORACLE core-0008]
- Windows GDI and Wine's GDI agree on most edges and are not proven to agree on all; the port has not run on real Windows yet (M10). [OPEN]

## Decision
1. The reference raster for a polygon mask is Wine's GDI (the oracle's Wine 11.0, checked against 11.20, 32- and 64-bit), held either as a table or as an emulation of its scan and pen rules, and pinned bit for bit by a Win32 test that draws the same shape through GDI under Wine. [DESIGN]
2. The port draws from that table or emulation on every host, so its pixels do not depend on the host's GDI. [DESIGN]

## Alternatives
- Issuing the bytes' `Polygon` calls on a 1-bpp bitmap at run time: pixels follow the host's GDI, which on real Windows may differ from the oracle's. Chosen against until a real-Windows run measures the difference (M10). [DESIGN]

## Consequences
- The Win32 tests `test_gfx_wing_cut_win32` and `test_gfx_pad_win32` pin the table or emulation against GDI under Wine; on Windows they also measure the two GDIs against each other. [DESIGN]
- Should Windows GDI differ, M10 decides whether the reference moves to Windows. [OPEN]
