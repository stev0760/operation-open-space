// seg10:0x3a2e, the rock images (rock_images.h). (M3 s2 fixer)
#include "world/rock_images.h"

namespace is::world {

namespace {
constexpr int kBig = 0x2b;    // [bp-0x30] = 43 [BYTES seg10:0x3a46]
constexpr int kSmall = 0x15;  // [bp-0x34] = 21 [BYTES seg10:0x3a4b]
constexpr int kEdge = 5;      // [bp-0x32] = 5 [BYTES seg10:0x3a4b]
constexpr int kFrames = 0x12; // 18 frames [BYTES seg10:0x3cb3]
}  // namespace

namespace {

void build_rocks(Blitter& out, ImageSlotTable& images, std::int16_t& dg_740) {
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

WaveGraphics g_wave_graphics;

}  // namespace

WaveGraphics& wave_graphics() { return g_wave_graphics; }

// [BYTES seg10:0x4bd9 seg10:0x4bcc seg10:0x4bbf seg10:0x4bb2 seg10:0x4ba5 seg10:0x4b98
// seg10:0x4b8b seg10:0x4b7e seg10:0x4b71]: DGROUP:0x3b76 ... 0x3b86.
const std::array<std::uint32_t, 10> kHeatColours = {
    0,          0x0040407Fu, 0x0030307Fu, 0x002020B0u, 0x001020C0u,
    0x000020D0u, 0x000030E0u, 0x000040EAu, 0x000060FFu, 0x000072FFu};
// [BYTES seg10:0x4d4d seg10:0x4d5b seg10:0x4d69 seg10:0x4d77 seg10:0x4d84]: DGROUP:0x3b8a ...
// 0x3b92 (plain RGB: 00007f, 007fc4, 00c4ff, 00ffff, ffffff as RRGGBB).
const std::array<std::uint32_t, 6> kBeamColours = {0,           0x007F0000u, 0x00C47F00u,
                                                   0x00FFC400u, 0x00FFFF00u, 0x00FFFFFFu};
// [BYTES seg10:0x4fe5 seg10:0x4ff3 seg10:0x5001 seg10:0x500f seg10:0x501d]: DGROUP:0x3b60 ...
// 0x3b68 (PALETTERGB 00ffff, 00d7e6, 00afcd, 0082a0, 006482 as RRGGBB).
const std::array<std::uint32_t, 15> kStarColours = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x02FFFF00u, 0x02E6D700u, 0x02CDAF00u, 0x02A08200u, 0x02826400u};
const std::uint8_t kMaskRows[8] = {0x82, 0x10, 0x45, 0x10, 0x84, 0x20, 0x02, 0x90};
const std::uint8_t kCheckerRows[8] = {0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55};

void build_rock_images(Blitter& out, ImageSlotTable& images, std::int16_t& dg_740) {
    build_rocks(out, images, dg_740);
}

void build_wave_images(Blitter& out, ImageSlotTable& images, std::int16_t& dg_740) {
    build_rocks(out, images, dg_740);
    build_gate_images(out, images, wave_graphics());  // seg9:0x26e5 [BYTES seg9:0x26e5]
    create_wave_brushes(out, wave_graphics());
}

void build_gate_images(Blitter& out, ImageSlotTable& images, WaveGraphics& wg) {
    auto slot = [&images](int s) -> DcId& { return images[static_cast<std::size_t>(s)]; };
    if (slot(kSlotGate) != kDcNone) return;  // [BYTES seg10:0x3eb1]
    constexpr int c = kGateCell, g = kGateGap;
    for (int k = 0; k < 2; ++k) {
        DcId a = kDcNone, b = kDcNone;
        if (k == 0) {
            // CreateCompatibleBitmap(DGROUP:0x13dc, 166, 46); #4000 210 in mode 3 (the 34 x 34
            // post) and #4000 820 in mode 0 (two 34 x 34 cells) [BYTES seg10:0x40fb seg10:0x4127
            // seg10:0x4138]. The palette DGROUP:0x920e selected into them is the renderer's
            // (ADR 0007).
            slot(kSlotGate) = out.create_image(kGateImageW, kGateImageH);
            a = out.load_graphic(210, 3, -1);
            b = out.load_graphic(820, 0, -1);
        } else {
            // CreateBitmap(166, 46, 1, 1, NULL); BW_BLOCKS and BW_MBLOCKS through seg10:0x6d38
            // (ISPACE.DLL ordinal 3, the bitmap loader) [BYTES seg10:0x3ec3 seg10:0x3ed1
            // seg10:0x3efc]; the 1-bpp bitmap is an index image of the renderer's, as the rock
            // masks (rock_images.h) [DESIGN ADR 0013]. Blitter::load_bitmap reads the INSPACE.EXE
            // resource; whether ordinal 3 maps its colours further is [OPEN].
            slot(kSlotGateMask) = out.create_image(kGateImageW, kGateImageH);
            a = out.load_bitmap("BW_BLOCKS");
            b = out.load_bitmap("BW_MBLOCKS");
        }
        const DcId d = slot(kSlotGate + k);
        out.pat_blt(d, 0, 0, kGateImageW, kGateImageH, kBlackness);  // [BYTES seg10:0x3f79]
        // The j loop runs twice with the same calls [BYTES seg10:0x3fcd seg10:0x3ff7
        // seg10:0x4023 seg10:0x402f].
        for (int j = 0; j < 2; ++j) {
            out.bit_blt(d, g, g, c, c, a, 0, 0, kSrcCopy);
            out.bit_blt(d, 2 * c + 3 * g, g, c, c, b, 0, 0, kSrcCopy);
            out.bit_blt(d, 4 * g + 3 * c, g, c, c, b, c, 0, kSrcCopy);
        }
    }
    for (int k = 0; k < 2; ++k) {
        const DcId d = slot(kSlotGate + k);
        // seg10:0x9962 (StretchBlt) in COLORONCOLOR: cell 1 = cell 0 mirrored, the negative
        // width from x 2(g + c) - 1 = 79 covering columns 46 ... 79 [BYTES seg10:0x4185
        // seg10:0x41b8].
        out.set_stretch_mode(d, kColorOnColor);
        out.stretch_blt(d, 2 * (g + c) - 1, g, -c, c, d, g, g, c, c, kSrcCopy);
        // The second call, (79, g, 5 - c, c) from (2g + c + 4, g, c - 5, c), reads and writes
        // the same cell [BYTES seg10:0x41f0]. Read as copy-then-write, column 46 + i (i = 5 ...
        // 33) takes graphic column i - 4, which is cell 0's column g + i - 4: the port makes it
        // the non-overlapping BitBlt of cell 0's columns 1 ... 29 to 51 ... 79, so no GDI's
        // overlap rule is involved [DESIGN] [ORACLE wave-0001 06-fly-up.png: the right post
        // equals this but for the 8 pixels the beam erase notches] [OPEN GDI's own overlap
        // behaviour].
        out.bit_blt(d, 2 * g + c + 5, g, c - 5, c, d, g + 1, g, kSrcCopy);
    }
    // OUT_SIGN frame 1 on cells 0 and 1 of slot 1 only [BYTES seg10:0x4202 seg10:0x4262
    // seg10:0x4288].
    wg.gate_dc = slot(kSlotGate);
    wg.out_sign = out.load_bitmap("OUT_SIGN");
    out.bit_blt(slot(kSlotGate), g + 2, g + 10, kOutSignW, kOutSignH, wg.out_sign, kOutSignW, 0,
                kSrcCopy);
    out.bit_blt(slot(kSlotGate), 2 * (g + 3) + c, g + 10, kOutSignW, kOutSignH, wg.out_sign,
                kOutSignW, 0, kSrcCopy);
}

void create_wave_brushes(Blitter& out, WaveGraphics& wg) {
    if (wg.brushes) return;
    wg.brushes = true;
    for (std::size_t n = 1; n < kHeatColours.size(); ++n) wg.heat[n] = out.create_solid_brush(kHeatColours[n]);
    for (std::size_t n = 1; n < kBeamColours.size(); ++n) wg.beam[n] = out.create_solid_brush(kBeamColours[n]);
    for (std::size_t n = 10; n < kStarColours.size(); ++n) wg.star[n] = out.create_solid_brush(kStarColours[n]);
    wg.mask = out.create_pattern_brush(kMaskRows);        // [BYTES seg10:0xe4 seg10:0xf2]
    wg.checker = out.create_pattern_brush(kCheckerRows);  // [BYTES seg10:0x10a]
}

}  // namespace is::world
