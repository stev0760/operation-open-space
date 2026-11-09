// ISPACE.DLL ordinal 12, the smoothed 2x renderer (src/gfx/smooth_render.h; work/analysis/m4/
// s6-ord12.md, all claims CONFIRMED in s6-ord12-verify.md): seg7:0x4ee8's four modes, the run
// path and the flags DGROUP:0x1eaa/0x1eac, a small render, and golden: vector A (the title emblem,
// graphic 300) against probe-0001's title and vector B (the TRANSFORMER's pad ship, IGRAPHIC.DLL
// #4000 60) against core-0003's landing scene, plus the scheme-12 designs that take modes 0 and 1.
// The hand values come from work/analysis/m4/s6-ord12-packs/ord12_model_runs.py (the analyst's
// ord12_model.py extended to modes 0/1 and the runs) through ord12_handcases.py and
// ord12_test_values.py there. The goldens read IS_ORIGINAL_DIR and work/ beside it and skip
// without them.
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "assets/asset_store.h"
#include "doctest.h"
#include "formats/zip.h"
#include "gfx/palette.h"
#include "gfx/smooth_render.h"
#include "png_decode.h"

using namespace is::gfx;

namespace {

// The synthetic working palette of the hand cases: entry i = (7i, 255 - i, 3i) bytes.
PaletteArray synth_pal() {
    PaletteArray p{};
    for (int i = 0; i < 256; ++i)
        p[static_cast<std::size_t>(i)] = PalEntry{static_cast<std::uint8_t>(i * 7), static_cast<std::uint8_t>(255 - i),
                                                  static_cast<std::uint8_t>(i * 3), 0};
    return p;
}

std::uint32_t crc(const std::vector<std::uint8_t>& v) { return is::fmt::crc32(v.data(), v.size()); }

struct RampCase {
    int kind, c, count, first, mode, lo, hi, t1, t0, w_lo, w_hi;
    std::vector<std::uint32_t> written;  // entries w_lo ... w_hi after the call
};

// ord12_handcases.py
const std::vector<RampCase> kRampCases = {
    {0, 200, 10, 0, 2, 185, 212, 43, 99, -1, -1, {}},
    {0, 110, 5, 10, 3, 213, 229, 102, 60, -1, -1, {}},
    {0, 8, 3, 2, 0, 190, 198, 144, 36, 190, 198,
     {0xc8b0c0, 0xbe96c8, 0xb27de3, 0x9d5dff, 0x8d4dff, 0x7c37f6, 0x6a14d4, 0x3000b0, 0x00007d}},
    {0, 4, 2, 11, 1, 216, 222, 171, 58, 216, 222, {0xc8ff00, 0xafe900, 0x91ce00, 0x73b300, 0x559800, 0x377d00, 0x196200}},
    {1, 13, 4, 6, 0, 53, 82, 163, 40, 53, 82,
     {0xf0d264, 0xeacc61, 0xe4c65e, 0xdec05b, 0xdaba58, 0xd7b455, 0xd4ae52, 0xd1a94f, 0xcea44c, 0xcba049,
      0xc89b46, 0xc29743, 0xbc9240, 0xb48c3c, 0xae8839, 0xa88336, 0xa27f33, 0x9c7930, 0x96732d, 0x906d2a,
      0x8a6727, 0x825f24, 0x7b5821, 0x73501e, 0x6c4a1b, 0x624217, 0x5a3c14, 0x543611, 0x4e300e, 0x482a0b}},
    {1, 0, 5, 10, 1, 83, 111, 143, 25, 83, 111,
     {0x00dcdc, 0x00d7d9, 0x00d1d5, 0x00c6ce, 0x00c1ca, 0x00bbc7, 0x00b0c0, 0x00abbc, 0x00a0b5, 0x009ab1,
      0x0095ae, 0x008aa6, 0x0084a3, 0x007f9f, 0x007498, 0x006e94, 0x00638d, 0x005e8a, 0x005886, 0x004d7f,
      0x00487b, 0x004278, 0x003770, 0x00326d, 0x002765, 0x002162, 0x001c5e, 0x001157, 0x000b53}},
    {0, 2, 10, 0, 0, 185, 212, 198, 32, 185, 212,
     {0xffffff, 0xf8f8f9, 0xf0f0f2, 0xe9e9eb, 0xe1e1e4, 0xd7d7da, 0xd0d0d3, 0xc8c8cc, 0xc1c1c4, 0xb9b9bd,
      0xafafb4, 0xa8a8ac, 0xa0a0a5, 0x99999e, 0x8f8f94, 0x87878d, 0x808086, 0x78787f, 0x717178, 0x67676e,
      0x5f5f67, 0x585860, 0x505058, 0x494951, 0x3f3f48, 0x373740, 0x303039, 0x282832}},
    {0, 9, 5, 10, 1, 213, 229, 171, 71, 213, 229,
     {0xffd2a0, 0xf7c997, 0xefc08e, 0xe3b381, 0xdbaa78, 0xd5a270, 0xcd9765, 0xc8905e, 0xc38957, 0xbb7f4d,
      0xb57947, 0xaf7341, 0xa66a38, 0xa06432, 0x985c2a, 0x8c501e, 0x844816}},
    {0, 3, 1, 9, 0, 210, 212, 120, 69, 210, 212, {0xa49c7d, 0x737b6e, 0x5f5946}},
    {1, 112, 5, 10, 3, 83, 111, 126, 87, -1, -1, {}},
};

}  // namespace

TEST_CASE("smooth: seg7:0x4ee8's four modes on both palette kinds") {
    for (const RampCase& rc : kRampCases) {
        CAPTURE(rc.kind);
        CAPTURE(rc.c);
        CAPTURE(rc.first);
        PaletteArray w = synth_pal();
        const PaletteArray before = w;
        int animated = 0;
        SmoothPalette sp;
        sp.working = &w;
        sp.kind = rc.kind;
        sp.animate = [&animated] { ++animated; };
        SmoothGroup g;
        const int mode = smooth_ramp(sp, rc.c, rc.count, rc.first, g);
        CHECK(mode == rc.mode);
        CHECK(g.lo == rc.lo);
        CHECK(g.hi == rc.hi);
        CHECK(g.t1 == rc.t1);
        CHECK(g.t0 == rc.t0);
        CHECK(animated == (mode < 2 ? 1 : 0));  // modes 0 and 1 animate
        for (int i = 0; i < 256; ++i) {
            CAPTURE(i);
            const bool in = i >= rc.w_lo && i <= rc.w_hi && rc.w_lo >= 0;
            if (in)
                CHECK(w[static_cast<std::size_t>(i)].rgb().rgb() == rc.written[static_cast<std::size_t>(i - rc.w_lo)]);
            else
                CHECK(w[static_cast<std::size_t>(i)] == before[static_cast<std::size_t>(i)]);
            CHECK(w[static_cast<std::size_t>(i)].flags == 0);  // only bytes 0-2 are written
        }
    }
    // c = -1 returns at once [BYTES ISPACE.DLL seg7:0x4f2d-0x4f33]
    PaletteArray w = synth_pal();
    SmoothPalette sp;
    sp.working = &w;
    SmoothGroup g{1, 2, 3, 4};
    CHECK(smooth_ramp(sp, -1, 10, 0, g) == -1);
    CHECK(g == SmoothGroup{1, 2, 3, 4});
}

TEST_CASE("smooth: the runs of b = 12 give one group per run of equal seg5:0x66ea ids") {
    // ord12_handcases.py: colours 0-9 and 10-14 non-monotonic, b1 = b2 = 12, kind 0.
    const std::vector<std::uint32_t> tab = {0x4040c0, 0x3030a0, 0xffe060, 0xe0c050, 0xc0a040, 0x20c020,
                                            0x109010, 0x086008, 0x909090, 0x505050, 0xc02020, 0x801010,
                                            0x2020e0, 0x101090, 0x606060, 0x000000};
    PaletteArray w = synth_pal();
    const PaletteArray before = w;
    int animated = 0;
    SmoothPalette sp;
    sp.working = &w;
    sp.animate = [&animated] { ++animated; };
    const SmoothTables t = smooth_tables(tab, 12, 12, sp);
    const std::array<int, 15> ids = {0, 0, 1, 1, 9, 4, 4, 4, 2, 2, 1, 1, 0, 0, 2};
    const std::array<int, 16> L = {83, 67, 174, 150, 127, 113, 84, 56, 116, 64, 86, 57, 85, 54, 77, 0};
    const std::array<int, 16> grp = {0, 0, 1, 1, 2, 3, 3, 3, 4, 4, 0, 0, 1, 1, 2, 100};
    for (int k = 0; k < 15; ++k) CHECK(t.id[static_cast<std::size_t>(k)] == ids[static_cast<std::size_t>(k)]);
    CHECK(t.id[15] == 100);
    CHECK(t.L == L);
    CHECK(t.grp == grp);
    CHECK(t.modes == std::vector<int>{0, 0, 0, 0, 0, 1, 1, 1});
    CHECK(animated == 8);
    const std::vector<SmoothGroup> g0 = {{185, 189, 123, 44}, {190, 195, 183, 47}, {196, 198, 171, 86},
                                         {199, 206, 165, 51}, {207, 212, 198, 44}};
    const std::vector<SmoothGroup> g1 = {{213, 218, 183, 87}, {219, 225, 143, 32}, {226, 229, 198, 46}};
    CHECK(t.groups[0] == g0);
    CHECK(t.groups[1] == g1);
    const std::array<std::uint32_t, 45> want = {
        0x00aaff, 0x008ae7, 0x0069ca, 0x0041ae, 0x001690, 0xfffa3c, 0xffad00, 0xfd6000, 0xe50800, 0xb40000,
        0x6c0000, 0xffd2a0, 0xca8e5c, 0x985c2a, 0xaaff00, 0x98e900, 0x84d000, 0x6fb700, 0x5ca000, 0x478800,
        0x326e00, 0x1d5500, 0xffffff, 0xd7d7da, 0xafafb4, 0x87878d, 0x5f5f67, 0x373740, 0xfffa3c, 0xffc81e,
        0xff8d00, 0xfc5700, 0xe92a00, 0xc60600, 0x00dcdc, 0x00c1ca, 0x00a0b5, 0x007f9f, 0x005e8a, 0x003d74,
        0x001c5e, 0xffffff, 0xc2c2c5, 0x7f7f84, 0x3b3b43};
    for (int i = 0; i < 256; ++i) {
        CAPTURE(i);
        if (i >= 185 && i <= 229)
            CHECK(w[static_cast<std::size_t>(i)].rgb().rgb() == want[static_cast<std::size_t>(i - 185)]);
        else
            CHECK(w[static_cast<std::size_t>(i)] == before[static_cast<std::size_t>(i)]);
    }
    // b != 12: one call per layer with b + 100, modes 2 and 3, no write (verified claims 17, 18, 22)
    PaletteArray w2 = synth_pal();
    SmoothPalette sp2;
    sp2.working = &w2;
    const SmoothTables t2 = smooth_tables(tab, 100, 100, sp2);
    CHECK(t2.modes == std::vector<int>{2, 3});
    CHECK(t2.groups[0].size() == 1);
    CHECK(t2.groups[0][0].lo == 185);
    CHECK(t2.groups[0][0].hi == 212);
    CHECK(t2.groups[1][0].lo == 213);
    CHECK(t2.groups[1][0].hi == 229);
    for (int k = 0; k < 15; ++k) CHECK(t2.grp[static_cast<std::size_t>(k)] == 0);
    CHECK(w2 == synth_pal());
}

TEST_CASE("smooth: seg5:0x26a2 keeps DGROUP:0x1eaa and DGROUP:0x1eac as manager state") {
    PaletteManager pm;
    const PalHandle pal = pm.create(0);
    CHECK(pm.dg_1eaa() == 0);  // BSS
    CHECK(pm.dg_1eac() == 0);
    // colours 0-9 darker step by step, colours 10-14 not
    std::vector<std::uint32_t> base(16, 0);
    for (int k = 0; k < 10; ++k) base[static_cast<std::size_t>(k)] = static_cast<std::uint32_t>(0x101010 * (15 - k));
    base[10] = 0x202020;
    base[11] = 0x808080;
    base[12] = 0x404040;
    base[13] = 0x303030;
    base[14] = 0x101010;
    std::vector<std::uint32_t> t = base;
    pm.recolour_graphic(pal, 300, t, 0, 12, 12, 0, true);
    CHECK(pm.dg_1eaa() == 1);
    CHECK(pm.dg_1eac() == 0);
    // layer 1's ramp was not written: entries 213-229 keep their black
    for (int i = 213; i <= 229; ++i) CHECK((*pm.working(pal))[static_cast<std::size_t>(i)].black());
    CHECK((*pm.working(pal))[185].rgb().rgb() == 0xf0f0f0u);
    // another load mode leaves them; so does a = 4, b = -1
    t = base;
    pm.recolour_graphic(pal, 300, t, 3, 1, 1, 0, false);
    t = base;
    pm.recolour_graphic(pal, 300, t, 0, 4, -1, 0, true);
    CHECK(pm.dg_1eaa() == 1);
    CHECK(pm.dg_1eac() == 0);
    // a scheme other than 12 forces its flag [BYTES ISPACE.DLL seg5:0x3106-0x3118]
    t = base;
    pm.recolour_graphic(pal, 300, t, 0, 12, 5, 0, true);
    CHECK(pm.dg_1eac() == 1);
    // blend rows: ordinal 12 writes -1 on a kind-0 palette
    pm.set_blend_from(pal, 0, 7);
    CHECK(pm.blend_from(pal, 0) == 7);
}

namespace {

// The 8 x 8 graphic of ord12_test_values.py: top-down nibbles, stored bottom-up at 4 bpp.
is::fmt::Dib hand_graphic() {
    const int img[8][8] = {
        {15, 15, 1, 2, 2, 1, 15, 15},     {15, 0, 3, 11, 11, 3, 0, 15}, {2, 3, 10, 12, 12, 10, 3, 2},
        {4, 11, 12, 14, 13, 12, 11, 4},   {4, 11, 12, 13, 14, 12, 11, 4}, {5, 6, 10, 12, 12, 10, 6, 5},
        {15, 7, 8, 9, 9, 8, 7, 15},       {15, 15, 9, 15, 15, 9, 15, 15},
    };
    is::fmt::Dib d;
    d.width = 8;
    d.height = 8;
    d.bpp = 4;
    d.bottom_up = true;
    for (int k = 0; k < 15; ++k)
        d.palette.push_back(static_cast<std::uint32_t>((0x10 * k) << 16 | (0x08 * k) << 8 | (0xff - 0x10 * k)));
    d.palette.push_back(0);
    d.bits.assign(4 * 8, 0);
    for (int r = 0; r < 8; ++r) {
        const int m = 7 - r;
        for (int x = 0; x < 8; ++x) {
            std::uint8_t& b = d.bits[static_cast<std::size_t>(m * 4 + x / 2)];
            b = static_cast<std::uint8_t>(b | ((x & 1) ? img[r][x] : img[r][x] << 4));
        }
    }
    return d;
}

}  // namespace

TEST_CASE("smooth: a small render through the whole ordinal (expansion, blur, staircase, output)") {
    PaletteManager pm;
    const PalHandle pal = pm.create(0);
    const is::fmt::Dib d = hand_graphic();
    int publishes = 0;
    pm.on_publish([&publishes](int, int) { ++publishes; });
    const SmoothPicture p = smooth_for(d, 60, pm, pal, smooth_args(0, 10));
    REQUIRE(p.ok());
    CHECK(p.h == 8);
    CHECK(p.side == 16);
    CHECK(p.bm_w == 16);
    CHECK(p.bm_h == 16);
    CHECK(p.dst_w == 16);
    CHECK(p.dst_h == 16);
    CHECK(p.fixes == 4);
    CHECK(publishes == 2);  // the load's command 3 and the end's [BYTES ISPACE.DLL seg5:0x1bff seg7:0x4ed2]
    const std::uint8_t want[16][16] = {
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 196, 191, 191, 191, 191, 191, 191, 196, 0, 0, 0, 0},
        {0, 0, 200, 195, 190, 192, 217, 217, 217, 217, 192, 190, 195, 200, 0, 0},
        {0, 0, 195, 190, 192, 193, 218, 218, 218, 218, 193, 192, 190, 195, 0, 0},
        {0, 197, 191, 192, 192, 216, 219, 220, 220, 218, 216, 192, 192, 191, 197, 0},
        {0, 194, 193, 193, 216, 218, 221, 223, 222, 220, 218, 216, 193, 193, 194, 0},
        {0, 196, 217, 218, 219, 221, 225, 226, 225, 223, 220, 218, 218, 217, 196, 0},
        {0, 197, 217, 218, 220, 223, 226, 227, 227, 225, 222, 220, 218, 217, 197, 0},
        {0, 198, 217, 218, 220, 222, 225, 227, 227, 226, 223, 220, 218, 217, 198, 0},
        {0, 198, 217, 218, 218, 220, 223, 225, 226, 225, 221, 219, 218, 217, 198, 0},
        {0, 201, 202, 203, 216, 218, 220, 222, 223, 221, 218, 216, 203, 202, 201, 0},
        {0, 205, 203, 204, 208, 216, 219, 221, 221, 220, 216, 208, 204, 203, 205, 0},
        {0, 0, 208, 206, 208, 210, 212, 212, 212, 212, 210, 208, 206, 208, 0, 0},
        {0, 0, 210, 210, 209, 211, 212, 212, 212, 212, 211, 209, 210, 210, 0, 0},
        {0, 0, 0, 0, 212, 212, 0, 0, 0, 0, 212, 212, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    };
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) {
            CAPTURE(x);
            CAPTURE(y);
            CHECK(p.at(x, y) == want[y][x]);
        }
    CHECK(crc(p.px) == 0x56fa8d63u);
    CHECK(pm.blend_from(pal, 0) == -1);
    CHECK(pm.blend_from(pal, 1) == -1);

    // w3 = 1 adds L sar 2 to the half level
    PaletteManager pm3;
    const PalHandle pal3 = pm3.create(0);
    SmoothArgs a3 = smooth_args(0, 10);
    a3.w3 = 1;
    CHECK(crc(smooth_for(d, 60, pm3, pal3, a3).px) == 0x4bcf456au);

    // b1 = b2 = 12 with colours that are not monotonic: both flags 0, the runs, modes 0 and 1
    PaletteManager pm12;
    const PalHandle pal12 = pm12.create(0);
    const SmoothPicture p12 = smooth_for(d, 60, pm12, pal12, smooth_args(12, 12));
    CHECK(pm12.dg_1eaa() == 0);
    CHECK(pm12.dg_1eac() == 0);
    CHECK(crc(p12.px) == 0x0473ff5cu);
    CHECK(p12.fixes == 4);

    // seg6:0xc4: byte 0xff returns 0; an unknown handle draws nothing [DESIGN]
    CHECK_FALSE(smooth_for(d, 60, pm, pal, smooth_args(0xff, 10)).ok());
    CHECK_FALSE(smooth_for(d, 60, pm, pal, smooth_args(0, -1)).ok());
    CHECK_FALSE(smooth_for(d, 60, pm, 999, smooth_args(0, 10)).ok());
    // the output sizes: w = -1 halves, -4 doubles; w4 != 0 widens by ordinal 2's N + 1
    SmoothArgs a4 = smooth_args(0, 10);
    a4.w1 = -4;
    a4.w2 = -1;
    a4.w4 = 1;
    PaletteManager pm4;
    const PalHandle pal4 = pm4.create(0);
    const SmoothPicture p4 = smooth_for(d, 60, pm4, pal4, a4, 3);
    CHECK(p4.side == 16);
    CHECK(p4.dst_w == 32);
    CHECK(p4.dst_h == 8);
    CHECK(p4.bm_w == 128);
    CHECK(p4.bm_h == 8);
}

// ---- golden

namespace {

std::filesystem::path original_dir() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    return v;
}

// work/ beside IS_ORIGINAL_DIR (work/extracted/game).
std::filesystem::path work_dir() {
    const auto o = original_dir();
    if (o.empty()) return {};
    std::filesystem::path base = o;
    if (!base.has_filename()) base = base.parent_path();
    return base.parent_path().parent_path();
}

std::map<int, int> histogram(const std::vector<std::uint8_t>& px, std::size_t from, std::size_t to) {
    std::map<int, int> h;
    for (std::size_t i = from; i < to; ++i) ++h[px[i]];
    return h;
}

}  // namespace

TEST_CASE("smooth golden: vector A, the title emblem, against probe-0001's title") {
    const auto dir = original_dir();
    if (dir.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    is::assets::AssetStore store(dir);
    const is::fmt::Dib d300 = store.sprite(300, is::assets::kGameDll);
    PaletteManager pm;
    const PalHandle pal = pm.create(0);
    // seg15:0x757c: seg10:0x73a6(300, "", -2, -2, 2, 0, 1, 1, 12) [BYTES seg15:0x7567-0x758c]
    const SmoothPicture p = smooth_for(d300, 300, pm, pal, smooth_args(1, 12));
    REQUIRE(p.ok());
    CHECK(p.side == 276);
    CHECK(pm.dg_1eaa() == 1);  // b1 = 1 forces it; colours 10-14 fall in brightness
    CHECK(pm.dg_1eac() == 1);
    CHECK(p.fixes == 466);
    CHECK(crc(p.px) == 0x6b8677a8u);
    const auto all = histogram(p.px, 0, p.px.size());
    CHECK(all.size() == 44);
    CHECK(all.at(0) == 54338);
    CHECK(all.at(0xf5) == 30);
    CHECK(all.count(185) == 0);
    CHECK(all.count(186) == 0);
    CHECK(all.count(187) == 0);
    // what title step 0 reveals: the top 210 rows
    const auto top = histogram(p.px, 0, 276u * 210u);
    CHECK(top.at(0) == 36122);
    CHECK(top.at(245) == 30);
    CHECK(top.at(188) == 639);
    CHECK(top.at(203) == 1049);
    CHECK(top.at(212) == 1014);
    CHECK(top.at(229) == 1067);
    // the palette the load wrote: entries 185-229 (palette-tables.md "Title palette at 8 bpp")
    const std::array<std::uint32_t, 45> want = {
        0xffe66e, 0xf9df6a, 0xf3d866, 0xecd162, 0xe6ca5e, 0xe0c35a, 0xd8bc56, 0xd2b552, 0xccae4e, 0xc4a74a,
        0xbea046, 0xb89942, 0xb0923e, 0xaa8b3a, 0xa48436, 0x9c7d31, 0x96762d, 0x906f29, 0x886825, 0x826121,
        0x7c5a1d, 0x745319, 0x6e4c15, 0x684511, 0x603e0d, 0x5a3709, 0x543005, 0x4c2800, 0xd7d7d6, 0xd0d0cf,
        0xc9c9c8, 0xc2c2c1, 0xb9baba, 0xb3b3b3, 0xadacac, 0xa7a5a5, 0x9e9e9e, 0x949494, 0x8a8a8a, 0x808080,
        0x737373, 0x6c6c6b, 0x656563, 0x5e5e5b, 0x565552};
    const PaletteArray& w = *pm.working(pal);
    for (int i = 185; i <= 229; ++i) {
        CAPTURE(i);
        CHECK(w[static_cast<std::size_t>(i)].rgb().rgb() == want[static_cast<std::size_t>(i - 185)]);
    }
    CHECK(w[245].black());
    // the oracle: probe-0001's title at (374, 229), 276 x 210 [ORACLE probe-0001]
    const auto png = work_dir() / "probe" / "run" / "probe-title.png";
    if (!std::filesystem::exists(png)) {
        MESSAGE("skipped the picture: work/probe/run/probe-title.png not found");
        return;
    }
    const pngtest::Image o = pngtest::read(png.string());
    REQUIRE(o.w >= 374 + 276);
    int same = 0;
    std::set<std::uint32_t> port_colours, oracle_colours;
    for (int y = 0; y < 210; ++y)
        for (int x = 0; x < 276; ++x) {
            const std::uint32_t pc = w[p.at(x, y)].rgb().rgb(), oc = o.at(374 + x, 229 + y);
            port_colours.insert(pc);
            oracle_colours.insert(oc);
            same += pc == oc;
        }
    MESSAGE("title emblem: " << same << " of 57960 pixels equal, " << port_colours.size() << " colours (oracle "
                             << oracle_colours.size() << ")");
    CHECK(same == 57960);
    CHECK(port_colours.size() == 43);
}

TEST_CASE("smooth golden: vector B, the TRANSFORMER, against core-0003's landing scene") {
    const auto dir = original_dir();
    if (dir.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    is::assets::AssetStore store(dir);
    const is::assets::Ships ships = store.ships(dir);
    const is::fmt::ShipRecord* r = ships.find("TRANSFORMER");
    REQUIRE(r);
    const is::fmt::BodyRecord* b = ships.body(*r);
    REQUIRE(b);
    CHECK(b->bitmap_id == 60);
    const is::fmt::Dib body = store.sprite(b->bitmap_id);
    PaletteManager pm;
    const PalHandle pal = pm.create(0);
    // seg15:0x2b8f / seg15:0x2165: (BOD +0x21, BOD +0x23, -2, -2, 2, 0, 1, SHP +0x07, SHP +0x08)
    const SmoothPicture p = smooth_for(body, b->bitmap_id, pm, pal, smooth_args(r->byte_07, r->byte_08));
    REQUIRE(p.ok());
    CHECK(p.side == 66);
    CHECK(p.fixes == 79);
    CHECK(crc(p.px) == 0x3e518112u);
    const auto all = histogram(p.px, 0, p.px.size());
    CHECK(all.size() == 40);
    CHECK(all.at(0) == 2622);
    CHECK(all.count(0xf5) == 0);
    CHECK(all.count(211) == 0);
    int first = -1;
    for (int y = 0; y < 66 && first < 0; ++y)
        for (int x = 0; x < 66; ++x)
            if (p.at(x, y)) {
                first = y;
                break;
            }
    CHECK(first == 8);
    const std::array<std::uint32_t, 45> want = {
        0xffffff, 0xf7f7f8, 0xefeff1, 0xe6e6e8, 0xdedee0, 0xd6d6d8, 0xcdcdd0, 0xc5c5c8, 0xbdbdc0, 0xb4b4b8,
        0xacacb0, 0xa4a4a8, 0x9b9ba0, 0x939398, 0x8b8b90, 0x828288, 0x7a7a80, 0x727278, 0x696970, 0x616168,
        0x595960, 0x505058, 0x484850, 0x404048, 0x373740, 0x2f2f38, 0x272730, 0x1e1e28, 0xc8ff00, 0xbcf400,
        0xb0e900, 0xa4de00, 0x96d200, 0x8ac700, 0x7ebc00, 0x72b100, 0x64a500, 0x589a00, 0x4c8f00, 0x408400,
        0x327800, 0x266d00, 0x1a6200, 0x0e5700, 0x004b00};
    const PaletteArray& w = *pm.working(pal);
    for (int i = 185; i <= 229; ++i) {
        CAPTURE(i);
        CHECK(w[static_cast<std::size_t>(i)].rgb().rgb() == want[static_cast<std::size_t>(i - 185)]);
    }
    // the oracle: frame 0 of the pad ship doubled at (446, 576) [ORACLE core-0003]
    const auto png = work_dir() / "oracle" / "core-0003" / "picture" / "landing-01-scene.png";
    if (!std::filesystem::exists(png)) {
        MESSAGE("skipped the picture: work/oracle/core-0003/picture/landing-01-scene.png not found");
        return;
    }
    const pngtest::Image o = pngtest::read(png.string());
    REQUIRE(o.w >= 446 + 132);
    int same = 0, lit = 0;
    for (int y = 0; y < 66; ++y)
        for (int x = 0; x < 66; ++x) {
            const std::uint8_t v = p.at(x, y);
            if (v == 0) continue;
            const std::uint32_t pc = w[v].rgb().rgb();
            for (int dy = 0; dy < 2; ++dy)
                for (int dx = 0; dx < 2; ++dx) {
                    ++lit;
                    same += pc == o.at(446 + 2 * x + dx, 576 + 2 * y + dy);
                }
        }
    MESSAGE("TRANSFORMER on the pad: " << same << " of " << lit << " screen pixels equal");
    CHECK(lit == 6936);
    CHECK(same == 6936);
}

TEST_CASE("smooth golden: seg10:0x73a6 on the program's palette (Gfx), title and pad ship") {
    const auto dir = original_dir();
    if (dir.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    is::assets::AssetStore store(dir);
    {
        // seg15:0x757c: graphic 300 from ISPACE.DLL by seg5:0x1b8e's module rule
        Gfx g(&store);
        g.palette_create();
        const SmoothPicture p = smooth_for(g, 300, smooth_args(1, 12));
        REQUIRE(p.ok());
        CHECK(p.side == 276);
        CHECK(crc(p.px) == 0x6b8677a8u);
        CHECK((*g.palettes().working(g.palette()))[185].rgb().rgb() == 0xffe66eu);
    }
    {
        // seg15:0x2b8f: the TRANSFORMER, BOD +0x21 = 60 from IGRAPHIC.DLL, SHP +0x07 = 0, +0x08 = 10
        Gfx g(&store);
        g.palette_create();
        const SmoothPicture p = smooth_for(g, 60, smooth_args(0, 10));
        REQUIRE(p.ok());
        CHECK(p.side == 66);
        CHECK(crc(p.px) == 0x3e518112u);
        // seg6:0xc4's 0xff test comes before the load; no palette, no picture [DESIGN]
        CHECK_FALSE(smooth_for(g, 60, smooth_args(0xff, 10)).ok());
        Gfx none(&store);
        CHECK_FALSE(smooth_for(none, 60, smooth_args(0, 10)).ok());
    }
}

TEST_CASE("smooth golden: the scheme-12 designs that take seg7:0x4ee8 modes 0 and 1") {
    // No oracle picture covers modes 0 and 1 (s6-ord12.md section 6, [OPEN]); the CRC-32 values are
    // ord12_model_runs.py's, so this checks the port against the model of the analysis.
    const auto dir = original_dir();
    if (dir.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    is::assets::AssetStore store(dir);
    const is::assets::Ships ships = store.ships(dir);
    struct Want {
        const char* name;
        int bitmap;
        std::vector<int> modes;
        std::uint32_t crc;
    };
    const std::vector<Want> wants = {
        {"KILLER WHALE", 600, {2, 1, 1}, 0x0a1e794eu},  {"REBEL", 177, {0, 0, 3}, 0xfec937dau},
        {"BALD EAGLE", 608, {2, 1, 1, 1}, 0x000d8919u}, {"SPECTRAL FORCE", 178, {0, 0, 0, 0, 3}, 0x3e528687u},
        {"FRUIT BAT", 601, {2, 1, 1, 1}, 0x6f199d5bu},
    };
    for (const Want& wt : wants) {
        CAPTURE(wt.name);
        const is::fmt::ShipRecord* r = ships.find(wt.name);
        REQUIRE(r);
        const is::fmt::BodyRecord* b = ships.body(*r);
        REQUIRE(b);
        CHECK(b->bitmap_id == wt.bitmap);
        const is::fmt::Dib body = store.sprite(b->bitmap_id);
        PaletteManager pm;
        const PalHandle pal = pm.create(0);
        int modes01 = 0;
        pm.on_publish([&modes01](int, int) { ++modes01; });
        const SmoothPicture p = smooth_for(body, b->bitmap_id, pm, pal, smooth_args(r->byte_07, r->byte_08));
        REQUIRE(p.ok());
        int expect = 2;  // the load and the end
        for (int m : wt.modes) expect += m < 2 ? 1 : 0;
        CHECK(modes01 == expect);
        CHECK(crc(p.px) == wt.crc);
    }
}

#ifdef _WIN32
TEST_CASE("smooth win32: the bitmap seg7:0x12d8 returns, through StretchBlt on index images") {
    Gfx g(nullptr);
    g.palette_create();
    const is::fmt::Dib d = hand_graphic();
    const SmoothPicture p = smooth_for(d, 60, g.palettes(), g.palette(), smooth_args(0, 10));
    REQUIRE(p.ok());
    const ImagePtr img = smooth_image(g, p);
    REQUIRE(img);
    CHECK(img->width() == 16);
    CHECK(img->height() == 16);
    int same = 0;
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) same += img->at(x, y) == p.at(x, y);
    CHECK(same == 256);  // w = -2: a 1:1 copy of the indices
    // w1 = -4: the bitmap twice as wide, every column doubled by COLORONCOLOR
    SmoothArgs a = smooth_args(0, 10);
    a.w1 = -4;
    const SmoothPicture p2 = smooth_for(d, 60, g.palettes(), g.palette(), a);
    const ImagePtr img2 = smooth_image(g, p2);
    REQUIRE(img2);
    CHECK(img2->width() == 32);
    CHECK(img2->height() == 16);
    int same2 = 0;
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 32; ++x) same2 += img2->at(x, y) == p2.at(x / 2, y);
    CHECK(same2 == 512);
    // an empty picture gives no bitmap
    CHECK_FALSE(smooth_image(g, SmoothPicture{}));
    // smooth_bitmap: the whole seg10:0x73a6 call; no asset store throws as Gfx::graphic_dib does
    CHECK_THROWS(smooth_bitmap(g, 60, smooth_args(0, 10)));
    CHECK_FALSE(smooth_bitmap(g, 60, smooth_args(0, 0xff)));  // seg6:0xc4 returns 0 first
}

TEST_CASE("smooth win32 golden: smooth_bitmap of the TRANSFORMER, the pad ship's 66 x 66 bitmap") {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    is::assets::AssetStore store(v);
    Gfx g(&store);
    g.palette_create();
    const ImagePtr img = smooth_bitmap(g, 60, smooth_args(0, 10));
    REQUIRE(img);
    CHECK(img->width() == 66);
    CHECK(img->height() == 66);
    std::vector<std::uint8_t> px;
    for (int y = 0; y < 66; ++y)
        for (int x = 0; x < 66; ++x) px.push_back(img->at(x, y));
    CHECK(crc(px) == 0x3e518112u);
}
#endif
