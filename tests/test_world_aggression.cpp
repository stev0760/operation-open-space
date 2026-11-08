// The violence decay seg7:0x2128 (src/world/aggression.h; M4 stage 5, track ICONS, triage M-2;
// work/analysis/m4/s5-icons.md 3.3-3.4 with s5-icons-verify.md claims 41-44).
#include <cstdint>

#include "doctest.h"
#include "world/aggression.h"

using namespace is::world;

TEST_CASE("aggression: one step is max(A / div, floor), floored at 0 [BYTES seg7:0x2173-0x218a]") {
    CHECK(violence_step(98, 50, 2) == 96);
    CHECK(violence_step(500, 50, 2) == 490);
    CHECK(violence_step(1, 50, 2) == 0);   // 1 - 2 < 0 -> 0 (`jns`)
    CHECK(violence_step(0, 40, 3) == 0);
    CHECK(violence_step(100, 18, 4) == 95);
    CHECK(violence_step(-10, 50, 2) == 0);  // a negative A: -10 / 50 = 0 -> 2, -12 -> 0
}

TEST_CASE("aggression: a solo wave steps 2 every 10 s, no time test (core-0003: 98 -> 56) [BYTES seg7:0x2135-0x2142]") {
    // ORACLE03 98 to lastgame 56: 21 solo steps (s5-icons.md 3.4, verified claim 44).
    World w;
    w.last_ship = 0;
    w.w9432 = 98;
    w.w8e28 = 1000000;  // a shot just now changes nothing in the solo case
    for (int k = 0; k < 21; ++k) decay_violence(w, 1000100);
    CHECK(w.w9432 == 56);
    w.w9432 = 1;
    decay_violence(w, 0);
    CHECK(w.w9432 == 0);
    decay_violence(w, 0);
    CHECK(w.w9432 == 0);
}

TEST_CASE("aggression: other ships, none hunting: (40, 3), then (36, 2) 15 s after the last shot (core-0003: 50 -> 45) [BYTES seg7:0x21b4-0x21f9]") {
    World w;
    w.last_ship = 2;  // two other ships
    w.player = 2;
    w.w9432 = 50;
    w.w8e28 = 10000;
    decay_violence(w, 25001);  // 10000 + 15000 < 25001
    CHECK(w.w9432 == 45);      // 50 - 3 = 47, 47 - 2 = 45 = ORACLE3X
    w.w9432 = 50;
    decay_violence(w, 25000);  // not yet 15 s: `jae` skips the second step
    CHECK(w.w9432 == 47);
    // Before the first shot DGROUP:0x8e28 is 0 (BSS): the second step applies once 15 s passed.
    w.w8e28 = 0;
    w.w9432 = 200;
    decay_violence(w, 15001);
    CHECK(w.w9432 == 200 - 5 - 5);  // 200 / 40 = 5; 195 / 36 = 5
}

TEST_CASE("aggression: a ship hunting the player: (18, 4), then (14, 4) after 15 s [BYTES seg7:0x215c-0x21b2]") {
    World w;
    w.last_ship = 2;
    w.player = 2;
    ShipRecord& s = w.ships[1];
    s.w32 = 14;  // +0x32, the literal 14
    s.w34 = 2;   // +0x34 = DGROUP:0x9646
    s.w38 = 1;
    w.w8e28 = 100000;
    w.w9432 = 100;
    decay_violence(w, 100001);  // a recent shot
    CHECK(w.w9432 == 95);
    w.w9432 = 100;
    decay_violence(w, 200000);  // an old shot: 100 -> 95 -> 95 - 6 = 89
    CHECK(w.w9432 == 89);
    // Any of the three words wrong: not a hunter.
    s.w38 = 0;
    w.w9432 = 100;
    decay_violence(w, 100001);
    CHECK(w.w9432 == 97);  // max(100 / 40, 3)
    // The last record DGROUP:0x9316 itself is never tested.
    s.w38 = 1;
    w.last_ship = 1;
    w.w9432 = 100;
    decay_violence(w, 100001);
    CHECK(w.w9432 == 97);
}
