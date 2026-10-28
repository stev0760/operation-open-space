// The HUD scrollbar (docs/re/world.md "Scrollbar", ADR 0013 decision 5) as a real SCROLLBAR
// child (src/hud/scrollbar_win32.cpp): created once when the HUD is first laid out in the wave
// states, moved on relayout, never while the race flag DGROUP:0x94e suppresses it, destroyed
// when the wave states end. Each layout sets the range 100..500 and the position 600 - zoom, as
// the original's creation does [BYTES seg11:0xceb-0xd00]. The zoom DGROUP:0xa6c and the
// WM_VSCROLL handler seg11:0x7798 are the radar's (src/hud/radar.h, Radar::scroll); the radar
// sets the position through hud::set_scroll_pos (src/hud/program.h).
#pragma once

#include <functional>

#include "hud/hud.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace is::hud {

// The range of SetScrollRange(SB_CTL, 100, 500, FALSE) [BYTES seg11:0xce4 seg11:0xce6].
constexpr int kScrollMin = 100, kScrollMax = 500;

#ifdef _WIN32
class ScrollBar final : public Children {
public:
    // `pos` gives 600 - DGROUP:0xa6c; `main` the parent DGROUP:0x16; `factor` the ADR 0006 scale
    // of the rectangle.
    ScrollBar(std::function<int()> pos, std::function<HWND()> main, std::function<int()> factor)
        : pos_(std::move(pos)), main_(std::move(main)), factor_(std::move(factor)) {}
    ~ScrollBar() override { destroy(); }

    // seg11:0xc5d-0xd00: in a race the control goes (DestroyWindow runs, CreateWindow does
    // not); otherwise it is created once, then moved, and gets the range and the position.
    void layout(const shell::HudLayout& layout, bool race) override;
    // seg11:0x2f0 and the end of the wave states.
    void destroy();
    // The last layout again, at the current scale factor [DESIGN].
    void relayout();
    // SetScrollPos(DGROUP:0xa7a, SB_CTL, pos, TRUE).
    void set_pos(int pos);
    HWND hwnd() const { return hwnd_; }

private:
    std::function<int()> pos_;
    std::function<HWND()> main_;
    std::function<int()> factor_;
    HWND hwnd_ = nullptr;  // DGROUP:0xa7a
    shell::Box box_{};
    bool laid_out_ = false;
};
#endif

}  // namespace is::hud
