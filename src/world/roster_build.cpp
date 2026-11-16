// seg14:0x0 and its pickers (roster_build.h). Every site cites the listing it ports; the
// readings are work/analysis/m5/s3-roster.md's with s3-roster-verify.md's corrections, and the
// routines part A left open are read again in work/world/m5s3/roster-report.md. Portable.
#include "world/roster_build.h"

#include <algorithm>
#include <string>

#include "formats/ships.h"
#include "shell/rng.h"
#include "world/collide.h"
#include "world/design_words.h"
#include "world/flight.h"
#include "world/rating.h"
#include "world/roster.h"

namespace is::world {

namespace {

std::int8_t s8(std::uint8_t b) { return static_cast<std::int8_t>(b); }
std::uint8_t lo8(int v) { return static_cast<std::uint8_t>(static_cast<unsigned>(v) & 0xffu); }
std::int16_t max16(std::int16_t a, std::int16_t b) { return a > b ? a : b; }
std::int16_t min16(std::int16_t a, std::int16_t b) { return a < b ? a : b; }

// 16-bit `idiv` with the port's zero guard (a zero divisor traps in the original)
// [DESIGN ADR 0016].
std::int16_t q16(std::int16_t n, std::int16_t d, bool& once, const char* addr) {
    if (d == 0) {
        guard_todo(once, "a zero 16-bit divisor gives 0", addr);
        return 0;
    }
    if (n == -32768 && d == -1) return n;
    return idiv16(n, d);
}
// seg5:0x110, the 32-bit signed long division, with the same guard [BYTES seg5:0x110-0x1a8].
std::int32_t q32(std::int32_t n, std::int32_t d, bool& once, const char* addr) {
    if (d == 0) {
        guard_todo(once, "a zero 32-bit divisor gives 0", addr);
        return 0;
    }
    if (n == INT32_MIN && d == -1) return n;
    return n / d;
}
// The low 32 bits of a 32 x 32 `imul` (edx dropped).
std::int32_t mul32(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}
std::int32_t add32(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
// rand() % m: `call seg5:0xdc; cdq; idiv cx`, the draw always made [BYTES seg5:0xdc].
std::int16_t rmod(shell::Rng& rng, std::int16_t m, bool& once, const char* addr) {
    const std::int16_t r = static_cast<std::int16_t>(rng.rand());
    if (m == 0) {
        guard_todo(once, "rand() % 0 gives 0", addr);
        return 0;
    }
    return imod16(r, m);
}

const fmt::ShipRecord& empty_shp() {
    static const fmt::ShipRecord e{};
    return e;
}
const fmt::BodyRecord& empty_body() {
    static const fmt::BodyRecord e{};
    return e;
}
int shp_count(RosterHost& h) { return static_cast<int>(h.ships().records.size()); }
// SHP record v, or an all-zero record outside the table (the original reads past it)
// [DESIGN ADR 0016].
const fmt::ShipRecord& shp_rec(RosterHost& h, int v) {
    const auto& r = h.ships().records;
    if (v >= 0 && v < static_cast<int>(r.size())) return r[static_cast<std::size_t>(v)];
    static bool once = false;
    guard_todo(once, "an SHP index outside MASTER.SHP reads a zero record", "seg14:0x2300");
    return empty_shp();
}
const fmt::BodyRecord& bod_rec(const fmt::MasterBod& bodies, int b) {
    if (b >= 0 && b < static_cast<int>(bodies.records.size())) return bodies.records[static_cast<std::size_t>(b)];
    static bool once = false;
    guard_todo(once, "a body index outside MASTER.BOD reads a zero record", "seg14:0x40fa");
    return empty_body();
}
// Roster record r; outside the block a zero sink the original's far pointer would reach
// (record -1 is the 0x7c bytes before the block) [DESIGN ADR 0016].
RosterRecord& rec(World& w, int r, const char* addr) {
    if (RosterRecord* p = roster_at(w, r)) return *p;
    static RosterRecord sink;
    static bool once = false;
    guard_todo(once, "a roster index outside the block reads a zero record", addr);
    sink = RosterRecord{};
    return sink;
}
bool ship_ok(int i) { return i >= 0 && i < kShipCount; }
std::int16_t ship_roster(const World& w, int j) {
    return ship_ok(j) ? w.ships[static_cast<std::size_t>(j)].roster_index : std::int16_t{-1};
}
// The word DGROUP:0x3824 + 2·k of the 8 x 8 team table; k outside it reads 0 [DESIGN ADR 0016].
std::int16_t team_word(int k) {
    if (k >= 0 && k < 64) return roster_words().team_table[static_cast<std::size_t>(k)];
    static bool once = false;
    guard_todo(once, "a team-table index outside 8 x 8 reads 0", "DGROUP:0x3824");
    return 0;
}
// The design team word of the player's design (DGROUP:0x970a + 0xad·(ship DGROUP:0x9646 +0x4e)).
std::int16_t player_team(const World& w) {
    return design_team(w, ship_ok(w.player) ? w.ships[static_cast<std::size_t>(w.player)].design_index : 0);
}
// The avenger list entry DGROUP:0x4f1c + 2a (World holds 8) [DESIGN ADR 0016 beyond them].
std::int16_t* avenger(World& w, int a) {
    if (a >= 0 && a < static_cast<int>(w.w4f1c.size())) return &w.w4f1c[static_cast<std::size_t>(a)];
    static std::int16_t sink = 0;
    static bool once = false;
    guard_todo(once, "an avenger entry past DGROUP:0x4f1c's 8 words reads 0", "DGROUP:0x4f1c");
    sink = 0;
    return &sink;
}
// The directory record d's list byte +0x1c0 + k.
std::uint8_t& dir_byte(DirRecord& d, int k) { return d.b[0x1c0 + static_cast<std::size_t>(k)]; }
DesignRating* design_at(World& w, int n) {
    if (n >= 0 && n < kShipCount) return &w.design_ratings[static_cast<std::size_t>(n)];
    static bool once = false;
    guard_todo(once, "a design index outside 0 ... 7 is not held", "DGROUP:0x96d2");
    return nullptr;
}

// The word seg14:0x2300 leaves in its frame's [bp-0x12] (its last score store). seg14:0x0 calls
// seg14:0x3274 right after it at the same stack depth, so that word is what seg14:0x3274's
// unwritten [bp-0x12] holds for record 0 (s3-roster-verify.md correction 33) [BYTES seg14:0x6c0
// seg14:0x6d0 seg14:0x750 seg14:0x766 seg14:0x32c0 seg14:0x32e7] [DESIGN ADR 0016].
std::int16_t g_score_slot = 0;

}  // namespace

RosterWords& roster_words() {
    static RosterWords w;
    return w;
}

// ---------------------------------------------------------------- the design words

std::int16_t design_team(const World& w, int n) {
    return n >= 0 && n < kShipCount ? w.design_ratings[static_cast<std::size_t>(n)].team : std::int16_t{0};
}
void set_design_team(World& w, int n, std::int16_t team) {
    if (DesignRating* d = design_at(w, n)) d->team = team;
}
void game_over_designs(World& w) {
    // seg9:0xdb4-0xdca: n = 0 ... 9, words +0x4e and +0x38 = 0 [BYTES seg9:0xdb4-0xdca]; the port
    // holds 0 ... 7 [DESIGN].
    for (DesignRating& d : w.design_ratings) {
        d.w4e = 0;
        d.team = 0;
    }
}

void design_fill_portable(World& w, int i, const fmt::ShipRecord& shp, const fmt::MasterBod& bodies,
                          std::uint32_t now, RosterHost* h) {
    if (!ship_ok(i)) return;
    ShipRecord& S = w.ship(i);
    const int n = S.design_index;
    const fmt::BodyRecord& body = bod_rec(bodies, shp.body);
    const FlightDesign fd = flight_design(shp.thruster, shp.armor, body.strength_base, body.byte_1c, body.byte_1d,
                                          body.flags, shp.flags);
    // ---- the design words [BYTES seg14:0x1b93-0x20a3]
    if (DesignRating* d = design_at(w, n)) {
        const DesignRating old = *d;
        *d = design_rating(shp, body);
        d->team = old.team;  // seg14:0x14ca wrote it before seg14:0x1b84
        d->w3a = old.w3a;
        d->w4e = 1;          // DGROUP:0x9720 = 1 [BYTES seg14:0x1b98 seg14:0x20a3]
        d->mass = fd.mass;   // +0x32 [BYTES seg14:0x1e59 seg14:0x1e8f]
        d->blast = idiv16(fd.mass, 22);  // +0x30 [BYTES seg14:0x1e93-0x1e99]
        if ((body.flags & fmt::kBodyPullOffWings) == 0) d->b_a3 = old.b_a3;  // [BYTES seg14:0x1f1f-0x1f24]
    }
    // A body without Pull-off Wings: ship bytes +0xad = +0xac = 0 (seg14:0x1430's roster fill
    // writes both again) [BYTES seg14:0x1de4-0x1dff].
    if ((body.flags & fmt::kBodyPullOffWings) == 0) {
        set_byte_at(S, 0xad, 0);
        set_byte_at(S, 0xac, 0);
    }
    // seg10:0x754a(SHP +0x1f, 0, 0, 5) for a positive hit sound, seg10:0x754a(5, 0, 0, 5) for a
    // Shield body: the mode-5 preloads (sound.md) [BYTES seg14:0x1e22-0x1e2d seg14:0x1f9f-0x1fb9].
    if (h != nullptr) {
        if (to_i16(shp.hit_sound) > 0) h->preload_sound(to_i16(shp.hit_sound));
        if ((body.flags & fmt::kBodyShield) != 0) h->preload_sound(5);
    }
    // The body bytes and D (design +0x3c) [BYTES seg14:0x2055-0x208b], design +0x04 =
    // -sext(BOD +0x19) [BYTES seg14:0x1c7f-0x1c88].
    set_design_body(n, body.byte_1a, static_cast<std::int8_t>(body.byte_1e), static_cast<std::int8_t>(body.byte_1f));
    DesignWordSet dw = design_words(n);
    dw.w3c = design_band(body.byte_1a, w.size);
    dw.w04 = sub16(0, body.byte_19);
    set_design_words(n, dw);
    // ---- the object of pool DGROUP:0x9324 [BYTES seg14:0x20a9-0x2194] and the ship's flight
    // words [BYTES seg14:0x21a0-0x22f2]
    const DesignRating* d = design_at(w, n);
    const RosterRecord* r = roster_at(w, S.roster_index);
    if (i < w.pools.records(w.roles.ships)) {
        ObjectRecord& o = w.ship_object(i);
        // +0x44 = design +0x32 (in apply_flight_design), the turn from roster +0x33 for a ship with
        // a record [BYTES seg14:0x20bc seg14:0x2274-0x22a7].
        apply_flight_design(S, o, fd, i == w.player && w.player_game != 0, now,
                            r ? std::int16_t{r->b33[0]} : std::int16_t{0});
        o.hit_sound = to_i16(shp.hit_sound);              // +0x42 = design +0x2e [BYTES seg14:0x20c0-0x20d3]
        o.radar_brush = d ? d->radar_brush : std::int16_t{7};  // +0x46 = design +0x2c [BYTES seg14:0x20d7-0x20e6]
        o.max_damage = 200;                               // +0x2c [BYTES seg14:0x20f5]
        o.blast_size = d ? d->blast : idiv16(fd.mass, 22);  // +0x48 = design +0x30 [BYTES seg14:0x20fb-0x210a]
        o.damage = 0;                                     // +0x1e [BYTES seg14:0x2119]
        o.on_destroy.set(kDestroyShip);                   // +0x66 seg16:0x8e4 [BYTES seg14:0x212a-0x2130]
        o.radar_draw.set(kRadarShip);                     // +0x62 seg11:0x37a0 [BYTES seg14:0x2141-0x2147]
        o.on_damage.set(kDamageShip);                     // +0x5e seg8:0x45e0 [BYTES seg14:0x2158-0x215e]
        o.b2e = 0;                                        // [BYTES seg14:0x216f]
        o.alt_dc = 0;                                     // +0x3c [BYTES seg14:0x217f]
        o.b54 = 0;                                        // [BYTES seg14:0x2194]
    } else {
        static bool once = false;
        guard_todo(once, "seg14:0x1b84 for a ship the pool does not hold: no object words", "seg14:0x20ad");
    }
    // The ship bytes apply_flight_design leaves [BYTES seg14:0x21e2-0x2253].
    if (d != nullptr) {
        S.anim_wrap = d->wrap;                        // +0xe6 = design +0xa2 [BYTES seg14:0x21e2-0x21e6]
        S.be5 = d->b_a3 != 0 ? 1 : 0;                 // +0xe5 [BYTES seg14:0x223c-0x2253]
    }
    if (w.wave == 1 || i != w.player) S.w0a = 1;      // [BYTES seg14:0x21fa-0x220e]
    S.bed = 0;                                        // +0xed [BYTES seg14:0x222c]
    set_word_at(S, 0xa2, 0);                          // [BYTES seg14:0x2230]
    set_word_at(S, 0xa0, 0);                          // [BYTES seg14:0x2234]
    set_byte_at(S, 0x5f, 0);                          // [BYTES seg14:0x2238]
}

void ship_tail_sizes(World& w, int i, std::int16_t bm_height) {
    if (!ship_ok(i) || i >= w.pools.records(w.roles.ships)) return;
    ShipRecord& S = w.ship(i);
    ObjectRecord& o = w.ship_object(i);
    const int n = S.design_index;
    std::int16_t side, gap;
    if (S.roster_index == -1) {  // [BYTES seg14:0xd6b-0xd75 seg14:0xb86-0xbad]
        side = o.size;
        gap = o.gap;
    } else {  // sext(roster +0x7a), sext(+0x7b) [BYTES seg14:0xd79-0xd9d]
        const RosterRecord& r = rec(w, S.roster_index, "seg14:0xd79");
        side = s8(r.size);
        gap = s8(r.gap);
    }
    DesignWordSet dw = design_words(n);
    dw.w2a = side;  // design +0x2a [BYTES seg14:0xba9 seg14:0xd94]
    if (w.size == 0 && (side & 1) != 0) {  // [BYTES seg14:0xbb5-0xbc5]
        side = add16(side, 1);
        gap = 4;
    }
    const int sz = w.size & 0x1f;
    o.size = sar16(shl16(side, sz & 15), 1);  // [BYTES seg14:0xbcb-0xbe5]
    o.gap = sar16(shl16(gap, sz & 15), 1);    // [BYTES seg14:0xbf8-0xc04]
    static bool once = false;
    const std::int16_t frames = q16(sub16(bm_height, o.gap), add16(o.size, o.gap), once, "seg14:0xc25");
    o.extent = shl16(side, 3);                                  // [BYTES seg14:0xc2a-0xc30]
    S.anim_last = lo8(sub16(frames, 1));                         // +0x5b [BYTES seg14:0xc34-0xc3e]
    const std::int16_t h = idiv16(side, 2);                      // `cwd; sub ax, dx; sar ax, 1` [BYTES seg14:0xc42-0xc48]
    dw.w00 = h;                                                  // [BYTES seg14:0xc50]
    dw.w02 = h;                                                  // [BYTES seg14:0xc54]
    if (dw.w04 < 0) dw.w04 = sub16(sub16(4, dw.w04), h);         // [BYTES seg14:0xc58-0xc68]
    set_design_words(n, dw);
}

// ---------------------------------------------------------------- the pickers

std::int16_t design_class(const fmt::MasterShp& ships, const fmt::MasterBod& bodies, int v) {
    static const fmt::ShipRecord zero{};
    const fmt::ShipRecord& s = v >= 0 && v < static_cast<int>(ships.records.size())
                                   ? ships.records[static_cast<std::size_t>(v)]
                                   : zero;
    const fmt::BodyRecord& b = bod_rec(bodies, s.body);
    static bool once = false;
    const std::int16_t b1b = b.strength_base;  // sext(BOD +0x1b) [BYTES seg14:0x410b-0x4110]
    // a = lo16(b1b · sext(SHP +2)) / 36 [BYTES seg14:0x40f3-0x411b]
    std::int16_t a = q16(mul16(b1b, s8(s.armor)), 36, once, "seg14:0x4119");
    // t = max(SHP +1, 22) as signed bytes [BYTES seg14:0x4124-0x4132]
    const std::int16_t t = s8(s.thruster) < 22 ? std::int16_t{22} : std::int16_t{s8(s.thruster)};
    const std::int16_t q = q16(static_cast<std::int16_t>(22 * b.byte_1d), t, once, "seg14:0x4145");  // `imul byte` [BYTES seg14:0x4134-0x4145]
    const std::int16_t num = sub16(static_cast<std::int16_t>(10 * s8(s.thruster)), b.byte_1c);    // [BYTES seg14:0x4149-0x4155]
    std::int16_t x = sub16(q16(num, b1b, once, "seg14:0x415a"), q);                                   // [BYTES seg14:0x4157-0x415d]
    x = add16(x, x);                                                                                 // [BYTES seg14:0x415f]
    const std::int16_t wv = weapon_strength(s8(s.weapon));                                           // DGROUP:0x1332 [BYTES seg14:0x4164-0x4173]
    if ((s.flags & 0x04) == 0) a = idiv16(mul16(a, 3), 4);                 // [BYTES seg14:0x4176-0x4189]
    if ((s.flags & 0x08) != 0) a = idiv16(mul16(a, 6), 5);                 // [BYTES seg14:0x4194-0x41a5]
    if ((b.flags & 0x02) != 0) a = idiv16(mul16(a, 6), 5);                 // [BYTES seg14:0x41b5-0x41c6]
    if ((s.flags & 0x10) == 0) x = idiv16(mul16(x, 5), 6);                 // [BYTES seg14:0x41d1-0x41e2]
    const std::int16_t sum = add16(add16(wv, x), a);                       // [BYTES seg14:0x41e5-0x41eb]
    return roster_class(sum, sum);                                          // [BYTES seg14:0x41ee-0x420f]
}

int demo_ship(const World& w, const fmt::MasterShp& ships, const fmt::MasterBod& bodies, shell::Rng& rng, int flag,
              std::string& buf) {
    const int count = static_cast<int>(ships.records.size());
    static bool once = false;
    std::int16_t c = add16(rmod(rng, 20, once, "seg14:0x31ec"), 10);  // [BYTES seg14:0x31e3-0x31f1]
    int v = 0;
    for (;;) {
        v = rmod(rng, static_cast<std::int16_t>(count), once, "seg14:0x31fa");  // [BYTES seg14:0x31f4-0x31fe]
        const bool in = v >= 0 && v < count;
        const fmt::ShipRecord& r = in ? ships.records[static_cast<std::size_t>(v)] : empty_shp();
        bool ok = true;
        if (r.name.empty()) ok = false;                                // [BYTES seg14:0x320d-0x3214]
        if (!ship_accepted(w, ships, bodies, v)) ok = false;           // [BYTES seg14:0x3219-0x3223]
        if (r.team == 7 && flag == 0) ok = false;                      // [BYTES seg14:0x322e-0x323b]
        if (c > 0) c = sub16(c, 1);                                    // [BYTES seg14:0x3240-0x3246]
        if (c == 0 && ok) {                                            // [BYTES seg14:0x3249-0x3253]
            buf = r.name;                                              // lstrcpy [BYTES seg14:0x3255-0x3264]
            return v;
        }
        if (count <= 0) return 0;  // nothing to find; the original loops for ever [DESIGN ADR 0016]
    }
}

int ship_with_roster(const World& w, int r, int last) {
    for (int j = 0; j <= last; ++j)  // `jg` exit: j = 0 ... last [BYTES seg14:0x4537-0x454d]
        if (ship_roster(w, j) == r) return j;
    return -1;  // [BYTES seg14:0x4552]
}

namespace {

// seg14:0x2300's entry and MainShip pass (claims 29-32 of M4, O3 of the verify file) on every
// call: one rand() % (count - 1) whose value is overwritten [BYTES seg14:0x2310-0x231d
// seg14:0x2435], DGROUP:0x931c = 0 above DGROUP:0x931a [BYTES seg14:0x232a-0x2333], then the
// MainShip validation [BYTES seg14:0x23a1-0x2425]. Returns the name buffer [bp-0x3c].
std::string pick_entry(World& w, RosterHost& h) {
    const int count = shp_count(h);
    (void)h.rng().rand();
    if (count == 1) {
        static bool once = false;
        guard_todo(once, "a one-record MASTER.SHP: the rand() % 0 dropped", "seg14:0x231d");
    }
    if (w.player_roster > roster_last(w)) w.player_roster = 0;
    if (w.kind == 6 || w.kind == 7) {
        // The race INI reads Ship1 / Ship2 and seg14:0x31d0 behind DGROUP:0x90f8 and DGROUP:0x8e5e
        // (M6) [BYTES seg14:0x2339-0x239c].
        static bool once = false;
        guard_todo(once, "seg14:0x2300's race and duel names (Ship1, Ship2) are M6", "seg14:0x2347");
    }
    std::string name;
    if (w.w9516 == 0) {
        name = h.main_ship();
        for (int v = 0; v < count; ++v) {
            if (h.ships().records[static_cast<std::size_t>(v)].name == name &&
                !ship_accepted(w, h.ships(), h.bodies(), v)) {
                name = "TRANSFORMER";        // [BYTES seg14:0x240a]
                h.main_ship_rewritten();     // WritePrivateProfileString [BYTES seg14:0x2420]
            }
        }
    } else {
        // seg14:0x31d0(0, buf): the demo's random ship (verify O3) [BYTES seg14:0x2427-0x242f].
        demo_ship(w, h.ships(), h.bodies(), h.rng(), 0, name);
    }
    return name;
}

// seg14:0x2300 kind 1 for record v [BYTES seg14:0x2c93-0x2d41].
std::int16_t score_teammate(World& w, RosterHost& h, int v, int slot) {
    const fmt::ShipRecord& r = shp_rec(h, v);
    static bool once = false;
    const std::int16_t c = design_class(h.ships(), h.bodies(), v);
    const std::int16_t e = s8(r.to_enemies), F = w.w9332, D = w.score_level;
    std::int16_t s = e == F ? std::int16_t{52} : std::int16_t{0};      // [BYTES seg14:0x2c93-0x2cb5]
    const std::int16_t d = sub16(e, F);
    if (d == -1 || d == 1) s = add16(s, 30);                           // `inc ax; je` / `loop` [BYTES seg14:0x2cba-0x2cc5]
    s = add16(s, 10);                                                  // [BYTES seg14:0x2cc9]
    if (c == D)
        s = add16(s, 25);                                              // [BYTES seg14:0x2cd5]
    else if (add16(D, 1) < c)
        s = add16(s, shl16(sub16(D, c), 5));                           // [BYTES seg14:0x2cdb-0x2cea]
    if (c < D) s = add16(s, mul16(sub16(c, D), 18));                   // [BYTES seg14:0x2ced-0x2cfd]
    s = add16(s, rmod(h.rng(), 30, once, "seg14:0x2d09"));             // [BYTES seg14:0x2d00-0x2d0b]
    for (int j = 0; j < slot; ++j) {                                   // [BYTES seg14:0x2d0e-0x2d3f]
        const int R = ship_roster(w, j);
        if (R < 0) continue;
        if (rec(w, R, "seg14:0x2d21").shp == v) s = sub16(s, 25);
    }
    return s;
}

// seg14:0x2300 kind 4 for record v [BYTES seg14:0x2d73-0x314b].
std::int16_t score_other(World& w, RosterHost& h, int v, int slot) {
    const fmt::ShipRecord& r = shp_rec(h, v);
    static bool once = false;
    RosterWords& rw = roster_words();
    const std::int16_t c = design_class(h.ships(), h.bodies(), v);
    const std::int16_t e = s8(r.to_enemies), F = w.w9332, D = w.score_level;
    const std::uint8_t Tb = r.team;
    const std::int16_t T = s8(Tb), Q = rw.mode;
    const std::int16_t A = team_word(add16(shl16(player_team(w), 3), T));  // [BYTES seg14:0x2d95-0x2db0]
    std::int16_t s = e == F ? std::int16_t{75} : std::int16_t{0};          // [BYTES seg14:0x2db3-0x2dbe]
    const std::int16_t d = sub16(e, F);
    if (d == -1 || d == 1) s = add16(s, 42);                              // [BYTES seg14:0x2dc3-0x2dd0]
    if (c == D)
        s = add16(s, 25);                                                  // [BYTES seg14:0x2ddc]
    else if (add16(D, 1) < c)
        s = add16(s, shl16(sub16(D, c), 4));                               // [BYTES seg14:0x2de2-0x2df1]
    if (c < D) s = add16(s, mul16(sub16(c, D), 11));                       // [BYTES seg14:0x2df4-0x2e04]
    if (F < 2 && A < -150) s = add16(s, 15);                               // [BYTES seg14:0x2e07-0x2e15]
    if (F > 2 && Tb == 2) s = add16(s, 30);                                // [BYTES seg14:0x2e19-0x2e2f]
    if ((Tb == 3 || Tb == 6 || Tb == 1) && w.w9432 > 300)                   // [BYTES seg14:0x2e38-0x2e6e]
        s = add16(s, max16(idiv16(w.w9432, 20), 30));
    if (Tb == 3 && ship_ok(w.player)) {                                     // [BYTES seg14:0x2e71-0x2ea7]
        const std::int16_t res = w.ship(w.player).resources();             // DGROUP:0x715e
        if (res > mul16(add16(w.detail, 4), 100)) s = add16(s, min16(idiv16(res, 20), 40));
    }
    // N, M over the roster records seen within 15 waves outside the player's team
    // [BYTES seg14:0x2eaa-0x2f29].
    std::int16_t N = 0, M = 0;
    const std::uint8_t pteam = static_cast<std::uint8_t>(rec(w, w.player_roster, "seg14:0x2ed1").team);
    for (int k = 0; k <= roster_last(w); ++k) {
        const RosterRecord& K = w.roster[static_cast<std::size_t>(k)];
        if (!(add16(s8(K.last_wave), 15) > w.wave)) continue;
        if (static_cast<std::uint8_t>(K.team) == pteam) continue;
        if (Tb == static_cast<std::uint8_t>(K.team)) {
            M = add16(M, s8(K.met));
            if (K.shp == v) M = add16(M, mul16(s8(K.met), 3));
        }
        N = add16(N, s8(K.met));
    }
    // s += 40 - lo16(seg5:0x110((N + 85)·M, N + 1)) [BYTES seg14:0x2f2b-0x2f47]
    const std::int32_t prod = static_cast<std::int32_t>(add16(N, 85)) * static_cast<std::int32_t>(M);
    s = add16(s, sub16(40, lo16(q32(prod, add16(N, 1), once, "seg14:0x2f3d"))));
    // The avenger list [BYTES seg14:0x2f4a-0x2fa8].
    for (int a = 0; a < w.w994; ++a) {
        std::int16_t& L = *avenger(w, a);
        if ((L <= 0 && add16(T, L) == 0) || (L == -100 && Tb == 3)) {
            s = add16(s, 2000);
            L = -120;
        }
    }
    if (Tb == 3 && Q != 3) s = sub16(s, 500);                              // [BYTES seg14:0x2faa-0x2fc0]
    if (T == Q) {                                                          // [BYTES seg14:0x2fc5-0x3017]
        s = add16(s, 1000);
        if (add16(D, 2) < c) s = add16(s, 150);
        if (Q == 4) {
            // [bp-0x10] has no writer in seg14:0x2300 (verify claim 28); 0 [DESIGN ADR 0016].
            static bool o10 = false;
            guard_todo(o10, "seg14:0x2300's unwritten [bp-0x10] reads 0", "seg14:0x2fef");
            const std::int16_t bp10 = 0;
            if (bp10 < 2) s = add16(s, 300);
        }
        if (Q == 3 && e < 2) s = add16(s, 300);
        s = add16(s, rmod(h.rng(), 80, once, "seg14:0x3015"));
    }
    s = add16(s, rmod(h.rng(), 30, once, "seg14:0x3023"));                // [BYTES seg14:0x301a-0x3025]
    // The earlier slots [BYTES seg14:0x3028-0x30d1]: a slot holding v takes -45 and skips the +24.
    std::int16_t n0 = 0, n1 = 0, n2 = 0;
    for (int j = 0; j < slot; ++j) {
        const int R = ship_roster(w, j);
        if (R < 0) continue;
        const RosterRecord& K = rec(w, R, "seg14:0x30ba");
        if (K.shp == v)
            s = sub16(s, 45);
        else if (static_cast<std::uint8_t>(K.team) == Tb)
            s = add16(s, 24);
        const std::uint8_t kt = static_cast<std::uint8_t>(K.team);
        if (kt == 2) ++n2;
        if (kt == 0) ++n0;
        if (kt == 1) ++n1;
    }
    if (n0 == 0 && n2 != 0 && Tb == 0) s = add16(s, 25);                  // [BYTES seg14:0x30d3-0x30ee]
    if (n0 != 0 && n2 == 0 && Tb == 2) s = add16(s, 25);                  // [BYTES seg14:0x30f2-0x310d]
    if (n1 == 0 && n2 != 0 && Tb == 1) s = add16(s, 25);                  // [BYTES seg14:0x3111-0x312c]
    if (n1 != 0 && n2 == 0 && Tb == 2) s = add16(s, 25);                  // [BYTES seg14:0x3130-0x314b]
    return s;
}

}  // namespace

int pick_design(World& w, RosterHost& h, int kind, int slot) {
    const std::string name = pick_entry(w, h);
    const int count = shp_count(h);
    std::int16_t best = -1000;  // [bp-0x14] [BYTES seg14:0x2325]
    int besti = 0;              // [bp-0x16], written whenever best is
    std::int16_t score = g_score_slot;  // [bp-0x12]
    static bool once = false;
    for (int v = 0; v < count; ++v) {
        const fmt::ShipRecord& r = h.ships().records[static_cast<std::size_t>(v)];
        if (r.name.empty() || !ship_accepted(w, h.ships(), h.bodies(), v)) {  // [BYTES seg14:0x3173 seg14:0x243d-0x244e]
            score = -1000;
            continue;
        }
        if (kind == 0) {
            // The MainShip record wins at once with 20000 (the race paths are M6) [BYTES
            // seg14:0x24e1-0x2520 seg14:0x2492-0x249d].
            if (r.name == name) {
                besti = v;
                best = 20000;
                break;
            }
            continue;
        }
        if (kind == 2) {
            if (r.team == 7 && s8(r.to_enemies) > 1) {  // [BYTES seg14:0x252a-0x2542]
                if (w.w9332 < 2 || w.w964a > 7)
                    score = r.to_enemies == 2 ? 100 : 20;  // PATROLMAN [BYTES seg14:0x2544-0x256e]
                else
                    score = r.to_enemies == 3 ? 50 : 20;   // CONSTABLE [BYTES seg14:0x2571-0x2585]
                if (score > best) {                         // [BYTES seg14:0x314f-0x315d]
                    besti = v;
                    best = score;
                }
            }
            continue;
        }
        if ((kind == 3 && r.team == 7 && r.to_enemies == 1) || (kind == 5 && r.team == 7 && r.to_enemies == 0)) {
            score = add16(rmod(h.rng(), 30, once, "seg14:0x25b1"), 50);  // [BYTES seg14:0x25a8-0x25b6]
            if (score > best) {                                         // `jle` [BYTES seg14:0x25b9-0x25c6]
                besti = v;
                best = score;
            }
            continue;
        }
        if ((w.kind == 6 && slot != w.player && r.team != 7 && kind == 4) ||
            (w.kind == 7 && slot != w.player && r.team != 7)) {  // [BYTES seg14:0x25ec-0x2612 seg14:0x2894-0x28b4]
            // The race and duel scores, the DGROUP:0x94aa switches (M6) [BYTES seg14:0x25ec-0x2c5e].
            static bool race = false;
            guard_todo(race, "seg14:0x2300's race and duel scores are M6: the normal scores stand in",
                       "seg14:0x2617");
        }
        if (kind == 1) {
            if (s8(r.team) != player_team(w)) continue;  // [BYTES seg14:0x2c71-0x2c8e]
            score = score_teammate(w, h, v, slot);
        } else if (kind == 4) {
            if (s8(r.team) == player_team(w) || r.team == 7) continue;  // [BYTES seg14:0x2d4a-0x2d6e]
            score = score_other(w, h, v, slot);
        } else {
            continue;
        }
        if (score > best) {  // [BYTES seg14:0x314f-0x315d]
            besti = v;
            best = score;
        }
    }
    g_score_slot = score;
    int out = besti;
    w.w95b4 = best;                      // [BYTES seg14:0x317f-0x3188]
    if (best <= -1000) out = 1;          // [BYTES seg14:0x318b-0x3190]
    if ((kind == 4 || kind == 1) && add16(w.w992, 30) > w.budget && roster_words().mode == -1 && best < 1000)
        w.w95b4 = -1;                    // [BYTES seg14:0x3195-0x31bb]
    return out;
}

namespace {

// The long-term term of seg14:0x3274 (seg14:0x3694-0x37f4 for kind 1, seg14:0x3982-0x3ae2 for
// kind 4), all 32-bit; `kind4` picks the second block's run test (0 against 0xff) [BYTES
// seg14:0x3744-0x3751 seg14:0x3a32-0x3a3f]. Read in work/world/m5s3/roster-report.md.
std::int32_t long_term(const World& w, const RosterRecord& R, bool kind4) {
    static bool once = false;
    const std::int32_t L = R.w06;  // `cdq` of word +0x06
    const std::int16_t lw = s8(R.last_wave);
    if (!(lw < sub16(w.wave, 1))) {
        // The ship played the last wave: R1 = run + 1; T = ((32 R1)/2 + (32 R1²)/(-9) + 32)·L / 32,
        // then 0 when negative [BYTES seg14:0x36bd-0x3741 seg14:0x39ab-0x3a2f].
        const std::int32_t R1 = add16(s8(R.run), 1);
        const std::int32_t q1 = q32(static_cast<std::int32_t>(static_cast<std::uint32_t>(mul32(R1, R1)) << 5), -9,
                                    once, "seg14:0x36eb");
        const std::int32_t S = static_cast<std::int32_t>(static_cast<std::uint32_t>(R1) << 5);  // seg5:0x8c0 (LXLSH)
        const std::int32_t T = q32(mul32(add32(add32(S / 2, q1), 32), L), 32, once, "seg14:0x3727");
        return T < 0 ? 0 : T;
    }
    if (R.run == (kind4 ? 0x00 : 0xff)) return L;
    // Away G waves: Rr = max(run + 1, 4); T = 0 while Rr/2 >= G, else min(((64 G)/Rr - 32)·L/32,
    // 5 L / 4) [BYTES seg14:0x3755-0x37f1 seg14:0x3a43-0x3adf].
    const std::int32_t G = sub16(sub16(0, sub16(lw, w.wave)), 1);
    const std::int32_t Rr = max16(add16(s8(R.run), 1), 4);
    if (q32(Rr, 2, once, "seg14:0x378b") >= G) return 0;
    if (Rr == 0) return L;
    const std::int32_t g64 = static_cast<std::int32_t>(static_cast<std::uint32_t>(G) << 6);
    const std::int32_t V = q32(mul32(add32(q32(g64, Rr, once, "seg14:0x37af"), -32), L), 32, once, "seg14:0x37bf");
    const std::int32_t W = q32(mul32(L, 5), 4, once, "seg14:0x37db");
    return V <= W ? V : W;
}

}  // namespace

int pick_known(World& w, RosterHost& h, int kind, int slot) {
    static bool once = false;
    (void)h.rng().rand();  // rand() % (count - 1) into [bp-6], overwritten [BYTES seg14:0x3283-0x3290 seg14:0x32aa]
    std::int16_t best = -1000;  // [BYTES seg14:0x3293]
    if (w.kind == 6) {          // [BYTES seg14:0x3298-0x32a7]
        w.w95b2 = -10000;
        return 0;
    }
    int besti = 0;
    std::int16_t score = g_score_slot;  // [bp-0x12]: seg14:0x2300's last score [DESIGN ADR 0016]
    {
        static bool o12 = false;
        guard_todo(o12, "seg14:0x3274's unwritten [bp-0x12] reads seg14:0x2300's last score", "seg14:0x32e7");
    }
    RosterWords& rw = roster_words();
    const RosterRecord& P = rec(w, w.player_roster, "seg14:0x3612");
    const std::uint8_t pteam = static_cast<std::uint8_t>(P.team);
    const std::int16_t F = w.w9332, D = w.score_level, Q = rw.mode;
    for (int r = 0; r <= roster_last(w); ++r) {
        const RosterRecord& R = w.roster[static_cast<std::size_t>(r)];
        for (int j = 0; j < slot; ++j)  // [BYTES seg14:0x32b2-0x32ce]
            if (ship_roster(w, j) == r) score = -888;
        // [BYTES seg14:0x32d0-0x3308]
        if (r == w.player_roster || R.systems[0] <= 0 || score == -888 ||
            !ship_accepted(w, h.ships(), h.bodies(), R.shp)) {
            score = -1000;
            continue;
        }
        if (w.kind == 7 && slot == 1 && r == w.w93b6) {  // the duel's challenger [BYTES seg14:0x35e7-0x3607]
            besti = r;
            best = 30000;
            continue;
        }
        const std::uint8_t T = static_cast<std::uint8_t>(R.team);
        const fmt::ShipRecord& sr = shp_rec(h, R.shp);
        const std::int16_t c = s8(R.cls);
        const std::int16_t e = s8(sr.to_enemies);
        const std::int16_t rel = s8(roster_byte(const_cast<RosterRecord&>(R), 0x3c + w.player_roster));
        if (kind == 1) {
            if (T != pteam) continue;  // [BYTES seg14:0x3612-0x3629]
            score = -10;               // [BYTES seg14:0x362d]
            if (e == F) score = 42;    // [BYTES seg14:0x3647-0x364d]
            const std::int16_t d = sub16(e, F);
            if (d == -1 || d == 1) score = add16(score, 30);  // [BYTES seg14:0x3652-0x365d]
            if (c == D)
                score = add16(score, 30);                     // [BYTES seg14:0x3669]
            else if (add16(D, 1) < c)
                score = add16(score, mul16(sub16(D, c), 28)); // [BYTES seg14:0x366f-0x367e]
            if (c < D) score = add16(score, shl16(sub16(c, D), 4));  // [BYTES seg14:0x3681-0x3691]
            const std::int16_t q = lo16(q32(long_term(w, R, false), 9, once, "seg14:0x37fc"));  // [BYTES seg14:0x37f4-0x37fc]
            score = add16(score, add16(q, add16(rel, rel)));  // [BYTES seg14:0x3801-0x3824]
            if (static_cast<std::int16_t>(s8(R.team)) == Q) score = add16(score, 1000);  // [BYTES seg14:0x3827-0x3832]
            score = add16(score, rmod(h.rng(), 30, once, "seg14:0x3d62"));  // [BYTES seg14:0x3837-0x3d64]
        } else if (kind == 4) {
            if (T == pteam || T == 7) continue;  // [BYTES seg14:0x3848-0x3866]
            // A = 10·rel + word DGROUP:0x3824 + 2·(8 P + T) [BYTES seg14:0x387a-0x38a1]
            const std::int16_t A = add16(static_cast<std::int16_t>(10 * rel),
                                         team_word(add16(shl16(player_team(w), 3), s8(R.team))));
            score = -10;                 // [BYTES seg14:0x38a4]
            if (e == F) score = 65;      // [BYTES seg14:0x38b6-0x38bc]
            const std::int16_t d = sub16(e, F);
            if (d == -1 || d == 1) score = add16(score, 50);  // [BYTES seg14:0x38c1-0x38cc]
            if (c == D)
                score = add16(score, 25);                     // [BYTES seg14:0x38d8]
            else if (add16(D, 1) < c)
                score = add16(score, shl16(sub16(D, c), 4));  // [BYTES seg14:0x38de-0x38ed]
            if (c < D) score = add16(score, mul16(sub16(c, D), 11));  // [BYTES seg14:0x38f0-0x3900]
            if (F < 2 && A < -150) score = add16(score, 20);  // [BYTES seg14:0x3903-0x3911]
            if (F > 2 && T == 2) score = add16(score, 25);    // [BYTES seg14:0x3915-0x392b]
            if (F > 1 && T == 4) score = add16(score, 10);    // [BYTES seg14:0x392f-0x3945]
            if ((T == 3 || T == 6 || T == 1) && w.w9432 > 300)  // [BYTES seg14:0x3949-0x397f]
                score = add16(score, max16(idiv16(w.w9432, 17), 50));
            // min(long term / 10, 60) + abs(rel) [BYTES seg14:0x3ae2-0x3b20]
            const std::int32_t lt = q32(long_term(w, R, true), 10, once, "seg14:0x3aee");
            const std::int16_t q = lt > 60 ? std::int16_t{60} : lo16(lt);
            score = add16(score, add16(q, abs16(rel)));
            for (int a = 0; a < w.w994; ++a) {  // [BYTES seg14:0x3b23-0x3b8f]
                std::int16_t& L = *avenger(w, a);
                if ((L <= 0 && add16(s8(R.team), L) == 0) || (L == -100 && T == 3) || L == r) {
                    score = add16(score, 2000);
                    L = -120;
                }
            }
            if (T == 3) score = sub16(score, 500);  // [BYTES seg14:0x3b91-0x3ba0]
            std::int16_t N = 0, M = 0;              // [BYTES seg14:0x3ba5-0x3c0e]
            for (int k = 0; k <= roster_last(w); ++k) {
                const RosterRecord& K = w.roster[static_cast<std::size_t>(k)];
                if (!(add16(s8(K.last_wave), 15) > w.wave)) continue;
                if (static_cast<std::uint8_t>(K.team) == pteam) continue;
                if (static_cast<std::uint8_t>(K.team) == T) M = add16(M, s8(K.met));
                N = add16(N, s8(K.met));
            }
            const std::int32_t prod = static_cast<std::int32_t>(add16(N, 85)) * static_cast<std::int32_t>(M);
            score = add16(score, sub16(40, lo16(q32(prod, add16(N, 1), once, "seg14:0x3c22"))));  // [BYTES seg14:0x3c10-0x3c2c]
            std::int16_t n0 = 0, n1 = 0, n2 = 0;  // [BYTES seg14:0x3c2f-0x3cbb]
            for (int j = 0; j < slot; ++j) {
                const int J = ship_roster(w, j);
                if (J < 0) continue;
                const std::uint8_t kt = static_cast<std::uint8_t>(rec(w, J, "seg14:0x3c5b").team);
                if (kt == T) score = add16(score, 24);
                if (kt == 2) ++n2;
                if (kt == 0) ++n0;
                if (kt == 1) ++n1;
            }
            const std::uint8_t ST = sr.team;  // SHP +0x03 of the record's design, not +0x64 [BYTES seg14:0x3cc9-0x3cd9]
            if (n0 == 0 && n2 != 0 && ST == 0) score = add16(score, 25);
            if (n0 != 0 && n2 == 0 && ST == 2) score = add16(score, 25);
            if (n1 == 0 && n2 != 0 && ST == 1) score = add16(score, 25);
            if (n1 != 0 && n2 == 0 && ST == 2) score = add16(score, 25);  // [BYTES seg14:0x3cbd-0x3d55]
            score = add16(score, rmod(h.rng(), 40, once, "seg14:0x3d62"));  // [BYTES seg14:0x3d59-0x3d64]
        } else {
            continue;
        }
        if (score > best) {  // `jle` [BYTES seg14:0x3d67-0x3d78]
            besti = r;
            best = score;
        }
    }
    w.w95b2 = best;                       // [BYTES seg14:0x3da2-0x3da5]
    // [bp-0x12] is the stack slot seg14:0x2300 shares at the same depth, so a later seg14:0x2300
    // call that writes no score reads this one's last (review L5) [DESIGN ADR 0016].
    g_score_slot = score;
    return best > -1000 ? besti : -1;     // [BYTES seg14:0x3d8e-0x3d9d]
}

// ---------------------------------------------------------------- records and names

void roster_name(World& w, RosterHost& h, int ship, int r) {
    RosterRecord* R = roster_at(w, r);
    if (!R) {
        static bool once = false;
        guard_todo(once, "seg9:0x5be0 for a roster index outside the block", "seg9:0x5be0");
        return;
    }
    std::string name;
    if (ship == w.player || w.handover.demons_gate != 0) {  // [BYTES seg9:0x5bee-0x5bfd seg9:0x5ca6-0x5cbb]
        name = h.pilot();
    } else {
        // seg13:0x218a(sext(+0x64), buf), cut at the first ' ' or a final 'S' within 20 bytes
        // [BYTES seg9:0x5c00-0x5c40]; then buf + " " (DGROUP:0x9ec) + seg6:0xf1a(r) [BYTES
        // seg9:0x5c44-0x5ca4 seg6:0xf1a-0xf53].
        std::string buf = h.team_name(s8(static_cast<std::uint8_t>(R->team)));
        for (std::size_t k = 0; k < 20 && k < buf.size(); ++k) {
            if (buf[k] == ' ' || (buf[k] == 'S' && k + 1 == buf.size())) {
                buf.resize(k);
                break;
            }
        }
        name = buf + " " + std::to_string(r);
    }
    // lstrcpy / lstrcat into +0x65 unbounded: the port writes up to the record's last byte
    // (a longer name runs into +0x72 ... as in the original) [DESIGN] [BYTES seg9:0x5c5e seg9:0x5c88].
    std::size_t k = 0;
    for (; k < name.size() && 0x65 + k < kRosterRecordSize - 1; ++k)
        roster_byte(*R, static_cast<int>(0x65 + k)) = static_cast<std::uint8_t>(name[k]);
    roster_byte(*R, static_cast<int>(0x65 + k)) = 0;
}

int roster_take(World& w, RosterHost& h, int slot, int shp, int r0) {
    static bool once = false;
    int r;
    if (w.w95b4 > w.w95b2) {  // [BYTES seg14:0xde5-0xdec]
        if (roster_last(w) <= 38) {
            // inc DGROUP:0x931a; GlobalReAlloc(…, 0x42): the new record is zero [BYTES seg14:0xecd-0xf00].
            w.roster.push_back(RosterRecord{});
            r = roster_last(w);
        } else {
            // The lowest +0x06 below 1000 among records 1 ... 39 no earlier slot holds [BYTES
            // seg14:0xdf9-0xe67].
            std::int16_t lowest = 1000;
            r = -1;
            for (int k = 1; k < 40; ++k) {
                const std::int16_t v = rec(w, k, "seg14:0xe57").w06;
                if (!(v < lowest)) continue;
                bool free = true;
                for (int j = 0; j < slot; ++j)
                    if (ship_roster(w, j) == k) free = false;
                if (free) {
                    lowest = v;
                    r = k;
                }
            }
            if (r == -1) {  // [BYTES seg14:0xe69-0xe76]
                h.beep();
                r = 10;
            }
            // Every list byte naming r in the directory records 0 ... DGROUP:0x94a4 - 1 [BYTES
            // seg14:0xe7b-0xec6].
            for (DirRecord& d : w.records)
                for (int k = 0; k < 8; ++k)
                    if (s8(dir_byte(d, k)) == r) dir_byte(d, k) = 0xff;
        }
        // The record's fields [BYTES seg14:0xf03-0x10bb].
        RosterRecord& N = rec(w, r, "seg14:0xf06");
        const fmt::ShipRecord& S = shp_rec(h, shp);
        N.shp = static_cast<std::int16_t>(shp);
        N.relation.fill(0);
        N.systems.fill(100);
        for (std::size_t a = 0; a < 5; ++a) {
            N.rel_a[a] = 0;
            N.rel_w[a] = 0;
            N.rel_r[a] = 0xff;
        }
        if (S.weapon != 0) {  // [BYTES seg14:0xf9b-0x1004]
            N.slots = 2;
            N.weapon[1] = remap_weapon(w, S.weapon);
            N.count[1] = 20;
        } else {
            N.slots = 1;
        }
        N.w06 = 15;  // [BYTES seg14:0x101b]
        N.wing1 = 1;
        N.wing0 = N.wing1;
        N.barrels = 1;
        N.b75 = 1;
        N.fuel40 = S.fuel_tank;  // [BYTES seg14:0x1052-0x1065]
        // rand() % ((DGROUP:0x9334 + 1)·10) + 10, then rand() % ((… + 1)·50) + 150 [BYTES
        // seg14:0x106c-0x109f].
        N.credit10 = add16(rmod(h.rng(), mul16(add16(w.score_level, 1), 10), once, "seg14:0x107a"), 10);
        N.resources = add16(rmod(h.rng(), mul16(add16(w.score_level, 1), 50), once, "seg14:0x1095"), 150);
        N.met = 0;  // [BYTES seg14:0x10a7]
        N.team = s8(S.team);  // [BYTES seg14:0x10ac-0x10bb]
        roster_name(w, h, slot, r);  // seg9:0x5be0(slot, r) [BYTES seg14:0x10bf-0x10c5]
    } else {
        r = r0;
        // The first byte naming r0 among the 10 bytes +0x1c0 ... +0x1c9 of every directory record
        // (two bytes past the 8-byte list) [BYTES seg14:0x10d0-0x111f].
        for (DirRecord& d : w.records)
            for (int k = 0; k < 10; ++k)
                if (s8(d.b[0x1c0 + static_cast<std::size_t>(k)]) == r0) {
                    d.b[0x1c0 + static_cast<std::size_t>(k)] = 0xff;
                    break;
                }
        DirRecord& cur = w.dir();  // [BYTES seg14:0x1121-0x1153]
        for (int k = 0; k < 8; ++k)
            if (dir_byte(cur, k) == 0xff) {
                dir_byte(cur, k) = lo8(r0);
                break;
            }
        if (r0 < 0 || r0 > roster_last(w)) {
            static bool o = false;
            guard_todo(o, "seg14:0xdd6's known path for a roster index outside the block", "seg14:0x1158");
        } else {
            RosterRecord& R = w.roster[static_cast<std::size_t>(r0)];
            const std::int16_t lw = s8(R.last_wave);
            if (lw < sub16(w.wave, 1)) {  // [BYTES seg14:0x1158-0x1189]
                const std::int16_t v = min16(mul16(sub16(w.wave, lw), add16(w.score_level, 15)), 200);
                R.credit10 = add16(R.credit10, v);
            }
        }
    }
    if (ship_ok(slot)) w.ship(slot).roster_index = static_cast<std::int16_t>(r);  // [BYTES seg14:0x118d-0x1195]
    w.w992 = add16(w.w992, 40);                                                    // [BYTES seg14:0x1199]
    return r;
}

void relation_memory(World& w, int i) {
    if (!ship_ok(i)) return;
    ShipRecord& I = w.ship(i);
    const std::uint8_t rb = lo8(I.roster_index);
    if (rb == 0xff) return;  // [BYTES seg14:0x422b-0x4239]
    RosterRecord& R = rec(w, s8(rb), "seg14:0x427f");
    const int last = std::min<int>(w.last_ship, kShipCount - 1);
    auto word = [&](int j) -> std::int16_t& { return I.w72[static_cast<std::size_t>(j)]; };
    auto bee = [&](int j) -> std::uint8_t& { return I.bee[static_cast<std::size_t>(j)]; };
    for (int j = 0; j <= last; ++j) word(j) = abs16(word(j));  // [BYTES seg14:0x4244-0x4267]
    for (int a = 0; a < 5; ++a)                                // [BYTES seg14:0x4270-0x42ce]
        for (int j = 0; j <= last; ++j)
            if (s8(R.rel_r[static_cast<std::size_t>(a)]) == ship_roster(w, j))
                R.rel_w[static_cast<std::size_t>(a)] = word(j);
    // The flag [bp-0x10]: unwritten when no ship is scored (verify correction 43); 0
    // [DESIGN ADR 0016].
    bool flag = false;
    bool scored = false;
    for (int j = 0; j <= last; ++j) {  // [BYTES seg14:0x42d0-0x440e]
        if (j == i || w.ship(j).bec == 0) {
            bee(j) = 0;
            word(j) = 0;
            continue;
        }
        std::int16_t S1 = 0, S2 = 0, m1 = 0, m2 = 0;
        std::uint8_t k1 = 0, k2 = 0;
        for (int k = 1; k <= 5; ++k) {
            const std::int16_t b = sbyte_at(I, 0xf6 + 9 * j + k);
            S1 = add16(S1, b);
            if (b > m1 && k != 2) {
                m1 = b;
                k1 = static_cast<std::uint8_t>(k);
            }
        }
        for (int k = 6; k <= 8; ++k) {
            const std::int16_t b = sbyte_at(I, 0xf6 + 9 * j + k);
            S2 = add16(S2, b);
            if (b > m2) {
                m2 = b;
                k2 = static_cast<std::uint8_t>(k);
            }
        }
        const std::int16_t enemy = mul16(sub16(8, sbyte_at(I, 0x5e)), S1);
        const std::int16_t friend_ = mul16(sub16(8, sbyte_at(I, 0x5d)), S2);
        scored = true;
        const std::int16_t sum = friend_ >= enemy ? S2 : S1;
        const std::uint8_t k = friend_ >= enemy ? k2 : k1;
        if (word(j) > sum) {
            flag = false;
        } else {
            bee(j) = k;
            word(j) = sum;
            flag = true;
        }
    }
    if (!scored) {
        static bool once = false;
        guard_todo(once, "seg14:0x421c's unwritten flag [bp-0x10] reads 0", "seg14:0x4411");
    }
    if (!flag) return;  // [BYTES seg14:0x4411-0x4415]
    for (int j = 0; j <= last; ++j) {  // [BYTES seg14:0x4419-0x4514]
        for (int a = 0; a < 5; ++a) {
            if (R.rel_w[static_cast<std::size_t>(a)] >= word(j)) continue;
            for (int m = 4; m > a; --m) {
                const auto mm = static_cast<std::size_t>(m);
                R.rel_a[mm] = R.rel_a[mm - 1];
                R.rel_w[mm] = R.rel_w[mm - 1];
                R.rel_r[mm] = R.rel_r[mm - 1];
            }
            const auto aa = static_cast<std::size_t>(a);
            R.rel_a[aa] = bee(j);
            R.rel_w[aa] = word(j);
            R.rel_r[aa] = lo8(ship_roster(w, j));
            break;
        }
    }
}

// ---------------------------------------------------------------- the placement

void ship_place(World& w, RosterHost& h, int i) {
    if (!ship_ok(i)) return;
    ShipRecord& S = w.ship(i);
    const int R = S.roster_index;
    // s: roster +0x00, or DGROUP:0x4ed8 + 2·sext(+0x5e) without a record [BYTES seg14:0x1444-0x146e].
    int s;
    if (R != -1) {
        s = rec(w, R, "seg14:0x1457").shp;
    } else {
        const int k = sbyte_at(S, 0x5e);
        if (k >= 0 && k < 4) {
            s = roster_words().enforcer_shp[static_cast<std::size_t>(k)];
        } else {
            static bool once = false;
            guard_todo(once, "DGROUP:0x4ed8 + 2k outside the four Enforcer words reads 0", "seg14:0x146a");
            s = 0;
        }
    }
    if (shp_count(h) - 1 < s) {  // `cmp; jge` [BYTES seg14:0x1471-0x148e]
        if (R != -1) {
            rec(w, R, "seg14:0x1482").shp = 0;
        } else {
            // The store lands 0x7c bytes before the block; the port skips it [DESIGN ADR 0016].
            static bool once = false;
            guard_todo(once, "seg14:0x1482's store into roster record -1 is skipped", "seg14:0x1482");
        }
        s = 0;
    }
    // The team word [BYTES seg14:0x149d-0x14ca].
    const std::int16_t team = R != -1 ? std::int16_t{s8(static_cast<std::uint8_t>(rec(w, R, "seg14:0x14ad").team))}
                                      : std::int16_t{s8(shp_rec(h, s).team)};
    set_design_team(w, S.design_index, team);
    S.radar_hide = 0;            // +0x147 [BYTES seg14:0x14d3]
    h.design_fill(w, i, s);      // seg14:0x1b84(i, n, R, s) [BYTES seg14:0x14d8-0x14e5]
    ship_from_roster(w, i, shp_rec(h, s));
}

RosterBuild build_wave_roster(World& w, RosterHost& h, int mode) {
    RosterWords& rw = roster_words();
    RosterBuild out;
    const bool k67 = w.kind == 6 || w.kind == 7;
    const bool demon = w.handover.demons_gate != 0;
    // ---- the head [BYTES seg14:0x10-0xf1]
    if (h.demo_death() != 0 && !k67) {
        if (w.w9336 < 1) w.w9336 = 1;
        if (w.opponents < 1) w.opponents = 1;
        if (w.opponents > 7) w.opponents = 7;
        h.clear_demo_death();
    }
    w.opponents = min16(sub16(7, w.w932c), w.opponents);  // [BYTES seg14:0x52-0x62]
    std::array<std::int16_t, 8>& fresh = out.fresh;
    fresh.fill(1);                                        // [BYTES seg14:0x65-0x7b]
    rw.mode = static_cast<std::int16_t>(mode);            // [BYTES seg14:0x7d-0x80]
    rw.enforcer_shp.fill(0);                              // [BYTES seg14:0x83-0x8e]
    if (w.opponents > 2) {                                // [BYTES seg14:0x91-0xca]
        if (w.detail < 2 && !k67) {
            w.opponents = sub16(w.opponents, 1);
            if (w.score_level > 0) w.score_level = add16(w.score_level, 1);
        }
        w.opponents = min16(add16(w.detail, 3), w.opponents);
    }
    int n = 0;  // [bp-0xe]
    if (mode >= 0) {  // [BYTES seg14:0xcd-0xda]
        w.opponents = add16(w.last_ship, 1);
        n = w.opponents;
    }
    if (k67) {  // [BYTES seg14:0xdd-0xf1]
        w.w932c = 0;
        w.w9346 = -1;
    }
    // The old slots, saved at the entry [BYTES seg14:0x1c8-0x230].
    const int old_last = std::min<int>(w.last_ship, kShipCount - 1);
    int last_saved = 0;                     // [bp-0x1c]
    std::array<std::int16_t, 8> kept{}, oldR{}, oldD{};
    std::array<SlotWords, 8> saved{};
    std::int16_t same = 0;                  // [bp-0x2a]
    std::int16_t c = 0;                     // [bp-0x32]
    if (mode == -1) {
        if (w.wave > 1) {  // the pull of every relation byte toward the team's diagonal [BYTES seg14:0x104-0x183]
            last_saved = old_last;
            for (int r = 0; r <= roster_last(w); ++r) {
                RosterRecord& K = w.roster[static_cast<std::size_t>(r)];
                const std::int16_t T = team_word(mul16(s8(static_cast<std::uint8_t>(K.team)), 9));
                for (int j = 0; j <= roster_last(w); ++j) {
                    std::uint8_t& b = roster_byte(K, 0x3c + j);
                    static bool once = false;
                    const std::int16_t q1 = q16(mul16(sub16(mul16(s8(b), 10), T), 9), 10, once, "seg14:0x161");
                    b = lo8(q16(add16(q1, T), 10, once, "seg14:0x16b"));
                }
            }
        }
        if (w.w932c != 0 || w.w9346 != -1) {  // [BYTES seg14:0x18a-0x1af]
            if (w.opponents > 2) w.opponents = sub16(w.opponents, 1);
            if (w.opponents < w.w9336) w.w9336 = w.opponents;
        }
        rw.first = 0;  // [BYTES seg14:0x1bb]
        n = 0;
        if (w.wave == 1) w.ship(0).roster_index = -1;  // DGROUP:0x7166 [BYTES seg14:0x1c8-0x1cf]
        for (int j = 0; j <= old_last; ++j) {
            const auto jj = static_cast<std::size_t>(j);
            kept[jj] = 0;
            oldR[jj] = w.ship(j).roster_index;
            oldD[jj] = w.ship(j).design_index;
            saved[jj] = h.slot_words(j);
        }
        // ---- the player [BYTES seg14:0x232-0x47e] (roster.h roster_placement)
        const RosterPlacement rp = roster_placement(w, h.ships(), h.bodies(), h.main_ship(), h.rng(), h.pilot());
        if (rp.pick.rewrite) h.main_ship_rewritten();
        fresh[0] = rp.fresh_ship ? 1 : 0;
        n = 1;  // [BYTES seg14:0x482]
        // ---- the directory's known ships [BYTES seg14:0x485-0x5d9]
        if (w.added == 0 && !demon && !k67) {
            for (int k = 0; k < 8; ++k) {
                if (add16(mul16(w.w994, 30), w.w992) > w.budget) break;  // [BYTES seg14:0x5cc-0x5d9]
                const std::uint8_t b = dir_byte(w.dir(), k);
                if (b == 0xff) continue;
                const int R = s8(b);
                if (!(n < 6)) continue;
                if (!(s8(rec(w, R, "seg14:0x4dc").run) < 5)) continue;
                if (!ship_accepted(w, h.ships(), h.bodies(), R)) continue;  // seg14:0x455c(R), the roster index [BYTES seg14:0x4fb]
                w.ship(n).roster_index = static_cast<std::int16_t>(R);
                if (w.restored == 0) fresh[static_cast<std::size_t>(n)] = 0;  // [BYTES seg14:0x525-0x531]
                const std::uint8_t pt = static_cast<std::uint8_t>(rec(w, w.player_roster, "seg14:0x53f").team);
                const std::uint8_t rt = static_cast<std::uint8_t>(rec(w, R, "seg14:0x54d").team);
                if (pt == rt) same = add16(same, 1);
                w.w992 = add16(w.w992, 40);  // [BYTES seg14:0x556]
                for (int a = 0; a < w.w994; ++a) {  // [BYTES seg14:0x562-0x5ae] (verify O8: +100 here)
                    std::int16_t& L = *avenger(w, a);
                    if (L == R || add16(L, s8(rt)) == 0 || (L == 100 && rt == 3)) L = -120;
                }
                ++n;
                if (add16(add16(mul16(w.w994, 30), w.w992), 40) > w.budget) break;  // [BYTES seg14:0x5b0-0x5c1]
            }
        }
    }
    // ---- the counts [BYTES seg14:0x5dd-0x66c]
    if (((same == 0 && w.w9336 != 0) || w.w932c != 0) && mode == -1 && !k67 && !demon)
        w.opponents = min16(max16(static_cast<std::int16_t>(n), w.opponents), 7);
    if (!k67 && !demon) {
        c = 0;
        for (int a = 0; a < w.w994; ++a)
            if (*avenger(w, a) != -120) ++c;
        w.opponents = max16(w.opponents, static_cast<std::int16_t>(c + n - 1));
    }
    bool full = n > w.opponents && w.w9346 == -1;  // [BYTES seg14:0x66f-0x67c]
    std::int16_t r0 = 0;  // [bp-0xa0]
    bool r0_set = false;
    if (!full) {
        // ---- the teammates [BYTES seg14:0x680-0x6fc]
        if (mode == -1 || player_team(w) == mode) {
            if (mode != -1) {
                w.w9336 = 1;
                same = 0;
            }
            for (int k = same; k < w.w9336; ++k) {
                if (!(n < 7)) continue;
                const int s = pick_design(w, h, 1, n);
                r0 = static_cast<std::int16_t>(pick_known(w, h, 1, n));
                r0_set = true;
                if (s != -1) {
                    roster_take(w, h, n, s, r0);
                    ++n;
                }
            }
        }
        if (!k67 && !demon) w.opponents = max16(w.opponents, static_cast<std::int16_t>(c + n - 1));  // [BYTES seg14:0x6fe-0x720]
        // ---- the other teams [BYTES seg14:0x723-0x795]
        if (mode == -1 || (player_team(w) != mode && mode <= 6)) {
            for (int k = n; k <= w.opponents; ++k) {
                if (n >= kShipCount) {
                    // The loop has no cap of its own; ship 8 would be the next table [DESIGN ADR 0016].
                    static bool once = false;
                    guard_todo(once, "seg14:0x0's other-teams loop past ship 7 stops", "seg14:0x74a");
                    break;
                }
                const int s = pick_design(w, h, 4, n);
                if (mode == -1) {
                    r0 = static_cast<std::int16_t>(pick_known(w, h, 4, n));
                    r0_set = true;
                } else if (!r0_set) {
                    // [bp-0xa0] is unwritten in a call-in that skipped the teammates
                    // (verify claim 14); 0 [DESIGN ADR 0016].
                    static bool once = false;
                    guard_todo(once, "seg14:0x0's unwritten [bp-0xa0] reads 0", "seg14:0x777");
                }
                if (s != -1) {
                    roster_take(w, h, n, s, r0);
                    ++n;
                }
            }
        }
        // ---- the Enforcers [BYTES seg14:0x797-0x8e1]
        if (n < 8) {
            if (!demon) {
                if (mode == -1 || mode == 0x14) {
                    const bool warrant = w.w9346 != -1 && ship_with_roster(w, w.w9346, n) != -1;
                    if (warrant || mode == 0x14) {  // the Terminator [BYTES seg14:0x7d2-0x7f5]
                        const int s = pick_design(w, h, 5, n);
                        rw.enforcer_shp[0] = static_cast<std::int16_t>(s);
                        w.ship(n).roster_index = -1;
                        set_byte_at(w.ship(n), 0x5e, 0);
                        ++n;
                        if (w.score_level > 6 && ship_with_roster(w, w.w9346, n) == w.player &&
                            static_cast<std::int8_t>(w.ba14d[0]) > 2 && n < 7) {  // a second one [BYTES seg14:0x7f8-0x830]
                            w.ship(n).roster_index = -1;
                            set_byte_at(w.ship(n), 0x5e, 0);
                            ++n;
                        }
                    }
                }
                if ((w.w932c != 0 && mode == -1 && rw.enforcer_shp[0] <= 0 && add16(w.w992, 25) < w.budget) ||
                    mode == 0x16) {  // a Patrol officer [BYTES seg14:0x833-0x884]
                    if (n < kShipCount) {
                        const int s = pick_design(w, h, 2, n);
                        rw.enforcer_shp[2] = rw.enforcer_shp[3] = static_cast<std::int16_t>(s);
                        w.ship(n).roster_index = -1;
                        set_byte_at(w.ship(n), 0x5e, 3);
                        w.w992 = add16(w.w992, 40);
                        ++n;
                    } else {
                        static bool once = false;
                        guard_todo(once, "the Patrol slot past ship 7 is skipped (verify O13)", "seg14:0x859");
                    }
                }
                if (mode == 0x15) {  // an Interceptor [BYTES seg14:0x887-0x8b5]
                    if (n < kShipCount) {
                        const int s = pick_design(w, h, 3, n);
                        rw.enforcer_shp[1] = static_cast<std::int16_t>(s);
                        w.ship(n).roster_index = -1;
                        set_byte_at(w.ship(n), 0x5e, 1);
                        w.w992 = add16(w.w992, 50);
                        ++n;
                    } else {
                        static bool once = false;
                        guard_todo(once, "the Interceptor slot past ship 7 is skipped (verify O13)", "seg14:0x88d");
                    }
                }
            }
            if (mode == 0x13 && n < kShipCount) {  // Demon's Gate's own ship [BYTES seg14:0x8b8-0x8e1]
                const int s = pick_design(w, h, 0, n);
                w.w95b2 = -3000;
                roster_take(w, h, n, s, 0);
                ++n;
            }
        }
    }
    // ---- the tail [BYTES seg14:0x8e4-0xa1d]: the slot cache
    if (mode == -1 && w.restored == 0) {
        for (int j = 0; j <= old_last; ++j) {  // [BYTES seg14:0x8f5-0x963]
            const auto jj = static_cast<std::size_t>(j);
            if (oldR[jj] != -1)
                for (int m = 0; m < n; ++m)
                    if (w.ship(m).roster_index == oldR[jj]) kept[jj] = 1;
            if (kept[jj] == 0) {
                if (DesignRating* d = design_at(w, oldD[jj])) d->w4e = 0;  // DGROUP:0x9720 [BYTES seg14:0x92a-0x930]
                h.slot_free(j);                                               // seg10:0x1eb6(j, oldD[j], 1)
            }
        }
        for (int k = 0; k < n; ++k) {  // [BYTES seg14:0x965-0x9fc]
            const auto kk = static_cast<std::size_t>(k);
            fresh[kk] = 1;
            for (int m = 0; m <= old_last; ++m) {
                const auto mm = static_cast<std::size_t>(m);
                if (oldR[mm] != w.ship(k).roster_index || w.ship(k).roster_index == -1) continue;
                fresh[kk] = 0;
                if (m != k) {
                    h.set_slot_words(k, saved[mm]);
                    if (saved[mm].dc == h.slot_words(m).dc) h.set_slot_words(m, SlotWords{});  // [BYTES seg14:0x9cb-0x9df]
                }
                break;
            }
        }
        for (int k = n; k < 8; ++k) h.slot_free(k);  // seg10:0x1eb6(k, -1, 0) [BYTES seg14:0x9ff-0xa1d]
    }
    // ---- the counts [BYTES seg14:0xa1f-0xa84]
    w.last_ship = static_cast<std::int16_t>(n - 1);
    w.pools.resize(w.roles.ships, static_cast<std::int16_t>(n));
    w.pools.set_in_use(w.roles.ships, static_cast<std::int16_t>(n));
    out.count = n;
    if (mode == -1) {
        // ---- the design indices and seg14:0x1430 per ship [BYTES seg14:0xa90-0xb5e]
        for (int k = 0; k < n; ++k) {
            ShipRecord& K = w.ship(k);
            if (fresh[static_cast<std::size_t>(k)] == 0) {
                if (k != w.player) {
                    for (int m = 0; m <= last_saved; ++m)
                        if (oldR[static_cast<std::size_t>(m)] == K.roster_index) {
                            K.design_index = oldD[static_cast<std::size_t>(m)];
                            break;
                        }
                }
            } else {
                for (std::int16_t d = 0; d < 10; ++d) {
                    bool free = true;
                    for (int m = 0; m <= last_saved; ++m)
                        if (oldD[static_cast<std::size_t>(m)] == d && kept[static_cast<std::size_t>(m)] != 0) free = false;
                    for (int m = 0; m < k; ++m)
                        if (w.ship(m).design_index == d) free = false;
                    if (free) {
                        K.design_index = d;
                        break;
                    }
                }
            }
            ship_place(w, h, k);
        }
        rw.first = 0;
    } else {
        const int L = w.last_ship;  // [BYTES seg14:0xb66-0xb7a]
        if (ship_ok(L)) {
            w.ship(L).design_index = static_cast<std::int16_t>(L);
            ship_place(w, h, L);
        }
        rw.first = static_cast<std::int16_t>(L);
    }
    // ---- the per-ship loop [BYTES seg14:0xb7d-0xcf9 seg14:0xd03-0xd9d]
    for (int i = rw.first; i <= w.last_ship && i < kShipCount; ++i) {
        w.ship(i).sprite_slot = static_cast<std::int16_t>(i);       // +0x50 [BYTES seg14:0xd0a]
        if (DesignRating* d = design_at(w, i)) d->w3a = static_cast<std::int16_t>(i);  // design i +0x3a [BYTES seg14:0xd12]
        if (fresh[static_cast<std::size_t>(i)] != 0 || w.wave == 1 || mode != -1)       // [BYTES seg14:0xd1a-0xd2b]
            h.purchases(w, i);                                       // seg16:0x3784(i, 1)
        h.ship_tail(w, i);
    }
    if (mode == -1) h.weapon_images();  // [BYTES seg14:0xda0-0xdbb]
    w.w9346 = -1;                       // [BYTES seg14:0xdbd]
    rw.first = 0;                       // [BYTES seg14:0xdc5]
    w.w994 = 0;                         // [BYTES seg14:0xdc8]
    return out;
}

}  // namespace is::world
