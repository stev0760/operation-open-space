// The wave frame's drawing: seg7:0x1208, seg7:0x3a42 and its frame loop, the rock cell, the
// ship bitmap table (docs/re/world.md "Drawing"). 16-bit arithmetic as the original's
// (ADR 0013 decision 7).
#include "world/draw.h"

namespace is::world {

PlayerCell player_cell(std::int16_t type, std::int16_t heading, std::int16_t size,
                       std::int16_t gap, std::uint8_t ship_be9) {
    PlayerCell cell;
    // Column [BYTES seg7:0x1315 seg7:0x131b seg7:0x1338 seg7:0x133d seg7:0x1346 seg7:0x1349 seg7:0x1358].
    if (type_cells_by_heading(type))
        cell.src_x = add16(mul16(heading, add16(gap, size)), gap);
    else
        cell.src_x = add16(mul16(size, 16), mul16(gap, 17));
    // Row [BYTES seg7:0x135c seg7:0x1365 seg7:0x1371 seg7:0x1384].
    if (type < 4) {
        if (ship_be9 == 0)
            cell.keep_y = true;
        else
            cell.src_y = gap;
    } else {
        cell.src_y = add16(mul16(gap, 2), size);
    }
    return cell;
}

bool player_sprite_drawn(const DrawContext& c) {
    const DrawGlobals& g = c.g;
    const int player = g.sprite.player;
    if (g.state != 1) return false;                                // [BYTES seg7:0x1216]
    if (c.pools.rec(kPoolShips, player).state != 0) return false;  // DGROUP:0x1e30 [BYTES seg7:0x1226]
    if (g.sprite.dg_951a != 0 || g.dg_9130 != 0) return false;  // [BYTES seg7:0x122d seg7:0x1234]
    // [BYTES seg7:0x123e seg7:0x1245 seg7:0x124f seg7:0x1257]
    if (g.sprite.dg_9e49 > 2 && g.sprite.dg_9e49 < 7 && player == g.sprite.dg_952e) return false;
    if (g.dg_3a38 == 0) return false;
    // The effect skip: ship byte +0xec = 0 and an entry 0..DGROUP:0x9434 with +6 = 14,
    // +8 = the player and 0 < +0 < 9, signed words [BYTES seg7:0x1266 seg7:0x127a
    // seg7:0x1283 seg7:0x128d seg7:0x1293 seg7:0x129a].
    if (c.ships[static_cast<std::size_t>(player)].bec == 0) {
        for (int k = 0; k <= g.last_effect && k < kEffectCount; ++k) {
            const EffectEntry& e = c.effects[static_cast<std::size_t>(k)];
            const std::int16_t kind = static_cast<std::int16_t>(e.kind);
            if (e.pool == 0xe && e.rec == player && kind > 0 && kind < 9) return false;
        }
    }
    return true;
}

void draw_player_sprite(DrawContext& c) {
    if (!player_sprite_drawn(c)) return;
    DrawGlobals& g = c.g;
    const int player = g.sprite.player;
    ObjectRecord& o = c.pools.rec(kPoolShips, player);
    const ShipRecord& ship = c.ships[static_cast<std::size_t>(player)];
    const std::int16_t size = o.size, gap = o.gap;
    // SetRect(DGROUP:0x1e5c, x + 2, y + 2, x + size - 2, y + size - 2) [BYTES seg7:0x12e4].
    g.player_rect = Rect16{add16(o.scr_x, 2), add16(o.scr_y, 2), sub16(add16(size, o.scr_x), 2),
                           sub16(add16(o.scr_y, size), 2)};
    const PlayerCell cell = player_cell(ship.type, o.frame, size, gap, ship.be9);
    o.src_x = cell.src_x;
    if (!cell.keep_y) o.src_y = cell.src_y;
    // seg8:0xb76((char)player | 0x0e00, 1, DGROUP:0x39e8[slot + 0x28], 0) [BYTES seg7:0x1388
    // seg7:0x1397 seg7:0x139d]: the pool is 14 whatever DGROUP:0x9324 holds.
    const std::int16_t pr = static_cast<std::int16_t>(
        static_cast<std::uint16_t>(static_cast<std::int16_t>(static_cast<std::int8_t>(player))) |
        0x0e00u);
    const int slot = ship_image_slot(player, ship);
    const DcId src =
        slot >= 0 && slot < kImageSlots ? c.images[static_cast<std::size_t>(slot)] : kDcNone;
    draw_sprite(c.pools, pr, 1, src, 0, g.sprite, c.to_screen, c.out);
    g.dg_58a = 0;  // [BYTES seg7:0x13a2]
}

void rock_cell(ObjectRecord& rock) {
    // [BYTES seg7:0x3d51 seg7:0x3d5d seg7:0x3d5f seg7:0x3d75 seg7:0x3d79 seg7:0x3d81
    // seg7:0x3d83 seg7:0x3d87]
    rock.frame = idiv16(rock.angle, 100);
    rock.src_x = add16(mul16(add16(rock.size, rock.gap), rock.frame), rock.gap);
}

void dissolve_icon(DrawContext& c, int i) {
    DrawGlobals& g = c.g;
    // Size and gap of pool-0 record 0 [BYTES seg7:0x3a55 seg7:0x3a5c].
    const std::int16_t size = c.pools.rec(kPoolIcons, 0).size;
    const std::int16_t gap = c.pools.rec(kPoolIcons, 0).gap;
    ObjectRecord& o = c.pools.rec(kPoolIcons, i);
    // Byte +0x51 -= DGROUP:0x8ffe, floor 0 as a signed byte [BYTES seg7:0x3a63 seg7:0x3a6a
    // seg7:0x3a72 seg7:0x3a79].
    o.race_status = static_cast<std::uint8_t>(o.race_status - g.dissolve_step);
    if (static_cast<std::int8_t>(o.race_status) < 0) o.race_status = 0;
    if (o.on_screen == 0 && !(c.hooks && c.hooks->seg13_59e8(0, i))) return;  // [BYTES seg7:0x3a86 seg7:0x3a92]

    // The cell grows inside the strip: StretchBlt COLORONCOLOR from 2 px around it
    // [BYTES seg7:0x3aa0-0x3ae3].
    const std::int16_t sx = add16(mul16(add16(gap, size), static_cast<std::int16_t>(i)), gap);
    c.out.set_stretch_mode(g.strip_dc, kColorOnColor);
    c.out.stretch_blt(g.strip_dc, sx, gap, size, size, g.strip_dc, sub16(sx, 2), sub16(gap, 2),
                      add16(size, 4), add16(size, 4), kSrcCopy);
    // Then onto the icon's screen square [BYTES seg7:0x3ae8-0x3b14].
    c.out.bit_blt(g.sprite.window_dc, o.scr_x, o.scr_y, size, size, g.strip_dc, sx, gap,
                  kSrcCopy);
    // The player's ship drawn over it on odd steps when it is the one +0x48 names and
    // Detail > 1 [BYTES seg7:0x3b19-0x3b38]. What +0x48 holds for an icon is [OPEN].
    if (o.blast_size == g.sprite.player && g.detail > 1 && (o.race_status & 1) != 0)
        draw_player_sprite(c);
    // The last step blacks the screen square and the strip cell [BYTES seg7:0x3b49-0x3b9d].
    if (static_cast<std::int8_t>(o.race_status) < 1) {
        c.out.pat_blt(g.sprite.window_dc, o.scr_x, o.scr_y, size, size, kBlackness);
        c.out.pat_blt(g.strip_dc, sx, gap, size, size, kBlackness);
        if (c.hooks) {
            c.hooks->seg7_65aa(0, i);
            c.hooks->seg7_2080(0, -1);
        }
    }
}

void dissolve_pass(DrawContext& c) {
    // [BYTES seg7:0xbf2 seg7:0xc01 seg7:0xc06 seg7:0xc0b seg7:0xc14 seg7:0xc1a]
    for (int i = 0; i < c.pools.in_use(kPoolIcons) && i < c.pools.records(kPoolIcons); ++i)
        if (c.pools.rec(kPoolIcons, i).race_status != 0) dissolve_icon(c, i);
}

void draw_stars(DrawContext& /*c*/, int /*arg*/) {
    // seg8:0x158a [OPEN]: world.md documents no drawing for it yet.
}

void draw_lines(DrawContext& /*c*/) {
    // seg12:0x3778 [OPEN]: world.md documents no drawing for it yet.
}

DcId load_ship_body(Blitter& out, const fmt::BodyRecord& body) {
    if (body.bitmap_id == fmt::kBodyBitmapFromFile) return kDcNone;  // [OPEN]
    return out.load_graphic(body.bitmap_id, 0, -1);                   // [DESIGN ADR 0013] [OPEN]
}

void set_ship_image(ImageSlotTable& images, int ship, const ShipRecord& record, DcId dc) {
    const int slot = ship_image_slot(ship, record);
    if (slot >= 0 && slot < kImageSlots) images[static_cast<std::size_t>(slot)] = dc;
}

}  // namespace is::world
