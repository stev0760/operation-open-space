// M4 stage 3, track AMBFLIGHT: the pick-up composite seg13:0x3d40 (src/world/draw.h
// pickup_composite; s3-ambflight.md 4.2, claims 45-48 unverified [OPEN]): the scaled size, the
// separate and the overlapping paths, the mask, and the two sprite draws from the buffers.
#include <cstdint>

#include "doctest.h"
#include "world/draw.h"

using namespace is::world;

namespace {

struct DrawRig {
    Pools pools;
    ShipTable ships{};
    EffectTable effects{};
    ImageSlotTable images{};
    DrawGlobals g;
    RecordingBlitter out;
    DrawContext c{pools, ships, effects, images, g, out,
                  [](std::int16_t x, std::int16_t y) { return Point16{static_cast<std::int16_t>(x >> 3), static_cast<std::int16_t>(y >> 3)}; }};
    DrawRig() {
        g.sprite.window_dc = kDcWindow;
        g.sprite.ship_pool = kPoolShips;
        g.sprite.player = 0;
        g.sprite.dg_952e = 0;
        g.sprite.dg_9e49 = 3;
        ObjectRecord& S = pools.rec(kPoolShips, 0);
        S.size = 32, S.gap = 6, S.frame = 2, S.w3a = 0x50, S.alt_dc = 0x51, S.owner_pool = -1;
        ObjectRecord& A = pools.rec(7, 0);
        A.size = 40, A.gap = 6, A.frame = 5, A.w3a = 0x60, A.alt_dc = 0x61, A.owner_pool = -1;
        A.on_screen = 1;
    }
};

}  // namespace

TEST_CASE("seg13:0x3d40: apart, each buffer from its own cell, the ship stretched") {
    DrawRig r;
    ObjectRecord& S = r.pools.rec(kPoolShips, 0);
    ObjectRecord& A = r.pools.rec(7, 0);
    S.x = 8000, S.y = 8000;
    A.x = 8000 + 8 * 100, A.y = 8000;  // 100 px right: no overlap
    pickup_composite(r.c, 150, 100, 0, 7, 0, kPoolShips);
    // s2 = lo16(32·100) / 150 = 21, off = 6 + (32 - 21) / 2 = 11
    const auto& k = r.out.calls;
    REQUIRE(k.size() >= 6);
    CHECK(k[0] == pat_blt_call(0x61, 0, 0, 52, 52, kBlackness));
    CHECK(k[1] == bit_blt_call(0x61, 6, 6, 40, 40, 0x60, 5 * 46 + 6, 6, kSrcCopy));
    CHECK(k[2].op == BlitCall::kStretchMode);
    CHECK(k[3] == pat_blt_call(0x51, 0, 0, 44, 44, kBlackness));
    CHECK(k[4].op == BlitCall::kStretchBlt);
    CHECK(k[4].dst == 0x51);
    CHECK(k[4].x == 11);
    CHECK(k[4].w == 21);
    CHECK(k[4].sx == 2 * 38 + 6);
    CHECK(k[4].sw == 32);
    // The ship's on_screen is left 0. Both draws take the buffers (state 3: seg8:0xb76's
    // alternate source, which resets src_x to the gap).
    CHECK(S.on_screen == 0);
    CHECK(S.src_x == 6);
    CHECK(A.src_x == 6);
    CHECK(k.back() == bit_blt_call(kDcWindow, A.scr_x, A.scr_y, 40, 40, 0x61, 6, 6, kSrcCopy));
}

TEST_CASE("seg13:0x3d40: overlapping, the mask and both buffers") {
    DrawRig r;
    ObjectRecord& S = r.pools.rec(kPoolShips, 0);
    ObjectRecord& A = r.pools.rec(7, 0);
    S.x = 8000, S.y = 8000;
    A.x = 8000 + 8 * 10, A.y = 8000;  // 10 px right: ox = 10, oy = 0
    pickup_composite(r.c, 100, 100, 0, 7, 0, kPoolShips);  // full size
    const auto& k = r.out.calls;
    // a) A's cell under S: dx_ = 10, w = min(32 - 10, 52) = 22, h = min(32, 52) = 32
    CHECK(k[0] == pat_blt_call(0x51, 0, 0, 44, 44, kBlackness));
    CHECK(k[1] == bit_blt_call(0x51, 16, 6, 22, 32, 0x60, 236, 6, kSrcCopy));
    // b) M and K: one image and one mask made, both deleted
    REQUIRE(r.out.images.size() == 2);
    const DcId M = r.out.images[0].id, K = r.out.images[1].id;
    CHECK(r.out.images[1].name == "mask");
    CHECK(k[3] == bit_blt_call(M, 6, 6, 32, 32, 0x50, 82, 6, kSrcCopy));
    CHECK(k[4] == bit_blt_call(0x51, 6, 6, 32, 32, K, 6, 6, kSrcAnd));
    CHECK(k[5] == bit_blt_call(0x51, 6, 6, 32, 32, M, 6, 6, kSrcPaint));
    // c) S over A's buffer: mx = 10, ax_ = 0, w = min(52, 22)
    CHECK(k[8] == bit_blt_call(0x61, 6, 6, 22, 32, K, 16, 6, kSrcAnd));
    CHECK(k[9] == bit_blt_call(0x61, 6, 6, 22, 32, M, 16, 6, kSrcPaint));
    CHECK(r.out.deleted.size() == 2);
}

TEST_CASE("seg13:0x3d40: no buffer, nothing drawn") {
    DrawRig r;
    r.pools.rec(7, 0).alt_dc = 0;
    pickup_composite(r.c, 100, 100, 0, 7, 0, kPoolShips);
    CHECK(r.out.calls.empty());
}
