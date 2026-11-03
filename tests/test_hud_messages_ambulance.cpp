// M4 stage 3, track AMBFLIGHT: the ambulance's replies of seg3:0x6ec (codes 7, 8, 132) and the
// low-fuel warning (code 9) (src/hud/messages.h; s3-ambflight.md 1.4, verified claim 2;
// s3-fuel.md 2.2, verified claim 16).
#include <map>
#include <string>
#include <vector>

#include "doctest.h"
#include "hud/messages.h"

using namespace is::hud;

namespace {

// [BYTES ENG_LANG.DLL STRING 7] id 110 [BYTES ENG_LANG.DLL STRING 8] id 121 [BYTES ENG_LANG.DLL
// STRING 22] ids 347-348 [BYTES ENG_LANG.DLL STRING 72] id 1136
const std::map<int, std::string> kStrings = {
    {110, "Hold Your Horses, %s. Rescue Unit Dispatched. "},
    {121, "FUEL LEVEL LOW. SEEK FUEL ASAP"},
    {347, "Ambulance^N"},
    {348, "Sorry! Ambulance Service Is Busy At This Time."},
    {1136, "Sorry! You Have No Credit. Help Is Not Coming."},
};

class AmbHost : public MessageHost {
public:
    std::uint32_t end = 0;
    std::vector<int> sounds, todos;
    std::string lang(int id) override {
        auto it = kStrings.find(id);
        return it == kStrings.end() ? std::string() : it->second;
    }
    std::uint32_t now() const override { return 20000; }
    std::string pilot() const override { return "PILOT"; }
    std::string name(int pool, int) override { return pool == 7 ? cut_name_code(lang(347)) : "?"; }
    void set_end(std::uint32_t t) override { end = t; }
    void sound(int id) override { sounds.push_back(id); }
    void todo(int code) override { todos.push_back(code); }
};

struct Shown {
    std::string l1, l2;
    std::vector<int> sounds;
};

Shown show(int code) {
    MessageState s;
    AmbHost h;
    dispatch_message(s, h, 0, 7, 0, code);
    CHECK(h.todos.empty());
    CHECK(s.sender == "Ambulance");
    CHECK(h.end == 27000u);  // now + 7000 outside a race
    return Shown{s.line1, s.line2, h.sounds};
}

}  // namespace

TEST_CASE("hud messages: the ambulance's replies 7, 8 and 132 and the low-fuel warning 9") {
    for (int c : {7, 8, 9, 132}) CHECK(message_code_ported(c));
    // Code 7: RESCUE.WAV (0x33) and string 110 with the pilot name, split and upper-cased.
    const Shown a = show(7);
    CHECK(a.sounds == std::vector<int>{0x33});
    CHECK(a.l1 + a.l2 == "HOLD YOUR HORSES, PILOT. RESCUE UNIT DISPATCHED. ");
    // Code 8: WARNING.WAV (0x2c) and string 348.
    const Shown b = show(8);
    CHECK(b.sounds == std::vector<int>{0x2c});
    CHECK(b.l1 + b.l2 == "SORRY! AMBULANCE SERVICE IS BUSY AT THIS TIME.");
    // Code 132: 0x2c and string 1136.
    const Shown n = show(132);
    CHECK(n.sounds == std::vector<int>{0x2c});
    CHECK(n.l1 + n.l2 == "SORRY! YOU HAVE NO CREDIT. HELP IS NOT COMING.");
    // Code 9: 0x2c and string 121.
    const Shown f = show(9);
    CHECK(f.sounds == std::vector<int>{0x2c});
    CHECK(f.l1 + f.l2 == "FUEL LEVEL LOW. SEEK FUEL ASAP");
}

TEST_CASE("hud messages: the split of the dispatch reply (seg3:0x3498)") {
    const Shown a = show(7);
    // 46 characters (+ name): the space nearest the middle ends line 1.
    CHECK_FALSE(a.l1.empty());
    CHECK_FALSE(a.l2.empty());
    CHECK(a.l1.back() == ' ');
}
