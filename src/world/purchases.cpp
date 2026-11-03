// owner: M4 stage 3, track VISIT.
// The purchase-applier seg16:0x3784 and its helpers (purchases.h). Every address below is
// INSPACE.EXE; the verdicts are work/analysis/m4/s3-visit-verify.md's.
#include "world/purchases.h"

#include <map>
#include <utility>

#include "world/world.h"
#include "shell/rng.h"

namespace is::world {

void Shop::clear_flags() {
    // seg16:0x5b24's clears and seg16:0x3784's end: 0x957f, 0x9541, 0x953f, 0x9540, 0x953e,
    // 0x9542 + w and 0x9564 + w (w = 1 ... 26), 0x955d ... 0x9562 and 0x9563 [BYTES
    // seg16:0x3a99-0x3b02] (verified claim 82).
    basic = wing = refuel = double_guns = new_body = unify = 0;
    bought.fill(0);
    sold.fill(0);
    powerup.fill(0);
}

Shop& shop() {
    static Shop s;
    return s;
}

std::int32_t credit_of(const ShipRecord& s) {
    return Dword{static_cast<std::uint16_t>(s.wdc), static_cast<std::uint16_t>(s.wde)}.get_signed();
}
void set_credit(ShipRecord& s, std::int32_t v) {
    s.wdc = to_i16(static_cast<std::uint32_t>(v));
    s.wde = to_i16(static_cast<std::uint32_t>(v) >> 16);
}
void charge(ShipRecord& s, std::int16_t cost) {
    // dword -= (int32)(int16)cost, `cdq`-less sign extension through `sbb` [BYTES seg16:0x37f8
    // seg16:0x3800].
    set_credit(s, static_cast<std::int32_t>(static_cast<std::uint32_t>(credit_of(s)) -
                                            static_cast<std::uint32_t>(static_cast<std::int32_t>(cost))));
}
std::int16_t& system_word(ShipRecord& s, int k) { return s.roster_word(k); }

namespace {

std::int8_t sb(const ShipRecord& s, int off) { return sbyte_at(s, off); }
ObjectRecord& object_of(PurchaseCtx& c, int ship) { return c.w.pools.rec(c.w.roles.ships, ship); }
std::int16_t row(PurchaseCtx& c, const ShipRecord& s, int k) {
    return c.h.design_word(s.design_index, 0x22 + 2 * k);  // DGROUP:0x96f4 + 2k
}

// The gun flags' common tail of seg16:0x3468 [BYTES seg16:0x3529-0x3714].
void gun_flags(PurchaseCtx& c, ShipRecord& S) {
    S.w24 = S.w0a;  // `barrels` = +0x0a [BYTES seg16:0x352e seg16:0x3532]
    const std::int16_t r3 = row(c, S, 3);
    if (sb(S, 0xac) != 0)
        set_byte_at(S, 0xe0, 1);  // [BYTES seg16:0x353f]
    else if (r3 == 3)
        set_byte_at(S, 0xe0, 0);  // [BYTES seg16:0x36ab]
    if (sb(S, 0xad) != 0)
        set_byte_at(S, 0xe1, 1);  // [BYTES seg16:0x36bc]
    else if (r3 == 3)
        set_byte_at(S, 0xe1, 0);  // [BYTES seg16:0x36d5]
    if (S.w0a == 1 || (sb(S, 0xac) == 0 && sb(S, 0xad) == 0 && r3 == 3)) {
        set_byte_at(S, 0xe0, 1);  // [BYTES seg16:0x3706]
        S.w24 = 1;                // [BYTES seg16:0x370b]
    }
}

}  // namespace

void remove_slot(ShipRecord& s, int j) {
    // [BYTES seg16:0x391c-0x3974] (verified claim 79); the same shape in seg16:0x3f88
    // [BYTES seg16:0x3f9c-0x3fca].
    s.weapon[static_cast<std::size_t>(j)] = 0;
    s.weapon_count[static_cast<std::size_t>(j)] = 0;
    for (int m = j; m < 9; ++m) {
        s.weapon[static_cast<std::size_t>(m)] = s.weapon[static_cast<std::size_t>(m + 1)];
        s.weapon_count[static_cast<std::size_t>(m)] = s.weapon_count[static_cast<std::size_t>(m + 1)];
    }
    s.weapon[9] = 0;        // +0xc1 [BYTES seg16:0x3fc6]
    s.weapon_count[9] = 0;  // +0xb7 [BYTES seg16:0x3fca]
}

// ---- seg16:0x3468(ship, k) -----------------------------------------------------------------------
std::int16_t repair_system(PurchaseCtx& c, int ship, int k) {
    ShipRecord& S = c.w.ship(ship);
    // The early return 0: no new body and design row k = -1, unless k = 3 with the sheet DC
    // DGROUP:0x3a38 + 2·ship = 0 (verified claim 89, CORRECTED) [BYTES seg16:0x3477-0x34a9].
    if (c.s.new_body == 0 && row(c, S, k) == -1) {
        if (k != 3 || c.h.sheet_dc(ship)) return 0;
    }
    // k = 0 below 40: the rating dword DGROUP:0x5078 = lo32((sys + 200)·rating) / 300, `imul
    // ecx` then `cdq; idiv` of the low dword [BYTES seg16:0x34ac-0x34e0].
    if (k == 0 && system_word(S, 0) < 40) {
        const std::int32_t a = static_cast<std::int32_t>(add16(system_word(S, 0), 200));
        const std::uint32_t prod = static_cast<std::uint32_t>(a) * c.w.rating;
        const std::int32_t lo = Dword{static_cast<std::uint16_t>(prod), static_cast<std::uint16_t>(prod >> 16)}.get_signed();
        c.w.rating = static_cast<std::uint32_t>(lo / 300);
    }
    const std::int16_t cost = sub16(100, system_word(S, k));  // [BYTES seg16:0x34e4-0x34f5]
    if (c.restored == 0) system_word(S, k) = 100;             // [BYTES seg16:0x34f8-0x34ff]
    S.bec = 1;  // `alive` [BYTES seg16:0x350a]
    if (k == 1) S.d8c.set(2);  // the gun interval [BYTES seg16:0x3520] (verified claim 90)
    if (k == 3) {
        if (c.s.new_body != 0) {
            // [BYTES seg16:0x3547-0x35d8] (verified claim 92)
            c.h.designs_begin();
            const std::string name =
                ship == c.w.player ? c.h.ini_main_ship() : c.h.design_name(S.design_index);
            c.h.set_body(ship, c.h.body_index(name));
            c.h.designs_end();
        } else {
            // [BYTES seg16:0x35db-0x3696]
            ObjectRecord& o = object_of(c, ship);
            S.w20 = sub16(o.size, 2);                       // [BYTES seg16:0x35f7]
            S.w22 = 1;                                      // [BYTES seg16:0x35fb]
            o.mass = c.h.design_word(S.design_index, 0x32);  // DGROUP:0x9704 [BYTES seg16:0x3607]
            const std::int8_t ac = sb(S, 0xac), ad = sb(S, 0xad);
            set_byte_at(S, 0xac, 0);   // [BYTES seg16:0x3621]
            c.h.build_sheet(ship);     // seg14:0x11a8 [BYTES seg16:0x362b]
            object_of(c, ship).w3a = c.h.sheet_dc_word(ship);  // [BYTES seg16:0x3636-0x3645]
            if (c.restored != 0) {     // [BYTES seg16:0x3649-0x365a]
                set_byte_at(S, 0xac, ac);
                set_byte_at(S, 0xad, ad);
            }
            if (S.roster_index >= 0) {  // [BYTES seg16:0x365e-0x3692]
                c.h.set_roster_byte(S.roster_index, 0x72, static_cast<std::uint8_t>(sb(S, 0xac)));
                c.h.set_roster_byte(S.roster_index, 0x73, static_cast<std::uint8_t>(sb(S, 0xad)));
            }
        }
    }
    gun_flags(c, S);
    c.h.rate(ship);  // seg13:0x33d6 [BYTES seg16:0x3714]
    return mul16(idiv16(cost, 10), 10);  // `cdq; idiv cx; imul ax, 10` [BYTES seg16:0x371a-0x3723]
}

// ---- seg16:0x3730(ship, slot) --------------------------------------------------------------------
std::int16_t reload_slot(PurchaseCtx& c, int ship, int slot) {
    ShipRecord& S = c.w.ship(ship);
    const std::int8_t a = static_cast<std::int8_t>(S.weapon_count[static_cast<std::size_t>(slot)]);
    if (a == -1) return 0;  // [BYTES seg16:0x3746-0x374f] (verified claim 87, CORRECTED)
    const std::int16_t cost = mul16(sub16(a, 20), -10);
    S.weapon_count[static_cast<std::size_t>(slot)] = 20;  // [BYTES seg16:0x375b]
    S.weapon_count[0] = 0xff;                              // slot 0's +0xae [BYTES seg16:0x3766]
    return mul16(idiv16(cost, 10), 10);                    // [BYTES seg16:0x376b-0x3779]
}

// ---- seg16:0x3e80(ship) --------------------------------------------------------------------------
std::int16_t refuel_ship(PurchaseCtx& c, int ship) {
    ShipRecord& S = c.w.ship(ship);
    const std::int16_t tank = c.h.design_word(S.design_index, 0x1e);  // DGROUP:0x96f0
    const std::int16_t cost = idiv16(sub16(tank, S.fuel), 7);          // [BYTES seg16:0x3e8e-0x3e99]
    if (c.restored == 0) S.fuel = tank;                                // [BYTES seg16:0x3e9f-0x3eae]
    return mul16(idiv16(cost, 10), 10);                                // [BYTES seg16:0x3eb5-0x3ec0]
}

// ---- seg16:0x3ece(ship) --------------------------------------------------------------------------
std::int16_t double_guns(PurchaseCtx& c, int ship) {
    ShipRecord& S = c.w.ship(ship);
    if (S.w0a == 2) return 0;  // [BYTES seg16:0x3ede-0x3ee7]
    S.w0a = 2;                 // [BYTES seg16:0x3ef2]
    S.w24 = 2;                 // [BYTES seg16:0x3ef6]
    const std::int16_t r3 = row(c, S, 3);
    set_byte_at(S, 0xe0, (sb(S, 0xac) != 0 || r3 != 3) ? 1 : 0);  // [BYTES seg16:0x3efa-0x3f1e]
    set_byte_at(S, 0xe1, (sb(S, 0xad) != 0 || r3 != 3) ? 1 : 0);  // [BYTES seg16:0x3f28-0x3f4d]
    if (sb(S, 0xac) == 0 && sb(S, 0xad) == 0 && r3 == 3) {        // [BYTES seg16:0x3f57-0x3f78]
        S.w24 = 1;
        set_byte_at(S, 0xe0, 1);
    }
    return 250;  // [BYTES seg16:0x3f7d]
}

// ---- seg16:0x3f88(ship) --------------------------------------------------------------------------
std::int16_t unify_forces(PurchaseCtx& c, int ship) {
    (void)ship;  // the PLAYER is changed, whatever ship is served (verified claim 84)
    ShipRecord& P = c.w.ship(c.w.player);
    for (int w = 24; w <= 25; ++w) {  // [BYTES seg16:0x3f95 seg16:0x3fd1]
        for (int j = 1; static_cast<std::int8_t>(P.slots_cb()) > j; ++j) {  // [BYTES seg16:0x3fe1-0x3ffc]
            if (static_cast<std::int8_t>(P.weapon[static_cast<std::size_t>(j)]) == w) {
                remove_slot(P, j);  // [BYTES seg16:0x3ffe-0x3fca]
                break;
            }
        }
    }
    P.slots_cb() = static_cast<std::uint8_t>(P.slots_cb() - 2);  // even when neither was found [BYTES seg16:0x4016]
    c.h.load_weapon(c.w.player, 26);                              // seg12:0x6d64(player, 26) [BYTES seg16:0x4021]
    return 250;
}

// ---- seg16:0x4030(ship, k) -----------------------------------------------------------------------
std::int16_t powerup(PurchaseCtx& c, int ship, int k) {
    ShipRecord& S = c.w.ship(ship);
    const int idx = S.roster_index;
    // The byte +0x32 + k += 1 before the index test, so index -1 writes 0x7c before the base
    // (verified claim 85) [BYTES seg16:0x405e seg16:0x4065 seg16:0x4069].
    const std::int8_t b = static_cast<std::int8_t>(c.h.roster_byte(idx, 0x32 + k) + 1);
    c.h.set_roster_byte(idx, 0x32 + k, static_cast<std::uint8_t>(b));
    if (idx != -1) {
        // `turn` +0x14 = seg5:0x110(1200·D, ((int16)((roster +0x33 + 10)·10) · thrust) / 100),
        // D = design word DGROUP:0x96f2 (verified claim 86) [BYTES seg16:0x406e-0x40ad].
        const std::int16_t t33 = static_cast<std::int8_t>(c.h.roster_byte(idx, 0x33));
        const std::int32_t q = static_cast<std::int32_t>(mul16(add16(t33, 10), 10)) *
                               static_cast<std::int32_t>(S.thrust) / 100;
        const std::int32_t n = 1200 * static_cast<std::int32_t>(c.h.design_word(S.design_index, 0x20));
        if (q != 0) S.turn = lo16(div32(n, q));  // a zero divisor traps in the original [DESIGN]
    }
    return k > 1 ? mul16(add16(b, 1), 100) : mul16(add16(b, 3), 100);  // [BYTES seg16:0x40ad]
}

// ---- seg16:0x3b18(ship, mode) --------------------------------------------------------------------
// The computer ships' shop [BYTES seg16:0x3b26-0x3e72] (verified claims 93-97). rand() draws:
// one rand() % 3 when the budget B > 100 (seg16:0x3c8f), then ten rand() % 20 + 1 for r = 0 with
// B >= 700 (seg16:0x3d52) or three rand() % 6 for r = 1 with B >= 400 and a roster record
// (seg16:0x3da2). The player's own visit in a player game returns before any draw. [M5: the
// computer ships' visits are M5; the routine is ported whole because seg14:0xd32's mode-1 pass
// runs it for every fresh ship and its draws move the generator.]
void computer_shop(PurchaseCtx& c, int ship, int mode) {
    Shop& s = c.s;
    ShipRecord& S = c.w.ship(ship);
    if (mode != 2) {  // the clears of seg16:0x5b24 [BYTES seg16:0x3b26]
        s.basic = s.wing = s.refuel = s.double_guns = s.new_body = s.unify = 0;
        s.bought.fill(0);
        s.powerup.fill(0);
    }
    if (c.restored != 0 && ship == c.w.player) return;
    if (s.alive == 0 && mode == 0) {
        s.new_body = 1;
        return;
    }
    if (ship == c.w.player && c.player_game) return;
    std::int32_t B = credit_of(S);
    if (system_word(S, 0) < 60 || system_word(S, 2) < 40) {
        s.basic = 1;
        for (int k = 0; k < 3; ++k) {  // no lamp-row test
            const std::int16_t cst = sub16(100, system_word(S, k));
            s.cost = add16(s.cost, cst);
            B -= cst;
        }
    }
    const std::int16_t tank = c.h.design_word(S.design_index, 0x1e);
    if ((idiv16(tank, 5) > S.fuel && c.w.pool4 < 7) || idiv16(tank, 12) > S.fuel) {
        s.refuel = 1;
        const std::int16_t cst = mul16(idiv16(sub16(tank, S.fuel), 70), 10);
        s.cost = add16(s.cost, cst);
        B -= cst;
    }
    if (B > 100) {  // signed dword [BYTES seg16:0x3c60]
        if (c.w.wave > 2 && S.w0a < 2) {
            s.double_guns = 1;
            s.cost = add16(s.cost, 250);
            B -= 250;
        }
        const int r = c.h.rand() % 3;  // [BYTES seg16:0x3c8f]
        if (r == 0 && B >= 700) {
            for (int i = 0; i < 10; ++i) {
                const int w = c.h.rand() % 20 + 1;  // [BYTES seg16:0x3d52]
                if (c.plus == 0 && c.W.word(w, 0x36) != -1) continue;
                bool owned = false;
                for (int j = 1; j < static_cast<std::int8_t>(S.slots_cb()); ++j)
                    if (static_cast<std::int8_t>(S.weapon[static_cast<std::size_t>(j)]) == w) owned = true;
                std::int16_t cst;
                if (!owned) {
                    cst = c.W.word(w, 0x34);
                } else {
                    // The ammo of slot i, the try index, not the slot found (verified claim 95)
                    // [BYTES seg16:0x3d03-0x3d0b].
                    const std::int8_t a = i < 10 ? static_cast<std::int8_t>(S.weapon_count[static_cast<std::size_t>(i)]) : 0;
                    if (a == -1 || a >= 20) continue;
                    cst = mul16(sub16(20, a), 10);
                }
                if (cst <= B) {
                    s.bought[static_cast<std::size_t>(w)] = 1;
                    B -= cst;
                    s.cost = add16(s.cost, cst);
                }
            }
        } else if (r == 1 && B >= 400 && S.roster_index >= 0) {
            for (int i = 0; i < 3; ++i) {
                const int k = c.h.rand() % 6;  // [BYTES seg16:0x3da2]
                const std::int8_t b = static_cast<std::int8_t>(c.h.roster_byte(S.roster_index, 0x32 + k));
                const std::int16_t cst = add16(mul16(add16(b, 2), 100), k < 2 ? 200 : 0);
                if (cst <= B && b < 5 && (k < 5 || row(c, S, 3) != -1)) {
                    s.powerup[static_cast<std::size_t>(k)] = static_cast<std::uint8_t>(s.powerup[static_cast<std::size_t>(k)] + 1);
                    B -= cst;
                    s.cost = add16(s.cost, cst);
                }
            }
        }
    }
    // The ship's credit, not the budget; design word DGROUP:0x96fc and system word +0x2e
    // (verified claim 97) [BYTES seg16:0x3e4c seg16:0x3e6e].
    if (credit_of(S) > 250 && c.h.design_word(S.design_index, 0x2a) != -1 &&
        (sb(S, 0xac) == 0 || sb(S, 0xad) == 0)) {
        s.wing = 1;
        s.cost = add16(s.cost, sub16(100, S.w2e));
    }
}

// ---- seg16:0x3784(ship, mode) --------------------------------------------------------------------
void apply_purchases(PurchaseCtx& c, int ship, int mode) {
    Shop& s = c.s;
    ShipRecord& S = c.w.ship(ship);
    if (mode == 0) {  // credit += (int32)resources; resources = 0 [BYTES seg16:0x3792-0x37a8]
        set_credit(S, credit_of(S) + S.resources());
        S.set_resources(0);
    }
    if (ship != c.w.player || mode != 0) computer_shop(c, ship, mode);  // [BYTES seg16:0x37ae-0x37c1]
    const bool keep = mode != 0 || ship == c.w.player;                  // [BYTES seg16:0x37c7-0x37da]
    const std::int32_t saved = credit_of(S);
    if (s.double_guns != 0) charge(S, double_guns(c, ship));
    if (s.unify != 0) charge(S, unify_forces(c, ship));
    for (int k = 0; k < kPowerups; ++k)
        for (int j = 0; j < static_cast<std::int8_t>(s.powerup[static_cast<std::size_t>(k)]); ++j)
            charge(S, powerup(c, ship, k));
    // Basic repairs: the flag in any mode, or mode != 0 with DGROUP:0x92e4 = 0 (verified claim
    // 77, CORRECTED) [BYTES seg16:0x3867-0x38a2].
    if (s.basic != 0 || (mode != 0 && c.restored == 0))
        for (int k = 0; k < 3; ++k) charge(S, repair_system(c, ship, k));
    if (s.wing == 0 && mode == 0) {
        if (s.new_body != 0) {  // (verified claim 78, CORRECTED) [BYTES seg16:0x38e8-0x3905]
            set_byte_at(S, 0xac, 0);
            for (int k = 0; k < 4; ++k) repair_system(c, ship, k);  // costs not subtracted
        }
    } else if (ship == c.w.player || credit_of(S) > 100 || mode != 0) {
        charge(S, repair_system(c, ship, 3));  // [BYTES seg16:0x38a4-0x38d2]
    }
    for (int w = 1; w <= kShopWeapons; ++w) {
        const int cb = static_cast<std::int8_t>(S.slots_cb());
        if (s.sold[static_cast<std::size_t>(w)] != 0) {
            for (int j = 0; j < cb; ++j) {  // the first slot only (verified claim 79)
                if (static_cast<std::int8_t>(S.weapon[static_cast<std::size_t>(j)]) == w) {
                    remove_slot(S, j);
                    S.slots_cb() = static_cast<std::uint8_t>(S.slots_cb() - 1);  // [BYTES seg16:0x3974]
                    break;
                }
            }
        } else if (s.bought[static_cast<std::size_t>(w)] != 0) {
            int j = 0;
            for (; j < cb; ++j)
                if (static_cast<std::int8_t>(S.weapon[static_cast<std::size_t>(j)]) == w) break;
            if (j < cb) {
                charge(S, reload_slot(c, ship, j));  // [BYTES seg16:0x39b7-0x39bc]
            } else {
                c.h.load_weapon(ship, w);  // seg12:0x6d64 [BYTES seg16:0x39e2]
                set_credit(S, credit_of(S) - static_cast<std::int32_t>(c.W.word(w, 0x34)));  // [BYTES seg16:0x39ee-0x39f4]
            }
        }
    }
    if (s.refuel != 0 || mode != 0) charge(S, refuel_ship(c, ship));  // [BYTES seg16:0x3a16-0x3a27]
    if (keep) set_credit(S, saved);  // [BYTES seg16:0x3a39-0x3a56]
    c.h.rate(ship);                  // seg13:0x33d6 [BYTES seg16:0x3a5c]
    S.thrusting = 0;                 // [BYTES seg16:0x3a6a]
    object_of(c, ship).angle = 0;    // object word +0x26 [BYTES seg16:0x3a7a] (verified claim 81)
    if (c.h.game_state() == 1 && ship == c.w.player) c.h.post_info_paint();  // [BYTES seg16:0x3a81-0x3abb]
    s.clear_flags();
    s.alive = 1;       // the dword write DGROUP:0x9e5b = 1 [BYTES seg16:0x3b07]
    s.sold_total = 0;  // ... which also zeroes DGROUP:0x9e5d
}

void wave_setup_purchases(World& w, PurchaseHost& h, const WeaponWords& W, int ship,
                          bool player_game, std::int16_t restored, std::int16_t plus) {
    PurchaseCtx c{w, shop(), W, h, player_game, restored, plus};
    apply_purchases(c, ship, 1);  // seg14:0xd32 [BYTES seg14:0xd16-0xd32]
}

// ---- WorldPurchaseHost --------------------------------------------------------------------------
namespace {
// The roster bytes the RosterRecord does not keep yet, by (index, offset) [OPEN: the ROSTER
// track's 0x7c-byte record replaces this store].
std::map<std::pair<int, int>, std::uint8_t>& roster_side() {
    static std::map<std::pair<int, int>, std::uint8_t> m;
    return m;
}
}  // namespace

std::int16_t WorldPurchaseHost::design_word(int design, int off) {
    if (design < 0 || design >= kShipCount) return 0;
    const DesignRating& r = w_.design_ratings[static_cast<std::size_t>(design)];
    switch (off) {
    case 0x1e: return r.tank;  // DGROUP:0x96f0
    case 0x22: return 0;       // the lamp rows 0, 1, 2 [BYTES seg14:0x1d3b seg14:0x1d44]
    case 0x24: return 1;
    case 0x26: return 2;
    case 0x28: return r.special;  // DGROUP:0x96fa
    case 0x2a: return idiv16(r.tank, 40);  // as collide.h design_from_world [OPEN: its writer]
    case 0x32: return on_design_mass ? on_design_mass(design) : std::int16_t{0};  // DGROUP:0x9704
    default:
        todo("design word +" + std::to_string(off), "DGROUP:0x96d2");
        return 0;
    }
}
std::string WorldPurchaseHost::design_name(int design) {
    if (on_design_name) return on_design_name(design);
    todo("the design name", "DGROUP:0x9742");
    return {};
}
int WorldPurchaseHost::roster_byte(int index, int off) {
    if (index >= 0 && static_cast<std::size_t>(index) < w_.roster.size()) {
        const RosterRecord& r = w_.roster[static_cast<std::size_t>(index)];
        if (off >= 0x33 && off <= 0x37) return static_cast<std::uint8_t>(r.b33[static_cast<std::size_t>(off - 0x33)]);
        if (off == 0x7a) return r.size;
    }
    auto it = roster_side().find({index, off});
    return it == roster_side().end() ? 0 : it->second;
}
void WorldPurchaseHost::set_roster_byte(int index, int off, int v) {
    if (index >= 0 && static_cast<std::size_t>(index) < w_.roster.size()) {
        RosterRecord& r = w_.roster[static_cast<std::size_t>(index)];
        if (off >= 0x33 && off <= 0x37) {
            r.b33[static_cast<std::size_t>(off - 0x33)] = static_cast<std::int8_t>(v);
            return;
        }
        if (off == 0x7a) {
            r.size = static_cast<std::uint8_t>(v);
            return;
        }
    }
    roster_side()[{index, off}] = static_cast<std::uint8_t>(v);
}
void WorldPurchaseHost::load_weapon(int ship, int w) {
    if (on_load_weapon) return on_load_weapon(ship, w);
    todo("load a weapon into a new slot", "seg12:0x6d64");
}
void WorldPurchaseHost::rate(int ship) { rate_ship(w_, ship); }
void WorldPurchaseHost::build_sheet(int ship) {
    if (on_build_sheet) return on_build_sheet(ship);
    todo("the ship sheet rebuild", "seg14:0x11a8");
}
bool WorldPurchaseHost::sheet_dc(int ship) {
    if (on_sheet_dc) return on_sheet_dc(ship);
    return slot_image(kShipImageSlotBase + ship) != nullptr;
}
std::int16_t WorldPurchaseHost::sheet_dc_word(int ship) {
    return sheet_dc(ship) ? static_cast<std::int16_t>(kShipImageSlotBase + ship) : std::int16_t{0};
}
void WorldPurchaseHost::designs_begin() { todo("the design table recompute", "seg14:0x3eda"); }
void WorldPurchaseHost::designs_end() { todo("the design table recompute", "seg14:0x4074"); }
std::string WorldPurchaseHost::ini_main_ship() {
    if (on_ini_main_ship) return on_ini_main_ship();
    return "EAGLE";  // the default DGROUP:0x112d [BYTES seg16:0x3566]
}
int WorldPurchaseHost::body_index(const std::string& name) {
    (void)name;
    todo("the body record search", "seg16:0x3595");
    return 0;
}
void WorldPurchaseHost::set_body(int ship, int index) {
    (void)ship, (void)index;
    todo("the new body's design switch (M8)", "seg16:0x5f78");
}
void WorldPurchaseHost::post_info_paint() {
    if (on_post_info_paint) on_post_info_paint();
}
int WorldPurchaseHost::game_state() { return on_game_state ? on_game_state() : 1; }
int WorldPurchaseHost::rand() { return shell::game_rng().rand(); }
void WorldPurchaseHost::todo(const std::string& what, const char* address) {
    if (todo_log) todo_log(what, address);
}

}  // namespace is::world
