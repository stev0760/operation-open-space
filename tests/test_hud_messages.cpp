// The message dispatcher seg3:0x6ec for the damage, death and repair codes (src/hud/messages.h)
// and the message display's pool-7 picture (src/hud/target.h plan_cell), M4 stage 1 SCREENS.
#include <map>
#include <string>
#include <vector>

#include "doctest.h"
#include "hud/messages.h"
#include "hud/target.h"

using namespace is::hud;

namespace {

// The ENG_LANG.DLL strings the cases load [BYTES ENG_LANG.DLL STRING 8] id 127 [BYTES ENG_LANG.DLL
// STRING 9] ids 128, 129, 134, 135, 138 [BYTES ENG_LANG.DLL STRING 22] ids 347, 350 [BYTES
// ENG_LANG.DLL STRING 33] ids 517, 519 [BYTES ENG_LANG.DLL STRING 43] ids 687-692 [BYTES
// ENG_LANG.DLL STRING 67] id 1064 [BYTES ENG_LANG.DLL STRING 139] id 2213.
const std::map<int, std::string> kStrings = {
    {127, "ENGINE"},
    {128, "GUN"},
    {129, "THRUSTER"},
    {134, "DANGER: %S %?. REPAIR OR ABORT%< MISSION%>."},
    {135, "TROUBLE"},
    {138, "Repairs Complete, %s. Try To Avoid Trouble."},
    {347, "Ambulance"},
    {350, "Be More Careful, %s. Avoid Dangerous Hazards."},
    {517, "%S SYSTEM OK REPAIRS COMPLETE"},
    {519, "FAILURE"},
    {687, "You Destroyed A Virus! %R %s!!"},
    {688, "Well Done, "},
    {689, "Way To Go, "},
    {690, "That's The Ticket, "},
    {691, "Right On, "},
    {692, "That Was A Nasty Icon. %R %s!!"},
    {1064, "He Shoots! He Scores! %R %s!!"},
    {2213, "NO MESSAGE"},
};

class Host : public MessageHost {
public:
    int player_ = 0;
    std::uint32_t now_ = 10000;
    int width_ = 1024;
    bool race_ = false;
    int mode_ = 2;
    int rand_ = 0;
    std::vector<int> sounds;
    std::vector<int> todos;
    std::vector<std::string> trace;
    std::uint32_t end = 0;
    int sender_pool = -9, sender_rec = -9;

    int player() const override { return player_; }
    std::uint32_t now() const override { return now_; }
    int width() const override { return width_; }
    bool race() const override { return race_; }
    std::string pilot() const override { return "Your Name"; }
    std::string lang(int id) override {
        auto it = kStrings.find(id);
        return it == kStrings.end() ? std::string() : it->second;
    }
    int rand() override { return rand_; }
    void set_sender(int pool, int rec) override { sender_pool = pool, sender_rec = rec; }
    int info_mode() const override { return mode_; }
    void set_info_mode(int m) override {
        mode_ = m;
        trace.push_back("mode " + std::to_string(m));
    }
    bool info_exists() const override { return true; }
    void info_flash() override { trace.push_back("flash"); }
    std::string name(int pool, int) override { return pool == 7 ? "Ambulance" : "?"; }
    void set_end(std::uint32_t t) override { end = t; }
    void sound(int id) override { sounds.push_back(id); }
    void todo(int code) override { todos.push_back(code); }
};

}  // namespace

TEST_CASE("hud messages: seg3:0x3498 splits at the space nearest the middle") {
    std::string a, b;
    // "DANGER: ENGINE TROUBLE. REPAIR OR ABORT MISSION." is 48 characters, middle 24 ('R' of
    // REPAIR); 24 + 0 is not a space, 24 - 0 neither, 24 + 1 neither, 24 - 1 is the space.
    split_lines("DANGER: ENGINE TROUBLE. REPAIR OR ABORT MISSION.", a, b);
    CHECK(a == "DANGER: ENGINE TROUBLE. ");
    CHECK(b == "REPAIR OR ABORT MISSION.");
    split_lines("GUN SYSTEM OK REPAIRS COMPLETE", a, b);
    CHECK(a == "GUN SYSTEM OK ");
    CHECK(b == "REPAIRS COMPLETE");
    split_lines("NOSPACES", a, b);
    CHECK(a == "NOSPACES");
    CHECK(b.empty());
    split_lines("", a, b);
    CHECK(a.empty());
    CHECK(b.empty());
    // cut at 78 characters
    split_lines(std::string(100, 'x'), a, b);
    CHECK(a.size() == 78);
}

TEST_CASE("hud messages: seg3:0x1542 cuts the name's code") {
    CHECK(cut_name_code("Ambulance^N") == "Ambulance");
    CHECK(cut_name_code("Defender") == "Defender");
}

TEST_CASE("hud messages: seg3:0x14d6 upper-cases above 635 pixels") {
    std::string a = "Repairs Complete, Your Name. ", b = "Try To Avoid Trouble.";
    upper_lines(635, a, b);
    CHECK(a == "Repairs Complete, Your Name. ");
    upper_lines(636, a, b);
    CHECK(a == "REPAIRS COMPLETE, YOUR NAME. ");
    CHECK(b == "TRY TO AVOID TROUBLE.");
}

TEST_CASE("hud messages: the lamp codes 10, 11, 12 match the oracle's lines") {
    MessageState s;
    Host h;
    dispatch_message(s, h, 0, 7, 0, 10);
    CHECK(h.sender_pool == 7);
    CHECK(h.sender_rec == 0);
    CHECK(h.trace == std::vector<std::string>{"mode 3", "flash"});
    CHECK(s.sender == "Ambulance");
    CHECK(h.sounds == std::vector<int>{0x6e, 0x2d});
    CHECK(h.end == 17000u);
    // [ORACLE core-0001] "DANGER: ENGINE TROUBLE. / REPAIR OR ABORT MISSION."
    CHECK(s.line1 == "DANGER: ENGINE TROUBLE. ");
    CHECK(s.line2 == "REPAIR OR ABORT MISSION.");
    dispatch_message(s, h, 0, 7, 0, 11);
    CHECK(s.line1 == "DANGER: GUN FAILURE. ");
    CHECK(s.line2 == "REPAIR OR ABORT MISSION.");
    dispatch_message(s, h, 0, 7, 0, 12);
    CHECK(s.line1 == "DANGER: THRUSTER FAILURE. ");
    CHECK(s.line2 == "REPAIR OR ABORT MISSION.");
    CHECK(h.sounds == std::vector<int>{0x6e, 0x2d, 0x6e, 0x2c, 0x6e, 0x2c});
    // The mode is 3 already: no second seg7:0x119c.
    CHECK(h.trace == std::vector<std::string>{"mode 3", "flash", "flash", "flash"});
    // Narrow: no " MISSION", mixed case kept at 635.
    Host n;
    n.width_ = 640;
    dispatch_message(s, n, 0, 7, 0, 10);
    CHECK(s.text == "DANGER: ENGINE TROUBLE. REPAIR OR ABORT.");
}

TEST_CASE("hud messages: the repair codes 82, 83, 84") {
    MessageState s;
    Host h;
    dispatch_message(s, h, 0, 7, 0, 82);
    // [ORACLE core-0001] "GUN SYSTEM OK / REPAIRS COMPLETE"
    CHECK(s.line1 == "GUN SYSTEM OK ");
    CHECK(s.line2 == "REPAIRS COMPLETE");
    dispatch_message(s, h, 0, 7, 0, 83);
    CHECK(s.line1 == "THRUSTER SYSTEM OK ");
    CHECK(s.line2 == "REPAIRS COMPLETE");
    dispatch_message(s, h, 0, 7, 0, 84);
    CHECK(s.text == "Repairs Complete, Your Name. Try To Avoid Trouble.");
    CHECK(s.line1 == "REPAIRS COMPLETE, YOUR NAME. ");
    CHECK(s.line2 == "TRY TO AVOID TROUBLE.");
    CHECK(h.sounds.empty());
}

TEST_CASE("hud messages: code 13 sounds at most every 4 s") {
    MessageState s;
    Host h;
    dispatch_message(s, h, 0, 7, 0, 13);
    CHECK(h.sounds == std::vector<int>{0x67});
    CHECK(s.ram_due == 14000u);
    CHECK(s.text == "Be More Careful, Your Name. Avoid Dangerous Hazards.");
    h.now_ = 14000;  // not past
    dispatch_message(s, h, 0, 7, 0, 13);
    CHECK(h.sounds.size() == 1);
    h.now_ = 14001;
    dispatch_message(s, h, 0, 7, 0, 13);
    CHECK(h.sounds.size() == 2);
}

TEST_CASE("hud messages: %R picks one of four praises") {
    MessageState s;
    Host h;
    h.rand_ = 6;  // 6 % 4 = 2: string 690
    dispatch_message(s, h, 0, 7, 0, 91);
    CHECK(s.text == "You Destroyed A Virus! That's The Ticket,  Your Name!!");
    h.rand_ = 0;
    dispatch_message(s, h, 0, 7, 0, 94);
    CHECK(s.text == "That Was A Nasty Icon. Well Done,  Your Name!!");
    dispatch_message(s, h, 0, 7, 0, 112);
    CHECK(s.text == "He Shoots! He Scores! Well Done,  Your Name!!");
}

TEST_CASE("hud messages: the head's filters, the end times and sender -1") {
    MessageState s;
    Host h;
    dispatch_message(s, h, 1, 7, 0, 10);  // not the player
    CHECK(h.trace.empty());
    dispatch_message(s, h, 0, 14, 0, 10);  // the player as sender
    CHECK(h.trace.empty());
    dispatch_message(s, h, 0, 3, 0, 10);  // pool 3: mode and flash, nothing else
    CHECK(h.trace == std::vector<std::string>{"mode 3", "flash"});
    CHECK(h.sounds.empty());
    CHECK(h.end == 0u);
    dispatch_message(s, h, 0, -1, 0, 10);
    CHECK(s.sender == "NO MESSAGE");
    CHECK(s.line1.empty());
    CHECK(h.end == 13000u);
    h.race_ = true;
    dispatch_message(s, h, 0, 7, 0, 82);
    CHECK(h.end == 12500u);
    dispatch_message(s, h, 0, 2, 0, 82);
    CHECK(h.end == 15000u);
    CHECK(s.last_code == 82);
    dispatch_message(s, h, 0, 7, 0, 5);  // a case the port does not carry
    CHECK(h.todos == std::vector<int>{5});
}

TEST_CASE("hud target: seg13:0x5c22 draws pool 7 from the logo strip") {
    CellInputs in;
    in.pool = 7;
    in.rec = 0;
    in.flag = 1;
    in.x = 10;
    in.y = 20;
    in.obj.state = 1;  // the ambulance is not on the field
    in.obj.extent = 0x140;
    in.obj.size = 40;
    in.obj.gap = 6;
    int glint = 0;
    CellPlan p = plan_cell(in, glint);
    CHECK(p.picture);
    CHECK(p.slot == 5);
    CHECK(p.sx == 6 * 6 + 5 * 40);  // cell 5 of 6 + 46k
    CHECK(p.sy == 6);
    CHECK(p.sw == 40);
    // a = (0x140 >> 3) / 2 = 20; off = (43 - 20) / 2 = 11
    CHECK(p.dw == 20);
    CHECK(p.dx == 21);
    CHECK(p.dy == 31);
    in.obj.state = 0;
    in.pool7_w10 = 52;
    in.obj.src_y = 3;
    p = plan_cell(in, glint);
    CHECK(p.sx == 52);
    CHECK(p.sy == 3);
}
