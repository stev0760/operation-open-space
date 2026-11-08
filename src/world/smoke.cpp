// The smoke (smoke.h). Each step cites the original's addresses; the arithmetic is 16-bit where
// the original's is (ADR 0013 decision 7).
#include "world/smoke.h"

#include "world/flight.h"
#include "world/shots.h"

namespace is::world {

namespace {

constexpr std::size_t ix(int k) { return static_cast<std::size_t>(k); }

Point16 screen_of(const SmokeCtx& c, std::int16_t x, std::int16_t y) {
    return c.to_screen ? c.to_screen(x, y) : Point16{};
}

bool record_ok(const Pools& p, int pool, int rec) {
    return pool >= 0 && pool < kPoolCount && rec >= 0 && rec < p.records(pool);
}

// The heading table DGROUP:0x1050 + 4h; a heading outside 0 ... 15 reads past the table in the
// original: the port takes (0, 0) [DESIGN].
std::int16_t head_x(int h) { return h >= 0 && h < 16 ? heading_dx(h) : std::int16_t{0}; }
std::int16_t head_y(int h) { return h >= 0 && h < 16 ? heading_dy(h) : std::int16_t{0}; }

// `cwd; sub ax, dx; sar ax, 1` and `cwd; and dx, 3; add ax, dx; sar ax, 2`: halves and quarters
// toward zero [BYTES seg7:0x5c26-0x5c29 seg7:0x5cd9-0x5cdf].
std::int16_t half(std::int16_t v) { return idiv16(v, 2); }
std::int16_t quarter(std::int16_t v) { return idiv16(v, 4); }

// T at [bp-0x1c] ... [bp-0x12]: dword 0x120006 at [bp-0x18], 0x3c0024 at [bp-0x14], words 0 at
// [bp-0x1c] and [bp-0x1a] [BYTES seg7:0x5e13-0x5e28].
constexpr std::int16_t kBandOffset[6] = {0, 0, 6, 0x12, 0x24, 0x3c};

}  // namespace

// seg10:0x5202 [BYTES seg10:0x5210-0x532b]
void build_smoke_image(Blitter& out, ImageSlotTable& images) {
    DcId& slot = images[ix(kSlotSmoke)];
    if (slot != kDcNone) return;  // the chain's DGROUP:0x3a00 = 0 test [BYTES seg9:0x265c]
    // seg10:0x6c02(200, 0, -1) into a CreateCompatibleDC [BYTES seg10:0x5210-0x5236].
    const DcId src = out.load_graphic(kGraphicSmoke, 0, -1);
    // CreateCompatibleBitmap(DGROUP:0x13dc, 270, 22) in DC DGROUP:0x3a00 [BYTES seg10:0x5256-0x5282].
    slot = out.create_image(kSmokeStripW, kSmokeStripH);
    out.pat_blt(slot, 0, 0, kSmokeStripW, kSmokeStripH, kBlackness);  // [BYTES seg10:0x529e-0x52b0]
    std::int16_t y = 0;  // [bp-0xa] [BYTES seg10:0x5251]
    for (int k = 0; k < kSmokeBands; ++k) {  // `jle 5` [BYTES seg10:0x52b5-0x52f4]
        const std::int16_t w = mul16(static_cast<std::int16_t>(k + 5), 6);  // 6(k + 5)
        const std::int16_t h = idiv16(w, 3);                                 // 2(k + 5)
        out.bit_blt(slot, mul16(y, 3), 0, w, h, src, 0, y, kSrcCopy);        // [BYTES seg10:0x52e5]
        y = add16(y, h);                                                     // [BYTES seg10:0x52ea]
    }
    out.delete_image(src);  // SelectObject back, DeleteObject, DeleteDC [BYTES seg10:0x52f6-0x531d]
}

void smoke_place(SmokeRecords& t) {
    for (SmokeRecord& s : t) s.counter = -1;  // [BYTES seg9:0x6750]
}

// seg7:0x5d6a [BYTES seg7:0x5d77-0x5df9]
int smoke_start(SmokeRecords& t, std::int16_t& dg_9456, std::int16_t detail, shell::Rng& rng,
                std::int16_t x, std::int16_t y, std::int16_t arg, std::int16_t vx, std::int16_t vy) {
    (void)arg;  // [bp+0xa] is never read (A3)
    std::int16_t threshold = 0;
    switch (detail) {
    case 0: threshold = 0x14; break;
    case 1: threshold = 0x2d; break;
    case 2: threshold = 0x46; break;
    case 3: threshold = 0x64; break;
    default: threshold = 0; break;  // unset in the original [DESIGN ADR 0016 decision 1]
    }
    if (detail != 3) {  // `jge` takes the record [BYTES seg7:0x5d8e-0x5da5]
        if (imod16(static_cast<std::int16_t>(rng.rand()), 100) < threshold) return -1;
    }
    for (int k = 0; k < kSmokeRecords; ++k) {  // [BYTES seg7:0x5dbc-0x5dd5]
        SmokeRecord& s = t[ix(k)];
        if (s.counter != -1) continue;
        s.counter = 0;  // [BYTES seg7:0x5dd7]
        s.x = x;        // [BYTES seg7:0x5de0 seg7:0x5de7]
        s.y = y;
        s.vx = vx;      // [BYTES seg7:0x5dee seg7:0x5df5]
        s.vy = vy;
        dg_9456 = add16(dg_9456, 1);  // [BYTES seg7:0x5df9]
        return k;
    }
    return -1;
}

// seg7:0x5b68 [BYTES seg7:0x5b68-0x5d68]
int smoke_puff(SmokeCtx& c, int pool, int rec, int arg) {
    // The threshold [bp-0xc] by Detail [BYTES seg7:0x5b77-0x5bbb]; another Detail leaves it unset.
    std::int16_t threshold = 100;  // [DESIGN ADR 0016 decision 1] takes the record
    switch (c.detail) {
    case 0: threshold = 0x19; break;
    case 1: threshold = 0x2d; break;
    case 2: threshold = 0x41; break;
    case 3: threshold = 0x5a; break;
    default: break;
    }
    if (c.detail != 3) {  // [BYTES seg7:0x5b8e-0x5ba5]: `jle` proceeds
        if (imod16(static_cast<std::int16_t>(c.rng.rand()), 100) > threshold) return -1;
    }
    int k = 0;
    for (; k < kSmokeRecords; ++k)  // [BYTES seg7:0x5bbd seg7:0x5c80-0x5c91]
        if (c.table[ix(k)].counter == -1) break;
    if (k == kSmokeRecords) return -1;  // [BYTES seg7:0x5c84]
    SmokeRecord& s = c.table[ix(k)];
    s.counter = 0;  // [BYTES seg7:0x5c93]
    const bool ok = record_ok(c.pools, pool, rec);  // [DESIGN] bounds
    if (pool != c.ship_pool) {  // [BYTES seg7:0x5c99-0x5c9f]
        // x, y, then the heading index [BYTES seg7:0x5bc5-0x5c5c]
        const ObjectRecord* o = ok ? &c.pools.rec(pool, rec) : nullptr;
        const std::int16_t extent = o ? o->extent : std::int16_t{0};
        const std::int16_t r1 = static_cast<std::int16_t>(c.rng.rand());
        s.x = add16(extent != 0 ? imod16(r1, extent) : std::int16_t{0}, o ? o->x : std::int16_t{0});
        const std::int16_t r2 = static_cast<std::int16_t>(c.rng.rand());
        s.y = add16(extent != 0 ? imod16(r2, extent) : std::int16_t{0}, o ? o->y : std::int16_t{0});
        const int d = imod16(static_cast<std::int16_t>(c.rng.rand()), 16);
        s.vx = add16(half(head_x(d)), half(o ? o->vx : std::int16_t{0}));  // [BYTES seg7:0x5c42]
        s.vy = add16(half(head_y(d)), half(o ? o->vy : std::int16_t{0}));  // [BYTES seg7:0x5c5c]
    } else {
        // The ship branch: R = word DGROUP:0x7ba0/0x7ba2 + 4(64n + h), muzzle entry 16 + h of
        // design n = ship word +0x4e (DGROUP:0x70de) [BYTES seg7:0x5ca3-0x5d59].
        const ObjectRecord* o = ok ? &c.pools.rec(pool, rec) : nullptr;
        const int h = o ? o->frame : 0;
        const int n = c.ships && rec >= 0 && rec < kShipCount ? c.ships->at(ix(rec)).design_index : 0;
        const std::int16_t rx = c.shots ? c.shots->mx(n, 16 + h) : std::int16_t{0};
        const std::int16_t ry = c.shots ? c.shots->my(n, 16 + h) : std::int16_t{0};
        s.x = add16(shl16(sub16(sub16(rx, quarter(head_x(h))), 4), 3), o ? o->x : std::int16_t{0});  // [BYTES seg7:0x5cf4]
        s.y = add16(shl16(sub16(sub16(ry, quarter(head_y(h))), 4), 3), o ? o->y : std::int16_t{0});  // [BYTES seg7:0x5d25]
        s.vx = sub16(o ? o->vx : std::int16_t{0}, half(head_x(h)));  // [BYTES seg7:0x5d3f]
        s.vy = sub16(o ? o->vy : std::int16_t{0}, half(head_y(h)));  // [BYTES seg7:0x5d59]
    }
    if (arg != 0 && c.sound) c.sound(1, pool, rec, 1);  // seg10:0x754a(1, pool, rec, 1) [BYTES seg7:0x5c60-0x5c6e]
    c.dg_9456 = add16(c.dg_9456, 1);                    // [BYTES seg7:0x5c76]
    return k;
}

// seg8:0x124a [BYTES seg8:0x1258-0x1280]
void smoke_screen(SmokeCtx& c, int k) {
    if (k < 0 || k >= kSmokeRecords) return;  // [DESIGN] bounds
    SmokeRecord& s = c.table[ix(k)];
    s.old_x = s.sx;  // [BYTES seg8:0x1260 seg8:0x1268]
    s.old_y = s.sy;
    const Point16 p = screen_of(c, s.x, s.y);  // seg8:0x29ea [BYTES seg8:0x1278]
    s.sx = p.x;  // dword [BYTES seg8:0x1280]
    s.sy = p.y;
}

// [BYTES seg7:0x60ac-0x6123]
PuffCell puff_cell(std::int16_t counter, std::int16_t size) {
    PuffCell r;
    const int s = size & 0x1f;  // a 386 masks the shift count to 5 bits [DESIGN] (review L14)
    r.q = idiv16(counter, 9);  // [BYTES seg7:0x60ac-0x60b8]
    r.p = sar16(shl16(add16(r.q, 5), (s + 1) & 0x1f), 1);  // `inc cl` then the masked count [BYTES seg7:0x610e-0x611a]
    r.h = sar16(shl16(r.q, s), 1);                // [bp-8] at each use [BYTES seg7:0x5eb3-0x5ebc]
    const std::int16_t band = mul16(r.q, sar16(shl16(0x1e, s), 1));  // [BYTES seg7:0x60da-0x60e6]
    std::int16_t t = 0;
    if (r.q >= 0 && r.q < 6) {
        t = kBandOffset[r.q];  // [bp + 2q - 0x1c] [BYTES seg7:0x60ea]
    } else {
        r.src_x_defined = false;  // [bp-0x10] and beyond (A1)
    }
    const std::int16_t cell = imod16(idiv16(counter, 3), 3);  // [BYTES seg7:0x6106-0x610c]
    r.src_x = add16(band, add16(sar16(shl16(t, s), 1), mul16(r.p, cell)));  // [BYTES seg7:0x611d-0x6123]
    return r;
}

// seg7:0x5e04 [BYTES seg7:0x5e04-0x6182]
void move_smoke(SmokeCtx& c) {
    const int s = c.size & 0x1f;  // the 386's 5-bit shift count
    for (int k = 0; k < kSmokeRecords; ++k) {  // [BYTES seg7:0x608f-0x6096]
        SmokeRecord& r = c.table[ix(k)];
        // [BYTES seg7:0x609e-0x60aa]: signed `jge 0x37` and `jle -1` go to the end test.
        if (r.counter < kSmokeLife && r.counter > -1) {
            const std::int16_t cc = r.counter;
            const PuffCell pc = puff_cell(cc, c.size);
            // c % 4 > Detail + 2 skips to the count [BYTES seg7:0x60bb-0x60cc].
            if (!(imod16(cc, 4) > add16(c.detail, 2))) {
                smoke_screen(c, k);  // [BYTES seg7:0x612f]
                bool draw = true;    // [bp-0x36] [BYTES seg7:0x6134]
                if (c.detail >= 2 && c.visible) {  // [BYTES seg7:0x6139-0x613e]
                    // SetRect(scr - h, scr - h + p) and IntersectRect with each listed object's
                    // (scr_x, scr_y, + size) [BYTES seg7:0x6142-0x6177 seg7:0x5e31-0x5e92].
                    const Rect16 puff = rect16(r.sx - pc.h, r.sy - pc.h, r.sx - pc.h + pc.p, r.sy - pc.h + pc.p);
                    const int n = c.visible->count < kVisibleMax ? c.visible->count : kVisibleMax;
                    for (int m = 0; m < n; ++m) {
                        const int lp = static_cast<std::int8_t>(c.visible->pool[ix(m)]);  // cwde [BYTES seg7:0x5e45]
                        const int lr = static_cast<std::int8_t>(c.visible->rec[ix(m)]);   // [BYTES seg7:0x5e3d]
                        if (!record_ok(c.pools, lp, lr)) continue;                        // [DESIGN]
                        const ObjectRecord& o = c.pools.rec(lp, lr);
                        if (intersect(puff, rect16(o.scr_x, o.scr_y, o.scr_x + o.size, o.scr_y + o.size))) draw = false;
                    }
                }
                r.x = add16(r.x, r.vx);  // [BYTES seg7:0x5e94-0x5ea4]
                r.y = add16(r.y, r.vy);
                // The previous square with this call's h and p (verified claim 46) [BYTES
                // seg7:0x5ea8-0x5ed8].
                if (r.drawn != 0 && c.out)
                    c.out->pat_blt(kDcWindow, sub16(r.old_x, pc.h), sub16(r.old_y, pc.h), pc.p, pc.p, kBlackness);
                if (c.dg_9130 != 0 && c.unported) c.unported("seg7:0x5ee6");  // [BYTES seg7:0x5edd-0x5f77] [OPEN M6]
                if (draw) {  // [BYTES seg7:0x5f79-0x5fbd]
                    const DcId strip = c.images ? (*c.images)[ix(kSlotSmoke)] : kDcNone;
                    // Counter 54's source x is undefined (A1): no BitBlt, the flag as drawn
                    // [DESIGN ADR 0016 decision 1].
                    if (c.out && pc.src_x_defined)
                        c.out->bit_blt(kDcWindow, sub16(r.sx, pc.h), sub16(r.sy, pc.h), pc.p, pc.p, strip, pc.src_x, 0,
                                       kSrcCopy);
                    r.drawn = 1;
                } else {
                    r.drawn = 0;  // [BYTES seg7:0x5fc9]
                }
                // The player's square grown by p [BYTES seg7:0x5fcf-0x6025].
                if (c.dg_58a && record_ok(c.pools, c.ship_pool, c.player)) {
                    const ObjectRecord& pl = c.pools.rec(c.ship_pool, c.player);
                    const Rect16 grown = rect16(pl.scr_x - pc.p, pl.scr_y - pc.p, pl.scr_x + pl.size + pc.p,
                                               pl.scr_y + pl.size + pc.p);
                    if (pt_in_rect(grown, r.sx, r.sy)) *c.dg_58a = 1;
                }
            }
            r.counter = add16(r.counter, 1);  // [BYTES seg7:0x602f]
        }
        // The end test [BYTES seg7:0x6037-0x608b].
        if (r.counter == kSmokeLife) {
            const std::int16_t o6 = sar16(shl16(6, s), 1), side = sar16(shl16(0x18, s), 1);
            if (c.out) c.out->pat_blt(kDcWindow, sub16(r.sx, o6), sub16(r.sy, o6), side, side, kBlackness);
            r.counter = -1;  // [BYTES seg7:0x6071]
            if (c.dg_9130 != 0 && c.unported) c.unported("seg18:0x5350");  // [BYTES seg7:0x6077-0x6088] [OPEN M6]
            c.dg_9456 = sub16(c.dg_9456, 1);  // [BYTES seg7:0x608b]
        }
    }
}

}  // namespace is::world
