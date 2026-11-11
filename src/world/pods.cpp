// Pool 11, the dropped pods (pods.h).
#include "world/pods.h"

#include "world/capture.h"
#include "world/rescale.h"
#include "world/update.h"

namespace is::world {

namespace {

std::int16_t u(const World& w, std::int16_t n) { return sar16(shl16(n, w.size & 15), 1); }

PodRecord& pod_at(int i) {
    static PodRecord scratch;
    if (i < 0 || i >= kPodRecords) {  // [DESIGN] the port's bound on DGROUP:0x8560
        scratch = PodRecord{};
        return scratch;
    }
    return effect_state().pods[static_cast<std::size_t>(i)];
}

}  // namespace

// seg12:0x6b1e [BYTES seg12:0x6b1e-0x6bd8 seg12:0x6be8-0x6ce5]
void move_pods(UpdateCtx& c, World& w) {
    MoverScope scope(c.host, c);
    Pools& pools = w.pools;
    UpdateHost& h = c.host;
    const int n = pools.in_use(11) < pools.records(11) ? pools.in_use(11) : pools.records(11);  // DGROUP:0x941c
    for (int i = 0; i < n; ++i) {
        ObjectRecord& o = pools.rec(11, i);
        const std::int16_t pr = pool_rec(11, i);
        PodRecord& P = pod_at(i);
        if (o.state != 0) {  // [BYTES seg12:0x6beb-0x6bf0 seg12:0x6bb4-0x6bc9]
            if (o.race_status != 0) h.pod_dissolve(11, i);
            continue;
        }
        o.w1c = 0;                           // its own `collided`, every frame [BYTES seg12:0x6bf2]
        if (pod_age(P) > 2) o.b53 = 1;       // capturable from age 3 [BYTES seg12:0x6bf8-0x6c09]
        integrate(c.flight, 1, pr);          // seg8:0x1b54(h, 1) first [BYTES seg12:0x6c0e-0x6c1a]
        limit_speed(pools, 0x28, pr);        // seg8:0x1d54(h, 0x28) [BYTES seg12:0x6c22]
        // The cell (kind - 1)·(size + gap) + gap, 16-bit [BYTES seg12:0x6c35-0x6c51]
        o.src_x = add16(mul16(sub16(P.kind, 1), add16(o.size, o.gap)), o.gap);
        h.draw_sprite(pr, 3, o.w4a, 0);      // seg8:0xb76(h, 3, DGROUP:0x39e8[+0x4a], 0) [BYTES seg12:0x6c55-0x6c6f]
        P.age.lo = static_cast<std::uint16_t>(P.age.lo + 1);  // [BYTES seg12:0x6c74]
        if (o.on_screen != 0 && o.w1c == 0) {  // [BYTES seg12:0x6c83-0x6c8f]
            c.f.dg_943a = 11;
            c.f.dg_943c = static_cast<std::int16_t>(i);
            const std::uint16_t hit = h.collide(pr);  // [BYTES seg12:0x6c9d-0x6ca7]
            if (hit != 0xffffu) {
                const int hp = hit >> 8, hr = hit & 0xff;
                CollideCtx x = frame_collide_ctx(c, w);
                bool touched = false;
                if (hp == c.flight.g.ship_pool) {
                    const ShipRecord& S = ship_at(w, hr);
                    if (S.bea == 0 && S.be8 == 0) {  // [BYTES seg12:0x6cac-0x6ce5]
                        const auto arg = static_cast<std::int16_t>(
                            (static_cast<std::uint16_t>(c.f.dg_943a) << 8) |
                            static_cast<std::uint16_t>(
                                static_cast<std::int16_t>(static_cast<std::int8_t>(c.f.dg_943c & 0xff))));
                        CaptureCtx cc{x, c.flight, h.capture_host()};
                        touch(cc, hr, arg);  // seg13:0x16ac
                        touched = true;
                    }
                }
                if (!touched) {
                    // seg8:0x546c(hit, &vx, &vy, extent + 2, 0x6e) [BYTES seg12:0x6b35-0x6b63]
                    const std::int16_t a = respond(x, kPodBounceMass, add16(o.extent, 2), &o.vy, &o.vx, hit);
                    dispatch(x, 11, i, hp, hr, a);  // [BYTES seg12:0x6b68-0x6b78]
                }
            }
        }
        // The expiry outside Demon's Gate (signed) [BYTES seg12:0x6b80-0x6bad]
        if (pod_age(P) > pod_lifetime(P) && c.flight.g.demons_gate == 0) {
            CollideCtx x = frame_collide_ctx(c, w);
            run_on_destroy(x, 11, i);              // the far call through +0x66 [BYTES seg12:0x6b95-0x6ba2]
            h.collide_host().seg7_2080(11, -1);    // [BYTES seg12:0x6ba9-0x6bad]
        }
    }
}

// seg12:0x6a60 [BYTES seg12:0x6a60-0x6b1c]
void pod_destroyed(World& w, int pool, int rec, const PodDestroyOut& d) {
    if (pool < 0 || pool >= kPoolCount || rec < 0 || rec >= w.pools.records(pool)) return;  // [DESIGN]
    ObjectRecord& o = w.pools.rec(pool, rec);
    // seg8:0x29ea(x, y), then seg8:0x7ba8 on the screen point [BYTES seg12:0x6a7d-0x6ac0]
    if (d.to_screen) {
        const ScreenPoint p = d.to_screen(o.x, o.y);
        if (in_play_area(p.x, p.y, w.W, w.H) && d.erase) d.erase(o.scr_x, o.scr_y, o.size, o.size);
    }
    // Pool 6 (a WILDCARD fruit): the missile entry's age = W(its weapon)+0x1e, then its weapon
    // -1, the weapon read before the store (work/analysis/m4/s7-fuel.md section 5, verified
    // claim 43; the words are collide.h weapon_word's, the program's table) [BYTES
    // seg12:0x6ac5-0x6adc] (M4 s7 FUEL).
    if (pool == 6) {
        MissileSlot& e = w.pools.missile(rec);
        e.age = weapon_word(e.weapon, 0x1e);
        e.weapon = -1;
    }
    for (int k = 0; k < 5; ++k)  // [BYTES seg12:0x6ae2-0x6afe]
        if (d.puff) d.puff(pool, rec, 1);
    o.state = 1;  // [BYTES seg12:0x6b0e]
}

// seg12:0x129e [BYTES seg12:0x129e-0x1774]
std::int16_t pod_dissolve(World& w, int pool, int rec, const DissolveOut& d,
                          const std::function<std::int16_t(int w, int off)>& weapon_word) {
    if (pool < 0 || pool >= kPoolCount || rec < 0 || rec >= w.pools.records(pool)) return 0;  // [DESIGN]
    ObjectRecord& o = w.pools.rec(pool, rec);
    const std::int16_t sz = o.size, g = o.gap;  // [bp-6], [bp-8] [BYTES seg12:0x12ad-0x12c8]
    const std::int16_t step = w.w8ffe;
    const bool big = step != 0 && static_cast<std::int8_t>(o.race_status) > 8;  // [BYTES seg12:0x12cb-0x12e0]
    o.race_status = static_cast<std::uint8_t>(o.race_status - static_cast<std::uint8_t>(step & 0xff));
    if (static_cast<std::int8_t>(o.race_status) < 0) o.race_status = 0;  // [BYTES seg12:0x12e5-0x1309]
    std::int16_t p = sub16(9, static_cast<std::int8_t>(o.race_status));  // [bp-4] [BYTES seg12:0x131c-0x1326]
    const std::int16_t phase = p;
    if (d.out) d.out->set_stretch_mode(d.window_dc, kColorOnColor);  // [BYTES seg12:0x1329-0x1331]
    // The slot [bp-0xa] and the x offset [bp-0xc] [BYTES seg12:0x1336-0x1375]; another pool leaves
    // both unset in the original: the port takes 0 [DESIGN ADR 0016 decision 1].
    std::int16_t slot = 0, xoff = 0;
    if (pool == 11) {
        slot = o.w4a;
    } else if (pool == 6) {
        const std::int16_t wpn = w.pools.missile(rec).weapon;  // DGROUP:0x66e4 + 0x1c·rec
        slot = weapon_word ? weapon_word(wpn, -4) : std::int16_t{0};          // DGROUP:0x2f50 + 0x50·W
        std::int16_t k = sub16(weapon_word ? weapon_word(wpn, 0x26) : std::int16_t{0}, 1);  // DGROUP:0x2f7a
        if (k < 0) k = 0;
        xoff = mul16(k, add16(g, sz));
    }
    if (slot > 100 || slot < 0) {  // [BYTES seg12:0x1378-0x1395] (verified claim 56)
        w.pools.bad_index_count = add16(w.pools.bad_index_count, 1);  // DGROUP:0x9498
        w.pools.missile(rec).weapon = 0x10;  // DGROUP:0x66e4 + 0x1c·rec, for a pod too
        // DGROUP:0x3450 = W(16)-4 = DGROUP:0x2f54 + 0x50·16 - 4, the image slot word of weapon 16,
        // the weapon just written: -1 from seg10:0x1fb2 until seg10:0x533a assigns a slot
        // [DESIGN ADR 0016 decision 3] [BYTES seg12:0x1392 seg10:0x2da8].
        slot = weapon_word ? weapon_word(0x10, -4) : std::int16_t{0};
    }
    if (o.blast_size == w.player && w.detail > 1 && (p & 1) != 0 && d.player_sprite)  // [BYTES seg12:0x1398-0x13bc]
        d.player_sprite();
    if (o.on_screen == 0) return phase;  // [BYTES seg12:0x13c1-0x13d6]
    const std::int16_t sx = o.scr_x, sy = o.scr_y;
    const DcId src = d.images && slot >= 0 && slot < kImageSlots ? (*d.images)[static_cast<std::size_t>(slot)] : kDcNone;
    const DcId dc = d.window_dc;
    const std::int16_t S = w.size & 15;
    const std::int16_t u2 = u(w, 2), u4 = u(w, 4), u6 = u(w, 6), u8 = u(w, 8), u16 = u(w, 16);
    switch (static_cast<std::uint16_t>(p)) {  // [BYTES seg12:0x166b-0x1673]
    case 0:  // [BYTES seg12:0x13df-0x1436]
        if (d.out)
            d.out->stretch_blt(dc, sx, sy, sz, sz, src, sub16(add16(xoff, g), u2), sub16(g, u2), add16(sz, u4),
                               add16(sz, u4), kSrcCopy);
        break;
    case 1:
    case 2:
    case 3:
    case 4: {  // [BYTES seg12:0x143e-0x1532]
        const auto q = static_cast<std::int16_t>(shl16(p, S) & 0xfffe);
        if (d.out) {
            d.out->stretch_blt(dc, add16(q, sx), sub16(sy, u2), sub16(sz, mul16(u4, p)), add16(u4, sz), src,
                               sub16(add16(xoff, g), u4), sub16(g, 2), add16(u8, sz), add16(u4, sz), kSrcCopy);
            if (step != 0) {
                d.out->pat_blt(dc, add16(mul16(u2, sub16(p, 1)), sx), sy, u2, sz, kBlackness);
                d.out->pat_blt(dc, add16(sub16(sx, q), sz), sy, u2, sz, kBlackness);
            }
        }
        break;
    }
    case 5:
    case 6:
    case 7:
    case 8: {  // [BYTES seg12:0x1535-0x162a]
        p = sub16(p, 4);
        if (d.out) {
            d.out->stretch_blt(dc, add16(u8, sx), sub16(add16(mul16(u2, p), sy), 1), sub16(sz, u16),
                               add16(sub16(u2, mul16(u4, p)), sz), src, sub16(add16(xoff, g), u4), sub16(g, u4),
                               add16(u8, sz), add16(u8, sz), kSrcCopy);
            if (step != 0) {
                d.out->pat_blt(dc, add16(u6, sx), sy, u2, sz, kBlackness);
                d.out->pat_blt(dc, add16(add16(sub16(sx, u16), u8), sz), sy, u2, sz, kBlackness);
            }
        }
        break;
    }
    case 9:  // [BYTES seg12:0x162d-0x1669]
        if (d.out) d.out->pat_blt(dc, sx, sy, sz, sz, kBlackness);
        if (d.in_use) d.in_use(pool, -1);
        if (d.player_sprite) d.player_sprite();
        break;
    default:
        break;
    }
    if (big && d.out) {  // [BYTES seg12:0x168c-0x1767]: the bottom strip at y + size - 3 (a literal 3)
        const std::int16_t u3 = u(w, 3);
        d.out->pat_blt(dc, sx, sy, sz, u3, kBlackness);
        d.out->pat_blt(dc, sx, sub16(add16(sy, sz), 3), sz, u3, kBlackness);
        d.out->pat_blt(dc, sx, add16(u3, sy), u3, sub16(sz, u6), kBlackness);
        d.out->pat_blt(dc, add16(sub16(sx, u3), sz), add16(u3, sy), u3, sub16(sz, u6), kBlackness);
    }
    return phase;
}

// seg10:0x42ee [BYTES seg10:0x42ee-0x440a]
void build_pod_image(Blitter& out, ImageSlotTable& images, int size_level) {
    DcId& dc = images[kSlotPod];
    if (dc != kDcNone) return;  // [BYTES seg10:0x42fb-0x4300]
    dc = out.create_image(0x59, 0x21);  // [BYTES seg10:0x434b-0x4365]
    if (dc == kDcNone) return;
    out.pat_blt(dc, 0, 0, 0x59, 0x21, kBlackness);  // [BYTES seg10:0x4368-0x4378]
    for (int k = 0; k < 3; ++k) {  // [BYTES seg10:0x437d-0x43e2]
        const DcId gr = out.load_graphic(0x2b2 + k, 3, -1);  // seg10:0x6c02(690 + k, 3, -1)
        if (gr == kDcNone) continue;
        out.bit_blt(dc, 0x1c * k + 5, 5, 0x17, 0x17, gr, 0, 0, kSrcCopy);
        out.delete_image(gr);
    }
    // seg10:0x6308(0x1d, -1, -1, -1) [BYTES seg10:0x43fe-0x440a] (M4 s8 SIZE-SITES).
    std::int16_t dg_740 = 0;  // slot 0x1d never sets it
    rescale_image(out, images, kSlotPod, kDcNone, -1, size_level, dg_740);
}

}  // namespace is::world
