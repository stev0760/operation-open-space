// The gun and its shots (shots.h). Every site cites the listing it ports.
#include "world/shots.h"

#include "world/bounce.h"
#include "world/damage.h"
#include "world/design_words.h"

namespace is::world {

namespace {

std::uint32_t add32u(std::uint32_t a, std::int32_t b) { return a + static_cast<std::uint32_t>(b); }

// A 16-bit `idiv` with the port's guard: a zero divisor, which the original traps on, gives 0
// [DESIGN].
std::int16_t q16(std::int16_t n, std::int16_t d) { return d == 0 ? std::int16_t{0} : idiv16(n, d); }
// `cwd; sub ax, dx; sar ax, 1`: / 2 toward zero.
std::int16_t half(std::int16_t v) { return idiv16(v, 2); }

// The world size DGROUP:0x5a0 << 3 and DGROUP:0x5a2 << 3 (both 25600) [BYTES DGROUP:0x5a0].
constexpr std::int16_t kWrapX = kWorldUnits;
constexpr std::int16_t kWrapY = kWorldUnits;

// (sext32(v) + (sext32(DG5a0) << 3)) % sext32(lo16(DG5a0 << 3)) through seg5:0x8c0 and the
// signed remainder seg5:0x1aa, the low word kept [BYTES seg11:0x5338-0x5360 seg8:0x742a-0x747e].
std::int16_t wrap_mod(std::int16_t v, std::int16_t world) {
    const std::int32_t n = static_cast<std::int32_t>(v) + static_cast<std::int32_t>(world);
    return lo16(n % static_cast<std::int32_t>(world));
}

std::uint8_t& obj_byte(ObjectRecord& o, int off) { return reinterpret_cast<std::uint8_t*>(&o)[off]; }

ShipRecord& ship_of(World& w, int i) { return ship_at(w, i); }

// seg8:0x29ea, the DGROUP:0x9130 = 0 path the flight view ports.
ScreenPoint to_screen(const ShotCtx& c, std::int16_t x, std::int16_t y) {
    return world_to_screen(c.x.view, x, y, c.x.g.wave_kind, c.x.g.dg_9eb5, c.x.g.dg_9130);
}

// The screen point of an object: (+0x00, +0x02) while `on_screen`, else seg8:0x29ea of its
// world position [BYTES seg11:0x4ed8-0x4f17].
ScreenPoint screen_of(const ShotCtx& c, const ObjectRecord& o) {
    if (o.on_screen != 0) return {o.scr_x, o.scr_y};
    return to_screen(c, o.x, o.y);
}

// seg8:0x1290(g, n): one Newton step of sqrt(n) from max(g, 3), 32-bit: (n / g + g) >> 1
// [BYTES seg8:0x129d-0x12c4].
std::int32_t newton_1290(std::int32_t g, std::int32_t n) {
    if (g < 3) g = 3;
    return sar32(static_cast<std::int32_t>(static_cast<std::uint32_t>(n / g) + static_cast<std::uint32_t>(g)), 1);
}

const ShotState* g_shot_source = nullptr;

}  // namespace

// ---- the table

std::int16_t& ShotState::word(int j, int off) {
    const int i = kShotSlotWords * j + off / 2;
    if (j < 0 || off < 0 || (off & 1) != 0 || i >= static_cast<int>(words.size())) {
        scratch_ = 0;
        return scratch_;
    }
    return words[static_cast<std::size_t>(i)];
}

std::int16_t ShotState::word(int j, int off) const {
    const int i = kShotSlotWords * j + off / 2;
    if (j < 0 || off < 0 || (off & 1) != 0 || i >= static_cast<int>(words.size())) return 0;
    return words[static_cast<std::size_t>(i)];
}

std::uint32_t ShotState::dword(int j, int off) const {
    return static_cast<std::uint16_t>(word(j, off)) |
           (static_cast<std::uint32_t>(static_cast<std::uint16_t>(word(j, off + 2))) << 16);
}

void ShotState::set_dword(int j, int off, std::uint32_t v) {
    word(j, off) = to_i16(v & 0xffffu);
    word(j, off + 2) = to_i16(v >> 16);
}

std::int16_t& ShotState::muzzle_word(int n, int k, int xy) {
    if (n < 0 || n >= kShipCount || k < 0 || k >= 64) {
        scratch_ = 0;
        return scratch_;
    }
    return muzzle[static_cast<std::size_t>((64 * n + k) * 2 + xy)];
}

ShotState& shot_state() {
    static ShotState s;
    return s;
}

void place_shot_slots(ShotState& s) {
    for (int j = 0; j < kShotSlots; ++j) {
        s.word(j, shot::kAge) = -1;
        s.word(j, shot::kLife) = 50;
    }
}

void set_shot_source(const ShotState* s) { g_shot_source = s; }

ShotSlotView shot_view(int k) {
    const ShotState& s = g_shot_source ? *g_shot_source : shot_state();
    ShotSlotView v;
    if (k < 0 || k >= kShotSlots) return v;
    // [BYTES seg8:0x47a7 seg8:0x47ae seg8:0x47b5 seg8:0x47bc seg8:0x47cb seg8:0x47d4]
    v.vx = s.word(k, shot::kVx);
    v.vy = s.word(k, shot::kVy);
    v.x = s.word(k, shot::kWorldX);
    v.y = s.word(k, shot::kWorldY);
    v.owner_pool = s.word(k, shot::kOwnerPool);
    v.owner_rec = s.word(k, shot::kOwnerRec);
    return v;
}

// ---- the muzzle tables

void muzzle_1460(ShotState& s, int n, const DesignWords& d) {
    const std::int16_t S = d.w2a;              // [BYTES seg10:0x146f-0x1478]
    const std::int16_t r = add16(d.w00, 3);    // [BYTES seg10:0x147b-0x1482]
    std::int16_t a = 0x1c2;                    // 450 [BYTES seg10:0x148a]
    for (int k = 0; k < 16; ++k) {
        // x = lo16(cos·r) / 128 + S / 2, y = S / 2 - lo16(sin·r) / 128, each / toward zero
        // [BYTES seg10:0x1491-0x14dd]
        s.mx(n, k) = add16(idiv16(mul16(cos128(a), r), 128), half(S));
        s.my(n, k) = sub16(half(S), idiv16(mul16(sin128(a), r), 128));
        a = sub16(a, static_cast<std::int16_t>((k + 1) % 2 + 22));  // [BYTES seg10:0x14e1-0x14f0]
    }
}

// seg10:0x1620, the rear muzzle table (core.md The muzzle tables; C033, C034): r = design word
// +0x02, a from 270, /127 and /(-127).
void muzzle_1620(ShotState& s, int n, const DesignWords& d) {
    const std::int16_t S = d.w2a;   // [BYTES seg10:0x1634]
    const std::int16_t r = d.w02;   // [BYTES seg10:0x163b]
    std::int16_t a = 0x10e;         // 270 [BYTES seg10:0x1647]
    for (int k = 0; k < 16; ++k) {
        // x = lo16(cos·r) / 127 + S / 2, y = S / 2 + lo16(sin·r) / -127 [BYTES seg10:0x164e-0x1694]
        s.mx(n, 16 + k) = add16(idiv16(mul16(cos128(a), r), 0x7f), half(S));
        s.my(n, 16 + k) = add16(half(S), idiv16(mul16(sin128(a), r), -0x7f));
        a = sub16(a, static_cast<std::int16_t>((k + 1) % 2 + 22));  // [BYTES seg10:0x1698-0x16a7]
    }
}

void muzzle_1502(ShotState& s, int n, const DesignWords& d) {
    const std::int16_t S = d.w2a;  // [BYTES seg10:0x1516]
    const std::int16_t ox[2] = {d.w68, d.w6c}, oy[2] = {d.w6a, d.w6e};  // [BYTES seg10:0x1522-0x1549]
    for (int b = 0; b < 2; ++b) {
        std::int16_t a = 0;  // [BYTES seg10:0x160b-0x1610]
        for (int k = 0; k < 16; ++k) {
            const std::int16_t c = cos128(a), sn = sin128(a);
            // The /128 (toward zero) on the 16-bit difference and sum, s2-gun-verify.md 14
            // [BYTES seg10:0x156e-0x15fb]
            s.mx(n, 32 + 16 * b + k) = add16(half(S), idiv16(sub16(mul16(ox[b], c), mul16(oy[b], sn)), 128));
            s.my(n, 32 + 16 * b + k) = add16(idiv16(add16(mul16(oy[b], c), mul16(ox[b], sn)), 128), half(S));
            a = add16(a, static_cast<std::int16_t>((k + 1) % 2 + 22));  // [BYTES seg10:0x1553-0x1562]
        }
    }
}

DesignWords design_words_from_world(const World& w, int n) {
    (void)w;
    const DesignWordSet& s = design_words(n);
    DesignWords d;
    d.w00 = s.w00;  // DGROUP:0x96d2 + 0xad·n, seg14:0x0's s [BYTES seg14:0xc50] (M4 s5 fixer)
    d.w02 = s.w02;  // DGROUP:0x96d4 + 0xad·n [BYTES seg14:0xc54]
    d.w2a = s.w2a;  // DGROUP:0x96fc + 0xad·n
    d.w68 = s.w68;  // DGROUP:0x973a ... 0x9740 + 0xad·n
    d.w6a = s.w6a;
    d.w6c = s.w6c;
    d.w6e = s.w6e;
    return d;
}

void fill_muzzles(ShotState& s, int n, const DesignWords& d) {
    muzzle_1460(s, n, d);
    muzzle_1620(s, n, d);
    muzzle_1502(s, n, d);
}

// ---- the brushes

// [BYTES seg10:0x4f52-0x5060] (Bright = 0)
const std::array<std::uint32_t, 20> kShotBrushes = {
    0x0200FFFF, 0x0200D7DC, 0x02008CC8, 0x020064AF, 0x02004187, 0x02001969, 0x02000550,
    0x02000032, 0x02000019, 0x0200000A, 0x02FFFF00, 0x02E6D700, 0x02CDAF00, 0x02A08200,
    0x02826400, 0x026E3C00, 0x025A1E00, 0x023C0000, 0x021A0000, 0x020A0000};
// [BYTES seg10:0x4e40-0x4f4f seg10:0x505b] (Bright != 0), s2-gun.md 3.2's RRGGBB as 0x02bbggrr
const std::array<std::uint32_t, 20> kShotBrushesBright = {
    0x0200FFFF, 0x0200EBEB, 0x0200D7DC, 0x0200B4D2, 0x02008CC8, 0x020078B9, 0x020064AF,
    0x02004187, 0x02004187, 0x02001969, 0x02FFFF00, 0x02F0EB00, 0x02E6D700, 0x02D7CD00,
    0x02CDAF00, 0x02C6A500, 0x02BE9B00, 0x02A08200, 0x02826400, 0x02826400};

std::int16_t shot_brush_index(std::int16_t age, std::int16_t lifetime, bool ship_shot, std::int8_t row_e3) {
    const std::int16_t b = q16(shl16(age, 3), lifetime);  // [BYTES seg8:0x3c75-0x3c7d]
    if (ship_shot) return add16(b, mul16(10, row_e3));   // `imul byte` [BYTES seg8:0x3c91-0x3ca1]
    return add16(b, 10);                                 // DGROUP:0x3b60 + 2b [BYTES seg8:0x3caf]
}

// ---- seg8:0x764

void trail_point(ShotCtx& c, int k, int j) {
    ShotState& s = c.s;
    for (int a = 0; a < s.word(j, shot::kBarrels); ++a) {  // [BYTES seg8:0x779 seg8:0x7c2]
        const int o = 12 * a + 2 * k;
        const ScreenPoint p = to_screen(c, s.word(j, shot::kWorldX + o), s.word(j, shot::kWorldY + o));
        s.word(j, shot::kScrX + o) = p.x;  // [BYTES seg8:0x7ac]
        s.word(j, shot::kScrY + o) = p.y;  // [BYTES seg8:0x7b3]
    }
}

// ---- seg11:0x4e0a

void fire(ShotCtx& c, int ship) {
    World& w = c.x.w;
    WaveGlobals& g = c.x.g;
    ShotState& s = c.s;
    const int ships = g.ship_pool;
    s.dg_594 = 7;  // [BYTES seg11:0x4e19]
    if (ship == g.dg_952e && g.dg_9e49 >= 3 && g.dg_9e49 <= 6 && g.dg_9130 == 0) return;  // [BYTES seg11:0x4e1f-0x4e3a]
    ShipRecord& S = ship_of(w, ship);
    if (S.w28 == 0) return;  // the gun's health [BYTES seg11:0x4e43]
    // The intervals, unsigned 32-bit (the stage-1 repair's +0x8c, then 5 frames) [BYTES
    // seg11:0x4e51-0x4e7f]
    const std::uint32_t last = ship_dword(S, 0xa0);
    const std::uint32_t frame = g.game_frames;  // DGROUP:0x1e68
    if (S.d8c.get() != 0 && S.d8c.get() + last > frame) return;
    if (last + 5u > frame) return;
    const ObjectRecord& O = obj(w, ships, ship);
    // Off screen, a goal outside pools 0, 9 and the ships [BYTES seg11:0x4e93-0x4eb4]
    if (O.on_screen == 0 && S.w32 != 0 && S.w32 != 9 && S.w32 != ships) return;
    // The aim test and the brake (s2-gun.md 1.1): not for the player in action mode [BYTES
    // seg11:0x4eb8-0x4fe2]
    if (!(ship == g.player && g.play_mode == 0)) {
        const ScreenPoint sp = screen_of(c, O);
        const ScreenPoint gp = screen_of(c, obj(w, S.w32, S.w34));
        // seg8:0x19b0(dy, dx) (Pascal: [bp+6] = the y difference) [BYTES seg11:0x4f8e-0x4f98]
        std::int16_t dir = direction16(sub16(gp.y, sp.y), sub16(gp.x, sp.x));
        const std::int16_t h = O.frame;  // [BYTES seg11:0x4faf]
        if (S.w32 == ships && ship_of(w, S.w34).be8 != 0) dir = h;  // [BYTES seg11:0x4fbf-0x4fd2]
        if (dir != h) return;  // [BYTES seg11:0x4fd5]
        c.h.thrust_step(5, ship);  // seg11:0x5ab6(5, s) [BYTES seg11:0x4fe2]
    }
    if (ship == g.player) w.w8e28 = g.now;  // DGROUP:0x8e28, World::w8e28 [BYTES seg11:0x4fed-0x4ff1]
    // The slot scan (s2-gun.md 1.1) [BYTES seg11:0x4ff5-0x5094]
    int ok = 1, player_owned = 0, owned = 0, free_count = 0;
    for (int j = 0; j < s.dg_594; ++j) {
        if (s.word(j, shot::kAge) != -1) {
            if (s.word(j, shot::kOwnerPool) == ships && s.word(j, shot::kOwnerRec) == ship) {
                owned = 1;  // [BYTES seg11:0x5020]
                const std::int16_t lim = q16(w.W, mul16(sub16(s.dg_594, 2), 10));  // 16-bit idiv [BYTES seg11:0x5025-0x5032]
                if (lim >= s.word(j, shot::kAge)) {  // signed `jl` [BYTES seg11:0x5034-0x5038]
                    ok = 0;
                    break;  // [BYTES seg11:0x503a]
                }
            }
        } else {
            ++free_count;  // [BYTES seg11:0x508b]
        }
        // Free slots too: the owner words outlive the shot [BYTES seg11:0x5056-0x506d]
        if (s.word(j, shot::kOwnerPool) == ships && s.word(j, shot::kOwnerRec) == g.player) player_owned = 1;
    }
    if (!(free_count >= 2 || player_owned != 0 || ship == g.player)) return;  // [BYTES seg11:0x503f-0x5053]
    if (ok == 0) return;  // [BYTES seg11:0x5090]
    // The bookkeeping, before a free slot is known [BYTES seg11:0x5098-0x50fa]
    {
        std::int16_t v = sub16(sub16(0, sub16(add16(w.w9000, w.w9000), 7)), w.score_level);
        if (v < 1) v = 1;
        set_byte_at(S, 0xcc, v);
        set_ship_dword(S, 0xa0, frame);
        set_ship_dword(S, 0x14a, static_cast<std::uint16_t>(O.x) | (static_cast<std::uint32_t>(static_cast<std::uint16_t>(O.y)) << 16));
        set_ship_dword(S, 0x14e, g.now);
        if (ship == g.player && g.play_mode != 0) c.h.player_sprite();  // seg7:0x1208
    }
    int j = 0;
    for (; j < s.dg_594; ++j)
        if (s.word(j, shot::kAge) == -1) break;  // [BYTES seg11:0x55f2-0x5604]
    if (j == s.dg_594) return;                   // no slot, no shot [BYTES seg11:0x55f8]
    s.word(j, shot::kAge) = 0;                   // [BYTES seg11:0x5606-0x5610]
    s.word(j, shot::kHits) = 0;
    s.word(j, shot::kW80) = 0;
    // The fill: the trail -100, seg8:0x764 with the slot's previous barrel count
    // (s2-gun.md 1.2) [BYTES seg11:0x5107-0x514c]
    for (int k = 0; k < s.dg_594; ++k) {
        for (int a = 0; a < S.w24; ++a) {
            s.word(j, shot::kWorldX + 12 * a + 2 * k) = -100;
            s.word(j, shot::kWorldY + 12 * a + 2 * k) = -100;
            trail_point(c, k, j);
        }
    }
    // [BYTES seg11:0x514e-0x51a4]
    s.word(j, shot::kWorldX + 12) = O.x;
    s.word(j, shot::kWorldX) = O.x;
    s.word(j, shot::kWorldY + 12) = O.y;
    s.word(j, shot::kWorldY) = O.y;
    s.word(j, shot::kOwnerPool) = static_cast<std::int16_t>(ships);
    s.word(j, shot::kOwnerRec) = static_cast<std::int16_t>(ship);
    s.word(j, shot::kBarrels) = S.w24;
    s.word(j, shot::kAlive) = sbyte_at(S, 0xe0);
    s.word(j, shot::kAlive + 2) = sbyte_at(S, 0xe1);
    // The velocity: 3·DGROUP:0x1050[4h] + ((v / 10)·10) / DGROUP:0x9006, 16-bit [BYTES
    // seg11:0x51a8-0x51ed]
    const int h = O.frame;
    const int ht = h & 0xf;  // the table's 16 entries [DESIGN: the index guard]
    s.word(j, shot::kVx) = add16(mul16(heading_dx(ht), 3), q16(mul16(idiv16(O.vx, 10), 10), g.dg_9006));
    s.word(j, shot::kVy) = add16(mul16(heading_dy(ht), 3), q16(mul16(idiv16(O.vy, 10), 10), g.dg_9006));
    s.set_dword(j, shot::kRicoAt, 0);  // [BYTES seg11:0x51f4]
    if (g.dg_9130 == 0) {              // [BYTES seg11:0x51f9-0x525b]
        // The player's start goes back by the ship's velocity, any other ship's forward
        const bool back = ship == g.player;
        for (int off : {shot::kWorldX, shot::kWorldX + 12})
            s.word(j, off) = back ? sub16(s.word(j, off), O.vx) : add16(s.word(j, off), O.vx);
        for (int off : {shot::kWorldY, shot::kWorldY + 12})
            s.word(j, off) = back ? sub16(s.word(j, off), O.vy) : add16(s.word(j, off), O.vy);
    }
    s.word(j, shot::kLife) = idiv16(w.W, 10);  // CX still 10 [BYTES seg11:0x525f-0x526a]
    if (ship == g.player && ship_of(w, g.player).w32 != 9) w.w9432 = add16(w.w9432, 1);  // [BYTES seg11:0x5274-0x5281]
    c.h.seg13_4eec(ship, 1);  // [BYTES seg11:0x528a]
    s.word(j, shot::kNoSkip) = 0;  // [BYTES seg11:0x5297]
    for (int p = w.roles.radar_first; p <= ships; ++p)  // [BYTES seg11:0x529d-0x52df]
        for (int r = 0; r < w.pools.in_use(p) && r < w.pools.records(p); ++r)
            obj_byte(obj(w, p, r), 0x55 + j) = 0;
    const int n = S.design_index;
    const auto tail = [&]() {
        // [BYTES seg11:0x53c2-0x53e5 seg11:0x55cb-0x55ed]
        if (g.dg_9130 != 0) {
            c.h.seg18_49d8(j, sub16(s.word(j, shot::kLife), s.word(j, shot::kAge)));
        } else if (g.wave_kind == 6 && g.dg_9eb5 == 0) {
            c.h.race_5626(j);  // the only ricochet writer (M6) [BYTES seg11:0x53e1]
        }
    };
    if (S.w24 == 1) {
        // One barrel: entry 64n + h, x 8, the wrap [BYTES seg11:0x52ef-0x538c]
        s.word(j, shot::kWorldX) = add16(s.word(j, shot::kWorldX), shl16(s.mx(n, h), 3));
        s.word(j, shot::kWorldY) = add16(s.word(j, shot::kWorldY), shl16(s.my(n, h), 3));
        s.word(j, shot::kWorldX) = wrap_mod(s.word(j, shot::kWorldX), kWrapX);
        s.word(j, shot::kWorldY) = wrap_mod(s.word(j, shot::kWorldY), kWrapY);
        if (ship == g.player) c.h.sound_744c(0x69);  // [BYTES seg11:0x5398]
        c.x.h.sound(3, 0x1e, j, 2);                  // [BYTES seg11:0x53a0-0x53a9]
        if (owned != 0) s.word(j, shot::kAge) = 15;  // [BYTES seg11:0x53b1-0x53bc]
        tail();
        return;
    }
    // Two barrels: both a = 0 and a = 1, entry 32 + 16a + h [BYTES seg11:0x53e8-0x54a4]
    for (int a = 0; a < 2; ++a) {
        const int ox = shot::kWorldX + 12 * a, oy = shot::kWorldY + 12 * a;
        s.word(j, ox) = add16(s.word(j, ox), shl16(s.mx(n, 32 + 16 * a + h), 3));
        s.word(j, oy) = add16(s.word(j, oy), shl16(s.my(n, 32 + 16 * a + h), 3));
        s.word(j, ox) = wrap_mod(s.word(j, ox), kWrapX);
        s.word(j, oy) = wrap_mod(s.word(j, oy), kWrapY);
    }
    // The convergence vector (s2-gun-verify.md 10) [BYTES seg11:0x54a8-0x5572]
    {
        const std::int32_t dx = sub16(s.word(j, shot::kWorldX + 12), s.word(j, shot::kWorldX));
        const std::int32_t dy = sub16(s.word(j, shot::kWorldY + 12), s.word(j, shot::kWorldY));
        s.word(j, shot::kConvX) = shl16(static_cast<std::int16_t>(dx), 4);
        s.word(j, shot::kConvY) = shl16(static_cast<std::int16_t>(dy), 4);
        const std::int32_t sq = static_cast<std::int32_t>(static_cast<std::uint32_t>(dy * dy) + static_cast<std::uint32_t>(dx * dx));
        const std::int32_t ab = static_cast<std::int32_t>(static_cast<std::uint32_t>(abs32(dy)) + static_cast<std::uint32_t>(abs32(dx)));
        std::int32_t L = lo16(newton_1290(ab, sq));  // AX, `cwd` [BYTES seg11:0x5529]
        if (L <= 1) L = 1;                           // [BYTES seg11:0x5530-0x553b]
        s.word(j, shot::kConvX) = lo16(div32(s.word(j, shot::kConvX), L));
        s.word(j, shot::kConvY) = lo16(div32(s.word(j, shot::kConvY), L));
        s.word(j, shot::kW6c) = 0x1c;  // dword 0x003e001c [BYTES seg11:0x5572]
        s.word(j, shot::kW6e) = 0x3e;
    }
    if (ship == g.player) c.h.sound_744c(0x69);  // [BYTES seg11:0x5583]
    // id 4 only with both barrel bytes set (s2-gun-verify.md 12, CORRECTED) [BYTES
    // seg11:0x5590-0x55b2]
    const int id = (sbyte_at(S, 0xe0) != 0 && sbyte_at(S, 0xe1) != 0) ? 4 : 3;
    c.x.h.sound(id, 0x1e, j, 2);
    if (owned != 0) s.word(j, shot::kAge) = 15;  // [BYTES seg11:0x55ba-0x55c5]
    tail();
}

// ---- seg8:0x3aec

std::int16_t shot_update(ShotCtx& c, int i, int j) {
    World& w = c.x.w;
    WaveGlobals& g = c.x.g;
    ShotState& s = c.s;
    const int ships = g.ship_pool;
    auto age = [&]() -> std::int16_t& { return s.word(j, shot::kAge); };
    const auto life = [&]() { return s.word(j, shot::kLife); };
    const auto barrels = [&]() { return s.word(j, shot::kBarrels); };
    const auto alive = [&](int a) { return s.word(j, shot::kAlive + 2 * a) != 0; };
    const auto scr_x = [&](int a, int k) -> std::int16_t& { return s.word(j, shot::kScrX + 12 * a + 2 * k); };
    const auto scr_y = [&](int a, int k) -> std::int16_t& { return s.word(j, shot::kScrY + 12 * a + 2 * k); };
    std::int16_t e = 0;
    if (age() >= 0 && life() > age()) {  // [BYTES seg8:0x3b00-0x3b0f]
        const std::int16_t T = add16(sar16(add16(g.detail, 1), 1), 3);  // [BYTES seg8:0x3b14-0x3b1d]
        if (g.dg_9130 != 0) {  // [BYTES seg8:0x3b20-0x3b36]
            e = add16(g.size, 1);
            c.h.seg18_4dc4(j);
        } else {
            e = sar16(shl16(2, g.size & 0x1f), 1);  // [BYTES seg8:0x3b38-0x3b43]
        }
        trail_point(c, 0, j);              // [BYTES seg8:0x3b4b]
        const std::int16_t r = shot_hit_test(c, j);  // [BYTES seg8:0x3b54]
        if (r != 0 && s.word(j, shot::kOwnerPool) == ships && s.word(j, shot::kOwnerRec) == g.player)
            s.word(j, shot::kHits) = add16(s.word(j, shot::kHits), 1);  // [BYTES seg8:0x3b5c-0x3b75]
        if (r == 2) {
            // The end: points T ... 0 of the live barrels blacked, 1 [BYTES seg8:0x3b7f-0x3be4]
            age() = -1;
            for (int k = T; k > -1; --k)
                for (int a = 0; a < barrels(); ++a)
                    if (alive(a)) c.h.patblt(scr_x(a, k), scr_y(a, k), e, e, kRopBlackness);
            return 1;
        }
        // The ricochet block (s2-gun-verify.md 23) [BYTES seg8:0x3bee-0x3c6a]
        if (r == 1 || s.dword(j, shot::kRicoAt) == s.dg_5cc) {
            if (s.dg_5cc != 0) {
                if (s.dword(j, shot::kRicoAt) == s.dg_5cc) {
                    c.x.h.sound(0x11, 0x1e, j, 2);
                    s.word(j, shot::kVx) = s.word(j, shot::kRicoVx);
                    s.word(j, shot::kVy) = s.word(j, shot::kRicoVy);
                    s.word(j, shot::kWorldX) = add16(s.word(j, shot::kWorldX), s.word(j, shot::kVx));
                    s.word(j, shot::kWorldY) = add16(s.word(j, shot::kWorldY), s.word(j, shot::kVy));
                }
                if (g.wave_kind == 6) c.h.race_5626(j);  // M6
            }
        }
        // The brush (s2-gun-verify.md 31) [BYTES seg8:0x3c75-0x3cb3]
        const bool ship_shot = s.word(j, shot::kOwnerPool) == ships;
        const std::int8_t row = ship_shot ? sbyte_at(ship_of(w, s.word(j, shot::kOwnerRec)), 0xe3) : std::int8_t{0};
        c.h.select_brush(shot_brush_index(age(), life(), ship_shot, row));
        // The heads, moved by the owner's velocity·i / -50, drawn inside the play area
        // (s2-gun-verify.md 25) [BYTES seg8:0x3cb8-0x3d8d]
        const ObjectRecord& own = obj(w, s.word(j, shot::kOwnerPool), s.word(j, shot::kOwnerRec));
        const std::int16_t ovx = own.vx, ovy = own.vy;
        for (int a = 0; a < barrels(); ++a) {
            if (!alive(a)) continue;
            scr_x(a, 0) = add16(scr_x(a, 0), idiv16(mul16(ovx, static_cast<std::int16_t>(i)), -50));
            scr_y(a, 0) = add16(scr_y(a, 0), idiv16(mul16(ovy, static_cast<std::int16_t>(i)), -50));
            const bool inside = scr_x(a, 0) > 0 && scr_x(a, 0) < w.W && scr_y(a, 0) > 0 && scr_y(a, 0) < w.H;
            if (inside || g.dg_9130 != 0) c.h.patblt(scr_x(a, 0), scr_y(a, 0), e, e, kRopPatCopy);
        }
        // The tail point T blacked while word +0x06 != -100 [BYTES seg8:0x3d90-0x3de8]
        if (s.word(j, 0x06) != -100)
            for (int a = 0; a < barrels(); ++a)
                if (alive(a)) c.h.patblt(scr_x(a, T), scr_y(a, T), e, e, kRopBlackness);
        age() = add16(age(), 1);  // [BYTES seg8:0x3def]
        // The shift, every barrel [BYTES seg8:0x3df3-0x3e3a]
        for (int k = T; k > 0; --k)
            for (int a = 0; a < barrels(); ++a) {
                scr_x(a, k) = scr_x(a, k - 1);
                scr_y(a, k) = scr_y(a, k - 1);
            }
        // The move and the wrap, live barrels [BYTES seg8:0x3e3c-0x3ee9]
        for (int a = 0; a < barrels(); ++a) {
            if (!alive(a)) continue;
            std::int16_t& wx = s.word(j, shot::kWorldX + 12 * a);
            wx = add16(wx, s.word(j, shot::kVx));
            if (g.dg_9130 == 0) {
                if (wx > kWrapX) wx = 0;
                if (wx < 0) wx = kWrapX;
            }
            std::int16_t& wy = s.word(j, shot::kWorldY + 12 * a);
            wy = add16(wy, s.word(j, shot::kVy));
            if (g.dg_9130 == 0) {
                if (wy > kWrapY) wy = 0;
                if (wy < 0) wy = kWrapY;
            }
        }
        // The convergence: (age & 0xf) = 0xf and exactly two barrels, no alive test, no wrap
        // (s2-gun-verify.md 28) [BYTES seg8:0x3eec-0x3f2c]
        if ((age() & 0xf) == 0xf && barrels() == 2) {
            const std::int16_t cx = sar16(s.word(j, shot::kConvX), 2), cy = sar16(s.word(j, shot::kConvY), 2);
            s.word(j, shot::kWorldX) = add16(s.word(j, shot::kWorldX), cx);
            s.word(j, shot::kWorldX + 12) = sub16(s.word(j, shot::kWorldX + 12), cx);
            s.word(j, shot::kWorldY) = add16(s.word(j, shot::kWorldY), cy);
            s.word(j, shot::kWorldY + 12) = sub16(s.word(j, shot::kWorldY + 12), cy);
        }
    }
    // The expiry: points 5 ... 0 of every barrel blacked [BYTES seg8:0x3f30-0x3f96]. A free slot
    // reaches here with e unset in the original ([bp-0xc]); its age -1 < lifetime skips the
    // erase unless the lifetime is <= -1 [OPEN].
    if (!(life() > age())) {
        age() = -1;
        for (int k = 5; k > -1; --k)
            for (int a = 0; a < barrels(); ++a) c.h.patblt(scr_x(a, k), scr_y(a, k), e, e, kRopBlackness);
    }
    return (age() == -1 && s.dg_9136 == 0) ? std::int16_t{0} : std::int16_t{1};  // [BYTES seg8:0x3f98-0x3fae]
}

// ---- seg7:0x938-0x970

void run_shots(ShotCtx& c) {
    ShotState& s = c.s;
    s.dg_9136 = 0;  // [BYTES seg7:0x938-0x93a]
    for (int i = 0; i < 5; ++i) {
        for (int j = 0; add16(s.dg_598, s.dg_594) > j; ++j) s.dg_9136 = shot_update(c, i, j);  // [BYTES seg7:0x942-0x95d]
        ++s.dg_5cc;  // [BYTES seg7:0x965]
    }
}

// ---- seg8:0x3688

std::int16_t shot_hit_test(ShotCtx& c, int j) {
    World& w = c.x.w;
    WaveGlobals& g = c.x.g;
    ShotState& s = c.s;
    const int nb = s.word(j, shot::kBarrels);
    // The screen heads [BYTES seg8:0x3697-0x36e6]
    for (int a = 0; a < nb; ++a) {
        const std::int16_t x = s.word(j, shot::kWorldX + 12 * a), y = s.word(j, shot::kWorldY + 12 * a);
        if (2 * a + 1 >= static_cast<int>(s.heads.size())) continue;  // [DESIGN: past DGROUP:0x516e]
        const ScreenPoint p = to_screen(c, x, y);
        s.heads[static_cast<std::size_t>(2 * a)] = p.x;
        s.heads[static_cast<std::size_t>(2 * a + 1)] = p.y;
    }
    // The word the head loop's last seg8:0x29ea left at T - 44: every seg8:0x6f3c below reads it
    // as [bp-0x1e]. With no barrel the loop calls nothing and the word stays [OPEN: the stack's
    // earlier contents] [BYTES seg8:0x369e-0x36e6].
    if (nb > 0) s.stack_1e = seam_word(c.x.view, s.word(j, shot::kWorldY + 12 * (nb - 1)), g.wave_kind, g.dg_9eb5, g.dg_9130);
    // The visibility list [BYTES seg8:0x36fd-0x3730]
    if (const VisibilityGlobals* v = c.x.vis) {
        for (int k = 0; k < v->count && k < kVisibleMax; ++k) {
            const int p = static_cast<std::int8_t>(v->pool[static_cast<std::size_t>(k)]);
            if (p == g.pool5) continue;
            const int rr = static_cast<std::int8_t>(v->rec[static_cast<std::size_t>(k)]);
            const std::int16_t r = shot_hit_object(c, p, rr, j);
            if (r == 2) return 2;
            if (r == 1) return 1;
        }
    }
    // The owner's goal off screen, for every pool-14 owner (the literal 0xe) [BYTES
    // seg8:0x3733-0x3789]
    if (s.word(j, shot::kOwnerPool) == 0xe) {
        const ShipRecord& S = ship_of(w, s.word(j, shot::kOwnerRec));
        const int gp = S.w32, gr = S.w34;
        if (obj(w, gp, gr).on_screen == 0) {
            const std::int16_t r = shot_hit_object(c, gp, gr, j);
            if (r == 2 || r == 1) return r;
        }
    }
    // The world heads and the gates' beam rectangles (not the posts) [BYTES seg8:0x378c-0x3827]
    for (int a = 0; a < nb; ++a) {
        if (2 * a + 1 >= static_cast<int>(s.heads.size())) continue;
        s.heads[static_cast<std::size_t>(2 * a)] = s.word(j, shot::kWorldX + 12 * a);
        s.heads[static_cast<std::size_t>(2 * a + 1)] = s.word(j, shot::kWorldY + 12 * a);
    }
    for (int gi = 0; gi < w.gates && gi < kMaxGates; ++gi) {
        const Rect16& b = w.gate[static_cast<std::size_t>(gi)].beam;
        const Rect16 R = rect16(b.l, sub16(b.t, 5), b.r, add16(b.b, 5));  // SetRect [BYTES seg8:0x37d7-0x37f3]
        if (pt_in_rect(R, s.heads[0], s.heads[1]) || pt_in_rect(R, s.heads[2], s.heads[3])) return 2;
    }
    // The seg18 mode's tail seg8:0x3833-0x3adf (M6) [OPEN]
    return 0;  // [BYTES seg8:0x382a-0x3831]
}

std::int16_t seam_word(const ViewState& v, std::int16_t y, std::int16_t wave_kind, std::uint8_t dg_9eb5,
                       std::int16_t dg_9130) {
    if (dg_9130 != 0) return 1;  // seg8:0x2b51: lo16(|DGROUP:0x88cc|) (claim 39) [OPEN M6]
    // shl ax, cl with cl = byte DGROUP:0x9150 - 1, then cwd [BYTES seg8:0x2abf-0x2acb]
    const std::int16_t half_h = shl16(v.play_h, (v.shift - 1) & 31);
    // sext32(y) - sext32(that) - dword DGROUP:0x2410, 32-bit [BYTES seg8:0x2a02-0x2a0b seg8:0x2acb-0x2ad9]
    const std::int32_t off = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(y)) -
                                                       static_cast<std::uint32_t>(static_cast<std::int32_t>(half_h)) -
                                                       static_cast<std::uint32_t>(v.y0l));
    // seg9:0x88dc(cx:bx) > sext32(lo16(DGROUP:0x5a2 << 2)), not in a kind-6 wave before
    // DGROUP:0x9eb5 = 1 [BYTES seg8:0x2ae9-0x2b0c]
    const bool wrap = abs32(off) > static_cast<std::int32_t>(shl16(kWorldCells, 2)) && !(wave_kind == 6 && dg_9eb5 != 1);
    if (wrap) return 0x2b4f;  // the return IP of the far call at seg8:0x2b4a [BYTES seg8:0x2b4a seg8:0x2b4f]
    return lo16(off);         // CX pushed at seg8:0x2ade [BYTES seg8:0x2ade]
}

// ---- seg8:0x6f3c

std::int16_t shot_power(ShotCtx& c, int j) {
    World& w = c.x.w;
    WaveGlobals& g = c.x.g;
    ShotState& s = c.s;
    const int ships = g.ship_pool;
    const int op = s.word(j, shot::kOwnerPool), orr = s.word(j, shot::kOwnerRec);
    const std::int16_t a = s.word(j, shot::kAge);
    std::int16_t p = sub16(14, q16(mul16(a, 14), s.word(j, shot::kLife)));  // [BYTES seg8:0x715d-0x7171]
    if (a < 20) p = 23;                                                     // [BYTES seg8:0x7174-0x717b]
    if (op == ships && orr == g.player) {
        p = half(mul16(p, 3));  // [BYTES seg8:0x7180-0x719b seg8:0x71c9-0x71ce]
    } else if (op == ships) {
        const ShipRecord& S = ship_of(w, orr);
        const DamageDesign D = c.x.h.design(w, S.design_index);
        if (D.team == 7 && (S.w5e & 0xff) == 0) p = half(mul16(p, 5));  // [BYTES seg8:0x719d-0x71ce]
    }
    if (op == ships)  // the design mass DGROUP:0x9704 [BYTES seg8:0x71d1-0x71f8]
        p = idiv16(mul16(c.h.design_mass(w, ships, orr), p), 250);
    if (g.demons_gate != 0 && orr != g.player) p = mul16(p, 3);  // the record only (claim 39) [BYTES seg8:0x71fb-0x7210]
    if (op == ships) {
        const ShipRecord& S = ship_of(w, orr);
        if (S.roster_index != -1)  // [BYTES seg8:0x7213-0x724b]
            p = idiv16(mul16(mul16(add16(c.h.roster_byte_32(S.roster_index), 5), p), 20), 100);
    }
    return p;
}

std::int16_t shot_hit_object(ShotCtx& c, int pool, int rec, int j) {
    World& w = c.x.w;
    WaveGlobals& g = c.x.g;
    CollideGlobals& cg = c.x.c;
    CollideHost& ch = c.x.h;
    ShotState& s = c.s;
    const int ships = g.ship_pool;
    {
        ObjectRecord& O = obj(w, pool, rec);
        if (O.state != 0) return 0;  // [BYTES seg8:0x6f59]
        // The near-miss skip byte (claim 36) [BYTES seg8:0x6f7d-0x6fa0]
        std::uint8_t& skip = obj_byte(O, 0x55 + j);
        if (static_cast<std::int8_t>(skip) > 1 && s.word(j, shot::kNoSkip) == 0 && g.dg_9130 == 0) {
            skip = static_cast<std::uint8_t>(skip - 1);
            return 0;
        }
        // [BYTES seg8:0x6fb0-0x6fc9]
        if (O.b2e != 0 && !(pool == ships && ship_of(w, rec).w2e == 2)) return 0;
    }
    const int op = s.word(j, shot::kOwnerPool), orr = s.word(j, shot::kOwnerRec);
    if (op == pool && orr == rec && s.word(j, shot::kAge) < 0x18) return 0;  // [BYTES seg8:0x6fde-0x6fef]
    const ScreenPoint t = to_screen(c, obj(w, pool, rec).x, obj(w, pool, rec).y);  // [BYTES seg8:0x700c]
    for (int a = 0; a < s.word(j, shot::kBarrels); ++a) {  // [BYTES seg8:0x7b30-0x7b51]
        if (s.word(j, shot::kAlive + 2 * a) == 0) continue;
        if (2 * a + 1 >= static_cast<int>(s.heads.size())) continue;  // [DESIGN: past DGROUP:0x516e]
        const std::int16_t dx = sub16(s.heads[static_cast<std::size_t>(2 * a)], t.x);      // [BYTES seg8:0x7b58]
        const std::int16_t dy = sub16(s.heads[static_cast<std::size_t>(2 * a + 1)], t.y);  // [BYTES seg8:0x7b62]
        ObjectRecord& O = obj(w, pool, rec);
        if (!(O.size > dx && dx > -1 && O.size > dy && dy > -1)) {  // [BYTES seg8:0x7b7a-0x7b8e]
            // The near miss (claim 38) [BYTES seg8:0x7026-0x714f]
            const int sz = g.size & 0x1f;
            const std::int16_t m = shl16(-25, sz), M = shl16(40, sz);
            std::uint8_t& skip = obj_byte(O, 0x55 + j);
            if (m <= dx && M >= dx && m <= dy && M >= dy) {
                skip = 0;  // [BYTES seg8:0x714a]
                continue;
            }
            const std::int16_t hs = half(O.size);
            std::int16_t sd = sub16(magnitude(add16(hs, dx), add16(hs, dy)), 5);  // seg8:0x35d0 [BYTES seg8:0x704b-0x707d]
            if (sd == 0) sd = 1;
            const std::int16_t v = add16(mul16(sub16(idiv16(O.vx, 5), s.word(j, shot::kVx)), dx),
                                         mul16(sub16(idiv16(O.vy, 5), s.word(j, shot::kVy)), dy));
            const std::int16_t q = q16(v, shl16(sd, 3));  // [BYTES seg8:0x70c3-0x70ca]
            if (q > 0) {
                const std::int32_t d2 = static_cast<std::int32_t>(add16(g.detail, 2));
                skip = static_cast<std::uint8_t>(div32(div32(sd, q), d2 == 0 ? 1 : d2) & 0xff);  // [BYTES seg8:0x70dd-0x70fc]
            } else {
                const std::int16_t dd = mul16(add16(g.detail, 4), 10);
                const std::uint8_t al = static_cast<std::uint8_t>(q16(sd, dd) & 0xff);
                skip = static_cast<std::uint8_t>(al - static_cast<std::uint8_t>(g.detail & 0xff) + 10);  // [BYTES seg8:0x7108-0x711c]
            }
            continue;
        }
        // HIT(a) (s2-gun.md 4.3)
        const std::int16_t p = shot_power(c, j);
        cg.a_pool = static_cast<std::int16_t>(pool);  // [BYTES seg8:0x724e-0x726d]
        cg.a_rec = static_cast<std::int16_t>(rec);
        c.x.dg_943a = 0x1e;
        cg.b_pool = 0x1e;
        c.x.dg_943c = static_cast<std::int16_t>(j);
        cg.b_rec = static_cast<std::int16_t>(j);
        if (op == ships && orr == g.player && pool != ships) ch.seg8_81f8(pool, rec);  // [BYTES seg8:0x7271-0x7290]
        std::int16_t bvx = mul16(s.word(j, shot::kVx), 5), bvy = mul16(s.word(j, shot::kVy), 5);  // [BYTES seg8:0x7296-0x72a8]
        std::int16_t& hx = s.word(j, shot::kWorldX + 12 * a);
        std::int16_t& hy = s.word(j, shot::kWorldY + 12 * a);
        if (s.stack_1e != 0) {  // the seam fix (claim 43), seam_word's value [BYTES seg8:0x72ab-0x7393]
            if (hx < 1000 && O.x > 5000)
                hx = add16(hx, kWrapX);
            else if (hx > 5000 && O.x < 1000)
                hx = sub16(hx, kWrapX);
            if (hy < 1000 && O.y > 5000)
                hy = add16(hy, kWrapY);
            else if (hy > 5000 && O.y < 1000)
                hy = sub16(hy, kWrapY);
        }
        const std::int16_t tvx0 = O.vx, tvy0 = O.vy;
        // ISPACE.DLL ordinal 29 through seg10:0x7dd4: the shot is party B (5·v, its head,
        // extent 30, mass p), the target party A (extent + 8, mass +0x44) (claim 41)
        // [BYTES seg8:0x73b3-0x7422]
        const int res = bounce(&cg.dA, &cg.dB, O.mass, p, add16(O.extent, 8), 30, &O.y, &O.x, &hy, &hx,
                               &O.vy, &O.vx, &bvy, &bvx);
        if (s.stack_1e != 0) {  // [BYTES seg8:0x742a-0x747e]
            hx = wrap_mod(hx, kWrapX);
            hy = wrap_mod(hy, kWrapY);
        }
        if (res == 0) continue;  // [BYTES seg8:0x7480-0x7486]
        if (O.b54 != 0) {  // `fixed` [BYTES seg8:0x7499-0x74b0]
            O.vy = 0;
            O.vx = O.vy;
        } else {  // [BYTES seg8:0x74b6-0x74e8]
            O.vx = half(add16(O.vx, tvx0));
            O.vy = half(add16(O.vy, tvy0));
        }
        s.word(j, shot::kVx) = idiv16(bvx, 5);  // [BYTES seg8:0x74ec-0x7504]
        s.word(j, shot::kVy) = idiv16(bvy, 5);
        // The sound (claim 47) [BYTES seg8:0x74ef-0x7546]
        bool fuel = pool == 4;
        if (pool == 8 && rec >= 0 && rec < kPool8Max) {
            const Pool8Entry& e8 = w.pool8[static_cast<std::size_t>(rec)];
            const int k = e8.at(0x30);
            const int cls = k >= 0 && k < static_cast<int>(w.classes.size()) ? w.classes[static_cast<std::size_t>(k)][3] : 0;
            if (e8.at(0x1c) == 2 && (cls == 5 || cls == 4)) fuel = true;
        }
        if (fuel)
            ch.sound(0x1e, pool, rec, 1);
        else
            ch.sound(0x11, 0x1e, j, 2);
        // 1. A computer owner's goal (s2-gun.md 4.7) [BYTES seg8:0x7559-0x75d7]
        if (op == ships) {
            ShipRecord& S = ship_of(w, orr);
            if (S.w38 == 1 && S.w32 == pool && S.w34 == rec) set_ship_dword(S, 0xa4, g.now);
            for (int i = 0; i <= w.last_ship && i < kShipCount; ++i) {
                if (i == orr) continue;
                const ShipRecord& I = ship_of(w, i);
                if (I.bec != 0 && I.w32 == pool && I.w34 == rec && I.w38 == 1) ch.seg13_2714(i, orr, 5, 10, 0, -1);
            }
        }
        // 2. [BYTES seg8:0x75d9-0x75f0]
        if (op == ships && pool == ships) ch.seg4_7d36(orr, rec, cg.dA);
        // 3. The player's target and DGROUP:0x9432 (claim 48) [BYTES seg8:0x7603-0x768b]
        if (op == ships && orr == g.player && !(pool == ships && rec == g.player)) {
            cg.hit_rec = static_cast<std::int16_t>(rec);
            cg.hit_time = g.now;
            cg.hit_pool = static_cast<std::int16_t>(pool);
            if (pool == ships || pool == g.rock_pool || pool == 8 || (pool == 0 && O.b53 == 1))
                w.w9432 = add16(w.w9432, idiv16(cg.dA, 3));
            if (pool == ships && ship_of(w, rec).w38 == 4 && ship_of(w, rec).w34 == g.player)
                w.w9432 = add16(w.w9432, half(cg.dA));
        }
        // 4. on_damage, attacker pool 30 [BYTES seg8:0x768f-0x76ae]
        const std::int16_t res_od = call_on_damage(c.x, pool, rec, cg.dA);
        // 5. The spark (claim 49) [BYTES seg8:0x76b1-0x76d3]
        if (res_od == 0 && g.detail >= 2 && mul16(p, cg.dB) > 600) spark(c, j, mul16(p, cg.dB), a);
        // 6. [BYTES seg8:0x76d6-0x76e8]
        seg8_99ce(c.x, pool, rec, op, orr, add16(res_od, 2));
        // 7. [BYTES seg8:0x76f0-0x770d]
        if (pool == 15) c.h.seg18_3f02(s.word(j, shot::kWorldX + 12 * a), s.word(j, shot::kWorldY + 12 * a), cg.dA);
        // 8. The goals and the escort messages [BYTES seg8:0x7715-0x792d]
        int flag = 0;
        if (res_od != 0 && op == ships) {
            ShipRecord& S = ship_of(w, orr);
            if (pool == 0 && obj(w, 0, rec).b53 == 1 && S.w32 == 0 && S.w34 == rec && S.w38 == 1) ch.seg13_b6f2(orr, 1);
            if (pool == 8 && rec >= 0 && rec < kPool8Max && w.pool8[static_cast<std::size_t>(rec)].at(0x0a) == 0x10 &&
                S.w32 == 8 && S.w34 == rec && S.w38 == 1)
                ch.seg13_b6f2(orr, 6);
            bool second = true;  // seg8:0x785a
            if (S.w52 >= 2 && S.w52 <= 4) {
                if (S.w58 != orr && S.w54 == pool) {
                    second = false;
                    bool said = false;
                    if (pool == 9) {
                        ch.message(orr, ships, S.w58, 0x75);
                        ch.seg13_2714(S.w58, orr, 15, 0x2d, 0, 6);
                        said = true;
                    } else if (S.w56 == rec) {
                        ch.message(orr, ships, S.w58, 0x76);
                        ch.seg13_2714(S.w58, orr, 0x28, 0x5a, 0, 6);
                        said = true;
                    }
                    if (said) {  // [BYTES seg8:0x77ff-0x7820]
                        if (g.play_mode == 0 || !(orr == g.player || g.demo != 0)) S.w52 = 0;
                        flag = 1;
                    }
                }
            }
            if (second) {
                if ((S.w52 == 1 || S.w52 == 5) && pool == ships) {  // [BYTES seg8:0x785a-0x78c5]
                    const ShipRecord& T = ship_of(w, rec);
                    if (T.w32 == ships && S.w58 == T.w34 && T.w38 == 1) {
                        ch.message(orr, ships, S.w58, 0x77);
                        ch.seg13_2714(S.w58, orr, 0x3c, 0x78, 0, 8);
                        flag = 1;
                    }
                } else {  // [BYTES seg8:0x78c8-0x792d]
                    for (int i = 0; i <= w.last_ship && i < kShipCount; ++i) {
                        if (i == orr) continue;
                        const ShipRecord& I = ship_of(w, i);
                        if (I.bec != 0 && I.w32 == pool && I.w34 == rec && I.w38 == 1) {
                            ch.seg13_2714(i, orr, 0x19, 0x28, 0, 6);
                            ch.message(orr, ships, i, 0x77);
                        }
                    }
                }
            }
        }
        // 9. A nasty icon: message 0x5e and 3000 on the score, not the rating (claim 51)
        // [BYTES seg8:0x792f-0x7977]
        if (res_od != 0 && pool == 0 && op == ships && orr == g.player && obj(w, 0, rec).b53 == 2) {
            ch.message(orr, 7, 0, 0x5e);
            w.score = add32u(w.score, 3000);
        }
        // 10. The race marker (M6) [BYTES seg8:0x797c-0x79e2]
        if (res_od != 0 && pool == 8 && g.wave_kind == 6) {
            const std::size_t k = static_cast<std::size_t>(w.course) % w.race_9eb9.size();
            if (add16(w.race_9eb9[k], add16(w.gates, w.gates)) <= rec) ch.race_marker(rec);
        }
        // 11. [BYTES seg8:0x79e7-0x7a19]
        if (res_od != 0 && pool == ships) {
            ch.seg4_7636(rec, op, orr, flag);
            if (op == ships && orr == g.player) w.w9432 = add16(w.w9432, 200);
        }
        // 12. (claim 52) [BYTES seg8:0x7a1f-0x7a3c]
        if (res_od != 0) ship_of(w, g.player).due156.set(g.now - 1u);
        // 13. The kill scorer, destroyed or not (claim 53) [BYTES seg8:0x7a40-0x7a5e]
        if (op == ships && orr == g.player) kill_score(c.x, pool, rec, cg.dA, res_od);
        // 14. A virus: message 0x5b (its %R draw is the message display's) [BYTES seg8:0x7a64-0x7a87]
        if (res_od != 0 && pool == 9 && op == ships && flag == 0) ch.message(orr, 7, 0, 0x5b);
        // 15. The tumble and the lock-out; pool 8's lock-out (claim 54) [BYTES seg8:0x7a8f-0x7b1c]
        if (pool == ships && ship_of(w, rec).bec != 0) {
            ShipRecord& T = ship_of(w, rec);
            if (g.demons_gate != 0 && rec != g.player) cg.dA = idiv16(cg.dA, 2);
            T.be2 = 1;
            ObjectRecord& TO = obj(w, ships, rec);
            if (TO.angle == 0) TO.angle = 1;
            T.lockout.set(add32u(g.now, mul16(cg.dA, 12)));
        } else if (pool == 8 && rec >= 0 && rec < kPool8Max) {
            Pool8Entry& e8 = w.pool8[static_cast<std::size_t>(rec)];
            const std::uint32_t t = add32u(g.now, mul16(cg.dA, 120));
            e8.at(0x04) = to_i16(t & 0xffffu);
            e8.at(0x06) = to_i16(t >> 16);
        }
        return res_od != 0 ? 2 : 1;  // 16. [BYTES seg8:0x7b20-0x7b2e]
    }
    return 0;  // [BYTES seg8:0x7b41]
}

// ---- seg8:0x804a

void spark(ShotCtx& c, int j, std::int16_t v, int a) {
    World& w = c.x.w;
    EffectTable& t = w.effects;
    EffectSides& sd = c.fx.sides;
    int e = 0;
    for (; e <= 3; ++e) {  // [BYTES seg8:0x81c1-0x81d7]
        const EffectEntry& x = t[static_cast<std::size_t>(e)];
        if (x.frame == kEffectFree || x.type < 6) break;
    }
    if (e > 3) return;
    EffectEntry& E = t[static_cast<std::size_t>(e)];
    if (E.frame < 0x8000) {
        // A busy entry moves to the first free one of 4 ... 44 (not its +0x04)
        // [BYTES seg8:0x8058-0x8105]
        for (int k = 4; k < kEffectCount; ++k) {
            EffectEntry& F = t[static_cast<std::size_t>(k)];
            if (F.frame != kEffectFree) continue;
            F.frame = E.frame;
            F.end = E.end;
            F.pool = E.pool;
            F.rec = E.rec;
            F.start = E.start;
            F.type = E.type;
            sd.centre[static_cast<std::size_t>(k)] = sd.centre[static_cast<std::size_t>(e)];
            sd.point[static_cast<std::size_t>(k)] = sd.point[static_cast<std::size_t>(e)];
            if (w.w9434 < k) w.w9434 = static_cast<std::int16_t>(k);
            F.side = E.side;
            F.brush = E.brush;
            break;
        }
    }
    // [BYTES seg8:0x8105-0x81b8]
    E.frame = 1;
    std::int16_t n = add16(idiv16(v, 0x276), 2);
    if (n > 0xe) n = 0xe;
    E.end = n;
    E.type = 7;
    E.side = 2;
    if (w.w9434 < e) w.w9434 = static_cast<std::int16_t>(e);
    const ShotState& s = c.s;
    const bool row = s.word(j, shot::kOwnerPool) == c.x.g.ship_pool &&
                     sbyte_at(ship_of(w, s.word(j, shot::kOwnerRec)), 0xe3) != 0;
    E.brush = row ? 10 : 15;
    w.w9430 = add16(w.w9430, 1);
    E.step = 2;
    E.pool = 0x1e;
    E.rec = static_cast<std::int16_t>(j);
    sd.point[static_cast<std::size_t>(e)] = {s.word(j, shot::kWorldX + 12 * a), s.word(j, shot::kWorldY + 12 * a)};
    sd.centre[static_cast<std::size_t>(e)] = {s.word(j, shot::kScrX + 12 * a), s.word(j, shot::kScrY + 12 * a)};
}

}  // namespace is::world
