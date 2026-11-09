// owner: M4 stage 6, track PAD.
// The launch pictures against the oracle (Win32, Wine in CTest): the desk's pad ship of
// seg15:0x2a08 (src/desk/gdi_desk.cpp on src/gfx/launch_ship.h), its rise strip of seg15:0x4c76 and
// its launch frame 0 of seg15:0x0, and the visit's ship of seg15:0x20ca with seg15:0x1eac's two
// frames, phase 2's landed ship and state 6's launch frame 0 (work/analysis/m4/s6-pad.md P1-P6).
// The goldens read IS_ORIGINAL_DIR (the game) and work/oracle beside it and skip without them:
// start-unreg/main/063-desk-t064.05.png (state 0, frame 0 at (446, 574)) [ORACLE start-0001],
// core-0003 picture/landing-01-scene.png (frame 1 at (446, 576)) and amb/visit-c0029.png (state 6,
// frame 0 at (446, 574)) [ORACLE core-0003], newgame-0002-intro/intro-03.png (the rise at offset 74)
// [ORACLE newgame-0002], core-0008 picture/desk-tf-full-ship-on-pad.png (the rise's end, offset 5)
// [ORACLE core-0008]. The 24-bpp oracle draws no shadow (ruling R2) and has the x = 1 flame the port
// does not read [OPEN: ORD18-x1], so each comparison counts the pixels that differ where the port
// is black (the checkerboard) and inside the flame box (x 498-525, y 670-701, s6-pad-verify.md claim
// 68) apart from the rest.
#ifdef _WIN32
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <filesystem>
#include <optional>
#include <string>

#include "assets/asset_store.h"
#include "desk/gdi_desk.h"
#include "doctest.h"
#include "gfx/gfx.h"
#include "gfx/launch_ship.h"
#include "gfx/pad.h"
#include "gfx/ship_flame.h"
#include "hud/visit_win32.h"
#include "png_decode.h"
#include "world/visit.h"

namespace {

std::filesystem::path original() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    return v;
}
std::filesystem::path oracle_png(const char* rel) {
    std::filesystem::path base = original();
    if (base.empty()) return {};
    if (!base.has_filename()) base = base.parent_path();
    const auto p = base.parent_path().parent_path() / "oracle" / rel;
    return std::filesystem::exists(p) ? p : std::filesystem::path();
}

// What src/desk/hooks.cpp ship_art hands the desk for a MainShip (with M4 s6's fields).
std::optional<is::gfx::LaunchShipArt> art_of(const is::assets::AssetStore& store, const std::string& name) {
    const is::assets::Ships ships = store.ships(original());
    const is::fmt::ShipRecord* r = ships.find(name);
    if (!r) return std::nullopt;
    const is::fmt::BodyRecord* b = ships.body(*r);
    if (!b) return std::nullopt;
    is::gfx::LaunchShipArt a;
    a.body = store.sprite(b->bitmap_id);
    a.id = b->bitmap_id;
    a.a = r->byte_07;
    a.b = r->byte_08;
    a.fy = b->byte_19;
    a.band = b->byte_1a;
    a.body_flags = b->flags;
    a.ship_flags = r->flags;
    a.flame = store.sprite(is::gfx::kFlameGraphic);
    return a;
}

struct Diff {
    int total = 0, compared = 0, shadow = 0, flame = 0, grey = 0, grey_max = 0, other = 0;
    std::string first;
};
// The surface's region (x, y, w, h) through the device palette against the oracle's pixels.
Diff compare(is::gfx::Gfx& g, const pngtest::Image& o, int x0, int y0, int w, int h, bool flame_box) {
    Diff d;
    g.surface().flush();
    const is::gfx::DeviceColours& dev = g.palettes().device();
    // IS_PAD_DUMP=<file prefix>: the port's region as a PPM, for a look [DESIGN: diagnostics].
    if (const char* dump = std::getenv("IS_PAD_DUMP"); dump && *dump) {
        static int n = 0;
        if (FILE* f = std::fopen((std::string(dump) + std::to_string(n++) + ".ppm").c_str(), "wb")) {
            std::fprintf(f, "P6 %d %d 255\n", w, h);
            for (int y = y0; y < y0 + h; ++y)
                for (int x = x0; x < x0 + w; ++x) {
                    const is::gfx::Rgb c = dev[g.surface().at(x, y)];
                    std::fputc(c.r, f), std::fputc(c.g, f), std::fputc(c.b, f);
                }
            std::fclose(f);
        }
    }
    for (int y = y0; y < y0 + h; ++y)
        for (int x = x0; x < x0 + w; ++x) {
            ++d.compared;
            const is::gfx::Rgb c = dev[g.surface().at(x, y)];
            const std::uint32_t pc = (std::uint32_t(c.r) << 16) | (std::uint32_t(c.g) << 8) | c.b;
            const std::uint32_t oc = o.at(x, y);
            if (pc == oc) continue;
            ++d.total;
            if (flame_box && x >= 498 && x <= 525 && y >= 670 && y <= 701)
                ++d.flame;
            else if (pc == 0)
                ++d.shadow;
            else if (((oc >> 16) & 0xff) == (oc & 0xff) && ((oc >> 8) & 0xff) == (oc & 0xff) && c.r == c.g && c.g == c.b) {
                // A pad grey on the 8-bit palette: the visit's PALETTERGB greys land on the entries
                // of seg15:0x3590's 47 greys of the second pass, read back through set 1 (ADR 0007
                // decision 1); the 24-bpp oracle shows the exact greys.
                ++d.grey;
                d.grey_max = std::max(d.grey_max, std::abs(int(c.r) - int(oc & 0xff)));
            }
            else {
                if (d.other < 4)
                    d.first += "(" + std::to_string(x) + ", " + std::to_string(y) + ") port " + std::to_string(pc) +
                               " oracle " + std::to_string(oc) + "; ";
                ++d.other;
            }
        }
    return d;
}

struct Desk {
    is::gfx::Gfx g;
    is::desk::GdiDesk desk;
    explicit Desk(const is::assets::AssetStore& store)
        : g(&store), desk([this] { return &g; }, [](const std::string&, const char*) {},
                          [&store](const std::string& n) { return art_of(store, n); }) {
        g.set_client_size(1024, 768, 1);
        g.palette_create();
        g.palette_save(0, 1);         // Desk::enter's seg10:0x6da6(0, 1)
        g.palette_scheme(0x6d, 2);    // and seg10:0x6e76(0x6d, 2)
        g.blacken();
    }
};

}  // namespace

TEST_CASE("launch pictures win32 golden: the desk's TRANSFORMER, the rise strip and launch frame 0") {
    if (original().empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    is::assets::AssetStore store(original());
    Desk d(store);
    REQUIRE(d.g.logical_width() == 1024);
    REQUIRE(d.g.logical_height() == 768);
    d.desk.make_pad_for(683, 768);
    const int s = d.desk.make_ship("TRANSFORMER");
    CHECK(s == 33);
    const int pw = 159, pc = 74, s4 = 4 * s;
    // The rise at offset 74 (intro-03) and at its end, offset 5 (core-0008's ship on the pad).
    for (const auto& [rel, off] : {std::pair<const char*, int>{"newgame-0002-intro/intro-03.png", 74},
                                   std::pair<const char*, int>{"core-0008/picture/desk-tf-full-ship-on-pad.png", 5}}) {
        const auto png = oracle_png(rel);
        if (png.empty()) {
            MESSAGE("skipped: work/oracle/" << rel << " not found");
            continue;
        }
        const pngtest::Image o = pngtest::read(png.string());
        d.g.blacken();
        d.desk.free_rise();
        const int y = 768 - s4 - 0x43 + off;
        d.desk.rise(pw, pc + 0x34, s4, 433, y);
        const Diff r = compare(d.g, o, 433, y, pw, std::min(s4 + 0x3e, 768 - y), false);
        MESSAGE(std::string(rel) << " (offset " << off << "): " << r.total << " of " << r.compared << " differ: " << r.shadow
                    << " where the port is black (the shadow), " << r.grey << " pad greys (at most "
                    << r.grey_max << " apart), " << r.other << " other" << (r.other ? " from " + r.first : ""));
        CHECK(r.other == 0);
    }
    // seg15:0x0(2)'s first drawing: frame 0 with the flame at (446, 574), the pad part, the shadow.
    const auto png = oracle_png("start-unreg/main/063-desk-t064.05.png");
    if (png.empty()) {
        MESSAGE("skipped: work/oracle/start-unreg/main/063-desk-t064.05.png not found");
        return;
    }
    const pngtest::Image o = pngtest::read(png.string());
    d.g.blacken();
    d.desk.pad_to_screen(433, 642);
    is::desk::LaunchDraw ld;
    ld.f = 0;
    ld.s = s;
    ld.ship = {446, 574, 446 + s4, 574 + s4 + 15};
    ld.scene = false;
    ld.pad = {433, 642, 433 + pw, 642 + pc + 0x34};
    ld.state = 0;
    ld.y0 = 574;
    ld.ye = 60;
    ld.pos10 = 5740;
    ld.prev_top = 574;
    d.desk.launch_frame(ld);
    const Diff r = compare(d.g, o, 446, 574, s4, s4 + 15, true);
    MESSAGE("063-desk-t064.05 frame 0: " << r.total << " of " << r.compared << " differ: " << r.flame << " in the flame box, "
                                         << r.shadow << " where the port is black, " << r.grey << " greys, " << r.other << " other"
                                         << (r.other ? " from " + r.first : ""));
    CHECK(r.other == 0);
}

TEST_CASE("launch pictures win32 golden: the desk's SKY TALON at the rise's end (core-0008)") {
    const auto png = oracle_png("core-0008/picture/desk-sky-full-ship-on-pad.png");
    if (original().empty() || png.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR or work/oracle/core-0008/picture/desk-sky-full-ship-on-pad.png not available");
        return;
    }
    is::assets::AssetStore store(original());
    Desk d(store);
    d.desk.make_pad_for(683, 768);
    const int s = d.desk.make_ship("SKY TALON");
    REQUIRE(s > 0);
    const int s4 = 4 * s, y = 768 - s4 - 0x43 + 5;
    d.desk.rise(159, 74 + 0x34, s4, 433, y);
    const pngtest::Image o = pngtest::read(png.string());
    const Diff r = compare(d.g, o, 433, y, 159, std::min(s4 + 0x3e, 768 - y), false);
    MESSAGE("desk-sky-full-ship-on-pad (s " << s << ", offset 5): " << r.total << " of " << r.compared << " differ: "
                                            << r.shadow << " where the port is black, " << r.grey << " pad greys (at most "
                                            << r.grey_max << " apart), " << r.other << " other"
                                            << (r.other ? " from " + r.first : ""));
    CHECK(r.other == 0);
}

TEST_CASE("launch pictures win32 golden: the visit's TRANSFORMER landed (frame 1) and its state-6 launch frame 0") {
    if (original().empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    is::assets::AssetStore store(original());
    is::gfx::Gfx g(&store);
    g.set_client_size(1024, 768, 1);
    g.palette_create();
    g.blacken();
    is::hud::VisitGdi v([&g] { return &g; }, [] { return static_cast<is::hud::GdiCanvas*>(nullptr); },
                        [] { return is::hud::VisitStatus{}; }, [] { return is::gfx::ImagePtr{}; });
    const std::optional<is::gfx::LaunchShipArt> a = art_of(store, "TRANSFORMER");
    REQUIRE(a);
    v.ship_look = [&a] {
        is::gfx::VisitShipArt s;
        s.body = a->body;
        s.id = a->id;
        s.a = a->a;
        s.b = a->b;
        s.flame_row = (a->ship_flags & 1) == 0 ? 1 : 0;
        s.behind = (a->body_flags & 8) != 0 ? 1 : 0;
        s.ability = (a->body_flags & 4) != 0 ? 3 : -1;
        s.r7a = 33;  // sz = 33: s4 = 132 [ORACLE core-0003]
        s.r7b = 6;
        s.fy = a->fy;
        s.band = a->band;
        s.detail = 3;
        s.flame = a->flame;
        return s;
    };
    v.play_height = [] { return 683; };
    const is::world::VisitLayout l = is::world::visit_layout(1024, 768);
    v.landing_scene(l, "", true, false);  // no scene hook: the black screen and seg15:0x1a42
    v.descent_start(l, true, false);      // no ordinal-14 steps without the jet's [DESIGN: the test]
    const is::desk::LaunchPictures pics = v.launch_pictures();
    REQUIRE(pics.body);
    REQUIRE(pics.square);
    REQUIRE(pics.pad);
    CHECK(pics.body->width() == 66);
    CHECK(pics.square->width() == 66);
    CHECK(pics.pad->width() == 159);
    CHECK(pics.pad->height() == 126);
    // Phase 2: frame 1 at (446, 576), then the visit's launch frame 0 through the desk machine's canvas.
    g.blacken();
    BitBlt(g.dc(), 433, 642, 159, 126, pics.pad->hdc(), 0, 0, SRCCOPY);
    v.ship_on_pad(l, 446, 576, 132, 132);
    if (const auto png = oracle_png("core-0003/picture/landing-01-scene.png"); !png.empty()) {
        const pngtest::Image o = pngtest::read(png.string());
        const Diff r = compare(g, o, 446, 576, 132, 132, false);
        MESSAGE("landing-01-scene frame 1: " << r.total << " of " << r.compared << " differ: " << r.grey
                                             << " pad greys (at most " << r.grey_max << " apart), " << r.other << " other"
                                             << (r.other ? " from " + r.first : ""));
        CHECK(r.other == 0);
        CHECK(r.shadow == 0);
    } else {
        MESSAGE("skipped: work/oracle/core-0003/picture/landing-01-scene.png not found");
    }
    is::desk::GdiDesk desk([&g] { return &g; }, [](const std::string&, const char*) {});
    desk.set_visit_pictures([&v] { return v.launch_pictures(); });
    CHECK(desk.pad_dc());
    const int s = desk.visit_ship("TRANSFORMER");
    CHECK(s == 33);
    g.blacken();
    BitBlt(g.dc(), 433, 642, 159, 126, pics.pad->hdc(), 0, 0, SRCCOPY);
    is::desk::LaunchDraw ld;
    ld.f = 0;
    ld.s = s;
    ld.ship = {446, 574, 446 + 132, 574 + 147};
    ld.pad = {433, 642, 433 + 159, 642 + 126};
    ld.state = 6;
    ld.y0 = 574;
    ld.ye = -10;
    ld.pos10 = 5740;
    ld.prev_top = 574;
    desk.launch_frame(ld);
    if (const auto png = oracle_png("core-0003/amb/visit-c0029.png"); !png.empty()) {
        const pngtest::Image o = pngtest::read(png.string());
        const Diff r = compare(g, o, 446, 574, 132, 147, true);
        MESSAGE("visit-c0029 frame 0 (state 6): " << r.total << " of " << r.compared << " differ: " << r.flame
                                                  << " in the flame box, " << r.shadow << " where the port is black, "
                                                  << r.grey << " pad greys (at most " << r.grey_max << " apart), "
                                                  << r.other << " other" << (r.other ? " from " + r.first : ""));
        CHECK(r.other == 0);
    } else {
        MESSAGE("skipped: work/oracle/core-0003/amb/visit-c0029.png not found");
    }
}
// Review H2 and M6: seg15:0x20ca's wing cut (ability 3, ship byte +0xac = 0) blacks the square's
// columns 0 ... k w8 - 1 and keeps them in the body DGROUP:0x92b0; with a wing repair bought,
// seg15:0x2a08 branch B's common part (gfx::ship_2a08_b) copies the uncut body over the square,
// so the launch has its wing back [BYTES seg15:0x2310-0x237a seg15:0x2ca3-0x2d81].
TEST_CASE("launch pictures win32 golden: the cut wing and the wing repair's rebuilt square (TRANSFORMER)") {
    if (original().empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    is::assets::AssetStore store(original());
    is::gfx::Gfx g(&store);
    g.set_client_size(1024, 768, 1);
    g.palette_create();
    g.blacken();
    is::hud::VisitGdi v([&g] { return &g; }, [] { return static_cast<is::hud::GdiCanvas*>(nullptr); },
                        [] { return is::hud::VisitStatus{}; }, [] { return is::gfx::ImagePtr{}; });
    const std::optional<is::gfx::LaunchShipArt> a = art_of(store, "TRANSFORMER");
    REQUIRE(a);
    is::gfx::VisitShipArt look;
    look.body = a->body;
    look.id = a->id;
    look.a = a->a;
    look.b = a->b;
    look.flame_row = (a->ship_flags & 1) == 0 ? 1 : 0;
    look.behind = 0;
    look.ability = 3;
    look.left_wing = false;  // ship byte +0xac = 0
    look.r7a = 33;
    look.r7b = 5;
    look.fy = a->fy;
    look.band = a->band;
    look.detail = 3;
    look.flame = a->flame;
    v.ship_look = [&look] { return look; };
    v.play_height = [] { return 683; };
    const is::world::VisitLayout l = is::world::visit_layout(1024, 768);
    v.landing_scene(l, "", true, false);
    const is::desk::LaunchPictures cut = v.launch_pictures();
    REQUIRE(cut.body);
    REQUIRE(cut.square);
    CHECK_FALSE(cut.wing_repair);
    const int k = is::gfx::kLaunchK, sz = 33;
    const int w8 = (sz / 2) - static_cast<std::int8_t>(static_cast<std::int8_t>(look.band) / 2);
    const int fx = k * ((sz - 9) / 2);  // the flame's left edge in the square
    const int cols = std::min(k * w8, fx);
    REQUIRE(cols > 0);
    cut.body->flush();
    cut.square->flush();
    int body_px = 0, square_px = 0;
    for (int y = 0; y < k * sz; ++y)
        for (int x = 0; x < cols; ++x) {
            body_px += cut.body->at(x, y) != 0 ? 1 : 0;
            square_px += cut.square->at(x, y) != 0 ? 1 : 0;
        }
    MESSAGE("columns 0-" << cols - 1 << ": body " << body_px << " px, cut square " << square_px << " px");
    CHECK(body_px > 0);
    CHECK(square_px == 0);
    // The launch with the wing repair (DGROUP:0x953f != 0): the square takes the body's columns.
    look.left_wing = true;  // phase 25 has set the ship and roster bytes to 1
    v.wing_repair = [] { return true; };
    is::desk::GdiDesk desk([&g] { return &g; }, [](const std::string&, const char*) {});
    desk.set_visit_pictures([&v] { return v.launch_pictures(); });
    CHECK(desk.visit_ship("TRANSFORMER") == 33);
    cut.square->flush();
    int same = 0, differ = 0;
    for (int y = 0; y < k * sz; ++y)
        for (int x = 0; x < cols; ++x) (cut.square->at(x, y) == cut.body->at(x, y) ? same : differ) += 1;
    MESSAGE("after the wing repair: " << same << " equal, " << differ << " differ");
    CHECK(differ == 0);
}
#endif
