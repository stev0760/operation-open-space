// Pool 1, the icon fragments (fragments.h).
#include "world/fragments.h"

#include "world/update.h"

namespace is::world {

void fragment_offscreen_step(UpdateCtx& c, int i) {
    const std::int16_t pr = pool_rec(1, i);
    limit_speed(c.flight.pools, 0x28, pr);  // [BYTES seg7:0x3417]
    integrate(c.flight, 3, pr);             // [BYTES seg7:0x341f]
}

// seg7:0x32d0 [BYTES seg7:0x32d0-0x33f0]
void move_fragments(UpdateCtx& c, World& w) {
    MoverScope scope(c.host, c);
    Pools& pools = w.pools;
    UpdateHost& h = c.host;
    const std::int16_t g6 = sar16(shl16(6, w.size & 15), 1);  // u(6) [BYTES seg7:0x32df-0x32ea]
    const int n = pools.in_use(1) < pools.records(1) ? pools.in_use(1) : pools.records(1);  // DGROUP:0x9408
    for (int i = 0; i < n; ++i) {
        ObjectRecord& o = pools.rec(1, i);
        const std::int16_t pr = pool_rec(1, i);
        if (o.state != 0) continue;  // [BYTES seg7:0x3323-0x3328]
        if (o.on_screen == 0 || !h.fragment_has_bitmap(i)) {  // [BYTES seg7:0x332a-0x333b]
            if (imod16(c.f.dg_9142, 3) == 0) fragment_offscreen_step(c, i);  // [BYTES seg7:0x32f4-0x3305]
            continue;
        }
        o.src_x = g6;                    // src_y is `gap`, u(6), already [BYTES seg7:0x3353-0x3356]
        limit_speed(pools, 0x28, pr);    // [BYTES seg7:0x335a-0x3366]
        integrate(c.flight, 1, pr);      // [BYTES seg7:0x336e]
        h.draw_fragment(pr, i);          // SelectObject, seg8:0xb76(h, 3, DGROUP:0x9288, 0), back [BYTES seg7:0x333d-0x3388]
        if (o.w1c != 0) continue;        // [BYTES seg7:0x3391-0x3396]
        c.f.dg_943a = 1;
        c.f.dg_943c = static_cast<std::int16_t>(i);
        const std::uint16_t hit = h.collide(pr);  // [BYTES seg7:0x33a6-0x33b0]
        if (hit == 0xffffu) continue;
        CollideCtx x = frame_collide_ctx(c, w);
        // seg8:0x546c(hit, &vx, &vy, extent + 2, mass) [BYTES seg7:0x33b4-0x33d6]
        const std::int16_t a = respond(x, o.mass, add16(o.extent, 2), &o.vy, &o.vx, hit);
        dispatch(x, 1, i, hit >> 8, hit & 0xff, a);  // [BYTES seg7:0x33db-0x33f0]
    }
}

}  // namespace is::world
