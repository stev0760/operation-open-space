// The briefing on the renderer under Win32 (Wine in CTest): the GDI canvas of src/briefing with
// the original files (IS_ORIGINAL_DIR), state 1 -> 3 -> 1 through shell::GameState on a fake
// clock, with the surface's indices checked against newgame-0002's briefing (shell.md "Screen
// states", State 3). Skips without the original files.
#ifdef _WIN32
#include <windows.h>

#include <cstdlib>
#include <filesystem>
#include <map>

#include "assets/asset_store.h"
#include "briefing/briefing.h"
#include "briefing/gdi_canvas.h"
#include "doctest.h"
#include "formats/ne.h"
#include "gfx/gfx.h"
#include "shell/game_state.h"

namespace {

std::filesystem::path original_dir() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    return v;
}

class StoreHost : public is::briefing::Host {
public:
    explicit StoreHost(const is::assets::AssetStore& s) : store(s) {}
    std::string lang(int id) override {
        auto v = store.find_string(static_cast<std::uint16_t>(id));
        return v ? *v : std::string();
    }
    void sound(int) override {}
    void todo(const std::string&, const char*) override {}
    std::vector<std::uint8_t> page(int id) override {
        const auto& m = store.language_dll();
        const auto* r = m.find(5000, static_cast<std::uint16_t>(id));
        if (!r) return {};
        return std::vector<std::uint8_t>(m.data(*r), m.data(*r) + m.size(*r));
    }
    const is::assets::AssetStore& store;
};

std::map<int, int> histogram(is::gfx::Gfx& g, int x0, int y0, int x1, int y1) {
    auto& s = g.surface();
    s.flush();
    std::map<int, int> h;
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) ++h[s.at(x, y)];
    return h;
}

}  // namespace

TEST_CASE("briefing win32: golden: the wave 1 briefing, ESC and the hand-off") {
    const auto dir = original_dir();
    if (dir.empty()) {
        MESSAGE("IS_ORIGINAL_DIR not set: skipped");
        return;
    }
    is::assets::AssetStore store(dir);
    is::gfx::Gfx g(&store);
    g.set_client_size(1024, 768, 1);
    g.palette_create();
    g.palette_scheme(8, 2);
    g.blacken();
    is::briefing::GdiCanvas canvas(g);
    StoreHost host(store);
    is::briefing::Briefing b(canvas, host);
    is::briefing::Setup setup;
    setup.W = 1024;
    setup.H = 768;
    setup.directory = "D:\\GAMES\\";
    setup.icons = 3;
    setup.gone = {false, false, false};
    // The icon cells: three 32x32 cells 38 px apart, filled grey (the wave set-up's are M3).
    auto cells = g.create_image(38 * 3 + 6, 44);
    PatBlt(cells->hdc(), 0, 0, 38 * 3 + 6, 44, WHITENESS);
    canvas.set_icon_cells(cells);

    is::shell::GameState s;
    int id = -1;
    bool wave = false;
    s.hooks().enter[is::shell::kBriefing] = [&](is::shell::GameState& st) {
        id = b.enter(st, setup);
    };
    s.hooks().frame[is::shell::kBriefing] = [&](is::shell::GameState& st) { b.frame(st); };
    s.hooks().enter[is::shell::kWave] = [&](is::shell::GameState&) { wave = s.state() == 1; };
    is::shell::Tick t = 473781;  // newgame-0002's first briefing draw
    s.request(is::shell::kWave);
    s.change_step(t);
    REQUIRE(s.state() == is::shell::kBriefing);
    CHECK(id == 101);
    CHECK(b.wave() == 1);

    const int white = g.colour_index(0x00FFFFFF);
    const int light = g.colour_index(0x02C0C0C0);
    const int shade = g.colour_index(0x02808080);
    const int face = g.colour_index(0x025A5A5A);
    const int highlight = g.colour_index(0x02FFFFFF);
    const int yellow = g.colour_index(0x0000FFFF);
    const int cyan = g.colour_index(0x00FFFF00);
    auto& surf = g.surface();
    surf.flush();
    // The frame's top line in WHITE_PEN, the bottom line in the shade, the bars filled light.
    CHECK(surf.at(400, 139) == white);
    CHECK(surf.at(400, 614) == shade);
    CHECK(surf.at(600, 160) == light);
    CHECK(surf.at(600, 470) == light);
    CHECK(surf.at(512, 300) == 0);  // the page is black
    // The title label: face and highlight pixels inside the title bar, left half.
    auto h = histogram(g, 207, 141, 440, 164);
    CHECK(h[face] > 100);
    CHECK(h[highlight] > 50);
    // "3 ICONS" in the status bar's right end, "D:\GAMES\" at its left.
    CHECK(histogram(g, 740, 455, 818, 474)[face] > 30);
    CHECK(histogram(g, 207, 455, 290, 474)[face] > 30);

    // The text types in three lines, as the oracle's enter-02 (25 px apart from y 489).
    for (int i = 0; i < 1000 && !b.typewriter().done(); ++i) {
        t += 16;
        if (s.pass(t)) s.frame();
    }
    REQUIRE(b.typewriter().done());
    CHECK(histogram(g, 220, 489, 800, 507)[yellow] > 200);
    CHECK(histogram(g, 220, 514, 800, 532)[yellow] > 200);
    CHECK(histogram(g, 220, 539, 800, 557)[yellow] > 100);
    CHECK(histogram(g, 220, 564, 800, 582)[yellow] == 0);
    // The first line ends with ICONS (before x 790), as in the oracle.
    CHECK(histogram(g, 790, 489, 816, 507)[yellow] == 0);

    // Phase 1: icons grow at y 324-388, then the cyan page from (247, 218).
    for (int i = 0; i < 2000 && s.briefing_skip() != 2; ++i) {
        t += 16;
        if (s.pass(t)) s.frame();
    }
    CHECK(s.briefing_phase() == 1);
    CHECK(histogram(g, 392, 324, 456, 388)[white] > 1000);
    CHECK(histogram(g, 240, 210, 700, 300)[cyan] > 100);

    // ESC: phase 18, then state 1 at the next pass.
    s.post_input({is::shell::kWmKeyUp, is::shell::kVkEscape, 0});
    t += 16;
    if (s.pass(t)) s.frame();
    CHECK(s.briefing_phase() == 18);
    t += 16;
    s.pass(t);
    CHECK(wave);
    CHECK(s.state() == is::shell::kWave);
}

TEST_CASE("briefing win32: golden: the box at 640x480") {
    const auto dir = original_dir();
    if (dir.empty()) {
        MESSAGE("IS_ORIGINAL_DIR not set: skipped");
        return;
    }
    is::assets::AssetStore store(dir);
    is::gfx::Gfx g(&store);
    g.set_client_size(640, 480, 1);
    g.palette_create();
    g.palette_scheme(8, 2);
    g.blacken();
    is::briefing::GdiCanvas canvas(g);
    StoreHost host(store);
    is::briefing::Briefing b(canvas, host);
    is::briefing::Setup setup;
    setup.W = 640;
    setup.H = 480;
    setup.directory = "C:\\";
    is::shell::GameState s;
    s.hooks().enter[is::shell::kBriefing] = [&](is::shell::GameState& st) { b.enter(st, setup); };
    s.request(is::shell::kWave);
    s.change_step(1000);
    auto& surf = g.surface();
    surf.flush();
    const int white = g.colour_index(0x00FFFFFF);
    // The frame from (10, 0) to (630, 470): the top line at y 0, the left line at x 10.
    CHECK(surf.at(300, 0) == white);
    CHECK(surf.at(10, 200) == white);
    CHECK(surf.at(5, 200) == 0);
    CHECK(b.box().pt == 26);
}
#endif
