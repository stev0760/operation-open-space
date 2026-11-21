// A computer ship's reactions (reactions.h): seg13:0x7ba2 and seg13:0x9bb8 as the listing reads,
// with s4-commands-verify.md claims 50-56 and its Omissions on both routines.
#include "world/reactions.h"

#include <map>
#include <string>

#include "world/pilot_orders.h"
#include "world/world.h"

namespace is::world {

namespace {

constexpr int kShips = 14;  // the literal pool 14 (DGROUP:0x1e30 and the goal tests)

void log_site(const char* what, const char* address) {
    static std::map<std::string, bool> once;
    guard_todo(once[std::string(address) + what], what, address);
}

ShipRecord& ship(CommandCtx& c, int j, const char* address) {
    if (j >= 0 && j < kShipCount) return c.p.s(j);
    static ShipRecord zero;
    zero = ShipRecord{};
    log_site("a ship record past the eight-record table reads 0", address);  // [DESIGN ADR 0016 decision 2]
    return zero;
}
std::int16_t w(CommandCtx& c, int j, int off) { return ship_w(ship(c, j, "seg13:0x7ba2"), off); }
void setw(CommandCtx& c, int j, int off, int v) { set_ship_w(ship(c, j, "seg13:0x7ba2"), off, static_cast<std::int16_t>(v)); }
std::uint32_t d(CommandCtx& c, int j, int off) { return ship_d(ship(c, j, "seg13:0x7ba2"), off); }
void setd(CommandCtx& c, int j, int off, std::uint32_t v) { set_ship_d(ship(c, j, "seg13:0x7ba2"), off, v); }
std::int8_t sb(CommandCtx& c, int j, int off) { return ship_sb(ship(c, j, "seg13:0x7ba2"), off); }
// Ship i's byte +base + k; past the record 0 [DESIGN ADR 0016 decision 2].
std::int8_t sbk(CommandCtx& c, int i, int base, int k) {
    const int off = base + k;
    if (off < 0 || off >= static_cast<int>(kShipRecordSize)) {
        log_site("a ship byte indexed past the record reads 0", "seg13:0x7ba2");
        return 0;
    }
    return sb(c, i, off);
}
bool in_play(CommandCtx& c, int j) { return c.p.obj(kShips, j).state == 0; }
bool alive(CommandCtx& c, int j) { return ship_b(ship(c, j, "seg13:0x717c"), pw::kAlive) != 0; }
std::int16_t team(CommandCtx& c, int j) { return c.p.host.design_word(ship(c, j, "seg13:0x970a").design_index, 0x38); }
int last(CommandCtx& c) { return c.p.g.last_ship < kShipCount ? c.p.g.last_ship : kShipCount - 1; }
std::int16_t imul8(int a, std::int8_t b) { return static_cast<std::int16_t>(static_cast<std::int8_t>(a) * b); }

// Some other ship m (in play, alive, m != i, m != P) holds order 1 on i (+0x56 = i)
// [BYTES seg13:0x7f6b-0x7fb5 seg13:0x803e-0x8088].
bool helped(CommandCtx& c, int i) {
    for (int m = 0; m <= last(c); ++m) {
        if (!in_play(c, m) || !alive(c, m) || m == i) continue;
        if (w(c, m, pw::kOrder) != 1 || w(c, m, pw::kOrderRec) != i) continue;
        if (m == c.p.g.player) continue;
        return true;
    }
    return false;
}

}  // namespace

// ---- seg13:0x9bb8(i, n) [BYTES seg13:0x9bb8-0x9cde]
std::int16_t ambulance_time(CommandCtx& c, int i, int n) {
    const int P = c.p.g.player;
    if (c.p.obj(7, 0).state == 0) return 0;                                // DGROUP:0x1e14 [BYTES seg13:0x9bc5-0x9bd2]
    // m = lo16(word +0xdc + word +0xce), the low word of the credit [BYTES seg13:0x9bd5-0x9be2]
    const std::int16_t m = add16(w(c, i, pw::kCredit), w(c, i, pw::kResources));
    if (m < 250 && i != P) return 0;
    // The player and the ship hunting each other [BYTES seg13:0x9bf2-0x9c34]
    const bool p_hunts = w(c, P, pw::kGoalPool) == kShips && w(c, P, pw::kGoalRec) == i;
    const bool i_hunts = w(c, i, pw::kGoalPool) == kShips && w(c, i, pw::kGoalRec) == P;
    if (p_hunts || i_hunts) {
        if (w(c, i, 0x82) < 1700) return 0;
        if (w(c, i, 0x26) > 1) return 0;
    }
    std::array<std::int16_t, 8>& mark = c.cw.ambulance_wave;               // DGROUP:0x1c8e + 2i
    if (i < 0 || i >= kShipCount) {
        log_site("DGROUP:0x1c8e past its eight words", "seg13:0x9c3e");
        return 0;
    }
    std::int16_t& mi = mark[static_cast<std::size_t>(i)];
    if (mi == c.p.w.wave) return 0;                                        // DGROUP:0x91f8
    const auto yes = [&]() -> std::int16_t {
        mi = c.p.w.wave;
        return 1;
    };
    if (w(c, i, 0x28) <= 0 || w(c, i, 0x2a) <= 0) return yes();            // [BYTES seg13:0x9c44-0x9c55]
    // clamp(20·(n + 2) + m / 10, 70, 180) [BYTES seg13:0x9c68-0x9c89]
    std::int16_t lim = add16(mul16(static_cast<std::int16_t>(n + 2), 20), idiv16(m, 10));
    if (lim > 180) lim = 180;
    if (lim < 70) lim = 70;
    if (add16(add16(w(c, i, 0x26), w(c, i, 0x28)), w(c, i, 0x2a)) < lim) return yes();
    if (w(c, i, pw::kResources) > 200 && mul16(add16(c.p.w.score_level, 20), 35) < m) return yes();  // DGROUP:0x9334
    const std::uint8_t wing0 = ship_b(ship(c, i, "seg13:0x9cbc"), 0xac), wing1 = ship_b(ship(c, i, "seg13:0x9cc3"), 0xad);
    if ((wing0 == 0 || wing1 == 0) && m > 500) return yes();               // [BYTES seg13:0x9cb7-0x9cd5]
    return 0;
}

// ---- seg13:0x7ba2(i) [BYTES seg13:0x7ba2-0x81c0]
void reactions(CommandCtx& c, int i) {
    const int P = c.p.g.player;
    const std::uint32_t now = c.p.g.now;
    // Exits [BYTES seg13:0x7bb5-0x7be6]
    if (i == P && c.p.g.demo != 0) return;
    if (c.p.g.demons_gate != 0 || c.p.g.wave_kind == 7 || team(c, i) == 7) return;
    // w: object +0x34 when object +0x32 = 14 [BYTES seg13:0x7be9-0x7c05]
    std::int16_t wobj = -1;                                                // [bp-0x10]
    {
        const ObjectRecord& o = c.p.obj(kShips, i);
        if (o.owner_pool == kShips) wobj = o.owner_rec;
    }
    // h [BYTES seg13:0x7c08-0x7c30]
    bool h = false;                                                        // [bp-0x14]
    if ((w(c, i, pw::kOrder) == 1 || w(c, i, pw::kOrder) == 5) && sbk(c, i, pw::kGrudge0, w(c, i, pw::kOrderRec)) == 0) h = true;
    std::int16_t best = 20000;                                             // [bp-0xa]
    int k = -1;                                                            // [bp-0xc]
    std::int16_t n = 0;                                                    // [bp-6]
    // Busy [BYTES seg13:0x7c3f-0x7c5b]
    if (w(c, i, pw::kOrder) != 0 && w(c, i, pw::kOrder) != 7 && d(c, i, pw::kOrderUntil) > now) return;
    // The threats [BYTES seg13:0x7c5f-0x7d0b]
    for (int j = 0; j <= last(c); ++j) {
        if (!in_play(c, j)) continue;
        if (w(c, j, pw::kGoalPool) != kShips || w(c, j, pw::kGoalRec) != i || w(c, j, pw::kGoalAct) != 1) continue;
        if (!alive(c, j)) continue;
        const std::int16_t s = c.p.host.strength(i, j);                    // seg4:0x5d9c(i, j)
        if (s < best) {
            best = s;
            k = j;
        }
        n = add16(n, 1);
    }
    if (std::vector<DefenderEntry>* t = c.h.defenders(); t) {
        for (int k2 = 0; k2 < c.p.pools.in_use(2) && k2 < static_cast<int>(t->size()); ++k2) {
            if (c.p.obj(2, k2).state != 0) continue;                       // DGROUP:0x1e00
            const DefenderEntry& e = (*t)[static_cast<std::size_t>(k2)];
            if (e.killer_pool == kShips && e.killer_rec == i && e.w00 == 10) n = add16(n, 1);
        }
    }
    if (wobj != -1) {
        k = wobj;
        n = add16(n, 1);
    }
    bool calls = false;
    if (n != 0) {                                                          // [BYTES seg13:0x7d0e-0x7d12]
        if (k != -1 && sbk(c, i, pw::kFight0, k) < -1) n = add16(n, 1);   // [BYTES seg13:0x7d16-0x7d2b]
        if (h && w(c, i, 0x26) > 320 && wobj == -1) return;                // [BYTES seg13:0x7d2e-0x7d45]
        calls = true;
        // Flee or repair [BYTES seg13:0x7d49-0x7da9]
        bool stand = w(c, i, pw::kGoalAct) == 4 || (w(c, i, pw::kOrder) == 3 && w(c, i, 0x26) >= 20);
        if (!stand) {
            const std::int16_t e6 = mul16(mul16(sb(c, i, pw::kClass), n), 6);
            const bool trigger = add16(e6, 15) > w(c, i, 0x26) || add16(e6, 17) > w(c, i, 0x28);
            if (!trigger) stand = true;
            else if (w(c, i, pw::kOrder) != 0 && d(c, i, pw::kOrderUntil) - 7000u >= now) stand = true;
        }
        if (!stand) {
            const bool flee_ok = add16(mul16(n, 20), 35) > best || add16(imul8(0x0f, sb(c, i, pw::kClass)), 20) > w(c, i, 0x26);
            if (flee_ok && k != -1 && sbk(c, i, pw::kFight0, k) < -1) {   // the flee [BYTES seg13:0x7dcd-0x7e25]
                setw(c, i, pw::kGoalPool, kShips);
                setw(c, i, pw::kEstPool, kShips);
                setw(c, i, pw::kGoalRec, k);
                setw(c, i, pw::kEstRec, k);
                setw(c, i, pw::kEstApp, 5);
                setw(c, i, pw::kGoalApp, 5);
                setw(c, i, pw::kEstAct, 4);
                setw(c, i, pw::kGoalAct, 4);
                setw(c, i, pw::kOrder, 7);
                setd(c, i, pw::kReplanAt, now + 4000u);
                c.p.host.message(k, kShips, i, 0x47);
            } else if (w(c, i, 0x2a) < 50 && add16(shl16(sb(c, i, pw::kClass), 3), 35) > w(c, i, 0x26)) {
                // The repair [BYTES seg13:0x7e28-0x7e6c]
                setw(c, i, pw::kOrder, 0x0d);
                setw(c, i, pw::kOrderBy, i);
                setd(c, i, pw::kOrderUntil, now + 1000u);
                setd(c, i, pw::kReplanAt, now + 1000u);
                set_ship_b(ship(c, i, "seg13:0x7e6c"), pw::kOneShot, 0);
                // seg13:0x7e71-0x7efb test +0x52 for 2 ... 4 and for 1 or 2 right after the store of
                // 0x0d: the messages 0x8a, 0x73, 0x13 and the grudge stores cannot run.
            }
        } else if (w(c, i, pw::kGoalAct) == 4 && add16(mul16(n, 20), 35) < best) {
            // Stand [BYTES seg13:0x7f06-0x7f31]
            setw(c, i, pw::kEstAct, 1);
            setw(c, i, pw::kGoalAct, 1);
            ship_reaction(c.p, i, w(c, i, pw::kGoalPool), w(c, i, pw::kGoalRec), 0x10);
        }
    }
    if (calls) {
        // The calls [BYTES seg13:0x7f39-0x80a2]
        if (ambulance_time(c, i, n) != 0) c.p.host.ambulance_call(i);     // seg16:0xb4e(i)
        if (w(c, i, 0x26) < add16(mul16(n, 20), 25) && k != -1) {
            if (helped(c, i)) return;
            if (help_command(c, i) == 0) setd(c, i, pw::kD9c, 1);          // the dword 1 (high word AX = 0)
        } else if (n != 0 && k != -1 &&
                   (n > 1 || best < 78 ||
                    (w(c, i, pw::kGoalRec) == c.p.g.dg_952e && w(c, i, pw::kGoalPool) == kShips && c.p.g.dg_9e49 != 10))) {
            c.p.host.target(i, kShips, k);                                 // seg3:0xf4(i, 14, k)
            if (ask_attack(c, i) == 0) setd(c, i, pw::kD9c, 1);            // seg13:0x9970(i)
        }
        // The FUZZY ONES [BYTES seg13:0x8020-0x80a2]
        if (team(c, i) == 2 && n != 0 && k != -1) {
            if (helped(c, i)) return;
            if (help_command(c, i) == 0) setd(c, i, pw::kD9c, 1);
        }
    }
    // Calm down [BYTES seg13:0x80a6-0x80d3]
    if ((n == 0 || best > 60) && w(c, i, pw::kGoalAct) == 4) {
        setw(c, i, pw::kEstAct, 3);
        setw(c, i, pw::kGoalAct, 3);
        setw(c, i, pw::kOrder, 0);
        setd(c, i, pw::kReplanAt, now);
    }
    if (ambulance_time(c, i, n) != 0) c.p.host.ambulance_call(i);         // [BYTES seg13:0x80d8-0x80f1]
    // Revenge: seg13:0x2688(i, k) with k = i's +0x34 (verify claim 54) [BYTES seg13:0x80f2-0x814d]
    if (w(c, i, pw::kGoalAct) == 1 && w(c, i, pw::kGoalPool) == kShips) {
        const int g = w(c, i, pw::kGoalRec);
        if (sbk(c, i, pw::kFight0, g) < -2 && c.p.host.relation(i, g) < -150 && alive(c, i)) {
            c.p.host.target(i, kShips, w(c, i, pw::kGoalRec));
            if (ask_attack(c, i) == 0) setd(c, i, pw::kD9c, 1);
        }
    }
    // The viruses [BYTES seg13:0x8151-0x81b5]
    if (c.p.pools.in_use(9) > 1 && w(c, i, pw::kGoalPool) != kShips) {    // DGROUP:0x9418
        const std::int16_t r = imod16(static_cast<std::int16_t>(c.p.rng.rand()), 3);  // seg5:0xdc, `cdq; idiv 3`
        if (r != 0) return;
        if (!(d(c, i, pw::kD9c) < now)) return;
        for (int v = 0; v < c.p.pools.in_use(9); ++v) {
            if (c.p.obj(9, v).state != 0) continue;                        // DGROUP:0x1e1c
            c.p.host.target(i, 9, v);
            ask_attack(c, i);
            return;
        }
    }
}

void command_reactions(int i) {
    if (command_links().with_ctx) return command_links().with_ctx([&](CommandCtx& c) { reactions(c, i); });
    log_site("the reactions to other ships", "seg13:0x7ba2");
}

}  // namespace is::world
