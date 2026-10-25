// The original's random numbers (docs/re/rng.md): INSPACE's runtime-library rand()/srand(),
// the linear congruential generator of the Microsoft C runtime with a 32-bit state. Portable.
#pragma once

#include <cstdint>

namespace is::shell {

class Rng {
public:
    // INSPACE seg5:0xc4: the 16-bit seed goes to the low word of the state, the high word is
    // cleared [BYTES seg5:0xcc seg5:0xcf].
    void srand(std::uint16_t seed) { state_ = seed; }
    // INSPACE seg5:0xdc: state = state * 214013 + 2531011 (mod 2^32), bits 16-30 returned,
    // 0..32767 [BYTES seg5:0xe1 seg5:0xf6 seg5:0x105].
    int rand() {
        state_ = state_ * 214013u + 2531011u;
        return static_cast<int>((state_ >> 16) & 0x7FFFu);
    }
    // DGROUP:0x144c.
    std::uint32_t state() const { return state_; }
    void set_state(std::uint32_t s) { state_ = s; }

private:
    std::uint32_t state_ = 1;  // DGROUP:0x144c starts at 1 [BYTES DGROUP:0x144c]
};

// INSPACE's one generator (DGROUP:0x144c). ISPACE.DLL's own copy (ISPACE.DLL DGROUP:0xb8c) is a
// separate Rng where its code is ported [DESIGN].
Rng& game_rng();

// rand() % n as the original's `cwd; idiv` computes it for a positive constant n (rng.md
// "Callers and range mapping").
inline int rand_mod(Rng& r, int n) { return r.rand() % n; }

}  // namespace is::shell
