// The entry's materialise effect (src/world/entry_effect.h): seg12:0x2556, seg12:0x26a4,
// seg7:0x13b2 and seg7:0x158e for the object pools. 16-bit arithmetic as the original's
// (ADR 0013 decision 7).
#include "world/entry_effect.h"

#include "world/world.h"

namespace is::world {

namespace {

bool rec_ok(const Pools& p, int pool, int rec) {
    return pool >= 0 && pool < kPoolCount && rec >= 0 && rec < p.records(pool);
}

}  // namespace

void clear_entries(EntryTable& t) {
    for (EntrySlot& s : t) s = EntrySlot{};
}

std::int16_t entry_block(std::int16_t size_level) {
    return sar16(shl16(2, size_level & 0x1f & 15), 1);
}

std::int16_t entry_blocks(std::int16_t size, std::int16_t size_level) {
    // [BYTES seg7:0x1417 seg7:0x1435]
    const std::int16_t b = entry_block(size_level);
    const std::int16_t rem = b == 0 ? std::int16_t{0} : imod16(size, b);
    return add16(static_cast<std::int16_t>(rem != 0 ? 1 : 0), sar16(size, size_level & 0x1f & 15));
}

void entry_buffer(EntryContext& c, int pool, int rec) {
    if (pool == 1 || !rec_ok(c.pools, pool, rec)) return;  // [BYTES seg12:0x2556]
    ObjectRecord& o = c.pools.rec(pool, rec);
    if (o.alt_dc != 0) return;
    if (pool == 7 && c.pools.records(7) > 0) c.pools.rec(7, 0).frame = 5;  // DGROUP:0x1e14 +0x28
    const std::int16_t side = add16(mul16(o.gap, 2), o.size);
    // CreateCompatibleDC + CreateCompatibleBitmap(DGROUP:0x13dc, side, side) + SelectObject
    // (+0x40) + SelectPalette (+0x6e) [BYTES seg12:0x2593 seg12:0x25b4]: one image.
    const DcId buf = c.out.create_image(side, side);
    o.alt_dc = buf;
    o.w3e = static_cast<std::int16_t>(buf);
    std::int16_t r = 0;  // [BYTES seg12:0x2621 seg12:0x262e]
    if (pool == c.ship_pool && rec < kShipCount && c.ships[static_cast<std::size_t>(rec)].type > 1)
        r = add16(o.size, o.gap);
    c.out.bit_blt(buf, 0, 0, side, side, static_cast<DcId>(o.w3a), mul16(add16(o.size, o.gap), o.frame),
                  r, kSrcCopy);  // [BYTES seg12:0x267d]
}

void entry_buffer_free(EntryContext& c, int pool, int rec) {
    if (!rec_ok(c.pools, pool, rec)) return;
    ObjectRecord& o = c.pools.rec(pool, rec);
    const DcId dc = o.alt_dc;
    if (dc != 0) {
        c.out.delete_image(dc);  // SelectObject, SelectPalette, DeleteDC
        o.alt_dc = 0;
    }
    if (o.w3e != 0) {
        if (static_cast<DcId>(o.w3e) != dc) c.out.delete_image(static_cast<DcId>(o.w3e));  // DeleteObject
        o.w3e = 0;
    }
}

void entry_start(EntryContext& c, std::uint8_t mode, int pool, int rec) {
    if (pool > 14 || !rec_ok(c.pools, pool, rec)) return;  // [OPEN] pools 20 ... 25
    ObjectRecord& o = c.pools.rec(pool, rec);
    if (static_cast<std::int8_t>(o.b2e) > 0) return;  // [BYTES seg7:0x13e1 seg7:0x13e6]
    if (c.hooks) c.hooks->sound(0x27, pool, rec, 1);   // [BYTES seg7:0x13f2]
    const std::int16_t n = entry_blocks(o.size, c.size_level);
    int k = 0;
    while (k < kEntrySlots && c.table[static_cast<std::size_t>(k)].pool != -1) ++k;  // [BYTES seg7:0x1494]
    if (k >= kEntrySlots) {
        // No free slot: the screen square blacked [BYTES seg7:0x1580].
        c.out.pat_blt(c.window_dc, o.scr_x, o.scr_y, o.size, o.size, kBlackness);
        return;
    }
    o.b2e = mode;  // [BYTES seg7:0x14b2]
    EntrySlot& s = c.table[static_cast<std::size_t>(k)];
    s.pool = static_cast<std::int16_t>(pool);  // [BYTES seg7:0x14bd]
    s.rec = static_cast<std::int16_t>(rec);
    s.total = mul16(n, n);  // [BYTES seg7:0x14cb]
    s.count = 0;
    s.n = n;                // [BYTES seg7:0x14d8]
    if (pool == 14 && rec < kShipCount) c.ships[static_cast<std::size_t>(rec)].beb = 1;  // [BYTES seg7:0x14e5]
    s.bits.fill(0);                // [BYTES seg7:0x14f6]
    c.dg_9148 = add16(c.dg_9148, 1);  // [BYTES seg7:0x1509]
    if (mode == 2) {               // [BYTES seg7:0x150d seg7:0x1548]
        const std::int16_t side = add16(mul16(o.gap, 2), o.size);
        c.out.pat_blt(o.alt_dc, 0, 0, side, side, kBlackness);
    }
}

namespace {

// The common tail of an effect's end [BYTES seg7:0x1acb-0x1af0].
void end_slot(EntryContext& c, EntrySlot& s) {
    if (s.pool == 0xe) {
        if (s.rec >= 0 && s.rec < kShipCount) c.ships[static_cast<std::size_t>(s.rec)].beb = 0;  // [BYTES seg7:0x1adc]
        if (c.hooks) c.hooks->menu_refresh();  // seg2:0x170e [BYTES seg7:0x1ae1]
    }
    s.pool = -1;                         // [BYTES seg7:0x1aea]
    c.dg_9148 = sub16(c.dg_9148, 1);     // [BYTES seg7:0x1af0]
}

// The pick that completes slot s [BYTES seg7:0x17dc-0x1ac8].
void finish(EntryContext& c, EntrySlot& s) {
    ObjectRecord& o = c.pools.rec(s.pool, s.rec);
    if (o.b2e == 1) {
        c.out.pat_blt(c.window_dc, o.scr_x, o.scr_y, o.size, o.size, kBlackness);  // [BYTES seg7:0x1819]
        if (c.hooks) c.hooks->vanished(s.pool, s.rec);  // [OPEN M4]
        end_slot(c, s);
        return;
    }
    if (o.on_screen != 0) {
        // The source is the buffer +0x3c at the sheet's coordinates [BYTES seg7:0x1a18-0x1a69]:
        // the cell itself only for heading 0, where the entry starts (turns wait for +0xeb).
        const std::int16_t sx = add16(mul16(add16(o.size, o.gap), o.frame), o.gap);
        c.out.bit_blt(c.window_dc, o.scr_x, o.scr_y, o.size, o.size, o.alt_dc, sx, o.gap, kSrcCopy);
    }
    o.b2e = 0;  // [BYTES seg7:0x1a8a]
    if (s.pool == 0xe && s.rec == c.player && c.player >= 0 && c.player < kShipCount &&
        c.ships[static_cast<std::size_t>(c.player)].w2e != 2 && c.hooks)
        c.hooks->sound(0x65, 0, 0, 0);  // [BYTES seg7:0x1a8f-0x1ab2]
    entry_buffer_free(c, s.pool, s.rec);  // seg12:0x26a4 [BYTES seg7:0x1ac3]
    end_slot(c, s);
}

}  // namespace

void entry_step(EntryContext& c) {
    const std::int16_t block = entry_block(c.size_level);
    const int sh = c.size_level & 0x1f & 15;
    for (int k = 0; k < kEntrySlots; ++k) {  // [BYTES seg7:0x1cdd-0x1cff]
        EntrySlot& s = c.table[static_cast<std::size_t>(k)];
        if (s.pool == -1) continue;
        if (s.pool > 0xe) continue;  // the pools 20 ... 25 [OPEN]
        if (!rec_ok(c.pools, s.pool, s.rec) || s.total <= 0) {
            end_slot(c, s);  // [DESIGN] a slot the port cannot run
            continue;
        }
        for (int pick = 0; pick < 10; ++pick) {  // [BYTES seg7:0x1cfa seg7:0x1c6a]
            std::int16_t r = 0;
            std::uint8_t bit = 0;
            int byte = 0;
            // The byte < 0x50 bound is the port's [DESIGN]: for n * n > 640 (n >= 26, Size 2
            // ships of side >= 51) the original's bits run on into the slot's words +0x50 ...
            do {  // [BYTES seg7:0x1c72-0x1ca3]
                r = imod16(static_cast<std::int16_t>(c.rng.rand()), s.total);
                byte = sar16(r, 3);
                bit = static_cast<std::uint8_t>(1u << (r & 7));
            } while (byte >= 0 && byte < 0x50 && (s.bits[static_cast<std::size_t>(byte)] & bit) != 0);
            if (byte >= 0 && byte < 0x50) s.bits[static_cast<std::size_t>(byte)] |= bit;  // [BYTES seg7:0x1cae]
            s.count = add16(s.count, 1);  // [BYTES seg7:0x1cb2]
            const std::int16_t x = shl16(imod16(r, s.n), sh);  // [BYTES seg7:0x1cc3-0x1ccd]
            const std::int16_t y = shl16(idiv16(r, s.n), sh);  // [BYTES seg7:0x1cd4-0x1cd8]
            if (!(s.total > s.count)) {  // [BYTES seg7:0x17d0-0x17d8]
                finish(c, s);
                return;  // [BYTES seg7:0x1af4]
            }
            ObjectRecord& o = c.pools.rec(s.pool, s.rec);
            const std::int16_t dx = add16(o.gap, x), dy = add16(o.gap, y);
            if (o.b2e == 1) {
                c.out.pat_blt(o.alt_dc, dx, dy, block, block, kBlackness);  // [BYTES seg7:0x1b7c]
            } else {
                const std::int16_t sx = add16(add16(mul16(add16(o.size, o.gap), o.frame), o.gap), x);
                c.out.bit_blt(o.alt_dc, dx, dy, block, block, static_cast<DcId>(o.w3a), sx, dy,
                              kSrcCopy);  // [BYTES seg7:0x1c62]
            }
        }
    }
}

}  // namespace is::world
