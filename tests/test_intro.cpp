// The intro's phase logic (src/intro/intro.h) against docs/re/shell.md "Intro and title
// screen", driven by the recording fakes of intro_fakes.h on a fake clock (13 ms frames,
// ADR 0001 decision 8).
#include <cstdio>
#include <map>
#include <set>
#include <string>

#include "doctest.h"
#include "intro/intro.h"
#include "intro_fakes.h"

using namespace intro_test;
using is::intro::Intro;

namespace {

struct Rig {
    FakeCanvas canvas;
    FakeHost host;
    is::shell::Rng rng;
    Intro intro{canvas, host, rng};
    Rig() { intro.enter(host.t); }
    int until(int phase, int step) {
        return run_until(intro, host, [&] { return intro.phase() == phase && intro.step() == step; });
    }
    int until_phase(int phase) {
        return run_until(intro, host, [&] { return intro.phase() == phase; });
    }
};

int cell_of(const std::string& blit) {  // "blit strip x y 40 40 sx 6"
    int dx, dy, w, h, sx, sy;
    std::sscanf(blit.c_str(), "blit strip %d %d %d %d %d %d", &dx, &dy, &w, &h, &sx, &sy);
    return (sx - 6) / 46;
}

}  // namespace

TEST_CASE("intro: the state-0 entry (seg15:0x5786)") {
    Rig r;
    CHECK(r.intro.phase() == 0);
    CHECK(r.intro.step() == 0);
    CHECK(r.intro.deadline() == 1000 + 65000);
    CHECK(r.intro.speed() == 290);
    CHECK(r.intro.spin() == 5000 + 290);  // the entry's frame seg15:0x5962(1) ran once
    CHECK(r.host.shown == 1);
    CHECK(r.host.started);
    CHECK(r.host.start == 1000);
    CHECK(r.host.music_codes == std::vector<int>{-1, -5});
    CHECK(r.host.sounds == std::vector<int>{0x6e});
    REQUIRE(r.canvas.ops.size() >= 4);
    CHECK(r.canvas.ops[0] == "black 0 0 1024 768");
    CHECK(r.canvas.ops[1] == "load_sd_logo");
    CHECK(r.canvas.ops[2] == "build_strip");
    CHECK(r.canvas.ops[3] == "blit strip 424 324 40 40 236 6");  // cell 5 at (W/2 - 88, H/2 - 60)
    CHECK(r.intro.trial_day_value() == 12088);                   // 2026-10-25
    CHECK(r.intro.attract());
}

TEST_CASE("intro: logo step 0 spins, Calibrate sets 240 and each wrap lowers it by 18") {
    Rig r;
    std::vector<int> speeds{r.intro.speed()};
    int frames = run_until(r.intro, r.host, [&] {
        if (r.intro.speed() != speeds.back()) speeds.push_back(r.intro.speed());
        return r.intro.step() == 1;
    });
    // 290 during the 1 s window (no decrement while DGROUP:0x120e != 0), 240 at its end, then
    // one decrement per wrap above 13999, and the step ends at 150 with f = 5.
    CHECK(speeds == std::vector<int>{290, 240, 222, 204, 186, 168, 150, 0});
    CHECK(r.host.calibrated());
    CHECK(r.host.sounds.back() == 0x23);
    CHECK(r.intro.spin() == 0);
    // the last blit was cell 5
    const auto blits = r.canvas.with_prefix("blit strip");
    CHECK(cell_of(blits.back()) == 5);
    // f is 0 from 9999 up: every blit shows one of cells 0-9
    for (const auto& b : blits) CHECK(cell_of(b) <= 9);
    CHECK(frames > 300);
}

TEST_CASE("intro: logo steps 1-4 (88, 156 and 87 frames, then 3000 ms of presents..)") {
    Rig r;
    r.until(0, 1);
    r.canvas.ops.clear();
    CHECK(r.until(0, 2) == 88);
    // cells 10-13 for 23, 22, 22 and 21 frames
    std::map<int, int> cells;
    for (const auto& b : r.canvas.with_prefix("blit strip")) ++cells[cell_of(b)];
    CHECK(cells == std::map<int, int>{{10, 23}, {11, 22}, {12, 22}, {13, 21}});
    CHECK(r.host.sounds.back() == 0x68);
    CHECK(r.intro.spin() == 620);

    r.canvas.ops.clear();
    CHECK(r.until(0, 3) == 156);
    CHECK(r.canvas.count_prefix("line ") == 2 * 156);
    // the first column: v = 620, x = W/2 + 124 - 38, source column 124, dark grey brush
    CHECK(r.canvas.with_prefix("merge")[0] == "merge sdlogo 598 328 2 32 124 0 02404040");
    // the colour pen, then the black pen from the same start
    const auto lines = r.canvas.with_prefix("line ");
    CHECK(lines[1].rfind("line 00000000 2 464 344 ", 0) == 0);
    std::set<std::string> pens;
    for (std::size_t i = 0; i < lines.size(); i += 2) pens.insert(lines[i].substr(5, 8));
    for (const auto& p : pens)
        CHECK((p == "00ff0000" || p == "00ffff00" || p == "00ffffff" || p == "00808000"));
    // 156 column merges and the 315 brush blits of the last frame, the last RGB(253, 253, 2)
    CHECK(r.canvas.count_prefix("merge sdlogo ") == 156 + 315);
    CHECK(r.canvas.with_prefix("merge").back() == "merge sdlogo 474 328 130 32 0 0 0002fdfd");
    CHECK(r.canvas.count_prefix("black 464 328 10 32") == 1);
    CHECK(r.intro.spin() == 3940);

    r.canvas.ops.clear();
    CHECK(r.until(0, 4) == 87);
    CHECK(r.intro.spin() == 25);
    CHECK(cell_of(r.canvas.with_prefix("blit strip").back()) == 5);
    CHECK(r.intro.deadline() == r.host.t + 3000);

    r.canvas.ops.clear();
    const Tick d = r.intro.deadline();
    CHECK(r.until_phase(1) == 231);  // the first frame after the deadline
    CHECK(r.host.t > d);
    CHECK(r.canvas.count_prefix("text 0000ffff 512 401 6 1 presents.. ") == 231);
    CHECK(r.canvas.count_prefix("font 38 0 0 1 Times New Roman") == 231);
    CHECK(r.canvas.ops.back() == "black 0 0 1024 768");  // the black-out of the transition
    CHECK(r.intro.deadline() == r.host.t + 1000);
}

TEST_CASE("intro: the phase timeline without input") {
    Rig r;
    r.until_phase(1);
    const Tick p1 = r.host.t;
    r.until_phase(2);
    CHECK(r.host.t - p1 == 1001);  // the first 13 ms frame past the 1000 ms deadline
    CHECK(r.intro.deadline() == r.host.t + 65000);
    CHECK(r.intro.title_d() == 0);

    // title steps 0-4: 105 + 1 + 1 + 115 + 20 frames
    CHECK(r.until(2, 1) == 105);
    CHECK(r.until(2, 2) == 1);
    CHECK(r.until(2, 3) == 1);
    CHECK(r.until(2, 4) == 115);
    CHECK(r.until(2, 5) == 20);
    CHECK(r.intro.title_c() == 40);
    const Tick end4 = r.host.t;
    CHECK(r.intro.deadline() == end4 + 51500);
    CHECK(r.intro.timer2() == end4 + 45000);
    run_until(r.intro, r.host, [&] { return r.intro.timer2() == 0; });
    CHECK(r.host.has_todo("scripted title scene"));
    r.until_phase(3);
    CHECK(r.host.t - end4 == 51506);

    // credits: 90 s deadlines that each reload renews, then the 4000 ms tail
    CHECK(r.intro.deadline() == r.host.t + 90000);
    r.until_phase(4);
    CHECK(r.intro.credit_entry() == 6);
    CHECK(r.intro.page() == 0);
    const Tick p4 = r.host.t;
    CHECK(r.intro.deadline() == p4 + 25000);
    run_until(r.intro, r.host, [&] { return r.intro.page() == 1; });
    CHECK(r.host.t - p4 == 25012);
    CHECK(r.intro.deadline() == r.host.t + 27000);
    r.until_phase(5);
    const Tick p5 = r.host.t;
    CHECK(r.intro.deadline() == p5 + 11000);
    CHECK(r.host.demo_starts == 0);
    r.until_phase(7);
    CHECK(r.host.t - p5 == 11011);
    CHECK(r.host.demo_starts == 1);
    CHECK(r.host.has_todo("demo start"));
    CHECK(!r.intro.attract());
    // phase 7 does nothing
    r.canvas.ops.clear();
    for (int i = 0; i < 10; ++i) r.intro.frame(r.host.t += 13, false);
    CHECK(r.canvas.ops.empty());
    // a new entry starts the logo again; DGROUP:0x95ce keeps the 40 of the last title
    r.intro.enter(r.host.t);
    CHECK(r.intro.phase() == 0);
    CHECK(r.intro.title_c() == 40);
}

TEST_CASE("intro: the title's first frame (seg15:0x6cdc)") {
    Rig r;
    r.until_phase(2);
    r.canvas.ops.clear();
    r.host.t += 13;
    r.intro.frame(r.host.t, false);
    const auto& o = r.canvas.ops;
    REQUIRE(o.size() > 20);
    CHECK(o[0] == "ramp 5");
    CHECK(o[1] == "font 24 11 600 0 Arial");
    // OPERATION: embossed at (W/2 - 90, H/2 + D - 245): white -1, shadow +1, main
    CHECK(o[2] == "text 00ffffff 421 138 6 1 OPERATION:");
    CHECK(o[3] == "text 02c00060 423 140 6 1 OPERATION:");
    CHECK(o[4] == "text 02ff60a0 422 139 6 1 OPERATION:");
    CHECK(o[5] == "font 35 19 0 0 Arial");
    CHECK(o[8] == "text 02ff60a0 512 164 6 1 limited inner space\x99");
    CHECK(o[9] == "font 22 0 0 0 Arial");
    CHECK(o[12] == "text 02ff60a0 512 198 6 1  Standard Edition - V1.29.08 Aug 9/2000");
    CHECK(o[15] == "text 02ff60a0 512 509 6 1 You are on day 12088 of your demo trial period.");
    CHECK(o[16] == "text 00000000 512 534 6 2  Press 'R' to order the full game package ");
    CHECK(o[17] == "font 16 0 500 0 Arial");
    CHECK(o[18] ==
          "text 02e080b4 512 487 6 1 \xA9 1994-2000 Software Dynamics, inc. All rights reserved.");
    CHECK(o[19] == "font 30 0 800 1 Arial");
    CHECK(o[22] == "text 02ff60a0 512 569 6 1 ESC to play    -    Alt-X to Exit");
    CHECK(o[23] == "load_title_picture");
    CHECK(o[24] == "double title 374 229 0 0 138 105");
    CHECK(o[25] == "free title");
    CHECK(o[26] == "make_title_reveal");
    CHECK(o[27].rfind("blit reveal 374 ", 0) == 0);
    CHECK(r.host.has_todo("ordinal 12"));
    // no 256-colour hint (ADR 0007 decision 5)
    for (const auto& s : o) CHECK(s.find("256 color") == std::string::npos);
    // the Press 'R' line restored the background and TRANSPARENT
    CHECK(r.canvas.mode == 2);  // release_dc
}

TEST_CASE("intro: the title reveals each of the 105 rows once") {
    Rig r;
    r.until_phase(2);
    r.canvas.ops.clear();
    r.until(2, 1);
    std::set<int> rows;
    for (const auto& b : r.canvas.with_prefix("blit reveal")) {
        int dx, dy, w, h, sx, sy;
        std::sscanf(b.c_str(), "blit reveal %d %d %d %d %d %d", &dx, &dy, &w, &h, &sx, &sy);
        CHECK(dx == 374);
        CHECK(w == 276);
        CHECK(h == 2);
        CHECK(sx == 0);
        CHECK(sy % 2 == 0);
        CHECK(dy == 229 + sy);
        rows.insert(sy / 2);
    }
    CHECK(rows.size() == 105);
    CHECK(*rows.rbegin() == 104);
    CHECK(r.canvas.count_prefix("free reveal") == 1);
}

TEST_CASE("intro: D follows the height at the title's entry") {
    for (auto [h, d] : {std::pair{400, 40}, {439, 40}, {440, 30}, {499, 30}, {500, 0}, {768, 0}}) {
        Rig r;
        r.host.a.h = h;
        r.until_phase(2);
        CHECK(r.intro.title_d() == d);
    }
}

TEST_CASE("intro: the credits on the port's path (Calibrate 14000, MixChannels 1)") {
    Rig r;
    r.until_phase(3);
    CHECK(r.canvas.ops.size() > 0);
    const auto ops = r.canvas.ops;
    // the entry: scheme 8 mode 0, animate, the cells, sound 0x13
    bool entry_seen = false;
    for (std::size_t i = 0; i + 2 < ops.size(); ++i)
        if (ops[i] == "scheme 8 0" && ops[i + 1] == "animate" && ops[i + 2] == "build_credit_cells 0")
            entry_seen = true;
    CHECK(entry_seen);
    CHECK(r.host.sounds.back() == 0x13);

    r.canvas.ops.clear();
    // 83 frames of growth alone; the 84th clamps to 300 and types (c = 0: a bolt, no glyph)
    int frames = run_until(r.intro, r.host, [&] { return r.canvas.count_prefix("line ") > 0; });
    CHECK(frames == 84);
    const auto st = r.canvas.with_prefix("stretch credits");
    CHECK(st.front().rfind("stretch credits 507 359 10 10 ", 0) == 0);  // s = 53 * 38 / 200
    CHECK(st.back().rfind("stretch credits 484 336 57 57 ", 0) == 0);   // s = 57
    CHECK(r.intro.typed() == 4);

    // "Directed By" (11): 6 typing frames and the completion frame; then "Bill Stewart" (12): 7 + 1
    frames = run_until(r.intro, r.host, [&] { return r.intro.credit_line() == 1; });
    CHECK(frames == 6);  // the call with c = 0 already ran
    r.canvas.ops.clear();
    frames = run_until(r.intro, r.host, [&] { return r.intro.timer2() != 0; });
    CHECK(frames == 8);
    CHECK(r.intro.timer2() == r.host.t + 3000);
    CHECK(r.intro.pending_bolt() == 0);
    // the full name in small capitals: 'B' big, 'i' as 'I' small; three texts per glyph
    CHECK(r.canvas.count_prefix("text 00ffffff") == 3 * 0 + (0 + 2 + 4 + 6 + 8 + 10 + 12) + 12);
    // the hold, then the erase in one frame with its busy waits (40 + 270 + 40 ms)
    const Tick before = r.host.t;
    run_until(r.intro, r.host, [&] { return r.intro.next_entry() != 0; });
    CHECK(r.host.t - before >= 3000 + 350);
    CHECK(r.intro.credit_entry() == 1);
    CHECK(r.intro.next_entry() == r.host.t + 1000);
    // one second of logo alone, then the reload frame (no typing), then the heading
    r.canvas.ops.clear();
    frames = run_until(r.intro, r.host, [&] { return r.intro.next_entry() == 0; });
    CHECK(frames == 77);
    CHECK(r.canvas.count_prefix("line ") == 0);
    CHECK(r.intro.deadline() == r.host.t + 90000);
}

TEST_CASE("intro: the pages of phase 4 and Basic Key Commands") {
    Rig r;
    r.until_phase(3);
    run_until(r.intro, r.host, [&] {
        if (r.intro.phase() == 3) r.canvas.ops.clear();
        return r.intro.phase() == 4;
    });
    auto o = r.canvas.with_prefix("font_text");
    REQUIRE(o.size() == 2);
    CHECK(o[0] == "font_text 0 6 00ff008c 512 209 Limited Inner Space V1.29.08");
    CHECK(o[1] == "font_text 0 6 00ffffff 512 557 press ESC to play Inner Space.. ");
    CHECK(r.canvas.count_prefix("black 0 269 1024 533") == 1);
    CHECK(r.canvas.count_prefix("font 18 10 0 0 Arial") == 1);
    auto t = r.canvas.with_prefix("text ");
    REQUIRE(t.size() == 11);
    CHECK(t[0] == "text 02ff8080 512 269 6 1 line 0");
    CHECK(t[10] == "text 02ff8080 512 509 6 1 line 10");
    r.canvas.ops.clear();
    run_until(r.intro, r.host, [&] { return r.intro.page() == 1; });
    t = r.canvas.with_prefix("text ");
    REQUIRE(t.size() == 11);
    CHECK(t[0] == "text 02ff8080 512 269 6 1 line 11");
    CHECK(t[9] == "text 02ff8080 512 485 6 1 line 20");
    CHECK(t[10] == "text 02ff8080 512 509 6 1 ");
    r.canvas.ops.clear();
    r.until_phase(5);
    o = r.canvas.with_prefix("font_text");
    REQUIRE(o.size() == 23);
    CHECK(o[0] == "font_text 0 6 000000ff 512 184 Basic Key Commands");
    CHECK(o[1] == "font_text 0 0 000b00ff 217 234 key0");
    CHECK(o[2] == "font_text 0 0 000b00ff 507 234 - desc0");
    CHECK(o[17] == "font_text 0 0 006b0097 217 474 Alt G");
    CHECK(o[22] == "font_text 0 0 0083007d 507 534 - Online Help");
    // phases 4 and 5 draw nothing on tick frames
    r.canvas.ops.clear();
    for (int i = 0; i < 20; ++i) r.intro.frame(r.host.t += 13, false);
    CHECK(r.canvas.count_prefix("font_text") == 0);
}

TEST_CASE("intro: Space skips the phase, ESC and the other keys only log") {
    Rig r;
    // during Calibrate Space does nothing (DGROUP:0x120e != 0)
    r.intro.key_up(0x20, r.host.t);
    CHECK(r.intro.deadline() == 66000);
    r.until(0, 1);
    r.intro.key_up(0x20, r.host.t);
    CHECK(r.intro.deadline() == r.host.t);
    r.intro.frame(r.host.t, false);  // the same pass: deadline == now, no transition
    CHECK(r.intro.phase() == 0);
    r.intro.frame(r.host.t += 13, false);
    CHECK(r.intro.phase() == 1);
    const auto before = r.host.todos.size();
    r.intro.key_up(0x1b, r.host.t);
    CHECK(r.host.todos.size() == before + 1);
    CHECK(r.host.has_todo("Restore Game"));
    CHECK(r.intro.phase() == 1);
    r.intro.key_up('R', r.host.t);
    CHECK(r.host.has_todo("Order"));
    CHECK(r.host.sounds.back() == 0x51);
    r.intro.key_up('S', r.host.t);
    r.intro.key_up('D', r.host.t);
    r.intro.key_up('C', r.host.t);
    CHECK(r.host.todos.size() == before + 5);
    r.intro.key_up(0x0d, r.host.t);  // Return does nothing
    r.intro.key_up('X', r.host.t);
    CHECK(r.host.todos.size() == before + 5);
    // each missing dialog or path is logged once, however often its key comes
    r.intro.key_up(0x1b, r.host.t);
    r.intro.key_up('R', r.host.t);
    r.intro.key_up('S', r.host.t);
    r.intro.key_up('D', r.host.t);
    r.intro.key_up('C', r.host.t);
    CHECK(r.host.todos.size() == before + 5);
}

TEST_CASE("intro: the repaint rule of seg11:0xafe") {
    Rig r;
    r.until_phase(2);
    r.until(2, 3);
    r.host.t += 5;
    r.intro.repaint(r.host.t - 5);
    CHECK(r.intro.deadline() == r.host.t);  // phase 2 ends at the next frame
    r.intro.frame(r.host.t += 13, false);
    CHECK(r.intro.phase() == 3);
    r.host.t += 7;
    r.intro.repaint(r.host.t - 7);
    CHECK(r.intro.deadline() == r.host.t + 15000);
    // a deadline already at DGROUP:0x1e50 stays
    r.intro.key_up(0x20, r.host.t);
    r.intro.repaint(r.host.t);
    CHECK(r.intro.deadline() == r.host.t);
}

TEST_CASE("intro: a dialog cancelled in the credits ends them (seg10:0x8550, seg11:0x164b)") {
    Rig r;
    r.until_phase(3);
    r.host.t += 40;
    r.intro.after_dialog(8, 0);  // Order Cancel: phase < 4 gives deadline = DGROUP:0x1e50
    CHECK(r.intro.deadline() == r.host.t);
    r.host.t += 30;  // the repaint comes later; it compares with the stored DGROUP:0x1e50
    r.intro.repaint(r.host.t);  // the deadline stays, so the repaint's frame ends the credits
    CHECK(r.intro.phase() == 4);
}

TEST_CASE("intro: a second run's credits keep the typing state (shell.md Skipped runs)") {
    Rig r;
    r.until_phase(3);
    run_until(r.intro, r.host, [&] { return r.intro.typed() == 8; });
    r.intro.key_up(0x20, r.host.t);
    r.until_phase(4);
    CHECK(r.intro.typed() == 12);  // one more typing frame ran before the transition
    r.until_phase(7);
    r.intro.enter(r.host.t);
    r.until_phase(3);
    CHECK(r.intro.typed() == 12);  // DGROUP:0x95da is not reset by the credits' entry
}
