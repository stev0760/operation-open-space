// M5 stage 3 fixer: seg3:0x6ec's cases for the order executor seg13:0x6900, the Enforcers'
// seg13:0xa6bc and the revenge recall seg4:0x702e (src/hud/messages.cpp; the case targets of the
// jump table seg3:0x1356 and the sub-switches seg3:0x9ab and seg3:0xb63).
#include <map>
#include <string>
#include <vector>

#include "doctest.h"
#include "hud/messages.h"

using namespace is::hud;

namespace {

// [BYTES ENG_LANG.DLL STRING 19] ids 293, 295 [STRING 22] id 345 [STRING 28] id 448
// [STRING 121] ids 1920, 1924
const std::map<int, std::string> kStrings = {
    {293, "I'm Coming To Help You!"},
    {295, "I'm Defending %D%< (you)%>."},
    {345, "That's Long Enough %D"},
    {448, "I Can't Find A Tank Of Gas Anywhere!"},
    {1920, "I Wouldn't Be Much Help Against That Monster."},
    {1924, "I Need More %D Before I Can Help You."},
};

class OrdersHost : public MessageHost {
public:
    std::uint32_t end = 0;
    std::vector<int> sounds, todos;
    std::map<int, int> words;  // ship word +off of the sender 1, by offset
    std::string taunt_text;
    std::string lang(int id) override {
        auto it = kStrings.find(id);
        return it == kStrings.end() ? std::string() : it->second;
    }
    std::uint32_t now() const override { return 20000; }
    std::string name(int pool, int rec) override {
        if (pool == 14) return rec == 0 ? "You" : "Ship" + std::to_string(rec);
        if (pool == 4) return "Fuel Tank";
        return "?";
    }
    int ship_word(int rec, int off) const override {
        if (rec != 1) return -1;
        auto it = words.find(off);
        return it == words.end() ? 0 : it->second;
    }
    void set_end(std::uint32_t t) override { end = t; }
    void sound(int id) override { sounds.push_back(id); }
    std::string taunt() const override { return taunt_text; }
    void todo(int code) override { todos.push_back(code); }
};

MessageState show(OrdersHost& h, int code) {
    MessageState s;
    s.line1 = "OLD 1";
    s.line2 = "OLD 2";
    dispatch_message(s, h, 0, 14, 1, code);  // from ship 1 to the player 0
    CHECK(h.todos.empty());
    return s;
}

}  // namespace

TEST_CASE("seg3:0x6ec code 1: by the sender's order, end now + 9000") {
    OrdersHost h;
    h.words = {{0x52, 1}};
    MessageState s = show(h, 0x01);
    CHECK(h.end == 29000u);
    CHECK(h.sounds == std::vector<int>{0x37});
    CHECK(s.line1 + s.line2 == "I'M COMING TO HELP YOU!");
    h.sounds.clear();
    h.words = {{0x52, 5}, {0x54, 14}, {0x56, 0}};  // defending the player: the (you) group
    s = show(h, 0x01);
    CHECK(h.sounds == std::vector<int>{0x6d});
    CHECK(s.line1 + s.line2 == "I'M DEFENDING YOU (YOU).");
    h.sounds.clear();
    h.words = {{0x52, 6}};  // no case: the lines stay
    s = show(h, 0x01);
    CHECK(s.line1 == "OLD 1");
    CHECK(h.sounds.empty());
}

TEST_CASE("seg3:0x6ec code 0x13: order 6 plays 0x6b with type 5, order 5 0x44 with type 0") {
    OrdersHost h;
    h.words = {{0x52, 6}, {0x54, 14}, {0x56, 2}};
    MessageState s = show(h, 0x13);
    CHECK(h.sounds == std::vector<int>{0x6b});
    CHECK(s.line1 + s.line2 == "THAT'S LONG ENOUGH SHIP2");
    h.sounds.clear();
    h.words[0x52] = 5;
    s = show(h, 0x13);
    CHECK(h.sounds == std::vector<int>{0x44});
}

TEST_CASE("seg3:0x6ec codes 0x38, 0xaf, 0xb3, 0x83 and the taunt 0x4f") {
    OrdersHost h;
    CHECK(show(h, 0x38).line1.find("I CAN'T FIND") == 0);
    CHECK(show(h, 0xaf).line1.find("I WOULDN'T") == 0);
    MessageState s = show(h, 0xb3);
    CHECK(s.line1 + s.line2 == "I NEED MORE FUEL TANK BEFORE I CAN HELP YOU.");
    s = show(h, 0x83);
    CHECK(s.line1 == "OLD 1");
    h.taunt_text = "Butt Head! I Remember That You Attacked Me!";
    s = show(h, 0x4f);
    CHECK(h.sounds.back() == 0x3b);
    CHECK(s.line1 + s.line2 == "BUTT HEAD! I REMEMBER THAT YOU ATTACKED ME!");
}
