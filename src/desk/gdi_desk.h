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

namespace is::desk {

// What seg15:0x2a08 renders for a MainShip: the body graphic (IGRAPHIC.DLL #4000 BOD + 0x21) and
// the scheme rows SHP + 0x07 and SHP + 0x08 of its MASTER.SHP record.
struct ShipArt {
    fmt::Dib body;
    int id = 0;
    int a = 0, b = 0;
};
// The record of a MainShip name, found by exact compare against SHP + 0x0a [BYTES seg15:0x2a50];
// nullopt when no record matches or the body cannot be read.
using ShipSource = std::function<std::optional<ShipArt>(const std::string& main_ship)>;

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
    void make_pad(int w, int h) override;
    int make_ship(const std::string& main_ship) override;
    void rise(int pad_w, int pad_h, int s4, int x, int y) override;
    void free_rise() override;
    void pad_to_screen(int x, int y) override;
    void launch_frame(const LaunchDraw& d) override;
    void free_all() override;
    bool pad_alive() override { return strip_ != nullptr; }

    // The scene's colours as the surface holds them (for the headless checks).
    COLORREF colour(const Rgb& c);

private:
    gfx::Gfx* g() const { return gfx_(); }
    HDC screen() const;
    HDC scene_dc() const { return scene_ ? scene_->hdc() : nullptr; }
    void frame34(HDC d, int x, int y, int w, int h, int style);  // ordinal 34 on `d`
    void draw_ship(HDC d, int x, int y, int size);

    std::function<gfx::Gfx*()> gfx_;
    Log todo_;
    ShipSource ships_;
    gfx::ImagePtr scene_;      // DGROUP:0x9294 in DGROUP:0x929a
    gfx::ImagePtr pad_;        // DGROUP:0x92a8 in DGROUP:0x92b6
    gfx::ImagePtr ship_;       // DGROUP:0x3ccc: the 16 frames, mask DGROUP:0x3cce
    gfx::ImagePtr ship_mask_;
    std::array<ShipFrame, kShipFrames> frames_{};  // DGROUP:0x3ce8, DGROUP:0x3d08
    gfx::ImagePtr work_;       // DGROUP:0x92b8: seg15:0x3bfa(4s, 4s + 15)
    gfx::ImagePtr strip_;      // DGROUP:0x953c / DGROUP:0x92ac
    HFONT font_ = nullptr;
    int pad_x_ = 0, pad_y_ = 0;
    bool pad_shown_ = false;
    std::map<std::tuple<int, int, int>, COLORREF> colours_;
};

}  // namespace is::desk
#endif
