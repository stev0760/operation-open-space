// The saved game on the program's state (save_game.h). Portable.
#include "dialogs/save_game.h"

#include <algorithm>

#include "nav/dir_bytes.h"
#include "nav/tree.h"
#include "world/roster.h"
#include "world/world.h"

namespace is::dialogs::save {

using namespace is::fmt::save;

namespace {

std::uint8_t lo8(int v) { return static_cast<std::uint8_t>(v & 0xff); }
std::int16_t rw16(const std::uint8_t* p) { return static_cast<std::int16_t>(p[0] | (p[1] << 8)); }
void ww16(std::uint8_t* p, int v) {
    p[0] = static_cast<std::uint8_t>(v & 0xff);
    p[1] = static_cast<std::uint8_t>((static_cast<unsigned>(v) >> 8) & 0xff);
}

std::string with_slash(std::string s) {
    if (s.size() > 10) s.resize(10);  // at most 10 characters [BYTES seg6:0x978 seg6:0x9a0]
    if (!s.empty()) s += '\\';
    return s;
}

world::ShipRecord& player_ship(world::World& w) {
    const int p = w.player >= 0 && w.player < world::kShipCount ? w.player : 0;  // [DESIGN]
    return w.ship(p);
}

// seg10:0x804c(0)'s date word: tm_mday | tm_mon << 8, tm_year in the high word [BYTES
// seg10:0x805d-0x8091].
std::uint32_t pack_date(const std::tm& t) {
    return (static_cast<std::uint32_t>(t.tm_year & 0xffff) << 16) |
           static_cast<std::uint32_t>((t.tm_mday & 0xff) | ((t.tm_mon & 0xff) << 8));
}

}  // namespace

SaveGlobals& globals() {
    static SaveGlobals g;
    return g;
}

void new_game(SaveGlobals& g) { g.granularity = kNewGameGranularity; }

std::string games_dir(SaveHost& h) { return with_slash(h.ini("Games", "", 11)); }
std::string dead_dir(SaveHost& h) { return with_slash(h.ini("Dead", "", 11)); }

// ---- the directory records

void sync_to_nav(const world::World& w, nav::Records& nav) {
    const std::size_t n = std::min(w.records.size(), nav.list.size());
    for (std::size_t i = 0; i < n; ++i) {
        const nav::DirRecord d = nav::dir_from_bytes(w.records[i].b.data());
        nav::DirRecord& r = nav.list[i];
        r.destroyed = d.destroyed;
        r.captured = d.captured;
        r.state = d.state;
        if (!d.path.empty()) r.path = d.path;
    }
}

void sync_from_nav(const nav::Records& nav, world::World& w) {
    if (w.records.size() < nav.list.size()) w.records.resize(nav.list.size());
    for (std::size_t i = 0; i < nav.list.size(); ++i) {
        std::uint8_t* b = w.records[i].b.data();
        nav::dir_path_to_bytes(nav.list[i].path, b);
        if (nav.list[i].state == 2) b[nav::kDirState] = 2;  // seg6:0x59b6 in Navigation's view
    }
}

// ---- the roster block

std::vector<RosterBytes> roster_out(const world::World& w) {
    std::vector<RosterBytes> out(w.roster.size());
    for (std::size_t k = 0; k < w.roster.size(); ++k) world::roster_to_bytes(w.roster[k], out[k].data());
    return out;
}

void roster_in(world::World& w, const std::vector<RosterBytes>& r) {
    w.roster.clear();
    for (const RosterBytes& b : r) w.roster.push_back(world::roster_from_bytes(b.data()));
}

void roster_check(std::vector<RosterBytes>& r, int shp_count, const std::function<bool(int)>& ship_ok) {
    if (r.empty()) return;
    const int last = static_cast<int>(r.size()) - 1;  // DGROUP:0x931a
    for (int k = 0; k <= last; ++k) {
        std::uint8_t* b = r[static_cast<std::size_t>(k)].data();
        if (k < last && b[0x64] == 7) b[0x64] = 0;  // [BYTES seg6:0x986d seg6:0x9874]
        const std::int16_t shp = rw16(b);
        // Signed compare with DGROUP:0x95aa (jge) [BYTES seg6:0x9881-0x9887]; the last record
        // gets this bounds part only (ADR 0014 decision 7) [DESIGN].
        if (shp >= shp_count) {
            ww16(b, 1);
        } else if (k == 0 && k < last && ship_ok && !ship_ok(shp)) {
            ww16(b, 1);  // seg14:0x455c [BYTES seg6:0x9892 seg6:0x98a4]
        }
    }
}

// ---- the header

Header gather_header(Ctx& c, const std::vector<RosterBytes>& roster, int dir_count) {
    world::World& w = c.w;
    world::ShipRecord& P = player_ship(w);
    Header h;
    // +0x170 [BYTES seg6:0x8efb-0x8f2d]
    h.preview = preview_flags(P.has_flame, static_cast<std::uint8_t>(world::sbyte_at(P, 0xac)),
                              static_cast<std::uint8_t>(world::sbyte_at(P, 0xad)));
    h.size = lo8(c.s.size);                                   // +0x171 [BYTES seg6:0x8f32-0x8f38]
    h.b5d = static_cast<std::int8_t>(P.b5d);                   // +0x26 [BYTES seg6:0x8f43-0x8f48]
    h.b5e = world::sbyte_at(P, 0x5e);                          // +0x28 [BYTES seg6:0x8f52-0x8f57]
    h.wave = static_cast<std::int16_t>(c.s.wave);              // +0x24 [BYTES seg6:0x8f5e]
    h.record = w.record;                                       // +0x2a [BYTES seg6:0x8f65]
    h.player_roster = w.player_roster;                         // +0x2c [BYTES seg6:0x8f6c]
    h.w9432 = w.w9432;                                         // +0x2e [BYTES seg6:0x8f73]
    h.w964a = w.w964a;                                         // +0x30 [BYTES seg6:0x8f7a]
    h.dir_count = static_cast<std::int16_t>(dir_count);        // +0x32 [BYTES seg6:0x8f81]
    h.roster_last = static_cast<std::int16_t>(static_cast<int>(roster.size()) - 1);  // +0x34 [BYTES seg6:0x8f88]
    h.score = w.score;                                         // +0x1e [BYTES seg6:0x8f8c-0x8f97]
    h.rating = w.rating;                                       // +0x16a [BYTES seg6:0x8f9b-0x8fa7]
    h.frames = c.frames;                                       // +0x18b [BYTES seg6:0x8fac-0x8fb8]
    h.destroyed = world::word_at(P, 0xd0);                     // +0x15c [BYTES seg6:0x8fbd-0x8fc7]
    h.captured = world::word_at(P, 0xd2);                      // +0x15a [BYTES seg6:0x8fcc-0x8fd6]
    std::int32_t credit = world::ship_credit(P);               // +0x162 [BYTES seg6:0x8fdb-0x9011]
    h.credit10 = credit_word(&credit);
    world::set_ship_credit(P, credit);                         // the cap in place [BYTES seg6:0x8fff]
    h.resources = P.resources();                               // +0x164 [BYTES seg6:0x9016-0x9020]
    h.weapon_slot = lo8(P.weapon_slot);                        // +0x172 [BYTES seg6:0x902b-0x902f]
    // +0x22: roster[DGROUP:0x931c] word +0x00 [BYTES seg6:0x9034-0x9043]; an index outside the
    // block reads 0 [DESIGN].
    if (w.player_roster >= 0 && static_cast<std::size_t>(w.player_roster) < roster.size())
        h.ship = rw16(roster[static_cast<std::size_t>(w.player_roster)].data());
    h.play_mode = lo8(c.s.play_mode);                          // +0x16e [BYTES seg6:0x904e-0x9051]
    h.drive = lo8(c.s.drive);                                  // +0x16f [BYTES seg6:0x9056-0x9059]
    h.w93ac = w.w93ac;                                         // +0x166 [BYTES seg6:0x9061]
    h.w94ca = w.w94ca;                                         // +0x168 [BYTES seg6:0x9069]
    h.w9346 = lo8(w.w9346);                                    // +0x18f [BYTES seg6:0x906e-0x9071]
    h.w967c = lo8(w.w967c);                                    // +0x18a [BYTES seg6:0x9076-0x9079]
    h.a14d = w.ba14d;                                          // +0x174 [BYTES seg6:0x9086-0x9090]
    h.a157 = w.ba157;                                          // +0x17e [BYTES seg6:0x9095-0x909c]
    h.game_id = c.s.game_id;                                   // +0x00 [BYTES seg6:0x90aa-0x90b7]
    h.start_mon = static_cast<std::uint8_t>((c.s.game_date >> 8) & 0xff);   // [BYTES seg6:0x90c1]
    h.start_mday = static_cast<std::uint8_t>(c.s.game_date & 0xff);         // [BYTES seg6:0x90c8]
    h.start_year = static_cast<std::uint8_t>((c.s.game_date >> 16) & 0xff);  // [BYTES seg6:0x90cf]
    const std::tm now = c.host.now();                          // seg10:0x804c(0) [BYTES seg6:0x90d3-0x90ee]
    h.save_mon = lo8(now.tm_mon);
    h.save_mday = lo8(now.tm_mday);
    h.save_year = lo8(now.tm_year);
    // +0x160 [BYTES seg6:0x90f2-0x911b]
    std::int16_t done = 0;
    for (int k = 0; k < dir_count && static_cast<std::size_t>(k) < w.records.size(); ++k)
        if (w.records[static_cast<std::size_t>(k)].b[0x1d0] == 2) ++done;
    h.completed = done;
    h.set_pilot(c.s.pilot);                                    // +0x04 [BYTES seg6:0x912c]
    // +0x15e: the design word +0x28 of design P +0x4e [BYTES seg6:0x9131-0x91b3]; a design the
    // port does not hold reads -1 [DESIGN].
    const int d = P.design_index;
    const std::int16_t special =
        d >= 0 && d < static_cast<int>(w.design_ratings.size()) ? w.design_ratings[static_cast<std::size_t>(d)].special
                                                                : std::int16_t{-1};
    h.shape = shape_of({P.w26, P.w28, P.w2a, P.w2c}, special);
    for (std::size_t i = 0; i < 8; ++i)                        // +0x36 [BYTES seg6:0x91c7-0x91dd]
        for (std::size_t j = 0; j < 8; ++j) h.table[8 * i + j] = c.s.table[i][j];
    h.w4eec = w.w4eec;                                         // +0xb6 [BYTES seg6:0x91fb-0x9206]
    h.w4fd0 = c.g.w4fd0;                                       // +0xe6
    h.w6b10 = c.g.w6b10;                                       // +0x116
    h.w6b40 = c.g.w6b40;                                       // +0x146 [BYTES seg6:0x922b-0x9236]
    h.w945c = w.w945c;                                         // +0x156 [BYTES seg6:0x924d]
    h.w945e = w.w945e;                                         // +0x158 [BYTES seg6:0x9255]
    split_a1cd(w.wa1cd, &h.a1cd_q, &h.a1cd_r);                 // +0x190 [BYTES seg6:0x925a-0x9274]
    h.a1cf = lo8(w.wa1cf);                                     // +0x196 [BYTES seg6:0x9279-0x927c]
    h.a193 = w.ba193;                                          // +0x192 [BYTES seg6:0x9281-0x92a2]
    h.w1318 = lo8(w.w1318);                                    // +0x173 [BYTES seg6:0x92a4-0x92aa]
    return h;
}

// ---- the writer seg6:0x885c

int save_game(Ctx& c, int named, int kind_n) {
    const Kind kind = kind_n == 2 ? Kind::kDon : kind_n == 1 ? Kind::kDed : Kind::kSav;
    // 1. The score gate: a signed dword below 1000 [BYTES seg6:0x886b-0x8884].
    if (kind != Kind::kSav && static_cast<std::int32_t>(c.w.score) < 1000) {
        c.host.log("save: seg6:0x885c(" + std::to_string(named) + ", " + std::to_string(kind_n) +
                   ") refused, score below 1000");
        return 0;
    }
    // 2. The trial gate; 2324 for a manual .sav only [BYTES seg6:0x8887-0x88b2] (ADR 0014 d8).
    if (c.edition == 0 && c.player_game != 0) {
        if (named != 0 && kind == Kind::kSav) c.host.message(2324);
        c.host.log("save: the limited version writes no saved game (seg6:0x8887)");
        return 0;
    }
    const std::string dead = dead_dir(c.host);
    // 3. The purge seg6:0x7042, every kind, under LocalPath (ADR 0014 decision 1) [BYTES
    // seg6:0x7050-0x7178].
    for (const std::string& f : c.host.find(dead, "*.ded")) {
        const auto head = c.host.read(dead + f, kScoreHead);
        if (head && purge_match(*head, c.s.game_id)) {
            const bool ok = c.host.remove(dead + f);
            c.host.log(std::string("save: purge seg6:0x7042 deleted a .ded of this game") + (ok ? "" : " (failed)"));
        }
    }
    // 4. The name [BYTES seg6:0x88ee-0x89be].
    const std::string dir = kind == Kind::kSav ? games_dir(c.host) : dead;
    const std::string base = base_name(named != 0, kind, c.host.ini("Restore", "", 9), c.s.pilot, c.player_game != 0);
    std::string file = base + extension(kind);
    // 5. The free-name loop, kind != 0 [BYTES seg6:0x89cc-0x8a67].
    if (kind != Kind::kSav) {
        const std::size_t need = round_block(kHeaderSize, c.g.granularity);  // GlobalSize(0x1a9)
        file = free_name(file, base, kind, [&](const std::string& n) {
            const auto head = c.host.read(dir + n, need);
            return name_check(head ? &*head : nullptr, need, c.s.game_id);
        });
    }
    // 7. The roster (DGROUP:0x95a4) and directory (DGROUP:0x94ae) blocks must exist for every
    // kind [BYTES seg6:0x8ac8-0x8adb]; the port then leaves the old file (ADR 0014 decision 1).
    const std::vector<RosterBytes> roster = roster_out(c.w);
    sync_from_nav(c.nav, c.w);
    int dir_count = static_cast<int>(c.nav.list.size());
    if (dir_count == 0) dir_count = static_cast<int>(c.w.records.size());
    if (c.w.record >= dir_count && c.w.record < kMaxDirRecords) dir_count = c.w.record + 1;  // [DESIGN]
    if (roster.empty() || dir_count == 0) {
        c.host.log("save: no roster or no directory records, nothing written (seg6:0x8ac8)");
        return 0;
    }
    if (c.w.records.size() < static_cast<std::size_t>(dir_count)) c.w.records.resize(static_cast<std::size_t>(dir_count));
    std::vector<DirBytes> dirs;
    if (kind == Kind::kSav)
        for (int k = 0; k < dir_count; ++k) dirs.push_back(c.w.records[static_cast<std::size_t>(k)].b);
    // 6, 8. The header seg6:0x8eea and the blocks, written atomically [DESIGN ADR 0014 d1, d9].
    const Header h = gather_header(c, roster, dir_count);
    const std::vector<std::uint8_t> bytes = build(encode_header(h), roster, dirs, kind, c.g.granularity);
    const bool ok = c.host.write_atomic(dir + file, bytes);
    c.host.log("save: seg6:0x885c(" + std::to_string(named) + ", " + std::to_string(kind_n) + ") " +
               std::to_string(bytes.size()) + " bytes, granularity " + granularity_name(c.g.granularity) +
               (ok ? "" : ", write failed"));
    return ok ? 1 : 0;
}

int roster_update_and_save(Ctx& c) {
    // seg6:0x99b6 skips its body when DGROUP:0x91f8 <= 0 (in roster_from_ships_for_save), then
    // calls seg6:0x885c(1, 0) [BYTES seg6:0x99c5 seg6:0x9e51-0x9e56].
    world::roster_from_ships_for_save(c.w);
    return save_game(c, 1, 0);
}

// ---- the reader seg6:0x930a

void install(Ctx& c, Parsed& p) {
    world::World& w = c.w;
    const Header& h = p.header;
    // The blocks (decision 2: the running game's are replaced only now) [BYTES seg6:0x944b].
    roster_in(w, p.roster);
    w.records.clear();
    c.nav.list.clear();
    for (const DirBytes& d : p.dirs) {
        world::DirRecord r;
        r.b = d;
        w.records.push_back(r);
        c.nav.list.push_back(nav::dir_from_bytes(d.data()));
    }
    // Step 6, Size [BYTES seg6:0x95b4-0x95ed].
    if (sx(h.size) != c.s.size) {
        c.s.size = sx(h.size);
        w.size = static_cast<std::int16_t>(c.s.size);
        c.host.ini_set_int("Size", c.s.size);
        c.g.w92e2 = 1;
    }
    // Step 7 in the reader's order [BYTES seg6:0x95f3-0x9856].
    c.s.game_id = h.game_id;
    c.s.game_date = (static_cast<std::uint32_t>(static_cast<std::uint16_t>(sx(h.start_year))) << 16) |
                    date_word(h.start_mon, h.start_mday);
    world::set_byte_at(w.ship(0), 0x5d, h.b5d);  // ship 0, not DGROUP:0x9646 [BYTES seg6:0x962d]
    world::set_byte_at(w.ship(0), 0x5e, h.b5e);  // [BYTES seg6:0x9634]
    c.s.wave = h.wave;                            // [BYTES seg6:0x963b]
    w.wave = h.wave;
    w.record = h.record;                          // [BYTES seg6:0x9642]
    c.nav.current = h.record;
    if (w.record >= 0 && static_cast<std::size_t>(w.record) < w.records.size()) {
        world::DirRecord& r = w.records[static_cast<std::size_t>(w.record)];
        if (r.b[0xad] == 0) r.b[0xad] = 1;        // [BYTES seg6:0x9645-0x9655]
    }
    w.player_roster = h.player_roster;            // [BYTES seg6:0x9663]
    w.w9432 = h.w9432;
    w.w964a = h.w964a;
    w.score = h.score;                            // [BYTES seg6:0x9682-0x968d]
    w.rating = h.rating;                          // [BYTES seg6:0x9691-0x969e]
    w.w93ac = h.w93ac;
    w.w94ca = h.w94ca;
    w.w9346 = sx(h.w9346);                        // [BYTES seg6:0x96b2-0x96b8]
    w.w967c = sx(h.w967c);                        // [BYTES seg6:0x96bb-0x96c1]
    w.ba14d = h.a14d;                             // [BYTES seg6:0x96c9-0x96e6]
    w.ba157 = h.a157;
    w.ba161 = h.a161;                             // [BYTES seg6:0x96f7-0x9704]
    c.g.a162 = h.a162;
    player_ship(w).weapon_slot = sx(h.weapon_slot);  // DGROUP:0x9646's word +0x18 [BYTES seg6:0x9707-0x9713]
    world::set_word_at(w.ship(0), 0xd2, h.captured);   // [BYTES seg6:0x971c]
    world::set_word_at(w.ship(0), 0xd0, h.destroyed);  // [BYTES seg6:0x9724]
    c.s.play_mode = sx(h.play_mode);              // [BYTES seg6:0x9727-0x972c]
    if (c.player_game == 0) c.s.play_mode = 1;    // [BYTES seg6:0x9730-0x9737]
    w.play_mode = static_cast<std::int16_t>(c.s.play_mode);
    c.s.drive = sx(h.drive);                      // [BYTES seg6:0x9741-0x9747]
    w.drive_offset = static_cast<std::int16_t>(c.s.drive);
    c.frames = h.frames;                          // [BYTES seg6:0x974a-0x9757]
    c.s.pilot = h.pilot_name();                   // [BYTES seg6:0x9764]
    for (std::size_t i = 0; i < 8; ++i)           // [BYTES seg6:0x9777-0x978a]
        for (std::size_t j = 0; j < 8; ++j) c.s.table[i][j] = h.table[8 * i + j];
    w.w4eec = h.w4eec;                            // [BYTES seg6:0x97b1-0x97c8]
    c.g.w4fd0 = h.w4fd0;
    c.g.w6b10 = h.w6b10;
    c.g.w6b40 = h.w6b40;                          // [BYTES seg6:0x97d5-0x97e5]
    w.w945c = h.w945c;                            // [BYTES seg6:0x97fe seg6:0x9806]
    w.w945e = h.w945e;
    w.wa1cd = join_a1cd(h.a1cd_q, h.a1cd_r);      // [BYTES seg6:0x9809-0x981b]
    w.wa1cf = sx(h.a1cf);                         // [BYTES seg6:0x981f-0x9825]
    w.ba193 = h.a193;                             // [BYTES seg6:0x9828-0x984a]
    w.w1318 = sx(h.w1318);                        // [BYTES seg6:0x9850-0x9856]
    // Step 9 [BYTES seg6:0x98bb-0x98c4].
    c.s.restored = 1;
    w.restored = 1;
    c.g.w951c = c.g.w104c = c.g.w104e = 0;
    c.g.granularity = p.granularity;  // ADR 0014 decision 1: written back in the same form
}

int restore_game(Ctx& c) {
    // Step 1, before the open [BYTES seg6:0x933f-0x9357].
    c.s.game_id = c.host.time();
    c.s.game_date = pack_date(c.host.now());
    // Step 2 [BYTES seg6:0x935b-0x93d1]: no space test (claim 23).
    const std::string name = c.player_game ? c.host.ini("Restore", "", 9) : std::string("demogame");
    const std::string path = games_dir(c.host) + name + ".sav";
    const auto bytes = c.host.read(path, 0x100000);
    if (!bytes) {
        c.host.log("restore: " + path + " does not open (seg6:0x93d1)");
        return 0;
    }
    std::string why;
    auto p = parse(*bytes, Kind::kSav, &why);
    if (!p) {
        c.host.log("restore: " + path + " refused: " + why + " (ADR 0014 decisions 1, 2, 9)");
        return 0;
    }
    // Step 8, the roster check with MASTER.SHP loaded (seg14:0x3eda) [BYTES seg6:0x9859-0x98b4].
    const int shp = c.host.shp_count();
    if (shp > 0)
        roster_check(p->roster, shp, [&](int v) { return c.host.ship_ok(v); });
    else
        c.host.log("restore: MASTER.SHP not loaded, roster check skipped [DESIGN]");
    // Step 10, the old-file rule on the checked records [BYTES seg6:0x98ce-0x9913].
    if (p->header.shape < 100) {
        const int last = static_cast<int>(p->roster.size()) - 1;
        for (int k = 0; k < last; ++k) {
            std::uint8_t* b = p->roster[static_cast<std::size_t>(k)].data() + 2;
            const std::int16_t v = rw16(b);
            const std::int16_t r = v >= -15000 ? static_cast<std::int16_t>(v / 10)
                                               : static_cast<std::int16_t>(static_cast<std::uint16_t>(v) / 10u);
            ww16(b, r);
        }
    }
    install(c, *p);
    c.host.log("restore: " + path + " read, " + std::to_string(bytes->size()) + " bytes, granularity " +
               granularity_name(p->granularity) + ", wave " + std::to_string(p->header.wave));
    return 1;
}

// ---- the score list and state 5

const ScoreList& score_list(Ctx& c) {
    if (c.g.scores) return *c.g.scores;  // DGROUP:0x6af8 kept [BYTES seg6:0x658b-0x6594]
    RunningGame rg;
    rg.score = static_cast<std::int32_t>(c.w.score);
    rg.game_id = c.s.game_id;
    if (c.w.player_roster >= 0 && static_cast<std::size_t>(c.w.player_roster) < c.w.roster.size())
        rg.ship = c.w.roster[static_cast<std::size_t>(c.w.player_roster)].shp;
    rg.wave = static_cast<std::int16_t>(c.s.wave);
    rg.date = static_cast<std::uint16_t>(c.s.game_date & 0xffff);
    rg.year = static_cast<std::uint8_t>((c.s.game_date >> 16) & 0xff);
    const std::tm now = c.host.now();
    rg.now_mon = lo8(now.tm_mon);
    rg.now_mday = lo8(now.tm_mday);
    rg.now_year = lo8(now.tm_year);
    rg.pilot = c.s.pilot;
    std::vector<ScoreFile> files;
    const std::string games = games_dir(c.host), dead = dead_dir(c.host);
    const struct {
        const std::string* dir;
        const char* pattern;
    } passes[3] = {{&games, "*.sav"}, {&dead, "*.ded"}, {&dead, "*.don"}};  // DGROUP:0xf61, 0xf6d, 0xf67
    for (int k = 0; k < 3; ++k) {
        for (const std::string& n : c.host.find(*passes[k].dir, passes[k].pattern)) {
            ScoreFile f;
            const auto head = c.host.read(*passes[k].dir + n, kScoreHead);
            f.opened = head.has_value();
            if (head) f.head = *head;
            f.name = n;
            f.pass = k;
            files.push_back(f);
        }
    }
    ScoreList l = build_score_list(rg, files, c.host.shp_count(), [&](int v) { return c.host.ship_ok(v); });
    if (l.rank_set) c.g.rank = static_cast<std::int16_t>(l.rank);
    l.rank = c.g.rank;
    if (l.skipped)
        c.host.log("score list: " + std::to_string(l.skipped) + " unreadable or short file(s) skipped (ADR 0014 decision 9)");
    c.g.scores = l;
    return *c.g.scores;
}

void free_scores(SaveGlobals& g) {
    g.scores.reset();  // seg6:0x717a [BYTES seg6:0x717a-0x71b1]
    g.table.clear();   // DGROUP:0x94d8 [BYTES seg6:0x62a3]
    g.table_drawn = false;
}

std::uint32_t table_deadline(std::uint32_t now, int rank, bool from_ending) {
    if (from_ending) return now + 11000;  // 0x2af8 [BYTES seg6:0x5e49-0x5e59]
    if (rank > 10) return now;            // [BYTES seg6:0x5ee9]
    if (rank == 1) return now + 8800 + 1000;  // [BYTES seg6:0x5ef9]
    // seg5:0x314, the long multiply [BYTES seg6:0x5f0c-0x5f39]
    return now + static_cast<std::uint32_t>(static_cast<std::int32_t>(12 - rank) * 600) + 1000;
}

bool table_due(std::uint32_t now, std::uint32_t due, bool drawn) {
    // seg6:0x6235-0x6241: skipped while DGROUP:0x6b00 >= now (unsigned) [BYTES seg6:0x6235]
    return now > due && !drawn;
}

}  // namespace is::dialogs::save
