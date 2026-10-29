// owner: world set-up (the single writer of this file in later stages).
// The wave's state: the part of the original's DGROUP that the wave set-up writes and the other
// wave routines read (docs/re/world.md "Wave set-up", docs/re/structs.md). One World lives for
// the whole game, as DGROUP does: pools keep their records across waves (GlobalReAlloc keeps the
// leading records), and the directory records keep what a revisit reads. Portable.
//
// Owners: the set-up (src/world/setup.*) writes it; flight (B), drawing (C) and the movers (D)
// read and write the pools, the ship records and the counters through this header. Each field
// names its DGROUP address. Fields that no stage-2 routine reads beyond the set-up are kept
// where the set-up writes them, so that a later port has the original's values.
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "formats/icon_source.h"
#include "world/records.h"

namespace is::world {

// ---- handler ids of the far-pointer slots (object +0x5e, +0x62, +0x66)
// A FarPtr slot holds handler(seg, off), the original routine's segment number and offset, so
// the dispatchers (B, C, D) switch on the routine the original would call [DESIGN ADR 0013].
constexpr std::uint32_t handler(int seg, std::uint16_t off) {
    return (static_cast<std::uint32_t>(seg) << 16) | off;
}
constexpr std::uint32_t kRadarDot = handler(11, 0x39d8);      // radar_draw seg11:0x39d8
constexpr std::uint32_t kRadarSmall = handler(11, 0x3914);    // radar_draw seg11:0x3914
constexpr std::uint32_t kRadarBig = handler(11, 0x3bae);      // radar_draw seg11:0x3bae
constexpr std::uint32_t kRadarShip = handler(11, 0x37a0);     // radar_draw seg11:0x37a0
constexpr std::uint32_t kDamageIcon = handler(7, 0x233c);     // on_damage seg7:0x233c
constexpr std::uint32_t kDamageGeneric = handler(7, 0x2652);  // on_damage seg7:0x2652
constexpr std::uint32_t kDamageRock = handler(7, 0x5090);     // on_damage seg7:0x5090
constexpr std::uint32_t kDamagePool2 = handler(8, 0x95f2);    // on_damage seg8:0x95f2
constexpr std::uint32_t kDamagePool4 = handler(12, 0xdc4);    // on_damage seg12:0xdc4
constexpr std::uint32_t kDamagePool6 = handler(7, 0x570a);    // on_damage seg7:0x570a
constexpr std::uint32_t kDamagePool7 = handler(16, 0x85e);    // on_damage seg16:0x85e
constexpr std::uint32_t kDestroyGeneric = handler(12, 0x274a);  // on_destroy seg12:0x274a
constexpr std::uint32_t kDestroyPickup = handler(7, 0x2e02);    // on_destroy seg7:0x2e02
constexpr std::uint32_t kDestroyPool11 = handler(12, 0x6a60);   // on_destroy seg12:0x6a60

// ---- the 0x297-byte directory record (formats.md "Directory records")
// The bytes of the current record DGROUP:0x69f4 + 0x297·DGROUP:0x94a2, as the set-up reads and
// writes them. The port keeps them in memory beside nav::Records, indexed by the same record
// number [DESIGN] (nav.md: the records are not saved yet).
constexpr std::size_t kDirRecordSize = 0x297;
struct DirRecord {
    std::array<std::uint8_t, kDirRecordSize> b{};
    // A new record: +0x1c0..+0x1c7 = 0xff [BYTES seg6:0x3788 seg6:0x3791].
    DirRecord() {
        for (int i = 0; i < 8; ++i) b[0x1c0 + static_cast<std::size_t>(i)] = 0xff;
    }
    std::int8_t sbyte(std::size_t off) const { return static_cast<std::int8_t>(b[off]); }
    std::uint8_t byte(std::size_t off) const { return b[off]; }
    void set_byte(std::size_t off, int v) { b[off] = static_cast<std::uint8_t>(v); }
    std::int16_t word(std::size_t off) const {
        return to_i16(static_cast<std::uint32_t>(b[off] | (b[off + 1] << 8)));
    }
    void set_word(std::size_t off, int v) {
        b[off] = static_cast<std::uint8_t>(v);
        b[off + 1] = static_cast<std::uint8_t>(static_cast<unsigned>(v) >> 8);
    }
    std::uint32_t dword(std::size_t off) const {
        return static_cast<std::uint16_t>(word(off)) |
               (static_cast<std::uint32_t>(static_cast<std::uint16_t>(word(off + 2))) << 16);
    }
    void set_dword(std::size_t off, std::uint32_t v) {
        set_word(off, static_cast<int>(v & 0xffff));
        set_word(off + 2, static_cast<int>(v >> 16));
    }
    // +0x1e2 + n/8, bit n % 8: icon number n already taken [BYTES seg9:0xac4].
    bool icon_taken(int n) const {
        const int byte_ix = idiv16(static_cast<std::int16_t>(n), 8);  // (n + (n>>15 & 7)) >> 3
        const int bit = imod16(static_cast<std::int16_t>(n), 8);
        return (b[0x1e2 + static_cast<std::size_t>(byte_ix)] & (1u << (bit & 0x1f))) != 0;
    }
};

// seg8:0x285a (the DGROUP:0x9130 = 0 path): the distance from (x1, y1) to (x2, y2) in 1/8-unit
// cells across the world's wrap: dx = (x1 >> 3) - (x2 >> 3), moved by the world size when |dx|
// exceeds half of it (DGROUP:0x5a0 >> 1), the same for dy; then one Newton step of the square
// root from the estimate max + min / 2: ((dx^2 + dy^2) / e + e) / 2, 1 when e is 0. dx and dy
// are returned through the out parameters [BYTES seg8:0x285a].
std::int16_t distance_cells(std::int16_t x1, std::int16_t y1, std::int16_t x2, std::int16_t y2,
                            std::int16_t& dx, std::int16_t& dy, std::int16_t cells_x = kWorldCells,
                            std::int16_t cells_y = kWorldCells);

class World;
// seg3:0x65e's code-3 table test on pool-8 record rec: word DGROUP:0x3d6c + 0x34·rec (the kind,
// +0x1c of the side table) = 2 and word DGROUP:0x3d5a + 0x34·rec (+0x0a, the class) > 0
// (world.md "Key messages", "Suit test"; M3 s3 fixer, KEY-14) [BYTES seg3:0x6a2-0x6b8].
bool gate_post_live(const World& w, int rec);

// ---- the icon records DGROUP:0x57a8 + 0x23·s (world.md "Programs and icons")
struct IconRecord {
    std::int16_t w00 = 0;        // DGROUP:0x57a8 + 0x23s, cleared at activation
    std::int16_t w02 = 0;        // DGROUP:0x57aa
    std::int16_t number = 0;     // DGROUP:0x57ae: the global icon number, -1 when unused
    std::uint8_t b0a = 0;        // DGROUP:0x57b2, 0xff at activation
    std::uint8_t b0b = 0;        // DGROUP:0x57b3, 0xff at activation
    std::string name;            // DGROUP:0x57b4: the found file name (8.3, at most 13 bytes)
    std::int16_t w19 = 0;        // DGROUP:0x57c1, cleared at activation
};
constexpr int kIconRecords = 8;  // pool 0 holds 8 records [BYTES seg9:0x24b5]

// One 32x32 cell of the icon strip DGROUP:0x3a02 at (38·s + 6, 6) [BYTES seg9:0x1d2].
struct IconSlot {
    enum class Kind { kBlack, kIcon, kPlaceholder };
    Kind kind = Kind::kBlack;
    fmt::IconCell cell;  // kIcon: DrawIcon over black
    // kPlaceholder: seg9:0x352(dc, 38s + 6, flag, 32, 6, name) with the name cut by seg9:0x278.
    int flag = 0;
    std::string name;
};
using StripCell = IconSlot;
constexpr int kStripCell = 32, kStripGap = 6;  // 38·N + 6 wide, 44 high [BYTES seg10:0x5b3d]
// The strip x of slot s: 38·s + 6 [BYTES seg9:0x1d2].
constexpr int strip_x(int s) { return (kStripCell + kStripGap) * s + kStripGap; }

// ---- gates DGROUP:0x6678 + 0x1c·p (world.md "Gates")
struct Rect16 {
    std::int16_t l = 0, t = 0, r = 0, b = 0;
};
struct GateRecord {
    Rect16 beam;               // +0x00 DGROUP:0x6678
    std::int16_t width = 0;    // +0x0a DGROUP:0x6682, 1200
    std::int16_t w0c = 0;      // +0x0c DGROUP:0x6684 [OPEN]
    std::int16_t w0e = 0;      // +0x0e DGROUP:0x6686, 80
    std::int16_t post = 0;     // +0x10 DGROUP:0x6688 = 2p
    std::int16_t w12 = 0;      // +0x12 DGROUP:0x668a: 1 for p = 1, 2 for p = 2
};
constexpr int kMaxGates = 4;  // DGROUP:0x9318 is at most 4 [BYTES seg9:0x4496-0x44f8]

// ---- the pool-8 side table DGROUP:0x3d50 + 0x34·k, words by offset (world.md "Gates")
struct Pool8Entry {
    std::array<std::int16_t, 0x1a> w{};
    std::int16_t& at(int off) { return w[static_cast<std::size_t>(off / 2)]; }
    std::int16_t at(int off) const { return w[static_cast<std::size_t>(off / 2)]; }
    // +0x0a (DGROUP:0x3d5a) class, +0x1c (DGROUP:0x3d6c) kind, +0x30 (DGROUP:0x3d80),
    // +0x32 (DGROUP:0x3d82) special-place index.
};
constexpr int kPool8Max = 80;  // the pool-8 cap [BYTES seg9:0x2563]

// ---- special places DGROUP:0x4f2c + 0x10·k, k < 10, DGROUP:0x9344 of them in use; what they
// are is [OPEN] (seg9:0x4896 makes them, the hazards keep their distance) [BYTES seg9:0x3202
// seg9:0x4896 seg9:0x10d4].
struct SpecialPlace {
    std::int16_t first = 0;   // +0x0 DGROUP:0x4f2c: first pool-8 record of the place
    std::int16_t end = 0;     // +0x2 DGROUP:0x4f2e: one past its last record
    std::int16_t radius = 0;  // +0x4 DGROUP:0x4f30
    std::int16_t w6 = 0;      // +0x6 DGROUP:0x4f32
    std::int16_t w8 = 0;      // +0x8 DGROUP:0x4f34
    std::int16_t x = 0;       // +0xa DGROUP:0x4f36
    std::int16_t y = 0;       // +0xc DGROUP:0x4f38
    std::int16_t we = 0;      // +0xe DGROUP:0x4f3a
};

// Beam slot j of the 0x94-byte table DGROUP:0x6098 (world.md "Update loop" step 4): the two
// words the placement writes. Weapons are M4.
struct BeamSlot {
    std::int16_t limit = 0;  // +0x74 DGROUP:0x610c, 50 at the placement
    std::int16_t index = 0;  // +0x76 DGROUP:0x610e, -1 at the placement
};

// A star of seg8:0x190e (DGROUP:0x2398 + 4k, colours DGROUP:0x2f04 + 4k, bytes
// DGROUP:0x91d4 + k, DGROUP:0x91e6 + k, DGROUP:0x91c2 + k) [BYTES seg8:0x190e].
struct Star {
    std::int16_t x = 0, y = 0;
    std::uint32_t colour = 0;  // 0x02bbggrr: palette-relative RGB(0xff, g, b) [BYTES seg8:0x190e]
    std::uint8_t speed = 0;    // DGROUP:0x91d4 + k: rand() % 3 + 1
    std::uint8_t phase = 0;    // DGROUP:0x91e6 + k: rand() % 40
    std::uint8_t on = 0;       // DGROUP:0x91c2 + k: 1
};

// The values Enter hands to the wave set-up (nav.md "Values handed to the wave set-up").
struct WaveHandover {
    std::string directory;     // DGROUP:0x38c8 ("D:\GAMES\") or a built-in level's name
    std::int16_t demons_gate = 0;  // DGROUP:0x92e8
    std::int16_t race = 0;         // DGROUP:0x94e
    std::int16_t duel = 0;         // DGROUP:0x950
    std::int16_t difficulty = 0;   // DGROUP:0x94a8
    std::int16_t opponent = 0;     // DGROUP:0x94aa
    std::int16_t laps = 0;         // DGROUP:0x9ea0
};

// Raw access to a record's bytes at the original's offsets (the records are standard layout at
// the original's offsets, records.cpp), for the fields records.h keeps as reserved arrays.
std::int16_t word_at(const ShipRecord& s, int off);
void set_word_at(ShipRecord& s, int off, int v);
std::int8_t sbyte_at(const ShipRecord& s, int off);
void set_byte_at(ShipRecord& s, int off, int v);
std::int16_t word_at(const ObjectRecord& r, int off);
void set_word_at(ObjectRecord& r, int off, int v);

class World {
public:
    // ---- objects
    Pools pools;              // DGROUP:0x1df8 with the counts DGROUP:0x92ec and DGROUP:0x9406
    PoolRoles roles;          // DGROUP:0x9324, 0x9326, 0x9328, 0x934a
    ShipTable ships{};        // DGROUP:0x7090
    EffectTable effects{};    // DGROUP:0x1e90
    std::int16_t player = 0;      // DGROUP:0x9646
    std::int16_t last_ship = 0;   // DGROUP:0x9316

    // ---- hand-over and directory
    WaveHandover handover;
    std::int16_t added = 0;       // DGROUP:0x94b0: Enter added the directory's record
    std::int16_t record = 0;      // DGROUP:0x94a2: the current record's index
    std::vector<DirRecord> records;  // DGROUP:0x69f4, beside nav::Records
    DirRecord& dir() {
        if (records.size() <= static_cast<std::size_t>(record)) records.resize(static_cast<std::size_t>(record) + 1);
        return records[static_cast<std::size_t>(record)];
    }
    std::string program_path;     // DGROUP:0x8f7a, INI Path with its '\'
    std::int16_t drive_offset = 0;  // DGROUP:0x94ac

    // ---- settings the set-up reads
    std::int16_t size = 1;        // DGROUP:0x9156 Size
    std::int16_t shift = 3;       // DGROUP:0x9150 = 4 - Size
    std::int16_t W = 0;           // DGROUP:0x90ea
    std::int16_t H = 0;           // DGROUP:0x90ec
    std::int16_t detail = 3;      // DGROUP:0x9208 Detail
    std::int16_t complex = 0;     // DGROUP:0x8ff8 Complex
    std::int16_t w9214 = 0;       // DGROUP:0x9214 [OPEN] pool-4 variant
    std::int16_t w740 = 0;        // DGROUP:0x740 [OPEN] small-rock gap switch
    std::int16_t play_mode = 0;   // DGROUP:0x9134 PlayMode
    std::int16_t player_game = 1; // DGROUP:0x95bc
    std::int16_t restored = 0;    // DGROUP:0x92e4
    std::int16_t star_count = 0;  // DGROUP:0x9152

    // ---- the generator and the counts (world.md "Difficulty inputs", "Pool counts")
    std::int16_t wave = 0;        // DGROUP:0x91f8, completed waves
    std::int16_t budget = 0;      // DGROUP:0x932a
    std::int16_t w9000 = 0;       // DGROUP:0x9000
    std::int16_t kind = 0;        // DGROUP:0x9320 wave kind (6 race, 7 duel)
    std::int16_t course = 0;      // DGROUP:0x9322
    std::int16_t opponents = 0;   // DGROUP:0x9338
    std::int16_t w9336 = 0;       // DGROUP:0x9336
    std::int16_t score_level = 0; // DGROUP:0x9334 (seg9:0x484c, from the rating DGROUP:0x5078)
    std::uint32_t rating = 0;     // DGROUP:0x5078, the rating (globals.md "Scoring")
    std::uint32_t score = 0;      // DGROUP:0x1e58, the score
    std::int16_t w932c = 0;       // DGROUP:0x932c
    std::int16_t w9332 = 0;       // DGROUP:0x9332
    std::int16_t w9344 = 0;       // DGROUP:0x9344 special places
    std::int16_t w9346 = -1;      // DGROUP:0x9346
    std::int16_t w9432 = 0;       // DGROUP:0x9432
    std::int16_t w964a = 0;       // DGROUP:0x964a
    std::int16_t w967c = 0;       // DGROUP:0x967c
    std::int16_t w94ca = 0;       // DGROUP:0x94ca
    std::int16_t w94a6 = 0;       // DGROUP:0x94a6
    std::int16_t w93ac = 0;       // DGROUP:0x93ac
    std::int16_t wa1cd = 0;       // DGROUP:0xa1cd [OPEN meaning]
    std::int16_t wa1cf = 0;       // DGROUP:0xa1cf [OPEN meaning]
    std::array<std::uint8_t, 4> ba193{};  // bytes DGROUP:0xa193 ... 0xa196 [OPEN meaning]
    std::int16_t w920c = 0;       // DGROUP:0x920c edition
    std::int16_t w992 = 0;        // DGROUP:0x992
    std::int16_t w994 = 0;        // DGROUP:0x994
    std::array<std::int16_t, 6> w986{};  // DGROUP:0x986..0x990 (0x98a, 0x98c, 0x98e, 0x990)
    std::array<std::int16_t, 5> categories{};  // DGROUP:0x933a, 0x933c, 0x933e, 0x9340, 0x9342
    std::int16_t gates = 0;       // DGROUP:0x9318
    std::int16_t pool4 = 0;       // DGROUP:0x940e (= DGROUP:0x92f4)
    std::int16_t pool2_units = 0; // DGROUP:0x932e
    std::int16_t small_rocks = 0; // DGROUP:0x92ea
    std::int16_t w94e2 = 0;       // DGROUP:0x94e2
    std::int16_t w9eb4 = 0;       // DGROUP:0x9eb4 race started
    std::array<std::int16_t, 2> hazard_class{};  // DGROUP:0x4ed4 [OPEN]
    std::array<std::int16_t, 2> w4ee0{};          // DGROUP:0x4ee0
    std::array<std::int16_t, 2> w4ee4{};          // DGROUP:0x4ee4
    std::array<std::int16_t, 2> w4ee8{};          // DGROUP:0x4ee8
    std::array<std::int16_t, 24> w4eec{};         // DGROUP:0x4eec, 8 x 3
    std::array<std::int16_t, 8> w4f1c{};          // DGROUP:0x4f1c
    std::array<std::uint8_t, 13> w1822{};         // DGROUP:0x1822 + k
    // The hazard class table DGROUP:0x4d90 + 0x14·k, k = 1..12: filled by code outside the
    // set-up [OPEN]; zero until a port fills it.
    std::array<std::array<std::uint8_t, 0x14>, 13> classes{};

    // ---- icons (world.md "Programs and icons")
    std::array<IconRecord, kIconRecords> icons;  // DGROUP:0x57a8
    std::array<IconSlot, kIconRecords> strip;    // DGROUP:0x3a02 cells
    std::int16_t icon_number = 0;   // DGROUP:0x9200
    std::int16_t icons_placed = 0;  // DGROUP:0x9202
    std::int16_t icon_limit = 8;    // DGROUP:0x9204
    std::int16_t icon_tries = 0;    // DGROUP:0x9206
    std::int16_t icons_a = 0;       // DGROUP:0x91fa
    std::int16_t icons_b = 0;       // DGROUP:0x91fc (the icon column exists while not 0)
    std::int16_t icons_c = 0;       // DGROUP:0x91fe (the briefing's and the column's rows)

    // ---- gates, pool-8 table, special places
    std::array<GateRecord, kMaxGates> gate{};
    std::array<Pool8Entry, kPool8Max> pool8{};
    std::array<SpecialPlace, 10> places{};

    // ---- placement (world.md "Placement")
    std::int16_t placed = 0;      // DGROUP:0x93b0 latch
    std::array<BeamSlot, 9> beams{};
    std::uint32_t clock = 0;      // DGROUP:0x1e50 at the placement
    std::uint32_t frames = 0;     // DGROUP:0x1e68
    std::uint32_t w1e6c = 0;      // DGROUP:0x1e6c = clock + 10000
    std::int16_t w9510 = 0;       // DGROUP:0x9510
    std::int16_t w9e49 = 0;       // DGROUP:0x9e49
    std::int16_t w9600 = 0;       // DGROUP:0x9600 pool-4 mover on
    std::int16_t w9140 = 0;       // DGROUP:0x9140

    // ---- wave start (world.md "Ships at wave start")
    std::vector<Star> stars;      // seg8:0x190e
    std::int16_t w913e = 0;       // DGROUP:0x913e
    std::int16_t w0a76 = 0;       // DGROUP:0xa76 target display mode, 2
    std::int16_t w934e = 0;       // DGROUP:0x934e frames since the wave start (ESC needs > 2)

    // ---- further set-up state (src/world/setup.cpp)
    std::int16_t strip_slots = 0;  // N of the strip seg10:0x5b30 created (the pool-0 in-use count)
    std::int16_t w9348 = 0;        // DGROUP:0x9348 (race)
    std::int16_t w952e = -1;       // DGROUP:0x952e
    std::int16_t w9ea2 = 0, w9ea4 = 0, w9ea6 = 0;  // DGROUP:0x9ea2, 0x9ea4, 0x9ea6 race counts
    std::int16_t w9430 = 0;        // DGROUP:0x9430 effects in use
    std::int16_t w9434 = 0;        // DGROUP:0x9434 last effect index
    std::int16_t w9456 = 0;        // DGROUP:0x9456
    std::int16_t w9130 = 0;        // DGROUP:0x9130 seg18 mode
    std::int16_t w913a = 0;        // DGROUP:0x913a
    std::int16_t w913c = 0;        // DGROUP:0x913c
    std::array<std::int16_t, 20> w58cc{};  // DGROUP:0x58cc + 0x14 k, -1 at the placement
    std::array<std::int16_t, 4> w226c{};   // DGROUP:0x226c + 0x5a k, -1 at the placement
    std::array<std::int16_t, 4> w8e34{};   // DGROUP:0x8e34 + 2 k, -1 at the briefing entry
    std::array<std::int16_t, 8> w8680{};   // DGROUP:0x8680 + 2 k, -1 at the briefing entry
    std::array<std::int16_t, 8> w937a{};   // DGROUP:0x937a, cleared by the load
    // The particle burst DGROUP:0x5b2c, 10 x 4 entries of 0x16 bytes: the dword (x, y) is
    // (-100, -100) and +8 (DGROUP:0x5b34) 0 at the placement [BYTES seg9:0x6624].
    struct Particle {
        std::int16_t x = 0, y = 0, w08 = 0;
    };
    std::array<Particle, 40> particles{};
    // The race tables (M6) [OPEN]: bytes DGROUP:0x9eb6, 0x9eb9, 0x9ebc, 0x9ebf + course; words
    // DGROUP:0xa120/0xa122 + 4 course (the start), DGROUP:0x9ea8 + 4 course.
    std::array<std::int8_t, 3> race_9eb6{}, race_9eb9{}, race_9ebc{}, race_9ebf{};
    std::array<std::int16_t, 6> race_a120{};
    std::array<std::int16_t, 6> race_9ea8{};

    // The far pool-12 record 0 position: the first gate's centre.
    std::int16_t gate_centre_x() const { return pools.rec(kPoolGateCentres, 0).x; }
    std::int16_t gate_centre_y() const { return pools.rec(kPoolGateCentres, 0).y; }
    ObjectRecord& ship_object(int i) { return pools.rec(roles.ships, i); }
    ShipRecord& ship(int i) { return ships[static_cast<std::size_t>(i)]; }
};

// The world's 16-bit Win16 rectangle tests (USER.PtInRect, USER.IntersectRect, USER.SetRect):
// half-open, signed 16-bit [REF Win16 SDK].
inline bool pt_in_rect(const Rect16& r, std::int16_t x, std::int16_t y) {
    return x >= r.l && x < r.r && y >= r.t && y < r.b;
}
inline bool intersect(const Rect16& a, const Rect16& b) {
    const std::int16_t l = a.l > b.l ? a.l : b.l, r = a.r < b.r ? a.r : b.r;
    const std::int16_t t = a.t > b.t ? a.t : b.t, bt = a.b < b.b ? a.b : b.b;
    return l < r && t < bt;
}
inline Rect16 rect16(int l, int t, int r, int b) {
    return Rect16{lo16(l), lo16(t), lo16(r), lo16(b)};
}

}  // namespace is::world
