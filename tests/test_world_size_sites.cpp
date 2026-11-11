// The Size sites (work/analysis/m4/s8-size.md P3 and P8, M4 stage 8, track SIZE-SITES): every
// builder that calls seg10:0x6308 (s8-size.md 2.4) rescales its image at Size 0 and 2, the
// sheets under seg10:0x6ee6's test, the port's copy of a halved sheet as GDI halves it, the
// bitmap form for OUT_SIGN and graphics 230 and 360, the briefing strip, and the visit's latched
// Size DGROUP:0x959e. The goldens read IS_ORIGINAL_DIR and work/oracle/core-0011 beside it and
// skip without them: the original's Size 0 cell is its Size 1 cell's pixels (2c, 2r), and the
// ship boxes of core-0011 [ORACLE core-0011]. On Win32 the GDI Blitter's StretchBlt under Wine is
// held to gdi_stretch_map (ruling R4, ADR 0017's approach).
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "assets/asset_store.h"
#include "doctest.h"
#include "gfx/palette.h"
#include "gfx/ship_quick.h"
#include "gfx/ship_sheet.h"
#include "hud/icon_images.h"
#include "png_decode.h"
#include "shell/rng.h"
#include "world/ambulance.h"
#include "world/death.h"
#include "world/debris.h"
#include "world/design_words.h"
#include "world/effects.h"
#include "world/entry_effect.h"
#include "world/icon_mover.h"
#include "world/player_sheet.h"
#include "world/pods.h"
#include "world/rescale.h"
#include "world/rock_images.h"
#include "world/smoke.h"
#include "world/tank.h"
#include "world/visit.h"
#include "world/world.h"

#ifdef _WIN32
#include <windows.h>

#include "gfx/gfx.h"
#include "gfx/surface.h"
#include "gfx/wing_cut.h"
#endif

using namespace is::world;

namespace {

// A RecordingBlitter whose loaded graphics and bitmaps have the sizes of the game's resources
// (IGRAPHIC.DLL #4000 headers; OUT_SIGN two 26 x 13 frames), so seg10:0x6308 can measure them.
class SizedBlitter : public RecordingBlitter {
public:
    std::map<int, std::pair<int, int>> graphic = {
        {170, {248, 52}}, {175, {196, 46}}, {200, {60, 90}}, {230, {160, 32}}, {360, {64, 32}},
        {690, {23, 23}},  {691, {23, 23}},  {692, {23, 23}}, {270, {180, 36}}, {50, {774, 43}},
        {210, {34, 34}},  {820, {68, 34}}};
    std::map<std::string, std::pair<int, int>> bitmap = {
        {"OUT_SIGN", {52, 13}}, {"ROCKS", {774, 43}}, {"BW_BLOCKS", {34, 34}}, {"BW_MBLOCKS", {68, 34}}};
    DcId load_graphic(int id, int mode, int arg) override {
        const DcId d = RecordingBlitter::load_graphic(id, mode, arg);
        if (auto it = graphic.find(id); it != graphic.end()) set_image_size(d, it->second.first, it->second.second);
        return d;
    }
    DcId load_bitmap(const std::string& name) override {
        const DcId d = RecordingBlitter::load_bitmap(name);
        if (auto it = bitmap.find(name); it != bitmap.end()) set_image_size(d, it->second.first, it->second.second);
        return d;
    }
    std::pair<int, int> size_of(DcId id) const {
        int w = 0, h = 0;
        if (!image_size(id, w, h)) return {-1, -1};
        return {w, h};
    }
    const Image* find(DcId id) const {
        for (auto it = images.rbegin(); it != images.rend(); ++it)
            if (it->id == id) return &*it;
        return nullptr;
    }
    bool was_deleted(DcId id) const {
        for (DcId d : deleted)
            if (d == id) return true;
        return false;
    }
    // The StretchBlt calls that wrote into `dst`.
    std::vector<BlitCall> stretches_into(DcId dst) const {
        std::vector<BlitCall> v;
        for (const BlitCall& c : calls)
            if (c.op == BlitCall::kStretchBlt && c.dst == dst) v.push_back(c);
        return v;
    }
    // The graphic id behind a mask image (create_mask records the source id in `graphic`).
    DcId mask_source(DcId mask) const {
        const Image* m = find(mask);
        return m && m->name == "mask" ? static_cast<DcId>(m->graphic) : DcId{0};
    }
};

using WH = std::pair<int, int>;

// A sheet renderer of a w x h picture whose pixel (x, y) is 1 + (x + 3y) % 250, source height 33.
SheetRenderer pattern_render(int w, int h) {
    return [w, h](const SheetRequest&, SheetPixels& px) {
        px.w = w;
        px.h = h;
        px.index.resize(static_cast<std::size_t>(w) * h);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                px.index[static_cast<std::size_t>(y) * w + x] = static_cast<std::uint8_t>(1 + (x + 3 * y) % 250);
        px.source_height = 33;
        return true;
    };
}

}  // namespace

// ---- the copy of a halved or doubled picture (ruling R4)

TEST_CASE("size sites: gdi_stretch_map halves to the second column and the first row of each pair") {
    // 2:1 on an even extent: column d takes 2d + 1, row d takes 2d.
    CHECK(is::gfx::gdi_stretch_map(8, 4, false) == std::vector<int>{1, 3, 5, 7});
    CHECK(is::gfx::gdi_stretch_map(8, 4, true) == std::vector<int>{0, 2, 4, 6});
    // The TRANSFORMER's sheet, 651 x 157 to 325 x 78: the run of three falls in the middle, at
    // column 162 and row 38, so the Size 0 cell of heading k <= 8 at 19k + 2 starts at the 1x
    // cell's first column 38k + 5 and those of k >= 9 one column later; rows 2 + r of row 0's
    // cells are the 1x rows 4 + 2r, the cell's row 2r - 1 (its row 0 the gap's) [ORACLE core-0011].
    const std::vector<int> m = is::gfx::gdi_stretch_map(651, 325, false), n = is::gfx::gdi_stretch_map(157, 78, true);
    REQUIRE(m.size() == 325u);
    REQUIRE(n.size() == 78u);
    bool all = true;
    for (int d = 0; d < 325; ++d) all = all && m[static_cast<std::size_t>(d)] == 2 * d + 1 + (d >= 162 ? 1 : 0);
    for (int d = 0; d < 78; ++d) all = all && n[static_cast<std::size_t>(d)] == 2 * d + (d > 38 ? 1 : 0);
    CHECK(all);
    CHECK(m[162] == 326);
    CHECK(n[38] == 76);
    for (int k = 0; k < 16; ++k) CHECK(m[static_cast<std::size_t>(19 * k + 2)] == 38 * k + 5 + (k >= 9 ? 1 : 0));
    CHECK(n[2] == 4);
    // Odd extents as Wine's GDI halves them (the Win32 case below holds them in two dimensions).
    CHECK(is::gfx::gdi_stretch_map(13, 6, false) == std::vector<int>{1, 3, 6, 8, 10, 12});
    CHECK(is::gfx::gdi_stretch_map(13, 6, true) == std::vector<int>{0, 2, 4, 7, 9, 11});
    CHECK(is::gfx::gdi_stretch_map(7, 3, false) == std::vector<int>{1, 4, 6});
    CHECK(is::gfx::gdi_stretch_map(7, 3, true) == std::vector<int>{0, 2, 5});
    CHECK(is::gfx::gdi_stretch_map(5, 2, false) == std::vector<int>{2, 4});
    CHECK(is::gfx::gdi_stretch_map(5, 2, true) == std::vector<int>{0, 3});
    CHECK(is::gfx::gdi_stretch_map(31, 15, false) ==
          std::vector<int>{1, 3, 5, 7, 9, 11, 13, 16, 18, 20, 22, 24, 26, 28, 30});
    CHECK(is::gfx::gdi_stretch_map(3, 1, false) == std::vector<int>{1});
    CHECK(is::gfx::gdi_stretch_map(2, 1, false) == std::vector<int>{0});
    CHECK(is::gfx::gdi_stretch_map(3, 1, true) == std::vector<int>{0});
    // 1:2 repeats each pixel, on both axes.
    CHECK(is::gfx::gdi_stretch_map(3, 6, false) == std::vector<int>{0, 0, 1, 1, 2, 2});
    CHECK(is::gfx::gdi_stretch_map(3, 6, true) == std::vector<int>{0, 0, 1, 1, 2, 2});
    // Same size is the identity; empty extents give an empty or zero map.
    CHECK(is::gfx::gdi_stretch_map(4, 4, true) == std::vector<int>{0, 1, 2, 3});
    CHECK(is::gfx::gdi_stretch_map(0, 3, false) == std::vector<int>{0, 0, 0});
    CHECK(is::gfx::gdi_stretch_map(5, 0, false).empty());
}

TEST_CASE("size sites: rescaled_sheet_pixels at Size 0, 1 and 2") {
    SheetPixels px;
    px.w = 4;
    px.h = 3;
    px.source_height = 9;
    px.index = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    const SheetPixels s1 = rescaled_sheet_pixels(px, 1);
    CHECK(s1.w == 4);
    CHECK(s1.index == px.index);
    const SheetPixels s0 = rescaled_sheet_pixels(px, 0);  // 2 x 1 from row 0, columns 1 and 3
    CHECK(s0.w == 2);
    CHECK(s0.h == 1);
    CHECK(s0.source_height == 9);
    CHECK(s0.index == std::vector<std::uint8_t>{2, 4});
    const SheetPixels s2 = rescaled_sheet_pixels(px, 2);
    CHECK(s2.w == 8);
    CHECK(s2.h == 6);
    CHECK(s2.index[0] == 1);
    CHECK(s2.index[1] == 1);
    CHECK(s2.index[2] == 2);
    CHECK(s2.index[static_cast<std::size_t>(5 * 8 + 7)] == 12);
    // A 1-pixel extent halves to 0: a 1 x 1 black picture, as seg10:0x6308's.
    SheetPixels one;
    one.w = 1;
    one.h = 4;
    one.index.assign(4, 9);
    const SheetPixels o = rescaled_sheet_pixels(one, 0);
    CHECK(o.w == 1);
    CHECK(o.h == 1);
    CHECK(o.index == std::vector<std::uint8_t>{0});
}

TEST_CASE("size sites: seg10:0x7032's flame bitmap rescaled before the headings") {
    is::gfx::FlameBitmap b;
    b.width = 45;
    b.height = 18;
    b.cell = 9;
    b.dirs = 5;
    b.px.resize(45u * 18u);
    for (int y = 0; y < 18; ++y)
        for (int x = 0; x < 45; ++x) b.px[static_cast<std::size_t>(y) * 45 + x] = static_cast<std::uint8_t>(1 + x + 45 * (y % 2));
    const is::gfx::FlameBitmap s0 = is::gfx::rescaled_flame_bitmap(b, 0);
    CHECK(s0.width == 22);
    CHECK(s0.height == 9);
    CHECK(s0.cell == is::gfx::flame_cell(0));  // f = 4
    CHECK(s0.dirs == 5);
    const std::vector<int> mx = is::gfx::gdi_stretch_map(45, 22, false), my = is::gfx::gdi_stretch_map(18, 9, true);
    for (int y = 0; y < 9; ++y)
        for (int x = 0; x < 22; ++x) CHECK(s0.at(x, y) == b.at(mx[static_cast<std::size_t>(x)], my[static_cast<std::size_t>(y)]));
    const is::gfx::FlameBitmap s2 = is::gfx::rescaled_flame_bitmap(b, 2);
    CHECK(s2.width == 90);
    CHECK(s2.height == 36);
    CHECK(s2.cell == is::gfx::flame_cell(2));  // f = 18
    CHECK(s2.at(89, 35) == b.at(44, 17));
    CHECK(is::gfx::rescaled_flame_bitmap(b, 1).px == b.px);
    // The headings at f from the rescaled cells take them unstretched.
    const is::gfx::FlameHeadings h = is::gfx::flame_headings(s0, 4);
    REQUIRE(h.ok());
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) CHECK(h.at(4 + x, y) == s0.at(4 + x, y));  // heading 1, row 0
}

// ---- seg10:0x6ee6's rescale

TEST_CASE("size sites: seg10:0x6ee6's test (gap word, DGROUP:0x9130, Size 2 with sel 0)") {
    CHECK(sheet_rescales(0, 99, 0));
    CHECK(sheet_rescales(1, 99, 0));  // seg10:0x6308 then returns at once
    CHECK_FALSE(sheet_rescales(2, 99, 0));  // the scale flag rendered it 2x instead
    CHECK(sheet_rescales(2, 99, 1));  // the quick renderer at Size 2: doubled
    CHECK_FALSE(sheet_rescales(0, 100, 0));  // w2 >= 100 clears [bp-0x14]
    CHECK_FALSE(sheet_rescales(0, 99, 0, 1));  // seg18 mode
}

TEST_CASE("size sites: the ship sheet at Size 0 is halved, the copy and bm_height with it") {
    reset_design_words();
    clear_sheet_copies();
    SizedBlitter out;
    ImageSlotTable images{};
    ShipRecord ship{};
    ship.roster_index = -1;
    ship.design_index = 0;
    SheetRequest req;
    req.slot = kShipImageSlotBase;
    req.graphic_id = 60;
    req.size_level = 0;
    req.sel = 0;
    req.scale = sheet_scale_flag(0, 0, 0);
    CHECK(req.scale == 0);
    // The TRANSFORMER's 651 x 157 (side 33, gap 5, mode 1).
    const SheetBuild b = build_ship_sheet(out, images, ship, 0, req, pattern_render(651, 157));
    REQUIRE(b.rendered);
    CHECK(out.size_of(images[kShipImageSlotBase]) == WH{325, 78});
    CHECK(b.dc == images[kShipImageSlotBase]);
    CHECK(b.bm_height == 78);
    CHECK(b.side == 33);  // ordinal 5's raw side stays (the object's size step halves it)
    CHECK(b.pixels.w == 325);
    CHECK(b.pixels.h == 78);
    REQUIRE(sheet_copy(0));
    CHECK(sheet_copy(0)->w == 325);
    // The copy is the render halved on gdi_stretch_map: (19k + 2 + c, 2 + r) from (38k + 5 + 2c,
    // 4 + 2r) for the cells left of the middle run.
    const SheetPixels full = [] {
        SheetPixels p;
        pattern_render(651, 157)(SheetRequest{}, p);
        return p;
    }();
    const std::vector<int> mx = is::gfx::gdi_stretch_map(651, 325, false), my = is::gfx::gdi_stretch_map(157, 78, true);
    for (int k : {0, 6, 15})
        for (int c : {0, 3, 16}) {
            const int x = 19 * k + 2 + c, y = 2 + 4;
            CHECK(sheet_copy(0)->index[static_cast<std::size_t>(y) * 325 + x] ==
                  full.index[static_cast<std::size_t>(my[static_cast<std::size_t>(y)]) * 651 + mx[static_cast<std::size_t>(x)]]);
            if (k == 0) CHECK(mx[static_cast<std::size_t>(x)] == 5 + 2 * c);
        }
    CHECK(my[6] == 12);
    // The slot's image went through one StretchBlt of 651 x 157 to 325 x 78.
    const auto st = out.stretches_into(images[kShipImageSlotBase]);
    REQUIRE(st.size() == 1u);
    CHECK(st[0].w == 325);
    CHECK(st[0].h == 78);
    CHECK(st[0].sw == 651);
    CHECK(st[0].sh == 157);
    // seg14:0x0's +0x5b with the halved object words 17 / 2: (78 - 2) / 19 - 1 = 3.
    CHECK(sheet_last_row(b.bm_height, 17, 2) == 3);
    clear_sheet_copies();
}

TEST_CASE("size sites: the ship sheet at Size 2 is rendered 2x with sel 0 and doubled with sel 1") {
    clear_sheet_copies();
    SizedBlitter out;
    ImageSlotTable images{};
    ShipRecord ship{};
    ship.roster_index = -1;
    SheetRequest req;
    req.slot = kShipImageSlotBase;
    req.graphic_id = 60;
    req.size_level = 2;
    req.sel = 0;
    // sel 0: the 2x render (1302 x 314 with the scale flag) stays as rendered: core-0011's fresh
    // render, not a doubling [ORACLE core-0011].
    const SheetBuild b = build_ship_sheet(out, images, ship, 0, req, pattern_render(1302, 314));
    REQUIRE(b.rendered);
    CHECK(out.size_of(images[kShipImageSlotBase]) == WH{1302, 314});
    CHECK(out.stretches_into(images[kShipImageSlotBase]).empty());
    CHECK(b.bm_height == 314);
    // sel 1 (INI Quick): the 1x render doubled by seg10:0x6308.
    set_byte_at(ship, 0xac, 0);
    req.sel = 1;
    const SheetBuild q = build_ship_sheet(out, images, ship, 0, req, pattern_render(651, 157));
    REQUIRE(q.rendered);
    CHECK(out.size_of(images[kShipImageSlotBase]) == WH{1302, 314});
    CHECK(q.pixels.w == 1302);
    CHECK(q.bm_height == 314);
    clear_sheet_copies();
}

TEST_CASE("size sites: seg10:0x533a's weapon sheets at Size 0, the animation-4 sheet too") {
    SizedBlitter out;
    ImageSlotTable images{};
    Pools pools;
    WeaponWords words;
    words.set_word(10, 0x0a, 1);
    words.set_word(10, 0x06, 4);
    words.set_word(10, -2, 150);
    words.set_word(10, 0x26, 6);
    words.set_word(8, 0x0a, 1);
    words.set_word(8, -2, 370);
    DcId anim = kDcNone;
    const Anim4Renderer anim4 = [&](int k, int, int, int) {
        anim = out.create_image(138, 30);  // the animation-4 sheet seg10:0x4412 made
        images[static_cast<std::size_t>(k)] = anim;
        return anim;
    };
    REQUIRE(load_weapon_sheet(out, images, pools, words, 10, 0, 0, pattern_render(23, 23), anim4));
    // seg10:0x4412's tail rescales it whatever sel is.
    CHECK(images[0x30] != anim);
    CHECK(out.size_of(images[0x30]) == WH{69, 15});
    // The ship renderer's weapon sheet, sel 0 at Size 0: halved.
    REQUIRE(load_weapon_sheet(out, images, pools, words, 8, 0, 0, pattern_render(23, 23), anim4));
    CHECK(out.size_of(images[0x31]) == WH{11, 11});
    // At Size 2 with sel 0 the scale flag rendered it; nothing more.
    ImageSlotTable fresh{};
    words.set_word(8, -4, 0);
    REQUIRE(load_weapon_sheet(out, fresh, pools, words, 8, 0, 2, pattern_render(46, 46), anim4));
    CHECK(out.size_of(fresh[0x30]) == WH{46, 46});
    CHECK(out.stretches_into(fresh[0x30]).empty());
}

// ---- the wave's image slots (seg9:0x22b4's chain and the builders of 2.4)

TEST_CASE("size sites: the rocks seg10:0x3a2e at Size 0, 1 and 2 (slots 6 ... 9, DGROUP:0x740)") {
    for (int size : {0, 1, 2}) {
        CAPTURE(size);
        SizedBlitter out;
        ImageSlotTable images{};
        std::int16_t f740 = 7;
        build_rock_images(out, images, f740, size);
        const auto s = [&](int k) { return out.size_of(images[static_cast<std::size_t>(k)]); };
        if (size == 1) {
            CHECK(s(6) == WH{869, 53});
            CHECK(s(7) == WH{869, 53});
            CHECK(s(8) == WH{473, 31});
            CHECK(s(9) == WH{473, 31});
            CHECK(f740 == 0);
        } else if (size == 0) {
            CHECK(s(6) == WH{434, 26});
            CHECK(s(7) == WH{434, 26});
            CHECK(s(8) == WH{236, 15});
            CHECK(s(9) == WH{236, 15});
            CHECK(f740 == 0);
            // WHITEONBLACK for the masks 6 and 8, COLORONCOLOR for 7 and 9.
            for (int k : {6, 7, 8, 9}) {
                int mode = -1;
                for (const BlitCall& c : out.calls)
                    if (c.op == BlitCall::kStretchMode && c.dst == images[static_cast<std::size_t>(k)])
                        mode = static_cast<int>(c.rop);
                CHECK(mode == ((k == 6 || k == 8) ? 2 : 3));
            }
        } else {
            // Slots 7 and 6 doubled; 9 and 8 hold their unscaled images and DGROUP:0x740 = 1.
            CHECK(s(6) == WH{1738, 106});
            CHECK(s(7) == WH{1738, 106});
            CHECK(s(8) == WH{869, 53});
            CHECK(s(9) == WH{869, 53});
            CHECK(f740 == 1);
            CHECK_FALSE(out.was_deleted(images[8]));
            CHECK_FALSE(out.was_deleted(images[9]));
        }
    }
}

TEST_CASE("size sites: the gate images seg10:0x3ea2, OUT_SIGN and refresh_wave_graphics") {
    for (int size : {0, 1, 2}) {
        CAPTURE(size);
        SizedBlitter out;
        ImageSlotTable images{};
        WaveGraphics wg;
        build_gate_images(out, images, wg, size);
        const WH gate = size == 0 ? WH{83, 23} : size == 1 ? WH{166, 46} : WH{332, 92};
        CHECK(out.size_of(images[kSlotGate]) == gate);
        CHECK(out.size_of(images[kSlotGateMask]) == gate);
        CHECK(wg.gate_dc == images[kSlotGate]);
        // The flash's OUT_SIGN: frames of sprite_side(26) x sprite_side(13).
        const WH sign = size == 0 ? WH{26, 6} : size == 1 ? WH{52, 13} : WH{104, 26};
        CHECK(out.size_of(wg.out_sign) == sign);
        CHECK(wg.out_sign_level == size);
        // The frame-1 blits onto slot 1 came from the unscaled OUT_SIGN, before the rescale.
        int signs = 0;
        for (const BlitCall& c : out.calls)
            if (c.op == BlitCall::kBitBlt && c.w == kOutSignW && c.h == kOutSignH && c.sx == kOutSignW) ++signs;
        CHECK(signs == 2);
        // A later Size: the copy is made again at the new Size; the same Size keeps it.
        const DcId before = wg.out_sign;
        refresh_wave_graphics(out, images, wg, size);
        CHECK(wg.out_sign == before);
        const int next = size == 2 ? 1 : size + 1;
        refresh_wave_graphics(out, images, wg, next);
        CHECK(wg.out_sign != before);
        CHECK(out.was_deleted(before));
        CHECK(wg.out_sign_level == next);
        CHECK(out.size_of(wg.out_sign) == (next == 1 ? WH{52, 13} : WH{104, 26}));
    }
}

TEST_CASE("size sites: effects, smoke, pods and the tank at Size 0 and 2") {
    for (int size : {0, 2}) {
        CAPTURE(size);
        SizedBlitter out;
        ImageSlotTable images{};
        build_effect_images(out, images, 1, size);
        CHECK(out.size_of(images[kSlotExplosion]) == (size == 0 ? WH{124, 26} : WH{496, 104}));
        CHECK(out.size_of(images[kSlotSnowball]) == (size == 0 ? WH{98, 23} : WH{392, 92}));
        build_smoke_image(out, images, size);
        CHECK(out.size_of(images[kSlotSmoke]) == (size == 0 ? WH{135, 11} : WH{540, 44}));
        build_pod_image(out, images, size);
        CHECK(out.size_of(images[kSlotPod]) == (size == 0 ? WH{44, 16} : WH{178, 66}));
        std::int16_t made_for = -1;
        build_tank_image(out, images, 0, made_for, size);
        CHECK(out.size_of(images[kSlotTank]) == (size == 0 ? WH{108, 24} : WH{432, 96}));
        // The whole chain through build_wave_images.
        SizedBlitter o2;
        ImageSlotTable im2{};
        std::int16_t f740 = 0;
        wave_graphics() = WaveGraphics{};
        build_wave_images(o2, im2, f740, 1, size);
        CHECK(o2.size_of(im2[kSlotExplosion]) == (size == 0 ? WH{124, 26} : WH{496, 104}));
        CHECK(o2.size_of(im2[kSlotSmoke]) == (size == 0 ? WH{135, 11} : WH{540, 44}));
        CHECK(o2.size_of(im2[kSlotRockBig]) == (size == 0 ? WH{434, 26} : WH{1738, 106}));
        CHECK(o2.size_of(im2[kSlotGate]) == (size == 0 ? WH{83, 23} : WH{332, 92}));
        CHECK(f740 == (size == 2 ? 1 : 0));
        wave_graphics() = WaveGraphics{};
    }
    // Size 1: no StretchBlt into any of them.
    SizedBlitter out;
    ImageSlotTable images{};
    build_effect_images(out, images, 1, 1);
    build_smoke_image(out, images, 1);
    build_pod_image(out, images, 1);
    std::int16_t made_for = -1;
    build_tank_image(out, images, 0, made_for, 1);
    CHECK(out.size_of(images[kSlotExplosion]) == WH{248, 52});
    CHECK(out.size_of(images[kSlotSmoke]) == WH{270, 22});
    CHECK(out.size_of(images[kSlotPod]) == WH{89, 33});
    CHECK(out.size_of(images[kSlotTank]) == WH{216, 48});
}

TEST_CASE("size sites: rescale_bitmap, the bitmap form") {
    SizedBlitter out;
    const DcId g = out.load_graphic(230, 0, -1);
    CHECK(rescale_bitmap(out, g, 1) == g);
    const DcId h = rescale_bitmap(out, g, 0);
    CHECK(h != g);
    CHECK(out.was_deleted(g));
    CHECK(out.size_of(h) == WH{80, 16});
    CHECK(rescale_bitmap(out, kDcNone, 0) == kDcNone);
}

TEST_CASE("size sites: graphic 230 in the debris strip seg13:0x447a and the burnt sheet seg13:0x4710") {
    {
        SizedBlitter out;
        DebrisTable table{};
        SmokeRecords smoke{};
        std::int16_t dg_9456 = 0;
        Pools pools;
        is::shell::Rng rng;
        DebrisCtx c{table, smoke, dg_9456, pools, rng, 1, 0, 0, 0, 3, nullptr, {}, nullptr, {}};
        c.size = 0;
        c.out = &out;
        DebrisEntry& e = table[0];
        e.counter = 1;
        e.end = 100;
        e.dc = out.create_image(80, 16);
        e.mask = out.create_image(80, 16);
        move_debris(c);
        // The 230 loaded, halved to 80 x 16, and the mask made from the halved image.
        DcId halved = kDcNone;
        for (const auto& im : out.images)
            if (im.w == 80 && im.h == 16 && im.id != e.dc && im.id != e.mask) halved = im.id;
        REQUIRE(halved != kDcNone);
        bool masked = false;
        for (const auto& im : out.images)
            if (im.name == "mask" && static_cast<DcId>(im.graphic) == halved) masked = true;
        CHECK(masked);
        int from = 0;
        for (const BlitCall& b : out.calls)
            if (b.op == BlitCall::kBitBlt && b.src == halved) {
                ++from;
                CHECK(b.w == 80);  // 5p, p = 16 at Size 0
                CHECK(b.h == 16);
            }
        CHECK(from == 1);
    }
    {
        World w;
        EffectState st;
        EffectWords in;
        EffectHost host;
        is::shell::Rng rng;
        SizedBlitter out;
        ImageSlotTable images{};
        EffectCtx c{w, st, in, host, rng, &out, &images, nullptr, nullptr, nullptr};
        w.size = 2;
        ObjectRecord& o = w.pools.rec(w.roles.ships, 0);
        o.size = 66;
        o.gap = 10;
        images[kShipImageSlotBase] = out.create_image(1302, 314);
        burn_ship_sheet(c, 0);
        DcId doubled = kDcNone;
        for (const auto& im : out.images)
            if (im.w == 320 && im.h == 64) doubled = im.id;
        REQUIRE(doubled != kDcNone);
        bool masked = false;
        for (const auto& im : out.images)
            if (im.name == "mask" && static_cast<DcId>(im.graphic) == doubled) masked = true;
        CHECK(masked);
        // Cell h % 5 of side p = 64 from x = 64·(h % 5).
        int paints = 0;
        for (const BlitCall& b : out.calls)
            if (b.op == BlitCall::kBitBlt && b.src == doubled) {
                ++paints;
                CHECK(b.w == 64);
                CHECK(b.sx % 64 == 0);
            }
        CHECK(paints == 16);
    }
}

TEST_CASE("size sites: the icon images, graphics 230 and 360 and the briefing strip (hud/icon_images.h)") {
    SizedBlitter out;
    ImageSlotTable images{};
    const DcId strip = out.create_image(38 * 8 + 6, 44);
    is::hud::IconImages ii(out, images);
    ii.strip = [&images, strip] { return images[kIconStripSlot] != kDcNone ? images[kIconStripSlot] : strip; };
    DcId rescaled = kDcNone;
    ii.strip_rescaled = [&rescaled](DcId d) { rescaled = d; };
    // seg9:0x5dae at Size 2: 310 x 44 to 620 x 88, before the overlays.
    ii.strip_rescale(2);
    CHECK(images[kIconStripSlot] != strip);
    CHECK(rescaled == images[kIconStripSlot]);
    CHECK(out.size_of(images[kIconStripSlot]) == WH{620, 88});
    // Size 1 leaves it.
    const DcId s2 = images[kIconStripSlot];
    ii.strip_rescale(1);
    CHECK(images[kIconStripSlot] == s2);
    // seg7:0x2512's overlay at Size 2: 360 doubled to 128 x 64, cells of 64.
    IconOverlayPlan ov;
    ov.icon = 1;
    ov.cell = 1;
    ov.size = 64;
    ov.gap = 12;
    ov.x = 88;
    ov.level = 2;
    ii.draw(ov);
    bool found = false;
    for (const auto& im : out.images)
        if (im.w == 128 && im.h == 64) {
            found = true;
            bool masked = false;
            for (const auto& m : out.images)
                if (m.name == "mask" && static_cast<DcId>(m.graphic) == im.id) masked = true;
            CHECK(masked);
        }
    CHECK(found);
    // seg7:0x477e's flames at Size 0: 230 halved to 80 x 16, the own image (5S + 6g) x (2g + S).
    IconFlamePlan fl;
    fl.icon = 0;
    fl.size = 16;
    fl.gap = 3;
    fl.h12 = 6;
    fl.strip_x = 3;
    fl.level = 0;
    ii.draw(fl);
    CHECK(out.size_of(images[static_cast<std::size_t>(kIconOwnSlot)]) == WH{98, 22});
    int paints = 0;
    for (const BlitCall& b : out.calls)
        if (b.op == BlitCall::kBitBlt && b.rop == kSrcPaint && b.dst == images[static_cast<std::size_t>(kIconOwnSlot)]) {
            ++paints;
            const SizedBlitter::Image* g = out.find(b.src);  // deleted after the call: its record
            REQUIRE(g);
            CHECK(g->w == 80);
            CHECK(g->h == 16);
            CHECK(b.sx == 16 * (paints - 1));
        }
    CHECK(paints == 5);
}

// ---- the Size readers beside the images (s8-size.md 4.2; the ring's offsets are
// tests/test_world_ring.cpp's)

TEST_CASE("size sites: the entry effect's blocks and the flame cell at Size 0, 1 and 2") {
    // seg7:0x13b2's block (2 << Size) >> 1 and the cell's blocks: the TRANSFORMER's 17, 33, 66
    // are 17 blocks at every Size [BYTES seg7:0x1b6b seg7:0x1417 seg7:0x1435].
    CHECK(entry_block(0) == 1);
    CHECK(entry_block(1) == 2);
    CHECK(entry_block(2) == 4);
    CHECK(entry_blocks(17, 0) == 17);
    CHECK(entry_blocks(33, 1) == 17);
    CHECK(entry_blocks(66, 2) == 17);
    // The SKY TALON's 16, 31, 62 (its odd side made even at Size 0).
    CHECK(entry_blocks(16, 0) == 16);
    CHECK(entry_blocks(31, 1) == 16);
    CHECK(entry_blocks(62, 2) == 16);
}

// ---- P8: the visit's latched Size

TEST_CASE("size sites: the visit's pad strip height reads the pickup's Size DGROUP:0x959e") {
    World w;
    w.player = 0;
    AmbulanceState& a = ambulance_state();
    const std::int16_t saved = a.pickup_size;
    // The TRANSFORMER picked up at Size 1 (size 33): sh = 264 >> 1 = 132.
    w.pools.rec(14, 0).size = 33;
    w.size = 1;
    a.pickup_size = 1;
    CHECK(visit_strip_shift(w) == 132);
    // Ctrl + while carried: size 66 at Size 2, the latch still 1: 528 >> 1 = 264 (the Size in
    // force would give 132).
    w.pools.rec(14, 0).size = 66;
    w.size = 2;
    CHECK(visit_strip_shift(w) == 264);
    // Picked up at Size 2: 132 again; at Size 0 with size 17: 136.
    a.pickup_size = 2;
    CHECK(visit_strip_shift(w) == 132);
    w.pools.rec(14, 0).size = 17;
    a.pickup_size = 0;
    CHECK(visit_strip_shift(w) == 136);
    // The byte's count masked as a 386 does: 0x21 shifts by 1.
    w.pools.rec(14, 0).size = 33;
    a.pickup_size = 0x21;
    CHECK(visit_strip_shift(w) == 132);
    a.pickup_size = saved;
}

// ---- the goldens

namespace {

std::filesystem::path original_dir() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    return v;
}

// work/oracle/core-0011 beside IS_ORIGINAL_DIR (work/extracted/game).
std::filesystem::path core0011_dir() {
    const auto o = original_dir();
    if (o.empty()) return {};
    std::filesystem::path base = o;
    if (!base.has_filename()) base = base.parent_path();
    const auto d = base.parent_path().parent_path() / "oracle" / "core-0011";
    return std::filesystem::is_directory(d) ? d : std::filesystem::path();
}

// The TRANSFORMER's cell origin in the core-0011 sweep stills at Size 1 and 0: (512, 341), the
// wave-0001 golden's (504 + 8, 333 + 8) (tests/test_ship_sheet.cpp); the digest's 342 is the top
// of the lit pixels' first row in a 3 x 3 grouping [ORACLE core-0011] [ORACLE wave-0001].
constexpr int kCellX = 512, kCellY = 341;

struct Box {
    int w = 0, h = 0, lit = 0;
};
// The box of the pixels with a channel above 50 (core-0011's measure) in the n x n square.
template <typename F>
Box lit_box(int n, const F& rgb_at) {
    int x0 = n, y0 = n, x1 = -1, y1 = -1;
    Box b;
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) {
            const std::uint32_t c = rgb_at(x, y);
            if (((c >> 16) & 0xff) <= 50 && ((c >> 8) & 0xff) <= 50 && (c & 0xff) <= 50) continue;
            ++b.lit;
            x0 = x < x0 ? x : x0, y0 = y < y0 ? y : y0, x1 = x > x1 ? x : x1, y1 = y > y1 ? y : y1;
        }
    if (x1 >= 0) b.w = x1 - x0 + 1, b.h = y1 - y0 + 1;
    return b;
}

}  // namespace

TEST_CASE("size sites: the original's Size 0 cell is its Size 1 cell halved as the port halves it (core-0011, golden)") {
    const auto od = core0011_dir();
    if (od.empty()) {
        MESSAGE("skipped: work/oracle/core-0011 not found");
        return;
    }
    const auto p1 = od / "tr3" / "sw01.png", p0 = od / "tr3" / "sw10.png";
    if (!std::filesystem::exists(p1) || !std::filesystem::exists(p0)) {
        MESSAGE("skipped: the tr3 sweep stills are missing");
        return;
    }
    // The TRANSFORMER at rest at one heading, Size 1, then Size 0 after Ctrl - (a live
    // seg10:0x6308 on the 1x sheet), the cell origin (kCellX, kCellY) = (512, 341) in both (the
    // digest's 342 is one row low, above) [ORACLE core-0011].
    const pngtest::Image s1 = pngtest::read(p1.string()), s0 = pngtest::read(p0.string());
    REQUIRE(s1.w >= kCellX + 40);
    REQUIRE(s0.w >= kCellX + 40);
    // The 1x cell as the sheet holds it: 651 x 157, the 33 x 33 cell of heading 0 at (5, 5), its
    // colours as indices, the gap black. Halved by rescaled_sheet_pixels, the 17 x 17 cell at
    // (2, 2) must be the original's Size 0 picture pixel for pixel (ruling R4).
    std::vector<std::uint32_t> colours{0};
    auto index_of = [&colours](std::uint32_t c) {
        for (std::size_t i = 0; i < colours.size(); ++i)
            if (colours[i] == c) return static_cast<std::uint8_t>(i);
        colours.push_back(c);
        return static_cast<std::uint8_t>(colours.size() - 1);
    };
    SheetPixels sheet;
    sheet.w = 651;
    sheet.h = 157;
    sheet.index.assign(static_cast<std::size_t>(sheet.w) * sheet.h, 0);
    for (int y = 0; y < 33; ++y)
        for (int x = 0; x < 33; ++x)
            sheet.index[static_cast<std::size_t>(5 + y) * sheet.w + 5 + x] = index_of(s1.at(kCellX + x, kCellY + y));
    REQUIRE(colours.size() < 250u);
    const SheetPixels half = rescaled_sheet_pixels(sheet, 0);
    REQUIRE(half.w == 325);
    REQUIRE(half.h == 78);
    int diff = 0;
    for (int r = 0; r < 17; ++r)
        for (int c = 0; c < 17; ++c)
            diff += colours[half.index[static_cast<std::size_t>(2 + r) * half.w + 2 + c]] != s0.at(kCellX + c, kCellY + r);
    CHECK(diff == 0);
    // The rule is cell0(c, r) = cell1(2c, 2r - 1); the other choice of each pair, on either axis,
    // would not give the original's picture.
    auto cell1 = [&s1](int x, int y) { return x < 0 || y < 0 ? 0u : s1.at(kCellX + x, kCellY + y); };
    for (const auto& dxy : {std::pair<int, int>{-1, -1}, {0, 0}, {-1, 0}}) {
        int other = 0;
        for (int r = 0; r < 17; ++r)
            for (int c = 0; c < 17; ++c) other += s0.at(kCellX + c, kCellY + r) != cell1(2 * c + dxy.first, 2 * r + dxy.second);
        CHECK(other > 0);
    }
    int rule = 0;
    for (int r = 0; r < 17; ++r)
        for (int c = 0; c < 17; ++c) rule += s0.at(kCellX + c, kCellY + r) != cell1(2 * c, 2 * r - 1);
    CHECK(rule == 0);
    // core-0011's boxes: 15 x 11 (101 px), 31 x 23 (423 px) [ORACLE core-0011].
    const Box b0 = lit_box(17, [&](int x, int y) { return s0.at(kCellX + x, kCellY + y); });
    const Box b1 = lit_box(33, [&](int x, int y) { return s1.at(kCellX + x, kCellY + y); });
    CHECK(b0.w == 15);
    CHECK(b0.h == 11);
    CHECK(b0.lit == 101);
    CHECK(b1.w == 31);
    CHECK(b1.h == 23);
    CHECK(b1.lit == 423);
}

TEST_CASE("size sites: the port's TRANSFORMER and SKY TALON at Size 0, 1 and 2 against core-0011 (golden)") {
    const auto dir = original_dir();
    if (dir.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    is::assets::AssetStore store(dir);
    const is::assets::Ships ships = store.ships(dir);
    is::gfx::PaletteManager pm;
    const is::fmt::Palette5500 p5 = store.palette5500();
    is::gfx::Colours5500 c5{};
    for (std::size_t i = 0; i < c5.size(); ++i) c5[i] = {p5.colours[i].r, p5.colours[i].g, p5.colours[i].b};
    pm.set_colours_5500(c5);
    const is::gfx::PalHandle pal = pm.create(0);
    pm.load_scheme(pal, 8, 0);  // the wave palette, seg10:0x6e76(8, 0) [BYTES seg10:0x37]
    struct Ship {
        const char* name;
        int heading;           // the sweep's heading (core-0011's stills; heading 0 nose up)
        Box box0, box1, box2;  // core-0011's boxes at Size 0, 1 and 2
        bool exact_lit;  // the lit counts too (the TRANSFORMER's)
    };
    const Ship list[] = {{"TRANSFORMER", 0, {15, 11, 101}, {31, 23, 423}, {62, 46, 1734}, true},
                         {"SKY TALON", 0, {15, 13, 89}, {31, 28, 398}, {62, 56, 1612}, false}};
    for (const Ship& sp : list) {
        CAPTURE(std::string(sp.name));
        const is::fmt::ShipRecord* r = ships.find(sp.name);
        REQUIRE(r);
        const is::fmt::BodyRecord* b = ships.body(*r);
        REQUIRE(b);
        const is::fmt::Dib body = store.sprite(b->bitmap_id);
        // seg10:0x6ee6's w6 at Size 1 and at Size 2 with sel 0 (sheet_scale_flag), mode 1.
        auto render = [&](int size_level) {
            is::gfx::SheetArgs a;
            a.gap = 99;
            a.four = 1;
            a.scale = sheet_scale_flag(size_level, 0, 0);
            a.scheme_a = r->byte_07;
            a.scheme_b = r->byte_08;
            a.mode = 1;
            return is::gfx::ship_sheet_for(body, b->bitmap_id, pm, pal, a);
        };
        const is::gfx::ShipSheet sh1 = render(1), sh2 = render(2);
        REQUIRE(sh1.ok());
        REQUIRE(sh2.ok());
        CHECK(sh2.cell_size() == 2 * body.height);  // the fresh 2x render, the object's (side << 2) sar 1
        const auto& wp = *pm.working(pal);
        auto rgb = [&wp](std::int16_t v) { return v <= 0 ? 0u : wp[static_cast<std::size_t>(v)].rgb().rgb(); };
        const int d = sp.heading;
        const Box p1 = lit_box(sh1.cell_size(), [&](int x, int y) { return rgb(sh1.at(sh1.cell_x(d) + x, sh1.cell_y(0) + y)); });
        const Box p2 = lit_box(sh2.cell_size(), [&](int x, int y) { return rgb(sh2.at(sh2.cell_x(d) + x, sh2.cell_y(0) + y)); });
        // Size 0 is the 1x sheet halved (rescaled_sheet_pixels). The TRANSFORMER's matches
        // core-0011's 15 x 11 (101 px); the SKY TALON's sweep stills show another picture at Size 0
        // than the halved Size 1 one (35 pixels differ at the best placement), so its box is
        // reported only [ORACLE core-0011] [OPEN: which frame the SKY TALON shows in sw10].
        SheetPixels px;
        REQUIRE(sheet_pixels_from(sh1, px));
        const SheetPixels h0 = rescaled_sheet_pixels(px, 0);
        const int n0 = (body.height + (body.height & 1)) >> 1, g0 = sh1.gap >> 1;
        const Box p0 = lit_box(n0, [&](int x, int y) {
            return rgb(static_cast<std::int16_t>(h0.index[static_cast<std::size_t>(g0 + y) * h0.w + d * (n0 + g0) + g0 + x]));
        });
        MESSAGE(std::string(sp.name) << ": Size 0 " << p0.w << " x " << p0.h << " (" << p0.lit << " px), Size 1 " << p1.w << " x "
                        << p1.h << " (" << p1.lit << " px), Size 2 " << p2.w << " x " << p2.h << " (" << p2.lit
                        << " px)");
        CHECK(p1.w == sp.box1.w);
        CHECK(p1.h == sp.box1.h);
        CHECK(p2.w == sp.box2.w);
        CHECK(p2.h == sp.box2.h);
        if (sp.exact_lit) {
            CHECK(p0.w == sp.box0.w);
            CHECK(p0.h == sp.box0.h);
            CHECK(p0.lit == sp.box0.lit);
            CHECK(p1.lit == sp.box1.lit);
            CHECK(p2.lit == sp.box2.lit);
        }
    }
}

#ifdef _WIN32
// GDI's StretchBlt in COLORONCOLOR under Wine against gdi_stretch_map, through the program's
// GDI Blitter and seg10:0x6308 (rescale_image): halvings of even and odd extents (the
// TRANSFORMER's 651 x 157 sheet among them) and a doubling, every pixel (ruling R4) [DESIGN
// ADR 0017]. The same under Wine 11.20 (CTest) and the oracle's Wine 11.0 old WoW64.
TEST_CASE("size sites: GDI's halving and doubling equal rescaled_sheet_pixels (Win32)") {
    is::gfx::Gfx gfx(nullptr);
    gfx.set_client_size(64, 48, 1);
    REQUIRE(gfx.logical_width() > 0);
    GdiBlitter b(gfx);
    const WH sizes[] = {{8, 4}, {13, 7}, {651, 157}, {31, 31}, {2, 3}};
    for (const WH& s : sizes)
        for (int level : {0, 2}) {
            CAPTURE(s.first);
            CAPTURE(s.second);
            CAPTURE(level);
            SheetPixels px;
            px.w = s.first;
            px.h = s.second;
            px.index.resize(static_cast<std::size_t>(px.w) * px.h);
            for (int y = 0; y < px.h; ++y)
                for (int x = 0; x < px.w; ++x)
                    px.index[static_cast<std::size_t>(y) * px.w + x] = static_cast<std::uint8_t>(1 + (x * 7 + y * 13) % 250);
            ImageSlotTable table{};
            std::int16_t f740 = 0;
            table[0x28] = b.create_image_from(px.w, px.h, px.index);
            REQUIRE(table[0x28] != kDcNone);
            rescale_image(b, table, 0x28, kDcNone, -1, level, f740);
            const SheetPixels want = rescaled_sheet_pixels(px, level);
            int w = 0, h = 0;
            REQUIRE(b.image_size(table[0x28], w, h));
            CHECK(w == want.w);
            CHECK(h == want.h);
            auto img = b.image(table[0x28]);
            REQUIRE(img);
            img->flush();
            int diff = 0;
            for (int y = 0; y < want.h; ++y)
                for (int x = 0; x < want.w; ++x)
                    diff += img->at(x, y) != want.index[static_cast<std::size_t>(y) * want.w + x];
            CHECK(diff == 0);
        }
}

// ISPACE.DLL ordinal 15 on a cell wider than 90 (S5-10): Size 2 doubles the add-on's 53-px
// IDRAGON to 106 (no body of the game reaches it: the widest, ORCA's 41, gives 82). For s > 90
// the window of mask cell 0 starts above the bitmap (o = 45 - s / 2 < 0) [BYTES ISPACE.DLL
// seg5:0x11b3-0x11f4 seg5:0x12e2-0x1321]; GDI clips the source to the bitmap and leaves the
// destination there as it is, which is gfx::wing_keep's rule. Heading 0 of a cut (the identity
// placement) through one BitBlt SRCAND from the 450 x 90 mask, as the sheet pass makes it
// [BYTES ISPACE.DLL seg5:0x1494-0x1521] [DESIGN ADR 0017].
TEST_CASE("size sites: ordinal 15's window outside the mask is clipped as GDI clips it (Win32)") {
    is::gfx::Gfx gfx(nullptr);
    gfx.set_client_size(64, 48, 1);
    REQUIRE(gfx.logical_width() > 0);
    GdiBlitter b(gfx);
    const is::gfx::WingMask& m = is::gfx::wing_mask();
    // CreateBitmap rows are word-aligned, the first pixel in the high bit, 1 = white.
    const int stride = ((is::gfx::kWingMaskW + 15) / 16) * 2;
    std::vector<std::uint8_t> bits(static_cast<std::size_t>(stride) * is::gfx::kWingMaskH, 0);
    for (int y = 0; y < is::gfx::kWingMaskH; ++y)
        for (int x = 0; x < is::gfx::kWingMaskW; ++x)
            if (m.at(x, y)) bits[static_cast<std::size_t>(y) * stride + x / 8] |= static_cast<std::uint8_t>(0x80 >> (x % 8));
    HBITMAP mono = CreateBitmap(is::gfx::kWingMaskW, is::gfx::kWingMaskH, 1, 1, bits.data());
    REQUIRE(mono);
    HDC mdc = CreateCompatibleDC(nullptr);
    HGDIOBJ old = SelectObject(mdc, mono);
    for (int s : {100, 106, 120})
        for (int D : {0, 3, 10}) {
            CAPTURE(s);
            CAPTURE(D);
            const is::gfx::WingWindow win = is::gfx::wing_window(s, D, 0);
            CHECK(win.sy < 0);  // the window starts above the mask
            const DcId cell = b.create_image_from(s, s, std::vector<std::uint8_t>(static_cast<std::size_t>(s) * s, 1));
            auto img = b.image(cell);
            REQUIRE(img);
            BitBlt(img->hdc(), 0, 0, win.w, win.h, mdc, win.sx, win.sy, SRCAND);
            img->flush();
            const std::vector<std::uint8_t> keep = is::gfx::wing_keep(s, D, 0, true);
            int diff = 0;
            for (int y = 0; y < s; ++y)
                for (int x = 0; x < s; ++x)
                    diff += (img->at(x, y) != 0) != (keep[static_cast<std::size_t>(y) * s + x] != 0);
            CHECK(diff == 0);
        }
    SelectObject(mdc, old);
    DeleteDC(mdc);
    DeleteObject(mono);
}
#endif
