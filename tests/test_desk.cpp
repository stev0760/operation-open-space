// src/desk: ordinal 32's projection, layout and slots (docs/re/ispace.md "Ordinal notes" 32),
// the #6600 100 decoder against the resource bytes (golden), and the desk scene's step table
// (docs/re/shell.md "Game-start dialogs" items 6-9) with a fake clock.
#include <doctest.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <tuple>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "desk/desk.h"
#include "desk/scene.h"
#include "formats/ne.h"
#include "formats/records.h"

using namespace is;
using namespace is::desk;
namespace fs = std::filesystem;

namespace {

Header scene100_header() {
    Header h;
    h.mode_x = 6;
    h.mode_y = 7;
    h.d5c = 500;
    h.d60 = 217;
    h.d64 = 1000;
    h.d68 = 0;
    return h;
}

fs::path find_ci(const fs::path& dir, const std::string& name) {
    auto up = [](std::string s) {
        for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return s;
    };
    if (dir.empty() || !fs::is_directory(dir)) return {};
    for (const auto& e : fs::directory_iterator(dir))
        if (up(e.path().filename().string()) == up(name)) return e.path();
    return {};
}

fs::path original_dir() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !fs::is_directory(v)) return {};
    return v;
}

}  // namespace

TEST_CASE("desk: ordinal 32's projection constants at 1024x768 (scene 100's header)") {
    const Projection p = projection(scene100_header(), 1024, 768, 1024, 768);
    CHECK(p.d28 == 1024);
    CHECK(p.d2c == 512);
    CHECK(p.d2e == 166);
    CHECK(p.d4c == 1024);
    CHECK(p.c6c == 0);
    CHECK(p.d48 == -324);
    CHECK(p.d44 == -324);
    // The vanishing point is (W/2, d2e): far depths converge there.
    CHECK(p.centre(p.d2e) == 512);
    // Record 18's depth 312 gives s = 780 and Yb = 40 (ispace.md "Front face").
    CHECK(p.scale(312) == 780);
    CHECK(p.yb(780) == 40);
}

TEST_CASE("desk: shade colours and ramps") {
    CHECK(shade_rgb({255, 255, 255}, 142) == Rgb{142, 142, 142});
    CHECK(shade_rgb({255, 255, 255}, 300) == Rgb{255, 255, 255});
    CHECK(shade_rgb({220, 202, 70}, 110) == Rgb{110, 101, 35});
    Header h = scene100_header();
    h.count = 3;
    h.colour = {Rgb{255, 255, 255}, Rgb{0, 224, 0}, Rgb{220, 202, 70}};
    h.dark = {Rgb{32, 32, 32}, Rgb{0, 100, 0}, Rgb{120, 108, 38}};
    h.divisor = {4, 4, 8};
    const auto ramps = shade_ramps(h);
    REQUIRE(ramps.size() == 3);
    CHECK(ramps[0].size() == 55);  // ispace.md "Colours": 55, 31 and 12
    CHECK(ramps[1].size() == 31);
    CHECK(ramps[2].size() == 12);
    CHECK(ramps[0][0] == Rgb{32, 32, 32});
}

TEST_CASE("desk: IGRAPHIC.DLL #6600 100 decoded, solved and drawn at 1024x768 (golden)") {
    const fs::path f = find_ci(original_dir(), "IGRAPHIC.DLL");
    if (f.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    const fmt::NeModule m = fmt::NeModule::load(f);
    const fmt::Scene6600 raw = fmt::load_scene6600(m, 100);
    const Scene s = decode(raw);
    // formats.md "IGRAPHIC.DLL #6600": 62 records, record 61 all zero; the header colours.
    REQUIRE(s.records.size() == 62);
    CHECK(s.records[61].type == 0);
    CHECK(s.header.count == 3);
    CHECK(s.header.colour[0] == Rgb{255, 255, 255});
    CHECK(s.header.colour[1] == Rgb{0, 224, 0});
    CHECK(s.header.colour[2] == Rgb{220, 202, 70});
    CHECK(s.header.divisor == std::array<int, 3>{4, 4, 8});
    CHECK(s.header.dark[0] == Rgb{32, 32, 32});
    CHECK(s.header.dark[1] == Rgb{0, 100, 0});
    CHECK(s.header.dark[2] == Rgb{120, 108, 38});
    CHECK(s.header.d5c == 500);
    CHECK(s.header.d60 == 217);
    CHECK(s.header.d64 == 1000);
    CHECK(s.header.d68 == 0);
    CHECK(s.records[0].resolved);
    // The slot records and the conditional speakers.
    CHECK(s.records[42].slot == 0);
    CHECK(s.records[17].slot == 1);
    CHECK(s.records[5].slot == 2);
    CHECK(s.records[18].slot == 3);
    CHECK(s.records[60].slot == 4);
    CHECK(s.records[60].colour == -1);
    CHECK(s.records[36].slot == -11);
    CHECK(s.records[37].slot == -10);
    CHECK(s.records[40].type == 3);
    CHECK(s.records[43].type == 3);
    for (int i = 20; i <= 35; ++i) CHECK(s.records[i].type == 2);

    const Render r = render(raw, 1024, 768, 1024, 768, Conditions{});
    CHECK(r.max_slot == 4);
    REQUIRE(r.slots.size() == 5);
    // ispace.md "Slots of scene 100" (left, top, right, bottom + 1).
    CHECK(r.slots[0] == Rect{350, 22, 672, 253});
    CHECK(r.slots[1] == Rect{197, 435, 300, 684});
    CHECK(r.slots[2] == Rect{281, 268, 744, 328});
    CHECK(r.slots[3] == Rect{42, 345, 978, 360});
    CHECK(r.slots[4] == Rect{812, 267, 873, 329});
    // The desk front (record 18) is one PatBlt in grey 142 over its slot.
    bool found = false;
    std::set<std::tuple<int, int, int>> colours;
    for (const Prim& p : r.prims) {
        colours.insert({p.c.r, p.c.g, p.c.b});
        if (p.record == 18 && p.kind == Prim::kFill && p.w == 936) {
            found = true;
            CHECK(p.x == 42);
            CHECK(p.y == 345);
            CHECK(p.h == 15);
            CHECK(p.c == Rgb{142, 142, 142});
        }
        CHECK(p.record != 2);    // condition byte 0xf4: never drawn
        CHECK(p.record != 36);   // no speakers without the sound conditions
        CHECK(p.record != 37);
        CHECK(p.record != 60);   // colour -1: the slot only
    }
    CHECK(found);
    // Fewer colours than palette entries; command 6 folds the near ones together.
    CHECK(colours.size() < 236);
    // Records in index order: the prims never go back to a lower record.
    int last = 0;
    for (const Prim& p : r.prims) {
        CHECK(p.record >= last);
        last = p.record;
    }
    Conditions both;
    both.async_wave = true;
    both.stereo = true;
    const Render rs = render(raw, 1024, 768, 1024, 768, both);
    std::set<int> recs;
    for (const Prim& p : rs.prims) recs.insert(p.record);
    CHECK(recs.count(36) == 1);
    CHECK(recs.count(37) == 1);
    CHECK(recs.count(38) == 1);
    CHECK(recs.count(39) == 1);
}

TEST_CASE("desk: meal window, day part and the formatter") {
    CHECK(meal_of({0, 8, 10}, 1200, 800, 1000, 1530) == 4);   // t = 811 in (800, 830)
    CHECK(meal_of({0, 8, 29}, 1200, 800, 1000, 1530) == 0);   // t = 830
    CHECK(meal_of({0, 12, 0}, 1200, 800, 1000, 1530) == 3);   // t = 1201
    CHECK(meal_of({0, 10, 5}, 1200, 800, 1000, 1530) == 1);
    CHECK(meal_of({0, 15, 31}, 1200, 800, 1000, 1530) == 2);
    CHECK(meal_of({0, 7, 59}, 1200, 800, 1000, 1530) == 0);
    CHECK(meal_graphic(1, 0) == 272);
    CHECK(meal_graphic(2, 0) == 274);
    CHECK(meal_graphic(3, 0) == 278);
    CHECK(meal_graphic(4, 0) == 70);
    CHECK(meal_graphic(4, 1) == 72);
    CHECK(day_part(3) == 0);
    CHECK(day_part(11) == 0);
    CHECK(day_part(12) == 1);
    CHECK(day_part(18) == 2);
    CHECK(day_part(2) == 3);
    std::map<int, std::string> strings{{597, "Saturday"}, {639, "Morning"}, {642, "Late"}, {648, "FOOD"}, {235, "Coffee^P"}};
    const Lookup lang = [&](int id) { return strings[id]; };
    const std::string g = format("IT'S %<%S %S%>%<%S%>, %s!%NI HOPE", {1, 597, 639, 0, 642, "Pilot"}, lang);
    CHECK(g == "IT'S Saturday Morning, Pilot!\r\nI HOPE");
    const std::string late = format("IT'S %<%S %S%>%<%S%>, %s!", {0, 597, 639, 1, 642, "Pilot"}, lang);
    CHECK(late == "IT'S Late, Pilot!");
    const std::string launch = format("%N %NPREPARE %<TO LAUNCH%>%<TO LUNCH%>...%N%<GOOD LUCK%>%<ENJOY YOUR %S%>, %s! ",
                                      {1, 0, 0, 1, 235, "P"}, lang);
    CHECK(launch == "\r\n \r\nPREPARE TO LAUNCH...\r\nENJOY YOUR Coffee, P! ");
}

namespace {

struct FakeCanvas final : Canvas {
    std::vector<std::string> calls;
    std::string typed;
    bool pad = false;
    void palette_save(int s, int b) override { calls.push_back("save " + std::to_string(s) + " " + std::to_string(b)); }
    void palette_scheme(int c, int m) override { calls.push_back("scheme " + std::to_string(c) + " " + std::to_string(m)); }
    void palette_restore(int s) override { calls.push_back("restore " + std::to_string(s)); }
    void palette_fade(int m, int p) override { calls.push_back("fade " + std::to_string(m) + " " + std::to_string(p)); }
    void scene(const Render&) override { calls.push_back("scene"); }
    void monitor(const Rect&) override { calls.push_back("monitor"); }
    void tower(const Rect&) override { calls.push_back("tower"); }
    void meal(int g, const Rect&) override { calls.push_back("meal " + std::to_string(g)); }
    void scene_to_screen() override { calls.push_back("show"); }
    void screen_to_scene(int, int, int, int) override { calls.push_back("save text"); }
    void scene_to_screen(int, int, int, int) override { calls.push_back("restore text"); }
    void black(int, int, int w, int h) override {
        if (w == 1024 && h == 768) calls.push_back("blackness");
    }
    void block(int, int, int, int, Colour) override {}
    void text_font(int h, int w) override { calls.push_back("font " + std::to_string(h) + " " + std::to_string(w)); }
    void text_char(int, int, char c) override { typed += c; }
    int text_extent(const std::string& s) override { return 6 * static_cast<int>(s.size()); }
    void scroll_up(const Rect&, int) override { calls.push_back("scroll"); }
    void flash(int, int) override { calls.push_back("flash"); }
    void make_pad(int w, int h) override { calls.push_back("pad " + std::to_string(w) + " " + std::to_string(h)); pad = true; }
    std::string main_ship;
    std::vector<LaunchDraw> draws;
    int make_ship(const std::string& n) override { main_ship = n; return 32; }
    void rise(int, int, int, int, int) override {}
    void free_rise() override { pad = false; calls.push_back("free rise"); }
    void pad_to_screen(int, int) override { calls.push_back("pad shown"); }
    void launch_frame(const LaunchDraw& d) override { draws.push_back(d); }
    void free_all() override { calls.push_back("free all"); }
    bool pad_alive() override { return pad; }
};

struct FakeHost final : Host {
    Tick t = 1000;
    std::map<std::string, int> ints{{"ShowLaunch", 2}};
    std::map<std::string, std::string> strs{{"Restore", " NEW GAME"}, {"ThePlayer", "Pilot"}};
    std::vector<int> sounds;
    Clock c{6, 9, 15};  // Saturday 09:15: morning, no meal
    Tick now() override { return t; }
    void sound(int id) override { sounds.push_back(id); }
    void sound_at(int id, int, int, int) override {
        if (id != 0x2b) sounds.push_back(id);
    }
    void music(int) override {}
    void pump() override {}
    void pump_until(Tick u) override { if (static_cast<std::int32_t>(u - t) > 0) t = u; }
    void busy_until(Tick u) override { if (static_cast<std::int32_t>(u - t) > 0) t = u; }
    std::string lang(int id) override {
        switch (id) {
        case 3032: return "IT'S %<%S %S%>%<%S%>, %s!%NI HOPE YOU'RE READY TO PLAY..%N%N";
        case 3030: return "%N %NPREPARE %<TO LAUNCH%>%<TO LUNCH%>...%N%<GOOD LUCK%>%<ENJOY YOUR %S%>, %s! ";
        case 597: return "Saturday";
        case 639: return "Morning";
        default: return "x";
        }
    }
    std::string lang_text(int, int) override { return "YOUR COMPUTER HAS BEEN INVADED!\r\n"; }
    std::string ini(const char* k, const char* d) override { auto i = strs.find(k); return i == strs.end() ? d : i->second; }
    int ini_int(const char* k, int d) override { auto i = ints.find(k); return i == ints.end() ? d : i->second; }
    Clock clock() override { return c; }
    int rand() override { return 0; }
    int calibrate_value() override { return 14000; }
    Conditions conditions() override { return {}; }
    Render render(int, int W, int H) override {
        Render r;
        r.proj.W = W;
        r.proj.H = H;
        r.max_slot = 4;
        r.slots = {Rect{350, 22, 672, 253}, Rect{197, 435, 300, 684}, Rect{281, 268, 744, 328},
                   Rect{42, 345, 978, 360}, Rect{812, 267, 873, 329}};
        return r;
    }
    void todo(const std::string&, const char*) override {}
};

}  // namespace

TEST_CASE("desk: the step table of seg15:0x10be with a fake clock") {
    FakeCanvas cv;
    FakeHost h;
    Desk d(cv, h);
    d.enter(h.t, 1024, 768, 768);
    CHECK(d.step() == 0);
    CHECK(cv.calls[0] == "save 0 1");
    CHECK(cv.calls[1] == "scheme 109 2");
    // Step 0 waits 1100 ms (DGROUP:0x3cc4).
    h.t += 1100;
    CHECK_FALSE(d.frame(h.t));
    CHECK(d.step() == 0);
    h.t += 1;
    d.frame(h.t);
    CHECK(d.step() == 1);
    CHECK(d.typing().h == 17);  // (322 - 63)/15 at 1024 wide
    CHECK(d.typing().w == 6);
    CHECK(d.text().rfind("IT'S SATURDAY MORNING, PILOT!\r\n", 0) == 0);
    // The first character 500 ms after the init, then 2 per 40 ms tick.
    h.t += 499;
    d.frame(h.t);
    CHECK(cv.typed.empty());
    h.t += 1;
    d.frame(h.t);
    CHECK(cv.typed == "IT");
    d.frame(h.t + 39);
    CHECK(cv.typed == "IT");
    // Typing runs to the CR whose index + 2 holds a space: step 2.
    for (int i = 0; i < 2000 && d.step() == 1; ++i) {
        h.t += 40;
        d.frame(h.t);
    }
    CHECK(d.step() == 2);
    const std::size_t typed_at_launch = cv.typed.size();
    d.frame(h.t);  // step 2: sound 0x4b, the pad and ship, the rise's first call
    CHECK(d.step() == 3);
    CHECK(cv.pad);
    // Step 3 repeats the rise until its offset is down to 5: (128 + 62 - 5)/2 + 1 calls.
    int rises = 0;
    while (d.step() == 3 && rises < 1000) {
        d.frame(h.t);
        ++rises;
    }
    CHECK(rises == (128 + 0x3e - 5 + 1) / 2 + 1);
    CHECK(d.step() == 4);
    CHECK_FALSE(cv.pad);
    // Step 4 types the launch line to the NUL: step 5.
    for (int i = 0; i < 2000 && d.step() == 4; ++i) {
        h.t += 40;
        d.frame(h.t);
    }
    CHECK(d.step() == 5);
    CHECK(cv.typed.size() > typed_at_launch);
    CHECK(cv.typed.find("PREPARE TO LAUNCH...") != std::string::npos);
    CHECK(cv.typed.find("GOOD LUCK, PILOT!") != std::string::npos);
    const Tick before5 = h.t;
    d.frame(h.t);  // step 5: 800 ms busy-wait, seg15:0x0(2), deadline + 950
    CHECK(d.step() == 6);
    CHECK(h.t == before5 + 800);
    d.frame(h.t + 950);
    CHECK(d.step() == 6);
    h.t += 951;
    d.frame(h.t);
    CHECK(d.step() == 7);
    int launches = 0;
    while (d.step() == 7 && launches < 5000) {
        d.frame(h.t);
        ++launches;
    }
    CHECK(d.step() == 8);
    CHECK(launches > 10);
    d.frame(h.t);
    CHECK(d.step() == 9);
    CHECK(d.frame(h.t));
    // Step 9: blackness, the 450 ms wait, scheme 8 and set 0 back.
    const auto& c = cv.calls;
    const auto at = std::find(c.begin(), c.end(), "blackness");
    REQUIRE(at != c.end());
    CHECK(*(at + 1) == "scheme 8 0");
    CHECK(*(at + 2) == "restore 0");
    // Sounds in order: entry flash 0x14, 0x4b, 5 (rise), 0x20, 0x1c, 0x4c.
    std::vector<int> want{0x14, 0x4b, 5, 0x20, 0x1c, 0x4c};
    CHECK(h.sounds == want);
}

TEST_CASE("desk: ESC jumps to step 9; Space at step 1 skips to the launch; ShowLaunch = 0") {
    {
        FakeCanvas cv;
        FakeHost h;
        Desk d(cv, h);
        d.enter(h.t, 1024, 768, 768);
        h.t += 1101;
        d.frame(h.t);
        REQUIRE(d.step() == 1);
        d.space(h.t);
        CHECK(d.step() == 2);
        d.escape();
        CHECK(d.step() == 9);
        CHECK(d.frame(h.t));
    }
    {
        FakeCanvas cv;
        FakeHost h;
        h.ints["ShowLaunch"] = 0;
        Desk d(cv, h);
        d.enter(h.t, 1024, 768, 768);
        CHECK(d.step() == 9);
        CHECK(std::find(cv.calls.begin(), cv.calls.end(), "scene") == cv.calls.end());
        CHECK(d.frame(h.t));
        // Nothing was saved, so set 0 is not restored [DESIGN].
        CHECK(std::find(cv.calls.begin(), cv.calls.end(), "restore 0") == cv.calls.end());
    }
    {
        // Breakfast: the croissant (#4000 70 or 72) and "ENJOY YOUR FOOD".
        FakeCanvas cv;
        FakeHost h;
        h.c = {6, 8, 10};
        Desk d(cv, h);
        d.enter(h.t, 1024, 768, 768);
        CHECK(d.meal() == 4);
        CHECK(std::find(cv.calls.begin(), cv.calls.end(), "meal 70") != cv.calls.end());
        CHECK(d.monitor_text().find("ENJOY YOUR X, PILOT!") != std::string::npos);  // string 648 is "x" here
    }
}

TEST_CASE("desk: seg15:0x9e6's frame sizes for s = 33") {
    const auto f = ship_frames(33);
    const int want_h[16] = {132, 118, 105, 92, 80, 68, 58, 48, 40, 32, 25, 29, 32, 32, 31, 28};
    int y = 0;
    for (int i = 0; i < 16; ++i) {
        CHECK(f[i].w == 132 - 7 * i);
        CHECK(f[i].h == want_h[i]);
        CHECK(f[i].y == y);
        y += f[i].h;
    }
    CHECK(f[15].w == 27);  // the oracle's last frame, 27 x 28
    const auto g = ship_frames(32);
    CHECK(g[0].w == 128);
    CHECK(g[1].w == 128 - (128 - 17) / 15);
}

TEST_CASE("desk: seg15:0x0's step rule, 108 and 50 calls for y0 = 574") {
    for (const bool fast : {true, false}) {
        const int y0 = 574, ye = 60;
        int pos10 = 10 * y0, f = 0, calls = 0;
        std::vector<int> steps;
        while (calls < 1000) {
            f = launch_frame_of(y0, ye, pos10, f);
            const int speed = launch_speed(f, fast);
            const int before = pos10;
            pos10 += launch_advance10(speed, fast);
            steps.push_back(before - pos10);  // tenths of a pixel
            ++calls;
            if (10 * ye > pos10) break;
        }
        CHECK(calls == (fast ? 108 : 50));
        CHECK(f == 15);
        if (fast) {
            CHECK(steps.front() == 64);  // about 6 px, falling to 4
            CHECK(steps.back() == 3 * 90 / 7);
        } else {
            CHECK(steps.front() == 107);  // about 11 px, then 10
            CHECK(steps.back() == 5 * 143 / 7);
        }
    }
    CHECK(launch_speed(0, true) == 150);
    CHECK(launch_speed(40, true) == 9);
    CHECK(launch_speed(15, false) == 143);
    CHECK(launch_advance10(150, true) == -64);
    CHECK(launch_advance10(150, false) == -107);
}

TEST_CASE("desk: the pad size at play height 683 and the launch drawing rectangles") {
    FakeCanvas cv;
    FakeHost h;
    Desk d(cv, h);
    d.enter(h.t, 1024, 768, 683);
    h.t += 1101;
    d.frame(h.t);
    d.space(h.t);
    REQUIRE(d.step() == 2);
    d.frame(h.t);
    // k = trunc(4 · 683/3) - 1100 = -190: width 159, DGROUP:0x3c9a 74, bitmap 159 x 126.
    CHECK(std::find(cv.calls.begin(), cv.calls.end(), "pad 159 126") != cv.calls.end());
    CHECK(cv.main_ship == "EAGLE");
    while (d.step() == 3) d.frame(h.t);
    for (int i = 0; i < 3000 && d.step() == 4; ++i) {
        h.t += 40;
        d.frame(h.t);
    }
    REQUIRE(d.step() == 5);
    d.frame(h.t);
    REQUIRE(!cv.draws.empty());
    const LaunchDraw& first = cv.draws.front();
    // s = 32: the work DC is 128 x 143 at (512 - 64, y0), y0 = 768 - 128 - 62.
    CHECK(first.ship == Rect{448, 578, 576, 721});
    CHECK(first.f == 0);
    CHECK(first.scene);
    CHECK(first.column == Rect{350, 22, 672, 360});
    CHECK(first.pad == Rect{512 - 79, 768 - 74 - 52, 512 - 79 + 159, 768});
}
