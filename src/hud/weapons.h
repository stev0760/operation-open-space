// The weapon table DGROUP:0x2f54 (seg10:0x1fb2, called from the state-1 entry seg10:0x51) and
// what the weapon display seg11:0x238e (id 0xdc) reads from it. Portable.
//
// seg10:0x1fb2 writes 28 blocks of 0x50 bytes from DGROUP:0x2f54 (the last, DGROUP:0x37c4, with
// name word -1); 23 words differ when DGROUP:0x920c (the edition, shell.md "Editions") is 3
// [BYTES seg10:0x1fb2-0x2ff8]. The words +0x48 ... +0x4e of block w are words -8 ... -2 of
// record w + 1: the picture routine seg11:0x2ff6(w) reads DGROUP:0x2f4c/0x2f4e/0x2f52 + 0x50·w
// [BYTES seg11:0x2ff6]. Record fields the HUD reads: +0x00 the ENG_LANG name string, +0x0a
// counted (non-zero: the line shows the count), +0x2e (non-zero: the count is not shown).
#pragma once

#include <array>
#include <cstdint>

#include "hud/host.h"

namespace is::hud {

constexpr int kWeaponBlocks = 28;        // DGROUP:0x2f54 ... DGROUP:0x3813
constexpr int kWeaponBlockSize = 0x50;   // bytes per record
constexpr std::uint16_t kDgWeaponTable = 0x2f54;

class WeaponTable {
public:
    using Block = std::array<std::int16_t, kWeaponBlockSize / 2>;
    // seg10:0x1fb2 with DGROUP:0x920c = `edition`.
    explicit WeaponTable(int edition = 0);
    // The signed word at DGROUP:0x2f54 + 0x50·w + offset (offset may be negative).
    std::int16_t word(int w, int offset) const;
    const Block& block(int w) const { return blocks_[static_cast<std::size_t>(w)]; }

private:
    std::array<Block, kWeaponBlocks> blocks_{};
};

// The weapon display's view of weapon w (the signed ship byte +0xb8 + slot) with count `count`
// (the ship byte +0xae + slot) [BYTES seg11:0x2a60-0x2ad1].
WeaponView weapon_view(const WeaponTable& t, int weapon, int count);

// seg11:0x2ff6(w), the weapon picture DGROUP:0x5040 of seg11:0x2f8c: none for w = 0; the
// graphic word +0x02 through seg10:0x6c02(id, 0, -1) when +0x0a = 0 or word -2 = -1 (and w !=
// 0x18); otherwise a composed bitmap (a base graphic by +0x14 and sprite cells sized by words
// -8 / -6), whose composition from seg11:0x30d0 on is unread [BYTES seg11:0x3003-0x303d
// seg11:0x3040-0x30bc] [OPEN].
struct WeaponPicture {
    enum class Kind { kNone, kGraphic, kComposed };
    Kind kind = Kind::kNone;
    int graphic = 0;  // kGraphic: the IGRAPHIC id
};
WeaponPicture weapon_picture(const WeaponTable& t, int weapon);

}  // namespace is::hud
