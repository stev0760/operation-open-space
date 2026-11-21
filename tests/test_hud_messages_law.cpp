// M5 stage 4 integrate: seg3:0x6ec's lines of the arrest and the warrant (src/hud/messages.cpp):
// codes 0x3d (552) and 0x57 (578) share seg3:0xf61 (cases 60 and 86 of the table seg3:0x1356,
// docs/re/ship-contact.md "The taunts and warnings of seg3:0xf61"), code 0xae (1940) is
// seg3:0x12cd's push 2 / push 0x794 [BYTES seg3:0x13ce seg3:0x1402 seg3:0x14b0 seg3:0x12cd-0x12d2].
#include <map>
#include <string>
#include <vector>

#include "doctest.h"
#include "hud/messages.h"

using namespace is::hud;

namespace {

// [BYTES ENG_LANG.DLL STRING 35] ids 552, 553, [BYTES ENG_LANG.DLL STRING 37] id 578,
// [BYTES ENG_LANG.DLL STRING 122] id 1940 (work/assets eng_lang.dll strings.tsv)
const std::map<int, std::string> kStrings = {
    {552, "Follow Me Out. And Don't Try Anything!"},
    {578, "You Will Pay For That Next Wave, Tough Guy!"},
    {1940, "Hey! I Have An Outstanding Warrant For Your Arrest!"},
};

class LawHost : public MessageHost {
public:
    std::vector<int> sounds, todos, draws;
    int next_rand = 0;
    std::string lang(int id) override {
        auto it = kStrings.find(id);
        return it == kStrings.end() ? std::string() : it->second;
    }
    std::uint32_t now() const override { return 20000; }
    std::string name(int pool, int rec) override { return pool == 14 ? "Interceptor" + std::to_string(rec) : "?"; }
    int rand() override {
        draws.push_back(next_rand);
        return next_rand;
    }
    void sound(int id) override { sounds.push_back(id); }
    void todo(int code) override { todos.push_back(code); }
};

std::string show(LawHost& h, int code) {
    MessageState s;
    dispatch_message(s, h, 0, 14, 3, code);  // to the player 0 from ship 3
    CHECK(h.todos.empty());
    CHECK(message_code_ported(code));
    return s.line1 + s.line2;
}

}  // namespace

TEST_CASE("seg3:0x6ec codes 0x3d and 0x57: the arrest's lines through seg3:0xf61, one rand() % 4") {
    LawHost h;
    h.next_rand = 0;  // rand() % 4 = 0: seg10:0x744c(0x3d)
    CHECK(show(h, 0x3d) == "FOLLOW ME OUT. AND DON'T TRY ANYTHING!");
    CHECK(h.sounds == std::vector<int>{0x3d});
    CHECK(h.draws.size() == 1);
    h.sounds.clear();
    h.draws.clear();
    h.next_rand = 7;  // 7 % 4 = 3: no sound
    CHECK(show(h, 0x57) == "YOU WILL PAY FOR THAT NEXT WAVE, TOUGH GUY!");
    CHECK(h.sounds.empty());
    CHECK(h.draws.size() == 1);
}

TEST_CASE("seg3:0x6ec code 0xae: the warrant's 1940, no sound, no rand()") {
    LawHost h;
    CHECK(show(h, 0xae) == "HEY! I HAVE AN OUTSTANDING WARRANT FOR YOUR ARREST!");
    CHECK(h.sounds.empty());
    CHECK(h.draws.empty());
}

TEST_CASE("seg3:0x6ec code 0 and codes above 0xc0: the tail, the lines stay (seg13:0xb6f2's crime 7)") {
    // `dec ax; cmp ax, 0xbf; ja seg3:0x14d6` [BYTES seg3:0x1346-0x134a]: code 0 is what the crime
    // routine sends for crime 7 with its [bp-0xc] read as 0 [DESIGN ADR 0016 decision 1].
    for (int code : {0, 0xc1, 0xff}) {
        LawHost h;
        MessageState s;
        s.line1 = "OLD 1";
        s.line2 = "OLD 2";
        dispatch_message(s, h, 0, 14, 3, code);
        CHECK(h.todos.empty());
        CHECK(message_code_ported(code));
        CHECK(s.line1 == "OLD 1");
        CHECK(s.line2 == "OLD 2");
    }
}
