// The original's rand()/srand() (docs/re/rng.md), checked against the doc's oracle values.
#include <cstdint>

#include "doctest.h"
#include "shell/rng.h"

using is::shell::Rng;

namespace {
// Steps the LCG backwards: the inverse of 214013 modulo 2^32.
std::uint32_t step_back(std::uint32_t s) {
    constexpr std::uint32_t kInv = 0xb9b33155u;  // 214013 * kInv == 1 (mod 2^32)
    return (s - 2531011u) * kInv;
}
}  // namespace

TEST_CASE("rng: the Microsoft C sequence from the initial state 1") {
    Rng r;
    CHECK(r.state() == 1);
    // MSVC's rand() without srand: 41, 18467, 6334, 26500, 19169
    CHECK(r.rand() == 41);
    CHECK(r.rand() == 18467);
    CHECK(r.rand() == 6334);
    CHECK(r.rand() == 26500);
    CHECK(r.rand() == 19169);
}

TEST_CASE("rng: srand stores the 16-bit seed and clears the high word") {
    Rng r;
    r.set_state(0xdeadbeefu);
    r.srand(0x3375);
    CHECK(r.state() == 0x3375u);
    Rng s;
    s.srand(1);
    Rng t;  // srand(1) equals the initial state
    for (int i = 0; i < 10; ++i) CHECK(s.rand() == t.rand());
}

TEST_CASE("rng: the probe's dumps lie on the orbits of their seeds (rng.md Oracle check)") {
    constexpr std::uint32_t kInv = 0xb9b33155u;
    CHECK(214013u * kInv == 1u);
    struct Dump {
        std::uint32_t state;
        std::uint16_t seed;
        int draws;
    };
    const Dump dumps[] = {
        {0x25da5ef6u, 0x3375, 891},  {0xe43e04a1u, 0xb411, 2512}, {0x1dae3fcbu, 0xb411, 2522},
        {0x732657cau, 0xb411, 2627}, {0x507159d6u, 0xb411, 2799},
    };
    for (const Dump& d : dumps) {
        Rng r;
        r.srand(d.seed);
        for (int i = 0; i < d.draws; ++i) r.rand();
        CHECK(r.state() == d.state);
        std::uint32_t s = d.state;
        for (int i = 0; i < d.draws; ++i) s = step_back(s);
        CHECK(s == d.seed);
    }
}

TEST_CASE("rng: rand_mod is the plain remainder") {
    Rng r;
    Rng q;
    for (int i = 0; i < 100; ++i) CHECK(is::shell::rand_mod(r, 105) == q.rand() % 105);
}
