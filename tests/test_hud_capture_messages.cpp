// M4 stage 2, track DISPLAY: the capture's message codes of seg3:0x6ec (src/hud/messages.h), the
// score panel's text (src/hud/panels.h score_text) and seg3:0x0 / seg13:0x371c (src/hud/target.h).
#include <map>
#include <string>
#include <vector>

#include "doctest.h"
#include "hud/messages.h"
#include "hud/panels.h"
#include "hud/target.h"

using namespace is::hud;

namespace {

// [BYTES ENG_LANG.DLL STRING 36] ids 574-575 [BYTES ENG_LANG.DLL STRING 41] id 652 [BYTES
// ENG_LANG.DLL STRING 43] id 680 [BYTES ENG_LANG.DLL STRING 67] id 1062 [BYTES ENG_LANG.DLL
// STRING 159] id 2530 [BYTES ENG_LANG.DLL STRING 784] id 12530 [BYTES ENG_LANG.DLL STRING 22]
// id 347.
const std::map<int, std::string> kStrings = {
    {347, "Ambulance"},
    {574, "Good Icon Capture! %s Resource Gained."},
    {575, "Good Icon Capture! %# Acquired."},
    {652, "You're In Trouble! You Captured A Virused Icon!"},
    {680, "Resource Pack Captured! %s Resource Gained."},
    {1062, "I've Captured An Icon. Score 1 For The %s."},
    {2530, "Defender"},
    {12530, "Defenders"},
};

class CapHost : public MessageHost {
public:
    int value = 100, defenders = 0;
    std::uint32_t end = 0;
    std::vector<int> sounds, todos;
    std::string lang(int id) override {
        auto it = kStrings.find(id);
        return it == kStrings.end() ? std::string() : it->second;
    }
    std::uint32_t now() const override { return 10000; }
    std::string name(int pool, int) override { return pool == 7 ? "Ambulance" : "?"; }
    void set_end(std::uint32_t t) override { end = t; }
    void sound(int id) override { sounds.push_back(id); }
    void todo(int code) override { todos.push_back(code); }
    int capture_value() const override { return value; }
    int capture_defenders() const override { return defenders; }
    std::string team_name(int) override { return "Red Team"; }
};

}  // namespace

TEST_CASE("hud capture messages: 0x55 shows the oracle's lines for 1.0, 0.79 and 0.34") {
    // core-0002: "GOOD ICON CAPTURE! / 1.0 RESOURCE GAINED." for 7.0-7.2 s [ORACLE core-0002]
    const std::pair<int, const char*> cases[] = {{100, "1.0 RESOURCE GAINED."},
                                                 {79, "0.79 RESOURCE GAINED."},
                                                 {34, "0.34 RESOURCE GAINED."}};
    for (const auto& c : cases) {
        MessageState s;
        CapHost h;
        h.value = c.first;
        dispatch_message(s, h, 0, 7, 0, 0x55);
        CHECK(s.sender == "Ambulance");
        CHECK(s.line1 == "GOOD ICON CAPTURE! ");
        CHECK(s.line2 == c.second);
        CHECK(h.end == 10000u + 7000u);  // [BYTES seg3:0x7f3]
        CHECK(h.sounds.empty());         // the capture's 0x23 is the touch's seg10:0x754a
        CHECK(h.todos.empty());
    }
}

TEST_CASE("hud capture messages: 0x55 with defenders takes 575 and the %# plural") {
    MessageState s;
    CapHost h;
    h.defenders = 1;
    dispatch_message(s, h, 0, 7, 0, 0x55);
    CHECK(s.text == "Good Icon Capture! 1 Defender Acquired.");
    h.defenders = 3;
    dispatch_message(s, h, 0, 7, 0, 0x55);
    CHECK(s.text == "Good Icon Capture! 3 Defenders Acquired.");
}

TEST_CASE("hud capture messages: 0x48 sounds 0x67, 0x56 and 0x6f format their strings") {
    MessageState s;
    CapHost h;
    dispatch_message(s, h, 0, 7, 0, 0x48);
    CHECK(h.sounds == std::vector<int>{0x67});
    CHECK(s.text == "You're In Trouble! You Captured A Virused Icon!");
    h.value = 150;
    dispatch_message(s, h, 0, 7, 0, 0x56);
    CHECK(s.text == "Resource Pack Captured! 1.50 Resource Gained.");
    dispatch_message(s, h, 0, 2, 3, 0x6f);
    CHECK(s.text == "I've Captured An Icon. Score 1 For The Red Team.");
    CHECK(h.todos.empty());
    CHECK(message_code_ported(0x48));
    CHECK(message_code_ported(0x55));
    CHECK(message_code_ported(0x56));
    CHECK(message_code_ported(0x6f));
}

TEST_CASE("hud score: seg11:0x4ea's text through seg6:0xeb8") {
    CHECK(score_text(0) == "000");        // DGROUP:0xa9e [ORACLE core-0002 score-000.png]
    CHECK(score_text(900) == "900");      // [ORACLE core-0002 score-900-kill.png]
    CHECK(score_text(4000) == "4000");
    CHECK(score_text(8000) == "8000");
    CHECK(score_text(0xffffff9cu) == "-100");  // a negative dword: "-" and the negation
    CHECK(score_text(0x80000000u) == "-2147483648");
    CHECK(score_text(0x7fffffffu) == "2147483647");
}

TEST_CASE("hud target: seg13:0x371c and seg3:0x0") {
    CHECK(target_kind_371c(0, 3) == 0);
    CHECK(target_kind_371c(12, 0) == 1);
    CHECK(target_kind_371c(12, 1) == 2);
    std::map<std::pair<int, int>, int> st;
    TargetValidity v;
    v.ship_pool = 14;
    v.player = 0;
    v.allocated = [](int p) { return p == 0 ? 8 : p == 2 ? 2 : p == 12 ? 2 : p == 14 ? 2 : p == 8 ? 1 : 0; };
    v.state = [&st](int p, int r) { auto it = st.find({p, r}); return it == st.end() ? 0 : it->second; };
    int hide = 0;
    v.ship_147 = [&hide](int) { return hide; };
    v.def_00 = [](int r) { return r == 1 ? 15 : 0; };
    v.def_pool = [](int) { return -1; };
    v.def_rec = [](int) { return 0; };
    v.gate_post_live = [](int) { return true; };
    CHECK(target_valid(v, 0, 1));
    st[{0, 1}] = 1;  // a captured icon: `state` 1
    CHECK(!target_valid(v, 0, 1));
    CHECK(!target_valid(v, 0, 8));   // at the allocated count
    CHECK(!target_valid(v, -1, 0));
    CHECK(!target_valid(v, 15, 0));  // above DGROUP:0x9324
    CHECK(!target_valid(v, 14, 0));  // the player: kind 0
    CHECK(target_valid(v, 14, 1));   // another ship, `state` 0
    st[{14, 1}] = 2;
    CHECK(!target_valid(v, 14, 1));
    hide = 1;                        // ship byte +0x147
    CHECK(target_valid(v, 14, 1));
    CHECK(!target_valid(v, 2, 0));
    CHECK(target_valid(v, 2, 1));    // DGROUP:0x52fe + 0x24 = 15
    CHECK(target_valid(v, 8, 0));
    CHECK(!target_valid(v, 12, 0));  // even exit post: kind 1
    CHECK(target_valid(v, 12, 1));   // odd: kind 2
}

TEST_CASE("hud target: a shattered icon's column row draws four quarters") {
    // seg13:0x5f49-0x6057 for flag 1, scale 0: quarters 22 apart, 20 x 20, from the fragments of
    // pool 1 whose icon word is the row's record (core-0002 shatter: four quarter pictures).
    CellInputs in;
    in.x = 0;
    in.y = 46 * 1 + 4;
    in.pool = 0;
    in.rec = 1;
    in.flag = 1;
    in.obj.state = 1;
    in.icon.w19 = 1;
    in.frag_size = 16;
    in.frag_gap = 5;
    for (int i = 0; i < 4; ++i) in.frags[i] = {i + 4, 0, 1};
    in.frags[2].state = 1;   // a fragment gone: that quarter is blacked
    in.frags[3].parent = 0;  // another icon's fragment
    int glint = 0;
    const CellPlan p = plan_cell(in, glint);
    CHECK(p.clear);
    CHECK(p.sub_cells);
    CHECK(!p.picture);
    CHECK(p.frag_sx == 5);
    CHECK(p.frag_size == 16);
    CHECK(p.subs[0].x == 0);
    CHECK(p.subs[0].y == 50);
    CHECK(p.subs[1].x == 22);
    CHECK(p.subs[2].y == 72);
    CHECK(p.subs[3].x == 22);
    CHECK(p.subs[3].y == 72);
    CHECK(p.subs[0].side == 20);
    CHECK(p.subs[0].draw);
    CHECK(p.subs[0].frag == 4);
    CHECK(p.subs[1].draw);
    CHECK(!p.subs[2].draw);
    CHECK(!p.subs[3].draw);
    // VScale 1 halves positions and sides
    in.scale = 1;
    const CellPlan q = plan_cell(in, glint);
    CHECK(q.subs[1].x == 11);
    CHECK(q.subs[0].side == 10);
    // DGROUP:0x57c1 = 0: a dissolving icon (+0x51 != 0) draws its strip cell, a finished one none
    in.scale = 0;
    in.icon.w19 = 0;
    in.obj.b51 = 3;
    CHECK(plan_cell(in, glint).picture);
    in.obj.b51 = 0;
    CHECK(!plan_cell(in, glint).picture);
}
