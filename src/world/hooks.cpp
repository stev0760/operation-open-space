// owner: world set-up (the single writer of this file in later stages).
// The wave set-up's hooks (hooks.h): the program's World, the state-1 load, the briefing entry's
// world part and the wave start, wired around the briefing's own state-1 and state-3 hooks
// (src/briefing/hooks.cpp). Win32 only: the host build compiles this file without its hooks.
#include "world/hooks.h"
#include "world/shots.h"

#include <array>

#include "world/gate_exit.h"

namespace is::world {
namespace {
World g_world;
std::function<void(World&)> g_on_wave_start;
std::function<void(World&)> g_on_placement;
std::array<std::int16_t, kShipCount> g_design_mass{};  // DGROUP:0x9704 + 0xad·design [OPEN]
}  // namespace
World& current() { return g_world; }
void on_wave_start(std::function<void(World&)> f) { g_on_wave_start = std::move(f); }
void on_briefing_placement(std::function<void(World&)> f) { g_on_placement = std::move(f); }
std::int16_t design_mass(int design) {
    return design >= 0 && design < kShipCount ? g_design_mass[static_cast<std::size_t>(design)]
                                              : std::int16_t{0};
}
WaveEndLinks& wave_end_links() {
    static WaveEndLinks l;
    return l;
}
}  // namespace is::world

#ifndef _WIN32
namespace is::world {
// The host build has no program: the END entry points run on the World with the default hosts.
void set_entry_context(std::function<EntryContext()>) {}
void on_debriefing(std::function<void(World&)>) {}
void run_gate_exit(int pool, int rec, std::uint32_t now) {
    GateExitHost h;
    GateExitCtx c{g_world, now, h, nullptr};
    gate_exit(c, pool, rec);
}
void run_vanish_end(int pool, int rec, std::uint32_t now) {
    GateExitHost h;
    GateExitCtx c{g_world, now, h, nullptr};
    vanish_end(c, pool, rec);
}
DeadlineResult run_wave_deadlines(shell::GameState& s) {
    WaveEndHost h;
    WaveEndCtx c{g_world, s, h, nullptr};
    return wave_deadlines(c);
}
}  // namespace is::world
#endif

#ifdef _WIN32
#include <windows.h>

#include <cstdlib>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "app/app.h"
#include "audio/audio.h"
#include "briefing/hooks.h"
#include "dialogs/audio_options.h"
#include "assets/asset_store.h"
#include "formats/icon_source.h"
#include "formats/ships.h"
#include "nav/enter.h"
#include "nav/fs.h"
#include "nav/nav.h"
#include "nav/nav_draw.h"
#include "shell/geometry.h"
#include "shell/rng.h"
#include "world/draw.h"
#include "world/flight_hooks.h"
#include "world/missile.h"
#include "world/roster.h"
#include "world/setup.h"
#include "world/update.h"
#include "world/visit.h"

namespace is::world {

namespace {

using app::g;

// The port's ExtractIcon for the files of the chosen directory: the file read through the
// walk's own Win32Fs (whose 8.3 names map back to the long names) and decoded by IconSource,
// never loaded or run [DESIGN ADR 0012 decision 5].
class Win32IconReader final : public IconReader {
public:
    explicit Win32IconReader(nav::Win32Fs& fs) : fs_(fs) {}
    void clear() { cache_.clear(); }
    int count(const std::string& path) override { return source(path).count(); }
    std::optional<fmt::IconCell> cell(const std::string& path, int index) override {
        return source(path).cell(index);
    }

private:
    const fmt::IconSource& source(const std::string& path) {
        auto it = cache_.find(path);
        if (it == cache_.end())
            it = cache_.emplace(path, fmt::IconSource::load(std::filesystem::path(fs_.native(path)))).first;
        return it->second;
    }
    nav::Win32Fs& fs_;
    std::map<std::string, fmt::IconSource> cache_;
};

std::unique_ptr<nav::Win32Fs> g_fs;
std::unique_ptr<nav::Walker> g_walker;
std::unique_ptr<Win32IconReader> g_icons;
std::vector<std::string> g_paths;  // without an Enter: the records' paths, by DGROUP:0x94a2 [DESIGN]
int g_enter_serial = 0;            // the nav::last_accepted() Enter the World holds (M3 s2 fixer)
gfx::ImagePtr g_strip;
bool g_loaded = false;   // seg10:0x0 ran its load: the next briefing entry comes from seg2:0x1b96
bool g_briefed = false;  // seg9:0x5cca ran: the next state-1 entry is seg9:0x60bc

void log(const std::string& s) { app::log(s); }

// INI ShipPath as ADR 0004 decision 4 resolves it (the rule of src/dialogs/hooks.cpp
// ship_dir): the key's directory when it holds MASTER.SHP, else the asset directory's SHIPS\,
// else the asset directory [DESIGN].
std::filesystem::path ship_dir() {
    std::error_code ec;
    std::string s;
    try {
        if (g.ini) s = g.ini->get_string(settings::kGameSection, "ShipPath", "", 0x6d);
    } catch (const std::exception&) {
    }
    if (!s.empty()) {
        const std::filesystem::path p(app::widen(s));
        if (std::filesystem::exists(p / L"MASTER.SHP", ec)) return p;
    }
    if (std::filesystem::exists(g.asset_dir / L"SHIPS" / L"MASTER.SHP", ec)) return g.asset_dir / L"SHIPS";
    return g.asset_dir;
}

// The player's ship of seg14's set-up seg14:0x0(-1) at the placement seg9:0x6624 (M4 s3 ROSTER;
// src/world/roster.h for the order): the set-up's player alone (ship_setup_minimal), the roster
// choice (the fresh record of wave 1 with seg14:0x2300's MainShip pick, or DGROUP:0x931c's
// record), the design seg14:0x1b84 (flight.h flight_design, apply_flight_design with roster byte
// +0x33, rating.h) from the SHP record that roster +0x00 names and its MASTER.BOD body, the
// roster -> ship fill seg14:0x1430 (roster.h ship_from_roster: systems, weapons, ammo, fuel,
// credit and resources carried from the last wave end, no refill), the cell sizes, and
// seg16:0x3784(i, 1) only for a fresh ship, wave 1 or a restored first wave (roster.h
// purchase_due / purchase_pass) [BYTES seg14:0x232-0x47e seg14:0x1430-0x1b82 seg14:0xd1a-0xd32].
// The ship sheet (seg14:0x11a8 through ISPACE.DLL ordinal 2) belongs to the placement's
// seg14:0x0(-1) at the briefing entry: the drawing side builds it in its on_briefing_placement
// hook (briefing_entry) [BYTES seg9:0x688a seg14:0xd32]. seg11:0xae94's weapon slot +0x18 runs
// at the wave start (src/hud/wave_win32.cpp, World::picture_due) [BYTES seg9:0x6951].
void ship_setup_player(World& w) {
    ship_setup_minimal(w);
    g_design_mass.fill(0);
    ShipRecord& s = w.ship(w.player);
    ObjectRecord& o = w.ship_object(w.player);
    // seg14 gives a ship record radar_draw seg11:0x37a0 and the radar brush of design word
    // DGROUP:0x96fe + 0xad·n, which seg14:0x1e08 sets to 7 on every path (the player's own
    // marker takes WHITE_BRUSH) [BYTES seg14:0x2141 seg14:0x20d7 seg14:0x1e08] (M3 s4 fixer:
    // the radar's player marker, comparison item 6), its on_damage seg8:0x45e0 and its
    // on_destroy seg16:0x8e4 [BYTES seg14:0x2158 seg14:0x212a] (M4 s1 fixer).
    o.radar_draw.set(kRadarShip);
    o.radar_brush = 7;
    o.on_damage.set(kDamageShip);
    o.on_destroy.set(kDestroyShip);
    s.sprite_slot = w.player;  // seg14:0xd0a: ship word +0x50 = i, the image slot 0x28 + i [BYTES seg14:0xd03-0xd0a]
    if (!g.store) return;
    try {
        std::string name = "EAGLE";
        if (g.ini) name = g.ini->get_string(settings::kGameSection, "MainShip", "EAGLE", 20);
        const assets::Ships ships = g.store->ships(ship_dir());
        // DGROUP:0x380c: INI ThePlayer, 12 bytes (shell.md "Player name") [BYTES seg10:0x8c].
        const std::string pilot = g.ini ? g.ini->get_string(settings::kGameSection, "ThePlayer", "", 12) : std::string();
        const RosterPlacement rp = roster_placement(w, ships.ships, ships.bodies, name, shell::game_rng(), pilot);
        if (rp.pick.rewrite && g.ini) {
            // seg14:0x2300's `MainShip=TRANSFORMER` [BYTES seg14:0x240a seg14:0x2420].
            g.ini->set(settings::kGameSection, "MainShip", "TRANSFORMER");
            try {
                g.ini->save();
            } catch (const std::exception& x) {
                log(std::string("INSPACE.INI: ") + x.what());
            }
        }
        if (rp.fresh_roster)
            log("world: ship set-up: fresh roster, MainShip \"" + rp.pick.name + "\" -> SHP index " +
                std::to_string(rp.pick.index) + " (DGROUP:0x95b4 = " + std::to_string(w.w95b4) + ")");
        if (rp.shp_index < 0) {
            log("world: ship set-up: MASTER.SHP holds no record [OPEN]");
            return;
        }
        const fmt::ShipRecord* shp = &ships.ships.records[static_cast<std::size_t>(rp.shp_index)];
        const fmt::BodyRecord* body = ships.body(*shp);
        if (!body) {
            log("world: ship set-up: no MASTER.BOD body for " + shp->name + " [OPEN]");
            return;
        }
        const RosterRecord* r = roster_at(w, s.roster_index);
        // seg14:0x1b84 inside seg14:0x1430 [BYTES seg14:0x14e5]; the turn takes roster +0x33
        // for a ship with a record [BYTES seg14:0x2274-0x22a7].
        const FlightDesign d = flight_design(shp->thruster, shp->armor, body->strength_base,
                                             body->byte_1c, body->byte_1d, body->flags, shp->flags);
        apply_flight_design(s, o, d, w.player_game != 0, static_cast<std::uint32_t>(app::now_tick()),
                            r ? std::int16_t{r->b33[0]} : std::int16_t{0});
        g_design_mass[0] = d.mass;
        // seg14:0x1b84 stores SHP word +0x1f, the ship's hit sound, in the design word +0x2e
        // (DGROUP:0x9700 + 0xad n) and in object word +0x42. seg8:0x45e0 plays the design word
        // through seg10:0x7944 on every hit, since DGROUP:0xb0a stays 0, and does not read +0x42
        // there (0x07 for TRANSFORMER) (structs.md Callbacks of the ship pool, The ship's hit
        // sound; C085) [BYTES seg14:0x1e18 seg14:0x1e2d seg14:0x20c0 seg14:0x20d3 seg8:0x49bc]
        // [BYTES MASTER.SHP] [ORACLE core-0001] [ORACLE probe-0001] (M4 s1 fixer, s4 fixer).
        o.hit_sound = static_cast<std::int16_t>(shp->hit_sound);
        // The design words of seg13:0x33d6 (rating.h) (M5 s2 BRIEFING).
        w.design_ratings[static_cast<std::size_t>(s.design_index >= 0 && s.design_index < kShipCount ? s.design_index : 0)] =
            design_rating(*shp, *body);
        // seg14:0x1430's roster -> ship fill (roster.h) [BYTES seg14:0x1430-0x1b82].
        ship_from_roster(w, w.player, *shp);
        // seg14:0x0's three muzzle-table calls for design 0 (shots.h fill_muzzles), from the
        // design words the World holds (design_words_from_world: +0x2a only) [BYTES seg14:0xcdc
        // seg14:0xce6 seg14:0xcf0] (M4 s2 fixer).
        fill_muzzles(shot_state(), 0, design_words_from_world(w, 0));
        // The cell sizes +0x20, +0x22, +0x24 (seg14:0x11a8's raw side and gap, seg14:0x0's
        // scaling; setup.h ship_cell_raw, ship_cell_sizes). The raw side is the body DIB's
        // height; TRANSFORMER's #4000 60 is 132 x 33, so Size 1 gives extent 264 and size 33,
        // which the oracle's entry origin (2796, 11188 - 30 (F - 1)) and its cell box
        // DGROUP:0x1e5c = (514, 343, 543, 372) need [ORACLE probe-0001] (M5 s1 VIEW). A body
        // from a .DIB file (+0x21 = 0xffff, read from +0x23) is [OPEN] here.
        if (body->bitmap_id != fmt::kBodyBitmapFromFile) {
            const fmt::Dib dib = g.store->sprite(body->bitmap_id);
            const ShipCellRaw raw = ship_cell_raw(static_cast<std::int16_t>(dib.height));
            ship_cell_sizes(o, raw, w.size);
            // With a roster record seg14:0x11a8 writes the raw side and gap to roster bytes
            // +0x7a / +0x7b (s1-sprite.md claim 14), the class size of the briefing's ship page
            // [BYTES seg14:0x131c seg14:0x1337 seg17:0x22b9-0x22e3] (M5 s2 BRIEFING).
            if (RosterRecord* rr = roster_at(w, s.roster_index)) {
                rr->size = static_cast<std::uint8_t>(raw.side);
                rr->gap = static_cast<std::uint8_t>(raw.gap);
            }
            o.src_y = o.gap;  // seg9:0x6624's +0x12 = +0x24 follows (setup.cpp place)
        } else {
            log("world: ship set-up: body " + body->name + " reads a .DIB file [OPEN]; no cell size");
        }
        // seg14:0xd1a-0xd32: seg16:0x3784(i, 1) for a fresh ship, wave 1 or the restored first
        // wave; it repairs to 100, sets the gun flags +0x24 / +0xe0 / +0xe1 from +0x0a and the
        // wings, and fills the tank (the oracle's wave 1: systems 100, +0x24 = 1, +0xe0 = +0xe1 =
        // 1, fuel 0x528 for TRANSFORMER; structs.md Ship records, Probe values, C035) [BYTES
        // seg14:0xd32 seg16:0x3468 seg16:0x3e80] [ORACLE probe-0001]. In later waves every value
        // above comes from the roster (core-0001's
        // 39, 9, 39, 1) [ORACLE core-0001].
        w.sheet_due = purchase_due(w, rp.fresh_ship);  // the sheet's path (C067) (M4 s4 fixer)
        if (w.sheet_due) purchase_pass(w, w.player, 1);
        log("world: ship set-up: the player flies " + shp->name + " (wave " + std::to_string(w.wave) +
            (rp.fresh_roster ? ", fresh roster" : "") + ", thrust " + std::to_string(s.thrust) + ", drag " +
            std::to_string(s.drag) + ", turn " + std::to_string(s.turn) + ", mass " + std::to_string(o.mass) +
            ", fuel " + std::to_string(s.fuel) + ", systems " + std::to_string(s.w26) + "/" + std::to_string(s.w28) +
            "/" + std::to_string(s.w2a) + "/" + std::to_string(s.w2c) + ")");
    } catch (const std::exception& x) {
        log(std::string("world: ship set-up: ") + x.what());
    }
}

std::unique_ptr<SetupHost> make_host() {
    auto h = std::make_unique<SetupHost>(shell::game_rng(), *g_walker, *g_icons);
    h->now = [] { return static_cast<std::uint32_t>(app::now_tick()); };
    h->log = [](const std::string& s) { log(s); };
    h->recentre = [](World& w) { recentre_view(w); };  // seg8:0x0 (flight role)
    h->ship_setup = [](World& w) { ship_setup_player(w); };  // seg14 (M3 s2 fixer)
    return h;
}

// The values the set-up reads from the program: the play area, the INI keys and Enter's
// hand-over (nav.md "Values handed to the wave set-up").
void read_inputs(World& w, bool new_entry) {
    w.W = static_cast<std::int16_t>(g.area.w);
    w.H = static_cast<std::int16_t>(g.area.play_h);
    if (g.ini) {
        const auto& k = settings::kGameSection;
        w.size = static_cast<std::int16_t>(g.ini->get_int(k, "Size", 1));       // DGROUP:0x9156
        w.detail = static_cast<std::int16_t>(g.ini->get_int(k, "Detail", 3));   // DGROUP:0x9208
        w.complex = static_cast<std::int16_t>(g.ini->get_int(k, "Complex", 0)); // DGROUP:0x8ff8
        std::string path = g.ini->get_string(k, "Path", "", 100);               // DGROUP:0x8f7a
        if (!path.empty() && path.back() != '\\') path += '\\';
        w.program_path = path;
        const std::string drive = g.ini->get_string(k, "Drive", "C", 10);
        char c = drive.empty() ? 'C' : drive[0];
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        w.drive_offset = static_cast<std::int16_t>(c - 'C');  // DGROUP:0x94ac
    }
    if (g.ini) {
        // DGROUP:0x8ff2 `Bright` and the star count DGROUP:0x9154 of seg6:0xe36; the copy into
        // DGROUP:0x9152 (seg6:0xb0d) at the state-1 load [DESIGN: the port reads the INI at the
        // wave's entries] (M3 s4 DRAW) [BYTES seg6:0xe36 seg6:0xe88 seg6:0xea3].
        w.bright = static_cast<std::int16_t>(g.ini->get_int(settings::kGameSection, "Bright", 0));
    }
    // seg6:0xe36 runs once per INI read in the original; the port reads at every entry, so it
    // starts each time from the count before its first run (a Detail outside 0 ... 3 keeps it,
    // and Bright must not take one more each entry) [DESIGN] (M3 s4 fixer, review L5).
    if (!w.star_base_set) {
        w.star_base = w.star_wanted;
        w.star_base_set = true;
    }
    w.star_wanted = star_count_for(w.detail, g.ini ? w.bright : std::int16_t{0}, w.star_base);
    if (new_entry) w.star_count = w.star_wanted;
    // DGROUP:0x8ffe, the dissolve step of the same routine seg6:0xdaa (capture.h dissolve_step_for)
    // (M4 s2 CAPTURE) [BYTES seg6:0xe41 seg6:0xe4f seg6:0xe5d seg6:0xe6b-0xe78].
    w.w8ffe = dissolve_step_for(w.detail, w.w8ffe);
    if (w.size < 0 || w.size > 2) w.size = 1;
    w.shift = static_cast<std::int16_t>(4 - w.size);  // DGROUP:0x9150
    w.play_mode = static_cast<std::int16_t>(g.play_mode);
    w.w920c = static_cast<std::int16_t>(g.edition.edition);  // DGROUP:0x920c (shell.md "Editions")
    w.w11aa = static_cast<std::int16_t>(g.edition.plus);     // DGROUP:0x11aa (shell/edition.h)
    w.player_game = 1;  // DGROUP:0x95bc: the port has no demo yet
    // DGROUP:0x94a6, the icon count of Navigation's last selection (seg6:0x4368's sum), which
    // the generator and the debriefing read (M4 s1 END) [BYTES seg6:0x589b seg9:0x70e7].
    w.w94a6 = static_cast<std::int16_t>(nav::selection().pane.icons);
    nav::Handover v = nav::enter_state().handover;
    if (v.directory.empty()) {
        // No Enter yet: the IS_TEST_BRIEFING directory of the briefing's test hook, else the INI
        // Drive root, as the briefing takes them [DESIGN].
        const char* t = std::getenv("IS_TEST_BRIEFING");
        if (t && *t && std::string(t) != "1")
            v.directory = t;
        else
            v.directory = std::string(1, static_cast<char>('C' + w.drive_offset)) + ":\\";
        if (!v.directory.empty() && v.directory.back() != '\\') v.directory += '\\';
    }
    w.handover.directory = v.directory;
    w.handover.demons_gate = static_cast<std::int16_t>(v.demons_gate);
    w.handover.race = static_cast<std::int16_t>(v.race);
    w.handover.duel = static_cast<std::int16_t>(v.duel);
    w.handover.difficulty = static_cast<std::int16_t>(v.difficulty);
    w.handover.opponent = static_cast<std::int16_t>(v.opponent);
    w.handover.laps = static_cast<std::int16_t>(v.laps);
    // DGROUP:0x94a2 and DGROUP:0x94b0 are Enter's (seg6:0x36b6): written once per accepted Enter
    // and kept through both generator runs, the state-1 load and the briefing entry's, and
    // through the next waves of the directory [BYTES seg6:0x36b6 seg9:0x3214 seg9:0x10d4]
    // (M3 s2 fixer: the briefing entry no longer turns a new directory into a revisited one).
    const nav::Accepted& acc = nav::last_accepted();
    if (acc.serial != 0) {
        if (acc.serial != g_enter_serial) {
            g_enter_serial = acc.serial;
            w.added = acc.added ? 1 : 0;
            w.record = static_cast<std::int16_t>(acc.record < 0 ? 0 : acc.record);
        }
    } else if (new_entry) {
        // No Enter (IS_TEST_BRIEFING): the port finds the record by the path, once per state-1
        // load; a path not seen in this run is new [DESIGN] (nav.md: NAV-19's records are not
        // saved yet).
        int index = -1;
        for (std::size_t i = 0; i < g_paths.size(); ++i)
            if (g_paths[i] == v.directory) index = static_cast<int>(i);
        w.added = index < 0 ? 1 : 0;
        if (index < 0) {
            g_paths.push_back(v.directory);
            index = static_cast<int>(g_paths.size()) - 1;
        }
        w.record = static_cast<std::int16_t>(index);
    }
    w.dir();
}

// ---- the strip DGROUP:0x3a02 (seg10:0x5b30, seg9:0x1d2, seg9:0x352)

void run_ops(HDC dc, const std::vector<nav::Op>& ops) {
    for (const nav::Op& o : ops) {
        switch (o.kind) {
        case nav::Op::Kind::kLine: {
            HPEN pen = CreatePen(PS_SOLID, 1, static_cast<COLORREF>(o.colour));
            HGDIOBJ old = SelectObject(dc, pen);
            MoveToEx(dc, o.x0, o.y0, nullptr);
            LineTo(dc, o.x1, o.y1);
            SelectObject(dc, old);
            DeleteObject(pen);
            break;
        }
        case nav::Op::Kind::kRect: {
            HPEN pen = CreatePen(PS_SOLID, 1, static_cast<COLORREF>(o.colour));
            HGDIOBJ op = SelectObject(dc, pen);
            HGDIOBJ ob = SelectObject(dc, GetStockObject(o.filled ? LTGRAY_BRUSH : NULL_BRUSH));
            Rectangle(dc, o.x0, o.y0, o.x1, o.y1);
            SelectObject(dc, ob);
            SelectObject(dc, op);
            DeleteObject(pen);
            break;
        }
        case nav::Op::Kind::kPixel:
            SetPixel(dc, o.x0, o.y0, static_cast<COLORREF>(o.colour));
            break;
        case nav::Op::Kind::kFill:
            if ((o.colour & 0xFFFFFFu) == 0) {
                PatBlt(dc, o.x0, o.y0, o.x1, o.y1, BLACKNESS);
            } else {
                HBRUSH b = CreateSolidBrush(static_cast<COLORREF>(o.colour));
                HGDIOBJ old = SelectObject(dc, b);
                PatBlt(dc, o.x0, o.y0, o.x1, o.y1, PATCOPY);
                SelectObject(dc, old);
                DeleteObject(b);
            }
            break;
        case nav::Op::Kind::kText: {
            HFONT f = CreateFontA(o.height, o.width, 0, 0, o.weight, 0, 0, 0, 0, 0, 0, 2, 0x22, o.face.c_str());
            HGDIOBJ old = SelectObject(dc, f);
            const UINT align = SetTextAlign(dc, o.align);
            const COLORREF tc = SetTextColor(dc, static_cast<COLORREF>(o.colour));
            const COLORREF bk = SetBkColor(dc, static_cast<COLORREF>(o.bk));
            const int mode = SetBkMode(dc, o.opaque ? OPAQUE : TRANSPARENT);
            TextOutA(dc, o.x0, o.y0, o.text.c_str(), static_cast<int>(o.text.size()));
            SetBkMode(dc, mode);
            SetBkColor(dc, bk);
            SetTextColor(dc, tc);
            SetTextAlign(dc, align);
            SelectObject(dc, old);
            DeleteObject(f);
            break;
        }
        }
    }
}

void build_strip(const World& w) {
    g_strip.reset();
    if (!g.gfx || w.strip_slots <= 0) return;
    try {
        g_strip = g.gfx->create_image(strip_x(w.strip_slots), 44);  // seg10:0x5b30
    } catch (const std::exception& x) {
        log(std::string("world: icon strip: ") + x.what());
        return;
    }
    HDC dc = g_strip->hdc();
    PatBlt(dc, 0, 0, strip_x(w.strip_slots), 44, BLACKNESS);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = fmt::kIconCell;
    bi.bmiHeader.biHeight = -fmt::kIconCell;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    for (int s = 0; s < w.strip_slots && s < kIconRecords; ++s) {
        const IconSlot& c = w.strip[static_cast<std::size_t>(s)];
        if (c.kind == IconSlot::Kind::kIcon)
            StretchDIBits(dc, strip_x(s), 6, 32, 32, 0, 0, 32, 32, c.cell.bgra.data(), &bi,
                          DIB_RGB_COLORS, SRCCOPY);
        else if (c.kind == IconSlot::Kind::kPlaceholder)
            run_ops(dc, nav::placeholder(strip_x(s), c.flag, 32, 6, c.name, w.drive_offset));
    }
    GdiFlush();
}

// ---- the entries

// seg10:0x0's load (seg9:0x22b4(0)) after the briefing's state-1 set-up (the greeting).
void state1(shell::GameState&) {
    World& w = g_world;
    read_inputs(w, true);
    w.wave = static_cast<std::int16_t>(briefing::wave());
    auto h = make_host();
    state1_load(w, *h);
    g_loaded = true;
}

// seg2:0x1b96 -> seg9:0xc06 -> seg9:0x5cca, or seg9:0x5cca from the debriefing timer.
void briefing_entry() {
    World& w = g_world;
    read_inputs(w, false);
    w.wave = static_cast<std::int16_t>(briefing::wave());
    if (g_loaded) new_game(w);  // seg9:0xc06 runs only on the state-change path
    g_loaded = false;
    if (g_icons) g_icons->clear();
    auto h = make_host();
    enter_briefing(w, *h);
    // seg9:0x6624's seg14:0x0(-1) builds the sheets (seg16:0x3784 -> seg16:0x3468 ->
    // seg14:0x11a8, only under World::sheet_due and roster.h placement_rebuilds_sheet's tests)
    // and the flame strips (seg10:0x5d3c) inside the briefing entry, before the text [BYTES
    // seg9:0x5e5d seg9:0x688a seg14:0xc7e seg14:0xd32 seg16:0x362b]: the drawing side registers
    // that work here (M5 s2 BRIEFING; world.md Ship sheets, Path, C067).
    if (g_on_placement) g_on_placement(w);
    build_strip(w);
    bind_flight_world(&w);
    g_briefed = true;
}

// The play rectangle DGROUP:0x1d30 (the stars' wrap) as seg2:0xca4 set it inside seg2:0xba4 at
// seg9:0x5d14: the briefing's hook (src/briefing/hooks.cpp enter_briefing) computes g.clip
// there; the World keeps its copy [BYTES seg2:0xd54 seg9:0x5d14] (M3 s4 DRAW, s4 fixer).
void play_region(World& w) {
    w.play_rect = rect16(g.clip.play.left, g.clip.play.top, g.clip.play.right, g.clip.play.bottom);
}

// seg9:0x60bc after the briefing's own part.
void wave_start() {
    World& w = g_world;
    auto h = make_host();
    start_wave(w, *h);
    g_briefed = false;
    wave_started();  // the latch's taps from the briefing are gone (M3 s2 fixer)
    // seg11:0x3cc2(player) and the rest of seg9:0x60bc's tail (src/hud) [BYTES seg9:0x6320].
    if (g_on_wave_start) g_on_wave_start(w);
}

// ---- M4 stage 1, track END: the hosts of the gate exit and the wave end

std::function<EntryContext()> g_entry_ctx;
std::function<void(World&)> g_on_debriefing;

void todo_once(const std::string& what, const char* address) {
    static std::set<std::string> seen;
    if (seen.insert(what + address).second) app::log_todo(what, address);
}

// seg10:0x754a(id, pool, rec, mode) through M7's one wrapper (dialogs/audio_options.h); the
// mode-1 point is the flight role's audio source (flight_hooks.cpp sound_source).
void play_object(int id, int pool, int rec, int mode) { is::sound::play_at(id, pool, rec, mode); }

class ProgramGateHost final : public GateExitHost {
public:
    void sound(int id, int pool, int rec, int arg) override { play_object(id, pool, rec, arg); }
    void destroy(int pool, int rec) override { destroy_object(pool, rec); }  // seg12:0x274a
    void pool5_slot(int rec) override {
        todo_once("pool-5 slot words of record " + std::to_string(rec), "seg12:0x22cf");
    }
    void removed(int pool, int rec) override {
        if (pool == 0 || pool == 14) return;  // a no-op for pools 0 and 14 (s1-end.md 1c.6)
        // seg7:0x2080 on the program's World (missile.h recount_in_use): the in-use count drops
        // past the record, so the ambulance's pool 7 reads 0 after its vanish and the next H is
        // not refused as busy (M4 s3 fixer, review M8) [BYTES seg7:0x2080].
        (void)rec;
        recount_in_use(g_world.pools, pool);
    }
    // seg7:0x5ae8 on the program's World (capture.h icon_counted) unless a link replaces it
    // (M4 s2 CAPTURE).
    void icon_counted(int rec, int arg) override {
        if (wave_end_links().icon_counted) return wave_end_links().icon_counted(rec, arg);
        World& w = current();
        ::is::world::icon_counted(w, rec, arg, w.roles.ships);
    }
    std::int16_t design_type(int design) override {
        return wave_end_links().design_type ? wave_end_links().design_type(design) : std::int16_t{0};
    }
    void column_row(int pool, int rec) override {
        if (wave_end_links().column_row) return wave_end_links().column_row(pool, rec);
        todo_once("icon column row", "seg7:0x65aa");
    }
    void message(int ship, int id) override {
        if (wave_end_links().message) return wave_end_links().message(ship, id);
        todo_once("message " + std::to_string(id), "seg3:0x6ec");
    }
    void lines() override { todo_once("the link lines after a vanish", "seg12:0x3778"); }
    void menu_refresh() override { todo_once("menu state after the vanish", "seg2:0x170e"); }
    // W(weapon)+0x1e from the program's weapon words (collide.h weapon_word) [BYTES seg7:0x1988].
    std::int16_t weapon_word_1e(int weapon) override { return weapon_word(weapon, 0x1e); }
    std::string lang(int id) override {
        try {
            if (g.store) return g.store->find_string(static_cast<std::uint16_t>(id)).value_or("");
        } catch (const std::exception&) {
        }
        return {};
    }
};

class ProgramWaveEndHost final : public WaveEndHost {
public:
    void music(int code, int arg) override {
        audio::music(code, arg);
        log("music seg10:0x7480(" + std::to_string(code) + ", " + std::to_string(arg) + ") at the wave end");
    }
    void reset_sounds() override {
        audio::reset_slots();
        log("sound reset: ISPACE ordinal 7 command 2 (seg10:0x7db8) at the wave end");
    }
    void close_watch_windows() override {
        if (wave_end_links().close_watch_windows) return wave_end_links().close_watch_windows();
        todo_once("watch windows", "seg13:0x65f2");
    }
    void clear_restored() override {
        if (wave_end_links().clear_restored) return wave_end_links().clear_restored();
        todo_once("DGROUP:0x92e4 in the session", "seg9:0x6ceb");
    }
    void delete_wave_objects() override { todo_once("the 32 objects DGROUP:0x4e94", "seg9:0x1092"); }
    void free_buffer(int pool, int rec) override {
        if (!g_entry_ctx) return;
        EntryContext c = g_entry_ctx();
        entry_buffer_free(c, pool, rec);  // seg12:0x26a4
    }
    void close_hud() override {
        if (wave_end_links().close_hud) return wave_end_links().close_hud();
        todo_once("the HUD windows", "seg11:0x2f0");
    }
    void release_wave_dcs() override { todo_once("the 8 wave DCs", "seg7:0x49b6"); }
    void black_client() override {
        if (g.gfx && !g.gfx->surface().empty())
            PatBlt(g.gfx->dc(), 0, 0, g.area.w, g.area.h, BLACKNESS);  // [BYTES seg9:0x6d7a]
    }
    void free_blocks() override {}  // seg6:0x7266: the port holds no such blocks [DESIGN]
    void flame_off(int ship) override { briefing_flame(g_world, 0, ship); }  // seg11:0xb09c(0, i)
    void meal_check() override { todo_once("the meal re-check", "seg10:0x9672"); }
    // seg6:0x885c(0, 0) [BYTES seg9:0x31f0 seg7:0x687] [DESIGN ADR 0014] (M4 s3 fixer).
    void save_lastgame() override {
        if (wave_end_links().save_lastgame) return wave_end_links().save_lastgame();
        todo_once("LASTGAME (stage 3, ADR 0014)", "seg6:0x885c");
    }
    void debriefing_started() override {
        if (g_on_debriefing) g_on_debriefing(g_world);
    }
    void history_box() override {
        if (wave_end_links().history_box) return wave_end_links().history_box();
        // seg16:0x68d6(main, 2328, 2329): string 2328 + 2329, caption "Inner Space"
        // (DGROUP:0x9690), MB_ICONEXCLAMATION [BYTES seg16:0x68ed seg16:0x6904 seg16:0x691b].
        ProgramGateHost h;
        const std::string text = h.lang(2328) + h.lang(2329);
        log("history MessageBox seg16:0x68d6: " + text);
        MessageBoxA(g.main, text.c_str(), "Inner Space", MB_ICONEXCLAMATION);
    }
    void hall_of_justice() override { todo_once("the Hall of Justice", "seg9:0x6c62"); }
    void race_credit() override { todo_once("the race credit", "seg9:0x6be3"); }
    void race_refuel() override { refuel_all(g_world); }  // seg9:0x6db0 (roster.h) [BYTES seg9:0x6cf6]
    // seg7:0x5fa-0x608: state 6, DGROUP:0x914a = 0, seg16:0xcfe(1) (visit.h visit_links().enter,
    // bound by src/hud) (M4 s3 fixer, review H3).
    void ambulance_enter() override {
        if (visit_links().enter) return visit_links().enter();
        todo_once("the ambulance's state 6", "seg7:0x5fa");
    }
    void demo_seg6_92b8() override { todo_once("the demo's seg6:0x92b8", "seg7:0x64d"); }
    // seg9:0x2e97-0x2f74 on the program's World with MASTER.SHP (roster.h roster_ratings)
    // [BYTES seg9:0x2e97].
    void roster_ratings() override {
        try {
            if (!g.store) return todo_once("the roster rating loop without the asset store", "seg9:0x2e97");
            const assets::Ships ships = g.store->ships(ship_dir());
            ::is::world::roster_ratings(g_world, ships.ships);
        } catch (const std::exception& x) {
            log(std::string("world: roster ratings: ") + x.what());
        }
    }
};

ProgramGateHost g_gate_host;
ProgramWaveEndHost g_wave_end_host;

}  // namespace

void set_entry_context(std::function<EntryContext()> f) { g_entry_ctx = std::move(f); }
void on_debriefing(std::function<void(World&)> f) { g_on_debriefing = std::move(f); }

void run_gate_exit(int pool, int rec, std::uint32_t now) {
    if (g_entry_ctx) {
        EntryContext e = g_entry_ctx();
        GateExitCtx c{g_world, now, g_gate_host, &e};
        gate_exit(c, pool, rec);
    } else {
        GateExitCtx c{g_world, now, g_gate_host, nullptr};
        gate_exit(c, pool, rec);
    }
}

void run_vanish_end(int pool, int rec, std::uint32_t now) {
    if (g_entry_ctx) {
        EntryContext e = g_entry_ctx();
        GateExitCtx c{g_world, now, g_gate_host, &e};
        vanish_end(c, pool, rec);
    } else {
        GateExitCtx c{g_world, now, g_gate_host, nullptr};
        vanish_end(c, pool, rec);
    }
}

DeadlineResult run_wave_deadlines(shell::GameState& s) {
    auto h = make_host();
    WaveEndCtx c{g_world, s, g_wave_end_host, h.get()};
    const DeadlineResult r = wave_deadlines(c);
    if (r == DeadlineResult::kEndPass) log("wave: the tick's deadline block ended the pass (seg7:0x5ce-0x707)");
    return r;
}

gfx::ImagePtr icon_strip() { return g_strip; }

}  // namespace is::world

namespace is::app {

void install_world_hooks(App& app) {
    using namespace is::world;
    g_fs = std::make_unique<nav::Win32Fs>();
    g_walker = std::make_unique<nav::Walker>(*g_fs, [](const std::string& s) { app::log("world fs " + s); });
    g_icons = std::make_unique<Win32IconReader>(*g_fs);
    power_on(g_world);
    // The roster routines' stubs log through the program's TODO facility (roster.h) (M4 s3 ROSTER).
    set_roster_todo([](const std::string& what, const char* address) { todo_once(what, address); });
    set_frame_world(&g_world);  // the frame's own drawing reads this World (update.h, M3 s4 DRAW)
    auto& h = app.game.hooks();
    // The state-1 entry: seg10:0x0 (the greeting, then the load) or seg9:0x60bc after the
    // briefing (the briefing's part, then the ships).
    auto prev_wave = h.enter[shell::kWave];
    h.enter[shell::kWave] = [prev_wave](shell::GameState& s) {
        const bool start = g_briefed;
        // seg10:0x0 for state 1: seg10:0x6e76(8, 0) at seg10:0x37 (scheme 8 in mode 0, which
        // puts the ramps at 103 ... 192, so the border tiles' trace colour c3af00 is entry
        // 126) and seg10:0x6e4c at seg10:0x3f, before the greeting and the load seg9:0x22b4 at
        // seg10:0xad (s4-draw.md 3.2) (M3 s4 DRAW) [BYTES seg10:0x35 seg10:0x37 seg10:0x3f
        // seg10:0xad].
        if (!start && g.gfx) {
            g.gfx->palette_scheme(8, 0);
            g.gfx->palette_animate();
        }
        if (prev_wave) prev_wave(s);
        if (start)
            wave_start();
        else
            state1(s);
    };
    // The briefing entry: the world part of seg9:0x5cca runs before the briefing's text, as the
    // load, the icon fill and the placement precede it [BYTES seg9:0x5d3c seg9:0x5d66 seg9:0x5e5d].
    auto prev_brief = h.enter[shell::kBriefing];
    h.enter[shell::kBriefing] = [prev_brief](shell::GameState& s) {
        briefing_entry();
        if (prev_brief) prev_brief(s);
        play_region(g_world);
    };
    auto prev_teardown = h.teardown;
    h.teardown = [prev_teardown](shell::GameState& s) {
        if (prev_teardown) prev_teardown(s);
        g_briefed = false;  // a state change leaves the wave or its briefing
        g_loaded = false;
    };
}

}  // namespace is::app
#endif
