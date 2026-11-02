// The M4 stage 2 fixer's wiring: the missile's on_damage seg7:0x2652 through the dispatcher's far
// call (collide.h call_on_damage), the one missile table DGROUP:0x66e0 (records.h
// Pools::missile) that the pair test reads, the selected weapon byte +0xb8 + slot (world.h
// selected_weapon) and the weapon words' default source (collide.h weapon_word).
#include <cstdint>
#include <utility>
#include <vector>

#include "doctest.h"
#include "shell/rng.h"
#include "world/collide.h"
#include "world/missile.h"

using namespace is::world;

namespace {

class Calls : public CollideHost {
public:
    std::vector<std::pair<int, int>> destroyed;
    void on_destroy(int pool, int rec) override { destroyed.push_back({pool, rec}); }
};

struct Rig {
    World w;
    WaveGlobals g;
    CollideGlobals c;
    Calls h;
    ViewState view;
    is::shell::Rng rng;
    std::int16_t dg_943a = 0, dg_943c = 0, dg_9454 = 0;
    CollideCtx x{w, g, c, h, view, nullptr, rng, dg_943a, dg_943c, dg_9454};
    Rig() {
        w.pools.resize(6, 2);
        ObjectRecord& m = w.pools.rec(6, 1);
        m.on_damage.set(kDamageGeneric);
        m.max_damage = 10;
    }
};

}  // namespace

TEST_CASE("m4s2 wiring: a missile's on_damage is seg7:0x2652, its far call the host's on_destroy") {
    Rig r;
    CHECK(call_on_damage(r.x, 6, 1, 4) == 0);  // damage 4 <= 10
    CHECK(r.w.pools.rec(6, 1).damage == 4);
    CHECK(call_on_damage(r.x, 6, 1, 7) == 1);  // 11 > 10: the far call through +0x66
    REQUIRE(r.h.destroyed.size() == 1);
    CHECK(r.h.destroyed[0] == std::pair<int, int>{6, 1});
}

TEST_CASE("m4s2 wiring: the selected weapon is the signed byte +0xb8 + slot") {
    ShipRecord s{};
    s.weapon_slot = 1;
    set_byte_at(s, 0xb9, 8);
    CHECK(selected_weapon(s) == 8);
    set_byte_at(s, 0xb9, 0xff);
    CHECK(selected_weapon(s) == -1);
    s.weapon_slot = -1;  // a byte before +0xb8: the port's bound
    CHECK(selected_weapon(s) == -1);
}

TEST_CASE("m4s2 wiring: the weapon words default to the program's mutable copy") {
    WeaponWords& words = missile_state().words;
    const std::int16_t old = words.word(8, 0x24);
    words.set_word(8, 0x24, 1);
    CHECK(weapon_word(8, 0x24) == 1);
    CHECK(CollideHost{}.weapon_kind(8) == 1);
    words.set_word(8, 0x24, old);
}
