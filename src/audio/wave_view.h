// What INSPACE's sound wrappers read of a running wave beyond the source point (sound.md
// "INSPACE wrappers" steps 4 and 5): seg10:0x7896's distance and activity test, and the Size
// wrap of seg10:0x754a, seg10:0x7944 and seg10:0x7b48. In its own header so audio.h (the
// parts' contract) and engine.h share it without engine.h including audio.h [DESIGN].
#pragma once

namespace is::audio {

struct WaveView {
    // The player's object, record DGROUP:0x9646 of pool DGROUP:0x9324: words +4, +6 (`x`, `y`,
    // world units), which seg10:0x7896 measures from [BYTES seg10:0x78d0-0x78e0], and words
    // +0, +2 (`scr_x`, `scr_y`), this frame's screen point, which the Size wrap moves points
    // around [BYTES seg10:0x77e0-0x77fa seg10:0x7a78-0x7a92 seg10:0x7cf0-0x7d0a].
    int x = 0, y = 0;
    int scr_x = 0, scr_y = 0;
    // `Size` DGROUP:0x9156 [BYTES seg10:0x77d2 seg10:0x7801 seg10:0x7a71 seg10:0x7ce9].
    int size = 1;
    // The activity rectangles DGROUP:0x23e0 (left, top, right, bottom, world units), the
    // first DGROUP:0x9158 of them (1, 2 or 4) [BYTES seg10:0x7914-0x792b].
    int rect_count = 0;
    int rects[4][4] = {};
};

}  // namespace is::audio
