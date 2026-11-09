// src/desk in state 6: the visit's launch, phases 61 and 63 of seg16:0xcfe. seg15:0xcb0's state
// != 0 path (Desk::enter_visit) and seg15:0x10be's steps 5-9 with seg15:0x0 at ye = -10
// (work/analysis/m4/s6-pacing.md section 7 with s6-pacing-verify.md claims 29-47) against the
// oracle's launch timings (core-0007 v1 ... v5, core-0008 sky, tf, pace) with a fake clock.
#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "desk/desk.h"

using namespace is::desk;

namespace {

struct Canvas6 final : Canvas {
    std::vector<std::string> calls;
    std::vector<LaunchDraw> draws;
    std::vector<Rect> blacks;
    std::vector<std::pair<int, int>> flashes;
    std::string ship_name;
    int s = 33;          // seg15:0x9e6's s for the TRANSFORMER (s6-pad.md section 2.1)
    bool pad = true;     // DGROUP:0x92b6
    void palette_save(int, int) override { calls.push_back("palette save"); }
    void palette_scheme(int c, int m) override { calls.push_back("scheme " + std::to_string(c) + " " + std::to_string(m)); }
    void palette_restore(int) override { calls.push_back("palette restore"); }
    void palette_fade(int, int) override {}
    void scene(const Render&) override { calls.push_back("scene"); }
    void monitor(const Rect&) override { calls.push_back("monitor"); }
    void tower(const Rect&) override { calls.push_back("tower"); }
    void meal(int, const Rect&) override { calls.push_back("meal"); }
    void scene_to_screen() override { calls.push_back("scene shown"); }
    void screen_to_scene(int, int, int, int) override { calls.push_back("text saved"); }
    void scene_to_screen(int, int, int, int) override { calls.push_back("text restored"); }
    void black(int x, int y, int w, int h) override { blacks.push_back({x, y, x + w, y + h}); }
    void block(int, int, int, int, Colour) override {}
    void text_font(int, int) override {}
    void text_char(int, int, char) override {}
    int text_extent(const std::string& t) override { return 6 * static_cast<int>(t.size()); }
    void scroll_up(const Rect&, int) override {}
    void flash(int x, int y) override { flashes.push_back({x, y}); }
    void make_pad(int, int) override { calls.push_back("make pad"); }
    int make_ship(const std::string&) override {
        calls.push_back("make ship");
        return 32;
    }
    void rise(int, int, int, int, int) override { calls.push_back("rise"); }
    void free_rise() override {}
    void pad_to_screen(int, int) override { calls.push_back("pad shown"); }
    void launch_frame(const LaunchDraw& d) override { draws.push_back(d); }
    void free_all() override { calls.push_back("free all"); }
    bool pad_alive() override { return false; }
    int visit_ship(const std::string& n) override {
        ship_name = n;
        calls.push_back("visit ship");
        return s;
    }
    bool pad_dc() override { return pad; }
};

struct Host6 final : Host {
    Tick t = 50000;
    int cal = 14000;
    std::vector<int> sounds;
    std::vector<std::vector<int>> sounds_at;
    std::vector<Tick> sound_times;
    int pumps = 0;
    Tick now() override { return t; }
    void sound(int id) override {
        sounds.push_back(id);
        sound_times.push_back(t);
    }
    void sound_at(int id, int x, int y, int m) override { sounds_at.push_back({id, x, y, m}); }
    void music(int) override {}
    void pump() override { ++pumps; }
    void pump_until(Tick u) override {
        ++pumps;
        if (static_cast<std::int32_t>(u - t) > 0) t = u;
    }
    void busy_until(Tick u) override {
        if (static_cast<std::int32_t>(u - t) > 0) t = u;
    }
    std::string lang(int) override { return {}; }
    std::string lang_text(int, int) override { return {}; }
    std::string ini(const char* k, const char* d) override { return std::string(k) == "MainShip" ? "TRANSFORMER" : d; }
    int ini_int(const char*, int d) override { return d; }
    Clock clock() override { return {}; }
    int rand() override { return 0; }
    int calibrate_value() override { return cal; }
    Conditions conditions() override { return {}; }
    Render render(int, int W, int H) override {
        Render r;
        r.proj.W = W;
        r.proj.H = H;
        return r;
    }
    void todo(const std::string&, const char*) override {}
};

// The visit's 1024 x 768 layout: pad width DGROUP:0x3ce6 = 162 and column DGROUP:0x3c9a = 78
// (world/visit.h visit_layout: k = 4·768/3 - 1100 = -76); the TRANSFORMER's descent ends at
// DGROUP:0x92d6 = (768 - 78 - 52) - 132 + 78 - 10 = 574 [ORACLE core-0007: 574/274 at the descent].
constexpr int kW = 1024, kH = 768, kPadW = 162, kPadC = 78, kEndY = 574;

}  // namespace

TEST_CASE("desk in state 6: seg15:0xcb0 plays 0x6e and 0x4b, sets step 5 and pumps 1550 ms") {
    Canvas6 cv;
    Host6 h;
    Desk d(cv, h);
    const Tick t0 = h.t;
    d.enter_visit(t0, kW, kH, kPadW, kPadC, kEndY);
    CHECK(d.state() == 6);
    CHECK(d.step() == 5);                               // DGROUP:0x92d4 [BYTES seg15:0xede]
    CHECK(h.sounds == std::vector<int>{0x6e, 0x4b});    // [BYTES seg15:0xcc5 seg15:0xeeb]
    CHECK(h.t == t0 + 1550);                            // [BYTES seg15:0xef8] [ORACLE core-0007: 1.547 - 1.552 s]
    CHECK(d.start_y() == kEndY);                        // DGROUP:0x92d8 = DGROUP:0x92d6 [BYTES seg15:0xf1a]
    CHECK(d.end_y() == 10);                             // [BYTES seg15:0xf20]
    // No ShowLaunch test, palette, scene, monitor or pad (verified claim 30).
    CHECK(cv.calls.empty());
}

TEST_CASE("desk in state 6: steps 5-9, the rise to ye = -10 and the exit without restores") {
    Canvas6 cv;
    Host6 h;
    Desk d(cv, h);
    d.enter_visit(h.t, kW, kH, kPadW, kPadC, kEndY);
    h.sounds.clear();
    h.sound_times.clear();
    // Step 5 (phase 63's first frame): seg15:0x9e6 from INI MainShip, seg15:0x0(2), 950 ms.
    const Tick t5 = h.t;
    CHECK_FALSE(d.frame(h.t));
    CHECK(d.step() == 6);
    CHECK(cv.ship_name == "TRANSFORMER");
    CHECK(std::find(cv.calls.begin(), cv.calls.end(), "text saved") == cv.calls.end());  // no BitBlt
    CHECK(std::find(cv.calls.begin(), cv.calls.end(), "pad shown") == cv.calls.end());   // no pad blit
    CHECK(h.t == t5);  // no 800 ms busy-wait (verified claim 35)
    REQUIRE(cv.draws.size() == 1);
    // y0 = pad top 638 - s4 132 + 78 - 10 = 574, ye = -10 [ORACLE core-0007: -10/574 at desk 6];
    // the work DC 132 x 147 at (512 - 66, 574), no scene part (verified claims 42, 43, 48).
    CHECK(d.start_y() == 574);
    CHECK(d.end_y() == -10);
    const LaunchDraw& first = cv.draws.front();
    CHECK(first.ship == Rect{446, 574, 578, 721});
    CHECK_FALSE(first.scene);
    CHECK(first.state == 6);
    CHECK(first.y0 == 574);
    CHECK(first.ye == -10);
    CHECK(first.prev_top == 574);
    CHECK(first.pad == Rect{512 - 81, 768 - 78 - 52, 512 - 81 + 162, 768});
    CHECK(h.sounds == std::vector<int>{0x20});
    // Step 6: the 950 ms (0x20 to 0x1c) [ORACLE core-0008: 0.956 - 0.968 s].
    d.frame(t5 + 950);
    h.t = t5 + 950;
    d.frame(h.t);
    CHECK(d.step() == 6);
    h.t = t5 + 951;
    d.frame(h.t);
    CHECK(d.step() == 7);
    CHECK(h.sounds == std::vector<int>{0x20, 0x1c});
    // Step 7: one seg15:0x0(0) per frame; at Calibrate >= 1400 the rise from 574 to -10 takes
    // 122 calls in all (step 5's and step 6's included).
    const std::size_t pumps7 = static_cast<std::size_t>(h.pumps);
    while (d.step() == 7 && cv.draws.size() < 1000) {
        h.t += 14;
        d.frame(h.t);
    }
    CHECK(d.step() == 8);
    CHECK(cv.draws.size() == 122);
    CHECK(cv.draws.back().f == 15);
    CHECK(static_cast<std::size_t>(h.pumps) == pumps7 + 1);  // the pump of step 7's last call
    // The exit outside state 0: 0x15, the last rectangle blacked, the flash at its centre and
    // top + 12 with 0x14 at that point (mode 4), no 0x4c, no restores, no hold, no 1460 ms.
    CHECK(h.sounds == std::vector<int>{0x20, 0x1c, 0x15});
    const Rect last = cv.draws.back().ship;
    REQUIRE(cv.blacks.size() == 1);
    CHECK(cv.blacks[0] == last);
    const int fx = last.l + (last.r - last.l) / 2, fy = last.t + 12;
    REQUIRE(cv.flashes.size() == 1);
    CHECK(cv.flashes[0] == std::make_pair(fx, fy));
    REQUIRE(h.sounds_at.size() == 1);
    CHECK(h.sounds_at[0] == std::vector<int>{0x14, fx, fy, 4});
    CHECK(std::find(cv.calls.begin(), cv.calls.end(), "text restored") == cv.calls.end());
    CHECK(h.sound_times.back() == h.t);  // no hold before 0x15
    // Step 8, then step 9: the pump, seg15:0x1328(1) and seg15:0x14be, and true; no black
    // screen, no 450 ms, no palette (verified claim 39).
    const Tick t8 = h.t;
    CHECK_FALSE(d.frame(h.t));
    CHECK(d.step() == 9);
    CHECK(d.frame(h.t));
    CHECK(h.t == t8);
    CHECK(cv.calls.back() == "free all");
    for (const std::string& c : cv.calls) {
        CHECK(c.rfind("scheme", 0) != 0);
        CHECK(c != "palette restore");
    }
    CHECK(cv.blacks.size() == 1);
}

TEST_CASE("desk in state 6: the rise's call count by the Calibrate branch") {
    // seg15:0x0's 3/7 factor and 4f falloff at Calibrate >= 1400 against 5/7 and f/2 below
    // [BYTES seg15:0x2c4 seg15:0x598]: 122 calls for 574 -> -10 on the fast branch, 57 on the
    // slow one, where the oracle (Calibrate -27858 and -28267) steps pos10 5633, 5526, 5419, ...
    // to -178 [ORACLE core-0007] [ORACLE core-0008].
    for (const int cal : {14000, 1000}) {
        Canvas6 cv;
        Host6 h;
        h.cal = cal;
        Desk d(cv, h);
        d.enter_visit(h.t, kW, kH, kPadW, kPadC, kEndY);
        d.frame(h.t);
        h.t += 951;
        d.frame(h.t);
        while (d.step() == 7 && cv.draws.size() < 1000) {
            h.t += 14;
            d.frame(h.t);
        }
        REQUIRE(d.step() == 8);
        if (cal >= 1400) {
            CHECK(cv.draws.size() == 122);
        } else {
            CHECK(cv.draws.size() == 57);
            REQUIRE(cv.draws.size() >= 3);
            CHECK(cv.draws[1].pos10 == 5633);
            CHECK(cv.draws[2].pos10 == 5526);
            CHECK(d.ship_y10() == -178);
        }
    }
}

TEST_CASE("desk in state 6: without the pad DC y0 keeps seg15:0xcb0's descent end") {
    Canvas6 cv;
    cv.pad = false;
    Host6 h;
    Desk d(cv, h);
    d.enter_visit(h.t, kW, kH, kPadW, kPadC, 600);
    d.frame(h.t);
    CHECK(d.start_y() == 600);  // [BYTES seg15:0xb2-0xc3]
    CHECK(d.end_y() == -10);
    REQUIRE(!cv.draws.empty());
    CHECK(cv.draws.front().ship.t == 600);
}

TEST_CASE("desk in state 6: the launch from phase 61 to phase 64 at 72 frames per second") {
    // Phase 61's 1550 ms, phase 63's step 5 to 6 (950 ms), 7 (0x1c) and the rise's 120 step-7
    // calls at the screen pace (ruling R1): 0x1c to 0x15 = 120 frames of 1000/72 ms.
    Canvas6 cv;
    Host6 h;
    Desk d(cv, h);
    const Tick t61 = h.t;
    d.enter_visit(h.t, kW, kH, kPadW, kPadC, kEndY);
    CHECK(h.t - t61 == 1550);
    // Frames on the 72 Hz grid from here: frame k at t + trunc(1000k/72).
    const Tick origin = h.t;
    std::uint64_t k = 0;
    auto next = [&] { h.t = origin + static_cast<Tick>(k++ * 1000 / 72); };
    next();
    d.frame(h.t);  // step 5: 0x20
    const Tick t20 = h.sound_times.back();
    while (d.step() == 6) {
        next();
        d.frame(h.t);
    }
    const Tick t1c = h.sound_times.back();
    CHECK(t1c - t20 == 958);  // the first 72 Hz frame after 950 ms [ORACLE core-0008: 0.956 - 0.968 s]
    while (d.step() == 7) {
        next();
        d.frame(h.t);
    }
    const Tick t15 = h.sound_times.back();
    CHECK(h.sounds.back() == 0x15);
    CHECK(t15 - t1c == 120 * 1000 / 72 + 1);  // 1667 ms: 120 calls; the oracle's paced 55 take 0.78 - 0.85 s
    next();
    d.frame(h.t);  // step 8
    next();
    CHECK(d.frame(h.t));  // step 9: phase 64 follows on the next frame
}
