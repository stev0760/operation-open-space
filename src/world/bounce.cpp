// ISPACE.DLL ordinal 29 (bounce.h), from work/analysis/m4/s1-bounce.md section 4 (verified).
#include "world/bounce.h"

#include "world/records.h"

namespace is::world {

namespace {

// seg2:0x802: 16-bit abs, -32768 stays -32768 [BYTES ISPACE.DLL seg2:0x80c seg2:0x81a].
std::int16_t abs16w(std::int16_t v) { return v < 0 ? sub16(0, v) : v; }
// seg2:0x824: 32-bit abs (neg/adc/neg), wrapping [BYTES ISPACE.DLL seg2:0x82e seg2:0x842].
std::int32_t abs32w(std::int32_t v) {
    return v < 0 ? lmul32(v, -1) : v;
}
// 32-bit add / sub with wrap (add/adc, sub/sbb).
std::int32_t add32(std::int32_t a, std::int32_t b) {
    return lmul32(static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b)), 1);
}
std::int32_t sub32(std::int32_t a, std::int32_t b) {
    return lmul32(static_cast<std::int32_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b)), 1);
}
// The 1-operand 16 x 16 -> 32 `imul`, exact.
std::int32_t imul32(std::int16_t a, std::int16_t b) {
    return static_cast<std::int32_t>(a) * static_cast<std::int32_t>(b);
}

}  // namespace

std::int32_t hyp32(std::int32_t a, std::int32_t b) {
    const std::int32_t B = abs32w(b), A = abs32w(a);
    // [BYTES ISPACE.DLL seg2:0x885 seg2:0x891 seg2:0x89d]: max + (min sar 1)
    const std::int32_t g = A >= B ? add32(A, sar32(B, 1)) : add32(B, sar32(A, 1));
    if (g == 0) return 1;  // [BYTES ISPACE.DLL seg2:0x8b8 seg2:0x8fb]
    const std::int32_t s = add32(lmul32(b, b), lmul32(a, a));  // [BYTES ISPACE.DLL seg2:0x8ca seg2:0x8df]
    return sar32(add32(ldiv32(s, g), g), 1);  // [BYTES ISPACE.DLL seg2:0x8ea seg2:0x8f5]
}

int bounce(std::int16_t* dA, std::int16_t* dB, std::int16_t mA, std::int16_t mB, std::int16_t eA,
           std::int16_t eB, std::int16_t* Ay, std::int16_t* Ax, std::int16_t* By, std::int16_t* Bx,
           std::int16_t* Avy, std::int16_t* Avx, std::int16_t* Bvy, std::int16_t* Bvx) {
    const std::int32_t LmB = mB, LmA = mA;  // cdq [BYTES ISPACE.DLL seg2:0x4b seg2:0x55]
    // The centre offset and the relative velocity, x10 on 16 bits [BYTES ISPACE.DLL seg2:0x5f
    // seg2:0x71 seg2:0x81 seg2:0x93 seg2:0x9f seg2:0xa2 seg2:0xb4 seg2:0xc6]
    std::int16_t h = sub16(idiv16(eB, 2), idiv16(eA, 2));
    std::int16_t DX = mul16(add16(sub16(h, *Ax), *Bx), 10);
    std::int16_t DY = mul16(add16(sub16(h, *Ay), *By), 10);
    const std::int16_t R = add16(mul16(add16(eA, eB), 5), 18);
    std::int16_t DVX = mul16(sub16(*Bvx, *Avx), 10);
    std::int16_t DVY = mul16(sub16(*Bvy, *Avy), 10);
    const std::int16_t D = lo16(hyp32(DX, DY));  // [BYTES ISPACE.DLL seg2:0xd9 seg2:0xdf]

    // Slow contact: B aimed at A at 5 units, B's velocity only [BYTES ISPACE.DLL seg2:0xea
    // seg2:0xed seg2:0xf7 seg2:0xfa seg2:0xfc seg2:0x101 seg2:0x10f seg2:0x125 seg2:0x137 seg2:0x149]
    if (abs16w(DVX) <= 40 && abs16w(DVY) <= 40) {
        *Bvx = sub16(*Avx, idiv16(mul16(DX, 5), D));
        *Bvy = sub16(*Avy, idiv16(mul16(DY, 5), D));
        DVX = mul16(sub16(*Bvx, *Avx), 10);
        DVY = mul16(sub16(*Bvy, *Avy), 10);
    }

    // The cosine test on the quotient's low word [BYTES ISPACE.DLL seg2:0x160 seg2:0x181
    // seg2:0x189 seg2:0x199 seg2:0x1a2 seg2:0x1ad seg2:0x1b4 seg2:0x1b9 seg2:0x1bb]
    const std::int32_t dot = add32(lmul32(DX, DVX), lmul32(DY, DVY));
    const std::int16_t V = lo16(hyp32(DVX, DVY));
    const std::int32_t c = ldiv32(lmul32(dot, 100), imul32(V, D));
    if (lo16(c) > 0) {
        // Separating: B.vx, B.vy, A.vx, A.vy sped up; no damage word [BYTES ISPACE.DLL seg2:0x1c0
        // seg2:0x1d3 seg2:0x1ee seg2:0x209 seg2:0x215 seg2:0x21d seg2:0x222 seg2:0x284 seg2:0x2e6
        // seg2:0x348]
        for (std::int16_t* p : {Bvx, Bvy, Avx, Avy}) {
            const std::int16_t a = abs16w(*p);
            if (a < 3)
                *p = shl16(*p, 1);
            else if (a < 7)
                *p = idiv16(shl16(*p, 2), 3);
            else if (a < 11)
                *p = idiv16(mul16(*p, 7), 6);
            else
                *p = idiv16(mul16(*p, 11), 10);
        }
        return 0;
    }

    // The collision branch: unit relative velocity x1000 [BYTES ISPACE.DLL seg2:0x35b seg2:0x360
    // seg2:0x369 seg2:0x376 seg2:0x379 seg2:0x37e]
    const std::int32_t WX = ldiv32(imul32(1000, DVX), V);
    const std::int32_t WY = ldiv32(imul32(1000, DVY), V);
    // The contact time per axis, the divisor floored at 1 [BYTES ISPACE.DLL seg2:0x389
    // seg2:0x38d seg2:0x3a3 seg2:0x3f8 seg2:0x3fe seg2:0x402 seg2:0x40d seg2:0x411 seg2:0x42b
    // seg2:0x484 seg2:0x48a seg2:0x48e]
    std::int32_t t = abs32w(WY);
    if (t < 1) t = 1;
    const std::int32_t TY = ldiv32(imul32(DVY > 0 ? add16(DY, R) : sub16(R, DY), 1000), t);
    t = abs32w(WX);
    if (t < 1) t = 1;
    const std::int32_t TX = ldiv32(imul32(DVX > 0 ? add16(DX, R) : sub16(R, DX), 1000), t);
    // The smaller magnitude, a tie keeping TY [BYTES ISPACE.DLL seg2:0x4b7 seg2:0x4bf seg2:0x4c9]
    const std::int32_t T = abs32w(TY) > abs32w(TX) ? TX : TY;

    // B alone steps back along the relative velocity [BYTES ISPACE.DLL seg2:0x4d5 seg2:0x4e2
    // seg2:0x4f1 seg2:0x505 seg2:0x514]
    *Bx = add16(*Bx, lo16(ldiv32(lmul32(WX, T), -10000)));
    *By = add16(*By, lo16(ldiv32(lmul32(WY, T), -10000)));

    // The normal from B's new centre toward A's, x1000 [BYTES ISPACE.DLL seg2:0x517 seg2:0x53c
    // seg2:0x545 seg2:0x55c seg2:0x56f seg2:0x580 seg2:0x588 seg2:0x59f seg2:0x5a7]
    h = sub16(idiv16(eB, 2), idiv16(eA, 2));
    DX = mul16(add16(sub16(h, *Ax), *Bx), 10);
    const std::int32_t M = add32(LmA, LmB);  // si:di until seg2:0x6ec
    DY = mul16(add16(sub16(h, *Ay), *By), 10);
    const std::int16_t D2 = lo16(hyp32(DX, DY));
    const std::int32_t NX = ldiv32(imul32(sub16(0, DX), 1000), D2);  // 16-bit neg, then imul
    const std::int32_t NY = ldiv32(imul32(sub16(0, DY), 1000), D2);

    // The normal components, each term truncated, the y term first, kept on 16 bits
    // [BYTES ISPACE.DLL seg2:0x5ca seg2:0x5ef seg2:0x5fb seg2:0x5fe seg2:0x61a seg2:0x642
    // seg2:0x64e seg2:0x651]
    const std::int16_t uB = add16(lo16(ldiv32(lmul32(NY, *Bvy), 1000)), lo16(ldiv32(lmul32(NX, *Bvx), 1000)));
    const std::int16_t uA = add16(lo16(ldiv32(lmul32(NY, *Avy), 1000)), lo16(ldiv32(lmul32(NX, *Avx), 1000)));

    // Strip the normal components (divide by -1000) [BYTES ISPACE.DLL seg2:0x654 seg2:0x674
    // seg2:0x69d seg2:0x6c6 seg2:0x6e9]
    *Bvx = add16(*Bvx, lo16(ldiv32(lmul32(uB, NX), -1000)));
    *Bvy = add16(*Bvy, lo16(ldiv32(lmul32(uB, NY), -1000)));
    *Avx = add16(*Avx, lo16(ldiv32(lmul32(uA, NX), -1000)));
    *Avy = add16(*Avy, lo16(ldiv32(lmul32(uA, NY), -1000)));

    // The 1-D elastic exchange [BYTES ISPACE.DLL seg2:0x6f4 seg2:0x6fa seg2:0x702 seg2:0x717
    // seg2:0x71e seg2:0x722 seg2:0x726 seg2:0x72b seg2:0x730]
    const std::int32_t lb = lmul32(uB, LmB);
    const std::int32_t num = add32(lmul32(sub32(LmA, LmB), uA), add32(lb, lb));
    const std::int16_t vA = lo16(ldiv32(num, M));
    const std::int16_t vB = add16(sub16(vA, uB), uA);

    // The new normal components back (divide by +1000) [BYTES ISPACE.DLL seg2:0x733 seg2:0x75a
    // seg2:0x77d seg2:0x7a5 seg2:0x7c8]
    *Bvx = add16(*Bvx, lo16(ldiv32(lmul32(vB, NX), 1000)));
    *Bvy = add16(*Bvy, lo16(ldiv32(lmul32(vB, NY), 1000)));
    *Avx = add16(*Avx, lo16(ldiv32(lmul32(vA, NX), 1000)));
    *Avy = add16(*Avy, lo16(ldiv32(lmul32(vA, NY), 1000)));

    // Damages, B first [BYTES ISPACE.DLL seg2:0x7cb seg2:0x7dc seg2:0x7df seg2:0x7f0 seg2:0x7f3]
    *dB = idiv16(abs16w(sub16(vB, uB)), 2);
    *dA = idiv16(abs16w(sub16(vA, uA)), 2);
    return 1;
}

}  // namespace is::world
