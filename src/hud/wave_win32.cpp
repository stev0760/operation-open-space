// The wave's integration (M3 s2 fixer, the review's H2): the calls of the wave pass and frame
// (src/world/update.h through flight_hooks.h WaveLinks) reach the drawing of src/world
// (draw.h, sprite.h, visibility.h, rock_images.h), the radar (hud/radar.h), the status tick
// (hud/status.h) and the HUD (hud/program.h); the HUD's slots (links()) reach the radar and the
// player's sprite; the wave start runs seg11:0x3cc2. One GdiBlitter draws on the surface, the
// window DC DGROUP:0x13dc (ADR 0013 decision 1). Win32 only.
#ifdef _WIN32
#include <windows.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>

#include "app/app.h"
#include "assets/asset_store.h"
#include "formats/ships.h"
#include "gfx/gfx.h"
#include "gfx/palette.h"
#include "gfx/ship_flame.h"
#include "gfx/ship_sheet.h"
#include "hud/gdi_canvas.h"
#include "hud/icon_column.h"
#include "hud/program.h"
#include "hud/radar.h"
#include "hud/status.h"
#include "hud/wave.h"
#include "shell/rng.h"
#include "world/draw.h"
#include "world/entry_effect.h"
#include "world/flight_hooks.h"
#include "world/hooks.h"
#include "world/player_sheet.h"
#include "world/rock_images.h"
#include "world/setup.h"
#include "world/sprite.h"
#include "world/visibility.h"
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

// The icon strip DGROUP:0x3a02 the briefing entry built, as a DC id.
world::DcId strip_dc() {
    gfx::ImagePtr s = world::icon_strip();
    if (s != g_strip) {
        g_strip = s;
        g_draw.strip_dc = s ? g_blit->bind(s) : world::kDcNone;
    }
    return g_draw.strip_dc;
}

// The frame's drawing state: DGROUP:0x3a38 is image slot 0x28 of the table DGROUP:0x39e8, the
// player's ship image (0x39e8 + 2·0x28) [BYTES seg7:0x1208] (M3 s2 fixer reading).
world::DrawContext draw_context() {
    g_draw.sprite = sprite_globals();
    g_draw.state = static_cast<std::int16_t>(g.game.state());
    g_draw.dg_9130 = w().w9130;
    g_draw.dg_3a38 = static_cast<std::int16_t>(g_images[world::kShipImageSlotBase]);
    g_draw.last_effect = w().w9434;
    g_draw.detail = w().detail;
    strip_dc();
    return world::DrawContext{w().pools, w().ships, w().effects, g_images, g_draw, *g_blit,
                              to_screen, nullptr};
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
    const world::DcId src =
        slot >= 0 && slot < world::kImageSlots ? g_images[static_cast<std::size_t>(slot)] : world::kDcNone;
    world::draw_sprite(w().pools, pr, static_cast<std::int16_t>(kind), src,
                       static_cast<std::int16_t>(masked), s, to_screen, *g_blit);
    after_draw(s);
}

void player_sprite() {
    if (!ready()) return;
    world::DrawContext c = draw_context();
    world::draw_player_sprite(c);
    // DGROUP:0x1e5c is one word set: the drawing's copy is the stars' exclusion box
    // [BYTES seg7:0x12e4 seg8:0x17a0] (M3 s4 fixer, draw request 3; inert until the player's
    // sprite draws, M5).
    w().player_cell = g_draw.player_rect;
    after_draw(g_draw.sprite);
}

void dissolve(int i) {
    if (!ready()) return;
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

void fuel_mass() {
    StatusHooks h;
    h.update = [](int arg) {
        if (Hud* x = hud()) x->panels().update(arg, x->gate());
    };
    h.score_shown = [] { return hud() ? hud()->panels().state().score_shown : 0u; };
    h.score = [] { return links().score ? links().score() : 0u; };
    h.design_mass = [](int d) { return world::design_mass(d); };
    h.seg11_4c72 = [](int) { todo_once("fuel-out damage", "seg11:0x4c72"); };
    h.seg3_332e = [](std::uint32_t) { todo_once("status message", "seg3:0x332e"); };
    h.seg13_2dcc = [] { todo_once("radar brushes", "seg13:0x2dcc"); };
    h.seg11_1d7c = [] { todo_once("seg11:0x1d7c after seg11:0x1bcc", "seg11:0x1d7c"); };
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
    const world::SheetBuild b = world::rebuild_ship_sheet(*g_blit, g_images, s, o, i, req, render_sheet);
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

// The effect's calls outside the drawing (world/entry_effect.h). The sounds go through no
// engine yet (the sound track) and seg2:0x170e's menu state is the shell's [OPEN].
class WaveEntryHooks final : public world::EntryHooks {
public:
    void sound(int id, int pool, int rec, int arg) override {
        (void)pool, (void)rec, (void)arg;
        todo_once("entry sound " + std::to_string(id), "seg10:0x754a");
    }
    void menu_refresh() override { todo_once("menu state after the entry", "seg2:0x170e"); }
    void vanished(int pool, int rec) override {
        (void)pool, (void)rec;
        todo_once("the vanish's end", "seg7:0x181e");
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

}  // namespace

std::vector<std::pair<int, int>> wave_visible() {
    std::vector<std::pair<int, int>> v;
    const int n = g_vis.count < 0 ? 0 : (g_vis.count > world::kVisibleMax ? world::kVisibleMax : g_vis.count);
    for (int k = 0; k < n; ++k)  // cbw of each byte [BYTES seg13:0x2271-0x2283]
        v.emplace_back(static_cast<std::int8_t>(g_vis.pool[static_cast<std::size_t>(k)]),
                       static_cast<std::int8_t>(g_vis.rec[static_cast<std::size_t>(k)]));
    return v;
}

void load_wave_resources() {
    if (!ready()) return;
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
    Links& k = links();
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
    world::SpriteLinks& sl = world::sprite_links();
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
    // seg9:0x6624's seg14:0x0(-1) at the briefing entry builds the sheets and the flame strip
    // (seg16:0x3784 -> seg16:0x3468 -> seg14:0x11a8, seg14:0xc7e) [BYTES seg9:0x688a seg14:0xd32
    // seg16:0x362b] (M5 s2 BRIEFING). Nothing rebuilds them at the wave start, so phase 11's
    // flame pastes stay in the sheet [BYTES seg9:0x60bc] (s2-briefing.md, "Flame pastes").
    world::on_briefing_placement([](world::World& wd) {
        refresh_player_design();
        build_player_sheet(wd);    // seg14:0x11a8 for the player (M5 s1 SPRITE)
    });
    world::on_wave_start([](world::World& wd) {
        refresh_player_design();  // the design words the panels read (M3 stage 4)
        clear_entries(g_entries);  // [DESIGN] (world/entry_effect.h)
        // seg9:0x695e-0x6963: seg11:0xae94 and seg11:0x2f8c (the weapon picture), only when the
        // placement saw DGROUP:0x91f8 = 1 or DGROUP:0x92e4 != 0 (World::picture_due) [BYTES
        // seg9:0x6951-0x695c] (M3 s4 fixer, review M6). seg11:0xae94 is unread [OPEN].
        if (wd.picture_due) {
            todo_once("seg11:0xae94 at the wave set-up", "seg11:0xae94");
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
