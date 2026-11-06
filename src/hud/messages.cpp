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
        return true;
    default: return false;
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
    default:
        h.todo(code);
        break;
    }
    upper_lines(W, s.line1, s.line2);  // seg3:0x14d6
}

}  // namespace is::hud
