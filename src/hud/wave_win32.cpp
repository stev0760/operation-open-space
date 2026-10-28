// The wave's integration (M3 s2 fixer, the review's H2): the calls of the wave pass and frame
// (src/world/update.h through flight_hooks.h WaveLinks) reach the drawing of src/world
// (draw.h, sprite.h, visibility.h, rock_images.h), the radar (hud/radar.h), the status tick
// (hud/status.h) and the HUD (hud/program.h); the HUD's slots (links()) reach the radar and the
// player's sprite; the wave start runs seg11:0x3cc2. One GdiBlitter draws on the surface, the
// window DC DGROUP:0x13dc (ADR 0013 decision 1). Win32 only.
#ifdef _WIN32
#include <windows.h>

#include <memory>
#include <set>
#include <string>

#include "app/app.h"
#include "hud/gdi_canvas.h"
#include "hud/program.h"
#include "hud/radar.h"
#include "hud/status.h"
#include "hud/wave.h"
#include "shell/rng.h"
#include "world/draw.h"
#include "world/flight_hooks.h"
#include "world/hooks.h"
#include "world/rock_images.h"
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
    s.player_mask_brush = world::kBrushNone;  // DGROUP:0x5ea8: seg10:0x0's pattern brush [OPEN]
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

}  // namespace

void load_wave_resources() {
    if (!ready()) return;
    // seg9:0x22b4's rock images (seg10:0x3a2e) [BYTES seg9:0x22b4].
    world::build_rock_images(*g_blit, g_images, w().w740);
    // seg11:0x0's radar part: DGROUP:0x90f0 = 1 on the port's surface (ADR 0007) [BYTES seg11:0xd].
    if (!g_radar.created()) {
        const world::BrushId metal = g_blit->bind_brush(metal_brush());
        g_radar.create(*g_blit, true, metal);
        app::log("hud: seg11:0x0 radar created (86 x 71, RADSHIP, 8 brushes)");
    }
}

void install_wave(app::App& app) {
    g_blit = std::make_unique<world::GdiBlitter>(*app.gfx);
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
    g_radar.set_scroll_pos = [](int pos) { set_scroll_pos(pos); };
    world::set_zoom_source([] { return g_radar.zoom; });
    // seg9:0x60bc's tail: seg11:0x3cc2(player), whose full repaint seeds rand() [BYTES
    // seg9:0x6320 seg11:0x3e48 seg11:0xb22]. DGROUP:0x8f64 = (seg13:0x65f2(0) != 0) after it is
    // [OPEN]: the port leaves the repaint flag as it is.
    world::on_wave_start([](world::World& wd) {
        wave_start();
        if (Hud* h = hud()) h->lamp_repaint(wd.player);
    });
}

}  // namespace is::hud
#endif
