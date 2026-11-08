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
#include "hud/visit_win32.h"
#include "hud/wave.h"
#include "hud/weapons.h"
#include "shell/rng.h"
#include "world/ambulance.h"
#include "world/flight.h"
#include "world/flight_hooks.h"
#include "world/hooks.h"
#include "world/icon_mover.h"
#include "world/player_sheet.h"
#include "world/missile.h"
#include "world/purchases.h"
#include "world/roster.h"
#include "world/visit.h"
#include "world/world.h"
#include "dialogs/ambulance_options.h"
#include "dialogs/audio_options.h"

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
std::function<gfx::ImagePtr(int)> g_fragment_images;
}  // namespace

void set_fragment_images(std::function<gfx::ImagePtr(int k)> f) { g_fragment_images = std::move(f); }
gfx::ImagePtr fragment_image(int k) { return g_fragment_images ? g_fragment_images(k) : nullptr; }

// seg13:0x5f49's inputs for icon rec (hud/target.h CellInputs): pool-1 record 0's size and gap
// (far DGROUP:0x1dfc) [BYTES seg13:0x5f49-0x5f58], and per quarter the fragment index byte
// DGROUP:0x57c3 + 0x23·rec + i (cbw), pool-1 record k's `state` and its icon word DGROUP:0x6038 +
// 2k [BYTES seg13:0x5f94-0x5fb3]. The bytes and the icon words are the shatter's (links()).
void fill_fragment_inputs(int rec, CellInputs& in) {
    world::World& wd = world::current();
    if (wd.pools.records(1) > 0) {
        in.frag_size = wd.pools.rec(1, 0).size;
        in.frag_gap = wd.pools.rec(1, 0).gap;
    }
    for (int i = 0; i < 4; ++i) {
        CellInputs::Fragment& f = in.frags[i];
        f.index = links().fragment_index ? static_cast<std::int8_t>(links().fragment_index(rec, i)) : -1;
        f.state = f.index >= 0 && f.index < wd.pools.records(1) ? int{wd.pools.rec(1, f.index).state} : 1;
        f.parent = links().fragment_parent && f.index >= 0 ? links().fragment_parent(f.index) : -1;
    }
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
    // DGROUP:0x9604 / 0x9602 and seg13:0x218a, the capture's (links(), M4 s2 DISPLAY).
    int capture_value() const override { return call(links().capture_value, 0); }
    int capture_defenders() const override { return call(links().capture_defenders, 0); }
    std::string team_name(int ship) override {
        if (links().team_name) return links().team_name(ship);
        app::log_todo("seg13:0x218a, the team name of message 0x6f", "seg13:0x218a");
        return {};
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

    // dword DGROUP:0x1e58: the World's score, which the kill scorer and the captures add to
    // (seg11:0x1bcc's DGROUP:0x5018 test, then seg11:0x4a0(0)) [BYTES seg11:0x1bda-0x1beb] (M4 s2
    // DISPLAY).
    std::uint32_t score() const override { return call(links().score, w().score); }
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

    // seg11:0x3f2e's attitude is not ported: attitude 0 [OPEN]. seg3:0x0 and seg13:0x371c run on
    // the World (hud/target.h target_valid, target_kind_371c; core.md "Target validity") (M4 s2
    // DISPLAY): a captured or destroyed icon (`state` 1) fails, and the 280 ms retarget moves on.
    int attitude(int a, int b) const override { return call(links().attitude, 0, a, b); }
    bool target_ok(int p, int r) const override {
        if (links().target_ok) return links().target_ok(p, r);
        return target_valid(validity(), p, r);
    }
    int target_kind(int p, int r) const override {
        return call(links().target_kind, target_kind_371c(p, r), p, r);
    }
    static TargetValidity validity() {
        TargetValidity v;
        v.ship_pool = w().roles.ships;
        v.player = w().player;
        v.allocated = [](int p) { return p >= 0 && p < world::kPoolCount ? int{w().pools.allocated(p)} : 0; };
        // A record the port's pool does not hold reads `state` 1 [DESIGN].
        v.state = [](int p, int r) {
            return p >= 0 && p < world::kPoolCount && r >= 0 && r < w().pools.records(p)
                       ? int{w().pools.rec(p, r).state}
                       : 1;
        };
        v.ship_147 = [](int r) { return r >= 0 && r < world::kShipCount ? int{w().ship(r).radar_hide} : 0; };
        // The defender table DGROUP:0x52fe (world/collide.h CollideGlobals::defenders); an entry
        // past its end reads 0 / -1 / 0 [DESIGN].
        const auto def = [](int r) -> const world::DefenderEntry* {
            const auto& d = world::collide_globals().defenders;
            return r >= 0 && r < static_cast<int>(d.size()) ? &d[static_cast<std::size_t>(r)] : nullptr;
        };
        v.def_00 = [def](int r) { return def(r) ? int{def(r)->w00} : 0; };
        v.def_pool = [def](int r) { return def(r) ? int{def(r)->owner_pool} : -1; };
        v.def_rec = [def](int r) { return def(r) ? int{def(r)->owner_rec} : 0; };
        v.gate_post_live = [](int r) { return world::gate_post_live(w(), r); };
        return v;
    }
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
            fill_fragment_inputs(r, out);
        }
        // Pool 7: record 0's word +0x10 (seg13:0x5d2f) and the logo strip DGROUP:0x39e8[5]
        // (M4 s1 SCREENS).
        if (p == 7 && w().pools.records(7) > 0) out.pool7_w10 = world::word_at(w().pools.rec(7, 0), 0x10);
        if (g_canvas) {
            g_canvas->set_image(Img::kIconStrip, world::icon_strip());
            // Slot 14 + r, the icon's own image (hud/icon_images.h) (M4 s5 ICONS).
            if (p == 0 && r < world::kIconRecords)
                g_canvas->set_image(Img::kIconOwn, world::slot_image(world::kIconOwnSlot + r));
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
    // seg13:0x5e05: seg10:0x6c02(500, 0, -1); the original loads and deletes it at each call
    // [BYTES seg13:0x5e05 seg13:0x5e8a], the port keeps the one image [DESIGN] (M4 s4 fixer B).
    bool gate_picture() override {
        if (!g_canvas || !g.gfx) return false;
        if (!gate_image_ && !gate_failed_) {
            try {
                gate_image_ = g.gfx->load_graphic(CellPlan::kGateGraphic, 0, -1);
            } catch (const std::exception& x) {
                if (!gate_failed_) app::log(std::string("hud: graphic 500 (seg13:0x5e05): ") + x.what());
                gate_failed_ = true;
                return false;
            }
        }
        g_canvas->set_image(Img::kGatePicture, gate_image_);
        return gate_image_ != nullptr;
    }
    bool fragment(int k) override {
        if (!g_canvas) return false;
        gfx::ImagePtr im = fragment_image(k);
        g_canvas->set_image(Img::kFragment, im);
        return im != nullptr;
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
        if (p == -1) {  // seg3:0x16a-0x1ae: strings 2205 and 2206, dead in the original: a pool of
                        // -1 fails both target checks above (structs.md target_pool; C081)
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
        } else if (p == 1 && r >= 0 && r < static_cast<int>(w().fragment_icon.size())) {
            // The icon fragment: its icon's file name and string 892 (hud/target.h
            // fragment_target_texts) [BYTES seg3:0x2cf-0x2ed seg3:0x40b-0x428] (M4 s3 fixer, FUEL
            // request 3). An icon index outside the table names nothing [DESIGN].
            const int icon = w().fragment_icon[static_cast<std::size_t>(r)];
            const std::string name =
                icon >= 0 && icon < world::kIconRecords ? w().icons[static_cast<std::size_t>(icon)].name : std::string();
            const FragmentTexts t = fragment_target_texts(name, lang(kStringIconFragment));
            t9628_ = t.t9628;
            t8718_ = t.t8718;
        } else if (p == 7) {
            // seg3:0x3aa-0x3d0: string 2211 "INNER SPACE" into the buffer (after a discarded
            // seg3:0x1542) and string 2212 "COMMAND" into DGROUP:0x960a [BYTES seg3:0x3b4
            // seg3:0x3c7].
            t8718_ = lang(0x8a3);
            t960a_ = lang(0x8a4);
        } else if (target_kind(p, r) != 0) {
            // seg3:0x3fb-0x419: an exit gate (seg13:0x371c != 0), string 585 "Exit Gate"
            // (hud/target.h gate_label) (M4 s4 fixer B).
            t8718_ = gate_label(lang(kStringExitGate));
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
    gfx::ImagePtr gate_image_;  // graphic 500, the exit gate's picture (M4 s4 fixer B)
    bool gate_failed_ = false;
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

namespace {

// The sheet DGROUP:0x39e8[W(w)-4] of seg11:0x2ff6. seg10:0x533a (MISSILE's, at the end of
// seg14:0x11a8) renders it into a slot 0x30 ... 0x38; while links().weapon_sheet_slot is unset or
// gives no image, the HUD renders the same call itself: seg10:0x6ee6(k, W(w)-2, "", 2, 0x63,
// W+0x1a, W+0x1c, 1, 0, 0xc, 0xc, 0, sel) is the ship renderer (gfx/ship_sheet.h) with gap 99,
// four headings (w5 = 1), schemes 12 / 12, mode 0 and the scale flag of Size >= 2
// (s2-display-verify.md claims 14-16) [BYTES seg10:0x53e3-0x5409 seg10:0x5426-0x5461]
// [OPEN: a stop-gap that duplicates seg10:0x533a; links().weapon_sheet_slot is bound to the
// world's W(w)-4 (wave_win32.cpp), so it runs only for a weapon the sheet build skipped]. W(w)+0x06 = 4's
// loader seg10:0x4412 is wave_win32.cpp anim4_sheet (M4 s5 WEAPONS): this stop-gap reaches it only
// for a weapon the world gave no slot. The quick renderer (sel = 1) is [OPEN]; so is
// seg10:0x6308's rescale at Size 0.
gfx::ImagePtr weapon_sheet(const WeaponTable& t, int wpn, const WeaponPicture& p, int size_level) {
    if (wpn == 0x18) return world::slot_image(p.sheet_slot);  // the player's own sheet
    if (links().weapon_sheet_slot) {
        const int k = links().weapon_sheet_slot(wpn);
        if (k >= 0)
            if (gfx::ImagePtr im = world::slot_image(k)) return im;
    }
    if (!g.store || !g.gfx) return nullptr;
    const int id = t.word(wpn, -2);
    if (t.word(wpn, 0x06) == 4) {
        app::log_todo("the animation-4 sheet of a weapon the world gave no slot", "seg10:0x4412");
        return nullptr;
    }
    // Rendered at each seg11:0x2f8c on the palette in force, as seg10:0x533a renders at each
    // sheet build [DESIGN].
    const int scale = world::sheet_scale_flag(size_level, 0, 0);
    gfx::ImagePtr im;
    try {
        im = render_weapon_sheet(*g.gfx, g.store->sprite(static_cast<std::uint16_t>(id)), id, scale);
        app::log("hud: weapon " + std::to_string(wpn) + " sheet of graphic " + std::to_string(id) + ": " +
                 (im ? std::to_string(im->width()) + " x " + std::to_string(im->height()) : std::string("none")) +
                 " (the HUD's seg10:0x533a)");
    } catch (const std::exception& x) {
        app::log("hud: weapon sheet " + std::to_string(id) + ": " + x.what());
    }
    return im;
}

}  // namespace

// seg11:0x2f8c: DeleteObject(DGROUP:0x5040); when the player's slot word +0x18 != 0,
// DGROUP:0x5040 = seg11:0x2ff6(the slot's weapon byte); PostMessage(DGROUP:0xa7e, WM_PAINT)
// [BYTES seg11:0x2f9a-0x2fe8]. The composed picture is hud/weapons.h compose_weapon_picture on
// the canvas (M4 s2 DISPLAY).
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
            const WeaponTable table(wd.w920c);
            WeaponPictureInputs in;
            in.size_level = wd.size;  // DGROUP:0x9156
            in.player = wd.player;
            if (links().weapon_sheet_slot) in.sheet_slot = links().weapon_sheet_slot(wpn);
            // w = 0x18's roster bytes +0x7a / +0x7b are not kept by the port [OPEN].
            const WeaponPicture p = weapon_picture(table, wpn, in);
            if (p.kind == WeaponPicture::Kind::kGraphic && g.gfx) {
                try {
                    g_canvas->set_image(Img::kWeapon, g.gfx->load_graphic(p.graphic, 0, -1));
                } catch (const std::exception& x) {
                    app::log("hud: weapon picture " + std::to_string(p.graphic) + ": " + x.what());
                }
            } else if (p.kind == WeaponPicture::Kind::kComposed && g.gfx) {
                if (p.overlay != 0) {  // seg10:0x6c02(G, 0, -1); a failed load leaves G = 0
                    try {
                        g_canvas->set_image(Img::kWeaponOverlay, g.gfx->load_graphic(p.overlay, 0, -1));
                    } catch (const std::exception& x) {
                        app::log("hud: weapon overlay " + std::to_string(p.overlay) + ": " + x.what());
                    }
                }
                g_canvas->set_image(Img::kWeaponSheet, weapon_sheet(table, wpn, p, wd.size));
                compose_weapon_picture(*g_canvas, p);
                g_canvas->set_image(Img::kWeaponOverlay, nullptr);  // DeleteObject(G) [BYTES seg11:0x3340]
                g_canvas->set_image(Img::kWeaponSheet, nullptr);    // the slot's DC stays
                app::log("hud: seg11:0x2ff6 weapon " + std::to_string(wpn) + ": " +
                         std::to_string(g_canvas->width(Img::kWeapon)) + " x " +
                         std::to_string(g_canvas->height(Img::kWeapon)) + " from (" + std::to_string(p.sx) +
                         ", " + std::to_string(p.sy) + ") " + std::to_string(p.cell));
            }
        }
    }
    if (g_hud) g_hud->info().invalidate(kInfoWeapon);  // seg11:0x2fe8
}

void refresh_object(int pool, int rec) { g_host.refresh_object(pool, rec); }

void weapon_panel_repaint() {
    if (g_hud) g_hud->info().invalidate(kInfoWeapon);
}

void pictures_tick() {
    world::World& wd = world::current();
    if (g_hud) {
        const int m = g_host.info_mode();
        // DGROUP:0x6e18 + 0x1e·DGROUP:0xa76 is the pool word of mark (mode - 2) [BYTES
        // seg13:0x6800-0x6805]; outside modes 2 and 3 seg13:0x58dc returns at its own mode test.
        if ((m == kModeTarget || m == kModeMessage) && g_hud->info().mark(m - 2).pool != wd.roles.ships)
            g_hud->info().target_picture_now(0);  // seg13:0x58dc(0, 0)
    }
    icon_column_rows_tick();  // seg7:0x64ac(0, 0, -1)
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

// DGROUP:0x9524, the slot seg11:0xadf0 keeps (no other reader ported) [BYTES seg11:0xae08
// seg11:0xae55].
std::int16_t g_dg_9524 = 0;

// seg11:0xadf0 then seg11:0x2f8c on the program's World [BYTES seg11:0x71e4 seg11:0x71e7].
void tab_key() {
    world::World& wd = world::current();
    const int p = wd.player;
    if (p < 0 || p >= world::kShipCount) return;
    world::ShipRecord& r = wd.ship(p);
    // The ship byte +0xb8 + j; the record ends at 0x15a, the port's bound [DESIGN].
    const auto byte = [&r](int j) {
        const int off = 0xb8 + j;
        return off >= 0 && off < 0x15a ? int{world::sbyte_at(r, off)} : 0;
    };
    const TabSelection t = tab_select(r.weapon_slot, r.slots_cb(), byte, wd.play_mode);
    r.weapon_slot = static_cast<std::int16_t>(t.slot);
    g_dg_9524 = static_cast<std::int16_t>(t.dg_9524);
    if (t.clear_b50 && wd.pools.records(wd.roles.ships) > p) wd.ship_object(p).b50 = 0;
    app::log("hud: Tab seg11:0xadf0: slot " + std::to_string(t.slot));
    weapon_picture_refresh();
}

// The weapon keys of the key switch seg11:0x6b64 (hud/weapons.h weapon_key) ahead of the flight
// role's route_wave: WM_KEYUP VK_RETURN / 'Z' and WM_KEYDOWN VK_TAB, VK_RETURN, 'Z'. Returns
// false for any other message, which goes on (ESC's WM_KEYUP to the briefing hook) [DESIGN: the
// HUD's wrapper of route_input; the gate and the demo filter are seg11:0x6b64's].
bool weapon_keys(const shell::GameState& s, const shell::Input& in) {
    if (s.state() != shell::kWave) return false;
    if (in.msg != shell::kWmKeyUp && in.msg != 0x100) return false;
    world::World& wd = world::current();
    if (world::flight_globals().dg_9130 != 0) return false;  // seg18:0xd4e, as route_wave [OPEN]
    static bool remap_read = false;
    static world::KeyRemap remap;
    if (!remap_read) {  // seg11:0xb306's strings 2900 / 2901 [BYTES seg6:0x178d seg6:0x179f]
        remap_read = true;
        remap = world::KeyRemap::from_strings(lang_text(2900), lang_text(2901));
    }
    const std::uint16_t key = remap.map(static_cast<std::uint16_t>(in.wparam));  // seg11:0x6aa8
    const int p = wd.player;
    if (p < 0 || p >= world::kShipCount) return false;
    WeaponKeyState ks;
    const std::int16_t pstate = wd.pools.records(wd.roles.ships) > p ? wd.ship_object(p).state : std::int16_t{0};
    ks.gated = wd.ships[static_cast<std::size_t>(p)].lockout.get() > s.now() || pstate != 0;
    ks.demo = wd.player_game;  // DGROUP:0x95bc
    ks.play_mode = wd.play_mode;
    ks.weapon = world::selected_weapon(wd.ship(p));  // the signed byte +0xb8 + slot
    switch (weapon_key(in.msg, key, ks)) {
    case WeaponKey::kNone: return false;
    case WeaponKey::kIgnored: return true;
    case WeaponKey::kTab: tab_key(); return true;
    case WeaponKey::kFire:
        if (links().weapon_system)
            links().weapon_system();  // seg11:0xac86
        else
            app::log_todo("the weapon system", "seg11:0xac86");
        return true;
    }
    return false;
}

// The launcher's PostMessage(DGROUP:0xa7e, WM_PAINT) after the count or fuel change [BYTES
// seg11:0x7e9a-0x7ea6] (s2-display-verify.md claims 19-21): the port repaints the weapon panel
// after the frame in which the line's inputs changed [DESIGN: until MISSILE's launch calls the
// panel; a launch that leaves the line as it was repaints nothing, which looks the same since
// seg11:0x238e blacks and redraws the panel whole].
void weapon_line_watch() {
    static WeaponView last;
    static bool have = false;
    if (!g_hud) return;
    const WeaponView v = g_host.weapon();
    if (have && (v.name_id != last.name_id || v.count != last.count || v.counted != last.counted ||
                 v.no_count != last.no_count))
        g_hud->info().invalidate(kInfoWeapon);
    last = v;
    have = true;
}

void close_hud() {
    if (g_scroll) g_scroll->destroy();       // seg11:0x2fa-0x30a: DGROUP:0xa7a
    if (g_hud) g_hud->info().close();         // seg11:0x310-0x336: DGROUP:0xa7c, DGROUP:0xa7e
}

}  // namespace


// ---- M4 stage 3, track VISIT: state 6, the ambulance visit ---------------------------------------
namespace {

using app::now_tick;
using shell::Tick;

void visit_todo(const std::string& what, const char* address) {
    static std::set<std::string> seen;
    if (seen.insert(what).second) app::log_todo(what, address);
}

// The busy-waits of the visit: a GetCurrentTime loop without pumping, as the original's
// [BYTES seg16:0x123b-0x1258]; the screen is shown before the wait [DESIGN: the port's surface
// reaches the window only when presented].
void busy_ms(int ms) {
    app::present_frame();
    const Tick t0 = now_tick();
    while (now_tick() - t0 < static_cast<Tick>(ms)) Sleep(1);
}

// seg16:0x3784's calls out on the program (world/purchases.h).
class ProgramPurchaseHost final : public world::WorldPurchaseHost {
public:
    ProgramPurchaseHost() : WorldPurchaseHost(world::current()) {
        todo_log = visit_todo;
        on_design_mass = [](int d) { return world::design_mass(d); };
        on_game_state = [] { return g.game.state(); };
        on_ini_main_ship = [] {
            return g.ini ? g.ini->get_string(settings::kGameSection, "MainShip", "EAGLE", 20) : std::string("EAGLE");
        };
        // The design name DGROUP:0x9742: the player's design is INI MainShip's record (the
        // ship set-up, src/world/hooks.cpp) [OPEN: the 0xad-byte design table is M5].
        on_design_name = [](int) {
            return g.ini ? g.ini->get_string(settings::kGameSection, "MainShip", "EAGLE", 20) : std::string();
        };
        // seg16:0x3468(i, 3)'s seg14:0x11a8 and DGROUP:0x3a38 + 2i on the program's sheet side
        // (world/player_sheet.h) (M4 s5 SHEET).
        on_build_sheet = [](int ship) { world::build_sheet_program(world::current(), ship); };
        on_sheet_dc = [](int ship) { return world::sheet_dc_program(ship); };
        // PostMessage(DGROUP:0xa7e / 0xa7c, WM_PAINT): the IS_INFO panels due (ADR 0013 decision 4).
        on_post_info_paint = [] {
            if (Hud* h = hud()) {
                h->info().invalidate(kInfoTarget);
                h->info().invalidate(kInfoWeapon);
            }
        };
    }
};

ProgramPurchaseHost& purchase_host() {
    static ProgramPurchaseHost h;
    return h;
}

// seg16:0x2144's status data: the player's systems and fuel gauge, the design's lamp rows and
// the shield and cloak bytes.
VisitStatus visit_status() {
    VisitStatus st;
    world::World& w = world::current();
    world::ShipRecord& P = w.ship(w.player);
    for (int k = 0; k < 4; ++k) st.value[k] = world::system_word(P, k);
    const std::int16_t tank = purchase_host().design_word(P.design_index, 0x1e);
    st.value[4] = tank != 0 ? world::div32(static_cast<std::int32_t>(P.fuel) * 100, tank) : 0;  // seg5:0x110
    for (int k = 0; k < 4; ++k) st.row[k] = purchase_host().design_word(P.design_index, 0x22 + 2 * k);
    st.shield = P.be9 != 0;
    st.cloak = P.be7 != 0;
    return st;
}

std::unique_ptr<VisitGdi> g_visit_screen;

// The visit's calls on the program (world/visit.h VisitHost).
class ProgramVisitHost final : public world::VisitHost {
public:
    std::uint32_t now() override { return g.game.now(); }
    std::uint32_t current_time() override { return now_tick(); }
    void busy_wait(int ms) override { busy_ms(ms); }
    void pump_wait(int ms) override { busy_ms(ms); }  // seg10:0x740c's pump [OPEN: the port presents only]
    void sound(int id) override { is::sound::play(id); }
    void sound_at(int id, int a, int b, int mode) override { is::sound::play_at(id, a, b, mode); }
    std::string lang(int id) override { return lang_text(id); }
    std::string ship_name(int) override { return lang_text(2702) + purchase_host().design_name(0); }
    std::string pilot() override {
        return g.ini ? g.ini->get_string(settings::kGameSection, "ThePlayer", "", 12) : std::string();
    }
    bool ctrl_held() override { return (GetKeyState(VK_CONTROL) & 0x8000) != 0; }
    int w8f70() override { return 0; }  // DGROUP:0x8f70 [OPEN: its writer]
    int calibrate() override { return 2000; }  // DGROUP:0x8ffc [OPEN: the port's Calibrate]
    int depth() override { return g.depth; }
    bool limited() override { return g.edition.edition == 0; }
    void options_dialog() override;
    void body_chooser() override { visit_todo("the New Ship Body chooser of a dead ship (M8)", "seg10:0x82a2"); }
    void enter_calls() override {
        close();  // seg11:0x2f0
        visit_todo("the image-slot DCs freed and remade", "seg10:0xa856");
    }
    void palette_back() override {
        if (g.gfx) {
            g.gfx->palette_restore(0);    // seg10:0x6dd4(0)
            g.gfx->palette_scheme(8, 0);  // seg10:0x6e76(8, 0)
        }
    }
    void launch_enter() override {
        visit_todo("the launch scene seg15:0xcb0 / seg15:0x10be in state 6 (the desk's)", "seg16:0x1b7c");
        launch_due_ = now_tick() + 2800;  // [ORACLE core-0003: PREPARE FOR LAUNCH 37.477, back 40.297]
    }
    bool launch_frame() override { return static_cast<std::int32_t>(now_tick() - launch_due_) >= 0; }
    void recreate_dcs() override {}
    void seg18_7b2() override { visit_todo("seg18:0x7b2 after the visit", "seg16:0x1c3b"); }
    void to_screen(std::int16_t x, std::int16_t y, std::int16_t& sx, std::int16_t& sy) override {
        (void)x, (void)y;
        sx = sy = -1000;  // seg8:0x29ea: the next frame's view places the ship [OPEN]
    }
    void watch_windows(int) override {}
    void cursor_home() override { SetCursorPos(g.start_cursor.x, g.area.h); }
    void music_back() override { is::sound::music(1, 1); }  // [OPEN: DGROUP:0x9220 = 2 gives 2]
    void message(int ship, int code) override { post_message(ship, 7, 0, code); }
    void repaint_wave() override { g.game.repaint(); }
    void set_state(int s) override { g.game.set(s); }
    void demon_end() override { visit_todo("the Inner Demon ending (M6)", "seg16:0x7210"); }
    void demon_click() override { visit_todo("the Inner Demon subgame (M6)", "seg16:0x638a"); }
    void box_2322() override { app::message_box_ids(g.main, 2322, -1); }
    int rand() override { return shell::game_rng().rand(); }
    void todo(const std::string& what, const char* address) override { visit_todo(what, address); }

private:
    Tick launch_due_ = 0;
};

ProgramVisitHost g_visit_host;

world::VisitCtx visit_ctx() {
    world::World& w = world::current();
    world::VisitCtx c(w, world::shop(), world::visit_state(), g_visit_host, *g_visit_screen, purchase_host(),
                      world::missile_state().words);
    c.player_game = w.player_game != 0;
    c.restored = w.restored;
    c.plus = static_cast<std::int16_t>(g.edition.plus);
    return c;
}

// DialogBox(0xc2): the Options dialog through the slot src/dialogs fills.
void ProgramVisitHost::options_dialog() {
    auto& run = ambulance_dialog_slot();
    if (!run) {
        visit_todo("the Ambulance Options dialog (the dialogs library's slot is empty)", "seg16:0x133f");
        return;
    }
    world::World& w = world::current();
    dialogs::ShopEnv e(w, world::shop(), purchase_host(), world::missile_state().words);
    e.player_game = w.player_game != 0;
    e.edition = static_cast<std::int16_t>(g.edition.edition);
    e.plus = static_cast<std::int16_t>(g.edition.plus);
    e.served = w.w952e;
    e.lang = lang_text;
    e.body_chooser = [] { visit_todo("the New Ship Body chooser (M8; the Limited edition keeps TRANSFORMER)", "seg10:0x82a2"); };
    e.write_main_ship = [](const std::string& name) {
        if (!g.ini) return;
        g.ini->set(settings::kGameSection, "MainShip", name);
        try {
            g.ini->save();
        } catch (const std::exception& x) {
            app::log(std::string("INSPACE.INI: ") + x.what());
        }
    };
    dialogs::AmbulanceEnv a;
    a.env.instance = g.instance;
    a.env.owner = g.main;  // DGROUP:0x16
    a.env.store = g.store.get();
    a.env.lang = lang_text;
    a.env.sound = [](int id) { is::sound::play(id); };
    a.env.todo = visit_todo;
    a.env.screen_cx = GetSystemMetrics(SM_CXSCREEN);
    a.env.screen_cy = GetSystemMetrics(SM_CYSCREEN);
    a.shop = &e;
    a.font = g.font;  // DGROUP:0x90fe
    app::present_frame();
    run(a);
}

}  // namespace

// State 6's hooks: the frame (seg7:0x18f's seg16:0xcfe(0)), the repaint (seg11:0xafe case 6:
// seg16:0xcfe(1)), the input (seg11:0x785e: WM_KEYUP VK_ESCAPE with DGROUP:0x93a6 = 0 reaches
// seg16:0x638a(0, 0, 0, 1)) and the slots of world/visit.h [BYTES seg7:0x18f seg11:0x16b6
// seg11:0x787c seg11:0x788b].
void install_visit(app::App& app) {
    g_visit_screen = std::make_unique<VisitGdi>([] { return g.gfx.get(); }, [] { return g_canvas.get(); }, visit_status,
                                                [] {
                                                    const world::World& w = world::current();
                                                    return world::slot_image(world::kShipImageSlotBase + w.player);
                                                });
    g_visit_screen->lang = lang_text;
    g_visit_screen->limited = [] { return g.edition.edition == 0; };
    g_visit_screen->ship_cell = [] {  // the player's first sheet cell (M4 s4 fixer)
        world::World& w = world::current();
        VisitGdi::Cell c;
        if (w.player < 0 || w.player >= world::kShipCount) return c;
        const world::ObjectRecord& o = w.ship_object(w.player);
        c.x = o.gap, c.y = o.gap, c.side = o.size;
        return c;
    };
    auto& h = app.game.hooks();
    h.frame[shell::kAmbulance] = [](shell::GameState&) {
        world::VisitCtx c = visit_ctx();
        world::visit(c, 0);
    };
    h.repaint[shell::kAmbulance] = [](shell::GameState&) {
        world::VisitCtx c = visit_ctx();
        world::visit(c, 1);
    };
    auto prev_route = h.route_input;
    h.route_input = [prev_route](shell::GameState& s, const shell::Input& in) {
        if (s.state() == shell::kAmbulance) {
            if (in.msg == shell::kWmKeyUp && in.wparam == shell::kVkEscape && world::visit_state().demon == 0) {
                world::VisitCtx c = visit_ctx();
                world::visit_click(c, 0, 0, 1);
            }
            return;
        }
        if (prev_route) prev_route(s, in);
    };
    world::VisitLinks& l = world::visit_links();
    l.enter = [] {  // seg7:0x5fa-0x608
        // DGROUP:0x9e5b / 0x9e5d live in the Shop, which AmbulanceState refers to (world/ambulance.h)
        // (M4 s5, D-25).
        g.game.set(shell::kAmbulance);
        world::VisitCtx c = visit_ctx();
        world::visit_begin(c);
    };
    l.wave_setup = [](int ship) {  // seg14:0xd32
        world::World& w = world::current();
        world::wave_setup_purchases(w, purchase_host(), world::missile_state().words, ship, w.player_game != 0,
                                    w.restored, static_cast<std::int16_t>(g.edition.plus));
    };
    // seg14:0xd32's seg16:0x3784(i, 1) for the placement (roster.h purchase_pass), the one port
    // of seg16:0x3784 (world/purchases.h) (M4 s3 fixer, review M1).
    world::set_purchase_pass([](world::World& w, int ship, int mode) {
        if (mode == 1 && world::visit_links().wave_setup) return world::visit_links().wave_setup(ship);
        world::PurchaseCtx p{w, world::shop(), world::missile_state().words, purchase_host(), w.player_game != 0,
                             w.restored, static_cast<std::int16_t>(g.edition.plus)};
        world::apply_purchases(p, ship, mode);
    });
    // The ambulance's calls out (world/ambulance.h AmbulanceLinks) that the HUD answers: the
    // purchases of seg16:0x872 (seg16:0x3784(DGROUP:0x952e, 0)), seg3:0x6ec, seg11:0x2f8c, the
    // sounds and the sprite (M4 s3 fixer, review H2 / H6).
    world::AmbulanceLinks& al = world::ambulance_links();
    al.purchases = [](int ship, int mode) {
        world::World& w = world::current();
        world::PurchaseCtx p{w, world::shop(), world::missile_state().words, purchase_host(), w.player_game != 0,
                             w.restored, static_cast<std::int16_t>(g.edition.plus)};
        world::apply_purchases(p, ship, mode);
    };
    al.message = [](int recipient, int pool, int rec, int code) { post_message(recipient, pool, rec, code); };
    al.weapon_picture = [] { weapon_picture_refresh(); };
    al.sound = [](int id, int pool, int rec, int arg) { is::sound::play_at(id, pool, rec, arg); };
    al.play = [](int id) { is::sound::play(id); };
    al.draw_sprite = [](std::int16_t pr, int kind, int slot, int masked) {
        if (world::wave_links().draw_sprite) world::wave_links().draw_sprite(pr, kind, slot, masked);
    };
    al.todo = [](const std::string& what, const char* address) { visit_todo(what, address); };
    l.click = [](int x, int y) {
        if (g.game.state() != shell::kAmbulance) return;
        world::VisitCtx c = visit_ctx();
        world::visit_click(c, x, y, 0);
    };
}

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
    install_visit(app);  // state 6, the ambulance visit (M4 s3 VISIT)
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
        if (s.state() == shell::kWave) weapon_line_watch();  // the launch's WM_PAINT (M4 s2 DISPLAY)
        if (s.state() == shell::kWave || s.state() == shell::kHelp) g_hud->info().flush();
    };
    // The weapon keys ahead of the flight role's router (M4 s2 DISPLAY).
    auto prev_route = h.route_input;
    h.route_input = [prev_route](shell::GameState& s, const shell::Input& in) {
        if (weapon_keys(s, in)) return;
        if (prev_route) prev_route(s, in);
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
