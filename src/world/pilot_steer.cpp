// How a computer ship flies and fires (pilot_steer.h). Each routine follows the original's
// instructions in address order; the comments cite the addresses the steps come from.
#include "world/pilot_steer.h"

#include "world/ambulance.h"
#include "world/collide.h"
#include "world/missile.h"
#include "world/pilot.h"

namespace is::world {

namespace {

constexpr int kPoolShips14 = 14;  // the literal 14 of DGROUP:0x1e30 and the pushes `push 0xe`
constexpr int kPoolGatePosts8 = 8;   // DGROUP:0x1e18
constexpr int kPoolGateMids12 = 12;  // DGROUP:0x1e28

// The weapon words of the original's table DGROUP:0x2f54 + 0x50·w that the routines read.
constexpr int kWKind = 0x0a;   // DGROUP:0x2f5e, the launch kind
constexpr int kWWait = 0x28;   // DGROUP:0x2f7c
constexpr int kWLong = 0x42;   // DGROUP:0x2f96

void log_once(bool& once, const char* what, const char* address) { guard_todo(once, what, address); }

// The signed byte +0xb8 + k: the weapon of slot k. An offset outside the 0x15a-byte record reads
// 0 where the original reads the next record's bytes [DESIGN ADR 0016 decision 2].
std::int8_t slot_weapon(const ShipRecord& s, int k) {
    const int off = 0xb8 + k;
    if (off < 0 || off >= static_cast<int>(kShipRecordSize)) {
        static bool once = false;
        log_once(once, "a weapon slot outside the ship record reads 0", "DGROUP:0x7148");
        return 0;
    }
    return ship_sb(s, off);
}

}  // namespace

// ---------------------------------------------------------------- SteerHost defaults

void SteerHost::think(int ship) {
    static bool once = false;
    (void)ship, log_once(once, "the think is not bound", "seg4:0x78");
}
void SteerHost::hazard_step(int ship) {
    static bool once = false;
    (void)ship, log_once(once, "the hazard step is not bound", "seg4:0x4fc8");
}
void SteerHost::weapon_choice(int ship) {
    static bool once = false;
    (void)ship, log_once(once, "the weapon choice is not bound", "seg4:0x6e70");
}
void SteerHost::race_course(int ship, int arg) {
    static bool once = false;
    (void)ship, (void)arg, log_once(once, "the race course (M6)", "seg4:0x144c");
}
// The touch, the view and the launchers: the routines M4 ported, through the MissileLinks the
// program binds (missile.h linked_missile_host), each with the launcher's pool and record.
void SteerHost::touch(int ship, std::int16_t pool_rec) { linked_missile_host().touch(ship, pool_rec); }
Point16 SteerHost::to_screen(std::int16_t x, std::int16_t y) { return linked_missile_host().to_screen(x, y); }
void SteerHost::capture_count(int ship, int v) { linked_missile_host().capture_count(ship, v); }
void SteerHost::launch(World& world, WaveGlobals& g, shell::Rng& rng, int ship, int pool, int weapon) {
    MissileCtx mc{world, g, missile_state(), linked_missile_host(), rng};
    ::is::world::launch(mc, ship, pool, weapon);  // seg11:0x7c12 [BYTES seg11:0x7c12]
}
void SteerHost::fuel_ball(int pool, int rec, int life, int kind) {
    linked_missile_host().fuel_ball(pool, rec, life, kind);
}
void SteerHost::ring_of_death(int pool, int rec) { linked_missile_host().ring_of_death(pool, rec); }
void SteerHost::tether_use(int pool, int rec, int w) { linked_missile_host().tether_use(pool, rec, w); }
void SteerHost::lightning(int pool, int rec) { linked_missile_host().lightning(pool, rec); }
void SteerHost::shock(int pool, int rec) {
    static bool once = false;
    (void)pool, (void)rec, log_once(once, "the shock (M6)", "seg11:0x9daa");
}
void SteerHost::wildcard(int pool, int rec) { linked_missile_host().wildcard(pool, rec); }
void SteerHost::weapon_display() {
    static bool once = false;
    log_once(once, "the Strategy player's weapon display", "seg11:0x2f8c");
}
std::int16_t SteerHost::weapon_word(int w, int off) { return missile_state().words.word(w, off); }

SteerHost& default_steer_host() {
    static SteerHost h;
    return h;
}

SteerGlobals& steer_state() {
    static SteerGlobals s;
    return s;
}

void steer_frame(World& w, FlightCtx& f, SteerHost& h, int i) {
    SteerCtx c{w, f, h, steer_state()};
    steer(c, i);
}
std::int16_t weapon_decision_frame(World& w, FlightCtx& f, SteerHost& h, int i) {
    SteerCtx c{w, f, h, steer_state()};
    return weapon_decision(c, i);
}
void use_weapon_frame(World& w, FlightCtx& f, SteerHost& h, int k, int s, int life) {
    SteerCtx c{w, f, h, steer_state()};
    use_weapon(c, k, s, life);
}

// ---------------------------------------------------------------- seg13:0xd6c

std::int16_t aim_heading(SteerCtx& c, const AimArgs& a) {
    World& w = c.w;
    const WaveGlobals& g = c.f.g;
    const std::int16_t kind = g.wave_kind;  // DGROUP:0x9320
    std::int16_t cur = a.cur;
    // A ship: byte +0xeb, or word +0x10 in 1 ... 6, keeps the heading [BYTES seg13:0xd7a-0xda3]
    if (a.ship > -1) {
        const ShipRecord& S = ship_at(w, a.ship);
        if (S.beb != 0) return cur;
        if (S.type > 0 && S.type < 7) return cur;
    }
    // The points in cells and the world wrap outside a race [BYTES seg13:0xda5-0xe2c]
    const std::int16_t my = sar16(a.my, 3), ty = sar16(a.ty, 3);
    const std::int16_t tx = sar16(a.tx, 3), mx = sar16(a.mx, 3);
    std::int16_t dx = sub16(tx, mx);  // [bp-4]
    if (abs16(dx) > sar16(kWorldCells, 1) && kind != 6)
        dx = sub16(dx < 0 ? add16(tx, kWorldCells) : sub16(tx, kWorldCells), mx);
    std::int16_t dy = sub16(ty, my);  // [bp-6]
    if (abs16(dy) > sar16(kWorldCells, 1) && kind != 6)
        dy = sub16(dy < 0 ? add16(ty, kWorldCells) : sub16(ty, kWorldCells), my);
    std::int16_t h = 0;  // [bp-8]
    switch (a.mode) {
    case 2:
        if (kind == 6 && a.est_pool != 4) {
            // The race's checkpoint bias [BYTES seg13:0xe48-0xef1] (M6 territory, ported as the
            // bytes): seg8:0x19b0(dx, dy), then for a ship with byte +0x149 != 0 and goal pool 8
            // the bias by byte +0x149 toward or away from pool-8 record sbyte +0x148 + 2 gates.
            h = direction16(dy, dx);
            if (a.ship > -1) {
                ShipRecord& S = ship_at(w, a.ship);
                const std::int8_t bias = ship_sb(S, 0x149);
                if (bias != 0 && S.w32 == 8) {
                    const int r = add16(ship_sb(S, 0x148), add16(c.f.g.gates, c.f.g.gates));
                    const std::uint8_t hb = static_cast<std::uint8_t>(h & 0xff);
                    const std::uint8_t bb = static_cast<std::uint8_t>(bias);
                    if (obj(w, kPoolGatePosts8, r).b52 != 0)
                        h = static_cast<std::int16_t>(static_cast<std::uint8_t>(bb + hb) & 0xf);
                    else
                        h = static_cast<std::int16_t>(static_cast<std::uint8_t>(hb - bb) & 0xf);
                }
                // The ship's heading against h decides byte +0x61 [BYTES seg13:0xead-0xef1]
                const std::int16_t frame = obj(w, kPoolShips14, a.ship).frame;
                const std::int16_t gap = abs16(sub16(frame, h));
                set_ship_b(S, pw::kNoCap, (gap < 6 || gap > 10) ? 1 : 0);
            }
        } else if (a.a0 != 0 || kind == 6) {
            // seg8:0x2658(DGROUP:0x9324, S, est pool, est rec), Pascal: the led heading of chaser
            // (ship pool, S) toward target (est pool, est rec) (s3-tick-verify.md claim 40)
            // [BYTES seg13:0xf01-0xf11]
            h = intercept_heading(w.pools, a.est_pool, a.est_rec, c.f.g.ship_pool, a.ship);
            if (kind == 6) set_ship_b(ship_at(w, a.ship), pw::kNoCap, 1);  // [BYTES seg13:0xf14-0xf20]
        } else {
            h = direction16(dy, dx);  // seg8:0x19b0(den = dx, num = dy) [BYTES seg13:0xf27 seg13:0xf7f]
        }
        break;
    case 4:  // [BYTES seg13:0xf2d-0xf48 seg13:0xf7c]
        h = direction16(sub16(0, add16(sar16(dy, 2), dx)), sub16(0, sub16(sar16(dx, 2), dy)));
        break;
    case 3:  // [BYTES seg13:0xf4a-0xf65 seg13:0xf7c]
        h = direction16(sub16(0, sub16(sar16(dy, 2), dx)), sub16(0, add16(sar16(dx, 2), dy)));
        break;
    case 5:
    case 0x1e:  // [BYTES seg13:0xf67-0xf7f]
        h = direction16(sub16(0, dy), sub16(0, dx));
        break;
    default: {
        // [bp-8] is written only by the modes above; the original compares a stale stack word
        // with 0 ... 15. The port keeps the heading, as missile.cpp turn_toward does for the
        // same site [DESIGN ADR 0016 decision 1] (D-4) [BYTES seg13:0xf87].
        static bool once = false;
        log_once(once, "seg13:0xd6c with a mode its switch does not list keeps the heading", "seg13:0xf87");
        return cur;
    }
    }
    if (h < 0 || h > 15) return cur;                  // [BYTES seg13:0xf87-0xf93]
    if (a.mode == 0x1e || a.ship == -100) return h;  // [BYTES seg13:0xf96-0xfa5]
    if (h == cur) return h;                           // [BYTES seg13:0xfa7-0xfad]
    // seg13:0x6716(S) [BYTES seg13:0xfaf-0xfb6]; S = -1 writes DGROUP:0x708c, which the port
    // drops [DESIGN ADR 0016 decision 3].
    if (a.ship >= 0 && a.ship < kShipCount) ship_touched(c.f, a.ship);
    if (h < cur) {  // [BYTES seg13:0xfb7-0xfdd]
        if (sub16(cur, h) < 9) {
            cur = sub16(cur, 1);
        } else {
            cur = add16(cur, 1);
            if (cur > 15) cur = 0;
        }
    } else {  // [BYTES seg13:0xfe0-0xffd]
        if (sub16(h, cur) < 9) {
            cur = add16(cur, 1);
        } else {
            cur = sub16(cur, 1);
            if (cur < 0) cur = 15;
        }
    }
    return cur;
}

// ---------------------------------------------------------------- seg4:0xbde

void steer(SteerCtx& c, int i) {
    World& w = c.w;
    WaveGlobals& g = c.f.g;
    if (i < 0 || i >= kShipCount) return;  // [DESIGN] seg13:0x0 passes 0 ... DGROUP:0x9316
    ShipRecord& S = w.ships[static_cast<std::size_t>(i)];
    Pilot P(S);
    // The served ship while the ambulance holds it [BYTES seg4:0xbed-0xc01]
    if (i == g.dg_952e && g.dg_9e49 >= 1 && g.dg_9e49 <= 6) return;
    std::int16_t& count = c.sg.steers[static_cast<std::size_t>(i)];
    count = add16(count, 1);  // DGROUP:0x1cb8 + 2i [BYTES seg4:0xc05-0xc0a]
    bool nobrake = true;      // [bp-0x16] = 1 [BYTES seg4:0xc0e]
    // The race path [BYTES seg4:0xc13-0xc89] (M6: seg4:0x144c is a logged stub)
    if (g.wave_kind == 6 && (g.dg_9eb5 == 0 || P.goal_pool() == 0)) {
        c.h.race_course(i, (g.detail > 0 || (count & 1) != 0) ? 1 : 0);
        P.est_pool(P.goal_pool());  // [BYTES seg4:0xc59-0xc7a]
        P.est_rec(P.goal_rec());
        P.est_app(P.goal_app());
        P.est_act(P.goal_act());
        if (g.play_mode != 2) c.h.hazard_step(i);  // seg4:0x4fc8(i) [BYTES seg4:0xc7e-0xc89]
    }
    // a0 = 1 when +0x36 != 1 and the estimate is the goal [BYTES seg4:0xc8c-0xcb3]
    const std::int16_t a0 =
        (P.est_act() != 1 && P.est_pool() == P.goal_pool() && P.est_rec() == P.goal_rec()) ? 1 : 0;
    // [bp-0xa], [bp-0xc]: read once, before the forced think [BYTES seg4:0xcb8-0xcc8]
    std::int16_t ep = P.est_pool();
    std::int16_t er = P.est_rec();
    if (ep == -1) {
        c.h.think(i);  // seg4:0x78(i) [BYTES seg4:0xccb-0xcd2]
        // The locals keep -1, so the aim and the turn read pool -1 through the far pointer at
        // DGROUP:0x1df4 (a null selector), which faults in the original (s3-tick-verify.md
        // Omission "Stale estimate"); the port reads the zeroed scratch record of collide.h obj()
        // and passes pool -1 on to the turn, whose led heading then reads 0 [DESIGN ADR 0016
        // decision 2] [BYTES seg4:0xcb8-0xcd5 seg4:0xddb-0xdee].
        static bool once = false;
        log_once(once, "the steer's forced think leaves the estimate pool -1", "seg4:0xcd5");
    }
    ObjectRecord& own = obj(w, kPoolShips14, i);  // DGROUP:0x1e30 + 0x70·i
    // The aim point [bp-0x1a], [bp-0x18] [BYTES seg4:0xcd5-0xdee]
    std::int16_t ax = 0, ay = 0;
    if (ep == 14 && ship_at(w, er).be8 != 0) {
        // A cloaked ship: the hunters' guess +0x14a / +0x14c (seg4:0x83ae) [BYTES seg4:0xcd5-0xcf5]
        const ShipRecord& T = ship_at(w, er);
        ax = ship_w(T, pw::kGuessX);
        ay = ship_w(T, pw::kGuessY);
    } else if (ep == 12 && (er & 1) == 0 && own.on_screen != 0 &&
               obj(w, kPoolGatePosts8, er).on_screen != 0 &&
               obj(w, kPoolGatePosts8, er).scr_y < own.scr_y) {
        // The gate detour: beside and above the gate before turning in [BYTES seg4:0xcf8-0xdd9].
        // The side is seg8:0x29ea of pool-12 record er's own position against the ship's screen
        // x: dx = er·0x70 [BYTES seg4:0xd1b seg4:0xd21] indexes the pool-12 block DGROUP:0x1e28
        // at `push dword es:[bx + di + 4]` with di = dx [BYTES seg4:0xd6b-0xd71], so the record is
        // er itself, the record whose x the detour moves (s3-tick-verify.md's Omission and
        // pilots.md read a fixed +0x74, record 1; the bytes give record er).
        const ObjectRecord& E = obj(w, kPoolGateMids12, er);
        ay = sub16(E.y, 400);  // [BYTES seg4:0xd60-0xd68]
        const Point16 sp = c.h.to_screen(E.x, E.y);
        ax = own.scr_x > sp.x ? add16(E.x, 1400) : sub16(E.x, 1400);  // [BYTES seg4:0xd8d-0xdb9]
        const std::int16_t span = shl16(kWorldCells, 3);  // 8·DGROUP:0x5a0
        if (ax < 0)
            ax = add16(ax, span);  // [BYTES seg4:0xdbc-0xdc6]
        else if (span < ax)
            ax = sub16(ax, span);  // [BYTES seg4:0xdcb-0xdd6]
    } else {
        const ObjectRecord& E = obj(w, ep, er);  // [BYTES seg4:0xddb-0xdee]
        ax = E.x;
        ay = E.y;
    }
    // seg8:0x285a(own position, aim point): dx [bp-6], dy [bp-8] = aim - own [BYTES seg4:0xdf2-0xe12]
    std::int16_t dx = 0, dy = 0;
    const std::int16_t d = distance_cells(ax, ay, own.x, own.y, dx, dy);  // [bp-4]
    // Near (+0x36 != 2, d < 220): the brake orders [BYTES seg4:0xe15-0xebb]
    if (P.est_act() != 2 && d < 220) {
        if ((ep == 14 && P.order() == 5) || P.order() == 0x14) {
            P.est_app(7);  // [BYTES seg4:0xe42-0xe47]
        } else {
            bool brake = false;
            if (ep == 9 && d < 180 && P.goal_pool() == 9 && P.est_act() == 1) {
                brake = true;  // [BYTES seg4:0xe4f-0xe6b]
            } else if (ep == 14) {
                const ShipRecord& T = ship_at(w, er);
                if (ship_w(T, pw::kEstPool) == 14 && ship_w(T, pw::kEstRec) == i &&
                    (abs16(own.vx) > 0x22 || abs16(own.vy) > 0x22))
                    brake = true;  // [BYTES seg4:0xe6d-0xeaf]
            }
            if (brake) {
                thrust_step(c.f, 5, i);  // seg11:0x5ab6(i, 5) [BYTES seg4:0xeb1-0xeb6]
                nobrake = false;
            }
        }
    }
    // Closing fast (+0x36 = 1, d > 100, Detail > 1): (vy·dy + vx·dx) / d < -50, 32-bit
    // [BYTES seg4:0xec0-0xf1e]
    if (P.est_act() == 1 && d > 100 && g.detail > 1) {
        const std::uint32_t pv = static_cast<std::uint32_t>(static_cast<std::int32_t>(own.vy) * dy) +
                                 static_cast<std::uint32_t>(static_cast<std::int32_t>(own.vx) * dx);
        const std::int32_t q = static_cast<std::int32_t>(pv) / static_cast<std::int32_t>(d);
        if (q < -50) {
            thrust_step(c.f, 5, i);
            nobrake = false;
        }
    }
    // The mode [bp-0x12]: 7 passes 3 and becomes 2 [BYTES seg4:0xf23-0xf39]
    std::int16_t mode = P.est_app();
    if (mode == 7) {
        mode = 3;
        P.est_app(2);
    }
    // The race's approach [BYTES seg4:0xf3f-0x1016] (M6 territory, ported as the bytes)
    if (g.wave_kind == 6 && (P.est_app() == 3 || P.est_app() == 4) && g.dg_9eb5 == 0) {
        ep = P.goal_pool();
        P.est_pool(ep);
        er = P.goal_rec();
        P.est_rec(er);
        const int r = add16(ship_sb(S, 0x148), add16(g.gates, g.gates));
        const bool lit = obj(w, kPoolGatePosts8, r).b52 != 0;
        const std::uint8_t b149 = ship_b(S, 0x149);
        if (P.est_app() == 3)
            set_ship_b(S, 0x149, static_cast<std::uint8_t>(lit ? b149 + 2 : b149 - 2));
        else
            set_ship_b(S, 0x149, static_cast<std::uint8_t>(lit ? b149 - 2 : b149 + 2));
        mode = 2;
        P.est_app(2);
        P.est_act(2);
        P.no_cap(1);
        const ObjectRecord& E = obj(w, ep, er);
        ax = E.x;
        ay = E.y;
    }
    // The turn [BYTES seg4:0x101a-0x106b]
    const std::int16_t h0 = own.frame;  // [bp-0xe]
    AimArgs A;
    A.a0 = a0;
    A.est_rec = er;
    A.est_pool = ep;
    A.ship = static_cast<std::int16_t>(i);
    A.mode = mode;
    A.cur = own.frame;
    A.tx = ax, A.ty = ay;
    A.mx = own.x, A.my = own.y;
    own.frame = aim_heading(c, A);
    if (own.frame != h0) ship_touched(c.f, i);  // seg13:0x6716(i) [BYTES seg4:0x105b-0x106b]
    // The thrust [BYTES seg4:0x1071-0x1170]. The step test is on frame - h0 = +1 or -1, so a
    // wrap 15 -> 0 (frame - h0 = -15) counts as a larger turn, as in the original.
    bool small = own.frame == h0;
    if (!small) {
        const std::int16_t delta = sub16(own.frame, h0);
        small = delta == 1 || delta == -1;
    }
    if (!small && g.wave_kind == 6 && P.goal_app() == 2) {
        // A race: a larger turn still thrusts while sbyte +0x149 < 5 (the abs of a flag) and
        // byte DGROUP:0x9eb5 = 0 [BYTES seg4:0x1090-0x10c1]
        if (abs16(ship_sb(S, 0x149) < 5 ? 1 : 0) != 0 && g.dg_9eb5 == 0) small = true;
    }
    if (small && P.goal_act() != 5 && nobrake) {
        // [BYTES seg4:0x10c3-0x10f4]
        if (g.wave_kind == 6 && P.no_cap() == 0 && g.dg_9eb5 == 0)
            thrust_step(c.f, 6, i);
        else
            thrust_step(c.f, 1, i);
    } else if (P.est_app() == 3 || P.est_app() == 4) {
        thrust_step(c.f, 1, i);  // [BYTES seg4:0x10fd-0x111f]
        P.no_cap(1);
    } else {
        thrust_step(c.f, 6, i);  // drift [BYTES seg4:0x1126-0x112b]
        if (g.wave_kind == 6) {  // [BYTES seg4:0x1130-0x116e]
            const std::int16_t t = c.sg.w8e3c[static_cast<std::size_t>(i)];
            if (t < 2) {
                S.turn_acc = idiv16(S.turn, 3);
            } else if (t < 4) {
                S.turn_acc = sar16(sub16(S.turn, S.turn < 0 ? -1 : 0), 1);
            }
        }
    }
    // The veer's turn counter [BYTES seg4:0x1172-0x1194]
    if (P.est_app() == 3 || P.est_app() == 4) S.turn_acc = idiv16(S.turn, 3);
    // Away within 70 cells: a burst [BYTES seg4:0x1198-0x11b5]
    if (P.est_app() == 5 && d < 0x46 && nobrake) thrust_step(c.f, 2, i);
    // The capture approach, only for +0x3a = 2 and +0x36 = 2 [BYTES seg4:0x11ba-0x11cd]
    if (P.est_app() != 2 || P.est_act() != 2) return;
    // Every branch below reaches the touch test seg4:0x139d.
    if ((P.thinks() & 1) == 0 && w.pool2_units > 1 && d < 0xdc && S.fuel > 0xc8 &&
        own.frame == h0 && ep == 0) {
        // The close burst, no rand() [BYTES seg4:0x11d1-0x1203 seg4:0x1393-0x1398]
        thrust_step(c.f, 2, i);
    } else {
        bool far_burst = d > 0x320 && S.w26 > 0x32 && S.fuel > 0xc8 && nobrake;  // [BYTES seg4:0x1207-0x1226]
        if (!far_burst && g.wave_kind == 6 && d > 0x1f4 && own.race_status == 0 && S.fuel > 0xc8)
            far_burst = true;  // [BYTES seg4:0x1228-0x1254]
        if (far_burst && own.frame == h0) {
            // The far burst: one rand() % 18; in a race a second draw [BYTES seg4:0x1256-0x12bc]
            const std::int16_t r = imod16(static_cast<std::int16_t>(c.f.rng.rand()), 0x12);
            bool fire = false;
            if (r == 0 && g.wave_kind != 6) {
                fire = true;
            } else if (g.wave_kind == 6 && own.race_status == 0) {
                const std::uint16_t r2 = c.f.rng.rand();
                if ((r2 & 7) == 0 || ship_sb(S, 0x149) > 2) fire = true;
            }
            if (fire) thrust_step(c.f, 2, i);  // [BYTES seg4:0x12a8-0x12ad]
            P.no_cap(1);                       // [BYTES seg4:0x12b2-0x12b7]
        } else if (d < 0x190) {
            if (d > 0x50 && g.detail > 1) {
                // The tether grab [BYTES seg4:0x12d8-0x1384]
                const ObjectRecord& E = obj(w, ep, er);
                if (E.b53 != 0 && (E.b54 == 0 || ep == 4)) {
                    int k = 1;
                    const int slots = static_cast<std::int8_t>(P.slots());
                    while (k < slots && slot_weapon(S, k) != 2) ++k;
                    if (k < slots && own.w36 != 10 && own.w36 != P.goal_pool()) {
                        bool ok = true;
                        if (ep == 0) {
                            // resources (sign-extended) + credit < 400, signed 32-bit
                            const std::int32_t sum = static_cast<std::int32_t>(
                                static_cast<std::uint32_t>(static_cast<std::int32_t>(P.resources())) + P.credit());
                            if (sum < 0x190) ok = false;  // [BYTES seg4:0x1347-0x1358]
                        }
                        if (ok && ep == 4) {
                            // the design tank DGROUP:0x96f0 + 0xad·n, - 50 < fuel skips
                            // [BYTES seg4:0x1360-0x1376]
                            const int n = S.design_index;
                            const std::int16_t tank =
                                (n >= 0 && n < kShipCount) ? w.design_ratings[static_cast<std::size_t>(n)].tank : 0;
                            if (sub16(tank, 0x32) < S.fuel) ok = false;
                        }
                        if (ok) use_weapon(c, k, i, 0);  // seg4:0x584c(k, i, 0) [BYTES seg4:0x1378-0x1381]
                    }
                }
            } else if (g.play_mode == 2 && d < 0x64) {
                thrust_step(c.f, 2, i);  // [BYTES seg4:0x1386-0x1398]
            }
        } else if (g.play_mode == 2 && d < 0x64) {
            thrust_step(c.f, 2, i);  // [BYTES seg4:0x1386-0x1398]
        }
    }
    // The touch of an unseen ship [BYTES seg4:0x139d-0x143e] (s3-tick-verify.md claim 37 as
    // corrected): the box around the estimate object's own position, the ship's square from
    // its pool-14 object.
    if (own.on_screen != 0) return;
    const ObjectRecord& E = obj(w, ep, er);
    const Rect16 box = rect16(sub16(E.x, 0x90), sub16(E.y, 0x90), add16(E.x, 0x180), add16(E.y, 0x180));
    const Rect16 sq = rect16(own.x, own.y, add16(own.x, own.extent), add16(own.y, own.extent));
    if (!intersect(box, sq)) return;
    // (pool << 8) | cbw(rec byte) [BYTES seg4:0x142a-0x1433]
    const std::uint16_t pr = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(ep & 0xff) << 8) |
        static_cast<std::uint16_t>(static_cast<std::int16_t>(static_cast<std::int8_t>(er & 0xff))));
    c.h.touch(i, static_cast<std::int16_t>(pr));  // seg13:0x16ac(i, pr), cdecl
}

// ---------------------------------------------------------------- seg4:0x5654

std::int16_t weapon_decision(SteerCtx& c, int i) {
    World& w = c.w;
    WaveGlobals& g = c.f.g;
    ShipRecord& S = ship_at(w, i);
    Pilot P(S);
    if (S.weapon_slot == 0) return 0;  // [BYTES seg4:0x5662-0x5670]
    // A tumbling ship goal with W(+0x18)+0x42 != 0: the table is indexed by the slot number
    // itself, not by the slot's weapon, as the original does (s3-tick-verify.md claim 51 (b))
    // [BYTES seg4:0x5673-0x569f]
    if (P.goal_pool() == 0xe && c.h.weapon_word(S.weapon_slot, kWLong) != 0 &&
        ship_at(w, P.goal_rec()).be2 != 0)
        c.h.weapon_choice(i);  // seg4:0x6e70(i)
    if (P.weapon_class() != 3) {
        // The direction test on screen points [BYTES seg4:0x56ae-0x57a0]
        const ObjectRecord& G = obj(w, P.goal_pool(), P.goal_rec());
        Point16 gp{G.scr_x, G.scr_y};
        if (G.on_screen == 0) gp = c.h.to_screen(G.x, G.y);  // [BYTES seg4:0x56d0-0x56ff]
        const ObjectRecord& O = obj(w, kPoolShips14, i);
        Point16 op{O.scr_x, O.scr_y};
        if (O.on_screen == 0) op = c.h.to_screen(O.x, O.y);  // [BYTES seg4:0x5724-0x573e]
        // seg8:0x19b0(den = goal x - own x, num = goal y - own y) [BYTES seg4:0x5747-0x5759]
        const std::int16_t h = direction16(sub16(gp.y, op.y), sub16(gp.x, op.x));
        const std::int16_t hd = O.frame;
        const std::int16_t opp = imod16(add16(hd, 8), 16);  // [BYTES seg4:0x576b-0x5778]
        std::int16_t m = 0;  // [bp-8]
        if (hd == h) {
            m = 1;
        } else if (opp == h) {
            m = 2;
        } else {
            // Neither: [bp-8] keeps a stale stack word in the original (s3-tick-verify.md claim
            // 51 (a)); the port reads 0, so only a class byte +0x5f of 0 (cleared by seg4:0x78's
            // class-2 step or seg4:0x6e70's one-slot exit) passes the compare and fires
            // [DESIGN ADR 0016 decision 1].
            static bool once = false;
            log_once(once, "seg4:0x5654's direction word is unwritten for this heading", "seg4:0x5793");
        }
        if (static_cast<std::int8_t>(P.weapon_class()) != m) return 0;  // cbw [BYTES seg4:0x5793-0x57a0]
    }
    // Fire unless cloaked or shielded within 3000 ms of the player's last use
    // [BYTES seg4:0x57a4-0x57c6]
    const bool open = (S.be8 == 0 && S.type <= 0) || !(w.w8e28 + 3000u > g.now);
    if (open) {
        use_weapon(c, S.weapon_slot, i, 0x19);  // seg4:0x584c(+0x18, i, 25) [BYTES seg4:0x582b-0x583d]
        return 1;
    }
    // The cloak off, then the shield down [BYTES seg4:0x57c8-0x5828]
    if (obj(w, kPoolShips14, i).b2e == 0xfe) {
        const std::int16_t r = c.f.host.seg11_4c72(i);
        if (r != 0 && i == g.player) c.f.host.seg3_332e(0x04090416u);
    }
    if (S.type == 7) {
        const std::int16_t r = c.f.host.seg11_4b00(i);
        if (r != 0 && i == g.player) c.f.host.seg3_332e(0x04080416u);
    }
    return 0;
}

// ---------------------------------------------------------------- seg4:0x584c

void use_weapon(SteerCtx& c, int k, int s, int life) {
    World& w = c.w;
    WaveGlobals& g = c.f.g;
    if (obj(w, kPoolShips14, s).b2e != 0) return;  // [BYTES seg4:0x585a-0x5867]
    if (s == g.player && g.play_mode == 0) w.w8e28 = g.now;  // DGROUP:0x8e28 [BYTES seg4:0x586b-0x587e]
    ShipRecord& S = ship_at(w, s);
    const std::int16_t wpn = slot_weapon(S, k);  // cbw [BYTES seg4:0x5882-0x588f]
    // The waits [BYTES seg4:0x5892-0x58c0]
    if (c.h.weapon_word(wpn, kWLong) == 0) {
        // byte arithmetic: (byte +0x5e << 2) + low byte of W+0x28, wrapping at 8 bits
        const std::uint8_t v = static_cast<std::uint8_t>(
            static_cast<std::uint8_t>(ship_b(S, pw::kClass) << 2) +
            static_cast<std::uint8_t>(c.h.weapon_word(wpn, kWWait) & 0xff));
        set_ship_b(S, pw::kWeaponWait, v);
    } else {
        set_ship_b(S, pw::kWeaponWait, 2);
        set_ship_w(S, pw::kWait2, mul16(c.h.weapon_word(wpn, kWWait), 3));
    }
    c.h.capture_count(s, 5);  // seg13:0x4eec(s, 5), cdecl [BYTES seg4:0x58c4-0x58ce]
    // The launch or the dispatch [BYTES seg4:0x58d1-0x5978] (s3-tick-verify.md claim 49)
    if (c.h.weapon_word(wpn, kWKind) != 0) {
        c.h.launch(w, g, c.f.rng, s, kPoolShips14, wpn);  // seg11:0x7c12(w, 14, s), Pascal
    } else if (wpn == 5 || wpn == 4) {
        c.h.fuel_ball(kPoolShips14, s, life, wpn);  // seg13:0x1318(14, s, life, w), Pascal
    } else if (wpn == 1) {
        c.h.ring_of_death(kPoolShips14, s);  // seg11:0x8a42(14, s)
    } else if (wpn == 2 || wpn == 0xf) {
        c.h.tether_use(kPoolShips14, s, wpn);  // seg11:0x947a(14, s, w)
    } else if (wpn == 3) {
        c.h.lightning(kPoolShips14, s);  // seg11:0x9fdc(14, s)
    } else if (wpn == 0x17) {
        c.h.shock(kPoolShips14, s);  // seg11:0x9daa(14, s) (M6)
    } else if (wpn == 0xd) {
        c.h.wildcard(kPoolShips14, s);  // seg11:0x9078(14, s)
    }
    // The Strategy player's slot [BYTES seg4:0x597b-0x599f]
    if (s == g.player && g.play_mode != 0 && S.weapon_slot != k) {
        c.sg.w9524 = static_cast<std::int16_t>(k);
        S.weapon_slot = static_cast<std::int16_t>(k);
        c.h.weapon_display();  // seg11:0x2f8c
    }
    S.weapon_slot = static_cast<std::int16_t>(k);  // [BYTES seg4:0x59a4-0x59ac]
}

}  // namespace is::world
