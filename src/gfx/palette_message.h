// The palette-message emulation of ADR 0007 decision 5: what WM_QUERYNEWPALETTE did to the
// main window on a 256-colour Windows, run by the port itself when the main window is
// reactivated while a palette exists (docs/re/shell.md "Palette", **Palette messages**).
// Portable: the caller passes the paint routine and the post of private 0x978 and, on Win32,
// presents the surface afterwards.
#pragma once

#include <functional>

namespace is::gfx {

class Gfx;

// The handler seg2:0x5cb with the synchronous WM_PAINT seg2:0x7ac it causes:
//  - no palette (DGROUP:0x920e = 0, Gfx::palette() == 0): nothing, returns false (the
//    handler's 0) [BYTES seg2:0x5cb seg2:0x39e];
//  - otherwise RealizePalette has no effect in the 8-bit model, InvalidateRect(hwnd, NULL,
//    TRUE) + UpdateWindow run WM_PAINT at once: with DGROUP:0x1e (`timer_seen`) set, states 1
//    and 7 call the paint routine seg2:0x970 first (`paint_routine`) and every other state
//    posts private 0x978 (`post_paint`) [BYTES seg2:0x7ac seg2:0x7c4 seg2:0x7d7]; then
//    DefWindowProc's BeginPaint erases the whole client area with the class brush
//    BLACK_BRUSH [BYTES seg6:0x14aa], which on the port is the whole surface set to index 0.
//    Returns true.
// So in every state the surface ends black; in states 1 and 7 the repaint comes before the
// erase, elsewhere the posted 0x978 runs after it. [DESIGN] ADR 0007 decision 5.
bool emulate_query_new_palette(Gfx& gfx, int state, bool timer_seen,
                               const std::function<void()>& paint_routine,
                               const std::function<void()>& post_paint);

}  // namespace is::gfx
