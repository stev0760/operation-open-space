// The intro on the renderer under Win32 (Wine in CTest): the GDI canvas of src/intro with the
// original files (IS_ORIGINAL_DIR), run through phases 0-5 on a fake clock, with the surface's
// indices checked at each phase. Skips without the original files.
#ifdef _WIN32
#include <windows.h>

#include <cstdlib>
#include <filesystem>
#include <map>
#include <set>

#include "assets/asset_store.h"
#include "doctest.h"
#include "formats/res16.h"
#include "gfx/gfx.h"
#include "intro/gdi_canvas.h"
#include "intro/intro.h"
#include "intro_fakes.h"

using namespace intro_test;

namespace {

std::filesystem::path original_dir() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    return v;
}

// The fake host with the original's strings and resources.
class StoreHost : public FakeHost {
public:
    explicit StoreHost(const is::assets::AssetStore& s) : store(s) {}
    std::string lang(int id) override {
        auto v = store.find_string(static_cast<std::uint16_t>(id));
        return v ? *v : std::string();
    }
    std::string exe_string(int id) override {
        auto v = is::fmt::load_string(store.module(is::assets::kMainProgram),
                                      static_cast<std::uint16_t>(id));
        return v ? *v : std::string();
    }
    std::vector<std::uint8_t> lang_resource(int type, int id) override {
        const auto& m = store.language_dll();
        const auto* r = m.find(static_cast<std::uint16_t>(type), static_cast<std::uint16_t>(id));
        if (!r) return {};
        return std::vector<std::uint8_t>(m.data(*r), m.data(*r) + m.size(*r));
    }
    const is::assets::AssetStore& store;
};

// The indices in a rectangle of the surface, with their counts.
std::map<int, int> histogram(is::gfx::Gfx& g, int x0, int y0, int x1, int y1) {
    auto& s = g.surface();
    s.flush();
    std::map<int, int> h;
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) ++h[s.at(x, y)];
    return h;
}
bool has(const std::map<int, int>& h, int index) { return h.count(index) != 0; }
int non_black(const std::map<int, int>& h) {
    int n = 0;
    for (const auto& [k, v] : h)
        if (k != 0) n += v;
    return n;
}

}  // namespace

TEST_CASE("intro win32: golden: phases 0-5 on the surface") {
    const auto dir = original_dir();
    if (dir.empty()) {
        MESSAGE("IS_ORIGINAL_DIR not set: skipped");
        return;
    }
    is::assets::AssetStore store(dir);
    is::gfx::Gfx g(&store);
    g.set_client_size(1024, 768, 1);
    // the start-up sequence: WM_CREATE, then the 0x9cf handler
    g.palette_create();
    g.palette_scheme(-1000, 2);
    g.palette_delete();
    g.palette_create();
    g.palette_scheme(8, 2);
    g.blacken();
    std::vector<std::string> errors;
    is::intro::GdiCanvas canvas(g, [&](const std::string& s) { errors.push_back(s); });
    StoreHost host(store);
    is::shell::Rng rng;
    is::intro::Intro in(canvas, host, rng);
    in.enter(host.t);
    CHECK(errors.empty());
    const auto& pal = g.palettes().device();
    const int yellow = g.colour_index(0x0000FFFF);          // the static 251
    const int main_colour = g.colour_index(0x02FF60A0);     // PALETTERGB(0xA0, 0x60, 0xFF)
    CHECK(yellow == 251);

    // phase 0, step 0: a cell of the strip at (W/2 - 88, H/2 - 60)
    auto h = histogram(g, 424, 324, 464, 364);
    CHECK(non_black(h) > 200);
    run_until(in, host, [&] { return in.step() == 4; });
    in.frame(host.t += 13, false);
    // the yellow logo letters and "presents.. " below them
    h = histogram(g, 474, 328, 604, 360);
    CHECK(h[yellow] > 300);
    h = histogram(g, 400, 401, 624, 445);
    CHECK(h[yellow] > 100);

    // phase 2: the title after step 0
    run_until(in, host, [&] { return in.phase() == 2 && in.step() == 1; });
    h = histogram(g, 374, 229, 650, 439);
    for (const auto& [k, n] : h) {
        CAPTURE(k);
        CHECK((k == 0 || (k >= 188 && k <= 229)));
    }
    CHECK(h.size() >= 12);
    // the ramps of ordinal 12 in entries 185-229: entry 188 is graphic 300's colour 1
    CHECK(pal[185].r == 0xff);
    CHECK(pal[185].g == 0xe6);
    CHECK(pal[185].b == 0x6e);
    // the embossed texts in the main colour, the white edge and the shadow
    h = histogram(g, 300, 130, 724, 225);
    CHECK(h[main_colour] > 500);
    CHECK(has(h, 255));
    CHECK(has(h, g.colour_index(0x02C00060)));
    // the Press 'R' box: opaque PALETTERGB(0xB4, 0x80, 0xE0)
    h = histogram(g, 340, 536, 680, 552);
    CHECK(h[g.colour_index(0x02E080B4)] > 2000);

    // phase 3: the heading of entry 0 typed in full
    run_until(in, host, [&] { return in.phase() == 3 && in.credit_line() == 1; });
    h = histogram(g, 300, 234, 724, 265);
    CHECK(h[g.colour_index(0x02FF60A0)] > 200);
    h = histogram(g, 470, 320, 560, 400);  // the grown logo
    CHECK(non_black(h) > 500);
    // the erase clears both lines
    run_until(in, host, [&] { return in.credit_entry() == 1; });
    h = histogram(g, 0, 234, 1024, 274);
    CHECK(non_black(h) == 0);

    // phase 4: the page, its bitmap-font heading and footer
    run_until(in, host, [&] { return in.phase() == 4; });
    h = histogram(g, 200, 209, 824, 229);
    CHECK(non_black(h) > 300);
    h = histogram(g, 200, 269, 824, 530);
    CHECK(h[g.colour_index(0x02FF8080)] > 1000);
    h = histogram(g, 200, 557, 824, 577);
    CHECK(h[255] > 300);

    // phase 5: the heading in red, the rows below
    run_until(in, host, [&] { return in.phase() == 5; });
    h = histogram(g, 300, 184, 724, 204);
    CHECK(h[g.colour_index(0x000000FF)] > 300);
    h = histogram(g, 200, 234, 824, 560);
    CHECK(non_black(h) > 2000);

    // then the demo start, which the port replaces by a new state-0 entry
    run_until(in, host, [&] { return in.phase() == 7; });
    CHECK(host.demo_starts == 1);
    CHECK(errors.empty());
}
#endif
