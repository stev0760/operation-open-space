// Design-record words the sheet build and the wing tear write, kept per design index n (ship
// word +0x4e) until the port has the 0xad-byte design table DGROUP:0x96d2 (structs.md "Design
// records") [DESIGN: one store per word set, the program's, as DGROUP holds them]. Readers:
// collide.cpp design_from_world (+0x2a, +0x3c), shots.cpp design_words_from_world (+0x2a,
// +0x68 ... +0x6e), purchases.cpp's design_word(0x2a), the wing picture (debris.h wing_picture).
//
// Sources: work/analysis/m4/s5-sheet.md sections 1.1, 5.4 and 6 (verified claims 4, 41, 42 in
// s5-sheet-verify.md) and work/analysis/m4/s5-weapons.md sections 8 and 9.6 (verified claims 42,
// 45, 46 in s5-weapons-verify.md). 16-bit arithmetic as the original's (ADR 0013 decision 7).
// Portable, no Win32. M4 stage 5, track SHEET.
#pragma once

#include <cstdint>

namespace is::world {

struct DesignWordSet {
    // +0x2a (DGROUP:0x96fc): the body's unscaled cell side h (ISPACE.DLL ordinal 5): sext(roster
    // byte +0x7a) at seg14:0xd94, or the object's `size` +0x22 before seg14:0x0's size step at
    // seg14:0xba9, both after the sheet build; seg10:0x9d1d/0x9e3a (the rescale), seg16:0x6012
    // (a new body) and seg18:0x3098 (seg18 mode) are the other writers [BYTES seg14:0xb97
    // seg14:0xba9 seg14:0xd86-0xd94 seg10:0x9d1d seg10:0x9e3a seg16:0x6012 seg18:0x3098]
    // (verified claim 41). The second wing loss sets ship word +0x20 = +0x2a - 14 (19 for the
    // TRANSFORMER and the SKY TALON) [ORACLE core-0001] [ORACLE core-0006].
    std::int16_t w2a = 0;
    // +0x3c (DGROUP:0x970e): D, the half-width of the fuselage band ordinal 15 keeps
    // (design_band) [BYTES seg14:0x2070-0x208b seg8:0x2d6f-0x2d8d].
    std::int16_t w3c = 0;
    // +0x68 ... +0x6e (DGROUP:0x973a ... 0x9740): the two barrels' offsets (design_barrels).
    std::int16_t w68 = 0, w6a = 0, w6c = 0, w6e = 0;
    // +0x00 and +0x02 (DGROUP:0x96d2, 0x96d4): seg14:0x0's s = trunc(side / 2) of the raw side
    // [bp-0x24] (design_half_side), the radii of the muzzle tables seg10:0x1460 (r = +0x00 + 3)
    // and seg10:0x1620 (r = +0x02) [BYTES seg14:0xc42-0xc54 seg10:0x147b seg10:0x163b]
    // (formats.md MASTER.BOD +0x19) (M4 s5 fixer).
    std::int16_t w00 = 0, w02 = 0;
};

// Design n's words; a zero set outside 0 ... 7 [DESIGN].
const DesignWordSet& design_words(int n);
void set_design_words(int n, const DesignWordSet& d);
// Every set back to zero (a new World; the tests).
void reset_design_words();

// The MASTER.BOD bytes seg14:0x11a8 and seg8:0x2cd8 read for design n's body (SHP word +0x21's
// record): +0x1a, +0x1e and +0x1f, each sign-extended at the read (`cbw`, `cwde`) [BYTES
// seg14:0x13a2 seg14:0x13a6 seg14:0x13c3 seg14:0x13c7 seg8:0x2d6f] [DESIGN: the port keeps the
// bytes per design where the original reads the loaded MASTER.BOD table DGROUP:0x6e1c].
struct DesignBody {
    std::int8_t b1a = 0, b1e = 0, b1f = 0;
    bool known = false;
};
const DesignBody& design_body(int n);
void set_design_body(int n, std::int8_t b1a, std::int8_t b1e, std::int8_t b1f);

// seg14:0x1b84's D (seg14:0x2070-0x208b): trunc(((sext(BOD +0x1a) << Size) sar 1) / 2), `shl ax,
// cl; sar ax, 1; cwd; sub ax, dx; sar ax, 1` in 16 bits, Size masked to 5 bits as a 386 does
// [BYTES seg14:0x2070-0x208b] (verified claim 4). TRANSFORMER (BOD 33 +0x1a = 10) at Size 1: 5.
std::int16_t design_band(std::int8_t bod_1a, int size_level);

// seg14:0x0's s for design words +0x00 / +0x02 [BYTES seg14:0xbb5-0xbc2 seg14:0xc42-0xc54]: the
// raw side [bp-0x24] (design +0x2a's value), one more at Size 0 for an odd side, then `cwd; sub
// ax, dx; sar ax, 1`. TRANSFORMER and SKY TALON (side 33) at Size 1: 16.
std::int16_t design_half_side(std::int16_t side, int size_level);

// seg8:0x2cd8's recompute at seg8:0x2d29-0x2d91, run while DGROUP:0x95a6 = 0 (the MASTER.SHP
// block is not loaded: always in a wave): the same formula on the same body byte, stored in
// design +0x3c. Outside seg18 mode it stores the value seg14:0x1b84 stored; in seg18 mode it
// replaces seg18:0x3307's sext(BOD +0x1a) (2 Size + 1) / 6 (verified claim 4). Nothing for a
// design whose body bytes are unknown [DESIGN]. seg14:0x3eda and seg14:0x4074 load and free the
// tables around it (verified claim 5); the port reads no file there [DESIGN].
void design_band_recompute(int n, int size_level);

// seg14:0x11a8's design tail (seg14:0x1380-0x13e0), on its render path only and while
// DGROUP:0x95a6 != 0 (the placement brackets seg14:0x0 with seg14:0x3eda / seg14:0x4074): with c
// = trunc(side / 2), side ordinal 5's value [bp - 0xa] (SheetBuild::side):
//   +0x68 = sext(BOD +0x1e) - c - Size, +0x6a = sext(BOD +0x1f) - c, +0x6c = -(+0x68 + Size),
//   +0x6e = +0x6a, all 16-bit
// [BYTES seg14:0x1380 seg14:0x1385 seg14:0x138d seg14:0x13a2 seg14:0x13a9 seg14:0x13b2
// seg14:0x13c3 seg14:0x13ca seg14:0x13d2 seg14:0x13d8 seg14:0x13e0] (s5-weapons-verify.md claim
// 42). SKY TALON (BOD 5: 1, 16) at side 33, Size 1: (-16, 0, 15, 0); TRANSFORMER (BOD 33: 4, 6):
// (-13, -10, 12, -10).
void design_barrels(DesignWordSet& d, std::int8_t b1e, std::int8_t b1f, std::int16_t side, int size_level);

}  // namespace is::world
