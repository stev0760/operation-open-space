// The radar (src/hud/radar.h; docs/re/world.md "Radar", "Scrollbar"): DGROUP:0x93d6 by zoom
// (6 at 100 ... 166 down to 4 at 500, the cap 26), the Detail gate of seg11:0x3396, the colour
// tables of both depths and the creation order, the torus deltas and the mapping, the repaint's
// call sequence, the four callbacks, the race path, and the zoom keys and scrollbar handler.
#include <cstdint>
#include <vector>

#include "doctest.h"
#include "hud/radar.h"

using namespace is::hud;
using is::world::BlitCall;
using is::world::bit_blt_call;
using is::world::kBlackness;
using is::world::kDcWindow;
using is::world::pat_blt_call;
using is::world::RecordingBlitter;

namespace {

struct Fixture {
    is::world::Pools pools;
    is::world::ShipTable ships{};
    RadarInputs in{pools, ships};
    Radar r;
    RecordingBlitter out;
    std::vector<int> scroll_pos;

    Fixture() {
        is::world::ObjectRecord& p = pools.rec(is::world::kPoolShips, 0);
        p.x = 12800, p.y = 12800;
        in.play_w = 1024, in.play_h = 683, in.shift = 3;
        in.view_x = 12800 - 4096, in.view_y = 12800 - 2732;
        in.hud_top = 683;
        in.detail = 3;
        r.create(out, true, 0x99);
        r.set_centre();
        r.set_scroll_pos = [this](int pos) { scroll_pos.push_back(pos); };
    }
    // An in-play object of pool `pool`, record `rec`, at (x, y) with callback `cb`.
    is::world::ObjectRecord& put(int pool, int rec, std::int16_t x, std::int16_t y,
                                 std::uint32_t cb) {
        is::world::ObjectRecord& o = pools.rec(pool, rec);
        o.x = x, o.y = y;
        o.mass = 0xa0;
        o.radar_draw.set(cb);
        if (pools.in_use(pool) <= rec) pools.set_in_use(pool, static_cast<std::int16_t>(rec + 1));
        return o;
    }
};

std::int16_t refresh_div_at(std::int32_t zoom) {
    Fixture f;
    f.r.zoom = zoom;
    f.r.repaint(f.in, f.out);
    return f.r.refresh_div;
}

}  // namespace

TEST_CASE("radar: DGROUP:0x93d6 by zoom") {
    CHECK(refresh_div_at(100) == 6);
    CHECK(refresh_div_at(120) == 6);
    CHECK(refresh_div_at(166) == 6);
    CHECK(refresh_div_at(167) == 5);
    CHECK(refresh_div_at(300) == 5);
    CHECK(refresh_div_at(500) == 4);
    // The cap: 20 * 1000 / 620 = 32 -> 26.
    Fixture f;
    f.r.rate_word = 20;
    f.r.repaint(f.in, f.out);
    CHECK(f.r.refresh_div == 26);
    // lo16(100 * 1000) is negative: no cap.
    f.r.rate_word = 100;
    f.r.repaint(f.in, f.out);
    CHECK(f.r.refresh_div == static_cast<std::int16_t>(-31072 / 620));
}

TEST_CASE("radar: the refresh gate counts calls by Detail") {
    auto calls_to_repaint = [](std::int16_t detail) {
        Fixture f;
        f.in.detail = detail;
        f.r.refresh_div = 6;
        for (int n = 1; n <= 40; ++n)
            if (f.r.refresh(f.in, f.out)) return n;
        return 0;
    };
    CHECK(calls_to_repaint(0) == 12);  // ((8 << 4) >> 6) * 6
    CHECK(calls_to_repaint(1) == 6);
    CHECK(calls_to_repaint(4) == 6);
    CHECK(calls_to_repaint(5) == 1);   // every call
    CHECK(calls_to_repaint(8) == 1);
    CHECK(calls_to_repaint(9) == 1);   // negative
    // Before any repaint DGROUP:0x93d6 is 0 (BSS): the first call repaints.
    Fixture f;
    f.r.refresh_div = 0;
    f.in.detail = 0;
    CHECK(f.r.refresh(f.in, f.out));
    CHECK(f.r.counter == 0);
    CHECK(f.r.refresh_div == 6);
    CHECK_FALSE(f.r.refresh(f.in, f.out));
    CHECK(f.r.counter == 1);
}

TEST_CASE("radar: the brush table for both colour depths") {
    CHECK(radar_brush_colour(0, true) == 0x02006000u);
    CHECK(radar_brush_colour(0, false) == 0x02004000u);
    CHECK(radar_brush_colour(1, true) == 0x0040C0FFu);  // RGB(255, 192, 64)
    CHECK(radar_brush_colour(2, true) == 0x02FF8000u);
    CHECK(radar_brush_colour(2, false) == 0x02FF3F00u);
    CHECK(radar_brush_colour(3, true) == 0u);  // METAL pattern
    CHECK(radar_brush_colour(4, false) == 0x00C0C0C0u);
    CHECK(radar_brush_colour(5, false) == 0x020040FFu);
    CHECK(radar_brush_colour(6, true) == 0x00FFA080u);  // RGB(128, 160, 255)
    CHECK(radar_brush_colour(7, false) == 0x02FFC800u);
    // The depths differ only in 0 and 2.
    for (int i = 0; i < kRadarBrushes; ++i)
        if (i != 0 && i != 2) CHECK(radar_brush_colour(i, true) == radar_brush_colour(i, false));
    // Creation: pattern brush 3 given, then 0, 7, 6, 2, 4, 1, 5; the 86 x 71 image; RADSHIP.
    Fixture f;
    REQUIRE(f.out.brushes.size() == 7);
    const int order[] = {0, 7, 6, 2, 4, 1, 5};
    for (int k = 0; k < 7; ++k) {
        CHECK(f.out.brushes[static_cast<std::size_t>(k)].colorref == radar_brush_colour(order[k], true));
        CHECK(f.r.brushes[static_cast<std::size_t>(order[k])] == f.out.brushes[static_cast<std::size_t>(k)].id);
    }
    CHECK(f.r.brushes[kRadarBrushMetal] == 0x99);
    REQUIRE(f.out.images.size() == 2);
    CHECK(f.out.images[0].w == 86);
    CHECK(f.out.images[0].h == 71);
    CHECK(f.r.dc == f.out.images[0].id);
    CHECK(f.out.images[1].name == "RADSHIP");
    CHECK(f.r.ship_dc == f.out.images[1].id);
    CHECK(f.r.div_x == 320000);
    CHECK(f.r.div_y == 320000);
    // Once only (DGROUP:0x5042).
    f.r.create(f.out, false, 0x98);
    CHECK(f.out.brushes.size() == 7);
}

TEST_CASE("radar: torus deltas and the mapping") {
    // Centre 1600 cells (x 12800).
    CHECK(Radar::wrap_delta(13600, 12800, 3200) == 100);
    CHECK(Radar::wrap_delta(12000, 12800, 3200) == -100);
    CHECK(Radar::wrap_delta(100, 12800, 3200) == -1588);   // 12 - 1600
    CHECK(Radar::wrap_delta(25000, 12800, 3200) == 1525);  // 3125 - 1600
    // Across the seam: centre x 100 (12 cells), x 25000 (3125): 87 cells the short way.
    CHECK(Radar::wrap_delta(25000, 100, 3200) == -87);
    CHECK(Radar::wrap_delta(100, 25000, 3200) == 87);
    // The shift is 32-bit `sar` (rounds down).
    CHECK(Radar::wrap_delta(-1, 0, 3200) == -1);
    Fixture f;
    f.r.zoom = 100;
    is::world::Point16 p = f.r.map(f.in, 13600, 12800);
    CHECK(p.x == 43 + 2);  // 100 * 100 * 86 / 320000 = 2.69
    CHECK(p.y == 35);
    p = f.r.map(f.in, 12000, 12000);
    CHECK(p.x == 43 - 2);
    CHECK(p.y == 35 - 2);  // 100 * 100 * 71 / 320000 = 2.22
    f.r.zoom = 500;
    p = f.r.map(f.in, 13600, 12800);
    CHECK(p.x == 43 + 13);  // 13.4
    // The centre follows DGROUP:0x6b50/0x6b52 while DGROUP:0x951a != 0, the world centre in a
    // race.
    f.in.dg_951a = 1;
    f.in.alt_centre_x = 13600;
    CHECK(f.r.map(f.in, 13600, 12800).x == 43);
    f.in.race = 1;
    CHECK(f.r.map(f.in, 12800, 12800).x == 43);
}

TEST_CASE("radar: the repaint clears, frames the view, draws the objects and blits") {
    Fixture f;
    f.out.calls.clear();
    f.r.repaint(f.in, f.out);
    REQUIRE(f.out.calls.size() == 5);
    CHECK(f.out.calls[0] == pat_blt_call(f.r.dc, 0, 0, 86, 71, kBlackness));
    // The view (origin 8704, 10068; far corner + (8192, 5464)) at zoom 120.
    BlitCall frame;
    frame.op = BlitCall::kFrameRect, frame.dst = f.r.dc, frame.x = 27, frame.y = 26, frame.w = 59,
    frame.h = 44, frame.brush = f.r.brushes[0];
    CHECK(f.out.calls[1] == frame);
    CHECK(f.out.calls[2].op == BlitCall::kSelectBrush);
    CHECK(f.out.calls[3].op == BlitCall::kSelectBrush);
    CHECK(f.out.calls[4] == bit_blt_call(kDcWindow, 512 - 43, 683 + 7, 86, 71, f.r.dc, 0, 0,
                                         is::world::kSrcCopy));
    CHECK(f.r.clip.l == 1);
    CHECK(f.r.clip.t == 1);
    CHECK(f.r.clip.r == 85);
    CHECK(f.r.clip.b == 69);
    // DGROUP:0x9158 frames, all the same.
    f.in.view_rects = 4;
    f.out.calls.clear();
    f.r.repaint(f.in, f.out);
    CHECK(f.out.calls.size() == 8);
    CHECK(f.out.calls[4] == frame);
    // No repaint without the HUD gate.
    f.in.hud_gate = false;
    f.out.calls.clear();
    f.r.repaint(f.in, f.out);
    CHECK(f.out.calls.empty());
}

TEST_CASE("radar: the callbacks") {
    Fixture f;
    // Box (seg11:0x39d8) for an icon, brush 2: 2 x 2 at zoom 120.
    is::world::ObjectRecord& icon = f.put(0, 0, 13600, 12800, kRadarDot);
    icon.radar_brush = 2;
    // Pixel (seg11:0x3914) in radar_color.
    is::world::ObjectRecord& dot = f.put(2, 0, 12000, 12800, kRadarSmall);
    dot.radar_color.set(0x0000FFFF);
    // Not drawn: mass 0, state not 0, outside the clip, an unknown callback.
    f.put(2, 1, 12000, 12000, kRadarSmall).mass = 0;
    f.put(2, 2, 12000, 12000, kRadarSmall).state = 1;
    f.put(2, 3, 25000, 12800, kRadarSmall);
    f.put(2, 4, 12000, 12000, 0x12345678);
    f.out.calls.clear();
    f.r.repaint(f.in, f.out);
    std::vector<BlitCall> draws(f.out.calls.begin() + 3, f.out.calls.end() - 2);
    REQUIRE(draws.size() == 3);
    CHECK(draws[0].op == BlitCall::kSelectBrush);
    CHECK(draws[0].brush == f.r.brushes[2]);
    CHECK(draws[1] == pat_blt_call(f.r.dc, 46, 35, 2, 2, is::world::kPatCopy));
    BlitCall px;
    px.op = BlitCall::kSetPixel, px.dst = f.r.dc, px.x = 40, px.y = 35, px.rop = 0x0000FFFF;
    CHECK(draws[2] == px);
    // Box sides by zoom: (zoom - 200)/150 + 2 by (zoom - 200)/200 + 2.
    struct Side {
        int zoom, w, h;
    };
    for (const Side& c : {Side{100, 2, 2}, Side{349, 2, 2}, Side{350, 3, 2}, Side{400, 3, 3},
                          Side{500, 4, 3}}) {
        CAPTURE(c.zoom);
        const int w = c.w, h = c.h;
        Fixture g;
        g.put(0, 0, 12800, 12800, kRadarDot);
        g.r.clip = is::world::Rect16{1, 1, 85, 69};
        g.r.zoom = c.zoom;
        g.out.calls.clear();
        g.r.draw_record(g.in, g.out, 0, 0);
        REQUIRE(g.out.calls.size() == 2);
        CHECK(g.out.calls[1].w == w);
        CHECK(g.out.calls[1].h == h);
    }
}

TEST_CASE("radar: rocks and ships") {
    // seg11:0x3bae: a pixel up to zoom 200, a square above.
    Fixture f;
    is::world::ObjectRecord& rock = f.put(3, 0, 12800, 12800, kRadarBig);
    rock.radar_color.set(0x00C0C0C0);
    rock.radar_brush = 4;
    f.r.clip = is::world::Rect16{1, 1, 85, 69};
    f.r.zoom = 200;
    f.r.draw_record(f.in, f.out, 3, 0);
    REQUIRE(f.out.calls.size() == 1);
    CHECK(f.out.calls[0].op == BlitCall::kSetPixel);
    f.out.calls.clear();
    f.r.zoom = 500;
    f.r.draw_record(f.in, f.out, 3, 0);
    REQUIRE(f.out.calls.size() == 2);
    CHECK(f.out.calls[1] == pat_blt_call(f.r.dc, 43, 35, 4, 4, is::world::kPatCopy));
    // seg11:0x37a0: the player WHITE_BRUSH, MERGECOPY of a 5 x 5 RADSHIP cell at heading * 5.
    Fixture s;
    s.r.clip = is::world::Rect16{1, 1, 85, 69};
    is::world::ObjectRecord& me = s.put(14, 0, 12800, 12800, kRadarShip);
    me.frame = 3;
    is::world::ObjectRecord& other = s.put(14, 1, 13600, 12800, kRadarShip);
    other.frame = 15;
    other.radar_brush = 5;
    s.r.draw_record(s.in, s.out, 14, 0);
    s.r.draw_record(s.in, s.out, 14, 1);
    REQUIRE(s.out.calls.size() == 4);
    CHECK(s.out.calls[0].brush == is::world::kBrushWhite);
    CHECK(s.out.calls[1] == bit_blt_call(s.r.dc, 41, 33, 5, 5, s.r.ship_dc, 15, 0,
                                         is::world::kMergeCopy));
    CHECK(s.out.calls[2].brush == s.r.brushes[5]);
    CHECK(s.out.calls[3] == bit_blt_call(s.r.dc, 44, 33, 5, 5, s.r.ship_dc, 75, 0,
                                         is::world::kMergeCopy));
    // Pool 7: 4 x 4 from (80, 0).
    s.out.calls.clear();
    s.put(7, 0, 12800, 12800, kRadarShip);
    s.r.draw_record(s.in, s.out, 7, 0);
    REQUIRE(s.out.calls.size() == 2);
    CHECK(s.out.calls[1] == bit_blt_call(s.r.dc, 41, 33, 4, 4, s.r.ship_dc, 80, 0,
                                         is::world::kMergeCopy));
    // Ship byte +0x147 hides a ship.
    s.out.calls.clear();
    s.ships[1].radar_hide = 1;
    s.r.draw_record(s.in, s.out, 14, 1);
    CHECK(s.out.calls.empty());
}

TEST_CASE("radar: the race repaint") {
    Fixture f;
    f.in.race = 1;
    f.in.last_ship = 1;
    f.r.dots_dc = 0x77;
    f.r.dot_x = {10, 20};
    f.r.dot_y = {30, 40};
    f.put(14, 0, 12800, 12800, kRadarShip).mass = 0;  // mass does not count in a race
    f.put(14, 1, 14400, 12800, kRadarShip);
    f.put(0, 0, 12800, 12800, kRadarDot);  // other pools are not drawn
    f.r.zoom = 300;
    f.out.calls.clear();
    f.r.repaint(f.in, f.out);
    CHECK(f.r.zoom == 100);
    REQUIRE(f.out.calls.size() == 9);
    CHECK(f.out.calls[1] == bit_blt_call(f.r.dc, 8, 28, 5, 5, 0x77, 8, 28, is::world::kSrcCopy));
    CHECK(f.out.calls[2] == bit_blt_call(f.r.dc, 18, 38, 5, 5, 0x77, 18, 38, is::world::kSrcCopy));
    // The dots: ship 1 is 200 cells right of the world centre at zoom 100: 200 * 8600 / 320000.
    CHECK(f.r.dot_x[0] == 43);
    CHECK(f.r.dot_x[1] == 43 + 5);
    CHECK(f.r.dot_y[1] == 35);
}

TEST_CASE("radar: the zoom keys and the scrollbar handler") {
    Fixture f;
    f.out.calls.clear();
    f.r.zoom_key(true, f.in, f.out);
    CHECK(f.r.zoom == 128);
    REQUIRE(f.scroll_pos.size() == 1);
    CHECK(f.scroll_pos[0] == 472);
    CHECK_FALSE(f.out.calls.empty());  // repainted
    f.r.zoom = 497;
    f.r.zoom_key(true, f.in, f.out);
    CHECK(f.r.zoom == 500);
    f.r.zoom_key(true, f.in, f.out);  // unchanged, still SetScrollPos and a repaint
    CHECK(f.scroll_pos.back() == 100);
    CHECK(f.scroll_pos.size() == 3);
    f.r.zoom = 104;
    f.r.zoom_key(false, f.in, f.out);
    CHECK(f.r.zoom == 100);
    CHECK(f.scroll_pos.back() == 500);

    Fixture s;
    s.out.calls.clear();
    CHECK_FALSE(s.r.scroll(5, 480, s.in, s.out));  // 600 - 480 = 120: unchanged
    CHECK(s.out.calls.empty());
    CHECK(s.scroll_pos.empty());
    CHECK(s.r.scroll(5, 400, s.in, s.out));
    CHECK(s.r.zoom == 200);
    CHECK(s.scroll_pos.back() == 400);
    CHECK_FALSE(s.out.calls.empty());
    CHECK(s.r.scroll(0, 0, s.in, s.out));  // SB_LINEUP
    CHECK(s.r.zoom == 220);
    CHECK(s.r.scroll(1, 0, s.in, s.out));  // SB_LINEDOWN
    CHECK(s.r.zoom == 200);
    CHECK_FALSE(s.r.scroll(2, 0, s.in, s.out));  // SB_PAGEUP does nothing
    CHECK_FALSE(s.r.scroll(3, 0, s.in, s.out));
    CHECK(s.r.scroll(6, 0, s.in, s.out));  // SB_TOP
    CHECK(s.r.zoom == 500);
    CHECK(s.r.scroll(7, 0, s.in, s.out));  // SB_BOTTOM
    CHECK(s.r.zoom == 100);
    CHECK(s.r.scroll(4, 50, s.in, s.out));  // 550, clamped
    CHECK(s.r.zoom == 500);
    CHECK_FALSE(s.r.scroll(0, 0, s.in, s.out));  // 520 clamped back to 500
    CHECK(scroll_pos_of(120) == 480);
    // The tail for a key switch that computes the zoom itself.
    s.scroll_pos.clear();
    s.out.calls.clear();
    s.r.zoom_changed(136, s.in, s.out);
    CHECK(s.r.zoom == 136);
    CHECK(s.scroll_pos == std::vector<int>{464});
    CHECK_FALSE(s.out.calls.empty());
}
