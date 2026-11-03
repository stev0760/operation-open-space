// Pool 4, the fuel tank (tank.h).
#include "world/tank.h"

#include "world/capture.h"
#include "world/update.h"

namespace is::world {

namespace {

// u(n) = (n << Size) >> 1 with `sar` (s3-fuel.md Conventions).
std::int16_t u(const World& w, std::int16_t n) { return sar16(shl16(n, w.size & 15), 1); }

DcId slot_dc(const DissolveOut& d, int slot) {
    if (!d.images || slot < 0 || slot >= kImageSlots) return kDcNone;
    return (*d.images)[static_cast<std::size_t>(slot)];
}

// The frame of width u(3) inside the square [BYTES seg12:0x11b6-0x128f].
void inner_frame(const DissolveOut& d, const World& w, std::int16_t sx, std::int16_t sy, std::int16_t sz) {
    if (!d.out) return;
    const std::int16_t u3 = u(w, 3), u6 = u(w, 6);
    d.out->pat_blt(d.window_dc, sx, sy, sz, u3, kBlackness);
    d.out->pat_blt(d.window_dc, sx, add16(sub16(sy, u3), sz), sz, u3, kBlackness);
    d.out->pat_blt(d.window_dc, sx, add16(sy, u3), u3, sub16(sz, u6), kBlackness);
    d.out->pat_blt(d.window_dc, sub16(add16(sx, sz), u3), add16(sy, u3), u3, sub16(sz, u6), kBlackness);
}

}  // namespace

// seg12:0xdc4 [BYTES seg12:0xdc4-0xe5a]
std::int16_t tank_damage(CollideCtx& x, int pool, int rec, std::int16_t amount) {
    ObjectRecord& o = obj(x.w, pool, rec);
    o.damage = add16(o.damage, amount);  // [BYTES seg12:0xdd0-0xde1]
    if (o.flame == 0) o.flame = 1;       // the wobble starts [BYTES seg12:0xded-0xdf4]
    // max(1, damage / 10), signed `idiv`; only the low byte reaches +0x50 [BYTES seg12:0xe09-0xe1f]
    std::int16_t b = idiv16(o.damage, 10);
    if (b < 1) b = 1;
    o.b50 = static_cast<std::uint8_t>(b & 0xff);
    if (static_cast<std::int8_t>(o.b50) > 5) {  // signed byte [BYTES seg12:0xe29-0xe2e]
        run_on_destroy(x, pool, rec);           // the far call through +0x66 [BYTES seg12:0xe30-0xe3a]
        x.h.seg7_2080(pool, rec);               // [BYTES seg12:0xe3d-0xe48]
        return 1;
    }
    return 0;
}

// seg12:0xae4 [BYTES seg12:0xae4-0xc5c]
void move_tanks(UpdateCtx& c, World& w) {
    MoverScope scope(c.host, c);
    Pools& pools = w.pools;
    UpdateHost& h = c.host;
    const int n = pools.in_use(kPool4) < pools.records(kPool4) ? pools.in_use(kPool4) : pools.records(kPool4);
    if (pools.records(kPool4) == 0) return;
    // Record 0's size and gap for every cell [BYTES seg12:0xaf3-0xb02].
    const std::int16_t sz0 = pools.rec(kPool4, 0).size, g0 = pools.rec(kPool4, 0).gap;
    for (int k = 0; k < n; ++k) pools.rec(kPool4, k).w1c = 0;  // [BYTES seg12:0xb05-0xb23]
    for (int i = 0; i < n; ++i) {
        ObjectRecord& o = pools.rec(kPool4, i);
        const std::int16_t pr = pool_rec(kPool4, i);
        if (o.state != 0) {  // [BYTES seg12:0xc6b seg12:0xc38-0xc4b]
            if (o.race_status != 0) h.tank_dissolve(i);
            continue;
        }
        if (o.b54 != 0) {  // `fixed` [BYTES seg12:0xc72-0xc89]
            o.vy = 0;
            o.vx = 0;
        }
        if (o.on_screen == 0) {  // [BYTES seg12:0xc95-0xc9c seg12:0xb2d-0xb4e]
            if (imod16(c.f.dg_9142, 3) == 0) {
                limit_speed(pools, 0x28, pr);   // seg8:0x1d54(h, 0x28)
                integrate(c.flight, 3, pr);     // seg8:0x1b54(h, 3)
            }
        } else {
            limit_speed(pools, 0x28, pr);  // [BYTES seg12:0xc9f-0xcb3]
            integrate(c.flight, 1, pr);
            // The cell (+0x26 / 5)·(size0 + gap0) + gap0, 16-bit [BYTES seg12:0xcc2-0xcdd]
            o.src_x = add16(mul16(idiv16(o.angle, 5), add16(sz0, g0)), g0);
            h.draw_sprite(pr, 1, kSlotTank, 0);  // seg8:0xb76(h, 1, DGROUP:0x39f0, 0) [BYTES seg12:0xce1-0xcec]
            if (w.w9214 == 0) {  // no special time of day [BYTES seg12:0xcf1-0xcf8]
                o.angle = add16(o.angle, static_cast<std::int8_t>(o.flame));  // [BYTES seg12:0xd07-0xd0c]
                if (o.flame != 0) {  // [BYTES seg12:0xd16-0xd1d]
                    if (o.angle < 5) {  // [BYTES seg12:0xd20-0xd37]
                        o.angle = 5;
                        o.flame = o.b50;
                    }
                    if (o.angle > 21) {  // 8-bit neg [BYTES seg12:0xd43-0xd5e]
                        o.angle = 21;
                        o.flame = static_cast<std::uint8_t>(0u - o.b50);
                    }
                    // The volatile roll: two rand() draws, the second only when the first test
                    // passes (s3-fuel.md 3.3) [BYTES seg12:0xd62-0xd97].
                    const auto b = static_cast<std::int8_t>(o.b50);
                    if (b > imod16(static_cast<std::int16_t>(c.flight.rng.rand()), 120) &&
                        b > imod16(static_cast<std::int16_t>(c.flight.rng.rand()), 20)) {
                        h.seg12_274a(kPool4, i);            // [BYTES seg12:0xd9c-0xda5]
                        h.collide_host().seg7_2080(kPool4, i);  // [BYTES seg12:0xda8-0xdb2]
                    }
                }
            }
        }
        // seg12:0xb53: an on-screen tank not `collided` (verified claims 24, 27-29) [BYTES
        // seg12:0xb5f-0xb6b].
        if (o.on_screen == 0 || o.w1c != 0) continue;
        c.f.dg_943a = kPool4;
        c.f.dg_943c = static_cast<std::int16_t>(i);
        const std::uint16_t hit = h.collide(pr);  // seg8:0x50aa [BYTES seg12:0xb7e-0xb87]
        if (hit == 0xffffu) continue;
        const int hp = hit >> 8, hr = hit & 0xff;
        CollideCtx x = frame_collide_ctx(c, w);
        if (hp == c.flight.g.ship_pool) {
            const ShipRecord& S = ship_at(w, hr);
            if (S.bea == 0 && S.be8 == 0) {
                // seg13:0x16ac(hr, (DGROUP:0x943a << 8) | cbw(byte DGROUP:0x943c)) [BYTES
                // seg12:0xb8c-0xbbf] (verified claim 28).
                const auto arg = static_cast<std::int16_t>(
                    (static_cast<std::uint16_t>(c.f.dg_943a) << 8) |
                    static_cast<std::uint16_t>(static_cast<std::int16_t>(static_cast<std::int8_t>(c.f.dg_943c & 0xff))));
                CaptureCtx cc{x, c.flight, h.capture_host()};
                touch(cc, hr, arg);
                continue;
            }
        }
        std::int16_t a;
        const bool icon_hot = hp == 0 && hr < kIconRecords && w.icons[static_cast<std::size_t>(hr)].w02 != 0;
        if (icon_hot || hp == c.flight.g.pool5) {
            a = 125;  // [BYTES seg12:0xbc5-0xbe7]
        } else {
            // seg8:0x546c(hit, &vx, &vy, extent + 2, mass) [BYTES seg12:0xbe9-0xc19]
            a = respond(x, o.mass, add16(o.extent, 2), &o.vy, &o.vx, hit);
        }
        dispatch(x, kPool4, i, hp, hr, a);  // seg8:0x5df2(4, i, hp, hr, a) [BYTES seg12:0xc1f-0xc33]
    }
}

// seg12:0xe5c [BYTES seg12:0xe5c-0x129c]
std::int16_t tank_dissolve(World& w, int i, const DissolveOut& d) {
    if (i < 0 || i >= w.pools.records(kPool4)) return 0;  // [DESIGN] bounds
    ObjectRecord& o = w.pools.rec(kPool4, i);
    const std::int16_t sz = o.size, g = o.gap;  // record i's own [BYTES seg12:0xe6b-0xe82]
    const std::int16_t step = w.w8ffe;
    const bool big = step != 0 && static_cast<std::int8_t>(o.race_status) > 8;  // [BYTES seg12:0xe85-0xe9a]
    o.race_status = static_cast<std::uint8_t>(o.race_status - static_cast<std::uint8_t>(step & 0xff));
    if (static_cast<std::int8_t>(o.race_status) < 0) o.race_status = 0;  // [BYTES seg12:0xe9f-0xeb9]
    std::int16_t p = sub16(9, static_cast<std::int8_t>(o.race_status));  // [BYTES seg12:0xebe-0xed0]
    const std::int16_t phase = p;
    if (d.out) d.out->set_stretch_mode(d.window_dc, kColorOnColor);  // [BYTES seg12:0xed3-0xed9]
    // The player's sprite on odd phases: +0x48 is the ship the touch wrote (seg13:0x1c7e) [BYTES
    // seg12:0xede-0xef8 seg13:0x1c7e].
    if (o.blast_size == w.player && w.detail > 1 && (p & 1) != 0 && d.player_sprite) d.player_sprite();
    const std::int16_t x0 =
        ((w.w9214 == 3 || w.w9214 == 4) && o.angle != 0) ? add16(g, sz) : std::int16_t{0};  // [BYTES seg12:0xefd-0xf25]
    if (o.on_screen == 0) return phase;  // no tail, no end [BYTES seg12:0xf2a-0xf39]
    const std::int16_t sx = o.scr_x, sy = o.scr_y;
    const DcId src = slot_dc(d, kSlotTank);
    const DcId dc = d.window_dc;
    const std::int16_t S = w.size & 15;
    const std::int16_t side2 = add16(shl16(g, 1), sz);  // 2·gap + size
    switch (static_cast<std::uint16_t>(p)) {  // [BYTES seg12:0x118c-0x1194]
    case 0:  // [BYTES seg12:0xf42-0xf8e]
        if (d.out)
            d.out->stretch_blt(dc, sx, sy, sz, sz, src, sub16(add16(x0, g), u(w, 3)), sub16(g, u(w, 3)),
                               add16(sz, u(w, 6)), add16(sz, u(w, 6)), kSrcCopy);
        break;
    case 1:
    case 2:
    case 3:
    case 4: {  // [BYTES seg12:0xf96-0x1078]
        const auto q = static_cast<std::int16_t>(shl16(p, S) & 0xfffe);
        const auto ww = sub16(sz, static_cast<std::int16_t>(shl16(p, S + 1) & 0xfffc));
        if (d.out) {
            d.out->stretch_blt(dc, add16(sx, q), sub16(sy, u(w, 2)), ww, add16(sz, u(w, 3)), src, x0, 0, side2,
                               side2, kSrcCopy);
            if (step != 0) {
                d.out->pat_blt(dc, add16(sub16(sx, u(w, 2)), q), sy, u(w, 2), sz, kBlackness);
                d.out->pat_blt(dc, add16(sub16(sx, q), sz), sy, u(w, 2), sz, kBlackness);
            }
        }
        break;
    }
    case 5:
    case 6:
    case 7:
    case 8: {  // [BYTES seg12:0x1080-0x1152]
        p = sub16(p, 4);
        if (d.out) {
            d.out->stretch_blt(dc, add16(sx, u(w, 8)), sub16(add16(sy, mul16(u(w, 2), p)), 1), sub16(sz, u(w, 16)),
                               add16(sub16(sz, mul16(u(w, 4), p)), 1), src, x0, 0, side2, side2, kSrcCopy);
            if (step != 0) {
                d.out->pat_blt(dc, add16(sx, u(w, 6)), sy, u(w, 2), sz, kBlackness);
                d.out->pat_blt(dc, add16(sub16(add16(sx, sz), u(w, 16)), u(w, 8)), sy, u(w, 2), sz, kBlackness);
            }
        }
        break;
    }
    case 9:  // [BYTES seg12:0x1155-0x1185]
        if (d.out) d.out->pat_blt(dc, sx, sy, sz, sz, kBlackness);
        if (d.in_use) d.in_use(kPool4, -1);
        if (d.player_sprite) d.player_sprite();
        break;
    default:
        break;  // p > 9: the tail only
    }
    if (big) inner_frame(d, w, sx, sy, sz);  // [BYTES seg12:0x11ad-0x128f]
    return phase;
}

// seg10:0x565a [BYTES seg10:0x565a-0x58cd]
void build_tank_image(Blitter& out, ImageSlotTable& images, std::int16_t w9214, std::int16_t& made_for) {
    DcId& dc = images[kSlotTank];
    if (dc != kDcNone) {  // [BYTES seg10:0x5667-0x5682]
        if (made_for == w9214) return;
        out.delete_image(dc);  // seg10:0x552(4, 1)
        dc = kDcNone;
    }
    made_for = w9214;  // DGROUP:0x17c0 [BYTES seg10:0x568a-0x568d]
    int id = 0x10e;
    std::int16_t s = 0x24, g = 6;
    if (w9214 != 0) {  // [BYTES seg10:0x56a5-0x56e3]
        s = 0x1e;
        g = 4;
        if (w9214 == 1) id = 0x110;
        else if (w9214 == 2) id = 0x112;
        else return;  // 3 (0x116) and 4 (0x48): two graphics and another layout [OPEN]
    }
    const int width = 5 * s + 6 * g, height = 2 * g + s;  // [BYTES seg10:0x58a5-0x58b8]
    dc = out.create_image(width, height);
    if (dc == kDcNone) return;
    out.pat_blt(dc, 0, 0, width, height, kBlackness);  // [BYTES seg10:0x575d-0x577d]
    const DcId gr = out.load_graphic(id, 0, -1);        // seg10:0x6c02(id, 0, -1) [BYTES seg10:0x5846-0x5856]
    if (gr == kDcNone) return;
    for (int k = 0; k < 5; ++k)  // [BYTES seg10:0x5782-0x57c2]
        out.bit_blt(dc, (g + s) * k + g, g, s, s, gr, s * k, 0, kSrcCopy);
    out.delete_image(gr);  // [BYTES seg10:0x580e-0x5836]
    // seg10:0x6308(4, -1, -1, -1), the rescale at Size != 1 [OPEN] [BYTES seg10:0x58bb-0x58c4].
}

}  // namespace is::world
