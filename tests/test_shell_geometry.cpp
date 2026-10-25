// The main window's geometry rules (docs/re/shell.md "Window geometry"), at the oracle's
// 1024 x 768 and at other sizes. The HUD rectangles at 1024 x 768 are newgame-0002's.
#include "doctest.h"
#include "shell/geometry.h"

using namespace is::shell;

TEST_CASE("geometry: the default Show rectangle is 4/5 of the screen, centred") {
    // 1024 x 768: 819 x 614 at (102, 77)
    CHECK(clamp_show_rect({}, 1024, 768) == Rect{102, 77, 921, 691});
    CHECK(clamp_show_rect({}, 800, 600) == Rect{80, 60, 720, 540});
    CHECK(clamp_show_rect({}, 1366, 768) == Rect{137, 77, 1229, 691});
    // ShowRight <= 0 triggers the default whatever the other three hold
    CHECK(clamp_show_rect({5, 6, -3, 400}, 1024, 768) == Rect{102, 77, 921, 691});
    // a rectangle with ShowRight > 0 inside the screen stays
    CHECK(clamp_show_rect({10, 20, 300, 200}, 1024, 768) == Rect{10, 20, 300, 200});
}

TEST_CASE("geometry: the right-edge and the asymmetric bottom-edge corrections") {
    // past the right edge by 176: moved left by the excess
    CHECK(clamp_show_rect({500, 100, 1200, 400}, 1024, 768) == Rect{324, 100, 1024, 400});
    // past the bottom by 132: ShowBottom loses twice the excess, ShowTop stays
    CHECK(clamp_show_rect({100, 500, 600, 900}, 1024, 768) == Rect{100, 500, 600, 636});
    // both
    CHECK(clamp_show_rect({0, 0, 900, 700}, 800, 600) == Rect{-100, 0, 800, 500});
    // exactly at the edges: no correction
    CHECK(clamp_show_rect({0, 0, 1024, 768}, 1024, 768) == Rect{0, 0, 1024, 768});
}

TEST_CASE("geometry: play area and HUD strip from WM_SIZE and from the client rectangle") {
    PlayArea a = play_area_from_size(1024, 768, 1024, 768);
    CHECK(a.w == 1024);
    CHECK(a.h == 768);
    CHECK(a.play_h == 683);
    CHECK(a.hud_top == 683);
    // WM_SIZE clamps to the screen (unsigned minimum)
    a = play_area_from_size(1600, 1200, 1024, 768);
    CHECK(a.w == 1024);
    CHECK(a.h == 768);
    a = play_area_from_size(640, 480, 1024, 768);
    CHECK(a.w == 640);
    CHECK(a.h == 480);
    CHECK(a.play_h == 395);
    CHECK(a.hud_top == 395);
    // the client-rectangle form takes the screen size when the screen is not greater
    a = play_area_from_client(811, 557, 1024, 768);
    CHECK(a.w == 811);
    CHECK(a.h == 557);
    CHECK(a.play_h == 472);
    a = play_area_from_client(2000, 768, 1024, 768);
    CHECK(a.w == 1024);
    CHECK(a.h == 768);
}

TEST_CASE("geometry: WM_SIZE moves a full-screen window back unless it covers the screen") {
    CHECK_FALSE(needs_full_screen_move(true, {0, 0, 1024, 768}, 1024, 768));
    CHECK(needs_full_screen_move(true, {0, 0, 800, 600}, 1024, 768));
    CHECK(needs_full_screen_move(true, {1, 0, 1025, 768}, 1024, 768));
    CHECK_FALSE(needs_full_screen_move(false, {0, 0, 800, 600}, 1024, 768));
}

TEST_CASE("geometry: clip rectangle with the Border and PlayMode insets") {
    const PlayArea a = play_area_from_size(1024, 768, 1024, 768);
    CHECK(border_inset(1024) == 68);  // 1024 mod 58 = 38: 78 - 10
    Clip c = clip_rect(a, true, false);
    CHECK(c.play == Rect{68, 78, 956, 663});
    CHECK(c.text_line == Box{68, 643, 888, 20});
    c = clip_rect(a, true, true);
    CHECK(c.play == Rect{68, 78, 956, 643});
    c = clip_rect(a, false, false);
    CHECK(c.play == Rect{0, 0, 1024, 683});
    CHECK(c.text_line == Box{0, 663, 1024, 20});
    c = clip_rect(a, false, true);
    CHECK(c.play == Rect{0, 0, 1024, 663});

    const PlayArea b = play_area_from_size(800, 600, 1024, 768);
    CHECK(border_inset(800) == 72);  // 800 mod 58 = 46: 78 - 6
    c = clip_rect(b, true, false);
    CHECK(c.play == Rect{72, 78, 728, 495});
    CHECK(c.text_line == Box{72, 475, 656, 20});
    CHECK(border_inset(58 * 14) == 49);  // a whole number of tiles: 78 - 29
}

TEST_CASE("geometry: HUD rectangles at 1024 x 768 as newgame-0002 recorded them") {
    const PlayArea a = play_area_from_size(1024, 768, 1024, 768);
    const HudLayout h = hud_layout(a, {142, 282, 668, 352});
    CHECK(h.scrollbar == Box{562, 683, 14, 85});
    CHECK(h.info1 == Box{147, 688, 273, 76});
    CHECK(h.info2 == Box{673, 688, 343, 76});
    CHECK(h.fill == Box{0, 683, 1024, 85});

    const PlayArea b = play_area_from_size(800, 600, 1024, 768);
    const HudLayout g = hud_layout(b, {100, 200, 500, 250});
    CHECK(g.scrollbar == Box{450, 515, 14, 85});
    CHECK(g.info1 == Box{105, 520, 191, 76});
    CHECK(g.fill == Box{0, 515, 800, 85});
}

TEST_CASE("geometry: the mode-switch style constants") {
    CHECK((kStyleWindowed & kFullScreenMask) == kStyleFull);
    CHECK((kStyleFull | kWindowedBits) == kStyleWindowed);
}
