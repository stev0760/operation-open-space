// owner: M4 stage 8, track VISIT.
// The visit's leftovers of M4 stage 8 (work/analysis/m4/s8-triage.md items 6 and 11):
//  - S3-9 / A-3: seg16:0x3468(i, 3)'s seg14:0x11a8 and the slot word DGROUP:0x3a38 + 2i through
//    world::WorldPurchaseHost's defaults, the program's sheet side (world/player_sheet.h);
//  - S6-5: the three visit pictures stage 6 left without a test, on hand-made graphics (no game
//    files): seg15:0x20ca's thrust-behind path (gfx::ship_20ca), seg15:0x454c's walls between the
//    panels (hud::VisitGdi::redraw) and the incoming jet seg15:0x3caa (hud::VisitGdi::incoming)
//    against work/analysis/m4/s6-scene.md section 2's vector (scene 105 at 1024 x 768: DGROUP:0x92d6
//    = 161, DGROUP:0x92d8 = 758, DGROUP:0x92de = 629), whose schedule the reading gives as 134
//    calls, 74 receding with frames 0 ... 14, then 60 with frame 15 (s6-scene-verify.md claims
//    61-68). The formulas are restated here from the readings (shell.md "State 6" for
//    seg15:0x454c, s6-pad.md 2.2 for seg15:0x20ca, s6-scene.md section 3 for the jet), not taken
//    from the port's code.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "doctest.h"
#include "world/player_sheet.h"
#include "world/purchases.h"
#include "world/records.h"
#include "world/world.h"

#ifdef _WIN32
#include <windows.h>

#include "formats/dib.h"
#include "gfx/gfx.h"
#include "gfx/launch_ship.h"
#include "hud/visit_win32.h"
#include "world/visit.h"
#endif

using namespace is::world;

// seg16:0x3468(i, 3) [BYTES seg16:0x3477-0x34a9 seg16:0x362b seg16:0x3631-0x3645]: an unbound
// host reaches the program's seg14:0x11a8 (build_sheet_program, which logs once and builds nothing
// without the drawing side) and the slot word DGROUP:0x3a38 + 2i of sprite_links().images; a slot
// still overrides.
TEST_CASE("visit s8: WorldPurchaseHost's seg14:0x11a8 and DGROUP:0x3a38 + 2i default to the program's sheet side (S3-9, A-3)") {
    World w;
    WorldPurchaseHost h(w);
    std::vector<std::string> host_todos, sheet_todos;
    h.todo_log = [&host_todos](const std::string& what, const char*) { host_todos.push_back(what); };
    const SpriteLinks saved = sprite_links();
    sprite_links() = SpriteLinks{};
    sprite_links().todo = [&sheet_todos](const std::string& what, const char* address) {
        sheet_todos.push_back(std::string(address) + ": " + what);
    };
    h.build_sheet(0);
    CHECK(host_todos.empty());
    REQUIRE(sheet_todos.size() == 1);
    CHECK(sheet_todos[0].rfind("seg14:0x11a8: ", 0) == 0);
    CHECK_FALSE(h.sheet_dc(0));
    CHECK(h.sheet_dc_word(0) == 0);
    ImageSlotTable images{};
    images[kShipImageSlotBase + 1] = 0x50;
    sprite_links().images = [&images]() -> ImageSlotTable* { return &images; };
    CHECK_FALSE(h.sheet_dc(0));
    CHECK(h.sheet_dc(1));
    CHECK(h.sheet_dc_word(1) == 0x50);
    int built = -1;
    h.on_build_sheet = [&built](int ship) { built = ship; };
    h.build_sheet(1);
    CHECK(built == 1);
    CHECK(sheet_todos.size() == 1);
    h.on_sheet_dc = [](int) { return false; };
    CHECK_FALSE(h.sheet_dc(1));
    sprite_links() = saved;
}

#ifdef _WIN32
namespace {

using is::gfx::Gfx;
using is::gfx::ImagePtr;

constexpr int kK = is::gfx::kLaunchK;  // 2 on the port's 8-bpp device

// cwd; sub ax, dx; sar ax, 1.
int half(int v) { return (v - (v < 0 ? -1 : 0)) >> 1; }

void set_nibble(is::fmt::Dib& d, int x, int y, int v) {
    const int row = d.bottom_up ? d.height - 1 - y : y;
    std::uint8_t& b = d.bits[static_cast<std::size_t>(row) * d.stride() + static_cast<std::size_t>(x / 2)];
    b = (x & 1) ? static_cast<std::uint8_t>((b & 0xf0) | v) : static_cast<std::uint8_t>((b & 0x0f) | (v << 4));
}

// A 20-row body graphic: ordinal 12 makes it the 40 x 40 body image DGROUP:0x92b0 (2H x 2H), the
// k sz square of sz = 20. Entry 15 is the black background, the colours those of the smooth
// renderer's hand graphic (tests/test_gfx_smooth_render.cpp).
is::fmt::Dib body_graphic() {
    is::fmt::Dib d;
    d.width = 20;
    d.height = 20;
    d.bpp = 4;
    d.bottom_up = true;
    for (int k = 0; k < 15; ++k)
        d.palette.push_back(static_cast<std::uint32_t>((0x10 * k) << 16 | (0x08 * k) << 8 | (0xff - 0x10 * k)));
    d.palette.push_back(0);
    d.bits.assign(d.stride() * 20, 0xff);
    for (int y = 3; y <= 16; ++y)
        for (int x = 3; x <= 16; ++x) set_nibble(d, x, y, (y == 3 || y == 16 || x == 3 || x == 16) ? 3 : 12);
    return d;
}

// An 18 x 18 4-bpp flame, graphic 280's shape for ordinal 18 with x = 1 (gfx::launch_flame): a
// diamond of bright entries, black elsewhere.
is::fmt::Dib flame_graphic() {
    is::fmt::Dib d;
    d.width = 18;
    d.height = 18;
    d.bpp = 4;
    d.bottom_up = true;
    d.palette.push_back(0);
    for (int k = 1; k < 15; ++k) d.palette.push_back(0x00ff0000u | static_cast<std::uint32_t>(0x10 * k) << 8);
    d.palette.push_back(0);
    d.bits.assign(d.stride() * 18, 0);
    for (int y = 0; y < 18; ++y)
        for (int x = 0; x < 18; ++x)
            if (std::abs(2 * x - 17) + std::abs(2 * y - 17) < 14) set_nibble(d, x, y, 1 + (x + y) % 14);
    return d;
}

// seg15:0x20ca's inputs for a ship of sz = 20 (r7a 20, r7b 6) without wings cut.
is::gfx::VisitShipArt look_of() {
    is::gfx::VisitShipArt v;
    v.body = body_graphic();
    v.id = 60;
    v.a = 0;
    v.b = 10;
    v.flame_row = 0;
    v.r7a = 20;
    v.r7b = 6;
    v.fy = 12;
    v.band = 0;
    v.detail = 3;
    v.flame = flame_graphic();
    return v;
}

std::uint8_t at(const ImagePtr& img, int x, int y) {
    if (!img || x < 0 || y < 0 || x >= img->width() || y >= img->height()) return 0;
    return img->at(x, y);
}

// The squares seg15:0x20ca leaves, from the body B and the flame F of the readings: thrust over
// the ship (DGROUP:0x971a + d = 0) copies B, cuts, copies into DGROUP:0x92b4 and then lays F over
// DGROUP:0x92b2 only; thrust behind lays F on a black square, then B through its mask, copies B
// into DGROUP:0x92b4 and cuts both (s6-pad.md 2.2, verified claims 33-35) [BYTES
// seg15:0x2273-0x23b3 seg15:0x23b9-0x25cd seg15:0x25d0-0x27c3].
struct Squares {
    std::vector<std::uint8_t> q, q2;
};
Squares expected(const ImagePtr& body, const ImagePtr& flame, int ks, int fx, int fy, bool behind, int cut_cols) {
    Squares s;
    s.q.assign(static_cast<std::size_t>(ks) * ks, 0);
    s.q2.assign(static_cast<std::size_t>(ks) * ks, 0);
    for (int y = 0; y < ks; ++y)
        for (int x = 0; x < ks; ++x) {
            const std::size_t i = static_cast<std::size_t>(y) * ks + x;
            const std::uint8_t b = at(body, x, y);
            const std::uint8_t f = at(flame, x - kK * fx, y - kK * fy);  // 9k x 9k from 18 x 18: 1:1 at k = 2
            const bool cut = x < cut_cols;
            if (behind) {
                s.q[i] = cut ? 0 : (b != 0 ? b : f);
                s.q2[i] = cut ? 0 : b;
            } else {
                const std::uint8_t q = cut ? 0 : b;
                s.q[i] = f != 0 ? f : q;
                s.q2[i] = q;
            }
        }
    return s;
}

int differ(const ImagePtr& img, const std::vector<std::uint8_t>& want, int ks) {
    img->flush();
    int n = 0;
    for (int y = 0; y < ks; ++y)
        for (int x = 0; x < ks; ++x) n += img->at(x, y) != want[static_cast<std::size_t>(y) * ks + x] ? 1 : 0;
    return n;
}

is::hud::VisitGdi make_visit(Gfx& g) {
    return is::hud::VisitGdi([&g] { return &g; }, [] { return static_cast<is::hud::GdiCanvas*>(nullptr); },
                             [] { return is::hud::VisitStatus{}; }, [] { return ImagePtr{}; });
}

}  // namespace

TEST_CASE("visit s8 win32: seg15:0x20ca's thrust behind lays the flame first and keeps it out of DGROUP:0x92b4 (S6-5)") {
    Gfx g(nullptr);
    g.palette_create();
    is::gfx::VisitShipArt v = look_of();
    const int sz = is::gfx::visit_ship_size(v), ks = kK * sz;
    REQUIRE(sz == 20);
    const int fx = half(sz) - 4;  // [BYTES seg15:0x23b9-0x25cd] (verified claim 35)
    for (const bool behind : {false, true})
        for (const bool cut : {false, true}) {
            CAPTURE(behind);
            CAPTURE(cut);
            v.behind = behind ? 1 : 0;
            v.ability = cut ? 3 : -1;
            v.left_wing = !cut;  // ship byte +0xac = 0
            const is::gfx::LaunchShip sh = is::gfx::ship_20ca(g, v, {});
            REQUIRE(sh.ok());
            REQUIRE(sh.flameless);
            CHECK(sh.s == sz);
            REQUIRE(sh.body->width() == ks);
            REQUIRE(sh.body->height() == ks);
            const is::gfx::LaunchFlame f = is::gfx::launch_flame(g, v.flame, 0);
            REQUIRE(f.img);
            sh.body->flush();
            f.img->flush();
            // w8 = half(sz) - trunc8(band / 2): a missing left wing blacks columns 0 ... k w8 - 1.
            const int cols = cut ? kK * (half(sz) - v.band / 2) : 0;
            const Squares want = expected(sh.body, f.img, ks, fx, v.fy, behind, cols);
            CHECK(differ(sh.square, want.q, ks) == 0);
            CHECK(differ(sh.flameless, want.q2, ks) == 0);
            // The pictures tell the two orders apart: the body hides the flame behind it, and the
            // flame over a cut wing stays only when it is drawn after the cut.
            sh.square->flush();
            int flame_over_body = 0, flame_in_cut = 0, body_px = 0;
            for (int y = 0; y < ks; ++y)
                for (int x = 0; x < ks; ++x) {
                    const std::uint8_t b = at(sh.body, x, y);
                    const std::uint8_t fp = at(f.img, x - kK * fx, y - kK * v.fy);
                    body_px += (x < cols && b != 0) ? 1 : 0;
                    if (b != 0 && fp != 0 && x >= cols && sh.square->at(x, y) == fp && fp != b) ++flame_over_body;
                    if (x < cols && fp != 0 && sh.square->at(x, y) != 0) ++flame_in_cut;
                }
            if (behind)
                CHECK(flame_over_body == 0);
            else
                CHECK(flame_over_body > 0);
            if (cut) {
                CHECK(body_px > 0);  // DGROUP:0x92b0 keeps both wings
                if (behind)
                    CHECK(flame_in_cut == 0);
                else
                    CHECK(flame_in_cut > 0);
            }
        }
}

// seg15:0x454c (shell.md "State 6", verified in work/analysis/m4/s7-owed-verify.md S6-3) [BYTES
// seg15:0x456d-0x4628 seg15:0x462b-0x4655 seg15:0x4655-0x4689 seg15:0x468b-0x4753]: ry, rx, bh, vy,
// edge = DGROUP:0x6dac + DGROUP:0x6db4, bottom = bh + ry, half = trunc(W/2), D = half - edge - 2,
// N = half - 0x5a - edge, y1 = (vy - ry)(D - 90)/D + ry, y2 = (vy - bottom)(D - 90)/D + bottom; for
// i = 0, 2, ... below N the grey c = (-150 i)/N + 170 as PALETTERGB, PatBlt (edge + i, ry +
// i (y1 - ry)/N, 2, bh + i ((y2 - bottom) - (y1 - ry))/N) and (rx - 2 - i, the same y, 2, that
// height + 2), drawn by seg16:0x2144 between its 47 greys and seg10:0x6e76(0, 1). Before it the
// landing scene's seg10:0x6da6(0, 1) blacks entries 10-245 [BYTES seg15:0x3099-0x309d], so the 47
// greys find entries, as in a visit.
TEST_CASE("visit s8 win32: seg15:0x454c's walls, 2-px grey columns from the panels toward the vanishing point (S6-5)") {
    const auto start = [](Gfx& x) {
        x.set_client_size(1024, 768, 1);
        x.palette_create();
        x.blacken();
    };
    Gfx g(nullptr);
    start(g);
    is::hud::VisitGdi v = make_visit(g);
    v.play_height = [] { return 683; };  // DGROUP:0x90ec under the HUD at 1024 x 768 [ORACLE core-0003]
    const VisitLayout l = visit_layout(1024, 768, 683);
    v.landing_scene(l, "", true, false);
    v.redraw(l, 1, 0, false);
    g.surface().flush();
    const int ry = l.ry, rx = l.rx, bh = l.panel_bg_h, vy = l.vy;
    const int edge = l.panel_w + l.lx, bottom = bh + ry, hw = l.W / 2;
    const int D = hw - edge - 2, N = hw - 0x5a - edge;
    const int y1 = (vy - ry) * (D - 90) / D + ry, y2 = (vy - bottom) * (D - 90) / D + bottom;
    REQUIRE(N > 8);
    CHECK(vy == 321);  // gfx/pad.h pad_geometry for the play height 683
    // The column colours in the palette of the moment: a twin through the same palette calls,
    // seg15:0x302e's seg10:0x6da6(0, 1), then seg16:0x2144's seg10:0x6da6(1, 0), seg10:0x6e76(0, 2),
    // the greys and seg10:0x6e4c [BYTES seg16:0x219d-0x21d2].
    Gfx twin(nullptr);
    start(twin);
    twin.palette_save(0, 1);
    twin.palette_save(1, 0);
    twin.palette_scheme(0, 2);
    for (int c = 40; c < 228; c += 4) twin.palette_colour(c, c, c);
    twin.palette_animate();
    std::vector<int> want, grey;
    for (int i = 0; i < N; i += 2) {
        const int c = (-150 * i) / N + 170;
        want.push_back(twin.colour_index(0x02000000u | RGB(c, c, c)));
        const is::gfx::Rgb e = twin.palettes().device()[static_cast<std::size_t>(want.back())];
        grey.push_back(e.r == e.g && e.g == e.b ? e.r : -1);
    }
    // The greys found entries: each column's entry is the ramp's grey nearest c, darker inward.
    CHECK(std::abs(grey.front() - 170) <= 2);
    for (std::size_t n = 1; n < grey.size(); ++n) CHECK(grey[n] <= grey[n - 1]);
    CHECK(grey.back() >= 0);
    int wrong = 0, columns = 0;
    std::string first;
    for (int i = 0, n = 0; i < N; i += 2, ++n) {
        const int top = i * (y1 - ry) / N + ry;
        const int h = i * ((y2 - bottom) - (y1 - ry)) / N + bh;
        const int mid = top + h / 2;
        const auto check = [&](int x, int y, int expect) {
            const int px = g.surface().at(x, y);
            if (px != expect) {
                if (wrong < 4)
                    first += "(" + std::to_string(x) + ", " + std::to_string(y) + ") " + std::to_string(px) + " not " +
                             std::to_string(expect) + "; ";
                ++wrong;
            }
        };
        const int c = want[static_cast<std::size_t>(n)];
        for (const int x : {edge + i, edge + i + 1}) {
            // The left panel's outline, drawn after the walls, takes column `edge` [BYTES
            // seg16:0x22a3-0x231a].
            if (x == edge) continue;
            // Inside the column away from the pen lines that run along its ends, and black past them.
            for (int y = top + 6; y < top + h - 14; y += 7) check(x, y, c);
            if (c != 0) check(x, top + h + 8, 0);
        }
        for (const int x : {rx - 2 - i, rx - 1 - i}) {
            for (int y = top + 6; y < top + h + 2 - 14; y += 7) check(x, y, c);
            if (c != 0) check(x, top + h + 2 + 8, 0);
        }
        check(edge + i + 1, mid, c);
        ++columns;
    }
    // CreatePen(PS_SOLID, 1, 0x00C0C0C0)'s line down the left panel's inner edge over the first column.
    CHECK(g.surface().at(edge, (ry + bottom) / 2) == g.colour_index(0x00c0c0c0));
    MESSAGE(columns << " column pairs, " << wrong << " pixels wrong" << (wrong ? " from " + first : ""));
    CHECK(wrong == 0);
    // Nothing between the walls: the room's middle stays black.
    CHECK(g.surface().at(hw, (vy + bottom) / 2) == 0);
}

// seg15:0x3caa(first) in state 6 (s6-scene.md section 3, verified claims 61-68) [BYTES
// seg15:0x3cc6-0x3d5a seg15:0x3d60-0x3e44 seg15:0x3f57-0x3f6a seg15:0x42ac-0x42de seg15:0x42e1-0x4540]:
// y10 = 10 DGROUP:0x92d8, step = (DGROUP:0x92d8 - DGROUP:0x92d6)/16, speed 150; each receding call
// idx = clamp(((y10 / -10) + DGROUP:0x92d8 - 55)/step, 0, 15), speed = max(150 - 4 idx, 9), frame idx
// at (W/2 - w0/2 + w0/2 - w_idx/2, y10/10), y10 += 2 speed / -3, and below 10 DGROUP:0x92d6 y10 -= 20,
// speed 200, phase 1; then speed = max(speed - 5, 50), frame 15 two rows down, y10 += 2 speed / 20,
// and 1 once y10 > 10 (DGROUP:0x92d6 + 44).
TEST_CASE("visit s8 win32: seg15:0x3caa's jet, 74 receding calls with frames 0 ... 14 and 60 with frame 15 (S6-5)") {
    Gfx g(nullptr);
    g.set_client_size(1024, 768, 1);
    g.palette_create();
    g.blacken();
    is::hud::VisitGdi v = make_visit(g);
    const is::gfx::VisitShipArt look = look_of();
    v.ship_look = [&look] { return look; };
    const VisitLayout l = visit_layout(1024, 768);
    v.landing_scene(l, "", true, false);  // no scene: step 5's words, then seg15:0x1a42's frames
    CHECK(v.d92d6 == 168);  // trunc(22 H / 100)
    CHECK(v.d92d8 == 758);  // H - 10
    CHECK(v.d92de == 768);
    v.d92d6 = 161;  // step 7 with scene 105: slot 1's top - 40 and slot 0's bottom + 1
    v.d92de = 629;
    const std::array<is::gfx::JetFrame, 16> t = is::gfx::jet_table(20, 3);
    const int W = 1024, w0 = t[0].w, x0 = half(W) - half(w0);
    REQUIRE(w0 > 0);
    int y10 = 10 * 758, phase = 0, idx = 0, speed = 150;
    const int step = (758 - 161) / 16;
    CHECK(step == 37);
    int calls = 0, receding = 0, r = 0, bad_box = 0, empty = 0, last_idx = -1;
    std::string first_bad;
    for (int first = 1;; first = 0) {
        // The model's draw for this call.
        int fi = 15, top = 0, row = 0;
        bool landed = false;
        if (phase == 1) {
            speed = std::max(speed - 5, 50);
            top = y10 / 10;
            row = top + 2;
            y10 += 2 * speed / 20;
            landed = y10 > 10 * (161 + 44);
        } else {
            idx = std::clamp((y10 / -10 + 758 - 55) / step, 0, 15);
            fi = last_idx = idx;
            top = row = y10 / 10;
            speed = std::max(150 - 4 * idx, 9);
            y10 += 2 * speed / -3;
            if (y10 < 10 * 161) {
                y10 -= 20;
                speed = 200;
                phase = 1;
            }
            ++receding;
        }
        r = v.incoming(first);
        ++calls;
        CHECK(r == (landed ? 1 : 0));
        // The frame's box on the screen; in phase 1 only the work rect's rows are the call's.
        const int fx = x0 + half(w0) - half(t[static_cast<std::size_t>(fi)].w);
        const int fw = t[static_cast<std::size_t>(fi)].w, fh = t[static_cast<std::size_t>(fi)].h;
        const int y_from = fi == 15 && top != row ? top : 0, y_to = fi == 15 && top != row ? top + t[0].h + 15 : 768;
        g.surface().flush();
        int inside = 0;
        for (int y = std::max(y_from, 0); y < std::min(y_to, 768); ++y)
            for (int x = 0; x < W; ++x) {
                if (g.surface().at(x, y) == 0) continue;
                if (x >= fx && x < fx + fw && y >= row && y < row + fh) {
                    ++inside;
                } else {
                    if (bad_box < 3)
                        first_bad += "call " + std::to_string(calls) + " (" + std::to_string(x) + ", " +
                                     std::to_string(y) + "); ";
                    ++bad_box;
                }
            }
        if (inside == 0 && row + fh <= 768) ++empty;  // the first frames reach below the screen
        if (r != 0 || calls >= 400) break;
    }
    MESSAGE(calls << " calls, " << receding << " receding, " << bad_box << " pixels outside the frame"
                  << (bad_box ? " from " + first_bad : ""));
    CHECK(calls == 134);
    CHECK(receding == 74);
    CHECK(last_idx == 14);
    CHECK(bad_box == 0);
    CHECK(empty == 0);
}
#endif
