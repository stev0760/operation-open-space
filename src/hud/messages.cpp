// seg3:0x6ec and its formatter parts (messages.h). M4 stage 1, track SCREENS; codes 7, 8, 9
// and 132 M4 stage 3, track AMBFLIGHT; codes 0x75 ... 0x77 M4 stage 4.
#include "hud/messages.h"

#include <functional>
#include <vector>

#include "briefing/briefing.h"
#include "briefing/lang_format.h"

namespace is::hud {

namespace {

std::int16_t w16(int v) { return static_cast<std::int16_t>(v); }

}  // namespace

void split_lines(const std::string& src_in, std::string& line1, std::string& line2) {
    std::string src = src_in.substr(0, 0x4e);  // [BYTES seg3:0x34a9]: byte +0x4e = 0
    const int len = static_cast<int>(src.size());
    const int mid = len / 2;  // cdq; sub; sar [BYTES seg3:0x34b5-0x34ba]
    int cut = mid;
    int i = 0;
    for (; i < mid; ++i) {  // [BYTES seg3:0x34c4-0x34f2]
        if (mid + i < len && src[static_cast<std::size_t>(mid + i)] == ' ') {
            cut = mid + i + 1;
            break;
        }
        if (src[static_cast<std::size_t>(mid - i)] == ' ') {
            cut = mid + 1 - i;
            break;
        }
    }
    // [BYTES seg3:0x34f5-0x3529]: line 1 = src, cut at `cut` when the loop index is below it.
    if (i < cut) {
        line1 = src.substr(0, static_cast<std::size_t>(cut));
        line2 = src.substr(static_cast<std::size_t>(cut));
    } else {
        line1 = src;
        line2.clear();
    }
}

void upper_lines(int W, std::string& line1, std::string& line2) {
    if (w16(W) <= 0x27b) return;  // [BYTES seg3:0x14d6-0x14dc]
    for (std::string* s : {&line1, &line2})
        for (char& ch : *s)  // signed byte: 0x60 < c < 0x7b [BYTES seg3:0x14e8-0x14f6]
            if (static_cast<signed char>(ch) > 0x60 && static_cast<signed char>(ch) < 0x7b) ch = static_cast<char>(ch - 0x20);
}

std::string cut_name_code(std::string name) {
    if (name.size() >= 2 && name[name.size() - 2] == '^') name.resize(name.size() - 2);
    return name;
}

bool message_code_ported(int code) {
    switch (code) {
    case 7: case 8: case 9: case 132:
    case 10: case 11: case 12: case 13: case 72: case 82: case 83: case 84: case 85: case 86: case 91:
    case 94: case 111: case 112: case 0x75: case 0x76: case 0x77:
    // M5 s3 CONTACT
    case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e:
    case 0x5f: case 0x60: case 0x61: case 0x62: case 0x63: case 0x89: case 0x8a:
    case 0x29: case 0x2a: case 0x2b: case 0x2d: case 0x2e: case 0x2f: case 0x3b: case 0x41: case 0x43:
    case 0x5c: case 0xbe: case 0x51: case 0x20: case 0x49: case 0x4a:
    case 0x44: case 0x45: case 0x46: case 0x5d: case 0x65: case 0x6e: case 0x72: case 0x86: case 0xb9: case 0xc0:
    // M5 s3 integrate: the pilots', the call-ins' and the shift's codes
    case 0x17: case 0x1f: case 0x3c: case 0x3f: case 0x58: case 0x66: case 0x67: case 0x68: case 0x69:
    case 0x6a: case 0x6b: case 0x6c: case 0x6d: case 0x71: case 0xa4: case 0xb4: case 0xb5: case 0xb7:
    case 0xb8: case 0xbf:
    // M5 s3 fixer: the orders', the revenge's and the Enforcers' codes (world/pilot_orders.h)
    case 0x01: case 0x13: case 0x21: case 0x22: case 0x38: case 0x39: case 0x3e: case 0x47: case 0x4f:
    case 0x50: case 0x59: case 0x73: case 0x7f: case 0x80: case 0x81: case 0x83: case 0x88: case 0xaf:
    case 0xb0: case 0xb1: case 0xb2: case 0xb3:
    // M5 s4 COMMANDS: the replies to the commands (world/commands.h)
    case 0x02: case 0x34: case 0x05: case 0x06: case 0x0e: case 0x0f: case 0x10: case 0x11: case 0x12:
    case 0x14: case 0x15: case 0x16: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27: case 0x31:
    case 0x32: case 0x35: case 0x36: case 0x40: case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x5a:
    case 0x64: case 0x74: case 0x82: case 0x8b: case 0xbc: case 0xbd:
    // M5 s4 integrate: the arrest's and the warrant's lines (world/pilot_orders.cpp, law.cpp)
    case 0x3d: case 0x57: case 0xae:
        return true;
    default:
        // seg3:0x128f and 0x8d's seg3:0x1255 (M5 s4 COMMANDS); code 0 and codes above 0xc0 miss the
        // table (`dec ax; cmp ax, 0xbf; ja` seg3:0x14d6): the tail (M5 s4 integrate) [BYTES seg3:0x1346-0x134a].
        return (code >= 0x8c && code <= 0xa3) || static_cast<std::uint16_t>(code - 1) > 0xbf;
    }
}

void dispatch_message(MessageState& s, MessageHost& h, int recipient, int pool, int rec, int code) {
    const int player = h.player();
    if (recipient != player) return;                // [BYTES seg3:0x6fa-0x700]
    if (pool == 0xe && rec == player) return;      // [BYTES seg3:0x704-0x70d]
    if (code == 0xb7 && (h.ship_goal_pool(rec) == 0xc || h.ship_word_06(rec) == 0xc))
        return;                                     // [BYTES seg3:0x711-0x72b]
    h.set_sender(pool, rec);                        // [BYTES seg3:0x72e-0x737]
    if (h.info_mode() != 3) h.set_info_mode(3);     // seg7:0x119c(3, 0) [BYTES seg3:0x75e-0x769]
    if (h.info_exists()) h.info_flash();            // seg11:0x3e6e [BYTES seg3:0x771-0x778]
    const std::uint32_t now = h.now();
    if (pool == -1) {  // [BYTES seg3:0x77d-0x7c3]: string 2213, empty lines (DGROUP:0x3c2, 0x3c3)
        s.sender = h.lang(0x8a5);
        s.line1.clear();
        s.line2.clear();
        h.set_end(now + 3000u);
        return;
    }
    if (pool != 0xe && pool != 7 && pool != 2) return;  // [BYTES seg3:0x7c6-0x7d6]
    s.sender = h.name(pool, rec);                       // seg3:0x1542 [BYTES seg3:0x7da-0x7e7]
    // The end time [BYTES seg3:0x7f3-0x82c].
    std::uint32_t end;
    if (!h.race())
        end = now + 7000u;
    else if (pool == 7)
        end = now + 2500u;
    else
        end = now + 5000u;
    h.set_end(end);
    s.last_rec = rec;    // DGROUP:0x1cb2 [BYTES seg3:0x835]
    s.last_code = code;  // DGROUP:0x1cb0 [BYTES seg3:0x83b]
    using briefing::FArg;
    const briefing::LangLookup lang = [&h](int id) { return h.lang(id); };
    const std::function<int()> rnd = [&h]() { return h.rand(); };
    const int W = h.width();
    // seg10:0xa332(DGROUP:0x8db0, id, 2, ...): mode 2 writes from the start and splits the text
    // into DGROUP:0x8768 and DGROUP:0x87b8 (seg3:0x3498) [BYTES seg10:0xa831-0xa843].
    // DGROUP:0x93aa, the briefing's phase, changes nothing in the formatter (%N always writes CR
    // LF, shell.md Formatter, Other directives; C076), so the port passes 0.
    const auto format2 = [&](int id, const std::vector<FArg>& args) {
        briefing::lang_format_into(s.text, id, 2, args, lang, 0, rnd);
        split_lines(s.text, s.line1, s.line2);
    };
    switch (code) {  // seg3:0x1351 on code - 1 [BYTES seg3:0x1346-0x1351]
    // M4 stage 3, track AMBFLIGHT: the H key's replies (s3-ambflight.md 1.4, verified claim 2)
    // and the low-fuel warning (s3-fuel.md 2.2, verified claim 16).
    case 7:  // case 6 [BYTES seg3:0xcbb seg3:0xcc4 seg3:0xcc9 seg3:0xbb4 seg3:0xbb8 seg3:0xbc0]
        // seg10:0x744c(0x33), RESCUE.WAV; string 110 "Hold Your Horses, %s. Rescue Unit
        // Dispatched." with the pilot name DGROUP:0x380c [BYTES ENG_LANG.DLL STRING 7] id 110
        h.sound(0x33);
        format2(0x6e, {FArg(h.pilot())});
        break;
    case 8:  // case 7 [BYTES seg3:0xcde seg3:0xce8 seg3:0x8d9 seg3:0x8dd seg3:0x8e5]
        // seg10:0x744c(0x2c); string 348 "Sorry! Ambulance Service Is Busy At This Time."
        // [BYTES ENG_LANG.DLL STRING 22] id 348
        h.sound(0x2c);
        format2(0x15c, {});
        break;
    case 9:  // case 8 [BYTES seg3:0xcee seg3:0xcf0 seg3:0xcf6 seg3:0xcf8 seg3:0x8d9 seg3:0x8dd]
        // seg10:0x744c(0x2c); string 121 "FUEL LEVEL LOW. SEEK FUEL ASAP" [BYTES ENG_LANG.DLL
        // STRING 8] id 121
        h.sound(0x2c);
        format2(0x79, {});
        break;
    case 132:  // case 131 [BYTES seg3:0xcce seg3:0xcd8 seg3:0x8d9]
        // seg10:0x744c(0x2c); string 1136 "Sorry! You Have No Credit. Help Is Not Coming."
        // [BYTES ENG_LANG.DLL STRING 72] id 1136
        h.sound(0x2c);
        format2(0x470, {});
        break;
    case 10:
    case 11:
    case 12:  // cases 9-11 [BYTES seg3:0xd2d-0xd83]
        h.sound(0x6e);
        h.sound(code == 10 ? 0x2d : 0x2c);
        format2(0x86, {FArg(code + 0x75), FArg(code == 10 ? 1 : 0), FArg(0x207), FArg(0x87),
                       FArg(w16(W) > 0x2bc ? 1 : 0)});
        break;
    case 13:  // case 12 [BYTES seg3:0xcfd-0xd2a]: the sound at most every 4000 ms
        if (!(s.ram_due >= now)) {  // cmp dword [0x1cb4], now; jae
            s.ram_due = now + 4000u;
            h.sound(0x67);
        }
        format2(0x15e, {FArg(h.pilot())});
        break;
    case 82:
    case 83:  // cases 81, 82 [BYTES seg3:0xd86-0xd92]
        format2(0x205, {FArg(code + 0x2e)});
        break;
    case 84:  // case 83 [BYTES seg3:0xd95-0xd9e]
        format2(0x8a, {FArg(h.pilot())});
        break;
    case 91:   // case 90: string 687 [BYTES seg3:0x1078-0x1086]
    case 94:   // case 93: string 692 [BYTES seg3:0x109a-0x10a8]
    case 112:  // case 111: string 1064 [BYTES seg3:0x1089-0x1097]
        format2(code == 91 ? 0x2af : code == 94 ? 0x2b4 : 0x428, {FArg(4), FArg(0x2b0), FArg(h.pilot())});
        break;
    case 0x48:  // case 71: sound 0x67 (seg10:0x744c), string 652 [BYTES seg3:0xfdd-0xfea]
        h.sound(0x67);
        format2(0x28c, {});
        break;
    case 0x55:  // case 84 [BYTES seg3:0xe21-0xe56]
        if (h.capture_defenders() == 0)
            // string 574 with seg6:0x1010(DGROUP:0x9604, 100, 2): "1.0", "0.79" [BYTES seg3:0xe26
            // seg3:0xe35]
            format2(0x23e, {FArg(briefing::hundredths_text(w16(h.capture_value()), 2))});
        else
            // string 575 "%# Acquired." with (DGROUP:0x9602, 2530): %# takes 12530 for n != 1
            // [BYTES seg3:0xe44 seg3:0xe4a seg3:0xe53]
            format2(0x23f, {FArg(h.capture_defenders()), FArg(0x9e2)});
        break;
    case 0x56:  // case 85: string 680 with seg6:0x1010(DGROUP:0x9604, 100, 2) [BYTES seg3:0xe59-0xe78]
        format2(0x2a8, {FArg(briefing::hundredths_text(w16(h.capture_value()), 2))});
        break;
    // The ship-credited codes of a destroyed target (core.md Messages, Ship-credited codes; the
    // hit dispatcher's seg8:0x6589-0x66e8) (M4 s4 fixer, C021): sound 0x3b through seg10:0x744c,
    // then seg3:0x8d9's seg10:0xa332(DGROUP:0x8db0, id, 2) with no argument.
    case 0x75:  // case 116: string 1409 "Not Bad! Lets Get Every Last Virus!" [BYTES seg3:0x11d0 seg3:0x11da] [BYTES ENG_LANG.DLL STRING 89] id 1409
        h.sound(0x3b);
        format2(0x581, {});
        break;
    case 0x76:  // case 117: string 1410 "Right On!!! You're Quite A Pilot!" [BYTES seg3:0x11e0 seg3:0x11ea]
        h.sound(0x3b);
        format2(0x582, {});
        break;
    case 0x77:  // case 118: 1411 when the player's goal pool (ship word +0x32) is the literal 14, else 1412 [BYTES seg3:0x11f0 seg3:0x11fe seg3:0x1207 seg3:0x120f]
        h.sound(0x3b);
        format2(h.ship_goal_pool(player) == 0xe ? 0x583 : 0x584, {});
        break;
    case 0x6f:  // case 110: string 1062 with the team name seg13:0x218a [BYTES seg3:0x118f-0x11b5]
        format2(0x426, {FArg(h.team_name(rec))});
        break;
    // ---- M5 s3 CONTACT: the jump table seg3:0x1356 + 2·(code − 1) (s3-contact-verify.md claims
    // 28, 29, 35, 47, 48). Every case formats in mode 2 into DGROUP:0x8db0.
    // The wing taunts of seg8:0x2cd8 (ship_react.h wing_taunts) [BYTES seg3:0xda9-0xdee]
    // [BYTES ENG_LANG.DLL STRING 24] ids 373-383; sound 0x3f is GETYOU.WAV.
    case 0x19:  // seg3:0xda9: seg10:0x744c(0x3f), 373 "Oohh, That Smarts. You Owe Me A Wing!!"
        h.sound(0x3f);
        format2(0x175, {});
        break;
    case 0x1a:  // seg3:0xdb9: 375 "There Goes The Other One. Must Be Wing Night Tonight."
        format2(0x177, {});
        break;
    case 0x1b:  // seg3:0xde1: 383 "Would You Look At That.. Now Our Wings Match!"
        format2(0x17f, {});
        break;
    case 0x1c:  // seg3:0xde9: 377 "You Ripped Off My Wing And I'm Still Tougher!!"
        format2(0x179, {});
        break;
    case 0x1d:  // seg3:0xdd1: seg10:0x744c(0x3f), 379 "I May Have No Wings, But You're Still Weaker!"
        h.sound(0x3f);
        format2(0x17b, {});
        break;
    case 0x1e:  // seg3:0xdc1: 381 "I Still Have More Wings, You Lousy %D", %D = (14, DGROUP:0x9646, 0)
        format2(0x17d, {FArg(h.object_name(0xe, player, 0)), FArg(player), FArg(0)});
        break;
    // The death lines of seg4:0x7636 (ship_react.h ship_killed), no sound [BYTES seg3:0x10ab-0x10f8]
    // [BYTES ENG_LANG.DLL STRING 66] ids 1050-1054 [BYTES ENG_LANG.DLL STRING 67] id 1058
    // [BYTES ENG_LANG.DLL STRING 90] id 1425.
    case 0x5f:  // seg3:0x10b3: 1050 "This Ship Is A Write Off. I Hope My Allies Get You!"
        format2(0x41a, {});
        break;
    case 0x60:  // seg3:0x10c3: 1052 "Flame Out..Arrrgghh! Get %D!", %D = (14, sender +0x08, 5)
    case 0x61:  // seg3:0x10d8: 1054 "I'm Outta Here, For Good. I Was Killed By %D!", the same %D
        format2(code == 0x60 ? 0x41c : 0x41e,
                {FArg(h.object_name(0xe, h.ship_word(rec, 0x08), 5)), FArg(h.ship_word(rec, 0x08)), FArg(5)});
        break;
    case 0x62:  // seg3:0x10e7: 1054 with %D = (sender +0x06, sender +0x08, 2), the dword push
        format2(0x41e, {FArg(h.object_name(h.ship_word(rec, 0x06), h.ship_word(rec, 0x08), 2)),
                        FArg(h.ship_word(rec, 0x08)), FArg(2)});
        break;
    // The attack reaction's lines (world/ship_react.h attack_reaction, seg4:0x5e8e), read from the
    // jump table seg3:0x1356 for M5 s3 part B (contact-report.md "Readings no verifier has
    // checked"); no %-arguments [BYTES ENG_LANG.DLL STRING 26] ids 411-414.
    case 0x29:  // seg3:0xef3: code + 0x172 = 411 "Hey Buddy, What's up? Why Don't You Chill Out?"
    case 0x2a:  // 412 "My Patience Is Running Out. Stop It Or I'll Get You!"
    case 0x2b:  // 413 "If You Insist On A Fight, Let's Get It On!" [BYTES seg3:0xef3-0xefb seg3:0x12bb]
        format2(code + 0x172, {});
        break;
    case 0x2d:  // seg3:0xefe: code + 0x171 = 414 "I've Had Enough Dancing, You Gonna Fight Or What?"
        format2(code + 0x171, {});  // [BYTES seg3:0xefe-0xf06]
        break;
    case 0x2e:  // the table's entries for 0x2e and 0x2f are the tail seg3:0x14d6: the lines stay
    case 0x2f:  // (strings 415, 416 have no reader here) [BYTES seg3:0x1356]
        break;
    // seg3:0xf61, shared by the Enforcers' lines: rand() % 4 (seg5:0xdc, a 16-bit `idiv`) picks
    // seg10:0x744c(0x3d), (0x3e), (0x3f) or no sound, then string code + 0x1eb [BYTES
    // seg3:0xf61-0xf8a] [BYTES ENG_LANG.DLL STRING 35] ids 550, 556, 558; 559 (code 0x44) is no
    // string, so the text is empty.
    case 0x3b:  // 550 "Do Not Provoke Me! Or There Will Be Trouble."
    case 0x41:  // 556 "Looking For Trouble? Let Me Do My Job!"
    case 0x43:  // 558 "You Know, A Guy Could Get Arrested For That!"
    case 0x44: {  // 559, seg13:0xb6f2's warning (stage 4)
        const int r = h.rand() % 4;
        if (r == 0) h.sound(0x3d);
        else if (r == 1) h.sound(0x3e);
        else if (r == 2) h.sound(0x3f);
        format2(code + 0x1eb, {});
        break;
    }
    case 0x5c:  // seg3:0xf95: 950 "You're Not Supposed To Bully. Now Stop It!" [BYTES seg3:0xf95-0xf9a] [BYTES ENG_LANG.DLL STRING 60] id 950
        format2(0x3b6, {});
        break;
    case 0xbe:  // seg3:0x130b: 2005 "Stop, I Can't Fight You. I'm Out Of Gas!" [BYTES seg3:0x130b-0x1310] [BYTES ENG_LANG.DLL STRING 126] id 2005
        format2(0x7d5, {});
        break;
    case 0x51:  // seg3:0x1029: seg10:0x744c(0x3b), 665 "He Deserved That, Nobody Messes With Me." (seg13:0x2714) [BYTES seg3:0x1029-0x1036] [BYTES ENG_LANG.DLL STRING 42] id 665
        h.sound(0x3b);
        format2(0x299, {});
        break;
    // The rivals' lines (world/capture.h rivals, seg13:0x3008).
    case 0x20: {  // seg3:0xdf1: 392 "I've Captured %D. Now To Other Business..", %D = ([bp-0x70],
        // [bp-0x72], 5): the sender's order words +0x54 / +0x56, which the head stores only for a
        // pool-14 sender; another pool leaves the stack words, 0 here [DESIGN ADR 0016 decision 1]
        // [BYTES seg3:0xdf1-0xdfe seg3:0x73b-0x74f] [BYTES ENG_LANG.DLL STRING 25] id 392
        if (pool != 0xe) h.guard("seg3:0x6ec code 0x20's [bp-0x70] / [bp-0x72] unwritten for a sender outside pool 14: 0", "seg3:0xdf1");
        const int op = pool == 0xe ? h.ship_word(rec, 0x54) : 0;
        const int orr = pool == 0xe ? h.ship_word(rec, 0x56) : 0;
        format2(0x188, {FArg(h.object_name(op, orr, 5)), FArg(orr), FArg(5)});
        break;
    }
    case 0x49:  // seg3:0xfed: 654 "Hey, I Wanted That %D! ", %D = (sender +0x32, +0x34, 0), the dword push [BYTES seg3:0xfed-0xffe] [BYTES ENG_LANG.DLL STRING 41] id 654
        format2(0x28e, {FArg(h.object_name(h.ship_word(rec, 0x32), h.ship_word(rec, 0x34), 0)),
                        FArg(h.ship_word(rec, 0x34)), FArg(0)});
        break;
    case 0x4a:  // seg3:0x1001: 655 "You Ordered Him To Take What Was Mine!" [BYTES seg3:0x1001-0x1006]
        format2(0x28f, {});
        break;
    case 0x63:  // seg3:0x10ab: 1058 "I Can't Believe You Did That. I'm Your Ally!"
        format2(0x422, {});
        break;
    case 0x89:  // seg3:0x10bb: 1425 "Silly Me. I Killed Myself."
        format2(0x591, {});
        break;
    case 0x8a:  // seg3:0x1245: seg10:0x744c(0x44), 1426 "That's Enough, You Win. I'm Breaking Off Attack."
        h.sound(0x44);
        format2(0x592, {});
        break;
    // The crime report's lines (seg13:0xb6f2, stage 4) [BYTES seg3:0xf9d-0xfd2 seg3:0x116f-0x117c]
    // [BYTES ENG_LANG.DLL STRING 36] ids 570, 572 [BYTES ENG_LANG.DLL STRING 61] ids 960-964
    // [BYTES ENG_LANG.DLL STRING 45] id 707.
    case 0x45: format2(0x23a, {}); break;  // seg3:0xf9d: 570 "Stop That! You Just Broke a Good Icon!"
    case 0x46: format2(0x23c, {}); break;  // seg3:0xfad: 572 "That Was Evidence! It's Mine To Take."
    case 0x5d: format2(0x3c0, {}); break;  // seg3:0xfb5: 960 "You're Charged With Using Excessive Force."
    case 0x65: format2(0x3c1, {}); break;  // seg3:0xfbd: 961
    case 0x86: format2(0x3c2, {}); break;  // seg3:0xfc5: 962
    case 0xb9: format2(0x3c3, {}); break;  // seg3:0xfcd: 963
    case 0xc0: format2(0x3c4, {}); break;  // seg3:0xfa5: 964 "You're Guilty Of Reckless Thrusting!!"
    case 0x6e:  // seg3:0x116f: seg10:0x744c(0x30), 707 "We Do Have Laws Here. Do I Have To Send The Patrol?"
        h.sound(0x30);
        format2(0x2c3, {});
        break;
    case 0x72:  // the table's entry is the tail seg3:0x14d6 itself: the lines stay [BYTES seg3:0x1356]
        break;
    // ---- M5 s3 integrate: the codes the computer pilots (pilot_think.h, pilot_tick.h), the
    // call-in (callin.h) and the perspective shift (shift.h) raise, read from the jump table
    // seg3:0x1356 + 2·(code − 1) for the integration (no verifier has checked them). seg3:0x8d9
    // formats string id in mode 2 with no argument; seg3:0xa5f with the three words pushed before
    // it, %D's (pool, rec, mode) [BYTES seg3:0x8d9 seg3:0xa5f-0xa6b]. Strings: [BYTES ENG_LANG.DLL
    // STRING 23] id 360, [STRING 25] id 385, [STRING 27] id 426, [STRING 35] ids 551, 554,
    // [STRING 44] ids 693-703, [STRING 45] id 705, [STRING 46] id 730, [STRING 67] id 1065,
    // [STRING 90] id 1427, [STRING 122] ids 1942, 1944, [STRING 126] id 2002.
    case 0x17: {  // seg3:0xca6: 360 "I See You Want %D. Well I'm Taking It!!", %D = (player +0x06, +0x08, 5) (the dword push)
        const int p06 = h.ship_word(player, 0x06), p08 = h.ship_word(player, 0x08);
        format2(0x168, {FArg(h.object_name(p06, p08, 5)), FArg(p08), FArg(5)});
        break;
    }
    case 0x1f:  // seg3:0xda1: 385 "I Love A Good Duel!, Who Will Come Out Alive?"
        format2(0x181, {});
        break;
    case 0x3c:  // seg3:0xf61, the Enforcers' shared case above: 551 "The Interceptor Will Make You Abide By The Law!!!"
    // The arrest's lines (cases 60 and 86 of the table seg3:0x1356) [BYTES seg3:0x13ce seg3:0x1402]
    // (M5 s4 integrate).
    case 0x3d:  // 552 "Follow Me Out. And Don't Try Anything!"
    case 0x57:  // 578 "You Will Pay For That Next Wave, Tough Guy!"
    case 0x3e:  // 553 "Alright Tough Guy, I'll Resort To Force."
    case 0x3f:  // 554 "I Must Pursue Someone Else. Try To Be A Better Citizen."
    case 0x58: {  // 579, no string in ENG_LANG.DLL: an empty line
        const int r = h.rand() % 4;
        if (r == 0) h.sound(0x3d);
        else if (r == 1) h.sound(0x3e);
        else if (r == 2) h.sound(0x3f);
        format2(code + 0x1eb, {});
        break;
    }
    case 0x66: {  // seg3:0x10fb: 693 "I'm Getting %D Leave It To Me.", %D = (sender +0x32, +0x34, 5) (the dword push)
        const int gp = h.ship_word(rec, 0x32), gr = h.ship_word(rec, 0x34);
        format2(0x2b5, {FArg(h.object_name(gp, gr, 5)), FArg(gr), FArg(5)});
        break;
    }
    case 0x67:  // seg3:0x110f: seg10:0x744c(0x3f), 695 "Aye Laddy, I'm Coming For Yer Booty!"
        h.sound(0x3f);
        format2(0x2b7, {});
        break;
    case 0x68:  // seg3:0x1127: seg10:0x744c(0x2c), 697 "Someone's In Trouble. I'm Coming To Help!"
        h.sound(0x2c);
        format2(0x2b9, {});
        break;
    case 0x69:  // seg3:0x1147: seg10:0x744c(0x2c), 699 "Hold On To Your Wallet, A Pirate Is Entering!"
        h.sound(0x2c);
        format2(0x2bb, {});
        break;
    case 0x6a:  // seg3:0x1137: seg10:0x744c(0x37), 703 "The Force Is with You, A Knight Is Coming!"
        h.sound(0x37);
        format2(0x2bf, {});
        break;
    case 0x6b:  // seg3:0x115f: seg10:0x744c(0x30), 701 "You're Loaded With Stuff. Watch Out for Thieves!"
        h.sound(0x30);
        format2(0x2bd, {});
        break;
    case 0x6c:  // seg3:0x1157: 705 "Hang In There, You Could Get Help Anytime Now."
        format2(0x2c1, {});
        break;
    case 0x6d:  // seg3:0x117f: seg10:0x744c(0x2c), 730 "Abide By The Law, The Patrolman Is Entering."
        h.sound(0x2c);
        format2(0x2da, {});
        break;
    case 0x71:  // seg3:0xecb: 1065 "Reality Is Changing... You're Becoming Me!"
        format2(0x429, {});
        break;
    case 0xa4:  // seg3:0x1299: seg10:0x744c(0x30), 1427 "Are You Alright? You Seem To Have Lost Your Mind!"
        h.sound(0x30);
        format2(0x593, {});
        break;
    case 0xae:  // seg3:0x12cd: 1940 "Hey! I Have An Outstanding Warrant For Your Arrest!", the warrant
        // (law.h warrant_check) [BYTES seg3:0x14b0 seg3:0x12cd-0x12d2] (M5 s4 integrate)
        format2(0x794, {});
        break;
    case 0xb4:  // seg3:0x12d5: 1942 "The Key To Victory Is To BE The Demon."
        format2(0x796, {});
        break;
    case 0xb5:  // seg3:0xe7b: 1944 "Now Take The Enlightened Path And Free Your Demon!"
        format2(0x798, {});
        break;
    case 0xb7:  // seg3:0x131b: seg10:0x744c(0x36), 426 "I'm Heading For The Exit. See You Later..."
        h.sound(0x36);
        format2(0x1aa, {});
        break;
    case 0xb8: {  // seg3:0x132b: 426 with %D = (sbyte sender +0x148, sbyte +0x149, 5), the claim bytes
        const int cp = static_cast<std::int8_t>(h.ship_word(rec, 0x148) & 0xff);
        const int cr = static_cast<std::int8_t>((h.ship_word(rec, 0x148) >> 8) & 0xff);
        format2(0x1aa, {FArg(h.object_name(cp, cr, 5)), FArg(cr), FArg(5)});
        break;
    }
    case 0xbf:  // seg3:0x1313: 2002 "By Law, You Can't Take That. It Contains Evidence!"
        format2(0x7d2, {});
        break;
    // ---- M5 s3 fixer: the codes of the order executor seg13:0x6900, the Enforcers' seg13:0xa6bc
    // and the revenge recall seg4:0x702e, read from the jump table seg3:0x1356 for the fix (no
    // verifier has checked them). [bp-0x70] / [bp-0x72] are the sender's +0x54 / +0x56 when the
    // pool is 14 [BYTES seg3:0x73b-0x74f]; mode 1 formats DGROUP:0x8db0 without the split into
    // the two lines, which keep their text [BYTES seg3:0x96b seg10:0xa831-0xa843]. Strings:
    // [BYTES ENG_LANG.DLL STRING 19] ids 293-297, [STRING 22] ids 342-345, 367, [STRING 25] ids
    // 387, 389, 394, 396, [STRING 28] ids 444-452, [STRING 41] id 650, [STRING 43] ids 681, 683,
    // [STRING 88] ids 1401-1403, [STRING 89] ids 1416-1418, 1424, [STRING 121] ids 1920-1924.
    case 0x01: {  // seg3:0x841: DGROUP:0x870c = now + 9000, then by the sender's order +0x52 [BYTES seg3:0x841-0x85c seg3:0x9a0-0x9ab]
        h.set_end(now + 9000u);
        const int ord = h.ship_word(rec, 0x52), op = h.ship_word(rec, 0x54), orc = h.ship_word(rec, 0x56);
        const int you = op == 0xe && orc == player ? 1 : 0;  // [bp-0x82] [BYTES seg3:0x876-0x88c]
        switch (ord) {
        case 1:  // seg3:0x85f: seg10:0x744c(0x37), 293 "I'm Coming To Help You!"
            h.sound(0x37);
            format2(0x125, {});
            break;
        case 2: case 3:  // seg3:0x8a5: seg10:0x744c(0x6c); a virus: 1402 / 1403 by seg13:0x2688(sender, player) > -50
            h.sound(0x6c);
            if (op == 9)
                format2(h.attitude(rec, player) > -50 ? 0x57a : 0x57b, {});
            else  // 296 "I'm Attacking %D%< (you)%>." [BYTES seg3:0x8e8-0x912]
                format2(0x128, {FArg(h.object_name(op, orc, 5)), FArg(orc), FArg(5), FArg(you)});
            break;
        case 4:  // seg3:0x924: seg10:0x744c(0x6c), 297 "I'm Going to Destroy %D%< (you)%>."
            h.sound(0x6c);
            format2(0x129, {FArg(h.object_name(op, orc, 5)), FArg(orc), FArg(5), FArg(you)});
            break;
        case 5:  // seg3:0x86e: seg10:0x744c(0x6d), 295 "I'm Defending %D%< (you)%>."
            h.sound(0x6d);
            format2(0x127, {FArg(h.object_name(op, orc, 5)), FArg(orc), FArg(5), FArg(you)});
            break;
        case 7:  // seg3:0x95b: 367 "I'm Just Going To Take It Easy."
            format2(0x16f, {});
            break;
        case 9:  // seg3:0x963: 389 "I'm Going To Capture %D" in mode 1: the lines stay
            briefing::lang_format_into(s.text, 0x185, 1, {FArg(h.object_name(op, orc, 5)), FArg(orc), FArg(5)}, lang, 0, rnd);
            break;
        case 10:  // seg3:0x131b, code 0xb7's: seg10:0x744c(0x36), 426
            h.sound(0x36);
            format2(0x1aa, {});
            break;
        case 12:  // seg3:0x973: 444 "I'm On My Way To Get Some Gas!"
            format2(0x1bc, {});
            break;
        case 13:  // seg3:0x97b: the needs; 450 with seg10:0x744c(0x44) when the sender's +0x56 is itself, else 681
            if (orc == rec) {
                h.sound(0x44);
                format2(0x1c2, {});
            } else {
                format2(0x2a9, {});
            }
            break;
        default:  // 6, 8, 11 and the rest: the tail seg3:0x14d6, the lines stay [BYTES seg3:0x9a4 seg3:0x9b0-0x9c8]
            break;
        }
        break;
    }
    case 0x13: {  // seg3:0xae0: by the sender's order +0x52 [BYTES seg3:0xae0-0xb63]
        const int ord = h.ship_word(rec, 0x52), op = h.ship_word(rec, 0x54), orc = h.ship_word(rec, 0x56);
        switch (ord) {
        case 1:  // seg3:0xaeb: seg10:0x744c(0x44), 342
            h.sound(0x44);
            format2(0x156, {});
            break;
        case 2: case 3: case 4:  // seg3:0xafb: seg10:0x744c(0x44), 344 "Breaking Off Attack On %D."
            h.sound(0x44);
            format2(0x158, {FArg(h.object_name(op, orc, 5)), FArg(orc), FArg(5)});
            break;
        case 5:  // seg3:0xb13: seg10:0x744c(0x44), 345 "That's Long Enough %D" with %D's type 0
            h.sound(0x44);
            format2(0x159, {FArg(h.object_name(op, orc, 0)), FArg(orc), FArg(0)});
            break;
        case 6:  // seg3:0xb2b: seg10:0x744c(0x6b), 345 with type 5
            h.sound(0x6b);
            format2(0x159, {FArg(h.object_name(op, orc, 5)), FArg(orc), FArg(5)});
            break;
        case 9:  // seg3:0xb37: 387 "Something's Come Up, I Can't Get %D"
            format2(0x183, {FArg(h.object_name(op, orc, 5)), FArg(orc), FArg(5)});
            break;
        case 10:  // seg3:0xb47: 424
            format2(0x1a8, {});
            break;
        case 12:  // seg3:0xb4f: 446 "Much As I'd Like A Full Tank, I Must Go."
            format2(0x1be, {});
            break;
        default:  // 7, 8, 11 and the rest: the tail [BYTES seg3:0xb5b-0xb5d seg3:0xb68-0xb7e]
            break;
        }
        break;
    }
    case 0x21: case 0x22: {  // seg3:0xe01 / 0xe11: 394 / 396 with %D = (sender +0x54, +0x56, 5)
        const int op = h.ship_word(rec, 0x54), orc = h.ship_word(rec, 0x56);
        format2(code == 0x21 ? 0x18a : 0x18c, {FArg(h.object_name(op, orc, 5)), FArg(orc), FArg(5)});
        break;
    }
    case 0x38: format2(0x1c0, {}); break;  // seg3:0xf49: 448 "I Can't Find A Tank Of Gas Anywhere!"
    case 0x39: format2(0x1c4, {}); break;  // seg3:0xf51: 452 "There Are No Threats To Worry About."
    case 0x47: format2(0x28a, {}); break;  // seg3:0xf29: 650 "Leave Me Be! Can't You See You've Done Enough?"
    case 0x4f: case 0x50:  // seg3:0x1039 / 0x103d: seg10:0x744c(0x3b / 0x3f), the taunt DGROUP:0x8d38 split by seg3:0x3498
        h.sound(code == 0x4f ? 0x3b : 0x3f);
        s.text = h.taunt();
        split_lines(s.text, s.line1, s.line2);
        break;
    case 0x59:  // seg3:0x1058: seg10:0x744c(0x36), 683 "I've Got To Go. The Action Is Getting Too Hot!"
        h.sound(0x36);
        format2(0x2ab, {});
        break;
    case 0x73:  // seg3:0x11b8: seg10:0x744c(0x34), 1401 "This Isn't Working... You'll Have To Finish It."
        h.sound(0x34);
        format2(0x579, {});
        break;
    case 0x7f:  // seg3:0x1215: seg10:0x744c(0x3f), 1416
        h.sound(0x3f);
        format2(0x588, {});
        break;
    case 0x80: format2(0x589, {}); break;  // seg3:0x1225: 1417
    case 0x81: format2(0x58a, {}); break;  // seg3:0x122d: 1418
    case 0x83: break;                      // the tail seg3:0x14d6 itself: the lines stay [BYTES seg3:0x1356]
    case 0x88: format2(0x590, {}); break;  // seg3:0x123d: 1424
    case 0xaf: case 0xb0: case 0xb1: case 0xb2:  // seg3:0x12b3: code + 0x6d1 = 1920 ... 1923
        format2(code + 0x6d1, {});
        break;
    case 0xb3:  // seg3:0x12bf: 1924 "I Need More %D Before I Can Help You." with %D = (4, 0, 0)
        format2(0x784, {FArg(h.object_name(4, 0, 0)), FArg(0), FArg(0)});
        break;
    // ---- M5 s4 COMMANDS: the replies of the commands (world/commands.h) and their requests, read
    // from the jump table seg3:0x1356 + 2·(code - 1) (s4-commands.md 1.10, s4-commands-verify.md;
    // the listing for the argument pushes). [bp-0x74] / [bp-0x76] are the mark DGROUP:0x6e54 /
    // 0x6e52 [BYTES seg3:0x752-0x75b]. Strings: [BYTES ENG_LANG.DLL STRING 19] ids 302, 303,
    // [STRING 21] ids 328-338, [STRING 22] id 340, [STRING 23] ids 356, 358, [STRING 25] ids 388,
    // 398, 400, 401, 404, 406, [STRING 27] ids 428-438, 442, [STRING 35] id 555, [STRING 42] ids
    // 657-663, [STRING 43] id 685, [STRING 89] ids 1408, 1419, [STRING 116-118] ids 1850-1874,
    // [STRING 122] id 1948, [STRING 126] id 2000, [STRING 189] ids 3019, 3020.
    case 0x02:
    case 0x34: {  // seg3:0x9ca [BYTES seg3:0x9ca-0xade]
        h.set_request_end();  // DGROUP:0x870c = 0, DGROUP:0x9608 = 1
        // o, p, r: the recipient's +0x52 / +0x54 / +0x56 (code 2) or the request record (0x34)
        const int o = code == 2 ? h.ship_word(recipient, 0x52) : h.request_order();
        const int p = code == 2 ? h.ship_word(recipient, 0x54) : h.request_pool();
        const int r = code == 2 ? h.ship_word(recipient, 0x56) : h.request_rec();
        switch (o) {  // o - 1 against 11, the table seg3:0xac8 [BYTES seg3:0xab8-0xade]
        case 1:  // seg3:0xa85: seg10:0x744c(0x34), 3020 "Please help me %s. I'm in trouble!" with the pilot
            h.sound(0x34);
            format2(0xbcc, {FArg(h.pilot())});
            break;
        case 2: case 3: case 4: {  // seg3:0xa10 [BYTES seg3:0xa10-0xa6b]
            // 0x34 when ship r hunts the sender (goal (14, sender), +0x38 = 1), else 0x40
            const bool hunts = p == 0xe && h.ship_word(r, 0x32) == 0xe && h.ship_word(r, 0x34) == rec &&
                               h.ship_word(r, 0x38) == 1;
            h.sound(hunts ? 0x34 : 0x40);
            if (h.ship_word(recipient, 0x54) == 9)  // the recipient's +0x54, for either code
                format2(0xbcb, {});                // 3019
            else                                    // 302 "Please Attack %D" with (p, r, 5)
                format2(0x12e, {FArg(h.object_name(p, r, 5)), FArg(r), FArg(5)});
            break;
        }
        case 5:  // seg3:0xa6e: seg10:0x744c(0x34), 303 with (p, r, 5)
            h.sound(0x34);
            format2(0x12f, {FArg(h.object_name(p, r, 5)), FArg(r), FArg(5)});
            break;
        case 9:  // seg3:0xa99: 388 with (p, r, 5)
            format2(0x184, {FArg(h.object_name(p, r, 5)), FArg(r), FArg(5)});
            break;
        case 10: format2(0x1ac, {}); break;  // seg3:0xaa8: 428
        case 12: format2(0x1ba, {}); break;  // seg3:0xab0: 442
        default: break;                      // 6, 7, 8, 11 and the rest: the tail, the lines stay
        }
        break;
    }
    case 0x05:  // seg3:0xc19: 331 "No One Can Help %D." with (mark, 5)
        format2(0x14b, {FArg(h.object_name(h.mark_pool(), h.mark_rec(), 5)), FArg(h.mark_rec()), FArg(5)});
        break;
    case 0x06: format2(0x149, {}); break;  // seg3:0xc29: 329 "You'll Have To Fend For Yourself"
    case 0x0e:  // seg3:0xc31: 332 "Sorry, %s. I Will Not Fight You." with the far pilot name ds:0x380c
        format2(0x14c, {FArg(h.pilot())});
        break;
    case 0x0f: format2(0x14e, {FArg(0x14e)}); break;  // seg3:0xc3d: 334 with an unused word 334
    case 0x10:  // seg3:0xc64: seg10:0x744c(0x3c), 340 "Busy Now, I'll Get You Later..."
        h.sound(0x3c);
        format2(0x154, {});
        break;
    case 0x11:
        // seg3:0xc74: 336 "Very Well, %s, I Accept Your Challenge!" with the one word 0x380c pushed:
        // %s takes its segment from the next stack word, seg3:0x6ec's lowest local [bp-0x82],
        // which only case 1 writes. The port reads the pilot name, the pointer's offset in DGROUP
        // [DESIGN ADR 0016 decision 1] [BYTES seg3:0xc74-0xc7c seg3:0xc45-0xc4e seg3:0x6f6].
        h.guard("seg3:0x6ec code 0x11's %s segment [bp-0x82] unwritten: the pilot name", "seg3:0xc74");
        format2(0x150, {FArg(h.pilot())});
        break;
    case 0x12: format2(0x152, {}); break;  // seg3:0xc7e: 338 "I Love A Good Fight! Prepare To Die, Fool!"
    case 0x14:  // seg3:0xc54: 334 (sic: not 354) with the unused (mark, 2)
        format2(0x14e, {FArg(h.mark_pool()), FArg(h.mark_rec()), FArg(2)});
        break;
    case 0x15:  // seg3:0xc96: seg10:0x744c(0x3f), 358 "Look, Friend... Do You Want To Rumble?"
        h.sound(0x3f);
        format2(0x166, {});
        break;
    case 0x16:  // seg3:0xc86: seg10:0x744c(0x3f), 356 "I've Had Enough of You. Let's Fight It Out!"
        h.sound(0x3f);
        format2(0x164, {});
        break;
    case 0x23:  // seg3:0xe83: seg10:0x744c(0x3c), 398 "I Can't Capture %D Should I Attack It?" with (mark, 5)
        h.sound(0x3c);
        format2(0x18e, {FArg(h.object_name(h.mark_pool(), h.mark_rec(), 5)), FArg(h.mark_rec()), FArg(5)});
        break;
    case 0x24:  // seg3:0xe9b: seg10:0x744c(0x3c), 400 "What Use Have I Got For %D?" with (mark, 2)
    case 0x25:  // seg3:0xeb3: seg10:0x744c(0x3c), 401 "Can't Capture That. %D Isn't Capturable." with (mark, 2)
        h.sound(0x3c);
        format2(code == 0x24 ? 0x190 : 0x191, {FArg(h.object_name(h.mark_pool(), h.mark_rec(), 2)), FArg(h.mark_rec()), FArg(2)});
        break;
    case 0x26:  // seg3:0xed3: seg10:0x744c(0x3c), 404
        h.sound(0x3c);
        format2(0x194, {});
        break;
    case 0x27: format2(0x196, {}); break;  // seg3:0xee3: 406
    case 0x31: format2(0x1b0, {}); break;  // seg3:0xf21: 432
    case 0x32: format2(0x1ae, {}); break;  // seg3:0xf19: 430 "You Do Not Have An Ally Willing To Leave."
    case 0x35: format2(0x1b6, {}); break;  // seg3:0xf41: 438
    case 0x36: format2(0x1b4, {}); break;  // seg3:0xf39: 436
    case 0x40: format2(0x22b, {}); break;  // seg3:0xf59: 555 "We Don't Take Orders, We Enforce Order!"
    case 0x4b: format2(0x291, {}); break;  // seg3:0x1009: 657
    case 0x4c: format2(0x293, {}); break;  // seg3:0x1011: 659
    case 0x4d: format2(0x295, {}); break;  // seg3:0x1019: 661
    case 0x4e: format2(0x297, {}); break;  // seg3:0x1021: 663
    case 0x5a:  // seg3:0x1068: seg10:0x744c(0x3c), 685 "Don't Worry! I'll Be Alright."
        h.sound(0x3c);
        format2(0x2ad, {});
        break;
    case 0x64:
        // seg3:0xbfe: 328 "Sorry, Can't Attack %D" with the far pointer ds:0x380c pushed where %D's
        // (pool, rec, code) belong; the port formats an empty name [DESIGN ADR 0016 decision 1]
        // [BYTES seg3:0xbfe-0xc07].
        h.guard("seg3:0x6ec code 0x64's %D arguments are a far pointer: an empty name", "seg3:0xbfe");
        format2(0x148, {FArg(std::string{}), FArg(0), FArg(0)});
        break;
    case 0x74: format2(0x580, {}); break;  // seg3:0x11c8: 1408 "We Are Already In a Duel!"
    case 0x82: format2(0x58b, {}); break;  // seg3:0x1235: 1419
    case 0x8b:  // seg3:0x1281: 1850 "Sorry, But I Can't Help. I'm Too Low On %D." with (4, 0, 0)
        format2(0x73a, {FArg(h.object_name(4, 0, 0)), FArg(0), FArg(0)});
        break;
    case 0x8d: {  // seg3:0x1255: seg10:0x744c(0x3c), 1852 with %d = lo16(seg5:0x2b4(sender's +0x98 - now, 1000))
        h.sound(0x3c);
        const std::uint32_t until = static_cast<std::uint32_t>(static_cast<std::uint16_t>(h.ship_word(rec, 0x98))) |
                                    (static_cast<std::uint32_t>(static_cast<std::uint16_t>(h.ship_word(rec, 0x9a))) << 16);
        const std::uint32_t q = (until - now) / 1000u;  // [BYTES seg3:0x1262-0x1278]
        format2(0x73c, {FArg(static_cast<int>(static_cast<std::int16_t>(q & 0xffffu)))});
        break;
    }
    case 0xbc:  // seg3:0x12ed: 1948 "You're Gonna Get It When I Get %D!" with (4, 0, 2)
        format2(0x79c, {FArg(h.object_name(4, 0, 2)), FArg(0), FArg(2)});
        break;
    case 0xbd: {  // seg3:0x12fb: 2000 "I Don't Accept. I'm Here To Help %D." with ([bp-0x70], [bp-0x72], 5)
        // The head's words [bp-0x70] / [bp-0x72], written for a pool-14 sender only; another pool
        // leaves them unwritten, 0 here (only seg13:0x915c sends 0xbd, from pool 14)
        // [DESIGN ADR 0016 decision 1] [BYTES seg3:0x12fb seg3:0x73b-0x74f].
        if (pool != 0xe) h.guard("seg3:0x6ec code 0xbd's [bp-0x70] / [bp-0x72] unwritten for a sender outside pool 14: 0", "seg3:0x12fb");
        const int op = pool == 0xe ? h.ship_word(rec, 0x54) : 0;
        const int orr = pool == 0xe ? h.ship_word(rec, 0x56) : 0;
        format2(0x7d0, {FArg(h.object_name(op, orr, 5)), FArg(orr), FArg(5)});
        break;
    }
    default:
        if (code >= 0x8c && code <= 0xa3) {  // seg3:0x128f: string code + 0x6af, no argument [BYTES seg3:0x128f-0x1297 seg3:0x12bb]
            format2(code + 0x6af, {});
            break;
        }
        // Code 0 (seg13:0xb6f2's [bp-0xc] read as 0 for crime 7, ADR 0016 decision 1) and codes above 0xc0 miss
        // the table: the tail, the lines stay [BYTES seg3:0x1346-0x134a] (M5 s4 integrate).
        if (static_cast<std::uint16_t>(code - 1) > 0xbf) break;
        h.todo(code);
        break;
    }
    upper_lines(W, s.line1, s.line2);  // seg3:0x14d6
}

}  // namespace is::hud
