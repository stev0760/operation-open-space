// LIGHTNING: seg11:0x9fdc, seg11:0xa234, seg11:0xa4a6, seg11:0xa9ea (lightning.h). M4 stage 7,
// track RING. Each step cites the original's addresses; the arithmetic is 16-bit where the
// original's is (ADR 0013 decision 7).
#include "world/lightning.h"

#include <cstddef>

#include "world/collide.h"
#include "world/flight.h"
#include "world/line.h"
#include "world/missile.h"
#include "world/shots.h"

namespace is::world {

namespace {

constexpr std::size_t ix(int k) { return static_cast<std::size_t>(k); }

std::int8_t charge_of(const ObjectRecord& o) { return static_cast<std::int8_t>(o.b50); }

// One seg12:0x7256(DGROUP:0x13dc, a, b) call [BYTES seg11:0xa8ea seg11:0xa968 seg11:0xa9ac
// seg11:0xaa94 seg11:0xab35 seg11:0xab79 seg11:0xabf5 seg11:0xac39]: the points are words.
void bolt_line(LightningCtx& c, const shell::BoltPoint& a, const shell::BoltPoint& b) {
    draw_clipped_line(*c.out, kDcWindow, c.w.W, c.w.H, lo16(a.x), lo16(a.y), lo16(b.x), lo16(b.y));
}

// The main line P(i-1) -> P(i) for i = 1 while `last` > i (the strike: N + 1; the frame call:
// |DGROUP:0x1016|) [BYTES seg11:0xa8cf-0xa8fc seg11:0xaa79-0xaaac]. A table shorter than the
// count stops the loop [DESIGN: the original reads the stale table].
void main_lines(LightningCtx& c, int last) {
    const auto& m = c.bolt.shape.main;
    for (int i = 1; last > i; ++i) {
        if (static_cast<std::size_t>(i) >= m.size()) break;
        bolt_line(c, m[ix(i - 1)], m[ix(i)]);
    }
}

// The branches b < DGROUP:0x9526, k = 1 ... 4, then the sub-branches s < DGROUP:0x9528, k = 1, 2
// [BYTES seg11:0xa940-0xa9c6 seg11:0xab0d-0xab93 seg11:0xabcd-0xac53].
void branch_lines(LightningCtx& c) {
    for (const auto& br : c.bolt.shape.branches)
        for (std::size_t k = 1; k < br.size(); ++k) bolt_line(c, br[k - 1], br[k]);
    for (const auto& sb : c.bolt.shape.subs)
        for (std::size_t k = 1; k < sb.size(); ++k) bolt_line(c, sb[k - 1], sb[k]);
}

// CreatePen(PS_SOLID, width, colour), SelectObject, the lines, SelectObject back, DeleteObject of
// the pen it returns [BYTES seg11:0xa8c1-0xa90b seg11:0xa932-0xa9d5 seg11:0xaa6b-0xaabb
// seg11:0xaaff-0xaba2]. Without a window DC the Blitter's SelectObject returns no pen; the port
// then deletes the pen it made, which it would otherwise keep [DESIGN] (review L3).
template <typename F>
void with_pen(LightningCtx& c, int width, std::uint32_t colour, F lines) {
    Blitter& o = *c.out;
    const PenId pen = o.create_pen(kPsSolid, width, colour);
    const PenId old = o.select_pen(kDcWindow, pen);
    lines();
    const PenId back = o.select_pen(kDcWindow, old);
    o.delete_pen(back != kPenNone ? back : pen);
}

// The logging defaults: once per site (world.h guard_todo).
void log_once(bool& once, const char* what, const char* address) { guard_todo(once, what, address); }

}  // namespace

BoltState& bolt_state() {
    static BoltState s;
    return s;
}

// ---- LightningHost's defaults

void LightningHost::sound(int id, int a, int b, int mode) {
    (void)id, (void)a, (void)b, (void)mode;
    static bool once = false;
    log_once(once, "LIGHTNING's sound (unbound)", "seg10:0x754a");
}

std::int16_t LightningHost::dispatch(int tp, int tr, int xp, int xr, std::int16_t amount) {
    (void)tp, (void)tr, (void)xp, (void)xr, (void)amount;
    static bool once = false;
    log_once(once, "LIGHTNING's hit dispatcher (unbound)", "seg8:0x5df2");
    return 0;
}

void LightningHost::icon_overlay(int icon, int k) {
    (void)icon, (void)k;
    static bool once = false;
    log_once(once, "the virus bolt's icon overlay (unbound)", "seg7:0x2512");
}

Point16 LightningHost::to_screen(std::int16_t x, std::int16_t y) {
    (void)x, (void)y;
    static bool once = false;
    log_once(once, "LIGHTNING's world-to-screen (unbound)", "seg8:0x29ea");
    return Point16{};
}

std::int16_t LightningHost::team(const World& w, int design) { return design_from_world(w, design).team; }

// ---- seg11:0x9fdc

std::int16_t lightning_range(std::int8_t charge, std::int16_t W, bool player) {
    std::int16_t r = shl16(static_cast<std::int16_t>(charge), 2);  // `cbw; shl ax, 2` [BYTES seg11:0xa044-0xa049]
    const std::int16_t half = idiv16(W, 2);  // `cwd; sub ax, dx; sar ax, 1` [BYTES seg11:0xa04e-0xa054]
    if (r > half) r = half;                  // `cmp cx, ax; jle` [BYTES seg11:0xa056-0xa05a]
    if (player && r < kLightningPlayerRange) r = kLightningPlayerRange;  // `jge` [BYTES seg11:0xa06f-0xa075]
    return r;
}

SearchResult lightning_search(LightningCtx& c, int pool, int rec) {
    World& w = c.w;
    SearchResult res;
    const std::int16_t cost = c.weapons.word(kWeaponLightning, kWordLightningCost);  // DGROUP:0x3072
    bool ok = false;  // [bp-0x22] [BYTES seg11:0x9feb]
    if (pool == w.roles.ships) {  // DGROUP:0x9324 [BYTES seg11:0x9ff0-0x9ff6]
        ShipRecord& s = ship_at(w, rec);
        // for l = 1; l < sext8(+0xcb); l++ [BYTES seg11:0x9ff8-0xa00f]
        for (std::int16_t l = 1; static_cast<std::int16_t>(static_cast<std::int8_t>(s.slots_cb())) > l; l = add16(l, 1)) {
            if (sbyte_at(s, 0xb8 + l) != kWeaponLightning) continue;  // [BYTES seg11:0xa018-0xa01d]
            if (!(s.fuel > cost)) continue;  // `jle` goes on to the next slot [BYTES seg11:0xa01f-0xa027]
            ok = true;                       // [BYTES seg11:0xa029]
            break;
        }
    } else {
        ok = true;  // [BYTES seg11:0xa029]
    }
    if (!ok) return res;  // [BYTES seg11:0xa02e-0xa032]
    res.acted = true;
    const ObjectRecord& U0 = obj(w, pool, rec);
    const std::int16_t R = lightning_range(charge_of(U0), w.W, pool == w.roles.ships && rec == w.player);
    res.range = R;  // [bp-0x10]
    int best_pool = -1, best_rec = 0;  // [bp-4], [bp-6]
    std::int16_t best_mass = 0;        // [bp-8] [BYTES seg11:0xa07b-0xa080]
    // p = DGROUP:0x9326 ... DGROUP:0x9324 (signed), 5 and 12 skipped [BYTES seg11:0xa085
    // seg11:0xa1ab-0xa1c7]
    for (int p = w.roles.radar_first; p <= w.roles.ships; ++p) {
        if (p == kPool5 || p == kPoolGateCentres) continue;  // literal 5 and 12 [BYTES seg11:0xa1b6-0xa1c0]
        if (p < 0 || p >= kPoolCount) continue;              // [DESIGN] bounds
        // k < word DGROUP:0x9406 + 2p (`jg`) [BYTES seg11:0xa198-0xa1a7]
        for (int k = 0; k < w.pools.in_use(p) && k < w.pools.records(p); ++k) {
            const ObjectRecord& T = w.pools.rec(p, k);
            if (T.state != 0) continue;              // [BYTES seg11:0xa09b-0xa0a0]
            if (p == pool && k == rec) continue;     // [BYTES seg11:0xa0a4-0xa0af]
            const ObjectRecord& U = obj(w, pool, rec);
            std::int16_t dx = 0, dy = 0;             // [bp-0x12], [bp-0x14]
            // seg8:0x285a(candidate, user): candidate - user, cells [BYTES seg11:0xa0b1-0xa0f2]
            const std::int16_t d = distance_cells(T.x, T.y, U.x, U.y, dx, dy);
            if (!(d < R)) continue;                  // `jge` [BYTES seg11:0xa0f5-0xa0f8]
            const std::int16_t h = direction16(dy, dx);  // seg8:0x19b0(den = dx, num = dy) [BYTES seg11:0xa0fa-0xa103]
            // h - 1 <= frame <= h + 1, signed, no wrap [BYTES seg11:0xa106-0xa11c]
            if (U.frame < sub16(h, 1)) continue;
            if (U.frame > add16(h, 1)) continue;
            // [bp-0x18], the weighted score, is dead [BYTES seg11:0xa120-0xa16b] (claim 13).
            if (T.mass > best_mass) {  // strictly greater (`jle` skips) [BYTES seg11:0xa16e-0xa195]
                best_pool = p;
                best_rec = k;
                best_mass = T.mass;
            }
        }
    }
    res.pool = best_pool;
    res.rec = best_rec;
    if (best_pool >= 0) {  // `cmp [bp-4], 0; jl` [BYTES seg11:0xa1c9-0xa1cd]
        if (pool == kPoolShips) {  // literal 14 [BYTES seg11:0xa1cf-0xa1d3]
            // fuel - DGROUP:0x3072, 0 when the result is negative (`jns`) [BYTES seg11:0xa1d5-0xa1e6]
            ShipRecord& s = ship_at(w, rec);
            std::int16_t f = sub16(s.fuel, cost);
            if (f < 0) f = 0;
            s.fuel = f;
            res.paid = true;
        }
        lightning_strike(c, pool, rec, best_pool, best_rec);  // cdecl, add sp 8 [BYTES seg11:0xa1ea-0xa1f8]
    }
    // The launcher zap of a pool-6 user [BYTES seg11:0xa1fb-0xa227] (claim 15).
    if (pool == kPoolMissiles) {
        MissileSlot& e = w.pools.missile(rec);
        if (e.launcher_pool == best_pool && best_pool == w.roles.ships && e.launcher_rec == best_rec)
            e.age = sub16(c.weapons.word(e.weapon, 0x1e), 1);  // DGROUP:0x2f72 + 0x50·w, minus 1
    }
    return res;
}

// ---- seg11:0xa234

std::int16_t strike_amount(int pool, std::int8_t charge, int r, bool* ok) {
    if (ok) *ok = true;
    if (pool != kPool9) return static_cast<std::int16_t>(charge);  // `cbw` [BYTES seg11:0xa3b4-0xa3c7]
    // c / 2 and c / 3 by 8-bit `idiv` of sext(c), the quotients `cbw`d; the % a 16-bit `idiv`
    // [BYTES seg11:0xa392-0xa3af].
    const std::int16_t half = static_cast<std::int16_t>(static_cast<std::int8_t>(charge / 2));
    const std::int16_t third = static_cast<std::int16_t>(static_cast<std::int8_t>(charge / 3));
    if (half == 0) {
        // The original's `idiv si` with si = 0 at seg11:0xa3a3 faults; the port skips it and
        // the stores that take its result [DESIGN ADR 0016 decision 4] (ruling R1).
        if (ok) *ok = false;
        return 0;
    }
    return add16(imod16(static_cast<std::int16_t>(r), half), third);
}

StrikeOutcome lightning_strike(LightningCtx& c, int pool, int rec, int tpool, int trec) {
    World& w = c.w;
    if (c.bolt.pending != 0) return StrikeOutcome::kPending;  // DGROUP:0x1016 [BYTES seg11:0xa243-0xa248]
    // [bp-0x20], [bp-0x1e] = DGROUP:0x2400 / 0x2402 sar 3: never read (O12) [BYTES seg11:0xa24c-0xa25b]
    c.bolt.src_rec = static_cast<std::int16_t>(rec);    // DGROUP:0x9520 [BYTES seg11:0xa25e-0xa261]
    c.bolt.src_pool = static_cast<std::int16_t>(pool);  // DGROUP:0x951e [BYTES seg11:0xa264-0xa267]
    // rec < word DGROUP:0x9406 + 2·pool (`jg`) [BYTES seg11:0xa26b-0xa271]; a pool outside the
    // table counts as a bad record [DESIGN].
    if (pool < 0 || pool >= kPoolCount || !(w.pools.in_use(pool) > rec)) {
        w.pools.bad_index_count = add16(w.pools.bad_index_count, 1);  // DGROUP:0x9498 [BYTES seg11:0xa273]
        return StrikeOutcome::kBadRecord;
    }
    c.h.sound(kSoundBolt, pool, rec, 1);  // seg10:0x754a(0x22, pool, rec, 1) [BYTES seg11:0xa27a-0xa28b]
    // The source point [BYTES seg11:0xa28e-0xa308] and the target's centre [BYTES seg11:0xa30b-0xa338]
    Point16 p0{}, p1{};
    {
        const ObjectRecord& U = obj(w, pool, rec);
        if (pool == w.roles.ships) {
            const ShipRecord& s = ship_at(w, rec);
            ShotState& sh = c.shots ? *c.shots : shot_state();
            const int n = s.design_index;  // ship +0x4e (DGROUP:0x70de)
            // x + (DGROUP:0x7b60[64n + frame] << 3), y + (DGROUP:0x7b62[...] << 3), 16-bit
            p0.x = add16(shl16(sh.mx(n, U.frame), 3), U.x);
            p0.y = add16(shl16(sh.my(n, U.frame), 3), U.y);
        } else {
            p0.x = add16(idiv16(U.extent, 2), U.x);
            p0.y = add16(idiv16(U.extent, 2), U.y);
        }
        const ObjectRecord& T = obj(w, tpool, trec);
        p1.x = add16(idiv16(T.extent, 2), T.x);
        p1.y = add16(idiv16(T.extent, 2), T.y);
    }
    const Point16 s0 = c.h.to_screen(p0.x, p0.y);  // seg8:0x29ea [BYTES seg11:0xa33b-0xa34d]
    const Point16 s1 = c.h.to_screen(p1.x, p1.y);  // [BYTES seg11:0xa354-0xa366]
    // seg11:0xa4a6(s0, s1, pool): the fifth argument is the pool pushed before seg8:0x29ea's
    // `retf 6` [BYTES seg11:0xa351 seg11:0xa36a-0xa374] (claim 18).
    bolt_draw(c, s0.x, s0.y, s1.x, s1.y, pool);
    // The amount [BYTES seg11:0xa377-0xa3c7]: a virus draws its rand() after the bolt's.
    const int r = pool == kPool9 ? c.rng.rand() : 0;  // seg5:0xdc [BYTES seg11:0xa37d]
    bool amount_ok = true;
    std::int16_t A = strike_amount(pool, charge_of(obj(w, pool, rec)), r, &amount_ok);
    if (!amount_ok) {
        static bool once = false;
        guard_todo(once, "a virus bolt with charge -1, 0 or 1 divides by zero (skipped)", "seg11:0xa3a3");
    }
    // The infection [BYTES seg11:0xa3ca-0xa3e0] (claim 22)
    if (pool == kPool9 && tpool == kPoolIcons) c.h.icon_overlay(trec, 0);
    // The dispatch [BYTES seg11:0xa3e3-0xa406] (claim 23 as corrected)
    if (c.dg_943c) *c.dg_943c = static_cast<std::int16_t>(rec);   // DGROUP:0x943c [BYTES seg11:0xa3f2]
    if (c.dg_943a) *c.dg_943a = static_cast<std::int16_t>(pool);  // DGROUP:0x943a [BYTES seg11:0xa3f9]
    if (amount_ok) {
        (void)c.h.dispatch(tpool, trec, pool, rec, add16(idiv16(A, 2), kLightningAmountBase));
        // The target's charge, outside the ship pool [BYTES seg11:0xa409-0xa429] (claim 24)
        if (tpool != w.roles.ships)
            obj(w, tpool, trec).b50 = static_cast<std::uint8_t>(idiv16(mul16(A, 3), 5) & 0xff);
        // The player's ship keeps two thirds [BYTES seg11:0xa42d-0xa446]
        if (pool == w.roles.ships && rec == w.player) A = idiv16(A, 3);
        // `sub byte`, 8-bit [BYTES seg11:0xa449-0xa45a]
        ObjectRecord& U = obj(w, pool, rec);
        U.b50 = static_cast<std::uint8_t>((U.b50 - (A & 0xff)) & 0xff);
    }
    // Team 7 [BYTES seg11:0xa45e-0xa497] (claim 25)
    if (pool == w.roles.ships) {
        const std::int16_t n = ship_at(w, rec).design_index;  // DGROUP:0x70de + 0x15a·rec
        if (c.h.team(w, n) == kLightningTeam) {                // DGROUP:0x970a + 0xad·n
            obj(w, tpool, trec).b50 = 0;
            obj(w, pool, rec).b50 = 0;
        }
    }
    return amount_ok ? StrikeOutcome::kStruck : StrikeOutcome::kGuarded;
}

// ---- seg11:0xa4a6

bool bolt_draw(LightningCtx& c, int x1, int y1, int x2, int y2, int pool) {
    // srand(GetCurrentTime()) before anything else (claim 20) [BYTES seg11:0xa4b5-0xa4bb]
    c.rng.srand(static_cast<std::uint16_t>(c.h.current_time(c.now) & 0xffffu));
    shell::BasicBolt<shell::BoltPoint> b;
    if (!shell::make_bolt(c.rng, x1, y1, x2, y2, &b)) return false;  // seg11:0xa54d, seg11:0xa684 -> seg11:0xa9e1
    c.bolt.shape = std::move(b);
    const int n = static_cast<int>(c.bolt.shape.main.size()) - 1;  // [bp-0xc]
    const std::uint32_t colour = pool == kPool9 ? kBoltVirusPen : kBoltStrikePen;  // [BYTES seg11:0xa89f-0xa8bf seg11:0xa910-0xa930]
    if (c.out) {
        with_pen(c, 2, colour, [&] { main_lines(c, n + 1); });  // i = 1 while N + 1 > i [BYTES seg11:0xa8f5-0xa8fc]
        with_pen(c, 1, colour, [&] { branch_lines(c); });
    }
    c.bolt.pending = static_cast<std::int16_t>(n + 1);  // DGROUP:0x1016 = N + 1 [BYTES seg11:0xa9da-0xa9de]
    return true;
}

// ---- seg11:0xa9ea

void bolt_frame(LightningCtx& c, int arg) {
    BoltState& b = c.bolt;
    c.h.bk_mode(2);  // SetBkMode(DGROUP:0x13dc, 2) [BYTES seg11:0xa9f7-0xa9fd]
    const std::int16_t D = b.pending;
    std::uint32_t colour = kBoltErasePen;  // D > 0: CreatePen(0, 2, 0) [BYTES seg11:0xaa02-0xaa15]
    if (!(D > 0)) {
        if (arg != 0) {  // the second thunder [BYTES seg11:0xaa17-0xaa3e]
            if (c.w.w1318 > 1) {  // DGROUP:0x1318 (`jle`)
                const shell::BoltPoint p0 = b.shape.main.empty() ? shell::BoltPoint{} : b.shape.main.front();
                c.h.sound(kSoundBolt, lo16(p0.x), lo16(p0.y), 4);  // dword DGROUP:0x6b5c
            } else {
                c.h.sound(kSoundBolt, b.src_pool, b.src_rec, 1);
            }
        }
        // [BYTES seg11:0xaa41-0xaa65]
        colour = (arg != 0 && b.src_pool == kPool9) ? kBoltVirusPen : kBoltFlashPen;
    }
    if (c.out) {
        with_pen(c, 2, colour, [&] { main_lines(c, abs16(D)); });  // seg9:0x88ba [BYTES seg11:0xaa79-0xaaac]
        with_pen(c, 1, colour, [&] { branch_lines(c); });          // [BYTES seg11:0xaac0-0xaba2]
    }
    if (D > 0 && b.flashed == 0) {  // [BYTES seg11:0xaba7-0xabb5]
        // GetStockObject(7), BLACK_PEN, over the branches again, selected back, not deleted
        // [BYTES seg11:0xabb9-0xac5c].
        if (c.out) {
            const PenId old = c.out->select_pen(kDcWindow, kPenBlack);
            branch_lines(c);
            c.out->select_pen(kDcWindow, old);
        }
        b.pending = sub16(0, b.pending);  // `neg` [BYTES seg11:0xac61]
    } else if (b.flashed == 0) {
        b.flashed = 1;                    // [BYTES seg11:0xac6e]
        b.pending = sub16(0, b.pending);  // [BYTES seg11:0xac74 seg11:0xac61]
    } else {
        b.pending = 0;  // [BYTES seg11:0xac76-0xac7b]
        b.flashed = 0;
    }
}

// ---- the program's links

LightningLinks& lightning_links() {
    static LightningLinks l;
    return l;
}

namespace {

class LinkedLightningHost final : public LightningHost {
public:
    void sound(int id, int a, int b, int mode) override {
        if (lightning_links().sound) return lightning_links().sound(id, a, b, mode);
        LightningHost::sound(id, a, b, mode);
    }
    std::int16_t dispatch(int tp, int tr, int xp, int xr, std::int16_t amount) override {
        if (lightning_links().dispatch) return lightning_links().dispatch(tp, tr, xp, xr, amount);
        return LightningHost::dispatch(tp, tr, xp, xr, amount);
    }
    void icon_overlay(int icon, int k) override {
        if (lightning_links().icon_overlay) return lightning_links().icon_overlay(icon, k);
        LightningHost::icon_overlay(icon, k);
    }
    Point16 to_screen(std::int16_t x, std::int16_t y) override {
        if (lightning_links().to_screen) return lightning_links().to_screen(x, y);
        return LightningHost::to_screen(x, y);
    }
    std::uint32_t current_time(std::uint32_t pass_now) override {
        if (lightning_links().current_time) return lightning_links().current_time();
        return LightningHost::current_time(pass_now);
    }
    void bk_mode(int mode) override {
        if (lightning_links().bk_mode) lightning_links().bk_mode(mode);
    }
    std::int16_t team(const World& w, int design) override {
        if (lightning_links().team) return lightning_links().team(design);
        return LightningHost::team(w, design);
    }
};

LightningCtx program_ctx(World& w, std::uint32_t now) {
    LightningCtx c{w, bolt_state(), linked_lightning_host(), shell::game_rng(), missile_state().words};
    c.now = now;
    c.out = wave_blitter();
    c.dg_943a = lightning_links().dg_943a;
    c.dg_943c = lightning_links().dg_943c;
    return c;
}

}  // namespace

LightningHost& linked_lightning_host() {
    static LinkedLightningHost h;
    return h;
}

SearchResult lightning_search_on(World& w, std::uint32_t now, int pool, int rec) {
    LightningCtx c = program_ctx(w, now);
    return lightning_search(c, pool, rec);
}

StrikeOutcome lightning_strike_on(World& w, std::uint32_t now, int pool, int rec, int tpool, int trec) {
    LightningCtx c = program_ctx(w, now);
    return lightning_strike(c, pool, rec, tpool, trec);
}

void bolt_frame_on(World& w, std::uint32_t now, int arg) {
    LightningCtx c = program_ctx(w, now);
    bolt_frame(c, arg);
}

}  // namespace is::world
