// seg8:0x4e14, the visibility pass (visibility.h). (M3 s2 fixer)
#include "world/visibility.h"

namespace is::world {

namespace {

// USER.PtInRect(rect, POINT{+0x04, +0x06}): half-open, signed 16-bit [REF Win16 SDK]
// [BYTES seg8:0x4e59-0x4e7c].
bool in_rect(const ViewRect& r, std::int16_t x, std::int16_t y) {
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

void list(VisibilityGlobals& g, int pool, int rec) {
    // DGROUP:0x507c + n = pool, DGROUP:0x50e0 + n = rec, n = DGROUP:0x9146++ [BYTES
    // seg8:0x4e8f-0x4ea1 seg8:0x4fc6-0x4fd5].
    if (g.count >= 0 && g.count < kVisibleMax) {
        g.pool[static_cast<std::size_t>(g.count)] = static_cast<std::uint8_t>(pool);
        g.rec[static_cast<std::size_t>(g.count)] = static_cast<std::uint8_t>(rec);
    }
    g.count = static_cast<std::int16_t>(g.count + 1);
}

}  // namespace

namespace {
const VisibilityGlobals* g_last = nullptr;
}  // namespace

const VisibilityGlobals* last_visibility() { return g_last; }
void set_last_visibility(const VisibilityGlobals* g) { g_last = g; }

void visibility(Pools& pools, const ViewState& view, VisibilityGlobals& g, shell::Rng& rng,
                const VisibilityHost& host) {
    if (g.dg_9130 == 0) {
        // [bp-0x18]: 1, or 0 outside DGROUP:0x9e49 = 3 ... 6 [BYTES seg8:0x4e23-0x4e40].
        int special = (g.dg_9e49 > 6 || g.dg_9e49 < 3) ? 0 : 1;
        for (int p = g.radar_first; p <= g.ship_pool; ++p) {  // [BYTES seg8:0x4f67-0x4f72]
            if (special != 0 && p == 7) continue;              // [BYTES seg8:0x4f75-0x4f7f]
            if (p == 0x0e && special != 0) special = 2;        // [BYTES seg8:0x4f81-0x4f8d]
            if (p < 0 || p >= kPoolCount) continue;
            for (int i = 0; i < pools.in_use(p) && i < pools.records(p); ++i) {
                ObjectRecord& o = pools.rec(p, i);
                // [BYTES seg8:0x4f30-0x4f5a]
                if (o.state != 0 || o.mass == 0) continue;
                if (p == 8 && host.pool8_3d80 && host.pool8_3d80(i) == 4) continue;
                if (special >= 2 && i == g.dg_952e) continue;
                bool found = false;  // [bp-0x16] [BYTES seg8:0x4f5c-0x4f61]
                for (int k = 0; k < view.active_count && k < static_cast<int>(view.active.size());
                     ++k) {
                    if (in_rect(view.active[static_cast<std::size_t>(k)], o.x, o.y)) {
                        o.w1c = 0;  // [BYTES seg8:0x4e89]
                        list(g, p, i);
                        found = true;
                        break;
                    }
                }
                if (found) {
                    o.on_screen = 1;  // [BYTES seg8:0x4ebe]
                } else if (o.on_screen > 0) {
                    o.on_screen = -1;  // [BYTES seg8:0x4f0a]
                } else {
                    o.on_screen = 0;  // [BYTES seg8:0x4edb-0x4ef4]
                    o.scr_x = -1000;
                    o.scr_y = -1000;
                }
            }
        }
    } else {
        // The seg18 mode: every record of the pools, listed and on screen [BYTES
        // seg8:0x4f9a-0x4ff9].
        for (int p = g.radar_first; p <= g.ship_pool; ++p) {
            if (p < 0 || p >= kPoolCount) continue;
            for (int i = 0; i < pools.in_use(p) && i < pools.records(p); ++i) {
                ObjectRecord& o = pools.rec(p, i);
                o.w1c = 0;
                list(g, p, i);
                o.on_screen = 1;
            }
        }
    }
    // The discharge loop (s7-ring-verify.md O7): rand() % 9 = 0 (`cwd; idiv 9`, `or dx, dx`)
    // [BYTES seg8:0x4ffb-0x500a].
    if (rng.rand() % 9 != 0) return;
    // i < DGROUP:0x9146, re-read each turn: the strike cannot change the list [BYTES
    // seg8:0x5060-0x5069]. The bytes are sign-extended (`cbw`); the port bounds the list and the
    // records [DESIGN].
    const int n = g.count < kVisibleMax ? g.count : kVisibleMax;
    for (int a = 0; a < n; ++a) {
        const int pa = static_cast<std::int8_t>(g.pool[static_cast<std::size_t>(a)]);
        const int ra = static_cast<std::int8_t>(g.rec[static_cast<std::size_t>(a)]);
        if (pa < 0 || pa >= kPoolCount || ra < 0 || ra >= pools.records(pa)) continue;
        // Byte +0x50 != 0 and pool != DGROUP:0x9324, no kind filter (claim 27) [BYTES
        // seg8:0x508b-0x5096].
        if (pools.rec(pa, ra).b50 == 0 || pa == g.ship_pool) continue;
        // j from 0: the first listed j != i whose object has `state` 0 (the test reads the
        // state before the index) [BYTES seg8:0x5098 seg8:0x5012-0x5047].
        for (int b = 0; b < n; ++b) {
            const int pb = static_cast<std::int8_t>(g.pool[static_cast<std::size_t>(b)]);
            const int rb = static_cast<std::int8_t>(g.rec[static_cast<std::size_t>(b)]);
            if (pb < 0 || pb >= kPoolCount || rb < 0 || rb >= pools.records(pb)) continue;
            if (pools.rec(pb, rb).state != 0 || b == a) continue;  // [BYTES seg8:0x503d-0x5047]
            // seg11:0x9fdc(pool[i], rec[i]), cdecl (`push rec; push pool`, `add sp, 4`), once per
            // qualifying i [BYTES seg8:0x5049-0x505d].
            if (host.seg11_9fdc) host.seg11_9fdc(pa, ra);
            break;
        }
    }
}

}  // namespace is::world
