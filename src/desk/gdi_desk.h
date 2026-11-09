// The desk scene's Canvas on the renderer of src/gfx: the screen DC DGROUP:0x13dc is the
// surface, the scene DC DGROUP:0x929a an image of the same size; every COLORREF goes through
// Gfx::dc_colour (ADR 0007). Win32 only; the host preset builds an empty translation unit.
#pragma once

#ifdef _WIN32
#include <windows.h>

#include <array>
#include <functional>
#include <map>
#include <optional>
#include <tuple>

#include "desk/desk.h"
#include "gfx/gfx.h"
#include "gfx/launch_ship.h"
#include "gfx/pad.h"

namespace is::desk {

// What seg15:0x2a08's branch A reads for INI MainShip (src/gfx/launch_ship.h LaunchShipArt): the
// MASTER.SHP record found by exact compare against SHP + 0x0a [BYTES seg15:0x2a50], its MASTER.BOD
// record and body graphic, graphic 280 and the kept game's wing bytes (M4 s6 PAD).
using ShipArt = gfx::LaunchShipArt;
// The record of a MainShip name; for a name no record matches, the zeroed scratch record (body 0,
// SHP +0x07 = +0x08 = +0x09 = 0) under ADR 0016 decision 2 (ruling R4 of M4 stage 6) [DESIGN];
// nullopt only when the body cannot be read.
using ShipSource = std::function<std::optional<ShipArt>(const std::string& main_ship)>;
// The program's MainShip lookup (src/desk/hooks.cpp ship_art), which install_desk_hooks fills, for
// the visit's launch canvas and seg15:0x4c76's new body (src/hud/hooks.cpp) [DESIGN: the slot
// stands for the far call across the port's libraries].
ShipSource& program_ships();

// The DGROUP pictures that state 6's launch reads from the visit (src/hud/visit_win32.h
// VisitGdi::launch_pictures): seg15:0x20ca's body DGROUP:0x92b0 and flame square DGROUP:0x92b2
// (or seg15:0x2a08's after a new body) and seg15:0x482e's pad DGROUP:0x92a8 (M4 s6 PAD).
struct LaunchPictures {
    gfx::ImagePtr body, square, pad;
    // DGROUP:0x953f != 0 (a wing repair bought) with DGROUP:0x9563 = 0, and seg15:0x2a08 branch B's
    // inputs at the launch (gfx::ship_2a08_b) [BYTES seg15:0x2ca3-0x2cbe] (review H2).
    bool wing_repair = false;
    std::optional<gfx::VisitShipArt> look;
};
using PictureSource = std::function<LaunchPictures()>;

class GdiDesk final : public Canvas {
public:
    using Log = std::function<void(const std::string&, const char*)>;
    GdiDesk(std::function<gfx::Gfx*()> gfx, Log todo, ShipSource ships = {})
        : gfx_(std::move(gfx)), todo_(std::move(todo)), ships_(std::move(ships)) {}
    ~GdiDesk() override;

    void palette_save(int set, int blank) override;
    void palette_scheme(int c, int mode) override;
    void palette_restore(int set) override;
    void palette_fade(int mode, int percent) override;
    void scene(const Render& r) override;
    void monitor(const Rect& s) override;
    void tower(const Rect& s) override;
    void meal(int graphic, const Rect& s) override;
    void scene_to_screen() override;
    void screen_to_scene(int x, int y, int w, int h) override;
    void scene_to_screen(int x, int y, int w, int h) override;
    void black(int x, int y, int w, int h) override;
    void block(int x, int y, int w, int h, Colour brush) override;
    void text_font(int h, int w) override;
    void text_char(int x, int y, char c) override;
    int text_extent(const std::string& cp1252) override;
    void scroll_up(const Rect& r, int pixels) override;
    void flash(int x, int y) override;
    // seg15:0x482e from the pad's size alone: the play height is found as the first that gives
    // w and h - 0x34 (the top width A needs it); the desk's step 2 calls make_pad_for [DESIGN].
    void make_pad(int w, int h) override;
    // seg15:0x482e and seg15:0x4a2e from the play height DGROUP:0x90ec and the client height
    // DGROUP:0x10 (gfx/pad.h); every grey allocated through command 6 and drawn with PALETTERGB, as
    // the scene's colours (ADR 0007) [DESIGN].
    void make_pad_for(int play_h, int client_h) override;
    int make_ship(const std::string& main_ship) override;
    // seg15:0x4c76(first, 0) in state 0: the strip built on the first call, then blitted at (x, y)
    // (s4 + 0x3e rows), through the work DC and the strip mask in desk phase 6 (the five-blit
    // composite) when the strip reaches the desk.
    void rise(int pad_w, int pad_h, int s4, int x, int y) override;
    void free_rise() override;
    void pad_to_screen(int x, int y) override;
    void launch_frame(const LaunchDraw& d) override;
    void free_all() override;
    bool pad_alive() override { return rise_work_ != nullptr; }  // DGROUP:0x92ac of the rise
    int visit_ship(const std::string& main_ship) override;
    bool pad_dc() override { return pad_now() != nullptr; }
    // State 6's pictures (hud/hooks.cpp launch_desk) [DESIGN: the DGROUP words the visit and the
    // desk machine share].
    void set_visit_pictures(PictureSource s) { visit_pictures_ = std::move(s); }

    // The scene's colours as the surface holds them (for the headless checks).
    COLORREF colour(const Rgb& c);

private:
    gfx::Gfx* g() const { return gfx_(); }
    HDC screen() const;
    HDC scene_dc() const { return scene_ ? scene_->hdc() : nullptr; }
    void frame34(HDC d, int x, int y, int w, int h, int style);  // ordinal 34 on `d`
    // seg15:0x9e6's frames from the square DGROUP:0x92b2 for size s; returns s.
    int build_frames(int s);
    gfx::ImagePtr pad_now() const;  // DGROUP:0x92b6's bitmap: the desk's, else the visit's

    std::function<gfx::Gfx*()> gfx_;
    Log todo_;
    ShipSource ships_;
    gfx::ImagePtr scene_;      // DGROUP:0x9294 in DGROUP:0x929a
    gfx::ImagePtr pad_;        // DGROUP:0x92a8 in DGROUP:0x92b6
    gfx::ImagePtr body_;       // DGROUP:0x92b0, ordinal 12's body image
    gfx::ImagePtr square_;     // DGROUP:0x92b2, the k s square with the flame
    gfx::ImagePtr ship_;       // DGROUP:0x3ccc: the 16 frames, mask DGROUP:0x3cce
    gfx::ImagePtr ship_mask_;
    std::array<ShipFrame, kShipFrames> frames_{};  // DGROUP:0x3ce8, DGROUP:0x3d08
    gfx::ImagePtr work_;       // DGROUP:0x92b8: seg15:0x3bfa(4s, 4s + 15)
    gfx::ImagePtr strip_;      // DGROUP:0x953c, padW x (s4 + 0x43)
    gfx::ImagePtr rise_work_;  // DGROUP:0x92ac in DGROUP:0x92b8: seg15:0x3bfa(padW, s4 + 0x3e)
    gfx::ImagePtr strip_mask_; // DGROUP:0x181e
    bool rise_started_ = false;
    Rect column_;              // (DGROUP:0x3d48, DGROUP:0x3d4a, + slot 0's width, DGROUP:0x92de)
    int client_h_ = 0;         // DGROUP:0x10
    int pad_play_h_ = 0;       // DGROUP:0x90ec of the pad's build
    PictureSource visit_pictures_;
    HFONT font_ = nullptr;
    int pad_x_ = 0, pad_y_ = 0;
    bool pad_shown_ = false;
    std::map<std::tuple<int, int, int>, COLORREF> colours_;
};

}  // namespace is::desk
#endif
