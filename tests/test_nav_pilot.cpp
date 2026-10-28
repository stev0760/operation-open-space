// The demo pilot seg6:0x1e68 (src/nav/pilot.h) against docs/re/nav.md "Demo pilot", with a
// tree model of fixture `basic` as the window, and the demogame.sav rule of seg7:0x610-0x64d.
#include <string>
#include <vector>

#include "doctest.h"
#include "nav/enter.h"
#include "nav/pilot.h"
#include "nav_fixture.h"

using namespace is::nav;

namespace {

// A scripted window: rows with sub digits, Enter accepted on the rows listed.
struct Script : PilotTarget {
    std::vector<int> subs;        // the sub digit of each row
    std::vector<int> accept;      // rows whose Enter succeeds
    int sel = 0, rows = 0;
    std::vector<std::string> log;
    int enter() override {
        log.push_back("enter " + std::to_string(sel));
        for (int r : accept)
            if (r == sel) return 1;
        return 0;
    }
    int count() override { return rows; }
    int selection() override { return sel; }
    int sub_digit() override { return sel < static_cast<int>(subs.size()) ? subs[sel] : 0; }
    void expand() override {
        log.push_back("expand " + std::to_string(sel));
        subs[sel] = 2;
        subs.insert(subs.begin() + sel + 1, 0);  // one child
        ++rows;
    }
    void select(int row) override {
        log.push_back("select " + std::to_string(row));
        sel = row;
    }
};

// The real model: Tree + Enter over the in-memory fixture, as nav.cpp drives them.
struct TreeTarget : PilotTarget {
    MemFs fs;
    navtest::FakeStrings s;
    Records records;
    Walker w{fs};
    Game g{0, false, 0};  // the demo
    Tree tree{w, s, records, g};
    EnterState st;
    std::string dir;
    int sel = 0;
    std::vector<std::string> entered;
    TreeTarget() {
        navtest::build_basic(fs);
        tree.fill("D:\\");
        dir = tree.path(0);
    }
    int enter() override {
        EnterInputs in;
        in.game = g;
        const EnterResult r = is::nav::enter(st, dir, in, w, s, records, nullptr, [] { return 0; });
        if (r.accepted) entered.push_back(r.handover.directory);
        return r.accepted ? 1 : 0;
    }
    int count() override { return tree.count(); }
    int selection() override { return sel; }
    int sub_digit() override { return tree.sub_digit(sel); }
    void expand() override { tree.expand(sel); }
    void select(int row) override {
        sel = row;
        dir = tree.path(row);
    }
};

}  // namespace

TEST_CASE("nav pilot: the first attempt waits 1200 ms, then one row per 600 ms") {
    Script t;
    t.rows = 4;
    t.subs = {10, 0, 0, 0};
    Pilot p;
    p.step(true, 5000, 5000, t);  // seg6:0x1e68(1) at creation
    CHECK(p.next() == 6200);
    CHECK(t.log.empty());
    CHECK_FALSE(p.done());
    p.step(false, 0, 6200, t);  // not yet: 6200 < 6200 is false
    CHECK(t.log.empty());
    p.step(false, 0, 6201, t);
    CHECK(p.next() == 6801);
    CHECK(t.log == std::vector<std::string>{"enter 0", "select 1"});
    t.log.clear();
    p.step(false, 0, 6900, t);
    CHECK(t.log == std::vector<std::string>{"enter 1", "select 2"});
    CHECK_FALSE(p.done());
    t.log.clear();
    p.step(false, 0, 7600, t);
    // Row 3 is the last: count - 1 <= selection after the move.
    CHECK(t.log == std::vector<std::string>{"enter 2", "select 3"});
    CHECK(p.done());
    t.log.clear();
    // The next pass clears DGROUP:0xd26 first; at the last row the move is still made while
    // selection < count, and done is set again.
    p.step(false, 0, 8300, t);
    CHECK(t.log == std::vector<std::string>{"enter 3", "select 4"});
    CHECK(p.done());
    t.log.clear();
    p.step(false, 0, 9000, t);
    CHECK(t.log == std::vector<std::string>{"enter 4"});
    CHECK(p.done());
}

TEST_CASE("nav pilot: a collapsed row is expanded before the move") {
    Script t;
    t.rows = 3;
    t.subs = {10, 1, 0};
    t.sel = 1;
    Pilot p;
    p.step(true, 0, 0, t);
    p.step(false, 0, 1201, t);
    CHECK(t.log == std::vector<std::string>{"enter 1", "expand 1", "select 2"});
    CHECK(t.rows == 4);
}

TEST_CASE("nav pilot: an accepted Enter stops the walk") {
    Script t;
    t.rows = 5;
    t.subs = {10, 0, 0, 0, 0};
    t.accept = {0};
    Pilot p;
    p.step(true, 0, 0, t);
    p.step(false, 0, 1201, t);
    CHECK(t.log == std::vector<std::string>{"enter 0"});
    CHECK_FALSE(p.done());
}

TEST_CASE("nav pilot: the unsigned tick compare") {
    Script t;
    t.rows = 2;
    t.subs = {10, 0};
    Pilot p;
    p.step(true, 0xFFFFFF00u, 0, t);  // next = 0x4b0 - 0x100 after the wrap
    CHECK(p.next() == 0x3B0u);
    CHECK(t.log.size() == 0);
    p.step(false, 0, 0x3B1u, t);
    CHECK(t.log.size() == 2);
}

TEST_CASE("nav pilot: the demo walks fixture basic and enters GAMES") {
    TreeTarget t;
    // Rows: " D:\", DEEP, DEMON'S GATE, Dodge City - DUEL, EMPTY, GAMES, ... (listbox order).
    Pilot p;
    std::uint32_t tick = 0;
    p.step(true, tick, tick, t);
    for (int i = 0; i < 40 && t.entered.empty(); ++i) {
        tick += 600 + 1201;
        p.step(false, 0, tick, t);
    }
    // The root holds no *.exe, DEEP is expanded and walked (A, then B, which has FOXTROT.EXE).
    REQUIRE_FALSE(t.entered.empty());
    CHECK(t.entered.front() == "D:\\DEEP\\A\\B\\");
}

TEST_CASE("nav pilot: the global driver needs a target") {
    set_pilot_target(nullptr);
    CHECK_FALSE(pilot_open(0, 0));
    CHECK_FALSE(pilot_frame(5000));
    Script t;
    t.rows = 2;
    t.subs = {10, 0};
    set_pilot_target(&t);
    CHECK(pilot_open(100, 100));
    CHECK(pilot().next() == 1300);
    CHECK(pilot_frame(1301));
    CHECK(t.log == std::vector<std::string>{"enter 0", "select 1"});
    set_pilot_target(nullptr);
}

TEST_CASE("nav pilot: demogame.sav at the demo's end") {
    CHECK(demo_save_path("E:\\SAVES\\Inner Space", "GAMES\\") ==
          "E:\\SAVES\\Inner Space\\GAMES\\demogame.sav");
    CHECK(demo_save_path("C:\\INSPACE\\", "GAMES") == "C:\\INSPACE\\GAMES\\demogame.sav");
    DemoEnd e;
    e.timer = 100;
    e.tick = 100;
    e.record_complete = true;
    e.pilot_done = true;
    CHECK(deletes_demo_save(e));
    e.tick = 99;
    CHECK_FALSE(deletes_demo_save(e));
    e.tick = 100;
    e.player = true;
    CHECK_FALSE(deletes_demo_save(e));
    e.player = false;
    e.pilot_done = false;
    CHECK_FALSE(deletes_demo_save(e));
    e.pilot_done = true;
    e.record_complete = false;
    CHECK_FALSE(deletes_demo_save(e));
    e.record_complete = true;
    e.timer = 0;
    CHECK_FALSE(deletes_demo_save(e));
}
