// The view (src/world/view.h; docs/re/world.md "View"): the origin of seg8:0x0 at 1024 x 768
// (W = 1024, H = 683, Size 1), the pinned player, the activity rectangles near the world's
// edges and the torus wrap of seg8:0x29ea.
#include <cstdint>

#include "doctest.h"
#include "world/view.h"

using namespace is::world;

namespace {

ViewState view_at(std::int16_t px, std::int16_t py) {
    ViewState v;
    v.play_w = 1024;
    v.play_h = 683;
    v.shift = 3;
    ViewInputs in;
    in.player_x = px;
    in.player_y = py;
    recentre(v, in);
    return v;
}

}  // namespace

TEST_CASE("view: the origin keeps the player at the centre") {
    const ViewState v = view_at(12800, 12800);
    // Ww = 1024 << 3 = 8192, Hw = 683 << 3 = 5464
    CHECK(v.x0 == 12800 - 4096);
    CHECK(v.y0 == 12800 - 2732);
    CHECK(v.x1 == v.x0 + 8192);
    CHECK(v.y1 == v.y0 + 5464);
    CHECK(v.x0l == v.x0);
    CHECK(v.y0l == v.y0);
    const ScreenPoint p = world_to_screen(v, 12800, 12800, 0, 0, 0);
    const ScreenPoint pin = pinned_player(v);
    CHECK(pin.x == 512);
    CHECK(pin.y == 341);
    CHECK(p.x == pin.x);  // (12800 - 8704) >> 3
    CHECK(p.y == pin.y);  // 2732 >> 3 = 341
    // One activity rectangle, the view grown by 600 units
    CHECK(v.active_count == 1);
    CHECK(v.active[0] == ViewRect{8104, 9468, 17496, 16132});
}

TEST_CASE("view: world to screen wraps across the torus") {
    // The player near the right edge sees an object near x = 0 to its right.
    const ViewState v = view_at(25000, 12800);
    CHECK(v.x0 == 20904);
    ScreenPoint p = world_to_screen(v, 100, 12800, 0, 0, 0);
    CHECK(p.x == (25600 + 100 - 20904) >> 3);  // 599
    // Unwrapped in a kind-6 wave before DGROUP:0x9eb5 = 1: (100 - 20904) >> 3 rounds down
    p = world_to_screen(v, 100, 12800, 6, 0, 0);
    CHECK(p.x == -2601);
    p = world_to_screen(v, 100, 12800, 6, 1, 0);
    CHECK(p.x == 599);
    // The player near the top sees an object near y = 25600 above it.
    const ViewState w = view_at(12800, 500);
    CHECK(w.y0 == 500 - 2732);
    p = world_to_screen(w, 12800, 25500, 0, 0, 0);
    CHECK(p.y == (25500 - 25600 - (500 - 2732)) >> 3);  // 266
    CHECK(p.x == 512);
}

TEST_CASE("view: activity rectangles split at the world's edges") {
    // Near the left edge, middle height: two rectangles, the right one across the wrap.
    ViewState v = view_at(1000, 12800);
    CHECK(v.active_count == 2);
    CHECK(v.active[0] == ViewRect{-600, 9468, 600 + 1000 + 4096, 16132});
    CHECK(v.active[1] == ViewRect{25600 - 4096 - 600 + 1000, 9468, 26200, 16132});
    // Near the left edge and the top: four.
    v = view_at(1000, 1000);
    CHECK(v.active_count == 4);
    CHECK(v.active[0].left == -600);
    CHECK(v.active[0].top == -600);
    CHECK(v.active[2].top == 25600 - 2732 - 600 + 1000);
    CHECK(v.active[3].bottom == 26200);
    // Near the right edge and the bottom: four.
    v = view_at(25000, 25000);
    CHECK(v.active_count == 4);
    CHECK(v.active[0] == ViewRect{25000 - 4096 - 600, 25000 - 2732 - 600, 26200, 26200});
    CHECK(v.active[2].top == -600);
    CHECK(v.active[2].bottom == 2732 - 25600 + 25000 + 600);
    // Near the top only: two, one above the wrap.
    v = view_at(12800, 1000);
    CHECK(v.active_count == 2);
    CHECK(v.active[0] == ViewRect{12800 - 4096 - 600, -600, 12800 - 4096 - 600 + 1200 + 8192,
                                  600 + 1000 - 2732 + 5464});
    CHECK(v.active[1].top == 25600 - 2732 - 600 + 1000);
    // A race before the start: one rectangle round the view.
    ViewInputs in;
    in.player_x = 1000;
    in.player_y = 1000;
    in.wave_kind = 6;
    v.play_w = 1024;
    v.play_h = 683;
    recentre(v, in);
    CHECK(v.active_count == 1);
    CHECK(v.active[0].left == v.x0 - 600);
}

TEST_CASE("view: 16-bit origins") {
    // Size 0 (s = 4): Ww = 1024 << 4 = 16384, Hw = 683 << 4 = 10928, all in 16 bits.
    ViewState v;
    v.play_w = 1024;
    v.play_h = 683;
    v.shift = 4;
    ViewInputs in;
    in.player_x = 25000;
    in.player_y = 25000;
    recentre(v, in);
    CHECK(v.x0 == 25000 - 8192);
    CHECK(v.x1 == static_cast<std::int16_t>(25000 - 8192 + 16384));  // 33192 wraps
    CHECK(v.x1 < 0);
}
