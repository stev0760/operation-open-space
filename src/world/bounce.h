// ISPACE.DLL ordinal 29 (seg2:0x3c), the elastic exchange the collision response seg8:0x546c
// calls through the forwarder seg10:0x7dd4 (docs/re/core.md "Bounce (ISPACE.DLL ordinal 29)",
// docs/re/ispace.md "Ordinal notes" (29)), and its helpers: the signed 32-bit square-root step
// seg2:0x850 and the Borland long helpers seg3:0xfe (multiply) and seg3:0x130 (divide).
//
// Ported line by line from the listing as work/analysis/m4/s1-bounce.md section 4 reads it, all
// 20 claims confirmed by s1-bounce-verify.md: every 16-bit product, sum and shift wraps on 16
// bits, every 32-bit one on 32 bits, every divide truncates toward zero (ADR 0013 decision 7).
// The function reads and writes only through its arguments; it draws no rand() and touches no
// global [BYTES ISPACE.DLL seg2:0x3c]. Portable, no Win32.
#pragma once

#include <cstdint>

namespace is::world {

// seg2:0x850(a, b): g = max(|a|, |b|) + (min(|a|, |b|) >> 1) (signed 32-bit compare, arithmetic
// shift); 1 when g = 0; else (seg3:0x130(a·a + b·b, g) + g) >> 1, the squares and their sum
// wrapping on 32 bits [BYTES ISPACE.DLL seg2:0x85f seg2:0x885 seg2:0x8b8 seg2:0x8ca seg2:0x8df
// seg2:0x8ea seg2:0x8f5 seg2:0x8fb]. Ordinal 29 keeps the low word.
std::int32_t hyp32(std::int32_t a, std::int32_t b);

// seg3:0xfe: the low 32 bits of a·b [BYTES ISPACE.DLL seg3:0xfe seg3:0x129].
constexpr std::int32_t lmul32(std::int32_t a, std::int32_t b) {
    const std::uint32_t p = static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b);
    return p < 0x80000000u ? static_cast<std::int32_t>(p) : -static_cast<std::int32_t>(~p) - 1;
}
// seg3:0x130: signed 32-bit divide truncating toward zero, the quotient's low 32 bits (the
// magnitudes divided unsigned, negated for one negative operand) [BYTES ISPACE.DLL seg3:0x136
// seg3:0x157 seg3:0x1b9 seg3:0x1bc]. Ordinal 29 never passes d = 0 (s1-bounce.md section 5).
constexpr std::int32_t ldiv32(std::int32_t n, std::int32_t d) {
    const std::int64_t q = static_cast<std::int64_t>(n) / static_cast<std::int64_t>(d);
    const std::uint32_t u = static_cast<std::uint32_t>(static_cast<std::uint64_t>(q));
    return u < 0x80000000u ? static_cast<std::int32_t>(u) : -static_cast<std::int32_t>(~u) - 1;
}

// Ordinal 29, Pascal far, `retf 0x30`. The arguments in [bp+] order, the reverse of the push
// order of seg8:0x57a3-0x57db (s1-bounce.md section 2, claim 1): A's and B's damage words
// (DGROUP:0x9438, 0x9436), the masses (0x9446, 0x9448), the extents already (extent >> s) + 1
// (0x9442, 0x9444), then the screen positions and velocities of A and B through pointers
// (DGROUP:0x52e4 ... 0x52ea, 0x52dc ... 0x52e2). Returns 0 for a separating pair (the four
// velocities sped up, nothing else written) and 1 for a collision (B moved back to the contact
// point, both velocities exchanged along the normal, both damage words written)
// [BYTES ISPACE.DLL seg2:0x3c seg2:0x348 seg2:0x7f3 seg2:0x7fe].
int bounce(std::int16_t* dA, std::int16_t* dB, std::int16_t mA, std::int16_t mB, std::int16_t eA,
           std::int16_t eB, std::int16_t* Ay, std::int16_t* Ax, std::int16_t* By, std::int16_t* Bx,
           std::int16_t* Avy, std::int16_t* Avx, std::int16_t* Bvy, std::int16_t* Bvx);

}  // namespace is::world
