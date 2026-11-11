// RING OF DEATH, seg11:0x8a42(pool, rec) (work/analysis/m4/s7-ring.md section 1 with
// s7-ring-verify.md: claims 1-5, 7-9 CONFIRMED, claim 6 CORRECTED; Omissions O1-O4). M4 stage 7,
// track RING.
//
// The routine only fills effect-table entries: 24 entries of type 1 on a ring of radius R = 80
// round the user's top-left point and two of type 0xff06 at that point. The effect frame
// seg8:0x3fbe and the blast seg8:0x5ab0 (effects.h effect_frame, blast) do the rest: each entry
// plays KABOOM seg10:0x754a(0x45, 0, 0, 3) at its world point on its first frame after its
// start (O1); a type-1 entry draws the fireballs and never blasts; a 0xff06 entry draws no
// fireball and blasts once, at its frame 6, at the world point fixed here, skipping its source,
// and only while DGROUP:0x1318 <= 6 (claims 8, 9) [BYTES seg8:0x400f-0x4037 seg8:0x41bc
// seg8:0x4230-0x4250 seg8:0x407b-0x4083 seg8:0x5aec seg8:0x5ce3-0x5cf8]. No rand(), no sound, no
// drawing, no slot count of its own (O3).
//
// Callers: the weapon system's case 0 seg11:0xad5a with (DGROUP:0x9324, DGROUP:0x9646) (the
// LAUNCHED track's MissileHost::ring_of_death); the computer pilots' seg4:0x591c (M5) and the
// pool-8 weapon dispatcher seg12:0x6dea's seg12:0x6e6c with pool 8 (O4): a user outside the
// ship pool passes no gate, pays nothing and searches from entry 0.
//
// 16-bit arithmetic as the original's (ADR 0013 decision 7). Portable, no Win32.
#pragma once

#include <cstdint>

#include "world/records.h"
#include "world/world.h"

namespace is::world {

// The routine's constants [BYTES seg11:0x8a50 seg11:0x8a55 seg11:0x8a91 seg11:0x8ab2
// seg11:0x8aaf seg11:0x8af2 seg11:0x8f4d seg11:0x8f54 seg11:0x8f58 seg11:0x8b4c seg11:0x9007
// seg11:0x902d].
constexpr int kWeaponRingOfDeath = 1;          // RING OF DEATH [BYTES ENG_LANG.DLL STRING 12] id 187
constexpr std::int16_t kRingRadius = 80;       // R, [bp-4]
constexpr std::int16_t kRingStepMs = 30;       // T, [bp-0x1a]
constexpr std::int16_t kRingFuelGate = 70;     // fuel > 70 to fire (signed `jle`)
constexpr std::int16_t kRingFuelFactor = -70;  // fuel += lo16((-70·mass) / 250)
constexpr std::int32_t kRingFuelDivisor = 250;
constexpr std::int16_t kRingFirstA = 11, kRingStepA = 21;
constexpr std::int16_t kRingEntryFrames = 10;   // type 1: +0x02 = 10
constexpr std::int16_t kRingCentreFrames = 13;  // type 0xff06: dword +0x00 = 0x000d0001

// What one call placed, for tests and logs.
struct RingResult {
    bool acted = false;      // the gate passed
    std::int16_t start = 0;  // the first search index l (the gate's slot, 0 for another pool)
    int ring = 0;            // type-1 entries placed (24 at most)
    int centre = 0;          // type-0xff06 entries placed (2 at most)
    std::int16_t cost = 0;   // the fuel added (negative), 0 for another pool
};

// seg11:0x8a42(pool, rec), cdecl [BYTES seg11:0x8a42-0x9077]:
//  1. Gate: for the ship pool (DGROUP:0x9324) the first slot l in 1 ... sext8(ship byte +0xcb)
//     - 1 holding weapon 1 decides: fuel word +0x1e > 70 (signed) acts, else nothing; no such
//     slot, nothing. Any other pool acts (claim 1).
//  2. Cost, ship pool only: fuel = lo16(fuel + lo16(seg5:0x110(-70 · mass, 250))), mass the
//     pool-14 object word +0x44 (through DGROUP:0x1e30), the 16 × 16 `imul` and the 32-bit
//     divide; no floor (claim 2).
//  3. The ring: for a = 11, 32, 53 (while 800 / 14 = 57 > a), b = lo16(seg8:0x1290(lo16(6400 -
//     a²), 80 - a / 4)) = 79, 73, 60; eight entries per a in the code order of claim 4 at the
//     world points (x ± 8u, y ± 8v) and screen centres (scr_x ± (u << Size) >> 1, scr_y ± (v <<
//     Size) >> 1) of the user's own +0x04/+0x06 and +0x00/+0x02 (no centring), (u, v) = (a, b)
//     or (b, a): type 1, frame 1, count 10, pool, rec, start = now + sext(lo16(30·m)) (claims 3,
//     4); DGROUP:0x9430 + 1 each (claim 7); DGROUP:0x9434 is not raised (O2).
//  4. The two centre entries k = 0, 1: the user's points, type 0xff06, DGROUP:0x9434 =
//     max(DGROUP:0x9434, index), frame 1, count 13, start = now + 360·k (15 - j with j = 3 after
//     the loop) (claim 5), DGROUP:0x9430 + 1 each.
//  The entry search (claim 6 as corrected): the first free entry (+0x00 = 0xffff) at an index
//  >= the running start, below 45; the start begins at l and becomes the stopping index + 1
//  after every search, a failed one too (45 -> 46), so once the table is full from the current
//  index every later search of the call fails.
// `now` is DGROUP:0x1e50; Size, the ship pool and the player's roles come from the World.
RingResult ring_of_death(World& w, EffectSides& sides, std::uint32_t now, int pool, int rec);

}  // namespace is::world
