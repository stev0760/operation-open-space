// owner: M4 stage 3, track VISIT.
// The landing scene of the ambulance visit on the ADR 0006 surface: world::VisitScreen with the
// GDI calls of seg15:0x302e, seg15:0x3590 / seg15:0x374c (the descent), seg16:0x1f78 /
// seg16:0x27ea / seg16:0x2144 (the panels, the monitors, the labels and the bars), seg16:0x2e3c
// (the monitor's font, colours, cursor and scroll), seg15:0x4c76 (the pad's strip), seg16:0x64fa
// (the left monitor) and seg15:0x84b0 (the CRT flash). Every COLORREF goes through
// gfx::Gfx::dc_colour (ADR 0007). The pad ship (seg15:0x20ca, seg15:0x1eac), the jet's frames
// (seg15:0x1a42) and flight (seg15:0x3caa), the pad (seg15:0x482e), the descent's drawing
// (seg15:0x374c) and the pad's move (seg15:0x4c76) are M4 stage 6's (track PAD:
// work/analysis/m4/s6-pad.md with s6-pad-verify.md, s6-scene.md section 3). Where the original's
// pictures are read only in summary (the #6600 scene, the monitor face DGROUP:0x95a0) the port
// draws a plain stand-in, tagged [OPEN] at each. Win32 only; the host build compiles an empty
// translation unit.
//
// This file does not reach app::g: the program (src/hud/hooks.cpp) hands it the renderer, the
// HUD canvas and its data through the constructor, so the tests and the dialogs library can
// link it.
#pragma once

#ifdef _WIN32
#include <windows.h>

#include <array>
#include <functional>
#include <optional>
#include <string>

#include "desk/gdi_desk.h"
#include "desk/scene.h"
#include "gfx/gfx.h"
#include "gfx/launch_ship.h"
#include "gfx/pad.h"
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

    std::optional<world::LandingSlots> landing_scene(const world::VisitLayout& l, const std::string& slow_text,
                                                     bool black, bool render) override;
    int incoming(int first) override;
    void descent_start(const world::VisitLayout& l, bool black, bool fade) override;
    void descent_frame(const world::VisitLayout& l, int y, int s4, int flag) override;
    void redraw(const world::VisitLayout& l, int phase, int saved_phase, bool left_on) override;
    void ship_on_pad(const world::VisitLayout& l, int x, int y, int s4, int strip_h) override;
    // seg15:0x4c76's drawing at `offset` without a first call (pad_move(l, 0, 0, offset, s4)); the
    // visit calls pad_move with its first and down arguments (M4 s6 PAD, binding 8) [DESIGN].
    void pad_strip(const world::VisitLayout& l, int offset, int s4) override;
    // seg15:0x4c76(first, down)'s drawing at DGROUP:0x181c = offset [BYTES seg15:0x4c76-0x52d8]: a
    // first upward call rebuilds the strip from the pad and the body image DGROUP:0x92b0
    // (gfx/launch_ship.h strip_rebuild), then rows 0 ... s4 + 0x3d to (half(W) - half(padW),
    // DGROUP:0x10 - s4 - 0x43 + offset) with no PatBlt before (verified claims 54-59). DGROUP:0x95b8
    // is 7 in a visit, so the desk's composite never runs here (s6-pad.md section 4) [DESIGN].
    void pad_move(const world::VisitLayout& l, int first, int down, int offset, int s4) override;
    // seg15:0x4c76's new body (state 6, first, down = 0, DGROUP:0x9563 != 0): DGROUP:0x92b0 ...
    // 0x92b4 deleted and seg15:0x2a08's branch A from INI MainShip (main_ship_art) [BYTES
    // seg15:0x4c84-0x4cea] (verified claim 33 of s3-visit-verify.md); the 2000 ms is the visit's.
    void rebuild_ship() override;
    // 4·trunc(bmHeight(DGROUP:0x92b0)/k) [BYTES seg15:0x4cec-0x4d1c] (review H1).
    int pad_s4(int roster_s4) override;
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
    // seg15:0x20ca's inputs for the player's ship (gfx/launch_ship.h VisitShipArt), read at
    // seg15:0x1a42 (the landing scene's end) [BYTES seg15:0x1aaa]; empty: no pad ship.
    std::function<gfx::VisitShipArt()> ship_look;
    // seg15:0x2a08's branch A for INI MainShip (the new body's rebuild), as the desk's
    // (src/desk/gdi_desk.h ShipSource).
    std::function<std::optional<gfx::LaunchShipArt>()> main_ship_art;
    // DGROUP:0x90ec, the play height seg15:0x482e reads; l.H (DGROUP:0x10) when empty [DESIGN].
    std::function<int()> play_height;
    // The visit's todo log (seg15:0x28b2, ordinal 18 with x = 1, a scene pattern that did not load).
    gfx::LaunchTodo todo;
    // seg10:0x9526: ordinal 32 for IGRAPHIC.DLL #6600 id at W x H (src/desk/scene_gdi.h
    // render_scene); the program fills it.
    std::function<desk::Render(int id, int W, int H)> scene;
    // DGROUP:0x929a (the bitmap DGROUP:0x9294), kept for seg15:0x3caa's clipped copies.
    const gfx::ImagePtr& scene_image() const { return scene_img_; }
    // The pictures state 6's launch reads (src/desk/gdi_desk.h GdiDesk::set_visit_pictures):
    // DGROUP:0x92b0, DGROUP:0x92b2 and the pad DGROUP:0x92a8.
    // With DGROUP:0x953f != 0 and DGROUP:0x9563 = 0 they carry ship_look() for seg15:0x2a08 branch
    // B's common part (review H2) [BYTES seg15:0x2ca3-0x2cbe].
    desk::LaunchPictures launch_pictures() const {
        desk::LaunchPictures p;
        p.body = body_;
        p.square = square_;
        p.pad = pad_;
        if (wing_repair && wing_repair() && ship_look) {
            p.wing_repair = true;
            p.look = ship_look();
        }
        return p;
    }
    // DGROUP:0x953f != 0 and DGROUP:0x9563 = 0 at the launch (world/purchases.h Shop wing and
    // new_body); the program fills it.
    std::function<bool()> wing_repair;

    // seg15:0x302e's words for the jet (s6-scene.md section 2): DGROUP:0x92d6 and DGROUP:0x92d8
    // (step 5: trunc(22 H / 100) and H - 10; step 7 with the scene: slot 1's top - 40), DGROUP:0x92de
    // (slot 0's bottom + 1), which the landing scene sets from the layout's jet words and the
    // scene's slots [BYTES seg15:0x314b-0x318f seg15:0x319c-0x3257].
    int d92d6 = 0, d92d8 = 0, d92de = 0;

private:
    HDC screen() const;
    void panels(HDC s, const world::VisitLayout& l);
    void sloped_sides(HDC s, const world::VisitLayout& l);  // seg15:0x454c
    void monitor_faces(HDC s, const world::VisitLayout& l);
    void status_block(const world::VisitLayout& l);
    void draw_pad(HDC d, int x, int y, int w, int h);  // the bare pad DGROUP:0x92b6
    // seg15:0x1a42: seg15:0x20ca (build_ship) and the jet's 16 frames into DGROUP:0x3ccc.
    void jet_frames(const world::VisitLayout& l);
    void build_ship();  // seg15:0x20ca
    void two_frames();  // seg15:0x1eac
    void build_pad(const world::VisitLayout& l);  // seg15:0x3590's greys and seg15:0x482e
    void free_frames();  // DGROUP:0x3ccc / 0x3cce

    std::function<gfx::Gfx*()> gfx_;
    std::function<GdiCanvas*()> canvas_;
    std::function<VisitStatus()> status_;
    std::function<gfx::ImagePtr()> ship_;
    gfx::ImagePtr monitor_;  // DGROUP:0x953a
    gfx::ImagePtr strip_;    // DGROUP:0x953c
    int strip_x_ = 0, strip_y_ = 0;
    gfx::ImagePtr body_;       // DGROUP:0x92b0
    gfx::ImagePtr square_;     // DGROUP:0x92b2, with the flame
    gfx::ImagePtr flameless_;  // DGROUP:0x92b4
    int sz_ = 0;               // seg15:0x20ca's sz
    gfx::ImagePtr pad_;        // DGROUP:0x92a8 in DGROUP:0x92b6
    gfx::PadGeometry pad_geo_;
    gfx::ImagePtr frames_;     // DGROUP:0x3ccc in DGROUP:0x3cdc: the jet's 16 or the descent's 2
    gfx::ImagePtr frames_mask_;  // DGROUP:0x3cce in DGROUP:0x3cde
    struct Frame {
        int w = 0, h = 0, y = 0;  // DGROUP:0x3d08 + 4i, DGROUP:0x3d0a + 4i, DGROUP:0x3ce8 + 2i
    };
    std::array<Frame, 16> tab_{};
    gfx::ImagePtr work_;       // DGROUP:0x92ac in DGROUP:0x92b8 (seg15:0x3bfa)
    // seg15:0x3caa's words [BYTES seg15:0x3cc6-0x3d5a].
    struct Jet {
        int y10 = 0;    // DGROUP:0x180c
        int phase = 0;  // DGROUP:0x180e
        int idx = 0;    // DGROUP:0x180a
        int speed = 0;  // DGROUP:0x1808
        int step = 0;   // DGROUP:0x1806
        int level = 0;  // DGROUP:0x1812
        int prevy = 0;  // DGROUP:0x1810
        RECT clip{};    // DGROUP:0x3cb0 ... 0x3cb6
        bool started = false;
    } jet_;
    gfx::ImagePtr scene_img_;  // DGROUP:0x9294 in DGROUP:0x929a [OPEN: seg15:0x1328, which may free it, is unread]
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
