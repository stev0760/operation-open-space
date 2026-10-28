// The HUD scrollbar as a real SCROLLBAR control (ADR 0013 decision 5). Win32 only: the host
// build compiles this file empty.
#ifdef _WIN32
#include "hud/scrollbar.h"

namespace is::hud {

void ScrollBar::layout(const shell::HudLayout& layout, bool race) {
    if (race) {  // seg11:0xc8c-0xca7: DestroyWindow runs; CreateWindow only when DGROUP:0x94e = 0
        destroy();
        return;
    }
    box_ = layout.scrollbar;  // (W/2 + 50, H', 14, 85) [BYTES seg11:0xcb6-0xcc8]
    laid_out_ = true;
    relayout();
}

void ScrollBar::relayout() {
    if (!laid_out_) return;
    const HWND parent = main_ ? main_() : nullptr;
    if (!parent) return;
    const int f = factor_ ? factor_() : 1;
    const int x = box_.x * f, y = box_.y * f, cx = box_.cx * f, cy = box_.cy * f;
    if (hwnd_ && !IsWindow(hwnd_)) hwnd_ = nullptr;
    if (!hwnd_) {
        // seg11:0xca9-0xcde: class "scrollbar", style 0x50000001 (WS_CHILD | WS_VISIBLE |
        // SBS_VERT), id 200 (0xc8), parent DGROUP:0x16.
        hwnd_ = CreateWindowExW(0, L"scrollbar", L"", 0x50000001, x, y, cx, cy, parent,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(0xc8)),
                                GetModuleHandleW(nullptr), nullptr);
        if (!hwnd_) return;
    } else {
        MoveWindow(hwnd_, x, y, cx, cy, TRUE);
    }
    SetScrollRange(hwnd_, SB_CTL, kScrollMin, kScrollMax, FALSE);  // seg11:0xceb
    if (pos_) set_pos(pos_());                                     // seg11:0xcf0-0xd00
}

void ScrollBar::set_pos(int pos) {
    if (hwnd_) SetScrollPos(hwnd_, SB_CTL, pos, TRUE);
}

void ScrollBar::destroy() {
    if (hwnd_ && IsWindow(hwnd_)) DestroyWindow(hwnd_);  // seg11:0x2fa-0x30a
    hwnd_ = nullptr;
    laid_out_ = false;
}

}  // namespace is::hud
#endif
