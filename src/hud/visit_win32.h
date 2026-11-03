// owner: M4 stage 3, track VISIT.
// The landing scene of the ambulance visit on the ADR 0006 surface: world::VisitScreen with the
// GDI calls of seg15:0x302e, seg15:0x3590 / seg15:0x374c (the descent), seg16:0x1f78 /
// seg16:0x27ea / seg16:0x2144 (the panels, the monitors, the labels and the bars), seg16:0x2e3c
// (the monitor's font, colours, cursor and scroll), seg15:0x4c76 (the pad's strip), seg16:0x64fa
// (the left monitor) and seg15:0x84b0 (the CRT flash). Every COLORREF goes through
// gfx::Gfx::dc_colour (ADR 0007). Where the original's pictures are read only in summary
// (the #6600 scene, the monitor face DGROUP:0x95a0, the descending ship's frames, the
// incoming-jet animation seg15:0x3caa) the port draws a plain stand-in, tagged [OPEN] at each.
// Win32 only; the host build compiles an empty translation unit.
//
// This file does not reach app::g: the program (src/hud/hooks.cpp) hands it the renderer, the
// HUD canvas and its data through the constructor, so the tests and the dialogs library can
// link it.
#pragma once

#ifdef _WIN32
#include <windows.h>

#include <functional>
#include <string>

#include "gfx/gfx.h"
#include "world/visit.h"

namespace is::dialogs {
struct AmbulanceEnv;
}

namespace is::hud {

class GdiCanvas;

// What seg16:0x2144's status block shows: the PLAYER's system words +0x26 ... +0x2c and the
// fuel gauge 100·fuel/tank, the design's lamp rows DGROUP:0x96f4 + 2k and the shield +0xe9 and
// cloak +0xe7 bytes [BYTES seg16:0x2448-0x25c9].
struct VisitStatus {
    int value[5] = {100, 100, 100, 100, 100};
    int row[4] = {0, 1, 2, -1};
    bool shield = false, cloak = false;
};

class VisitGdi final : public world::VisitScreen {
public:
    VisitGdi(std::function<gfx::Gfx*()> gfx, std::function<GdiCanvas*()> canvas,
             std::function<VisitStatus()> status, std::function<gfx::ImagePtr()> ship)
        : gfx_(std::move(gfx)), canvas_(std::move(canvas)), status_(std::move(status)), ship_(std::move(ship)) {}
    ~VisitGdi() override;

    void landing_scene(const world::VisitLayout& l, const std::string& slow_text) override;
    int incoming(int first) override;
    void descent_start(const world::VisitLayout& l) override;
    void descent_frame(const world::VisitLayout& l, int y, int s4, bool braking) override;
    void redraw(const world::VisitLayout& l, int phase, int saved_phase, bool left_on) override;
    void ship_on_pad(const world::VisitLayout& l, int x, int y, int s4, int strip_h) override;
    void pad_strip(const world::VisitLayout& l, int offset, int s4) override;
    void rebuild_ship() override {}
    void bar(int x, int y, int w, int h, int v) override;
    void monitor_begin(int font_h) override;
    void monitor_end() override;
    int text_extent(const std::string& cp1252) override;
    void glyph(int x, int y, char c) override;
    void black(int x, int y, int w, int h) override;
    void block(int x, int y, int w, int h, std::uint32_t colour) override;
    void scroll_up(int l, int t, int r, int b, int n) override;
    void flash(int x, int y) override;
    void save_monitor(int x, int y, int w, int h) override;
    void left_monitor(const world::VisitLayout& l, bool limited) override;
    void black_screen() override;
    void free_scene() override;

    // The strings of the left monitor (2330, 2331, 2027) [BYTES seg16:0x6574-0x65fb].
    std::function<std::string(int)> lang;
    // DGROUP:0x920c = 0, the Limited edition.
    std::function<bool()> limited;

private:
    HDC screen() const;
    void panels(HDC s, const world::VisitLayout& l);
    void monitor_faces(HDC s, const world::VisitLayout& l);
    void status_block(const world::VisitLayout& l);
    void draw_pad(HDC d, int x, int y, int w, int h);
    void draw_ship(HDC d, int x, int y, int s4);

    std::function<gfx::Gfx*()> gfx_;
    std::function<GdiCanvas*()> canvas_;
    std::function<VisitStatus()> status_;
    std::function<gfx::ImagePtr()> ship_;
    gfx::ImagePtr monitor_;  // DGROUP:0x953a
    gfx::ImagePtr strip_;    // DGROUP:0x953c
    int strip_x_ = 0, strip_y_ = 0;
    HFONT font_ = nullptr;
    HGDIOBJ old_font_ = nullptr;
    int saved_dc_ = 0;
    int incoming_ = 0;
};

// The slot through which src/dialogs reaches the program's Options dialog: src/dialogs/dialogs.cpp
// fills it with dialogs::run_ambulance_options at start-up, src/hud/hooks.cpp calls it (ishud
// does not link isdialogs, which the link order puts first) [DESIGN].
std::function<int(dialogs::AmbulanceEnv&)>& ambulance_dialog_slot();

}  // namespace is::hud
#endif
