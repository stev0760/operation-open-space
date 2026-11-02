// The hook point of src/hud (install_hud_hooks, called by WinMain after the other modules'):
// the wave HUD on the surface (ADR 0013), the repaint of states 1 and 7 (seg11:0xafe), the HUD's
// part of the state-1 entry (seg11:0x0), the IS_INFO panels' paint after each wave frame, the
// scrollbar's relayout and seg11:0x2f0 at the end of the wave states. Win32 only: the host build
// compiles this file empty.
#ifdef _WIN32
#include <windows.h>

#include <cstdio>
#include <exception>
#include <memory>
#include <set>
#include <string>

#include "app/app.h"
#include "assets/asset_store.h"
#include "audio/audio.h"
#include "briefing/debriefing.h"
#include "briefing/hooks.h"
#include "formats/ships.h"
#include "hud/design.h"
#include "hud/gdi_canvas.h"
#include "hud/hooks.h"
#include "hud/hud.h"
#include "hud/icon_column.h"
#include "hud/messages.h"
#include "hud/program.h"
#include "hud/screens.h"
#include "hud/scrollbar.h"
#include "hud/target.h"
#include "hud/wave.h"
#include "hud/weapons.h"
#include "shell/rng.h"
#include "world/flight.h"
#include "world/flight_hooks.h"
#include "world/hooks.h"
#include "world/world.h"

namespace is::hud {

namespace {

using app::g;

template <class T, class F, class... A>
T call(const F& f, T fallback, A... a) {
    return f ? f(a...) : fallback;
}

std::unique_ptr<GdiCanvas> g_canvas;

// The player's design as the panels read it (hud/design.h), from the MASTER.SHP record the
// world set-up flies (INI MainShip, the first record when it is missing) and its MASTER.BOD
// body, with the ship-path rule of src/world/hooks.cpp ship_dir. The world set-up does not
// hand the HUD its design record yet (seg14:0x1b84 and seg14:0x21ea-0x21f6 are the world's,
// M5), so the HUD reads the same two records itself [DESIGN, M3 stage 4].
struct PlayerDesign {
    bool found = false;
    DesignPanels panels;
    fmt::ShipRecord shp;   // the two records, for the player's sheet (wave_win32.cpp, M5 s1 SPRITE)
    fmt::BodyRecord body;
};
PlayerDesign g_design;

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

}  // namespace

void refresh_player_design() {
    g_design = PlayerDesign{};
    if (!g.store) return;
    try {
        std::string name = "EAGLE";
        if (g.ini) name = g.ini->get_string(settings::kGameSection, "MainShip", "EAGLE", 20);
        const assets::Ships ships = g.store->ships(ship_dir());
        const fmt::ShipRecord* shp = ships.find(name);
        if (!shp && !ships.ships.records.empty()) shp = &ships.ships.records.front();
        const fmt::BodyRecord* body = shp ? ships.body(*shp) : nullptr;
        if (!shp || !body) return;
        g_design.found = true;
        g_design.panels = design_panels(*shp, *body);
        g_design.shp = *shp;
        g_design.body = *body;
    } catch (const std::exception& x) {
        app::log(std::string("hud: the player's design: ") + x.what());
    }
}

bool player_design_records(fmt::ShipRecord& shp, fmt::BodyRecord& body) {
    if (!g_design.found) return false;
    shp = g_design.shp;
    body = g_design.body;
    return true;
}

namespace {

// The dispatcher's words (DGROUP:0x8740, 0x8768, 0x87b8, 0x8db0, 0x1cb0 ... 0x1cb6).
MessageState g_msg;

std::string lang_text(int id) {
    try {
        if (g.store)
            if (auto s = g.store->find_string(static_cast<std::uint16_t>(id))) return *s;
    } catch (const std::exception& x) {
        app::log("hud: string " + std::to_string(id) + ": " + x.what());
    }
    return {};
}

// seg3:0x6ec's surroundings for the program (messages.h): the World, the flight role's frame
// words DGROUP:0x870c / 0x9608 / 0xa76, the left IS_INFO panel and the sound engine.
class AppMessageHost final : public MessageHost {
public:
    int player() const override { return w().player; }
    std::uint32_t now() const override { return g.game.now(); }  // DGROUP:0x1e50
    int width() const override { return g.area.w; }               // DGROUP:0x90ea
    bool race() const override { return w().handover.race != 0; }  // DGROUP:0x94e
    // DGROUP:0x380c is INI ThePlayer after the game-start command (src/dialogs session); the HUD
    // reads the INI key, the same text [DESIGN].
    std::string pilot() const override {
        try {
            if (g.ini) return g.ini->get_string(settings::kGameSection, "ThePlayer", "", 0x1e);
        } catch (const std::exception&) {
        }
        return {};
    }
    std::string lang(int id) override { return lang_text(id); }
    int rand() override { return shell::game_rng().rand(); }  // seg5:0xdc
    int ship_goal_pool(int rec) const override {
        return rec >= 0 && rec < world::kShipCount ? world::word_at(w().ship(rec), 0x32) : -1;
    }
    int ship_word_06(int rec) const override {
        return rec >= 0 && rec < world::kShipCount ? world::word_at(w().ship(rec), 0x06) : -1;
    }
    void set_sender(int pool, int rec) override {  // DGROUP:0x6e70 / 0x6e72
        if (Hud* h = hud()) {
            h->info().mark(1).rec = rec;
            h->info().mark(1).pool = pool;
        }
    }
    int info_mode() const override { return w().w0a76; }
    // seg7:0x119c(m, 0). The flight role keeps its own copy of DGROUP:0xa76 for the frame and
    // writes it back after the frame (world/flight_hooks.cpp sync_out), so both are set.
    void set_info_mode(int m) override { info_mode_119c(m); }
    bool info_exists() const override { return hud() && hud()->info().exists(); }
    void info_flash() override {
        if (Hud* h = hud()) h->info().flash();
    }
    // seg3:0x1542(0, pool, rec, ..., 0) for the dispatcher's senders 7 and 2 (core.md "Names":
    // 347 "Ambulance", 154 "Defender"). A ship's name (pool 14) is the ship track's [OPEN].
    std::string name(int pool, int rec) override {
        if (pool == 7) return cut_name_code(lang_text(0x15b));
        if (pool == 2) return cut_name_code(lang_text(0x9a));
        app::log_todo("seg3:0x1542 for a ship sender " + std::to_string(rec), "seg3:0x1542");
        return {};
    }
    void set_end(std::uint32_t t) override {
        world::FrameGlobals& f = world::frame_globals();
        f.speed_shown = t;  // DGROUP:0x870c
        f.dg_9608 = 0;      // DGROUP:0x9608
    }
    void sound(int id) override {  // seg10:0x744c
        const int r = audio::play(id);
        app::log("sound 0x" + hex(id) + " seg10:0x744c (seg3:0x6ec) -> " + std::to_string(r));
    }
    void todo(int code) override {
        if (logged_.insert(code).second)
            app::log_todo("message code " + std::to_string(code) + " of seg3:0x1351", "seg3:0x1351");
    }

private:
    static world::World& w() { return world::current(); }
    static std::string hex(int v) {
        char b[16];
        std::snprintf(b, sizeof b, "%x", v);
        return b;
    }
    std::set<int> logged_;
};
AppMessageHost g_msg_host;

// hud::Host for the program: the shell's globals from app::g, the rest through links().
class AppHost final : public Host {
public:
    int state() const override { return g.game.state(); }
    shell::PlayArea area() const override { return g.area; }
    bool border() const override { return g.keys.border != 0; }
    std::uint32_t now() const override { return g.game.now(); }
    std::string lang(int id) override {
        try {
            if (g.store)
                if (auto s = g.store->find_string(static_cast<std::uint16_t>(id))) return *s;
        } catch (const std::exception& x) {
            app::log("hud: string " + std::to_string(id) + ": " + x.what());
        }
        return {};
    }
    bool monochrome() const override { return g.depth == 1; }  // DGROUP:0x90ee
    void log(const std::string& s) override { app::log(s); }
    // srand(GetCurrentTime()): the low word of the port's tick (ADR 0001 decision 5) [DESIGN].
    void seed_rng() override {
        shell::game_rng().srand(static_cast<std::uint16_t>(app::now_tick() & 0xffff));
    }

    // The World (src/world/hooks.h) holds the DGROUP words the set-up writes; a links() slot,
    // when filled, takes precedence.
    bool race() const override { return call(links().race, w().handover.race != 0); }
    bool demo_gate() const override { return call(links().demo_gate, w().w9130 != 0); }
    bool alt_view() const override { return call(links().alt_view, false); }
    int player() const override { return call(links().player, int{w().player}); }
    int ship_pool() const override { return call(links().ship_pool, int{w().roles.ships}); }
    int watch_ship() const override { return call(links().watch_ship, int{w().w952e}); }
    int view_mode() const override { return call(links().view_mode, int{w().w9e49}); }
    bool blink() const override { return call(links().blink, false); }

    std::uint32_t score() const override { return call(links().score, std::uint32_t{0}); }
    bool score_alt() const override { return call(links().score_alt, false); }
    bool score_alt_hi() const override { return call(links().score_alt_hi, false); }
    std::uint32_t wave_ticks() const override { return call(links().wave_ticks, w().frames); }
    void set_wave_seconds(int s) override { w().w934e = static_cast<std::int16_t>(s); }
    // The ship record's fields, and for the player the design words the panels read
    // (refresh_player_design): the tank DGROUP:0x96f0, the lamp rows DGROUP:0x96f4 ... 0x96fa,
    // and the cloak and shield flags +0xe7 and +0xe9, which the world set-up copies
    // (world/flight.cpp apply_flight_design) [BYTES seg14:0x1d1f seg14:0x1d3b-0x1dd6
    // seg14:0x21ea-0x21f6]. Other ships keep ShipView's design defaults until links().ship fills
    // them (M5) [OPEN].
    ShipView ship(int i) const override {
        if (links().ship) return links().ship(i);
        ShipView v;
        if (i < 0 || i >= world::kShipCount) return v;
        world::ShipRecord& r = w().ship(i);
        for (int k = 0; k < 4; ++k) v.health[k] = r.roster_word(k);
        v.b_e7 = r.be7 != 0;
        v.b_e9 = r.be9 != 0;
        v.fuel = r.fuel;
        if (i == w().player && g_design.found) {
            const DesignPanels& d = g_design.panels;
            for (int k = 0; k < 4; ++k) v.lamp_row[k] = d.lamp_row[k];
            v.fuel_max = d.tank;
        }
        return v;
    }
    // seg11:0x2a60-0x2ad1: the player's slot word +0x18, its weapon byte +0xb8 + slot (signed)
    // and count byte +0xae + slot, and the weapon table DGROUP:0x2f54 of seg10:0x1fb2 for the
    // edition DGROUP:0x920c (hud/weapons.h). A slot outside the ten bytes reads no weapon
    // [DESIGN].
    WeaponView weapon() const override {
        if (links().weapon) return links().weapon();
        const int p = w().player;
        if (p < 0 || p >= world::kShipCount) return WeaponView{};
        const world::ShipRecord& r = w().ship(p);
        const int slot = r.weapon_slot;
        if (slot < 0 || slot >= static_cast<int>(r.weapon.size())) return WeaponView{};
        const int wpn = static_cast<std::int8_t>(r.weapon[static_cast<std::size_t>(slot)]);
        const int count = r.weapon_count[static_cast<std::size_t>(slot)];
        if (!table_ || table_edition_ != w().w920c) {
            table_edition_ = w().w920c;
            table_ = std::make_unique<WeaponTable>(table_edition_);
        }
        return weapon_view(*table_, wpn, count);
    }
    // seg3:0x6ec(ship, 7, 0, id), the lamp and fuel routines' messages (messages.h).
    void message(int ship, int id) override {
        if (links().message) return links().message(ship, id);
        post_message(ship, 7, 0, id);
    }
    int frame_rate_set() const override {
        return links().frame_rate_set ? links().frame_rate_set() : g.scheduler.rate();
    }
    // DGROUP:0x59a, the frames of the last second, counted by the wave frame's seg7:0xa60 port
    // (world/flight_hooks.h frame_rate_actual) [BYTES seg7:0xa60-0xaf7] (M3 s4 fixer, review M3).
    int frame_rate_actual() const override {
        return links().frame_rate_actual ? links().frame_rate_actual() : world::frame_rate_actual();
    }

    // seg11:0x3f2e's attitude, seg3:0x0 and seg13:0x371c are not ported: unlinked, attitude 0,
    // every target acceptable and kind 0 [OPEN target tests, a later stage].
    int attitude(int a, int b) const override { return call(links().attitude, 0, a, b); }
    bool target_ok(int p, int r) const override { return call(links().target_ok, true, p, r); }
    int target_kind(int p, int r) const override { return call(links().target_kind, 0, p, r); }
    void bump_bad_index() override {
        if (links().bump_bad_index) links().bump_bad_index();
    }
    std::string text_9628() const override { return call(links().text_9628, t9628_); }
    std::string text_8718() const override { return call(links().text_8718, t8718_); }
    std::string text_960a() const override { return call(links().text_960a, t960a_); }
    std::string text_8740() const override { return call(links().text_8740, g_msg.sender); }
    std::string text_8768() const override { return call(links().text_8768, g_msg.line1); }
    std::string text_87b8() const override { return call(links().text_87b8, g_msg.line2); }
    // seg13:0x5a6a's record fields (Info::target_picture): the object record, the pool-0 icon
    // record DGROUP:0x57a8 + 0x23·rec, seg13:0x371c, DGROUP:0x9324, DGROUP:0x9208. DGROUP:0x131c
    // is not ported: 0 [OPEN]. The canvas's icon strip is the briefing entry's DGROUP:0x3a02.
    bool cell(int p, int r, CellInputs& out) const override {
        if (links().cell) return links().cell(p, r, out);
        if (p < 0 || p >= world::kPoolCount || r < 0 || r >= w().pools.records(p)) return false;
        const world::ObjectRecord& o = w().pools.rec(p, r);
        out = CellInputs{};
        out.kind = target_kind(p, r);
        out.ship_pool = w().roles.ships;
        out.detail = w().detail;
        out.obj.state = o.state;
        out.obj.extent = o.extent;
        out.obj.size = o.size;
        out.obj.gap = o.gap;
        out.obj.src_y = o.src_y;
        out.obj.b51 = o.race_status;
        if (p == 0 && r < world::kIconRecords) {
            const world::IconRecord& ic = w().icons[static_cast<std::size_t>(r)];
            out.icon.w00 = ic.w00;
            out.icon.w02 = ic.w02;
            out.icon.w19 = ic.w19;
        }
        // Pool 7: record 0's word +0x10 (seg13:0x5d2f) and the logo strip DGROUP:0x39e8[5]
        // (M4 s1 SCREENS).
        if (p == 7 && w().pools.records(7) > 0) out.pool7_w10 = world::word_at(w().pools.rec(7, 0), 0x10);
        if (g_canvas) {
            g_canvas->set_image(Img::kIconStrip, world::icon_strip());
            // The wave's slot 5 when the world holds one, else the renderer's seg10:0x39a8 strip
            // (650 x 52, 14 logo cells), as the briefing's canvas takes it [DESIGN].
            if (p == 7) {
                gfx::ImagePtr strip = world::slot_image(5);
                if (!strip && g.gfx) strip = g.gfx->logo_strip();
                g_canvas->set_image(Img::kLogoStrip, strip);
            }
        }
        return true;
    }
    int game_mode() const override { return call(links().game_mode, int{w().kind}); }
    void draw_race(Canvas& c) override {
        if (links().draw_race) links().draw_race(c);
    }

    int player_target_pool() const override {
        return call(links().player_target_pool, int{w().ship(w().player).target_pool});
    }
    int player_target_rec() const override {
        return call(links().player_target_rec, int{w().ship(w().player).target_rec});
    }
    // seg13:0x2206 (hud/target.h next_target) on the World: the visible list of the last
    // visibility pass, the player's heading +0x28, DGROUP:0x1e50. The last contact DGROUP:0x943e
    // / 0x9440 / 0x52d8 has no writer in the port (seg8:0x4817, seg8:0x6286, seg8:0x761c are
    // M4): 0, 0, 0 as in the file [OPEN].
    int next_target() override {
        if (links().next_target) return links().next_target();
        world::World& wd = w();
        NextTargetInputs in;
        in.ship_pool = wd.roles.ships;
        in.player = wd.player;
        if (in.ship_pool < 0 || in.ship_pool >= world::kPoolCount ||
            in.player >= wd.pools.records(in.ship_pool))
            return -1;
        in.heading = wd.ship_object(wd.player).frame;
        in.now = g.game.now();
        in.visible = wave_visible();
        NextTargetHost nh;
        nh.point = [](int p, int r) {
            if (p < 0 || p >= world::kPoolCount || r < 0 || r >= w().pools.records(p)) return Point{};
            const world::ObjectRecord& o = w().pools.rec(p, r);
            return Point{o.scr_x, o.scr_y};
        };
        nh.direction16 = [](int num, int den) {
            return int{world::direction16(static_cast<std::int16_t>(num), static_cast<std::int16_t>(den))};
        };
        nh.accept = [this](int p, int r) { return target_ok(p, r); };
        nh.distance = [this, &in](int p, int r) { return distance(p, r, in.ship_pool, in.player); };
        nh.in_use = [this](int p) { return in_use(p); };
        return ::is::hud::next_target(in, nh);
    }
    // seg3:0xf4(ship, pool, rec) as far as the HUD knows it, until links().set_target holds the
    // whole routine: the ship's target words, the target display's object and texts, and the
    // repaint [BYTES seg3:0x102-0x1ae seg3:0x259-0x26c seg3:0x41f-0x440]. The texts that
    // seg3:0x1542, seg13:0x218a and seg17:0x8e60 make, and seg7:0x65aa, are [OPEN].
    void set_target(int s, int p, int r) override {
        if (links().set_target) return links().set_target(s, p, r);
        if (s < 0 || s >= world::kShipCount) return;
        world::ShipRecord& ship = w().ship(s);
        ship.target_pool = static_cast<std::int16_t>(p);  // seg3:0x10a: +0x3e
        ship.target_rec = static_cast<std::int16_t>(r);   // seg3:0x111: +0x40
        if (s != w().player) return;                       // seg3:0x115-0x11b
        Hud* h = hud();
        if (!h) return;
        h->info().mark(0).pool = p;  // seg3:0x122: DGROUP:0x6e54
        h->info().mark(0).rec = r;   // seg3:0x128: DGROUP:0x6e52
        if (!target_ok(p, r) && target_kind(p, r) == 0) {  // seg3:0x12c-0x14d
            bump_bad_index();
            return;
        }
        t960a_.clear();  // seg3:0x150-0x165: DGROUP:0x13be, DGROUP:0x13bf
        t9628_.clear();
        t8718_.clear();
        if (p == -1) {  // seg3:0x16a-0x1ae: strings 2205 and 2206
            t9628_ = lang(0x89d);
            t8718_ = lang(0x89e);
        } else if (p == 0 && r >= 0 && r < world::kIconRecords) {
            // seg3:0x259-0x2c6: the icon's file name (DGROUP:0x57b4 + 0x23·rec), then
            // seg3:0x1542(0, 0, rec, buf, 0) "Icon N" and string 2208 or 2209 by byte +0x53
            // appended by lstrcat (hud/target.h) [BYTES seg3:0x27f seg3:0x28d seg3:0x2b1 seg3:0x2bd
            // seg3:0x428].
            t9628_ = w().icons[static_cast<std::size_t>(r)].name;
            t8718_ = icon_label(lang(0x98), r);
            if (r < w().pools.records(0))
                if (const int id = icon_suffix_id(w().pools.rec(0, r).b53)) t8718_ += lang(id);
        } else if (p == 7) {
            // seg3:0x3aa-0x3d0: string 2211 "INNER SPACE" into the buffer (after a discarded
            // seg3:0x1542) and string 2212 "COMMAND" into DGROUP:0x960a [BYTES seg3:0x3b4
            // seg3:0x3c7].
            t8718_ = lang(0x8a3);
            t960a_ = lang(0x8a4);
        } else {
            log_once("the target texts of pool " + std::to_string(p) + " (seg3:0x1542)");
        }
        h->info().invalidate(kInfoTarget);  // seg3:0x42d-0x440
        refresh_object(p, r);               // seg3:0x449: seg7:0x65aa(pool, rec)
    }
    // seg7:0x65aa(pool, rec) [BYTES seg7:0x65aa-0x6621]: the icon column's row, the watch
    // window (M5), and the left display's picture when its mark (DGROUP:0xa76 - 2) holds the
    // object.
    void refresh_object(int p, int r) {
        if (p == 0) icon_column_row(r);  // seg7:0x64ac(0, 1, rec)
        else if (p == w().roles.ships && r != w().player)
            log_once("seg13:0x6450, the watch window of ship " + std::to_string(r));
        Hud* h = hud();
        if (!h) return;
        const int m = info_mode();
        if (m != kModeTarget && m != kModeMessage) return;
        const Mark& k = h->info().mark(m - 2);
        if (k.pool == p && k.rec == r) h->info().target_picture_now();  // seg13:0x58dc(0, 1)
    }
    // seg13:0x2577-0x25aa: (+0x00, +0x02) when word +0x1a != 0, else seg8:0x29ea of the world
    // position dword +0x04 (world::flight_to_screen).
    Point screen_point(int p, int r) const override {
        if (links().screen_point) return links().screen_point(p, r);
        if (p < 0 || p >= world::kPoolCount || r < 0 || r >= w().pools.records(p)) return Point{};
        const world::ObjectRecord& o = w().pools.rec(p, r);
        if (o.on_screen != 0) return Point{o.scr_x, o.scr_y};
        const world::ScreenPoint s = world::flight_to_screen(o.x, o.y);
        return Point{s.x, s.y};
    }
    Point player_point() const override {
        if (links().player_point) return links().player_point();
        const world::ObjectRecord& p = w().pools.rec(w().roles.ships, w().player);
        return Point{p.scr_x, p.scr_y};  // +0x00, +0x02 [BYTES seg13:0x25b0-0x25cb]
    }
    // seg8:0x19b0 is Pascal with num at [bp+6]: seg13:0x25c6 pushes dx first and dy last, so
    // num = dy and den = dx [BYTES seg13:0x25c6 seg13:0x25d2 seg13:0x25d3].
    int direction16(int dx, int dy) const override {
        if (links().direction16) return links().direction16(dx, dy);
        return world::direction16(static_cast<std::int16_t>(dy), static_cast<std::int16_t>(dx));
    }
    int in_use(int p) const override {
        if (links().in_use) return links().in_use(p);
        return p >= 0 && p < world::kPoolCount ? w().pools.in_use(p) : 0;
    }
    // seg8:0x285a on the two records' world positions (world::distance_cells).
    int distance(int pa, int ra, int pb, int rb) const override {
        if (links().distance) return links().distance(pa, ra, pb, rb);
        const world::ObjectRecord& a = w().pools.rec(pa, ra);
        const world::ObjectRecord& b = w().pools.rec(pb, rb);
        std::int16_t dx = 0, dy = 0;
        return world::distance_cells(a.x, a.y, b.x, b.y, dx, dy);
    }
    // seg3:0x65e code 3 on the World's pool-8 side table (M3 s3 fixer) [BYTES seg3:0x6a2-0x6b8].
    bool gate_post_live(int r) const override {
        return call(links().gate_post_live, world::gate_post_live(w(), r), r);
    }
    int record_bound_9422() const override { return call(links().record_bound_9422, 0); }
    bool mark2_window() const override { return call(links().mark2_window, false); }
    void draw_mark2() override {
        if (links().draw_mark2) links().draw_mark2();
    }

    void demo_mark() override { run(links().demo_mark); }
    void pin_player() override { run(links().pin_player); }
    void draw_player() override { run(links().draw_player); }
    void set_58c() override { run(links().set_58c); }
    void draw_1f38() override { run(links().draw_1f38); }
    void radar() override { run(links().radar); }
    void radar_centre() override { run(links().radar_centre); }

    // DGROUP:0xa76 is the World's w0a76, which the flight role shares.
    int info_mode() const override { return w().w0a76; }
    void set_info_mode(int m) override { w().w0a76 = static_cast<std::int16_t>(m); }

private:
    static world::World& w() { return world::current(); }
    void log_once(const std::string& what) {
        if (logged_.insert(what).second) app::log_todo(what, "seg3:0xf4");
    }
    std::string t9628_, t8718_, t960a_;  // DGROUP:0x9628, 0x8718, 0x960a (the fallback's)
    mutable std::unique_ptr<WeaponTable> table_;  // DGROUP:0x2f54
    mutable int table_edition_ = 0;
    std::set<std::string> logged_;
    static void run(const std::function<void()>& f) {
        if (f) f();
    }
};

AppHost g_host;

// The HUD's part of the key switch seg11:0x6b64 (world::KeyActions, set_key_actions).
class HudKeyActions final : public world::KeyActions {
public:
    void target_code(bool ctrl, int code) override {
        if (Hud* h = hud()) h->info().target_code(ctrl, code);  // seg11:0x772e
    }
    void toggle_manual_target() override {
        if (Hud* h = hud()) h->info().toggle_manual();  // seg11:0x7138
    }
    void zoom_changed(std::int32_t zoom) override {
        if (links().zoom_changed) return links().zoom_changed(zoom);
        // seg11:0x723a-0x7247: SetScrollPos(600 - zoom), then the radar.
        set_scroll_pos(static_cast<std::int16_t>(600 - static_cast<std::int16_t>(zoom)));
        if (links().radar) links().radar();
    }
    // B, M and seg10:0x7e1c's repaint (M3 s2 fixer).
    void show_frame_rate() override {
        if (Hud* h = hud()) h->info().show_frame_rate();  // seg11:0x71d4
    }
    void show_messages() override {
        if (Hud* h = hud()) h->info().show_messages();  // seg11:0x71a9
    }
    void info_repaint() override {
        if (Hud* h = hud()) h->info().invalidate(kInfoTarget);  // PostMessage(DGROUP:0xa7c, WM_PAINT)
    }
};
HudKeyActions g_actions;
std::unique_ptr<ScrollBar> g_scroll;
std::unique_ptr<Hud> g_hud;
bool g_load_failed = false;

// seg11:0x0's HUD part, once (while DGROUP:0x5042 = 0 in the original) [BYTES seg11:0xd].
void ensure_loaded() {
    if (!g_hud || g_hud->is_loaded() || g_load_failed || !g.gfx || g.gfx->surface().empty())
        return;
    std::string why;
    if (!g_canvas->load(&why)) {
        g_load_failed = true;
        app::log("hud: the HUD bitmaps did not load: " + why);
        return;
    }
    g_hud->loaded();
    app::log("hud: seg11:0x0 loaded METAL, 250, 240, 45, 260, DIGITAL, SYSTEMS");
}

}  // namespace

HBRUSH metal_brush() { return g_canvas ? g_canvas->metal_brush() : nullptr; }

// seg11:0x2f8c: DeleteObject(DGROUP:0x5040); when the player's slot word +0x18 != 0,
// DGROUP:0x5040 = seg11:0x2ff6(the slot's weapon byte); PostMessage(DGROUP:0xa7e, WM_PAINT)
// [BYTES seg11:0x2f9a-0x2fe8]. The composed pictures are not made [OPEN], so a weapon on that
// path shows none.
void weapon_picture_refresh() {
    if (!g_canvas) return;
    g_canvas->set_image(Img::kWeapon, nullptr);  // seg11:0x2f9a-0x2faa
    world::World& wd = world::current();
    if (wd.player >= 0 && wd.player < world::kShipCount) {
        const world::ShipRecord& r = wd.ship(wd.player);
        const int slot = r.weapon_slot;
        // seg11:0x2fb6 tests slot != 0 only; the range test is the port's bound [DESIGN].
        if (slot > 0 && slot < static_cast<int>(r.weapon.size())) {
            const int wpn = static_cast<std::int8_t>(r.weapon[static_cast<std::size_t>(slot)]);
            const WeaponPicture p = weapon_picture(WeaponTable(wd.w920c), wpn);
            if (p.kind == WeaponPicture::Kind::kGraphic && g.gfx) {
                try {
                    g_canvas->set_image(Img::kWeapon, g.gfx->load_graphic(p.graphic, 0, -1));
                } catch (const std::exception& x) {
                    app::log("hud: weapon picture " + std::to_string(p.graphic) + ": " + x.what());
                }
            } else if (p.kind == WeaponPicture::Kind::kComposed) {
                app::log_todo("the composed weapon picture of weapon " + std::to_string(wpn),
                              "seg11:0x30d0");
            }
        }
    }
    if (g_hud) g_hud->info().invalidate(kInfoWeapon);  // seg11:0x2fe8
}

namespace {

void post_message_now(int recipient, int pool, int rec, int code) {
    dispatch_message(g_msg, g_msg_host, recipient, pool, rec, code);
}

void info_mode_now(int mode) {
    world::current().w0a76 = static_cast<std::int16_t>(mode);
    world::frame_globals().info_mode = static_cast<std::int16_t>(mode);
    if (g_hud) g_hud->info().set_mode_119c(mode);
}

// seg9:0x6a12's inputs from the program: DGROUP:0x91f8 is the briefing's wave counter, the
// player's words +0xd2 and +0xd0, the score dword DGROUP:0x1e58. DGROUP:0x95bc (a player game)
// is taken as set: state 4 follows a player's game [OPEN: the demo's path to state 4].
std::vector<briefing::GameOverLine> game_over_lines_now() {
    world::World& wd = world::current();
    int captured = 0, destroyed = 0;
    if (wd.player >= 0 && wd.player < world::kShipCount) {
        captured = world::word_at(wd.ship(wd.player), 0xd2);
        destroyed = world::word_at(wd.ship(wd.player), 0xd0);
    }
    return briefing::game_over_lines(g.area.w, g.area.h, briefing::wave(), captured, destroyed, wd.score,
                                     true, [](int id) { return lang_text(id); });
}

void close_hud() {
    if (g_scroll) g_scroll->destroy();       // seg11:0x2fa-0x30a: DGROUP:0xa7a
    if (g_hud) g_hud->info().close();         // seg11:0x310-0x336: DGROUP:0xa7c, DGROUP:0xa7e
}

}  // namespace

}  // namespace is::hud

namespace is::app {

void install_hud_hooks(App& app) {
    using namespace is::hud;
    g_canvas = std::make_unique<GdiCanvas>(*app.gfx, [] { return g.font; });
    // The position 600 - zoom comes from the radar (links().scroll_pos); 600 - 120 before it
    // is linked [BYTES DGROUP:0xa6c].
    g_scroll = std::make_unique<ScrollBar>(
        [] { return links().scroll_pos ? links().scroll_pos() : 600 - 120; },
        [] { return g.main; }, [] { return g.gfx ? g.gfx->factor() : 1; });
    g_hud = std::make_unique<Hud>(*g_canvas, g_host, g_scroll.get());
    bind_program(
        g_hud.get(), [](int pos) { g_scroll->set_pos(pos); }, [] { close_hud(); });

    bind_messages(post_message_now, info_mode_now);  // seg3:0x6ec, seg7:0x119c (M4 s1 SCREENS)
    world::set_key_actions(&g_actions);
    // The 0x9aa handler seg2:0x4f0 (Alt+I and the wave-start post): the IS_ICON column (M3
    // stage 4).
    app::icon_watch() = app::WatchToggle{[] { return icon_column_exists(); }, [] { icon_column_create(); },
                                         [] { icon_column_destroy(); }};
    install_wave(app);  // the wave's integration, src/hud/wave_win32.cpp (M3 s2 fixer)
    // The palette message's InvalidateRect(main, NULL, TRUE) reaches the IS_INFO children too
    // (src/app/wndproc.cpp palette_message): both panels are due and paint after the next frame.
    app::children_invalidated() = [] {
        if (!g_hud) return;
        g_hud->info().invalidate(kInfoTarget);
        g_hud->info().invalidate(kInfoWeapon);
    };

    auto& h = app.game.hooks();
    // seg11:0xafe for states 1 and 7 (shell::GameState::repaint runs entry 1 for both): the HUD
    // and then the state dispatch; the IS_INFO panels paint after it, as the new child windows'
    // first WM_PAINT did [DESIGN ADR 0013 decision 4].
    h.repaint[shell::kWave] = [](shell::GameState&) {
        ensure_loaded();
        g_hud->repaint();
        g_hud->info().flush();
    };
    // The state-1 entry (seg10:0x0, whose seg10:0x112 runs seg11:0x0 once) [BYTES seg10:0x112].
    auto prev_enter = h.enter[shell::kWave];
    h.enter[shell::kWave] = [prev_enter](shell::GameState& s) {
        if (prev_enter) prev_enter(s);
        ensure_loaded();
        load_wave_resources();  // the rock images and the radar (M3 s2 fixer)
    };
    // After each wave frame: the IS_INFO panels a PostMessage(WM_PAINT) marked (ADR 0013
    // decision 4).
    auto prev_frame = h.frame[shell::kWave];
    h.frame[shell::kWave] = [prev_frame](shell::GameState& s) {
        if (prev_frame) prev_frame(s);
        if (s.state() == shell::kWave || s.state() == shell::kHelp) g_hud->info().flush();
    };
    // The play area changed (seg2:0x1ac8): the scrollbar follows the scale factor [DESIGN].
    auto prev_area = h.play_area;
    h.play_area = [prev_area](shell::GameState& s) {
        if (prev_area) prev_area(s);
        if (g_scroll) g_scroll->relayout();
    };
    // seg11:0x2f0 at the wave teardown (seg10:0x259) and at the entries of the states that
    // leave the wave (shell.md "Closing the HUD"); state 7 keeps the HUD.
    auto prev_teardown = h.teardown;
    h.teardown = [prev_teardown](shell::GameState& s) {
        if (prev_teardown) prev_teardown(s);
        close_hud();
    };
    for (int st = 0; st < shell::kStateCount; ++st) {
        if (st == shell::kWave || st == shell::kHelp) continue;
        auto prev = h.enter[static_cast<std::size_t>(st)];
        h.enter[static_cast<std::size_t>(st)] = [prev](shell::GameState& s) {
            close_hud();
            if (prev) prev(s);
        };
    }
    // State 4, GAME OVER (M4 s1 SCREENS): seg9:0xd6c's blackout and seg9:0x6a12 at the entry
    // (GameState::enter_game_over runs the hook only with no request pending, so the blackout of
    // a call with DGROUP:0x13de = 1 is not drawn [OPEN]), and seg11:0xafe's case 4, seg9:0x6a12
    // alone [BYTES seg9:0xdcc-0xde9 seg11:0x16af].
    // State 5's entry seg6:0x5d58 calls seg13:0x65f2(1) and seg11:0x2f0 [BYTES seg6:0x5d9e
    // seg6:0x5da4]: the IS_ICON column goes with the HUD (M4 s1 fixer).
    auto prev_scores = h.enter[shell::kHighScores];
    h.enter[shell::kHighScores] = [prev_scores](shell::GameState& s) {
        if (icon_column_exists()) icon_column_destroy();
        if (prev_scores) prev_scores(s);
    };
    auto prev_over = h.enter[shell::kGameOver];
    h.enter[shell::kGameOver] = [prev_over](shell::GameState& s) {
        // The column is not closed by the bytes read so far on this path (seg9:0xd6c; the state
        // teardown seg10:0x1fe is unread, shell.md "Exit"); the port closes it with the wave
        // [OPEN seg10:0x1fe] [DESIGN].
        if (icon_column_exists()) icon_column_destroy();
        if (prev_over) prev_over(s);
        if (!g_canvas || !g.gfx || g.gfx->surface().empty()) return;
        game_over_entry(*g_canvas, g.area.w, g.area.h, s.change_pending(), game_over_lines_now());
        app::log("hud: seg9:0xd6c GAME OVER drawn");
    };
    // The abort (0x991, request 0) and the demo's end return to state 0 with the column still
    // up; the same rule as above [OPEN seg10:0x1fe] [DESIGN].
    auto prev_intro = h.enter[shell::kIntro];
    h.enter[shell::kIntro] = [prev_intro](shell::GameState& s) {
        if (icon_column_exists()) icon_column_destroy();
        if (prev_intro) prev_intro(s);
    };
    auto prev_over_paint = h.repaint[shell::kGameOver];
    h.repaint[shell::kGameOver] = [prev_over_paint](shell::GameState& s) {
        if (prev_over_paint) prev_over_paint(s);
        if (!g_canvas || !g.gfx || g.gfx->surface().empty()) return;
        draw_game_over(*g_canvas, game_over_lines_now());
    };
}

}  // namespace is::app
#endif
