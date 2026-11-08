// M4 stage 5, track WEAPONS (hud/weapons.h): the run-time weapon words for every edition, the
// registered one's -1 included (triage S3-10), and seg10:0x4412's animation-4 sheet for weapons
// 10, 16 and 17 (work/analysis/m4/s5-weapons.md 2.5, verified claims 14-18), with the bitmap
// sizes of IGRAPHIC.DLL #4000 150, 840 and 860 [BYTES IGRAPHIC.DLL #4000 150] [BYTES
// IGRAPHIC.DLL #4000 840] [BYTES IGRAPHIC.DLL #4000 860].
#include <optional>

#include "doctest.h"
#include "hud/weapons.h"
#include "world/missile.h"
#include "world/sprite.h"
#include "world/world.h"

using namespace is::hud;
using is::world::BlitCall;

TEST_CASE("weapons: the words of the registered edition -1 are filled once (S3-10)") {
    is::world::World w;
    w.w920c = -1;  // the registered edition (shell.md "Editions")
    is::world::WeaponWords words;
    std::optional<int> filled;
    CHECK(fill_weapon_words(w.w920c, words, filled));
    CHECK(words.word(7, 0x0a) == 4);     // HEATSEEKER MISSILES: launched [BYTES seg10:0x2389]
    CHECK(words.word(7, 0x18) == 0x1c);  // its launch sound [BYTES seg10:0x23d4]
    CHECK(words.word(7, -2) == 370);     // its graphic [BYTES seg10:0x23bf]
    CHECK(words.word(7, 0x34) == 800);   // its price [BYTES seg10:0x243c]
    CHECK(words.word(8, -4) == -1);      // no image slot before seg10:0x533a
    // The same edition again copies nothing, so seg10:0x533a's W(w)-4 stays
    words.set_word(8, -4, 0x30);
    CHECK_FALSE(fill_weapon_words(-1, words, filled));
    CHECK(words.word(8, -4) == 0x30);
    // Another edition copies again; edition 3 takes its 23 words (W(6)+0x36 = 7)
    CHECK(fill_weapon_words(0, words, filled));
    CHECK(words.word(8, -4) == -1);
    CHECK(words.word(6, 0x36) == 8);
    CHECK(fill_weapon_words(3, words, filled));
    CHECK(words.word(6, 0x36) == 7);
}

TEST_CASE("weapons: seg10:0x4412's plan for weapon 10, graphic 150 (22 x 22, n 6)") {
    const Anim4Sheet p = anim4_sheet_plan(22, 6);
    CHECK(p.gap == 6);
    CHECK(p.width == 174);
    CHECK(p.height == 34);
    CHECK(p.stretch);
    REQUIRE(p.frames.size() == 6);
    for (int i = 0; i < 6; ++i) {
        CHECK(p.frames[static_cast<std::size_t>(i)].dx == 6 + 28 * i);
        CHECK(p.frames[static_cast<std::size_t>(i)].dy == 6);
        CHECK(p.frames[static_cast<std::size_t>(i)].side == 10 + 2 * i);
    }
    // The weapon picture's source cell of W(10)+0x06 = 4: (W+0x26 - 1)(cell + gap) + gap = 146,
    // frame 5 of side 20 inside the 22 x 22 cell (core.md "Picture seg11:0x2ff6")
    const WeaponPicture pic = weapon_picture(WeaponTable(0), 10);
    CHECK(pic.sx == 146);
    CHECK(pic.sy == 6);
    CHECK(pic.cell == 22);
    CHECK(p.frames[5].dx == pic.sx);
}

TEST_CASE("weapons: seg10:0x4412's plan for weapons 16 and 17") {
    const Anim4Sheet d = anim4_sheet_plan(23, 8);  // graphic 840, DOUGHNUTS
    CHECK(d.gap == 5);
    CHECK(d.width == 229);
    CHECK(d.height == 33);
    REQUIRE(d.frames.size() == 8);
    for (int i = 0; i < 8; ++i) {
        CHECK(d.frames[static_cast<std::size_t>(i)].dx == 5 + 28 * i);
        CHECK(d.frames[static_cast<std::size_t>(i)].side == 7 + 2 * i);
    }
    const Anim4Sheet p = anim4_sheet_plan(9, 1);  // graphic 860, PROTECTIVE PELLETS
    CHECK(p.gap == 5);
    CHECK(p.width == 19);
    CHECK(p.height == 19);
    CHECK_FALSE(p.stretch);
    REQUIRE(p.frames.size() == 1);
    CHECK(p.frames[0].dx == 5);
    CHECK(p.frames[0].dy == 5);
    CHECK(p.frames[0].side == 9);
    // The words that send them here: W+0x06 = 4 and W+0x26 = n [BYTES seg10:0x27f8 seg10:0x27ad
    // seg10:0x2ddd seg10:0x2d84 seg10:0x2bcc seg10:0x2c76]
    const WeaponTable t(0);
    CHECK(t.word(10, 0x06) == 4);
    CHECK(t.word(10, 0x26) == 6);
    CHECK(t.word(16, 0x06) == 4);
    CHECK(t.word(16, 0x26) == 8);
    CHECK(t.word(17, 0x06) == 4);
    CHECK(t.word(17, 0x26) == 1);
}

TEST_CASE("weapons: seg10:0x4412's GDI calls on the Blitter, and the gap word") {
    is::world::RecordingBlitter b;
    const is::world::DcId g = b.load_graphic(150, 3, -1);
    const Anim4Sheet p = anim4_sheet_plan(22, 6);
    const is::world::DcId k = render_anim4_sheet(b, g, p);
    REQUIRE(k != is::world::kDcNone);
    REQUIRE(b.images.size() == 2);
    CHECK(b.images[1].w == 174);
    CHECK(b.images[1].h == 34);
    REQUIRE(b.calls.size() == 8);
    CHECK(b.calls[0] == is::world::pat_blt_call(k, 0, 0, 174, 34, is::world::kBlackness));
    CHECK(b.calls[1].op == BlitCall::kStretchMode);
    CHECK(b.calls[1].rop == 3u);  // COLORONCOLOR
    for (int i = 0; i < 6; ++i) {
        BlitCall c = is::world::bit_blt_call(k, 6 + 28 * i, 6, 10 + 2 * i, 10 + 2 * i, g, 0, 0, is::world::kSrcCopy);
        c.op = BlitCall::kStretchBlt, c.sw = 22, c.sh = 22;
        CHECK(b.calls[static_cast<std::size_t>(2 + i)] == c);
    }
    // n = 1: one BitBlt, no stretch mode
    is::world::RecordingBlitter b1;
    const is::world::DcId g1 = b1.load_graphic(860, 3, -1);
    const is::world::DcId k1 = render_anim4_sheet(b1, g1, anim4_sheet_plan(9, 1));
    REQUIRE(b1.calls.size() == 2);
    CHECK(b1.calls[1] == is::world::bit_blt_call(k1, 5, 5, 9, 9, g1, 0, 0, is::world::kSrcCopy));
    // The gap into pool 6's record 0 +0x24 [BYTES seg10:0x4453 seg10:0x4465]
    is::world::Pools pools;
    anim4_gap_record(pools, 6, 5);
    CHECK(pools.rec(6, 0).gap == 5);
}
