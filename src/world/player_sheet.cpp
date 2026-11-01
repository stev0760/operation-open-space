// A ship's sprite sheet (src/world/player_sheet.h): seg14:0x11a8, seg10:0x6ee6 and the sheet
// measures of ISPACE.DLL seg6:0x11c. 16-bit arithmetic as the original's (ADR 0013 decision 7).
#include "world/player_sheet.h"

#include "world/world.h"

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

SpriteLinks& sprite_links() {
    static SpriteLinks l;
    return l;
}

}  // namespace is::world
