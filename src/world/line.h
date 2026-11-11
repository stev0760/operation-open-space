// The clipped line seg12:0x7256(hdc, x0, y0, x1, y1) (work/analysis/m4/s7-tether.md section 7,
// verified claims 55 and 56 and Omission N) and the Borland RTL long helpers it computes with.
// Every rope draw and erase of the GRAPPLING HOOK and HANDCUFFS, LIGHTNING (seg11:0xa4a6,
// seg11:0xa9ea) and the race mover (seg12:0x3e98) draw through it, so it lives on its own
// (M4 s7 TETHER).
//
// clip_line is the routine's arithmetic, pure and exact: it returns the two points the routine
// passes to MoveTo and LineTo, or nothing when it returns without drawing. draw_clipped_line
// adds the two GDI calls on a Blitter. Portable, no Win32.
#pragma once

#include <cstdint>
#include <optional>

#include "world/records.h"
#include "world/sprite.h"

namespace is::world {

// --- Borland RTL long arithmetic (seg5) ------------------------------------------------------
// The 32-bit low word of a wrapping sum, difference and negation.
constexpr std::int32_t to_i32(std::uint32_t v) {
    return v < 0x80000000u ? static_cast<std::int32_t>(v) : -static_cast<std::int32_t>(~v) - 1;
}
constexpr std::int32_t sub32(std::int32_t a, std::int32_t b) {
    return to_i32(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}
constexpr std::int32_t add32(std::int32_t a, std::int32_t b) {
    return to_i32(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
// `neg ax; adc dx, 0; neg dx`: the 32-bit negation [BYTES seg12:0x7395-0x739a].
constexpr std::int32_t neg32(std::int32_t a) { return to_i32(0u - static_cast<std::uint32_t>(a)); }
// seg5:0x8c0: dx:ax shifted left by cl (`shl ax, 1; rcl dx, 1` per bit), bits past 32 lost
// [BYTES seg5:0x8c0].
constexpr std::int32_t rtl_lshl(std::int32_t a, int n) {
    return to_i32(static_cast<std::uint32_t>(a) << n);
}
// seg5:0x346: dx:ax shifted right arithmetically by cl (`sar dx, 1; rcr ax, 1` per bit)
// [BYTES seg5:0x34a seg5:0x34c].
constexpr std::int32_t rtl_lsar(std::int32_t a, int n) { return sar32(a, n); }
// seg5:0x314: the low 32 bits of the product [BYTES seg5:0x314].
constexpr std::int32_t rtl_lmul(std::int32_t a, std::int32_t b) {
    return to_i32(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}
// seg5:0x110: signed 32/32 divide truncating toward zero, the dividend the last-pushed long and
// the divisor the first; both made positive, divided unsigned and the sign restored, so
// INT_MIN / -1 gives INT_MIN (verified claim 13) [BYTES seg5:0x116-0x1a7]. d = 0 has no
// defined result; callers test it first (this returns 0 there) [DESIGN].
constexpr std::int32_t rtl_ldiv(std::int32_t n, std::int32_t d) {
    if (d == 0) return 0;
    const std::uint32_t un = n < 0 ? 0u - static_cast<std::uint32_t>(n) : static_cast<std::uint32_t>(n);
    const std::uint32_t ud = d < 0 ? 0u - static_cast<std::uint32_t>(d) : static_cast<std::uint32_t>(d);
    const std::uint32_t q = un / ud;
    return to_i32(((n < 0) != (d < 0)) ? 0u - q : q);
}

// --- seg12:0x7256 -----------------------------------------------------------------------------
// The two points MoveTo and LineTo receive.
struct LineEnds {
    std::int16_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    bool operator==(const LineEnds& o) const {
        return x0 == o.x0 && y0 == o.y0 && x1 == o.x1 && y1 == o.y1;
    }
};

// seg12:0x7256's clip of (x0, y0)-(x1, y1) to [0, w] x [0, h] (w = DGROUP:0x90ea, h =
// DGROUP:0x90ec), the routine's arithmetic step by step [BYTES seg12:0x7264-0x75a3]:
//  1. all four coordinates inside, inclusive: the line as given;
//  2. x1 = x0 nudges x1 + 1, y1 = y0 nudges y1 + 1 (before the tests of step 3);
//  3. nothing when both y < 0, both y > h, both x < 0 or both x > w;
//  4. m = (sext(y1 - y0) << 7) / sext(x1 - x0) (seg5:0x8c0, seg5:0x110), then x1 = x0 when x1 =
//     x0 + 1, b = y0 - ((m·x0) sar 7) (seg5:0x314, seg5:0x346);
//  5. an end above the top: nothing when m = 0, else the crossing -((b << 7) / m) when 0 < it
//     < w; 6. an end below the bottom: nothing when m = 0, else ((h - b) << 7) / m;
//  7. m' = (sext(x1 - x0) << 7) / sext(y1 - y0), then y1 = y0 when y1 = y0 + 1, b' = x0 -
//     ((m'·y0) sar 7); 8. the left and right edges alike with m', b' and 0 < y < h;
//  9. an end no crossing replaced takes its own (nudged) coordinates.
// A later edge's crossing overwrites an earlier one for the same end (Omission N).
std::optional<LineEnds> clip_line(std::int16_t w, std::int16_t h, std::int16_t x0,
                                  std::int16_t y0, std::int16_t x1, std::int16_t y1);

// seg12:0x7256 in full on the Blitter: the clip, then MoveTo(dc, P0) and LineTo(dc, P1)
// [BYTES seg12:0x75a6-0x75bd]. Draws nothing when clip_line returns nothing.
void draw_clipped_line(Blitter& out, DcId dc, std::int16_t w, std::int16_t h, std::int16_t x0,
                       std::int16_t y0, std::int16_t x1, std::int16_t y1);

}  // namespace is::world
