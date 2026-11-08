// The ambulance's call and flight (ambulance.h). M4 stage 3, track AMBFLIGHT. 16-bit arithmetic
// as the original's (ADR 0013 decision 7).
#include "world/ambulance.h"

#include <set>

#include "world/collide.h"
#include "world/missile.h"
#include "world/update.h"

namespace is::world {

namespace {

constexpr int kAmb = 7;  // pool 7, record 0: far DGROUP:0x1e14 [BYTES seg16:0xa2]

bool rec_ok(const Pools& p, int pool, int rec) {
    return pool >= 0 && pool < kPoolCount && rec >= 0 && rec < p.records(pool);
}
bool ship_ok(int i) { return i >= 0 && i < kShipCount; }

ObjectRecord g_scratch_obj{};
ShipRecord g_scratch_ship{};

ObjectRecord& amb(World& w) { return w.pools.records(kAmb) > 0 ? w.pools.rec(kAmb, 0) : (g_scratch_obj = ObjectRecord{}); }
// S, the served ship's object: pool DGROUP:0x9324 record DGROUP:0x952e. A record the port does
// not hold reads a scratch record [DESIGN] (the original reads whatever lies there).
ObjectRecord& served_obj(World& w) {
    const int sp = w.roles.ships;
    if (!rec_ok(w.pools, sp, w.w952e)) return g_scratch_obj = ObjectRecord{};
    return w.pools.rec(sp, w.w952e);
}
ShipRecord& served_ship(World& w) {
    if (!ship_ok(w.w952e)) return g_scratch_ship = ShipRecord{};
    return w.ships[static_cast<std::size_t>(w.w952e)];
}

// The credit dword +0xdc, signed, is purchases.h credit_of [BYTES seg16:0xb9e].

// 32-bit products as the 386 `imul r32` keeps them: the low dword (seg8:0x2658, claim 35).
std::int32_t mul32(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}
// `idiv r32` / seg5:0x110: toward zero; the trap cases (d = 0, INT_MIN / -1) give 0 [DESIGN].
std::int32_t sdiv32(std::int32_t n, std::int32_t d) {
    if (d == 0 || (d == -1 && n == INT32_MIN)) return 0;
    return n / d;
}
// `idiv r16` with the trap cases giving 0 [DESIGN] (the original faults).
std::int16_t div16(std::int16_t n, std::int16_t d) {
    if (d == 0 || (d == -1 && n == INT16_MIN)) return 0;
    return idiv16(n, d);
}

}  // namespace

AmbulanceState& ambulance_state() {
    static AmbulanceState s;
    return s;
}

void ambulance_placement(AmbulanceState& a) {
    a.served_alive = 0;  // DGROUP:0x9e5b [BYTES seg9:0x6865 seg9:0x6867]
    a.dist = 0;          // DGROUP:0x9e4b [BYTES seg9:0x686a]
    a.frame = 0;         // DGROUP:0x9e45 [BYTES seg9:0x686d]
}

void ambulance_briefing_entry(AmbulanceState& a, World& w) {
    a.served_alive = 1;  // DGROUP:0x9e5b [BYTES seg9:0x5d35 seg9:0x5d38]
    w.w1e88 = 0;         // dword DGROUP:0x1e88 [BYTES seg9:0x5d58]
}

// ---------------------------------------------------------------- the call

std::int16_t ambulance_call(AmbulanceCtx& c, int ship) {
    World& w = c.w;
    AmbulanceState& a = c.a;
    Pools& p = w.pools;
    if (w.w934e < 3) return 0;                                 // signed [BYTES seg16:0xb5c]
    if (w.w913a != 0) return 0;                                // [BYTES seg16:0xb68]
    if (w.w913c != 0) return 0;                                // [BYTES seg16:0xb6f]
    if (w.handover.demons_gate != 0 && w.w94b2 == 0) return 0;  // [BYTES seg16:0xb76-0xb82]
    if (w.kind == 6 || w.kind == 7) return 0;                  // [BYTES seg16:0xb84-0xb90]
    if (!ship_ok(ship)) return 0;  // [DESIGN] the port holds 8 ship records
    const ShipRecord& s = w.ships[static_cast<std::size_t>(ship)];
    const std::int16_t res = s.resources();                       // word +0xce [BYTES seg16:0xb97]
    const std::int32_t r = static_cast<std::int32_t>(static_cast<std::uint32_t>(res) +
                                                     static_cast<std::uint32_t>(credit_of(s)));  // cwd; add; adc
    // Every ship, the player included: r < -350 (dx:ax below 0xffff:0xfea2) and resources < 200
    // (verify claim 4, the earlier test) [BYTES seg16:0xb92-0xbbc].
    if (r < -350 && res < 200) return 0;
    // Another ship: a negative high word, or a zero high word and a low word below 200 unsigned,
    // that is r < 200 (verified claim 4) [BYTES seg16:0xbbe-0xbd1].
    if (ship != w.player && r < 200) return 0;
    ObjectRecord& A = amb(w);
    if (A.b2e != 0 && A.state == 0) return 0;  // [BYTES seg16:0xbd7-0xbe3]
    if (w.w9e49 < 9) return 0;                  // signed [BYTES seg16:0xbe5 seg16:0xbea]
    a.dist = 1400;                              // [BYTES seg16:0xbef]
    w.w952e = static_cast<std::int16_t>(ship);  // [BYTES seg16:0xbf8]
    w.w9e49 = 0;                                // [BYTES seg16:0xbfb]
    // An ambulance in flight only switches its target (verified claim 5) [BYTES seg16:0xc01
    // seg16:0xc06 seg16:0xcf1].
    if (A.state == 0) return 1;
    A.state = 0;  // [BYTES seg16:0xc0a]
    A.b2e = 0;    // [BYTES seg16:0xc14]
    // The nearest gate: the even pool-12 records k < 2·DGROUP:0x9318 against the ship, from
    // 20000, strictly smaller wins (verified claims 6, 7) [BYTES seg16:0xc19-0xc73]. The
    // original leaves the index unset when none is nearer; the port starts at 0 [DESIGN].
    std::int16_t best = 20000;
    int pick = 0;
    const ObjectRecord& S = served_obj(w);
    const std::int16_t limit = mul16(2, w.gates);
    for (std::int16_t k = 0; k < limit; k = add16(k, 2)) {
        if (!rec_ok(p, kPoolGateCentres, k)) break;
        const ObjectRecord& g = p.rec(kPoolGateCentres, k);
        std::int16_t dx = 0, dy = 0;
        const std::int16_t d = distance_cells(g.x, g.y, S.x, S.y, dx, dy);  // seg8:0x285a [BYTES seg16:0xc51]
        if (d < best) {  // `jge` skip [BYTES seg16:0xc59-0xc64]
            pick = k;
            best = d;
        }
    }
    // The placement: post `pick` of pool 8 + (480, -extent), 16-bit (core.md "Placement")
    // [BYTES seg16:0xc75-0xcdf].
    const ObjectRecord& post = rec_ok(p, kPoolGatePosts, pick) ? p.rec(kPoolGatePosts, pick) : g_scratch_obj;
    A.x = add16(post.x, 480);
    A.y = sub16(post.y, A.extent);
    A.on_screen = 0;
    A.scr_x = -1000;
    A.scr_y = -1000;
    A.old_scr_x = 100;
    A.old_scr_y = 100;
    A.vx = -20;
    A.vy = -70;
    recount_in_use(p, kAmb);  // seg7:0x2080(7, -1) (verified claim 8) [BYTES seg16:0xce5-0xcee]
    return 1;
}

int ambulance_reply_code(const World& w, int ship, std::int16_t called) {
    if (called != 0) return 7;  // [BYTES seg11:0x6fb6-0x6fbc]
    if (!ship_ok(ship)) return 8;
    const ShipRecord& s = w.ships[static_cast<std::size_t>(ship)];
    // lo16(credit) + resources, a 16-bit add; `cmp ax, 0xfed4; jge` -> 8 (verified claim 1)
    // [BYTES seg11:0x6fc4 seg11:0x6fc8 seg11:0x6fcc seg11:0x6fcf].
    const std::int16_t sum = add16(s.wdc, s.resources());
    return sum < -300 ? 132 : 8;
}

int ambulance_key(AmbulanceCtx& c, bool ctrl, bool alt) {
    World& w = c.w;
    const int P = w.player;
    if (ctrl) c.host.ctrl_h(P);  // seg13:0x81c2(player) [BYTES seg11:0x6f7a-0x6f89] [OPEN M5]
    if (alt) {
        c.host.alt_h(P);  // seg13:0x9ae8(player), then jmp seg11:0x6e0d [BYTES seg11:0x6f8a-0x6f99] [OPEN M5]
        return 0;
    }
    if (ctrl) return 0;               // [BYTES seg11:0x6f9c-0x6fa0]
    if (w.player_game == 0) return 0;  // DGROUP:0x95bc: the demo [BYTES seg11:0x6fa2]
    const std::int16_t r = ambulance_call(c, P);  // [BYTES seg11:0x6fb0]
    const int code = ambulance_reply_code(w, P, r);
    c.host.message(P, 7, 0, code);  // seg3:0x6ec(player, 7, 0, code), cdecl [BYTES seg11:0x6fd8-0x6fe5]
    return code;
}

// ---------------------------------------------------------------- the heading

std::int32_t newton_root(std::int32_t n, std::int32_t g) {
    if (g < 3) g = 3;  // signed dword [BYTES seg8:0x129d-0x12a4]
    // (N / g + g) >> 1, `idiv`, `add`, `sar` [BYTES seg8:0x12b2 seg8:0x12b6 seg8:0x12ba seg8:0x12c4]
    const std::int32_t q = sdiv32(n, g);
    return sar32(static_cast<std::int32_t>(static_cast<std::uint32_t>(q) + static_cast<std::uint32_t>(g)), 1);
}

std::int16_t intercept_heading(const Pools& p, int a_pool, int a_rec, int b_pool, int b_rec) {
    if (!rec_ok(p, a_pool, a_rec) || !rec_ok(p, b_pool, b_rec)) return 0;  // [DESIGN]
    const ObjectRecord& a = p.rec(a_pool, a_rec);
    const ObjectRecord& b = p.rec(b_pool, b_rec);
    std::int16_t dx = 0, dy = 0;
    // dx, dy = target - chaser in cells (seg8:0x285a, the target first) [BYTES seg8:0x2667-0x26a7]
    const std::int16_t d = distance_cells(a.x, a.y, b.x, b.y, dx, dy);
    // T: two signed 32-bit divides of full 16 x 16 products (seg5:0x110) [BYTES seg8:0x26bc
    // seg8:0x26cd seg8:0x26e1 seg8:0x26ec seg8:0x26f7].
    const std::int32_t T = static_cast<std::int32_t>(
        static_cast<std::uint32_t>(sdiv32(mul32(a.vx, dy), d)) - static_cast<std::uint32_t>(sdiv32(mul32(a.vy, dx), d)));
    // V = seg8:0x35d0(chaser vx, vy), cdecl, sign-extended; below 30 (signed dword) it is 50
    // [BYTES seg8:0x2708-0x272f].
    std::int32_t V = magnitude(b.vx, b.vy);
    if (V < 30) V = 50;
    // V < T (signed dword): seg8:0x19b0(dy, dx) [BYTES seg8:0x2737-0x2749].
    if (V < T) return direction16(dy, dx);
    // U = V² - T² (seg5:0x314, the low dword), g = |V| - |T| (seg9:0x88dc); W = the low word of
    // seg8:0x1290's result, sign-extended [BYTES seg8:0x274c-0x27a2 seg8:0x27b7-0x27bc].
    const std::int32_t U = static_cast<std::int32_t>(static_cast<std::uint32_t>(mul32(V, V)) -
                                                     static_cast<std::uint32_t>(mul32(T, T)));
    const std::int32_t g = static_cast<std::int32_t>(static_cast<std::uint32_t>(abs32(V)) -
                                                     static_cast<std::uint32_t>(abs32(T)));
    const std::int32_t W = lo16(newton_root(U, g));
    // X = W·dx/d + T·dy/d, Y = -(T·dx/d) + W·dy/d: each product a 32-bit `imul` whose high dword
    // is dropped (`cdq`), a 32-bit `idiv`; T·dx/d through seg5:0x110 and negated in 32 bits; X and
    // Y are 16-bit sums of the low words (verified claim 35) [BYTES seg8:0x27a5-0x2848].
    const std::int16_t X = add16(lo16(sdiv32(mul32(W, dx), d)), lo16(sdiv32(mul32(T, dy), d)));
    const std::int32_t tdx = sdiv32(mul32(T, dx), d);
    const std::int16_t Y = add16(lo16(static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(tdx))),
                                 lo16(sdiv32(mul32(W, dy), d)));
    return direction16(Y, X);  // seg8:0x19b0(num = Y, den = X) [BYTES seg8:0x284a seg8:0x284b]
}

// ---------------------------------------------------------------- park, repairs, drop, re-entry

void ambulance_park(AmbulanceCtx& c) {
    World& w = c.w;
    AmbulanceState& a = c.a;
    c.host.buffer(kAmb, 0);     // seg12:0x2556(7, 0) [BYTES seg16:0x8aa]
    c.host.effect(1, kAmb, 0);  // seg7:0x13b2(1, 7, 0): the vanish; its end gives A `state` 1
                                // and byte +0x2e = 0xff (gate_exit.h vanish_end) [BYTES seg16:0x8b6]
    w.w9e49 = 10;               // [BYTES seg16:0x8c1]
    w.w952e = -1;               // [BYTES seg16:0x8c7]
    a.busy = 0;                 // DGROUP:0x111e [BYTES seg16:0x8cd]
    a.reentry_due = 0;          // dword DGROUP:0x6da8 [BYTES seg16:0x8d3]
}

void ambulance_repairs_done(AmbulanceCtx& c) {
    const int s = c.w.w952e;
    c.host.purchases(s, 0);         // seg16:0x3784(DGROUP:0x952e, 0), cdecl [BYTES seg16:0x87c-0x883]
    c.host.message(s, 7, 0, 0x54);  // seg3:0x6ec(DGROUP:0x952e, 7, 0, 0x54) [BYTES seg16:0x889-0x893]
}

void ambulance_drop(AmbulanceCtx& c) {
    World& w = c.w;
    AmbulanceState& a = c.a;
    a.lift_step = -1;  // dword DGROUP:0x9e41 = 0x0032ffff [BYTES seg16:0x32aa]
    a.spin = 50;
    a.frame = 5;       // dword DGROUP:0x9e45 = 0x000f0005 [BYTES seg16:0x32b3]
    a.lift = 15;
    ambulance_repairs_done(c);  // seg16:0x872 [BYTES seg16:0x32bd]
    ObjectRecord& A = amb(w);
    A.state = 0;  // [BYTES seg16:0x32c4]
    if (A.b2e == 0xff) {  // [BYTES seg16:0x32ce-0x32ec] (verify claim 52b)
        c.host.buffer(kAmb, 0);     // seg12:0x2556(7, 0)
        c.host.effect(2, kAmb, 0);  // seg7:0x13b2(2, 7, 0)
        a.lift_step = 1;
    }
    w.w9e49 = 6;  // [BYTES seg16:0x32f2]
    a.busy = 0;   // DGROUP:0x111e [BYTES seg16:0x32f8]
    // No `mass`, no ship `state` and no DGROUP:0x9e4b (verify claim 52b): a computer ship stays
    // hidden at A.x + 2 with `state` 0, and A keeps mass 100 (s3-ambflight.md 6.3).
}

bool ambulance_reentry_clear(const AmbulanceCtx& c) {
    const World& w = c.w;
    const Pools& p = w.pools;
    if (p.records(kAmb) < 1) return true;
    const ObjectRecord& A = p.rec(kAmb, 0);
    // SetRect(&r, A.x - 0x230, A.y - 0x280, A.x + 0x640, A.y + 0x280), 16-bit [BYTES
    // seg16:0x33c3-0x33e4].
    const Rect16 r = rect16(sub16(A.x, 0x230), sub16(A.y, 0x280), add16(A.x, 0x640), add16(A.y, 0x280));
    const int sp = w.roles.ships;
    for (int pool = 0; pool <= sp && pool < kPoolCount; ++pool) {  // [BYTES seg16:0x33fb seg16:0x345c]
        if (pool == kAmb) continue;
        for (int k = 0; k < p.in_use(pool) && k < p.records(pool); ++k) {  // DGROUP:0x9406 + 2p
            if (pool == sp && k == w.w952e) continue;  // [BYTES seg16:0x341d]
            const ObjectRecord& o = p.rec(pool, k);
            if (o.state != 0) continue;  // [BYTES seg16:0x3434]
            if (pt_in_rect(r, o.x, o.y)) return false;  // USER PtInRect [BYTES seg16:0x343f seg16:0x344f]
        }
    }
    return true;
}

void ambulance_reentry(AmbulanceCtx& c) {
    World& w = c.w;
    AmbulanceState& a = c.a;
    const int sp = w.roles.ships;
    ObjectRecord& S = served_obj(w);
    ObjectRecord& A = amb(w);
    S.state = 0;         // [BYTES seg16:0x3321]
    A.state = 0;         // [BYTES seg16:0x332e]
    a.reentry_due = 0;   // dword DGROUP:0x6da8 [BYTES seg16:0x3332]
    c.host.buffer_free(sp, w.w952e);  // seg12:0x26a4 [BYTES seg16:0x333e]
    c.host.buffer(sp, w.w952e);       // seg12:0x2556 [BYTES seg16:0x334e]
    c.host.weapon_picture();          // seg11:0x2f8c [BYTES seg16:0x3356]
    if (A.b2e == 0xff) {              // [BYTES seg16:0x335f] (verified claim 51)
        c.host.buffer(kAmb, 0);       // [BYTES seg16:0x336a]
        c.host.effect(2, kAmb, 0);    // seg7:0x13b2(2, 7, 0): A materialises [BYTES seg16:0x3378]
        a.lift_step = 1;              // [BYTES seg16:0x3383]
        a.lift = 1;                   // [BYTES seg16:0x3386]
        A.mass = 800;                 // [BYTES seg16:0x338d]
    }
    w.w9e49 = 6;  // [BYTES seg16:0x3393]
}

// ---------------------------------------------------------------- the mover seg16:0xa2

void ambulance_move(UpdateCtx& u, AmbulanceCtx& c) {
    World& w = c.w;
    AmbulanceState& a = c.a;
    Pools& p = w.pools;
    if (p.records(kAmb) < 1) return;
    AmbulanceHost& h = c.host;
    const std::uint32_t now = c.now;
    const int sp = w.roles.ships;
    ObjectRecord& A = p.rec(kAmb, 0);
    ObjectRecord& S = served_obj(w);
    ShipRecord& Sship = served_ship(w);
    // size and gap, read at entry [BYTES seg16:0xb1-0xc0]
    const std::int16_t size = A.size, gap = A.gap;

    // ---- the head, every call [BYTES seg16:0xc3-0x26e]
    a.tick = add16(a.tick, 1);  // [BYTES seg16:0xc3-0xce]
    if (a.tick > 7) a.tick = 0;
    a.busy = (w.w9e49 >= 1 && w.w9e49 < 6) ? 1 : 0;  // signed (verified claim 9) [BYTES seg16:0xd4-0xea]
    if (w.w9e49 == 0) a.lift = 0;                     // [BYTES seg16:0xf0-0xf7]
    if (a.lift != 0 && A.b2e == 0) {                  // [BYTES seg16:0xfd-0x109]
        std::int16_t f = idiv16(a.lift, 5);           // `cwd; idiv` (verified claim 10) [BYTES seg16:0x10b-0x11f]
        if (f > 3) f = 3;
        a.frame = add16(f, 10);
        a.lift = add16(a.lift, a.lift_step);          // [BYTES seg16:0x122-0x125]
        if (a.lift > 15) {                            // [BYTES seg16:0x129-0x136]
            a.lift_step = 0;
            a.lift = 15;
        } else if (a.lift <= 0) {                     // [BYTES seg16:0x13e-0x14a]
            a.lift_step = 0;
            a.lift = 0;
            if (w.w9e49 == 8) {  // the drop-off's end (verified claim 11) [BYTES seg16:0x14d-0x172]
                w.w9e49 = 9;
                S.b54 = 0;  // `fixed`
                A.b54 = 0;
            }
        }
    } else {  // (verified claim 12) [BYTES seg16:0x178-0x19a]
        if (w.w9e49 == 4) a.spin = 50;
        a.frame = a.spin < 99 ? idiv16(a.spin, 10) : std::int16_t{0};  // signed `jge` 0x63
    }
    // The sprite, skipped only in state 3 or state 6 at lift 15 with byte +0x2e = 0 (verified
    // claims 13, 14): cell (gap + size)·frame + gap of the logo strip, seg8:0xb76(0x700, 1,
    // DGROUP:0x39f2 = DGROUP:0x39e8[5], 0) [BYTES seg16:0x1a0-0x1e0].
    const bool hidden = (w.w9e49 == 3 || (w.w9e49 == 6 && a.lift == 15)) && A.b2e == 0;
    if (!hidden) {
        A.src_x = add16(mul16(add16(gap, size), a.frame), gap);  // 16-bit `imul` [BYTES seg16:0x1c0-0x1d1]
        h.draw_sprite(pool_rec(kAmb, 0), 1, 5, 0);
    }
    // The spin: vx / 8 toward zero (`cwd; and dx, 7; add; sar 3`), 0 -> 1, above 7 -> 7, no
    // lower clamp; 130 wraps to 0 and below 0 to 130 (verified claim 15) [BYTES seg16:0x1e5-0x22e].
    a.spin_step = idiv16(A.vx, 8);
    if (a.spin_step == 0)
        a.spin_step = 1;
    else if (a.spin_step > 7)
        a.spin_step = 7;
    a.spin = add16(a.spin, a.spin_step);
    if (a.spin > 130)
        a.spin = 0;
    else if (a.spin < 0)
        a.spin = 130;
    // The served ship gone: back home, one dword store 0x04b00009 (verified claim 17)
    // [BYTES seg16:0x234-0x262].
    if (w.w9e49 != 9 && w.w952e >= 0 && (S.state != 0 || S.b2e == 1)) {
        w.w9e49 = 9;
        a.dist = 1200;
    }

    // ---- the states (the table at seg16:0x5e5; 5, 7, 8 and above 9 do nothing) [BYTES seg16:0x5d8-0x5e0]
    switch (w.w9e49) {
    case 0:  // the approach (verified claim 18) [BYTES seg16:0x271-0x327]
        if (a.dist < 120) {
            A.vx = idiv16(A.vx, 3);  // `cwd; idiv`, stored [BYTES seg16:0x27a-0x297]
            A.vy = idiv16(A.vy, 3);
            if (abs16(A.vx) < 10 && abs16(A.vy) < 10) {  // seg9:0x88ba after the division [BYTES seg16:0x2a3 seg16:0x2b6]
                A.vy = 0;
                A.vx = 0;
                S.b54 = 1;        // the ship's `fixed` [BYTES seg16:0x2e7]
                A.mass = 100;     // kept until seg16:0x3306 [BYTES seg16:0x2f0]
                a.frame = 5;      // [BYTES seg16:0x2f6]
                a.spin_step = 0;  // [BYTES seg16:0x2fc]
                w.w9e49 = 1;
                a.prev_heading = -1;  // DGROUP:0x1c42 [BYTES seg16:0x308]
            }
        }
        if (a.dist < 250 && a.frame == 5) a.spin_step = 0;  // a dead store (claim 16) [BYTES seg16:0x30e-0x321]
        break;
    case 1: {  // waiting for the ship to face A (verified claims 19-21) [BYTES seg16:0x32a-0x4af]
        if (a.frame == 5) a.spin_step = 0;  // dead store [BYTES seg16:0x331]
        a.busy = 1;                          // [BYTES seg16:0x337]
        if (ship_ok(w.w952e) && rec_ok(p, sp, w.w952e)) brake(u.flight, w.w952e);  // seg11:0x6874 [BYTES seg16:0x33d-0x346]
        S.vy = 0;  // [BYTES seg16:0x357-0x373]
        S.vx = 0;
        if (a.tick == 0 || a.tick == 4) {  // [BYTES seg16:0x377 seg16:0x37e]
            if (Sship.bec != 0) {           // `alive` [BYTES seg16:0x387-0x392]
                a.prev_heading = S.frame;   // [BYTES seg16:0x3a4-0x3a8]
                // seg13:0xd6c(0, -1, -1, DGROUP:0x952e, 2, heading, A.x, A.y, S.x, S.y) from
                // [bp+6] up (verified claim 20) [BYTES seg16:0x3bd-0x3ea]
                TurnArgs t;
                t.a0 = 0, t.a1 = -1, t.a2 = -1, t.ship = w.w952e, t.mode = 2, t.cur = S.frame;
                t.tx = A.x, t.ty = A.y, t.mx = S.x, t.my = S.y;
                FlightCtx& f = u.flight;
                S.frame = turn_toward(t, w.kind, w.ships, [&f](int s) { ship_touched(f, s); });
            }
            // The pickup: the heading unchanged (or the ship dead) and the lift down [BYTES
            // seg16:0x3ee-0x41b].
            if (!((a.prev_heading == S.frame || Sship.bec == 0) && a.lift == 0)) break;
            a.spin_step = 0;  // [BYTES seg16:0x41e]
            a.spin = 50;      // dword DGROUP:0x9e43 = 0x00050032 (verified claim 21) [BYTES seg16:0x424]
            a.frame = 5;
            a.lift = 1;       // [BYTES seg16:0x430]
            a.lift_step = 1;  // [BYTES seg16:0x433]
            a.served_alive = static_cast<std::int8_t>(Sship.bec);  // `cbw` [BYTES seg16:0x43c seg16:0x441]
            Sship.radar_hide = 1;  // +0x147 [BYTES seg16:0x446]
            Sship.bec = 1;         // `alive` [BYTES seg16:0x44a]
            if (w.w952e == w.player) {  // a dead player's game-over deadlines (verified claim 57) [BYTES seg16:0x44e-0x45e]
                w.w1e84 = 0;
                w.w1e80 = 0;
            }
            h.sound(0x23, kAmb, 0, 1);  // CAPTURED.WAV [BYTES seg16:0x46a]
            a.due = now + 750u;         // dword DGROUP:0x6e10 [BYTES seg16:0x47c]
            w.w9e49 = 2;
            A.b54 = 1;                  // A `fixed` [BYTES seg16:0x480]
            a.pickup_size = w.size;     // DGROUP:0x959e = DGROUP:0x9156 [BYTES seg16:0x492]
        } else {
            a.prev_heading = S.frame;  // [BYTES seg16:0x498-0x4ac]
        }
        break;
    }
    case 2:  // the lift is up: "Let me guide you in." (verified claim 22) [BYTES seg16:0x4b2-0x509]
        if (!(a.due >= now)) {  // `jae` skips while due >= now, unsigned
            h.sound(0x2f, kAmb, 0, 1);   // LET_ME.WAV
            a.due = now + 1300u;         // overwritten by seg13:0x3748 (now + 1100) [BYTES seg16:0x4d7 seg13:0x38e5]
            w.w9e49 = 3;
            a.w9596 = 1;                 // [BYTES seg16:0x4e1]
            h.buffer(sp, w.w952e);       // seg12:0x2556(DGROUP:0x9324, DGROUP:0x952e)
            h.buffer(kAmb, 0);           // seg12:0x2556(7, 0)
            a.pickup_dist = a.dist;      // DGROUP:0x9e53 [BYTES seg16:0x506]
        }
        break;
    case 3:
    case 4:  // the lift down and the deadline passed: the vanish and the visit's deadline
             // (verified claim 23) [BYTES seg16:0x50c-0x56f]
        if (a.lift == 0 && a.due < now) {  // `jb`, unsigned
            if (A.b2e == 0) {
                h.buffer(kAmb, 0);     // seg12:0x2556(7, 0) [BYTES seg16:0x525]
                h.effect(1, kAmb, 0);  // seg7:0x13b2(1, 7, 0) [BYTES seg16:0x53e]
            }
            // No state change: the next frames rewrite DGROUP:0x1e88 until the vanish's end gives
            // A `state` 1 (gate_exit.h vanish_end, the port's mode-1 end: `state` 1, +0x2e = 0xff,
            // seg7:0x2080) and the frame stops calling seg16:0xa2; the deadline then falls due
            // 1000 ms (4000 ms) after the vanish ends [BYTES seg16:0x546-0x56b] [OPEN: the
            // original's seg7:0x13b2 end for pool 7 is the port's vanish_end reading, verify
            // claim 23 note].
            w.w1e88 = now + (w.w952e == w.player ? 1000u : 4000u);  // 32-bit `adc`
        }
        break;
    case 6:  // the drop-off: rebuild the buffers (verified claim 24) [BYTES seg16:0x572-0x5ad]
        if (S.alt_dc == 0) h.buffer(sp, w.w952e);
        if (A.alt_dc == 0) h.buffer(kAmb, 0);
        break;
    case 9:  // the return (claim 25) [BYTES seg16:0x5b2-0x5d6]
        a.busy = 0;
        // DGROUP:0x9e4b is last frame's here; the 1200 of the gone test delays the park by a frame.
        if ((A.on_screen == 0 && a.dist < 200) || a.dist < 110) ambulance_park(c);  // seg16:0x8a0 [BYTES seg16:0x5d3]
        break;
    default:
        break;
    }

    // ---- the abort (verified claim 26) [BYTES seg16:0x5f9-0x623]
    if (w.w9e49 < 3 && w.w9e49 != 0 && a.dist > 140) {
        a.lift_step = -1;  // dword 0x0032ffff to DGROUP:0x9e41 [BYTES seg16:0x60f]
        a.spin = 50;
        a.frame = 5;       // [BYTES seg16:0x618]
        a.busy = 0;        // [BYTES seg16:0x620]
        w.w9e49 = 0;       // [BYTES seg16:0x623]
        // S `fixed`, A `fixed` and A's mass 100 stay (claim 26).
    }
    // The target: the served ship, or pool 12 record 1 in state 9 (verified claim 27)
    // [BYTES seg16:0x626-0x63e].
    int tpool = sp, trec = w.w952e;
    if (w.w9e49 == 9) {
        tpool = kPoolGateCentres;
        trec = 1;
    }
    const auto distance = [&] {  // seg8:0x285a(target, ambulance) (verified claim 31) [BYTES seg16:0x773-0x79e]
        if (!rec_ok(p, tpool, trec)) return;  // [DESIGN]
        const ObjectRecord& T = p.rec(tpool, trec);
        std::int16_t dx = 0, dy = 0;
        a.dist = distance_cells(T.x, T.y, A.x, A.y, dx, dy);
    };
    switch (w.w9e49) {  // [BYTES seg16:0x7a3-0x7b8]
    case 0:
    case 9:
        // The steering, 2 frames in 8 without the cooldown (verified claims 27, 28) [BYTES
        // seg16:0x649-0x6b0].
        if (a.cooldown == 0 && a.tick < 2) {
            std::int16_t hd = intercept_heading(p, tpool, trec, kAmb, 0);  // seg8:0x2658(trec, tpool, 0, 7)
            if (hd < 0 || hd > 15) hd = 0;  // [DESIGN] seg8:0x19b0 gives 0 ... 15
            const std::int16_t ovx = A.vx, ovy = A.vy;  // the dword copy [BYTES seg16:0x66b]
            const std::int16_t tx = heading_dx(hd), ty = heading_dy(hd);  // DGROUP:0x1050 / 0x1052 + 4h
            if (A.mass != 0) {  // [DESIGN] the original traps on 0
                A.vx = add16(A.vx, div16(sub16(mul16(tx, 500), mul16(ovx, 40)), A.mass));
                A.vy = add16(A.vy, div16(sub16(mul16(ty, 500), mul16(ovy, 40)), A.mass));
            }
            if (w.w9e49 == 0) {  // [BYTES seg16:0x6b4]
                // The push off the gate centres: pool 12 record 2k + 1 for k < DGROUP:0x9318
                // (verify claim 29, CORRECTED: offset k·0xe0 + 0x74) [BYTES seg16:0x6be-0x76f].
                for (int k = 0; k < w.gates; ++k) {
                    const int rec = 2 * k + 1;
                    if (!rec_ok(p, kPoolGateCentres, rec)) break;
                    const ObjectRecord& C = p.rec(kPoolGateCentres, rec);
                    std::int16_t dx = 0, dy = 0;  // A - centre, cells [bp-0x18], [bp-0x1a]
                    const std::int16_t d = distance_cells(A.x, A.y, C.x, C.y, dx, dy);
                    if (d < 150) {  // `jge` skip (verified claim 30) [BYTES seg16:0x6ef]
                        // -(3·t)/4 toward zero (`cwd; and dx, 3; add; sar 2`) [BYTES seg16:0x6fa-0x70c]
                        A.vx = sub16(A.vx, idiv16(mul16(tx, 3), 4));
                        A.vy = sub16(A.vy, idiv16(mul16(ty, 3), 4));
                        A.vx = add16(A.vx, div16(mul16(dx, 42), d));  // [BYTES seg16:0x722 seg16:0x726]
                        A.vy = add16(A.vy, div16(mul16(dy, 42), d));  // [BYTES seg16:0x732 seg16:0x736]
                        A.vx = add16(A.vx, div16(mul16(dy, -25), d)); // [BYTES seg16:0x742 seg16:0x746]
                        A.vy = add16(A.vy, div16(mul16(dx, 25), d));  // [BYTES seg16:0x752-0x762]
                    }
                }
            }
        }
        distance();
        break;
    case 1:
    case 2:
        distance();
        break;
    default:
        break;
    }

    // ---- the tail [BYTES seg16:0x7ba-0x84c]
    if (a.lift == 0) integrate(u.flight, 1, pool_rec(kAmb, 0));  // seg8:0x1b54(0x700, 1) (verified claim 32)
    if (a.cooldown != 0) a.cooldown = sub16(a.cooldown, 1);     // [BYTES seg16:0x7cb-0x7d2]
    if (A.on_screen != 0) {  // (verified claim 33) [BYTES seg16:0x7d6-0x7df]
        u.f.dg_943a = kAmb;  // DGROUP:0x943a = 7, DGROUP:0x943c = 0
        u.f.dg_943c = 0;
        const std::uint16_t hit = u.host.collide(pool_rec(kAmb, 0));  // seg8:0x50aa(0x700) [BYTES seg16:0x7f0]
        if (hit != 0xffffu) {
            a.cooldown = 10;  // [BYTES seg16:0x7fd]
            if (World* fw = u.host.frame_world(u.flight.pools)) {
                CollideCtx x = frame_collide_ctx(u, *fw);
                // seg8:0x546c(800, extent + 2, &vy, &vx, hit): the constant 800 [BYTES seg16:0x81e seg16:0x825]
                const std::int16_t r = respond(x, 800, add16(A.extent, 2), &A.vy, &A.vx, hit);
                // seg8:0x5df2(7, 0, hit pool, hit rec, r), cdecl [BYTES seg16:0x828-0x83c]
                dispatch(x, kAmb, 0, static_cast<int>(hit >> 8), static_cast<int>(hit & 0xffu), r);
            }
            h.sound(0x08, kAmb, 0, 1);  // SHORT.WAV [BYTES seg16:0x844-0x84c]
        }
    }
}

// ---------------------------------------------------------------- seg13:0x3748
// core.md "The ship's part" (C036-C043) [BYTES seg13:0x3748-0x3b7c].

void served_ship_move(FlightCtx& f, AmbulanceCtx& c, int i) {
    World& w = c.w;
    AmbulanceState& a = c.a;
    Pools& p = w.pools;
    const int sp = w.roles.ships;
    if (!ship_ok(i) || !rec_ok(p, sp, i) || p.records(kAmb) < 1) return;
    ObjectRecord& A = p.rec(kAmb, 0);
    ObjectRecord& S = p.rec(sp, i);
    ShipRecord& Sship = w.ships[static_cast<std::size_t>(i)];
    AmbulanceHost& h = c.host;
    if (w.w9e49 == 3) {  // State 3, into A [BYTES seg13:0x3757-0x3971] (C037-C040)
        // DGROUP:0x9e5b = 0 for a dead ship, and `alive` = 1 on every call (M4 s4 fixer, C039)
        // [BYTES seg13:0x3766 seg13:0x3772 seg13:0x377b].
        if (Sship.bec == 0) a.served_alive = 0;
        Sship.bec = 1;
        // The centre sums at [bp-0x16] ... [bp-0x10] are never read; the distance takes the raw
        // positions, A first (C036) [BYTES seg13:0x3796 seg13:0x37c8 seg13:0x37f8].
        std::int16_t dx = 0, dy = 0;
        const std::int16_t D = distance_cells(A.x, A.y, S.x, S.y, dx, dy);
        if (a.served_alive == 0) {  // a dead ship spins as it is pulled in (C039) [BYTES seg13:0x3800-0x3860]
            S.angle = 1;
            if (Sship.turn_acc < 1) {
                S.frame = turn_step(Sship, S, S.frame);  // seg13:0x100c
                ship_touched(f, i);                      // seg13:0x6716
                Sship.turn_acc = 1;
            } else {
                Sship.turn_acc = sub16(Sship.turn_acc, 1);
            }
        }
        if (D < 5) {  // signed (C037, C038) [BYTES seg13:0x3864-0x3902]
            S.vx = 0;
            S.vy = 0;
            h.buffer_free(sp, i);    // seg12:0x26a4(DGROUP:0x9324, DGROUP:0x952e)
            h.buffer_free(kAmb, 0);  // seg12:0x26a4(7, 0)
            a.lift_step = -1;
            a.spin = 50;
            a.frame = 5;
            a.spin_step = 0;
            h.sound(0x23, kAmb, 0, 1);  // CAPTURED.WAV [BYTES seg13:0x38cd]
            w.w9e49 = 4;                // [BYTES seg13:0x38d5]
            a.due = c.now + 1100u;     // dword DGROUP:0x6e10 [BYTES seg13:0x38e5]
            S.x = add16(A.x, 2);        // [BYTES seg13:0x38e9-0x38fe]
        } else {  // (C038) [BYTES seg13:0x3904-0x3971]
            A.frame = a.frame;
            h.composite(add16(a.pickup_dist, 50), add16(D, 50), 0, kAmb, i, sp);  // seg13:0x3d40
            S.vx = div16(mul16(dx, 70), D);  // 3-operand 16-bit `imul`; `cwd; idiv` [BYTES seg13:0x3947]
            S.vy = div16(mul16(dy, 70), D);  // [BYTES seg13:0x394c]
            integrate(f, 1, pool_rec(sp, i));  // seg8:0x1b54
        }
        return;
    }
    if (w.w9e49 != 6 || A.b2e != 0) return;  // State 6, out of A (C040-C043) [BYTES seg13:0x3976 seg13:0x397f seg13:0x3983]
    if (a.lift == 15) {  // [BYTES seg13:0x398c]
        std::int16_t dx = 0, dy = 0;
        std::int16_t D = distance_cells(A.x, A.y, S.x, S.y, dx, dy);
        if (dx == 0 && dy == 0) {  // one dword store 0x000afff6 over [bp-6], [bp-4] (C042) [BYTES seg13:0x3a15-0x3a21]
            dx = -10;
            D = 10;
        }
        if (D > a.pickup_dist || Sship.bec == 0) {  // the drop-off ends (C040) [BYTES seg13:0x3a29-0x3a93]
            h.sound(0x23, kAmb, 0, 1);
            h.buffer_free(sp, i);
            h.buffer_free(kAmb, 0);
            a.spin = 50;
            a.spin_step = 0;
            a.lift_step = -1;
            recount_in_use(p, kAmb);  // seg7:0x2080(7, -1)
            w.w9e49 = 8;
            Sship.radar_hide = 0;     // +0x147
        } else if (D != 0) {  // pushed out, away from A, the composite before the velocities (C041) [BYTES seg13:0x3a9b-0x3b2c]
            A.frame = a.frame;
            const std::int16_t m = D < a.pickup_dist ? D : a.pickup_dist;
            h.composite(add16(a.pickup_dist, 50), add16(m, 50), 0, kAmb, i, sp);
            S.vx = div16(mul16(dx, -50), D);
            S.vy = div16(mul16(dy, -50), D);
            S.frame = direction16(S.vy, S.vx);  // seg8:0x19b0(num = vy, den = vx)
            integrate(f, 1, pool_rec(sp, i));
        }
    } else if (a.lift == 2) {  // lift below 15 (C043) [BYTES seg13:0x3b46-0x3b55]
        h.sound(0x23, kAmb, 0, 1);
    } else if (a.lift == 14) {  // [BYTES seg13:0x3b57-0x3b77]
        if (w.w952e == w.player) h.play(0x6e);  // seg10:0x744c(0x6e): the sounds stop
        h.sound(0x1c, kAmb, 0, 1);              // MISSILE.WAV
    }
}

// ---------------------------------------------------------------- the program's links

namespace {

AmbulanceLinks g_links;

class LinkedHost final : public AmbulanceHost {
public:
    std::set<std::string> logged;
    void once(const std::string& what, const char* address) {
        if (!logged.insert(what).second) return;
        if (g_links.todo) g_links.todo(what, address);
    }
    void sound(int id, int pool, int rec, int arg) override {
        if (g_links.sound) return g_links.sound(id, pool, rec, arg);
        once("ambulance sound", "seg10:0x754a");
    }
    void play(int id) override {
        if (g_links.play) return g_links.play(id);
        once("ambulance play", "seg10:0x744c");
    }
    void buffer(int pool, int rec) override {
        if (g_links.buffer) return g_links.buffer(pool, rec);
        once("the ambulance's buffer", "seg12:0x2556");
    }
    void buffer_free(int pool, int rec) override {
        if (g_links.buffer_free) return g_links.buffer_free(pool, rec);
        once("the ambulance's buffer free", "seg12:0x26a4");
    }
    void effect(int mode, int pool, int rec) override {
        if (g_links.effect) return g_links.effect(mode, pool, rec);
        once("the ambulance's materialise", "seg7:0x13b2");
    }
    void message(int recipient, int pool, int rec, int code) override {
        if (g_links.message) return g_links.message(recipient, pool, rec, code);
        once("ambulance message " + std::to_string(code), "seg3:0x6ec");
    }
    void purchases(int ship, int mode) override {
        if (g_links.purchases) return g_links.purchases(ship, mode);
        once("the purchases", "seg16:0x3784");
    }
    void weapon_picture() override {
        if (g_links.weapon_picture) return g_links.weapon_picture();
        once("the weapon picture after the re-entry", "seg11:0x2f8c");
    }
    void draw_sprite(std::int16_t pr, int kind, int slot, int masked) override {
        if (g_links.draw_sprite) return g_links.draw_sprite(pr, kind, slot, masked);
        once("the ambulance's sprite", "seg8:0xb76");
    }
    void composite(std::int16_t den, std::int16_t num, int o_rec, int o_pool, int s_rec, int s_pool) override {
        if (g_links.composite) return g_links.composite(den, num, o_rec, o_pool, s_rec, s_pool);
        once("the pick-up composite", "seg13:0x3d40");
    }
    void ctrl_h(int) override { once("Ctrl+H", "seg13:0x81c2"); }
    void alt_h(int) override { once("Alt+H", "seg13:0x9ae8"); }
    void todo(const std::string& what, const char* address) override { once(what, address); }
};

LinkedHost g_linked;

}  // namespace

AmbulanceLinks& ambulance_links() { return g_links; }
AmbulanceHost& linked_ambulance_host() { return g_linked; }

int ambulance_key_program(World& w, std::uint32_t now, bool ctrl, bool alt) {
    AmbulanceCtx c{w, ambulance_state(), now, g_linked};
    return ambulance_key(c, ctrl, alt);
}

void ambulance_frame(UpdateCtx& u, World& w) {
    AmbulanceState& a = ambulance_state();
    AmbulanceCtx c{w, a, u.flight.g.now, g_linked};
    ambulance_move(u, c);
    // The frame's copies of the same DGROUP words [DESIGN] (ADR 0013, Consequences: the flight
    // globals copied between World and the frame state): the ship mover runs after this in the
    // frame and reads them (flight.cpp).
    u.flight.g.dg_9e49 = w.w9e49;
    u.flight.g.dg_952e = w.w952e;
    u.flight.g.dg_9e5b = a.served_alive;
    u.f.dg_6da8 = a.reentry_due;
}

bool ambulance_reentry_clear_program(World& w, std::uint32_t now) {
    AmbulanceCtx c{w, ambulance_state(), now, g_linked};
    return ambulance_reentry_clear(c);
}

void ambulance_reentry_program(World& w, WaveGlobals& g, FrameGlobals& f) {
    AmbulanceState& a = ambulance_state();
    AmbulanceCtx c{w, a, g.now, g_linked};
    ambulance_reentry(c);
    g.dg_9e49 = w.w9e49;  // the frame's copies [DESIGN] (ADR 0013, Consequences)
    f.dg_6da8 = a.reentry_due;
}

void served_ship_move_program(FlightCtx& f, World& w, int i) {
    AmbulanceState& a = ambulance_state();
    AmbulanceCtx c{w, a, f.g.now, g_linked};
    served_ship_move(f, c, i);
    f.g.dg_9e49 = w.w9e49;
    f.g.dg_9e5b = a.served_alive;
}

}  // namespace is::world
