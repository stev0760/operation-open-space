// The rock images of the wave's load, seg10:0x3a2e (called from seg9:0x22b4, which picks it or
// seg10:0x5102 by DGROUP:0x90f0): image slots 6 and 7 (DGROUP:0x39f4, 0x39f6), 869 x 53 with the 18 big-rock
// frames of 43 x 43 at (48k + 5, 5), and slots 8 and 9 (DGROUP:0x39f8, 0x39fa), 473 x 31 with
// the same frames stretched to 21 x 21 at (26k + 5, 5). Slots 6 and 8 hold BITMAP ROCKS (the
// heated rocks' MERGECOPY source), slots 7 and 9 IGRAPHIC.DLL #4000 50 in mode 3 (the colour
// rocks the rock mover draws). The rock mover's cells (setup.cpp templates: size 44 or 22, gap
// 4) read them at x = (size + gap)·frame + gap.
//
// Ported from the bytes by the M3 s2 fixer for a display of 8 bits per pixel or more
// (DGROUP:0x90f0 != 0, which the port's surface is, ADR 0007); world.md has no section for it
// yet [BYTES seg10:0x3a2e-0x3e98]. Portable, no Win32.
#pragma once

#include "world/records.h"
#include "world/sprite.h"

namespace is::world {

// The image slots seg10:0x3a2e fills.
constexpr int kSlotRockMaskBig = 6;    // DGROUP:0x39f4
constexpr int kSlotRockBig = 7;        // DGROUP:0x39f6
constexpr int kSlotRockMaskSmall = 8;  // DGROUP:0x39f8
constexpr int kSlotRockSmall = 9;      // DGROUP:0x39fa

// seg10:0x3a2e: runs only while slot 6 is empty [BYTES seg10:0x3a3d]. Sets DGROUP:0x740 = 0
// through `dg_740` [BYTES seg10:0x3aa1]. The 1-bpp CreateBitmap of slots 6 and 8 becomes an
// image of the renderer's (index) format [DESIGN ADR 0013]. seg10:0x6308(slot, -1, -1, -1) for
// slots 7, 6, 9, 8 at the end is [OPEN].
void build_rock_images(Blitter& out, ImageSlotTable& images, std::int16_t& dg_740);

}  // namespace is::world
