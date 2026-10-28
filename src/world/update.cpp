// The wave's update (update.h).
#include "world/update.h"

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

// seg7:0x7fe-0x993 [BYTES seg7:0x7fe-0x998]
void wave_pass(UpdateCtx& c, bool max_rate) {
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
    // 3. Fire poll on the player in play [BYTES seg7:0x871-0x881 seg11:0x4dba-0x4e00].
    ObjectRecord& player = c.flight.ship_obj(g.player);
    if (player.state == 0 && g.play_mode == 0) {
        const ShipRecord& s = c.flight.ships[static_cast<std::size_t>(g.player)];
        if (s.lockout.get() < g.now) {
            // Shift, then Space only when Shift is up; 0x8001 for 0x8000 [DESIGN ADR 0013 decision 6].
            if (c.flight.keys.test(kVkShift, kFireMask) || c.flight.keys.test(kVkSpace, kFireMask) ||
                g.joy_fire != 0)
                h.fire(g.player);
        }
    }
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
    // 4. Beam slots: 5 x (DGROUP:0x594 + DGROUP:0x598) calls [BYTES seg7:0x938-0x970].
    f.beam_result = 0;
    for (int i = 0; i < 5; ++i) {
        for (int j = 0; add16(f.beam_b, f.beam_a) > j; ++j) f.beam_result = h.beam(i, j);
        ++f.beam_passes;
    }
    // 5. Effects and the particle burst [BYTES seg7:0x972-0x993].
    if (f.effects_on > 0)
        for (int k = 0; k <= f.last_effect; ++k) h.effect(k);
    h.burst();
}

// seg7:0x9ce-0x118d [BYTES seg7:0x9d6-0x118d]
void wave_frame(UpdateCtx& c) {
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
            h.joy_read();
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
    h.stars(0);  // seg7:0xb87 [OPEN Draw order] (M3 s2 fixer)
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
    h.lines();  // seg7:0xbed [OPEN Draw order] (M3 s2 fixer)
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
    h.mover(Mover::k12_633e);
    h.mover(Mover::k8_8256);
    h.mover(Mover::k7_309c);
    h.mover(Mover::k7_32d0);
    h.mover(Mover::k7_40c4);
    h.mover(Mover::k12_0);
    h.mover(Mover::k6_73f6);
    h.mover(Mover::k6_7c14);
    if (f.dg_9456 > 0) h.mover(Mover::k7_5e04);
    h.mover(Mover::k12_3148);
    h.mover(Mover::k12_6b1e);
    if (f.dg_9600 != 0) h.mover(Mover::k12_ae4);
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

// seg7:0x1d0c [BYTES seg7:0x1d1b-0x1f2c]. The entry moves the player from the set-up's gate
// centre to the post: the port's reading [OPEN Ships at wave start, Start heading] until a
// verifier confirms it (M3 s2 fixer).
void ship_entry(UpdateCtx& c) {
    WaveGlobals& g = c.flight.g;
    FrameGlobals& f = c.f;
    Pools& pools = c.flight.pools;
    if (c.flight.ship_obj(g.player).state == 100) c.host.player_sprite();  // [BYTES seg7:0x1d24]
    for (int i = 0; i <= g.last_ship && i < kShipCount; ++i) {
        ShipRecord& si = c.flight.ships[static_cast<std::size_t>(i)];
        if (!(si.d88.get() < g.now)) continue;  // [BYTES seg7:0x1efe-0x1f16]
        const std::int16_t kind = c.flight.host.design_kind(si.design_index);  // DGROUP:0x970a
        // Every later off-world ship waits 5000 ms more [BYTES seg7:0x1d37-0x1d74].
        for (int j = i + 1; j <= g.last_ship && j < kShipCount; ++j) {
            if (c.flight.ship_obj(i).state == 100 && c.flight.ship_obj(j).state == 100)
                c.flight.ships[static_cast<std::size_t>(j)].d88.set(g.now + 5000u);
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
        if (f.entry_pending == 0 && g.wave_kind == 7) f.dg_668a = 1;  // [BYTES seg7:0x1ebb-0x1ec9]
        if ((f.race != 0 || f.dg_950 != 0) && i != g.player) c.host.seg13_563c();  // [BYTES seg7:0x1ecf-0x1eee]
        return;
    }
}

// seg7:0x3bae [BYTES seg7:0x3bbd-0x40ba]
void move_rocks(UpdateCtx& c) {
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

}  // namespace is::world
