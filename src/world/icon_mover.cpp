// The icon mover (icon_mover.h).
#include "world/icon_mover.h"

#include "world/capture.h"
#include "world/damage.h"
#include "world/icon_effects.h"

namespace is::world {

namespace {

// The frame's context on the host while the mover runs (as update.cpp's movers do).
struct Scope {
    UpdateHost& h;
    UpdateCtx* prev;
    Scope(UpdateHost& host, UpdateCtx& c) : h(host), prev(host.frame_ctx) {
        if (!prev) h.frame_ctx = &c;
    }
    ~Scope() { h.frame_ctx = prev; }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
};

IconRecord& icon_rec(World& w, int i) {
    static IconRecord scratch;
    if (i < 0 || i >= kIconRecords) {
        scratch = IconRecord{};
        return scratch;
    }
    return w.icons[static_cast<std::size_t>(i)];
}

}  // namespace

void icon_offscreen_step(UpdateCtx& c, int i) {
    const std::int16_t pr = pool_rec(kPoolIcons, i);
    limit_speed(c.flight.pools, 0x1e, pr);                              // [BYTES seg7:0x32a9]
    if (c.flight.pools.rec(kPoolIcons, i).b54 == 0) integrate(c.flight, 3, pr);  // [BYTES seg7:0x32b6-0x32c0]
}

void move_icons(UpdateCtx& c, World& w) {
    Scope scope(c.host, c);
    UpdateHost& h = c.host;
    Pools& pools = w.pools;
    if (w.handover.demons_gate != 0) return;  // [BYTES seg7:0x30ab]
    if (pools.records(kPoolIcons) == 0) return;
    // The cell of pool-0 record 0 [BYTES seg7:0x30b4-0x30c3]
    const std::int16_t size = pools.rec(kPoolIcons, 0).size;
    const std::int16_t gap = pools.rec(kPoolIcons, 0).gap;
    for (int i = 0; i < w.icons_a && i < pools.records(kPoolIcons); ++i) {  // DGROUP:0x91fa [BYTES seg7:0x320d]
        ObjectRecord& o = pools.rec(kPoolIcons, i);
        if (o.state != 0) continue;  // [BYTES seg7:0x321d]
        IconRecord& ic = icon_rec(w, i);
        int reset = 0;  // [bp-0x1c]
        if (ic.w00 != 0) {  // DGROUP:0x57a8 [BYTES seg7:0x322d-0x324b]
            icon_melt_step(c, w, i);  // seg7:0x4d86(i) [BYTES seg7:0x323a]
            if (ic.w00 < 7) {
                ic.w00 = 0;
                reset = 1;
            }
        }
        const std::int16_t pr = pool_rec(kPoolIcons, i);
        if (o.on_screen != 0) {  // [BYTES seg7:0x325c]
            int slot;
            if (ic.w02 == 0) {  // the strip cell [BYTES seg7:0x3272-0x3285]
                o.src_x = add16(mul16(add16(gap, size), static_cast<std::int16_t>(i)), gap);
                slot = kIconStripSlot;
            } else {  // the icon's own image, cell w02/2 - 1 [BYTES seg7:0x30ce-0x30fb]
                slot = kIconOwnSlot + i;
                o.src_x = add16(mul16(sub16(idiv16(ic.w02, 2), 1), add16(gap, size)), gap);
            }
            limit_speed(pools, 0x28, pr);              // seg8:0x1d54(0x28, i) [BYTES seg7:0x3108]
            if (o.b54 == 0) integrate(c.flight, 1, pr);  // seg8:0x1b54(1, i) unless `fixed` [BYTES seg7:0x3115-0x311f]
            h.draw_sprite(pr, 1, slot, 0);             // seg8:0xb76(i, 1, dc, 0) [BYTES seg7:0x3132]
            // On screen after the draw and not `collided` (verified claim 2) [BYTES seg7:0x3143-0x3177]
            if (o.on_screen != 0 && o.w1c == 0) {
                c.f.dg_943a = 0;
                c.f.dg_943c = static_cast<std::int16_t>(i);
                const std::uint16_t hit = h.collide(pr);
                if (hit != 0xffffu) icon_hit(c, w, hit >> 8, hit & 0xff, i);
            }
        } else if (imod16(c.f.dg_9142, 3) == 0) {  // DGROUP:0x9142 [BYTES seg7:0x317f-0x3190]
            icon_offscreen_step(c, i);
        }
        // Every 30th frame a negative heat rises; at 0 the icon is capturable again; at -2
        // seg7:0x5660 [BYTES seg7:0x3193-0x31d8].
        if (imod16(c.f.dg_9142, 30) == 0 && static_cast<std::int8_t>(o.flame) < 0) {
            o.flame = static_cast<std::uint8_t>(o.flame + 1);
            if (o.flame == 0) o.b53 = 1;
            if (o.flame == 0xfe) {  // seg7:0x5660(i) [BYTES seg7:0x31cd-0x31d8]
                CollideCtx x = frame_collide_ctx(c, w);
                icon_restore(x, i);
            }
        }
        // DGROUP:0x57aa counts 2 ... 11 while not 0 [BYTES seg7:0x31dc-0x31f8]
        if (ic.w02 != 0) {
            if (ic.w02 > 10) ic.w02 = 1;
            ic.w02 = add16(ic.w02, 1);
        }
        if (reset != 0) icon_burnt_out(c, w, i);  // seg7:0x4ffe(i) [BYTES seg7:0x31fc-0x3206]
    }
}

void icon_hit(UpdateCtx& c, World& w, int P, int R, int icon) {
    UpdateHost& h = c.host;
    WaveGlobals& g = c.flight.g;
    CollideCtx x = frame_collide_ctx(c, w);
    ObjectRecord& I = obj(w, kPoolIcons, icon);
    // The touch: a ship of a capturing design, ship byte +0xea = 0 and the icon not dissolving
    // (verified claim 1) [BYTES seg7:0x4a47-0x4a87].
    bool touched = false;
    if (P == g.ship_pool) {
        const ShipRecord& S = ship_at(w, R);
        if (x.h.design(w, S.design_index).captures != 0 && S.bea == 0 && I.race_status == 0) {
            CaptureCtx cc{x, c.flight, h.capture_host()};
            touch(cc, R, pool_rec(kPoolIcons, icon));  // seg13:0x16ac(R, icon) [BYTES seg7:0x4a7f]
            touched = true;
        }
    }
    if (!touched && P == g.pool5) icon_pool5_hit(c, w, R, icon);  // [BYTES seg7:0x4a8a-0x4c4f]
    // The tail seg7:0x4c54 (verified claim 3): the response and the dispatch with the icon as T,
    // after a touch too, unless a pool-5 FIREBALL (mass 0x78) met it [BYTES seg7:0x4c54-0x4cbd].
    if (!(P == g.pool5 && obj(w, P, R).mass == 0x78)) {
        const std::int16_t a = respond(x, I.mass, add16(I.extent, 2), &I.vy, &I.vx,
                                       static_cast<std::uint16_t>(pool_rec(P, R)));
        dispatch(x, kPoolIcons, icon, P, R, a);
    }
    // A hot rock (heat > 4) sets DGROUP:0x57a8 = 80 and plays 0x6a [BYTES seg7:0x4cc0-0x4cef]
    if (P == g.rock_pool && static_cast<std::int8_t>(obj(w, P, R).flame) > 4) {
        icon_rec(w, icon).w00 = 80;
        h.sound(0x6a, 0, icon, 1);
    }
    // While DGROUP:0x57aa of the icon is > 0 [BYTES seg7:0x4cf7-0x4d73]
    if (icon_rec(w, icon).w02 > 0) {
        if (P == 0) {
            icon_rec(w, R).w02 = 2;
            icon_ignite(c, w, R);  // seg7:0x477e(R) [BYTES seg7:0x4d45-0x4d4f]
        } else if (P == 3) {
            ObjectRecord& r = obj(w, 3, R);
            std::int16_t t = add16(static_cast<std::int8_t>(r.flame), 2);
            if (t > 9) t = 9;
            r.flame = static_cast<std::uint8_t>(t);
            r.d6a.set(g.now + 5000u);
        }
        if (P == g.ship_pool && R == g.player) ship_damage(x, g.ship_pool, g.player, 25);  // seg8:0x45e0
    }
}

}  // namespace is::world
