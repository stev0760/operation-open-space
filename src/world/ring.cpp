// RING OF DEATH seg11:0x8a42 (ring.h). M4 stage 7, track RING. Each step cites the original's
// addresses; the arithmetic is 16-bit where the original's is (ADR 0013 decision 7).
#include "world/ring.h"

#include <array>
#include <cstddef>

#include "world/ambulance.h"
#include "world/collide.h"

namespace is::world {

namespace {

constexpr std::size_t ix(int k) { return static_cast<std::size_t>(k); }

// One of the eight placements of an iteration, in code order [BYTES seg11:0x8b02-0x8b77
// seg11:0x8b8a-0x8c02 seg11:0x8c15-0x8c8d seg11:0x8ca0-0x8d16 seg11:0x8d29-0x8da1
// seg11:0x8db4-0x8e2c seg11:0x8e3f-0x8eb7 seg11:0x8eca-0x8f42] (verified claims 3, 4): the x
// offset's sign and whether x takes a (y then takes b) or b (y takes a), the y offset's sign, and
// the start multiplier m = base + sign·j.
struct RingBlock {
    std::int16_t sx;      // +1 / -1
    bool x_takes_a;       // x ± 8a, y ± 8b; else x ± 8b, y ± 8a
    std::int16_t sy;
    std::int16_t m_base;  // [BYTES seg11:0x8b52 seg11:0x8bda seg11:0x8c65 seg11:0x8cea seg11:0x8d79 seg11:0x8e04 seg11:0x8e8f seg11:0x8f1a]
    std::int16_t m_sign;
};
constexpr std::array<RingBlock, 8> kRingBlocks = {{
    {+1, true, -1, 0, +1},    // 1: (X + A8, Y - B8), m = j
    {+1, false, -1, 5, -1},   // 2: (X + B8, Y - A8), m = 5 - j
    {+1, false, +1, 5, +1},   // 3: (X + B8, Y + A8), m = j + 5
    {+1, true, +1, 10, -1},   // 4: (X + A8, Y + B8), m = 10 - j
    {-1, true, -1, 20, -1},   // 5: (X - A8, Y - B8), m = 20 - j
    {-1, false, -1, 15, +1},  // 6: (X - B8, Y - A8), m = j + 15
    {-1, false, +1, 15, -1},  // 7: (X - B8, Y + A8), m = 15 - j
    {-1, true, +1, 10, +1},   // 8: (X - A8, Y + B8), m = j + 10
}};

std::int16_t signed_add(std::int16_t base, std::int16_t sign, std::int16_t v) {
    return sign > 0 ? add16(base, v) : sub16(base, v);
}

// The search of every placement: the first free entry k >= `from`, k < 45 (`cmp 0x2d; jge`,
// `cmp -1; jne`), else 45 or `from` when it is already past [BYTES seg11:0x8aff-0x8b11
// seg11:0x8fd1-0x8fe3].
std::int16_t search_free(const EffectTable& t, std::int16_t from) {
    std::int16_t k = from;
    while (k < kEffectCount && t[ix(k)].frame != kEffectFree) k = add16(k, 1);
    return k;
}

}  // namespace

RingResult ring_of_death(World& w, EffectSides& sides, std::uint32_t now, int pool, int rec) {
    RingResult res;
    const bool ship_user = pool == w.roles.ships;  // DGROUP:0x9324 [BYTES seg11:0x8a62-0x8a68]
    std::int16_t l = 0;                            // [bp-8] [BYTES seg11:0x8a5c]
    bool ok = false;                               // [bp-0x24] [BYTES seg11:0x8a5f]
    if (ship_user) {
        ShipRecord& s = ship_at(w, rec);
        // for l = 1; l < sext8(+0xcb); l++ (`cmp ax, l; jle` exits) [BYTES seg11:0x8a6a-0x8a81]
        for (l = 1; static_cast<std::int16_t>(static_cast<std::int8_t>(s.slots_cb())) > l; l = add16(l, 1)) {
            if (sbyte_at(s, 0xb8 + l) != kWeaponRingOfDeath) continue;  // [BYTES seg11:0x8a8a-0x8a8f]
            // The first slot holding 1 decides: fuel <= 70 refuses (`jle`) (claim 1) [BYTES
            // seg11:0x8a91-0x8a96].
            if (s.fuel > kRingFuelGate) ok = true;  // [BYTES seg11:0x8a98]
            break;
        }
    } else {
        ok = true;  // [BYTES seg11:0x8a68 seg11:0x8a98]
    }
    if (!ok) return res;  // [BYTES seg11:0x8a9d-0x8aa1]
    res.acted = true;
    res.start = l;
    if (ship_user) {
        // fuel += lo16(seg5:0x110(-70 · mass, 250)): the pool-14 object through DGROUP:0x1e30
        // (literal pool 14), the 16 × 16 `imul`, the 32-bit divide; no floor (claim 2) [BYTES
        // seg11:0x8aad-0x8acd].
        const std::int16_t mass = obj(w, kPoolShips, rec).mass;
        const std::int16_t cost =
            lo16(div32(static_cast<std::int32_t>(kRingFuelFactor) * mass, kRingFuelDivisor));
        ShipRecord& s = ship_at(w, rec);
        s.fuel = add16(s.fuel, cost);
        res.cost = cost;
    }
    // The user's points, read as two dwords: X, Y = +0x04, +0x06 ([bp-0x1e], [bp-0x1c]); SX, SY
    // = +0x00, +0x02 ([bp-0x22], [bp-0x20]) [BYTES seg11:0x8ad1-0x8aee].
    const ObjectRecord& u = obj(w, pool, rec);
    const std::int16_t X = u.x, Y = u.y, SX = u.scr_x, SY = u.scr_y;
    const std::int16_t R = kRingRadius;
    const int size = w.size;  // DGROUP:0x9156 [BYTES seg11:0x8f9f]
    std::int16_t a = kRingFirstA;  // [bp-0xc] [BYTES seg11:0x8af2]
    std::int16_t j = 0;            // [bp-0x18] [BYTES seg11:0x8af7]
    // while R·10 / 14 > a (`imul 0xa`, `cwd; idiv 14`, `jle` exits) [BYTES seg11:0x8f54-0x8f61]
    while (idiv16(mul16(R, 10), 14) > a) {
        // b = lo16(seg8:0x1290(sext(lo16(R·R - a·a)), sext(R - a / 4))), the Newton step with N
        // pushed first (Pascal); a / 4 is `cwd; and dx, 3; add; sar 2` [BYTES seg11:0x8f63-0x8f93].
        const std::int16_t n = sub16(mul16(R, R), mul16(a, a));
        const std::int16_t g = sub16(R, idiv16(a, 4));
        const std::int16_t b = lo16(newton_root(n, g));
        const std::int16_t A8 = shl16(a, 3), AS = sprite_side(a, size);  // [bp-0x14], [bp-0xe] [BYTES seg11:0x8f96-0x8faa]
        const std::int16_t B8 = shl16(b, 3), BS = sprite_side(b, size);  // [bp-0x16], [bp-0x12] [BYTES seg11:0x8fad-0x8fbd]
        std::int16_t from = l;  // [bp-0xa] = [bp-8] [BYTES seg11:0x8fc0-0x8fc3]
        for (const RingBlock& blk : kRingBlocks) {
            const std::int16_t k = search_free(w.effects, from);
            if (k < kEffectCount) {
                const std::int16_t wx = blk.x_takes_a ? A8 : B8, wy = blk.x_takes_a ? B8 : A8;
                const std::int16_t sx = blk.x_takes_a ? AS : BS, sy = blk.x_takes_a ? BS : AS;
                sides.point[ix(k)] = EffectPoint{signed_add(X, blk.sx, wx), signed_add(Y, blk.sy, wy)};    // DGROUP:0x5224
                sides.centre[ix(k)] = EffectPoint{signed_add(SX, blk.sx, sx), signed_add(SY, blk.sy, sy)}; // DGROUP:0x5170
                EffectEntry& e = w.effects[ix(k)];
                e.type = 1;                  // +0x0a [BYTES seg11:0x8b44]
                e.frame = 1;                 // +0x00 [BYTES seg11:0x8b48]
                e.end = kRingEntryFrames;    // +0x02 [BYTES seg11:0x8b4c]
                // now + sext(lo16(30·m)): `imul word`, then `cwd` over dx [BYTES seg11:0x8b52-0x8b65]
                const std::int16_t m = add16(blk.m_base, mul16(blk.m_sign, j));
                e.start.set(now + static_cast<std::uint32_t>(static_cast<std::int32_t>(mul16(kRingStepMs, m))));
                e.pool = static_cast<std::int16_t>(pool);  // +0x06 [BYTES seg11:0x8b6c]
                e.rec = static_cast<std::int16_t>(rec);    // +0x08 [BYTES seg11:0x8b73]
                w.w9430 = add16(w.w9430, 1);               // [BYTES seg11:0x8b77]
                ++res.ring;
            }
            // The next search starts after where this one stopped, a failed one too (claim 6 as
            // corrected) [BYTES seg11:0x8b7b-0x8b82 seg11:0x8f46-0x8f4a].
            l = add16(k, 1);
            from = l;
        }
        a = add16(a, kRingStepA);  // [BYTES seg11:0x8f4d]
        j = add16(j, 1);           // [BYTES seg11:0x8f51]
    }
    // The two centre entries, i = 0, 1 ([bp-6]) [BYTES seg11:0x8fc9-0x906d].
    for (std::int16_t i = 0; i < 2; i = add16(i, 1)) {
        const std::int16_t k = search_free(w.effects, l);
        if (k < kEffectCount) {
            sides.point[ix(k)] = EffectPoint{X, Y};     // [BYTES seg11:0x8fe5-0x8ffc]
            sides.centre[ix(k)] = EffectPoint{SX, SY};  // [BYTES seg11:0x8ff2-0x9003]
            EffectEntry& e = w.effects[ix(k)];
            e.type = kEffectTypeFF06;  // [BYTES seg11:0x9007]
            if (k > w.w9434) w.w9434 = k;  // signed `jle` [BYTES seg11:0x900d-0x9018]
            e.pool = static_cast<std::int16_t>(pool);  // [BYTES seg11:0x9022]
            e.rec = static_cast<std::int16_t>(rec);    // [BYTES seg11:0x9029]
            e.frame = 1;                               // dword 0x000d0001 [BYTES seg11:0x902d]
            e.end = kRingCentreFrames;
            // now + sext(lo16(lo16((15 - j)·30)·i)) [BYTES seg11:0x9036-0x904f]
            const std::int16_t t = mul16(mul16(sub16(15, j), kRingStepMs), i);
            e.start.set(now + static_cast<std::uint32_t>(static_cast<std::int32_t>(t)));
            w.w9430 = add16(w.w9430, 1);  // [BYTES seg11:0x9053]
            ++res.centre;
        }
        l = add16(k, 1);  // [BYTES seg11:0x9057-0x906a]
    }
    return res;
}

}  // namespace is::world
