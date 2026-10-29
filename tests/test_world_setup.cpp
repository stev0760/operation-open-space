// owner: world set-up (the single writer of this file in later stages).
// The wave set-up (src/world/setup.h; docs/re/world.md "Wave set-up"): the difficulty inputs,
// the power-on words, the generator's bounds, the icon fill through nav::Walker and IconReader
// (ADR 0012 decision 5) with its logged calls, the placement's invariants and the wave start.
// Everything runs on nav::MemFs with a fake icon reader, so the suite is portable.
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "doctest.h"
#include "nav/fs.h"
#include "shell/rng.h"
#include "world/setup.h"

using namespace is::world;
namespace nav = is::nav;

namespace {

// ExtractIcon stand-in: a file's icon count, and cells whose pixel (1, 1) is white (a blank
// cell, all pixel (0, 0)'s colour, becomes a placeholder: seg9:0x8da).
class FakeIcons : public IconReader {
public:
    std::map<std::string, int> counts;
    std::vector<std::string> asked;
    int count(const std::string& path) override {
        asked.push_back(path);
        auto it = counts.find(path);
        return it == counts.end() ? 0 : it->second;
    }
    std::optional<is::fmt::IconCell> cell(const std::string& path, int index) override {
        auto it = counts.find(path);
        if (it == counts.end() || index >= it->second) return std::nullopt;
        is::fmt::IconCell c;
        c.bgra[static_cast<std::size_t>(1 * is::fmt::kIconCell + 1)] = 0xffffffffu;
        return c;
    }
};

struct Rig {
    nav::MemFs fs;
    nav::Walker walker{fs};
    FakeIcons icons;
    is::shell::Rng rng;
    std::vector<nav::FsCall> calls;
    std::vector<std::string> log;
    World w;
    SetupHost host{rng, walker, icons};

    Rig() {
        fs.add_drive('D');
        fs.add_dir("D:\\GAMES");
        fs.add_file("D:\\GAMES\\ALPHA.EXE");
        fs.add_file("D:\\GAMES\\BETA.EXE");
        fs.add_file("D:\\GAMES\\GAMMA.DLL");
        fs.add_file("D:\\GAMES\\README.TXT");
        icons.counts["D:\\GAMES\\ALPHA.EXE"] = 2;
        icons.counts["D:\\GAMES\\GAMMA.DLL"] = 1;
        walker.record(&calls);
        host.now = [] { return 1000u; };
        host.log = [this](const std::string& s) { log.push_back(s); };
        power_on(w);
        rng.srand(1234);
        w.W = 640;
        w.H = 400;
        w.size = 1;
        w.shift = 3;
        w.detail = 3;
        w.complex = 0;
        w.handover.directory = "D:\\GAMES\\";
        w.added = 1;
        w.record = 0;
        w.dir();
    }
};

}  // namespace

TEST_CASE("world setup: power-on words [BYTES DGROUP:0x986 DGROUP:0x990]") {
    World w;
    power_on(w);
    CHECK(w.w986 == std::array<std::int16_t, 6>{1, 1, 1, 1, 1, 2});
    CHECK(w.w9346 == 0);
    CHECK(w.w9432 == 0);
    CHECK(w.w992 == 0);
}

TEST_CASE("world setup: budget from Complex [BYTES seg6:0x1204-0x1284]") {
    World w;
    w.wave = 20;
    for (int c = 0; c < 5; ++c) {
        w.complex = static_cast<std::int16_t>(c);
        read_budget(w);
        static const int kBudget[5] = {130, 220, 310, 420, 2000};
        static const int kW9000[5] = {0, 0, 1, 2, 2};
        CHECK(w.budget == kBudget[c]);
        CHECK(w.w9000 == kW9000[c]);
    }
    // The easy first waves: wave 0 halves the budget, wave 10 keeps it.
    w.complex = 0;
    w.wave = 0;
    read_budget(w);
    CHECK(w.budget == 65);
    w.wave = 4;
    read_budget(w);
    CHECK(w.budget == 65 + 26);
    w.wave = 10;
    read_budget(w);
    CHECK(w.budget == 130);
    // An unknown Complex leaves both words.
    w.complex = 7;
    w.wave = 10;
    w.budget = 99;
    read_budget(w);
    CHECK(w.budget == 99);
}

TEST_CASE("world setup: score level [BYTES seg9:0x484c]") {
    World w;
    w.rating = 0;
    level_from_score(w);
    CHECK(w.score_level == 0);
    w.rating = 26000;
    level_from_score(w);
    CHECK(w.score_level == 0);
    w.rating = 26001;
    level_from_score(w);
    CHECK(w.score_level == 1);
    w.rating = 700001;
    level_from_score(w);
    CHECK(w.score_level == 12);
}

TEST_CASE("world setup: new game resets [BYTES seg9:0xc06 seg9:0xc56]") {
    World w;
    power_on(w);
    w.restored = 0;
    w.score = 5;
    w.rating = 6;
    w.wa1cd = 3;
    w.wave = 7;
    new_game(w);
    CHECK(w.score == 0);
    CHECK(w.wave == 0);
    CHECK(w.w9346 == -1);
    CHECK(w.w9432 == 100);
    CHECK(w.w992 == 0);
    // world.md "New-game reset" (M3 s3 fixer, C016) [BYTES seg9:0xc52 seg9:0xc7f seg9:0xc97
    // seg9:0xcb0 seg9:0xcc8]: the rating and DGROUP:0xa1cd cleared, DGROUP:0xa1cf and the bytes
    // DGROUP:0xa193 ... 0xa196 = 1, all 8 x 3 words of DGROUP:0x4eec = -1.
    CHECK(w.rating == 0);
    CHECK(w.wa1cd == 0);
    CHECK(w.wa1cf == 1);
    for (std::uint8_t b : w.ba193) CHECK(b == 1);
    for (std::int16_t v : w.w4eec) CHECK(v == -1);
    CHECK(w.w986[0] == 1);
    CHECK(w.w986[5] == 2);
    // A restored game keeps the score and the wave.
    World r;
    power_on(r);
    r.restored = 1;
    r.score = 5;
    r.wave = 7;
    new_game(r);
    CHECK(r.score == 5);
    CHECK(r.wave == 7);
}

TEST_CASE("world setup: seg9:0x2828, the type-7 test and DGROUP:0x9346") {
    // world.md "seg9:0x2828" (M3 s3 fixer, C026): DGROUP:0x964a = 9 ... 15 with DGROUP:0x9332 >= 2
    // draws one rand() % 100 for 80 %, but a ship of design type 7 makes it certain (no draw)
    // [BYTES seg9:0x2862 seg9:0x2aa6 seg9:0x2aad seg9:0x2abc]; wave 0 clears DGROUP:0x932c again.
    Rig r;
    r.w.wave = 0;
    r.w.w964a = 10;
    r.w.w9432 = 100;  // below 180: DGROUP:0x9332 = 2
    r.w.last_ship = 1;
    std::uint32_t before = r.rng.state();
    wave0_followups(r.w, r.host);
    CHECK(r.w.w9332 == 2);
    CHECK(r.w.w932c == 0);
    CHECK(r.rng.state() != before);  // the 80 % draw
    r.w.ship(1).design_index = 3;
    r.host.design_type = [](int d) { return static_cast<std::int16_t>(d == 3 ? 7 : 0); };
    before = r.rng.state();
    wave0_followups(r.w, r.host);
    CHECK(r.rng.state() == before);  // certain: no draw
    // world.md "DGROUP:0x9346" (C017): a ship with byte +0xec != 0, type 7, byte +0x5e = 0,
    // +0x54 = 14 and +0x56 = the player gives DGROUP:0x9346 = the player's +0xd6
    // [BYTES seg9:0x29d6 seg9:0x29ea seg9:0x29f1 seg9:0x29fb seg9:0x2a05 seg9:0x2a09].
    r.w.w9346 = -1;
    ShipRecord& s = r.w.ship(1);
    s.bec = 1;
    s.w5e = 0;
    s.w54 = 14;
    s.w56 = static_cast<std::int16_t>(r.w.player);
    std::int16_t d6 = 4321;
    std::memcpy(reinterpret_cast<unsigned char*>(&r.w.ship(r.w.player)) + 0xd6, &d6, 2);
    wave0_followups(r.w, r.host);
    CHECK(r.w.w9346 == 4321);
    // +0x54 != 14 leaves it.
    r.w.w9346 = -1;
    s.w54 = 13;
    wave0_followups(r.w, r.host);
    CHECK(r.w.w9346 == -1);
}

TEST_CASE("world setup: centre exclusion [BYTES seg10:0x998 seg10:0x99d]") {
    World w;
    w.W = 640;
    w.H = 400;
    w.shift = 3;
    const Rect16 r = centre_exclusion(w);
    CHECK(r.l == static_cast<std::int16_t>((1600 - 320) << 3));
    CHECK(r.r == static_cast<std::int16_t>((1600 + 320) << 3));
    CHECK(r.t == static_cast<std::int16_t>((1600 - 200) << 3));
    CHECK(r.b == static_cast<std::int16_t>((1600 + 200) << 3));
}

TEST_CASE("world setup: blank cell [BYTES seg9:0x8da]") {
    StripCell c;
    CHECK(blank_cell(c));
    c.kind = StripCell::Kind::kIcon;
    CHECK(blank_cell(c));  // all black still matches pixel (0, 0)
    c.cell.bgra[33] = 0xffffffffu;
    CHECK_FALSE(blank_cell(c));
}

TEST_CASE("world setup: the generator stays within the pools' bounds for 200 seeds") {
    for (int seed = 0; seed < 200; ++seed) {
        Rig r;
        r.rng.srand(static_cast<std::uint16_t>(seed));
        r.w.wave = static_cast<std::int16_t>(seed % 40);
        r.w.rating = static_cast<std::uint32_t>(seed) * 5000u;
        state1_load(r.w, r.host);
        for (int p = 0; p <= kPoolShips; ++p) {
            CHECK(r.w.pools.in_use(p) >= 0);
            CHECK(r.w.pools.in_use(p) <= r.w.pools.allocated(p));
        }
        CHECK(r.w.pools.allocated(kPoolIcons) == kIconRecords);
    }
}

TEST_CASE("world setup: the load is deterministic in the seed") {
    Rig a, b;
    a.w.wave = b.w.wave = 3;
    a.rng.srand(77);
    b.rng.srand(77);
    state1_load(a.w, a.host);
    state1_load(b.w, b.host);
    CHECK(a.w.kind == b.w.kind);
    CHECK(describe_counts(a.w) == describe_counts(b.w));
    CHECK(a.rng.state() == b.rng.state());
}

TEST_CASE("world setup: icon fill through the walker and the reader [BYTES seg9:0x0 seg9:0x156]") {
    Rig r;
    r.w.wave = 1;
    state1_load(r.w, r.host);
    r.calls.clear();
    r.icons.asked.clear();
    fill_icons(r.w, r.host);
    // *.exe then *.dll with attribute 1 (read-only), from DGROUP:0x38c8 [BYTES seg9:0x2a].
    REQUIRE(r.calls.size() >= 2);
    CHECK(r.calls[0].op == nav::FsCall::Op::kFindFirst);
    CHECK(r.calls[0].path == "D:\\GAMES\\*.exe");
    CHECK(r.calls[0].arg == static_cast<int>(nav::kAttrReadOnly));
    CHECK(r.calls[0].routine == "seg9:0x0");
    bool dll = false;
    for (const nav::FsCall& c : r.calls)
        if (c.op == nav::FsCall::Op::kFindFirst && c.path == "D:\\GAMES\\*.dll") dll = true;
    CHECK(dll);
    // ExtractIcon(path, -1) once per listed program, through the walker's log.
    CHECK(r.icons.asked == std::vector<std::string>{"D:\\GAMES\\ALPHA.EXE", "D:\\GAMES\\BETA.EXE",
                                                    "D:\\GAMES\\GAMMA.DLL"});
    int extract_count = 0;
    for (const nav::FsCall& c : r.calls)
        if (c.op == nav::FsCall::Op::kExtractIcon && c.arg == -1) ++extract_count;
    CHECK(extract_count == 3);
    // 2 + 0 (a placeholder) + 1 icons = 4 candidates, all within the limit of 8.
    CHECK(r.w.icon_tries == 4);
    CHECK(r.w.icons_a == 4);
    CHECK(r.w.icons_c == 4);
    CHECK(r.w.icons[0].name == "ALPHA.EXE");
    CHECK(r.w.icons[2].name == "BETA.EXE");
    CHECK(r.w.icons[3].name == "GAMMA.DLL");
    CHECK(r.w.strip[0].kind == StripCell::Kind::kIcon);
    CHECK(r.w.strip[1].kind == StripCell::Kind::kIcon);
    CHECK(r.w.strip[2].kind == StripCell::Kind::kPlaceholder);
    CHECK(r.w.strip[3].kind == StripCell::Kind::kIcon);
    // The README is never asked about: the masks filter it.
    for (const std::string& s : r.icons.asked) CHECK(s.find("README") == std::string::npos);
}

TEST_CASE("world setup: the icon limit of 10 candidates [BYTES seg9:0x0]") {
    Rig r;
    for (int i = 0; i < 6; ++i) {
        const std::string name = "D:\\GAMES\\P" + std::to_string(i) + ".EXE";
        r.fs.add_file(name);
        r.icons.counts[name] = 3;
    }
    r.w.wave = 1;
    state1_load(r.w, r.host);
    fill_icons(r.w, r.host);
    CHECK(r.w.icon_tries <= 10);
    CHECK(r.w.icons_a <= 8);
    CHECK(r.w.pools.in_use(kPoolIcons) <= kIconRecords);
}

TEST_CASE("world setup: the briefing entry places once and the wave start follows") {
    Rig r;
    r.w.wave = 1;
    state1_load(r.w, r.host);
    new_game(r.w);
    r.w.wave = 1;
    enter_briefing(r.w, r.host);
    CHECK(r.w.placed == 1);
    CHECK(r.w.strip_slots == r.w.pools.in_use(kPoolIcons));
    CHECK(r.w.strip_slots >= 1);
    // The placers keep every record inside the world [BYTES seg10:0x864 seg10:0xa26 seg9:0xdf6].
    const int cells = static_cast<int>(kWorldCells) << r.w.shift;
    for (int p : {kPoolIcons, kPoolRocks}) {
        for (int i = 0; i < r.w.pools.in_use(p); ++i) {
            const ObjectRecord& o = r.w.pools.rec(p, i);
            CHECK(o.x >= 0);
            CHECK(o.y >= 0);
            CHECK(o.x < cells);
            CHECK(o.y < cells);
        }
    }
    // The latch DGROUP:0x93b0: place() returns at once while it is set [BYTES seg9:0x6624
    // seg9:0x6633]; the briefing entry clears it first [BYTES seg9:0x5d24] (M3 s2 fixer, review
    // M6: the old check held trivially).
    REQUIRE(r.w.placed == 1);
    const std::int16_t gx = r.w.pools.rec(kPoolGatePosts, 0).x, gy = r.w.pools.rec(kPoolGatePosts, 0).y;
    const std::uint32_t state = r.rng.state();
    place(r.w, r.host);
    CHECK(r.rng.state() == state);
    CHECK(r.w.pools.rec(kPoolGatePosts, 0).x == gx);
    CHECK(r.w.pools.rec(kPoolGatePosts, 0).y == gy);
    r.w.wave = 0;
    enter_briefing(r.w, r.host);
    CHECK(r.w.placed == 1);
    CHECK(r.rng.state() != state);  // placed again, drawing rand()
    // The wave start: the player's ship (ship_setup_minimal) is in play.
    start_wave(r.w, r.host);
    CHECK(r.w.pools.in_use(kPoolShips) >= 1);
    CHECK(r.w.pools.rec(kPoolShips, r.w.player).state != 0);
}

TEST_CASE("world setup: the chance block needs an edition other than 0 and 3") {
    // world.md "Ships at wave start", "Chance block" (M3 s3 fixer, C029) [BYTES seg9:0x6126
    // seg9:0x612c]: score level 5, kind 0, six waves since DGROUP:0x93ac, no weapons: chance
    // ((6 - 3) * 2 + 5) * 2 = 22, one rand() % 100 when the edition allows it.
    for (int edition : {-1, 0, 1, 2, 3}) {
        Rig r;
        state1_load(r.w, r.host);
        new_game(r.w);
        enter_briefing(r.w, r.host);
        r.w.w920c = static_cast<std::int16_t>(edition);
        r.w.score_level = 5;
        r.w.kind = 0;
        r.w.handover.demons_gate = 0;
        r.w.wave = 6;
        r.w.w93ac = 0;
        r.w.w992 = 0;
        r.w.budget = 100;
        r.w.added = 1;  // no resident ships: the block is the only draw
        const std::uint32_t before = r.rng.state();
        start_wave(r.w, r.host);
        INFO("edition " << edition);
        if (edition == 0 || edition == 3) {
            CHECK(r.rng.state() == before);
            CHECK(r.w.w913e == 0);
        } else {
            CHECK(r.rng.state() != before);
        }
    }
}

TEST_CASE("world setup: seed_wave takes the low word of the tick [BYTES seg11:0xb22 seg11:0xb28]") {
    is::shell::Rng rng;
    seed_wave(rng, 0x12345678u);
    CHECK(rng.state() == 0x5678u);
}

TEST_CASE("world: gate_post_live, seg3:0x65e's code-3 table test") {
    // world.md "Key messages", "Suit test": kind word DGROUP:0x3d6c + 0x34·rec = 2 and class word
    // DGROUP:0x3d5a + 0x34·rec > 0 (M3 s3 fixer) [BYTES seg3:0x6a2-0x6b8].
    World w;
    CHECK_FALSE(gate_post_live(w, 0));
    w.pool8[3].at(0x1c) = 2;
    CHECK_FALSE(gate_post_live(w, 3));  // class 0
    w.pool8[3].at(0x0a) = 1;
    CHECK(gate_post_live(w, 3));
    w.pool8[3].at(0x1c) = 3;  // a gate post (kind 3)
    CHECK_FALSE(gate_post_live(w, 3));
    w.pool8[4].at(0x1c) = 2;
    w.pool8[4].at(0x0a) = -1;
    CHECK_FALSE(gate_post_live(w, 4));
    CHECK_FALSE(gate_post_live(w, -1));
    CHECK_FALSE(gate_post_live(w, kPool8Max));
}
