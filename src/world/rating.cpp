// seg13:0x33d6 and its inputs (src/world/rating.h). Portable (M5 s2 BRIEFING).
#include "world/rating.h"

#include "formats/ships.h"
#include "world/world.h"

namespace is::world {

namespace {

// 16-bit `idiv` with the port's guard for a zero divisor [DESIGN].
std::int16_t q16(std::int16_t n, std::int16_t d) { return d == 0 ? 0 : idiv16(n, d); }

// The 32 initialised bytes DGROUP:0x1332-0x1351 [BYTES DGROUP:0x1332].
constexpr std::int8_t kWeaponStrength[32] = {0, 8, 10, 8, 4, 5, 10, 6, 7, 6, 4, 3, 4, 6, 9, 7,
                                             3, 6, 5, 6, 5, 6, 8,  8, 7, 5, 9, 0, 4, 0, 0, 0};

}  // namespace

DesignRating design_rating(const fmt::ShipRecord& shp, const fmt::BodyRecord& body) {
    DesignRating d;
    // `idiv bl` / `idiv cl` of the sign-extended armor byte [BYTES seg14:0x1cae-0x1d0e].
    const std::int16_t armor = static_cast<std::int8_t>(shp.armor);
    d.armor4a = idiv16(armor, 4);
    d.armor5a = idiv16(armor, 5);
    d.armor4b = idiv16(armor, 4);
    d.armor5b = idiv16(armor, 5);
    d.tank = mul16(40, static_cast<std::int8_t>(shp.fuel_tank));  // `imul byte` [BYTES seg14:0x1d1c]
    if (body.flags & fmt::kBodyPullOffWings)
        d.special = 3;
    else if (shp.flags & fmt::kShipCloak)
        d.special = 4;
    else if (body.flags & fmt::kBodyShield)
        d.special = 5;
    else
        d.special = -1;
    d.captures = (shp.flags & fmt::kShipCapturesIcons) ? 1 : 0;
    d.cloak = (shp.flags & fmt::kShipCloak) ? 1 : 0;
    d.repair = (shp.flags & fmt::kShipRepair) ? 1 : 0;
    d.shield = (body.flags & fmt::kBodyShield) ? 1 : 0;
    return d;
}

std::int8_t weapon_strength(int weapon) {
    return weapon >= 0 && weapon < 32 ? kWeaponStrength[weapon] : 0;
}

std::int16_t ship_rating(const ShipRecord& s, const ObjectRecord& o, const DesignRating& d,
                         const RosterRecord* r) {
    // t = design +0x1e / 20 [BYTES seg13:0x33ea-0x33fa]
    const std::int16_t t = idiv16(d.tank, 20);
    // q [BYTES seg13:0x33fd-0x34a0]
    std::int16_t q;
    if (r == nullptr) {
        q = d.armor4a;
    } else {
        auto den = [&](int k) { return sub16(20, mul16(3, r->b33[static_cast<std::size_t>(k)])); };
        const std::int16_t c35 = den(2);  // 20 - 3 r+0x35
        const std::int16_t p12 = mul16(d.armor5a, 100);
        const std::int16_t x14 = q16(mul16(d.armor4b, 80), den(3));  // / (20 - 3 r+0x36)
        const std::int16_t x10 = q16(mul16(d.armor4a, 80), den(1));  // / (20 - 3 r+0x34)
        std::int16_t sum = add16(add16(x10, x14), q16(p12, c35));
        if (d.special == -1) {
            q = idiv16(sum, 12);
        } else {
            sum = add16(sum, q16(mul16(d.armor5b, 100), den(4)));  // / (20 - 3 r+0x37)
            q = idiv16(sum, 16);
        }
    }
    // a = max(1, ((object +0x44 + t) q) / 86), the product 32-bit [BYTES seg13:0x34a3-0x34cf]
    const std::int16_t mt = add16(o.mass, t);
    std::int16_t a = lo16(div32(static_cast<std::int32_t>(mt) * q, 86));
    if (a < 1) a = 1;
    // T: +0x0c, or ((r+0x33 + 10) 10 T) / 100 with a roster record [BYTES seg13:0x34d2-0x3508]
    std::int16_t thrust = s.thrust;
    if (r != nullptr) {
        const std::int16_t f = mul16(add16(r->b33[0], 10), 10);
        thrust = lo16(div32(static_cast<std::int32_t>(f) * thrust, 100));
    }
    // c = 2 (((2T - +0x0e) 10) / (object +0x44 + t) - +0x14 / 10) [BYTES seg13:0x350b-0x354b]
    const std::int16_t num = mul16(sub16(add16(thrust, thrust), s.drag), 10);
    std::int16_t c = mul16(sub16(q16(num, mt), idiv16(s.turn, 10)), 2);
    // w = sum over j = 1 ... byte +0xcb - 1 of DGROUP:0x1332[weapon byte +0xb8 + j] / ((j + 1) / 2)
    // [BYTES seg13:0x354e-0x358c]
    std::int16_t w = 0;
    const int n = sbyte_at(s, 0xcb);
    for (int j = 1; j < n; ++j) {
        const std::int16_t div = static_cast<std::int16_t>((j + 1) / 2);
        w = add16(w, q16(weapon_strength(sbyte_at(s, 0xb8 + j)), div));
    }
    // The design terms [BYTES seg13:0x358e-0x363d]
    if (d.repair == 0) a = static_cast<std::int16_t>(mul16(a, 3) / 4);  // cwd; and 3; sar 2: trunc
    if (d.cloak != 0) a = idiv16(mul16(a, 6), 5);
    if (d.shield != 0) a = idiv16(mul16(a, 6), 5);
    if (d.captures == 0) c = idiv16(mul16(c, 5), 6);
    c = idiv16(mul16(c, 5), 6);
    a = idiv16(mul16(a, 6), 5);
    if (mul16(a, 5) < c) c = mul16(a, 5);
    if (mul16(c, 5) < a) a = mul16(c, 5);
    // ((+0x26 / 2) + 50) (c + w + a), 32-bit; a positive product above 0x7fbc is 32700, a negative
    // one keeps its low word [BYTES seg13:0x3640-0x366a].
    const std::int16_t f = add16(idiv16(s.w26, 2), 50);
    const std::int32_t p = static_cast<std::int32_t>(f) * add16(add16(c, w), a);
    const std::int16_t hi = static_cast<std::int16_t>(static_cast<std::uint32_t>(p) >> 16);
    const std::uint16_t lo = static_cast<std::uint16_t>(p);
    if (hi < 0) return static_cast<std::int16_t>(lo);
    if (hi > 0 || lo > 0x7fbc) return 0x7fbc;
    return static_cast<std::int16_t>(lo);
}

}  // namespace is::world
