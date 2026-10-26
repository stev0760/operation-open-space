// State 3, the briefing (src/briefing/briefing.h): the box geometry, the labels, the text
// selection, the generated text, the typers, the phase table and the timeline on a fake clock,
// with a recording canvas (docs/re/shell.md "Screen states", State 3).
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "briefing/briefing.h"
#include "doctest.h"
#include "shell/game_state.h"

using namespace is::briefing;
using is::shell::GameState;
using is::shell::Tick;

namespace {

class FakeCanvas : public Canvas {
public:
    std::vector<std::string> ops;
    void put(const char* fmt, ...) {
        char buf[256];
        va_list a;
        va_start(a, fmt);
        std::vsnprintf(buf, sizeof buf, fmt, a);
        va_end(a);
        ops.emplace_back(buf);
    }
    int count(const std::string& p) const {
        int n = 0;
        for (const auto& s : ops)
            if (s.compare(0, p.size(), p) == 0) ++n;
        return n;
    }
    std::vector<std::string> with(const std::string& p) const {
        std::vector<std::string> r;
        for (const auto& s : ops)
            if (s.compare(0, p.size(), p) == 0) r.push_back(s);
        return r;
    }
    void black(int x, int y, int w, int h) override { put("black %d %d %d %d", x, y, w, h); }
    void fill(int x, int y, int w, int h, Colour c) override {
        put("fill %d %d %d %d %08x", x, y, w, h, c);
    }
    void line(Colour p, int x0, int y0, int x1, int y1) override {
        put("line %08x %d %d %d %d", p, x0, y0, x1, y1);
    }
    void rectangle(Colour p, bool lt, int l, int t, int r, int b) override {
        put("rect %08x %d %d %d %d %d", p, lt ? 1 : 0, l, t, r, b);
    }
    void set_pixel(int x, int y, Colour c) override { put("pixel %d %d %08x", x, y, c); }
    void text(const Font& f, Colour c, unsigned align, int x, int y,
              const std::string& s) override {
        put("text %d/%d/%d %08x %u %d %d %s", f.height, f.width, f.weight, c, align, x, y,
            s.c_str());
    }
    int text_width(const Font&, const std::string& s) override {
        return 10 * static_cast<int>(s.size());
    }
    void clip(bool on, int l, int t, int r, int b) override {
        if (on) put("clip %d %d %d %d", l, t, r, b);
    }
    void icon(int x, int y, int w, int h, int sx, int sy, int sw, int sh) override {
        put("icon %d %d %d %d %d %d %d %d", x, y, w, h, sx, sy, sw, sh);
    }
};

class FakeHost : public Host {
public:
    std::map<int, std::string> strings = {
        {2537, "WAVE %d MISSION BRIEFING"}, {2268, "PRESS 'ESC' TO START"}, {2316, "%#"},
        {2515, "ICON"}, {12515, "ICONS"}, {2538, "SHIP IS"}, {12538, "SHIPS ARE"},
        {3036, "WATCH OUT FOR THE %<%D%<%D%<%D%> IN THIS WAVE."},
        {3037, "%# OUT TO GET YOU."}, {1189, "CONCENTRATE ON GETTING THE ICONS."},
        {1190, "THIS DIRECTORY HAS FALLEN ILL WITH VIRUSES!"},
        {1998, "THE ICONS HAVE BEEN INFECTED BY VIRUSES!"}, {10153, "Rocks"},
        {10170, "Viruses"}, {797, " AND"}};
    std::map<int, std::string> pages = {{300, "IF YOU CAPTURE AN ICON"}};
    std::vector<std::string> todos;
    std::vector<int> sounds;
    std::string lang(int id) override {
        auto it = strings.find(id);
        return it == strings.end() ? std::string() : it->second;
    }
    void sound(int id) override { sounds.push_back(id); }
    void todo(const std::string& w, const char* a) override { todos.push_back(w + " " + a); }
    std::vector<std::uint8_t> page(int id) override {
        auto it = pages.find(id);
        if (it == pages.end()) return {};
        std::vector<std::uint8_t> v(it->second.begin(), it->second.end());
        v.push_back(0);
        v.push_back('X');  // after the NUL: never read
        return v;
    }
};

// GameState with the briefing's hooks, ticking every 14 ms (the frame rate of the headless
// runs) with a frame in every pass.
struct Rig {
    GameState s;
    FakeCanvas c;
    FakeHost h;
    Briefing b{c, h};
    Setup setup;
    std::string text = "ENTER INNER SPACE";
    Tick t = 1000;
    Rig() {
        setup.W = 1024;
        setup.H = 768;
        setup.directory = "D:\\GAMES\\";
        auto& hk = s.hooks();
        hk.enter[is::shell::kBriefing] = [this](GameState& g) { b.enter_text(g, setup, text); };
        hk.frame[is::shell::kBriefing] = [this](GameState& g) { b.frame(g); };
        hk.repaint[is::shell::kBriefing] = [this](GameState& g) { b.repaint(g); };
    }
    void start() {
        s.request(is::shell::kWave);
        s.change_step(t);  // state 1 -> 3 in the same step (seg2:0x1b96)
    }
    void tick(Tick dt = 14) {
        t += dt;
        if (s.pass(t)) s.frame();
    }
    // Ticks until the phase is p (or the state leaves 3); false after `max` ticks.
    bool until_phase(int p, int max = 5000) {
        for (int i = 0; i < max; ++i) {
            if (s.state() == is::shell::kBriefing && s.briefing_phase() == p) return true;
            tick();
        }
        return false;
    }
    void key_up(std::uint32_t vk) {
        s.post_input({is::shell::kWmKeyUp, vk, 0});
    }
};

}  // namespace

TEST_CASE("briefing: box geometry at 1024x768 and 640x480") {
    // shell.md "Box": at 1024x768 L 202, R 822, T 154, Y 434, bottom 614, page (208, 165) -
    // (816, 443) [ORACLE newgame-0002].
    Box b = box_for(1024, 768);
    CHECK(b.L == 202);
    CHECK(b.R == 822);
    CHECK(b.T == 154);
    CHECK(b.Y == 434);
    CHECK(b.bottom == 614);
    CHECK(b.cL == 202);
    CHECK(b.cR == 822);
    CHECK(b.pl == 208);
    CHECK(b.pt == 165);
    CHECK(b.pr == 816);
    CHECK(b.pb == 443);

    // 640x480: bw 620, bh 460, T = 10 raised to 15, Y from the unclamped T.
    b = box_for(640, 480);
    CHECK(b.L == 10);
    CHECK(b.R == 630);
    CHECK(b.T == 15);
    CHECK(b.Y == 290);
    CHECK(b.bottom == 470);
    CHECK(b.pl == 16);
    CHECK(b.pt == 26);
    CHECK(b.pr == 624);
    CHECK(b.pb == 299);

    // Narrower than 640: bw = W + 20, so cL and cR clip to the client; H = 400 is not raised.
    b = box_for(600, 400);
    CHECK(b.L == -10);
    CHECK(b.R == 610);
    CHECK(b.cL == 0);
    CHECK(b.cR == 600);
    CHECK(b.T == -10);
    CHECK(b.Y == 230);

    const auto v = box_bevels(box_for(1024, 768));
    REQUIRE(v.size() == 3);
    CHECK(v[0].width == 2);
    CHECK(v[0].t == 139);
    CHECK(v[0].b == 614);
    CHECK(v[1].width == -1);
    CHECK(v[1].b == 164);
    CHECK(v[2].t == 454);
    CHECK(v[2].b == 474);
}

TEST_CASE("briefing: the labels") {
    FakeHost h;
    const Box b = box_for(1024, 768);
    auto v = box_labels(h, b, 1, "D:\\GAMES\\", 3);
    REQUIRE(v.size() == 4);
    CHECK(v[0].text == "WAVE 1 MISSION BRIEFING");
    CHECK(v[0].x == 207);
    CHECK(v[0].y == 141);
    CHECK(v[0].height == 22);
    CHECK(v[0].width == 10);
    CHECK(v[1].text == "PRESS 'ESC' TO START");
    CHECK(v[1].x == 815);
    CHECK(v[1].y == 148);
    CHECK(v[1].align == kTaRight);
    CHECK(v[2].text == "D:\\GAMES\\");
    CHECK(v[2].y == 456);
    CHECK(v[3].text == "3 ICONS");
    CHECK(v[3].x == 817);
    CHECK(box_labels(h, b, 2, "C:\\", 1)[3].text == "1 ICON");
    CHECK(box_labels(h, b, 2, "C:\\", 0).size() == 3);  // no count when DGROUP:0x91fe = 0

    // seg9:0x8bec: highlight at +2 in white, then the face in 0x5A5A5A; 13 becomes 14 bare.
    FakeCanvas c;
    draw_label(c, v[0]);
    REQUIRE(c.ops.size() == 2);
    CHECK(c.ops[0] == "text 22/10/600 02ffffff 0 209 143 WAVE 1 MISSION BRIEFING");
    CHECK(c.ops[1] == "text 22/10/600 025a5a5a 0 207 141 WAVE 1 MISSION BRIEFING");
    c.ops.clear();
    draw_label(c, Label{5, 6, 13, 0, kTaLeft, "X"});
    REQUIRE(c.ops.size() == 1);
    CHECK(c.ops[0] == "text 14/0/600 025a5a5a 0 5 6 X");
}

TEST_CASE("briefing: the bevel seg11:0x1774") {
    FakeCanvas c;
    draw_bevel(c, -1, 0, 0, 100, 20);  // a filled bar: width 2 with LTGRAY_BRUSH
    CHECK(c.count("line ") == 12);
    const auto r = c.with("rect ");
    REQUIRE(r.size() == 2);
    CHECK(r[0] == "rect 02c0c0c0 1 2 2 99 19");
    CHECK(r[1] == "rect 02c0c0c0 1 3 3 98 18");
    CHECK(c.with("pixel ").size() == 4);
    CHECK(c.ops[5] == "line 00000000 100 20 100 0");  // the stock BLACK_PEN on the right edge
    c.ops.clear();
    draw_bevel(c, 2, 0, 0, 100, 50);
    CHECK(c.with("rect ")[0] == "rect 02c0c0c0 0 2 2 99 49");
}

TEST_CASE("briefing: text selection and the generated text") {
    WaveFlags f;
    CHECK(text_resource(f, 0) == 101);
    CHECK(text_resource(f, 3) == 0);
    f.f950 = 1;
    CHECK(text_resource(f, 3) == 104);
    f.f94e = 1;
    CHECK(text_resource(f, 3) == 103);
    f.f92e8 = 1;
    CHECK(text_resource(f, 0) == 102);

    FakeHost h;
    GeneratedInputs in;
    GeneratedPlan p = generated_plan(in);
    CHECK(p.lead == 1189);
    CHECK(p.body == 1189);
    CHECK(p.slots.empty());
    CHECK(generated_text(h, p, nullptr) ==
          "CONCENTRATE ON GETTING THE ICONS. CONCENTRATE ON GETTING THE ICONS.");

    // Rocks: DGROUP:0x933c > 15 gives slot (3, 0); kind -1 has no lead and no space.
    in.kind = -1;
    in.v933c = 20;
    p = generated_plan(in);
    CHECK(p.lead == 0);
    CHECK_FALSE(p.lead_space);
    REQUIRE(p.slots.size() == 1);
    CHECK(p.slots[0].type == 3);
    CHECK(generated_text(h, p, nullptr) == "WATCH OUT FOR THE ROCKS IN THIS WAVE.");
    // DGROUP:0x933a moves into DGROUP:0x933c for the test.
    in.v933c = 0;
    in.v933a = 1;
    CHECK(generated_plan(in).slots.size() == 1);

    // Viruses (category 3) outrank, rocks follow; slots print in the order 2, 1, 0.
    in = GeneratedInputs{};
    in.kind = 2;
    in.byte53 = 2;
    in.v9340 = 5;
    in.v933c = 20;
    in.roster_count = 2;
    p = generated_plan(in);
    CHECK(p.lead == 1998);
    REQUIRE(p.slots.size() == 2);
    CHECK(p.slots[0].type == 3);  // slot 1: rocks
    CHECK(p.slots[1].type == 9);  // slot 0: viruses
    CHECK(generated_text(h, p, nullptr) ==
          "THE ICONS HAVE BEEN INFECTED BY VIRUSES! WATCH OUT FOR THE ROCKS AND VIRUSES IN THIS "
          "WAVE.2 SHIPS ARE OUT TO GET YOU.");

    // Type 8 names a ship class (M3): flagged.
    in = GeneratedInputs{};
    in.v933e = 4;
    in.v9318 = 1;
    p = generated_plan(in);
    REQUIRE(p.slots.size() == 1);
    CHECK(p.slots[0].type == 8);
    CHECK(p.slots[0].arg == 2);
    bool todo = false;
    generated_text(h, p, &todo);
    CHECK(todo);

    // Three filled slots: rocks replace slot 2 only when it is not viruses and DGROUP:0x933c
    // beats its value and is above 15.
    in = GeneratedInputs{};
    in.v933e = 9;
    in.v9340 = 8;
    in.v4ee0 = 7;
    in.v933c = 16;
    p = generated_plan(in);
    REQUIRE(p.slots.size() == 3);
    CHECK(p.slots[0].type == 3);  // slot 2 became rocks
    in.v4ee0 = 0;
    in.v4ee2 = 0;
    in.v9340 = 0;
}

TEST_CASE("briefing: pages") {
    CHECK(page_resource(1, 0, 0) == 300);
    CHECK(page_resource(4, 0, 0) == 302);
    CHECK(page_resource(5, 0, 0) == 303);
    CHECK(page_resource(6, 0, 0) == 304);
    CHECK(page_resource(7, 0, 0) == 305);
    CHECK(page_resource(3, 0, 0) == 301);
    CHECK(page_resource(3, 3, 0) == 180);
    CHECK(page_resource(3, 9, 0) == 3);
    CHECK(page_resource(15, 0, 2) == 322);
    CHECK(page_resource(17, 0, 0) == 137);
    CHECK(page_resource(402, 0, 0) == 402);
    CHECK(page_resource(8, 0, 0) == 0);
    CHECK(page_resource(406, 0, 0) == 0);
    for (int p : {2, 3, 8, 9, 10, 11, 12, 13, 15, 16, 17}) CHECK(page_wraps(p));
    for (int p : {0, 1, 4, 5, 6, 7, 14, 18}) CHECK_FALSE(page_wraps(p));
    CHECK(slide_in_frames(3) == 150);
    CHECK(slide_in_frames(7) == 190);
    CHECK(slide_in_frames(8) == 112);
    CHECK(slide_in_frames(9) == 102);
}

TEST_CASE("briefing: the typewriter's steps") {
    FakeCanvas c;
    FakeHost h;
    Typewriter tw(c, h);
    tw.set_text("AB CD");
    tw.set_panel(202, 434, 822, 614, 816);
    CHECK(tw.step(1000, 1, 0) == 0);  // reset: x L + 25, y Y + 55, the first step at now + 500
    CHECK(tw.x() == 227);
    CHECK(tw.y() == 489);
    CHECK(tw.step(1499, 0, 0) == 0);
    CHECK(c.count("text ") == 0);
    CHECK(tw.step(1500, 0, 0) == 0);  // two characters
    CHECK(c.count("text ") == 2);
    CHECK(c.ops.back() == "fill 251 491 12 15 0000ffff");  // the cursor at (x + 4, y + 2)
    CHECK(tw.step(1549, 0, 0) == 0);  // behind the 50 ms gate
    CHECK(tw.step(1550, 0, 0) == 0);  // the space and C
    CHECK(tw.step(1600, 0, 0) == 1);  // D, then NUL: done
    CHECK(c.count("text ") == 5);
    CHECK(h.sounds.size() == 5);
    // Done: the cursor blinks every 400 ms, black first.
    c.ops.clear();
    CHECK(tw.step(1650, 0, 0) == 1);
    CHECK(c.ops.back().compare(0, 6, "black ") == 0);
    CHECK(tw.step(1700, 0, 0) == 1);  // behind the 400 ms gate
    CHECK(tw.step(2050, 0, 0) == 1);
    CHECK(c.ops.back().compare(0, 5, "fill ") == 0);

    // Wrap: at a space, edge - m < x + width of (unknown byte + next word).
    FakeCanvas c2;
    Typewriter w(c2, h);
    w.set_text("AAAA BBBBBB\r\nC");
    w.set_panel(0, 0, 200, 180, 100);  // x starts at 25
    w.step(0, 1, 0);
    w.step(0, 0, 1);  // instant
    // AAAA ends at x 65; at the space 100 - 9 < 65 + 70: wrap, the space is not drawn.
    const auto t = c2.with("text ");
    REQUIRE(t.size() >= 6);
    CHECK(t[4] == "text 17/11/520 0000ffff 0 25 80 B");
    // CR LF starts the line at y 105.
    CHECK(c2.with("text ").back() == "text 17/11/520 0000ffff 0 25 105 C");
    CHECK(w.done());
    // LF followed by CR (a blank line) pauses 2000 ms, also when instant.
    FakeCanvas c3;
    Typewriter q(c3, h);
    q.set_text("A\r\n\r\nB");
    q.set_panel(0, 0, 200, 180, 100);
    q.step(0, 1, 0);
    CHECK(q.step(0, 0, 1) == 0);
    CHECK(c3.count("text ") == 1);
    CHECK(q.step(1999, 0, 0) == 0);
    CHECK(q.step(2000, 0, 0) == 1);
    CHECK(c3.with("text ").back() == "text 17/11/520 0000ffff 0 25 105 B");
}

TEST_CASE("briefing: the page typer") {
    FakeCanvas c;
    FakeHost h;
    PageTyper p(c, h);
    p.set_page("A\t050B\r\n\r\nC", false, 3000);
    p.set_origin(247, 218, 816);
    CHECK(p.step(1000, 1, 0, 1) == 0);  // reset, not instant: nothing drawn
    CHECK(c.count("text ") == 0);
    CHECK(p.step(1499, 0, 0, 1) == 0);
    CHECK(p.step(1500, 0, 0, 1) == 0);  // A, TAB 050, B, CR LF; then LF CR pauses 900 ms
    const auto t = c.with("text ");
    REQUIRE(t.size() == 2);
    CHECK(t[0] == "text 13/6/0 00ffff00 0 247 218 A");
    CHECK(t[1] == "text 13/6/0 00ffff00 0 297 218 B");
    CHECK(p.step(1600, 0, 0, 1) == 0);  // the pause
    CHECK(p.step(2400, 0, 0, 1) == 1);  // CR LF, C, NUL: done
    CHECK(c.with("text ").back() == "text 13/6/0 00ffff00 0 247 254 C");
    CHECK(p.step(2449, 0, 0, 1) == 0);  // the 50 ms gate of the last step
    CHECK(p.step(2450, 0, 0, 1) == 1);  // the hold is set: until 5450; the gate is now 400 ms
    CHECK(p.step(2849, 0, 0, 1) == 0);
    CHECK(p.step(5450, 0, 0, 1) == 1);
    CHECK(p.step(5851, 0, 0, 1) == 2);
    // Phases 15 and 17 do not pause.
    FakeCanvas c2;
    PageTyper q(c2, h);
    q.set_page("A\r\n\r\nC", false, 0);
    q.set_origin(0, 0, 816);
    q.step(0, 1, 0, 15);
    CHECK(q.step(500, 0, 0, 15) == 1);
}

TEST_CASE("briefing: entry, phase 0 and the advance to phase 1") {
    Rig r;
    r.start();
    CHECK(r.s.state() == is::shell::kBriefing);
    CHECK(r.b.wave() == 1);  // seg9:0x5fbf
    CHECK(r.s.briefing_phase() == 0);
    // Three bevels and four labels (no count without icons: three labels).
    CHECK(r.c.count("rect ") == 2 + 2 + 2);
    CHECK(r.c.count("text 22/10/600") == 2);
    // The text types from entry + 500 ms, two characters per 50 ms.
    const Tick t0 = r.t;
    while (!r.b.typewriter().done()) r.tick();
    CHECK(r.t - t0 >= 500 + 8 * 50);
    CHECK(r.t - t0 < 500 + 9 * 56 + 14);
    // Phase 1 comes more than 500 ms after the text is done.
    const Tick done = r.t;
    REQUIRE(r.until_phase(1));
    CHECK(r.t - done > 500);
    CHECK(r.t - done <= 500 + 28);
    r.tick();
    CHECK(r.c.count("clip 208 165 816 443") > 0);
}

TEST_CASE("briefing: the phase cycle of a player game, ESC and the hand-off") {
    Rig r;
    r.start();
    REQUIRE(r.until_phase(1));
    // No icons: phase 1 ends at the next frame; 2-4 pass one per frame; 5 holds 190 frames
    // (slide-in kind 7, wave 1).
    REQUIRE(r.until_phase(5));
    int frames = 0;
    while (r.s.briefing_phase() == 5) {
        r.tick();
        ++frames;
    }
    CHECK(frames == 190);
    // 11 (the ship page) holds one frame; 17 turns into 1 when PlayMode is 0.
    REQUIRE(r.until_phase(17));
    r.tick();
    CHECK(r.s.briefing_phase() == 1);
    // ESC: phase 18 at the pass, DGROUP:0x5ac at the frame, state 1 at the next pass.
    bool wave_entered = false;
    r.s.hooks().enter[is::shell::kWave] = [&](GameState&) { wave_entered = true; };
    r.key_up(0x1b);
    r.tick();
    CHECK(r.s.briefing_phase() == 18);
    CHECK(r.s.deadlines().briefing_end == r.t);
    r.tick();
    CHECK(r.s.state() == is::shell::kWave);
    CHECK(wave_entered);
    CHECK(r.s.deadlines().briefing_end == 0);
}

TEST_CASE("briefing: PlayMode != 0 ends the briefing after phase 17") {
    Rig r;
    r.setup.play_mode = 1;
    r.start();
    REQUIRE(r.until_phase(17));
    r.tick();
    CHECK(r.s.briefing_phase() == 18);
    r.tick();
    CHECK(r.s.state() == is::shell::kWave);
}

TEST_CASE("briefing: Space finishes the text and skips one phase") {
    Rig r;
    r.text = "A LONG BRIEFING TEXT THAT TAKES A WHILE";
    r.start();
    r.tick(600);
    CHECK_FALSE(r.b.typewriter().done());
    r.key_up(0x20);
    r.tick();
    // The text is finished at once, and the frame advances: phase 1.
    CHECK(r.b.typewriter().done());
    CHECK(r.s.briefing_phase() == 1);
    CHECK(r.s.briefing_skip() == 0);  // seg17:0x3b2 cleared it
    // Space in phase 5 skips the slide-in.
    REQUIRE(r.until_phase(5));
    r.key_up(0x20);
    r.tick();
    CHECK(r.s.briefing_phase() == 6);
}

TEST_CASE("briefing: QuickIntro skips to phase 18 without the box") {
    Rig r;
    r.setup.quick_intro = 1;
    r.start();
    CHECK(r.c.count("rect ") == 0);
    CHECK(r.c.count("text ") == 0);
    CHECK(r.s.briefing_phase() == 18);
    r.tick();  // case 18 arms DGROUP:0x5ac
    CHECK(r.s.deadlines().briefing_end != 0);
    r.tick();
    CHECK(r.s.state() == is::shell::kWave);
}

TEST_CASE("briefing: a repaint replays from phase 0") {
    Rig r;
    r.start();
    REQUIRE(r.until_phase(1));
    r.c.ops.clear();
    r.s.repaint();
    CHECK(r.s.briefing_phase() == 0);
    CHECK(r.c.count("rect ") == 6);
    CHECK_FALSE(r.b.typewriter().done());
    // Phase 18 is not repainted.
    r.key_up(0x1b);
    r.tick();
    r.c.ops.clear();
    r.s.repaint();
    CHECK(r.c.ops.empty());
}

TEST_CASE("briefing: phase 1 with three icons") {
    Rig r;
    r.setup.icons = 3;
    r.setup.gone = {false, false, false};
    r.start();
    REQUIRE(r.until_phase(1));
    // The init draws the first frame: size 4 at slot 0. Page (208, 165) - (816, 443): middle x
    // 512, slots 80 apart from 392; y = (165 - 443)/2 - 2 + 443 + 52 = 356.
    auto icons = r.c.with("icon ");
    REQUIRE_FALSE(icons.empty());
    CHECK(icons[0] == "icon 422 354 4 4 6 6 32 32");
    // Grow to 64, then the next icon.
    while (r.b.icon_index() == 0) r.tick();
    icons = r.c.with("icon ");
    CHECK(icons.size() == 21);  // 4, 7, ... 64
    CHECK(icons.back() == "icon 392 324 64 64 6 6 32 32");
    CHECK(std::count(r.h.sounds.begin(), r.h.sounds.end(), 0x13) == 1);  // at size 13
    // After the third icon the page #5000 300 types from (247, 324 - 106).
    while (r.s.briefing_skip() == 0) r.tick();
    CHECK(r.s.briefing_skip() == 1);
    CHECK(r.c.with("icon ").back() == "icon 552 324 64 64 82 6 32 32");
    while (r.c.count("text 13/6/0") == 0) r.tick();
    CHECK(r.c.with("text 13/6/0").front() == "text 13/6/0 00ffff00 0 247 218 I");
    // Hold 3000 ms (wave 1) after the page, then shrink; the phase ends 1000 ms after the last.
    while (r.s.briefing_skip() == 1) r.tick();
    CHECK(r.s.briefing_skip() == 2);
    r.c.ops.clear();
    r.tick();
    CHECK(r.c.with("icon ").front() == "icon 392 324 64 64 4 4 36 36");
    REQUIRE(r.until_phase(2));
    CHECK(r.t - r.b.phase1_end() < 1000);  // DGROUP:0x187a was armed, then passed
}
