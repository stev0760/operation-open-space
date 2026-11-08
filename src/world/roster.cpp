// The ship between waves (roster.h). Every site cites the listing it ports; the claims are
// work/analysis/m4/s3-roster.md's with s3-roster-verify.md's verdicts applied. Portable.
#include "world/roster.h"

#include "world/purchases.h"

#include <algorithm>
#include <cstring>

#include "formats/ships.h"
#include "shell/rng.h"
#include "world/collide.h"
#include "world/player_sheet.h"

namespace is::world {

namespace {

RosterTodo& todo_fn() {
    static RosterTodo f;
    return f;
}
void todo(const std::string& what, const char* address) {
    if (todo_fn()) todo_fn()(what, address);
}
// A fallback's one-time log: `once` is the site's own flag [DESIGN ADR 0014 decision 11].
void fallback_once(bool& once, const char* what, const char* address) {
    if (once) return;
    once = true;
    todo(what, address);
}

PurchasePass& purchase_fn() {
    static PurchasePass f;
    return f;
}

std::int8_t s8(std::uint8_t b) { return static_cast<std::int8_t>(b); }
std::uint8_t lo8(int v) { return static_cast<std::uint8_t>(static_cast<unsigned>(v) & 0xffu); }

// The two class tables (claim 22): words DGROUP:0x952 + 2k and DGROUP:0x96c + 2k for k = 1 ... 10
// [BYTES DGROUP:0x952 DGROUP:0x96c].
constexpr std::int16_t kClassLo[11] = {0, 0, 28, 34, 40, 48, 55, 63, 70, 80, 97};
constexpr std::int16_t kClassHi[11] = {0, 28, 34, 40, 48, 55, 63, 70, 80, 97, 114};

// seg14:0x455c's reject list, the subtraction chain's values (claim 33) [BYTES seg14:0x4589-0x45f0].
constexpr std::uint16_t kRejected[] = {178, 310, 320, 400, 440, 450, 520, 550, 560, 590, 600,
                                       604, 605, 710, 720, 730, 750, 890, 900, 930, 950};

// The design tank of seg14:0x1b84: design +0x1e = 40 x the signed SHP byte +0x00 (`imul byte`)
// [BYTES seg14:0x1d1a seg14:0x1d1c seg14:0x1d1f].
std::int16_t tank_of(const fmt::ShipRecord& shp) { return mul16(40, s8(shp.fuel_tank)); }

// The design tank of ship i's design (rating.h DesignRating::tank, DGROUP:0x96f0 + 0xad·n).
std::int16_t design_tank(const World& w, int i) {
    const int n = w.ships[static_cast<std::size_t>(i)].design_index;
    return n >= 0 && n < kShipCount ? w.design_ratings[static_cast<std::size_t>(n)].tank : std::int16_t{0};
}
std::int16_t design_special(const World& w, int i) {
    const int n = w.ships[static_cast<std::size_t>(i)].design_index;
    return n >= 0 && n < kShipCount ? w.design_ratings[static_cast<std::size_t>(n)].special : std::int16_t{-1};
}

bool ship_ok(const World&, int i) { return i >= 0 && i < kShipCount; }
bool object_ok(const World& w, int i) { return ship_ok(w, i) && i < w.pools.records(w.roles.ships); }

// seg16:0x3468(i, 3)'s path seg16:0x35db (DGROUP:0x9563 = 0), as purchases.cpp repair_system
// runs it (work/analysis/m4/s5-sheet.md 5.2, s5-sheet-verify.md claims 37, 38) [BYTES
// seg16:0x35db-0x3696]: ship words +0x20 = object size - 2 and +0x22 = 1, byte +0xac = 0 so
// seg14:0x11a8 renders the sheet again with its wings (player_sheet.h build_sheet_program),
// object +0x3a = the sheet DC, the wing bytes back in a restored game, the roster bytes +0x72 /
// +0x73. Object +0x44 = design +0x32 [BYTES seg16:0x3601-0x360b]: the ship set-up stored the
// same value just before this fallback (hooks.cpp apply_flight_design), so it stays [DESIGN].
// The early return for k = 3 [BYTES seg16:0x3477-0x34a9]: a design without wings (row 3 = -1)
// whose sheet DC exists keeps its sheet (verified claim 37). The new-body path seg16:0x3547 is
// the visit's [OPEN here].
void wing_rebuild(World& w, int i) {
    if (shop().new_body != 0) {
        todo("seg16:0x3468(i, 3)'s new body in the placement's fallback", "seg16:0x3547");
        return;
    }
    if (design_special(w, i) == -1 && sheet_dc_program(i)) return;  // [BYTES seg16:0x3477-0x34a9]
    if (!object_ok(w, i)) return;
    ShipRecord& S = w.ship(i);
    ObjectRecord& o = w.ship_object(i);
    S.w20 = sub16(o.size, 2);  // [BYTES seg16:0x35ec-0x35f7]
    S.w22 = 1;                 // [BYTES seg16:0x35fb]
    const std::int8_t ac = static_cast<std::int8_t>(sbyte_at(S, 0xac));  // [BYTES seg16:0x3611-0x3619]
    const std::int8_t ad = static_cast<std::int8_t>(sbyte_at(S, 0xad));
    set_byte_at(S, 0xac, 0);          // [BYTES seg16:0x3621]
    build_sheet_program(w, i);        // seg14:0x11a8 [BYTES seg16:0x362b]
    o.w3a = sheet_dc_word_program(i);  // [BYTES seg16:0x3631-0x3645]
    if (w.restored != 0) {            // DGROUP:0x92e4 [BYTES seg16:0x3649-0x365a]
        set_byte_at(S, 0xac, ac);
        set_byte_at(S, 0xad, ad);
    }
    if (S.roster_index >= 0 && static_cast<std::size_t>(S.roster_index) < w.roster.size()) {  // [BYTES seg16:0x365e-0x3692]
        RosterRecord& r = w.roster[static_cast<std::size_t>(S.roster_index)];
        r.wing0 = static_cast<std::uint8_t>(sbyte_at(S, 0xac));
        r.wing1 = static_cast<std::uint8_t>(sbyte_at(S, 0xad));
    }
}

// seg16:0x3468(i, k) past its early return (claims 13, 14) [BYTES seg16:0x34b7-0x3723].
void repair_fallback(World& w, int i, int k) {
    ShipRecord& S = w.ship(i);
    if (k == 0 && S.w26 < 40) {
        // rating = rating x (system 0 + 200) / 300, 32-bit signed [BYTES seg16:0x34c3-0x34e0]
        const std::int64_t r = static_cast<std::int32_t>(w.rating);
        w.rating = static_cast<std::uint32_t>(static_cast<std::int32_t>(
            static_cast<std::int32_t>(r * add16(S.w26, 200)) / 300));
    }
    if (w.restored == 0) S.roster_word(k) = 100;  // [BYTES seg16:0x34f8 seg16:0x34ff]
    S.bec = 1;                                    // [BYTES seg16:0x350a]
    if (k == 1) S.d8c.set(2);                     // [BYTES seg16:0x3520]
    if (k == 3) wing_rebuild(w, i);               // [BYTES seg16:0x354c seg16:0x35db]
    // [BYTES seg16:0x3529-0x353f seg16:0x3699-0x370b]
    S.w24 = S.w0a;
    const bool wings = design_special(w, i) == 3;
    const std::uint8_t ac = static_cast<std::uint8_t>(sbyte_at(S, 0xac));
    const std::uint8_t ad = static_cast<std::uint8_t>(sbyte_at(S, 0xad));
    if (ac != 0)
        set_byte_at(S, 0xe0, 1);
    else if (wings)
        set_byte_at(S, 0xe0, 0);
    if (ad != 0)
        set_byte_at(S, 0xe1, 1);
    else if (wings)
        set_byte_at(S, 0xe1, 0);
    if (S.w0a == 1 || (ac == 0 && ad == 0 && wings)) {
        set_byte_at(S, 0xe0, 1);
        S.w24 = 1;
    }
    rate_ship(w, i);  // seg13:0x33d6(i) [BYTES seg16:0x3714]
}

}  // namespace

void set_roster_todo(RosterTodo f) { todo_fn() = std::move(f); }

// ---------------------------------------------------------------- the block

int roster_last(const World& w) { return static_cast<int>(w.roster.size()) - 1; }

RosterRecord* roster_at(World& w, int r) {
    if (r < 0 || r >= static_cast<int>(w.roster.size())) return nullptr;
    return &w.roster[static_cast<std::size_t>(r)];
}

std::uint8_t& roster_byte(RosterRecord& r, int off) {
    static std::uint8_t sink = 0;  // an offset outside the record [DESIGN]
    if (off < 0 || off >= static_cast<int>(kRosterRecordSize)) return sink = 0;
    return reinterpret_cast<std::uint8_t*>(&r)[off];
}

void roster_to_bytes(const RosterRecord& r, std::uint8_t* out) {
    std::memcpy(out, &r, kRosterRecordSize);  // records.h: no padding, the original's offsets
    auto put = [&](std::size_t off, std::int16_t v) {
        out[off] = static_cast<std::uint8_t>(static_cast<std::uint16_t>(v) & 0xff);
        out[off + 1] = static_cast<std::uint8_t>(static_cast<std::uint16_t>(v) >> 8);
    };
    put(0x00, r.shp);
    put(0x02, r.credit10);
    put(0x04, r.resources);
    put(0x06, r.w06);
    for (std::size_t a = 0; a < 5; ++a) put(0x08 + 2 * a, r.rel_w[a]);
}

RosterRecord roster_from_bytes(const std::uint8_t* p) {
    RosterRecord r;
    std::memcpy(&r, p, kRosterRecordSize);
    auto get = [&](std::size_t off) {
        return static_cast<std::int16_t>(static_cast<std::uint16_t>(p[off] | (p[off + 1] << 8)));
    };
    r.shp = get(0x00);
    r.credit10 = get(0x02);
    r.resources = get(0x04);
    r.w06 = get(0x06);
    for (std::size_t a = 0; a < 5; ++a) r.rel_w[a] = get(0x08 + 2 * a);
    return r;
}

std::int32_t ship_credit(const ShipRecord& s) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::uint16_t>(s.wdc)) |
                                     (static_cast<std::uint32_t>(static_cast<std::uint16_t>(s.wde)) << 16));
}
void set_ship_credit(ShipRecord& s, std::int32_t v) {
    s.wdc = to_i16(static_cast<std::uint32_t>(v));
    s.wde = to_i16(static_cast<std::uint32_t>(v) >> 16);
}

// ---------------------------------------------------------------- MainShip

bool ship_accepted(const World& w, const fmt::MasterShp& ships, const fmt::MasterBod& bodies, int v) {
    if (w.w11aa != 0) return true;  // [BYTES seg14:0x4567 seg14:0x456c]
    if (v < 0 || v >= static_cast<int>(ships.records.size())) {  // [DESIGN ADR 0014 decision 11]
        static bool once = false;
        fallback_once(once, "an SHP index outside MASTER.SHP is accepted", "seg14:0x455c");
        return true;
    }
    const std::uint16_t body = ships.records[static_cast<std::size_t>(v)].body;  // SHP +0x21
    if (body >= bodies.records.size()) {  // [DESIGN ADR 0014 decision 11] (roster.h)
        static bool once = false;
        fallback_once(once, "a body index outside MASTER.BOD is accepted", "seg14:0x4574");
        return true;
    }
    // MASTER.BOD word +0x21 of that body (record 0x2c bytes at far DGROUP:0x6e1c) [BYTES
    // seg14:0x4574-0x4585].
    const std::uint16_t b = bodies.records[body].bitmap_id;
    for (std::uint16_t x : kRejected)
        if (b == x) return false;  // [BYTES seg14:0x45f0]
    return true;                   // [BYTES seg14:0x45f4]
}

MainShipPick pick_main_ship(World& w, const fmt::MasterShp& ships, const fmt::MasterBod& bodies,
                            const std::string& main_ship, shell::Rng& rng) {
    MainShipPick p;
    const int count = static_cast<int>(ships.records.size());  // DGROUP:0x95aa
    // rand() % (DGROUP:0x95aa - 1) into [bp-6], overwritten later: only the call counts
    // [BYTES seg14:0x2310 seg14:0x231b seg14:0x231d seg14:0x2435]. A count of 1 divides by 0 in
    // the original; the port consumes the rand() alone [DESIGN ADR 0014 decision 11].
    (void)rng.rand();
    if (count == 1) {
        static bool once = false;
        fallback_once(once, "a one-record MASTER.SHP: the rand() % 0 dropped", "seg14:0x231d");
    }
    std::int16_t best = -1000;  // [BYTES seg14:0x2325]
    if (w.player_roster > roster_last(w)) w.player_roster = 0;  // signed `jle` [BYTES seg14:0x232a-0x2333]
    // Kind 6 / 7 with DGROUP:0x90f8 and DGROUP:0x8e5e set read `Ship1` / `Ship2` and take the
    // race path seg14:0x2451-0x24df [OPEN M6] [BYTES seg14:0x2355-0x239c].
    p.name = main_ship;
    if (w.w9516 == 0) {
        // The validation pass [BYTES seg14:0x23a1-0x2425] (claim 29).
        for (int v = 0; v < count; ++v) {
            if (ships.records[static_cast<std::size_t>(v)].name == p.name && !ship_accepted(w, ships, bodies, v)) {
                p.name = "TRANSFORMER";  // [BYTES seg14:0x23f9]
                p.rewrite = true;        // `MainShip=TRANSFORMER` [BYTES seg14:0x240a seg14:0x2420]
            }
        }
    } else {
        todo("seg14:0x31d0(0, buffer): the demo's ship name", "seg14:0x2427");
    }
    // The selection pass for kind 0, slot 0 = DGROUP:0x9646 (claims 30, 31 within their scope)
    // [BYTES seg14:0x2441-0x2520 seg14:0x3173 seg14:0x317c].
    int chosen = -1;
    for (int v = 0; v < count; ++v) {
        const fmt::ShipRecord& r = ships.records[static_cast<std::size_t>(v)];
        if (r.name.empty() || !ship_accepted(w, ships, bodies, v)) continue;  // -1000, skipped
        if (r.name == p.name) {
            best = 20000;  // [BYTES seg14:0x2504]
            chosen = v;
            break;
        }
    }
    w.w95b4 = best;  // [BYTES seg14:0x317f-0x3188]
    if (best <= -1000) {
        chosen = 1;  // [BYTES seg14:0x318b seg14:0x318e seg14:0x3190]
        if (count < 2) {
            // ADR 0014 decision 7: index 1 lies outside a MASTER.SHP of fewer than two records;
            // the port keeps index 0 [DESIGN].
            chosen = count > 0 ? 0 : -1;
            todo("MainShip \"" + p.name + "\" names no record and MASTER.SHP has " + std::to_string(count) +
                     " record(s): index " + std::to_string(chosen) + " [DESIGN]",
                 "seg14:0x3190");
        }
    }
    p.index = chosen;
    return p;
}

// ---------------------------------------------------------------- the placement

std::uint8_t remap_weapon(const World& w, std::uint8_t wpn) {
    if (w.w11aa != 0) return wpn;  // [BYTES seg14:0x33a seg14:0x16e5]
    if (weapon_word(0, 0) == 0) {
        // The weapon table is not filled yet (seg10:0x1fb2 at the state-1 entry)
        // [DESIGN ADR 0014 decision 11].
        todo("the weapon remap before the weapon table DGROUP:0x2f54 is filled", "seg14:0x16e5");
        return wpn;
    }
    const std::int16_t m = weapon_word(s8(wpn), 0x36);  // DGROUP:0x2f8a + 0x50·cbw(w)
    return m != -1 ? lo8(m) : wpn;  // [BYTES seg14:0x34d seg14:0x354 seg14:0x170a]
}

RosterRecord fresh_record(const World& w, const fmt::ShipRecord& shp, int index) {
    RosterRecord r;                                   // GlobalAlloc(0x42, 0x7c) (claim 1)
    r.shp = static_cast<std::int16_t>(index);         // [BYTES seg14:0x2b0]
    r.relation.fill(0);                               // +0x3c ... +0x63 [BYTES seg14:0x2c3]
    r.systems.fill(100);                              // +0x38 ... +0x3b [BYTES seg14:0x2e1]
    for (std::size_t a = 0; a < 5; ++a) {             // [BYTES seg14:0x2fb seg14:0x306 seg14:0x313]
        r.rel_a[a] = 0;
        r.rel_w[a] = 0;
        r.rel_r[a] = 0xff;
    }
    if (shp.weapon != 0) {  // claim 2 [BYTES seg14:0x32a]
        r.slots = 2;        // [BYTES seg14:0x335]
        r.weapon[1] = remap_weapon(w, shp.weapon);  // +0x29 [BYTES seg14:0x33a-0x36b]
        r.count[1] = 20;    // +0x1f [BYTES seg14:0x373]
    } else {
        r.slots = 1;        // [BYTES seg14:0x37e]
    }
    r.w06 = 0;              // [BYTES seg14:0x387]
    r.wing1 = 1;            // [BYTES seg14:0x391]
    r.wing0 = 1;
    r.barrels = 1;          // [BYTES seg14:0x39e]
    r.b75 = 1;              // [BYTES seg14:0x3a6]
    r.fuel40 = shp.fuel_tank;  // [BYTES seg14:0x3af]
    r.credit10 = 15;        // [BYTES seg14:0x3cc]
    r.resources = 0;        // [BYTES seg14:0x3d8]
    r.team = s8(shp.team);  // DGROUP:0x970a = cbw(SHP +0x03), +0x64 its low byte [BYTES seg14:0x3e9 seg14:0x3f3]
    return r;
}

void roster_pilot_name(RosterRecord& r, const std::string& pilot) {
    r.name.fill(0);
    const std::size_t n = std::min(pilot.size(), r.name.size() - 1);
    for (std::size_t i = 0; i < n && pilot[i] != '\0'; ++i) r.name[i] = pilot[i];
}

RosterPlacement roster_placement(World& w, const fmt::MasterShp& ships, const fmt::MasterBod& bodies,
                                 const std::string& main_ship, shell::Rng& rng, const std::string& pilot) {
    RosterPlacement out;
    ShipRecord& s0 = w.ship(0);
    if (w.wave <= 1 || w.roster.empty()) {
        // seg14:0x232-0x438 [BYTES seg14:0x232]. A later wave without a block cannot happen in
        // the original; the port treats it as the fresh path [DESIGN ADR 0014 decision 11].
        if (w.wave > 1) {
            static bool once = false;
            fallback_once(once, "a later wave without a roster block takes the fresh path", "seg14:0x232");
        }
        out.fresh_roster = true;
        w.player_roster = 0;  // DGROUP:0x931c [BYTES seg14:0x23e seg14:0x240]
        w.player = 0;         // DGROUP:0x9646 [BYTES seg14:0x243]
        out.pick = pick_main_ship(w, ships, bodies, main_ship, rng);  // [BYTES seg14:0x248]
        // GlobalAlloc(0x42, 0x7c): the old block is overwritten without GlobalFree (a leak);
        // the port replaces the block [DESIGN] [BYTES seg14:0x252-0x26c]. DGROUP:0x931a = 0.
        w.roster.assign(1, RosterRecord{});
        if (w.player_game != 0 && w.kind != 6 && w.kind != 7) w.opponents = 0;  // DGROUP:0x9338 [BYTES seg14:0x28d-0x2a2]
        if (out.pick.index >= 0)
            w.roster[0] = fresh_record(w, ships.records[static_cast<std::size_t>(out.pick.index)], out.pick.index);
        w.ba14d.fill(0);  // [BYTES seg14:0x3fc-0x410]
        w.ba157.fill(0);
        roster_pilot_name(w.roster[0], pilot);  // seg9:0x5be0(DGROUP:0x9646, 0) [BYTES seg14:0x418] (M4 s4 fixer B)
        out.fresh_ship = true;  // [bp-0x54] for slot 0 [BYTES seg14:0x425]
        s0.roster_index = 0;    // [BYTES seg14:0x42c-0x435]
        s0.design_index = 0;
        w.w1318 = 0;
    } else {
        // seg14:0x43a-0x47e [BYTES seg14:0x43a-0x450]
        s0.design_index = w.ship(ship_ok(w, w.player) ? w.player : 0).design_index;
        w.player = 0;
        if (!roster_at(w, w.player_roster)) {
            todo("DGROUP:0x931c = " + std::to_string(w.player_roster) +
                     " outside the roster: record 0 [DESIGN ADR 0014 decision 11]",
                 "seg14:0x44d");
            w.player_roster = 0;
        }
        s0.roster_index = w.player_roster;
        if (w.restored == 0) {
            out.fresh_ship = false;  // [BYTES seg14:0x453-0x45f]
        } else {
            s0.design_index = 0;     // the first wave after a restore [BYTES seg14:0x466]
        }
        // The design team word DGROUP:0x970a + 0xad·n = cbw(roster +0x64): the port has no
        // design table; seg14:0x1430 writes the same word [BYTES seg14:0x46c-0x47e].
    }
    // seg14:0x1430's s = roster +0x00, with `cmp; jge` against the SHP count - 1: the record and s
    // become 0 [BYTES seg14:0x1457 seg14:0x1471-0x148e].
    RosterRecord* r = roster_at(w, s0.roster_index);
    const int count = static_cast<int>(ships.records.size());
    if (r) {
        if (r->shp > count - 1) r->shp = 0;
        out.shp_index = r->shp;
    }
    // A negative index reads record 0 [DESIGN ADR 0014 decision 11]; an empty MASTER.SHP has none.
    if (out.shp_index < 0 || out.shp_index >= count) {
        if (count > 0) {
            static bool once = false;
            fallback_once(once, "a negative SHP index reads record 0", "seg14:0x1457");
        }
        out.shp_index = count > 0 ? 0 : -1;
    }
    return out;
}

void ship_from_roster(World& w, int i, const fmt::ShipRecord& shp) {
    if (!ship_ok(w, i)) return;
    ShipRecord& S = w.ship(i);
    RosterRecord* r = roster_at(w, S.roster_index);
    if (!r && S.roster_index != -1) {
        todo("ship " + std::to_string(i) + "'s +0xd6 = " + std::to_string(S.roster_index) +
                 " outside the roster: the no-roster path [DESIGN ADR 0014 decision 11]",
             "seg14:0x16c5");
    }
    const std::int16_t tank = tank_of(shp);
    // The team word design +0x38 (DGROUP:0x970a + 0xad·n): no design table in the port
    // [BYTES seg14:0x14ad-0x14ca]. Ship byte +0x147 = 0 [BYTES seg14:0x14d3].
    S.radar_hide = 0;
    // The table DGROUP:0x7186 + 0x15a·i + 9j + k = 0, k = 0 ... 8, j = 0 ... DGROUP:0x9316: ship
    // bytes +0xf6 + 9j + k (s3-roster.md writes +0x96 for the same address), which the
    // debriefing's ranking sums (structs.md +0xf6, C082) [BYTES seg14:0x150b seg9:0x8227]
    // [OPEN: the field's meaning].
    for (int j = 0; j <= w.last_ship && j < kShipCount; ++j)
        for (int k = 0; k < 9; ++k) set_byte_at(S, 0xf6 + 9 * j + k, 0);
    set_word_at(S, 0x04, i + 0x28);  // [BYTES seg14:0x152d]
    S.weapon[0] = 0;                 // [BYTES seg14:0x1531]
    S.weapon_count[0] = 1;           // [BYTES seg14:0x1536]
    if (r) S.slots_cb() = r->slots;  // [BYTES seg14:0x1551]
    w.d8e08[static_cast<std::size_t>(i)] = 1000000u;  // [BYTES seg14:0x155b]
    for (int k = s8(S.slots_cb()); k < 10; ++k)        // [BYTES seg14:0x1564-0x1587]
        if (k >= 0) S.weapon[static_cast<std::size_t>(k)] = 0;
    // Bytes +0x5d / +0x5e = design bytes +0xa4 / +0xa5, the SHP attitudes +0x04 / +0x05
    // [BYTES seg14:0x1596 seg14:0x159f seg14:0x2040].
    set_byte_at(S, 0x5d, shp.to_friends);
    set_byte_at(S, 0x5e, shp.to_enemies);
    // [BYTES seg14:0x15b4-0x161b]
    S.w06 = 8;
    S.w32 = 8;
    S.w08 = 0;
    S.w34 = 0;
    S.w38 = 3;
    S.w3c = 2;
    set_byte_at(S, 0x149, 0xff);
    S.checkpoint = 0xff;  // +0x148
    S.d94.set(0);
    S.bea = 0;
    if (object_ok(w, i)) {
        ObjectRecord& o = w.ship_object(i);
        o.frame = 0;        // object word +0x28
        o.race_status = 0;  // object byte +0x51
    }
    S.type = 0;       // +0x10
    S.thrusting = 0;  // +0x02
    S.be8 = 0;        // +0xe8 `cloaked`
    // +0x52 = 7 for the player with PlayMode != 0, else 0 [BYTES seg14:0x162e seg14:0x163b]; a
    // team-7 design with +0x5e > 1 and the kind-7 block are M5 [OPEN M5] [BYTES seg14:0x1657-0x16bd].
    S.w52 = (i == w.player && w.play_mode != 0) ? 7 : 0;

    if (!r) {
        // seg14:0x1a5d-0x1b78, the computer ships' path (M5): what the analysis read
        // (s3-roster.md section 1, the verifier's "Outside the Claims").
        S.slots_cb() = 3;                          // [BYTES seg14:0x1a62]
        S.weapon[1] = 15;                          // [BYTES seg14:0x1a67]
        S.weapon_count[1] = 1;                     // [BYTES seg14:0x1a6c]
        S.weapon[2] = shp.weapon;                  // [BYTES seg14:0x1a81]
        S.weapon_count[2] = 20;                    // [BYTES seg14:0x1a85]
        if (sbyte_at(S, 0x5e) == 0)
            todo("the no-roster ship's +0x52 = 0x17 target block (seg14:0x4520)", "seg14:0x1a91");  // [OPEN M5]
        for (int k = 0; k < 4; ++k) S.roster_word(k) = 100;  // [BYTES seg14:0x1ae4]
        for (int k = 0; k < 5; ++k) {                        // [BYTES seg14:0x1b00 seg14:0x1b0e]
            S.bee[static_cast<std::size_t>(k)] = 0;
            S.w72[static_cast<std::size_t>(k)] = 0;
        }
        S.fuel = mul16(40, s8(shp.fuel_tank));  // signed `imul byte` [BYTES seg14:0x1b27]
        set_byte_at(S, 0xac, 1);                // [BYTES seg14:0x1b2f]
        set_byte_at(S, 0xad, 1);
        S.w24 = 2;                              // [BYTES seg14:0x1b35]
        S.w0a = 2;                              // [BYTES seg14:0x1b39]
        S.set_resources(100);                   // [BYTES seg14:0x1b40]
        set_ship_credit(S, 0);                  // [BYTES seg14:0x1b44 seg14:0x1b48]
        S.w82 = 0;                              // [BYTES seg14:0x1b51]
        for (int j = 0; j < w.last_ship && j < 8; ++j) S.r62[static_cast<std::size_t>(j)] = 0;  // [BYTES seg14:0x1b69]
        return;
    }

    // ---- the roster path [BYTES seg14:0x16c5]
    // Weapons (claim 5 as corrected) [BYTES seg14:0x16da seg14:0x16e5-0x170a seg14:0x172f seg14:0x173b].
    S.slots_cb() = r->slots;
    for (int k = 1; k < s8(r->slots); ++k) {
        if (k >= 10) break;  // the slots past +0xc1 [DESIGN]
        const auto kk = static_cast<std::size_t>(k);
        r->weapon[kk] = remap_weapon(w, r->weapon[kk]);  // written back into the roster
        S.weapon[kk] = r->weapon[kk];
        S.weapon_count[kk] = r->count[kk];
    }
    // DGROUP:0x104e != 0: seg12:0x6d64(i, 13) (world/weapon_load.h) [BYTES seg14:0x1758-0x1764];
    // the port keeps no DGROUP:0x104e (the restore clears it, formats.md "Reading" step 9), so the
    // call never runs here [OPEN: DGROUP:0x104e's other writers].
    // The systems, sign-extended (claim 3) [BYTES seg14:0x1775-0x179b].
    for (int k = 0; k < 4; ++k) S.roster_word(k) = r->systems[static_cast<std::size_t>(k)];
    // The relation tables: cleared for k = 0 ... DGROUP:0x9316, then filled from +0x08/+0x12/
    // +0x17 and DGROUP:0x4eec's rivals [BYTES seg14:0x17ae seg14:0x17bc] [OPEN M5: the fill
    // seg14:0x180f-0x18b0].
    for (int k = 0; k <= w.last_ship && k < 8; ++k) {
        S.bee[static_cast<std::size_t>(k)] = 0;
        S.w72[static_cast<std::size_t>(k)] = 0;
    }
    if (w.last_ship > 0) todo("seg14:0x1430's relation fill from roster +0x08/+0x12/+0x17", "seg14:0x180f");
    // Fuel (claim 4): 40 x roster +0x1c as an unsigned 8-bit `mul` (0 ... 10200); a ship other
    // than DGROUP:0x9646 gets at least (2 tank) / 3; then the cap at the tank, which rewrites
    // +0x1c. No refill [BYTES seg14:0x18c9-0x1931].
    S.fuel = static_cast<std::int16_t>(40u * r->fuel40);
    if (i != w.player) {
        const std::int16_t two_thirds = idiv16(mul16(2, tank), 3);
        if (S.fuel < two_thirds) S.fuel = two_thirds;
    }
    if (tank < S.fuel) {
        S.fuel = tank;
        r->fuel40 = lo8(idiv16(tank, 40));
    }
    // The wings from the roster unless the player's ship (DGROUP:0x9646, not ship i) has no
    // flame (claim 6) [BYTES seg14:0x1935-0x1970].
    if (w.ship(ship_ok(w, w.player) ? w.player : 0).has_flame == 0) {
        set_byte_at(S, 0xac, 1);
        set_byte_at(S, 0xad, 1);
    } else {
        set_byte_at(S, 0xac, r->wing0);
        set_byte_at(S, 0xad, r->wing1);
    }
    // Claim 7 [BYTES seg14:0x1980-0x19b2].
    S.w24 = r->barrels;
    S.w0a = r->b75;
    set_ship_credit(S, 10 * static_cast<std::int32_t>(r->credit10));  // signed `imul word`
    S.set_resources(r->resources);
    S.w82 = r->w06;
    // The wave counters (claim 8) [BYTES seg14:0x19b6-0x1a1e].
    S.w00 = 0;
    if (s8(r->last_wave) == sub16(w.wave, 1))
        r->run = lo8(r->run + 1);
    else if (s8(r->run) > 1) {
        S.w00 = 4;  // seg4:0x702e's message code 0x88 (structs.md +0x00)
        r->run = 1;
    } else
        r->run = 0;
    if (s8(r->last_wave) != w.wave) r->met = lo8(r->met + 1);
    r->last_wave = lo8(w.wave);
    // Word +0x62 + 2j = 10 x the signed roster byte +0x3c + ship[j].+0xd6 for j = 0 ...
    // DGROUP:0x9316 - 1 (a ship j with -1 reads +0x3b) [BYTES seg14:0x1a2c-0x1a57].
    for (int j = 0; j < w.last_ship && j < 8; ++j)
        S.r62[static_cast<std::size_t>(j)] =
            mul16(10, s8(roster_byte(*r, 0x3c + w.ship(j).roster_index)));
}

bool purchase_due(const World& w, bool fresh, int arg) { return fresh || w.wave == 1 || arg != -1; }

bool placement_rebuilds_sheet(bool sheet_due, std::uint8_t new_body, std::int16_t special, bool slot_has_dc) {
    if (!sheet_due || new_body != 0) return false;   // [BYTES seg14:0xd1a seg16:0x3547]
    return !(special == -1 && slot_has_dc);          // [BYTES seg16:0x3477-0x34a9]
}

void set_purchase_pass(PurchasePass f) { purchase_fn() = std::move(f); }

void purchase_pass(World& w, int i, int mode) {
    if (purchase_fn()) return purchase_fn()(w, i, mode);
    if (mode != 1) {
        todo("seg16:0x3784 mode " + std::to_string(mode) + " (the VISIT track's purchases)", "seg16:0x3784");
        return;
    }
    purchase_pass_fallback(w, i);
}

void purchase_pass_fallback(World& w, int i) {
    if (!ship_ok(w, i)) return;
    ShipRecord& S = w.ship(i);
    todo("seg16:0x3784(i, 1) unbound: the placement's fallback (src/world/roster.cpp)", "seg16:0x3784");
    const std::int32_t credit = ship_credit(S);  // saved in mode != 0 [BYTES seg16:0x37da]
    // seg16:0x3b18(i, 1) [OPEN] [BYTES seg16:0x37bc].
    if (shop().basic != 0 || w.restored == 0)  // DGROUP:0x953e; claim 11 as corrected [BYTES seg16:0x3867-0x3879]
        for (int k = 0; k < 3; ++k) repair_fallback(w, i, k);
    repair_fallback(w, i, 3);  // [BYTES seg16:0x38a4-0x38d2]
    // seg16:0x3e80(i): the tank when DGROUP:0x92e4 = 0 (claim 12) [BYTES seg16:0x3e99-0x3eb5].
    if (w.restored == 0) S.fuel = design_tank(w, i);
    set_ship_credit(S, credit);  // [BYTES seg16:0x3a47-0x3a56]
    rate_ship(w, i);             // seg13:0x33d6(i) [BYTES seg16:0x3a5d]
    S.thrusting = 0;             // [BYTES seg16:0x3a6a]
    if (object_ok(w, i)) w.ship_object(i).angle = 0;  // object word +0x26 [BYTES seg16:0x3a7d]
}

void refuel_all(World& w) {
    for (int i = 0; i <= w.last_ship && i < kShipCount; ++i) w.ship(i).fuel = design_tank(w, i);  // [BYTES seg9:0x6dc5 seg9:0x6dd4]
}

// ---------------------------------------------------------------- the wave end

std::int16_t roster_class(std::int16_t v, std::int16_t prev) {
    for (int k = 1; k <= 10; ++k)  // [BYTES seg9:0x2d49-0x2d5e]
        if (kClassLo[k] <= v && v < kClassHi[k]) return static_cast<std::int16_t>(k);
    return prev;
}

void roster_from_ship(World& w, int i, RosterUpdate& u) {
    if (!ship_ok(w, i)) return;
    ShipRecord& S = w.ship(i);
    const int R = S.roster_index;
    RosterRecord* rp = roster_at(w, R);
    // The callers test +0xd6 != -1; an index the block lacks is skipped [DESIGN ADR 0014 decision 11].
    if (!rp) {
        static bool once = false;
        if (R != -1) fallback_once(once, "a ship's +0xd6 outside the roster: no write-back", "seg9:0x2b5d");
        return;
    }
    RosterRecord& r = *rp;
    // +0x3c + ship[j].+0xd6 = lo8(word +0x62 + 2j / 10), j = 0 ... DGROUP:0x9316 (i itself
    // included; -1 reaches +0x3b, which the next loop overwrites) [BYTES seg9:0x2b6a seg9:0x2b88].
    for (int j = 0; j <= w.last_ship && j < 8; ++j)
        roster_byte(r, 0x3c + w.ship(j).roster_index) = lo8(idiv16(S.r62[static_cast<std::size_t>(j)], 10));
    // The systems' low bytes, 101 as 101 (ADR 0014 decision 6) (claim 16) [BYTES seg9:0x2b97-0x2bca].
    for (int k = 0; k < 4; ++k) r.systems[static_cast<std::size_t>(k)] = static_cast<std::int8_t>(lo8(S.roster_word(k)));
    if (S.bec == 0) r.systems[0] = 0;  // [BYTES seg9:0x2bd1 seg9:0x2be1]
    // Slots 0 ... +0xcb - 1 and +0x1d (claim 17) [BYTES seg9:0x2bf4-0x2c4f].
    for (int k = 0; k < s8(S.slots_cb()) && k < 10; ++k) {
        r.weapon[static_cast<std::size_t>(k)] = S.weapon[static_cast<std::size_t>(k)];
        r.count[static_cast<std::size_t>(k)] = S.weapon_count[static_cast<std::size_t>(k)];
    }
    r.slots = S.slots_cb();
    todo("seg14:0x421c(i): the roster table +0x08/+0x12/+0x17", "seg9:0x2c5a");  // [OPEN]
    // Claim 18 [BYTES seg9:0x2c60-0x2cdb].
    r.w06 = S.w82;
    r.fuel40 = lo8(idiv16(S.fuel, 40));  // `cwd; idiv`, every time
    r.barrels = static_cast<std::int8_t>(lo8(S.w24));
    r.b75 = static_cast<std::int8_t>(lo8(S.w0a));
    r.wing0 = static_cast<std::uint8_t>(sbyte_at(S, 0xac));
    r.wing1 = static_cast<std::uint8_t>(sbyte_at(S, 0xad));
    r.resources = S.resources();
    // The credit cap, signed, stored back into the ship [BYTES seg9:0x2cdf-0x2cf6].
    std::int32_t credit = ship_credit(S);
    if (credit > 0x4fff6) {
        credit = 0x4fff6;
        set_ship_credit(S, credit);
    }
    r.credit10 = to_i16(static_cast<std::uint32_t>(credit / 10));  // seg5:0x110 [BYTES seg9:0x2d09 seg9:0x2d15]
    // v = lo16((credit x 3 + sext(+0xd8)) / 100): seg5:0x314 the 32-bit product, seg5:0x110 the
    // signed division [BYTES seg9:0x2d21 seg9:0x2d26 seg9:0x2d3a].
    const std::uint32_t c3 = static_cast<std::uint32_t>(credit) * 3u;
    const std::int32_t sum = static_cast<std::int32_t>(c3 + static_cast<std::uint32_t>(static_cast<std::int32_t>(S.kill_value)));
    const std::int16_t v = to_i16(static_cast<std::uint32_t>(sum / 100));
    u.cls = roster_class(v, u.cls);
    r.cls = lo8(u.cls);  // [BYTES seg9:0x2d63 seg9:0x2d77]
    // The current directory record's roster list +0x1c0 [BYTES seg9:0x2d83 seg9:0x2d8a
    // seg9:0x2d91 seg9:0x2dd2 seg9:0x2e54].
    DirRecord& d = w.dir();
    const std::uint8_t rb = lo8(R);
    if (S.bec == 0 || S.w82 < 0 || i == w.player) {
        for (int j = 0; j < 8; ++j)
            if (d.byte(0x1c0 + static_cast<std::size_t>(j)) == rb) d.set_byte(0x1c0 + static_cast<std::size_t>(j), 0xff);
    } else {
        bool present = false;
        for (int j = 0; j < 8; ++j) present = present || d.byte(0x1c0 + static_cast<std::size_t>(j)) == rb;
        if (!present)
            for (int j = 0; j < 8; ++j)
                if (d.byte(0x1c0 + static_cast<std::size_t>(j)) == 0xff) {
                    d.set_byte(0x1c0 + static_cast<std::size_t>(j), rb);
                    break;
                }
    }
}

void roster_from_ships_for_save(World& w) {
    if (!(w.wave > 0)) return;  // [BYTES seg6:0x99c5 seg6:0x99ca]
    RosterUpdate u;
    for (int i = 0; i <= w.last_ship && i < kShipCount; ++i)
        if (w.ship(i).roster_index != -1) roster_from_ship(w, i, u);
    if (ship_ok(w, w.player)) w.player_roster = w.ship(w.player).roster_index;  // [BYTES seg6:0x9cf9 seg6:0x9d03]
}

void roster_ratings(World& w, const fmt::MasterShp& ships) {
    // seg14:0x3eda / seg14:0x4074 bracket the reads when DGROUP:0x95a6 = 0: the port holds
    // MASTER.SHP in `ships` [DESIGN] [BYTES seg9:0x2ed6 seg9:0x2f2a].
    for (int k = 0; k <= roster_last(w); ++k) {  // `jg` exit [BYTES seg9:0x2f49-0x2f7f]
        RosterRecord& r = w.roster[static_cast<std::size_t>(k)];
        const std::int16_t c = s8(r.cls);
        const std::int16_t D = w.score_level;  // DGROUP:0x9334
        const std::int16_t x = r.w06;
        std::int16_t d;
        if (add16(c, 3) < D)  // [BYTES seg9:0x2f51 seg9:0x2f5a]
            d = idiv16(x, -3);
        else if (add16(c, 1) < D)  // `cwd; and dx, 7; add; sar 3`, then `neg` [BYTES seg9:0x2ea4-0x2eb8]
            d = sub16(0, static_cast<std::int16_t>(x / 8));
        else
            d = idiv16(x, -14);  // [BYTES seg9:0x2ec4 seg9:0x2ecc]
        const int s = r.shp;
        std::int16_t att = 0;  // an index outside `ships` reads byte 0 [DESIGN ADR 0014 decision 11]
        if (s >= 0 && s < static_cast<int>(ships.records.size())) {
            att = s8(ships.records[static_cast<std::size_t>(s)].to_enemies);
        } else {
            static bool once = false;
            fallback_once(once, "an SHP index outside MASTER.SHP reads byte 0", "seg9:0x2e97");
        }
        std::int16_t m = sub16(att, w.w9332);  // seg9:0x88ba abs [BYTES seg9:0x2efb seg9:0x2f00]
        if (m < 0) m = sub16(0, m);
        d = add16(d, idiv16(mul16(x, m), -20));  // `imul word`, cwd, idiv [BYTES seg9:0x2f07-0x2f1f]
        const std::int16_t sum = add16(x, d);
        r.w06 = sum < 0 ? std::int16_t{0} : sum;  // `jns` on the 16-bit sum [BYTES seg9:0x2f3b-0x2f42]
    }
}

// ---------------------------------------------------------------- M5

int roster_add(World& w, int slot, const fmt::ShipRecord& shp, int shp_index, int r0, shell::Rng& rng,
               const std::function<void()>& beep) {
    if (!(w.w95b4 > w.w95b2)) {  // [BYTES seg14:0xde5-0xdec]
        todo("seg14:0x10d0: a known ship's record into the directory list", "seg14:0x10d0");  // [OPEN M5]
        return r0;
    }
    int r = -1;
    if (roster_last(w) <= 38) {
        // inc DGROUP:0x931a; GlobalReAlloc(h, (931a + 1)·0x7c, 0x42): the new record is zero
        // [BYTES seg14:0xecd seg14:0xee1-0xeeb].
        w.roster.push_back(RosterRecord{});
        r = roster_last(w);
    } else {
        // The lowest signed +0x06 below 1000 among records 1 ... 39 that ships 0 ... slot - 1 do
        // not hold [BYTES seg14:0xdf9-0xe67].
        std::int16_t best = 1000;
        for (int k = 1; k <= kRosterMaxIndex && k <= roster_last(w); ++k) {
            bool held = false;
            for (int j = 0; j < slot && j < kShipCount; ++j) held = held || w.ship(j).roster_index == k;
            if (!held && w.roster[static_cast<std::size_t>(k)].w06 < best) {
                best = w.roster[static_cast<std::size_t>(k)].w06;
                r = k;
            }
        }
        if (r < 0) {  // [BYTES seg14:0xe69-0xe76]
            if (beep) beep();  // MessageBeep(0)
            r = 10;
        }
        // Every directory list byte naming r, records 0 ... DGROUP:0x94a4 - 1, back to 0xff
        // [BYTES seg14:0xe7b-0xec6].
        for (DirRecord& d : w.records)
            for (int j = 0; j < 8; ++j)
                if (d.byte(0x1c0 + static_cast<std::size_t>(j)) == lo8(r)) d.set_byte(0x1c0 + static_cast<std::size_t>(j), 0xff);
    }
    // The initialisation from seg14:0xf03, for a new or a reused record (a reused one keeps its
    // other bytes) [BYTES seg14:0xf10-0x10c5].
    RosterRecord& n = w.roster[static_cast<std::size_t>(r)];
    const RosterRecord f = fresh_record(w, shp, shp_index);
    n.shp = f.shp;
    n.relation.fill(0);
    n.systems.fill(100);
    n.rel_a = f.rel_a;  // the table cleared as in the fresh path [OPEN: the 0xff of +0x17]
    n.rel_w = f.rel_w;
    n.rel_r = f.rel_r;
    n.slots = f.slots;
    n.weapon[1] = f.weapon[1];
    n.count[1] = f.count[1];
    n.w06 = 15;  // [BYTES seg14:0x101b]
    n.wing0 = n.wing1 = 1;
    n.barrels = 1;
    n.b75 = 1;
    n.fuel40 = shp.fuel_tank;
    // rand() % ((DGROUP:0x9334 + 1) 10) + 10, then rand() % ((DGROUP:0x9334 + 1) 50) + 150,
    // signed `idiv` [BYTES seg14:0x106c-0x1083 seg14:0x1087-0x109f] (claim 37).
    const std::int16_t m2 = mul16(add16(w.score_level, 1), 10);
    const std::int16_t r2 = static_cast<std::int16_t>(rng.rand());
    n.credit10 = add16(m2 == 0 ? std::int16_t{0} : static_cast<std::int16_t>(r2 % m2), 10);
    const std::int16_t m4 = mul16(add16(w.score_level, 1), 50);
    const std::int16_t r4 = static_cast<std::int16_t>(rng.rand());
    n.resources = add16(m4 == 0 ? std::int16_t{0} : static_cast<std::int16_t>(r4 % m4), 150);
    n.met = 0;  // [BYTES seg14:0x10a7]
    n.team = f.team;
    todo("seg9:0x5be0(slot, r): the roster name +0x65", "seg14:0x10c5");
    return r;
}

}  // namespace is::world
