// The IS_INFO displays (docs/re/world.md "IS_INFO displays", ADR 0013 decision 4): their
// rectangles at 1024 x 768, the paint of the target, message, frame-rate and weapon displays,
// the dirty mark that stands for PostMessage(WM_PAINT), the direction marks and their 280 ms
// deadline, the attitude text, and the target codes seg11:0x772e / seg3:0x65e / seg3:0x45a.
#include "doctest.h"
#include "hud/hud.h"
#include "hud/info.h"
#include "hud_fakes.h"
#include "shell/geometry.h"

using namespace is::hud;
using namespace is::shell;
using hudtest::Call;
using hudtest::FakeHost;
using hudtest::RecordingCanvas;

namespace {
struct Rig {
    RecordingCanvas cv;
    FakeHost h;
    Info info{cv, h};
    Rig() {
        const PlayArea a = play_area_from_size(1024, 768, 1024, 768);
        const HudValues v = hud_values(1024, false);
        info.layout(hud_layout(a, hud_inputs(v)), v);
        info.set_mode(kModeTarget);
    }
    std::vector<Call> texts() const { return cv.of(Call::kText); }
};
}  // namespace

TEST_CASE("hud info: the panels' rectangles are the oracle's IS_INFO windows") {
    Rig r;
    CHECK(r.info.dirty(kInfoTarget));
    CHECK(r.info.dirty(kInfoWeapon));
    r.info.flush();
    CHECK(!r.info.dirty(kInfoTarget));
    CHECK(!r.info.dirty(kInfoWeapon));
    const auto views = r.cv.of(Call::kView);
    REQUIRE(views.size() == 2u);
    CHECK(views[0].x == 147);
    CHECK(views[0].y == 688);
    CHECK(views[0].w == 273);
    CHECK(views[0].h == 76);
    CHECK(views[1].x == 673);
    CHECK(views[1].y == 688);
    CHECK(views[1].w == 343);
    CHECK(views[1].h == 76);
    // each paint starts with a black client area
    CHECK(r.cv.calls[1].op == Call::kPat);
    CHECK(r.cv.calls[1].rop == kBlackness);
    CHECK(r.cv.calls[1].w == 273);
    // a PostMessage(WM_PAINT) marks; the next flush paints that one only
    r.cv.calls.clear();
    r.info.invalidate(kInfoWeapon);
    r.info.flush();
    REQUIRE(r.cv.of(Call::kView).size() == 1u);
    CHECK(r.cv.of(Call::kView)[0].x == 673);
    // after seg11:0x2f0 nothing paints
    r.cv.calls.clear();
    r.info.close();
    r.info.invalidate(kInfoTarget);
    r.info.flush();
    CHECK(r.cv.calls.empty());
}

TEST_CASE("hud info: the target display") {
    Rig r;
    r.info.mark(0).pool = 0;
    r.info.mark(0).rec = 1;
    r.info.paint(kInfoTarget);
    const auto t = r.texts();
    REQUIRE(t.size() == 4u);
    CHECK(t[0].text == "CURRENT TARGET  (MANUAL)");
    CHECK(t[0].x == 1);
    CHECK(t[0].y == 0);
    CHECK(t[0].colour == 0x0000FF00);
    CHECK(t[1].text == "ALPHA.EXE");
    CHECK(t[1].align == static_cast<int>(kTaRight));
    CHECK(t[1].x == 282 - 15);
    CHECK(t[1].y == 18);
    CHECK(t[1].colour == 0x0000FF00);
    CHECK(t[2].text == "Icon 0");
    CHECK(t[2].y == 32);
    CHECK(t[2].colour == 0x00008000);  // not a ship
    CHECK(t[3].y == 46);
    // the icon is drawn after the mark, and the rule closes the panel
    REQUIRE(r.h.trace.size() == 1u);
    CHECK(r.h.trace[0] == "icon");
    const Call rule = r.cv.of(Call::kLines).back();
    CHECK(rule.pen.colour == 0x00008000);
    CHECK(rule.pts[0].y == 14);
    CHECK(rule.pts[1].x == 273);
    // AUTO, a ship target with its attitude, and a narrow window
    r.cv.calls.clear();
    r.info.set_manual(false);
    r.info.mark(0).pool = 14;
    r.info.mark(0).rec = 2;
    r.h.attitude_ = -100;
    r.info.paint(kInfoTarget);
    const auto s = r.texts();
    REQUIRE(s.size() == 5u);
    CHECK(s[0].text == "CURRENT TARGET  (AUTO)");
    CHECK(s[2].colour == 0x02006EFF);
    CHECK(s[4].y == 60);
    CHECK(s[4].text == "#581 -10");  // string 581 for -250 <= r < -80, " ", r / 10
    // an object that is no target bumps DGROUP:0x9498
    r.h.not_ok[{14, 2}] = true;
    r.info.paint(kInfoTarget);
    CHECK(r.h.bad_index == 1);
    // in a race the race display replaces it
    r.h.race_ = true;
    r.h.trace.clear();
    r.info.paint(kInfoTarget);
    CHECK(r.h.trace == std::vector<std::string>{"race"});
}

TEST_CASE("hud info: the target title below 700 drops CURRENT") {
    RecordingCanvas cv;
    FakeHost h;
    h.area_ = play_area_from_size(640, 480, 1024, 768);
    Info info(cv, h);
    const HudValues v = hud_values(640, false);
    info.layout(hud_layout(h.area_, hud_inputs(v)), v);
    info.set_mode(kModeTarget);
    info.paint(kInfoTarget);
    const auto t = cv.of(Call::kText);
    REQUIRE(!t.empty());
    CHECK(t[0].text == "TARGET  (MANUAL)");
    CHECK(t[1].x == v.target_w - 12);
}

TEST_CASE("hud info: the message display and the frame-rate display") {
    Rig r;
    r.info.flush();  // the new windows' first paint
    r.info.show_messages();
    CHECK(r.info.mode() == kModeMessage);
    CHECK(r.info.dirty(kInfoTarget));
    r.info.mark(1).pool = 14;
    r.info.mark(1).rec = 3;
    r.h.attitude_ = 100;
    r.cv.calls.clear();
    r.info.flush();
    auto t = r.texts();
    REQUIRE(t.size() == 5u);
    CHECK(t[0].text == "MESSAGE from");
    CHECK(t[1].text == "Ambulance");
    CHECK(t[1].colour == 0x02FFB900);
    CHECK(t[2].y == 32);
    CHECK(t[3].text == "GOOD ICON CAPTURE!");
    CHECK(t[3].colour == 0x0000FFFF);
    CHECK(t[4].y == 60);
    // its own rule to x 80, and no full-width rule
    const auto lines = r.cv.of(Call::kLines);
    CHECK(lines.back().pts[1].x == 80);
    // the sender is the player: no attitude line
    r.cv.calls.clear();
    r.info.mark(1).rec = 0;
    r.info.paint(kInfoTarget);
    CHECK(r.texts().size() == 4u);
    // M again does not repaint
    r.info.flush();
    r.info.show_messages();
    CHECK(!r.info.dirty(kInfoTarget));

    // B: mode 4
    r.cv.calls.clear();
    r.info.show_frame_rate();
    r.info.flush();
    t = r.texts();
    REQUIRE(t.size() == 3u);
    CHECK(t[0].text == "GAME STATUS");
    CHECK(t[1].text == "36 frames/ second SET");
    CHECK(t[1].y == 22);
    CHECK(t[2].text == "35 frames/ second ACTUAL");
    CHECK(t[2].y == 38);
    r.cv.calls.clear();
    r.h.fr_set_ = 1200;
    r.info.paint(kInfoTarget);
    CHECK(r.texts()[1].text == "FULL OUT");
    // the measurement repaints mode 4 only
    r.info.frame_rate_measured();
    CHECK(r.info.dirty(kInfoTarget));
}

TEST_CASE("hud info: the weapon display") {
    Rig r;
    r.h.weapon_ = WeaponView{0xc3, true, false, 20};
    r.cv.sizes[Img::kWeapon] = {40, 12};
    r.info.paint(kInfoWeapon);
    auto t = r.texts();
    REQUIRE(t.size() == 2u);
    CHECK(t[0].text == "WEAPON SYSTEM");
    CHECK(t[0].colour == 0x000000FF);
    CHECK(t[1].text == "20 - #195");
    CHECK(t[1].y == 17);
    const Call rule = r.cv.of(Call::kLines).back();
    CHECK(rule.pen.colour == 0x00FF00FF);
    CHECK(rule.pts[0].y == 15);
    CHECK(rule.pts[1].x == 343);
    const Call pic = r.cv.of(Call::kBlt).back();
    CHECK(pic.src == Img::kWeapon);
    CHECK(pic.x == 352 / 2 - 20);
    CHECK(pic.y == 28);
    CHECK(pic.rop == kSrcPaint);
    // uncounted: FUEL - unless the name is 0xc9, 0xba or 0xbc; 0xcb's picture at 40
    r.cv.calls.clear();
    r.h.weapon_ = WeaponView{0xcb, false, false, 0};
    r.info.paint(kInfoWeapon);
    CHECK(r.texts()[1].text == "FUEL - #203");
    CHECK(r.cv.of(Call::kBlt).back().y == 40);
    r.cv.calls.clear();
    r.h.weapon_ = WeaponView{0xba, true, true, 3};
    r.info.paint(kInfoWeapon);
    CHECK(r.texts()[1].text == "#186");
    CHECK(r.cv.of(Call::kBlt).back().y == 28);
}

TEST_CASE("hud info: the direction marks") {
    Point p[3];
    CHECK(mark_segment(0, p) == 2);
    CHECK(p[0].x == 14);
    CHECK(p[1].x == 29);
    CHECK(mark_segment(3, p) == 3);
    CHECK(p[1].x == 43);
    CHECK(p[1].y == 43);
    CHECK(mark_segment(8, p) == 0);
    CHECK(mark_segment(-1, p) == 0);
    // seg8:0x1290: one Newton step from max(guess, 3)
    CHECK(newton_root(0, 0) == 1);
    CHECK(newton_root(7, 25) == 5);   // (25 / 7 + 7) >> 1
    CHECK(newton_root(140, 10000) == 105);

    Rig r;
    r.h.target_pool_ = 0;
    r.h.target_rec_ = 1;
    r.h.player_point_ = {512, 341};
    r.h.point_ = {512 + 200, 341};
    r.h.dir16_ = 4;
    r.info.update_mark(0);
    CHECK(r.info.mark(0).dir == 2);
    CHECK(r.info.mark(0).colour == 0x02FFFFFF);  // (200 / 3 + 3)... < 300
    CHECK(r.info.mark(0).lit == 1);
    // drawn on the left panel at origin (2, 17): first the old direction 0 in black, then 2
    const auto lines = r.cv.of(Call::kLines);
    REQUIRE(lines.size() == 2u);
    CHECK(lines[0].pen.is_stock);
    CHECK(lines[0].pts[0].x == 16);
    CHECK(lines[0].pts[0].y == 17);
    CHECK(lines[1].pen.colour == 0x02FFFFFF);
    CHECK(lines[1].pts[0].x == 45);
    CHECK(lines[1].pts[0].y == 31);
    // the next update blinks it black
    r.cv.calls.clear();
    r.info.update_mark(0);
    CHECK(r.info.mark(0).lit == 0);
    CHECK(r.cv.of(Call::kLines).back().pen.is_stock);
    // distance bands: yellow, green, dark green
    r.h.point_ = {512 + 400, 341};
    r.info.update_mark(0);
    CHECK(r.info.mark(0).colour == 0x0200FFFF);
    r.h.point_ = {512 + 600, 341};
    r.info.update_mark(0);
    CHECK(r.info.mark(0).colour == 0x0200FF00);
    r.h.point_ = {512 + 900, 341};
    r.info.update_mark(0);
    CHECK(r.info.mark(0).colour == 0x02007200);
    // no target, or a race: nothing
    r.cv.calls.clear();
    r.h.target_pool_ = -1;
    r.info.update_mark(0);
    CHECK(r.cv.calls.empty());
}

TEST_CASE("hud info: the 280 ms deadline of the wave frame") {
    Rig r;
    r.h.target_pool_ = 0;
    r.info.frame_step(100);
    CHECK(r.info.deadline() == 380);
    r.cv.calls.clear();
    r.info.frame_step(380);  // not past it
    CHECK(r.cv.calls.empty());
    r.info.frame_step(381);
    CHECK(r.info.deadline() == 661);
    CHECK(!r.cv.calls.empty());
    // the message mode updates mark 1
    r.info.set_mode(kModeMessage);
    r.info.mark(1).pool = -1;
    r.cv.calls.clear();
    r.info.frame_step(700);
    CHECK(r.cv.calls.empty());
}

TEST_CASE("hud info: the attitude text") {
    Rig r;
    r.info.attitude_text(300);
    CHECK(r.info.text_93e0() == "#584 +30");
    r.info.attitude_text(0);
    CHECK(r.info.text_93e0() == "#582 0");
    r.info.attitude_text(-251);
    CHECK(r.info.text_93e0() == "#580 -25");
    r.info.attitude_text(80);
    CHECK(r.info.text_93e0() == "#583 +8");
}

TEST_CASE("hud info: the target codes") {
    Rig r;
    // seg3:0x65e
    CHECK(r.info.code_accepts(5, 1, 0));
    CHECK(!r.info.code_accepts(14, 0, 0));  // the player
    CHECK(r.info.code_accepts(14, 1, 1));
    CHECK(!r.info.code_accepts(0, 1, 1));
    CHECK(r.info.code_accepts(0, 1, 2));
    CHECK(r.info.code_accepts(11, 1, 2));
    CHECK(!r.info.code_accepts(8, 1, 3));
    CHECK(r.info.code_accepts(9, 0, 4));
    CHECK(r.info.code_accepts(2, 0, 5));
    CHECK(r.info.code_accepts(3, 0, 9));  // above 5: any target
    r.h.not_ok[{0, 1}] = true;
    CHECK(!r.info.code_accepts(0, 1, 2));
    r.h.not_ok.clear();

    // I (code 2) from BRAVO (pool 0 record 1) steps to the next icon, then wraps
    r.h.in_use_ = {{0, 3}};
    r.info.mark(0).pool = 0;
    r.info.mark(0).rec = 1;
    r.info.target_code(false, 2);
    REQUIRE(r.h.targets.size() == 1u);
    CHECK(r.h.targets[0] == std::vector<int>{0, 0, 2});
    r.info.mark(0).rec = 2;
    r.info.target_code(false, 2);
    CHECK(r.h.targets[1] == std::vector<int>{0, 0, 0});
    // a target that does not suit the code: the nearest suitable one within 20000
    r.info.mark(0).pool = 5;
    r.info.mark(0).rec = 0;
    r.h.dist_ = {{{0, 0}, 900}, {{0, 1}, 300}, {{0, 2}, 600}};
    r.info.target_code(false, 2);
    CHECK(r.h.targets[2] == std::vector<int>{0, 0, 1});
    // nothing suits: pool 12 record 1
    r.h.in_use_.clear();
    r.info.target_code(false, 1);
    CHECK(r.h.targets[3] == std::vector<int>{0, 12, 1});
    // from another mode: mode 2 and a repaint; a target that suits ends it
    r.info.set_mode(kModeMessage);
    r.info.flush();
    r.info.mark(0).pool = 0;
    r.info.mark(0).rec = 1;
    r.info.target_code(false, 2);
    CHECK(r.info.mode() == kModeTarget);
    CHECK(r.info.dirty(kInfoTarget));
    CHECK(r.h.targets.size() == 4u);
    // Ctrl in mode 2 restarts from pool 3 record 0
    r.h.in_use_ = {{0, 3}, {3, 2}};
    r.info.target_code(true, 0);
    CHECK(r.h.targets.back() == std::vector<int>{0, 3, 1});
    CHECK(r.info.manual());
}

TEST_CASE("hud info: Ctrl+T toggles MANUAL and retargets when it turns on") {
    Rig r;
    r.info.flush();
    CHECK(r.info.manual());
    r.info.toggle_manual();
    CHECK(!r.info.manual());
    CHECK(r.h.targets.empty());
    CHECK(r.info.dirty(kInfoTarget));
    r.info.flush();
    r.info.toggle_manual();
    CHECK(r.info.manual());
    REQUIRE(r.h.targets.size() == 1u);
    // FakeHost's next_target is Host's -1: pool 0xff, record 0xff, as the bytes split it
    CHECK(r.h.targets[0] == std::vector<int>{0, 0xff, 0xff});
    CHECK(r.info.dirty(kInfoTarget));
}
