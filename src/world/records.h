// The wave's data structures (docs/re/structs.md "Object pools", "Object record (0x70 bytes)",
// "Effect table", "Ship records"; docs/re/world.md "Notation", "World coordinates"): the 21
// object pools and their role indices, the 0x70-byte object record, the 45 x 0x14 effect table,
// the 8 x 0x15a ship records, the image-slot table DGROUP:0x39e8, the world constants and the
// 16-bit arithmetic helpers of ADR 0013 decision 7. Portable, no Win32.
//
// Every record is a standard-layout struct whose members sit at the original's byte offsets
// (static_asserts below and in records.cpp). Field names follow structs.md; a field structs.md
// names only by its offset is called by that offset (`w1c` = word +0x1c, `b2e` = byte +0x2e,
// `d6a` = dword +0x6a), so world.md's "+0x1c" reads as `.w1c`. Words are 16-bit signed unless
// the original compares them unsigned; dwords are `Dword` (two words, alignment 2), because
// several sit at offsets that are not multiples of 4 (+0x6a, ship +0x156) and the records'
// sizes (0x70, 0x15a) are not multiples of 4 either. Far pointers (callbacks, structs.md
// +0x5e/+0x62/+0x66) are `FarPtr`, an opaque 32-bit slot: the port keeps a handler id there,
// not a code address [DESIGN ADR 0013].
//
// Header-only, so a test or a module links it without a library; records.cpp holds the
// offsetof checks of every field. Pools owns the records' storage.
// The layouts assume a little-endian host, as every port target is (x86, x86-64).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace is::world {

// --- 16-bit arithmetic (ADR 0013 decision 7; world.md "Notation") --------------------------
// The original computes in 16-bit registers: a product whose high word is dropped (`imul` then
// `cwd`, world.md's lo16), sums that wrap, and arithmetic shifts. These helpers do exactly that
// on any host, without signed-overflow undefined behaviour.

// The two's-complement 16-bit value of the low word of v.
constexpr std::int16_t to_i16(std::uint32_t v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v & 0xffffu);
    return w < 0x8000u ? static_cast<std::int16_t>(w)
                       : static_cast<std::int16_t>(static_cast<int>(w) - 0x10000);
}
// The low word of a 16 x 16 `imul` (world.md lo16(a*b)) [BYTES seg11:0x66e6 seg11:0x60ec].
constexpr std::int16_t mul16(std::int16_t a, std::int16_t b) {
    return to_i16(static_cast<std::uint32_t>(static_cast<std::int32_t>(a)) *
                  static_cast<std::uint32_t>(static_cast<std::int32_t>(b)));
}
constexpr std::int16_t lo16(std::int32_t v) { return to_i16(static_cast<std::uint32_t>(v)); }
// 16-bit `add` / `sub`, wrapping.
constexpr std::int16_t add16(std::int16_t a, std::int16_t b) {
    return to_i16(static_cast<std::uint32_t>(static_cast<std::int32_t>(a)) +
                  static_cast<std::uint32_t>(static_cast<std::int32_t>(b)));
}
constexpr std::int16_t sub16(std::int16_t a, std::int16_t b) {
    return to_i16(static_cast<std::uint32_t>(static_cast<std::int32_t>(a)) -
                  static_cast<std::uint32_t>(static_cast<std::int32_t>(b)));
}
// 16-bit `shl` (bits shifted out of the word are lost), n in 0..15.
constexpr std::int16_t shl16(std::int16_t a, int n) {
    return to_i16(static_cast<std::uint32_t>(static_cast<std::uint16_t>(a)) << n);
}
// Arithmetic right shifts (`sar`), rounding toward minus infinity, n in 0..31 / 0..15.
constexpr std::int32_t sar32(std::int32_t v, int n) {
    return v >= 0 ? (v >> n) : static_cast<std::int32_t>(~(~v >> n));
}
constexpr std::int16_t sar16(std::int16_t v, int n) {
    return static_cast<std::int16_t>(sar32(v, n));
}
// Signed division truncating toward zero (`idiv` on 16-bit operands, world.md "/"). The
// original traps on d = 0 and on -32768 / -1; callers must not pass them [OPEN].
constexpr std::int16_t idiv16(std::int16_t n, std::int16_t d) {
    return to_i16(static_cast<std::uint32_t>(static_cast<std::int32_t>(n) / d));
}
constexpr std::int16_t imod16(std::int16_t n, std::int16_t d) {
    return to_i16(static_cast<std::uint32_t>(static_cast<std::int32_t>(n) % d));
}
// The RTL long division seg5:0x110 (32-bit, truncating toward zero) [BYTES seg5:0x110].
constexpr std::int32_t div32(std::int32_t n, std::int32_t d) { return n / d; }

// --- World constants (world.md "World coordinates", "Notation") ----------------------------
constexpr std::uint16_t kDgWorldCellsX = 0x5a0;  // DGROUP:0x5a0, never written [BYTES DGROUP:0x5a0]
constexpr std::uint16_t kDgWorldCellsY = 0x5a2;  // DGROUP:0x5a2 [BYTES DGROUP:0x5a2]
constexpr std::int16_t kWorldCells = 3200;       // the value of both: world size in 1/8-unit cells
constexpr std::int16_t kWorldUnits = 25600;      // kWorldCells << 3: world size in world units
static_assert(kWorldUnits == kWorldCells * 8, "world.md World coordinates");
constexpr std::uint16_t kDgSize = 0x9156;        // DGROUP:0x9156 `Size` [BYTES seg10:0xbaa]
constexpr std::uint16_t kDgPixelShift = 0x9150;  // DGROUP:0x9150 s = 4 - Size [BYTES seg9:0x2408]
// s = 4 - Size: one pixel is 1 << s world units (8 at the default Size 1).
constexpr int pixel_shift(int size) { return 4 - size; }
constexpr int pixel_units(int size) { return 1 << pixel_shift(size); }
// A sprite cell of side c at Size: (c << Size) >> 1 pixels, 16-bit [BYTES seg10:0xbae
// seg10:0xbb1 seg10:0xbb3 seg10:0xbbd].
constexpr std::int16_t sprite_side(std::int16_t c, int size) {
    return sar16(shl16(c, size), 1);
}

// --- Little helpers for the layouts -----------------------------------------------------------
// A dword at a 2-aligned offset: low word first, as in the original's memory.
struct Dword {
    std::uint16_t lo = 0;
    std::uint16_t hi = 0;
    constexpr std::uint32_t get() const { return lo | (static_cast<std::uint32_t>(hi) << 16); }
    constexpr void set(std::uint32_t v) {
        lo = static_cast<std::uint16_t>(v);
        hi = static_cast<std::uint16_t>(v >> 16);
    }
    constexpr std::int32_t get_signed() const {
        const std::uint32_t v = get();
        return v < 0x80000000u ? static_cast<std::int32_t>(v)
                               : -static_cast<std::int32_t>(~v) - 1;
    }
};
// A far pointer slot (offset, segment). The port stores its own handler id in it [DESIGN ADR
// 0013]; 0 means none.
using FarPtr = Dword;

// --- Object record (0x70 bytes; structs.md "Object record") ----------------------------------
struct ObjectRecord {
    std::int16_t scr_x;        // +0x00 screen x [BYTES seg12:0x27ee seg7:0x11e8]
    std::int16_t scr_y;        // +0x02 screen y [BYTES seg12:0x27f3 seg7:0x11fc]
    std::int16_t x;            // +0x04 world x [BYTES seg8:0x1be0]
    std::int16_t y;            // +0x06 world y [BYTES seg8:0x1c1a]
    std::int16_t old_scr_x;    // +0x08 screen x before the last move; placement -1000 [BYTES seg8:0xb76 seg10:0x8c7]
    std::int16_t old_scr_y;    // +0x0a [BYTES seg10:0x8e4]
    std::int16_t vx;           // +0x0c world units per integrator step [BYTES seg8:0x1bdc]
    std::int16_t vy;           // +0x0e [BYTES seg8:0x1bea]
    std::int16_t src_x;        // +0x10 sprite cell x in the source DC [BYTES seg7:0x133d seg8:0xb76]
    std::int16_t src_y;        // +0x12 [BYTES seg7:0x1384 seg9:0x6624]
    std::int16_t w14;          // +0x14 [OPEN]
    std::int16_t w16;          // +0x16 [OPEN]
    std::int16_t state;        // +0x18 0 = in play; destroy writes 1 [BYTES seg12:0x2a97 seg11:0x372f]
    std::int16_t on_screen;    // +0x1a 0 while off screen [BYTES seg8:0xbc5 seg7:0x3bae seg13:0x631]
    std::int16_t w1c;          // +0x1c [OPEN]; collision test returns while not 0 [BYTES seg8:0x50d7]
    std::int16_t damage;       // +0x1e [BYTES seg7:0x2699]
    std::int16_t extent;       // +0x20 side in world units [BYTES seg10:0xbc5 seg9:0x60bc]
    std::int16_t size;         // +0x22 cell side in pixels, (side << Size) >> 1 [BYTES seg10:0xbbd]
    std::int16_t gap;          // +0x24 gap between source cells, (gap << Size) >> 1 [BYTES seg10:0xbda]
    std::int16_t angle;        // +0x26 rock spin 0..1799, pool 9 rand()%900; ship turn clears it [BYTES seg10:0x994 seg7:0x3bae seg11:0x6437]
    std::int16_t frame;        // +0x28 ship heading 0..15, icon index, rock angle/100 [BYTES seg10:0xbe5 seg7:0x132e]
    std::int16_t w2a;          // +0x2a [OPEN]
    std::int16_t max_damage;   // +0x2c [BYTES seg7:0x2678]
    std::uint8_t b2e;          // +0x2e [OPEN] alternate source: DC word +0x3c; 0xff draws nothing; 0xfe player mask [BYTES seg8:0xb76 seg9:0x6624]
    std::uint8_t flame;        // +0x2f ship flame length (thrust x mult)/20; gate posts 2 [BYTES seg11:0x6865 seg9:0x10d4]
    std::int16_t w30;          // +0x30 [OPEN]
    std::int16_t owner_pool;   // +0x32 -1 = none [BYTES seg11:0x9a19 seg8:0x5f0b seg9:0x6624]
    std::int16_t owner_rec;    // +0x34 [BYTES seg11:0x9a24 seg8:0x5f14]
    std::int16_t w36;          // +0x36 [OPEN]; -1 at wave start [BYTES seg9:0x6624]
    std::int16_t w38;          // +0x38 [OPEN]
    std::int16_t w3a;          // +0x3a [OPEN]
    std::uint16_t alt_dc;      // +0x3c DC word the sprite draw uses while b2e != 0 [BYTES seg8:0xb76]
    std::int16_t w3e;          // +0x3e [OPEN]
    std::int16_t w40;          // +0x40 [OPEN]
    std::int16_t hit_sound;    // +0x42 sound id [BYTES seg8:0x6459 seg14:0x20d3]
    std::int16_t mass;         // +0x44 m [BYTES seg11:0x6613 seg11:0x1d26 seg11:0x35f6]
    std::int16_t radar_brush;  // +0x46 index into DGROUP:0x5004 [BYTES seg11:0x3884]
    std::int16_t blast_size;   // +0x48 fragment count [BYTES seg12:0x2816]
    std::int16_t w4a;          // +0x4a [OPEN]
    Dword radar_color;         // +0x4c COLORREF for SetPixel [BYTES seg11:0x39c3 seg11:0x3cad]
    std::uint8_t b50;          // +0x50 [OPEN]; pool 4 placement writes 0 [BYTES seg9:0xdf6]
    std::uint8_t race_status;  // +0x51 pool 14 race code; pool 0 icon dissolve counter [BYTES seg4:0x26b7 seg7:0x3a42]
    std::uint8_t b52;          // +0x52 [OPEN]
    std::uint8_t b53;          // +0x53 [OPEN] 1 for icons, 0 for rocks [BYTES seg10:0xc2b seg10:0xec2]
    std::uint8_t b54;          // +0x54 [OPEN] kill bonus test; gate posts 1 [BYTES seg8:0x7c6d seg8:0x1b7e]
    std::uint8_t b55;          // +0x55 [OPEN]
    std::array<std::uint8_t, 8> r56;  // +0x56..+0x5d [OPEN]
    FarPtr on_damage;          // +0x5e cdecl (pool, record, amount) [BYTES seg8:0x647c]
    FarPtr radar_draw;         // +0x62 Pascal (pool, record, hDC) [BYTES seg11:0x375f]
    FarPtr on_destroy;         // +0x66 cdecl (pool, record) [BYTES seg7:0x26b1]
    Dword d6a;                 // +0x6a [OPEN] rock off-screen age reference [BYTES seg10:0x981 seg7:0x3bae]
    std::int16_t w6e;          // +0x6e [OPEN]
};
constexpr std::size_t kObjectRecordSize = 0x70;

// --- Effect table entry (0x14 bytes; structs.md "Effect table") ------------------------------
// Word +0x00 is a frame counter, not a kind: every creator writes 1, the frame routine
// seg8:0x3fbe adds 1 per call and frees the entry (0xffff) once it reaches word +0x02; the
// entry's type is word +0x0a (work/analysis/m4/s1-death.md section 1, verified claims 1-3)
// [BYTES seg16:0x91e seg12:0x28d8 seg8:0x4190 seg8:0x419c seg8:0x41aa]. The entry's screen
// centre and world point sit in the side arrays DGROUP:0x5170 and DGROUP:0x5224 (EffectSides).
struct EffectEntry {
    std::uint16_t frame;  // +0x00 1 at creation, +1 per seg8:0x3fbe call; 0xffff = free [BYTES seg16:0x91e seg12:0x28d8 seg8:0x4190 seg8:0x41aa]
    std::int16_t end;     // +0x02 the frame count: the entry ends when +0x00 >= +0x02 [BYTES seg16:0x939 seg12:0x291b seg8:0x419c]
    std::int16_t step;    // +0x04 step of the 5 x 5 piece loops (types >= 6) [BYTES seg8:0x4523]
    std::int16_t pool;    // +0x06 source pool [BYTES seg16:0x942 seg12:0x295f]
    std::int16_t rec;     // +0x08 source record [BYTES seg16:0x949 seg12:0x2966]
    std::int16_t type;    // +0x0a 1 secondary blast, 4 first fragment, 5 ship death entry 0, 3, 6/7 pieces, 0xff06 [BYTES seg16:0x914 seg16:0x9f1 seg12:0x28ce seg12:0x2a6f]
    std::int16_t side;    // +0x0c piece side (types >= 6) [BYTES seg8:0x4326]
    std::int16_t brush;   // +0x0e piece brush index (types >= 6) [BYTES seg8:0x42f5]
    Dword start;          // +0x10 start time on the DGROUP:0x1e50 clock; types 1 and < 0 wait for it [BYTES seg16:0x931 seg12:0x2913 seg8:0x3ffe]
};
constexpr std::size_t kEffectEntrySize = 0x14;
constexpr int kEffectCount = 45;
constexpr std::uint16_t kDgEffectTable = 0x1e90;  // DGROUP:0x1e90
constexpr std::uint16_t kEffectFree = 0xffff;
// The type word 0xff06 (-250) of seg11:0x9007: no blackout on its last frame, and its blast
// hits its own source [BYTES seg8:0x407b seg11:0x9007].
constexpr std::int16_t kEffectTypeFF06 = static_cast<std::int16_t>(-250);
using EffectTable = std::array<EffectEntry, kEffectCount>;
// seg18:0x8bc and the placement seg9:0x6624 mark all 45 entries free [BYTES seg18:0x9b2 seg9:0x6624].
inline void clear_effects(EffectTable& t) {
    for (EffectEntry& e : t) e.frame = kEffectFree;
}
// The side arrays of the effect table, indexed 4e: DGROUP:0x5170 + 4e the screen centre
// (x, y), fixed at creation, and DGROUP:0x5224 + 4e the world point, the sound position and the
// blast's centre [BYTES seg16:0x99c seg16:0x9b2 seg12:0x298a seg12:0x2a34].
struct EffectPoint {
    std::int16_t x = 0, y = 0;
};
struct EffectSides {
    std::array<EffectPoint, kEffectCount> centre{};  // DGROUP:0x5170
    std::array<EffectPoint, kEffectCount> point{};   // DGROUP:0x5224
};

// --- Ship record (0x15a bytes; structs.md "Ship records") ------------------------------------
struct ShipRecord {
    std::int16_t w00;             // +0x00 [OPEN] countdown; message 0x88 at 4 [BYTES seg4:0x7084]
    std::int16_t thrusting;       // +0x02 [BYTES seg11:0x63e2 seg11:0x6894]
    std::int16_t w04, w06, w08, w0a;  // +0x04..+0x0b [OPEN]
    std::int16_t thrust;          // +0x0c T = SHP byte +1 x 5 [BYTES seg14:0x21a9]
    std::int16_t drag;            // +0x0e D = body byte +0x1c [BYTES seg14:0x21b1]
    std::int16_t type;            // +0x10 ship-type word: sprite layout type [BYTES seg7:0x12ee]
    std::int16_t spin;            // +0x12 -1, 0, 1 [BYTES seg11:0x599b seg4:0x0]
    std::int16_t turn;            // +0x14 turn word r [BYTES seg14:0x22a2 seg13:0x616]
    std::int16_t turn_acc;        // +0x16 turn accumulator [BYTES seg13:0x616]
    std::int16_t weapon_slot;     // +0x18 [BYTES seg11:0xacc5]
    std::int16_t w1a;             // +0x1a [OPEN]
    std::int16_t thrust_mult;     // +0x1c k: OverThruster 5, decays to 1 [BYTES seg11:0x5c21 seg14:0x21bb]
    std::int16_t fuel;            // +0x1e [BYTES seg11:0x65d7 seg11:0x1cfe]
    std::int16_t w20, w22, w24;   // +0x20..+0x25 [OPEN]
    std::int16_t w26, w28, w2a, w2c;  // +0x26..+0x2d [OPEN] roster bytes +0x38..+0x3b; +0x2a gates thrust and go [BYTES seg18:0x4a7 seg11:0x662b seg11:0x5b35]
    std::int16_t w2e, w30;        // +0x2e, +0x30 [OPEN]
    std::int16_t w32;             // +0x32 [OPEN] 8 at a race flag [BYTES seg4:0x216e seg11:0x5ba5]
    std::int16_t w34;             // +0x34 [OPEN] checkpoint + 2 x gates [BYTES seg4:0x2174]
    std::int16_t w36;             // +0x36 [OPEN]
    std::int16_t w38;             // +0x38 [OPEN] go flag of a computer ship [BYTES seg11:0x5ba7]
    std::int16_t w3a;             // +0x3a [OPEN] thrust adds 2 to k when 5 [BYTES seg11:0x6613]
    std::int16_t w3c;             // +0x3c [OPEN]
    std::int16_t target_pool;     // +0x3e [BYTES seg3:0x10a]
    std::int16_t target_rec;      // +0x40 [BYTES seg3:0x111]
    std::int16_t w42, w44, w46, w48, w4a, w4c;  // +0x42..+0x4d [OPEN]
    std::int16_t design_index;    // +0x4e 0xad-byte design record index [BYTES seg3:0x208]
    std::int16_t sprite_slot;     // +0x50 [OPEN] image slot of a body-thrust ship [BYTES seg7:0x130b]
    std::int16_t w52, w54, w56, w58;  // +0x52..+0x59 [OPEN]
    std::int8_t anim_step;        // +0x5a 1 or -1 [BYTES seg13:0x2c0]
    std::uint8_t anim_last;       // +0x5b [BYTES seg13:0x2d3]
    std::uint8_t anim_frame;      // +0x5c [BYTES seg13:0x2c4]
    std::uint8_t b5d;             // +0x5d [OPEN]
    std::int16_t w5e;             // +0x5e [OPEN]
    std::uint8_t overthrust_cool; // +0x60 OverThruster cooldown [BYTES seg11:0x64b6 seg7:0xafd]
    std::uint8_t b61;             // +0x61 [OPEN]
    std::array<std::int16_t, 8> r62;   // +0x62..+0x71 [OPEN]
    std::array<std::int16_t, 8> w72;   // +0x72 one word per other ship j [BYTES seg4:0x7060]
    std::int16_t w82;             // +0x82 [OPEN]
    Dword lockout;                // +0x84 control lock-out time; seg16:0x8e4 sets now + 6000 at death, the tumble ends when it passes (locked_out) [BYTES seg11:0x5938 seg11:0x5c03 seg16:0xa2d seg13:0x14b]
    Dword d88;                    // +0x88 [OPEN] wave-start time [BYTES seg9:0x60bc]
    Dword d8c;                    // +0x8c [OPEN]
    Dword flame_until;            // +0x90 end of the 250 ms flame window [BYTES seg11:0x63e2]
    Dword d94;                    // +0x94 [OPEN] [BYTES seg11:0x9fbc]
    std::array<std::uint8_t, 0x16> r98;  // +0x98..+0xad [OPEN]
    std::array<std::uint8_t, 10> weapon_count;  // +0xae count per weapon slot [BYTES seg11:0x2a66]
    std::array<std::uint8_t, 10> weapon;        // +0xb8 weapon per slot; extent past 10 [OPEN] [BYTES seg11:0xacc9]
    std::array<std::uint8_t, 0x14> rc2;  // +0xc2..+0xd5 [OPEN]
    std::int16_t roster_index;    // +0xd6 [BYTES seg18:0x4ad]
    std::int16_t kill_value;      // +0xd8 [BYTES seg8:0x7c50]
    std::int16_t laps;            // +0xda [BYTES seg4:0x8bb7]
    std::int16_t wdc, wde, we0;   // +0xdc..+0xe1 [OPEN]
    std::uint8_t be2;             // +0xe2 `tumbling`: seg16:0x8e4 sets 1, the mover steps the heading while set; skips action-mode flight [BYTES seg16:0x9ff seg13:0xdc seg13:0x224 seg11:0x600d]
    std::uint8_t be3;             // +0xe3 [OPEN]
    std::uint8_t has_flame;       // +0xe4 0 for a body-thrust ship [BYTES seg14:0x1f41 seg11:0xb122]
    std::uint8_t be5;             // +0xe5 [OPEN]
    std::uint8_t anim_wrap;       // +0xe6 not 0 wraps, 0 bounces [BYTES seg13:0x25a]
    std::uint8_t be7;             // +0xe7 [OPEN] lamp row 4 [BYTES seg11:0x3d0e]
    std::uint8_t be8;             // +0xe8 [OPEN]
    std::uint8_t be9;             // +0xe9 [OPEN] lamp row 5, sprite row [BYTES seg11:0x3db2 seg7:0x1371]
    std::uint8_t bea;             // +0xea [OPEN]
    std::uint8_t beb;             // +0xeb [OPEN] turn input ignored while set [BYTES seg11:0x5938]
    std::uint8_t bec;             // +0xec `alive`: 1 from seg14:0x21a0, 0 from seg16:0x8e4 (dead) [BYTES seg16:0xa3d seg4:0x8c3f seg7:0x1266]
    std::uint8_t bed;             // +0xed [OPEN]
    std::array<std::uint8_t, 8> bee;     // +0xee one byte per other ship j [BYTES seg4:0x704d]
    std::array<std::uint8_t, 0x51> rf6;  // +0xf6..+0x146 [OPEN]
    std::uint8_t radar_hide;      // +0x147 [OPEN] radar leaves the ship out while set [BYTES seg11:0x37af]
    std::uint8_t checkpoint;      // +0x148 [BYTES seg4:0x215c]
    std::array<std::uint8_t, 13> r149;   // +0x149..+0x155 [OPEN]
    Dword due156;                 // +0x156 [OPEN] due time, 1500 ms ahead [BYTES seg7:0xdbc]

    // Byte +0xcb: the weapon slots in use + 1 (seg13:0x4ba0 copies slots 1 ... +0xcb - 1 and
    // sets 1) [BYTES seg13:0x4c10 seg13:0x4c51], and word +0xce, the resources the pod takes
    // [BYTES seg13:0x4c49 seg13:0x4c84]. Both sit in the reserved block rc2 (+0xc2 ... +0xd5).
    std::uint8_t& slots_cb() { return rc2[0xcb - 0xc2]; }
    std::int16_t resources() const {
        return static_cast<std::int16_t>(rc2[0xce - 0xc2] | (rc2[0xcf - 0xc2] << 8));
    }
    void set_resources(std::int16_t v) {
        rc2[0xce - 0xc2] = static_cast<std::uint8_t>(v & 0xff);
        rc2[0xcf - 0xc2] = static_cast<std::uint8_t>((static_cast<std::uint16_t>(v) >> 8) & 0xff);
    }

    // The four sign-extended roster words +0x26..+0x2c by index 0..3 [BYTES seg18:0x4a7].
    std::int16_t& roster_word(int i) {
        switch (i) {
        case 0: return w26;
        case 1: return w28;
        case 2: return w2a;
        default: return w2c;
        }
    }
};
constexpr std::size_t kShipRecordSize = 0x15a;
constexpr int kShipCount = 8;
constexpr std::uint16_t kDgShipRecords = 0x7090;  // DGROUP:0x7090; ends at DGROUP:0x7b60 [BYTES seg4:0x7084 seg7:0xdbc]
using ShipTable = std::array<ShipRecord, kShipCount>;
// DGROUP address of field `offset` of ship record i (for logs and cross-checks).
constexpr std::uint16_t ship_field_address(int i, std::uint16_t offset) {
    return static_cast<std::uint16_t>(kDgShipRecords + kShipRecordSize * i + offset);
}

// The ship-type word +0x10 (DGROUP:0x70a0) as seg7:0x1208 reads it [BYTES seg7:0x12ee seg7:0x1315
// seg7:0x131b]: types 0 and 7 pick the sprite cell by heading.
constexpr bool type_cells_by_heading(std::int16_t type) { return type == 0 || type == 7; }

// --- Image-slot table DGROUP:0x39e8 (world.md "The player's sprite"; shell.md "Image slots") ---
// One memory-DC word per image slot; parallel tables hold the slot's original bitmap
// (DGROUP:0x3974), palette (DGROUP:0x3a5c) and bitmap (DGROUP:0x3ad0) [BYTES seg10:0x552
// seg10:0x6f8e]. The tables are 0x74 bytes apart, so 58 slots each: an inference from the
// addresses, [OPEN] until a verifier confirms no slot index reaches 58.
constexpr std::uint16_t kDgImageDc = 0x39e8;
constexpr std::uint16_t kDgImageOldBitmap = 0x3974;
constexpr std::uint16_t kDgImagePalette = 0x3a5c;
constexpr std::uint16_t kDgImageBitmap = 0x3ad0;
constexpr int kImageSlots = (kDgImagePalette - kDgImageDc) / 2;  // 58 [OPEN]
static_assert(kDgImageDc - kDgImageOldBitmap == kDgImagePalette - kDgImageDc &&
                  kDgImageBitmap - kDgImagePalette == kDgImagePalette - kDgImageDc,
              "parallel image-slot tables");
constexpr int kShipImageSlotBase = 0x28;  // seg7:0x1208 uses DGROUP:0x39e8[slot + 0x28]
// The image slot of ship `ship`'s sprite: the ship index when +0xe4 != 0, else word +0x50
// [BYTES seg7:0x1208 seg7:0x12fa seg7:0x130b seg7:0x1397].
constexpr int ship_image_slot(int ship, const ShipRecord& s) {
    return (s.has_flame != 0 ? ship : s.sprite_slot) + kShipImageSlotBase;
}
using ImageSlotTable = std::array<std::uint16_t, kImageSlots>;

// --- The missile entries DGROUP:0x66e0 + 0x1c·m (M4 s2 MISSILE) -------------------------------
// One 0x1c-byte entry per pool-6 record m, 14 words (work/analysis/m4/s2-missile.md 0.1, claims
// 19-22, 29, 40-43, 49-50, 56, 72 verified). The launcher seg11:0x7c12 grows pool 6 only while
// DGROUP:0x9412 < 10, so m < 10 [BYTES seg11:0x7c2b seg11:0x7c3a seg11:0x7c45].
struct MissileSlot {
    std::int16_t target_pool = 0;   // +0x00 DGROUP:0x66e0, -1 = none [BYTES seg11:0x7f57 seg12:0x7aa]
    std::int16_t target_rec = 0;    // +0x02 DGROUP:0x66e2 [BYTES seg11:0x7f47 seg12:0x7b1]
    std::int16_t weapon = 0;        // +0x04 DGROUP:0x66e4, -1 = free [BYTES seg11:0x7ebc seg11:0x7d8b]
    std::int16_t w06 = 0;           // +0x06 DGROUP:0x66e6 [OPEN] no writer in the launch or mover
    std::int16_t launcher_pool = 0; // +0x08 DGROUP:0x66e8 [BYTES seg11:0x89cb]
    std::int16_t launcher_rec = 0;  // +0x0a DGROUP:0x66ea [BYTES seg11:0x89d2]
    std::int16_t age = 0;           // +0x0c DGROUP:0x66ec, mover passes [BYTES seg11:0x8981 seg12:0x445 seg9:0x66a0]
    std::int16_t steer_delay = 0;   // +0x0e DGROUP:0x66ee, kind 1 steers once age > it [BYTES seg12:0x156]
    std::int16_t alt_pool = 0;      // +0x10 DGROUP:0x66f0, the target of modes != 2 (seg4:0x5514, M4-later)
    std::int16_t alt_rec = 0;       // +0x12 DGROUP:0x66f2
    std::int16_t mode = 0;          // +0x14 DGROUP:0x66f4, the steering mode, 2 at launch [BYTES seg11:0x7ec4]
    std::int16_t anim = 0;          // +0x16 DGROUP:0x66f6, the animation counter [BYTES seg11:0x7ec0 seg12:0x23]
    std::int16_t w18 = 0;           // +0x18 DGROUP:0x66f8, 0 at launch [BYTES seg11:0x7eb5]
    std::int16_t lock = 0;          // +0x1a DGROUP:0x66fa, homing on [BYTES seg12:0x5e9 seg12:0x7a1 seg12:0x8e9]
};
constexpr std::size_t kMissileSlotSize = 0x1c;
constexpr int kMissileSlots = 10;
constexpr std::uint16_t kDgMissileTable = 0x66e0;

// --- The weapon table DGROUP:0x2f54 as the wave reads and writes it (M4 s2 MISSILE) -----------
// 28 blocks of 0x50 bytes (src/hud/weapons.h holds the filler seg10:0x1fb2's values; isworld
// does not link src/hud, so the program copies its blocks in, block(w) = hud::WeaponTable::
// block(w)) [DESIGN]. Words -8 ... -2 of weapon w are words +0x48 ... +0x4e of block w - 1;
// word -4 (the image slot) is written at run time by seg10:0x533a [BYTES seg10:0x5389
// seg10:0x542d], so the wave holds a mutable copy (s2-missile.md 0.2 "Port note") [DESIGN].
constexpr int kWeaponTableBlocks = 28;
class WeaponWords {
public:
    using Block = std::array<std::int16_t, 0x50 / 2>;
    // The signed word DGROUP:0x2f54 + 0x50·w + off. A word outside the table (w = -1 reads
    // DGROUP:0x2f04 + off, the star colours, in the original) reads 0 [DESIGN].
    std::int16_t word(int w, int off) const {
        const int a = w * 0x50 + off;
        if (a < 0 || a >= kWeaponTableBlocks * 0x50 || (a & 1) != 0) return 0;
        return blocks_[static_cast<std::size_t>(a / 0x50)][static_cast<std::size_t>((a % 0x50) / 2)];
    }
    void set_word(int w, int off, std::int16_t v) {
        const int a = w * 0x50 + off;
        if (a < 0 || a >= kWeaponTableBlocks * 0x50 || (a & 1) != 0) return;
        blocks_[static_cast<std::size_t>(a / 0x50)][static_cast<std::size_t>((a % 0x50) / 2)] = v;
    }
    Block& block(int w) { return blocks_[static_cast<std::size_t>(w)]; }
    const Block& block(int w) const { return blocks_[static_cast<std::size_t>(w)]; }

private:
    std::array<Block, kWeaponTableBlocks> blocks_{};
};

// --- Pools (structs.md "Object pools") -------------------------------------------------------
constexpr int kPoolCount = 21;               // DGROUP:0x1df8 holds 21 far pointers [BYTES seg6:0x1bab seg18:0x1e19]
constexpr int kStartupPools = 15;            // pools 0..14 by seg6:0x1b62 [BYTES seg6:0x1bb1]
constexpr int kStartupRecords = 10;          // 0x460 / 0x70 [BYTES seg6:0x1bb5]
constexpr std::uint32_t kStartupBytes = 0x460;
constexpr std::uint16_t kGmemFlags = 0x42;   // GMEM_MOVEABLE | GMEM_ZEROINIT [BYTES seg6:0x1bb5 seg7:0x52e2]
constexpr int kSeg18FirstPool = 15;          // pools 15..20 by seg18:0x1dd0, one record each [BYTES seg18:0x1dde]
constexpr std::uint16_t kDgPoolTable = 0x1df8;       // + 4k far pointer
constexpr std::uint16_t kDgPool14Ptr = 0x1e30;       // pool 14's entry, read directly [BYTES seg4:0x8b95]
constexpr std::uint16_t kDgPool15Ptr = 0x1e34;       // pool 15's entry, seg18 only [BYTES seg18:0xe81]
constexpr std::uint16_t kDgPoolHandles = 0x9100;     // + 2k [BYTES seg6:0x1bc2]
constexpr std::uint16_t kDgPoolAllocated = 0x92ec;   // + 2k [BYTES seg7:0x52c7]
constexpr std::uint16_t kDgPoolInUse = 0x9406;       // + 2k [BYTES seg11:0x3766]
constexpr std::uint16_t kDgShipPool = 0x9324;        // role: ship pool [BYTES seg18:0x203]
constexpr std::uint16_t kDgRadarFirstPool = 0x9326;  // role: first pool the radar draws [BYTES seg11:0x35c7]
constexpr std::uint16_t kDgPool5Role = 0x9328;       // role: pool 5 (seg7:0x40c4) [BYTES seg7:0x4146]
constexpr std::uint16_t kDgRockPool = 0x934a;        // role: rock pool [BYTES seg7:0x3bd2]
constexpr std::uint16_t kDgLastShip = 0x9316;        // ships - 1 [BYTES seg14:0xa23]
constexpr std::uint16_t kDgPlayer = 0x9646;          // iMainSmart, the player's ship record [BYTES seg18:0x203]
constexpr std::uint16_t kDgBadIndexCount = 0x9498;   // incremented on a bad index [BYTES seg12:0x2765 seg3:0x14d]

// Pool numbers of the wave (structs.md "Wave contents").
enum Pool : int {
    kPoolIcons = 0,
    kPoolRocks = 3,
    kPool4 = 4,
    kPool5 = 5,
    kPool7 = 7,
    kPoolGatePosts = 8,  // gate posts (2 per gate), then hazards
    kPool9 = 9,
    kPool10 = 10,
    kPoolGateCentres = 12,
    kPoolShips = 14,
};

// The role indices (structs.md "Pool roles").
struct PoolRoles {
    std::int16_t ships = 14;        // DGROUP:0x9324
    std::int16_t radar_first = 0;   // DGROUP:0x9326
    std::int16_t pool5 = 5;         // DGROUP:0x9328
    std::int16_t rocks = 3;         // DGROUP:0x934a
};
// seg9:0x22b4 then seg9:0x2588 (and seg18:0x3d6): 14, 0 or 1 for Demon's Gate, 5, 3
// [BYTES seg9:0x22c2 seg9:0x22cd seg9:0x22d0 seg9:0x22d6 seg9:0x2581 seg9:0x2588 seg18:0x428].
constexpr PoolRoles wave_roles(bool demons_gate) {
    return PoolRoles{14, static_cast<std::int16_t>(demons_gate ? 1 : 0), 5, 3};
}
// seg18:0x8bc: 20, 15, 17, 19 [BYTES seg18:0x973 seg18:0x979 seg18:0x97f seg18:0x985].
constexpr PoolRoles seg18_roles() { return PoolRoles{20, 15, 17, 19}; }

// The 21 pools: storage plus the parallel counts DGROUP:0x92ec (allocated) and DGROUP:0x9406
// (in use). Storage holds max(allocated, 1) records, as GlobalReAlloc does; growing zero-fills
// (GMEM_ZEROINIT), shrinking keeps the leading records. The handle array DGROUP:0x9100 has no
// counterpart: the vector is the block [DESIGN ADR 0013].
class Pools {
public:
    // seg6:0x1b62 (pools 0..14, ten zeroed records each) and seg18:0x1dd0 is not run: pools
    // 15..20 start empty until create_seg18() [BYTES seg6:0x1b62 seg18:0x1dd0].
    Pools() {
        for (int p = 0; p < kStartupPools; ++p) resize(p, kStartupRecords);
    }

    // seg18:0x1dd0: pools 15..20 with one 0x70-byte record each [BYTES seg18:0x1dde seg18:0x1e23].
    void create_seg18() {
        for (int p = kSeg18FirstPool; p < kPoolCount; ++p) resize(p, 1);
    }
    // seg18:0x1e46 frees them [BYTES seg18:0x1e77 seg18:0x1e90].
    void free_seg18() {
        for (int p = kSeg18FirstPool; p < kPoolCount; ++p) {
            store_[idx(p)].clear();
            store_[idx(p)].shrink_to_fit();
        }
    }

    std::int16_t allocated(int pool) const { return allocated_[idx(pool)]; }
    std::int16_t in_use(int pool) const { return in_use_[idx(pool)]; }
    void set_in_use(int pool, std::int16_t n) { in_use_[idx(pool)] = n; }
    // Sets the allocated count without reallocating (the counts seg9:0x22b4 writes before its
    // reallocation loop).
    void set_allocated(int pool, std::int16_t n) { allocated_[idx(pool)] = n; }
    // Reallocates pool p to max(allocated, 1) records [BYTES seg9:0x2619 seg14:0xa61].
    void realloc(int pool) {
        const int n = allocated(pool) > 1 ? allocated(pool) : 1;
        store_[idx(pool)].resize(static_cast<std::size_t>(n), ObjectRecord{});
    }
    // Sets the allocated count and reallocates.
    void resize(int pool, std::int16_t n) {
        set_allocated(pool, n);
        realloc(pool);
    }
    // seg7:0x52c7: grows pool p so record index i exists (count = i + 1 when i >= count)
    // [BYTES seg7:0x52b8 seg7:0x52be seg7:0x52c7 seg7:0x5308].
    void grow_to(int pool, std::int16_t i) {
        if (i >= allocated(pool)) {
            set_allocated(pool, add16(i, 1));
            realloc(pool);
        }
    }
    // seg7:0x1090: in-use count of pools 0..last reset to the allocated count when negative or
    // larger [BYTES seg7:0x1090 seg7:0x109b seg7:0x10aa seg7:0x10b1].
    void clamp_in_use(int last_pool) {
        for (int p = 0; p <= last_pool; ++p)
            if (in_use(p) < 0 || in_use(p) > allocated(p)) set_in_use(p, allocated(p));
    }
    // The index test at the head of seg12:0x274a: bad_index_count + 1 whenever i is at or above
    // in_use, and false when then i >= allocated, or pool = ships and i > last_ship [BYTES
    // seg12:0x2761 seg12:0x2765 seg12:0x2767 seg12:0x276b seg12:0x2777 seg12:0x277f].
    bool destroy_index_ok(int pool, std::int16_t i, std::int16_t ship_pool,
                          std::int16_t last_ship) {
        if (i < in_use(pool)) return true;
        bad_index_count = add16(bad_index_count, 1);
        if (i >= allocated(pool) || (pool == ship_pool && i > last_ship)) return false;
        return true;
    }

    // Record i of pool p; i must be below the storage size (records()).
    ObjectRecord& rec(int pool, int i) { return store_[idx(pool)][static_cast<std::size_t>(i)]; }
    const ObjectRecord& rec(int pool, int i) const {
        return store_[idx(pool)][static_cast<std::size_t>(i)];
    }
    int records(int pool) const { return static_cast<int>(store_[idx(pool)].size()); }
    bool exists(int pool) const { return !store_[idx(pool)].empty(); }

    std::int16_t bad_index_count = 0;  // DGROUP:0x9498

    // The missile entries DGROUP:0x66e0 (MissileSlot), pool 6's side table. They live beside
    // the pool storage so that every routine holding the pools reaches the one copy (the
    // launcher, the mover, the destroy seg12:0x274a, the collisions) [DESIGN]. BSS: zero at
    // start-up, as DGROUP is. m outside 0 ... 9 reads and writes a scratch entry, where the
    // original reads the next table [DESIGN].
    std::array<MissileSlot, kMissileSlots> missiles{};
    MissileSlot& missile(int m) {
        if (m < 0 || m >= kMissileSlots) return scratch_missile_ = MissileSlot{};
        return missiles[static_cast<std::size_t>(m)];
    }
    const MissileSlot& missile(int m) const {
        static const MissileSlot none{};
        if (m < 0 || m >= kMissileSlots) return none;
        return missiles[static_cast<std::size_t>(m)];
    }

private:
    MissileSlot scratch_missile_{};
    static std::size_t idx(int pool) { return static_cast<std::size_t>(pool); }
    std::array<std::vector<ObjectRecord>, kPoolCount> store_;
    std::array<std::int16_t, kPoolCount> allocated_{};  // DGROUP:0x92ec
    std::array<std::int16_t, kPoolCount> in_use_{};     // DGROUP:0x9406
};

// The record operand pool * 256 + rec that seg8:0xb76, seg8:0x1b54 and seg7:0x1208 take
// [BYTES seg8:0x1b54 seg7:0x139d].
constexpr std::int16_t pool_rec(int pool, int rec) {
    return static_cast<std::int16_t>(pool * 256 + rec);
}
constexpr int pool_of(std::int16_t pr) { return static_cast<std::uint16_t>(pr) >> 8; }
constexpr int rec_of(std::int16_t pr) { return static_cast<std::uint16_t>(pr) & 0xff; }

// --- Layout checks ---------------------------------------------------------------------------
static_assert(sizeof(Dword) == 4 && alignof(Dword) == 2, "Dword");
static_assert(sizeof(ObjectRecord) == kObjectRecordSize, "object record 0x70");
static_assert(sizeof(EffectEntry) == kEffectEntrySize, "effect entry 0x14");
static_assert(sizeof(MissileSlot) == kMissileSlotSize, "missile entry 0x1c");
static_assert(sizeof(ShipRecord) == kShipRecordSize, "ship record 0x15a");
static_assert(sizeof(ShipTable) == kShipCount * kShipRecordSize, "ship table");
static_assert(kDgShipRecords + kShipCount * kShipRecordSize == 0x7b60, "structs.md: next table at DGROUP:0x7b60");
static_assert(kStartupRecords * kObjectRecordSize == kStartupBytes, "0x460 = ten records");
static_assert(kDgPoolTable + 4 * kPoolCount == 0x1e4c, "table ends at DGROUP:0x1e4b");
static_assert(kDgPoolTable + 4 * kPoolShips == kDgPool14Ptr, "DGROUP:0x1e30 = pool 14");
static_assert(kDgPoolTable + 4 * 15 == kDgPool15Ptr, "DGROUP:0x1e34 = pool 15");

}  // namespace is::world
