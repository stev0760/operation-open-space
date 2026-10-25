// The intro's portable rules: texts, layout, trial day, text pages, key rows (src/intro/texts.h),
// the bolt (src/intro/bolt.h) and the title picture's pixels (src/intro/pixels.h). The golden
// cases read the original files (IS_ORIGINAL_DIR) and skip without them.
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "assets/asset_store.h"
#include "doctest.h"
#include "formats/res16.h"
#include "intro/bolt.h"
#include "intro/pixels.h"
#include "intro/texts.h"

using namespace is::intro;

namespace {
std::filesystem::path original_dir() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    return v;
}
std::vector<std::uint8_t> bytes(const std::string& s) {
    std::vector<std::uint8_t> v(s.begin(), s.end());
    v.push_back(0);
    return v;
}
}  // namespace

TEST_CASE("intro texts: LoadString limits") {
    CHECK(load_limit("presents.. ", 40) == "presents.. ");
    CHECK(load_limit("abcdef", 4) == "abc");
    CHECK(load_limit("abc", 0).empty());
    CHECK(load_limit(std::string("ab\0cd", 5), 10) == "ab");
}

TEST_CASE("intro texts: seg11:0x3fd4's case rule") {
    CHECK(case_rule("Limited Inner Space", 0) == "limited inner space");
    CHECK(case_rule("Inner Space", 0) == "inner space");
    CHECK(case_rule("A (Big) Deal", 0) == "a (Big) deal");  // the character after '(' stays
    CHECK(case_rule("ABC \xC9T\xC9", 0) == "abc \xC9t\xC9");  // signed compare: bytes >= 0x80 stay
    CHECK(case_rule("abc def", 1) == "ABC DEF");
    CHECK(case_rule("abc def", 2) == "Abc Def");
    CHECK(case_rule("one. two! three", 3) == "One. Two! Three");
    CHECK(title_name_line("Limited Inner Space") == "limited inner space\x99");
}

TEST_CASE("intro texts: the version and edition lines (seg15:0x681c)") {
    CHECK(version_string("129", "08") == "V1.29.08");
    CHECK(version_string("1", "08") == "V1.");  // the string's NUL ends the version at byte 3
    CHECK(edition_line(" Standard Edition", "V1.29.08", "Aug 9/2000") ==
          " Standard Edition - V1.29.08 Aug 9/2000");
    CHECK(edition_line("", "V1.29.08", "12345678901234567890xyz") ==
          " - V1.29.08 1234567890123456789");
}

TEST_CASE("intro texts: the trial day of seg10:0x885e") {
    // the oracle's day 12086 on 2026-10-23 (yday 295) and the probe's 12087 on 2026-10-24
    CHECK(trial_day(day_value(126, 295), 0, 0) == 12086);
    CHECK(trial_day(day_value(126, 296), 0, 0) == 12087);
    CHECK(day_value(93, 0) == 0);
    CHECK(trial_day(254, 0, 0) == 200);  // below 1
    CHECK(trial_day(255, 0, 0) == 1);
    // k = (P - V) with its low byte inverted
    CHECK(trial_day(1000, 0, 0x0100) == 1000 - (0x01ff) + 1);
    CHECK(trial_day(1000, 3, 0) == 1000 - (-3 ^ 0xff) + 1);
}

TEST_CASE("intro texts: the trial line by edition") {
    const std::string f1 = "You are on day %d of your demo trial period.", f2 = "Day %d of 35",
                      f3 = "invalid";
    TrialLine t = trial_line(0, 0, 12088, f1, f2, f3);
    CHECK(t.text == "You are on day 12088 of your demo trial period.");
    CHECK(t.order_line);
    CHECK_FALSE(t.none);
    CHECK(trial_line(1, 0, -5, f1, f2, f3).text == "Day 5 of 35");
    CHECK(trial_line(0, 1, 5, f1, f2, f3).text == "invalid");
    t = trial_line(2, 0, -7, f1, f2, f3);
    CHECK(t.text == "Day 7 of 35");
    CHECK_FALSE(t.order_line);
    CHECK(trial_line(3, 0, 1, f1, f2, f3).none);
    CHECK(format_day("no number", 3) == "no number");
}

TEST_CASE("intro texts: the text pages split at CR, 11 lines a page") {
    std::string s;
    for (int i = 0; i < 13; ++i) s += "L" + std::to_string(i) + "\r\n";
    auto p0 = text_page_lines(bytes(s), 0);
    CHECK(p0[0] == "L0");
    CHECK(p0[10] == "L10");
    auto p1 = text_page_lines(bytes(s), 1);
    CHECK(p1[0] == "L11");
    CHECK(p1[1] == "L12");
    CHECK(p1[2].empty());
    CHECK(p1[10].empty());
    // an empty line is a line; text after the NUL is not read
    auto e = text_page_lines(bytes("a\r\n\r\nb\r\n\0zzz"), 0);
    CHECK(e[0] == "a");
    CHECK(e[1].empty());
    CHECK(e[2] == "b");
    CHECK(e[3].empty());
    CHECK(text_page_resource(0) == 136);
    CHECK(text_page_resource(1) == 136);
    CHECK(text_page_resource(2) == 200);
    CHECK(text_page_resource(-1) == 200);
}

TEST_CASE("intro texts: the key rows of seg15:0x6b52") {
    const auto r = key_rows();
    CHECK(r[0].key_id == 1223);
    CHECK(r[0].desc_id == 1323);
    CHECK(r[0].colour == 0x000b00ffu);  // RGB(255, 0, 11)
    CHECK(r[0].dy == -150);
    CHECK(r[7].key_id == 1230);
    CHECK(r[7].desc_id == 1330);
    CHECK(r[8].key_id == 1209);
    CHECK(r[8].desc_id == 1309);
    CHECK(r[9].key_id == 1213);
    CHECK(r[9].desc_id == 1313);
    CHECK(r[10].key_id == 1207);
    CHECK(r[10].desc_id == 1307);
    CHECK(r[10].colour == 0x0083007du);  // RGB(125, 0, 131)
    CHECK(r[10].dy == 150);
    for (int i = 0; i < kKeyRows; ++i) {
        CHECK((r[i].colour & 0xff) == static_cast<unsigned>(255 - 13 * i));
        CHECK(((r[i].colour >> 8) & 0xff) == 0u);
        CHECK((r[i].colour >> 16) == static_cast<unsigned>(12 * i + 11));
    }
}

TEST_CASE("intro texts: the title layout relative to W/2, H/2, D and C") {
    TitleLayout t = title_layout(1024, 768, 0, 40);
    CHECK(t.operation == Point{422, 139});
    CHECK(t.name == Point{512, 164});
    CHECK(t.edition == Point{512, 198});
    CHECK(t.trial == Point{512, 549});
    CHECK(t.order == Point{512, 574});
    CHECK(t.hint == Point{512, 510});
    CHECK(t.copyright == Point{512, 527});
    CHECK(t.esc == Point{512, 609});
    CHECK(t.picture == Point{374, 229});
    t = title_layout(640, 430, 40, 0);
    CHECK(t.operation == Point{230, 10});
    CHECK(t.picture == Point{182, 100});
    CHECK(t.esc == Point{320, 400});
    CHECK(title_offset(439) == 40);
    CHECK(title_offset(440) == 30);
    CHECK(title_offset(499) == 30);
    CHECK(title_offset(500) == 0);
}

TEST_CASE("intro bolt: seg8:0x1290 and the point rules of seg11:0xa4a6") {
    CHECK(newton_root(0, 0) == 1);   // g raised to 3: (0 / 3 + 3) / 2
    CHECK(newton_root(7, 25) == 5);  // (3, 4): (25 / 7 + 7) / 2
    CHECK(newton_root(250, 250 * 250) == 250);
    is::shell::Rng rng;
    rng.srand(0x1234);
    Bolt b;
    REQUIRE(make_bolt(rng, 512, 364, 450, 234, &b));
    // dx -62, dy -130: g = 192, L = (20744 / 192 + 192) / 2 = 150, N = 150 / 30 = 5
    REQUIRE(b.main.size() == 6);
    CHECK(b.main.front() == Point{512, 364});
    CHECK(b.main.back() == Point{450, 234});
    CHECK(b.branches.size() <= 16);
    for (const auto& br : b.branches) {
        bool on_main = false;
        for (const auto& p : b.main) on_main |= br[0] == p;
        CHECK(on_main);
    }
    for (const auto& sb : b.subs) {
        bool on_branch = false;
        for (const auto& br : b.branches) on_branch |= sb[0] == br[2];
        CHECK(on_branch);
    }
    // the same seed gives the same bolt
    is::shell::Rng again;
    again.srand(0x1234);
    Bolt c;
    REQUIRE(make_bolt(again, 512, 364, 450, 234, &c));
    CHECK(c.main == b.main);
    CHECK(c.branches.size() == b.branches.size());
    // a short bolt: L < 30 gives the end point alone
    Bolt s;
    REQUIRE(make_bolt(rng, 0, 0, 10, 10, &s));
    CHECK(s.main.size() == 1);
    CHECK(s.main[0] == Point{10, 10});
    // too long: L / 50 > 22
    Bolt l;
    CHECK_FALSE(make_bolt(rng, 0, 0, 1200, 0, &l));
    // segments of 50 between L / 30 > 22 and L / 50 <= 22
    Bolt m;
    if (make_bolt(rng, 0, 0, 1000, 0, &m)) CHECK(m.main.size() == 21);  // L = 1000, N = 20
}

TEST_CASE("intro bolt: seg12:0x7256 drops lines beyond one edge") {
    CHECK(line_visible({0, 0}, {10, 10}, 100, 50));
    CHECK(line_visible({-5, 10}, {10, 10}, 100, 50));
    CHECK_FALSE(line_visible({-5, 10}, {-1, 20}, 100, 50));
    CHECK_FALSE(line_visible({5, 51}, {10, 60}, 100, 50));
    CHECK_FALSE(line_visible({101, 5}, {120, 6}, 100, 50));
    CHECK_FALSE(line_visible({5, -1}, {10, -9}, 100, 50));
    CHECK(line_visible({100, 50}, {100, 50}, 100, 50));
}

TEST_CASE("intro pixels: the exact 2x doubling, clipped") {
    std::vector<std::uint8_t> src = {1, 2, 3, 0,  //
                                     4, 5, 6, 0};  // 3 x 2, stride 4
    std::vector<std::uint8_t> dst(8 * 5, 0xee);
    double_copy({dst.data(), 8, 5, 8}, 1, 1, {src.data(), 3, 2, 4}, 0, 0, 3, 2);
    const std::uint8_t want[5][8] = {
        {0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee, 0xee},
        {0xee, 1, 1, 2, 2, 3, 3, 0xee},
        {0xee, 1, 1, 2, 2, 3, 3, 0xee},
        {0xee, 4, 4, 5, 5, 6, 6, 0xee},
        {0xee, 4, 4, 5, 5, 6, 6, 0xee},
    };
    for (int y = 0; y < 5; ++y)
        for (int x = 0; x < 8; ++x) {
            CAPTURE(x);
            CAPTURE(y);
            CHECK(dst[static_cast<std::size_t>(y * 8 + x)] == want[y][x]);
        }
    // clipped at the destination's edges and to the source rows asked for
    std::vector<std::uint8_t> d2(4 * 2, 0);
    double_copy({d2.data(), 4, 2, 4}, -1, -1, {src.data(), 3, 2, 4}, 1, 0, 2, 1);
    CHECK(d2 == std::vector<std::uint8_t>{2, 3, 3, 0, 0, 0, 0, 0});
}

TEST_CASE("intro pixels: ordinal 12's stand-in maps colour k to its ramp entry") {
    CHECK(ordinal12_entry(0) == 185);
    CHECK(ordinal12_entry(1) == 188);
    CHECK(ordinal12_entry(9) == 212);
    CHECK(ordinal12_entry(10) == 213);
    CHECK(ordinal12_entry(14) == 229);
    CHECK(ordinal12_entry(15) == 0);
    is::fmt::Dib d;
    d.width = 2;
    d.height = 1;
    d.bpp = 8;
    d.palette.assign(256, 0);
    d.bits = {1, 14, 0, 0};
    d.bottom_up = false;
    const auto px = ordinal12_plain(d);
    CHECK(px == std::vector<std::uint8_t>{188, 188, 229, 229, 188, 188, 229, 229});
}

TEST_CASE("intro texts: golden: the strings and resources the intro reads") {
    const auto dir = original_dir();
    if (dir.empty()) {
        MESSAGE("IS_ORIGINAL_DIR not set: skipped");
        return;
    }
    is::assets::AssetStore store(dir);
    const auto& exe = store.module(is::assets::kMainProgram);
    const auto s167 = is::fmt::load_string(exe, 167), s233 = is::fmt::load_string(exe, 233),
               s607 = is::fmt::load_string(exe, 607);
    REQUIRE(s167);
    REQUIRE(s233);
    REQUIRE(s607);
    CHECK(version_string(*s167, *s233) == "V1.29.08");
    CHECK(edition_line(store.string(2339), version_string(*s167, *s233), *s607) ==
          " Standard Edition - V1.29.08 Aug 9/2000");
    CHECK(title_name_line(store.string(2338)) == "limited inner space\x99");
    CHECK(title_name_line(store.string(2337)) == "inner space\x99");
    CHECK(store.string(2118) == "presents.. ");
    CHECK(format_day(store.string(2341), 12086) == "You are on day 12086 of your demo trial period.");
    CHECK(store.string(1000) == "Directed By");
    CHECK(store.string(982) == "Doug 'The Thin Man' Blackley ");
    CHECK(store.string(1005) == "For");
    CHECK(store.string(985) == "Chaela & Chance");
    for (const KeyRow& r : key_rows()) {
        CHECK(store.find_string(static_cast<std::uint16_t>(r.key_id)));
        CHECK(store.find_string(static_cast<std::uint16_t>(r.desc_id)));
    }
    CHECK(store.string(1207) == "F1, F2");
    CHECK(store.string(1313) == "Exit Game");

    const auto& lang = store.language_dll();
    const is::fmt::NeResource* res = lang.find(5000, 136);
    REQUIRE(res);
    const std::vector<std::uint8_t> raw(lang.data(*res), lang.data(*res) + lang.size(*res));
    const auto p0 = text_page_lines(raw, 0), p1 = text_page_lines(raw, 1);
    CHECK(p0[0] == "We hope you have enjoyed playing Inner Space for the ");
    CHECK(p0[2].empty());
    CHECK(p0[10] == "ACT NOW AND GET INNER SPACE AT A GREAT PRICE!");
    CHECK(p1[0] == "Inner Space is an exciting action game with stimulating audio");
    CHECK(p1[9] == " 250-721-2206 ");
    CHECK(p1[10].empty());
    CHECK(lang.find(5000, 200));

    // graphic 300: colour 0 is never used, so the stand-in uses entries 188-229 and black only
    const is::fmt::Dib d300 = store.sprite(300, is::assets::kGameDll);
    CHECK(d300.width == 138);
    CHECK(d300.height == 138);
    const auto px = ordinal12_plain(d300);
    CHECK(px.size() == 276u * 276u);
    std::set<int> used(px.begin(), px.end());
    CHECK(used.count(0) == 1);
    for (int e : used) CHECK((e == 0 || (e >= 188 && e <= 229)));
    CHECK(used.count(185) == 0);
}
