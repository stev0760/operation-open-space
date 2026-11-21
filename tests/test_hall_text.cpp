// The Hall of Justice's text and punishment (src/hall/hall.h, seg16:0x6afe and its helpers),
// against numbers worked from the listing (s4-hall.md "Facts for tests" with s4-hall-verify.md)
// and the oracle's arrests (ships-0003 k1 and k5, ships-0004's fines). The pilot name is a
// stand-in: no file holds the oracle's.
#include <doctest.h>

#include <map>
#include <string>

#include "hall/hall.h"
#include "shell/rng.h"
#include "world/pilot.h"
#include "world/world.h"

using namespace is;
using namespace is::hall;

namespace {

// ENG_LANG.DLL strings (work/assets/eng_lang.dll/strings.tsv).
const std::map<int, std::string>& strings() {
    static const std::map<int, std::string> m = {
        {186, "NO WEAPON"}, {201, "HANDCUFFS"}, {234, "%< AND %>%<, %>%# OF %S"}, {248, "FRIENDLY"},
        {291, "SINCE THERE IS AN OUTSTANDING WARRANT FOR YOUR ARREST"},
        {630, "WE ARE LOOKING AT %S IDIOT HERE! (HE HE HE)"}, {632, "AN "}, {633, "A REAL "},
        {634, "A SUPREME "}, {635, "A SUPER SUPREME "}, {636, "FURTHER IDIOCY WILL BE DEALT WITH MORE SEVERELY.."},
        {637, "DON'T DO IT AGAIN! WE CAN'T THINK OF ANY MORE INSULTS.."},
        {723, "CONSIDERING THE SERIOUSNESS OF YOUR CRIME AND THAT THIS IS YOUR %s OFFENSE, "},
        {725, "WE ARE ASSESSING YOU A FINE OF %s ICONS"}, {726, "WE ARE CONFISCATING YOUR %S"},
        {788, "Officer"}, {860, "WE'LL BE WATCHING YOU.."}, {861, "OBEY THE LAW OR YOU'RE GOING TO GET IT!"},
        {862, "FLY CAREFULLY AND BE A GOOD CITIZEN.."}, {863, "IF YOU IGNORE THE LAW, WE'LL TAKE YOU DOWN!"},
        {908, "THIS IS THE HALL OF JUSTICE, %s.%N%N"}, {909, "THIS IS YOUR %s ARREST IN %#, %s.%N%N"},
        {911, "THOSE WHO BREAK THE LAWS OF INNER SPACE ARE BROUGHT HERE TO MEET THEIR FATE. %N%N"},
        {912, "AFTER OBSERVING YOU"}, {913, " ARRESTED YOU AT "}, {914, "YOU ARE CHARGED WITH "},
        {915, "AFTER DUE CONSIDERATION, YOU HAVE BEEN FOUND GUILTY AS CHARGED"}, {916, "STAY OUT OF TROUBLE"},
        {1093, " Thrusting Recklessly"}, {1097, "AFTER WITNESSING YOUR DISGRACEFUL,"},
        {1098, " Murdering An Enforcer."}, {1099, " Destroying An Icon"}, {1100, " Removing Evidence"},
        {1101, " Attacking The Defenseless"}, {1102, " Taking An Enforcer's Doughnut"},
        {1103, " Firing Upon An Officer"}, {1104, " Destroying Doughnut Dispensors"},
        {1105, " Resisting Arrest."}, {1106, " Unsportsmanlike Race Conduct"}, {1107, " Being An Idiot."},
        {1158, "YOUR CREDIT IS NOW %s ICONS. "}, {2095, "ST"}, {2096, "ND"}, {2097, "RD"}, {2098, "TH"},
        {2518, "WAVE"}, {12518, "WAVES"}, {2519, "COUNT"}, {12519, "COUNTS"},
    };
    return m;
}
std::string lang(int id) {
    auto it = strings().find(id);
    return it == strings().end() ? std::string() : it->second;
}

// ships-0003 k1: the player ship 0 (credit 150, resources 0, two weapon slots), the arresting
// Interceptor ship 3 (class 1, no roster record, design "INTERCEPTOR"), one count of crime 2.
struct K1 {
    world::World w;
    TextInputs in;
    shell::Rng rng;
    K1() {
        w.player = 0;
        w.last_ship = 3;
        w.wave = 3;
        for (auto& s : w.ships) s.roster_index = -1;
        w.ships[0].roster_index = 0;
        world::set_ship_d(w.ships[0], 0xdc, 150);
        world::set_ship_w(w.ships[0], 0xce, 0);
        world::set_ship_b(w.ships[0], 0xcb, 2);
        world::set_ship_b(w.ships[0], 0xb8, 1);   // the gun
        world::set_ship_b(w.ships[0], 0xb9, 18);  // HEATSEEKER MISSILES (186 + 18 = 204)
        world::set_ship_b(w.ships[0], 0xaf, 5);
        world::set_ship_b(w.ships[3], 0x5e, 1);
        w.ships[3].design_index = 2;
        w.design_names[2] = "INTERCEPTOR";
        w.ba14d[2] = 1;
        w.ba161 = 2;
        in.pilot = "PILOT";
        in.a162 = 3;
        in.now = Clock{13, 13, 10, 15, 126};
        rng.srand(77);
    }
};

}  // namespace

TEST_CASE("hall: seg17:0x7ba6 ordinals") {
    CHECK(ordinal(1, lang) == "1ST");
    CHECK(ordinal(2, lang) == "2ND");
    CHECK(ordinal(3, lang) == "3RD");
    CHECK(ordinal(4, lang) == "4TH");
    CHECK(ordinal(11, lang) == "11TH");
    CHECK(ordinal(12, lang) == "12TH");
    CHECK(ordinal(13, lang) == "13TH");
    CHECK(ordinal(21, lang) == "21ST");
    CHECK(ordinal(111, lang) == "111ST");  // n % 10 = 1 and n != 11
    CHECK(ordinal(-1, lang) == "-1TH");    // the signed remainder
}

TEST_CASE("hall: seg6:0x1010 and seg6:0xf54 without zero padding") {
    CHECK(fixed_word(150) == "1.50");
    CHECK(fixed_word(105) == "1.5");
    CHECK(fixed_word(100) == "1.0");
    CHECK(fixed_word(-150) == "-1.50");
    CHECK(fixed_word(275) == "2.75");
    CHECK(fixed_dword(0) == "0.0");
    CHECK(fixed_dword(-250) == "-2.50");
    CHECK(fixed_dword(-525) == "-5.25");
    CHECK(fixed_dword(1205) == "12.5");
}

TEST_CASE("hall: seg10:0x97ce prints tm_year raw") {
    CHECK(hall_clock(Clock{13, 13, 10, 15, 126}) == "1:13PM 11/15/126");  // ships-0003 k1
    CHECK(hall_clock(Clock{0, 5, 0, 1, 100}) == "12:05AM 1/1/100");
    CHECK(hall_clock(Clock{12, 30, 11, 31, 99}) == "12:30PM 12/31/99");
    CHECK(hall_clock(Clock{11, 59, 5, 9, 126}) == "11:59AM 6/9/126");
}

TEST_CASE("hall: seg17:0x8e60 officer numbers") {
    world::World w;
    for (auto& s : w.ships) s.roster_index = -1;
    world::set_ship_b(w.ships[3], 0x5e, 1);
    world::set_ship_b(w.ships[1], 0x5e, 1);
    world::set_ship_b(w.ships[4], 0x5e, 3);
    CHECK(officer_name(w, 3, lang) == "Officer 2171");  // the oracle's OFFICER 2171
    CHECK(officer_name(w, 1, lang) == "Officer 1376");
    CHECK(officer_name(w, 4, lang) == "Officer 2972");
}

TEST_CASE("hall: seg16:0x71a2 takes slot 1 and closes the gap") {
    world::ShipRecord s{};
    world::set_ship_b(s, 0xcb, 3);
    for (int k = 0; k < 10; ++k) {
        world::set_ship_b(s, 0xb8 + k, static_cast<std::uint8_t>(10 + k));
        world::set_ship_b(s, 0xae + k, static_cast<std::uint8_t>(20 + k));
    }
    CHECK(confiscate(s, 1) == 11);
    CHECK(world::ship_b(s, 0xb8) == 10);
    CHECK(world::ship_b(s, 0xb9) == 12);
    CHECK(world::ship_b(s, 0xaf) == 22);
    CHECK(world::ship_b(s, 0xc0) == 19);
    CHECK(world::ship_b(s, 0xc1) == 0);
    CHECK(world::ship_b(s, 0xb7) == 0);
    CHECK(world::ship_b(s, 0xcb) == 2);
}

TEST_CASE("hall: the severity and the punishment") {
    std::array<std::uint8_t, 10> a{}, b{};
    a[2] = 1;
    CHECK(severity(a, b) == 550);
    a = {};
    a[0] = 3;  // 700 + 140·2
    CHECK(severity(a, b) == 980);
    a[9] = 4;  // weight 0
    CHECK(severity(a, b) == 980);
    // First offences.
    CHECK(punish(550, 0, 150, 2).fine == 150);   // k1: 183 capped at the credit
    CHECK(punish(450, 0, 150, 2).fine == 150);   // k5
    CHECK(punish(550, 0, 1000, 2).fine == 183);
    CHECK(punish(980, 0, 50, 2).fine == 50);     // 326 capped at 50
    CHECK(punish(300, 0, 100, 1).fine == 100);   // ships-0004: crime 5, fine 1.00
    CHECK(punish(300, 0, 0, 1).fine == 100);     // credit 0: no cap, and 0 + 100 < 100 fails
    CHECK(punish(300, 0, -50, 1).fine == 50);    // -50 + 100 < 100: one slot, halved
    {
        const Sentence s = punish(300, 0, -50, 3);  // more than one slot: the confiscation
        CHECK(s.fine == 0);
        CHECK(s.confiscate);
    }
    // Repeat offenders (ships-0004): broke, one slot, the fine halved.
    CHECK(punish(550, 7, -525, 1).fine == 275);  // crime 2, 8th: 2.75
    CHECK(punish(400, 3, -250, 1).fine == 200);  // crime 1: 2.00
    CHECK(punish(500, 3, -250, 1).fine == 250);  // crime 4 and 8: 2.50
    CHECK(punish(300, 8, -995, 1).fine == 150);  // crime 5, 9th: 1.50
    CHECK(punish(550, 5, 1000, 1).fine == 550);  // paid in full: credit + 100 >= S
    CHECK(punish(550, 5, 450, 1).fine == 550);   // to -100
    CHECK(punish(0, 2, -500, 3).fine == 0);      // crime 9 alone: no confiscation
    CHECK_FALSE(punish(0, 2, -500, 3).confiscate);
}

TEST_CASE("hall: ships-0003 k1's eight paragraphs") {
    K1 f;
    const HallText t = hall_text(f.w, f.in, lang, f.rng);
    const std::string expect =
        "THIS IS THE HALL OF JUSTICE, PILOT.\r\n\r\n"
        "THOSE WHO BREAK THE LAWS OF INNER SPACE ARE BROUGHT HERE TO MEET THEIR FATE. \r\n\r\n"
        "AFTER OBSERVING YOU REMOVING EVIDENCE, OFFICER 2171 (INTERCEPTOR) ARRESTED YOU AT 1:13PM 11/15/126. \r\n\r\n"
        "YOU ARE CHARGED WITH 1 COUNT OF  REMOVING EVIDENCE. \r\n \r\n"
        "AFTER DUE CONSIDERATION, YOU HAVE BEEN FOUND GUILTY AS CHARGED. \r\n\r\n"
        "CONSIDERING THE SERIOUSNESS OF YOUR CRIME AND THAT THIS IS YOUR 1ST OFFENSE, WE ARE ASSESSING YOU A FINE OF "
        "1.50 ICONS. \r\n\r\n"
        "YOUR CREDIT IS NOW 0.0 ICONS. \r\n \r\n"
        "STAY OUT OF TROUBLE, PILOT. ";
    CHECK(t.text == expect);
    CHECK(t.s.fine == 150);
    CHECK(world::ship_d(f.w.ships[0], 0xdc) == 0u);  // 150 -> 0 [ORACLE ships-0003]
    CHECK(f.w.ba157[2] == 1);                        // lastgame 0x180 = 1
    CHECK(f.w.ba14d[2] == 1);
    CHECK_FALSE(t.drew);  // no rand() for a first offence
    CHECK(f.rng.state() == 77u);
}

TEST_CASE("hall: seg11:0x3fd4 mode 1 keeps the case of the character after '('") {
    // A design name in lower case (a Ship Factory design): " (iNTERCEPTOR)" [BYTES seg16:0x7184-0x7190
    // seg11:0x40ce-0x40da] (review L3).
    K1 f;
    f.w.design_names[2] = "interceptor";
    const HallText t = hall_text(f.w, f.in, lang, f.rng);
    CHECK(t.text.find("OFFICER 2171 (iNTERCEPTOR) ARRESTED YOU") != std::string::npos);
}

TEST_CASE("hall: ships-0003 k5, resisting arrest") {
    K1 f;
    f.w.ba14d = {};
    f.w.ba14d[7] = 1;
    f.w.ba161 = 7;
    f.in.now = Clock{13, 27, 10, 15, 126};
    const HallText t = hall_text(f.w, f.in, lang, f.rng);
    CHECK(t.text.find("AFTER OBSERVING YOU RESISTING ARREST., OFFICER 2171 (INTERCEPTOR) ARRESTED YOU AT 1:27PM 11/15/126.") !=
          std::string::npos);
    CHECK(t.text.find("YOU ARE CHARGED WITH 1 COUNT OF  RESISTING ARREST.. \r\n \r\n") != std::string::npos);
    CHECK(t.s.fine == 150);
}

TEST_CASE("hall: the resources go into the credit first") {
    K1 f;
    world::set_ship_d(f.w.ships[0], 0xdc, 0);
    world::set_ship_w(f.w.ships[0], 0xce, 1000);
    const HallText t = hall_text(f.w, f.in, lang, f.rng);
    CHECK(t.s.fine == 183);
    CHECK(world::ship_d(f.w.ships[0], 0xdc) == 817u);
    CHECK(world::ship_w(f.w.ships[0], 0xce) == 0);
    CHECK(t.text.find("A FINE OF 1.83 ICONS") != std::string::npos);
    CHECK(t.text.find("YOUR CREDIT IS NOW 8.17 ICONS") != std::string::npos);
}

TEST_CASE("hall: a repeat offender, two charges and the warning line") {
    K1 f;
    f.w.ba157[2] = 1;  // served once
    f.w.ba14d[2] = 3;  // two new counts of crime 2
    f.w.ba14d[4] = 1;  // one of crime 4
    f.w.ba161 = 4;
    world::set_ship_d(f.w.ships[0], 0xdc, 5000);
    shell::Rng probe = f.rng;
    const int r = probe.rand();
    const HallText t = hall_text(f.w, f.in, lang, f.rng);
    CHECK(t.text.rfind("THIS IS YOUR 4TH ARREST IN 3 WAVES, PILOT.\r\n\r\n", 0) == 0);
    CHECK(t.text.find("THOSE WHO BREAK") == std::string::npos);
    CHECK(t.text.find("YOU ARE CHARGED WITH 2 COUNTS OF  REMOVING EVIDENCE AND 1 COUNT OF  TAKING AN ENFORCER'S DOUGHNUT. ") !=
          std::string::npos);
    // S = 550 + 110 + 500 = 1160, paid in full.
    CHECK(t.s.severity == 1160);
    CHECK(t.s.fine == 1160);
    CHECK(t.text.find("THIS IS YOUR 4TH OFFENSE, WE ARE ASSESSING YOU A FINE OF 11.60 ICONS") != std::string::npos);
    CHECK(t.drew);
    const std::string warn = lang(860 + r % 4);
    CHECK(t.text.size() >= warn.size());
    CHECK(t.text.compare(t.text.size() - warn.size(), warn.size(), warn) == 0);
    CHECK(f.w.ba157 == f.w.ba14d);
}

TEST_CASE("hall: three charges read A, B AND C") {
    K1 f;
    f.w.ba14d = {};
    f.w.ba14d[1] = 1;
    f.w.ba14d[2] = 1;
    f.w.ba14d[5] = 1;
    const HallText t = hall_text(f.w, f.in, lang, f.rng);
    CHECK(t.text.find("WITH 1 COUNT OF  DESTROYING AN ICON, 1 COUNT OF  REMOVING EVIDENCE AND 1 COUNT OF  FIRING UPON AN OFFICER. ") !=
          std::string::npos);
}

TEST_CASE("hall: a broke first offender loses a weapon") {
    K1 f;
    world::set_ship_d(f.w.ships[0], 0xdc, static_cast<std::uint32_t>(-500));
    const HallText t = hall_text(f.w, f.in, lang, f.rng);
    CHECK(t.s.fine == 0);
    CHECK(t.s.confiscate);
    CHECK(t.conf == 18);
    CHECK(t.text.find("WE ARE CONFISCATING YOUR ") != std::string::npos);
    CHECK(world::ship_b(f.w.ships[0], 0xcb) == 1);
    CHECK(world::ship_b(f.w.ships[0], 0xb9) == 0);
    CHECK(static_cast<std::int32_t>(world::ship_d(f.w.ships[0], 0xdc)) == -500);
    CHECK(t.text.find("STAY OUT OF TROUBLE, PILOT. ") != std::string::npos);
}

TEST_CASE("hall: crime 9, the insult and ADR 0016's conf 0") {
    K1 f;
    f.w.ba14d = {};
    f.w.ba14d[9] = 1;
    f.w.ba161 = 9;
    const HallText t = hall_text(f.w, f.in, lang, f.rng);
    CHECK(t.conf_unset);
    CHECK(t.text.find("AFTER OBSERVING YOU THRUSTING RECKLESSLY, OFFICER 2171") != std::string::npos);
    CHECK(t.text.find("1 COUNT OF  BEING AN IDIOT.. ") != std::string::npos);
    CHECK(t.text.find("WE ARE LOOKING AT AN  IDIOT HERE! (HE HE HE). ") != std::string::npos);
    CHECK(t.text.find("FURTHER IDIOCY WILL BE DEALT WITH MORE SEVERELY.., PILOT. ") != std::string::npos);
    CHECK(world::ship_d(f.w.ships[0], 0xdc) == 150u);
    // The fifth crime 9: DGROUP:0xa160 = 4 before the copy.
    K1 g;
    g.w.ba14d = {};
    g.w.ba157 = {};
    g.w.ba14d[9] = 5;
    g.w.ba157[9] = 4;
    g.w.ba161 = 9;
    const HallText u = hall_text(g.w, g.in, lang, g.rng);
    CHECK(u.text.find("A SUPER SUPREME  IDIOT") != std::string::npos);
    CHECK(u.text.find("DON'T DO IT AGAIN!") != std::string::npos);
}

TEST_CASE("hall: crime 8 and a negative arrest crime") {
    K1 f;
    f.w.ba14d = {};
    f.w.ba14d[8] = 1;
    f.w.ba161 = 8;
    HallText t = hall_text(f.w, f.in, lang, f.rng);
    CHECK(t.text.find("AFTER WITNESSING YOUR DISGRACEFUL, UNSPORTSMANLIKE RACE CONDUCT, OFFICER") != std::string::npos);
    K1 g;
    g.w.ba161 = 0xff;  // string 291 is loaded and lost
    t = hall_text(g.w, g.in, lang, g.rng);
    CHECK(t.text.find("FATE. \r\n\r\n, OFFICER 2171 (INTERCEPTOR) ARRESTED") != std::string::npos);
    CHECK(t.text.find("WARRANT") == std::string::npos);
}
