// The wave's update (update.h).
#include "world/update.h"

#include "world/rock_images.h"

#include "world/damage.h"
#include "world/fragments.h"
#include "world/icon_mover.h"
#include "world/missile.h"
#include "world/pods.h"
#include "world/tank.h"

namespace is::world {

const char* mover_address(Mover m) {
    switch (m) {
    case Mover::k12_532c: return "seg12:0x532c";
    case Mover::k12_633e: return "seg12:0x633e";
    case Mover::k8_8256: return "seg8:0x8256";
    case Mover::k7_309c: return "seg7:0x309c";
    case Mover::k7_32d0: return "seg7:0x32d0";
    case Mover::k7_40c4: return "seg7:0x40c4";
    case Mover::k12_0: return "seg12:0x0";
    case Mover::k6_73f6: return "seg6:0x73f6";
    case Mover::k6_7c14: return "seg6:0x7c14";
    case Mover::k7_5e04: return "seg7:0x5e04";
    case Mover::k12_3148: return "seg12:0x3148";
    case Mover::k12_6b1e: return "seg12:0x6b1e";
    case Mover::k12_ae4: return "seg12:0xae4";
    case Mover::k12_3e98: return "seg12:0x3e98";
    }
    return "?";
}

namespace {

// `cmp dword [d], now; jae skip`: true when the deadline d lies before now (unsigned).
bool passed(std::uint32_t d, std::uint32_t now) { return d < now; }

}  // namespace

namespace {
World* g_frame_world = nullptr;
}  // namespace

void set_frame_world(World* w) { g_frame_world = w; }

World* UpdateHost::frame_world(Pools& pools) {
    return g_frame_world && &g_frame_world->pools == &pools ? g_frame_world : nullptr;
}

std::uint16_t UpdateHost::select_rock_brush(int b) {
    Blitter* out = blitter();
    if (!out || b < 1 || b > 9) return 0;
    return out->select_brush(kDcWindow, wave_graphics().heat[static_cast<std::size_t>(b)]);
}

void UpdateHost::restore_brush(std::uint16_t old) {
    if (Blitter* out = blitter(); out && old != 0) out->select_brush(kDcWindow, old);
}

// ---- the collisions (M4 s1 COLLIDE)

namespace {

// The frame's context on the host while a mover runs; nested scopes keep the outer one.
struct FrameScope {
    UpdateHost& h;
    UpdateCtx* prev;
    FrameScope(UpdateHost& host, UpdateCtx& c) : h(host), prev(host.frame_ctx) {
        if (!prev) h.frame_ctx = &c;
    }
    ~FrameScope() { h.frame_ctx = prev; }
    FrameScope(const FrameScope&) = delete;
    FrameScope& operator=(const FrameScope&) = delete;
};

CollideCtx collide_ctx(UpdateHost& h, UpdateCtx& c, World& w) {
    return CollideCtx{w,        c.flight.g,     h.collide_globals(), h.collide_host(),
                      c.view,   h.visible_list(), c.flight.rng,      c.f.dg_943a,
                      c.f.dg_943c, c.f.dg_9454};
}

}  // namespace

void UpdateHost::Forward::flame_off(int ship) {
    if (u->frame_ctx) ship_touched(u->frame_ctx->flight, ship);  // seg13:0x6716
}

void UpdateHost::Forward::erase(int x, int y, int w, int h) {
    if (Blitter* out = u->blitter()) out->pat_blt(kDcWindow, x, y, w, h, kBlackness);
}

void UpdateHost::CaptureForward::erase(int x, int y, int w, int h) {
    if (Blitter* out = u->blitter()) out->pat_blt(kDcWindow, x, y, w, h, kBlackness);
}

// ---- M4 s3 FUEL: the cloak, the shield and the small movers

namespace {
SmallMoverLinks g_small_links;
}  // namespace

SmallMoverLinks& small_mover_links() { return g_small_links; }

std::int16_t UpdateHost::seg11_4c72(int ship) {
    if (!frame_ctx) return 0;
    World* w = frame_world(frame_ctx->flight.pools);
    if (!w) return 0;
    SystemsCtx sc{*w, frame_ctx->flight.g.now, frame_ctx->flight.g.game_frames, systems_host()};
    return cloak_toggle(sc, ship);  // seg11:0x4c72 [BYTES seg13:0x45f]
}

std::int16_t UpdateHost::seg11_4b00(int ship) {
    if (!frame_ctx) return 0;
    World* w = frame_world(frame_ctx->flight.pools);
    if (!w) return 0;
    SystemsCtx sc{*w, frame_ctx->flight.g.now, frame_ctx->flight.g.game_frames, systems_host()};
    return shield_toggle(sc, ship);  // seg11:0x4b00 [BYTES seg13:0x491]
}

void UpdateHost::seg3_332e(std::uint32_t ids) {
    if (!frame_ctx) return;
    World* w = frame_world(frame_ctx->flight.pools);
    if (!w) return;
    SystemsCtx sc{*w, frame_ctx->flight.g.now, frame_ctx->flight.g.game_frames, systems_host()};
    strategy_notice(sc, ids);  // seg3:0x332e [BYTES seg13:0x4a3]
}

const ImageSlotTable* UpdateHost::images() {
    return g_small_links.images ? g_small_links.images() : nullptr;
}

namespace {
DissolveOut dissolve_out(UpdateHost& h) {
    DissolveOut d;
    d.out = h.blitter();
    d.images = h.images();
    d.window_dc = kDcWindow;
    d.player_sprite = [&h] { h.player_sprite(); };
    d.in_use = [&h](int pool, int rec) { h.collide_host().seg7_2080(pool, rec); };
    return d;
}
}  // namespace

void UpdateHost::tank_dissolve(int rec) {
    if (!frame_ctx) return;
    if (World* w = frame_world(frame_ctx->flight.pools)) ::is::world::tank_dissolve(*w, rec, dissolve_out(*this));
}

void UpdateHost::pod_dissolve(int pool, int rec) {
    if (!frame_ctx) return;
    World* w = frame_world(frame_ctx->flight.pools);
    if (!w) return;
    ::is::world::pod_dissolve(*w, pool, rec, dissolve_out(*this),
                              [this](int wpn, int off) { return weapon_word(wpn, off); });
}

bool UpdateHost::fragment_has_bitmap(int r) {
    return g_small_links.fragment_has_bitmap ? g_small_links.fragment_has_bitmap(r) : false;
}

void UpdateHost::draw_fragment(std::int16_t pr, int r) {
    if (g_small_links.draw_fragment) g_small_links.draw_fragment(pr, r);
}

void UpdateHost::set_design_word_22(int design, std::int16_t v) {
    if (!frame_ctx) return;
    World* w = frame_world(frame_ctx->flight.pools);
    if (!w || design < 0 || design >= kShipCount) return;
    w->design_ratings[static_cast<std::size_t>(design)].captures = v;
}

CollideCtx frame_collide_ctx(UpdateCtx& c, World& w) { return collide_ctx(c.host, c, w); }

// ---- the gun and the shots (shots.h) (M4 s2 fixer)

void UpdateHost::fire(int ship) {
    with_shots([ship](ShotCtx& sc) { ::is::world::fire(sc, ship); });  // seg11:0x4e0a(ship)
}

std::int16_t UpdateHost::beam(int i, int j) {
    std::int16_t r = 0;
    with_shots([&](ShotCtx& sc) { r = shot_update(sc, i, j); });  // seg8:0x3aec(j, i)
    return r;
}

void UpdateHost::ShotForward::seg13_4eec(int ship, int arg) {
    if (!u->frame_ctx) return;
    if (World* w = u->frame_world(u->frame_ctx->flight.pools))
        bump_w82(ship_at(*w, ship), static_cast<std::int16_t>(arg));  // capture.h seg13:0x4eec
}

void UpdateHost::ShotForward::thrust_step(int mode, int ship) {
    if (u->frame_ctx) ::is::world::thrust_step(u->frame_ctx->flight, static_cast<std::int16_t>(mode), ship);
}

// SelectObject(DGROUP:0x13dc, DGROUP:0x3b4c[n]): WaveGraphics::star holds the 20 brushes of
// seg10:0x4dfe; an index past them (ship byte +0xe3 >= 2) reaches DGROUP:0x3b74, the heat
// brushes, in the original [OPEN] and selects nothing here [DESIGN].
void UpdateHost::ShotForward::select_brush(int n) {
    Blitter* out = u->blitter();
    if (!out) return;
    WaveGraphics& wg = wave_graphics();
    create_wave_brushes(*out, wg);
    if (n >= 0 && n < static_cast<int>(wg.star.size())) out->select_brush(kDcWindow, wg.star[static_cast<std::size_t>(n)]);
}

void UpdateHost::ShotForward::patblt(std::int16_t x, std::int16_t y, std::int16_t w, std::int16_t h,
                                     std::uint32_t rop) {
    if (Blitter* out = u->blitter()) out->pat_blt(kDcWindow, x, y, w, h, rop);
}

std::uint16_t UpdateHost::collide(std::int16_t pool_rec) {
    if (!frame_ctx) return 0xffffu;
    World* w = frame_world(frame_ctx->flight.pools);
    if (!w) return 0xffffu;
    CollideCtx x = collide_ctx(*this, *frame_ctx, *w);
    return find_pair(x, pool_rec);
}

void UpdateHost::rock_hit(int rock, std::uint16_t hit) {
    if (!frame_ctx) return;
    World* w = frame_world(frame_ctx->flight.pools);
    if (!w) return;
    CollideCtx x = collide_ctx(*this, *frame_ctx, *w);
    const int pool = x.g.rock_pool;
    ObjectRecord& o = obj(*w, pool, rock);
    // seg8:0x546c(+0x44, +0x20 + 2, &+0x0e, &+0x0c, hit) [BYTES seg7:0x4014-0x4049]
    const std::int16_t amount = respond(x, o.mass, add16(o.extent, 2), &o.vy, &o.vx, hit);
    // seg8:0x5df2(DGROUP:0x934a, i, hit pool, hit rec, amount) [BYTES seg7:0x404e-0x4065]; the
    // site passes a separating response's -1 on as it is, and the dispatcher ends it for every
    // attacker but a seg18 one (collide.cpp dispatch) [BYTES seg7:0x404e seg8:0x5e93].
    dispatch(x, pool, rock, hit >> 8, hit & 0xff, amount);
}

void UpdateHost::post_hit(int rec, std::uint16_t hit) {
    if (!frame_ctx) return;
    World* w = frame_world(frame_ctx->flight.pools);
    if (!w) return;
    CollideCtx x = collide_ctx(*this, *frame_ctx, *w);
    ObjectRecord& o = obj(*w, kPoolGatePosts, rec);
    // [BYTES seg12:0x1d79-0x1da9]; the post site leaves DGROUP:0x9454 at 0 (s1-hit-verify.md 36)
    const std::int16_t amount = respond(x, o.mass, add16(o.extent, 2), &o.vy, &o.vx, hit);
    dispatch(x, kPoolGatePosts, rec, hit >> 8, hit & 0xff, amount);  // [BYTES seg12:0x1dae-0x1dc3]
}

void UpdateHost::ship_collide(int i) {
    if (!frame_ctx) return;
    World* w = frame_world(frame_ctx->flight.pools);
    if (!w) return;
    UpdateCtx& c = *frame_ctx;
    const int ships = c.flight.g.ship_pool;
    ObjectRecord& o = obj(*w, ships, i);
    // The object's word +0x1a = 0 jumps to seg13:0xcf9 past the hit test and the weapon tail
    // alike (verified claim 10: the ship's object in pool DGROUP:0x9324, not the ship record)
    // [BYTES seg13:0xb69 seg13:0xb6e].
    if (o.on_screen == 0) return;
    c.f.dg_943c = static_cast<std::int16_t>(i);  // [BYTES seg13:0xb75 seg13:0xb7d]
    c.f.dg_943a = static_cast<std::int16_t>(ships);
    const std::uint16_t hit = collide(pool_rec(ships, i));  // [BYTES seg13:0xb8a]
    if (hit != 0xffffu) {                                    // else to seg13:0xc95 [BYTES seg13:0xb93]
        CollideCtx x = collide_ctx(*this, c, *w);
        ObjectRecord& s = obj(*w, ships, i);
        // The response on every hit, before the touch branches (verified claim 5): a touch still
        // bounces [BYTES seg13:0xb97-0xbd1]; the dispatcher call seg8:0x5df2(DGROUP:0x9324, ...)
        // drops its return [BYTES seg13:0xc79-0xc95] (core.md Ship against ship, C025).
        const std::int16_t amount = respond(x, s.mass, add16(s.extent, 2), &s.vy, &s.vx, hit);
        const int hp = hit >> 8, hr = hit & 0xff;
        ObjectRecord& H = obj(*w, hp, hr);
        ShipRecord& S = ship_at(*w, i);
        CaptureCtx cc{x, c.flight, capture_host()};
        // The missile entry's weapon DGROUP:0x66e4 + 0x1c·rec (records.h Pools::missile) and the
        // ship's selected weapon byte +0xb8 + slot [BYTES seg13:0xc12-0xc5b].
        auto weapon_of = [](const World& wr, int rec) -> int { return wr.pools.missile(rec).weapon; };
        const int sel = selected_weapon(S);
        if (H.b53 != 0 && S.bea == 0) {
            // [BYTES seg13:0xbee-0xc0e]. In a normal wave this site meets only ships (pool >= 14);
            // whether a ship's object byte +0x53 is ever not 0 is [OPEN] (s2-capture.md 1a).
            touch(cc, i, static_cast<std::int16_t>(hit));
        } else if (hp == 6 && x.h.weapon_kind(weapon_of(*w, hr)) == 6 && x.h.weapon_kind(sel) == 6) {
            // [BYTES seg13:0xc12-0xc77]: dead code in a normal wave (the pair finder hands a
            // pool-6 pair to the pool-6 mover, seg12:0x3ad-0x40b, its live copy). +0x53 = 1 before
            // the call (verified claim 6), and the count byte +0xae + slot (the slot re-read)
            // rises whatever the touch did, an early exit included.
            H.b53 = 1;
            touch(cc, i, static_cast<std::int16_t>(hit));
            const int s2 = ship_at(*w, i).weapon_slot;
            if (s2 >= 0 && s2 < 10) ++ship_at(*w, i).weapon_count[static_cast<std::size_t>(s2)];
        } else {
            dispatch(x, ships, i, hp, hr, amount);  // [BYTES seg13:0xc79-0xc92]
        }
    }
    // The weapon tail seg13:0xc95-0xcf6, hit or not (verified claims 7-9) [BYTES seg13:0xc9e].
    const int wsel = selected_weapon(ship_at(*w, i));
    if (wsel == 0x18 || wsel == 0x1a) seg18_551a(ships, i);  // [BYTES seg13:0xca6-0xcb7]
    if ((wsel == 0x1a || wsel == 0x19) && (c.flight.g.game_frames & 7u) == 0 &&
        (c.flight.keys.test(0x0d, kMaskDown) || c.flight.keys.test(0x5a, kMaskDown)))
        seg18_59ec(ships, i);  // no test that i is the player [BYTES seg13:0xcbf-0xcf1]
}

namespace {

// seg8:0x158a(arg) at seg7:0xb87 with the frame's words (draw.h draw_stars), or the host's
// stars() while the frame has no World or Blitter.
void frame_stars(UpdateCtx& c, int arg) {
    UpdateHost& h = c.host;
    World* w = h.frame_world(c.flight.pools);
    Blitter* out = h.blitter();
    if (!w || !out) {
        h.stars(arg);
        return;
    }
    // The brush tables on first use, when the load did not make them (rock_images.h) [DESIGN].
    create_wave_brushes(*out, wave_graphics());
    const WaveGlobals& g = c.flight.g;
    StarInputs in;
    in.ship_pool = g.ship_pool;
    in.player = g.player;
    in.size = g.size;
    in.autopilot = g.autopilot;
    in.shake = g.dg_238c;
    in.shake_x = c.view.shake_x;  // DGROUP:0x2390
    in.shake_y = c.view.shake_y;  // DGROUP:0x2392
    in.demons_gate = g.demons_gate;
    // DGROUP:0x2394 / 0x2396, the 0x951a motion, have no port yet [OPEN].
    draw_stars(*w, in, wave_graphics(), *out, c.flight.rng, arg,
               [&h](int a) { h.bright_stars(a); });
}

// seg12:0x3778 at seg7:0xbed (draw.h draw_lines), or the host's lines() without a World.
void frame_lines(UpdateCtx& c) {
    World* w = c.host.frame_world(c.flight.pools);
    if (!w) {
        c.host.lines();
        return;
    }
    draw_lines(*w, c.flight.g.ship_pool, kDcWindow, c.host.link_hooks());
}

}  // namespace

// seg7:0x7fe-0x993 [BYTES seg7:0x7fe-0x998]
void wave_pass(UpdateCtx& c, bool max_rate) {
    FrameScope scope(c.host, c);
    WaveGlobals& g = c.flight.g;
    FrameGlobals& f = c.f;
    UpdateHost& h = c.host;
    // 1. Turn follow-up and radar. The pass that precedes a due frame stands for the original's
    // counter >= threshold [DESIGN ADR 0001]; with threshold 0 (FrameRateSet > 1000) the test is
    // dword DGROUP:0x5a8 % 3 = 0 [BYTES seg7:0x7fe-0x83d].
    if (g.turned != 0 || !max_rate || g.frames % 3u == 0) {
        g.turned = 0;
        h.fuel_mass();
        h.turn_followup();
        h.radar_refresh();
    }
    h.select_clip();  // seg7:0x84a
    // 2. Pending input (seg7:0x863): the port's GameState::pass routes it before this pass and
    // ends the pass when it does (shell/game_state.cpp) [DESIGN ADR 0001].
    // 3. The fire poll seg11:0x4db0 on the player in play (input.h fire_poll), then the gun
    // seg11:0x4e0a(player) [BYTES seg7:0x871-0x881 seg11:0x4dba-0x4e00].
    ObjectRecord& player = c.flight.ship_obj(g.player);
    if (player.state == 0 &&
        fire_poll(g, c.flight.keys, c.flight.ships[static_cast<std::size_t>(g.player)].lockout.get()))
        h.fire(g.player);
    // [BYTES seg7:0x886-0x8d0] [OPEN]
    if (f.dg_1048 != 0) {
        if (!(g.now < f.dg_1e6c) && f.dg_92f0 != 0) {
            f.dg_1e6c = g.now + 10000u;
            h.seg8_9464();
            f.dg_914e = 0;
        }
        if (!(f.dg_1e6c - 500u > g.now) && f.dg_914e == 0) h.seg8_9564();
    }
    // [BYTES seg7:0x8d5-0x922]
    if (player.state == 0) {
        if (!(g.now < f.dg_5a4)) f.dg_5a4 = g.now + 50u;
        if (passed(f.dg_5b8, g.now)) {
            f.dg_5b8 = g.now + 10000u;
            h.seg7_2128();
        }
    }
    // 4. The shot slots: five sub-steps of every slot j < DGROUP:0x598 + DGROUP:0x594, the
    // bound re-read each time (shots.h run_shots, here through the host's seg8:0x3aec) [BYTES
    // seg7:0x938-0x970]; the original's DGROUP:0x9144 / frame-skip gate is the one pass per frame
    // [DESIGN ADR 0001].
    {
        ShotState& ss = h.shots();
        ss.dg_9136 = 0;
        for (int i = 0; i < 5; ++i) {
            for (int j = 0; add16(ss.dg_598, ss.dg_594) > j; ++j) ss.dg_9136 = h.beam(i, j);
            ++ss.dg_5cc;
        }
    }
    // 5. Effects and the particle burst [BYTES seg7:0x972-0x993]. DGROUP:0x9430 is tested once;
    // the loop bound DGROUP:0x9434 is read again on every iteration (`mov ax,[0x9434]` inside the
    // loop at seg7:0x98b), so the entries a finished death entry starts run in the same pass
    // (s1-death.md 1) [BYTES seg7:0x972 seg7:0x98b]. Both words are the World's.
    if (World* w = h.frame_world(c.flight.pools); w && w->w9430 > 0)
        for (int k = 0; k <= w->w9434 && k < kEffectCount; ++k) h.effect(k);
    h.burst();
}

// seg7:0x9ce-0x118d [BYTES seg7:0x9d6-0x118d]
void wave_frame(UpdateCtx& c) {
    FrameScope scope(c.host, c);
    WaveGlobals& g = c.flight.g;
    FrameGlobals& f = c.f;
    UpdateHost& h = c.host;
    f.dg_9142 = add16(f.dg_9142, 1);  // [BYTES seg7:0x9dc-0x9e7]
    if (f.dg_9142 > 54) f.dg_9142 = 0;
    // seg7:0x9ed-0xaf7, the adaptive threshold: not ported [DESIGN ADR 0001 decision 7].
    // 6. The OverThruster re-arm [BYTES seg7:0xafd-0xb2b]
    f.dg_17b6 = add16(f.dg_17b6, 1);
    if (f.dg_17b6 >= 8) {
        f.dg_17b6 = 0;
        for (int i = 0; i <= g.last_ship && i < kShipCount; ++i) {
            ShipRecord& s = c.flight.ships[static_cast<std::size_t>(i)];
            if (s.overthrust_cool != 0) --s.overthrust_cool;
        }
    }
    ++g.frames;       // DGROUP:0x5a8 [BYTES seg7:0xb2d]
    ++g.game_frames;  // DGROUP:0x1e68 [BYTES seg7:0xb32]
    // 7. Joystick [BYTES seg7:0xb37-0xb5f]
    if (g.joystick != 0) {
        if (passed(f.joy_due, g.now)) {
            f.joy_due = g.now + 50u;
            // seg11:0x72b2: the read runs only while the player's byte +0xe2 is 0, and a result
            // of 0xa7 clears `JoyStick` (world.md "Joystick", M3 s3 fixer, C039) [BYTES
            // seg11:0x72c2 seg11:0x72c9 seg11:0x72ce seg11:0x72d3].
            if (c.flight.ships[static_cast<std::size_t>(g.player)].be2 == 0 && h.joy_read() == 0xa7)
                g.joystick = 0;
        }
        joystick_poll(g, c.joy, c.joy_buttons, c.flight.ship_obj(g.player).frame,
                      [&h](std::uint32_t msg, std::uint16_t key) { h.post_key(msg, key); });
    }
    if (g.dg_9130 != 0) {  // [BYTES seg7:0xb64-0xb70]
        h.seg18_frame();
        return;
    }
    if (f.size_reread != 0) {  // [BYTES seg7:0xb73-0xb7f]
        h.size_reread();
        f.size_reread = 0;
    }
    frame_stars(c, 0);  // seg7:0xb87 seg8:0x158a(0) (M3 s4 DRAW)
    // 8. The view [BYTES seg7:0xb8c]
    {
        ObjectRecord& p = c.flight.ship_obj(g.player);
        ViewInputs vi;
        vi.player_x = p.x;
        vi.player_y = p.y;
        vi.autopilot = g.autopilot;
        vi.shake = g.dg_238c;
        vi.wave_kind = g.wave_kind;
        vi.dg_9eb5 = g.dg_9eb5;
        recentre(c.view, vi);
    }
    if (f.entry_pending != 0) ship_entry(c);  // [BYTES seg7:0xb91-0xb99]
    f.dg_9146 = 0;
    h.visibility();  // seg7:0xba2
    // The speed display's end [BYTES seg7:0xba7-0xbea]
    if (f.speed_shown != 0 && f.speed_shown < g.now) {
        f.speed_shown = 0;
        if (f.dg_9608 != 0)
            h.speed_display_end_3_6ec();
        else
            h.speed_display_end_7_119c();
    }
    frame_lines(c);  // seg7:0xbed seg12:0x3778 (M3 s4 DRAW)
    // The icon dissolves [BYTES seg7:0xbf2-0xc1a] [OPEN Draw order] (M3 s2 fixer)
    for (int i = 0; i < c.flight.pools.in_use(kPoolIcons) && i < c.flight.pools.records(kPoolIcons);
         ++i)
        if (c.flight.pools.rec(kPoolIcons, i).race_status != 0) h.dissolve(i);
    // 9. Information displays [BYTES seg7:0xc1c-0xc5b]
    if (passed(f.info_due, g.now)) {
        f.info_due = g.now + 280u;
        if (f.info_mode == 2)
            h.direction_mark(0);
        else if (f.info_mode == 3)
            h.direction_mark(1);
        if (f.dg_95fc != 0) h.direction_mark(2);
    }
    // [BYTES seg7:0xc61-0xc8f]: pool 7, record 0
    if (c.flight.pools.records(kPool7) > 0 && c.flight.pools.rec(kPool7, 0).state == 0) {
        h.seg16_a2();
    } else if (f.dg_6da8 != 0 && passed(f.dg_6da8, g.now)) {
        if (h.seg16_33a2()) h.seg16_3306();
    }
    f.dg_5d2 = f.dg_5d2 == 0 ? 1 : 0;  // [BYTES seg7:0xc94-0xc9d]
    if (f.dg_1016 != 0) h.seg11_a9ea(1);
    h.sound_frame();  // seg7:0xcaf
    if (g.wave_kind == 6) h.seg12_4d8e();
    if (g.dg_913a != 0 || g.dg_913c != 0) h.seg18_d94();
    // 10. The movers in their order [BYTES seg7:0xcd3-0xd2e]
    move_ships(c.flight);  // seg13:0x0
    move_rocks(c);         // seg7:0x3bae
    h.mover(Mover::k12_532c);
    if (World* w = h.frame_world(c.flight.pools))  // seg12:0x633e (M3 s4 DRAW)
        move_pool8(c, *w);
    else
        h.mover(Mover::k12_633e);
    h.mover(Mover::k8_8256);
    if (World* w = h.frame_world(c.flight.pools))  // seg7:0x309c (M4 s2 CAPTURE)
        move_icons(c, *w);
    else
        h.mover(Mover::k7_309c);
    if (World* w = h.frame_world(c.flight.pools))  // seg7:0x32d0, pool 1 (fragments.h) (M4 s3 FUEL)
        move_fragments(c, *w);
    else
        h.mover(Mover::k7_32d0);
    h.mover(Mover::k7_40c4);
    if (World* w = h.frame_world(c.flight.pools))  // seg12:0x0, the pool-6 mover (missile.h) (M4 s2 fixer)
        move_missiles(*w, c);
    else
        h.mover(Mover::k12_0);
    h.mover(Mover::k6_73f6);
    h.mover(Mover::k6_7c14);
    if (World* w = h.frame_world(c.flight.pools); w && w->w9456 > 0) h.mover(Mover::k7_5e04);
    h.mover(Mover::k12_3148);
    // seg12:0x6b1e (pool 11), seg12:0xae4 (pool 4) while DGROUP:0x9600 != 0, then seg12:0x3e98(0)
    // [BYTES seg7:0xd1b seg7:0xd20 seg7:0xd27 seg7:0xd2e] (s3-fuel-verify.md claim 58) (M4 s3 FUEL).
    // DGROUP:0x9600 has one writer, seg9:0x6833 (= 1), and nothing clears it: once a placement
    // has run, the pool-4 mover runs on every later frame of the session (verified claim 57).
    if (World* w = h.frame_world(c.flight.pools)) {
        move_pods(c, *w);
        if (f.dg_9600 != 0) move_tanks(c, *w);
    } else {
        h.mover(Mover::k12_6b1e);
        if (f.dg_9600 != 0) h.mover(Mover::k12_ae4);
    }
    // seg12:0x3e98(0) returns at once unless the wave is kind 6 [BYTES seg12:0x3ea8-0x3eaf]; its
    // race body, which calls the tank respawn seg12:0x4f4e at seg12:0x3ee5, is M6: the stub
    // stays. A kind-7 wave's frame calls the duel respawn seg12:0x515e instead [BYTES seg7:0xe71
    // seg7:0xe78] (core.md "The fuel tank", Respawns; C080), also M6 and not ported; outside both
    // a taken tank is not respawned, as in the original (M4 s4 fixer).
    h.mover(Mover::k12_3e98);
    // [BYTES seg7:0xd34-0xd78]
    if (passed(f.music_due, g.now) && f.dg_950 == 0 && f.race == 0 && g.demons_gate == 0 &&
        g.dg_9130 == 0)
        h.music_due();
    if (g.game_frames % 300u == 0) h.seg13_b93a();
    // The player's sprite or the 1500 ms tick [BYTES seg7:0xd7d-0x117d]
    const ShipRecord& ps = c.flight.ships[static_cast<std::size_t>(g.player)];
    if (f.dg_58a != 0 || f.dg_588 != 0 || ps.beb != 0) {
        if (g.autopilot == 0 && g.dg_238c == 0) h.player_sprite();  // seg7:0xda9
    } else if (!(ps.due156.get() > g.now)) {
        h.player_tick();  // seg7:0xdce-0x117d [OPEN]
    }
    if (f.dg_9148 > 0) h.seg7_158e();  // [BYTES seg7:0x1182-0x118d]
    h.seg13_447a();
}

// seg7:0x1d0c [BYTES seg7:0x1d1b-0x1f2c], docs/re/world.md "Ship entry (seg7:0x1d0c)". The entry
// moves the player from the set-up's gate centre to the post [BYTES seg7:0x1dd0 seg9:0x6320]
// (M3 s3 fixer, C033-C036); the start heading stays [OPEN Start heading].
void ship_entry(UpdateCtx& c) {
    WaveGlobals& g = c.flight.g;
    FrameGlobals& f = c.f;
    Pools& pools = c.flight.pools;
    if (c.flight.ship_obj(g.player).state == 100) c.host.player_sprite();  // [BYTES seg7:0x1d24]
    for (int i = 0; i <= g.last_ship && i < kShipCount; ++i) {
        ShipRecord& si = c.flight.ships[static_cast<std::size_t>(i)];
        if (!(si.d88.get() < g.now)) continue;  // [BYTES seg7:0x1efe-0x1f16]
        const std::int16_t kind = c.flight.host.design_kind(si.design_index);  // DGROUP:0x970a
        // When ship i waits off-world, only the first later waiting ship gets now + 5000: the
        // store falls through out of the j loop (M3 s3 fixer, C036) [BYTES seg7:0x1d4c seg7:0x1d57
        // seg7:0x1d70 seg7:0x1d78].
        for (int j = i + 1; j <= g.last_ship && j < kShipCount; ++j) {
            if (c.flight.ship_obj(i).state == 100 && c.flight.ship_obj(j).state == 100) {
                c.flight.ships[static_cast<std::size_t>(j)].d88.set(g.now + 5000u);
                break;
            }
        }
        ObjectRecord& o = c.flight.ship_obj(i);
        if (o.state != 100) continue;                          // [BYTES seg7:0x1d80]
        if (kind == 7 && (si.w5e & 0xff) == 1) continue;       // byte +0x5e [BYTES seg7:0x1d89-0x1d99]
        std::int16_t gate = g.gates != 0 ? imod16(static_cast<std::int16_t>(c.flight.rng.rand()), g.gates) : 0;
        if (i == g.player) gate = 0;  // [BYTES seg7:0x1daa-0x1db2]
        const int post = gate * 2;     // 0xe0 bytes: records 2g and 2g + 1 of pool 8
        if (pools.records(kPoolGatePosts) <= post + 1) return;  // [DESIGN] storage bound
        const ObjectRecord& p0 = pools.rec(kPoolGatePosts, post);
        const ObjectRecord& p1 = pools.rec(kPoolGatePosts, post + 1);
        o.x = p0.x;  // dword +0x04 [BYTES seg7:0x1dc0-0x1dd4]
        o.y = p0.y;
        const std::int16_t e0 = pools.rec(kPoolGatePosts, 0).extent;  // record 0's +0x20
        const std::int16_t hs = idiv16(o.extent, 2);
        const std::int16_t off =
            add16(sub16(idiv16(sub16(p1.x, p0.x), 2), hs), idiv16(e0, 2));  // [BYTES seg7:0x1dea-0x1e27]
        o.x = add16(o.x, off);
        o.state = 0;  // [BYTES seg7:0x1e35]
        if (i == g.player) c.host.set_design_word_22(si.design_index, 1);  // DGROUP:0x9714 + 0xad n
        {
            ObjectRecord& p = c.flight.ship_obj(g.player);  // seg8:0x0 [BYTES seg7:0x1e54]
            ViewInputs vi;
            vi.player_x = p.x;
            vi.player_y = p.y;
            vi.autopilot = g.autopilot;
            vi.shake = g.dg_238c;
            vi.wave_kind = g.wave_kind;
            vi.dg_9eb5 = g.dg_9eb5;
            recentre(c.view, vi);
        }
        c.flight.host.sound(0x27, 0x0e, i, 1);  // seg10:0x754a(0x27, 14, i, 1)
        c.host.ship_entered(i);                 // seg12:0x2556(14, i), seg7:0x13b2(2, 14, i)
        f.entry_pending = 0;                    // [BYTES seg7:0x1e85-0x1eb9]
        for (int j = 0; j <= g.last_ship && j < kShipCount; ++j)
            if (c.flight.ship_obj(j).state == 100 && c.flight.ships[static_cast<std::size_t>(j)].bec != 0)
                f.entry_pending = 1;
        if (f.entry_pending == 0 && g.wave_kind == 7) {  // [BYTES seg7:0x1ebb-0x1ec9]
            // DGROUP:0x668a is gate 0's speed word World::gate[0].w12, its one copy (M3 s4 DRAW,
            // s4 fixer review L6).
            if (World* w = c.host.frame_world(pools)) w->gate[0].w12 = 1;
        }
        if ((f.race != 0 || f.dg_950 != 0) && i != g.player) c.host.seg13_563c();  // [BYTES seg7:0x1ecf-0x1eee]
        return;
    }
}

// seg7:0x3bae [BYTES seg7:0x3bbd-0x40ba]
void move_rocks(UpdateCtx& c) {
    FrameScope scope(c.host, c);
    WaveGlobals& g = c.flight.g;
    FrameGlobals& f = c.f;
    Pools& pools = c.flight.pools;
    UpdateHost& h = c.host;
    const int pool = g.rock_pool;  // DGROUP:0x934a
    for (int i = 0; i < pools.in_use(pool) && i < pools.records(pool); ++i) {
        ObjectRecord& o = pools.rec(pool, i);
        if (o.state != 0) continue;
        const std::int16_t pr = pool_rec(pool, i);
        auto heat = [&o]() { return static_cast<std::int8_t>(o.flame); };
        if (o.on_screen != 0) {
            int slot = 0;
            int masked = 0;
            std::uint16_t old_brush = 0;
            if (heat() > 0) {  // [BYTES seg7:0x40a0-0x3c43]
                slot = g.dg_9130 == 0 ? 6 : 0x24;
                old_brush = h.select_rock_brush(heat());
                masked = 1;
                if (o.d6a.get() < g.now) {
                    o.flame = static_cast<std::uint8_t>(o.flame - 1);
                    o.d6a.set(g.now + 3000u);
                }
            } else {  // [BYTES seg7:0x3c45-0x3ca1]
                if (heat() < 0) {
                    if (imod16(static_cast<std::int16_t>(c.flight.rng.rand()), 7) == 2)
                        h.puff(pool, i, 0);  // seg7:0x5b68(rocks, i, 0)
                    o.flame = static_cast<std::uint8_t>(o.flame + 1);
                }
                slot = g.dg_9130 != 0 ? 0x23 : 7;
                masked = 0;
            }
            if (g.dg_9130 == 0) {  // small cells take the next slot [BYTES seg7:0x3ca6-0x3ccd]
                if (o.size < sprite_side(30, g.size)) slot += 2;
            } else if ((g.size == 0 && o.size < 12) || (g.size == 1 && o.size < 30)) {
                slot += 2;  // [BYTES seg7:0x3cd1-0x3d12]
            }
            limit_speed(pools, 0x28, pr);   // seg8:0x1d54(0x28, ...)
            integrate(c.flight, 1, pr);     // seg8:0x1b54(1, ...)
            o.frame = idiv16(o.angle, 100);  // [BYTES seg7:0x3d40-0x3d87]
            o.src_x = add16(mul16(add16(o.size, o.gap), idiv16(o.angle, 100)), o.gap);
            h.draw_sprite(pr, 1, slot, masked);  // seg8:0xb76(pr, 1, DGROUP:0x39e8[slot], masked)
            if (heat() > 0) h.restore_brush(old_brush);
            // The spin [BYTES seg7:0x3dc5-0x3e5a]
            std::int16_t d = add16(abs16(o.vx), abs16(o.vy));
            if (o.vx < 0) d = sub16(0, d);
            o.angle = add16(o.angle, d);
            if (o.angle > 1799) o.angle = 0;
            if (o.angle < 0) o.angle = 1799;
        } else if (imod16(f.dg_9142, 3) == 0) {  // off screen [BYTES seg7:0x3e63-0x3ef7]
            limit_speed(pools, 0x28, pr);
            integrate(c.flight, 3, pr);
            if (heat() > 0) {
                if (o.d6a.get() < g.now) {
                    o.flame = static_cast<std::uint8_t>(o.flame - 1);
                    o.d6a.set(g.now + 3000u);
                }
            } else if (heat() < 0) {
                o.flame = static_cast<std::uint8_t>(o.flame + 1);
            }
        }
        // Collisions [BYTES seg7:0x3efb-0x4068]
        if (o.on_screen == 0 || o.w1c != 0) continue;
        f.dg_943c = static_cast<std::int16_t>(i);
        f.dg_943a = static_cast<std::int16_t>(pool);
        const std::uint16_t hit = h.collide(pr);
        if (hit == 0xffffu) continue;
        const int hp = hit >> 8, hr = hit & 0xff;
        if (hp == g.pool5 && hp < kPoolCount && hr < pools.records(hp)) {
            if (pools.rec(hp, hr).mass == 0x78) {  // [BYTES seg7:0x3f65-0x3fa9]
                std::int16_t b = add16(heat(), 1);
                if (b > 9) b = 9;
                o.flame = static_cast<std::uint8_t>(b);
                o.d6a.set(g.now + 5000u);
            } else if (pools.rec(hp, hr).mass == 0xc8 && heat() > 0) {  // [BYTES seg7:0x3fab-0x3ffa]
                o.flame = static_cast<std::uint8_t>(-static_cast<int>(static_cast<std::uint8_t>(5 * o.flame)));
                h.puff(pool, i, 0);
            }
        }
        f.dg_9454 = heat();  // [BYTES seg7:0x3ffd-0x4068]
        h.rock_hit(i, hit);
        f.dg_9454 = 0;
    }
}

// ---- seg12:0x633e, seg12:0x1776, seg12:0x1f7a (work/analysis/m3/s4-draw.md 1) (M3 s4 DRAW)

void move_pool8(UpdateCtx& c, World& w) {
    WaveGlobals& g = c.flight.g;
    UpdateHost& h = c.host;
    Pools& p = w.pools;
    // [BYTES seg12:0x634b-0x6356]
    w.w94a0 = add16(idiv16(add16(w.detail, 1), 2), 2);
    if (w.w1c32 == 0) h.seg12_7006();  // [BYTES seg12:0x6359-0x6361]
    w.w1c32 = add16(w.w1c32, 1);       // [BYTES seg12:0x6364-0x636f]
    if (w.w1c32 > 1) w.w1c32 = 0;
    w.w0d08 = add16(w.w0d08, w.w0d06);  // [BYTES seg12:0x6375-0x638a]
    if (w.w0d08 > 4 || w.w0d08 < 2) w.w0d06 = sub16(0, w.w0d06);
    w.w949e = 1;  // [BYTES seg12:0x638e-0x6399]
    w.w949c = 0;
    w.w949a = 0;
    // The kind dispatch [BYTES seg12:0x63a1-0x6406].
    for (int i = 0; i < p.in_use(kPoolGatePosts) && i < p.records(kPoolGatePosts) && i < kPool8Max; ++i) {
        ObjectRecord& o = p.rec(kPoolGatePosts, i);
        if (o.state != 0) continue;
        o.w1c = 0;
        const std::int16_t kind = w.pool8[static_cast<std::size_t>(i)].at(0x1c);  // DGROUP:0x3d6c
        if (kind == 1) {
            h.hazard(1, i);  // seg12:0x390e(i) (M5)
        } else if (kind == 2) {
            h.hazard(2, i);  // seg12:0x5636(i) (M5)
        } else if (kind == 3) {
            move_gate(c, w, i / 2);
            ++i;
        }
    }
    // The hazard marks: written by seg12:0x5636 only [BYTES seg12:0x6408-0x6458].
    if (w.w949e != 0) {
        Blitter* out = h.blitter();
        const std::int16_t side = sprite_side(2, g.size);
        for (int k = 0; k < w.w94a0 && k < static_cast<int>(w.marks.size()); ++k) {
            const HazardMark& m = w.marks[static_cast<std::size_t>(k)];
            if (m.w8 != 0 && m.w8 < m.wa && out) out->pat_blt(kDcWindow, m.x, m.y, side, side, kBlackness);
        }
    }
    // [BYTES seg12:0x645a-0x649f]
    if (w.w949a != 0) w.w6988 = g.now + 1000u;
    if (w.w949c != 0) w.w666c = g.game_frames + (w.kind == 6 ? 3u : 8u);
}

void move_gate(UpdateCtx& c, World& w, int p) {
    FrameScope scope(c.host, c);
    WaveGlobals& g = c.flight.g;
    FrameGlobals& f = c.f;
    UpdateHost& h = c.host;
    Pools& pools = w.pools;
    const int r0i = 2 * p, r1i = 2 * p + 1;
    if (p < 0 || p >= kMaxGates || pools.records(kPoolGatePosts) <= r1i) return;  // [DESIGN] bounds
    GateRecord& gate = w.gate[static_cast<std::size_t>(p)];
    ObjectRecord& r0 = pools.rec(kPoolGatePosts, r0i);
    ObjectRecord& r1 = pools.rec(kPoolGatePosts, r1i);
    const std::int16_t s = w.shift;  // DGROUP:0x9150
    const int size = g.size;          // DGROUP:0x9156
    Blitter* out = h.blitter();
    WaveGraphics& wg = wave_graphics();
    if (out) create_wave_brushes(*out, wg);  // on first use (rock_images.h) [DESIGN]
    r0.w1c = 0;  // [BYTES seg12:0x178f-0x17a9]
    r1.w1c = 0;
    // The beam brush index, gate 0 only [BYTES seg12:0x17ab-0x17cd].
    if (w.beam_index != 0 && p == 0) {
        w.beam_index = add16(w.beam_index, w.beam_step);
        if (w.beam_index > 4 || w.beam_index < 2) w.beam_step = sub16(0, w.beam_step);
    }
    std::int16_t& v = gate.w12;  // DGROUP:0x668a + 0x1c·p, the speed
    std::int16_t draw_w = 0;     // [bp-0xc], the PATCOPY width
    std::int16_t erase_w = 0;    // [bp-0xe], the erase width (gap + 16)
    for (int r = r0i; r <= r1i; ++r) {
        ObjectRecord& o = pools.rec(kPoolGatePosts, r);
        const bool second = r == r1i;
        // a. The gap and the moving gate [BYTES seg12:0x1862-0x1888 seg12:0x1ed6-0x1f6a
        // seg12:0x17dc-0x1860].
        if (v == 0) {
            if (second) draw_w = erase_w = sub16(sub16(r1.x, r1.extent), r0.x);
        } else {
            if (second) erase_w = add16(sub16(sub16(r1.x, r1.extent), r0.x), 16);
            const std::int16_t step = mul16(v, 8);
            if (!second) {
                r0.x = add16(r0.x, step);
                r0.vx = shl16(v, 3);
                draw_w = sub16(sub16(r1.x, r0.x), r0.extent);
                gate.beam.l = add16(gate.beam.l, step);
            } else {
                r1.x = add16(r1.x, sub16(0, step));
                r1.vx = mul16(v, -8);
                draw_w = sub16(sub16(r1.x, r1.extent), r0.x);
                gate.beam.r = add16(gate.beam.r, sub16(0, step));
            }
            if (gate.width < draw_w || draw_w < gate.w0e) v = w.kind == 7 ? std::int16_t{0} : sub16(0, v);
        }
        // b. The post [BYTES seg12:0x18a3-0x19e7].
        if (o.on_screen != 0) {
            int slot = kSlotGate;
            int masked = 0;
            std::uint16_t old = 0;
            const std::int8_t heat = static_cast<std::int8_t>(o.flame);  // byte +0x2f
            if (heat < 3) {
                if (heat < 0) {  // [BYTES seg12:0x195b seg12:0x1969 seg12:0x1972 seg12:0x1982]
                    if (imod16(static_cast<std::int16_t>(c.flight.rng.rand()), 7) == 2)
                        c.flight.host.puff(kPoolGatePosts, r, 0);  // seg7:0x5b68(8, r, 0)
                    o.flame = static_cast<std::uint8_t>(o.flame + 1);
                }
            } else {
                slot = kSlotGateMask;  // [BYTES seg12:0x18ad-0x1940]
                masked = 1;
                int b = heat < 5 ? heat + w.beam_index - 1 : heat - w.beam_index;
                if (b < 1) b = 1;
                old = h.select_rock_brush(b);  // DGROUP:0x3b74[b]
                if (o.d6a.get() < g.now) {
                    o.flame = static_cast<std::uint8_t>(o.flame - 1);
                    o.d6a.set(g.now + 3000u);
                }
            }
            // +0x10 = (size + gap)·((r - 2p) % 2) + gap: cell 0 for 2p, cell 1 for 2p + 1
            // [BYTES seg12:0x19a1 seg12:0x19ad seg12:0x19b5 seg12:0x19bb].
            o.src_x = add16(mul16(add16(o.size, o.gap), static_cast<std::int16_t>((r - 2 * p) % 2)), o.gap);
            // seg8:0xb76((char)r | 0x800, 1, DGROUP:0x39e8[slot], masked) [BYTES seg12:0x19d5].
            h.draw_sprite(pool_rec(kPoolGatePosts, r), 1, slot, masked);
            if (old != 0) h.restore_brush(old);
        }
        // c. The beam and d. the OUT sign, on r1 [BYTES seg12:0x19fa-0x1c9d].
        if (second && (r0.on_screen != 0 || r1.on_screen != 0)) {
            const std::size_t bi = static_cast<std::size_t>(w.beam_index);
            const BrushId old =
                out && bi < wg.beam.size() ? out->select_brush(kDcWindow, wg.beam[bi]) : kBrushNone;
            gate.old_beam_x = gate.beam_x;
            gate.old_beam_y = gate.beam_y;
            std::int16_t px = r0.scr_x, py = r0.scr_y;
            if (r0.on_screen == 0) {  // seg8:0x29ea(r0.x, r0.y) [BYTES seg12:0x1a54]
                const ScreenPoint sp = world_to_screen(c.view, r0.x, r0.y, g.wave_kind, g.dg_9eb5, g.dg_9130);
                px = sp.x, py = sp.y;
            }
            gate.beam_x = add16(px, r0.size);
            gate.beam_y = add16(sprite_side(15, size), py);
            if (out) {
                if (gate.beam_x != gate.old_beam_x || gate.beam_y != gate.old_beam_y)
                    out->pat_blt(kDcWindow, gate.old_beam_x, gate.old_beam_y,
                                 add16(sprite_side(1, size), sar16(erase_w, s & 15)),
                                 sprite_side(2, size), kBlackness);
                out->pat_blt(kDcWindow, gate.beam_x, gate.beam_y, sar16(draw_w, s & 15),
                             sprite_side(2, size), kPatCopy);
                if (old != kBrushNone) out->select_brush(kDcWindow, old);
            }
            // The OUT sign flashes once the icons are gone (DGROUP:0x91fc = 0) [BYTES seg12:0x1b3b
            // seg12:0x1b5d seg12:0x1b63 seg12:0x1b73 seg12:0x1c0e seg12:0x1c64]. The second
            // frame lands one column right of seg10:0x3ea2's (2 gap + size + 7 against 2 (gap +
            // 3) + size), as the bytes say.
            if (w.icons_b == 0 && w.out_due < g.now) {
                w.out_due = g.now + 400u;
                w.out_frame = w.out_frame == 0 ? 1 : 0;
                if (out && wg.gate_dc != kDcNone && wg.out_sign != kDcNone) {
                    const std::int16_t fw = sprite_side(kOutSignW, size), fh = sprite_side(kOutSignH, size);
                    const std::int16_t sy = add16(sprite_side(10, size), r1.gap);
                    out->bit_blt(wg.gate_dc, add16(sprite_side(2, size), r1.gap), sy, fw, fh,
                                 wg.out_sign, mul16(fw, w.out_frame), 0, kSrcCopy);
                    out->bit_blt(wg.gate_dc, add16(add16(mul16(r1.gap, 2), r1.size), sprite_side(7, size)),
                                 sy, fw, fh, wg.out_sign, mul16(fw, w.out_frame), 0, kSrcCopy);
                }
            }
        }
        // e. Collisions and f. the passage [BYTES seg12:0x1ca2-0x1ebd].
        if (o.on_screen == 0 || o.w1c != 0) continue;
        f.dg_943a = kPoolGatePosts;
        f.dg_943c = static_cast<std::int16_t>(r);
        const std::uint16_t hit = h.collide(pool_rec(kPoolGatePosts, r));
        if (hit != 0xffffu) {
            const int hp = hit >> 8, hr = hit & 0xff;
            if (hp == g.pool5 && hp < kPoolCount && hr < pools.records(hp)) {
                const std::int8_t heat = static_cast<std::int8_t>(o.flame);
                if (pools.rec(hp, hr).mass == 0x78) {  // [BYTES seg12:0x1d06 seg12:0x1d17]
                    o.flame = static_cast<std::uint8_t>(heat + 1 > 9 ? 9 : heat + 1);
                    o.d6a.set(g.now + 5000u);
                } else if (pools.rec(hp, hr).mass == 200 && heat > 0) {  // [BYTES seg12:0x1d2a seg12:0x1d56]
                    o.flame = static_cast<std::uint8_t>(heat * -5);
                }
            }
            h.post_hit(r, hit);  // seg8:0x546c, seg8:0x5df2 [BYTES seg12:0x1da9 seg12:0x1dbe] (M4)
        }
        const std::uint16_t t = gate_test(c, w, p);  // [BYTES seg12:0x1dca]
        const int tp = t >> 8, tr = t & 0xff;
        if (t == 0xffffu || tp >= kPoolCount || tr >= pools.records(tp)) continue;
        // Ship byte +0xe8 (DGROUP:0x7178) [BYTES seg12:0x1dda seg12:0x1dff].
        const std::int8_t be8 = tp == g.ship_pool && tr < kShipCount
                                    ? static_cast<std::int8_t>(c.flight.ships[static_cast<std::size_t>(tr)].be8)
                                    : std::int8_t{0};
        ObjectRecord& to = pools.rec(tp, tr);
        if (to.b2e != 0 && be8 == 0) continue;
        h.gate_passed(tp, tr);  // seg12:0x2288 [OPEN]
        if (tp == g.ship_pool && tr == g.player) {
            w.w1e80 = g.now + 1000u;  // [BYTES seg12:0x1e11 seg12:0x1e28]
        } else {
            // [BYTES seg12:0x1e81 seg12:0x1eb2 seg12:0x1ebd]
            if (out) out->pat_blt(kDcWindow, to.scr_x, to.scr_y, to.size, to.size, kBlackness);
            c.flight.host.sound(0x27, tp, tr, 1);
        }
    }
}

std::uint16_t gate_test(UpdateCtx& c, World& w, int p) {
    WaveGlobals& g = c.flight.g;
    Pools& pools = w.pools;
    const GateRecord& gate = w.gate[static_cast<std::size_t>(p)];
    // A duel's closed static gate [BYTES seg12:0x1f89-0x1faf].
    if (w.kind == 7 && w.gate[0].w12 == 0 && pools.records(kPoolGatePosts) > 1) {
        const ObjectRecord& a = pools.rec(kPoolGatePosts, 0);
        const ObjectRecord& b = pools.rec(kPoolGatePosts, 1);
        if (sub16(sub16(b.x, a.x), a.extent) < 100) return 0xffffu;
    }
    const VisibilityGlobals* list = c.host.visible_list();
    if (!list) return 0xffffu;
    const int n = list->count < kVisibleMax ? list->count : kVisibleMax;  // DGROUP:0x9146
    for (int k = 0; k < n; ++k) {
        const int lp = static_cast<std::int8_t>(list->pool[static_cast<std::size_t>(k)]);
        const int lr = static_cast<std::int8_t>(list->rec[static_cast<std::size_t>(k)]);
        if (lp < 0 || lp >= kPoolCount || lr < 0 || lr >= pools.records(lp)) continue;
        ObjectRecord& o = pools.rec(lp, lr);
        if (o.state != 0) continue;
        const bool ship = lp == g.ship_pool && lr < kShipCount;
        const Rect16 sq = rect16(o.x, o.y, o.x + o.extent, o.y + o.extent);
        if (intersect(gate.beam, sq)) {
            bool due = true;
            if (ship) {  // dword +0x88 + 300 (the player) or + 3000, unsigned [BYTES seg12:0x2040-0x20a6]
                const std::uint32_t t =
                    c.flight.ships[static_cast<std::size_t>(lr)].d88.get() + (lr == g.player ? 300u : 3000u);
                due = t < g.now;
            }
            if (lp != 7 && o.mass != 0xdac && due) {
                if (o.b2e != 1 || (ship && word_at(c.flight.ships[static_cast<std::size_t>(lr)], 0x2e) == 2))
                    return static_cast<std::uint16_t>((lp << 8) | (lr & 0xff));
            }
        }
        // A computer ship heading for this gate (ship words +0x32 = 12, +0x34 = the centre
        // record, +0x38 = 2) stops near it and gets its AI words (M5) [BYTES seg12:0x206f-0x21ca].
        if (!ship) continue;
        ShipRecord& sr = c.flight.ships[static_cast<std::size_t>(lr)];
        if (word_at(sr, 0x32) != 12 || word_at(sr, 0x34) != gate.post || word_at(sr, 0x38) != 2) continue;
        if (lr == g.player && g.play_mode == 0) continue;
        if (pools.records(kPoolGateCentres) <= gate.post || gate.post < 0) continue;
        const ObjectRecord& centre = pools.rec(kPoolGateCentres, gate.post);
        const Rect16 around = rect16(o.x - 320, o.y - 320, o.x + 800, o.y + 800);
        if (!pt_in_rect(around, centre.x, centre.y)) continue;
        o.vx = 0;
        o.vy = 0;
        // The hand-off runs when the gate stands, or closes (speed < 0) with the beam within 16
        // of 480 wide: seg9:0x88ba is abs, its argument beam.r - beam.l - 0x1e0
        // [BYTES seg12:0x216e-0x218c] (M3 s4 fixer, review M1).
        const std::int16_t sp = gate.w12;
        const std::int16_t off = sub16(sub16(gate.beam.r, gate.beam.l), 0x1e0);
        const std::int16_t aoff = off < 0 ? sub16(0, off) : off;
        if (sp == 0 || (sp < 0 && aoff <= 16)) {
            const std::int16_t next = add16(gate.post, 1);
            set_word_at(sr, 0x32, 12);
            set_word_at(sr, 0x06, 12);
            set_word_at(sr, 0x34, next);
            set_word_at(sr, 0x08, next);
            set_word_at(sr, 0x38, 2);
            set_word_at(sr, 0x36, 2);
            set_word_at(sr, 0x3c, 2);
            set_word_at(sr, 0x3a, 2);
            c.host.gate_ai(lr);  // seg13:0x2f30 (M5)
        }
    }
    return 0xffffu;
}

}  // namespace is::world
