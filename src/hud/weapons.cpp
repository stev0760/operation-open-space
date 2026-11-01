// The weapon table DGROUP:0x2f54 that seg10:0x1fb2 fills (weapons.h). The values were read by
// executing the filler's straight-line stores (mov word ptr [DGROUP], imm/ax/cx; the one
// cmp [0x920c], 3 / jne pair per edition-dependent word) for DGROUP:0x920c != 3 and = 3: 816
// stores to 792 words, 28 blocks [BYTES seg10:0x1fb2-0x2ff8]. Words the filler does not write
// are BSS (0).
#include "hud/weapons.h"

namespace is::hud {

namespace {

// The words of block w for DGROUP:0x920c != 3 [BYTES seg10:0x1fb2-0x2ff8].
const WeaponTable::Block kBase[kWeaponBlocks] = {
    {{186, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},  // w0 DGROUP:0x2f54
    {{187, 194, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 0, 0, 0, 25, 6, 1, 20, 0, 200, 600, 17, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0}},  // w1 DGROUP:0x2fa4
    {{188, 190, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 40, 6, 2, 25, 60, 280, 1000, 15, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0}},  // w2 DGROUP:0x2ff4
    {{189, 197, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 80, 7, 4, 20, 20, 300, 950, 14, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0}},  // w3 DGROUP:0x3044
    {{190, 196, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, 5, 0, 8, 20, 350, 650, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},  // w4 DGROUP:0x3094
    {{191, 191, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 350, 0, 0, 0, 7, 4, 0, 8, 20, 0, 700, -1, 0, 0, 0, 0, 0, 0, 0, 0, 23, 1, -1, 370}},  // w5 DGROUP:0x30e4
    {{192, -1, 1, 2, 1, 2, 8, 100, 20, 0, 0, 0, 28, 3, 2, 200, 50, 7, 1, 0, 20, 9, 3, 0, 100, 700, 1100, 8, 1, 1, 0, 0, 0, 0, 0, 0, 23, 1, -1, 370}},  // w6 DGROUP:0x3134
    {{193, -1, 1, 2, 1, 4, 7, 100, 20, 0, 0, 0, 28, 3, 2, 200, 0, 7, 1, 0, 16, 7, 8, 0, 110, 360, 800, -1, 0, 1, 1, 1, 0, 0, 0, 0, 23, 1, -1, 370}},  // w7 DGROUP:0x3184
    {{194, -1, 1, 2, 1, 3, 10, 120, 20, 0, 0, 0, 28, 3, 2, 200, 40, 9, 1, 0, 20, 7, 8, 0, 130, 380, 750, -1, 0, 1, 1, 0, 0, 0, 0, 0, 21, 3, -1, 161}},  // w8 DGROUP:0x31d4
    {{195, -1, 2, 1, 1, 4, 9, 70, 20, 2, 4, 3, 2, 2, 3, 200, 0, 6, 1, 0, 16, 6, 7, 0, 85, 270, 550, 10, 1, 1, 0, 1, 5, 0, 0, 0, 22, 5, -1, 150}},  // w9 DGROUP:0x3224
    {{196, 150, 2, 4, 1, 1, 7, 50, 20, 0, 0, 0, 24, 3, 2, 64, 0, 6, 1, 6, 12, 4, 5, 0, 80, 260, 450, -1, 0, 1, 0, 0, 6, 0, 0, 0, 23, 1, -1, 370}},  // w10 DGROUP:0x3274
    {{197, -1, 1, 2, 1, 1, 7, 100, 20, 0, 0, 0, 28, 3, 2, 100, 0, 8, 1, 0, 12, 4, 3, 0, 90, 290, 450, -1, 0, 1, 0, 1, 0, 0, 0, 0, 19, 3, -1, 160}},  // w11 DGROUP:0x32c4
    {{198, -1, 1, 1, 1, 4, 8, 90, 20, 2, 1, 2, 78, 2, 3, 200, 0, 6, 1, 0, 16, 5, 6, 0, 120, 300, 500, 5, 0, 1, 0, 0, 7, 0, 0, 0, 0, 0, 0, 0}},  // w12 DGROUP:0x3314
    {{199, 195, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 40, 300, 750, 16, 0, 0, 0, 0, 0, 0, 0, 1, 21, 3, -1, 680}},  // w13 DGROUP:0x3364
    {{200, -1, 1, 2, 1, 4, 7, 100, 20, 0, 3, 1, 28, 6, 3, 210, 0, 6, 1, 0, 30, 8, 9, 0, 100, 370, 900, -1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0}},  // w14 DGROUP:0x33b4
    {{201, 190, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 26, 0, 2, 20, 60, 280, 750, -1, 0, 0, 0, 0, 0, 0, 0, 0, 23, 5, -1, 840}},  // w15 DGROUP:0x3404
    {{202, 840, 2, 4, 1, 1, 7, 50, 20, 0, 0, 0, 24, 4, 4, 350, 0, 6, 1, 8, 14, 4, 5, 0, 80, 260, 500, -1, 0, 1, 0, 0, 6, 1, 0, 0, 9, 3, -1, 860}},  // w16 DGROUP:0x3454
    {{203, -1, 3, 4, 1, 5, 7, 120, 20, 0, 0, 0, 28, 6, 3, 60, 0, 1, 1, 1, 8, 2, 1, 0, 100, 370, 600, -1, 0, 1, 0, 1, 0, 0, 0, 1, 23, 3, -1, 330}},  // w17 DGROUP:0x34a4
    {{204, -1, 1, 1, 1, 1, 6, 75, 20, 0, 0, 0, 2, 4, 3, 106, 0, 4, 4, 0, 16, 4, 1, 0, 120, 300, 650, 4, 0, 1, 0, 0, 0, 0, 0, 0, 27, 3, -1, 335}},  // w18 DGROUP:0x34f4
    {{205, -1, 1, 1, 1, 1, 4, 75, 20, 0, 0, 0, 2, 3, 3, 106, 0, 7, 6, 0, 16, 4, 1, 0, 120, 300, 700, 21, 0, 1, 0, 0, 0, 0, 0, 0, 21, 3, -1, 185}},  // w19 DGROUP:0x3544
    {{206, -1, 1, 1, 1, 1, 6, 75, 20, 0, 0, 0, 2, 4, 3, 106, 0, 5, 4, 0, 16, 4, 1, 0, 120, 300, 550, 11, 0, 1, 0, 0, 0, 0, 0, 0, 15, 3, -1, 188}},  // w20 DGROUP:0x3594
    {{207, -1, 1, 1, 3, 5, 4, 95, 20, -1, 0, 0, 2, 3, 3, 85, 0, 3, 1, 0, 8, 5, 1, 0, 90, 350, 650, -1, 0, 1, 0, 0, 0, 0, 0, 1, 2, 0, -1, -1}},  // w21 DGROUP:0x35e4
    {{208, 199, 1, 2, 1, 3, 5, 80, 20, 0, 0, 0, 28, 3, 2, 200, 0, 20, 1, 0, 30, 7, 8, 150, 110, 360, 0, -1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0}},  // w22 DGROUP:0x3634
    {{209, 198, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 700, 7, 0, 100, 50, 900, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0}},  // w23 DGROUP:0x3684
    {{210, 189, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 40, 0, 0, 0, 3, 0, 0, 0, 20, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0}},  // w24 DGROUP:0x36d4
    {{211, 192, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 40, 0, 0, 0, 3, 0, 0, 0, 20, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},  // w25 DGROUP:0x3724
    {{212, 193, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 40, 0, 0, 0, 3, 0, 0, 0, 20, 0, 0, -1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 3, -1, 0}},  // w26 DGROUP:0x3774
    {{-1, -1, 1, 5, 1, 1, 9, 70, 20, 2, 0, 0, 0, 0, 0, 50, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 1, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0}},  // w27 DGROUP:0x37c4
};

struct Override {
    int block, offset;
    std::int16_t value;
};
// The 23 words whose store sits behind cmp word ptr [0x920c], 3 / jne: their values for
// DGROUP:0x920c = 3 [BYTES seg10:0x1fb2-0x2ff8].
const Override kEdition3[] = {
    {1, 0x36, 5},  // DGROUP:0x2fda
    {2, 0x36, 5},  // DGROUP:0x302a
    {3, 0x36, 5},  // DGROUP:0x307a
    {4, 0x36, 5},  // DGROUP:0x30ca
    {6, 0x36, 7},  // DGROUP:0x316a
    {8, 0x36, 7},  // DGROUP:0x320a
    {11, 0x36, 7},  // DGROUP:0x32fa
    {12, 0x36, 10},  // DGROUP:0x334a
    {13, 0x36, 10},  // DGROUP:0x339a
    {14, 0x36, 7},  // DGROUP:0x33ea
    {15, 0x36, 5},  // DGROUP:0x343a
    {16, 0x36, 10},  // DGROUP:0x348a
    {17, 0x36, 10},  // DGROUP:0x34da
    {18, 0x36, 7},  // DGROUP:0x352a
    {19, 0x36, 7},  // DGROUP:0x357a
    {20, 0x36, 7},  // DGROUP:0x35ca
    {21, 0x36, 7},  // DGROUP:0x361a
    {22, 0x36, 7},  // DGROUP:0x366a
    {23, 0x36, 5},  // DGROUP:0x36ba
    {24, 0x36, 5},  // DGROUP:0x370a
    {25, 0x36, 7},  // DGROUP:0x375a
    {26, 0x36, 5},  // DGROUP:0x37aa
    {27, 0x36, 5},  // DGROUP:0x37fa
};

}  // namespace

WeaponTable::WeaponTable(int edition) {
    for (int w = 0; w < kWeaponBlocks; ++w) blocks_[static_cast<std::size_t>(w)] = kBase[w];
    if (edition != 3) return;
    for (const Override& o : kEdition3)
        blocks_[static_cast<std::size_t>(o.block)][static_cast<std::size_t>(o.offset / 2)] = o.value;
}

std::int16_t WeaponTable::word(int w, int offset) const {
    // The record w's word at offset (negative offsets reach the block before, as seg11:0x2ff6's
    // DGROUP:0x2f4c + 0x50·w does). Outside the table: 0 [DESIGN] (the original reads whatever
    // DGROUP holds there).
    const int at = w * kWeaponBlockSize + offset;
    if (at < 0 || at >= kWeaponBlocks * kWeaponBlockSize || (at & 1)) return 0;
    return blocks_[static_cast<std::size_t>(at / kWeaponBlockSize)]
                  [static_cast<std::size_t>((at % kWeaponBlockSize) / 2)];
}

WeaponView weapon_view(const WeaponTable& t, int weapon, int count) {
    WeaponView v;
    v.name_id = t.word(weapon, 0x00);       // DGROUP:0x2f54 + 0x50·w [BYTES seg11:0x2ab9 seg11:0x2ad1]
    v.counted = t.word(weapon, 0x0a) != 0;  // DGROUP:0x2f5e + 0x50·w [BYTES seg11:0x2a72]
    v.no_count = t.word(weapon, 0x2e) != 0; // DGROUP:0x2f82 + 0x50·w [BYTES seg11:0x2a79]
    v.count = count;
    return v;
}

WeaponPicture weapon_picture(const WeaponTable& t, int weapon) {
    WeaponPicture p;
    if (weapon == 0) return p;  // seg11:0x3003-0x300b
    // seg11:0x300e-0x3024: +0x0a = 0, or word -2 = -1, and not weapon 0x18.
    if ((t.word(weapon, 0x0a) == 0 || t.word(weapon, -2) == -1) && weapon != 0x18) {
        p.kind = WeaponPicture::Kind::kGraphic;
        p.graphic = t.word(weapon, 0x02);  // seg11:0x302e: DGROUP:0x2f56 + 0x50·w
        return p;
    }
    p.kind = WeaponPicture::Kind::kComposed;  // seg11:0x3040 onward [OPEN]
    return p;
}

}  // namespace is::hud
