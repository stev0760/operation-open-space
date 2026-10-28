// The HUD on GDI (Wine in CTest): the canvas's colours, lines, views and pattern brush on the
// surface, the scrollbar control (ADR 0013 decision 5), and, with the original's files
// (IS_ORIGINAL_DIR), the repaint at 1024 x 768 compared pixel by pixel with the oracle's
// wave-0001 screenshots (work/oracle/wave-0001). The comparison reports the differing pixels per
// region; it does not require a match where the drawing is [OPEN] or the palette differs.
#ifdef _WIN32
#include <windows.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "assets/asset_store.h"
#include "doctest.h"
#include "gfx/gfx.h"
#include "help/png.h"
#include "hud/gdi_canvas.h"
#include "hud/hud.h"
#include "hud/scrollbar.h"
#include "hud_fakes.h"
#include "png_decode.h"
#include "shell/geometry.h"

using namespace is::hud;
using hudtest::FakeHost;

namespace {

std::filesystem::path original_dir() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    return v;
}

// work/oracle/wave-0001 beside IS_ORIGINAL_DIR (work/extracted/shipdisk).
std::filesystem::path oracle_dir() {
    const auto o = original_dir();
    if (o.empty()) return {};
    std::filesystem::path base = o;
    if (!base.has_filename()) base = base.parent_path();
    const auto d = base.parent_path().parent_path() / "oracle" / "wave-0001";
    return std::filesystem::is_directory(d) ? d : std::filesystem::path();
}

std::uint32_t rgb_of(const is::gfx::Gfx& g, int x, int y) {
    const auto& c = g.palettes().device()[g.surface().at(x, y)];
    return (static_cast<std::uint32_t>(c.r) << 16) | (c.g << 8) | c.b;
}

struct Region {
    const char* name;
    int x, y, w, h;
};

// Differing pixels of a region, the port's surface against the oracle's screenshot; `wide`
// counts those with a channel more than `tol` levels apart.
int diff(const is::gfx::Gfx& g, const pngtest::Image& o, const Region& r, int* wide = nullptr,
         int tol = 12) {
    int n = 0;
    if (wide) *wide = 0;
    for (int y = r.y; y < r.y + r.h; ++y)
        for (int x = r.x; x < r.x + r.w; ++x) {
            const std::uint32_t a = rgb_of(g, x, y), b = o.at(x, y);
            if (a == b) continue;
            ++n;
            for (int sh = 0; sh <= 16; sh += 8) {
                const int d = static_cast<int>((a >> sh) & 0xff) - static_cast<int>((b >> sh) & 0xff);
                if (wide && (d > tol || d < -tol)) {
                    ++*wide;
                    break;
                }
            }
        }
    return n;
}

}  // namespace

TEST_CASE("hud win32: the canvas draws on the surface with the original's calls") {
    is::gfx::Gfx g(nullptr);
    g.set_client_size(640, 480, 1);
    g.palette_create();
    g.palette_scheme(8, 2);
    g.blacken();
    GdiCanvas cv(g, [] { return static_cast<HFONT>(GetStockObject(ANSI_VAR_FONT)); });
    // a line excludes its end pixel; a plain RGB is the nearest static colour
    const Point p[2] = {{10, 10}, {20, 10}};
    cv.lines(Img::kScreen, Pen::solid(0x00FFFFFF), p, 2);
    g.surface().flush();
    CHECK(g.surface().at(10, 10) == 255);
    CHECK(g.surface().at(19, 10) == 255);
    CHECK(g.surface().at(20, 10) == 0);
    // PatBlt with a solid brush, BLACKNESS, and a stock LTGRAY brush (static silver, entry 7)
    cv.pat_blt(Img::kScreen, 0, 20, 5, 5, kPatCopy, Brush::solid(0x0000FF00));
    cv.pat_blt(Img::kScreen, 10, 20, 5, 5, kPatCopy, Brush::stock_object(kLtGrayBrush));
    g.surface().flush();
    CHECK(g.surface().at(2, 22) == g.colour_index(0x0000FF00));
    CHECK(g.surface().at(12, 22) == g.colour_index(0x00C0C0C0));
    // a view clips to its rectangle and moves the origin
    cv.set_view(100, 100, 20, 10);
    cv.pat_blt(Img::kScreen, 0, 0, 50, 50, kPatCopy, Brush::solid(0x00FFFFFF));
    cv.clear_view();
    g.surface().flush();
    CHECK(g.surface().at(100, 100) == 255);
    CHECK(g.surface().at(119, 109) == 255);
    CHECK(g.surface().at(120, 100) == 0);
    CHECK(g.surface().at(100, 110) == 0);
    CHECK(g.surface().at(99, 100) == 0);
    // after the view, the origin is back
    cv.set_pixel(Img::kScreen, 1, 1, 0x00FFFFFF);
    g.surface().flush();
    CHECK(g.surface().at(1, 1) == 255);
    // a created image is black and takes blits
    cv.create(Img::kDamage, 84, 61);
    CHECK(cv.has(Img::kDamage));
    CHECK(cv.width(Img::kDamage) == 84);
    cv.pat_blt(Img::kDamage, 0, 0, 84, 61, kWhiteness, Brush::none());
    cv.bit_blt(Img::kScreen, 200, 200, 84, 61, Img::kDamage, 0, 0, kSrcCopy, Brush::none());
    g.surface().flush();
    CHECK(g.surface().at(200, 200) == 255);
    CHECK(g.surface().at(283, 260) == 255);
    CHECK(g.surface().at(284, 260) == 0);
}

TEST_CASE("hud win32: the scrollbar control is created once, moved, and goes in a race") {
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"HudTestMain";
    RegisterClassW(&wc);
    HWND main = CreateWindowExW(0, L"HudTestMain", L"", WS_POPUP, 0, 0, 2048, 1536, nullptr,
                                nullptr, GetModuleHandleW(nullptr), nullptr);
    REQUIRE(main != nullptr);
    int factor = 1, pos = 600 - 120;
    ScrollBar sb([&] { return pos; }, [&] { return main; }, [&] { return factor; });
    is::shell::HudLayout l;
    l.scrollbar = {562, 683, 14, 85};
    sb.layout(l, false);
    HWND h = sb.hwnd();
    REQUIRE(h != nullptr);
    CHECK(GetDlgCtrlID(h) == 200);
    CHECK((GetWindowLongW(h, GWL_STYLE) & 0x50000001) == 0x50000001);
    RECT r;
    GetWindowRect(h, &r);
    MapWindowPoints(nullptr, main, reinterpret_cast<POINT*>(&r), 2);
    CHECK(r.left == 562);
    CHECK(r.top == 683);
    CHECK(r.right - r.left == 14);
    CHECK(r.bottom - r.top == 85);
    int lo = 0, hi = 0;
    GetScrollRange(h, SB_CTL, &lo, &hi);
    CHECK(lo == 100);
    CHECK(hi == 500);
    CHECK(GetScrollPos(h, SB_CTL) == 480);  // zoom 120
    // a second layout at factor 2 moves the same control and sets the position again
    factor = 2;
    pos = 400;
    sb.layout(l, false);
    CHECK(sb.hwnd() == h);
    GetWindowRect(h, &r);
    MapWindowPoints(nullptr, main, reinterpret_cast<POINT*>(&r), 2);
    CHECK(r.left == 1124);
    CHECK(r.top == 1366);
    CHECK(r.right - r.left == 28);
    CHECK(GetScrollPos(h, SB_CTL) == 400);
    sb.set_pos(300);
    CHECK(GetScrollPos(h, SB_CTL) == 300);
    // a race destroys it and creates none
    sb.layout(l, true);
    CHECK(sb.hwnd() == nullptr);
    CHECK(!IsWindow(h));
    sb.layout(l, false);
    CHECK(sb.hwnd() != nullptr);
    sb.destroy();
    CHECK(sb.hwnd() == nullptr);
    DestroyWindow(main);
}

TEST_CASE("hud win32: golden: the repaint at 1024 x 768 against the oracle's wave-0001") {
    const auto dir = original_dir();
    if (dir.empty()) {
        MESSAGE("IS_ORIGINAL_DIR not set: skipped");
        return;
    }
    is::assets::AssetStore store(dir);
    is::gfx::Gfx g(&store);
    g.set_client_size(1024, 768, 1);
    // The palette of the program's start (seg2:0x458-0x484) and the intro's last scheme; the
    // wave's own palette work is the set-up's [OPEN].
    g.palette_create();
    g.palette_scheme(8, 0);
    g.blacken();
    // DGROUP:0x90fe as WM_CREATE makes it (seg6:0x1934, src/app/wndproc.cpp).
    HFONT font = CreateFontW(14, 0, 0, 0, 0, 0, 0, 0, ANSI_CHARSET, 0, 0, PROOF_QUALITY,
                             VARIABLE_PITCH | FF_SWISS, L"Arial");
    GdiCanvas cv(g, [font] { return font; });
    std::string why;
    REQUIRE_MESSAGE(cv.load(&why), why);
    // the bitmaps of seg11:0x0
    CHECK(cv.width(Img::kTiles) == 174);
    CHECK(cv.height(Img::kTiles) == 58);
    CHECK(cv.width(Img::kDigital) == 140);
    CHECK(cv.width(Img::kSystems) == 32);
    CHECK(cv.height(Img::kSystems) == 77);
    CHECK(cv.width(Img::kLamps) == 27);
    CHECK(cv.width(Img::kButton) == 40);
    CHECK(cv.height(Img::kFuelBar) == 60);

    FakeHost h;
    h.ship_.fuel_max = 660;
    h.ship_.fuel = 660;
    Hud hud(cv, h);
    hud.loaded();
    hud.info().set_mode(kModeTarget);
    hud.repaint();
    hud.info().flush();
    g.surface().flush();

    // IS_HUD_DUMP names a PNG file to write the port's picture to, for a look [DESIGN].
    if (const char* dump = std::getenv("IS_HUD_DUMP"); dump && *dump) {
        is::fmt::Dib dib;
        dib.width = 1024;
        dib.height = 768;
        dib.bpp = 8;
        dib.bottom_up = false;
        for (const auto& c : g.palettes().device())
            dib.palette.push_back((static_cast<std::uint32_t>(c.r) << 16) | (c.g << 8) | c.b);
        dib.bits.resize(dib.stride() * 768);
        for (int y = 0; y < 768; ++y)
            for (int x = 0; x < 1024; ++x) dib.bits[dib.stride() * y + x] = g.surface().at(x, y);
        const auto png = is::help::png_from_dib(dib);
        std::ofstream(dump, std::ios::binary)
            .write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
    }
    const auto od = oracle_dir();
    if (od.empty()) {
        MESSAGE("work/oracle/wave-0001 not found: the comparison is skipped");
        return;
    }
    const pngtest::Image full = pngtest::read((od / "13-strategy-no.png").string());
    const pngtest::Image start = pngtest::read((od / "05-wave.png").string());
    REQUIRE(full.w == 1024);
    REQUIRE(full.h == 768);
    REQUIRE(start.w == 1024);
    // Regions of the drawing world.md documents. The icon column (976, 0) 50 x 150 covers the
    // right tiles' top, the IS_INFO panels, the scrollbar and the radar are other routines'.
    const Region regions[] = {
        {"top tiles", 0, 0, 976, 58},
        {"left tiles", 0, 58, 48, 625},
        {"right tiles below the icon column", 976, 170, 48, 513},
        {"bevel top", 48, 58, 928, 20},
        {"bevel left", 48, 78, 20, 585},
        {"bevel right", 956, 78, 20, 585},
        {"bevel bottom", 68, 663, 888, 20},
        {"HUD line left of the scrollbar", 0, 683, 562, 1},
        {"HUD line right of the scrollbar", 576, 683, 448, 1},
        {"SCORE box", 0, 684, 142, 84},
        {"target frame edges", 142, 684, 283, 4},
        {"fuel gauge", 428, 684, 30, 84},
        {"radar frame left", 462, 684, 6, 84},
        {"button and damage panel", 580, 684, 28, 84},
        {"damage panel lamps", 580, 707, 84, 61},
        {"clock box", 608, 684, 56, 18},
        {"weapon frame edges", 668, 684, 356, 4},
        // the IS_INFO panels: the texts the fake host gives, the icon and the weapon picture are
        // other routines' [OPEN]
        {"target display title", 147, 688, 273, 16},
        {"target display texts", 300, 704, 120, 60},
        {"weapon display title", 673, 688, 343, 16},
    };
    long total = 0, total_far = 0, total_px = 0;
    for (const Region& r : regions) {
        int wide = 0;
        const int d = diff(g, full, r, &wide);
        total += d;
        total_far += wide;
        total_px += static_cast<long>(r.w) * r.h;
        MESSAGE("13-strategy-no ", std::string(r.name), ": ", d, " of ", r.w * r.h,
                " pixels differ, ", wide, " by more than 12 levels");
    }
    MESSAGE("13-strategy-no, all regions: ", total, " of ", total_px, " pixels differ, ",
            total_far, " by more than 12 levels");
    // At the wave start (05) the oracle shows no strip, no border and no panels [OPEN]: the
    // same regions there.
    long total05 = 0;
    for (const Region& r : regions) total05 += diff(g, start, r);
    MESSAGE("05-wave, all regions: ", total05, " of ", total_px, " pixels differ");
    CHECK(total_px > 0);
    DeleteObject(font);
}
#endif
