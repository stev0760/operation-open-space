#include "gfx/palette_message.h"

#include "gfx/gfx.h"
#include "shell/game_state.h"

namespace is::gfx {

bool emulate_query_new_palette(Gfx& gfx, int state, bool timer_seen,
                               const std::function<void()>& paint_routine,
                               const std::function<void()>& post_paint) {
    if (gfx.palette() == 0) return false;  // seg2:0x5cb: DGROUP:0x920e = 0 returns 0
    // GetDC, SelectPalette, RealizePalette: no device palette to realise (ADR 0007 decision 5).
    // InvalidateRect(hwnd, NULL, TRUE), UpdateWindow: WM_PAINT seg2:0x7ac.
    if (timer_seen) {  // DGROUP:0x1e [BYTES seg2:0x7ac]
        if (state == shell::kWave || state == shell::kHelp) {
            if (paint_routine) paint_routine();  // seg2:0x7c4: seg2:0x970 before the erase
        } else if (post_paint) {
            post_paint();  // seg2:0x7d7: PostMessage 0x978, runs after the erase
        }
    }
    // DefWindowProc: BeginPaint's WM_ERASEBKGND fills the client with BLACK_BRUSH.
    gfx.blacken();
    return true;
}

}  // namespace is::gfx
