// The flight model and the ship mover (flight.h). Each routine follows the original's
// instructions in address order; the comments cite the addresses the steps come from.
#include "world/flight.h"

#include "world/input.h"

namespace is::world {

const std::array<std::uint8_t, 91> kSineTable = {
    0,   2,   4,   6,   9,   11,  13,  15,  18,  20,  22,  24,  27,  29,  31,  33,  35,  38,  40,
    42,  44,  46,  49,  51,  53,  55,  57,  59,  61,  63,  65,  67,  69,  71,  73,  74,  76,  78,
    80,  82,  83,  85,  87,  88,  90,  92,  93,  95,  96,  98,  99,  101, 102, 103, 105, 106, 107,
    108, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 119, 120, 121, 122, 122, 123, 123, 124,
    124, 125, 125, 125, 126, 126, 127, 127, 127, 127, 127, 127, 128, 128, 128};

namespace {

std::int16_t table(int i) {
    if (i < 0 || i > 90) return 0;  // outside DGROUP:0xf00 ... 0xf5a [OPEN]
    return kSineTable[static_cast<std::size_t>(i)];
}

// (a + 360) % 360 with a 16-bit add and `idiv` [BYTES seg6:0x71bf-0x71ca].
std::int16_t fold360(std::int16_t a) { return imod16(add16(a, 360), 360); }

// A dword compare `hi ja / jb, lo ja` as the original writes it: d > now, unsigned 32-bit.
bool after(const Dword& d, std::uint32_t now) { return d.get() > now; }

}  // namespace

// [BYTES seg6:0x71cc-0x7203]
std::int16_t sin128(std::int16_t a) {
    const std::int16_t r = fold360(a);
    if (r < 90) return table(r);                        // DGROUP:0xf00 + r
    if (r < 180) return table(180 - r);                 // byte 0xfb4 - r
    if (r < 270) return sub16(0, table(r - 180));       // -byte 0xe4c + r
    return sub16(0, table(360 - r));                    // -byte 0x1068 - r
}

// [BYTES seg6:0x7226-0x725d]
std::int16_t cos128(std::int16_t a) {
    const std::int16_t r = fold360(a);
    if (r < 90) return table(90 - r);                   // byte 0xf5a - r
    if (r < 180) return sub16(0, table(r - 90));        // -byte 0xea6 + r
    if (r < 270) return sub16(0, table(270 - r));       // -byte 0x100e - r
    return table(r - 270);                              // byte 0xdf2 + r
}

// [BYTES seg8:0x35df-0x367b]
std::int16_t magnitude(std::int32_t a, std::int32_t b) {
    const std::int32_t ab = abs32(b);  // [bp-0xa]
    const std::int32_t aa = abs32(a);  // [bp-6]
    std::int32_t g;
    if (aa < ab)
        g = static_cast<std::int32_t>(static_cast<std::uint32_t>(sar32(aa, 1)) +
                                      static_cast<std::uint32_t>(ab));
    else
        g = static_cast<std::int32_t>(static_cast<std::uint32_t>(sar32(ab, 1)) +
                                      static_cast<std::uint32_t>(aa));
    if (g == 0) return 1;  // [BYTES seg8:0x367b]
    // seg5:0x314 is the RTL's 32-bit multiply: the low 32 bits [BYTES seg8:0x3646 seg8:0x3657].
    const std::uint32_t sq = static_cast<std::uint32_t>(b) * static_cast<std::uint32_t>(b) +
                             static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(a);
    const std::int32_t q = static_cast<std::int32_t>(sq) / g;  // idiv ecx
    const std::int32_t r = sar32(static_cast<std::int32_t>(static_cast<std::uint32_t>(q) +
                                                           static_cast<std::uint32_t>(g)),
                                 1);
    return lo16(r);
}

// [BYTES seg8:0x19bf-0x1b46]
std::int16_t direction16(std::int16_t num, std::int16_t den) {
    const std::int32_t n = num;
    const std::int32_t d = den == 0 ? 1 : den;
    const std::int32_t r = static_cast<std::int32_t>(static_cast<std::uint32_t>(n) * 100u) / d;
    const bool pos = n > 0;
    std::int16_t out = 0;
    if (r > 500 || r < -500) out = pos ? 8 : 0;
    if (r < 501 && r > 154) out = pos ? 7 : 15;
    if (r < 155 && r > 65) out = pos ? 6 : 14;
    if (r < 66 && r > 19) out = pos ? 5 : 13;
    if (r < 20 && r > -20) out = den > 0 ? 4 : 12;  // [bp-0xe]: den sign-extended
    if (r < -19 && r > -65) out = pos ? 11 : 3;
    if (r < -64 && r > -154) out = pos ? 10 : 2;
    if (r < -153 && r > -501) out = pos ? 9 : 1;
    return out;
}

// seg8:0x1b54 [BYTES seg8:0x1b62-0x1cb3]
void integrate(FlightCtx& c, std::int16_t steps, std::int16_t pool_rec) {
    const int pool = pool_of(pool_rec);
    const int rec = rec_of(pool_rec);
    ObjectRecord& o = c.pools.rec(pool, rec);
    // The literal pool 14, not DGROUP:0x9324 [BYTES seg8:0x1b72].
    if (pool == 0x0e && o.b54 != 0 && c.g.demons_gate != 0 && c.g.dg_9e49 == 10) return;
    if (o.owner_pool != -1 && pool != 10) c.host.follow_owner(pool, rec);  // seg8:0x1e32
    std::int16_t saved = 0;
    const bool pool5 = pool == c.g.pool5;  // DGROUP:0x9328 [BYTES seg8:0x1bb2-0x1bc0]
    if (pool5) {
        saved = c.g.dg_9006;
        c.g.dg_9006 = 10;
    }
    if (steps == 1) {
        o.x = add16(o.x, o.vx);
        o.y = add16(o.y, o.vy);
    } else {
        o.x = add16(o.x, mul16(o.vx, steps));
        o.y = add16(o.y, mul16(o.vy, steps));
    }
    if (c.g.dg_9130 != 0) return;  // seg8:0x1c23 skips the wrap and the restore
    if (o.x > kWorldUnits) o.x = 0;
    if (o.x < 0) o.x = kWorldUnits;
    if (o.y > kWorldUnits) o.y = 0;
    if (o.y < 0) o.y = kWorldUnits;
    if (pool5) c.g.dg_9006 = saved;  // [BYTES seg8:0x1ca5-0x1cb0]
}

// seg8:0x1d54 [BYTES seg8:0x1d72-0x1e27]
void limit_speed(Pools& pools, std::int16_t limit, std::int16_t pool_rec) {
    ObjectRecord& o = pools.rec(pool_of(pool_rec), rec_of(pool_rec));
    std::int16_t sx, sy, s;
    if (o.vx > 0) {
        s = o.vx;
        sx = 1;
    } else {
        s = sub16(0, o.vx);
        sx = -1;
    }
    if (o.vy > 0) {
        s = add16(s, o.vy);
        sy = 1;
    } else {
        s = sub16(s, o.vy);
        sy = -1;
    }
    if (s < limit) return;
    if (o.vx != 0) o.vx = sub16(o.vx, sx);
    if (o.vy != 0) o.vy = sub16(o.vy, sy);
}

// seg13:0x6716 [BYTES seg13:0x6720-0x6753]
void ship_touched(FlightCtx& c, int ship) {
    if (ship == c.g.player && c.g.autopilot == 0 && c.g.dg_238c == 0) {
        c.host.player_sprite();
        return;
    }
    c.ships[static_cast<std::size_t>(ship)].due156.set(c.g.now - 1u);
}

// seg11:0xb09c [BYTES seg11:0xb0db-0xb100]
void flame(FlightCtx& c, int on, int ship) {
    ObjectRecord& o = c.ship_obj(ship);
    const ShipRecord& s = c.ships[static_cast<std::size_t>(ship)];
    o.flame = on != 0 ? static_cast<std::uint8_t>(idiv16(s.thrust, 20)) : 0;
    c.host.flame_tail(on, ship);
}

// seg4:0x0 [BYTES seg4:0xb-0x68]
void spin(FlightCtx& c, int ship) {
    ObjectRecord& o = c.pools.rec(kPoolShips, ship);  // DGROUP:0x1e30, pool 14 directly
    if (o.angle != 0 && c.g.dg_238c == 0) c.host.player_sprite();
    ShipRecord& s = c.ships[static_cast<std::size_t>(ship)];
    if (s.spin == 0) return;
    o.frame = add16(o.frame, s.spin);
    if (o.frame > 15) o.frame = 0;
    if (o.frame < 0) o.frame = 15;
    c.host.player_sprite();
}

// seg11:0x592a [BYTES seg11:0x5938-0x5aa7]. It runs only on the cadence's add frames (move_ships,
// seg13:0x616-0x688), and the accumulator +0x16 runs whether a key is held or not, so with the
// player's turn word 21 a key held 10 ... 12 frames (a 0.3 s press at 36 frames per second)
// turns 3 or 4 steps by +0x16 at key-down: the oracle turned 4 for Left and 3 for Right in one
// run [BYTES seg13:0x61b seg13:0x654 seg13:0x688] [ORACLE wave-0001] (tests/test_world_flight.cpp).
void turn_input(FlightCtx& c) {
    WaveGlobals& g = c.g;
    if (g.dg_1034 == 0) return;
    if (g.wave_kind == 6 && g.dg_9eb4 == 0) return;
    if (g.play_mode != 0) return;
    ShipRecord& s = c.ships[static_cast<std::size_t>(g.player)];
    if (s.beb != 0) return;
    if (!(s.lockout.get() < g.now)) return;  // jb seg11:0x5971: lock-out before now
    ObjectRecord& o = c.ship_obj(g.player);
    // VK_RIGHT, then VK_LEFT: both can run (seg11:0x5971, seg11:0x5a0f).
    if (c.keys.test(kVkRight, kMaskDown) || g.joy_right != 0) {
        if (c.keys.test(kVkMenu, kMaskDown)) {  // GetAsyncKeyState(VK_MENU) & 0x8000
            s.spin = add16(s.spin, 1);
            if (s.spin > 1) s.spin = 1;
        } else {
            o.angle = 0;
            s.spin = 0;
            o.frame = add16(o.frame, 1);
            if (o.frame > 15) o.frame = 0;
        }
        if (g.dg_238e == 0) c.host.player_sprite();  // seg11:0x5a04
        g.turned = 1;                                // DGROUP:0x9138
    }
    if (c.keys.test(kVkLeft, kMaskDown) || g.joy_left != 0) {
        if (c.keys.test(kVkMenu, kMaskDown)) {
            s.spin = sub16(s.spin, 1);
            if (s.spin < -1) s.spin = -1;
        } else {
            o.angle = 0;
            s.spin = 0;
            o.frame = sub16(o.frame, 1);
            if (o.frame < 0) o.frame = 15;
        }
        if (g.dg_238e == 0) c.host.player_sprite();  // seg11:0x5aa2
        g.turned = 1;
    }
}

// The go flag [BYTES seg11:0x5b30-0x5c1c]
bool go_flag(const ShipRecord& s, const WaveGlobals& g, std::int16_t mode, bool action_player) {
    bool go = s.w2a != 0;
    if (s.w2a < 50) {
        const bool kind67 = g.wave_kind == 6 || g.wave_kind == 7;
        if ((!kind67 || s.fuel > 50) && g.cycle < 4) go = false;
    }
    if (s.fuel < 50) {
        go = false;
        std::int16_t f = idiv16(s.fuel, 12);
        if (f < 2) f = 2;
        if (!(f < idiv16(g.cycle, 2)) || mode == 1) go = true;
        if (!action_player && s.w32 == 4 && s.w38 == 2) go = true;
    }
    if (after(s.lockout, g.now)) {
        go = false;
    } else if (g.wave_kind == 6) {
        // +0x84 + 400 > now (32-bit add with carry) [BYTES seg11:0x5c03-0x5c1a]
        const std::uint32_t t = s.lockout.get() + 400u;
        if (t > g.now) go = false;
    }
    return go;
}

// [BYTES seg11:0x6630-0x66a9]
std::int16_t thrust_force(const ShipRecord& s, std::int16_t roster_byte) {
    std::int16_t t = s.thrust;
    if (s.roster_index != -1) {
        // (lo16((b + 10) * 10) * T) / 100, the product 32-bit, seg5:0x110 [BYTES seg11:0x6652-0x665c]
        const std::int16_t f = mul16(add16(roster_byte, 10), 10);
        const std::int32_t p = static_cast<std::int32_t>(f) * static_cast<std::int32_t>(t);
        t = lo16(div32(p, 100));
    }
    if (s.w2a == 0 || s.fuel == 0) {
        t = idiv16(t, 3);
    } else if (s.w2a < 35) {
        t = sub16(t, sar16(mul16(sub16(40, s.w2a), t), 6));
    }
    if (t < 1) t = 1;
    return t;
}

// [BYTES seg11:0x66b3-0x6713]
Dv thrust_dv(std::int16_t heading, std::int16_t t, std::int16_t k, std::int16_t drag,
             std::int16_t mass, std::int16_t vx, std::int16_t vy) {
    Dv d;
    const std::int16_t ax = sub16(mul16(mul16(heading_dx(heading), t), k), mul16(drag, vx));
    d.dx = mass == 0 ? 0 : idiv16(ax, mass);  // m = 0 traps in the original [OPEN]
    const std::int16_t ay = sub16(mul16(mul16(heading_dy(heading), t), k), mul16(drag, vy));
    d.dy = mass == 0 ? 0 : idiv16(ay, mass);
    return d;
}

// [BYTES seg11:0x6120-0x6282]
Dv drift_dv(std::int16_t heading, std::int16_t drag, std::int16_t mass, std::int16_t vx,
            std::int16_t vy, bool autopilot) {
    const std::int32_t sp = magnitude(vx, vy);           // seg8:0x35d0, cdq
    const std::int16_t dir = direction16(vy, vx);        // seg8:0x19b0(num = vy, den = vx)
    const std::int16_t a = add16(mul16(sub16(0, sub16(heading, dir)), 23), 360);
    const std::int32_t d32 = drag;
    std::int16_t along = 0, across = 0;
    {
        const std::int32_t den = static_cast<std::int32_t>(static_cast<std::uint32_t>(
                                     static_cast<std::int32_t>(mass)) << 9);
        const std::uint32_t num = static_cast<std::uint32_t>(static_cast<std::int32_t>(cos128(a))) *
                                  static_cast<std::uint32_t>(d32) * static_cast<std::uint32_t>(sp);
        along = den == 0 ? 0 : lo16(div32(static_cast<std::int32_t>(num), den));
    }
    {
        const std::int32_t den = static_cast<std::int32_t>(mass) * 178;  // imul word: 32-bit
        const std::uint32_t num = static_cast<std::uint32_t>(static_cast<std::int32_t>(sin128(a))) *
                                  static_cast<std::uint32_t>(d32) * static_cast<std::uint32_t>(sp);
        across = den == 0 ? 0 : lo16(div32(static_cast<std::int32_t>(num), den));
    }
    if (autopilot) {  // [BYTES seg11:0x621c-0x6234]
        along = idiv16(shl16(along, 2), 3);
        across = idiv16(shl16(across, 2), 3);
    }
    const int h4 = imod16(add16(heading, 4), 16);
    Dv d;
    d.dx = idiv16(add16(mul16(heading_dx(heading), along), mul16(heading_dx(h4), across)), -20);
    d.dy = idiv16(add16(mul16(heading_dy(heading), along), mul16(heading_dy(h4), across)), -20);
    return d;
}

// seg11:0x63b4 [BYTES seg11:0x63c3-0x6865]
void thrust(FlightCtx& c, std::int16_t mode, int ship) {
    WaveGlobals& g = c.g;
    if (g.dg_9e49 >= 1 && g.dg_9e49 <= 6 && ship == g.dg_952e && g.dg_9130 == 0) return;
    ShipRecord& s = c.ships[static_cast<std::size_t>(ship)];
    ObjectRecord& o = c.ship_obj(ship);
    // Flame on, at most once per 250 ms [BYTES seg11:0x63e2-0x6440].
    if (!(s.flame_until.get() >= g.now)) {
        s.flame_until.set(g.now + 250u);
        if (s.thrusting == 0) {
            s.thrusting = 1;
            flame(c, 1, ship);
            o.angle = 0;
            s.spin = 0;
            ship_touched(c, ship);
        }
    }
    // The OverThruster [BYTES seg11:0x6446-0x65e2].
    const std::uint16_t ctrl = g.play_mode != 0 ? 0 : c.keys.async(kVkControl);
    if (ship == g.player && g.joy_overthrust != 0) g.dg_952a = 1;
    if (s.overthrust_cool < 4) {
        const bool ctrl_hit = (ctrl & 0x8000) != 0 && ship == g.player;
        if ((ctrl_hit || g.dg_952a != 0) && s.fuel >= 30) {
            if (o.on_screen != 0) {
                c.host.sound(0x29, g.ship_pool, ship, 1);
                for (BurstEntry& e : g.burst) {  // 10 x 4 entries, stride 0x16
                    e.w8 = 0;
                    e.w0 = -100;
                    e.w2 = -100;
                }
                g.burst_pool = g.ship_pool;
                g.burst_rec = static_cast<std::int16_t>(ship);
                g.burst_count = 0;
                if (g.play_mode != 0 && g.play_mode != 2 && ship == g.player && s.w3a == 5 &&
                    s.w06 == 8) {
                    c.host.command_mode_line();  // seg11:0x6544-0x65a2 [OPEN]
                }
                if (s.overthrust_cool != 0 && !c.action_player(ship))
                    s.overthrust_cool = 0x1c;
                else
                    s.overthrust_cool = 8;
            }
            s.thrust_mult = 5;
            s.fuel = sub16(s.fuel, 25);
        }
    }
    std::int16_t t = 0, k = 0;
    if (mode != 1) {
        const std::int16_t h = o.frame;
        const std::int16_t m = o.mass;
        k = s.thrust_mult;
        if (k > 1 && s.w3a == 5) k = add16(k, 2);
        t = thrust_force(s, s.roster_index != -1 ? c.host.roster_byte_33(s.roster_index) : 0);
        const Dv d = thrust_dv(h, t, k, s.drag, m, o.vx, o.vy);
        o.vx = add16(o.vx, d.dx);
        o.vy = add16(o.vy, d.dy);
        // The race timer path of kind-6 waves [BYTES seg11:0x6730-0x6846] (M6).
        if (g.wave_kind == 6 && !(g.race_timer >= g.now) && g.dg_9eb5 == 0) c.host.race_thrust(ship);
    }
    // Mode 1 reaches the store with T ([bp - 0x14]) and k ([bp - 0x10]) never written in that
    // call, so the original's +0x2f is stale stack; the port uses 0 (world.md "Flame length",
    // M3 s3 fixer, C054) [BYTES seg11:0x65e7 seg11:0x684a] [DESIGN no ADR yet].
    o.flame = static_cast<std::uint8_t>(idiv16(mul16(t, k), 20));  // [BYTES seg11:0x684a-0x6865]
}

// seg11:0x6874 [BYTES seg11:0x6883-0x6a9a]
void brake(FlightCtx& c, int ship) {
    WaveGlobals& g = c.g;
    ObjectRecord& o = c.ship_obj(ship);
    if (o.vx == 0 && o.vy == 0) return;
    o.angle = 0;
    ShipRecord& s = c.ships[static_cast<std::size_t>(ship)];
    if (!(s.flame_until.get() >= g.now)) {
        s.flame_until.set(g.now + 250u);
        if (s.thrusting != 0) {
            s.thrusting = 0;
            flame(c, 0, ship);
        }
    }
    const std::int16_t m = o.mass;
    // B = m/30 + T0/8 + D + 30, 16-bit [BYTES seg11:0x6909-0x693b]
    const std::int16_t b =
        add16(add16(add16(idiv16(m, 30), idiv16(s.thrust, 8)), s.drag), 30);
    std::int16_t dvx = 0, dvy = 0;
    if (m != 0) {
        dvx = sub16(0, lo16(div32(static_cast<std::int32_t>(b) * o.vx, m)));
        dvy = sub16(0, lo16(div32(static_cast<std::int32_t>(b) * o.vy, m)));
    }
    if (abs16(o.vx) < abs16(dvx)) dvx = sub16(0, o.vx);
    if (abs16(o.vy) < abs16(dvy)) dvy = sub16(0, o.vy);
    if (dvx == 0 && dvy == 0) {
        if (o.vx > 0) dvx = -1;
        else if (o.vx < 0) dvx = 1;
        if (o.vy > 0) dvy = -1;
        else if (o.vy < 0) dvy = 1;
    }
    o.vx = add16(o.vx, dvx);
    o.vy = add16(o.vy, dvy);
}

// seg11:0x5ab6 [BYTES seg11:0x5ac5-0x63a8]
void thrust_step(FlightCtx& c, std::int16_t mode, int ship) {
    WaveGlobals& g = c.g;
    if (g.wave_kind == 6 && g.dg_9eb4 == 0) return;
    const bool action = c.action_player(ship);
    bool auto_flag = false;  // [bp-0x1c]
    if (!action && g.dg_9e49 == 0 && ship == g.dg_952e) auto_flag = true;
    if (g.autopilot != 0 && ship == g.player) auto_flag = true;
    if (ship == g.dg_952e && g.dg_9e49 >= 3 && g.dg_9e49 <= 6 && g.dg_9130 == 0) return;
    ShipRecord& s = c.ships[static_cast<std::size_t>(ship)];
    ObjectRecord& o = c.ship_obj(ship);
    const bool go = go_flag(s, g, mode, action);
    // Multiplier decay [BYTES seg11:0x5c26-0x5c33]
    s.thrust_mult = sub16(s.thrust_mult, 1);
    if (s.thrust_mult < 1) s.thrust_mult = 1;
    if (mode == 2) {
        g.dg_952a = 1;
        mode = 1;
    } else {
        g.dg_952a = 0;
    }
    bool no_thrust = false;  // [bp-0x1a]
    bool flag = true;        // [bp-0x1e]
    if (s.b61 != 0 && s.w2a > 75) flag = false;  // byte +0x61 [BYTES seg11:0x5c5d-0x5c6b]
    if (s.w32 == 12) flag = true;
    if (g.wave_kind == 6) flag = o.race_status != 0;
    if (action || !flag) {
        if (g.wave_kind != 6 && s.drag != 0)
            s.laps = idiv16(mul16(s.thrust, 19), s.drag);  // +0xda [BYTES seg11:0x5cc1-0x5cd0]
    } else {
        c.host.seg11_ai(ship);  // seg11:0x5cd7-0x5f11, the computer ships' speed cap (M5)
    }
    if (auto_flag) {  // [BYTES seg11:0x5f1d-0x5f75]
        std::int16_t v = idiv16(s.laps, 2);
        if (v > 40) v = 40;
        s.laps = v;
        no_thrust = true;
        if (abs16(o.vx) > 30 || abs16(o.vy) > 30) mode = 1;
    }
    // Brake or thrust [BYTES seg11:0x5f7a-0x600b]
    const bool down = c.keys.test(kVkDown, kMaskDownOrPressed);
    if ((down && g.play_mode == 0 && ship == g.player && go) || (mode == 5 && go)) {
        brake(c, ship);
        return;  // jmp seg11:0x62ed, which returns for the action player
    }
    const bool up = c.keys.test(kVkUp, kMaskDownOrPressed) || g.joy_thrust != 0;
    if (up && g.play_mode == 0 && mode == 4 && go && !no_thrust) {
        thrust(c, mode, ship);
        return;
    }
    if (((s.thrusting != 0 && mode == 3) || mode == 1) && go && !no_thrust) {
        thrust(c, mode, ship);
        return;
    }
    // Drift and drag [BYTES seg11:0x600d-0x62e9]
    if (s.be2 != 0 && g.dg_9130 == 0) return;
    if (s.thrusting != 0) {
        s.thrusting = 0;
        flame(c, 0, ship);
        ship_touched(c, ship);
    }
    const std::int16_t d = s.drag;
    const std::int16_t m = o.mass;
    Dv dv;
    if (g.dg_9130 != 0) {
        const std::int16_t m4 = shl16(m, 2);
        if (m4 != 0) {
            dv.dx = sub16(0, idiv16(mul16(d, o.vx), m4));
            dv.dy = sub16(0, idiv16(mul16(o.vy, d), m4));
        }
    } else if (g.braking != 0 && ship == g.player && g.play_mode == 0) {
        if (m != 0) {
            const std::int16_t c0 = sub16(sub16(sub16(idiv16(m, -60), idiv16(s.thrusting, 8)), d), 30);
            dv.dx = idiv16(mul16(c0, o.vx), m);
            dv.dy = idiv16(mul16(c0, o.vy), m);
        }
    } else if (g.detail > 2) {
        dv = drift_dv(o.frame, d, m, o.vx, o.vy, no_thrust);
    } else {
        const std::int16_t m8 = shl16(m, 3);
        if (m8 != 0) {
            dv.dx = sub16(0, idiv16(mul16(o.vx, d), m8));
            dv.dy = sub16(0, idiv16(mul16(o.vy, d), m8));
        }
    }
    o.vx = add16(o.vx, dv.dx);
    o.vy = add16(o.vy, dv.dy);
    // seg11:0x62ed: the escort slow-down for ships other than the action player (M5).
    if (!action && s.w3a == 2 && s.w38 == 3) c.host.seg11_ai(ship);
}

// seg13:0x0 [BYTES seg13:0x0-0xd67]
void move_ships(FlightCtx& c) {
    WaveGlobals& g = c.g;
    c.host.seg13_675e();
    g.cycle = add16(g.cycle, 1);  // [BYTES seg13:0x18-0x23]
    if (g.cycle > 8) g.cycle = 0;
    g.dg_134e = add16(g.dg_134e, 1);
    if (g.dg_134e > 100) g.dg_134e = 0;
    const std::int16_t n = add16(g.last_ship, 1);  // [BYTES seg13:0x3a-0x48]
    c.pools.set_in_use(g.ship_pool, n);
    c.pools.set_allocated(g.ship_pool, n);
    bool due9000 = false;  // [bp-0xe]
    if (!(g.dg_1350 >= g.now)) {
        due9000 = true;
        g.dg_1350 = g.now + 9000u;
    }
    for (int i = 0; i <= g.last_ship; ++i) {
        if (i >= c.pools.records(g.ship_pool)) break;  // [DESIGN] storage bound
        ObjectRecord& o = c.ship_obj(i);
        ShipRecord& s = c.ships[static_cast<std::size_t>(i)];
        const std::int16_t size = o.size;     // [bp-0x1a]
        const std::int16_t gap = o.gap;       // [bp-0x1c]
        const std::int16_t type = s.type;     // [bp-0x22]
        const std::int16_t bea = static_cast<std::int8_t>(s.bea);  // [bp-0xa]
        if (o.state != 0) continue;  // [BYTES seg13:0xd37]
        // The move [BYTES seg13:0x7c-0xaf seg13:0xd3e-0xd5c]
        if (i == g.player && (g.dg_913a == 2 || g.dg_913c == 2)) {
            c.host.seg13_3b88();
        } else if (i == g.dg_952e && g.dg_9e49 >= 1 && g.dg_9e49 <= 6) {
            c.host.seg13_3748(i);
        } else {
            integrate(c, 1, pool_rec(g.ship_pool, i));
        }
        // [BYTES seg13:0xb0-0x18c]
        bool special = s.bec == 0;
        if (!special && i == g.dg_952e && g.dg_9e5b == 0 && g.dg_9e49 == 2) special = true;
        if (special) {
            if (s.be2 == 0) return;  // seg13:0xe1: the remaining ships are skipped
            o.angle = 1;
            if (s.turn_acc < 1) {
                o.frame = c.host.seg13_100c(i, o.frame);
                ship_touched(c, i);
                s.turn_acc = 1;
            } else {
                s.turn_acc = sub16(s.turn_acc, 1);
            }
            if (s.bec == 0 && s.lockout.get() < g.now) {  // [BYTES seg13:0x146-0x181]
                s.be2 = 0;
                o.angle = 0;
                c.host.seg12_274a(g.ship_pool, i);
            }
            goto tail;  // jmp seg13:0x68d
        }
        if (s.be2 != 0) {  // [BYTES seg13:0x18c-0x221]
            o.frame = c.host.seg13_100c(i, o.frame);
            ship_touched(c, i);
            if (s.lockout.get() < g.now) {
                s.be2 = 0;
                o.angle = 0;
            } else if (g.wave_kind == 6) {
                if (g.play_mode == 0 && i == g.player)
                    c.host.seg4_277a();
                else
                    c.host.seg4_144c(i, 0);
            }
            goto tail;
        }
        // [BYTES seg13:0x224-0x255]
        if (c.action_player(i)) {
            spin(c, i);
            if ((g.cycle & 1) == 0) thrust_step(c, 4, i);
        } else if ((g.cycle & 1) == 0) {
            thrust_step(c, 3, i);
        }
        // Body thrust [BYTES seg13:0x25a-0x3db]
        if (s.has_flame == 0 && s.thrusting != 0 && o.b2e == 0 &&
            (o.on_screen != 0 || c.host.seg13_59e8(i))) {
            std::int16_t& cnt = g.body_anim[static_cast<std::size_t>(i)];
            cnt = add16(cnt, 1);
            if (cnt > 1) {
                cnt = 0;
                s.anim_frame = static_cast<std::uint8_t>(s.anim_frame + static_cast<std::uint8_t>(s.anim_step));
                const auto frame = static_cast<std::int8_t>(s.anim_frame);
                const auto last = static_cast<std::int8_t>(s.anim_last);
                if (s.anim_wrap != 0) {
                    if (!(last >= frame)) s.anim_frame = 0;
                } else if (s.anim_step == 1 && !(last >= frame)) {
                    s.anim_frame = static_cast<std::uint8_t>(s.anim_frame - 1);
                    s.anim_step = -1;
                } else if (frame < 0) {
                    s.anim_frame = static_cast<std::uint8_t>(s.anim_frame + 1);
                    s.anim_step = 1;
                }
                o.src_y = add16(mul16(static_cast<std::int8_t>(s.anim_frame), add16(gap, size)), gap);
                ship_touched(c, i);
            }
        } else if (s.has_flame == 0 && s.anim_frame != 0 && o.on_screen != 0) {
            o.src_y = gap;
            s.anim_frame = 0;
            s.anim_step = 1;
            ship_touched(c, i);
        } else if (s.be9 != 0 && o.on_screen != 0 && type > 0 && type < 7) {
            if (bea != 0)
                s.type = add16(s.type, 1);
            else
                s.type = sub16(s.type, 1);  // [bp-0x22] keeps the old type for the sprite
            flame(c, s.thrusting, i);
            ship_touched(c, i);
        }
        {   // [BYTES seg13:0x3e3-0x4ae]: damage effects (M4) on the 0x134e cycle
            const std::int16_t d2 = shl16(g.detail, 1);
            const std::int16_t div1 = sub16(10, d2 < 5 ? 5 : d2);
            const std::int16_t div2 = sub16(18, shl16(g.detail, 2));
            bool run = false;
            const bool rem1 = div1 == 0 || imod16(g.dg_134e, div1) != 0;  // /0 traps [OPEN]
            if (!rem1 && o.on_screen != 0) {
                run = true;
            } else {
                const bool rem2 = div2 == 0 || imod16(g.dg_134e, div2) != 0;
                run = !rem2 || g.wave_kind == 6;
            }
            if (run) {
                std::int16_t r = 0;
                bool called = false;
                if (s.be8 != 0) {
                    if (s.w2c < 50 && imod16(static_cast<std::int16_t>(c.rng.rand()),
                                              add16(shl16(s.w2c, 1), 1)) == 0) {
                        r = c.host.seg11_4c72(i);
                        called = true;
                    }
                } else if (s.bea != 0 && s.w2c < 50 &&
                           imod16(static_cast<std::int16_t>(c.rng.rand()),
                                  add16(shl16(s.w2c, 1), 1)) == 0) {
                    r = c.host.seg11_4b00(i);
                    called = true;
                }
                if (called && r != 0 && i == g.player) c.host.seg3_332e(0x04090417u);
                // [BYTES seg13:0x4ae-0x4d8]
                if (c.action_player(i)) {
                    if (g.wave_kind == 6) c.host.seg4_277a();
                } else {
                    c.host.seg4_78(i);
                }
            }
        }
        // [BYTES seg13:0x4dd-0x533]
        if (due9000 && s.w38 == 1 && !c.action_player(i)) c.host.seg4_6e70(i);
        if (g.wave_kind == 6 && g.game_frames % 9u == 0 && i == g.player) c.host.seg11_4108();
        if (g.wave_kind == 6 && g.dg_9eb4 != 1) {
            c.host.race_step(i);  // seg13:0x545-0x614 (M6)
            goto tail;
        }
        // The turn cadence [BYTES seg13:0x616-0x688]
        if (s.turn_acc < 10) {
            const std::int16_t add =
                (o.on_screen != 0 || g.wave_kind == 6) ? s.turn : mul16(s.turn, 3);
            s.turn_acc = add16(s.turn_acc, add);
            if (c.action_player(i))
                turn_input(c);
            else
                c.host.seg4_bde(i);
        } else {
            s.turn_acc = sub16(s.turn_acc, 10);
        }
    tail:
        // [BYTES seg13:0x68d-0x6e2]: smoke from a damaged ship on screen (M4)
        if (o.on_screen != 0 && s.w26 < 50) {
            const std::int16_t r = imod16(static_cast<std::int16_t>(c.rng.rand()), add16(s.w26, 30));
            if (r < 2 && !(i == g.dg_952e && g.dg_9e49 == 4)) c.host.puff(g.ship_pool, i, 1);
        }
        if (g.detail > 1) c.host.seg13_6e5(i);  // seg13:0x6e5-0xa57 [OPEN]
        // The sprite of a ship other than the pinned player [BYTES seg13:0xa57-0xb55]
        if (o.on_screen != 0 && !(i == g.player && g.autopilot == 0) &&
            !(g.dg_9e49 >= 3 && g.dg_9e49 <= 6 && i == g.dg_952e)) {
            const int slot = i + kShipImageSlotBase;  // [bp-0x14] = i + 0x28
            if (type == 0 || type == 7)
                o.src_x = add16(mul16(add16(o.size, o.gap), o.frame), o.gap);
            else
                o.src_x = add16(shl16(o.size, 4), mul16(o.gap, 0x11));
            if (type > 3)
                o.src_y = add16(shl16(o.gap, 1), o.size);
            else if (s.be9 != 0)
                o.src_y = o.gap;
            c.host.draw_sprite(pool_rec(g.ship_pool, i), 1, slot, 0);
        }
        if (o.on_screen != 0) c.host.ship_collide(i);  // seg13:0xb5a-0xcf9 [OPEN] M4
    }
}

// [BYTES seg14:0x1c8c-0x1caa seg14:0x1e35-0x1e8f seg14:0x1f41-0x1f4d]
FlightDesign flight_design(std::uint8_t shp_thruster, std::uint8_t shp_armor,
                           std::int8_t body_strength, std::int8_t body_1c, std::int8_t body_1d,
                           std::uint8_t body_flags) {
    FlightDesign d;
    d.thrust = static_cast<std::int16_t>(5 * static_cast<std::int8_t>(shp_thruster));  // imul byte
    d.drag = body_1c;
    d.turn_base = body_1d;
    const std::int16_t m1 = static_cast<std::int16_t>(10 * body_strength);  // imul byte: ax
    // al = cbw(armor) / 4 (8-bit idiv), cbw, + 100 in 32 bits
    const std::int32_t q = static_cast<std::int8_t>(static_cast<std::int8_t>(shp_armor) / 4);
    d.mass = lo16(div32(static_cast<std::int32_t>(static_cast<std::uint32_t>(m1) *
                                                  static_cast<std::uint32_t>(q + 100)),
                        100));
    // 0 for Body Thrust [BYTES seg14:0x1f41]; the 1 of the other bodies is [OPEN] (M3 s2 fixer).
    d.has_flame = (body_flags & 0x01) != 0 ? 0 : 1;
    return d;
}

// [BYTES seg14:0x21a0-0x22f2 seg14:0x20bc]
void apply_flight_design(ShipRecord& s, ObjectRecord& o, const FlightDesign& d,
                         bool player_game_player, std::uint32_t now, std::int16_t roster_byte) {
    s.bec = 1;
    s.thrust = d.thrust;
    s.drag = d.drag;
    s.w36 = 0;
    s.thrust_mult = 1;
    s.lockout.set(now);
    s.has_flame = d.has_flame;
    s.anim_step = 1;
    s.anim_frame = 0;
    s.w52 = 0;
    s.w54 = 0;
    std::int32_t t = d.thrust;  // T'
    if (s.roster_index != -1) {
        const std::int16_t f = mul16(add16(roster_byte, 10), 10);
        t = div32(static_cast<std::int32_t>(f) * d.thrust, 100);
    }
    s.turn = t == 0 ? 0 : lo16(div32(1200 * static_cast<std::int32_t>(d.turn_base), t));
    if (player_game_player) s.turn = idiv16(mul16(s.turn, 3), 4);  // [BYTES seg14:0x22e4-0x22f2]
    o.mass = d.mass;
}

}  // namespace is::world
