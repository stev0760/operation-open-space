// The Navigation window's portable rules (src/nav/nav_rules.h) against docs/re/shell.md "From
// ESC to the wave", steps 11 and 12, and the oracle's window of newgame-0001.
#include "doctest.h"
#include "nav/nav_rules.h"

using namespace is::nav;

TEST_CASE("nav: the defaults, the style and the key writes of seg6:0x2ebc") {
    const Placement p;
    CHECK(p.x == 30);
    CHECK(p.y == 30);
    CHECK(p.w == 560);
    CHECK(p.h == 350);
    CHECK(kStyle == 0x10cc0000ul);
    CHECK(kTitleString == 2140);
    // newgame-0001: a 560 x 350 window at 30,30 writes NavLeft=30 NavRight=560 NavTop=30
    // NavBottom=350, in this order
    const auto w = close_writes(30, 30, 590, 380);
    REQUIRE(w.size() == 4);
    CHECK(w[0] == std::make_pair(std::string("NavLeft"), 30));
    CHECK(w[1] == std::make_pair(std::string("NavRight"), 560));
    CHECK(w[2] == std::make_pair(std::string("NavTop"), 30));
    CHECK(w[3] == std::make_pair(std::string("NavBottom"), 350));
    // a moved and resized window keeps x, y, width and height
    const auto m = close_writes(100, 50, 800, 500);
    CHECK(m[1].second == 700);
    CHECK(m[3].second == 450);
}

TEST_CASE("nav: the layout of the stub's children") {
    // newgame-0001: a 552 x 316 client; Enter 447,5 90 x 26, Help 447,40, the directory list
    // 0,0 160 x 290
    CHECK(enter_box(552, 316) == Box{447, 5, 90, 26});
    CHECK(help_box(552, 316) == Box{447, 40, 90, 26});
    CHECK(dir_list_box(552, 316) == Box{0, 0, 160, 290});
    CHECK(kEnterId == 1);
    CHECK(kHelpId == 0x70);
    CHECK(kDirListId == 0x65);
    CHECK(kDirListStyle == 0x50b30153ul);
}

TEST_CASE("nav: drive roots and Enter's rule seg6:0x301a") {
    CHECK(drive_root(0) == "c:\\");
    CHECK(drive_root(1) == "d:\\");  // newgame-0001's tree root
    CHECK(enter_action(false, 0, 0) == EnterAction::kRefused);
    CHECK(enter_action(true, 0, 0) == EnterAction::kRequestWave);  // a new game
    CHECK(enter_action(true, 5, 1) == EnterAction::kRequestWave);  // a restored game
    CHECK(enter_action(true, 5, 0) == EnterAction::kArmDebriefing);
}
