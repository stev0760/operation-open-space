// The status panels (docs/re/world.md "Status panels"): the composed damage and fuel panels
// (seg11:0x344), the panel frames (seg11:0x1774), the score digits (seg11:0x4ea), the lamps
// (seg11:0x5fe) and their composition (seg11:0x3cc2), the red button (seg11:0x894), the fuel
// level (seg11:0x978), the clock (seg11:0x1ef6) and the DIGITAL text (seg11:0x2154).
#include "doctest.h"
#include "hud/hud.h"
#include "hud/panels.h"
#include "hud_fakes.h"
#include "shell/geometry.h"

using namespace is::hud;
using hudtest::Call;
using hudtest::FakeHost;
using hudtest::RecordingCanvas;

namespace {
struct Rig {
    RecordingCanvas cv;
    FakeHost h;
    Panels p{cv, h};
    Rig() { p.set_layout(is::shell::hud_values(1024, false), 683, 1024); }
};
}  // namespace

TEST_CASE("hud panels: seg11:0x344 composes the damage panel and the fuel background") {
    Rig r;
    r.p.compose();
    const auto& c = r.cv.calls;
    REQUIRE(c.size() == 1u + 1u + 8u + 1u + 2u + 1u);
    CHECK(c[0].op == Call::kCreate);
    CHECK(c[0].dst == Img::kDamage);
    CHECK(c[0].w == 84);
    CHECK(c[0].h == 61);
    CHECK(c[1].op == Call::kFrame);
    CHECK(c[1].x == 0);
    CHECK(c[1].w == 83);
    CHECK(c[1].h == 60);
    CHECK(c[1].style == 2);
    for (int k = 0; k < 4; ++k) {
        const Call& well = c[static_cast<std::size_t>(2 + 2 * k)];
        CHECK(well.op == Call::kFrame);
        CHECK(well.x == 5);
        CHECK(well.y == 14 * k + 5);
        CHECK(well.w == 28);
        CHECK(well.h == 7);
        CHECK(well.style == 3);
        const Call& label = c[static_cast<std::size_t>(3 + 2 * k)];
        CHECK(label.op == Call::kBlt);
        CHECK(label.dst == Img::kDamage);
        CHECK(label.src == Img::kSystems);
        CHECK(label.x == 41);
        CHECK(label.y == 14 * k + 5);
        CHECK(label.w == 32);
        CHECK(label.h == 11);
        CHECK(label.sy == 11 * (k + 1));
    }
    CHECK(c[10].dst == Img::kFuelPanel);
    CHECK(c[10].w == 30);
    CHECK(c[10].h == 85);
    CHECK(c[11].w == 29);
    CHECK(c[11].h == 84);
    CHECK(c[12].x == 6);
    CHECK(c[12].y == 18);
    CHECK(c[12].w == 16);
    CHECK(c[12].h == 62);
    CHECK(c[13].src == Img::kSystems);  // "Fuel", SYSTEMS row 0, at (5, 5) 21 x 11
    CHECK(c[13].x == 5);
    CHECK(c[13].w == 21);
    CHECK(c[13].sy == 0);
}

TEST_CASE("hud panels: seg11:0x1774 frames the target display at 1024") {
    Rig r;
    r.p.panel_frame(2, 142, 683, 424, 767);
    const auto lines = r.cv.of(Call::kLines);
    REQUIRE(lines.size() == 7u);
    // grey bottom edge, grey inner bottom-right, grey inset top-left at n + 2
    CHECK(lines[0].pen.colour == 0x02808080);
    CHECK(lines[0].pts[0].x == 142);
    CHECK(lines[0].pts[0].y == 767);
    CHECK(lines[0].pts[1].x == 424);
    CHECK(lines[2].pts[0].x == 146);
    CHECK(lines[2].pts[0].y == 762);
    CHECK(lines[2].pts[1].y == 687);
    CHECK(lines[2].pts[2].x == 420);
    // the right edge in BLACK_PEN for styles other than 4
    CHECK(lines[3].pen.is_stock);
    CHECK(lines[3].pen.stock == kBlackPen);
    // white: the inset bottom-right, then the outer and inner top-left
    CHECK(lines[4].pen.stock == kWhitePen);
    CHECK(lines[4].pts[2].y == 686);
    CHECK(lines[5].pts[1].x == 142);
    CHECK(lines[5].pts[1].y == 683);
    const auto rects = r.cv.of(Call::kRect);
    REQUIRE(rects.size() == 2u);
    CHECK(rects[0].x == 144);
    CHECK(rects[0].y == 685);
    CHECK(rects[0].w == 423);
    CHECK(rects[0].h == 766);
    CHECK(rects[1].x == 145);
    CHECK(rects[0].brush.stock == kNullBrush);
    CHECK(rects[0].pen.colour == 0x02C0C0C0);
    const auto px = r.cv.of(Call::kPixel);
    REQUIRE(px.size() == 4u);
    CHECK(px[0].x == 424);
    CHECK(px[0].y == 683);
    CHECK(px[0].colour == 0x02808080);
    CHECK(px[3].x == 143);
    CHECK(px[3].y == 766);
    // style 4 (the radar) draws its right edge grey and four rectangles; style -1 fills them
    r.cv.calls.clear();
    r.p.panel_frame(4, 462, 683, 561, 767);
    CHECK(r.cv.of(Call::kLines)[3].pen.colour == 0x02808080);
    CHECK(r.cv.of(Call::kRect).size() == 4u);
    r.cv.calls.clear();
    r.p.panel_frame(-1, 0, 0, 50, 50);
    CHECK(r.cv.of(Call::kRect).size() == 2u);
    CHECK(r.cv.of(Call::kRect)[0].brush.stock == kLtGrayBrush);
}

TEST_CASE("hud panels: the score digits redraw only when the score changes") {
    Rig r;
    r.p.state().score_shown = 0xffffffffu;
    r.p.score_digits();
    REQUIRE(r.cv.calls.size() == 1u);
    CHECK(r.cv.calls[0].text == "000");
    CHECK(r.cv.calls[0].colour == 0x0200A0FF);  // orange on an 8-bit display
    r.p.score_digits();
    CHECK(r.cv.calls.size() == 1u);
    r.h.score_ = 4000;
    r.p.score_digits();
    REQUIRE(r.cv.calls.size() == 2u);
    CHECK(r.cv.calls[1].text == "4000");
    CHECK(r.cv.calls[1].style == 0);
    CHECK(r.cv.calls[1].align == 2);
    // the narrow layouts: font 1 below 760, the DIGITAL text below 660
    r.p.set_layout(is::shell::hud_values(700, false), 683, 700);
    r.h.score_ = 5;
    r.p.score_digits();
    CHECK(r.cv.calls.back().op == Call::kFont);
    CHECK(r.cv.calls.back().style == 1);
    CHECK(r.cv.calls.back().x == 0x5a + 1 - 1);
}

TEST_CASE("hud panels: seg11:0x2154 draws digits and the letters of SCORE from DIGITAL") {
    Rig r;
    r.p.small_text(0, 0x000000FF, 10, 20, "SCORE 9");
    const auto& c = r.cv.calls;
    REQUIRE(c.size() == 7u);
    const int cells[] = {5, 11, 0, 12, 13};
    for (int i = 0; i < 5; ++i) {
        CHECK(c[static_cast<std::size_t>(i)].op == Call::kBlt);
        CHECK(c[static_cast<std::size_t>(i)].src == Img::kDigital);
        CHECK(c[static_cast<std::size_t>(i)].x == 10 + 10 * i);
        CHECK(c[static_cast<std::size_t>(i)].sx == 10 * cells[i]);
        CHECK(c[static_cast<std::size_t>(i)].rop == kMergeCopy746);
        CHECK(c[static_cast<std::size_t>(i)].brush.colour == 0x000000FF);
    }
    CHECK(c[5].op == Call::kPat);  // the space: a black cell
    CHECK(c[5].rop == kBlackness);
    CHECK(c[6].sx == 90);
    // right alignment (2) and centring (6)
    r.cv.calls.clear();
    r.p.small_text(2, 0, 100, 0, "12");
    CHECK(r.cv.calls[0].x == 80);
    r.cv.calls.clear();
    r.p.small_text(6, 0, 100, 0, "123");
    CHECK(r.cv.calls[0].x == 85);
}

TEST_CASE("hud panels: the lamps follow the systems' health") {
    Rig r;
    r.h.ship_.health[0] = 100;  // > 75: green, row 0
    r.h.ship_.health[1] = 60;   // > 50: row 1
    r.h.ship_.health[2] = 30;   // > 25: row 2
    r.h.ship_.health[3] = 0;    // dead: LTGRAY
    r.p.lamps(1);
    const auto& c = r.cv.calls;
    REQUIRE(c.size() == 4u);
    CHECK(c[0].op == Call::kBlt);
    CHECK(c[0].src == Img::kLamps);
    CHECK(c[0].x == 586);
    CHECK(c[0].y == 713);
    CHECK(c[0].w == 27);
    CHECK(c[0].h == 6);
    CHECK(c[0].sy == 0);
    CHECK(c[1].sy == 6);
    CHECK(c[1].y == 727);
    CHECK(c[2].sy == 12);
    CHECK(c[3].op == Call::kPat);
    CHECK(c[3].brush.stock == kLtGrayBrush);
    CHECK(r.p.state().lamp_shown[3] == 0x2a);
    // unchanged lamps do not redraw without force
    r.cv.calls.clear();
    r.p.lamps(0);
    CHECK(r.cv.calls.empty());
    // 1..25 blinks: redrawn every call, red with DGROUP:0x93fe, grey without
    r.h.ship_.health[0] = 10;
    r.p.lamps(0);
    REQUIRE(r.cv.calls.size() == 1u);
    CHECK(r.cv.calls[0].op == Call::kPat);
    r.h.blink_ = true;
    r.p.lamps(0);
    CHECK(r.cv.calls.back().op == Call::kBlt);
    CHECK(r.cv.calls.back().sy == 12);
    // a design without the system (-1) shows grey
    r.cv.calls.clear();
    r.h.ship_.lamp_row[1] = -1;
    r.p.lamps(0);
    CHECK(r.p.state().lamp_shown[1] == 0x2a);
}

TEST_CASE("hud panels: the lamps' status messages wait for the wave's fourth second") {
    Rig r;
    r.p.lamps(1);  // all green
    r.h.ship_.health[1] = 0;
    r.p.state().seconds = 3;
    r.p.lamps(0);
    CHECK(r.h.messages.empty());
    r.h.ship_.health[1] = 100;
    r.p.lamps(0);
    r.h.ship_.health[1] = 0;
    r.h.ship_.health[2] = 0;
    r.h.ship_.health[0] = 20;  // 0x1e on lamp 0: message 0xa, and the others stay quiet
    r.p.state().seconds = 10;
    r.p.lamps(0);
    REQUIRE(r.h.messages.size() == 1u);
    CHECK(r.h.messages[0].second == 0xa);
    // the gun dead alone: message 0xb; the thrust: 0xc
    Rig q;
    q.p.lamps(1);
    q.p.state().seconds = 10;
    q.h.ship_.health[1] = 0;
    q.h.ship_.health[2] = 0;
    q.p.lamps(0);
    REQUIRE(q.h.messages.size() == 2u);
    CHECK(q.h.messages[0].second == 0xb);
    CHECK(q.h.messages[1].second == 0xc);
    // repaired, on the player's own screen: 0x52 and 0x53
    q.h.ship_.health[1] = 100;
    q.h.ship_.health[2] = 100;
    q.p.lamps(0);
    REQUIRE(q.h.messages.size() == 4u);
    CHECK(q.h.messages[2].second == 0x52);
    CHECK(q.h.messages[3].second == 0x53);
}

TEST_CASE("hud panels: the lamp composition of seg11:0x3cc2") {
    Rig r;
    r.h.ship_.lamp_row[0] = -1;
    r.h.ship_.b_e9 = true;
    r.p.compose_lamps(0);
    const auto& c = r.cv.calls;
    REQUIRE(c.size() == 4u);
    CHECK(c[0].op == Call::kPat);
    CHECK(c[0].dst == Img::kDamage);
    CHECK(c[0].x == 41);
    CHECK(c[0].y == 5);
    CHECK(c[1].sy == 11 * 2);  // row 1
    CHECK(c[3].sy == 11 * 6);  // byte +0xe9: 5
    r.cv.calls.clear();
    r.h.ship_.b_e9 = false;
    r.h.ship_.b_e7 = true;
    r.p.compose_lamps(0);
    CHECK(r.cv.calls[3].sy == 11 * 5);  // byte +0xe7: 4
}

TEST_CASE("hud panels: the red button blinks only for the watched player") {
    Rig r;
    r.p.state().button = 1;
    r.p.button();
    REQUIRE(r.cv.calls.size() == 1u);
    CHECK(r.cv.calls[0].src == Img::kButton);
    CHECK(r.cv.calls[0].x == 580);
    CHECK(r.cv.calls[0].y == 685);
    CHECK(r.cv.calls[0].sx == 20);
    CHECK(r.p.state().button_due == 1300);
    // not due: nothing
    r.h.now_ = 1299;
    r.p.button();
    CHECK(r.cv.calls.size() == 1u);
    // due, but unchanged
    r.h.now_ = 1300;
    r.p.button();
    CHECK(r.cv.calls.size() == 1u);
    // the player is DGROUP:0x952e with DGROUP:0x9e49 = 0: the cell toggles every 300 ms
    r.h.watch_ = 0;
    r.h.now_ = 1600;
    r.p.button();
    REQUIRE(r.cv.calls.size() == 2u);
    CHECK(r.cv.calls[1].sx == 0);
    r.h.now_ = 1900;
    r.p.button();
    CHECK(r.cv.calls[2].sx == 20);
}

TEST_CASE("hud panels: the fuel level and its warnings") {
    Rig r;
    r.h.ship_.fuel_max = 660;  // 11 litres a step
    r.h.ship_.fuel = 660;
    r.p.state().fuel_shown = -1;
    r.p.fuel();
    REQUIRE(r.cv.calls.size() == 2u);
    CHECK(r.p.state().fuel_shown == 59);  // 60 capped at 59
    CHECK(r.cv.calls[0].op == Call::kPat);
    CHECK(r.cv.calls[0].x == 436);
    CHECK(r.cv.calls[0].y == 702);
    CHECK(r.cv.calls[0].w == 14);
    CHECK(r.cv.calls[0].h == 1);
    CHECK(r.cv.calls[1].src == Img::kFuelBar);
    CHECK(r.cv.calls[1].y == 683 - 59 + 79);
    CHECK(r.cv.calls[1].w == 13);
    CHECK(r.cv.calls[1].h == 59);
    CHECK(r.cv.calls[1].sy == 1);
    // the same level draws nothing
    r.p.fuel();
    CHECK(r.cv.calls.size() == 2u);
    // crossing 250 litres raises message 9, then not again for 30 s
    r.h.ship_.fuel = 240;
    r.h.now_ = 5000;
    r.p.fuel();
    REQUIRE(r.h.messages.size() == 1u);
    CHECK(r.h.messages[0].second == 9);
    CHECK(r.p.state().fuel_warn_until == 35000);
    r.h.ship_.fuel = 180;
    r.p.fuel();
    CHECK(r.h.messages.size() == 1u);
    r.h.now_ = 35001;
    r.h.ship_.fuel = 120;
    r.p.fuel();
    CHECK(r.h.messages.size() == 2u);
}

TEST_CASE("hud panels: the clock counts the wave's frames at 36 a second") {
    Rig r;
    r.h.ticks_ = 36 * 66;  // 1:06
    r.p.clock(1, true);
    CHECK(r.p.state().seconds == 66);
    const auto& c = r.cv.calls;
    REQUIRE(c.size() == 4u);
    CHECK(c[0].op == Call::kPat);  // " 1": the blank minute digit
    CHECK(c[0].x == 613);
    CHECK(c[0].y == 687);
    CHECK(c[1].src == Img::kDigital);
    CHECK(c[1].x == 623);
    CHECK(c[1].sx == 10);
    CHECK(c[1].rop == kMergeCopy746);
    CHECK(c[1].brush.colour == 0x0000FF00);
    CHECK(c[2].x == 638);  // "06"
    CHECK(c[2].sx == 0);
    CHECK(c[2].rop == kMergeCopy);
    CHECK(c[3].x == 648);
    CHECK(c[3].sx == 60);
    // the same second draws nothing; a new one draws
    r.cv.calls.clear();
    r.p.clock(0, true);
    CHECK(r.cv.calls.empty());
    r.h.ticks_ += 36;
    r.p.clock(0, true);
    CHECK(r.cv.calls.size() == 4u);
    // the gate stops it
    r.cv.calls.clear();
    r.h.ticks_ += 36;
    r.p.clock(1, false);
    CHECK(r.cv.calls.empty());
}

TEST_CASE("hud panels: seg11:0x4a0 dispatches the game's updates") {
    Rig r;
    r.p.update(0, true);  // the score (0 vs the file's 0: nothing)
    CHECK(r.cv.calls.empty());
    r.h.score_ = 1;
    r.p.update(0, false);
    CHECK(r.cv.calls.empty());
    r.p.update(0, true);
    CHECK(r.cv.calls.size() == 1u);
    r.p.update(7, true);  // the clock: second 0 is the one shown
    CHECK(r.cv.calls.size() == 1u);
    r.h.ticks_ = 36;
    r.p.update(7, true);  // " 0" and "01"
    CHECK(r.cv.calls.size() == 1u + 4u);
}
