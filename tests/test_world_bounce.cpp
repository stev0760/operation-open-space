// ISPACE.DLL ordinal 29 (src/world/bounce.h): the five worked vectors of
// work/analysis/m4/s1-bounce.md section 6, recomputed by s1-bounce-verify.md, with every output.
#include <cstdint>

#include "doctest.h"
#include "world/bounce.h"

using namespace is::world;

namespace {

struct Io {
    std::int16_t dA = 0x7777, dB = 0x7777;
    std::int16_t mA = 0, mB = 0, eA = 0, eB = 0;
    std::int16_t Ax = 0, Ay = 0, Bx = 0, By = 0;
    std::int16_t Avx = 0, Avy = 0, Bvx = 0, Bvy = 0;
    int run() { return bounce(&dA, &dB, mA, mB, eA, eB, &Ay, &Ax, &By, &Bx, &Avy, &Avx, &Bvy, &Bvx); }
};

}  // namespace

TEST_CASE("bounce: seg2:0x850 returns 1 at (0, 0) and one Newton step otherwise") {
    CHECK(hyp32(0, 0) == 1);           // [BYTES ISPACE.DLL seg2:0x8b8 seg2:0x8fb]
    CHECK(hyp32(150, 0) == 150);
    CHECK(hyp32(100, 130) == 164);     // s1-bounce.md V2: g 180, 26900 / 180 = 149
    CHECK(hyp32(-30, -80) == 85);
    CHECK(hyp32(60, 10) == 60);
    CHECK(hyp32(60, -90) == 108);
    CHECK(hyp32(60, -120) == 135);
    CHECK(hyp32(1, 0) == 1);
    CHECK(ldiv32(-7, 2) == -3);
    CHECK(ldiv32(7, -2) == -3);
    CHECK(lmul32(0x10000, 0x10000) == 0);
}

TEST_CASE("bounce V1: head-on, rock A against ship B") {
    Io io;
    io.mA = 1400, io.mB = 160, io.eA = 22, io.eB = 12;
    io.Ax = 100, io.Ay = 100, io.Avx = 2, io.Avy = 0;
    io.Bx = 120, io.By = 105, io.Bvx = -6, io.Bvy = 0;
    CHECK(io.run() == 1);
    CHECK(io.Avx == 0);
    CHECK(io.Avy == 0);
    CHECK(io.Bvx == 8);
    CHECK(io.Bvy == 0);
    CHECK(io.Bx == 123);
    CHECK(io.By == 105);
    CHECK(io.Ax == 100);
    CHECK(io.Ay == 100);
    CHECK(io.dB == 7);
    CHECK(io.dA == 1);
}

TEST_CASE("bounce V2: glancing hit, small rock A at rest against ship B") {
    Io io;
    io.mA = 350, io.mB = 160, io.eA = 22, io.eB = 12;
    io.Ax = 100, io.Ay = 100;
    io.Bx = 115, io.By = 118, io.Bvx = -3, io.Bvy = -8;
    CHECK(io.run() == 1);
    CHECK(io.Avx == -2);
    CHECK(io.Avy == -3);
    CHECK(io.Bvx == 1);
    CHECK(io.Bvy == -1);
    CHECK(io.Bx == 117);
    CHECK(io.By == 123);
    CHECK(io.Ax == 100);
    CHECK(io.Ay == 100);
    CHECK(io.dB == 5);
    CHECK(io.dA == 2);
}

TEST_CASE("bounce V3: a separating pair returns 0 and writes no damage word") {
    Io io;
    io.mA = 1400, io.mB = 160, io.eA = 22, io.eB = 12;
    io.Ax = 100, io.Ay = 100, io.Avx = -2, io.Avy = 0;
    io.Bx = 120, io.By = 105, io.Bvx = 4, io.Bvy = 1;
    CHECK(io.run() == 0);
    CHECK(io.Avx == -4);
    CHECK(io.Avy == 0);
    CHECK(io.Bvx == 5);
    CHECK(io.Bvy == 2);
    CHECK(io.Ax == 100);
    CHECK(io.Ay == 100);
    CHECK(io.Bx == 120);
    CHECK(io.By == 105);
    CHECK(io.dA == 0x7777);
    CHECK(io.dB == 0x7777);
}

TEST_CASE("bounce V4: a hit on a fixed post, A of mass 20000") {
    Io io;
    io.mA = 20000, io.mB = 160, io.eA = 10, io.eB = 12;
    io.Ax = 200, io.Ay = 200;
    io.Bx = 205, io.By = 190, io.Bvx = 0, io.Bvy = 7;
    CHECK(io.run() == 1);
    CHECK(io.Avx == 0);
    CHECK(io.Avy == 0);
    CHECK(io.Bvx == 4);
    CHECK(io.Bvy == -3);
    CHECK(io.Bx == 205);
    CHECK(io.By == 187);
    CHECK(io.Ax == 200);
    CHECK(io.Ay == 200);
    CHECK(io.dB == 6);
    CHECK(io.dA == 0);
}

TEST_CASE("bounce V5: the slow contact aims B at A, then the collision branch") {
    Io io;
    io.mA = 1400, io.mB = 160, io.eA = 22, io.eB = 12;
    io.Ax = 100, io.Ay = 100, io.Avx = 1, io.Avy = 0;
    io.Bx = 120, io.By = 105, io.Bvx = 0, io.Bvy = 0;
    CHECK(io.run() == 1);
    CHECK(io.Avx == 0);
    CHECK(io.Avy == 0);
    CHECK(io.Bvx == 5);
    CHECK(io.Bvy == 0);
    CHECK(io.Bx == 123);
    CHECK(io.By == 105);
    CHECK(io.dB == 4);
    CHECK(io.dA == 0);
}
