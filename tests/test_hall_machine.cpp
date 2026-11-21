// State 8's phase machine (src/hall/hall.h Hall: seg16:0x6928 and the routines it calls) on fakes,
// and the way in seg9:0x6c62-0x6cb3 (world/wave_end.cpp wave_over): the phases in order, the holds
// of 1500, 4000, 1550 and 2000 ms, the typewriter's stops at the CR-space pairs and the NUL, its 40
// ms pace and 1900 ms pauses, the descent onto the disc and the launch to the debriefing.
#include <doctest.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "hall/hall.h"
#include "shell/game_state.h"
#include "world/law.h"
#include "world/pilot.h"
#include "world/visit.h"
#include "world/wave_end.h"
#include "world/world.h"

using namespace is;
using namespace is::hall;

namespace {

std::string lang(int id) {
    static const std::map<int, std::string> m = {
        {234, "%< AND %>%<, %>%# OF %S"}, {723, "THIS IS YOUR %s OFFENSE, "}, {725, "A FINE OF %s ICONS"},
        {788, "Officer"}, {908, "THIS IS THE HALL, %s.%N%N"}, {911, "FATE. %N%N"}, {912, "AFTER OBSERVING YOU"},
        {913, " ARRESTED YOU AT "}, {914, "CHARGED WITH "}, {915, "GUILTY"}, {916, "STAY OUT"},
        {1100, " Removing Evidence"}, {1158, "CREDIT %s ICONS. "}, {2095, "ST"}, {2519, "COUNT"},
    };
    auto it = m.find(id);
    return it == m.end() ? std::string() : it->second;
}

struct FakeScreen final : Screen {
    std::vector<std::string> calls;
    int jet_calls = 0, jet_landing = 3;
    std::optional<desk::Rect> landing(int, int, int, const std::string&, bool render) override {
        calls.push_back("landing");
        if (!render) return std::nullopt;
        return desk::Rect{200, 100, 600, 400};
    }
    void jet_frames(bool dead, shell::Rng&) override { calls.push_back(dead ? "jet dead" : "jet"); }
    int incoming(int first, int, int, int) override {
        if (first) return 0;
        return ++jet_calls >= jet_landing ? 1 : 0;
    }
    void two_frames() override { calls.push_back("two"); }
    void black(int, int) override {}
    HallScene hall_scene(int, int) override {
        calls.push_back("115");
        return HallScene{desk::Rect{300, 100, 700, 420}, 600};
    }
    void ship_frame(int, int, int) override { calls.push_back("ship"); }
    std::vector<int> descent_y;
    void descent_frame(int, int y, int, int, int) override { descent_y.push_back(y); }
    void lend_ship() override {}
    void free_scene() override { calls.push_back("free"); }
};

struct FakeCanvas final : desk::Canvas {
    std::string typed;
    int scrolls = 0, launches = 0, flashes = 0, monitors = 0;
    void palette_save(int, int) override {}
    void palette_scheme(int, int) override {}
    void palette_restore(int) override {}
    void palette_fade(int, int) override {}
    void scene(const desk::Render&) override {}
    void monitor(const desk::Rect&) override { ++monitors; }
    void tower(const desk::Rect&) override {}
    void meal(int, const desk::Rect&) override {}
    void scene_to_screen() override {}
    void screen_to_scene(int, int, int, int) override {}
    void scene_to_screen(int, int, int, int) override {}
    void black(int, int, int, int) override {}
    void block(int, int, int, int, desk::Colour) override {}
    void text_font(int, int) override {}
    void text_char(int, int, char c) override { typed += c; }
    int text_extent(const std::string& s) override { return 8 * static_cast<int>(s.size()); }
    void scroll_up(const desk::Rect&, int) override { ++scrolls; }
    void flash(int, int) override { ++flashes; }
    void make_pad(int, int) override {}
    int make_ship(const std::string&) override { return 33; }
    void rise(int, int, int, int, int) override {}
    void free_rise() override {}
    void pad_to_screen(int, int) override {}
    void launch_frame(const desk::LaunchDraw&) override { ++launches; }
    void free_all() override {}
    bool pad_alive() override { return false; }
};

struct FakeHost final : Host {
    Tick t = 100000, pass = 100000;
    std::vector<int> sounds;
    int debriefs = 0;
    int pops = 0;
    Tick now() override { return t; }
    Tick pass_now() override { return pass; }
    void sound(int id) override { sounds.push_back(id); }
    void sound_at(int id, int, int, int) override {
        if (id == 0x2b) ++pops;
        else sounds.push_back(id);
    }
    void music(int, int) override {}
    void pump_until(Tick u) override {
        if (static_cast<std::int32_t>(u - t) > 0) t = u;
    }
    void pump() override {}
    bool ctrl_held() override { return false; }
    int quick_intro() override { return 0; }
    int calibrate() override { return 14000; }
    int depth() override { return 8; }
    std::string lang(int id) override { return ::lang(id); }
    std::string pilot() override { return "PILOT"; }
    Clock clock_now{13, 13, 10, 15, 126};
    Clock clock() override { return clock_now; }
    std::string main_ship() override { return "EAGLE"; }
    void close_hud() override {}
    void watch_windows(int) override {}
    void debriefing() override { ++debriefs; }
    void set_state(int) override {}
    void todo(const std::string&, const char*) override {}
};

struct Fixture {
    world::World w;
    FakeScreen scr;
    FakeCanvas canvas;
    FakeHost host;
    shell::Rng rng;
    Hall hall{w, scr, canvas, host, rng};
    Fixture() {
        world::visit_state() = world::VisitState{};
        world::shop().alive = 1;
        world::law_words().ba162 = 3;
        w.player = 0;
        w.last_ship = 3;
        for (auto& s : w.ships) s.roster_index = -1;
        w.ships[0].roster_index = 0;
        w.roster.resize(1);
        w.roster[0].size = 33;
        w.roster[0].gap = 5;
        world::set_ship_d(w.ships[0], 0xdc, 150);
        world::set_ship_b(w.ships[0], 0x5e, 0);
        world::set_ship_b(w.ships[3], 0x5e, 1);
        w.ba14d[2] = 1;
        w.ba161 = 2;
    }
    // One state-8 frame at 72 Hz: DGROUP:0x1e50 and the clock move 14 ms.
    void step() {
        host.t += 14;
        host.pass = host.t;
        hall.frame(0);
    }
};

}  // namespace

TEST_CASE("hall: the phases from the landing to the debriefing") {
    Fixture f;
    f.hall.enter(1024, 768, 683);
    // Phase 0 ran inside the entry: scene 110 and the jet, sounds 0x6e and 0x1c.
    CHECK(f.hall.words().phase == 1);
    REQUIRE(f.scr.calls.size() >= 2);
    CHECK(f.scr.calls[0] == "landing");
    CHECK(f.scr.calls[1] == "jet");
    CHECK(f.host.sounds == std::vector<int>{0x6e, 0x1c});
    CHECK(f.hall.words().monitor == desk::Rect{200, 100, 600, 400});
    CHECK(world::visit_state().end_y == 120);  // DGROUP:0x3caa + 20
    std::map<int, Tick> start;
    start[1] = f.host.t;
    int guard = 0;
    while (f.host.debriefs == 0 && guard++ < 20000) {
        const int before = f.hall.words().phase;
        if (before == 1 && world::visit_state().descent == 1 && f.scr.descent_y.size() == 1) {
            // seg15:0x374c(1): scene 115, the end y on the disc and the start under the monitor.
            CHECK(world::visit_state().end_y == 600 - 132);
            CHECK(f.hall.words().w92ba == 600);
            CHECK(f.scr.descent_y.front() == 420 - 132);
        }
        f.step();
        const int after = f.hall.words().phase;
        if (after != before) start[after] = f.host.t;
        if (before == 3 && after == 4) {
            // Phase 3 stops before the CR of the first "\r\n \r\n" (DGROUP:0x1188).
            const std::string& t = f.hall.words().text;
            const std::size_t i = f.hall.words().index;
            REQUIRE(i + 2 < t.size());
            CHECK(t[i] == '\r');
            CHECK(t[i + 2] == ' ');
            CHECK(t.substr(0, i).find("CHARGED WITH 1 COUNT OF  REMOVING EVIDENCE. ") != std::string::npos);
        }
        if (before == 5 && after == 6) {
            const std::string& t = f.hall.words().text;
            CHECK(t.substr(0, f.hall.words().index).rfind("CREDIT 0.0 ICONS. ") != std::string::npos);
        }
    }
    REQUIRE(f.host.debriefs == 1);
    // The holds, measured on the clock the machine waits on.
    // A phase starts at the frame after the one that raised it (14 ms), then holds.
    CHECK(start[5] - start[4] == 14 + 1500);      // phase 4, inside one frame
    CHECK(start[7] - start[6] == 14 + 4004 + 1550);  // phase 6: 4000 ms from its first frame, seg15:0xcb0's 1550
    CHECK(start[9] - start[8] == 14 + 1500);      // phase 8
    CHECK(f.hall.words().done == 1);              // DGROUP:0x92a at the NUL
    CHECK(f.canvas.typed.find("STAY OUT, PILOT.") != std::string::npos);
    CHECK(f.canvas.launches > 0);
    CHECK(f.host.pops > 100);
    // The end of the descent: DGROUP:0x92d8 = -1, sound 9, 1000 ms, then phase 2.
    CHECK(std::find(f.host.sounds.begin(), f.host.sounds.end(), 9) != f.host.sounds.end());
    CHECK(std::find(f.host.sounds.begin(), f.host.sounds.end(), 0x4b) != f.host.sounds.end());
    CHECK(std::find(f.host.sounds.begin(), f.host.sounds.end(), 0x15) != f.host.sounds.end());
    // The fine was taken at phase 2.
    CHECK(world::ship_d(f.w.ships[0], 0xdc) == 0u);
    CHECK(f.w.ba157[2] == 1);
}

TEST_CASE("hall: the typewriter's pace and pauses (seg15:0x1584 in state 8)") {
    Fixture f;
    f.hall.enter(1024, 768, 683);
    Words& k = f.hall.words();
    k.phase = 3;
    k.monitor = {300, 100, 700, 420};
    k.mon_x = 300;
    k.mon_y = 100;
    k.w92d2 = 305;
    k.text = "AB\r\n\r\nCD\r\n EF";
    k.step = 0;
    f.hall.type(1);  // the init: 500 ms
    CHECK(k.next == f.host.pass + 500);
    f.host.pass += 499;
    f.hall.type(0);
    CHECK(f.canvas.typed.empty());
    f.host.pass += 1;
    f.hall.type(0);  // two characters
    CHECK(f.canvas.typed == "AB");
    f.host.pass += 40;
    f.hall.type(0);  // the CR LF, then the next CR follows an LF: the 1900 ms pause
    CHECK(k.index == 4);
    CHECK(k.next == f.host.pass + 1900);
    f.host.pass += 1899;
    f.hall.type(0);
    CHECK(k.index == 4);
    f.host.pass += 1;
    f.hall.type(0);  // the second CR LF and 'C'
    CHECK(f.canvas.typed == "ABC");
    f.host.pass += 40;
    f.hall.type(0);  // 'D', then the next CR has a space at +2: the phase rises
    CHECK(f.canvas.typed == "ABCD");
    CHECK(k.phase == 4);
    k.phase = 5;
    f.host.pass += 40;
    f.hall.type(0);  // the CR LF and the space
    f.host.pass += 40;
    f.hall.type(0);  // E, F, then the NUL
    CHECK(f.canvas.typed == "ABCD EF");
    CHECK(k.phase == 6);
    CHECK(k.done == 1);
}

TEST_CASE("hall: seg9:0x6c62-0x6cb3, the way in") {
    world::World w;
    shell::GameState s;
    struct Host final : world::WaveEndHost {
        int halls = 0;
        shell::GameState* s = nullptr;
        int state_seen = -1;
        void hall_of_justice() override {
            ++halls;
            state_seen = s->state();
        }
    } h;
    h.s = &s;
    w.player = 0;
    w.last_ship = 4;
    w.w93a4 = 1;
    w.w9346 = 5;
    for (int i = 0; i < 5; ++i) {
        w.ships[static_cast<std::size_t>(i)].bec = 1;
        w.ships[static_cast<std::size_t>(i)].design_index = static_cast<std::int16_t>(i);
    }
    for (int d = 2; d <= 4; ++d) w.design_ratings[static_cast<std::size_t>(d)].team = 7;
    world::set_ship_b(w.ships[2], 0x5e, 0);  // a Terminator against the player
    world::set_ship_w(w.ships[2], 0x56, 0);
    world::set_ship_b(w.ships[3], 0x5e, 1);  // the escort
    world::set_ship_w(w.ships[3], 0x56, 0);
    world::set_ship_b(w.ships[4], 0x5e, 0);  // a Terminator against ship 1
    world::set_ship_w(w.ships[4], 0x56, 1);
    world::WaveEndCtx c{w, s, h};
    world::wave_over(c);
    CHECK(h.halls == 1);
    CHECK(h.state_seen == shell::kJustice);
    CHECK(s.state() == shell::kJustice);
    CHECK(w.w9346 == -1);
    CHECK(w.ships[2].bec == 0);
    CHECK(w.ships[3].bec == 1);
    CHECK(w.ships[4].bec == 1);
    CHECK(w.ships[0].bec == 1);
}
