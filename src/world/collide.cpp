// Collisions (collide.h). Every site cites the listing it ports.
#include "world/collide.h"

#include <utility>

#include "world/bounce.h"
#include "world/damage.h"
#include "world/missile.h"

namespace is::world {

namespace {

ObjectRecord g_scratch_obj{};
ShipRecord g_scratch_ship{};

// 32-bit add with wrap (add/adc).
std::uint32_t add32u(std::uint32_t a, std::int32_t b) { return a + static_cast<std::uint32_t>(b); }

// The screen point of an object: (+0x00, +0x02) while `on_screen`, else seg8:0x29ea of its
// world position [BYTES seg8:0x6d83 seg8:0x6dad seg8:0x565b seg8:0x568f].
ScreenPoint screen_of(CollideCtx& x, const ObjectRecord& o) {
    if (o.on_screen != 0) return {o.scr_x, o.scr_y};
    return world_to_screen(x.view, o.x, o.y, x.g.wave_kind, x.g.dg_9eb5, x.g.dg_9130);
}

DefenderEntry g_scratch_defender{};
DefenderEntry& defender(CollideGlobals& c, int k) {
    if (k < 0 || k >= static_cast<int>(c.defenders.size())) return g_scratch_defender = DefenderEntry{};
    return c.defenders[static_cast<std::size_t>(k)];
}
template <typename T, std::size_t N>
T& entry(std::array<T, N>& a, int k, T& scratch) {
    if (k < 0 || k >= static_cast<int>(N)) return scratch = T{};
    return a[static_cast<std::size_t>(k)];
}
Pool5Entry g_scratch_p5{};
std::int16_t g_scratch_word = 0;

Pool8Entry& pool8_at(World& w, int rec) {
    static Pool8Entry scratch{};
    if (rec < 0 || rec >= kPool8Max) return scratch = Pool8Entry{};
    return w.pool8[static_cast<std::size_t>(rec)];
}

// The two words of the World and the flight globals that stand for one DGROUP word.
void set_9e49(CollideCtx& x, std::int16_t v) {
    x.w.w9e49 = v;
    x.g.dg_9e49 = v;
}

}  // namespace

// ---- record access

ObjectRecord& obj(World& w, int pool, int rec) {
    if (pool < 0 || pool >= kPoolCount || rec < 0 || rec >= w.pools.records(pool)) {
        g_scratch_obj = ObjectRecord{};
        return g_scratch_obj;
    }
    return w.pools.rec(pool, rec);
}

ShipRecord& ship_at(World& w, int i) {
    if (i < 0 || i >= kShipCount) {
        g_scratch_ship = ShipRecord{};
        return g_scratch_ship;
    }
    return w.ships[static_cast<std::size_t>(i)];
}

std::uint32_t ship_dword(const ShipRecord& s, int off) {
    return static_cast<std::uint16_t>(word_at(s, off)) |
           (static_cast<std::uint32_t>(static_cast<std::uint16_t>(word_at(s, off + 2))) << 16);
}

void set_ship_dword(ShipRecord& s, int off, std::uint32_t v) {
    set_word_at(s, off, static_cast<int>(v & 0xffff));
    set_word_at(s, off + 2, static_cast<int>(v >> 16));
}

DamageDesign design_from_world(const World& w, int n) {
    DamageDesign d;
    if (n < 0 || n >= kShipCount) return d;
    const DesignRating& r = w.design_ratings[static_cast<std::size_t>(n)];
    d.hit_div = {r.armor4a, r.armor5a, r.armor4b, r.armor5b};
    d.lamp_row = {0, 1, 2, r.special};
    d.captures = r.captures;
    d.repair = r.repair;
    // Design word +0x2a: the second wing of core-0001 sets ship word +0x20 = +0x2a - 14 = 19 for
    // TRANSFORMER, whose SHP tank byte is 33 = design +0x1e / 40 [ORACLE core-0001]; the
    // writer of +0x2a is [OPEN].
    d.w2a = idiv16(r.tank, 40);
    return d;
}

// ---- the words past the in-use counts, the weapon words (M4 s2 GUN)

std::int16_t in_use_word(const CollideCtx& x, int pool) {
    if (pool >= 0 && pool < kPoolCount) return x.w.pools.in_use(pool);
    // DGROUP:0x9406 + 2·pool for pool 21 ... 31 [BYTES DGROUP:0x9430-0x9444 seg7:0x509f-0x50ac]
    switch (pool) {
    case 21: return x.w.w9430;     // DGROUP:0x9430
    case 22: return x.w.w9432;     // DGROUP:0x9432
    case 23: return x.w.w9434;     // DGROUP:0x9434
    case 24: return x.c.dB;        // DGROUP:0x9436
    case 25: return x.c.dA;        // DGROUP:0x9438
    case 26: return x.dg_943a;     // DGROUP:0x943a
    case 27: return x.dg_943c;     // DGROUP:0x943c
    case 28: return x.c.hit_pool;  // DGROUP:0x943e
    case 29: return x.c.hit_rec;   // DGROUP:0x9440
    case 30: return x.c.eA;        // DGROUP:0x9442 (a gun hit's B pool)
    case 31: return x.c.eB;        // DGROUP:0x9444
    default: return 0;             // [OPEN]
    }
}

namespace {
std::function<std::int16_t(int, int)>& weapon_words_fn() {
    static std::function<std::int16_t(int, int)> f;
    return f;
}
}  // namespace

void set_weapon_words(std::function<std::int16_t(int w, int off)> f) { weapon_words_fn() = std::move(f); }
// Without a registered source the words are the program's one mutable copy DGROUP:0x2f54,
// missile.h MissileState::words, which the program fills from hud::WeaponTable at the load and
// seg10:0x533a writes W(w)-4 into (M4 s2 fixer).
std::int16_t weapon_word(int w, int off) {
    const auto& f = weapon_words_fn();
    return f ? f(w, off) : missile_state().words.word(w, off);
}

// ---- tables

namespace {
// DGROUP:0xb0e and DGROUP:0xb38, 21 words each [BYTES DGROUP:0xb0e DGROUP:0xb38].
constexpr std::int16_t kKillPoints[kPoolCount] = {1000, 600, 1200, 900, 200, 0, 1000, 0, 3400, 500, 500,
                                                  0,    0,   5000, 0,   0,   0, 0,    0, 0,    0};
constexpr std::int16_t kKillWeight[kPoolCount] = {1, 1, 1, 2, 0, 0, 0, 0, 4, 3, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0};
}  // namespace

std::int16_t kill_points(int pool) { return pool >= 0 && pool < kPoolCount ? kKillPoints[pool] : 0; }
std::int16_t kill_weight(int pool) { return pool >= 0 && pool < kPoolCount ? kKillWeight[pool] : 0; }

// ---- seg8:0x2bbe

ScreenPoint screen_to_world(const ViewState& v, std::int16_t x, std::int16_t y, std::int16_t dg_9130) {
    if (dg_9130 != 0) return {x, y};  // seg8:0x2c7d [OPEN]
    // The origin, moved into the world when negative [BYTES seg8:0x2bd6-0x2c01]
    std::int16_t x0 = v.x0, y0 = v.y0;
    if (x0 < 0) x0 = add16(x0, shl16(kWorldCells, 3));
    if (y0 < 0) y0 = add16(y0, shl16(kWorldCells, 3));
    // (x << s) sign-extended plus the origin, 32-bit [BYTES seg8:0x2c04-0x2c32]
    const int s = v.shift & 0x1f;
    std::int32_t X = static_cast<std::int32_t>(x0) + shl16(x, s);
    std::int32_t Y = static_cast<std::int32_t>(y0) + shl16(y, s);
    const std::int32_t W = shl16(kWorldCells, 3);
    if (X > W) X -= W;  // signed 32-bit compare [BYTES seg8:0x2c3c-0x2c48]
    if (Y > W) Y = static_cast<std::int32_t>((static_cast<std::uint32_t>(Y) & 0xffff0000u) |
                                             (static_cast<std::uint16_t>(Y - W)));  // low word only [BYTES seg8:0x2c51-0x2c5d]
    return {lo16(X), lo16(Y)};
}

// ---- seg8:0x50aa

std::uint16_t find_pair(CollideCtx& x, std::int16_t pr) {
    const int pool = pool_of(pr), rec = rec_of(pr);  // [BYTES seg8:0x50b9-0x50c6]
    ObjectRecord& o = obj(x.w, pool, rec);
    if (o.w1c == 0) {  // [BYTES seg8:0x50d7]
        if (x.g.wave_kind == 6 && o.on_screen == 0 && x.w.detail > 1) {
            // The race-track path seg8:0x50fc-0x5221 (kind-6 waves, M6): re-projects the caller
            // and tests every ship [OPEN].
        } else {
            CollideGlobals& c = x.c;
            // The caller's square and the search box [BYTES seg8:0x5224-0x5294]
            c.square = rect16(o.scr_x, o.scr_y, add16(o.scr_x, o.size), add16(o.scr_y, o.size));
            const int sz = x.g.size & 0x1f;
            const std::int16_t h = sar16(shl16(0x46, sz), 1), q = sar16(shl16(0xf, sz), 1);
            c.box = rect16(sub16(o.scr_x, h), sub16(o.scr_y, h), add16(add16(o.scr_x, o.size), q),
                           add16(add16(o.scr_y, o.size), q));
            // The visibility list, pools below the caller's and the caller skipped, `collided`
            // candidates skipped [BYTES seg8:0x52a0-0x5302]
            const int n = x.vis ? x.vis->count : 0;
            for (int i = 0; i < n && i < kVisibleMax; ++i) {
                const int kp = static_cast<std::int8_t>(x.vis->pool[static_cast<std::size_t>(i)]);
                const int kr = static_cast<std::int8_t>(x.vis->rec[static_cast<std::size_t>(i)]);
                if (kp < pool) continue;
                if (kp == pool && kr == rec) continue;
                if (obj(x.w, kp, kr).w1c != 0) continue;
                const std::int16_t k = pool_rec(kp, kr & 0xff);
                if (pair_test(x, pr, k) != 0) return static_cast<std::uint16_t>(k);
            }
        }
    }
    // [BYTES seg8:0x5305-0x530c]; the seg18 path seg8:0x530f-0x545a is [OPEN].
    return 0xffffu;
}

// ---- seg8:0x6afa

int pair_test(CollideCtx& x, std::int16_t cw, std::int16_t kw) {
    World& w = x.w;
    CollideGlobals& c = x.c;
    const int kp = pool_of(kw), kr = rec_of(kw), cp = pool_of(cw), cr = rec_of(cw);  // [BYTES seg8:0x6b09-0x6b26]
    ObjectRecord& K = obj(w, kp, kr);
    ObjectRecord& C = obj(w, cp, cr);
    if (K.b54 != 0 && C.b54 != 0) return 0;  // both fixed [BYTES seg8:0x6b3b seg8:0x6b4d seg8:0x6b53]
    if (K.state != 0) return 0;                // [BYTES seg8:0x6b65]
    if (!pt_in_rect(c.box, K.scr_x, K.scr_y)) return 0;  // [BYTES seg8:0x6b7d]
    c.other = rect16(K.scr_x, K.scr_y, add16(K.scr_x, K.size), add16(K.scr_y, K.size));  // [BYTES seg8:0x6ba6]
    c.meet = Rect16{};
    if (!intersect(c.square, c.other)) return 0;  // [BYTES seg8:0x6bb7]
    // A pool-10 object and its owner [BYTES seg8:0x6bc0-0x6c09]
    if (kp == 10 && K.owner_pool == cp && K.owner_rec == cr) return 0;
    if (cp == 10 && C.owner_pool == kp && C.owner_rec == kr) return 0;
    // Byte +0x2e, unless a ship with word +0x2e = 2 [BYTES seg8:0x6c1a-0x6c64]
    if (K.b2e != 0 && (kp != x.g.ship_pool || word_at(ship_at(w, kr), 0x2e) != 2)) return 0;
    if (C.b2e != 0 && (cp != x.g.ship_pool || word_at(ship_at(w, cr), 0x2e) != 2)) return 0;
    // A young missile and its launcher [BYTES seg8:0x6c67-0x6cb0]
    if (kp == 6) {
        const MissileSlot& m = w.pools.missile(kr);  // DGROUP:0x66e0 (records.h)
        if (m.age < 10 && m.launcher_pool == cp && m.launcher_rec == cr) return 0;
    }
    if (cp == 6) {
        const MissileSlot& m = w.pools.missile(cr);
        if (m.age < 10 && m.launcher_pool == kp && m.launcher_rec == kr) return 0;
    }
    // The ambulance [BYTES seg8:0x6cb3-0x6cc6]
    if ((cp == 7 || kp == 7) && x.g.dg_9e49 == 1) set_9e49(x, 0);
    // Both collided [BYTES seg8:0x6cda seg8:0x6cfa]
    K.w1c = 1;
    C.w1c = K.w1c;
    // The orientation, the last write winning; nothing else writes the word [DESIGN ADR 0015]
    // [BYTES seg8:0x6d0e seg8:0x6d23 seg8:0x6d3d seg8:0x6d57 seg8:0x6d5c]
    if (kp == x.g.ship_pool && kr == x.g.player) c.orientation = 1;
    if (cp == x.g.ship_pool && cr == x.g.player) c.orientation = 0;
    if (K.b54 != 0) c.orientation = 1;
    if (C.b54 != 0) c.orientation = 0;
    const ScreenPoint kpos = screen_of(x, K);
    if (c.orientation != 0) {  // K is A [BYTES seg8:0x6d65-0x6e47]
        c.Avx = K.vx;
        c.Avy = K.vy;
        c.Ax = kpos.x;
        c.Ay = kpos.y;
        c.b_pool = static_cast<std::int16_t>(cp);
        c.b_rec = static_cast<std::int16_t>(cr);
        c.a_pool = static_cast<std::int16_t>(kp);
        c.a_rec = static_cast<std::int16_t>(kr);
        if (x.g.dg_9130 == 0) {
            c.eB = add16(C.extent, 2);
            c.eA = add16(K.extent, 2);
        } else {
            c.eB = C.size;
            c.eA = K.size;
        }
        c.mA = K.mass;
    } else {  // K is B [BYTES seg8:0x6e4a-0x6f29]
        c.Bvx = K.vx;
        c.Bvy = K.vy;
        c.Bx = kpos.x;
        c.By = kpos.y;
        c.b_pool = static_cast<std::int16_t>(kp);
        c.b_rec = static_cast<std::int16_t>(kr);
        c.a_pool = static_cast<std::int16_t>(cp);
        c.a_rec = static_cast<std::int16_t>(cr);
        if (x.g.dg_9130 == 0) {
            c.eB = add16(K.extent, 2);
            c.eA = add16(C.extent, 2);
        } else {
            c.eB = K.size;
            c.eA = C.size;
        }
        c.mB = K.mass;
    }
    return 1;  // [BYTES seg8:0x6f2c]
}

// ---- seg8:0x546c

namespace {

// Response step 1 for one orientation: party p a pool-5 object meeting party q
// [BYTES seg8:0x548e-0x5520 seg8:0x5524-0x55b3].
bool pool5_seen(CollideCtx& x, int p_pool, int p_rec, int q_pool, int q_rec) {
    if (p_pool != x.g.pool5) return false;
    Pool5Entry& e = entry(x.c.pool5, p_rec, g_scratch_p5);
    if (e.hit_pool == q_pool && e.hit_rec == q_rec) return true;
    if (q_pool == x.g.ship_pool) {
        const ShipRecord& s = ship_at(x.w, q_rec);
        x.h.design_sound(x.h.design(x.w, s.design_index).hit_sound, q_pool, q_rec, 1);  // seg10:0x7944
    } else {
        x.h.sound(obj(x.w, q_pool, q_rec).hit_sound, q_pool, q_rec, 1);  // seg10:0x754a
    }
    e.hit_pool = static_cast<std::int16_t>(q_pool);
    e.hit_rec = static_cast<std::int16_t>(q_rec);
    return false;
}

// Step 4, the capture pass-through: a ship with +0xea = 0, +0xe8 = 0 and Captures Icons
// meeting a capturable object of heat >= 0, in both orientations [BYTES seg8:0x56dd-0x5774
// seg8:0x5912-0x59a9].
bool capture_pass(CollideCtx& x) {
    const CollideGlobals& c = x.c;
    auto test = [&](int sp, int sr, int op, int orc) {
        if (sp != x.g.ship_pool) return false;
        const ShipRecord& s = ship_at(x.w, sr);
        if (s.bea != 0 || s.be8 != 0) return false;
        if (x.h.design(x.w, s.design_index).captures == 0) return false;
        const ObjectRecord& o = obj(x.w, op, orc);
        return o.b53 != 0 && static_cast<std::int8_t>(o.flame) >= 0;
    };
    return test(c.b_pool, c.b_rec, c.a_pool, c.a_rec) || test(c.a_pool, c.a_rec, c.b_pool, c.b_rec);
}

}  // namespace

std::int16_t respond(CollideCtx& x, std::int16_t mass, std::int16_t ext2, std::int16_t* vy,
                     std::int16_t* vx, std::uint16_t hit) {
    (void)ext2;  // [bp+8] is never read (core.md "Response")
    CollideGlobals& c = x.c;
    World& w = x.w;
    int res = 1;                                       // [BYTES seg8:0x547b]
    const int hp = hit >> 8, hr = hit & 0xff;          // [BYTES seg8:0x5480-0x548b]
    // 1. Shot bookkeeping, both orientations
    if (pool5_seen(x, c.a_pool, c.a_rec, c.b_pool, c.b_rec)) return -1;
    if (pool5_seen(x, c.b_pool, c.b_rec, c.a_pool, c.a_rec)) return -1;
    // 2. The ambulance's pass-through [BYTES seg8:0x55b7-0x55fa]
    if (x.g.dg_9e49 >= 3 && x.g.dg_9e49 <= 6) {
        if (c.b_pool == 7 && c.a_pool == x.g.ship_pool && c.a_rec == x.w.w952e) return -1;
        if (c.a_pool == 7 && c.b_pool == x.g.ship_pool && c.b_rec == x.w.w952e) return -1;
    }
    const bool hit_is_a = hp == c.a_pool && hr == c.a_rec;  // [BYTES seg8:0x55fd-0x560e]
    if (hit_is_a) {
        // The caller is B [BYTES seg8:0x5611-0x56d7]
        const ObjectRecord& B = obj(w, c.b_pool, c.b_rec);
        const ObjectRecord& A = obj(w, c.a_pool, c.a_rec);
        c.Bvx = B.vx;
        c.Bvy = B.vy;
        c.Avx = A.vx;
        c.Avy = A.vy;
        const ScreenPoint bp = screen_of(x, B);
        c.Bx = bp.x;
        c.By = bp.y;
        c.mB = mass;
        if (c.a_pool == 0 && c.b_pool == 2) c.mA = mul16(10, c.mA);
        if (obj(w, c.a_pool, c.a_rec).b54 != 0) c.mA = 20000;
    } else {
        // The caller is A [BYTES seg8:0x5870-0x590c]
        const ObjectRecord& B = obj(w, c.b_pool, c.b_rec);
        const ObjectRecord& A = obj(w, c.a_pool, c.a_rec);
        c.Bvx = B.vx;
        c.Bvy = B.vy;
        c.Avx = A.vx;
        c.Avy = A.vy;
        const ScreenPoint ap = screen_of(x, A);
        c.Ax = ap.x;
        c.Ay = ap.y;
        c.mA = mass;
        if (obj(w, c.a_pool, c.a_rec).b54 != 0) c.mA = 20000;
    }
    // 4. The capture pass-through
    if (capture_pass(x)) return -1;
    // 5. Units [BYTES seg8:0x5777-0x5791 seg8:0x59ac-0x59c6]
    if (x.g.dg_9130 == 0) {
        const int s = w.shift & 0x1f;
        c.eA = add16(sar16(c.eA, s), 1);
        c.eB = add16(sar16(c.eB, s), 1);
    }
    // 6. Physics: the bounce, or result 1 with a pool-5 party [BYTES seg8:0x5794-0x57e0
    // seg8:0x59c9-0x5a15]
    if (c.b_pool != x.g.pool5 && c.a_pool != x.g.pool5)
        res = bounce(&c.dA, &c.dB, c.mA, c.mB, c.eA, c.eB, &c.Ay, &c.Ax, &c.By, &c.Bx, &c.Avy,
                     &c.Avx, &c.Bvy, &c.Bvx);
    if (res == 0) {
        // 7. Separating: the hit object's velocity and `collided`, no dispatch [BYTES
        // seg8:0x57e3-0x57ef seg8:0x5a18-0x5a24]
        if (hit_is_a)
            tail_a(x, 0);
        else
            tail_b(x, 0);
        return -1;
    }
    // 8. Result 1 (s1-hit-verify.md claim 29, CORRECTED: the caller takes its own slot)
    if (hit_is_a) {
        // [BYTES seg8:0x57f2-0x586d]
        *vx = c.Bvx;
        *vy = c.Bvy;
        ObjectRecord& B = obj(w, c.b_pool, c.b_rec);
        if (B.b54 == 0) {
            const ScreenPoint p = screen_to_world(x.view, c.Bx, c.By, x.g.dg_9130);
            B.x = p.x;
            B.y = p.y;
        }
        c.dA = add16(c.dA, mul16(x.dg_9454, 5));
        tail_a(x, 1);
        return c.dB;
    }
    // [BYTES seg8:0x5a27-0x5aa0]
    *vx = c.Avx;
    *vy = c.Avy;
    ObjectRecord& B = obj(w, c.b_pool, c.b_rec);
    if (B.b54 == 0) {
        const ScreenPoint p = screen_to_world(x.view, c.Bx, c.By, x.g.dg_9130);
        B.x = p.x;
        B.y = p.y;
    }
    c.dB = add16(c.dB, shl16(x.dg_9454, 2));
    tail_b(x, 1);
    return c.dA;
}

void tail_a(CollideCtx& x, int flag) {
    CollideGlobals& c = x.c;
    ObjectRecord& A = obj(x.w, c.a_pool, c.a_rec);  // [BYTES seg8:0x69a8 seg8:0x69c1 seg8:0x69d8]
    A.w1c = 1;
    A.vx = c.Avx;
    A.vy = c.Avy;
    if (flag == 0) return;  // [BYTES seg8:0x69e0]
    if (A.b54 != 0) {       // [BYTES seg8:0x69ff seg8:0x6a1b]
        A.vy = 0;
        A.vx = A.vy;
    }
    dispatch(x, c.a_pool, c.a_rec, c.b_pool, c.b_rec, c.dA);  // [BYTES seg8:0x6a1f-0x6a34]
}

void tail_b(CollideCtx& x, int flag) {
    CollideGlobals& c = x.c;
    ObjectRecord& B = obj(x.w, c.b_pool, c.b_rec);  // [BYTES seg8:0x6a62 seg8:0x6a7b seg8:0x6a92]
    B.w1c = 1;
    B.vx = c.Bvx;
    B.vy = c.Bvy;
    if (flag == 0) return;
    if (B.b54 != 0) {  // [BYTES seg8:0x6aae seg8:0x6ad5]
        B.vy = 0;
        B.vx = B.vy;
    }
    dispatch(x, c.b_pool, c.b_rec, c.a_pool, c.a_rec, c.dB);  // [BYTES seg8:0x6aea]
}

// ---- seg8:0x5df2

std::int16_t call_on_damage(CollideCtx& x, int pool, int rec, std::int16_t a) {
    const std::uint32_t id = obj(x.w, pool, rec).on_damage.get();
    if (id == kDamageShip) return ship_damage(x, pool, rec, a);
    if (id == kDamageRock) return rock_damage(x, pool, rec, a);
    if (id == kDamageIcon) return icon_damage(x, pool, rec, a);  // seg7:0x233c (M4 s2 GUN)
    // seg7:0x2652, the generic handler of the missiles and the other generic records (missile.h
    // generic_on_damage); its far call through +0x66 is CollideHost::on_destroy (M4 s2 fixer).
    if (id == kDamageGeneric) {
        struct Forward final : MissileHost {
            CollideHost& h;
            explicit Forward(CollideHost& c) : h(c) {}
            void on_destroy(int p, int r) override { h.on_destroy(p, r); }
        } f{x.h};
        return generic_on_damage(x.w.pools, pool, rec, a, f);
    }
    // The ship set-up does not write the ship pool's slot yet (seg14:0x2158, the set-up's
    // owner): a ship record without one takes seg8:0x45e0 [DESIGN].
    if (id == 0 && pool == x.g.ship_pool) return ship_damage(x, pool, rec, a);
    return x.h.other_damage(id, pool, rec, a);
}

std::int16_t dispatch(CollideCtx& x, int tp, int tr, int xp, int xr, std::int16_t a) {
    World& w = x.w;
    WaveGlobals& g = x.g;
    CollideGlobals& c = x.c;
    const int ships = g.ship_pool;
    int disp = 1, big = 0, neg = 0;  // [BYTES seg8:0x5e00 seg8:0x5e05 seg8:0x5e41]
    const std::int16_t mT = obj(w, tp, tr).mass;                       // [BYTES seg8:0x5e18]
    const std::int16_t mX = xp == 0x1f ? std::int16_t{20000} : obj(w, xp, xr).mass;  // [BYTES seg8:0x5e1f-0x5e3e]
    if (a < -1) {  // [BYTES seg8:0x5e46-0x5e51]
        neg = 1;
        a = sub16(0, a);
    }
    int cp = -1, cr = 0;
    std::int16_t res = 0;
    if (xp > ships) {
        // seg18 attackers go straight to the handler [BYTES seg8:0x5e54-0x5e5c seg8:0x642c],
        // past the -1 test of seg8:0x5e93: a separating response's -1 (not negated: `cmp -1;
        // jge` at seg8:0x5e46) reaches the handler as the amount [BYTES seg8:0x5e46 seg8:0x5e4a]
        // (M4 s1 fixer, review M12);
        // the credit rec [bp-0x12] is never written on this path (s1-hit-verify.md 3): the port
        // passes 0 [OPEN].
        cp = xp;
    } else {
        // 1.1 the amount rules [BYTES seg8:0x5e5f-0x5e8e]
        if (a > 2000 && a < 3000) {
            a = sub16(a, 2000);
            big = 1;
            if (xp == ships && x.h.design(w, ship_at(w, xr).design_index).team == 7) a = 1;
        }
        if (a == -1) return 0;  // [BYTES seg8:0x5e93-0x5e9b]
        // The OverThruster ram [BYTES seg8:0x5e9e-0x5ef0]
        if (ship_at(w, g.player).thrust_mult > 2 && xp == ships && xr == g.player &&
            (tp == g.rock_pool || tp == 8 || tp == 9) && g.play_mode < 2) {
            x.h.message(g.player, 7, 0, 0xd);
            x.h.seg13_b6f2(g.player, 9);
        }
        // The credit [BYTES seg8:0x5ef3-0x5f3b]
        cp = -1;
        {
            const ObjectRecord& X = obj(w, xp, xr);
            if (X.owner_pool == ships) {
                cp = ships;
                cr = X.owner_rec;
            }
            const ObjectRecord& T = obj(w, tp, tr);
            if (T.owner_pool == ships) {
                cp = ships;
                cr = T.owner_rec;
            }
        }
        // A tethered target [BYTES seg8:0x5f3e-0x5fe2]
        if (obj(w, tp, tr).owner_pool != -1) {
            const int s = static_cast<std::int8_t>(obj(w, tp, tr).w30 & 0xff);
            std::int16_t& tw = entry(c.link_65da, s, g_scratch_word);
            if (add16(mX, 100) > mT) tw = tw != 0 ? 0 : 1;
            const std::int16_t r = static_cast<std::int16_t>(x.rng.rand());
            if (add16(imod16(r, 12), 30) < a && a != 100 && tp == ships && s >= 0 && s < kLinkSlots) {
                const LinkSlot& l = w.links[static_cast<std::size_t>(s)];
                x.h.seg11_947a(l.pool_b, l.rec_b, 2);
            }
        }
        // Pool 5, the FIREBALL rule and the shooter [BYTES seg8:0x5fe5-0x6020]
        if (xp == g.pool5) {
            if (obj(w, g.pool5, xr).mass == 0x78) {
                const std::int16_t d2 = add16(a, a);
                a = d2 < 20 ? std::int16_t{20} : d2;
            }
            const Pool5Entry& e = entry(c.pool5, xr, g_scratch_p5);
            cp = e.shooter_pool;
            cr = e.shooter_rec;
        }
        // Pool 6, the launcher [BYTES seg8:0x6023-0x6067]
        if (xp == 6) {
            const MissileSlot& m = w.pools.missile(xr);  // DGROUP:0x66e0 (records.h)
            cp = m.launcher_pool;
            cr = m.launcher_rec;
            if (x.h.weapon_kind(m.weapon) == 6 && tp == ships) {
                const ShipRecord& ts = ship_at(w, tr);
                if (x.h.weapon_kind(sbyte_at(ts, 0xb8 + ts.weapon_slot)) == 6) disp = 0;
            }
        }
        // Pool 10 or an owned attacker [BYTES seg8:0x606c-0x60a2]
        {
            const ObjectRecord& X = obj(w, xp, xr);
            if (xp == 10 || X.owner_pool != -1) {
                cp = X.owner_pool;
                cr = X.owner_rec;
            }
        }
        if (cp == -1) {  // [BYTES seg8:0x60a5-0x60b4]
            cp = xp;
            cr = xr;
        }
        // 1.2 the credit bookkeeping [BYTES seg8:0x60b7-0x6303] (s1-hit-verify.md: not
        // verified, "Not covered by any claim") [OPEN]
        if (cp == 9 || cp == 2 || cp == ships) {
            if (cp == ships) {
                ShipRecord& cs = ship_at(w, cr);
                if (cs.w38 == 1 && cs.w32 == tp && cs.w34 == tr) set_ship_dword(cs, 0x144, g.now);
                for (int j = 0; j <= w.last_ship; ++j) {
                    if (j == cr) continue;
                    const ShipRecord& js = ship_at(w, j);
                    if (js.bec != 0 && js.w32 == tp && js.w34 == tr && js.w38 == 1)
                        x.h.seg13_2714(j, cr, 5, 10, 0, -1);
                }
            }
            if ((tp == ships || tp == 0) && c.w0b0c != 0) {
                const DefenderEntry& dc = defender(c, cr);
                if (cp == ships || (cp == 2 && !(dc.owner_pool == tp && dc.owner_rec == tr))) {
                    for (int k = 0; k < static_cast<int>(c.defenders.size()); ++k) {
                        const DefenderEntry& d = c.defenders[static_cast<std::size_t>(k)];
                        if (d.owner_pool == tp && d.owner_rec == tr) x.h.seg8_9028(k, 10, cp, cr);
                    }
                    if (cp == ships && tp == ships) {
                        if (!(xp == ships && big == 0)) x.h.seg4_5e8e(tr, cr, a, 1);
                        const ShipRecord& ts = ship_at(w, tr);
                        if (cr == g.player && ts.w38 == 4 && ts.w34 == cr) w.w9432 = add16(w.w9432, a);
                        x.h.seg4_7d36(cr, tr, a);
                    }
                    if (cp == ships && cr == g.player && xp != 10 && tp != ships) x.h.seg8_81f8(tp, tr);
                }
            }
            if (cp == ships && cr == g.player && !(tp == ships && tr == g.player)) {
                c.hit_pool = static_cast<std::int16_t>(tp);
                c.hit_rec = static_cast<std::int16_t>(tr);
                c.hit_time = g.now;
                if (xp != ships && tp == ships) w.w9432 = add16(w.w9432, idiv16(a, 2));
                if (xp != ships && (tp == g.rock_pool || tp == 8)) w.w9432 = add16(w.w9432, idiv16(a, 4));
                if (xp != ships && tp == 0 && obj(w, 0, tr).b53 == 1) w.w9432 = add16(w.w9432, idiv16(a, 3));
            }
        }
        // 1.3 the gates [BYTES seg8:0x6303-0x6425]
        const ObjectRecord& T = obj(w, tp, tr);
        if (T.b53 != 0) {
            if (xp == 2) disp = 0;
            if (xp == ships) {
                const ShipRecord& xs = ship_at(w, xr);
                if (xs.type < 6 && x.h.design(w, xs.design_index).captures != 0 && xs.be8 == 0 &&
                    static_cast<std::int8_t>(T.flame) >= 0 && neg == 0 && big == 0)
                    disp = 0;
            }
            if (big != 0 && cp == 9) disp = 1;
        }
        if (tp == 11 && xp == ships && entry(c.pool11_856a, tr, g_scratch_word) == xr &&
            ship_at(w, xr).bec == 0)
            disp = 0;
        if (tp == ships && ship_at(w, tr).bec == 0 && xp == 11 &&
            entry(c.pool11_856a, xr, g_scratch_word) == tr)
            disp = 0;
        if (tp == 2) {
            const DefenderEntry& d = defender(c, tr);
            if ((d.owner_pool == xp && d.owner_rec == xr) || xp == 2) disp = 0;
            a = sub16(0, a);
        }
        res = 0;
        if (xp == 10) {
            x.h.seg12_3498(tp, tr, xr);
            disp = 0;
        }
    }
    // 1.4 the handler [BYTES seg8:0x6432-0x64ba]
    if (disp != 0) {
        if (tp != ships) x.h.sound(obj(w, tp, tr).hit_sound, tp, tr, 1);  // [BYTES seg8:0x643b-0x645d]
        res = call_on_damage(x, tp, tr, a);                                 // [BYTES seg8:0x647c seg8:0x6483]
        const bool by_player = cp == ships && cr == g.player;
        if (by_player && !(tp == ships && tr == g.player)) kill_score(x, tp, tr, a, res);  // [BYTES seg8:0x6486-0x64ae]
        if (res != 0) {
            // 1.5 the target destroyed [BYTES seg8:0x64bd-0x686a]
            if (!by_player && !(tp == ships && tr == g.player)) x.h.seg8_7ce0(tp, tr, a, res);
            int kill = 0;
            if (cp == ships) {
                ShipRecord& cs = ship_at(w, cr);
                if (xp != 10 && cs.w32 == tp && cs.w34 == tr && cs.w38 == 1) {  // [BYTES seg8:0x6500-0x655e]
                    if (tp == 0 && obj(w, 0, tr).b53 == 1) x.h.seg13_b6f2(cr, 1);
                    if (tp == 8 && pool8_at(w, tr).at(0x0a) == 0x10) x.h.seg13_b6f2(cr, 6);
                }
                bool done = false;
                if (cs.w52 >= 2 && cs.w52 <= 4 && cs.w58 != cr && cs.w54 == tp && c.w0b0c != 0) {
                    // [BYTES seg8:0x6561-0x661d]; the texts of codes 0x75 ... 0x77 (strings
                    // 1409 ... 1411, sound 0x3b) are the message display's (s1-hit.md claim 38,
                    // unchecked) [OPEN]
                    done = true;
                    bool said = false;
                    if (tp == 9) {
                        x.h.message(cr, ships, cs.w58, 0x75);
                        x.h.seg13_2714(cs.w58, cr, 15, 40, 0, 6);
                        said = true;
                    } else if (cs.w56 == tr) {
                        x.h.message(cr, ships, cs.w58, 0x76);
                        x.h.seg13_2714(cs.w58, cr, 50, 90, 0, 6);
                        said = true;
                    }
                    if (said) {
                        if (g.play_mode == 0 || (cr != g.player && w.player_game == 0)) cs.w52 = 0;
                        kill = 1;
                    }
                }
                if (!done) {
                    if (cs.w52 == 1 && tp == ships) {  // [BYTES seg8:0x661f-0x6680]
                        const ShipRecord& ts = ship_at(w, tr);
                        if (ts.w32 == ships && ts.w34 == cs.w58 && ts.w38 == 1) {
                            x.h.message(cr, ships, cs.w58, 0x77);
                            x.h.seg13_2714(cs.w58, cr, 60, 120, 0, 8);
                            kill = 1;
                        }
                    } else {  // [BYTES seg8:0x6683-0x66e8]
                        for (int j = 0; j <= w.last_ship; ++j) {
                            if (j == cr) continue;
                            const ShipRecord& js = ship_at(w, j);
                            if (js.bec != 0 && js.w32 == tp && js.w34 == tr && js.w38 == 1) {
                                x.h.seg13_2714(j, cr, 30, 50, 0, 6);
                                x.h.message(cr, ships, j, 0x77);
                            }
                        }
                    }
                }
            }
            // The race marker [BYTES seg8:0x66ea-0x674a] (M6)
            if (tp == 8 && w.kind == 6) {
                const std::size_t k = static_cast<std::size_t>(w.course) % w.race_9eb9.size();
                if (add16(w.race_9eb9[k], add16(w.gates, w.gates)) <= tr) x.h.race_marker(tr);
            }
            // A virused icon, a virus [BYTES seg8:0x674f-0x67c3]
            if (tp == 0 && obj(w, 0, tr).b53 == 2 && by_player) {
                x.h.message(cr, 7, 0, 0x5e);
                w.score = add32u(w.score, 3000);
            }
            if (tp == 9 && by_player) {
                w.score = add32u(w.score, 1000);
                if (kill == 0) x.h.message(g.player, 7, 0, 0x5b);
            }
            x.h.seg7_2080(tp, tr);  // [BYTES seg8:0x67cc]
            if (cp == ships && tp == ships) {  // a ship kill [BYTES seg8:0x67d4-0x6835]
                if (cr == g.player) w.w9432 = add16(w.w9432, 200);
                x.h.seg13_2714(tr, cr, -50, -150, 1, -1);
                for (int j = 0; j < w.last_ship; ++j)
                    if (x.h.seg13_2688(j, tr) > 0) x.h.seg13_2e60(4, cr, j);
            }
            if (tp == ships) x.h.seg4_7636(tr, cp, cr, kill);  // [BYTES seg8:0x684b]
            ship_at(w, g.player).due156.set(g.now - 1u);       // [BYTES seg8:0x6853-0x686a]
        }
    }
    // 1.6 the tail [BYTES seg8:0x686e-0x697f]
    seg8_99ce(x, tp, tr, cp, cr, res);
    if (tp == ships) {
        ShipRecord& ts = ship_at(w, tr);
        if (disp != 0 && ts.bec != 0) {
            // The tumble and the lock-out (s1-hit-verify.md 22, CORRECTED) [BYTES seg8:0x68a5
            // seg8:0x68b8 seg8:0x68bf seg8:0x68cb-0x68e9 seg8:0x68f0-0x6950]
            ts.be2 = 1;
            ObjectRecord& T = obj(w, tp, tr);
            if (T.angle == 0) T.angle = 1;
            std::int16_t L;
            if (big != 0) {
                L = mul16(a, 25);
                if (L < 900) L = 900;
            } else if (xp == 8 && pool8_at(w, xr).at(0x0a) == -3) {
                L = mul16(a, 6);
                if (L < 300) L = 300;
            } else {
                L = mul16(a, 12);
                if (L < 600) L = 600;
                // The seg18 mode scales L: (int16)(L·(signed byte DGROUP:0xa198 + 3)) / 6 when
                // DGROUP:0x90f4 = 1, else (int16)(L·(byte DGROUP:0xa198 + 3)) / 12, 16-bit
                // `imul` and `cwd` [BYTES seg8:0x691e-0x694e] (s1-hit-verify.md 22, CORRECTED);
                // DGROUP:0x90f4 is the game state, DGROUP:0xa198's writer is the seg18 mode's
                // [OPEN M6].
                if (g.dg_9130 != 0) {
                    if (c.state_90f4 == 1)
                        L = idiv16(mul16(L, add16(static_cast<std::int8_t>(c.ba198), 3)), 6);
                    else
                        L = idiv16(mul16(L, add16(static_cast<std::int16_t>(c.ba198), 3)), 12);
                }
            }
            ts.lockout.set(add32u(g.now, L));
        }
    } else if (tp == 8) {
        // [BYTES seg8:0x6952-0x697b]
        Pool8Entry& e = pool8_at(w, tr);
        const std::uint32_t t = add32u(g.now, mul16(a, big != 0 ? 200 : 140));
        e.at(0x04) = to_i16(t & 0xffffu);
        e.at(0x06) = to_i16(t >> 16);
    }
    return res;  // [BYTES seg8:0x697f]
}

// ---- seg8:0x7be8

void kill_score(CollideCtx& x, int pool, int rec, std::int16_t a, std::int16_t res) {
    World& w = x.w;
    const int ships = x.g.ship_pool;
    if (pool == ships && rec == x.g.player) return;  // [BYTES seg8:0x7bf3-0x7c03]
    auto rating = [&]() { return static_cast<std::int32_t>(w.rating); };
    if (res != 0) {
        const std::int16_t P = kill_points(pool);
        std::int32_t r = static_cast<std::int32_t>(static_cast<std::uint32_t>(rating()) + static_cast<std::uint32_t>(static_cast<std::int32_t>(P)));
        if (r > 500000) r = 500000;  // [BYTES seg8:0x7c22-0x7c34]
        w.rating = static_cast<std::uint32_t>(r);
        w.score = add32u(w.score, P);  // [BYTES seg8:0x7c3b]
        if (pool == ships) w.rating = add32u(w.rating, add16(ship_at(w, rec).kill_value, ship_at(w, rec).kill_value));
        if (pool == 8 && obj(w, 8, rec).b54 != 0) w.rating = add32u(w.rating, P);  // [BYTES seg8:0x7c6d seg8:0x7c79]
        return;
    }
    const std::int16_t v = mul16(kill_weight(pool), a);  // [BYTES seg8:0x7c84 seg8:0x7c88]
    w.rating = add32u(w.rating, v);
    if (pool == 8 && obj(w, 8, rec).mass > 2000) w.rating = add32u(w.rating, add16(v, v));  // [BYTES seg8:0x7ca2 seg8:0x7caa]
    if (pool == ships) w.rating = add32u(w.rating, idiv16(ship_at(w, rec).kill_value, 8));  // [BYTES seg8:0x7cc2 seg8:0x7ccc]
}

// ---- seg8:0x99ce

void seg8_99ce(CollideCtx& x, int tp, int tr, int cp, int cr, std::int16_t res) {
    (void)res;
    World& w = x.w;
    CollideGlobals& c = x.c;
    // An icon remembers who hit it, by the literal pool 0xe [BYTES seg8:0x99dc-0x99fc]
    if (tp == 0 && (cp == 9 || cp == 0xe) && tr >= 0 && tr < kIconRecords) {
        w.icons[static_cast<std::size_t>(tr)].b0a = static_cast<std::uint8_t>(cp);
        w.icons[static_cast<std::size_t>(tr)].b0b = static_cast<std::uint8_t>(cr);
    }
    // The first idle defender aimed at the target turns on the attacker [BYTES seg8:0x9a00-0x9aa9]
    for (int k = 0; k < static_cast<int>(c.defenders.size()); ++k) {
        if (obj(w, 2, k).state != 0) continue;
        if (cp != 0xe && cp != 2) continue;
        if (cp == 2) {
            const DefenderEntry& d = defender(c, cr);
            if (d.w00 != 0xf) continue;
            const DefenderEntry& e = c.defenders[static_cast<std::size_t>(k)];
            if (e.owner_pool == d.owner_pool && e.owner_rec == d.owner_rec) continue;
        }
        const DefenderEntry& e = c.defenders[static_cast<std::size_t>(k)];
        if (e.owner_pool != tp || e.owner_rec != tr) continue;
        if (e.owner_pool == cp && e.owner_rec == cr) continue;
        if (e.w00 == 0xf) continue;
        x.h.seg8_9028(k, 0xf, cp, cr);
        return;
    }
}

}  // namespace is::world
