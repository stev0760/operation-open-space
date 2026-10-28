// The HUD repaint seg11:0xafe (docs/re/world.md "Play-area frame and status display"): the
// layout table at W = 1024, 640 and 800, the children's rectangles, the tile sequence of
// seg11:0x170a, the border tiles and the bevel at 1024 x 768, the clip region and message line
// of seg2:0xca4, and the repaint's order and gate.
#include <algorithm>

#include "doctest.h"
#include "hud/hud.h"
#include "hud_fakes.h"
#include "shell/geometry.h"

using namespace is::hud;
using namespace is::shell;
using hudtest::Call;
using hudtest::FakeHost;
using hudtest::RecordingCanvas;

TEST_CASE("hud layout: the table at 1024, 800 and 640, and in a race") {
    // world.md "Layout": at 1024 L = 4, Lw = 134, 282, 142, 462, 428, 580, 608, 668, 352.
    HudValues v = hud_values(1024, false);
    CHECK(v.left == 4);
    CHECK(v.left_w == 134);
    CHECK(v.target_w == 282);
    CHECK(v.target_x == 142);
    CHECK(v.radar_x == 462);
    CHECK(v.fuel_x == 428);
    CHECK(v.damage_x == 580);
    CHECK(v.clock_x == 608);
    CHECK(v.weapon_x == 668);
    CHECK(v.weapon_w == 352);
    // 800: W/2 = 400; at or above 760 and 700
    v = hud_values(800, false);
    CHECK(v == HudValues{4, 134, 170, 142, 350, 316, 468, 496, 556, 240});
    // 640: below 660 (1, 0x49) and below 700 (+9, -2, no -4, +4)
    v = hud_values(640, false);
    CHECK(v == HudValues{1, 73, 160, 76, 270, 240, 388, 416, 476, 164});
    // 700 is not below 700; 699 is
    CHECK(hud_values(700, false).fuel_x == 350 - 50 - 30 - 4);
    CHECK(hud_values(699, false).fuel_x == 349 - 50 - 30);
    // 760 takes (4, 0x86); 759 (1, 0x5a)
    CHECK(hud_values(760, false).left_w == 0x86);
    CHECK(hud_values(759, false).left_w == 0x5a);
    // odd widths halve toward zero
    CHECK(hud_values(1025, false).radar_x == 512 - 50);
    // the race moves the damage panel, the clock and the weapon display 14 to the left and
    // widens the weapon display by 14
    v = hud_values(1024, true);
    CHECK(v.damage_x == 566);
    CHECK(v.clock_x == 594);
    CHECK(v.weapon_x == 654);
    CHECK(v.weapon_w == 366);
}

TEST_CASE("hud layout: the children at 1024 x 768 are the oracle's") {
    const PlayArea a = play_area_from_size(1024, 768, 1024, 768);
    const HudLayout l = hud_layout(a, hud_inputs(hud_values(a.w, false)));
    // newgame-0002: the scrollbar (562, 683) 14 x 85, IS_INFO (147, 688) 273 x 76 and
    // (673, 688) 343 x 76
    CHECK(l.scrollbar == Box{562, 683, 14, 85});
    CHECK(l.info1 == Box{147, 688, 273, 76});
    CHECK(l.info2 == Box{673, 688, 343, 76});
    CHECK(l.fill == Box{0, 683, 1024, 85});
}

TEST_CASE("hud layout: the tile counter runs 1, 2, 1, 0, 2, 1, 0, 1, 2 after its reset") {
    TileCounter c;
    c.next(1);  // seg11:0xe19(1): uses up counter value 0
    const int expect[] = {1, 2, 1, 0, 2, 1, 0, 1, 2, 1, 0, 2, 1, 0};
    for (int e : expect) CHECK(c.next(0) == e);
}

TEST_CASE("hud layout: the border tiles at 1024 x 768") {
    CHECK(tile_offset(1024) == 10);
    CHECK(tile_offset(58 * 17) == 29);  // W mod 58 = 0
    TileCounter c;
    const std::vector<Tile> t = border_tiles(1024, 683, c);
    // top row: n = 17 + 1, i = 0 .. 18; left: 10 whole tiles + 1 partial; right the same
    REQUIRE(t.size() == 19u + 11u + 11u);
    CHECK(t[0] == Tile{-10, 0, 58, 58, 1});
    CHECK(t[1] == Tile{48, 0, 58, 58, 2});
    CHECK(t[18] == Tile{18 * 58 - 10, 0, 58, 58, 2});
    CHECK(t[19] == Tile{-10, 58, 58, 58, 1});
    CHECK(t[28] == Tile{-10, 580, 58, 58, 1});
    CHECK(t[29] == Tile{-10, 638, 58, 45, 2});  // 683 = 11 * 58 + 45
    CHECK(t[30] == Tile{976, 58, 58, 58, 1});
    CHECK(t[40] == Tile{976, 638, 58, 45, 1});
    // the choice continues through the whole border: t = 1, 2, 1, 0, 2, 1, 0, ...
    const int seq[7] = {1, 2, 1, 0, 2, 1, 0};
    for (std::size_t i = 0; i < t.size(); ++i) CHECK(t[i].t == seq[i % 7]);
    // a second border starts again from the reset
    const std::vector<Tile> t2 = border_tiles(1024, 683, c);
    CHECK(t2[0].t == 1);
}

TEST_CASE("hud layout: the bevel leaves the play area x 68..955, y 78..662 at 1024 x 768") {
    const std::vector<Band> b = bevel_bands(1024, 683);
    REQUIRE(b.size() == 40u);
    // first and last of each side, in drawing order: top, bottom, left, right
    CHECK(b[0] == Band{48, 58, 928, 2, 0x028C8C8C});   // v = 140
    CHECK(b[9] == Band{66, 76, 892, 2, 0x02323232});   // i = 9: v = 50
    CHECK(b[10] == Band{48, 681, 928, 2, 0x02F0F0F0}); // v = 240
    CHECK(b[19] == Band{66, 663, 892, 2, 0x023C3C3C}); // v = 60
    CHECK(b[20] == Band{48, 58, 2, 625, 0x028C8C8C});
    CHECK(b[29] == Band{66, 76, 2, 589, 0x02323232});
    CHECK(b[30] == Band{974, 58, 2, 625, 0x02F0F0F0});
    CHECK(b[39] == Band{956, 76, 2, 589, 0x023C3C3C});
    // the bands' union frames x 68..955, y 78..662
    int inner_l = 0, inner_t = 0, inner_r = 2000, inner_b = 2000;
    for (int i = 0; i < 10; ++i) {
        inner_t = std::max(inner_t, b[i].y + b[i].h);
        inner_b = std::min(inner_b, b[10 + i].y - 1);
        inner_l = std::max(inner_l, b[20 + i].x + b[20 + i].w);
        inner_r = std::min(inner_r, b[30 + i].x - 1);
    }
    CHECK(inner_l == 68);
    CHECK(inner_r == 955);
    CHECK(inner_t == 78);
    CHECK(inner_b == 662);
}

TEST_CASE("hud layout: the clip region and the message line of seg2:0xca4") {
    const PlayArea a = play_area_from_size(1024, 768, 1024, 768);
    Clip c = clip_rect(a, true, false);
    CHECK(c.play == Rect{68, 78, 956, 663});
    CHECK(c.text_line == Box{68, 643, 888, 20});
    c = clip_rect(a, false, false);
    CHECK(c.play == Rect{0, 0, 1024, 683});
    CHECK(c.text_line == Box{0, 663, 1024, 20});
    // PlayMode adds 20 to the bottom inset
    CHECK(clip_rect(a, true, true).play == Rect{68, 78, 956, 643});
    CHECK(clip_rect(a, false, true).play == Rect{0, 0, 1024, 663});
    // the border's inset at 800: 78 - (58 - 800 mod 58) / 2 = 78 - 6 = 72
    CHECK(border_inset(800) == 72);
}

namespace {
// The index of the first call of `op` whose fields match, or -1.
template <class F>
int find(const RecordingCanvas& c, F f, int from = 0) {
    for (int i = from; i < static_cast<int>(c.calls.size()); ++i)
        if (f(c.calls[static_cast<std::size_t>(i)])) return i;
    return -1;
}
}  // namespace

TEST_CASE("hud repaint: the gate, the strip, the border and the order of the parts") {
    RecordingCanvas cv;
    FakeHost h;
    Hud hud(cv, h);
    // before seg11:0x0 there is no strip brush: only the seed and the state dispatch
    hud.repaint();
    CHECK(cv.calls.empty());
    REQUIRE(h.trace.size() == 6u);
    CHECK(h.trace[0] == "srand");
    CHECK(h.trace[1] == "seg7:0x11cc");
    CHECK(h.trace[2] == "seg7:0x1208");
    CHECK(h.trace[3] == "58c");
    CHECK(h.trace[4] == "seg7:0x1f38");
    CHECK(h.trace[5] == "seg11:0x33cc");

    hud.loaded();  // composes the panels
    cv.calls.clear();
    h.trace.clear();
    hud.repaint();
    REQUIRE(!cv.calls.empty());
    // the strip comes first: PatBlt(0, H', W, 85, PATCOPY) with METAL
    const Call& s = cv.calls[0];
    CHECK(s.op == Call::kPat);
    CHECK(s.x == 0);
    CHECK(s.y == 683);
    CHECK(s.w == 1024);
    CHECK(s.h == 85);
    CHECK(s.rop == kPatCopy);
    CHECK(s.brush.kind == Brush::Kind::kMetal);
    // then the 41 tiles from graphic 45 and the 40 bands
    for (int i = 1; i <= 41; ++i) {
        const Call& t = cv.calls[static_cast<std::size_t>(i)];
        CHECK(t.op == Call::kBlt);
        CHECK(t.src == Img::kTiles);
        CHECK(t.rop == kSrcCopy);
    }
    CHECK(cv.calls[1].x == -10);
    CHECK(cv.calls[1].sx == 58);  // t = 1
    for (int i = 42; i < 82; ++i) {
        CHECK(cv.calls[static_cast<std::size_t>(i)].op == Call::kPat);
        CHECK(cv.calls[static_cast<std::size_t>(i)].brush.kind == Brush::Kind::kSolid);
    }
    // the HUD line from (0, H') to (W, H') in silver
    const Call& line = cv.calls[82];
    CHECK(line.op == Call::kLines);
    REQUIRE(line.pts.size() == 2u);
    CHECK(line.pts[0].x == 0);
    CHECK(line.pts[0].y == 683);
    CHECK(line.pts[1].x == 1024);
    CHECK(line.pen.colour == 0x00C0C0C0);
    // then the SCORE box: PatBlt BLACKNESS (4, 695, 134, 60) and the five letters in font 0
    const Call& box = cv.calls[83];
    CHECK(box.op == Call::kPat);
    CHECK(box.rop == kBlackness);
    CHECK(box.x == 4);
    CHECK(box.y == 695);
    CHECK(box.w == 134);
    CHECK(box.h == 60);
    for (int i = 0; i < 5; ++i) {
        const Call& f = cv.calls[static_cast<std::size_t>(84 + i)];
        CHECK(f.op == Call::kFont);
        CHECK(f.style == 0);
        CHECK(f.colour == 0x000000FF);
        CHECK(f.x == 22 * i + 4 + 25);
        CHECK(f.y == 699);
        CHECK(f.text == std::string(1, "SCORE"[i]));
    }
    // the score digits "000", right-aligned at (Lw + L - 3, H' + 49)
    const Call& digits = cv.calls[89];
    CHECK(digits.op == Call::kFont);
    CHECK(digits.align == 2);
    CHECK(digits.text == "000");
    CHECK(digits.x == 135);
    CHECK(digits.y == 732);
    // the fuel background at (DGROUP:0x93bc, H'), the damage panel at (DGROUP:0x93c0, H' + 24)
    const int fuel = find(cv, [](const Call& c) { return c.op == Call::kBlt && c.src == Img::kFuelPanel; });
    REQUIRE(fuel > 0);
    CHECK(cv.calls[static_cast<std::size_t>(fuel)].x == 428);
    CHECK(cv.calls[static_cast<std::size_t>(fuel)].y == 683);
    const int dmg = find(cv, [](const Call& c) { return c.op == Call::kBlt && c.src == Img::kDamage; });
    REQUIRE(dmg > fuel);
    CHECK(cv.calls[static_cast<std::size_t>(dmg)].x == 580);
    CHECK(cv.calls[static_cast<std::size_t>(dmg)].y == 707);
    // the red button at (DGROUP:0x93c0, H' + 2) from graphic 240's cell 1 (DGROUP:0x93d8 = 1)
    const int btn = find(cv, [](const Call& c) { return c.op == Call::kBlt && c.src == Img::kButton; });
    REQUIRE(btn > dmg);
    CHECK(cv.calls[static_cast<std::size_t>(btn)].x == 580);
    CHECK(cv.calls[static_cast<std::size_t>(btn)].y == 685);
    CHECK(cv.calls[static_cast<std::size_t>(btn)].sx == 20);
    // the clock box and its colon (DIGITAL (100, 0), MERGECOPY with a green brush) at 631
    const int clock = find(cv, [](const Call& c) { return c.op == Call::kFrame && c.dst == Img::kScreen; });
    REQUIRE(clock > btn);
    CHECK(cv.calls[static_cast<std::size_t>(clock)].x == 608);
    CHECK(cv.calls[static_cast<std::size_t>(clock)].y == 684);
    CHECK(cv.calls[static_cast<std::size_t>(clock)].w == 55);
    CHECK(cv.calls[static_cast<std::size_t>(clock)].h == 17);
    const Call& colon = cv.calls[static_cast<std::size_t>(clock + 1)];
    CHECK(colon.src == Img::kDigital);
    CHECK(colon.x == 631);
    CHECK(colon.y == 687);
    CHECK(colon.sx == 100);
    CHECK(colon.rop == kMergeCopy);
    CHECK(colon.brush.colour == 0x0000FF00);
    // the state dispatch after the HUD
    REQUIRE(h.trace.size() == 6u);
    CHECK(h.trace[0] == "srand");
    CHECK(h.trace[5] == "seg11:0x33cc");
}

TEST_CASE("hud repaint: Border off, state 7, a race, DGROUP:0x9130 and DGROUP:0x951a") {
    RecordingCanvas cv;
    FakeHost h;
    Hud hud(cv, h);
    hud.loaded();
    cv.calls.clear();
    h.border_ = false;
    hud.repaint();
    // no tiles, no bands: the strip, then the HUD line
    CHECK(cv.calls[0].op == Call::kPat);
    CHECK(cv.calls[1].op == Call::kLines);
    CHECK(cv.of(Call::kBlt).front().src != Img::kTiles);

    // state 7 (the help window) draws the HUD and dispatches as state 1
    cv.calls.clear();
    h.trace.clear();
    h.state_ = 7;
    hud.repaint();
    CHECK(!cv.calls.empty());
    CHECK(h.trace.back() == "seg11:0x33cc");
    // other states: neither
    cv.calls.clear();
    h.trace.clear();
    h.state_ = 3;
    hud.repaint();
    CHECK(cv.calls.empty());
    REQUIRE(h.trace.size() == 1u);  // the seed alone
    h.state_ = 1;

    // in a race the damage panel moves to 566
    cv.calls.clear();
    h.race_ = true;
    hud.repaint();
    CHECK(hud.values().damage_x == 566);
    h.race_ = false;

    // DGROUP:0x9130: no HUD, the dispatch marks the record
    cv.calls.clear();
    h.trace.clear();
    h.demo_ = true;
    hud.repaint();
    CHECK(cv.calls.empty());
    REQUIRE(h.trace.size() == 2u);
    CHECK(h.trace[1] == "demo_mark");
    h.demo_ = false;

    // DGROUP:0x951a: the HUD, but the dispatch draws nothing
    cv.calls.clear();
    h.trace.clear();
    h.alt_ = true;
    hud.repaint();
    CHECK(!cv.calls.empty());
    REQUIRE(h.trace.size() == 1u);
}

TEST_CASE("hud repaint: the children follow the layout") {
    struct Kids : Children {
        int calls = 0;
        bool race = false;
        HudLayout last;
        void layout(const HudLayout& l, bool r) override {
            ++calls;
            last = l;
            race = r;
        }
    } kids;
    RecordingCanvas cv;
    FakeHost h;
    Hud hud(cv, h, &kids);
    hud.loaded();
    hud.repaint();
    CHECK(kids.calls == 1);
    CHECK(kids.last.scrollbar == Box{562, 683, 14, 85});
    CHECK(!kids.race);
    CHECK(hud.info().exists());
    CHECK(hud.info().dirty(kInfoTarget));
    CHECK(hud.info().dirty(kInfoWeapon));
    h.race_ = true;
    hud.repaint();
    CHECK(kids.race);
}
