// The target display's texts, picture and next target (src/hud/target.h): seg3:0x1542's "Icon N"
// and seg3:0xf4's suffix, seg13:0x2206's cone, contact and nearest rules, seg13:0x5a6a /
// seg13:0x5c22 for pool 0, and seg13:0x58dc through Info::target_picture.
#include <map>

#include "doctest.h"
#include "hud/hud.h"
#include "hud/info.h"
#include "hud/target.h"
#include "hud_fakes.h"
#include "shell/geometry.h"

using namespace is::hud;
using hudtest::Call;

TEST_CASE("hud target: seg3:0x1542's pool-0 label and seg3:0xf4's suffix") {
    // string 152 "Icon ^N": the last 2 characters go, the record index follows
    // [ORACLE wave-0001 trace LSTRCAT("Icon ","1")]
    CHECK(icon_label("Icon ^N", 1) == "Icon 1");
    CHECK(icon_label("Icon ^N", 0) == "Icon 0");
    CHECK(icon_label("Icon ^N", 7) == "Icon 7");
    CHECK(icon_label("", 2) == "2");
    CHECK(icon_suffix_id(2) == 2208);  // " (VIRUSED)" [BYTES seg3:0x298]
    CHECK(icon_suffix_id(0) == 2209);  // " (FROZEN)" [BYTES seg3:0x2ca]
    CHECK(icon_suffix_id(1) == 0);     // seg10:0xc2b's icons: no suffix
}

namespace {

struct Scene {
    std::map<std::pair<int, int>, Point> points;
    std::map<std::pair<int, int>, int> dirs, dists;
    std::map<std::pair<int, int>, bool> refused;
    std::map<int, int> in_use;
    NextTargetHost host() {
        NextTargetHost h;
        h.point = [this](int p, int r) { return points[{p, r}]; };
        // the test's direction: looked up by the (dy, dx) seg13:0x22f4 passes
        h.direction16 = [this](int num, int den) { return dirs[{num, den}]; };
        h.accept = [this](int p, int r) { return !refused[{p, r}]; };
        h.distance = [this](int p, int r) {
            auto it = dists.find({p, r});
            return it == dists.end() ? 0x7fff : it->second;
        };
        h.in_use = [this](int p) { return in_use[p]; };
        return h;
    }
};

}  // namespace

TEST_CASE("hud target: seg13:0x2206's heading cone and nearest candidate") {
    Scene s;
    s.points[{14, 0}] = {100, 100};  // the player
    s.points[{0, 1}] = {100, 50};    // dy -50, dx 0
    s.points[{0, 2}] = {150, 100};   // dy 0, dx 50
    s.points[{9, 3}] = {50, 100};    // dy 0, dx -50
    s.dirs[{-50, 0}] = 4;
    s.dirs[{0, 50}] = 6;
    s.dirs[{0, -50}] = 12;
    s.dists[{0, 1}] = 300;
    s.dists[{0, 2}] = 200;
    s.dists[{9, 3}] = 100;
    NextTargetInputs in;
    in.visible = {{14, 0}, {0, 1}, {0, 2}, {9, 3}};
    in.now = 100000;
    in.contact_time = 0;
    // heading 5: |5 - a| < 3 takes directions 4 and 6, not 12; the nearer is record 2
    in.heading = 5;
    CHECK(next_target(in, s.host()) == ((0 << 8) | 2));
    // a refused candidate is passed over
    s.refused[{0, 2}] = true;
    CHECK(next_target(in, s.host()) == ((0 << 8) | 1));
    s.refused[{0, 2}] = false;
    // heading 14 and up: h - a < 3 (signed: every a above h too) or a = 0
    in.heading = 14;
    s.dirs[{-50, 0}] = 15;
    s.dirs[{0, 50}] = 0;
    s.dirs[{0, -50}] = 11;
    CHECK(next_target(in, s.host()) == ((0 << 8) | 2));
    s.dirs[{0, -50}] = 12;  // 14 - 12 = 2 < 3: rock 3 joins and is nearest
    CHECK(next_target(in, s.host()) == ((9 << 8) | 3));
    // heading 2 and below: a - h < 3 (every a below h too) or a = 15
    in.heading = 1;
    s.dirs[{-50, 0}] = 15;  // a = 15
    s.dirs[{0, 50}] = 4;    // 4 - 1 = 3: out
    s.dirs[{0, -50}] = 9;   // out
    CHECK(next_target(in, s.host()) == ((0 << 8) | 1));
}

TEST_CASE("hud target: seg13:0x2206's last contact, fallback and none") {
    Scene s;
    s.points[{14, 0}] = {0, 0};
    s.points[{0, 1}] = {0, -10};
    s.points[{0, 2}] = {0, -20};
    s.dirs[{-10, 0}] = 0;
    s.dirs[{-20, 0}] = 0;
    s.dists[{0, 1}] = 10;
    s.dists[{0, 2}] = 20;
    NextTargetInputs in;
    in.heading = 0;
    in.visible = {{0, 1}, {0, 2}};
    in.contact_pool = 0;
    in.contact_rec = 2;
    in.contact_time = 1000;
    in.now = 1000 + 4999;  // within 5000 ms: the contact wins over the nearer one
    CHECK(next_target(in, s.host()) == 2);
    in.now = 1000 + 5000;  // jae: 5000 is too late
    CHECK(next_target(in, s.host()) == 1);
    // no candidate in the cone: the nearest accepted object of pools 0 ... DGROUP:0x9324,
    // not the player
    in.visible.clear();
    s.in_use[0] = 3;
    s.in_use[14] = 1;
    s.dists[{0, 0}] = 30;
    s.dists[{14, 0}] = 0;  // the player itself is skipped (seg13:0x241c)
    CHECK(next_target(in, s.host()) == 1);
    // none: pool 0xff, the record byte the port fixes at 0xff [DESIGN]
    s.refused[{0, 0}] = s.refused[{0, 1}] = s.refused[{0, 2}] = true;
    CHECK(next_target(in, s.host()) == -1);
}

TEST_CASE("hud target: seg13:0x229e skips only a pool equal to DGROUP:0x9324 and 0x9646") {
    Scene s;
    s.points[{14, 0}] = {0, 0};
    s.points[{14, 1}] = {0, -10};
    s.dirs[{-10, 0}] = 0;
    s.dists[{14, 1}] = 10;
    NextTargetInputs in;
    in.heading = 0;
    in.visible = {{14, 1}};
    in.player = 0;
    CHECK(next_target(in, s.host()) == ((14 << 8) | 1));
    in.player = 14;  // the quirk: ship pool 14 and player 14 skip every pool-14 entry
    s.dists[{14, 1}] = 0x7fff;
    s.in_use[14] = 2;
    s.dists[{14, 0}] = 50;
    CHECK(next_target(in, s.host()) == ((14 << 8) | 0));  // the fallback: record 0 is not 14
}

TEST_CASE("hud target: the packed record byte is sign-extended (cbw)") {
    Scene s;
    s.points[{14, 0}] = {0, 0};
    NextTargetInputs in;
    in.visible.clear();
    s.in_use[3] = 0x81;
    for (int r = 0; r < 0x81; ++r) s.refused[{3, r}] = r != 0x80;
    s.dists[{3, 0x80}] = 5;
    // al = 0x80, cbw -> ah = 0xff, or with ch = 3: 0xff80
    CHECK(next_target(in, s.host()) == static_cast<std::int16_t>(0xff80));
}

TEST_CASE("hud target: seg13:0x5a6a / seg13:0x5c22 for a pool-0 icon") {
    CellInputs in;
    in.x = 2;
    in.y = 17;  // mark 0's origin (seg11:0x2ce6)
    in.pool = 0;
    in.rec = 1;
    in.flag = 1;
    in.obj.extent = 256;
    in.obj.src_y = 6;
    int glint = 2;
    CellPlan p = plan_cell(in, glint);
    CHECK(glint == 0);  // a flag resets the counter
    CHECK(!p.glint);    // and suppresses the glint
    CHECK(p.clear);
    CHECK(p.clear_x == 2);
    CHECK(p.clear_y == 17);
    CHECK(p.clear_size == 45);
    REQUIRE(p.picture);
    CHECK(p.slot == 13);  // the icon strip DGROUP:0x3a02
    CHECK(p.sx == 38 * 1 + 6);
    CHECK(p.sy == 6);
    CHECK(p.sw == 32);
    CHECK(p.sh == 32);
    // (43 - 32) / 2 = 5 into the 43-pixel cell
    CHECK(p.dx == 7);
    CHECK(p.dy == 22);
    CHECK(p.dw == 32);
    CHECK(p.dh == 32);
    // the column's half scale (VScale 1)
    in.x = 0;
    in.y = 46 * 2 + 4;
    in.rec = 2;
    in.scale = 1;
    p = plan_cell(in, glint);
    CHECK(p.clear_x == 0);
    CHECK(p.clear_y == 48);
    CHECK(p.clear_size == 22);
    CHECK(p.dx == 2);
    CHECK(p.dy == (5 + 96) >> 1);
    CHECK(p.dw == 16);
    CHECK(p.sx == 38 * 2 + 6);
    // a captured icon's own image: slot rec + 14 at (w02 / 2 - 1)·38 + 6
    in.icon.w02 = 6;
    p = plan_cell(in, glint);
    CHECK(p.slot == 16);
    CHECK(p.sx == 2 * 38 + 6);
}

TEST_CASE("hud target: seg13:0x5a6a's unflagged calls and glint counter") {
    CellInputs in;
    in.pool = 0;
    in.obj.extent = 256;
    in.flag = 0;
    int glint = 0;
    // 0, 1, 2, 0: the glint runs on every third call
    CellPlan p = plan_cell(in, glint);
    CHECK(glint == 1);
    CHECK(!p.glint);
    p = plan_cell(in, glint);
    CHECK(glint == 2);
    p = plan_cell(in, glint);
    CHECK(glint == 0);
    CHECK(p.glint);
    CHECK(!p.clear);
    // an icon with DGROUP:0x57a8 = 0 and +0x51 = 0 is skipped unflagged
    CHECK(!p.picture);
    in.icon.w00 = 1;
    p = plan_cell(in, glint);
    CHECK(p.picture);
    // Detail < 2 and a dead object have no glint
    glint = 2;
    in.detail = 1;
    CHECK(!plan_cell(in, glint).glint);
    // the ship pool is never drawn unflagged; seg13:0x371c != 0 takes graphic 500
    in.pool = 14;
    CHECK(!plan_cell(in, glint).picture);
    in.pool = 0;
    in.flag = 1;
    in.kind = 1;
    p = plan_cell(in, glint);
    CHECK(p.special);
    CHECK(!p.picture);
    // a dead icon with +0x51 = 0 draws nothing; with DGROUP:0x57c1 the sub-cells
    in.kind = 0;
    in.obj.state = 1;
    p = plan_cell(in, glint);
    CHECK(!p.picture);
    CHECK(!p.sub_cells);
    in.icon.w19 = 1;
    CHECK(plan_cell(in, glint).sub_cells);
}

namespace {
struct Rig {
    hudtest::RecordingCanvas cv;
    hudtest::FakeHost h;
    Info info{cv, h};
    Rig() {
        const is::shell::PlayArea a = is::shell::play_area_from_size(1024, 768, 1024, 768);
        const is::shell::HudValues v = is::shell::hud_values(1024, false);
        info.layout(is::shell::hud_layout(a, is::shell::hud_inputs(v)), v);
        info.set_mode(kModeTarget);
    }
};
}  // namespace

TEST_CASE("hud target: seg13:0x58dc on the target display") {
    Rig r;
    r.info.mark(0).pool = 0;
    r.info.mark(0).rec = 1;
    r.info.paint(kInfoTarget);
    const auto st = r.cv.of(Call::kStretch);
    REQUIRE(st.size() == 1u);
    CHECK(st[0].src == Img::kIconStrip);
    CHECK(st[0].x == 7);   // mark 0's origin (2, 17) + 5
    CHECK(st[0].y == 22);
    CHECK(st[0].w == 32);
    CHECK(st[0].sx == 44);
    CHECK(st[0].rop == kSrcCopy);
    // the 45 x 45 black cell precedes it, over the mark's frame
    bool cleared = false;
    for (const Call& c : r.cv.of(Call::kPat))
        if (c.x == 2 && c.y == 17 && c.w == 45 && c.rop == kBlackness) cleared = true;
    CHECK(cleared);
    // game mode 6 shows no picture in mode 2
    r.cv.calls.clear();
    r.h.game_mode_ = 6;
    r.info.paint(kInfoTarget);
    CHECK(r.cv.of(Call::kStretch).empty());
}

TEST_CASE("hud target: seg13:0x58dc returns the message display to the target display") {
    Rig r;
    r.info.set_mode(kModeMessage);
    r.info.mark(1).pool = 9;
    r.info.mark(1).rec = 4;
    r.h.not_ok[{9, 4}] = true;  // the sender is gone
    r.info.flush();
    r.info.target_picture_now();
    CHECK(r.info.mode() == kModeTarget);
    CHECK(r.info.dirty(kInfoTarget));
    // pools 7 and 12 are exempt
    r.info.set_mode(kModeMessage);
    r.info.mark(1).pool = 7;
    r.h.not_ok[{7, 4}] = true;
    r.info.flush();
    r.info.target_picture_now();
    CHECK(r.info.mode() == kModeMessage);
}
