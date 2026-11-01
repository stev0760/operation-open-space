// The materialise effect of a ship's entry (work/analysis/m5/s1-sprite.md item 6): the
// per-object buffer seg12:0x2556 and its free seg12:0x26a4, the effect start seg7:0x13b2 and
// the frame's step seg7:0x158e, which copies 10 random 2 x 2 blocks a frame from the ship's
// sheet cell into the buffer that seg8:0xb76 draws while object byte +0x2e = 2. Only the
// object pools (< 15) are ported; the pool-20 ... 25 effects of the same table are [OPEN].
// Portable, no Win32 (M5 s1 SPRITE).
#pragma once

#include <array>
#include <cstdint>

#include "shell/rng.h"
#include "world/records.h"
#include "world/sprite.h"

namespace is::world {

// One slot of the table at DGROUP:0x221c, stride 0x5a, four slots: 0x50 bytes of block bits
// (the port bounds them at 0x50 [DESIGN]; the original's run on for n * n > 640),
// then the pool +0x50 (DGROUP:0x226c, -1 = free), the record +0x52 (0x226e), the side n in
// blocks +0x54 (0x2270), n² +0x56 (0x2272) and the blocks done +0x58 (0x2274) [BYTES
// seg7:0x1494 seg7:0x14bd seg7:0x14cb seg7:0x14d8 seg7:0x14f6 seg7:0x1c97 seg7:0x1cb2].
struct EntrySlot {
    std::array<std::uint8_t, 0x50> bits{};
    std::int16_t pool = -1;
    std::int16_t rec = 0;
    std::int16_t n = 0;
    std::int16_t total = 0;
    std::int16_t count = 0;
};
constexpr int kEntrySlots = 4;
using EntryTable = std::array<EntrySlot, kEntrySlots>;
constexpr std::uint16_t kDgEntryTable = 0x221c;
// Who marks the four slots free before the first entry is [OPEN]; the port frees them at each
// wave start (src/hud/wave_win32.cpp) [DESIGN].
void clear_entries(EntryTable& t);

// The routines the effect calls outside the drawing; the defaults do nothing.
class EntryHooks {
public:
    virtual ~EntryHooks() = default;
    // seg10:0x754a(id, pool, rec, arg): 0x27 at the start [BYTES seg7:0x13f2], 0x65 at the end
    // for the player [BYTES seg7:0x1ab2].
    virtual void sound(int id, int pool, int rec, int arg) { (void)id, (void)pool, (void)rec, (void)arg; }
    // seg2:0x170e, the menu items' enable state, after a ship's effect [BYTES seg7:0x1ae1].
    virtual void menu_refresh() {}
    // The rest of a mode-1 effect's end (the vanish of a destroyed object: object `state` 1,
    // seg7:0x65aa, byte +0x2e = 0xff, seg12:0x3778, seg7:0x2080, or +0x2e = 0xfe for a ship with
    // word +0x2e = 2) [BYTES seg7:0x181e-0x19ff] [OPEN M4].
    virtual void vanished(int pool, int rec) { (void)pool, (void)rec; }
};

// Everything the effect reads and writes.
struct EntryContext {
    Pools& pools;
    ShipTable& ships;
    EntryTable& table;
    Blitter& out;
    shell::Rng& rng;
    std::int16_t& dg_9148;              // DGROUP:0x9148, the effects running
    std::int16_t size_level = 1;        // DGROUP:0x9156 `Size`
    std::int16_t ship_pool = kPoolShips;  // DGROUP:0x9324
    std::int16_t player = 0;            // DGROUP:0x9646
    DcId window_dc = kDcWindow;         // DGROUP:0x13dc
    EntryHooks* hooks = nullptr;
};

// The block side of the effect: (2 << Size) >> 1 [BYTES seg7:0x1b6b seg7:0x1bc4].
std::int16_t entry_block(std::int16_t size_level);
// n, the cell's side in blocks: (size mod ((2 << Size) >> 1) != 0) + (size >> Size)
// [BYTES seg7:0x1417 seg7:0x1435].
std::int16_t entry_blocks(std::int16_t size, std::int16_t size_level);

// seg12:0x2556(pool, rec) [BYTES seg12:0x2556-0x26a1]: for a pool other than 1 whose object has
// no buffer DC +0x3c, a (size + 2 gap)² image in +0x3c (the port's one id stands for the DC
// +0x3c and the bitmap +0x3e, both written) [DESIGN ADR 0013], pool 7 record 0's +0x28 = 5
// first, and the square at ((size + gap) heading, r) of the DC word +0x3a copied in, r = size +
// gap for a ship (pool DGROUP:0x9324) whose type word +0x10 is above 1, else 0.
void entry_buffer(EntryContext& c, int pool, int rec);
// seg12:0x26a4(pool, rec) [BYTES seg12:0x26a4-0x2749]: the buffer freed, +0x3c and +0x3e = 0.
void entry_buffer_free(EntryContext& c, int pool, int rec);

// seg7:0x13b2(mode, pool, rec) for pool < 15 [BYTES seg7:0x13b2-0x158c]: returns at once when
// object byte +0x2e > 0 (signed); sound 0x27; takes the first free slot (pool -1) of four and
// stores pool, rec, n, n², 0; object +0x2e = mode; ship byte +0xeb = 1 for pool 14; the bits
// cleared; DGROUP:0x9148 + 1; mode 2 blacks the buffer. With no free slot the object's screen
// square is blacked on the window instead [BYTES seg7:0x1580]. A pool of 15 or more (the
// effects of pools 20 ... 25) is [OPEN] and does nothing here.
void entry_start(EntryContext& c, std::uint8_t mode, int pool, int rec);

// seg7:0x158e [BYTES seg7:0x158e-0x1d0a]: for each slot in use (pool < 15), 10 picks; each draws
// rand() % n² again while that block's bit is set, sets it and counts it. While count < n² the
// block (x, y) = ((r mod n) << Size, (r / n) << Size) is copied from the DC +0x3a at
// ((size + gap) heading + gap + x, gap + y) to the buffer at (gap + x, gap + y), or blacked
// there in mode 1. The pick that reaches n² ends the effect instead, and seg7:0x158e returns:
// mode 2 blits the buffer to the window at the object's square when +0x1a != 0, from
// ((size + gap) heading + gap, gap) of the buffer as the original does, clears +0x2e, plays 0x65
// for the player unless ship word +0x2e = 2, frees the buffer, clears ship byte +0xeb (pool 14)
// with seg2:0x170e, frees the slot and takes 1 from DGROUP:0x9148.
void entry_step(EntryContext& c);

}  // namespace is::world
