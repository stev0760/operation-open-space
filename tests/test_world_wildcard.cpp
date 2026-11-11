// WILDCARD DEFENSE (M4 s7 FUEL): src/world/wildcard.h and pods.cpp's pool-6 branch against
// work/analysis/m4/s7-fuel.md section 5 with s7-fuel-verify.md (claims 36-43, 46; O10-O12,
// O14; the key-down rule) and core-0009's WILDCARD DEFENSE section [ORACLE core-0009].
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <tuple>
#include <vector>

#include "assets/asset_store.h"
#include "doctest.h"
#include "formats/dib.h"
#include "hud/weapons.h"
#include "shell/rng.h"
#include "world/collide.h"
#include "world/missile.h"
#include "world/pods.h"
#include "world/sprite.h"
#include "world/wildcard.h"

using namespace is::world;

namespace {

class RecWildcardHost : public WildcardHost {
public:
    std::vector<std::tuple<int, int, int, int>> erased;
    std::vector<int> pictures;
    std::int16_t height = 23;
    void erase(int x, int y, int w, int h) override { erased.emplace_back(x, y, w, h); }
    std::int16_t fruit_image(int id) override {
        pictures.push_back(id);
        return height;
    }
};

struct WildRig {
    World w;
    WaveGlobals g;
    WeaponWords W;
    is::shell::Rng rng;
    RecWildcardHost h;
    WildRig() {
        std::optional<int> filled;
        is::hud::fill_weapon_words(-1, W, filled);  // seg10:0x1fb2, the registered edition
        g.player = 0;
        w.player = 0;
        w.size = 1;
        // The user: ship 0 with WILDCARD in slot 1 of two, at (8000, 8000).
        ShipRecord& s = w.ship(0);
        s.slots_cb() = 2;
        set_byte_at(s, 0xb9, kWeaponWildcard);
        ObjectRecord& u = w.pools.rec(kPoolShips, 0);
        u.x = 8000;
        u.y = 8000;
        w.pools.set_in_use(kPoolMissiles, 0);
    }
    // A live weapon-w missile in record m at (x, y), age `age`.
    void missile(int m, int wpn, std::int16_t x, std::int16_t y, std::int16_t age = 5) {
        if (w.pools.in_use(kPoolMissiles) <= m) w.pools.set_in_use(kPoolMissiles, static_cast<std::int16_t>(m + 1));
        MissileSlot& e = w.pools.missile(m);
        e.weapon = static_cast<std::int16_t>(wpn);
        e.age = age;
        ObjectRecord& o = w.pools.rec(kPoolMissiles, m);
        o.x = x;
        o.y = y;
        o.scr_x = static_cast<std::int16_t>(100 + m);
        o.scr_y = static_cast<std::int16_t>(200 + m);
        o.size = 12;
        o.gap = 4;
        o.extent = 0x60;
        o.on_destroy.set(kDestroyGeneric);
    }
    void run(int pool = kPoolShips, int rec = 0) { wildcard(w, g, W, rng, h, pool, rec); }
};

}  // namespace

TEST_CASE("wildcard: a missile in range becomes fruit 27 [BYTES seg11:0x943b-0x9138] (verified claims 37, 38)") {
    WildRig r;
    r.missile(0, kWeaponRadarMissiles, 8100, 7900);
    is::shell::Rng want = r.rng;
    r.run();
    const MissileSlot& e = r.w.pools.missile(0);
    CHECK(e.weapon == kWeaponFruit);
    CHECK(e.age == 1);
    CHECK(e.steer_delay == 50);  // W(27)+0x1e
    CHECK(r.W.word(kWeaponFruit, -4) == kFruitSlot);  // the run-time writes (O12)
    CHECK(r.W.word(kWeaponFruit, 0x24) == 5);
    CHECK(r.w.pools.rec(kPoolMissiles, 0).on_destroy.get() == kDestroyPool11);
    // The square blacked with no margin.
    CHECK(r.h.erased == std::vector<std::tuple<int, int, int, int>>{{100, 200, 12, 12}});
    // One rand() for the picture: 660 + rand() % 3.
    REQUIRE(r.h.pictures.size() == 1);
    CHECK(r.h.pictures[0] == kFruitGraphic + want.rand() % 3);
    CHECK(r.rng.state() == want.state());
    // h = 23 (odd): `extent` 23 << 3, `size` ((23 + 1) << 1) >> 1, `gap` (4 << 1) >> 1.
    const ObjectRecord& o = r.w.pools.rec(kPoolMissiles, 0);
    CHECK(o.extent == 184);
    CHECK(o.size == 24);
    CHECK(o.gap == 4);
    // No fuel, no count: nothing else of the ship changes.
    CHECK(r.w.ship(0).fuel == 0);
}

TEST_CASE("wildcard: the fruit sizes by height and Size [BYTES seg11:0x934f-0x93a8] (verified claim 40)") {
    auto sized = [](std::int16_t h, std::int16_t size) {
        WildRig r;
        r.w.size = size;
        r.h.height = h;
        r.missile(0, kWeaponRadarMissiles, 8100, 8000);
        r.run();
        const ObjectRecord& o = r.w.pools.rec(kPoolMissiles, 0);
        return std::make_tuple(o.size, o.gap, o.extent);
    };
    CHECK(sized(27, 1) == std::make_tuple<std::int16_t, std::int16_t, std::int16_t>(28, 4, 216));
    CHECK(sized(23, 2) == std::make_tuple<std::int16_t, std::int16_t, std::int16_t>(48, 8, 184));
    CHECK(sized(23, 0) == std::make_tuple<std::int16_t, std::int16_t, std::int16_t>(12, 2, 184));
    // An even height keeps `size` unscaled and takes gap (6 << Size) >> 1.
    CHECK(sized(24, 1) == std::make_tuple<std::int16_t, std::int16_t, std::int16_t>(24, 6, 192));
}

TEST_CASE("wildcard: the tests: age, lifetime, weapon 27 and 650 cells [BYTES seg11:0x93db-0x9439] (verified claim 37)") {
    WildRig r;
    r.missile(0, kWeaponRadarMissiles, 8100, 8000, 0);      // age 0: just launched, skipped
    r.missile(1, kWeaponRadarMissiles, 8100, 8000, 200);    // age = lifetime 200: skipped
    r.missile(2, kWeaponFruit, 8100, 8000, 3);              // already fruit
    r.missile(3, kWeaponRadarMissiles, 8000 + 5200, 8000);  // 650 cells: skipped
    r.missile(4, kWeaponRadarMissiles, 8000 + 5192, 8000);  // 649 cells: converted
    r.missile(5, -1, 8100, 8000, 7);                        // a free entry: W(-1)+0x1e reads 0 (O10)
    REQUIRE(r.W.word(kWeaponRadarMissiles, 0x1e) == 200);
    r.run();
    CHECK(r.w.pools.missile(0).weapon == kWeaponRadarMissiles);
    CHECK(r.w.pools.missile(1).weapon == kWeaponRadarMissiles);
    CHECK(r.w.pools.missile(2).age == 3);
    CHECK(r.w.pools.missile(3).weapon == kWeaponRadarMissiles);
    CHECK(r.w.pools.missile(4).weapon == kWeaponFruit);
    CHECK(r.w.pools.missile(5).weapon == -1);
    // A live fruit (record 2) exists: record 4 copies its `size` and `gap` and draws no rand().
    CHECK(r.h.pictures.empty());
    CHECK(r.w.pools.rec(kPoolMissiles, 4).size == 12);
    CHECK(r.w.pools.rec(kPoolMissiles, 4).gap == 4);
    CHECK(r.w.pools.rec(kPoolMissiles, 4).extent == 0x60);  // its own
    CHECK(r.rng.state() == is::shell::Rng().state());
}

TEST_CASE("wildcard: every live missile in range, the user's own included, one picture for all (verified claim 41, O10)") {
    WildRig r;
    r.missile(0, kWeaponRadarMissiles, 8100, 8000);
    r.missile(1, 7, 7900, 8000);
    r.w.pools.missile(1).launcher_pool = kPoolShips;  // the user's own
    r.w.pools.missile(1).launcher_rec = 0;
    r.run();
    CHECK(r.w.pools.missile(0).weapon == kWeaponFruit);
    CHECK(r.w.pools.missile(1).weapon == kWeaponFruit);
    // The first conversion found no live fruit and drew the picture; the second found m = 0.
    CHECK(r.h.pictures.size() == 1);
    CHECK(r.w.pools.rec(kPoolMissiles, 1).size == 24);
    CHECK(r.w.pools.rec(kPoolMissiles, 1).extent == 0x60);
    // A second call converts nothing more and draws nothing.
    r.h.erased.clear();
    r.run();
    CHECK(r.h.erased.empty());
    CHECK(r.h.pictures.size() == 1);
}

TEST_CASE("wildcard: the gate: a ship needs weapon 13 or 23 in slots 1 ... +0xcb - 1; any other pool passes [BYTES seg11:0x9087-0x90c9] (verified claim 36)") {
    WildRig r;
    r.missile(0, kWeaponRadarMissiles, 8100, 8000);
    set_byte_at(r.w.ship(0), 0xb9, 4);
    r.run();
    CHECK(r.w.pools.missile(0).weapon == kWeaponRadarMissiles);
    set_byte_at(r.w.ship(0), 0xb9, kWeaponShock);
    r.run();
    CHECK(r.w.pools.missile(0).weapon == kWeaponFruit);
    // A pool-8 user (seg12:0x6dea's hazards) needs no slot.
    WildRig q;
    q.w.pools.rec(8, 2).x = 8000;
    q.w.pools.rec(8, 2).y = 8000;
    q.missile(0, kWeaponRadarMissiles, 8100, 8000);
    set_byte_at(q.w.ship(0), 0xb9, 0);
    q.run(8, 2);
    CHECK(q.w.pools.missile(0).weapon == kWeaponFruit);
}

TEST_CASE("wildcard: seg12:0x6a60 on a pool-6 record: the entry's age = W(weapon)+0x1e, weapon -1, five puffs [BYTES seg12:0x6ac5-0x6b0e] (verified claim 43, O14)") {
    WildRig r;
    r.missile(3, kWeaponFruit, 8100, 8000, 50);
    set_weapon_words([&r](int w, int off) { return r.W.word(w, off); });
    std::vector<std::tuple<int, int, int>> puffs;
    std::vector<std::tuple<int, int, int, int>> erased;
    PodDestroyOut d;
    d.puff = [&puffs](int p, int q, int a) { puffs.emplace_back(p, q, a); };
    d.erase = [&erased](int x, int y, int w, int h) { erased.emplace_back(x, y, w, h); };
    d.to_screen = [](std::int16_t, std::int16_t) { return ScreenPoint{10, 10}; };
    r.w.W = 640;
    r.w.H = 480;
    pod_destroyed(r.w, kPoolMissiles, 3, d);
    set_weapon_words({});
    CHECK(r.w.pools.missile(3).age == 50);
    CHECK(r.w.pools.missile(3).weapon == -1);
    CHECK(puffs == std::vector<std::tuple<int, int, int>>(5, {kPoolMissiles, 3, 1}));
    CHECK(erased == std::vector<std::tuple<int, int, int, int>>{{103, 203, 12, 12}});
    CHECK(r.w.pools.rec(kPoolMissiles, 3).state == 1);
}

TEST_CASE("wildcard: the picture of slot 0x39 on a Blitter [BYTES seg11:0x9146-0x9338] (verified claim 39)") {
    RecordingBlitter out;
    ImageSlotTable images{};
    images[kFruitSlot] = 0x77;
    const DcId graphic = out.load_graphic(kFruitGraphic + 1, 0, -1);
    out.calls.clear();
    render_fruit_image(out, images, graphic, 27);
    REQUIRE(out.deleted.size() == 2);
    CHECK(out.deleted[0] == 0x77);     // the old picture first
    CHECK(out.deleted[1] == graphic);  // the graphic last
    const DcId slot = images[kFruitSlot];
    REQUIRE(slot != kDcNone);
    REQUIRE(out.images.size() == 2);
    CHECK(out.images.back().w == 37);  // 2·5 + 27
    CHECK(out.images.back().h == 37);
    REQUIRE(out.calls.size() == 2);
    CHECK(out.calls[0] == pat_blt_call(slot, 0, 0, 37, 37, kBlackness));
    CHECK(out.calls[1] == bit_blt_call(slot, 5, 5, 27, 27, graphic, 0, 0, kSrcCopy));
    // An even height takes gap 6.
    RecordingBlitter out2;
    ImageSlotTable im2{};
    render_fruit_image(out2, im2, out2.load_graphic(kFruitGraphic, 0, -1), 24);
    CHECK(out2.images.back().w == 36);
}

TEST_CASE("wildcard: the default host returns IGRAPHIC's heights and logs once [BYTES IGRAPHIC.DLL 4000 660-662]") {
    WildcardHost h;
    CHECK(h.fruit_image(kFruitGraphic) == 23);
    CHECK(h.fruit_image(kFruitGraphic + 1) == 27);
    CHECK(h.fruit_image(kFruitGraphic + 2) == 23);
    CHECK(h.fruit_image(0x10) == 0);
}

TEST_CASE("wildcard: WM_KEYDOWN fires weapon 13 only, with no repeat test; WM_KEYUP fires any [BYTES seg11:0x6f58-0x6f74 seg11:0x6e11]") {
    is::hud::WeaponKeyState s;
    s.weapon = kWeaponWildcard;
    CHECK(is::hud::weapon_key(0x100, 'Z', s) == is::hud::WeaponKey::kFire);
    CHECK(is::hud::weapon_key(0x100, 0x0d, s) == is::hud::WeaponKey::kFire);
    CHECK(is::hud::weapon_key(0x101, 'Z', s) == is::hud::WeaponKey::kFire);
    s.weapon = 4;
    CHECK(is::hud::weapon_key(0x100, 'Z', s) == is::hud::WeaponKey::kIgnored);
    CHECK(is::hud::weapon_key(0x101, 'Z', s) == is::hud::WeaponKey::kFire);
    s.weapon = kWeaponWildcard;
    s.play_mode = 1;
    CHECK(is::hud::weapon_key(0x100, 'Z', s) == is::hud::WeaponKey::kIgnored);
}

TEST_CASE("wildcard: graphics 660 ... 662 are 23, 27 and 23 high, all odd (golden) [BYTES IGRAPHIC.DLL 4000 660-662] (verified claim 46)") {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    is::assets::AssetStore store{std::filesystem::path(v)};
    for (int k = 0; k < 3; ++k) {
        const is::fmt::Dib d = store.sprite(static_cast<std::uint16_t>(kFruitGraphic + k));
        CHECK(d.height == kFruitHeights[static_cast<std::size_t>(k)]);
        CHECK(d.width == d.height);
        CHECK((d.height & 1) == 1);
    }
}
