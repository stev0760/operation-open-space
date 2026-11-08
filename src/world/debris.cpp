// The wing debris (debris.h).
#include "world/debris.h"

#include "world/design_words.h"
#include "world/effects.h"
#include "world/flight.h"
#include "world/player_sheet.h"
#include "world/world.h"

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

// ---- the debris over the torn wing (M4 s5 SHEET)

// DGROUP:0x66e: 128 sin and -128 cos of 22.5 degree steps, (x, y) per heading [BYTES DGROUP:0x66e].
const std::array<std::int16_t, 32> kDebrisT128 = {
    0,    -128, 48,   -110, 91,   -91,  110,  -48, 128,  0,   110,  48,   91,   91,   48,   110,
    0,    128,  -48,  110,  -91,  91,   -110, 48,  -128, 0,   -110, -48,  -91,  -91,  -48,  -110};

std::int16_t debris_reach(std::int16_t D, std::int16_t extent) {
    const std::int16_t a = shl16(idiv16(D, 2), 3);          // cwd; sub; sar 1; shl 3 [BYTES seg8:0x3414-0x341b]
    const std::int16_t e = sub16(extent, shl16(D, 3));     // [BYTES seg8:0x342f-0x3436]
    return add16(a, idiv16(e, 4));                          // cdq; and dx, 3; add; sar 2 [BYTES seg8:0x2f0b-0x2f16]
}

namespace {

// imul R; cwd (of the low word); and dx, 0x7f; add ax, dx; sar ax, 7 [BYTES seg8:0x2fb5-0x2fbe]
std::int16_t scale128(std::int16_t t, std::int16_t R) { return idiv16(mul16(t, R), 128); }

}  // namespace

DebrisWindow debris_place(DebrisEntry& e, std::int16_t heading, std::int16_t D, bool cut) {
    DebrisWindow win;
    const std::int16_t E = e.extent;                       // entry +0x1e [BYTES seg8:0x2f96]
    win.R = debris_reach(D, E);
    const std::int16_t half = idiv16(E, 2);                // [BYTES seg8:0x2f9a-0x2f9d seg8:0x2fd6-0x2fd9]
    // (heading + 12) % 16 for cut, else (heading + 4) % 16, signed idiv [BYTES seg8:0x2fa1-0x2fac
    // seg8:0x2fdd-0x2feb]
    int m = imod16(add16(heading, cut ? 12 : 4), 16);
    if (m < 0) m = 0;  // [DESIGN] a heading outside 0 ... 15 reads before the table in the original
    const std::int16_t tx = kDebrisT128[static_cast<std::size_t>(2 * m)];
    const std::int16_t ty = kDebrisT128[static_cast<std::size_t>(2 * m + 1)];
    win.px = add16(half, scale128(tx, win.R));             // [BYTES seg8:0x2fb1-0x2fc5 seg8:0x2ff0-0x3004]
    win.py = add16(half, scale128(ty, win.R));             // [BYTES seg8:0x300d-0x301b]
    const std::int16_t c = 128;                            // trunc(256 / 2) [BYTES seg8:0x301e-0x3024]
    std::int16_t v = add16(c, win.px);
    win.x1 = v <= E ? v : E;                               // jle [BYTES seg8:0x302f-0x3039]
    v = sub16(win.px, c);
    win.x0 = v >= 0 ? v : 0;                               // jge [BYTES seg8:0x303e-0x3049]
    v = add16(c, win.py);
    win.y1 = v <= E ? v : E;                               // [BYTES seg8:0x304e-0x305b]
    v = sub16(win.py, c);
    win.y0 = v >= 0 ? v : 0;                               // [BYTES seg8:0x3060-0x306b]
    v = sub16(c, win.px);
    win.ox = v >= 0 ? v : 0;                               // jns [BYTES seg8:0x3070-0x3077]
    v = sub16(c, win.py);
    win.oy = v >= 0 ? v : 0;                               // [BYTES seg8:0x307c-0x3083]
    e.x = add16(e.x, sub16(win.px, c));                    // [BYTES seg8:0x3086-0x308d]
    e.y = add16(e.y, sub16(win.py, c));                    // [BYTES seg8:0x3091-0x3096]
    return win;
}

std::array<DebrisBlit, 5> debris_strip_plan(const DebrisWindow& win, int shift) {
    std::array<DebrisBlit, 5> p{};
    const int s = shift & 0x1f;  // sar ax, cl: a 386 masks the count [DESIGN]
    for (int j = 0; j < 5; ++j) {  // [BYTES seg8:0x3183-0x31de]
        DebrisBlit& b = p[ix(j)];
        b.x = sar16(add16(mul16(256, static_cast<std::int16_t>(j)), win.ox), s);  // [BYTES seg8:0x319d-0x31aa]
        b.y = sar16(win.oy, s);                                                   // [BYTES seg8:0x31ad-0x31b0]
        b.w = sar16(sub16(win.x1, win.x0), s);                                    // [BYTES seg8:0x31b3-0x31b9]
        b.h = sar16(sub16(win.y1, win.y0), s);                                    // [BYTES seg8:0x31bc-0x31c2]
        b.sx = sar16(win.x0, s);                                                  // [BYTES seg8:0x31c8-0x31cb]
        b.sy = sar16(win.y0, s);                                                  // [BYTES seg8:0x31ce-0x31d1]
    }
    return p;
}

bool wing_picture(World& w, DebrisTable& t, Blitter* out, const ImageSlotTable* images, int ship, int cut,
                  bool make_debris) {
    const int pool = w.roles.ships;  // DGROUP:0x9324
    if (ship < 0 || ship >= kShipCount || ship >= w.pools.records(pool)) return false;  // [DESIGN] bounds
    ObjectRecord& o = w.pools.rec(pool, ship);
    const ShipRecord& S = w.ship(ship);
    const std::int16_t D = design_words(S.design_index).w3c;  // design +0x3c [BYTES seg8:0x30b5-0x30c0]
    const int k = make_debris ? debris_start(t, o) : -1;     // [BYTES seg8:0x2d96 seg8:0x33d7-0x33e9]
    if (k < 0) {
        // seg10:0x7370(DGROUP:0x3a38[ship], cut, size, D, gap, heading, 0) [BYTES seg8:0x343d-0x3482]
        wing_cut_ship(out, images, ship, cut != 0, o.size, D, o.gap, o.frame, false);
        return false;
    }
    if (w.w9130 != 0) sheet_todo("the wing debris in the seg18 mode", "seg8:0x2e67");  // [OPEN M6]
    DebrisEntry& e = t[ix(k)];
    const DebrisWindow win = debris_place(e, o.frame, D, cut != 0);  // [BYTES seg8:0x2f90-0x3096]
    // seg10:0x7370(DGROUP:0x3a38[ship], cut, size, D, gap, heading, 1) [BYTES seg8:0x309a-0x30d9]
    const std::vector<std::uint8_t> cell = wing_cut_ship(out, images, ship, cut != 0, o.size, D, o.gap, o.frame, true);
    if (out) {
        const std::int16_t p = sar16(256, w.shift & 0x1f);  // [bp-0x12] [BYTES seg8:0x33f9-0x3402]
        // The strip: CreateCompatibleBitmap(5p, p), PatBlt BLACKNESS [BYTES seg8:0x310f-0x3177]
        e.dc = out->create_image(mul16(p, 5), p);
        if (!cell.empty()) {
            const DcId wing = out->create_image_from(o.size, o.size, cell);  // ordinal 15's bitmap
            for (const DebrisBlit& b : debris_strip_plan(win, w.shift))  // [BYTES seg8:0x31d9]
                out->bit_blt(e.dc, b.x, b.y, b.w, b.h, wing, b.sx, b.sy, kSrcCopy);
            out->delete_image(wing);  // DeleteDC, DeleteObject [BYTES seg8:0x3391-0x33bd]
        }
        e.mask = out->create_mask(e.dc);  // seg10:0x6ec6, the ordinal-6 mask [BYTES seg8:0x3344-0x338d]
    }
    return true;  // [bp-6] = 1 [BYTES seg8:0x3391]; seg8:0x2cd8 plays 0x6a [BYTES seg8:0x33c2-0x33cd]
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
