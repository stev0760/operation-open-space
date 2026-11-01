// The wave frame's drawing (src/world/draw.h; docs/re/world.md "The player's sprite", "Icon
// dissolve", "Rocks"): the player's cell origin for types 0/7 and the others with 16-bit
// products, the conditions and the effect skip of seg7:0x1208, its blit through seg8:0xb76, the
// rock cell from the rotation, the dissolve's StretchBlt geometry and its last step, the frame
// loop, and the ship image slots.
#include <array>
#include <cstdint>
#include <vector>

#include "doctest.h"
#include "world/draw.h"

using namespace is::world;

namespace {

struct Fixture {
    Pools pools;
    ShipTable ships{};
    EffectTable effects{};
    ImageSlotTable images{};
    DrawGlobals g;
    RecordingBlitter out;
    DrawContext c{pools, ships, effects, images, g, out,
                  [](std::int16_t x, std::int16_t y) {
                      return Point16{static_cast<std::int16_t>(x >> 3),
                                     static_cast<std::int16_t>(y >> 3)};
                  },
                  nullptr};

    Fixture() {
        clear_effects(effects);
        g.state = 1;
        g.dg_3a38 = 1;
        g.sprite.player = 2;
        g.sprite.play_w = 1024;
        g.sprite.play_h = 683;
        g.strip_dc = 0x30;
        ObjectRecord& p = pools.rec(kPoolShips, 2);
        p.on_screen = 1;
        p.scr_x = 512, p.scr_y = 341;
        p.size = 32, p.gap = 4;
        p.frame = 5;
        ships[2].has_flame = 1;  // slot = ship index
        images[2 + kShipImageSlotBase] = 0x55;
    }
};

}  // namespace

TEST_CASE("player sprite: the cell origin by ship type") {
    // Types 0 and 7: heading * (size + gap) + gap; the row stays without +0xe9.
    PlayerCell c = player_cell(0, 5, 32, 4, 0);
    CHECK(c.src_x == 5 * 36 + 4);
    CHECK(c.keep_y);
    // Type 7 is > 3, so its row is 2 * gap + size whatever +0xe9 says.
    c = player_cell(7, 15, 32, 4, 1);
    CHECK(c.src_x == 15 * 36 + 4);
    CHECK_FALSE(c.keep_y);
    CHECK(c.src_y == 2 * 4 + 32);
    CHECK(player_cell(7, 15, 32, 4, 0).src_y == 2 * 4 + 32);
    // Others: 16 * size + 17 * gap whatever the heading.
    c = player_cell(1, 9, 32, 4, 1);
    CHECK(c.src_x == 16 * 32 + 17 * 4);
    CHECK(c.src_y == 4);
    CHECK(player_cell(3, 9, 32, 4, 0).keep_y);
    c = player_cell(4, 9, 32, 4, 0);
    CHECK(c.src_x == 16 * 32 + 17 * 4);
    CHECK(c.src_y == 40);
    // The products are 16-bit: 1000 * 36 = 36000 wraps to -29536.
    CHECK(player_cell(0, 1000, 32, 4, 0).src_x == -29536 + 4);
    CHECK(player_cell(2, 0, 2100, 1, 0).src_x == static_cast<std::int16_t>(16 * 2100 + 17));
}

TEST_CASE("player sprite: seg7:0x1208 blits the cell through seg8:0xb76") {
    Fixture f;
    draw_player_sprite(f.c);
    REQUIRE(f.out.calls.size() == 1);
    // The pinned path: SRCCOPY at the stored (512, 341) from slot 2 + 0x28.
    CHECK(f.out.calls[0] == bit_blt_call(kDcWindow, 512, 341, 32, 32, 0x55, 5 * 36 + 4, 0, kSrcCopy));
    CHECK(f.g.player_rect.l == 514);
    CHECK(f.g.player_rect.t == 343);
    CHECK(f.g.player_rect.r == 542);
    CHECK(f.g.player_rect.b == 371);
    CHECK(f.pools.rec(kPoolShips, 2).src_x == 184);
    // A body-thrust ship (+0xe4 = 0) uses its word +0x50 as the slot.
    Fixture b;
    b.ships[2].has_flame = 0;
    b.ships[2].sprite_slot = 6;
    b.images[6 + kShipImageSlotBase] = 0x66;
    draw_player_sprite(b.c);
    REQUIRE(b.out.calls.size() == 1);
    CHECK(b.out.calls[0].src == 0x66);
}

TEST_CASE("player sprite: the conditions") {
    auto drawn = [](auto&& set) {
        Fixture f;
        set(f);
        return player_sprite_drawn(f.c);
    };
    CHECK(drawn([](Fixture&) {}));
    CHECK_FALSE(drawn([](Fixture& f) { f.g.state = 3; }));
    CHECK_FALSE(drawn([](Fixture& f) { f.pools.rec(kPoolShips, 2).state = 100; }));
    CHECK_FALSE(drawn([](Fixture& f) { f.g.sprite.dg_951a = 1; }));
    CHECK_FALSE(drawn([](Fixture& f) { f.g.dg_9130 = 1; }));
    CHECK_FALSE(drawn([](Fixture& f) { f.g.dg_3a38 = 0; }));
    CHECK_FALSE(drawn([](Fixture& f) { f.g.sprite.dg_9e49 = 4, f.g.sprite.dg_952e = 2; }));
    CHECK(drawn([](Fixture& f) { f.g.sprite.dg_9e49 = 7, f.g.sprite.dg_952e = 2; }));
    CHECK(drawn([](Fixture& f) { f.g.sprite.dg_9e49 = 4, f.g.sprite.dg_952e = 1; }));
    // An effect of kind 1..8 on the player's ship hides it while ship byte +0xec is 0.
    auto effect = [](Fixture& f, std::uint16_t kind, int k, int last) {
        f.effects[static_cast<std::size_t>(k)].kind = kind;
        f.effects[static_cast<std::size_t>(k)].pool = 0xe;
        f.effects[static_cast<std::size_t>(k)].rec = 2;
        f.g.last_effect = static_cast<std::int16_t>(last);
    };
    CHECK_FALSE(drawn([&](Fixture& f) { effect(f, 3, 4, 4); }));
    CHECK(drawn([&](Fixture& f) { effect(f, 3, 4, 3); }));  // past DGROUP:0x9434
    CHECK(drawn([&](Fixture& f) { effect(f, 9, 0, 4); }));
    CHECK(drawn([&](Fixture& f) { effect(f, 0, 0, 4); }));
    CHECK(drawn([&](Fixture& f) { effect(f, kEffectFree, 0, 4); }));
    CHECK(drawn([&](Fixture& f) { effect(f, 3, 4, 4), f.ships[2].bec = 1; }));
    Fixture f;
    f.g.state = 0;
    draw_player_sprite(f.c);
    CHECK(f.out.calls.empty());
}

TEST_CASE("rocks: the frame and cell from the rotation") {
    ObjectRecord r{};
    r.size = 30, r.gap = 6;
    r.angle = 1799;
    rock_cell(r);
    CHECK(r.frame == 17);
    CHECK(r.src_x == 36 * 17 + 6);
    r.angle = 99;
    rock_cell(r);
    CHECK(r.frame == 0);
    CHECK(r.src_x == 6);
}

TEST_CASE("icon dissolve: the StretchBlt geometry and the blit to the screen") {
    Fixture f;
    // Pool 0 record 0 gives the cell: 32 with a 6 gap, as the strip of world.md "Icons".
    f.pools.rec(kPoolIcons, 0).size = 32;
    f.pools.rec(kPoolIcons, 0).gap = 6;
    ObjectRecord& icon = f.pools.rec(kPoolIcons, 2);
    icon.size = 99;  // not used: record 0's size counts
    icon.on_screen = 1;
    icon.scr_x = 300, icon.scr_y = 200;
    icon.race_status = 10;
    icon.blast_size = -1;
    f.g.dissolve_step = 3;
    dissolve_icon(f.c, 2);
    CHECK(icon.race_status == 7);
    REQUIRE(f.out.calls.size() == 3);
    BlitCall mode;
    mode.op = BlitCall::kStretchMode, mode.dst = 0x30, mode.rop = kColorOnColor;
    CHECK(f.out.calls[0] == mode);
    // x = (gap + size) * i + gap = 82, from 2 px around it: (80, 4), 36 x 36.
    BlitCall s = bit_blt_call(0x30, 82, 6, 32, 32, 0x30, 80, 4, kSrcCopy);
    s.op = BlitCall::kStretchBlt, s.sw = 36, s.sh = 36;
    CHECK(f.out.calls[1] == s);
    CHECK(f.out.calls[2] == bit_blt_call(kDcWindow, 300, 200, 32, 32, 0x30, 82, 6, kSrcCopy));
}

TEST_CASE("icon dissolve: the last step blacks the square and the strip cell") {
    struct Hooks : DissolveHooks {
        int a = 0, b = 0;
        bool show = false;
        bool seg13_59e8(int, int) override { return show; }
        void seg7_65aa(int, int i) override { a = 100 + i; }
        void seg7_2080(int, int v) override { b = v; }
    } hooks;
    Fixture f;
    f.c.hooks = &hooks;
    f.pools.rec(kPoolIcons, 0).size = 32;
    f.pools.rec(kPoolIcons, 0).gap = 6;
    ObjectRecord& icon = f.pools.rec(kPoolIcons, 1);
    icon.on_screen = 1;
    icon.scr_x = 40, icon.scr_y = 50;
    icon.race_status = 2;
    icon.blast_size = -1;
    f.g.dissolve_step = 5;  // 2 - 5 < 0: floor 0
    dissolve_icon(f.c, 1);
    CHECK(icon.race_status == 0);
    REQUIRE(f.out.calls.size() == 5);
    CHECK(f.out.calls[3] == pat_blt_call(kDcWindow, 40, 50, 32, 32, kBlackness));
    CHECK(f.out.calls[4] == pat_blt_call(0x30, 44, 6, 32, 32, kBlackness));
    CHECK(hooks.a == 101);
    CHECK(hooks.b == -1);
    // Off screen it draws only when seg13:0x59e8 says so, but always counts down.
    Fixture o;
    o.c.hooks = &hooks;
    ObjectRecord& off = o.pools.rec(kPoolIcons, 1);
    off.race_status = 9;
    o.g.dissolve_step = 1;
    dissolve_icon(o.c, 1);
    CHECK(off.race_status == 8);
    CHECK(o.out.calls.empty());
    hooks.show = true;
    dissolve_icon(o.c, 1);
    CHECK(o.out.calls.size() == 3);
}

TEST_CASE("icon dissolve: the player's ship is redrawn on odd steps") {
    Fixture f;
    f.pools.rec(kPoolIcons, 0).size = 32;
    f.pools.rec(kPoolIcons, 0).gap = 6;
    ObjectRecord& icon = f.pools.rec(kPoolIcons, 0);
    icon.on_screen = 1;
    icon.race_status = 8;
    icon.blast_size = 2;  // the player
    f.g.detail = 2;
    f.g.dissolve_step = 1;  // 7: odd
    dissolve_icon(f.c, 0);
    REQUIRE(f.out.calls.size() == 4);
    CHECK(f.out.calls[3].src == 0x55);  // seg7:0x1208's blit
    f.out.calls.clear();
    dissolve_icon(f.c, 0);  // 6: even
    CHECK(f.out.calls.size() == 3);
    f.out.calls.clear();
    f.g.detail = 1;
    dissolve_icon(f.c, 0);  // 5: odd, but Detail 1
    CHECK(f.out.calls.size() == 3);
}

TEST_CASE("icon dissolve: the frame loop visits in-use icons with a counter") {
    Fixture f;
    f.pools.rec(kPoolIcons, 0).size = 32;
    f.pools.rec(kPoolIcons, 0).gap = 6;
    f.pools.set_in_use(kPoolIcons, 3);
    for (int i = 0; i < 4; ++i) {
        f.pools.rec(kPoolIcons, i).on_screen = 1;
        f.pools.rec(kPoolIcons, i).blast_size = -1;
    }
    f.pools.rec(kPoolIcons, 1).race_status = 5;
    f.pools.rec(kPoolIcons, 3).race_status = 5;  // not in use
    f.g.dissolve_step = 1;
    dissolve_pass(f.c);
    CHECK(f.pools.rec(kPoolIcons, 0).race_status == 0);
    CHECK(f.pools.rec(kPoolIcons, 1).race_status == 4);
    CHECK(f.pools.rec(kPoolIcons, 3).race_status == 5);
    CHECK(f.out.calls.size() == 3);
}

TEST_CASE("ship images: the body bitmap in the ship's slot") {
    RecordingBlitter out;
    is::fmt::BodyRecord body;
    body.bitmap_id = 4321;
    const DcId dc = load_ship_body(out, body);
    REQUIRE(out.images.size() == 1);
    CHECK(out.images[0].graphic == 4321);
    CHECK(dc == out.images[0].id);
    body.bitmap_id = is::fmt::kBodyBitmapFromFile;
    CHECK(load_ship_body(out, body) == kDcNone);
    ImageSlotTable images{};
    ShipRecord s{};
    s.has_flame = 1;
    set_ship_image(images, 3, s, dc);
    CHECK(images[3 + 0x28] == dc);
    s.has_flame = 0;
    s.sprite_slot = 9;
    set_ship_image(images, 3, s, 0x77);
    CHECK(images[9 + 0x28] == 0x77);
    s.sprite_slot = 40;  // past the table: ignored
    set_ship_image(images, 3, s, 0x78);
}

TEST_CASE("frame draws: the link lines draw nothing without a link (seg12:0x3778)") {
    // The placement leaves every slot of DGROUP:0x65cc without ends (s4-draw.md 2.5) [BYTES
    // seg9:0x66dd seg9:0x66e1]; the stars are tests/test_world_stars.cpp's.
    World w;
    w.pools.set_allocated(10, 3);
    struct Hooks : LinkHooks {
        int lines = 0;
        void line(DcId, std::int16_t, std::int16_t, std::int16_t, std::int16_t) override { ++lines; }
    } h;
    draw_lines(w, kPoolShips, kDcWindow, &h);
    CHECK(h.lines == 0);
}

TEST_CASE("frame draws: a link whose end left play breaks (seg12:0x3778) [BYTES seg12:0x3778-0x390c]") {
    World w;
    w.pools.set_allocated(10, 3);
    w.pools.rec(10, 1).state = 0;
    LinkSlot& l = w.links[1];
    l.pool_a = 4, l.rec_a = 2, l.pool_b = 3, l.rec_b = 5, l.w08 = 9;
    l.line = {1, 2, 3, 4, 5, 6, 7, 8};
    w.pools.rec(4, 2).state = 0;
    w.pools.rec(3, 5).state = 0;
    struct Hooks : LinkHooks {
        std::vector<std::array<int, 4>> lines;
        int calls_2080 = 0;
        void line(DcId, std::int16_t a, std::int16_t b, std::int16_t c, std::int16_t d) override {
            lines.push_back({a, b, c, d});
        }
        void seg7_2080(int, int) override { ++calls_2080; }
    } h;
    draw_lines(w, kPoolShips, kDcWindow, &h);
    CHECK(h.lines.empty());  // both ends in play
    w.pools.rec(3, 5).state = 1;
    draw_lines(w, kPoolShips, kDcWindow, &h);
    REQUIRE(h.lines.size() == 1);
    // seg12:0x7256(hdc, 0x65e8, 0x65ea, 0x65e0, 0x65e2): words 6, 7, 2, 3 of the slot's line.
    CHECK(h.lines[0] == std::array<int, 4>{7, 8, 3, 4});
    CHECK(h.calls_2080 == 1);
    CHECK(l.pool_a == -1);
    CHECK(l.pool_b == -1);
    CHECK(l.w08 == 0);
    CHECK(l.line == std::array<std::int16_t, 8>{1, 2, 1, 2, 5, 6, 5, 6});
    CHECK(w.pools.rec(10, 1).state == 1);
    CHECK(w.pools.rec(4, 2).owner_pool == -1);
    CHECK(w.pools.rec(3, 5).w36 == -1);
}
