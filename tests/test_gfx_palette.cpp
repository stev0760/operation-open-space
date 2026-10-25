// The palette manager of ISPACE.DLL (docs/re/shell.md "Palette", docs/re/palette-tables.md):
// every command's entry ranges, ordinal 14, the colour tables, and the title palette rebuilt from
// the documented call sequence. The #5500 100 golden part reads ISPACE.DLL when IS_ORIGINAL_DIR
// is set.
#include <cstdlib>
#include <filesystem>

#include "assets/asset_store.h"
#include "doctest.h"
#include "gfx/palette.h"

using namespace is::gfx;

namespace {

// A #5500 100 stand-in: 92 distinct non-black colours.
Colours5500 fake5500() {
    Colours5500 c{};
    for (int i = 0; i < 92; ++i)
        c[i] = {static_cast<std::uint8_t>(i + 1), static_cast<std::uint8_t>(200 - i),
                static_cast<std::uint8_t>(50 + i)};
    return c;
}

bool all_black(const PaletteArray& p, int first, int last) {
    for (int i = first; i <= last; ++i)
        if (!p[i].black()) return false;
    return true;
}

void check_row(const PaletteArray& p, int first, const Scheme10& row) {
    for (int k = 0; k < 10; ++k) {
        CAPTURE(first + k);
        CHECK(p[first + k].rgb() == row[k]);
    }
}

std::filesystem::path original_dir() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    return v;
}

}  // namespace

TEST_CASE("gfx palette: the colour tables keep seg5:0x5846's interpolations") {
    // Loop 1: rows 0, 1, 3, 6, 10 and 12 of 0x117c keep T0 - idiv((T0 - T9) k, 9)
    for (int row : {0, 1, 3, 6, 10, 12}) {
        const Scheme10& r = kSchemes10[row];
        for (int k = 1; k <= 8; ++k) {
            CAPTURE(row);
            CAPTURE(k);
            CHECK(r[k].r == r[0].r - (r[0].r - r[9].r) * k / 9);
            CHECK(r[k].g == r[0].g - (r[0].g - r[9].g) * k / 9);
            CHECK(r[k].b == r[0].b - (r[0].b - r[9].b) * k / 9);
        }
    }
    // Loop 2: rows 0, 1, 3, 5, 6, 8, 10, 12 and 13 of 0x1cdc keep U0 - trunc((U0 - U4) k / 4)
    for (int row : {0, 1, 3, 5, 6, 8, 10, 12, 13}) {
        const Scheme5& r = kSchemes5[row];
        for (int k = 1; k <= 3; ++k) {
            CAPTURE(row);
            CHECK(r[k].r == r[0].r - (r[0].r - r[4].r) * k / 4);
            CHECK(r[k].g == r[0].g - (r[0].g - r[4].g) * k / 4);
            CHECK(r[k].b == r[0].b - (r[0].b - r[4].b) * k / 4);
        }
    }
    for (const Rgb& c : kSchemes10[12]) CHECK(c.black());
    for (const Rgb& c : kSchemes5[12]) CHECK(c.black());
    CHECK(kSchemes10[7][0] == Rgb::of(0xc8b0c0));
    CHECK(kSchemes10[17][9] == Rgb::of(0x783c0a));
    CHECK(kSchemes5[16][4] == Rgb::of(0x3c3c3c));
    // command 7's switch
    CHECK(ramp_row(8) == 7);
    CHECK(ramp_row(5) == 1);
    CHECK(ramp_row(3) == 15);
    CHECK(ramp_row(13) == 2);
    CHECK(ramp_row(4) == -1);
    CHECK(ramp_row(2) == -1);
}

TEST_CASE("gfx palette: command 0 creates the 256-entry palette with the static colours") {
    PaletteManager pm;
    int published = 0, first = -1, count = -1;
    pm.on_publish([&](int f, int n) {
        ++published;
        first = f;
        count = n;
    });
    int alphabet = 0;
    pm.on_alphabet([&](bool load) { alphabet += load ? 1 : -1; });
    const PalHandle h = pm.create(0);
    REQUIRE(h != 0);
    CHECK(pm.kind(h) == 0);
    CHECK(alphabet == 1);
    CHECK(pm.alphabet_refs() == 1);
    const PaletteArray& w = *pm.working(h);
    CHECK(w[0].rgb() == Rgb::of(0x000000));
    CHECK(w[1].rgb() == Rgb::of(0x800000));
    CHECK(w[7].rgb() == Rgb::of(0xc0c0c0));
    CHECK(w[9].rgb() == Rgb::of(0xa6caf0));
    CHECK(w[246].rgb() == Rgb::of(0xfffbf0));
    CHECK(w[247].rgb() == Rgb::of(0xa0a0a4));
    CHECK(w[249].rgb() == Rgb::of(0xff0000));
    CHECK(w[255].rgb() == Rgb::of(0xffffff));
    for (int i = 0; i < 256; ++i) {
        CAPTURE(i);
        if (i >= 10 && i <= 245) {
            CHECK(w[i] == PalEntry{0, 0, 0, kPcReserved});
        } else {
            CHECK(w[i].flags == 0);
            CHECK(is_static_entry(i));
        }
        CHECK(pm.device()[i] == w[i].rgb());
    }
    CHECK(*pm.stored(h, 0) == w);
    CHECK(*pm.stored(h, 1) == w);
    CHECK(published == 1);
    CHECK(first == 0);
    CHECK(count == 256);
    // three slots; a fourth create fails; delete frees a slot and the handle is new
    const PalHandle h2 = pm.create(1), h3 = pm.create(0);
    CHECK(h2 != 0);
    CHECK(h3 != 0);
    CHECK(pm.create(0) == 0);
    CHECK(pm.alphabet_refs() == 3);
    pm.destroy(h2);
    CHECK_FALSE(pm.valid(h2));
    const PalHandle h4 = pm.create(0);
    CHECK(h4 != 0);
    CHECK(h4 != h2);
    pm.destroy(h);
    pm.destroy(h3);
    CHECK(alphabet == 1);
    pm.destroy(h4);
    CHECK(alphabet == 0);  // the last palette deletes ALPHABET
    CHECK(pm.alphabet_refs() == 0);
    // commands on an unknown handle do nothing
    pm.load_scheme(h, 8, 0);
    pm.animate(h);
    CHECK(pm.allocate_colour(h, 1, 2, 3) == -1);
    CHECK(published == 4);
}

TEST_CASE("gfx palette: command 1 schemes and the ramps of each mode") {
    PaletteManager pm;
    pm.set_colours_5500(fake5500());
    const PalHandle h = pm.create(0);

    SUBCASE("scheme -1000: entries 10-245 white, no #5500, no ramp") {
        pm.load_scheme(h, -1000, 2);
        const PaletteArray& w = *pm.working(h);
        CHECK(pm.mode(h) == 2);
        for (int i = 10; i < 246; ++i) {
            CAPTURE(i);
            CHECK(w[i] == PalEntry{255, 255, 255, kPcReserved});
        }
        // nothing animates in command 1 itself without a ramp
        CHECK(pm.device()[10].black());
        // mode 2 leaves the ramp tables alone
        CHECK(pm.ramp_of(2, 0) == -1);
    }
    SUBCASE("scheme 8 mode 0: #5500 at 11-102 and nine ramps from 103") {
        const auto gen = pm.generation();
        pm.load_scheme(h, 8, 0);
        const PaletteArray& w = *pm.working(h);
        CHECK(w[10].black());
        const Colours5500 f = fake5500();
        for (int i = 0; i < 92; ++i) CHECK(w[11 + i].rgb() == f[i]);
        // 8 at 103, then 5, 6, 7, 9, 10, 11, 12, 13
        const int order[] = {8, 5, 6, 7, 9, 10, 11, 12, 13};
        for (int k = 0; k < 9; ++k) {
            CAPTURE(order[k]);
            check_row(w, 103 + 10 * k, kSchemes10[ramp_row(order[k])]);
            CHECK(pm.ramp_of(0, order[k]) == 5 + k);
            CHECK(pm.ramp_record(0, 5 + k).first == 103 + 10 * k);
            CHECK(pm.ramp_record(0, 5 + k).last == 112 + 10 * k);
            CHECK(pm.ramp_uses(0, 5 + k) == 1);
        }
        CHECK(all_black(w, 193, 245));
        for (int i = 10; i < 246; ++i) CHECK(w[i].flags == kPcReserved);
        // the presets of records 0-4 and of the unused records
        CHECK(pm.ramp_record(0, 0).first == 11);
        CHECK(pm.ramp_record(0, 0).last == 27);
        CHECK(pm.ramp_record(0, 1).first == 28);
        CHECK(pm.ramp_record(0, 1).last == 62);
        CHECK(pm.ramp_record(0, 2).first == 73);
        CHECK(pm.ramp_record(0, 2).last == 89);
        CHECK(pm.ramp_record(0, 3).first == 63);
        CHECK(pm.ramp_record(0, 3).last == 72);
        CHECK(pm.ramp_record(0, 4).first == 90);
        CHECK(pm.ramp_record(0, 4).last == 102);
        CHECK(pm.ramp_record(0, 18).first == 233);
        CHECK(pm.ramp_record(0, 18).last == 242);
        for (int c = 0; c < 5; ++c) CHECK(pm.ramp_of(0, c) == c);
        CHECK(pm.ramp_of(0, 14) == -1);
        // the ramps animated (seg5:0x61b2 ends with command 3)
        CHECK(pm.generation() == gen + 9);
        CHECK(pm.device()[103] == kSchemes10[7][0]);
    }
    SUBCASE("scheme 8 mode 1 and mode 2 allocate only scheme 8") {
        pm.load_scheme(h, 8, 1);
        check_row(*pm.working(h), 103, kSchemes10[7]);
        CHECK(all_black(*pm.working(h), 113, 245));
        pm.load_scheme(h, 8, 2);
        check_row(*pm.working(h), 103, kSchemes10[7]);
        CHECK(all_black(*pm.working(h), 113, 245));
    }
    SUBCASE("other negative schemes black only 10-182") {
        pm.load_scheme(h, -1000, 2);
        pm.load_scheme(h, -5, 1);
        const PaletteArray& w = *pm.working(h);
        CHECK(w[10].black());
        CHECK(w[11].rgb() == fake5500()[0]);
        CHECK(all_black(w, 103, 182));
        for (int i = 183; i < 246; ++i) CHECK(w[i].rgb() == Rgb{255, 255, 255});
    }
    SUBCASE("c >= 100 and c = -2 black 73-182 and subtract 100") {
        pm.load_scheme(h, 108, 1);
        const PaletteArray& w = *pm.working(h);
        CHECK(w[72].rgb() == fake5500()[61]);
        // the first black pair is now 73/74
        check_row(w, 73, kSchemes10[7]);
        CHECK(all_black(w, 83, 245));
        pm.load_scheme(h, -2, 1);
        CHECK(all_black(*pm.working(h), 73, 245));
        CHECK(pm.working(h)->at(72).rgb() == fake5500()[61]);
    }
    SUBCASE("a kind-1 palette gets no #5500 and no ramps") {
        const PalHandle k1 = pm.create(1);
        pm.load_scheme(k1, 8, 0);
        CHECK(all_black(*pm.working(k1), 10, 245));
    }
}

TEST_CASE("gfx palette: command 7 reuses, reallocates and stops as seg5:0x61b2") {
    PaletteManager pm;
    pm.set_colours_5500(fake5500());
    const PalHandle h = pm.create(0);
    pm.load_scheme(h, 8, 1);
    // c = 5 goes to the next black pair, 113
    pm.allocate_ramp(h, 5);
    check_row(*pm.working(h), 113, kSchemes10[1]);
    CHECK(pm.ramp_of(1, 5) == 6);
    CHECK(pm.ramp_record(1, 6).first == 113);
    // asked again in mode 1: reused, its count goes up, nothing is written
    const auto gen = pm.generation();
    pm.allocate_ramp(h, 105);
    CHECK(pm.ramp_uses(1, 6) == 2);
    CHECK(all_black(*pm.working(h), 123, 245));
    CHECK(pm.generation() == gen);
    // -1 does nothing; c = 4 takes a record but its row is unset: nothing written, animated
    pm.allocate_ramp(h, -1);
    CHECK(pm.generation() == gen);
    pm.allocate_ramp(h, 4);
    CHECK(pm.ramp_of(1, 4) == 4);  // scheme 4 is preset to record 4: reused
    pm.allocate_ramp(h, 14);
    CHECK(pm.ramp_of(1, 14) == 7);
    CHECK(pm.ramp_record(1, 7).first == 123);
    CHECK(all_black(*pm.working(h), 123, 245));
    CHECK(pm.generation() == gen + 1);
    // mode 2 reallocates every time
    pm.load_scheme(h, 8, 2);
    pm.allocate_ramp(h, 8);
    check_row(*pm.working(h), 113, kSchemes10[7]);
    CHECK(pm.ramp_record(2, 5).first == 113);
    // a kind-1 palette: nothing
    const PalHandle k1 = pm.create(1);
    pm.allocate_ramp(k1, 8);
    CHECK(all_black(*pm.working(k1), 10, 245));
}

TEST_CASE("gfx palette: command 7 without a black pair or a free record") {
    PaletteManager pm;
    const PalHandle h = pm.create(0);
    pm.load_scheme(h, -1000, 1);  // all white: no black pair anywhere
    auto gen = pm.generation();
    pm.allocate_ramp(h, 6);
    CHECK(pm.ramp_of(1, 6) == 5);
    CHECK(pm.generation() == gen + 1);  // animates without writing
    for (int i = 10; i < 246; ++i) CHECK(pm.working(h)->at(i).rgb() == Rgb{255, 255, 255});
    // records 6-18 for schemes 7-13 and 14-19: then none is free
    for (int c = 7; c <= 19; ++c) pm.allocate_ramp(h, c);
    CHECK(pm.ramp_uses(1, 18) == 1);
    gen = pm.generation();
    pm.allocate_ramp(h, 20);
    CHECK(pm.ramp_of(1, 20) == -1);
    CHECK(pm.generation() == gen);  // exits without scanning or animating
}

TEST_CASE("gfx palette: command 3 publishes entries 10-245 only") {
    PaletteManager pm;
    pm.set_colours_5500(fake5500());
    const PalHandle h = pm.create(0);
    int first = -1, count = -1;
    pm.on_publish([&](int f, int n) {
        first = f;
        count = n;
    });
    pm.load_scheme(h, -1000, 2);
    CHECK(pm.device()[100].black());
    pm.animate(h);
    CHECK(first == 10);
    CHECK(count == 236);
    CHECK(pm.device()[10] == Rgb{255, 255, 255});
    CHECK(pm.device()[245] == Rgb{255, 255, 255});
    CHECK(pm.device()[9] == Rgb::of(0xa6caf0));
    CHECK(pm.device()[246] == Rgb::of(0xfffbf0));
}

TEST_CASE("gfx palette: commands 4 and 5 save, blank and restore entries 10-245") {
    PaletteManager pm;
    pm.set_colours_5500(fake5500());
    const PalHandle h = pm.create(0);
    pm.load_scheme(h, 8, 0);
    const PaletteArray before = *pm.working(h);
    pm.save_set(h, 1, 1);
    CHECK(all_black(*pm.working(h), 10, 245));
    CHECK(pm.working(h)->at(5).rgb() == before[5].rgb());
    for (int i = 10; i < 246; ++i) CHECK(pm.stored(h, 1)->at(i).rgb() == before[i].rgb());
    CHECK(pm.device()[103].black());  // animated
    pm.restore_set(h, 1);
    CHECK(*pm.working(h) == before);
    CHECK(pm.device()[103] == before[103].rgb());
    // c = 0 saves without blanking; set 0 is separate
    pm.save_set(h, 0, 0);
    CHECK(*pm.working(h) == before);
    // an invalid set does nothing
    pm.save_set(h, 2, 1);
    CHECK(*pm.working(h) == before);
}

TEST_CASE("gfx palette: command 6 allocates a colour") {
    PaletteManager pm;
    const PalHandle h = pm.create(0);
    // kind 0 scans upward from 10; the first entry scanned is never taken
    CHECK(pm.allocate_colour(h, 100, 100, 100) == 11);
    CHECK(pm.working(h)->at(11).rgb() == Rgb{100, 100, 100});
    CHECK(pm.allocate_colour(h, 100, 100, 100) == -1);   // already there
    CHECK(pm.allocate_colour(h, 103, 97, 102) == -1);    // within 3
    CHECK(pm.allocate_colour(h, 104, 100, 100) == 12);   // 4 away
    CHECK(pm.allocate_colour(h, 2, 2, 2) == -1);         // within 3 of black entry 10
    // kind 1 scans downward from 245
    const PalHandle k1 = pm.create(1);
    CHECK(pm.allocate_colour(k1, 50, 60, 70) == 244);
    // (1, 11, 111) counts as free
    pm.swap_black(k1, 0);
    CHECK(pm.working(k1)->at(200).rgb() == Rgb{1, 11, 111});
    CHECK(pm.allocate_colour(k1, 90, 60, 70) == 243);
    // a full palette returns -1
    pm.load_scheme(h, -1000, 2);
    CHECK(pm.allocate_colour(h, 10, 20, 30) == -1);
}

TEST_CASE("gfx palette: command 8 swaps black and (1, 11, 111) on kind 1 only") {
    PaletteManager pm;
    const PalHandle k0 = pm.create(0), k1 = pm.create(1);
    pm.swap_black(k1, 0);
    for (int i = 10; i < 245; ++i) CHECK(pm.working(k1)->at(i).rgb() == Rgb{1, 11, 111});
    CHECK(pm.working(k1)->at(245).black());
    pm.swap_black(k1, 1);
    CHECK(all_black(*pm.working(k1), 10, 245));
    pm.swap_black(k0, 0);
    CHECK(all_black(*pm.working(k0), 10, 245));
    // the dispatcher reaches it
    pm.command(k1, 0, 0, 0, 8);
    CHECK(pm.working(k1)->at(10).rgb() == Rgb{1, 11, 111});
}

TEST_CASE("gfx palette: ordinal 14 scales 185-229 (mode 3) or 230-245 with 16-bit products") {
    PaletteManager pm;
    const PalHandle h = pm.create(0);
    pm.load_scheme(h, -1000, 1);  // 10-245 white
    pm.scale(h, 3, 50);
    const PaletteArray& w = *pm.working(h);
    CHECK(w[184].rgb() == Rgb{255, 255, 255});
    // 255 * 50 = 12750: 127
    for (int i = 185; i < 230; ++i) CHECK(w[i].rgb() == Rgb{127, 127, 127});
    CHECK(w[230].rgb() == Rgb{255, 255, 255});
    CHECK(pm.device()[200] == Rgb{127, 127, 127});  // animated
    pm.scale(h, 2, 40);
    for (int i = 230; i < 246; ++i) CHECK(w[i].rgb() == Rgb{102, 102, 102});
    CHECK(w[229].rgb() == Rgb{127, 127, 127});
    // 127 * 300 = 38100 wraps to -27436 as a word: / 100 = -274, low byte 0xee
    // [BYTES ISPACE.DLL seg5:0x5186-0x5192]; shell.md states only the cap [OPEN]
    pm.scale(h, 3, 300);
    CHECK(w[185].rgb() == Rgb{0xee, 0xee, 0xee});
    // 102 * 300 = 30600 stays positive: 306, clamped to 255
    pm.scale(h, 0, 300);
    CHECK(w[230].rgb() == Rgb{255, 255, 255});
    // percent 0 blacks the range
    pm.scale(h, 3, 0);
    CHECK(all_black(w, 185, 229));
}

TEST_CASE("gfx palette: ordinal 14 on a kind-1 palette blends rows into 10-82") {
    PaletteManager pm;
    const PalHandle h = pm.create(1);
    // with both rows 0 (the slot's start), percent 40 blends row 0 into itself
    pm.scale(h, 0, 90);  // capped at 40
    const PaletteArray& w = *pm.working(h);
    CHECK(w[10].rgb() == kSchemes10[0][0]);
    CHECK(w[82].rgb() == kSchemes10[0][9]);
    CHECK(w[83].black());
    pm.scale(h, 1, 40);
    CHECK(w[83].rgb() == kSchemes5[0][0]);
    CHECK(w[111].rgb() == kSchemes5[0][4]);
    pm.scale(h, 2, 40);
    CHECK(w[112].rgb() == kSchemes5[0][0]);
    CHECK(w[128].rgb() == kSchemes5[0][4]);
    CHECK(w[129].black());
}

TEST_CASE("gfx palette: the dispatcher maps a, b, c to each command") {
    PaletteManager pm;
    pm.set_colours_5500(fake5500());
    const int h = pm.command(0, 0, 0, 0, 0);
    REQUIRE(h != 0);
    const PalHandle ph = static_cast<PalHandle>(h);
    pm.command(ph, 0, 1, 8, 1);  // scheme c = 8, mode b = 1
    CHECK(pm.mode(ph) == 1);
    check_row(*pm.working(ph), 103, kSchemes10[7]);
    pm.command(ph, 0, 0, 5, 7);  // ramp c = 5
    check_row(*pm.working(ph), 113, kSchemes10[1]);
    pm.command(ph, 0, 1, 1, 4);  // save to set b = 1, blank (c)
    CHECK(all_black(*pm.working(ph), 10, 245));
    pm.command(ph, 0, 1, 0, 5);  // restore set 1
    CHECK(pm.working(ph)->at(103).rgb() == kSchemes10[7][0]);
    CHECK(pm.command(ph, 9, 8, 7, 6) == 123);  // colour (a, b, c) = (9, 8, 7)
    CHECK(pm.working(ph)->at(123).rgb() == Rgb{9, 8, 7});
    pm.command(ph, 0, 0, 0, 2);
    CHECK_FALSE(pm.valid(ph));
}

TEST_CASE("gfx palette: seg5:0x26a2 recolours graphic tables") {
    PaletteManager pm;
    const PalHandle h = pm.create(0);
    std::vector<std::uint32_t> t(16, 0);
    for (int k = 0; k < 15; ++k) t[k] = 0x101010u * static_cast<std::uint32_t>(15 - k);
    SUBCASE("a = b = 12 leaves the table; d = 3 writes no entry") {
        auto u = t;
        const auto gen = pm.generation();
        pm.recolour_graphic(h, 300, u, 3, 12, 12, 0, false);
        CHECK(u == t);
        CHECK(pm.generation() == gen);
    }
    SUBCASE("rows a and b replace 0-9 and 10-14") {
        auto u = t;
        pm.recolour_graphic(h, 300, u, 3, 4, 7, 0, false);
        for (int k = 0; k < 10; ++k) CHECK(u[k] == kSchemes10[4][k].rgb());
        for (int k = 10; k < 15; ++k) CHECK(u[k] == kSchemes5[7][k - 10].rgb());
        // id 0x118 skips it
        auto v = t;
        pm.recolour_graphic(h, 0x118, v, 3, 4, 7, 0, false);
        CHECK(v == t);
    }
    SUBCASE("entries 10-14 that repeat one of 0-9 move by R +- 4 and G +- 4") {
        auto u = t;
        u[11] = u[3];
        u[12] = 0xfffe10;
        u[2] = 0xfffe10;
        pm.recolour_graphic(h, 300, u, 3, 12, 12, 0, false);
        CHECK(u[11] == t[3] + 0x040400);
        CHECK(u[12] == 0xfbfa10);
    }
    SUBCASE("a non-black entry 15 brings the fixed 16 colours") {
        auto u = t;
        u[15] = 0x000100;
        pm.recolour_graphic(h, 300, u, 3, 12, 12, 0, false);
        CHECK(u[0] == 0x00ff00);
        CHECK(u[12] == 0xffffff);
        CHECK(u[15] == 0);
        // red alone counts only for ordinal 12 (DGROUP:0x188); green and blue count always
        // [BYTES ISPACE.DLL seg5:0x2706-0x2726]
        auto v = t;
        v[15] = 0x010000;
        pm.recolour_graphic(h, 300, v, 3, 12, 12, 0, false);
        CHECK(v[0] == t[0]);
        pm.recolour_graphic(h, 300, v, 3, 12, 12, 0, true);
        CHECK(v[0] == 0x00ff00);
        auto b = t;
        b[15] = 0x000001;
        pm.recolour_graphic(h, 300, b, 3, 12, 12, 0, true);
        CHECK(b[0] == 0x00ff00);
    }
    SUBCASE("a non-zero last argument copies the table into row 12") {
        auto u = t;
        pm.recolour_graphic(h, 300, u, 3, 12, 12, 1, false);
        CHECK(pm.schemes10()[12][3].rgb() == t[3]);
        CHECK(pm.schemes5()[12][1].rgb() == t[11]);
        // command 0's seg5:0x5846 clears it again
        const PalHandle h2 = pm.create(0);
        (void)h2;
        CHECK(pm.schemes10()[12][3].black());
    }
    SUBCASE("d = 2 allocates every colour with command 6 and animates") {
        std::vector<std::uint32_t> u = {0x102030, 0x405060};
        const auto gen = pm.generation();
        pm.recolour_graphic(h, 300, u, 2, 12, 12, 0, false);
        CHECK(u.size() == 2);
        CHECK(pm.working(h)->at(11).rgb() == Rgb::of(0x102030));
        CHECK(pm.working(h)->at(12).rgb() == Rgb::of(0x405060));
        CHECK(pm.generation() == gen + 1);
    }
    SUBCASE("d = 0 with rising brightness and a = b = 12 writes no ramp") {
        std::vector<std::uint32_t> u(16, 0);
        for (int k = 0; k < 15; ++k) u[k] = 0x0a0a0au * static_cast<std::uint32_t>(k + 1);
        pm.recolour_graphic(h, 300, u, 0, 12, 12, 0, false);
        CHECK(all_black(*pm.working(h), 185, 229));
    }
}

TEST_CASE("gfx palette: colour_index resolves COLORREFs as the 8-bit display") {
    PaletteManager pm;
    const PalHandle h = pm.create(0);
    pm.load_scheme(h, -1000, 2);
    pm.animate(h);  // 10-245 white
    const DeviceColours& d = pm.device();
    CHECK(colour_index(0x01000005, d) == 5);    // PALETTEINDEX(5)
    CHECK(colour_index(0x10FF00C8, d) == 200);  // DIBINDEX(200)
    CHECK(colour_index(0x00FFFFFF, d) == 255);  // RGB white: the static white
    CHECK(colour_index(0x02FFFFFF, d) == 10);   // PALETTERGB white: the first white entry
    CHECK(colour_index(0x00000000, d) == 0);
    CHECK(colour_index(0x00000080, d) == 1);    // RGB(0x80, 0, 0)
    CHECK(colour_index(0x0000FF00, d) == 250);  // green
    CHECK(colour_index(0x0000FFFF, d) == 251);  // yellow
    CHECK(colour_index(0x00404040, d) == 0);    // dark grey: ties with 0x808080 go to 0
    CHECK(nearest_index(d, Rgb{250, 250, 250}) == 10);
}

TEST_CASE("gfx palette: the title palette from the documented call sequence") {
    PaletteManager pm;
    Colours5500 res = fake5500();
    bool golden = false;
    const auto dir = original_dir();
    if (!dir.empty()) {
        try {
            is::assets::AssetStore store(dir);
            const auto p = store.palette5500();
            for (int i = 0; i < 92; ++i) res[i] = {p.colours[i].r, p.colours[i].g, p.colours[i].b};
            golden = true;
        } catch (const std::exception& x) {
            MESSAGE("ISPACE.DLL #5500 100 not read: " << x.what());
        }
    }
    pm.set_colours_5500(res);
    // start-up: seg6:0x190c, then the 0x9cf handler seg2:0x458 (shell.md "Palette", Events)
    PalHandle h = pm.create(0);
    pm.load_scheme(h, -1000, 2);
    pm.animate(h);
    pm.destroy(h);
    h = pm.create(0);
    pm.load_scheme(h, 8, 2);
    pm.animate(h);
    // title step 0: seg15:0x5c35 (scheme 8 mode 1), seg15:0x6cfb (command 7 with c = 5),
    // seg15:0x757c (ordinal 12 of graphic 300: seg5:0x26a2 with d 0, a 1, b 12)
    pm.load_scheme(h, 8, 1);
    pm.animate(h);
    pm.allocate_ramp(h, 5);
    // graphic 300's colour table (ispace.md ordinal 1): 0-9 the gold ramp, 10-14 the greys
    std::vector<std::uint32_t> t300 = {0xffe66e, 0xecd162, 0xd8bc56, 0xc4a74a, 0xb0923e, 0x9c7d31,
                                       0x886825, 0x745319, 0x603e0d, 0x4c2800, 0xd7d7d6, 0xb9baba,
                                       0x9e9e9e, 0x737373, 0x565552, 0x000000};
    pm.recolour_graphic(h, 300, t300, 0, 1, 12, 0, true);

    const PaletteArray& w = *pm.working(h);
    const DeviceColours& dev = pm.device();
    for (int i = 0; i < 256; ++i) CHECK(dev[i] == w[i].rgb());  // all published
    for (int i = 10; i < 246; ++i) CHECK(w[i].flags == kPcReserved);
    CHECK(w[10].black());
    for (int i = 0; i < 92; ++i) CHECK(w[11 + i].rgb() == res[i]);
    check_row(w, 103, kSchemes10[7]);
    check_row(w, 113, kSchemes10[1]);
    CHECK(all_black(w, 123, 184));
    const std::uint32_t r185[] = {0xffe66e, 0xf9df6a, 0xf3d866, 0xecd162, 0xe6ca5e, 0xe0c35a,
                                  0xd8bc56, 0xd2b552, 0xccae4e, 0xc4a74a, 0xbea046, 0xb89942,
                                  0xb0923e, 0xaa8b3a, 0xa48436, 0x9c7d31, 0x96762d, 0x906f29,
                                  0x886825, 0x826121, 0x7c5a1d, 0x745319, 0x6e4c15, 0x684511,
                                  0x603e0d, 0x5a3709, 0x543005, 0x4c2800};
    for (int k = 0; k < 28; ++k) {
        CAPTURE(185 + k);
        CHECK(w[185 + k].rgb() == Rgb::of(r185[k]));
    }
    const std::uint32_t r213[] = {0xd7d7d6, 0xd0d0cf, 0xc9c9c8, 0xc2c2c1, 0xb9baba, 0xb3b3b3,
                                  0xadacac, 0xa7a5a5, 0x9e9e9e, 0x949494, 0x8a8a8a, 0x808080,
                                  0x737373, 0x6c6c6b, 0x656563, 0x5e5e5b, 0x565552};
    for (int k = 0; k < 17; ++k) {
        CAPTURE(213 + k);
        CHECK(w[213 + k].rgb() == Rgb::of(r213[k]));
    }
    CHECK(all_black(w, 230, 245));
    if (golden) {
        // 79 black entries: 10, 123-184 and 230-245 (palette-tables.md)
        int black = 0;
        for (int i = 10; i < 246; ++i) black += w[i].black();
        CHECK(black == 79);
        CHECK(w[11].rgb() == Rgb::of(0x00ffff));
        CHECK(w[12].rgb() == Rgb::of(0x00e9ff));
        CHECK(w[14].rgb() == Rgb::of(0x00bdff));
        CHECK(w[99].rgb() == Rgb::of(0x007200));
        CHECK(w[102].rgb() == Rgb::of(0x003f00));
        CHECK(w[73].rgb() == Rgb::of(0xffffff));
    } else {
        MESSAGE("IS_ORIGINAL_DIR not set: #5500 100 golden checks skipped");
    }
}
