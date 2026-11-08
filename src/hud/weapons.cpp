// The weapon table DGROUP:0x2f54 that seg10:0x1fb2 fills (weapons.h). The values were read by
// executing the filler's straight-line stores (mov word ptr [DGROUP], imm/ax/cx; the one
// cmp [0x920c], 3 / jne pair per edition-dependent word) for DGROUP:0x920c != 3 and = 3: 816
// stores to 792 words, 28 blocks [BYTES seg10:0x1fb2-0x2ff8]. Words the filler does not write
// are BSS (0).
#include "hud/weapons.h"

#include "world/world.h"  // guard_todo

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
    // DGROUP:0x2f4c + 0x50·w does). Outside the table: 0, logged once
    // [DESIGN ADR 0016 decision 2] (the original reads whatever DGROUP holds there).
    const int at = w * kWeaponBlockSize + offset;
    if (at < 0 || at >= kWeaponBlocks * kWeaponBlockSize) {
        static bool once = false;
        world::guard_todo(once, "a weapon word outside the table reads 0", "DGROUP:0x2f54");
        return 0;
    }
    if (at & 1) return 0;
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

namespace {

std::int16_t w16(int v) { return static_cast<std::int16_t>(v); }
// cwd; sub ax, dx; sar ax, 1: a signed halving toward zero.
std::int16_t half16(int v) {
    const std::int16_t x = w16(v);
    return w16((x - (x < 0 ? -1 : 0)) >> 1);
}

}  // namespace

WeaponPicture weapon_picture(const WeaponTable& t, int weapon, const WeaponPictureInputs& in) {
    WeaponPicture p;
    if (weapon == 0) return p;  // seg11:0x3003-0x300b
    // seg11:0x300e-0x3024: +0x0a = 0, or word -2 = -1, and not weapon 0x18.
    if ((t.word(weapon, 0x0a) == 0 || t.word(weapon, -2) == -1) && weapon != 0x18) {
        p.kind = WeaponPicture::Kind::kGraphic;
        p.graphic = t.word(weapon, 0x02);  // seg11:0x302e: DGROUP:0x2f56 + 0x50·w
        return p;
    }
    p.kind = WeaponPicture::Kind::kComposed;
    p.weapon = weapon;
    p.name = t.word(weapon, 0x00);  // [BYTES seg11:0x317d seg11:0x3222]
    // seg11:0x3044-0x3078: A = W-8 (B = W-6 is overwritten before use, claim 7) when +0x0a != 0;
    // only w = 0x18 reaches the roster bytes (cbw) (s2-display-verify.md claim 2).
    int a = t.word(weapon, 0x0a) != 0 ? t.word(weapon, -8) : static_cast<std::int8_t>(in.roster_7a);
    p.a_raw = w16(a);
    // seg11:0x307b-0x30cb: the overlay graphic.
    switch (t.word(weapon, 0x14)) {
    case 1:
    case 4: p.overlay = 0xbf; break;
    case 2: p.overlay = 0xc4; break;
    case 3: p.overlay = 0xc5; break;
    case 0: p.overlay = weapon == 0x18 ? 0xbd : 0; break;
    default: p.overlay_unset = true; break;  // [bp-0xe] unwritten [OPEN]
    }
    // seg11:0x30e2-0x30f3: S = A for w = 0x18, else 2·A (16-bit, the A before the odd rule).
    p.side_overlay = weapon == 0x18 ? p.a_raw : w16(2 * p.a_raw);
    // seg11:0x3179-0x31bc: names 0xc4, 0xc6, 0xc3 take (W-8 << 2) / 3 (signed idiv), the rest
    // 2·(W-8): W-8 read again from the table, not A.
    const std::int16_t w8 = t.word(weapon, -8);
    if (p.name == 0xc4 || p.name == 0xc6 || p.name == 0xc3)
        p.side_plain = w16(w16(w8 << 2) / 3);
    else
        p.side_plain = w16(2 * w8);
    // seg11:0x3229-0x3253: an odd A grows by one with gap 4, an even one keeps gap 6; both
    // scale by Size as (v << Size) sar 1.
    int b;
    if (a & 1) {
        a = w16(a + 1);
        b = 4;
    } else {
        b = 6;
    }
    const int sz = in.size_level & 0xff;  // mov cl, byte [0x9156]
    p.gap = w16(w16(b << sz) >> 1);
    p.cell = w16(w16(a << sz) >> 1);
    // seg11:0x325a-0x32a6: the source cell; the test is W+0x06, not the kind +0x0a (claim 8).
    const std::int16_t k06 = t.word(weapon, 0x06);
    if (k06 == 5 || p.name == 0xc6 || p.name == 0xc3)
        p.sx = p.gap;
    else if (k06 == 4)
        p.sx = w16(w16(w16(t.word(weapon, 0x26) - 1) * w16(p.cell + p.gap)) + p.gap);
    else
        p.sx = w16(w16(4 * p.cell) + w16(5 * p.gap));  // frame 4 of the sheet
    p.sy = p.gap;
    // seg11:0x32c7-0x32eb: the source DC.
    if (weapon == 0x18)
        p.sheet_slot = 0x28 + in.player;  // DGROUP:0x3a38[DGROUP:0x9646]
    else
        p.sheet_slot = in.sheet_slot >= 0 ? in.sheet_slot : t.word(weapon, -4);  // DGROUP:0x39e8[W-4]
    return p;
}

void compose_weapon_picture(Canvas& c, const WeaponPicture& p) {
    if (p.kind != WeaponPicture::Kind::kComposed) return;
    // G != 0 [BYTES seg11:0x30d0-0x30d4]: the overlay the caller loaded.
    const bool g = p.overlay != 0 && !p.overlay_unset && c.has(Img::kWeaponOverlay);
    int side, wd, ht, ys = 0, yg = 0, gw = 0, gh = 0;
    if (g) {
        // seg11:0x30d8-0x3177: GetObject's bmWidth / bmHeight [BYTES seg11:0x30e2-0x313f].
        gw = c.width(Img::kWeaponOverlay);
        gh = c.height(Img::kWeaponOverlay);
        side = p.side_overlay;
        ht = gh > side ? gh : side;  // H = max(gh, S) [BYTES seg11:0x3106]
        wd = w16(gw + side);
        if (ht > side)
            ys = half16(ht - side);
        else
            yg = half16(side - gh);
    } else {
        side = p.side_plain;  // seg11:0x3179-0x31c7
        wd = ht = side;
    }
    // CreateCompatibleBitmap(DGROUP:0x13dc, Wd, H) and PatBlt BLACKNESS over it [BYTES
    // seg11:0x31d2 seg11:0x3219] (claim 6).
    c.create(Img::kWeapon, wd, ht);
    c.pat_blt(Img::kWeapon, 0, 0, wd, ht, kBlackness, Brush::none());
    // SetStretchBltMode(3) and StretchBlt SRCCOPY from the sheet [BYTES seg11:0x32ae
    // seg11:0x3300] (claim 9). The canvas's stretch_blt sets COLORONCOLOR itself.
    if (c.has(Img::kWeaponSheet))
        c.stretch_blt(Img::kWeapon, 0, ys, side, side, Img::kWeaponSheet, p.sx, p.sy, p.cell, p.cell,
                      kSrcCopy);
    // The overlay, BitBlt SRCCOPY at (S, yG) [BYTES seg11:0x3326] (claim 4).
    if (g) c.bit_blt(Img::kWeapon, side, yg, gw, gh, Img::kWeaponOverlay, 0, 0, kSrcCopy);
}

bool fill_weapon_words(int edition, world::WeaponWords& words, std::optional<int>& filled_for) {
    if (filled_for && *filled_for == edition) return false;
    filled_for = edition;
    const WeaponTable t(edition);
    for (int k = 0; k < world::kWeaponTableBlocks && k < kWeaponBlocks; ++k) words.block(k) = t.block(k);
    return true;
}

Anim4Sheet anim4_sheet_plan(int h, int n) {
    Anim4Sheet p;
    const std::int16_t H = w16(h), N = w16(n);
    // GetObject's bmHeight `test al, 1`: 5 for odd, 6 for even [BYTES seg10:0x4445-0x4453]
    p.gap = (H & 1) != 0 ? 5 : 6;
    const std::int16_t G = w16(p.gap);
    // Two 16-bit `imul`s and a 16-bit add [BYTES seg10:0x4483-0x44a1]
    p.width = world::add16(world::mul16(H, N), world::mul16(G, world::add16(N, 1)));
    p.height = world::add16(world::mul16(2, G), H);  // [BYTES seg10:0x44b9-0x44c7]
    p.stretch = N != 1;                              // [BYTES seg10:0x4540-0x4544]
    if (!p.stretch) {
        p.frames.push_back({p.gap, p.gap, h});  // BitBlt(DC[k], gap, gap, h, h, ...) [BYTES seg10:0x4546-0x4568]
        return p;
    }
    // for i = 0; i < n (signed): s = h + 2(i - n) at x, x += gap + h [BYTES seg10:0x458e-0x45f5]
    std::int16_t x = G;
    for (std::int16_t i = 0; i < N; ++i) {
        const std::int16_t side = world::add16(world::mul16(2, world::sub16(i, N)), H);  // [BYTES seg10:0x45aa-0x45b1]
        p.frames.push_back({x, p.gap, side});
        x = world::add16(x, world::add16(G, H));  // [BYTES seg10:0x45e6-0x45e9]
    }
    return p;
}

world::DcId render_anim4_sheet(world::Blitter& out, world::DcId graphic, const Anim4Sheet& plan) {
    // CreateCompatibleDC + CreateCompatibleBitmap(DGROUP:0x13dc, width, 2 gap + h) + SelectObject
    // [BYTES seg10:0x44aa-0x44e2]; the palette selections of seg10:0x44e6-0x450d are the
    // Blitter's (ADR 0007).
    const world::DcId k = out.create_image(plan.width, plan.height);
    if (k == world::kDcNone) return k;
    out.pat_blt(k, 0, 0, plan.width, plan.height, world::kBlackness);  // rop 0x42 [BYTES seg10:0x453b]
    if (graphic == world::kDcNone) return k;
    if (!plan.stretch) {
        for (const Anim4Frame& f : plan.frames)  // [BYTES seg10:0x4568]
            out.bit_blt(k, f.dx, f.dy, f.side, f.side, graphic, 0, 0, world::kSrcCopy);
        return k;
    }
    out.set_stretch_mode(k, world::kColorOnColor);  // SetStretchBltMode(DC[k], 3) [BYTES seg10:0x457b]
    const int h = plan.height - 2 * plan.gap;
    for (const Anim4Frame& f : plan.frames)  // seg10:0x9962(0, DC[k], x, gap, s, s, dcS, 0, 0, h, h, SRCCOPY) [BYTES seg10:0x45d8]
        out.stretch_blt(k, f.dx, f.dy, f.side, f.side, graphic, 0, 0, h, h, world::kSrcCopy);
    return k;
}

void anim4_gap_record(world::Pools& pools, int pool, int gap) {
    if (pool < 0 || pool >= world::kPoolCount || pools.records(pool) < 1) return;
    pools.rec(pool, 0).gap = w16(gap);  // [BYTES seg10:0x4453 seg10:0x4465]
}

TabSelection tab_select(int slot, int slots, const std::function<int(int)>& weapon_byte, int play_mode) {
    TabSelection r;
    const std::int16_t old = w16(slot);
    r.dg_9524 = old;  // seg11:0xae04-0xae0b
    r.slot = old;
    const std::int8_t cb = static_cast<std::int8_t>(slots);
    if (cb > 1) {  // cmp byte [+0xcb], 1; jle (signed) [BYTES seg11:0xae0e seg11:0xae13]
        std::int16_t s = w16(old + 1);  // seg11:0xae15
        // One weapon-0 slot is skipped, once [BYTES seg11:0xae1b-0xae23].
        if ((weapon_byte(s) & 0xff) == 0) s = w16(s + 1);
        // cbw of +0xcb against DGROUP:0x9524, jg (signed) [BYTES seg11:0xae2c-0xae35]; the wrap
        // is 0 in Strategy mode and 1 in Action mode [BYTES seg11:0xae37-0xae46].
        if (!(cb > s)) s = play_mode != 0 ? 0 : 1;
        r.dg_9524 = s;
        r.slot = s;  // [BYTES seg11:0xae55]
    } else {
        r.slot = 0;  // [BYTES seg11:0xae61]: DGROUP:0x9524 keeps the old slot
    }
    // The old slot's weapon 3 (LIGHTNING): object byte +0x50 = 0 [BYTES seg11:0xae70-0xae87].
    r.clear_b50 = (weapon_byte(old) & 0xff) == 3;
    return r;
}

int wave_start_slot(int slot, int slots, int restored) {
    if (restored != 0) return slot;  // [BYTES seg11:0xae9e-0xaea3]
    // cmp byte [+0xcb], 1; jle [BYTES seg11:0xaeab-0xaec0]
    return static_cast<std::int8_t>(slots) > 1 ? 1 : 0;
}

WeaponKey weapon_key(std::uint32_t msg, std::uint16_t key, const WeaponKeyState& s) {
    const bool fire_key = key == 0x0d || key == 'Z';
    if (msg == 0x101) {
        if (!fire_key) return WeaponKey::kNone;
        if (s.gated || s.demo == 0) return WeaponKey::kIgnored;  // [BYTES seg11:0x6cd1 seg11:0x6d1e]
        return s.play_mode == 0 ? WeaponKey::kFire : WeaponKey::kIgnored;  // [BYTES seg11:0x6e11-0x6e16]
    }
    if (msg != 0x100) return WeaponKey::kNone;
    if (key == 0x09) {
        if (s.gated || s.demo == 0) return WeaponKey::kIgnored;  // [BYTES seg11:0x6e19]
        return WeaponKey::kTab;
    }
    if (!fire_key) return WeaponKey::kNone;
    if (s.gated || s.demo == 0) return WeaponKey::kIgnored;
    // [BYTES seg11:0x6f58-0x6f69]: PlayMode 0 and weapon 13
    return s.play_mode == 0 && s.weapon == 13 ? WeaponKey::kFire : WeaponKey::kIgnored;
}

}  // namespace is::hud
