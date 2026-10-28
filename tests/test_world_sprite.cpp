// The sprite routine seg8:0xb76 (src/world/sprite.h; docs/re/world.md "Sprites") against a
// recording Blitter: the early exits, the position update, the source choice, the three erase
// cases (the gap's one larger blit, the L-shaped strips in all four directions, the old cell
// blacked), the ROPs, the pinned player's path with DGROUP:0x951a, 0x238e and 0x238c and the
// 0x00A000C9 mask, and kinds 4 to 7. On Win32, the GDI Blitter draws on a real surface.
#include <cstdint>
#include <vector>

#include "doctest.h"
#include "world/records.h"
#include "world/sprite.h"

#ifdef _WIN32
#include "gfx/gfx.h"
#endif

using namespace is::world;

namespace {

constexpr DcId kSrc = 0x40;  // the caller's source DC
constexpr DcId kAlt = 0x41;  // object +0x3c

// A rock-like object in pool 3: cell 32, gap 4, on screen at (100, 100) before the call.
struct Fixture {
    Pools pools;
    SpriteGlobals g;
    RecordingBlitter out;
    Point16 next{100, 100};  // where seg8:0x29ea puts the object this call
    ToScreen to_screen = [this](std::int16_t, std::int16_t) { return next; };

    Fixture() {
        g.play_w = 1024;
        g.play_h = 683;
        g.player = 0;
        ObjectRecord& o = obj();
        o.on_screen = 1;
        o.owner_pool = -1;
        o.scr_x = 100, o.scr_y = 100;
        o.size = 32, o.gap = 4;
        o.src_x = 200, o.src_y = 4;
        o.alt_dc = kAlt;
    }
    ObjectRecord& obj() { return pools.rec(kPoolRocks, 2); }
    ObjectRecord& player() { return pools.rec(kPoolShips, 0); }
    int draw(std::int16_t kind = 1, std::int16_t masked = 0, int pool = kPoolRocks, int rec = 2) {
        return draw_sprite(pools, pool_rec(pool, rec), kind, kSrc, masked, g, to_screen, out);
    }
    // Move the object to (x, y) on this call.
    int move_to(std::int16_t x, std::int16_t y, std::int16_t masked = 0) {
        next = Point16{x, y};
        return draw(1, masked);
    }
};

}  // namespace

TEST_CASE("sprite: a move within the gap is one larger blit, no erase") {
    Fixture f;
    CHECK(f.move_to(102, 97) == 1);  // dx 2, dy -3, both <= gap 4
    REQUIRE(f.out.calls.size() == 1);
    // At (x - max(dx, 0), y - max(dy, 0)), (size + |dx|) x (size + |dy|), source offset alike.
    CHECK(f.out.calls[0] == bit_blt_call(kDcWindow, 100, 97, 34, 35, kSrc, 198, 4, kSrcCopy));
    CHECK(f.obj().old_scr_x == 100);
    CHECK(f.obj().old_scr_y == 100);
    CHECK(f.obj().scr_x == 102);
    CHECK(f.obj().scr_y == 97);
    CHECK(f.obj().w3a == kSrc);  // +0x3a keeps the DC passed in
}

TEST_CASE("sprite: a negative move within the gap blits from the new position") {
    Fixture f;
    f.move_to(96, 104);  // dx -4, dy 4
    REQUIRE(f.out.calls.size() == 1);
    CHECK(f.out.calls[0] == bit_blt_call(kDcWindow, 96, 100, 36, 36, kSrc, 200, 0, kSrcCopy));
}

TEST_CASE("sprite: MERGECOPY lowers a negative move by 2 before the tests") {
    Fixture f;
    // dy -3 becomes -5 > gap: the L strips, then the cell with MERGECOPY.
    f.move_to(102, 97, 1);
    REQUIRE(f.out.calls.size() == 3);
    // dx 2 >= 0, dy -5 < 0: the strip left of the cell and the strip below it.
    CHECK(f.out.calls[0] == pat_blt_call(kDcWindow, 100, 97, 2, 37, kBlackness));
    CHECK(f.out.calls[1] == pat_blt_call(kDcWindow, 102, 129, 32, 5, kBlackness));
    CHECK(f.out.calls[2] == bit_blt_call(kDcWindow, 102, 97, 32, 32, kSrc, 200, 4, kMergeCopy));
    // A positive move is not lowered: dx 3 and dy 4 stay in the gap.
    Fixture g;
    g.move_to(103, 104, 1);
    REQUIRE(g.out.calls.size() == 1);
    CHECK(g.out.calls[0] == bit_blt_call(kDcWindow, 100, 100, 35, 36, kSrc, 197, 0, kMergeCopy));
}

TEST_CASE("sprite: a move within the size blacks the L-shaped strips (four directions)") {
    struct Case {
        std::int16_t x, y;
        BlitCall a, b;
    };
    // Old cell (100, 100) 32 x 32; the strips cover exactly the part of the old cell outside
    // the new one.
    const Case cases[] = {
        // dx 10, dy 8: left strip (x - dx, y - dy, dx, size + dy), top strip (x, y - dy, size, dy)
        {110, 108, pat_blt_call(kDcWindow, 100, 100, 10, 40, kBlackness),
         pat_blt_call(kDcWindow, 110, 100, 32, 8, kBlackness)},
        // dx -10, dy 8: right strip (x + size, y - dy, -dx, size + dy), top strip
        {90, 108, pat_blt_call(kDcWindow, 122, 100, 10, 40, kBlackness),
         pat_blt_call(kDcWindow, 90, 100, 32, 8, kBlackness)},
        // dx 10, dy -8: left strip (x - dx, y, dx, size - dy), bottom strip (x, y + size, size, -dy)
        {110, 92, pat_blt_call(kDcWindow, 100, 92, 10, 40, kBlackness),
         pat_blt_call(kDcWindow, 110, 124, 32, 8, kBlackness)},
        // dx -10, dy -8: right strip (x + size, y, -dx, size - dy), bottom strip
        {90, 92, pat_blt_call(kDcWindow, 122, 92, 10, 40, kBlackness),
         pat_blt_call(kDcWindow, 90, 124, 32, 8, kBlackness)},
    };
    for (const Case& c : cases) {
        CAPTURE(c.x);
        CAPTURE(c.y);
        Fixture f;
        f.move_to(c.x, c.y);
        REQUIRE(f.out.calls.size() == 3);
        CHECK(f.out.calls[0] == c.a);
        CHECK(f.out.calls[1] == c.b);
        CHECK(f.out.calls[2] == bit_blt_call(kDcWindow, c.x, c.y, 32, 32, kSrc, 200, 4, kSrcCopy));
    }
    // A move along one axis only still makes both calls; the second is empty.
    Fixture f;
    f.move_to(110, 100);
    REQUIRE(f.out.calls.size() == 3);
    CHECK(f.out.calls[0] == pat_blt_call(kDcWindow, 100, 100, 10, 32, kBlackness));
    CHECK(f.out.calls[1] == pat_blt_call(kDcWindow, 110, 100, 32, 0, kBlackness));
}

TEST_CASE("sprite: a move of the size or more blacks the whole old cell") {
    for (const Point16 p : {Point16{132, 100}, Point16{100, 68}, Point16{300, 400}}) {
        CAPTURE(p.x);
        CAPTURE(p.y);
        Fixture f;
        f.move_to(p.x, p.y);
        REQUIRE(f.out.calls.size() == 2);
        CHECK(f.out.calls[0] == pat_blt_call(kDcWindow, 100, 100, 32, 32, kBlackness));
        CHECK(f.out.calls[1] == bit_blt_call(kDcWindow, p.x, p.y, 32, 32, kSrc, 200, 4, kSrcCopy));
    }
}

TEST_CASE("sprite: the erase geometry is 16-bit") {
    RecordingBlitter out;
    // dx = 30000 - (-30000) wraps to -5536; |dx| is not within the size: the old cell.
    erase_and_merge(out, kDcWindow, kSrc, 30000, 0, -30000, 0, 32, 4, 0, 0, kSrcCopy);
    REQUIRE(out.calls.size() == 1);
    CHECK(out.calls[0] == pat_blt_call(kDcWindow, -30000, 0, 32, 32, kBlackness));
    // x + size wraps in the strip's x.
    RecordingBlitter o2;
    // old_x = -32766, x = 32760: dx = 32760 + 32766 = 65526 -> -10 (16-bit).
    erase_and_merge(o2, kDcWindow, kSrc, 32760, 0, -32766, 0, 32, 4, 0, 0, kSrcCopy);
    REQUIRE(o2.calls.size() == 2);
    CHECK(o2.calls[0] == pat_blt_call(kDcWindow, static_cast<std::int16_t>(32760 + 32), 0, 10,
                                      32, kBlackness));
}

TEST_CASE("sprite: early exits and kinds 4 and 5") {
    Fixture f;
    f.obj().on_screen = 0;
    CHECK(f.draw(1) == 0);  // off screen, not pool 7, kind 1, no owner
    CHECK(f.out.calls.empty());
    CHECK(f.obj().scr_x == 100);  // not even moved
    // An owner keeps it going.
    f.obj().owner_pool = 3;
    f.next = Point16{140, 150};
    CHECK(f.draw(1) == 1);
    CHECK(f.obj().scr_x == 140);
    // Kind 4: the position moves, nothing is drawn.
    Fixture k4;
    k4.obj().on_screen = 0;
    k4.next = Point16{7, 9};
    CHECK(k4.draw(4) == 1);
    CHECK(k4.out.calls.empty());
    CHECK(k4.obj().scr_x == 7);
    CHECK(k4.obj().old_scr_x == 100);
    // Kind 5: the same off screen, a normal draw on screen.
    Fixture k5;
    k5.obj().on_screen = 0;
    k5.next = Point16{7, 9};
    CHECK(k5.draw(5) == 1);
    CHECK(k5.out.calls.empty());
    Fixture k5b;
    k5b.next = Point16{101, 101};
    CHECK(k5b.draw(5) == 1);
    CHECK(k5b.out.calls.size() == 1);
    // Pool 7 never exits early.
    Fixture p7;
    p7.pools.rec(7, 0) = p7.obj();
    p7.pools.rec(7, 0).on_screen = 0;
    CHECK(p7.draw(1, 0, 7, 0) == 1);
}

TEST_CASE("sprite: kinds 6 and 7") {
    Fixture f;
    f.next = Point16{150, 160};
    f.draw(6, 1);
    REQUIRE(f.out.calls.size() == 1);  // no erase
    CHECK(f.out.calls[0] == bit_blt_call(kDcWindow, 150, 160, 32, 32, kSrc, 200, 4, kSrcPaint));
    Fixture g;
    g.next = Point16{150, 160};
    g.draw(7);
    REQUIRE(g.out.calls.size() == 1);
    CHECK(g.out.calls[0] == pat_blt_call(kDcWindow, 150, 160, 3, 3, kWhiteness));
}

TEST_CASE("sprite: the alternate source and the no-draw byte +0x2e") {
    Fixture f;
    f.obj().b2e = 1;
    f.next = Point16{140, 100};
    f.draw(1);
    REQUIRE(f.out.calls.size() == 2);
    CHECK(f.out.calls[1] == bit_blt_call(kDcWindow, 140, 100, 32, 32, kAlt, 4, 4, kSrcCopy));
    CHECK(f.obj().src_x == 4);
    CHECK(f.obj().w3a == kSrc);
    // 0x80 and above draw nothing for kinds other than 7, but still move [BYTES seg8:0xcf1].
    for (const std::uint8_t b : {std::uint8_t{0x80}, std::uint8_t{0xfe}, std::uint8_t{0xff}}) {
        Fixture n;
        n.obj().b2e = b;
        n.next = Point16{140, 100};
        CHECK(n.draw(1) == 1);
        CHECK(n.out.calls.empty());
        CHECK(n.obj().scr_x == 140);
        CHECK(n.draw(6) == 1);
        CHECK(n.out.calls.empty());
        CHECK(n.draw(7) == 1);
        CHECK(n.out.calls.size() == 1);
    }
    // The special-wave modes take the alternate source for pool 7 and ship DGROUP:0x952e.
    Fixture s;
    s.pools.rec(7, 1) = s.obj();
    s.g.dg_9e49 = 6;
    s.g.dg_9e47 = 0xf;
    s.next = Point16{140, 100};
    s.draw(1, 0, 7, 1);
    REQUIRE(s.out.calls.size() == 2);
    CHECK(s.out.calls[1].src == kAlt);
}

TEST_CASE("sprite: the pinned player is blitted SRCCOPY in place, no erase") {
    Fixture f;
    ObjectRecord& p = f.player();
    p = f.obj();
    p.scr_x = 512, p.scr_y = 341;
    f.next = Point16{1, 1};  // not used
    CHECK(f.draw(1, 1, kPoolShips, 0) == 1);
    REQUIRE(f.out.calls.size() == 1);
    CHECK(f.out.calls[0] == bit_blt_call(kDcWindow, 512, 341, 32, 32, kSrc, 200, 4, kSrcCopy));
    CHECK(p.old_scr_x == 0);  // untouched
    // Kind 4 does not stop the pinned path.
    f.out.calls.clear();
    CHECK(f.draw(4, 0, kPoolShips, 0) == 1);
    CHECK(f.out.calls.size() == 1);
    // +0x2e = 0xfe: the cell and then the mask with brush DGROUP:0x5ea8.
    Fixture m;
    m.player() = m.obj();
    m.player().scr_x = 512, m.player().scr_y = 341;
    m.player().b2e = 0xfe;
    m.g.player_mask_brush = 0x77;
    m.draw(1, 0, kPoolShips, 0);
    REQUIRE(m.out.calls.size() == 4);
    CHECK(m.out.calls[0] == bit_blt_call(kDcWindow, 512, 341, 32, 32, kSrc, 200, 4, kSrcCopy));
    CHECK(m.out.calls[1].op == BlitCall::kSelectBrush);
    CHECK(m.out.calls[1].brush == 0x77);
    CHECK(m.out.calls[2] == pat_blt_call(kDcWindow, 512, 341, 32, 32, kDpa));
    CHECK(m.out.calls[3].op == BlitCall::kSelectBrush);
    CHECK(m.out.calls[3].brush == kBrushWhite);
    // +0x2e = 0xff draws nothing.
    Fixture n;
    n.player() = n.obj();
    n.player().b2e = 0xff;
    CHECK(n.draw(1, 0, kPoolShips, 0) == 1);
    CHECK(n.out.calls.empty());
    CHECK(n.player().src_x == 4);  // the alternate source was taken
}

TEST_CASE("sprite: DGROUP:0x238e re-pins the player and takes the erase path") {
    Fixture f;
    f.player() = f.obj();
    f.player().scr_x = 500, f.player().scr_y = 300;
    f.player().old_scr_x = 490, f.player().old_scr_y = 300;
    f.g.dg_238e = 1;
    f.draw(1, 0, kPoolShips, 0);
    CHECK(f.g.dg_238e == 0);
    CHECK(f.player().scr_x == 512);  // W / 2
    CHECK(f.player().scr_y == 341);  // H / 2 = 683 / 2
    CHECK(f.player().old_scr_x == 490);  // not saved
    // dx = 512 - 490 = 22 within the size, dy = 41 beyond: the old cell is blacked.
    REQUIRE(f.out.calls.size() == 2);
    CHECK(f.out.calls[0] == pat_blt_call(kDcWindow, 490, 300, 32, 32, kBlackness));
    // While the screen shakes the position stays and DGROUP:0x238e stays set.
    Fixture s;
    s.player() = s.obj();
    s.g.dg_238e = 1;
    s.g.dg_238c = 1;
    s.draw(1, 0, kPoolShips, 0);
    CHECK(s.g.dg_238e == 1);
    CHECK(s.player().scr_x == 100);
    // DGROUP:0x951a != 0: the player moves like any object.
    Fixture v;
    v.player() = v.obj();
    v.g.dg_951a = 1;
    v.next = Point16{200, 210};
    v.draw(1, 0, kPoolShips, 0);
    CHECK(v.player().scr_x == 200);
    CHECK(v.player().old_scr_x == 100);
}

#ifdef _WIN32
TEST_CASE("sprite: the GDI blitter draws on the surface") {
    is::gfx::Gfx gfx(nullptr);
    gfx.set_client_size(64, 48, 1);
    REQUIRE(gfx.logical_width() > 0);
    GdiBlitter b(gfx);
    const DcId img = b.create_image(8, 8);
    REQUIRE(img != kDcNone);
    b.pat_blt(img, 0, 0, 8, 8, kWhiteness);
    b.bit_blt(kDcWindow, 4, 4, 8, 8, img, 0, 0, kSrcCopy);
    b.pat_blt(kDcWindow, 6, 6, 2, 2, kBlackness);
    gfx.surface().flush();
    CHECK(gfx.surface().at(4, 4) == 255);
    CHECK(gfx.surface().at(11, 11) == 255);
    CHECK(gfx.surface().at(6, 6) == 0);
    CHECK(gfx.surface().at(12, 12) == 0);
    // Brushes: select returns the one before, stock ones included.
    const BrushId w = kBrushWhite;
    const BrushId old = b.select_brush(img, w);
    CHECK(old == kBrushWhite);  // a new DC holds WHITE_BRUSH
    const BrushId k = b.select_brush(img, kBrushBlack);
    CHECK(k == kBrushWhite);
    b.pat_blt(img, 0, 0, 2, 2, kPatCopy);
    gfx.surface().flush();
    b.image(img)->flush();
    CHECK(b.image(img)->at(0, 0) == 0);
    CHECK(b.image(img)->at(3, 3) == 255);
    CHECK(b.load_bitmap("NO_SUCH_BITMAP") == kDcNone);  // no asset store
}
#endif

#ifdef _WIN32
// The pass's SelectClipRgn(DGROUP:0x13dc, DGROUP:0x12) on the wave's blits, and the HUD's own
// window DC id outside it [DESIGN ADR 0013] (M3 s2 fixer).
TEST_CASE("sprite: the GDI blitter clips the wave's window draws, not the HUD's") {
    is::gfx::Gfx gfx(nullptr);
    gfx.set_client_size(64, 48, 1);
    REQUIRE(gfx.logical_width() > 0);
    GdiBlitter b(gfx);
    b.set_window_clip(Rect16{0, 0, 32, 24});
    b.pat_blt(kDcWindow, 0, 0, 64, 48, kWhiteness);
    gfx.surface().flush();
    CHECK(gfx.surface().at(10, 10) == 255);
    CHECK(gfx.surface().at(40, 30) == 0);
    b.pat_blt(kDcHudWindow, 0, 0, 64, 48, kWhiteness);
    gfx.surface().flush();
    CHECK(gfx.surface().at(40, 30) == 255);
}
#endif
