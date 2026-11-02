// A ship's sprite sheet (src/world/player_sheet.h): seg14:0x11a8, seg10:0x6ee6 and the sheet
// measures of ISPACE.DLL seg6:0x11c. 16-bit arithmetic as the original's (ADR 0013 decision 7).
#include "world/player_sheet.h"

#include "world/rock_images.h"

#include <algorithm>

#include "world/world.h"

#include "gfx/ship_flame.h"
#include "gfx/ship_sheet.h"

namespace is::world {

gfx::SheetArgs sheet_args(const SheetRequest& req) {
    gfx::SheetArgs a;
    a.gap = req.gap_word;
    a.four = req.w5;
    a.scale = req.scale;
    a.scheme_a = req.colour_a;
    a.scheme_b = req.colour_b;
    a.mode = req.mode;
    return a;
}

bool sheet_pixels_from(const gfx::ShipSheet& sh, SheetPixels& px) {
    if (!sh.ok()) return false;
    px.w = sh.width;
    px.h = sh.height;
    px.source_height = sh.cell;
    px.index.resize(sh.px.size());
    for (std::size_t k = 0; k < sh.px.size(); ++k)
        px.index[k] = sh.px[k] < 0 ? std::uint8_t{0} : static_cast<std::uint8_t>(sh.px[k] & 0xff);
    return true;
}

std::int16_t sheet_gap(int gap_word, int side) {
    if (gap_word != kSheetGapAuto) return static_cast<std::int16_t>(gap_word);
    return static_cast<std::int16_t>((side & 1) != 0 ? 5 : 6);
}

SheetGeometry sheet_geometry(int src_w, int src_h, int gap_word, int mode, int scale) {
    SheetGeometry g;
    g.size = src_h;
    g.gap = sheet_gap(gap_word, src_h);
    g.frames = src_h > 0 ? src_w / src_h : 0;
    g.rows = (mode == 1 && g.frames > 1) ? 2 : g.frames;
    g.scale = scale;
    g.extra = mode == 1 ? ((g.size + g.gap) << scale) : 0;
    g.w = ((16 * g.size + 17 * g.gap) << scale) + g.extra;
    g.h = ((g.size + g.gap) * g.frames + g.gap) << scale;
    return g;
}

Point16 sheet_cell(int column, int row, std::int16_t size, std::int16_t gap) {
    const std::int16_t step = add16(size, gap);
    return Point16{add16(mul16(static_cast<std::int16_t>(column), step), gap),
                   add16(mul16(static_cast<std::int16_t>(row), step), gap)};
}

std::uint8_t sheet_mode(std::uint8_t has_flame, std::uint8_t be9) {
    std::uint8_t m = has_flame == 0 ? 2 : 0;  // [BYTES seg14:0x1245 seg14:0x124c seg14:0x1252]
    if (be9 != 0) m = 1;                      // [BYTES seg14:0x125a seg14:0x1261]
    return m;
}

std::int16_t sheet_sel(int quick, int state, std::uint32_t frames, int ship, int player) {
    // [BYTES seg14:0x1265-0x1297]
    if (state == 1 && frames != 0) {
        if ((quick == 1 && ship != player) || quick == 2) return 1;
        return 0;
    }
    return quick == 2 ? 1 : 0;
}

std::int16_t sheet_scale_flag(int size_level, int w6, int sel) {
    // [BYTES seg10:0x6f21-0x6f41]
    if (w6 > 1) return 1;
    return (size_level >= 2 && w6 != -1 && sel == 0) ? 1 : 0;
}

SheetPixels unrotated_sheet(const std::vector<std::uint8_t>& src, int src_w, int src_h,
                            const SheetRequest& req) {
    SheetPixels px;
    if (src_w <= 0 || src_h <= 0 || src.size() < static_cast<std::size_t>(src_w) * src_h) return px;
    const SheetGeometry g = sheet_geometry(src_w, src_h, req.gap_word, req.mode, req.scale);
    px.w = g.w;
    px.h = g.h;
    px.index.assign(static_cast<std::size_t>(g.w) * g.h, 0);
    px.source_height = src_h;
    const int f = 1 << g.scale;
    for (int r = 0; r < g.rows; ++r)
        for (int c = 0; c < 16; ++c) {
            const Point16 o = sheet_cell(c, r, static_cast<std::int16_t>(g.size),
                                         static_cast<std::int16_t>(g.gap));
            for (int y = 0; y < g.size * f; ++y)
                for (int x = 0; x < g.size * f; ++x) {
                    const int dx = (o.x << g.scale) + x, dy = (o.y << g.scale) + y;
                    if (dx < 0 || dy < 0 || dx >= g.w || dy >= g.h) continue;
                    const int sx = r * g.size + x / f, sy = y / f;
                    if (sx >= src_w) continue;
                    px.index[static_cast<std::size_t>(dy) * g.w + dx] =
                        src[static_cast<std::size_t>(sy) * src_w + sx];
                }
        }
    return px;
}

SheetBuild build_ship_sheet(Blitter& out, ImageSlotTable& images, ShipRecord& ship, int i,
                            const SheetRequest& req, const SheetRenderer& render) {
    SheetBuild b;
    if (req.slot < 0 || req.slot >= kImageSlots) return b;  // [DESIGN] storage bound
    std::uint16_t& slot = images[static_cast<std::size_t>(req.slot)];
    const auto byte = [](const ShipRecord& s, int off) {
        return static_cast<std::uint8_t>(sbyte_at(s, off));
    };
    // The shared sheet of ship +0x50 [BYTES seg14:0x11cf seg14:0x11db seg14:0x11e2]: not ported
    // [OPEN] (dead in the wave set-up).
    (void)i;
    // Already built [BYTES seg14:0x120e seg14:0x1219 seg14:0x1220].
    if (slot != 0 && byte(ship, 0xac) != 0 && byte(ship, 0xad) != 0) {
        b.dc = slot;
        set_byte_at(ship, 0xad, 1);
        set_byte_at(ship, 0xac, 1);
        return b;
    }
    const bool was_empty = slot == 0;  // the slot word before the call [BYTES seg14:0x122e]
    // seg10:0x6ee6: seg10:0x552(slot, 0) when the slot holds a DC [BYTES seg10:0x6ee6].
    if (slot != 0) {
        out.delete_image(slot);
        slot = 0;
    }
    SheetPixels px;
    if (render && render(req, px) && px.w > 0 && px.h > 0) {
        slot = out.create_image_from(px.w, px.h, px.index);  // [BYTES seg10:0x6f7f seg10:0x6f89]
        b.rendered = slot != 0;
        b.bm_height = static_cast<std::int16_t>(px.h);
        if (b.rendered) b.pixels = px;
    }
    b.dc = slot;
    // Ordinal 5 after the render and the gap [BYTES seg10:0x86d7 seg14:0x12d8 seg14:0x12e0].
    b.side = static_cast<std::int16_t>(px.source_height);
    b.gap = sheet_gap(kSheetGapAuto, b.side);
    // The object words +0x22 / +0x24 only for no roster record with the slot empty before, else
    // the roster bytes +0x7a / +0x7b [BYTES seg14:0x12f5 seg14:0x130c seg14:0x131c seg14:0x1322
    // seg14:0x1337 seg14:0x134d].
    b.to_object = ship.roster_index == -1 && was_empty;
    // [BYTES seg14:0x13eb seg14:0x13ef]
    set_byte_at(ship, 0xad, 1);
    set_byte_at(ship, 0xac, 1);
    return b;
}

std::int16_t weapon_sheet_sel(int quick, int state, std::uint32_t frames) {
    if (state == 1 && frames != 0 && (quick == 1 || quick == 2)) return 1;  // [BYTES seg10:0x53b3-0x53d0]
    return quick == 2 ? 1 : 0;                                              // [BYTES seg10:0x53d7-0x53de]
}

int build_weapon_sheets(Blitter& out, ImageSlotTable& images, Pools& pools, WeaponWords& words,
                        const ShipRecord& ship, int sel, int size_level,
                        const SheetRenderer& render) {
    int filled = 0;
    // l = 0 ... (int8) byte +0xcb - 1 [BYTES seg14:0x13f3-0x1425]
    const int n = static_cast<std::int8_t>(sbyte_at(ship, 0xcb));
    for (int l = 0; l < n; ++l) {
        const int w = sbyte_at(ship, 0xb8 + l);
        if (words.word(w, 0x0a) == 0) continue;  // [BYTES seg14:0x1407]
        const WeaponSlotChoice ch = weapon_image_slot(words, images, pools, w);  // seg10:0x533a
        if (!ch.load) continue;
        if (words.word(w, 0x06) == 4) continue;  // seg10:0x4412(k, 6, W-2, W+0x26) [OPEN]
        SheetRequest req;
        req.slot = ch.slot;
        req.graphic_id = words.word(w, -2);
        req.file.clear();     // far DGROUP:0x76d, the empty string
        req.w1 = 2;
        req.gap_word = kSheetGapAuto;  // 0x63
        req.recolour_a = words.word(w, 0x1a);
        req.recolour_b = words.word(w, 0x1c);
        req.w5 = 1;
        req.scale = sheet_scale_flag(size_level, 0, sel);
        req.colour_a = 12;
        req.colour_b = 12;
        req.mode = 0;
        req.sel = sel;
        req.size_level = size_level;
        std::uint16_t& slot = images[static_cast<std::size_t>(ch.slot)];
        if (slot != 0) {  // seg10:0x552(slot, 0) [BYTES seg10:0x6ee6]
            out.delete_image(slot);
            slot = 0;
        }
        SheetPixels px;
        if (render && render(req, px) && px.w > 0 && px.h > 0) {
            slot = out.create_image_from(px.w, px.h, px.index);  // [BYTES seg10:0x6f7f seg10:0x6f89]
            if (slot != 0) ++filled;
        }
    }
    return filled;
}

SheetBuild rebuild_ship_sheet(Blitter& out, ImageSlotTable& images, ShipRecord& ship,
                              ObjectRecord& obj, int i, const SheetRequest& req,
                              const SheetRenderer& render) {
    set_byte_at(ship, 0xac, 0);  // [BYTES seg16:0x3621]
    SheetBuild b = build_ship_sheet(out, images, ship, i, req, render);  // [BYTES seg16:0x362b]
    const int dcslot = kShipImageSlotBase + i;  // DGROUP:0x3a38[i] [BYTES seg16:0x3636 seg16:0x3645]
    if (dcslot >= 0 && dcslot < kImageSlots)
        obj.w3a = static_cast<std::int16_t>(images[static_cast<std::size_t>(dcslot)]);
    return b;
}

std::uint8_t sheet_last_row(std::int16_t bm_height, std::int16_t size, std::int16_t gap) {
    const std::int16_t d = add16(size, gap);
    if (d == 0) return 0;
    return static_cast<std::uint8_t>(sub16(idiv16(sub16(bm_height, gap), d), 1));
}

// ---- the thrust flame (M5 s2 FLAME)

const std::array<std::int16_t, 5> kFlameCx = {0, -98, -181, -236, -256};  // [BYTES DGROUP:0x6d6]
const std::array<std::int16_t, 5> kFlameCy = {-256, -236, -181, -98, 0};  // [BYTES DGROUP:0x6e0]

FlameDesign flame_design(std::uint8_t shp_flags, std::uint8_t bod_flags, std::int8_t bod_19) {
    FlameDesign d;
    d.type = (shp_flags & 0x01) != 0 ? 0 : 1;  // [BYTES seg14:0x1ed6-0x1eef]
    if ((shp_flags & 0x20) != 0) d.type = static_cast<std::int16_t>(d.type | 0x20);  // [BYTES seg14:0x1f09]
    d.behind = (bod_flags & 0x08) != 0;  // [BYTES seg14:0x1d55-0x1d69]
    d.radius = bod_19;
    return d;
}

std::int16_t flame_radius_word(std::int8_t bod_19, std::int16_t side) {
    std::int16_t v = sub16(0, static_cast<std::int16_t>(bod_19));  // cbw, neg [BYTES seg14:0x1c7f-0x1c88]
    // side / 2: cwd; sub ax, dx; sar ax, 1, toward zero [BYTES seg14:0xc42-0xc48]
    const std::int16_t s = idiv16(side, 2);
    if (v < 0) v = sub16(sub16(4, v), s);  // [BYTES seg14:0xc58-0xc68]
    return v;
}

namespace {

// cwd; sub dh, dh; add ax, dx; sar ax, 8: a 16-bit value / 256 toward zero [BYTES seg10:0x1803-0x180b].
std::int16_t div256(std::int16_t v) {
    const std::int16_t bias = v < 0 ? 0xff : 0;
    return static_cast<std::int16_t>(add16(v, bias) >> 8);
}

// sar by cl: a 386 masks the count to 5 bits [DESIGN].
std::int16_t sar_cl(std::int16_t v, int cl) { return sar16(v, cl & 0x1f); }

}  // namespace

FlameAnchors flame_anchors(std::int16_t side2a, std::int16_t r04, int size_level) {
    FlameAnchors a;
    // half = (+0x2a << Size) >> 1 [BYTES seg10:0x16ea-0x16f9]
    const std::int16_t half = sar16(shl16(side2a, size_level & 0x0f), 1);
    const std::int16_t c2 = idiv16(half, 2);  // [BYTES seg10:0x1822-0x1827]
    const std::int16_t r0 = mul16(add16(r04, 1), 2);  // [BYTES seg10:0x1703-0x170a]
    const int cl = (2 - size_level) & 0xff;  // mov cl, 2; sub cl, [0x9156] [BYTES seg10:0x180e-0x1810]
    for (int k = 0; k < 5; ++k) {
        std::int16_t r = r0;
        if (k == 0 || k == 3 || k == 4) r = sub16(r, 1);  // [BYTES seg10:0x186d-0x187f]
        if (k == 0) r = sub16(r, 1);                       // [BYTES seg10:0x1715]
        std::int16_t wx = 9, wy = 9;                       // [BYTES seg10:0x1718]
        if (k == 1) wx = 7;                                // [BYTES seg10:0x17ba-0x17bf]
        if (k == 3 && side2a > 0x1d) wy = 10;              // [BYTES seg10:0x17c7-0x17df]
        if (k == 4) wx = size_level >= 2 ? 8 : 6;          // [BYTES seg10:0x17e7-0x17f3]
        const std::int16_t tx = div256(mul16(kFlameCx[static_cast<std::size_t>(k)], r));
        const std::int16_t ty = div256(mul16(kFlameCy[static_cast<std::size_t>(k)], r));
        Point16& d = a.d[static_cast<std::size_t>(k)];
        d.x = add16(sub16(sar_cl(tx, cl), sar_cl(wx, cl)), c2);  // [BYTES seg10:0x1817-0x183a]
        d.y = sub16(sub16(c2, sar_cl(ty, cl)), sar_cl(wy, cl));  // [BYTES seg10:0x183e-0x185a]
    }
    return a;
}

void flame_strip_create(FlameStrip& strip, int f) {
    if (strip.ok() && strip.f == f) return;  // once [BYTES seg10:0x5c8f]
    strip.f = f;
    strip.w = strip.h = 16 * f;  // [BYTES seg10:0x5ca8]
    strip.px.assign(static_cast<std::size_t>(strip.w) * strip.h, 0);  // BLACKNESS [BYTES seg10:0x5cf2]
}

namespace {

// A raster-operation copy of w x h pixels from `src` (sw x sh) into the strip, clipped to both;
// op(dst, src) gives the new destination.
template <typename Src, typename Op>
void strip_rop(FlameStrip& st, int dx, int dy, int w, int h, int sx, int sy, int sw, int sh,
               const Src& src, const Op& op) {
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int tx = dx + x, ty = dy + y, ux = sx + x, uy = sy + y;
            if (tx < 0 || ty < 0 || tx >= st.w || ty >= st.h) continue;
            if (ux < 0 || uy < 0 || ux >= sw || uy >= sh) continue;  // a clipped source [DESIGN]
            std::uint8_t& d = st.px[static_cast<std::size_t>(ty) * st.w + tx];
            d = op(d, src(ux, uy));
        }
}

void strip_black(FlameStrip& st, int x0, int y0, int w, int h) {
    for (int y = y0; y < y0 + h; ++y)
        for (int x = x0; x < x0 + w; ++x)
            if (x >= 0 && y >= 0 && x < st.w && y < st.h) st.px[static_cast<std::size_t>(y) * st.w + x] = 0;
}

// The ordinal-6 mask as the raster operation sees it: white (all index bits) where the pixel is
// black, black (0) elsewhere [DESIGN index 0 is black].
std::uint8_t mask_of(std::uint8_t v) { return v == 0 ? 0xff : 0; }

}  // namespace

void build_flame_strip(FlameStrip& st, const gfx::FlameHeadings& flame, int i,
                       const SheetPixels& sheet, std::int16_t size, std::int16_t gap,
                       const FlameDesign& design, const FlameAnchors& anchors,
                       std::uint8_t has_flame) {
    if (!st.ok() || i < 0 || i >= 8) return;  // [DESIGN] 8 ships in a 16-row strip
    st.anchors[static_cast<std::size_t>(i)] = anchors;
    const int f = st.f;
    const int r0 = 2 * i * f, r1 = (2 * i + 1) * f;
    const auto copy = [](std::uint8_t, std::uint8_t s) { return s; };
    const auto band = [](std::uint8_t d, std::uint8_t s) { return static_cast<std::uint8_t>(d & s); };
    const auto bor = [](std::uint8_t d, std::uint8_t s) { return static_cast<std::uint8_t>(d | s); };
    if (has_flame != 0) {  // [BYTES seg10:0x5e1e-0x5e30]
        const auto sheet_px = [&sheet](int x, int y) {
            return sheet.index[static_cast<std::size_t>(y) * sheet.w + x];
        };
        // Row 2i: the sheet patches of headings 0 ... 4 [BYTES seg10:0x5e36-0x5eb6].
        for (int k = 0; k < 5; ++k) {
            const Point16 d = anchors.d[static_cast<std::size_t>(k)];
            const int sx = gap + k * (size + gap) + d.x, sy = gap + d.y;
            strip_rop(st, f * k, r0, f, f, sx, sy, sheet.w, sheet.h, sheet_px, copy);
        }
        const int fw = flame.ok() ? flame.width : 0, fh = flame.ok() ? flame.height : 0;
        const auto flame_px = [&flame](int x, int y) { return flame.at(x, y); };
        const auto flame_mask = [&flame](int x, int y) { return mask_of(flame.at(x, y)); };
        const int fy = design.type * f;  // the whole word [BYTES seg10:0x5da7 seg10:0x5feb-0x5fee]
        // A snapshot for the strip's own reads (row 2i, the strip's mask) [DESIGN: the blits
        // read rows the step does not write].
        const FlameStrip before = st;
        const auto strip_px = [&before](int x, int y) { return before.at(x, y); };
        if (!design.behind) {
            // [BYTES seg10:0x5fa2-0x6019]
            strip_rop(st, 0, r1, 5 * f, f, 0, r0, st.w, st.h, strip_px, copy);
            strip_rop(st, 0, r1, 5 * f, f, 0, fy, fw, fh, flame_mask, band);   // 0x8800c6
            strip_rop(st, 0, r1, 5 * f, f, 0, fy, fw, fh, flame_px, bor);      // 0xee0086
        } else {
            // [BYTES seg10:0x5ecc-0x5f9b]
            strip_rop(st, 0, r1, 5 * f, f, 0, fy, fw, fh, flame_px, copy);
            const FlameStrip masked = st;  // the mask of DGROUP:0x3b14 after the copy [BYTES seg10:0x5f43]
            const auto strip_mask = [&masked](int x, int y) { return mask_of(masked.at(x, y)); };
            strip_rop(st, 0, r1, 5 * f, f, 0, 0, st.w, st.h, strip_mask, band);
            strip_rop(st, 0, r1, 5 * f, f, 0, r0, st.w, st.h, strip_px, bor);
        }
        // The trim [BYTES seg10:0x601e-0x6100].
        for (int k = 0; k < 5; ++k) {
            const Point16 d = anchors.d[static_cast<std::size_t>(k)];
            const int x = f * k, y = r1;
            strip_black(st, x, y, f, std::max(0, -d.y));
            strip_black(st, x, y, std::max(0, -d.x), f);
            strip_black(st, x, y + std::max(0, size - d.y), f, std::max(0, d.y - size + f));
            strip_black(st, x + std::min(size - d.x - 1, f), y, std::max(0, d.x - size + f + 1), f);
        }
    }
    // The mirror of rows 2i and 2i + 1 [BYTES seg10:0x6104-0x6263].
    for (int rr = 0; rr < 2; ++rr) {
        const int y0 = (2 * i + rr) * f;
        const auto put = [&](int col, int x, int y, std::uint8_t v) {
            st.px[static_cast<std::size_t>(y0 + y) * st.w + col * f + x] = v;
        };
        for (int k = 0; k < 5; ++k)
            for (int y = 0; y < f; ++y)
                for (int x = 0; x < f; ++x) {
                    const std::uint8_t v = st.at(k * f + x, y0 + y);
                    const int fx = f - 1 - x, fyy = f - 1 - y;
                    if (k == 0) {
                        put(8, x, fyy, v);
                    } else if (k == 4) {
                        put(12, fx, y, v);
                    } else {
                        put(8 - k, x, fyy, v);
                        put(k + 8, fx, fyy, v);
                        put(16 - k, fx, y, v);
                    }
                }
    }
}

void upload_flame_strip(Blitter& out, FlameStrip& strip) {
    if (strip.dc != kDcNone) out.delete_image(strip.dc);
    strip.dc = strip.ok() ? out.create_image_from(strip.w, strip.h, strip.px) : kDcNone;
}

std::vector<FlamePaste> flame_pastes(int f, const FlameAnchors& a, int i, int on,
                                     std::int16_t type, std::int16_t heading, std::int16_t size,
                                     std::int16_t gap, std::uint8_t has_flame) {
    std::vector<FlamePaste> v;
    if (has_flame == 0) return v;                // [BYTES seg11:0xb11d-0xb127]
    const bool all = !(type > 0 && type < 7);    // [BYTES seg11:0xb105-0xb118]
    const int sy = (2 * i + (on != 0 ? 1 : 0)) * f;
    for (int h = 0; h < 16; ++h) {
        if (!all && h != heading) continue;      // [BYTES seg11:0xb2b9-0xb2d5]
        int ox, oy;
        if (h < 5) {
            ox = a.d[static_cast<std::size_t>(h)].x;
            oy = a.d[static_cast<std::size_t>(h)].y;
        } else if (h < 9) {
            ox = a.d[static_cast<std::size_t>(8 - h)].x;
            oy = size - a.d[static_cast<std::size_t>(8 - h)].y - f;
        } else if (h < 12) {
            ox = size - a.d[static_cast<std::size_t>(h - 8)].x - f;
            oy = size - a.d[static_cast<std::size_t>(h - 8)].y - f;
        } else {
            ox = size - a.d[static_cast<std::size_t>(16 - h)].x - f;
            oy = a.d[static_cast<std::size_t>(16 - h)].y;
        }
        const int sx = f * h;
        if (all) {
            const int base = type > 3 ? size + gap : 0;  // [BYTES seg11:0xb1d0-0xb1e2]
            v.push_back({h * (size + gap) + gap + ox, base + gap + oy, sx, sy});
        } else {
            const int x = 16 * size + 17 * gap + ox;  // column 16 [BYTES seg11:0xb1ed-0xb264]
            v.push_back({x, gap + oy, sx, sy});
            v.push_back({x, 2 * gap + size + oy, sx, sy});
        }
    }
    return v;
}

void apply_flame_pastes(SheetPixels& sheet, const FlameStrip& strip,
                        const std::vector<FlamePaste>& pastes) {
    const int f = strip.f;
    for (const FlamePaste& p : pastes)
        for (int y = 0; y < f; ++y)
            for (int x = 0; x < f; ++x) {
                const int tx = p.dx + x, ty = p.dy + y, ux = p.sx + x, uy = p.sy + y;
                if (tx < 0 || ty < 0 || tx >= sheet.w || ty >= sheet.h) continue;
                if (ux < 0 || uy < 0 || ux >= strip.w || uy >= strip.h) continue;
                sheet.index[static_cast<std::size_t>(ty) * sheet.w + tx] = strip.at(ux, uy);
            }
}

void paste_flame(Blitter& out, const ImageSlotTable& images, const FlameStrip& strip, int i,
                 const std::vector<FlamePaste>& pastes) {
    const int slot = kShipImageSlotBase + i;
    if (slot < 0 || slot >= kImageSlots || strip.dc == kDcNone) return;
    const DcId dst = images[static_cast<std::size_t>(slot)];
    if (dst == kDcNone) return;
    for (const FlamePaste& p : pastes)
        out.bit_blt(dst, p.dx, p.dy, strip.f, strip.f, strip.dc, p.sx, p.sy, kSrcCopy);
}

FlameStrip& flame_strip() {
    static FlameStrip s;
    return s;
}

void ship_flame_after_sheet(Blitter& out, const gfx::FlameHeadings& flame, int i,
                            const SheetBuild& build, const ObjectRecord& obj,
                            const ShipRecord& ship, const FlameDesign& design, int size_level) {
    if (!build.rendered || build.pixels.w <= 0 || !flame.ok()) return;
    FlameStrip& st = flame_strip();
    const bool remade = !st.ok() || st.f != flame.f;
    flame_strip_create(st, flame.f);  // seg10:0x5c42 [BYTES seg10:0x5c8f-0x5cf2]
    if (remade && st.dc != kDcNone) {
        out.delete_image(st.dc);
        st.dc = kDcNone;
    }
    // [bp - 0x24] is one more at Size 0 for an odd side [BYTES seg14:0xbb5-0xbc2]; +0x2a is not.
    const std::int16_t side24 =
        (size_level == 0 && (build.side & 1) != 0) ? add16(build.side, 1) : build.side;
    const FlameAnchors a =
        flame_anchors(build.side, flame_radius_word(design.radius, side24), size_level);
    build_flame_strip(st, flame, i, build.pixels, obj.size, obj.gap, design, a, ship.has_flame);
    upload_flame_strip(out, st);
}

void ship_flame_tail(Blitter& out, const ImageSlotTable& images, int i, int on,
                     const ShipRecord& ship, const ObjectRecord& obj) {
    const FlameStrip& st = flame_strip();
    if (!st.ok() || i < 0 || i >= 8) return;
    const auto p = flame_pastes(st.f, st.anchors[static_cast<std::size_t>(i)], i, on, ship.type,
                                obj.frame, obj.size, obj.gap, ship.has_flame);
    paste_flame(out, images, st, i, p);
}

SpriteLinks& sprite_links() {
    static SpriteLinks l;
    return l;
}

}  // namespace is::world
