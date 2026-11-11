// A dead ship's sheet and pod (death.h).
#include "world/death.h"

#include "world/flight.h"
#include "world/rock_images.h"

namespace is::world {

namespace {

constexpr std::size_t ix(int k) { return static_cast<std::size_t>(k); }

constexpr int kPodPool = 11;  // DGROUP:0x1e24 = DGROUP:0x1df8 + 4·11 [BYTES seg13:0x49b5]

}  // namespace

// seg13:0x4710 [BYTES seg13:0x4710-0x498d]
void burn_ship_sheet(EffectCtx& c, int ship) {
    World& w = c.w;
    c.host.sound(0x6a, w.roles.ships, ship, 1);  // [BYTES seg13:0x471f-0x472a]
    if (!c.out || !c.images) return;
    const int slot = kShipImageSlotBase + ship;  // DGROUP:0x3a38 + 2 ship, not +0x50 [BYTES seg13:0x47b8]
    if (ship < 0 || slot >= kImageSlots || w.pools.records(w.roles.ships) <= ship) return;
    const DcId sheet = (*c.images)[ix(slot)];
    if (sheet == kDcNone) return;  // [DESIGN]
    Blitter& out = *c.out;
    const int sl = w.size & 15;
    const std::int16_t p = sar16(shl16(0x20, sl), 1);  // [bp-0x1e] [BYTES seg13:0x4732-0x473d]
    // Graphic 230, then seg10:0x6308(-1, -1, bitmap, -1) [BYTES seg13:0x4740-0x475e] (rock_images.h
    // rescale_bitmap) (M4 s8 SIZE-SITES).
    const DcId g = rescale_bitmap(out, out.load_graphic(kGraphicFlames, 0, -1), w.size);  // [BYTES seg13:0x4749]
    const DcId gm = out.create_mask(g);                        // seg10:0x6ec6 [BYTES seg13:0x4762]
    const DcId sm = out.create_mask(sheet);                    // [BYTES seg13:0x47cb]
    const ObjectRecord& o = w.pools.rec(w.roles.ships, ship);
    const std::int16_t size = o.size, gap = o.gap;             // [bp-0x20], [bp-0x26]
    const std::int16_t q = size > p ? p : size;                // [bp-0x22] [BYTES seg13:0x481e-0x4826]
    std::int16_t off = idiv16(sub16(size, p), 2);              // [bp-0x24] [BYTES seg13:0x4829-0x483b]
    if (off < 0) off = 0;
    // The raised silhouette [BYTES seg13:0x483e-0x4870].
    out.bit_blt(sm, 0, sub16(0, sar16(shl16(9, sl), 1)), add16(shl16(size, 4), mul16(gap, 0x11)),
                add16(size, shl16(gap, 1)), sm, 0, 0, kSrcAnd);
    for (std::int16_t h = 0; h < 16; ++h) {  // [BYTES seg13:0x4875-0x491d]
        const std::int16_t x = add16(mul16(add16(gap, size), h), gap);
        const std::int16_t sx = mul16(imod16(h, 5), p);
        out.bit_blt(sheet, add16(x, off), add16(gap, off), q, q, gm, sx, 0, kSrcAnd);    // [BYTES seg13:0x48c9]
        out.bit_blt(sheet, add16(x, off), add16(gap, off), q, q, g, sx, 0, kSrcPaint);   // [BYTES seg13:0x48eb]
        out.bit_blt(sheet, x, gap, size, size, sm, x, gap, kDsna);                       // [BYTES seg13:0x4911]
    }
    // DeleteDC / DeleteObject of the three [BYTES seg13:0x4955-0x4980].
    out.delete_image(g);
    out.delete_image(sm);
    out.delete_image(gm);
}

// seg13:0x4ba0 [BYTES seg13:0x4ba0-0x4ec9]
void fill_pod(EffectCtx& c, int ship, int rec, int flag) {
    World& w = c.w;
    Pools& pools = w.pools;
    if (rec < 0 || rec >= kPodRecords || rec >= pools.records(kPodPool) || ship < 0 ||
        ship >= kShipCount || pools.records(w.roles.ships) <= ship)
        return;  // [DESIGN] bounds
    PodRecord& pd = c.st.pods[ix(rec)];
    ShipRecord& s = w.ships[ix(ship)];
    pd.age.set(0x01f40000u);  // [BYTES seg13:0x4bb3]
    {
        ObjectRecord& po = pools.rec(kPodPool, rec);
        if (w.handover.demons_gate == 0 || ship == w.player) {  // [BYTES seg13:0x4bbc-0x4bc9]
            if (flag != 0) {
                for (int j = 1; j < 10; ++j) {  // [BYTES seg13:0x4bd5-0x4c38]
                    if (static_cast<std::int8_t>(s.slots_cb()) > j) {
                        pd.weapon[ix(j)] = static_cast<std::int8_t>(s.weapon[ix(j)]);
                        pd.count[ix(j)] = static_cast<std::int8_t>(s.weapon_count[ix(j)]);
                    } else {
                        pd.weapon[ix(j)] = -1;
                        pd.count[ix(j)] = -1;
                    }
                    s.weapon[ix(j)] = 0;
                    s.weapon_count[ix(j)] = 0;
                }
                pd.kind = 1;                  // [BYTES seg13:0x4c3e]
                pd.resources = s.resources(); // [BYTES seg13:0x4c49-0x4c4d]
                s.slots_cb() = 1;             // [BYTES seg13:0x4c51]
            } else {
                pd.kind = 3;  // [BYTES seg13:0x4c5c]
                const std::int16_t r = s.resources();
                pd.resources = r > 400 ? std::int16_t{400} : r;  // [BYTES seg13:0x4c67-0x4c73]
            }
            s.set_resources(sub16(s.resources(), pd.resources));  // [BYTES seg13:0x4c77-0x4c84]
            po.w4a = 0x1d;                                         // [BYTES seg13:0x4c90]
        } else {
            po.w4a = 0x1d;  // [BYTES seg13:0x4ca0]
            pd.kind = 2;    // [BYTES seg13:0x4caa]
        }
        pd.pool = w.roles.ships;                 // [BYTES seg13:0x4cb7]
        pd.rec = static_cast<std::int16_t>(ship);  // [BYTES seg13:0x4cbe]
        const int sl = w.size & 15;
        po.size = sar16(shl16(0x18, sl), 1);  // [BYTES seg13:0x4cd5]
        po.gap = sar16(shl16(4, sl), 1);      // [BYTES seg13:0x4ce8]
        po.extent = 0xb8;                     // [BYTES seg13:0x4cf0]
        po.state = 0;                         // [BYTES seg13:0x4cfa]
    }
    ObjectRecord& po = pools.rec(kPodPool, rec);
    const ObjectRecord& so = pools.rec(w.roles.ships, ship);
    const std::int16_t wrap = shl16(kWorldCells, 3);  // DGROUP:0x5a0 << 3 [BYTES seg13:0x4d88]
    // The heading of (frame + 8) & 15 as a byte [BYTES seg13:0x4d5e-0x4d67].
    const int h8 = (static_cast<std::uint8_t>(so.frame & 0xff) + 8) & 15;
    // x: the ship's centre less the pod's half, plus dx·extent / 16, then % 25600
    // [BYTES seg13:0x4d2c-0x4d9c].
    po.x = sub16(add16(idiv16(so.extent, 2), so.x), idiv16(po.extent, 2));
    po.x = add16(po.x, idiv16(mul16(heading_dx(h8), so.extent), 16));
    po.x = imod16(po.x, wrap);
    // y: the same, the offset taken % 25600 before the add and the sum after, all signed
    // remainders (core.md The dropped pod, The pod's position, C083) [BYTES seg13:0x4da0-0x4e40].
    po.y = sub16(add16(idiv16(so.extent, 2), so.y), idiv16(po.extent, 2));
    po.y = add16(po.y, imod16(idiv16(mul16(heading_dy(h8), so.extent), 16), wrap));
    po.y = imod16(po.y, wrap);
    // The velocity, the heading from pool 14 by its literal pointer DGROUP:0x1e30
    // [BYTES seg13:0x4e4f-0x4e95].
    po.vx = so.vx;
    po.vy = so.vy;
    {
        const ObjectRecord& s14 = pools.records(kPoolShips) > ship ? pools.rec(kPoolShips, ship) : so;
        const int hv = (static_cast<std::uint8_t>(s14.frame & 0xff) + 8) & 15;
        po.vx = add16(po.vx, mul16(heading_dx(hv), 5));
        po.vy = add16(po.vy, mul16(heading_dy(hv), 5));
    }
    po.mass = 0x65;       // [BYTES seg13:0x4e9d]
    po.b53 = 0;           // `capturable` [BYTES seg13:0x4ea7]
    po.race_status = 0;   // +0x51 [BYTES seg13:0x4eb0]
    c.host.seg7_2080(kPodPool, -1);  // [BYTES seg13:0x4eb9]
}

// seg13:0x498e [BYTES seg13:0x498e-0x4ac3]
void drop_pod(EffectCtx& c, int ship, int flag) {
    World& w = c.w;
    Pools& pools = w.pools;
    const int n = pools.allocated(kPodPool);  // DGROUP:0x9302
    int rec = -1;
    for (int i = 0; i < n && i < pools.records(kPodPool); ++i) {  // [BYTES seg13:0x49a9-0x49d6]
        if (pools.rec(kPodPool, i).state != 0) {
            fill_pod(c, ship, i, flag);
            rec = i;
            break;
        }
    }
    if (rec < 0) {  // [BYTES seg13:0x49e1-0x4a43]
        std::int16_t best = 0;
        int pick = -1;
        for (int i = 0; i < n && i < kPodRecords; ++i) {
            const std::int16_t a = static_cast<std::int16_t>(c.st.pods[ix(i)].age.lo);  // word DGROUP:0x8564
            if (a > best) {
                best = a;
                pick = i;
            }
        }
        if (pick < 0) return;  // the original's index is unset [DESIGN ADR 0016 decision 1]
        c.host.destroy(kPodPool, pick);  // the record's +0x66 [BYTES seg13:0x4a1d]
        fill_pod(c, ship, pick, flag);
        rec = pick;
        c.host.seg7_2080(kPodPool, -1);  // [BYTES seg13:0x4a3e]
    }
    if (flag == 0) return;  // [BYTES seg13:0x4a46]
    for (int k = 0; k <= w.last_ship && k < kShipCount; ++k) {  // [BYTES seg13:0x4a4c-0x4ab8]
        ShipRecord& s = w.ships[ix(k)];
        if (s.bec == 0) continue;
        const std::int16_t kind = c.host.design_kind(s.design_index);  // DGROUP:0x970a
        const bool hunter = (kind == 7 && static_cast<std::int8_t>(s.w5e & 0xff) > 1) || kind == 3;
        if (!hunter) continue;
        s.w52 = 0;                                // [BYTES seg13:0x4a95]
        s.w32 = 11;                               // [BYTES seg13:0x4a9e]
        s.w06 = 11;                               // [BYTES seg13:0x4aa2]
        s.w08 = static_cast<std::int16_t>(rec);  // [BYTES seg13:0x4aa9]
        s.w34 = static_cast<std::int16_t>(rec);  // [BYTES seg13:0x4aad]
        s.w38 = 2;                                // [BYTES seg13:0x4ab4]
        s.w3c = 2;                                // [BYTES seg13:0x4ab8]
        return;
    }
}

}  // namespace is::world
