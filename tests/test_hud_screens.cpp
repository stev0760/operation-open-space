// M4 stage 1 SCREENS: the message display's flash and mode switch (seg11:0x3e6e, seg7:0x119c), the
// pool-7 picture on the message display, the debriefing box (seg9:0x6dec's drawing) and the GAME
// OVER screen (seg9:0xd6c, seg9:0x6a12).
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "briefing/briefing.h"
#include "briefing/debriefing.h"
#include "doctest.h"
#include "hud/info.h"
#include "hud/screens.h"
#include "hud_fakes.h"
#include "shell/geometry.h"

using namespace is::hud;
using namespace is::shell;
using hudtest::Call;
using hudtest::FakeHost;
using hudtest::RecordingCanvas;

namespace {

struct Rig {
    RecordingCanvas cv;
    FakeHost h;
    Info info{cv, h};
    Rig() {
        const PlayArea a = play_area_from_size(1024, 768, 1024, 768);
        const HudValues v = hud_values(1024, false);
        info.layout(hud_layout(a, hud_inputs(v)), v);
        info.set_mode(kModeTarget);
        info.flush();
        cv.calls.clear();
    }
};

// A recording briefing::Canvas.
struct BCanvas : is::briefing::Canvas {
    std::vector<std::string> ops;
    void black(int x, int y, int w, int h) override {
        ops.push_back("black " + std::to_string(x) + "," + std::to_string(y) + "," + std::to_string(w) + "," +
                      std::to_string(h));
    }
    void fill(int, int, int, int, is::briefing::Colour) override { ops.push_back("fill"); }
    void line(is::briefing::Colour, int, int, int, int) override { ops.push_back("line"); }
    void rectangle(is::briefing::Colour, bool lt, int l, int t, int r, int b) override {
        ops.push_back(std::string("rect") + (lt ? " lt " : " ") + std::to_string(l) + "," + std::to_string(t) +
                      "," + std::to_string(r) + "," + std::to_string(b));
    }
    void set_pixel(int, int, is::briefing::Colour) override { ops.push_back("pixel"); }
    void text(const is::briefing::Font& f, is::briefing::Colour c, unsigned align, int x, int y,
              const std::string& s) override {
        char b[160];
        std::snprintf(b, sizeof b, "text %dx%d w%d %08x a%u %d,%d %s", f.height, f.width, f.weight, c, align, x, y,
                      s.c_str());
        ops.push_back(b);
    }
    int text_width(const is::briefing::Font&, const std::string& s) override { return static_cast<int>(s.size()); }
    void clip(bool, int, int, int, int) override { ops.push_back("clip"); }
    void icon(int, int, int, int, int, int, int, int) override {}
};

std::string lang(int id) {
    switch (id) {
    case 3003: return "WAVE %d MISSION DEBRIEFING";
    case 2238: return "PRESS 'ESC' TO CONTINUE";
    case 325: return "GAME OVER";
    case 2532: return "LAST WAVE: %d";
    case 2533: return "ICONS:";
    case 2534: return "CAPTURED: %d";
    case 2535: return "DESTROYED: %d";
    case 2536: return "SCORE: %l";
    default: return {};
    }
}

}  // namespace

TEST_CASE("hud screens: seg11:0x3e6e whitens the message display and repaints it") {
    Rig r;
    r.info.flash();
    REQUIRE(r.cv.calls.size() == 3u);
    CHECK(r.cv.calls[0].op == Call::kView);
    CHECK(r.cv.calls[1].op == Call::kPat);
    CHECK(r.cv.calls[1].rop == kWhiteness);
    CHECK(r.cv.calls[1].w == 273);
    CHECK(r.cv.calls[1].h == 76);
    CHECK(r.info.dirty(kInfoTarget));
    // mode 4 keeps the frame-rate display
    Rig q;
    q.info.set_mode(kModeFrameRate);
    q.info.flash();
    CHECK(q.cv.calls.empty());
    CHECK(!q.info.dirty(kInfoTarget));
}

TEST_CASE("hud screens: seg7:0x119c sets the mode and repaints") {
    Rig r;
    r.info.set_mode_119c(kModeMessage);
    CHECK(r.info.mode() == kModeMessage);
    CHECK(r.info.dirty(kInfoTarget));
    r.info.flush();
    // "MESSAGE from", the sender and the two yellow lines [ORACLE core-0001]
    const auto t = r.cv.of(Call::kText);
    REQUIRE(t.size() >= 4u);
    CHECK(t[0].text == "MESSAGE from");
    CHECK(t[t.size() - 2].colour == 0x0000FFFFu);
    CHECK(t[t.size() - 2].y == 0x2e);
    CHECK(t[t.size() - 1].y == 0x3c);
}

TEST_CASE("hud screens: the message display draws the ambulance's logo cell") {
    Rig r;
    CellInputs c;
    c.obj.state = 1;
    c.obj.extent = 0x140;
    c.obj.size = 40;
    c.obj.gap = 6;
    r.h.cells_[{7, 0}] = c;
    r.cv.sizes[Img::kLogoStrip] = {650, 52};
    r.info.mark(1).pool = 7;
    r.info.mark(1).rec = 0;
    r.info.set_mode_119c(kModeMessage);
    r.info.flush();
    const auto s = r.cv.of(Call::kStretch);
    REQUIRE(s.size() == 1u);
    CHECK(s[0].src == Img::kLogoStrip);
    CHECK(s[0].sx == 236);
    CHECK(s[0].sy == 6);
    CHECK(s[0].sw == 40);
    CHECK(s[0].w == 20);
}

TEST_CASE("hud screens: the debriefing box at 1024 x 768") {
    BCanvas c;
    const is::briefing::DebriefBox b = is::briefing::debrief_box(1024, 768, 1, lang);
    draw_debrief_box(c, b);
    // The two bevels and the title texts with their white copies [ORACLE core-0001]
    // (43-debriefing.png: the box at 216,196-808,564).
    int texts = 0;
    for (const std::string& o : c.ops)
        if (o.rfind("text", 0) == 0) ++texts;
    CHECK(texts == 4);
    CHECK(std::find(c.ops.begin(), c.ops.end(), "text 22x10 w600 02ffffff a0 223,200 WAVE 1 MISSION DEBRIEFING") !=
          c.ops.end());
    CHECK(std::find(c.ops.begin(), c.ops.end(), "text 22x10 w600 025a5a5a a0 221,198 WAVE 1 MISSION DEBRIEFING") !=
          c.ops.end());
    CHECK(std::find(c.ops.begin(), c.ops.end(), "text 14x0 w600 025a5a5a a2 801,205 PRESS 'ESC' TO CONTINUE") !=
          c.ops.end());
    CHECK(std::find(c.ops.begin(), c.ops.end(), "rect lt 218,198,807,220") != c.ops.end());
    debrief_blackout(c, 1024, 768);
    CHECK(c.ops.back() == "black 0,0,1024,768");
}

TEST_CASE("hud screens: GAME OVER in font 0 at 1024 x 768") {
    RecordingCanvas cv;
    const auto lines = is::briefing::game_over_lines(1024, 768, 3, 1, 0, 4900, true, lang);
    game_over_entry(cv, 1024, 768, false, lines);
    REQUIRE(cv.calls.size() == 7u);
    CHECK(cv.calls[0].op == Call::kPat);
    CHECK(cv.calls[0].rop == kBlackness);
    CHECK(cv.calls[0].w == 1024);
    CHECK(cv.calls[0].h == 768);
    const auto f = cv.of(Call::kFont);
    REQUIRE(f.size() == 6u);
    // [ORACLE core-0001] after-c0004.png: "GAME OVER" orange, "LAST WAVE: 3", "ICONS:",
    // "CAPTURED: 1", "DESTROYED: 0" green, "SCORE: 4900" red.
    CHECK(f[0].text == "GAME OVER");
    CHECK(f[0].x == 512);
    CHECK(f[0].y == 209);
    CHECK(f[0].colour == 0x020080FFu);
    CHECK(f[0].align == 6);
    CHECK(f[1].text == "LAST WAVE: 3");
    CHECK(f[1].x == 272);
    CHECK(f[1].y == 269);
    CHECK(f[3].text == "CAPTURED: 1");
    CHECK(f[3].x == 636);
    CHECK(f[3].align == 2);
    CHECK(f[4].text == "DESTROYED: 0");
    CHECK(f[4].y == 329);
    CHECK(f[5].text == "SCORE: 4900");
    CHECK(f[5].y == 449);
    CHECK(f[5].colour == 0x000000FFu);
    // With a request pending only the blackout
    RecordingCanvas q;
    game_over_entry(q, 1024, 768, true, lines);
    CHECK(q.calls.size() == 1u);
}
