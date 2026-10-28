// seg10:0x3a2e, the rock images (rock_images.h). (M3 s2 fixer)
#include "world/rock_images.h"

namespace is::world {

namespace {
constexpr int kBig = 0x2b;    // [bp-0x30] = 43 [BYTES seg10:0x3a46]
constexpr int kSmall = 0x15;  // [bp-0x34] = 21 [BYTES seg10:0x3a4b]
constexpr int kEdge = 5;      // [bp-0x32] = 5 [BYTES seg10:0x3a4b]
constexpr int kFrames = 0x12; // 18 frames [BYTES seg10:0x3cb3]
}  // namespace

void build_rock_images(Blitter& out, ImageSlotTable& images, std::int16_t& dg_740) {
    auto slot = [&images](int s) -> DcId& { return images[static_cast<std::size_t>(s)]; };
    if (slot(kSlotRockMaskBig) != kDcNone) return;  // [BYTES seg10:0x3a3d-0x3a42]
    // DGROUP:0x90f0 != 0: #4000 50 in mode 3 [BYTES seg10:0x3a61-0x3a68].
    const DcId colour = out.load_graphic(0x32, 3, -1);
    dg_740 = 0;                                  // [BYTES seg10:0x3aa1]
    const DcId rocks = out.load_bitmap("ROCKS");  // seg10:0x6d38(DGROUP:0x745) [BYTES seg10:0x3aa8]
    // 0x13·5 + 0x12·43 = 869 by 2·5 + 43 = 53, and 0x13·5 + 0x12·21 = 473 by 31 [BYTES
    // seg10:0x3acd-0x3b36].
    const int big_w = kEdge * 0x13 + kBig * kFrames, big_h = 2 * kEdge + kBig;
    const int small_w = kEdge * 0x13 + kSmall * kFrames, small_h = 2 * kEdge + kSmall;
    for (int k = 0; k < 2; ++k) {  // slots 6, 7 [BYTES seg10:0x3b3e-0x3bae]
        slot(kSlotRockMaskBig + k) = out.create_image(big_w, big_h);
        out.pat_blt(slot(kSlotRockMaskBig + k), 0, 0, big_w, big_h, kBlackness);
    }
    for (int k = 0; k < 2; ++k) {  // slots 8, 9 [BYTES seg10:0x3bb5-0x3c27]
        slot(kSlotRockMaskSmall + k) = out.create_image(small_w, small_h);
        out.pat_blt(slot(kSlotRockMaskSmall + k), 0, 0, small_w, small_h, kBlackness);
    }
    // The ROCKS frames, then the 21 x 21 copies by seg10:0x9962's StretchBlt (43 is not twice
    // 21) [BYTES seg10:0x3c35-0x3ca0].
    for (int k = 0; k < kFrames; ++k) {
        out.bit_blt(slot(kSlotRockMaskBig), k * 0x30 + kEdge, kEdge, kBig, kBig, rocks, k * kBig, 0,
                    kSrcCopy);
        out.set_stretch_mode(slot(kSlotRockMaskSmall), 2);
        out.stretch_blt(slot(kSlotRockMaskSmall), k * 0x1a + kEdge, kEdge, kSmall, kSmall, rocks,
                        k * kBig, 0, kBig, kBig, kSrcCopy);
    }
    // The colour frames [BYTES seg10:0x3cbf-0x3d40].
    for (int k = 0; k < kFrames; ++k) {
        out.bit_blt(slot(kSlotRockBig), k * 0x30 + kEdge, kEdge, kBig, kBig, colour, k * kBig, 0,
                    kSrcCopy);
        out.set_stretch_mode(slot(kSlotRockSmall), kColorOnColor);
        out.stretch_blt(slot(kSlotRockSmall), k * 0x1a + kEdge, kEdge, kSmall, kSmall, colour,
                        k * kBig, 0, kBig, kBig, kSrcCopy);
    }
    // The sources are deleted [BYTES seg10:0x3e0a-0x3e5c]; the Blitter keeps no handle the
    // slots do not hold, so nothing else is freed here. seg10:0x6308 [OPEN].
}

}  // namespace is::world
