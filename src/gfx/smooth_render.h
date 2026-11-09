// ISPACE.DLL ordinal 12 (seg6:0xc4), the smoothed 2x renderer, and its worker ISPACE.DLL
// seg7:0x12d8 (seg7:0x12d8-0x4ee5) with the ramp builder ISPACE.DLL seg7:0x4ee8, as INSPACE.EXE
// calls them through seg10:0x73a6 for the title emblem (seg15:0x757c), the desk launch's pad ship
// (seg15:0x2b8f in seg15:0x2a08) and the visit's landing ship (seg15:0x2165 in seg15:0x20ca)
// (work/analysis/m4/s6-ord12.md sections 1-2, all 48 claims CONFIRMED in s6-ord12-verify.md, its
// Omissions 29, 30 and 41 kept here). The routine turns each colour of a 4-bpp graphic into a
// brightness, builds two 2x brightness layers (colours 0-9 and 10-14) with edge and corner terms,
// blurs them with the diagonals, cuts the staircases, and maps each brightness back to an entry
// of its colour's ramp with a linear guess and seg5:0x6b72's walk. The result is a picture of hPal
// indices: 0 (nothing drawn), 0xf5 (too dark for its ramp), or a ramp entry.
//
// Portable and pure except smooth_image (Win32). The branch ported is ISPACE.DLL DGROUP:0x1e9a >= 8
// (the port's 8-bpp device, ADR 0007): the output scale k is 1, so the picture is 2H square. Not
// ported [DESIGN]: below 8 bpp the runs for every scheme with lo/hi from the run's own colours
// (seg7:0x170e, seg7:0x1c40, seg7:0x1754-0x1a3f, seg7:0x1cad-0x1f98), the blur as a copy
// (seg7:0x34a2), k = 2 for id != 300 (seg7:0x3e05) and the 24-bit error diffusion (seg7:0x3f1b,
// seg7:0x3f25-0x450f, seg7:0x4b61, seg7:0x46b8-0x4b20); GetCurrentTime's unread difference
// (seg7:0x139d, seg7:0x4d60) and the WaveMixPump calls of seg9:0x360a once per row (seg7:0x30c5,
// seg7:0x3df9) [BYTES ISPACE.DLL seg9:0x3614-0x3622]; seg6:0x7fc8 for a graphic that took the
// fixed 16-colour set (DGROUP:0x1ea8, seg7:0x13aa-0x13bd), which no graphic of the distribution
// reaches [OPEN: unread].
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

#include "formats/dib.h"
#include "gfx/gfx.h"
#include "gfx/palette.h"

namespace is::gfx {

// The arguments seg7:0x12d8 reads (ordinal 12 is pascal, `retf 0x18`: hPal, id, lib, hdc, w1 ...
// w5, b1, b2 [BYTES ISPACE.DLL seg6:0xce-0x118]; s6-ord12.md 1.4).
struct SmoothArgs {
    int w1 = -2;  // [bp+0x12] output width: negative becomes lo16(-H w1), w2 likewise (only w1's
                  // sign is tested) [BYTES ISPACE.DLL seg7:0x143e-0x1455]
    int w2 = -2;  // [bp+0x10] output height
    int w3 = 2;   // [bp+0xe] 1 adds L sar 2 to the half level [BYTES ISPACE.DLL seg7:0x2062-0x206d
                  // seg7:0x2527-0x2532]; the quarter shift is 2 for every value [BYTES ISPACE.DLL
                  // seg7:0x1458-0x147c]
    int w4 = 0;   // [bp+0xc] 0 clears DGROUP:0x1ccc, ordinal 2's direction count N [BYTES
                  // ISPACE.DLL seg7:0x130a-0x1310]; the bitmap is k w1 (N + 1) wide
    int w5 = 1;   // [bp+0xa] never read [BYTES ISPACE.DLL seg7:0x12d8-0x4ee5]
    int b1 = 12;  // [bp+8] byte: the scheme row of colours 0-9 (SHP +0x07; 12 keeps the graphic's)
    int b2 = 12;  // [bp+6] byte: the scheme row of colours 10-14 (SHP +0x08)
};
// The arguments of all three INSPACE calls, (-2, -2, 2, 0, 1, b1, b2) [BYTES seg15:0x7567-0x758c
// seg15:0x2b75-0x2b97 seg15:0x2137-0x216d] (verified claims 2-4).
SmoothArgs smooth_args(int b1, int b2);

// One group of a layer, as seg7:0x4ee8 stores it: the ramp's first and last entry and the
// brightness of those two entries (T1 of lo, T0 of hi). Layer 0 in ISPACE.DLL DGROUP:0x1108,
// 0x111c, 0x1130, 0x1144 (10 words each), layer 1 in DGROUP:0x1ca4, 0x1cae, 0x1cb8, 0x1cc2 (5
// words each) [BYTES ISPACE.DLL seg7:0x5102 seg7:0x5124 seg7:0x5245 seg7:0x536b seg7:0x5495
// seg7:0x54b7 seg7:0x55d8 seg7:0x56f8].
struct SmoothGroup {
    int lo = 0, hi = 0, t1 = 0, t0 = 0;
    bool operator==(const SmoothGroup& o) const {
        return lo == o.lo && hi == o.hi && t1 == o.t1 && t0 == o.t0;
    }
};

// The palette side of seg7:0x12d8: hPal's working entries and kind, the two colour tables and
// the animate call.
struct SmoothPalette {
    PaletteArray* working = nullptr;  // DGROUP:0x1168 = DGROUP:0x116c[slot] + 4
    int kind = 0;                     // DGROUP:0x1e92[slot]: 0 the game palette
    // DGROUP:0x117c (18 rows of 10) and DGROUP:0x1cdc (18 rows of 5) [BYTES ISPACE.DLL
    // seg7:0x505d seg7:0x53f0]; null: the constant tables kSchemes10 / kSchemes5.
    const std::array<Scheme10, kSchemeRows>* schemes10 = nullptr;
    const std::array<Scheme5, kSchemeRows>* schemes5 = nullptr;
    // seg5:0x4378(DGROUP:0x1cd6[slot], 0, DGROUP:0x1a32), command 3 (may be empty).
    std::function<void()> animate;
};

// ISPACE.DLL seg7:0x4ee8(slot, c, g, count, first), cdecl [BYTES ISPACE.DLL seg7:0x4ee8-0x5bb2]
// (s6-ord12.md 2.4, verified claims 20-26):
//  - first >= 10 picks layer 1 and subtracts 10 [BYTES ISPACE.DLL seg7:0x4ef7-0x4f09]; a, b, e are
//    DGROUP:0x178, 0x17c, 0x180 by kind (byte offsets of entries 185, 213, 230 for kind 0; 10, 83,
//    112 for kind 1) [BYTES ISPACE.DLL seg7:0x4f0d-0x4f2a DGROUP:0x178 DGROUP:0x17c DGROUP:0x180];
//    kinds above 1 take kind 1's [DESIGN: the original reads past the table];
//  - c = -1 returns at once and stores nothing [BYTES ISPACE.DLL seg7:0x4f2d-0x4f33];
//  - mode 2 (layer 0, c >= 100): lo = a/4, hi = b/4 - 1; mode 3 (layer 1): lo = b/4, hi = e/4 - 1;
//    no palette write [BYTES ISPACE.DLL seg7:0x4f4e seg7:0x4fe9 seg7:0x5708-0x572c
//    seg7:0x595f-0x5983];
//  - mode 0 (layer 0, c < 100) writes working entries from DGROUP:0x117c row R (seg7:0x4fc8's
//    switch of c) over the byte offsets 4 trunc(lo16(D first) / 40) + a ... (exclusive) 4
//    trunc(lo16(D (first + count)) / 40) + a, D = b - a: entry i, channel j gets u8(T[x] +
//    trunc(lo16((T[x + 3] - T[x]) (t mod 10)) / 10)), x = 3 (t / 10 + 10 R) + j, t =
//    min(ldiv(3600 i, sext(lo16(D count))), 89); then lo = (trunc(lo16(D first) / 10) + a) / 4,
//    hi = (trunc(lo16(D (first + count)) / 10) + a) / 4 - 1 [BYTES ISPACE.DLL seg7:0x4ff6-0x5128];
//  - mode 1 (layer 1, c < 100): the same with D = e - b from b, 20 for 40, t = min(trunc(lo16(800
//    i) / lo16(D count)), 39), x = 3 (t / 10 + 5 R) + j in DGROUP:0x1cdc, and 5 for 10 [BYTES
//    ISPACE.DLL seg7:0x538f-0x54b7];
//  - every mode then measures T1 = the brightness of working entry lo and T0 = that of hi
//    (entry_brightness, B' = b >> 1) [BYTES ISPACE.DLL seg7:0x5128-0x536b seg7:0x5838-0x5ba6];
//    modes 0 and 1 then animate [BYTES ISPACE.DLL seg7:0x536f-0x5383 seg7:0x56fc].
// c from 14 to 99 leaves R unset in the original (the uninitialised local [bp - 0x12]); the port
// writes no entry there [DESIGN: no INSPACE call passes it; the run ids of seg5:0x66ea are 0-13].
// Returns the mode (0-3), or -1 for c = -1 with `out` unchanged.
int smooth_ramp(SmoothPalette& p, int c, int count, int first, SmoothGroup& out);

// Section 2.3 of s6-ord12.md: the brightness, ids and groups of the 15 colours, at depth >= 8.
struct SmoothTables {
    std::array<int, 16> L{};    // [bp - 0x166 + 2c]: colour_brightness, no +5; L[15] unset (0)
    std::array<int, 16> grp{};  // [bp - 0x146 + 2c]: the colour's group; grp[15] = 100
    std::array<int, 16> id{};   // [bp - 0x1da + 2c]: 100, then seg5:0x66ea or seg5:0x6f58
    std::array<std::vector<SmoothGroup>, 2> groups;  // by layer, indexed by group
    std::vector<int> modes;     // seg7:0x4ee8's mode for each call, in call order
};
// seg7:0x1541-0x1ffe [BYTES ISPACE.DLL seg7:0x1555-0x1ffe] with b1, b2 as they stand after the
// b := 100 rule (smooth_loaded applies it):
//  - colours 0-9 first: L and the ids (seg5:0x66ea for b1 = 12, colour_ramp_id; else seg5:0x6f58,
//    scheme_ramp_id), then b1 != 12: seg7:0x4ee8(b1 + 100, group 0, count 10, first 0), every
//    group 0 [BYTES ISPACE.DLL seg7:0x1708-0x174a]; b1 = 12: each run of equal consecutive ids
//    ends at colour i - 1 when id[i - 1] != id[i] with seg7:0x4ee8(id, group, run, i - run) and
//    the next group; colour 10's id is still 100 then, so colour 9 always ends a run [BYTES
//    ISPACE.DLL seg7:0x1a4c-0x1a99] (verified claim 19);
//  - then colours 10-14 likewise with b2 (seg7:0x4ee8(b2 + 100, 0, 5, 10) or the runs, groups
//    from 0 again; colour 15's id stays 100) [BYTES ISPACE.DLL seg7:0x1c3a-0x1c7d
//    seg7:0x1fa5-0x1ff5].
// The b compares are byte compares (`cmp byte ptr [bp + 8], 0xc`) and b + 100 sign-extends the
// byte (`cwde`). A group whose seg7:0x4ee8 call stored nothing (c = -1) reads the stale DGROUP
// words in the original; the port keeps it zero [DESIGN].
SmoothTables smooth_tables(const std::vector<std::uint32_t>& colours, int b1, int b2, SmoothPalette& p);

// The picture seg7:0x12d8 builds: top-down, one hPal index per pixel, side = 2 k H with k = 1.
struct SmoothPicture {
    int h = 0;      // biHeight, ISPACE.DLL DGROUP:0x1c72 [BYTES ISPACE.DLL seg7:0x1437-0x143b]
    int side = 0;   // 2 k H: the DIB seg5:0x23a4 allocates and StretchBlt's source extents
    std::vector<std::uint8_t> px;  // side x side, top-down (memory row side - 1 - r is row r)
    // CreateCompatibleBitmap(DGROUP:0x1a32, lo16(k w1 (N + 1)), lo16(k w2)) and the StretchBlt
    // destination (0, 0, k w1, k w2) [BYTES ISPACE.DLL seg7:0x4d98-0x4db8 seg7:0x4e2d-0x4e5e].
    int bm_w = 0, bm_h = 0;
    int dst_w = 0, dst_h = 0;
    int fixes = 0;  // staircase sub-pixels the fix rewrote (diagnostics)
    bool ok() const { return side > 0; }
    std::uint8_t at(int x, int y) const { return px[static_cast<std::size_t>(y) * side + x]; }
};

// Sections 2.5-2.8 of s6-ord12.md at depth >= 8 [BYTES ISPACE.DLL seg7:0x200d-0x30f2
// seg7:0x30f4-0x3516 seg7:0x3518-0x3e03 seg7:0x450f-0x46b5 seg7:0x4b20-0x4cb0]:
//  - the expansion: two passes (2 for colours 0-9, 3 for 10-14) over source columns and rows
//    0 ... H - 1 (a multi-frame strip renders frame 0) into planes P[2], P[3] and the class
//    buffer C, all (2H)^2 zero bytes [BYTES ISPACE.DLL seg7:0x1487-0x14ee]; nibble 15 writes class
//    100 into its 2x2 block in both passes; a pixel writes only in its own pass and only when
//    half = u8(L sar 1) (+ L sar 2 for w3 = 1) is not 0: class u8(2 (grp - 1) + pass), half to its
//    four bytes, q << (n - 1) (byte shift) to a corner with n facing neighbours of the other
//    layer inside the square, q = u8(L sar 2) to the two facing sub-pixels of each neighbour that
//    is neither the other layer nor 15 (Omissions 29 and 30);
//  - the blur: P[k] += s >> 1, s >> 2 and (n > 20 ? n : s) >> 4 for the four diagonals n of
//    s = P[k + 2] != 0, rows and columns 1 ... 2H - 2, byte adds (verified claims 32-33);
//  - the staircase of render_ship_sheet on P[0], P[1] for source x, y in 2 ... H - 3 (claim 35);
//  - the output: plane 0 when P0 > 2 and P1 < P0, else plane 1 when P1 > 2, else 0; the class
//    less 101 above 100, odd made 0 (layer 0) or even made 1 (layer 1), group = class >> 1; 0xf5
//    when 2v <= T0 (signed), else seg5:0x6b72(v, lo, hi, clamp(hi - ldiv((v - T0) (hi - lo),
//    T1 - T0))) with the clamp to lo first and then to hi (Omission 41).
// A group index past the layer's groups (a class byte of 100 or 201 on a lit layer-0 sub-pixel)
// reads other DGROUP words in the original [OPEN: their values]; the port takes group 0 [DESIGN].
// T1 = T0 faults seg3:0x130 in the original; the port takes a quotient of 0 [DESIGN]. Returns an
// empty picture for a graphic that is not 4 bpp [DESIGN: every graphic the callers pass is; the
// original rebuilds the header as 4 bpp, seg7:0x13fb-0x1424]. `dgroup_1ccc` is ordinal 2's N,
// used only when w4 != 0.
SmoothPicture render_smooth(const fmt::Dib& graphic, const SmoothTables& t, const PaletteArray& working,
                            const SmoothArgs& a, int dgroup_1ccc = 0);

// Ordinal 12 after its load: `loaded` is the graphic as seg5:0x1b8e left it (Gfx::graphic_dib(id,
// 0, b1, b2, kind != 0, true): its table recoloured, the ramps of seg5:0x26a2 written and
// animated, the flags DGROUP:0x1eaa/0x1eac set). Applies b1 := 100 when DGROUP:0x1eaa != 0 and
// b2 := 100 when DGROUP:0x1eac != 0 [BYTES ISPACE.DLL seg7:0x1541-0x1551], runs smooth_tables
// (modes 0 and 1 write pm's working entries and animate) and render_smooth, and animates at the
// end [BYTES ISPACE.DLL seg7:0x4ec9-0x4ed2]. The blend rows DGROUP:0x1e80[3 slot + 0, 1] end as
// the original leaves them: -1 on a kind-0 palette (seg7:0x1328-0x134a; the load writes them
// only when its last argument, kind != 0, is set). The caller has done seg6:0xc4's 0xff test.
// An unknown handle returns an empty picture [DESIGN: the original goes on with slot 3].
SmoothPicture smooth_loaded(const fmt::Dib& loaded, PaletteManager& pm, PalHandle pal, const SmoothArgs& a,
                            int dgroup_1ccc = 0);

// The whole ordinal 12 from the graphic as stored (`graphic`: the #4000 DIB, `id` its resource
// id): seg6:0xc4 returns 0 (an empty picture) when byte b1 or b2 is 0xff [BYTES ISPACE.DLL
// seg6:0xda seg6:0xe0]; the load seg5:0x1b8e(hPal, id, lib, 0, b1, b2, kind != 0) with DGROUP:0x188
// = 1 [BYTES ISPACE.DLL seg7:0x1367 seg7:0x1373-0x139a] is PaletteManager::recolour_graphic(pal,
// id, table, 0, b1, b2, kind != 0, true); then smooth_loaded. The pad ship of seg15:0x2a08 and
// seg15:0x20ca is smooth_for(store.sprite(BOD +0x21), BOD +0x21, pm, DGROUP:0x920e,
// smooth_args(SHP +0x07, SHP +0x08)).
SmoothPicture smooth_for(const fmt::Dib& graphic, int id, PaletteManager& pm, PalHandle pal, const SmoothArgs& a,
                         int dgroup_1ccc = 0);

// INSPACE's wrapper seg10:0x73a6(id, lib, w1 ... w5, b1, b2), cdecl, on the program's palette:
// hPal = DGROUP:0x920e (gfx.palette()), the manager gfx.palettes(), the graphic by Gfx::sprite's
// module rule of seg5:0x1b8e (220, 221 and 300 from ISPACE.DLL, every other id from IGRAPHIC.DLL)
// [BYTES seg10:0x73b0-0x73d6 ISPACE.DLL seg5:0x1c23 seg5:0x1c38] (verified claim 1), then
// smooth_for. The lib argument (BOD +0x23 for a body, "" for the title) is not passed: a body
// whose BOD +0x21 is 0xFFFF (fmt::kBodyBitmapFromFile) names a .DIB file the caller must load
// itself and hand to the Dib overload above [OPEN: seg5:0x1b8e's file path, M8]. Throws as
// Gfx::sprite does (no asset store, an id the module lacks).
SmoothPicture smooth_for(Gfx& gfx, int id, const SmoothArgs& a, int dgroup_1ccc = 0);

#ifdef _WIN32
// The bitmap seg7:0x12d8 returns, on the port's index images: seg5:0x2080 makes the picture a
// side x side image, CreateCompatibleBitmap(bm_w, bm_h) is Gfx::create_image, then
// SetStretchBltMode(COLORONCOLOR) and StretchBlt(dst, 0, 0, dst_w, dst_h, src, 0, 0, side, side,
// SRCCOPY) [BYTES ISPACE.DLL seg7:0x4d75-0x4e5e]. The palette selections of both DCs are not
// needed on index images (ADR 0007 decision 2) [DESIGN]. Null for an empty picture or a
// bitmap size that is not positive [DESIGN: CreateCompatibleBitmap fails there].
ImagePtr smooth_image(Gfx& gfx, const SmoothPicture& p);

// The whole call as INSPACE makes it: smooth_image(gfx, smooth_for(gfx, id, a)), the bitmap that
// seg15:0x757c keeps in DGROUP:0x95be (the title) and seg15:0x2a08 and seg15:0x20ca keep in
// DGROUP:0x92b0 (the pad ship) [BYTES seg15:0x7567-0x7591 seg15:0x2b75-0x2b97
// seg15:0x2137-0x216d] (verified claims 2-4). The pad ship's call is
//   smooth_bitmap(gfx, BOD +0x21, smooth_args(int8(SHP +0x07), int8(SHP +0x08)))
// (the bytes sign-extended, `cbw`), a 2H x 2H device bitmap of frame 0 of the body strip; null
// when seg6:0xc4 returns 0 (a scheme byte 0xff) or the palette handle is unknown.
ImagePtr smooth_bitmap(Gfx& gfx, int id, const SmoothArgs& a, int dgroup_1ccc = 0);
#endif

}  // namespace is::gfx
