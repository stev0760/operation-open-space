// The wave's integration (M3 s2 fixer, the review's H2): the calls of the wave pass and frame
// (src/world/update.h through flight_hooks.h WaveLinks) reach the drawing of src/world
// (draw.h, sprite.h, visibility.h, rock_images.h), the radar (hud/radar.h), the status tick
// (hud/status.h) and the HUD (hud/program.h); the HUD's slots (links()) reach the radar and the
// player's sprite; the wave start runs seg11:0x3cc2. One GdiBlitter draws on the surface, the
// window DC DGROUP:0x13dc (ADR 0013 decision 1). Win32 only.
#ifdef _WIN32
#include <windows.h>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "app/app.h"
#include "assets/asset_store.h"
#include "dialogs/audio_options.h"
#include "formats/ships.h"
#include "gfx/gfx.h"
#include "gfx/palette.h"
#include "gfx/ship_flame.h"
#include "gfx/ship_quick.h"
#include "gfx/ship_sheet.h"
#include "hud/gdi_canvas.h"
#include "hud/icon_images.h"
#include "hud/icon_column.h"
#include "hud/program.h"
#include "hud/radar.h"
#include "hud/status.h"
#include "hud/wave.h"
#include "hud/weapons.h"
#include "shell/rng.h"
#include "world/ambulance.h"
#include "world/draw.h"
#include "world/effects.h"
#include "settings/settings.h"
#include "world/entry_effect.h"
#include "world/flight_hooks.h"
#include "world/hooks.h"
#include "world/icon_mover.h"
#include "world/input.h"
#include "world/missile.h"
#include "world/pods.h"
#include "world/player_sheet.h"
#include "world/purchases.h"
#include "world/roster.h"
#include "world/rock_images.h"
#include "world/setup.h"
#include "world/sprite.h"
#include "world/tank.h"
#include "world/visibility.h"
#include "world/weapon_load.h"
#include "world/world.h"

namespace is::hud {

namespace {

using app::g;

std::unique_ptr<world::GdiBlitter> g_blit;
world::ImageSlotTable g_images{};  // DGROUP:0x39e8
Radar g_radar;
world::DrawGlobals g_draw;
world::VisibilityGlobals g_vis;
StatusState g_status;
gfx::ImagePtr g_strip;  // the strip image bound as g_draw.strip_dc
std::set<std::string> g_logged;
world::EntryTable g_entries;  // DGROUP:0x221c, the entry effects (world/entry_effect.h)

void todo_once(const std::string& what, const char* address) {
    if (g_logged.insert(what).second) app::log_todo(what, address);
}

world::World& w() { return world::current(); }

// seg8:0x0b76's DGROUP words from the World and the flight globals.
world::SpriteGlobals sprite_globals() {
    const world::WaveGlobals& fg = world::flight_globals();
    world::SpriteGlobals s;
    s.window_dc = world::kDcWindow;
    s.ship_pool = w().roles.ships;
    s.player = w().player;
    s.play_w = w().W;
    s.play_h = w().H;
    s.dg_951a = fg.autopilot;
    s.dg_238c = fg.dg_238c;
    s.dg_238e = fg.dg_238e;
    s.dg_952e = w().w952e;
    s.dg_9e49 = w().w9e49;
    // DGROUP:0x9e47, the ambulance's lift (seg8:0xd04's state-6 alternate source with the lift at
    // 15) (world/ambulance.h) (M4 s3 fixer, review M7).
    s.dg_9e47 = world::ambulance_state().lift;
    s.dg_913a = w().w913a;
    s.dg_913c = w().w913c;
    // DGROUP:0x5ea8, the start-up's sparse-dot pattern brush (rock_images.h WaveGraphics::mask)
    // [BYTES seg10:0xe4 seg10:0xf2] (M3 s4 fixer, draw request 2).
    s.player_mask_brush = world::wave_graphics().mask;
    return s;
}

world::Point16 to_screen(std::int16_t x, std::int16_t y) {
    const world::ScreenPoint p = world::flight_to_screen(x, y);
    return world::Point16{p.x, p.y};
}

// The icon strip DGROUP:0x3a02 the briefing entry built, as a DC id; DGROUP:0x3a02 is slot 13 of
// the table DGROUP:0x39e8 (icon_mover.h kIconStripSlot), which the icon mover draws from
// [BYTES seg7:0x3282] (M4 s2 fixer). The per-icon slots 14 + i (DGROUP:0x3a04 + 2i) are
// hud/icon_images.h's.
world::DcId strip_dc() {
    gfx::ImagePtr s = world::icon_strip();
    if (s != g_strip) {
        g_strip = s;
        g_draw.strip_dc = s ? g_blit->bind(s) : world::kDcNone;
    }
    g_images[world::kIconStripSlot] = g_draw.strip_dc;
    return g_draw.strip_dc;
}

// The frame's drawing state: DGROUP:0x3a38 is image slot 0x28 of the table DGROUP:0x39e8, the
// player's ship image (0x39e8 + 2·0x28) [BYTES seg7:0x1208] (M3 s2 fixer reading).
// The dissolve's calls into the HUD (world/draw.h DissolveHooks) (M4 s2 DISPLAY): seg7:0x65aa(0,
// i) at the last step redraws the icon's column row and the target display's picture
// [BYTES seg7:0x3b93] (core.md "The icon after capture"); seg7:0x2080(0, -1) changes nothing for
// pool 0 [BYTES seg7:0x208d seg7:0x20c9]; seg13:0x59e8 is [OPEN] (0: an off-screen icon's
// dissolve stops after its count step, as before).
class HudDissolveHooks final : public world::DissolveHooks {
public:
    void seg7_65aa(int a, int i) override { refresh_object(a, i); }
};
HudDissolveHooks g_dissolve_hooks;

world::DrawContext draw_context() {
    g_draw.sprite = sprite_globals();
    g_draw.state = static_cast<std::int16_t>(g.game.state());
    g_draw.dg_9130 = w().w9130;
    g_draw.dg_3a38 = static_cast<std::int16_t>(g_images[world::kShipImageSlotBase]);
    g_draw.last_effect = w().w9434;
    g_draw.detail = w().detail;
    // The byte DGROUP:0x8ffe the dissolve subtracts: the World's word (capture.h
    // dissolve_step_for at the set-up) [BYTES seg7:0x3a63] (M4 s2 fixer).
    g_draw.dissolve_step = static_cast<std::uint8_t>(w().w8ffe);
    strip_dc();
    return world::DrawContext{w().pools, w().ships, w().effects, g_images, g_draw, *g_blit,
                              to_screen, &g_dissolve_hooks};
}

void after_draw(const world::SpriteGlobals& s) {
    world::flight_globals().dg_238e = s.dg_238e;  // seg8:0xb76 clears it [BYTES seg8:0xc75]
}

RadarInputs radar_inputs() {
    const world::WaveGlobals& fg = world::flight_globals();
    const world::ViewState& v = world::flight_view();
    RadarInputs in{w().pools, w().ships};
    in.window_dc = world::kDcHudWindow;  // the HUD's blit, outside the pass's clip [DESIGN ADR 0013]
    in.hud_gate = hud() != nullptr && hud()->gate();
    in.radar_first = w().roles.radar_first;
    in.ship_pool = w().roles.ships;
    in.player = w().player;
    in.last_ship = w().last_ship;
    in.race = w().handover.race;
    in.dg_951a = fg.autopilot;
    in.alt_centre_x = v.cam_x;
    in.alt_centre_y = v.cam_y;
    in.view_x = v.x0;
    in.view_y = v.y0;
    in.play_w = w().W;
    in.play_h = w().H;
    in.shift = w().shift;
    in.view_rects = v.active_count;
    in.hud_top = static_cast<std::int16_t>(g.area.hud_top);
    in.detail = w().detail;
    return in;
}

bool ready() { return g_blit && g.gfx && !g.gfx->surface().empty(); }

// ---- the wave frame's calls (world::WaveLinks)

void draw_sprite(std::int16_t pr, int kind, int slot, int masked) {
    if (!ready()) return;
    world::SpriteGlobals s = sprite_globals();
    if (slot == world::kIconStripSlot) strip_dc();
    const world::DcId src =
        slot >= 0 && slot < world::kImageSlots ? g_images[static_cast<std::size_t>(slot)] : world::kDcNone;
    world::draw_sprite(w().pools, pr, static_cast<std::int16_t>(kind), src,
                       static_cast<std::int16_t>(masked), s, to_screen, *g_blit);
    after_draw(s);
}

void player_sprite() {
    if (!ready()) return;
    world::DrawContext c = draw_context();
    // seg7:0x1208 clears DGROUP:0x58a on its drawing path only [BYTES seg7:0x13a2]: the frame's
    // copy (flight_hooks.h FrameGlobals::dg_58a, which seg7:0x5e04 sets) with it (M4 s5 EFFECTS).
    const bool drawn = world::player_sprite_drawn(c);
    world::draw_player_sprite(c);
    if (drawn) world::frame_globals().dg_58a = 0;
    // DGROUP:0x1e5c is one word set: the drawing's copy is the stars' exclusion box
    // [BYTES seg7:0x12e4 seg8:0x17a0] (M3 s4 fixer, draw request 3; inert until the player's
    // sprite draws, M5).
    w().player_cell = g_draw.player_rect;
    after_draw(g_draw.sprite);
}

void dissolve(int i) {
    if (!ready()) {
        // Without the surface the step still counts, so the icon ends: byte +0x51 -= DGROUP:0x8ffe,
        // floor 0 [BYTES seg7:0x3a63-0x3a79] [DESIGN: the blits need the surface].
        if (i < 0 || i >= w().pools.records(world::kPoolIcons)) return;
        world::ObjectRecord& o = w().pools.rec(world::kPoolIcons, i);
        o.race_status = static_cast<std::uint8_t>(o.race_status - static_cast<std::uint8_t>(w().w8ffe));
        if (static_cast<std::int8_t>(o.race_status) < 0) o.race_status = 0;
        return;
    }
    world::DrawContext c = draw_context();
    world::dissolve_icon(c, i);
}

void visibility() {
    g_vis.dg_9130 = w().w9130;
    g_vis.dg_9e49 = w().w9e49;
    g_vis.dg_952e = w().w952e;
    g_vis.radar_first = w().roles.radar_first;
    g_vis.ship_pool = w().roles.ships;
    g_vis.count = world::frame_globals().dg_9146;  // the frame set DGROUP:0x9146 = 0 before the call
    world::VisibilityHost vh;
    vh.pool8_3d80 = [](int rec) -> std::int16_t {
        return rec >= 0 && rec < world::kPool8Max ? w().pool8[static_cast<std::size_t>(rec)].at(0x30)
                                                 : std::int16_t{0};
    };
    vh.seg11_9fdc = [](int, int) { todo_once("seg11:0x9fdc from the visibility pass", "seg11:0x9fdc"); };
    world::visibility(w().pools, world::flight_view(), g_vis, shell::game_rng(), vh);
    world::frame_globals().dg_9146 = g_vis.count;
}

void radar_refresh() {
    if (!ready() || !g_radar.created()) return;
    g_radar.refresh(radar_inputs(), *g_blit);
}

// ---- the cloak and the shield (world/input.h) (M4 s3 FUEL)

world::EntryContext entry_context();

// The toggles' calls: the vanish through the entry effect (world/entry_effect.h), the flame
// seg11:0xb09c outside the frame (world.h briefing_flame), M7's sounds, the ENG_LANG strings and
// the pilot name DGROUP:0x380c (INI ThePlayer, 12 bytes, as the state-1 set-up reads it) [BYTES
// seg10:0x8c].
class HudSystemsHost final : public world::SystemsHost {
public:
    void vanish_buffer(int pool, int ship) override {
        if (!ready()) return;
        world::EntryContext c = entry_context();
        world::entry_buffer(c, pool, ship);  // seg12:0x2556
    }
    void vanish_start(int mode, int pool, int ship) override {
        if (!ready()) return;
        world::EntryContext c = entry_context();
        world::entry_start(c, static_cast<std::uint8_t>(mode), pool, ship);  // seg7:0x13b2
    }
    // seg10:0x7328, ordinal 17 into column 16 of the sheet (world/player_sheet.h) (M4 s5 SHEET).
    void shield_render(int i) override {
        if (ready()) world::shield_render_program(w(), i);
    }
    void flame(int on, int ship) override { world::briefing_flame(w(), on, ship); }
    void sound(int id, int pool, int rec, int mode) override { is::sound::play_at(id, pool, rec, mode); }
    std::string lang(int id) override {
        try {
            if (g.store) return g.store->find_string(static_cast<std::uint16_t>(id)).value_or("");
        } catch (const std::exception&) {
        }
        return {};
    }
    std::string pilot() override {
        return g.ini ? g.ini->get_string(settings::kGameSection, "ThePlayer", "", 12) : std::string();
    }
    void strategy_line(const std::string& text) override {
        // seg7:0x1f38 draws "Cmd Mode: " + DGROUP:0x8630 only in Strategy mode [BYTES seg7:0x1f45]
        if (w().play_mode == 1) todo_once("the strategy command line", "seg7:0x1f38");
        app::log("wave: seg3:0x332e " + text);
    }
};
HudSystemsHost g_systems;

world::SystemsCtx systems_ctx() {
    const world::WaveGlobals& fg = world::flight_globals();
    return world::SystemsCtx{w(), fg.now, fg.game_frames, g_systems};
}

// WM_KEYUP VK_CAPITAL (seg11:0x6dd6) ahead of the flight role's router (world/input.h
// capslock_action) [BYTES seg11:0x6d19 seg11:0x6dd6-0x6e0a]. The remap seg11:0xb306 leaves
// 0x14 as it is (its table holds letters) [DESIGN].
bool caps_key(const shell::GameState& s, const shell::Input& in) {
    if (s.state() != shell::kWave || in.msg != shell::kWmKeyUp || in.wparam != world::kVkCapital) return false;
    if (world::flight_globals().dg_9130 != 0) return false;  // seg18:0xd4e, as route_wave [OPEN]
    world::World& wd = w();
    const int p = wd.player;
    if (p < 0 || p >= world::kShipCount || wd.pools.records(wd.roles.ships) <= p) return false;
    const world::ShipRecord& r = wd.ship(p);
    const bool gated = r.lockout.get() > s.now() || wd.ship_object(p).state != 0;
    world::WaveGlobals& fg = world::flight_globals();
    fg.now = s.now();
    world::SystemsCtx sc = systems_ctx();
    switch (world::capslock_action(gated, wd.player_game, wd.play_mode, r.be9, r.be7)) {
    case world::CapsAction::kShield: world::shield_toggle(sc, p); break;  // seg11:0x4b00(player)
    case world::CapsAction::kCloak: world::cloak_toggle(sc, p); break;    // seg11:0x4c72(player)
    case world::CapsAction::kNone: break;
    }
    return true;
}

void fuel_mass() {
    StatusHooks h;
    h.update = [](int arg) {
        if (Hud* x = hud()) x->panels().update(arg, x->gate());
    };
    h.score_shown = [] { return hud() ? hud()->panels().state().score_shown : 0u; };
    h.score = [] { return links().score ? links().score() : w().score; };  // DGROUP:0x1e58 (M4 s2 DISPLAY)
    h.design_mass = [](int d) { return world::design_mass(d); };
    // The cloak's auto-off below 120 litres and its notice (world/input.h cloak_toggle,
    // strategy_notice), the result ignored [BYTES seg11:0x1c8f-0x1c9e] (M4 s3 FUEL).
    h.seg11_4c72 = [](int i) {
        world::SystemsCtx sc = systems_ctx();
        world::cloak_toggle(sc, i);
    };
    h.seg3_332e = [](std::uint32_t ids) {
        world::SystemsCtx sc = systems_ctx();
        world::strategy_notice(sc, ids);
    };
    h.seg13_2dcc = [] { todo_once("radar brushes", "seg13:0x2dcc"); };
    h.seg11_1d7c = [] { world::repair_tick(); };  // the auto-repair (world/damage.h auto_repair) (M4 s1 fixer)
    status_tick(w(), world::flight_globals().now, g_status, h);
}

// ---- the player's sheet (world/player_sheet.h) and the entry effect (world/entry_effect.h),
// M5 s1 SPRITE

// The renderer seg14:0x11a8 reaches through ordinal 2: ISPACE.DLL seg6:0x11c as the render
// track ports it (src/gfx/ship_sheet.h ship_sheet_for) on the game palette DGROUP:0x920e, its
// logical-palette indices taken as the port's index pixels (ADR 0007 decision 2) and the
// BLACKNESS-only pixels as index 0. When it gives no sheet, the stand-in fills every heading with
// the unrotated frame (world::unrotated_sheet) [DESIGN]: the body graphic recoloured as the New
// Game preview does (PaletteManager::recolour_graphic with SHP +0x07 and +0x08,
// src/dialogs/preview.cpp ship_bitmap), then seg5:0x2080's mapping (Gfx::image_from_dib). A body
// from a .DIB file (+0x21 = 0xffff) is [OPEN].
bool render_sheet(const world::SheetRequest& req, world::SheetPixels& px) {
    if (!g.store || !g.gfx) return false;
    if (req.graphic_id == fmt::kBodyBitmapFromFile) {
        todo_once("ship sheet from a .DIB file " + req.file, "seg14:0x11a8");
        return false;
    }
    try {
        fmt::Dib dib = g.store->sprite(static_cast<std::uint16_t>(req.graphic_id));
        const gfx::ShipSheet sh =
            req.sel == 0 ? gfx::ship_sheet_for(dib, req.graphic_id, g.gfx->palettes(), g.gfx->palette(),
                                               world::sheet_args(req))
                         : gfx::ShipSheet{};
        if (req.sel != 0) todo_once("the quick sheet renderer", "ISPACE.DLL seg6:0x78ba");
        if (world::sheet_pixels_from(sh, px)) return true;
        std::vector<std::uint32_t> table = dib.palette;
        const std::size_t n = table.size();
        gfx::PaletteManager pm;
        pm.recolour_graphic(0, req.graphic_id, table, 0, req.colour_a, req.colour_b, 0, false);
        table.resize(std::max(n, table.size()));
        dib.palette = table;
        const gfx::ImagePtr im = g.gfx->image_from_dib(dib);
        if (!im || im->empty()) return false;
        std::vector<std::uint8_t> src(static_cast<std::size_t>(im->width()) * im->height());
        for (int y = 0; y < im->height(); ++y)
            for (int x = 0; x < im->width(); ++x)
                src[static_cast<std::size_t>(y) * im->width() + x] = im->at(x, y);
        px = world::unrotated_sheet(src, im->width(), im->height(), req);
        todo_once("ship sheet: no render, the stand-in draws the unrotated frame", "ISPACE.DLL seg6:0x11c");
        return px.w > 0;
    } catch (const std::exception& x) {
        app::log(std::string("hud: ship sheet: ") + x.what());
        return false;
    }
}

// seg10:0x7328's ordinal 17 (gfx/ship_quick.h quick_cells_for) on the game palette DGROUP:0x920e,
// for world/player_sheet.h shield_render (M4 s5 SHEET). A body from a .DIB file is [OPEN].
bool render_quick(const world::QuickRequest& q, gfx::QuickCells& cells) {
    if (!g.store || !g.gfx) return false;
    if (q.graphic_id == fmt::kBodyBitmapFromFile) {
        todo_once("the shield's quick cells from a .DIB file " + q.file, "seg10:0x7328");
        return false;
    }
    try {
        const fmt::Dib dib = g.store->sprite(static_cast<std::uint16_t>(q.graphic_id));
        cells = gfx::quick_cells_for(dib, q.graphic_id, g.gfx->palettes(), g.gfx->palette(), q.colour_a,
                                     q.colour_b, q.heading, q.gap_word);
        return cells.ok();
    } catch (const std::exception& x) {
        app::log(std::string("hud: quick cells: ") + x.what());
        return false;
    }
}

// The weapon words DGROUP:0x2f54 the wave reads and writes (world/missile.h MissileState::words):
// seg10:0x1fb2's table for the edition DGROUP:0x920c (hud/weapons.h fill_weapon_words), copied
// in whole when the edition changes, so seg10:0x533a's run-time W(w)-4 stays between the sheet
// builds [DESIGN: one mutable copy, M4 s2 fixer]. The cache starts empty: the registered
// edition is -1 (M4 s5, triage S3-10).
std::optional<int> g_words_edition;
void fill_weapon_words(const world::World& wd) {
    if (!hud::fill_weapon_words(wd.w920c, world::missile_state().words, g_words_edition)) return;
    app::log("hud: seg10:0x1fb2 weapon words for edition " + std::to_string(wd.w920c));
}

// seg10:0x4412(k, pool, id, n), the animation-4 sheet of a weapon whose W(w)+0x06 = 4 (weapons
// 10, 16, 17; hud/weapons.h anim4_sheet_plan, render_anim4_sheet) [BYTES seg10:0x4412-0x4648]:
// the graphic through seg10:0x6c02(id, 3, -1) [BYTES seg10:0x4421-0x4429], its bmHeight
// (GetObject, seg10:0x443a), the gap into the pool's record 0 +0x24, the sheet into
// DGROUP:0x39e8[k], then DeleteDC / DeleteObject of the graphic [BYTES seg10:0x45fd-0x4626]. The
// original overwrites DGROUP:0x39e8[k]; the port frees the image it held [DESIGN]. seg10:0x6308(k,
// -1, -1, -1) returns at once at `Size` 1 and is [OPEN] at the other sizes [BYTES seg10:0x463a].
// player_sheet.h load_weapon_sheet calls it as its Anim4Renderer (M4 s5 WEAPONS).
world::DcId anim4_sheet(int k, int pool, int id, int n) {
    if (!ready() || k < 0 || k >= world::kImageSlots) return world::kDcNone;
    const world::DcId graphic = g_blit->load_graphic(id, 3, -1);
    int h = 0;
    if (graphic != world::kDcNone)
        if (const std::shared_ptr<gfx::Surface> im = g_blit->image(graphic)) h = im->height();
    const Anim4Sheet plan = anim4_sheet_plan(h, n);
    anim4_gap_record(w().pools, pool, plan.gap);
    world::DcId& slot = g_images[static_cast<std::size_t>(k)];
    if (slot != world::kDcNone) g_blit->delete_image(slot);
    slot = render_anim4_sheet(*g_blit, graphic, plan);
    if (graphic != world::kDcNone) g_blit->delete_image(graphic);
    if (w().size != 1) todo_once("seg10:0x6308 on the animation-4 sheet", "seg10:0x6308");
    app::log("hud: seg10:0x4412 sheet " + std::to_string(k) + " of graphic " + std::to_string(id) + ": h " +
             std::to_string(h) + ", n " + std::to_string(n) + ", " + std::to_string(plan.width) + " x " +
             std::to_string(plan.height) + ", gap " + std::to_string(plan.gap));
    return slot;
}

// seg10:0x533a(w) for seg12:0x6d64 (world/weapon_load.h): player_sheet.h load_weapon_sheet, the
// sheet build's one-weapon load, on the program's image table [BYTES seg12:0x6ddc
// seg10:0x533a-0x5464].
void weapon_image(int wpn) {
    if (!ready()) return;
    const int quick = g.ini ? g.ini->get_int(settings::kGameSection, "Quick", 0) : 0;  // DGROUP:0x931e
    const bool loaded = world::load_weapon_sheet(*g_blit, g_images, w().pools, world::missile_state().words, wpn,
                                                 world::weapon_sheet_sel(quick, g.game.state(), w().frames),
                                                 w().size, render_sheet, anim4_sheet);
    app::log("hud: seg10:0x533a for weapon " + std::to_string(wpn) + ": " + (loaded ? "loaded" : "no new sheet") +
             ", W-4 = " + std::to_string(world::missile_state().words.word(wpn, -4)));
}

// seg16:0x3468(player, 3)'s sheet: seg14:0x11a8 into slot 0x28 + player, object +0x3a, and
// seg14:0x0's ship byte +0x5b from the sheet's height [BYTES seg16:0x3621 seg16:0x362b
// seg16:0x3645 seg14:0xd59 seg14:0xc3e]. It runs in the placement seg9:0x6624 (seg14:0x0(-1))
// at the briefing entry (world::on_briefing_placement, install_wave) [BYTES seg9:0x688a
// seg14:0xd32], from the records the HUD reads (player_design_records) [DESIGN: the player's
// sheet only; the port places no other ship at the briefing]. After the sheet seg14:0x0 builds
// the flame strip (seg10:0x5c42, seg10:0x5d3c(i)) [BYTES seg14:0xc7e]: seg10:0x5c42 calls
// seg10:0x7032 on every call (s2-claims-verify.md sprite 27), so the flame's headings are made
// again each time on the palette in force. The object's size, gap and extent are
// the ship set-up's (src/world/hooks.cpp ship_setup_player, setup.h ship_cell_sizes); a raw
// side that disagrees with them is logged.
void build_player_sheet(world::World& wd) {
    if (!ready()) return;
    fmt::ShipRecord shp;
    fmt::BodyRecord body;
    if (!player_design_records(shp, body)) {
        app::log("hud: the player's sheet: no design records");
        return;
    }
    const int i = wd.player;
    if (i < 0 || i >= world::kShipCount || wd.pools.records(wd.roles.ships) <= i) return;
    world::ShipRecord& s = wd.ship(i);
    world::ObjectRecord& o = wd.ship_object(i);
    world::SheetRequest req;
    req.slot = world::kShipImageSlotBase + i;
    req.graphic_id = body.bitmap_id;
    req.file = body.bitmap_file;
    req.colour_a = shp.byte_07;  // design byte +0x18 [BYTES seg14:0x129c-0x12d0] [OPEN: = SHP +0x07]
    req.colour_b = shp.byte_08;  // design byte +0x1a
    // Ship byte +0xe9 (the body's Shield bit) comes from the ship set-up's apply_flight_design
    // [BYTES seg14:0x21f2 seg14:0x21f6].
    req.mode = world::sheet_mode(s.has_flame, s.be9);
    const int quick = g.ini ? g.ini->get_int(settings::kGameSection, "Quick", 0) : 0;  // DGROUP:0x931e
    req.sel = world::sheet_sel(quick, g.game.state(), wd.frames, i, wd.player);
    req.scale = world::sheet_scale_flag(wd.size, 0, req.sel);
    req.size_level = wd.size;
    // seg16:0x3468(i, 3) builds the sheet only under seg14:0xd1a's test and its own early return
    // (world.md Ship sheets, Path; C067) (M4 s4 fixer): a later wave without seg14:0x0's local
    // flag keeps the old sheet in the slot, and seg14:0x0's +0x5b and flame strip then read that
    // sheet, here the pixels of its last build [DESIGN].
    static world::SheetBuild last;
    const bool has_dc = g_images[static_cast<std::size_t>(req.slot)] != world::kDcNone;
    const std::int16_t special =
        s.design_index >= 0 && s.design_index < world::kShipCount
            ? wd.design_ratings[static_cast<std::size_t>(s.design_index)].special
            : std::int16_t{-1};
    world::SheetBuild b;
    if (world::placement_rebuilds_sheet(wd.sheet_due, world::shop().new_body, special, has_dc)) {
        b = world::rebuild_ship_sheet(*g_blit, g_images, s, o, i, req, render_sheet);
        last = b;
    } else {
        b = last;
        // The sheet as drawn, its flame pastes and wing cuts included (world/player_sheet.h
        // sheet_copy), for seg14:0x0's flame strip (M4 s5 SHEET) [DESIGN].
        if (const world::SheetPixels* c = world::sheet_copy(i)) b.pixels = *c;
        o.w3a = static_cast<std::int16_t>(g_images[static_cast<std::size_t>(req.slot)]);  // the slot's DC
        app::log("hud: seg16:0x3468 keeps ship " + std::to_string(i) + "'s sheet (wave " + std::to_string(wd.wave) + ")");
    }
    // seg14:0x11a8's tail after every build path: the launched weapons' images seg10:0x533a
    // into slots 0x30 ... 0x38 and W(w)-4 [BYTES seg14:0x13f3-0x1425] (M4 s2 fixer). The port
    // runs it at every placement, also when the sheet is kept, so a later wave has the images
    // of the roster's weapons [DESIGN] [OPEN: which path builds them in the original then].

    fill_weapon_words(wd);
    const int wsheets = world::build_weapon_sheets(*g_blit, g_images, wd.pools, world::missile_state().words, s,
                                                   world::weapon_sheet_sel(quick, g.game.state(), wd.frames),
                                                   wd.size, render_sheet, anim4_sheet);
    app::log("hud: seg10:0x533a weapon sheets: " + std::to_string(wsheets) + ", W(8)-4 = " +
             std::to_string(world::missile_state().words.word(world::kWeaponRadarMissiles, -4)));
    if (b.rendered) {
        s.anim_last = world::sheet_last_row(b.bm_height, o.size, o.gap);  // ship byte +0x5b
        // The sheet's cell is ordinal 5's side << the scale flag; the object's size comes from
        // seg14:0x0's step. They part at Size 0, where seg10:0x6308's rescale of the sheet is
        // not ported (review M3) [OPEN].
        // seg10:0x5c42 + seg10:0x5d3c(i) [BYTES seg14:0xc7e seg10:0x5c80 seg10:0x7041-0x7051].
        try {
            const int f = gfx::flame_cell(wd.size);
            const gfx::FlameHeadings fh = gfx::flame_headings(
                gfx::flame_bitmap_for(g.store->sprite(gfx::kFlameGraphic), g.gfx->palettes(), g.gfx->palette()), f);
            world::ship_flame_after_sheet(*g_blit, fh, i, b, o, s,
                                          world::flame_design(shp.flags, body.flags, body.byte_19), wd.size);
            app::log("hud: seg10:0x5d3c flame strip for ship " + std::to_string(i) + ", f " + std::to_string(f) +
                     ", +0xe4 " + std::to_string(s.has_flame));
        } catch (const std::exception& x) {
            app::log(std::string("hud: flame strip: ") + x.what());
        }
        const int cell = b.side << req.scale, cell_gap = b.gap << req.scale;
        if (cell != o.size || cell_gap != o.gap)
            todo_once("ship sheet cell " + std::to_string(cell) + " gap " + std::to_string(cell_gap) +
                          " against the object's size " + std::to_string(o.size) + " gap " +
                          std::to_string(o.gap) + " at Size " + std::to_string(wd.size),
                      "seg10:0x6308");
    }
    app::log("hud: seg14:0x11a8 player sheet slot " + std::to_string(req.slot) + " body " +
             std::to_string(req.graphic_id) + " mode " + std::to_string(req.mode) + ": " +
             (b.rendered ? "rendered, side " + std::to_string(b.side) + " gap " + std::to_string(b.gap) +
                               ", +0x5b " + std::to_string(s.anim_last)
                         : std::string("none")));
}

// The effect's calls outside the drawing (world/entry_effect.h): the sounds through M7's
// seg10:0x754a and the vanish's end (world/gate_exit.h vanish_end) (M4 s1 fixer); seg2:0x170e's
// menu state is the shell's [OPEN].
class WaveEntryHooks final : public world::EntryHooks {
public:
    void sound(int id, int pool, int rec, int arg) override { is::sound::play_at(id, pool, rec, arg); }
    void menu_refresh() override { todo_once("menu state after the entry", "seg2:0x170e"); }
    void vanished(int pool, int rec) override {
        world::run_vanish_end(pool, rec, world::flight_globals().now);  // [BYTES seg7:0x181e-0x19ff]
    }
};
WaveEntryHooks g_entry_hooks;

world::EntryContext entry_context() {
    world::EntryContext c{w().pools, w().ships, g_entries, *g_blit, shell::game_rng(),
                          world::frame_globals().dg_9148};
    c.size_level = w().size;
    c.ship_pool = w().roles.ships;
    c.player = w().player;
    c.window_dc = world::kDcWindow;
    c.hooks = &g_entry_hooks;
    return c;
}

// ---- the icon's crack and fragments (world/collide.h CrackPlan, FragmentPlan) (M4 s2 fixer)

std::array<world::DcId, 64> g_fragment_dc{};  // DGROUP:0x4e94 + 2r, pool-1 record r's picture
world::DcId g_cracks = world::kDcNone;        // "CRACKS" (DGROUP:0xb8b)
world::DcId g_piece = world::kDcNone;         // "piece" (DGROUP:0xb92)

// seg7:0x233c's two BitBlts onto the strip DC [BYTES seg7:0x23cf-0x24a1]. seg10:0x6d38 loads
// the graphic on each call; the port loads it once [DESIGN]. seg10:0x6308(-1, -1, bitmap) of the
// amount != 0 path returns at once at Size 1 and is [OPEN] at the other sizes.
void icon_crack(const world::CrackPlan& p) {
    if (!ready()) return;
    if (g_cracks == world::kDcNone) g_cracks = g_blit->load_bitmap("CRACKS");
    if (p.scaled && w().size != 1) todo_once("seg10:0x6308 on CRACKS", "seg10:0x6308");
    const world::DcId strip = strip_dc();
    g_blit->bit_blt(strip, p.dest_x, p.dest_y, p.side, p.side, g_cracks, p.src_merge_x, 0, world::kRopMergePaint);
    g_blit->bit_blt(strip, p.dest_x, p.dest_y, p.side, p.side, g_cracks, p.src_and_x, 0, world::kRopSrcAnd);
}

// seg7:0x2768's CreateCompatibleBitmap(DGROUP:0x13dc, side, side) at DGROUP:0x4e94 + 2r [BYTES
// seg7:0x29ce-0x2a20]; the port drops the record's previous picture [DESIGN].
void fragment_bitmap(int r, std::int16_t side) {
    if (!ready() || r < 0 || r >= static_cast<int>(g_fragment_dc.size())) return;
    world::DcId& d = g_fragment_dc[static_cast<std::size_t>(r)];
    if (d != world::kDcNone) g_blit->delete_image(d);
    d = g_blit->create_image(side, side);
}

// seg7:0x2768's pictures [BYTES seg7:0x2a20-0x2c82]; "piece" goes through seg10:0x6308(-1, -1),
// which returns at once at Size 1 [OPEN: the other sizes].
void fragment_pictures(const world::FragmentPlan& p) {
    if (!ready()) return;
    if (g_piece == world::kDcNone) g_piece = g_blit->load_bitmap("piece");
    if (w().size != 1) todo_once("seg10:0x6308 on piece", "seg10:0x6308");
    const world::DcId strip = strip_dc();
    for (int q = 0; q < 4; ++q) {
        const int r = p.rec[static_cast<std::size_t>(q)];
        if (r < 0 || r >= static_cast<int>(g_fragment_dc.size())) continue;
        const world::DcId d = g_fragment_dc[static_cast<std::size_t>(r)];
        if (d == world::kDcNone) continue;
        g_blit->pat_blt(d, 0, 0, p.side, p.side, world::kBlackness);
        g_blit->bit_blt(d, p.piece_at, p.piece_at, p.piece, p.piece, g_piece, 0, 0, world::kRopSrcCopy);
        g_blit->bit_blt(d, p.quad_at, p.quad_at, p.quad, p.quad, strip, p.src_x[static_cast<std::size_t>(q)],
                        p.src_y[static_cast<std::size_t>(q)], world::kRopSrcCopy);
    }
}

}  // namespace

std::vector<std::pair<int, int>> wave_visible() {
    std::vector<std::pair<int, int>> v;
    const int n = g_vis.count < 0 ? 0 : (g_vis.count > world::kVisibleMax ? world::kVisibleMax : g_vis.count);
    for (int k = 0; k < n; ++k)  // cbw of each byte [BYTES seg13:0x2271-0x2283]
        v.emplace_back(static_cast<std::int8_t>(g_vis.pool[static_cast<std::size_t>(k)]),
                       static_cast<std::int8_t>(g_vis.rec[static_cast<std::size_t>(k)]));
    return v;
}

std::int16_t g_tank_made_for = -1;  // DGROUP:0x17c0, the DGROUP:0x9214 of slot 4's image

void load_wave_resources() {
    if (!ready()) return;
    // seg9:0x26f6's seg10:0x565a (the fuel tank, slot 4) and seg9:0x2733's seg10:0x42ee (the
    // pods, slot 0x1d) [BYTES seg9:0x26f6 seg9:0x2733] (M4 s3 FUEL).
    world::build_tank_image(*g_blit, g_images, w().w9214, g_tank_made_for);
    world::build_pod_image(*g_blit, g_images);
    // Image slot 5 (DGROUP:0x39f2), the logo strip seg10:0x39a8 builds once: the ambulance draws
    // its cell from it (ambulance.cpp seg16:0xa2's head) [BYTES seg10:0x39b2 seg10:0x3a24]
    // (shell.md "Strip") (M4 s3 fixer, review M7).
    if (g_images[5] == world::kDcNone) {
        if (gfx::ImagePtr logo = g.gfx->logo_strip()) g_images[5] = g_blit->bind(logo);
    }
    // seg9:0x22b4's graphics chain: the rock images seg10:0x3a2e, the gate images seg10:0x3ea2
    // (seg9:0x26e5) and the wave's brushes [BYTES seg9:0x22b4 seg9:0x26e5] (M3 s4 fixer, review
    // H1).
    world::build_wave_images(*g_blit, g_images, w().w740);
    // seg11:0x0's radar part: DGROUP:0x90f0 = 1 on the port's surface (ADR 0007) [BYTES seg11:0xd].
    if (!g_radar.created()) {
        const world::BrushId metal = g_blit->bind_brush(metal_brush());
        g_radar.create(*g_blit, true, metal);
        app::log("hud: seg11:0x0 radar created (86 x 71, RADSHIP, 8 brushes)");
    }
}

void install_wave(app::App& app) {
    g_blit = std::make_unique<world::GdiBlitter>(*app.gfx);
    // The gate test's list DGROUP:0x507c / 0x50e0 is the program's one visibility list (M3 s4
    // fixer, review M7: a static, never a caller's local).
    world::set_last_visibility(&g_vis);
    world::WaveLinks& l = world::wave_links();
    l.draw_sprite = draw_sprite;
    l.player_sprite = player_sprite;
    l.dissolve = dissolve;
    l.visibility = visibility;
    l.radar_refresh = radar_refresh;
    l.fuel_mass = fuel_mass;
    // seg11:0x894 at the pass's step 1: the red button of C's reading [OPEN Turn follow-up,
    // Red button].
    l.turn_followup = [] {
        if (Hud* h = hud()) h->panels().button();
    };
    // SelectClipRgn(DGROUP:0x13dc, DGROUP:0x12) [BYTES seg7:0x842-0x84a]: the play rectangle
    // of seg2:0xca4 (shell::clip_rect) for the wave's blits.
    l.select_clip = [] {
        if (!g_blit) return;
        const shell::Rect& r = g.clip.play;
        g_blit->set_window_clip(world::rect16(r.left, r.top, r.right, r.bottom));
    };
    l.direction_mark = [](int d) {
        if (Hud* h = hud()) h->info().update_mark(d);  // seg13:0x24aa(d)
    };
    // ---- M4 s1 fixer: the stage's links into the HUD
    // The image table DGROUP:0x39e8 for the effects, the burnt sheet and the debris.
    l.images = []() -> const world::ImageSlotTable* { return &g_images; };
    // seg7:0x119c(2, 0) at the speed or message display's end [BYTES seg7:0xbe3-0xbe7].
    l.info_target = [] { info_mode_119c(2); };
    // ---- M4 s3 FUEL: the small movers' drawing and the ship systems
    world::SmallMoverLinks& sm = world::small_mover_links();
    sm.images = []() -> const world::ImageSlotTable* { return &g_images; };
    sm.fragment_has_bitmap = [](int r) {  // DGROUP:0x4e94 + 2r != 0 [BYTES seg7:0x3336]
        return r >= 0 && r < static_cast<int>(g_fragment_dc.size()) &&
               g_fragment_dc[static_cast<std::size_t>(r)] != world::kDcNone;
    };
    // SelectObject(DGROUP:0x9288, DGROUP:0x4e94[r]) and seg8:0xb76(h, 3, DGROUP:0x9288, 0): the
    // port's fragment image is its own DC [DESIGN ADR 0013] [BYTES seg7:0x333d-0x3388].
    sm.draw_fragment = [](std::int16_t pr, int r) {
        if (!ready() || r < 0 || r >= static_cast<int>(g_fragment_dc.size())) return;
        world::SpriteGlobals sg = sprite_globals();
        world::draw_sprite(w().pools, pr, 3, g_fragment_dc[static_cast<std::size_t>(r)], 0, sg, to_screen, *g_blit);
        after_draw(sg);
    };
    world::set_systems_host(&g_systems);
    {
        auto& gh = app.game.hooks();
        auto prev_route = gh.route_input;
        gh.route_input = [prev_route](shell::GameState& st, const shell::Input& in) {
            if (caps_key(st, in)) return;
            if (prev_route) prev_route(st, in);
        };
    }
    // seg3:0x6ec from the collisions and the damage (hud/messages.h).
    world::collide_links().message = [](int ship, int pool, int rec, int code) {
        post_message(ship, pool, rec, code);
    };
    // The gate exit's and the vanish's entry effect, and the wave end's calls into the HUD and
    // the shell (world/hooks.h WaveEndLinks).
    world::set_entry_context([] { return entry_context(); });
    world::WaveEndLinks& e = world::wave_end_links();
    e.close_hud = [] { close(); };  // seg11:0x2f0
    // seg13:0x65f2(1)'s seg7:0x63f2, the IS_ICON column [BYTES seg13:0x669b]; seg13:0x5850 and
    // seg13:0x6362 (the watch and IS_SMART windows) have no port [OPEN].
    e.close_watch_windows = [] {
        if (icon_column_exists()) icon_column_destroy();
    };
    e.message = [](int ship, int id) { post_message(ship, 7, 0, id); };  // seg3:0x6ec(ship, 7, 0, id)
    // seg7:0x65aa(pool, rec) of the vanish (seg7:0x18c5) and the other world callers (M4 s2
    // DISPLAY).
    e.column_row = [](int pool, int rec) { refresh_object(pool, rec); };
    // seg16:0x68d6(main, 2328, 2329) at seg7:0x6c8 [BYTES seg7:0x6c8 seg16:0x68ed seg16:0x6904
    // seg16:0x691b].
    e.history_box = [] { app::message_box_ids(g.main, 2328, 2329); };
    // ---- M4 s2 fixer: the stage's links
    l.pictures = [] { pictures_tick(); };  // seg13:0x675e's HUD part at the ship mover's head
    world::CollideLinks& cl = world::collide_links();
    cl.icon_crack = icon_crack;
    // The icon effects' images: slots 14 + i and 32 of g_images (hud/icon_images.h) (M4 s5 ICONS).
    install_icon_images(*g_blit, g_images, [] { return strip_dc(); }, icon_crack, [] { return ready(); },
                        [](const std::string& what, const char* address) { todo_once(what, address); });
    cl.fragment_bitmap = fragment_bitmap;
    cl.fragment_pictures = fragment_pictures;
    // DGROUP:0x4e94 + 2r for seg7:0x2e02's break-up source (world/effects.h EffectLinks) (M4 s5 EFFECTS).
    world::effect_links().fragment_dc = [](int r) {
        return r >= 0 && r < static_cast<int>(g_fragment_dc.size()) ? g_fragment_dc[static_cast<std::size_t>(r)]
                                                                     : world::kDcNone;
    };
    set_fragment_images([](int k) -> gfx::ImagePtr {
        if (!g_blit || k < 0 || k >= static_cast<int>(g_fragment_dc.size())) return nullptr;
        const world::DcId d = g_fragment_dc[static_cast<std::size_t>(k)];
        return d == world::kDcNone ? nullptr : g_blit->image(d);
    });
    // The launcher's PostMessage(DGROUP:0xa7e, WM_PAINT) [BYTES seg11:0x7e9a-0x7ea6].
    world::missile_links().paint_weapons = [] { weapon_panel_repaint(); };
    // seg12:0x6d64's seg10:0x533a (world/weapon_load.h) (M4 s5 WEAPONS).
    world::set_weapon_image(weapon_image);
    Links& k = links();
    // seg11:0xac86 on the program's World (Z / Enter, hooks.cpp weapon_keys): the flight role's
    // globals with the clock of the key's message.
    k.weapon_system = [] {
        world::WaveGlobals& fg = world::flight_globals();
        fg.now = g.game.now();
        world::fire_weapon_system(w(), fg, shell::game_rng());
    };
    // W(w)-4, the slot seg10:0x533a wrote (MissileState::words).
    k.weapon_sheet_slot = [](int wpn) { return int{world::missile_state().words.word(wpn, -4)}; };
    // DGROUP:0x9604 and DGROUP:0x9602, the capture's words (messages 0x55 / 0x56).
    k.capture_value = [] { return int{w().w9604}; };
    k.capture_defenders = [] { return int{w().w9602}; };
    // DGROUP:0x57c3 + 0x23·rec + i and DGROUP:0x6038 + 2k, the shatter's (World).
    k.fragment_index = [](int rec, int i) {
        return rec >= 0 && rec < world::kIconRecords && i >= 0 && i < 4
                   ? int{w().icons[static_cast<std::size_t>(rec)].fragment[static_cast<std::size_t>(i)]}
                   : -1;
    };
    k.fragment_parent = [](int f) {
        return f >= 0 && f < static_cast<int>(w().fragment_icon.size()) ? int{w().fragment_icon[static_cast<std::size_t>(f)]}
                                                                         : -1;
    };
    k.radar = [] {
        if (ready() && g_radar.created()) g_radar.repaint(radar_inputs(), *g_blit);  // seg11:0x33cc
    };
    k.radar_centre = [] { g_radar.set_centre(); };
    k.scroll_pos = [] { return scroll_pos_of(g_radar.zoom); };
    k.vscroll = [](int code, int pos) {
        if (ready() && g_radar.created()) g_radar.scroll(code, pos, radar_inputs(), *g_blit);
    };
    k.zoom_changed = [](std::int32_t z) {
        if (ready() && g_radar.created())
            g_radar.zoom_changed(z, radar_inputs(), *g_blit);
        else
            g_radar.zoom = z;
    };
    k.blink = [] { return g_status.blink != 0; };
    // seg7:0x11cc: the player's (+0x00, +0x02) = (W / 2, H / 2) [BYTES seg7:0x11e8 seg7:0x11fc].
    k.pin_player = [] {
        if (w().pools.records(w().roles.ships) <= w().player) return;
        const world::ScreenPoint p = world::pinned_player(world::flight_view());
        world::ObjectRecord& o = w().ship_object(w().player);
        o.scr_x = p.x;
        o.scr_y = p.y;
    };
    k.draw_player = player_sprite;
    // The frame host's calls into the sprite side (world/player_sheet.h SpriteLinks): the
    // entry seg7:0x1d0c's seg12:0x2556(14, i) and seg7:0x13b2(2, 14, i) [BYTES seg7:0x1e7f],
    // and the frame's seg7:0x158e [BYTES seg7:0x118a] (M5 s1 SPRITE).
    // The ambulance's buffer, materialise and pick-up composite (world/ambulance.h
    // AmbulanceLinks): seg12:0x2556, seg12:0x26a4 and seg7:0x13b2 through the entry effect as the
    // ship's entry uses them, and seg13:0x3d40 (world/draw.h pickup_composite) (M4 s3 fixer,
    // review M7).
    world::AmbulanceLinks& al = world::ambulance_links();
    al.buffer = [](int pool, int rec) {
        if (!ready()) return;
        world::EntryContext c = entry_context();
        world::entry_buffer(c, pool, rec);
    };
    al.buffer_free = [](int pool, int rec) {
        if (!ready()) return;
        world::EntryContext c = entry_context();
        world::entry_buffer_free(c, pool, rec);
    };
    al.effect = [](int mode, int pool, int rec) {
        if (!ready()) return;
        world::EntryContext c = entry_context();
        world::entry_start(c, static_cast<std::uint8_t>(mode), pool, rec);
    };
    al.composite = [](std::int16_t den, std::int16_t num, int o_rec, int o_pool, int s_rec, int s_pool) {
        if (!ready()) return;
        world::DrawContext c = draw_context();
        world::pickup_composite(c, den, num, o_rec, o_pool, s_rec, s_pool);
        after_draw(c.g.sprite);
    };
    world::SpriteLinks& sl = world::sprite_links();
    // The program's sheet side (world/player_sheet.h): the image table, ordinal 2, ordinal 17,
    // seg10:0x4412 and the todo log, for the rebuild after a visit, the shield's quick cells and
    // the restore re-cut (M4 s5 SHEET).
    sl.images = []() -> world::ImageSlotTable* { return &g_images; };
    sl.render = render_sheet;
    sl.quick = render_quick;
    sl.anim4 = anim4_sheet;
    sl.todo = [](const std::string& what, const char* address) { todo_once(what, address); };
    sl.ship_entered = [](int ship) {
        if (!ready()) return;
        world::EntryContext c = entry_context();
        world::entry_buffer(c, w().roles.ships, ship);
        world::entry_start(c, 2, w().roles.ships, ship);
    };
    sl.entry_step = [] {
        if (!ready()) return;
        world::EntryContext c = entry_context();
        world::entry_step(c);
    };
    // seg11:0xb105 to the end of seg11:0xb09c(i, on): the flame pastes into the sheet DC
    // DGROUP:0x39e8[0x28 + i] (world/player_sheet.h ship_flame_tail); the caller's seg13:0x6716
    // redraws [BYTES seg11:0xb105-0xb302] (M5 s2 FLAME).
    sl.flame_tail = [](int on, int ship) {
        if (!ready() || ship < 0 || ship >= world::kShipCount || w().pools.records(w().roles.ships) <= ship) return;
        world::ship_flame_tail(*g_blit, g_images, ship, on, w().ship(ship), w().ship_object(ship));
    };
    g_radar.set_scroll_pos = [](int pos) { set_scroll_pos(pos); };
    world::set_zoom_source([] { return g_radar.zoom; });
    // seg9:0x60bc's tail: seg11:0x3cc2(player), whose full repaint seeds rand() [BYTES
    // seg9:0x6320 seg11:0x3e48 seg11:0xb22]. DGROUP:0x8f64 = (seg13:0x65f2(0) != 0) after it is
    // [OPEN]: the port leaves the repaint flag as it is.
    // The image table DGROUP:0x39e8 for the briefing's phase 11 (world.h slot_image): the image
    // behind slot n on the blitter, the ships' sheets 0x28 + i among them [BYTES
    // seg17:0x2109-0x211a] (M5 s2 BRIEFING).
    world::set_slot_images([](int slot) -> std::shared_ptr<gfx::Surface> {
        if (!g_blit || slot < 0 || slot >= world::kImageSlots) return nullptr;
        const world::DcId dc = g_images[static_cast<std::size_t>(slot)];
        return dc == world::kDcNone ? nullptr : g_blit->image(dc);
    });
    // seg9:0x6624's seg14:0x0(-1) at the briefing entry builds the sheets, under world.md Ship
    // sheets' Path tests (C067), and the flame strip (seg16:0x3784 -> seg16:0x3468 ->
    // seg14:0x11a8, seg14:0xc7e) [BYTES seg9:0x688a seg14:0xd32 seg16:0x362b] (M5 s2 BRIEFING). Nothing rebuilds them at the wave start, so phase 11's
    // flame pastes stay in the sheet [BYTES seg9:0x60bc] (s2-briefing.md, "Flame pastes").
    world::on_briefing_placement([](world::World& wd) {
        refresh_player_design();
        build_player_sheet(wd);    // seg14:0x11a8 for the player (M5 s1 SPRITE)
    });
    world::on_wave_start([](world::World& wd) {
        refresh_player_design();  // the design words the panels read (M3 stage 4)
        clear_entries(g_entries);  // [DESIGN] (world/entry_effect.h)
        if (ready()) world::refresh_shot_brushes(*g_blit);  // the shot and star brushes [DESIGN ADR 0007]
        // seg9:0x695e-0x6963: seg11:0xae94 and seg11:0x2f8c (the weapon picture), only when the
        // placement saw DGROUP:0x91f8 = 1 or DGROUP:0x92e4 != 0 (World::picture_due) [BYTES
        // seg9:0x6951-0x695c] (M3 s4 fixer, review M6); seg11:0xae94 as core.md "The ship
        // between waves", Weapon picture, reads it [BYTES seg11:0xae9e-0xaec0].
        if (wd.picture_due) {
            // seg11:0xae94: unless DGROUP:0x92e4 != 0, slot 1 when byte +0xcb > 1, else 0
            // (hud/weapons.h wave_start_slot) (M4 s2 DISPLAY).
            if (wd.player >= 0 && wd.player < world::kShipCount) {
                world::ShipRecord& r = wd.ship(wd.player);
                r.weapon_slot = static_cast<std::int16_t>(wave_start_slot(r.weapon_slot, r.slots_cb(), wd.restored));
            }
            weapon_picture_refresh();
        }
        wave_start();
        // seg11:0x3e48-0x3e5f: seg13:0x65f2(0) after the repaint, whose column part posts 0x9aa
        // when IconWnd is set and the column does not exist (the IS_ICON column, M3 stage 4),
        // and DGROUP:0x8f64 = 1 when it posted, else 0 [BYTES seg11:0x3e4d seg11:0x3e57
        // seg11:0x3e5f seg13:0x66af-0x66ca]. The column's creation reactivates main, whose erase
        // the port follows with a second repaint, so the full status display stands from the
        // first frame (icon_column_win32.cpp) [ORACLE win98-0001] [DESIGN ADR 0013 decision 3].
        // The IS_SMART post 0x9b4 (SmWnd, seg13:0x66d4) is not ported [OPEN].
        Hud* h = hud();
        const bool repainted = h && h->lamp_repaint(wd.player);
        if (repainted) g.hud_dirty = icon_column_wave_start();
        app::log(std::string("hud: seg11:0x3cc2 at the wave start: ") +
                 (h ? (repainted ? "repaint, seg13:0x65f2(0)" : "DGROUP:0x951a set") : "no HUD"));
    });
}

}  // namespace is::hud
#endif
