// The saved-game files (src/formats/savegame.h, ADR 0014): the header field by field, the block
// lengths of each granularity, the reader's refusals, the writer's names, the purge and the
// score list. Pins oracle/digests/core-0003.md's ORACLE03.sav (1248 bytes) and, when the run's
// bulk is present, reads the oracle's files (their pilot name is never printed).
#include "doctest.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>

#include "formats/savegame.h"

using namespace is::fmt::save;

namespace {

Header full_header() {
    Header h;
    h.game_id = 0x11223344u;
    h.set_pilot("Pilot Test");
    h.start_mon = 10;
    h.start_mday = 2;
    h.start_year = 126;
    h.save_mon = 10;
    h.save_mday = 3;
    h.save_year = 126;
    h.score = 0x000c3500u;
    h.ship = 7;
    h.wave = 4;
    h.b5d = -2;
    h.b5e = 3;
    h.record = 1;
    h.player_roster = 0;
    h.w9432 = 0x1234;
    h.w964a = -5;
    h.dir_count = 2;
    h.roster_last = 1;
    for (std::size_t i = 0; i < 64; ++i) h.table[i] = static_cast<std::int16_t>(i * 3 - 100);
    for (std::size_t i = 0; i < 24; ++i) {
        h.w4eec[i] = static_cast<std::int16_t>(i + 1);
        h.w4fd0[i] = static_cast<std::int16_t>(-static_cast<int>(i));
        h.w6b10[i] = static_cast<std::int16_t>(i * 7);
    }
    for (std::size_t i = 0; i < 8; ++i) h.w6b40[i] = static_cast<std::int16_t>(i + 0x100);
    h.w945c = 11;
    h.w945e = 12;
    h.captured = 13;
    h.destroyed = 14;
    h.shape = 101;
    h.completed = 1;
    h.credit10 = 15;
    h.resources = 16;
    h.w93ac = 17;
    h.w94ca = 18;
    h.rating = 0xdeadbeefu;
    h.play_mode = 1;
    h.drive = 2;
    h.preview = 3;
    h.size = 1;
    h.weapon_slot = 0xff;
    h.w1318 = 1;
    for (std::size_t k = 0; k < 10; ++k) {
        h.a14d[k] = static_cast<std::uint8_t>(k + 1);
        h.a157[k] = static_cast<std::uint8_t>(0x80 + k);
    }
    h.a161 = 0x21;
    h.a162 = 0x22;
    h.w967c = 0xfe;
    h.frames = 0x00012345u;
    h.w9346 = 0xff;
    split_a1cd(-300, &h.a1cd_q, &h.a1cd_r);
    h.a193 = {1, 2, 3, 4};
    h.a1cf = 9;
    return h;
}

std::uint16_t u16(const HeaderBytes& b, std::size_t o) { return static_cast<std::uint16_t>(b[o] | (b[o + 1] << 8)); }
std::uint32_t u32(const HeaderBytes& b, std::size_t o) { return u16(b, o) | (static_cast<std::uint32_t>(u16(b, o + 2)) << 16); }

std::vector<std::uint8_t> synthetic(Granularity g, int roster_last, int dirs, int record = 0) {
    Header h = full_header();
    h.roster_last = static_cast<std::int16_t>(roster_last);
    h.dir_count = static_cast<std::int16_t>(dirs);
    h.record = static_cast<std::int16_t>(record);
    std::vector<RosterBytes> r(static_cast<std::size_t>(roster_last + 1));
    for (std::size_t k = 0; k < r.size(); ++k) r[k].fill(static_cast<std::uint8_t>(0x40 + k));
    std::vector<DirBytes> d(static_cast<std::size_t>(dirs));
    for (std::size_t k = 0; k < d.size(); ++k) d[k].fill(static_cast<std::uint8_t>(0x60 + k));
    return build(encode_header(h), r, d, Kind::kSav, g);
}

std::vector<std::uint8_t> read_file(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

}  // namespace

TEST_CASE("savegame: the header, field by field at the original's offsets") {
    const Header h = full_header();
    const HeaderBytes b = encode_header(h);
    CHECK(u32(b, 0x00) == 0x11223344u);
    CHECK(std::string(reinterpret_cast<const char*>(&b[0x04])) == "Pilot Test");
    CHECK(b[0x18] == 10);
    CHECK(b[0x19] == 2);
    CHECK(b[0x1a] == 126);
    CHECK(b[0x1b] == 10);
    CHECK(b[0x1c] == 3);
    CHECK(b[0x1d] == 126);
    CHECK(u32(b, 0x1e) == 0xc3500u);
    CHECK(u16(b, 0x22) == 7);
    CHECK(u16(b, 0x24) == 4);
    CHECK(u16(b, 0x26) == 0xfffe);
    CHECK(u16(b, 0x28) == 3);
    CHECK(u16(b, 0x2a) == 1);
    CHECK(u16(b, 0x2e) == 0x1234);
    CHECK(u16(b, 0x30) == 0xfffb);
    CHECK(u16(b, 0x32) == 2);
    CHECK(u16(b, 0x34) == 1);
    CHECK(u16(b, 0x36) == static_cast<std::uint16_t>(-100));
    CHECK(u16(b, 0x36 + 2 * 63) == static_cast<std::uint16_t>(63 * 3 - 100));
    CHECK(u16(b, 0xb6) == 1);
    CHECK(u16(b, 0xe6 + 2) == 0xffff);
    CHECK(u16(b, 0x116 + 2 * 23) == 23 * 7);
    CHECK(u16(b, 0x146 + 14) == 0x107);
    CHECK(u16(b, 0x156) == 11);
    CHECK(u16(b, 0x158) == 12);
    CHECK(u16(b, 0x15a) == 13);
    CHECK(u16(b, 0x15c) == 14);
    CHECK(u16(b, 0x15e) == 101);
    CHECK(u16(b, 0x160) == 1);
    CHECK(u16(b, 0x162) == 15);
    CHECK(u16(b, 0x164) == 16);
    CHECK(u16(b, 0x166) == 17);
    CHECK(u16(b, 0x168) == 18);
    CHECK(u32(b, 0x16a) == 0xdeadbeefu);
    CHECK(b[0x16e] == 1);
    CHECK(b[0x16f] == 2);
    CHECK(b[0x170] == 3);
    CHECK(b[0x171] == 1);
    CHECK(b[0x172] == 0xff);
    CHECK(b[0x173] == 1);
    CHECK(b[0x174] == 1);
    CHECK(b[0x17d] == 10);
    CHECK(b[0x17e] == 0x80);
    CHECK(b[0x187] == 0x89);
    CHECK(b[0x188] == 0x21);
    CHECK(b[0x189] == 0x22);
    CHECK(b[0x18a] == 0xfe);
    CHECK(u32(b, 0x18b) == 0x12345u);  // unaligned
    CHECK(b[0x18f] == 0xff);
    CHECK(b[0x192] == 1);
    CHECK(b[0x195] == 4);
    CHECK(b[0x196] == 9);
    for (std::size_t o = 0x197; o < kHeaderSize; ++o) CHECK(b[o] == 0);
    // Decoding gives every field back.
    const Header d = decode_header(b.data());
    CHECK(encode_header(d) == b);
    CHECK(d.pilot_name() == "Pilot Test");
    CHECK(d.b5d == -2);
    CHECK(sx(d.weapon_slot) == -1);  // the reader's cbw
    CHECK(join_a1cd(d.a1cd_q, d.a1cd_r) == -300);
}

TEST_CASE("savegame: the pilot is bounded at its 20-byte field") {
    Header h;
    h.set_pilot("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    CHECK(h.pilot_name() == "ABCDEFGHIJKLMNOPQRS");
    CHECK(h.pilot[19] == 0);
}

TEST_CASE("savegame: computed fields and the reader's decodings") {
    std::int32_t c = 0x50000;
    CHECK(credit_word(&c) == static_cast<std::int16_t>(0x4fff6 / 10));
    CHECK(c == 0x4fff6);  // capped in place
    c = -25;
    CHECK(credit_word(&c) == -2);  // toward zero
    CHECK(c == -25);
    CHECK(shape_of({82, 83, 82, 80}, 4) == 100);   // core-0003 ORACLE03: 100
    CHECK(shape_of({50, 45, 50, 31}, 4) == 102);   // core-0003 ORACLE3X: 102
    CHECK(shape_of({71, 71, 71, 0}, -1) == 100);   // three words when the design word is -1
    CHECK(shape_of({71, 71, 71, 0}, 4) == 101);
    CHECK(shape_of({27, 27, 27, 27}, 4) == 103);
    CHECK(preview_flags(0, 0, 0) == 0);
    CHECK(preview_flags(1, 0, 0) == 3);
    CHECK(preview_flags(1, 1, 0) == 2);
    CHECK(preview_flags(1, 0, 1) == 1);
    std::uint8_t q = 0, r = 0;
    for (int v : {0, 1, 127, 128, -1, -127, -128, -129, 16383, -16511}) {
        split_a1cd(static_cast<std::int16_t>(v), &q, &r);
        CHECK(join_a1cd(q, r) == v);
    }
    split_a1cd(16384, &q, &r);  // q = 128 does not fit a signed byte
    CHECK(join_a1cd(q, r) != 16384);
    CHECK(date_word(10, 2) == 0x0a02);
    CHECK(date_word(10, 0x80) == 0xff80);  // the cbw of +0x19 sets the high byte
}

TEST_CASE("savegame: the block lengths of each granularity") {
    CHECK(round_block(kHeaderSize, Granularity::k32) == 0x1c0);
    CHECK(round_block(kHeaderSize, Granularity::kOdd16) == 0x1b0);
    CHECK(round_block(0x7c, Granularity::kOdd16) == 0x90);
    CHECK(round_block(0x20, Granularity::kOdd16) == 0x30);
    CHECK(round_block(0x10, Granularity::kOdd16) == 0x10);
    CHECK(round_block(kHeaderSize, Granularity::k64) == 0x1c0);
    CHECK(round_block(0x297, Granularity::k64) == 0x2c0);
    CHECK(round_block(0x297, Granularity::k256) == 0x300);
}

TEST_CASE("savegame: core-0003's ORACLE03.sav and ORACLE3X.sav lengths") {
    // +0x32 = 1, +0x34 = 0: 0x1c0 + 0x80 + 0x2a0 = 1248 [ORACLE core-0003]
    const auto a = synthetic(Granularity::k32, 0, 1);
    CHECK(a.size() == 1248);
    // +0x34 = 2: 0x1c0 + 0x180 + 0x2a0 = 1504 [ORACLE core-0003]
    CHECK(synthetic(Granularity::k32, 2, 1).size() == 1504);
    // The padding is zero.
    for (std::size_t o = 0x1a9; o < 0x1c0; ++o) CHECK(a[o] == 0);
    for (std::size_t o = 0x1c0 + 0x7c; o < 0x240; ++o) CHECK(a[o] == 0);
    for (std::size_t o = 0x240 + 0x297; o < 0x4e0; ++o) CHECK(a[o] == 0);
    const auto p = parse(a, Kind::kSav);
    REQUIRE(p);
    CHECK(p->granularity == Granularity::k32);
    CHECK(p->roster.size() == 1);
    CHECK(p->dirs.size() == 1);
    CHECK(p->roster[0][0] == 0x40);
    CHECK(p->dirs[0][0x296] == 0x60);
}

TEST_CASE("savegame: the reader finds each granularity and refuses a length none fits") {
    for (Granularity g : {Granularity::k32, Granularity::kOdd16, Granularity::k64}) {
        const auto f = synthetic(g, 2, 3, 2);
        std::string why;
        const auto p = parse(f, Kind::kSav, &why);
        REQUIRE_MESSAGE(p, why);
        CHECK(p->granularity == g);
        CHECK(p->roster.size() == 3);
        CHECK(p->roster[2][0x7b] == 0x42);
        CHECK(p->dirs.size() == 3);
        CHECK(p->dirs[2][0] == 0x62);
        CHECK(p->header.record == 2);
        // Written back in the same form.
        CHECK(build(p->header_bytes, p->roster, p->dirs, Kind::kSav, g) == f);
    }
    auto f = synthetic(Granularity::k32, 0, 1);
    f.push_back(0);
    std::string why;
    CHECK_FALSE(parse(f, Kind::kSav, &why));
    CHECK(why.find("granularity") != std::string::npos);
    f.resize(0x100);
    CHECK_FALSE(parse(f, Kind::kSav));
}

TEST_CASE("savegame: the reader refuses +0x34, +0x32, +0x2a and a pilot without its NUL") {
    auto set16 = [](std::vector<std::uint8_t>& f, std::size_t o, int v) {
        f[o] = static_cast<std::uint8_t>(v & 0xff);
        f[o + 1] = static_cast<std::uint8_t>((v >> 8) & 0xff);
    };
    auto f = synthetic(Granularity::k32, 0, 1);
    set16(f, 0x34, 255);
    CHECK_FALSE(parse(f, Kind::kSav));
    set16(f, 0x34, -1);
    CHECK_FALSE(parse(f, Kind::kSav));
    f = synthetic(Granularity::k32, 0, 1);
    set16(f, 0x32, 0);
    CHECK_FALSE(parse(f, Kind::kSav));
    set16(f, 0x32, 99);
    CHECK_FALSE(parse(f, Kind::kSav));
    f = synthetic(Granularity::k32, 0, 1);
    set16(f, 0x2a, 1);  // ADR 0014 decision 9: +0x2a below +0x32
    CHECK_FALSE(parse(f, Kind::kSav));
    set16(f, 0x2a, -1);
    CHECK_FALSE(parse(f, Kind::kSav));
    f = synthetic(Granularity::k32, 0, 1);
    for (std::size_t i = 0; i < kPilotSize; ++i) f[0x04 + i] = 'A';
    std::string why;
    CHECK_FALSE(parse(f, Kind::kSav, &why));
    CHECK(why.find("pilot") != std::string::npos);
    // A .ded has no directory block: +0x32 and +0x2a are not tested.
    Header h = full_header();
    h.roster_last = 0;
    h.dir_count = 0;
    h.record = 5;
    const auto ded = build(encode_header(h), std::vector<RosterBytes>(1), {}, Kind::kDed, Granularity::k32);
    CHECK(ded.size() == 0x1c0 + 0x80);
    CHECK(parse(ded, Kind::kDed));
}

TEST_CASE("savegame: the writer's names") {
    CHECK(base_name(false, Kind::kSav, "ORACLE03", "Pilot", true) == "lastgame");
    CHECK(base_name(false, Kind::kSav, "ORACLE03", "Pilot", false) == "demogame");
    CHECK(base_name(true, Kind::kSav, "ORACLE03", "Pilot", true) == "ORACLE03");
    CHECK(base_name(true, Kind::kSav, "", "Pilot", true) == "");
    // .ded with Restore[3] = ' ' ("NEW GAME", as GetPrivateProfileString returns the " NEW GAME"
    // New Game OK wrote): the pilot cut at 7, then at its first space.
    CHECK(base_name(true, Kind::kDed, "NEW GAME", "Your Name", true) == "Your");
    CHECK(base_name(true, Kind::kDed, "NEW GAME", "Abcdefghij", true) == "Abcdefg");
    CHECK(base_name(true, Kind::kDon, "ORACLE03", "Your Name", true) == "ORACLE03");
    CHECK(base_name(true, Kind::kSav, "NEW GAME", "Your Name", true) == "NEW GAME");  // .sav: no cut
    CHECK(retry_name("ORACLE03", 1, Kind::kDed) == "ORACL1.ded");
    CHECK(retry_name("ORACLE03", 12, Kind::kDon) == "ORACL12.don");
    CHECK(std::string(extension(Kind::kSav)) == ".sav");
}

TEST_CASE("savegame: seg6:0x8776 and the free-name loop") {
    const std::uint32_t id = 0x01020304u;
    std::vector<std::uint8_t> same(0x1c0, 0), other(0x1c0, 0), shortf(0x20, 0);
    same[0] = 4, same[1] = 3, same[2] = 2, same[3] = 1;
    other[0] = 9;
    shortf[0] = 9;
    CHECK(name_check(nullptr, 0x1c0, id) == 1);    // does not open (corrected claim 16)
    CHECK(name_check(&shortf, 0x1c0, id) == 1);    // short read
    CHECK(name_check(&same, 0x1c0, id) == 1);      // the same game
    CHECK(name_check(&other, 0x1c0, id) == 0);     // another game's file
    std::map<std::string, const std::vector<std::uint8_t>*> files = {
        {"Your.ded", &other}, {"Your1.ded", &other}, {"Your2.ded", &same}};
    std::vector<std::string> tried;
    auto check = [&](const std::string& n) {
        tried.push_back(n);
        auto it = files.find(n);
        return name_check(it == files.end() ? nullptr : it->second, 0x1c0, id);
    };
    CHECK(free_name("Your.ded", "Your", Kind::kDed, check) == "Your2.ded");
    CHECK(tried == std::vector<std::string>{"Your.ded", "Your1.ded", "Your2.ded"});
    tried.clear();
    files.erase("Your1.ded");
    CHECK(free_name("Your.ded", "Your", Kind::kDed, check) == "Your1.ded");
}

TEST_CASE("savegame: the purge matches the whole game id of a full 0x26-byte read") {
    std::vector<std::uint8_t> head(kScoreHead, 0);
    head[0] = 0x44, head[1] = 0x33, head[2] = 0x22, head[3] = 0x11;
    CHECK(purge_match(head, 0x11223344u));
    CHECK_FALSE(purge_match(head, 0x11223345u));
    head.resize(kScoreHead - 1);
    CHECK_FALSE(purge_match(head, 0x11223344u));
}

namespace {
ScoreFile score_file(std::uint32_t id, std::int32_t score, const std::string& pilot, const std::string& name, int pass,
                     std::int16_t ship = 0) {
    Header h;
    h.game_id = id;
    h.score = static_cast<std::uint32_t>(score);
    h.ship = ship;
    h.set_pilot(pilot);
    const HeaderBytes b = encode_header(h);
    ScoreFile f;
    f.opened = true;
    f.head.assign(b.begin(), b.begin() + kScoreHead);
    f.name = name;
    f.pass = pass;
    return f;
}
}  // namespace

TEST_CASE("savegame: the score list's rank and order") {
    RunningGame g;
    g.game_id = 100;
    g.score = 5000;
    g.pilot = "Me";
    std::vector<ScoreFile> files = {
        score_file(100, 4000, "Me", "MINE.sav", 0),      // the running game's own, lower: dropped
        score_file(200, 9000, "Other", "OTHER.ded", 1),
        score_file(300, 3000, "Third", "THIRD.don", 2, 99),
        score_file(200, 8000, "Other", "OTHER1.ded", 1), // the same game, lower: one entry per game
    };
    ScoreFile bad;
    bad.opened = false;
    bad.name = "BAD.sav";
    files.insert(files.begin() + 1, bad);  // ADR 0014 decision 9: skipped, the list goes on
    const ScoreList l = build_score_list(g, files, 10, {});
    CHECK(l.skipped == 1);
    REQUIRE(l.list.size() == 3);
    CHECK(l.list[0].score() == 9000);
    CHECK(l.list[0].file() == "OTHER");
    CHECK(l.list[0].status() == 1);
    CHECK(l.list[1].score() == 5000);
    CHECK(l.list[1].file() == "--");
    CHECK(l.list[2].score() == 3000);
    CHECK(l.list[2].ship() == 0);  // 99 above the SHP count - 1
    CHECK(l.rank_set);
    CHECK(l.rank == 2);
    const auto slots = champion_slots(l, [](int s) { return "SHIP" + std::to_string(s); });
    REQUIRE(slots.size() == 3);
    CHECK(slots[1].place == 2);
    CHECK(slots[1].text == "Me - SHIP0");
    CHECK(slots[0].score == 9000);
}

TEST_CASE("savegame: a saved file of the running game with a higher score shows the game twice") {
    RunningGame g;
    g.game_id = 100;
    g.score = 1000;
    const ScoreList l = build_score_list(g, {score_file(100, 2000, "Me", "MINE.sav", 0)}, 10, {});
    // The order pass zeroes ids from index 1 when the pick is not entry 0 (claim 35).
    REQUIRE(l.list.size() == 2);
    CHECK(l.list[0].score() == 2000);
    CHECK(l.list[1].score() == 1000);
}

TEST_CASE("savegame: the champions table's slot selection") {
    ScoreList l;
    for (int i = 0; i < 20; ++i) {
        ScoreEntry e;
        e.set_id(static_cast<std::uint32_t>(i + 1));
        e.set_score(20000 - i);
        l.list.push_back(e);
    }
    l.rank = 12;
    const auto s = champion_slots(l, {});
    // i < 3, then 7 < i < 15
    std::vector<int> places;
    for (const auto& t : s) places.push_back(t.place);
    CHECK(places == std::vector<int>{1, 2, 3, 9, 10, 11, 12, 13, 14, 15});
    l.rank = 3;
    CHECK(champion_slots(l, {}).size() == 10);
}

TEST_CASE("savegame: the oracle's files when present") {
    // work/oracle/core-0003/reg beside IS_ASSETS_DIR (work/assets).
    const char* env = std::getenv("IS_ASSETS_DIR");
    if (!env || !*env) {
        MESSAGE("IS_ASSETS_DIR unset: skipped");
        return;
    }
    const std::filesystem::path dir = std::filesystem::path(env).parent_path() / "oracle" / "core-0003" / "reg";
    std::error_code ec;
    if (!std::filesystem::exists(dir / "ORACLE03.sav", ec)) {
        MESSAGE("work/oracle/core-0003/reg absent: skipped");
        return;
    }
    const auto a = read_file(dir / "ORACLE03.sav");
    CHECK(a.size() == 1248);
    const auto p = parse(a, Kind::kSav);
    REQUIRE(p);
    CHECK(p->granularity == Granularity::k32);
    CHECK(p->header.score == 900);
    CHECK(p->header.wave == 1);
    CHECK(p->header.shape == 100);
    CHECK(p->header.credit10 == 15);
    CHECK(p->header.frames == 4005);
    CHECK(p->header.dir_count == 1);
    CHECK(p->header.roster_last == 0);
    CHECK(p->roster[0][0x1c] == 29);
    // The header re-encodes to the file's bytes.
    CHECK(encode_header(p->header) == p->header_bytes);
    CHECK(build(p->header_bytes, p->roster, p->dirs, Kind::kSav, p->granularity) == a);
    if (std::filesystem::exists(dir / "ORACLE3X.sav", ec)) {
        const auto x = read_file(dir / "ORACLE3X.sav");
        CHECK(x.size() == 1504);
        const auto q = parse(x, Kind::kSav);
        REQUIRE(q);
        CHECK(q->header.wave == 2);
        CHECK(q->header.roster_last == 2);
        CHECK(q->header.shape == 102);
    }
}
