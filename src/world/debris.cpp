// The wing debris (debris.h).
#include "world/debris.h"

#include "world/effects.h"
#include "world/flight.h"

namespace is::world {

namespace {

constexpr std::size_t ix(int k) { return static_cast<std::size_t>(k); }

Point16 screen_of(const DebrisCtx& c, std::int16_t x, std::int16_t y) {
    return c.to_screen ? c.to_screen(x, y) : Point16{};
}

// (-40, -40) ... (W + 40, H + 40), exclusive: `jle -0x28` / `jge W + 0x28` [BYTES seg8:0x9c4-0x9e8].
bool on_screen(const DebrisCtx& c, std::int16_t x, std::int16_t y) {
    return x > -40 && x < add16(c.W, 40) && y > -40 && y < add16(c.H, 40);
}

}  // namespace

std::int16_t debris_cell(std::int16_t size, std::int16_t dg_9130) {
    if (dg_9130 == 0) return sar16(shl16(0x20, size & 15), 1);  // [BYTES seg8:0x8e9-0x8f2]
    return idiv16(add16(shl16(size, 6), 0x20), 3);               // [BYTES seg8:0x8f6-0x903]
}

// seg7:0x5d6a [BYTES seg7:0x5d77-0x5df9]
int smoke_start(SmokeTable& t, std::int16_t& dg_9456, std::int16_t detail, shell::Rng& rng,
                std::int16_t x, std::int16_t y, std::int16_t arg, std::int16_t a, std::int16_t b) {
    (void)arg;  // not stored (verified claim 47)
    std::int16_t threshold = 0;
    switch (detail) {
    case 0: threshold = 0x14; break;
    case 1: threshold = 0x2d; break;
    case 2: threshold = 0x46; break;
    case 3: threshold = 0x64; break;
    default: threshold = 0; break;  // unset in the original [DESIGN]
    }
    if (detail != 3) {  // [BYTES seg7:0x5d8e-0x5da5]
        if (imod16(static_cast<std::int16_t>(rng.rand()), 100) < threshold) return -1;
    }
    for (int i = 0; i < static_cast<int>(t.size()); ++i) {  // [BYTES seg7:0x5dbc-0x5dd5]
        SmokeEntry& e = t[ix(i)];
        if (e.used != -1) continue;
        e.used = 0;
        e.x = x;
        e.y = y;
        e.a = a;
        e.b = b;
        dg_9456 = add16(dg_9456, 1);  // [BYTES seg7:0x5df9]
        return i;
    }
    return -1;
}

// seg8:0x2cd8 [BYTES seg8:0x2f19-0x2f8c]
int debris_start(DebrisTable& t, const ObjectRecord& so) {
    int k = -1;
    for (int i = 0; i < kDebrisCount; ++i)
        if (t[ix(i)].counter == 0) {
            k = i;
            break;
        }
    if (k < 0) return -1;
    DebrisEntry& e = t[ix(k)];
    e.counter = 1;     // dword 0x00460001 [BYTES seg8:0x2f1d]
    e.end = 0x46;
    e.extent = so.extent;  // [BYTES seg8:0x2f3b]
    e.x = so.x;            // dword +0x04 [BYTES seg8:0x2f44]
    e.y = so.y;
    e.sx = -1000;          // 0xfc18 [BYTES seg8:0x2f4c seg8:0x2f50]
    e.sy = -1000;
    e.vx = so.vx;          // dword +0x0c [BYTES seg8:0x2f60]
    e.vy = so.vy;
    const int h = imod16(add16(so.frame, 4), 16);  // [BYTES seg8:0x2f65-0x2f71]
    e.vx = add16(e.vx, idiv16(heading_dx(h < 0 ? h + 16 : h), 2));  // [BYTES seg8:0x2f7f]
    e.vy = add16(e.vy, idiv16(heading_dy(h < 0 ? h + 16 : h), 2));  // [BYTES seg8:0x2f8c]
    return k;
}

// seg8:0x8a6 [BYTES seg8:0x8a6-0xb71]
void draw_debris(DebrisCtx& c, int k) {
    if (k < 0 || k >= kDebrisCount) return;
    DebrisEntry& e = c.table[ix(k)];
    if (e.counter < 2) {  // [BYTES seg8:0x8c0-0x8e0]
        const Point16 p = screen_of(c, e.x, e.y);
        e.sx = p.x;
        e.sy = p.y;
        return;
    }
    const std::int16_t p = debris_cell(c.size, c.dg_9130);
    // The old square against the visibility list [BYTES seg8:0x908-0x9b4].
    const Rect16 old = rect16(e.sx, e.sy, e.sx + p, e.sy + p);
    bool overlap = false;
    if (c.visible) {
        const int n = c.visible->count < kVisibleMax ? c.visible->count : kVisibleMax;
        for (int i = 0; i < n; ++i) {
            const int lp = static_cast<std::int8_t>(c.visible->pool[ix(i)]);  // cbw [BYTES seg8:0x945]
            const int lr = static_cast<std::int8_t>(c.visible->rec[ix(i)]);
            if (lp < 0 || lp >= kPoolCount || lr < 0 || lr >= c.pools.records(lp)) continue;  // [DESIGN]
            const ObjectRecord& o = c.pools.rec(lp, lr);
            if (intersect(old, rect16(o.scr_x, o.scr_y, o.scr_x + o.size, o.scr_y + o.size))) overlap = true;
        }
    }
    // Frame ((counter / 2) % 5)·p [BYTES seg8:0xa8e-0xa9f].
    const std::int16_t src_x = mul16(imod16(idiv16(e.counter, 2), 5), p);
    if (overlap) {  // [BYTES seg8:0x9c0-0xaab]
        if (on_screen(c, e.sx, e.sy) && c.out)
            c.out->bit_blt(kDcWindow, e.sx, e.sy, p, p, e.mask, 0, 0, kSrcAnd);
        const Point16 np = screen_of(c, e.x, e.y);
        e.sx = np.x;
        e.sy = np.y;
        if (!on_screen(c, e.sx, e.sy) || !c.out) return;
        c.out->bit_blt(kDcWindow, e.sx, e.sy, p, p, e.mask, 0, 0, kSrcAnd);
        c.out->bit_blt(kDcWindow, e.sx, e.sy, p, p, e.dc, src_x, 0, kSrcPaint);
    } else {  // [BYTES seg8:0xaae-0xb64]
        if (on_screen(c, e.sx, e.sy) && c.out) c.out->pat_blt(kDcWindow, e.sx, e.sy, p, p, kBlackness);
        const Point16 np = screen_of(c, e.x, e.y);
        e.sx = np.x;
        e.sy = np.y;
        if (!on_screen(c, e.sx, e.sy) || !c.out) return;
        c.out->bit_blt(kDcWindow, e.sx, e.sy, p, p, e.dc, src_x, 0, kSrcCopy);
    }
}

// seg13:0x447a [BYTES seg13:0x447a-0x470f]
void move_debris(DebrisCtx& c) {
    for (int k = 0; k < kDebrisCount; ++k) {  // [BYTES seg13:0x4631-0x4642]
        if (c.table[ix(k)].counter <= 0) continue;
        draw_debris(c, k);  // [BYTES seg13:0x4649]
        DebrisEntry& e = c.table[ix(k)];
        e.x = add16(e.x, e.vx);  // [BYTES seg13:0x464e-0x465a]
        e.y = add16(e.y, e.vy);
        e.vx = sub16(e.vx, sar16(e.vx, 4));  // [BYTES seg13:0x465e-0x4670]
        e.vy = sub16(e.vy, sar16(e.vy, 4));
        if (e.counter == 1) {
            // The flames into the strip, then the player's sprite [BYTES seg13:0x467d-0x4554];
            // the rescale seg10:0x6308 at Size != 1 is not ported [OPEN].
            if (c.out) {
                const std::int16_t p = debris_cell(c.size, c.dg_9130);
                const DcId g = c.out->load_graphic(kGraphicFlames, 0, -1);
                const DcId gm = c.out->create_mask(g);
                const std::int16_t w5 = mul16(p, 5);
                c.out->bit_blt(e.dc, 0, 0, w5, p, gm, 0, 0, kSrcAnd);      // [BYTES seg13:0x44c7]
                c.out->bit_blt(e.dc, 0, 0, w5, p, g, 0, 0, kSrcPaint);     // [BYTES seg13:0x44e5]
                c.out->bit_blt(e.dc, 0, 0, w5, p, e.mask, 0, 0, kDsna);    // [BYTES seg13:0x4503]
                if (c.player_sprite) c.player_sprite();                     // seg7:0x1208 [BYTES seg13:0x4508]
                c.out->delete_image(g);  // [BYTES seg13:0x4534-0x454f]
                c.out->delete_image(gm);
            } else if (c.player_sprite) {
                c.player_sprite();
            }
        } else if (e.counter == 0x32 || e.counter == 0x38 || e.counter == 0x3d || e.counter == 0x3f ||
                   e.counter == 0x41 || e.counter == 0x42) {
            // [BYTES seg13:0x4556-0x45c0]: r1 is drawn first and is the 5th argument.
            const std::int16_t px = add16(e.x, 0x80), py = add16(e.y, 0x80);
            const std::int16_t r1 = sub16(imod16(static_cast<std::int16_t>(c.rng.rand()), 60), 30);
            const std::int16_t r2 = sub16(imod16(static_cast<std::int16_t>(c.rng.rand()), 60), 30);
            smoke_start(c.smoke, c.dg_9456, c.detail, c.rng, px, py, 1, r2, r1);
        }
        e.counter = add16(e.counter, 1);  // [BYTES seg13:0x45cc]
        if (e.counter == e.end) {         // [BYTES seg13:0x45d0-0x45d8]
            const std::int16_t p = debris_cell(c.size, c.dg_9130);
            e.counter = 0;  // [BYTES seg13:0x4604]
            if (c.out) {
                c.out->pat_blt(kDcWindow, e.sx, e.sy, p, p, kBlackness);  // [BYTES seg13:0x4620]
                // seg10:0x37d0(k): the entry's DCs released [BYTES seg13:0x4628] [OPEN its other work].
                if (e.dc != kDcNone) c.out->delete_image(e.dc);
                if (e.mask != kDcNone) c.out->delete_image(e.mask);
            }
            e.dc = kDcNone;
            e.mask = kDcNone;
        }
    }
}

}  // namespace is::world
