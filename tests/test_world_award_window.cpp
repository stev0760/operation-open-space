// M4 stage 5, track WEAPONS: the 800 award's window (src/world/gate_exit.h push_hit, award_entry;
// work/analysis/m4/s5-weapons.md 9.2, verified claims 23-25 with claim 24 as corrected) and the
// 900 ram of seg7:0x5090 (section 6, verified claims 29-31 with claim 29 as corrected), with the
// vectors of the analysis. No code change came with them: the port already reads the bytes so.
#include <cstdint>
#include <utility>
#include <vector>

#include "doctest.h"
#include "shell/rng.h"
#include "world/collide.h"
#include "world/damage.h"
#include "world/gate_exit.h"
#include "world/world.h"

using namespace is::world;

namespace {

// seg8:0x81f8 at frame p, then seg7:0x158e's search at frame t.
int award_after(std::uint32_t p, std::uint32_t t) {
    World w;
    w.frames = p;
    push_hit(w, 3, 1);
    w.frames = t;
    return award_entry(w, 3, 1);
}

}  // namespace

TEST_CASE("award: d <= 190 frames always counts, d >= 200 never [BYTES seg8:0x8240 seg7:0x190a-0x1928]") {
    CHECK(award_after(0, 190) == 0);
    CHECK(award_after(0, 199) == 0);
    CHECK(award_after(0, 200) == -1);
    CHECK(award_after(9, 199) == 0);
    CHECK(award_after(9, 200) == -1);  // d = 191, p mod 10 = 9 > 199 - 191
    CHECK(award_after(5, 196) == 0);   // d = 191, p mod 10 = 5 <= 8
    // A previous wave's entry (the counter reset at seg9:0x6689): a negative difference ends it
    CHECK(award_after(5000, 100) == -1);
}

TEST_CASE("award: a newer other entry is skipped, an old one in front ends the search [BYTES seg7:0x1923 seg7:0x1936]") {
    World w;
    w.frames = 1000;
    push_hit(w, 3, 1);   // the match, 100 tens
    w.frames = 1100;
    push_hit(w, 0, 4);   // a newer hit on another object
    w.frames = 1150;
    CHECK(award_entry(w, 3, 1) == 1);
    // An entry 20 tens old in front of the match: no award
    World s;
    s.frames = 1000;
    push_hit(s, 3, 1);
    s.frames = 1010;
    push_hit(s, 0, 4);
    s.w937a[0] = static_cast<std::int16_t>(s.w937a[0] - 20);
    s.frames = 1020;
    CHECK(award_entry(s, 3, 1) == -1);
    // A record >= 128 is stored as a byte and compared after cbw: never found (claim 24)
    World b;
    b.frames = 10;
    push_hit(b, 3, 130);
    CHECK(award_entry(b, 3, 130) == -1);
    // F0 / 10 >= 0x8000: the stored word turns negative and no entry is found (claim 24)
    World f;
    f.frames = 327680;
    push_hit(f, 3, 1);
    CHECK(award_entry(f, 3, 1) == -1);
}

namespace {

class RockCalls : public CollideHost {
public:
    std::vector<int> sounds;
    void sound(int id, int, int, int) override { sounds.push_back(id); }
};

}  // namespace

TEST_CASE("ram: a cold big rock heats on the first ram and breaks on the second within 3 s (M-10)") {
    World w;
    WaveGlobals g;
    CollideGlobals c;
    RockCalls h;
    ViewState view;
    is::shell::Rng rng;
    std::int16_t a = kPoolRocks, b = 0, d = 0;
    CollideCtx x{w, g, c, h, view, nullptr, rng, a, b, d};
    g.now = 100000;
    g.size = 1;
    g.rock_pool = kPoolRocks;
    w.pools.set_in_use(kPoolShips, 1);
    w.pools.set_in_use(kPoolRocks, 1);
    c.b_pool = kPoolShips;  // the rammer, B of the collision
    c.b_rec = 0;
    ObjectRecord& r = w.pools.rec(kPoolRocks, 0);
    r.mass = 0x578;  // a big rock
    r.max_damage = 60;
    r.size = 44;
    r.flame = 0;     // heat 0
    r.damage = 0;
    // d = 40: d' = 60, the heat 0 + 15 capped at 9, damage 60 then 120, no break
    CHECK(rock_damage(x, kPoolRocks, 0, 40) == 0);
    CHECK(static_cast<std::int8_t>(r.flame) == 9);
    CHECK(r.damage == 120);
    CHECK(r.d6a.get() == g.now + 3000u);
    CHECK(h.sounds.empty());
    // Again within the 3 s: old heat 9 > 6 and damage 180 > 60, the split (0x45) and result 1,
    // after which the kill scorer adds P[3] = 900 (DGROUP:0xb14) through the dispatcher
    g.now += 1000;
    CHECK(rock_damage(x, kPoolRocks, 0, 40) == 1);
    CHECK(h.sounds == std::vector<int>{0x45});
    CHECK(kill_points(3) == 900);
}
