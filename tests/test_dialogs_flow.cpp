// The state-0 game-start flow of src/intro (Intro::key_up with a GameStart, shell.md "From ESC
// to the wave" and "Input") driven by the intro's recording fakes and a recording GameStart.
#include <string>
#include <vector>

#include "doctest.h"
#include "intro/intro.h"
#include "intro_fakes.h"

using namespace intro_test;
using is::intro::Intro;

namespace {

class FakeGameStart : public is::intro::GameStart {
public:
    std::vector<std::string> calls;
    std::vector<int> results;  // the dialog results to return, in order (0 when empty)
    int restore_result = 1;
    int quick = 0;
    int accept_value = 0;
    int order_result = 0;
    bool order_opens = true;
    bool held = false;
    int demo_restore = -1;

    int next() {
        if (results.empty()) return 0;
        const int r = results.front();
        results.erase(results.begin());
        return r;
    }
    int dialog(int cmd) override {
        calls.push_back("dialog " + std::to_string(cmd));
        return next();
    }
    int order(bool* opened) override {
        calls.push_back("order");
        *opened = order_opens;
        return order_opens ? order_result : 0;
    }
    int accept() override { return accept_value; }
    int restore(int edition, int player) override {
        calls.push_back("restore " + std::to_string(edition) + " " + std::to_string(player));
        return restore_result;
    }
    void load_table() override { calls.push_back("table"); }
    void delete_demo_save() override { calls.push_back("delete demo"); }
    void stamp_dates() override { calls.push_back("stamp"); }
    void set_demo_restore(int v) override { demo_restore = v; }
    int quick_intro() override { return quick; }
    void clear_restored() override { calls.push_back("clear restored"); }
    void clear_wave_if_new() override { calls.push_back("clear wave"); }
    std::string drive_text() override { return "C:Unknown_HD"; }
    void open_navigation() override { calls.push_back("navigation"); }
    void keep_key() override { calls.push_back("keep key"); }
    bool keys_held(Tick) override { return held; }
    void present() override { calls.push_back("present"); }
    bool called(const std::string& c) const {
        for (const auto& s : calls)
            if (s == c) return true;
        return false;
    }
};

// A host whose edition check gives a chosen DGROUP:0x920c.
class EditionHost : public FakeHost {
public:
    int edition = 0;
    is::shell::Edition edition_check() override {
        is::shell::Edition e;
        e.edition = edition;
        e.day = 12;
        return e;
    }
};

struct Rig {
    FakeCanvas canvas;
    EditionHost host;
    is::shell::Rng rng;
    FakeGameStart gs;
    Intro intro{canvas, host, rng};
    explicit Rig(int edition = 0) {
        host.edition = edition;
        host.strings[2141] = "Analysing Drive ";
        host.strings[2142] = "(Starting Demo)";
        is::intro::set_game_start(&gs);
        intro.enter(host.t);
    }
    ~Rig() { is::intro::set_game_start(nullptr); }
    void calibrated() {
        run_until(intro, host, [&] { return host.calibrated(); });
    }
    void until_phase(int p) {
        run_until(intro, host, [&] { return intro.phase() == p; });
    }
};

}  // namespace

TEST_CASE("dialogs flow: ESC waits for Calibrate and opens Restore Game below phase 6") {
    Rig r;
    r.intro.key_up(0x1b, r.host.t);  // during Calibrate (DGROUP:0x120e != 0)
    CHECK(r.gs.calls.empty());
    CHECK(r.host.sounds.back() == 0x6e);
    r.calibrated();
    r.gs.results = {0};  // Cancel
    r.host.sounds.clear();
    r.intro.key_up(0x1b, r.host.t);
    CHECK(r.gs.calls == std::vector<std::string>{"clear restored", "dialog 1"});
    CHECK(r.host.sounds == std::vector<int>{0x6e, 0x13});
    // a cancelled dialog below phase 4 ends the phase now (seg10:0x8556)
    CHECK(r.intro.deadline() == r.host.t);
    CHECK(r.intro.phase() == 0);
    r.intro.frame(r.host.t += 13, false);
    CHECK(r.intro.phase() == 1);
}

TEST_CASE("dialogs flow: NEW GAME through the desk scene's exit into Navigation") {
    Rig r;
    r.calibrated();
    r.until_phase(2);
    r.gs.results = {1};
    r.canvas.ops.clear();
    const Tick before = r.host.t;
    r.intro.key_up(0x1b, r.host.t);
    CHECK(r.gs.calls == std::vector<std::string>{"clear restored", "dialog 1", "table",
                                                 "clear wave", "present", "navigation"});
    CHECK(r.host.has_todo("desk scene"));
    CHECK(r.intro.phase() == 7);
    CHECK(r.host.music_codes.back() == -1);
    CHECK(r.host.t == before + 450);  // step 9's wait
    // the deadline of the non-zero result: now + 40000 (the wait came after it)
    CHECK(r.intro.deadline() == before + 40000);
    REQUIRE(r.canvas.ops.size() >= 3);
    CHECK(r.canvas.ops[0] == "black 0 0 1024 768");
    CHECK(r.canvas.ops[1] == "scheme 8 0");
    CHECK(r.canvas.ops[2] == "font_text 0 6 0000ff00 512 344 Analysing Drive C:Unknown_HD...");
    CHECK(r.canvas.count_prefix("font_text") == 1);  // a player game: no "(Starting Demo)"
    // phase 7 runs no frame work
    r.canvas.ops.clear();
    r.intro.frame(r.host.t += 13, false);
    CHECK(r.canvas.count_prefix("blit") == 0);
    // ESC in phase 7 does nothing more
    r.gs.calls.clear();
    r.intro.key_up(0x1b, r.host.t);
    CHECK(r.gs.calls.empty());
}

TEST_CASE("dialogs flow: QuickIntro 2 goes to phase 7 at once") {
    Rig r;
    r.calibrated();
    r.gs.quick = 2;
    r.gs.results = {1};
    r.canvas.ops.clear();
    r.intro.key_up(0x1b, r.host.t);
    CHECK(r.intro.phase() == 7);
    CHECK_FALSE(r.host.has_todo("desk scene"));
    CHECK(r.canvas.ops == std::vector<std::string>{
                             "font_text 0 6 0000ff00 512 344 Analysing Drive C:Unknown_HD..."});
    CHECK(r.gs.called("navigation"));
}

TEST_CASE("dialogs flow: a restored game and the refused restore") {
    Rig r;
    r.calibrated();
    r.gs.results = {2};
    r.gs.restore_result = 0;  // the limited version's 2325, or no file
    r.intro.key_up(0x1b, r.host.t);
    CHECK(r.gs.calls == std::vector<std::string>{"clear restored", "dialog 1", "restore 0 1",
                                                 "keep key"});
    CHECK(r.intro.phase() == 0);
    r.gs.calls.clear();
    r.gs.results = {2};
    r.gs.restore_result = 1;
    r.gs.quick = 2;
    r.intro.key_up(0x1b, r.host.t);
    // seg10:0x38e0 runs only for the other results
    CHECK(r.gs.calls == std::vector<std::string>{"clear restored", "dialog 1", "restore 0 1",
                                                 "clear wave", "present", "navigation"});
    CHECK(r.intro.phase() == 7);
}

TEST_CASE("dialogs flow: Important Information for edition 2 and the key hold") {
    Rig r(2);
    r.calibrated();
    r.gs.results = {1};
    r.host.sounds.clear();
    r.intro.key_up(0x1b, r.host.t);
    CHECK(r.gs.calls == std::vector<std::string>{"dialog 11"});  // and nothing else
    CHECK(r.host.sounds == std::vector<int>{0x6e, 0x13});        // [BYTES seg11:0x7a02 seg11:0x7a39]
    r.gs.calls.clear();
    r.gs.accept_value = 42;
    r.gs.results = {0};
    r.intro.key_up(0x1b, r.host.t);
    CHECK(r.gs.called("dialog 1"));
    r.gs.calls.clear();
    r.gs.held = true;  // DGROUP:0x3b94 not reached
    r.intro.key_up(0x1b, r.host.t);
    r.intro.key_up(0x20, r.host.t);
    CHECK(r.gs.calls.empty());
}

TEST_CASE("dialogs flow: R opens Order for editions 0 and 1 only") {
    Rig r;
    r.calibrated();
    r.until_phase(2);
    r.intro.key_up('R', r.host.t);
    CHECK(r.gs.calls == std::vector<std::string>{"order"});
    CHECK(r.host.sounds.back() == 0x51);
    CHECK(r.intro.deadline() == r.host.t);  // a cancelled Order ends the title
    // after the 20th dialog seg10:0x80a8 opens nothing and the deadline stays
    r.gs.calls.clear();
    r.gs.order_opens = false;
    r.until_phase(3);
    const Tick d = r.intro.deadline();
    r.intro.key_up('R', r.host.t);
    CHECK(r.intro.deadline() == d);
    Rig reg(-1);
    reg.calibrated();
    reg.intro.key_up('R', reg.host.t);
    CHECK(reg.gs.calls.empty());
}

TEST_CASE("dialogs flow: S, D and C") {
    Rig r;
    r.calibrated();
    r.gs.results = {0};
    r.intro.key_up('S', r.host.t);
    CHECK(r.gs.calls == std::vector<std::string>{"dialog 4"});
    CHECK(r.intro.phase() == 0);
    r.gs.calls.clear();
    r.gs.results = {1};
    r.intro.key_up('S', r.host.t);
    CHECK(r.gs.calls == std::vector<std::string>{"dialog 4", "delete demo", "table", "stamp"});
    CHECK(r.intro.phase() == 5);
    CHECK(r.gs.demo_restore == 0);  // S stores 0: only D stores 1 [BYTES seg11:0x79bd]
    CHECK(r.intro.deadline() == r.host.t);
    Rig d;
    d.calibrated();
    d.intro.key_up('D', d.host.t);
    CHECK(d.gs.calls == std::vector<std::string>{"table", "stamp"});
    CHECK(d.gs.demo_restore == 1);
    CHECK(d.intro.phase() == 5);
    Rig c;
    c.calibrated();
    c.intro.key_up('C', c.host.t);
    CHECK(c.gs.calls == std::vector<std::string>{"stamp"});
    CHECK(c.gs.demo_restore == 0);
    CHECK(c.intro.phase() == 5);
    // the next frame takes the demo-start path (phase 6 > 5)
    c.intro.frame(c.host.t += 13, false);
    CHECK(c.intro.phase() == 7);
    CHECK(c.host.demo_starts == 1);
}

TEST_CASE("dialogs flow: seg10:0x82a2's deadline tail") {
    Rig r;
    r.calibrated();
    r.until_phase(4);
    const Tick d = r.intro.deadline();
    r.intro.after_dialog(5, 0);  // the exit dialog's Cancel leaves it
    CHECK(r.intro.deadline() == d);
    r.intro.after_dialog(12, 1);  // command 12 returns before the tail
    CHECK(r.intro.deadline() == d);
    r.intro.after_dialog(8, 0);  // phase 4: now + 15000
    CHECK(r.intro.deadline() == r.host.t + 15000);
    r.intro.after_dialog(5, 1);
    CHECK(r.intro.deadline() == r.host.t + 40000);
    CHECK(Intro::current() == &r.intro);
}
