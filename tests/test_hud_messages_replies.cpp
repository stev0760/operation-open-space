// M5 stage 4, track COMMANDS: seg3:0x6ec's replies to the commands (src/hud/messages.cpp; the case
// targets of the jump table seg3:0x1356 and the sub-switch seg3:0xac3, s4-commands.md 1.10 with
// s4-commands-verify.md), each with its string id, sound and %-arguments, and the lines
// ships-0005 recorded.
#include <map>
#include <string>
#include <vector>

#include "doctest.h"
#include "hud/messages.h"

using namespace is::hud;

namespace {

// [BYTES ENG_LANG.DLL STRING 19-21, 22-27, 35, 42, 43, 89, 116-118, 122, 126, 189] (work/assets
// eng_lang.dll strings.tsv)
const std::map<int, std::string> kStrings = {
    {302, "Please Attack %D"},
    {303, "Please Defend %D"},
    {328, "Sorry, Can't Attack %D"},
    {329, "You'll Have To Fend For Yourself"},
    {331, "No One Can Help %D."},
    {332, "Sorry, %s. I Will Not Fight You."},
    {334, "You Are Not Worth The Trouble of Destroying!"},
    {336, "Very Well, %s, I Accept Your Challenge!"},
    {338, "I Love A Good Fight! Prepare To Die, Fool!"},
    {340, "Busy Now, I'll Get You Later..."},
    {356, "I've Had Enough of You. Let's Fight It Out!"},
    {358, "Look, Friend... Do You Want To Rumble?"},
    {388, "I Want You To Go Get %D"},
    {398, "I Can't Capture %D Should I Attack It?"},
    {400, "What Use Have I Got For %D?"},
    {401, "Can't Capture That. %D Isn't Capturable."},
    {404, "I'm Too Busy Right Now. You Go Ahead And Get It."},
    {406, "It's Nice To Share, But No Teammates Are Around."},
    {428, "I'll Clean Up Here, Why Don't You Leave Early?"},
    {430, "You Do Not Have An Ally Willing To Leave."},
    {432, "I'm Staying. I've Got Some Business To Take Care Of."},
    {436, "You'll Have To Get Your Own Fuel Buddy.."},
    {438, "I'm Busy. I'll Get Gas When I Need Some."},
    {442, "See That Fuel? Go Get It!"},
    {555, "We Don't Take Orders, We Enforce Order!"},
    {657, "If I Die, I Die. But You'll Still Suffer!"},
    {659, "I Can't Take You Now, But Your Day Will Come."},
    {661, "You're Crazy To Take Me On! But That's Your Problem!"},
    {663, "If You Were Stronger, I Would Fight You!"},
    {685, "Don't Worry! I'll Be Alright."},
    {1408, "We Are Already In a Duel!"},
    {1419, "I Don't Want Any More Duels. So I'll Kill You Now!!"},
    {1850, "Sorry, But I Can't Help. I'm Too Low On %D."},
    {1851, "Well Where Is He? He Must Be Cloaked."},
    {1852, "Busy With A Request. Try Again In %d Seconds."},
    {1856, "Why Would You Want That? I Happen To Like Him."},
    {1858, "We Don't Work That Way, We Are Pacifists."},
    {1874, "Try That Command When There's Someone Around."},
    {1948, "You're Gonna Get It When I Get %D!"},
    {2000, "I Don't Accept. I'm Here To Help %D."},
    {3019, "Can you help me attack this Virus?"},
    {3020, "Please help me %s. I'm in trouble!"},
};

class ReplyHost : public MessageHost {
public:
    std::uint32_t end = 1;
    bool request_end = false;
    std::vector<int> sounds, todos;
    std::vector<std::string> guards;
    std::map<int, std::map<int, int>> words;  // ship -> offset -> word
    int mp = -1, mr = -1;
    int ro = 0, rp = 0, rr = 0;
    std::string lang(int id) override {
        auto it = kStrings.find(id);
        return it == kStrings.end() ? std::string() : it->second;
    }
    std::uint32_t now() const override { return 20000; }
    std::string pilot() const override { return "Pilot"; }
    std::string name(int pool, int rec) override {
        if (pool == 14) return rec == 0 ? "Sky Talon" : "Ship" + std::to_string(rec);
        if (pool == 4) return "Fuel Tank";
        if (pool == 0) return "Icon " + std::to_string(rec);
        if (pool == 7) return "Ambulance";
        return "?";
    }
    int ship_word(int rec, int off) const override {
        auto it = words.find(rec);
        if (it == words.end()) return 0;
        auto jt = it->second.find(off);
        return jt == it->second.end() ? 0 : jt->second;
    }
    int mark_pool() const override { return mp; }
    int mark_rec() const override { return mr; }
    int request_order() const override { return ro; }
    int request_pool() const override { return rp; }
    int request_rec() const override { return rr; }
    void set_end(std::uint32_t t) override { end = t; }
    void set_request_end() override { request_end = true; }
    void sound(int id) override { sounds.push_back(id); }
    void todo(int code) override { todos.push_back(code); }
    void guard(const char* what, const char*) override { guards.push_back(what); }
};

std::string show(ReplyHost& h, int code, int pool = 14, int rec = 1) {
    MessageState s;
    s.line1 = "OLD 1";
    s.line2 = "OLD 2";
    dispatch_message(s, h, 0, pool, rec, code);  // to the player 0
    CHECK(h.todos.empty());
    CHECK(message_code_ported(code));
    return s.line1 + s.line2;
}

}  // namespace

TEST_CASE("seg3:0x6ec code 2: the recipient's order, the request kept up (DGROUP:0x9608 = 1)") {
    ReplyHost h;
    h.words[0] = {{0x52, 1}};  // the recipient (the player) given order 1: "Please help me"
    CHECK(show(h, 0x02, 14, 2) == "PLEASE HELP ME PILOT. I'M IN TROUBLE!");  // ships-0005 c1
    CHECK(h.sounds == std::vector<int>{0x34});
    CHECK(h.request_end);
    h.sounds.clear();
    h.words[0] = {{0x52, 2}, {0x54, 0}, {0x56, 1}};
    CHECK(show(h, 0x02, 14, 2) == "PLEASE ATTACK ICON 1");
    CHECK(h.sounds == std::vector<int>{0x40});  // ship r is not a ship hunting the sender
    h.sounds.clear();
    h.words[0] = {{0x52, 3}, {0x54, 14}, {0x56, 3}};
    h.words[3] = {{0x32, 14}, {0x34, 2}, {0x38, 1}};  // ship 3 hunts the sender 2
    CHECK(show(h, 0x02, 14, 2) == "PLEASE ATTACK SHIP3");
    CHECK(h.sounds == std::vector<int>{0x34});
    h.sounds.clear();
    h.words[0] = {{0x52, 2}, {0x54, 9}, {0x56, 0}};
    CHECK(show(h, 0x02, 14, 2) == "CAN YOU HELP ME ATTACK THIS VIRUS?");
    h.words[0] = {{0x52, 5}, {0x54, 14}, {0x56, 3}};
    CHECK(show(h, 0x02, 14, 2) == "PLEASE DEFEND SHIP3");
    h.words[0] = {{0x52, 7}};
    CHECK(show(h, 0x02, 14, 2) == "OLD 1OLD 2");  // the tail: the lines stay
}

TEST_CASE("seg3:0x6ec code 0x34: the request record DGROUP:0x964e / 0x9650 / 0x9652") {
    ReplyHost h;
    h.ro = 4;
    h.rp = 0;
    h.rr = 2;
    CHECK(show(h, 0x34, 14, 2) == "PLEASE ATTACK ICON 2");
    h.ro = 9;
    h.rp = 0;
    h.rr = 1;
    CHECK(show(h, 0x34, 14, 2) == "I WANT YOU TO GO GET ICON 1");
    h.ro = 10;
    CHECK(show(h, 0x34, 14, 2) == "I'LL CLEAN UP HERE, WHY DON'T YOU LEAVE EARLY?");
    h.ro = 12;
    CHECK(show(h, 0x34, 14, 2) == "SEE THAT FUEL? GO GET IT!");
}

TEST_CASE("seg3:0x6ec: the refusals and acceptances of the commands (strings and sounds)") {
    struct Case {
        int code;
        const char* text;
        std::vector<int> snd;
    };
    const std::vector<Case> cases = {
        {0x06, "YOU'LL HAVE TO FEND FOR YOURSELF", {}},
        {0x0e, "SORRY, PILOT. I WILL NOT FIGHT YOU.", {}},
        {0x0f, "YOU ARE NOT WORTH THE TROUBLE OF DESTROYING!", {}},
        {0x10, "BUSY NOW, I'LL GET YOU LATER...", {0x3c}},
        {0x12, "I LOVE A GOOD FIGHT! PREPARE TO DIE, FOOL!", {}},  // ships-0005 b1, the Pirate
        {0x14, "YOU ARE NOT WORTH THE TROUBLE OF DESTROYING!", {}},  // ships-0005, an icon targeted
        {0x15, "LOOK, FRIEND... DO YOU WANT TO RUMBLE?", {0x3f}},
        {0x16, "I'VE HAD ENOUGH OF YOU. LET'S FIGHT IT OUT!", {0x3f}},
        {0x26, "I'M TOO BUSY RIGHT NOW. YOU GO AHEAD AND GET IT.", {0x3c}},
        {0x27, "IT'S NICE TO SHARE, BUT NO TEAMMATES ARE AROUND.", {}},
        {0x31, "I'M STAYING. I'VE GOT SOME BUSINESS TO TAKE CARE OF.", {}},
        {0x32, "YOU DO NOT HAVE AN ALLY WILLING TO LEAVE.", {}},  // ships-0005 c3, Ctrl-E
        {0x35, "I'M BUSY. I'LL GET GAS WHEN I NEED SOME.", {}},
        {0x36, "YOU'LL HAVE TO GET YOUR OWN FUEL BUDDY..", {}},
        {0x40, "WE DON'T TAKE ORDERS, WE ENFORCE ORDER!", {}},
        {0x4b, "IF I DIE, I DIE. BUT YOU'LL STILL SUFFER!", {}},
        {0x4c, "I CAN'T TAKE YOU NOW, BUT YOUR DAY WILL COME.", {}},
        {0x4d, "YOU'RE CRAZY TO TAKE ME ON! BUT THAT'S YOUR PROBLEM!", {}},
        {0x4e, "IF YOU WERE STRONGER, I WOULD FIGHT YOU!", {}},
        {0x5a, "DON'T WORRY! I'LL BE ALRIGHT.", {0x3c}},  // ships-0005 b1, Ctrl-S
        {0x74, "WE ARE ALREADY IN A DUEL!", {}},
        {0x82, "I DON'T WANT ANY MORE DUELS. SO I'LL KILL YOU NOW!!", {}},
        {0x8c, "WELL WHERE IS HE? HE MUST BE CLOAKED.", {}},
        {0x91, "WHY WOULD YOU WANT THAT? I HAPPEN TO LIKE HIM.", {}},  // ships-0005 c3, Ctrl-A
        {0x93, "WE DON'T WORK THAT WAY, WE ARE PACIFISTS.", {}},       // ships-0005 b1, Ctrl-A
        {0xa3, "TRY THAT COMMAND WHEN THERE'S SOMEONE AROUND.", {}},
        {0x8b, "SORRY, BUT I CAN'T HELP. I'M TOO LOW ON FUEL TANK.", {}},
        {0xbc, "YOU'RE GONNA GET IT WHEN I GET FUEL TANK!", {}},
    };
    for (const Case& c : cases) {
        ReplyHost h;
        CAPTURE(c.code);
        CHECK(show(h, c.code) == c.text);
        CHECK(h.sounds == c.snd);
    }
}

TEST_CASE("seg3:0x6ec: the mark's %D (codes 5, 0x23, 0x24, 0x25) and the sender's order (0xbd)") {
    ReplyHost h;
    h.mp = 0;
    h.mr = 3;
    CHECK(show(h, 0x05) == "NO ONE CAN HELP ICON 3.");
    CHECK(show(h, 0x23) == "I CAN'T CAPTURE ICON 3 SHOULD I ATTACK IT?");
    CHECK(show(h, 0x24) == "WHAT USE HAVE I GOT FOR ICON 3?");
    CHECK(show(h, 0x25) == "CAN'T CAPTURE THAT. ICON 3 ISN'T CAPTURABLE.");
    CHECK(h.sounds == std::vector<int>{0x3c, 0x3c, 0x3c});
    h.words[1] = {{0x54, 14}, {0x56, 2}};
    CHECK(show(h, 0xbd) == "I DON'T ACCEPT. I'M HERE TO HELP SHIP2.");
}

TEST_CASE("seg3:0x6ec code 0x8d: the seconds left of the sender's +0x98, sound 0x3c") {
    ReplyHost h;
    h.words[1] = {{0x98, 32500}, {0x9a, 0}};  // (32500 - 20000) / 1000 = 12
    CHECK(show(h, 0x8d) == "BUSY WITH A REQUEST. TRY AGAIN IN 12 SECONDS.");
    CHECK(h.sounds == std::vector<int>{0x3c});
}

TEST_CASE("seg3:0x6ec codes 0x11 and 0x64: the ADR 0016 sites log and format") {
    ReplyHost h;
    CHECK(show(h, 0x11) == "VERY WELL, PILOT, I ACCEPT YOUR CHALLENGE!");
    CHECK(show(h, 0x64) == "SORRY, CAN'T ATTACK ");
    CHECK(h.guards.size() == 2u);
}
