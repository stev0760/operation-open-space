// The Hall of Justice's pictures on the renderer of src/gfx (hall.h Screen): scene 110 and the
// jet of the landing (seg15:0x302e, seg15:0x1a42, seg15:0x3caa), the two ship frames
// (seg15:0x1eac), the descent in state 8 (seg15:0x374c), the dead ship's marks (seg15:0x28b2) and
// scene 115 (seg15:0x760). The scene DC DGROUP:0x929a of scene 115, the monitor (seg15:0x8604), the
// typer's glyphs and the launch frames are a desk::GdiDesk's, which this screen owns and lends to
// the Hall as its desk::Canvas. Every COLORREF goes through gfx::Gfx::dc_colour (ADR 0007). Win32
// only; the host build compiles an empty translation unit.
#pragma once

#ifdef _WIN32
#include <windows.h>

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "desk/gdi_desk.h"
#include "gfx/gfx.h"
#include "gfx/launch_ship.h"
#include "hall/hall.h"

namespace is::hall {

// seg15:0x20ca's inputs for the player's ship, as the visit's (src/hud/hooks.cpp visit_ship_look
// fills this slot: binding request) [DESIGN: the slot stands for the far call across the port's
// libraries]; empty: no ship.
std::function<gfx::VisitShipArt()>& ship_look_slot();

// seg15:0x28b2(Q, Q2, k, sz) [BYTES seg15:0x28b2-0x2a06] (read in part B, no verifier has checked
// it): a = lo16(sz·k) - 3, b = lo16((sz - 8)·k); 250 times: x = rand() % a + 3, y = rand() % b, then
// rand() % 3 = 0: d = rand() % 4 + 2 and PatBlt(Q and Q2, x, y, d, d, 0x00A000C9 PATAND) under the
// sparse-dot brush DGROUP:0x5ea8; else d = rand() % 4 + 2 and Ellipse(Q and Q2, x, y, x + d, y + d)
// under the stock BLACK_BRUSH with each DC's pen. Four draws a mark, 1000 in all. A zero divisor
// (sz·k = 3 or sz = 8) faults in the original: the port skips the draws and logs once [DESIGN
// ADR 0016 decision 4]. Returns the number of draws.
int dead_ship_marks(HDC q, HDC q2, int k, int sz, shell::Rng& rng, const gfx::LaunchTodo& todo);

class HallGdi final : public Screen {
public:
    HallGdi(std::function<gfx::Gfx*()> gfx, gfx::LaunchTodo todo);
    ~HallGdi() override;

    std::optional<desk::Rect> landing(int W, int H, int play_h, const std::string& slow, bool render) override;
    void jet_frames(bool dead, shell::Rng& rng) override;
    int incoming(int first, int d92d6, int d92d8, int d92de) override;
    void two_frames() override;
    void black(int W, int H) override;
    HallScene hall_scene(int W, int H) override;
    void ship_frame(int x, int y, int s4) override;
    void descent_frame(int W, int y, int s4, int flag, int hgt) override;
    void lend_ship() override {}
    void free_scene() override { scene110_.reset(); }

    desk::GdiDesk& desk() { return desk_; }
    // seg10:0x9526: ordinal 32 for IGRAPHIC.DLL #6600 id at W x H (src/desk/scene_gdi.h
    // render_scene); the program fills it.
    std::function<desk::Render(int id, int W, int H)> scene;

private:
    HDC screen() const;
    void free_frames();

    std::function<gfx::Gfx*()> gfx_;
    gfx::LaunchTodo todo_;
    desk::GdiDesk desk_;
    gfx::ImagePtr scene110_;   // DGROUP:0x9294 in DGROUP:0x929a while it holds scene 110
    gfx::ImagePtr body_;       // DGROUP:0x92b0
    gfx::ImagePtr square_;     // DGROUP:0x92b2, with the flame
    gfx::ImagePtr flameless_;  // DGROUP:0x92b4
    int sz_ = 0;
    int detail_ = 3;
    gfx::ImagePtr frames_;     // DGROUP:0x3ccc in DGROUP:0x3cdc
    gfx::ImagePtr frames_mask_;// DGROUP:0x3cce in DGROUP:0x3cde
    struct Frame {
        int w = 0, h = 0, y = 0;
    };
    std::array<Frame, 16> tab_{};
    gfx::ImagePtr work_;       // DGROUP:0x92b8
    struct Jet {
        int y10 = 0, phase = 0, idx = 0, speed = 0, step = 0, level = 0, prevy = 0;
        RECT clip{};
        bool started = false;
    } jet_;
    int d92d6_ = 0, d92d8_ = 0;
};

}  // namespace is::hall
#endif
