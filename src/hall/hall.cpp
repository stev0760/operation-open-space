// State 8, the Hall of Justice (hall.h). 16-bit words and signed 32-bit dwords as the original's
// (ADR 0013 decision 7).
#include "hall/hall.h"

#include <algorithm>
#include <utility>

#include "briefing/lang_format.h"
#include "world/law.h"
#include "world/pilot.h"
#include "world/purchases.h"
#include "world/records.h"
#include "world/visit.h"
#include "world/world.h"

namespace is::hall {

using world::add16;
using world::idiv16;
using world::lo16;
using world::mul16;
using world::sub16;

namespace {

std::string L(const Lang& lang, int id) { return lang ? lang(id) : std::string(); }

// `cwd; sub ax, dx; sar ax, 1`: half toward zero.
int half16(int v) { return idiv16(lo16(v), 2); }

// The ten bytes as `cbw` reads them.
std::int16_t sb(std::uint8_t b) { return static_cast<std::int8_t>(b); }

// seg6:0x10ce: the remainder as an unsigned decimal, cut at `digits` characters [BYTES
// seg6:0x10ce-0x11e2].
std::string cut2(std::uint32_t r) { return std::to_string(r).substr(0, 2); }

// A read of ship e outside the eight records: the zeroed scratch record [DESIGN ADR 0016 decision 2].
const world::ShipRecord& ship_or_scratch(const world::World& w, int e) {
    static const world::ShipRecord kZero{};
    if (e >= 0 && e < world::kShipCount) return w.ships[static_cast<std::size_t>(e)];
    static bool once = false;
    world::guard_todo(once, "seg16:0x6afe reads the officer DGROUP:0xa162 outside the ship records: a zeroed record",
                      "seg16:0x6c9f");
    return kZero;
}

}  // namespace

// ---- helpers ------------------------------------------------------------------------------------

std::string ordinal(std::int16_t n, const Lang& lang) {
    std::string s = std::to_string(n);  // seg6:0xf1a [BYTES seg17:0x7bb7]
    const std::int16_t r = world::imod16(n, 10);
    int id = 2098;
    if (r == 1 && n != 11) id = 2095;  // [BYTES seg17:0x7bd0-0x7bde]
    else if (r == 2 && n != 12) id = 2096;
    else if (r == 3 && n != 13) id = 2097;
    return s + L(lang, id);  // seg9:0x9580(id, buf) [BYTES seg17:0x7c1d]
}

std::string fixed_word(std::int16_t v) {
    std::string s;
    if (v < 0) {  // DGROUP:0x491 [BYTES seg6:0x1032-0x1040]
        s = "-";
        v = sub16(0, v);
    }
    s += std::to_string(idiv16(v, 100));
    s += ".";  // DGROUP:0x493
    s += cut2(static_cast<std::uint16_t>(world::imod16(v, 100)));
    return s;
}

std::string fixed_dword(std::int32_t v) {
    std::string s;
    if (v < 0) {  // DGROUP:0x48d [BYTES seg6:0xf63-0xf80]
        s = "-";
        v = static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(v));
    }
    s += std::to_string(v / 100);
    s += ".";  // DGROUP:0x48f
    s += cut2(static_cast<std::uint32_t>(v % 100));
    return s;
}

std::string hall_clock(const Clock& t) {
    int h = t.hour;
    const bool pm = h > 11;  // [BYTES seg10:0x9808]
    if (pm) h -= 12;
    if (h == 0) h = 12;
    std::string s = std::to_string(h) + ":";        // DGROUP:0x8e7 "", DGROUP:0x8e8 ":"
    if (t.minute < 10) s += "0";                    // DGROUP:0x8ea
    s += std::to_string(t.minute);
    s += pm ? "PM" : "AM";                          // DGROUP:0x8ec, DGROUP:0x8ef
    s += " " + std::to_string(t.mon + 1) + "/" + std::to_string(t.mday) + "/" + std::to_string(t.year);  // DGROUP:0x8f2-0x8f6
    return s;
}

std::string officer_name(const world::World& w, int i, const Lang& lang) {
    const world::ShipRecord& S = ship_or_scratch(w, i);
    if (S.roster_index >= 0) {  // [BYTES seg17:0x8e6f-0x8e96]
        const world::RosterRecord* r =
            static_cast<std::size_t>(S.roster_index) < w.roster.size() ? &w.roster[static_cast<std::size_t>(S.roster_index)] : nullptr;
        std::string s;
        if (r)
            for (char c : r->name) {
                if (c == 0) break;
                s += c;
            }
        return s;
    }
    const std::int16_t c = world::ship_sb(S, 0x5e);
    const std::int16_t ii = lo16(i);
    std::int16_t ax = add16(add16(ii, ii), 3);
    ax = mul16(ax, add16(c, 2));
    ax = mul16(ax, add16(ii, 6));
    ax = mul16(ax, add16(ii, 7));
    ax = add16(ax, ax);
    const std::int16_t d = add16(c, 3);
    std::int16_t q = 0;
    if (d != 0) {
        q = idiv16(ax, d);
    } else {
        static bool once = false;  // [DESIGN ADR 0016 decision 4]
        world::guard_todo(once, "seg17:0x8e60's divisor c + 3 = 0 gives 0", "seg17:0x8ee3");
    }
    return L(lang, 788) + " " + std::to_string(add16(q, 0x3bc));  // DGROUP:0xced [BYTES seg17:0x8ee5]
}

int confiscate(world::ShipRecord& s, int k) {
    const int w = world::ship_sb(s, 0xb8 + k);  // [BYTES seg16:0x71af-0x71bc]
    world::set_ship_b(s, 0xb8 + k, 0);
    world::set_ship_b(s, 0xae + k, 0);
    for (int j = k; j <= 8; ++j) {  // [BYTES seg16:0x71c6-0x71f0]
        world::set_ship_b(s, 0xb8 + j, world::ship_b(s, 0xb8 + j + 1));
        world::set_ship_b(s, 0xae + j, world::ship_b(s, 0xae + j + 1));
    }
    world::set_ship_b(s, 0xc1, 0);  // +0xb8 + 9 [BYTES seg16:0x71f2]
    world::set_ship_b(s, 0xb7, 0);  // +0xae + 9
    world::set_ship_b(s, 0xcb, static_cast<std::uint8_t>(world::ship_b(s, 0xcb) - 1));  // `dec byte` [BYTES seg16:0x71ff]
    return w;
}

// ---- the punishment ----------------------------------------------------------------------------

std::int32_t severity(const std::array<std::uint8_t, 10>& a14d, const std::array<std::uint8_t, 10>& a157) {
    std::int32_t S = 0;
    for (std::size_t c = 0; c < 10; ++c) {
        const std::int16_t d = sub16(sb(a14d[c]), sb(a157[c]));
        if (d == 0) continue;
        const std::int16_t w = kCrimeWeights[c];
        S += w;  // `movsx eax, word [bx+0x116a]` [BYTES seg16:0x6dec-0x6dfa]
        if (d > 1) S += mul16(idiv16(w, 5), sub16(d, 1));  // CWD drops the high word [BYTES seg16:0x6dff-0x6e1a]
    }
    return S;
}

Sentence punish(std::int32_t S, std::int16_t served, std::int32_t credit, std::int8_t slots) {
    Sentence out;
    out.severity = S;
    out.served = served;
    if (served == 0 && S > 0) {  // [BYTES seg16:0x6ebf-0x6ecc]
        S = S / 3;               // `idiv ecx` [BYTES seg16:0x6ece-0x6ede]
        if (S < 100) S = 100;    // [BYTES seg16:0x6ee3-0x6ef8]
        if (credit > 0 && credit <= S) S = credit;  // [BYTES seg16:0x6efc-0x6f26]
    }
    // credit + 100 < S, signed 32-bit, and S != 0 [BYTES seg16:0x6f2a-0x6f48].
    if (static_cast<std::int32_t>(static_cast<std::uint32_t>(credit) + 100u) < S && S != 0) {
        if (slots > 1) {  // `cmp byte +0xcb, 1; jle` [BYTES seg16:0x6f4d]
            S = 0;
            out.confiscate = true;  // seg16:0x71a2(P, 1) [BYTES seg16:0x6f51-0x6f67]
        } else {
            S = S / 2;  // [BYTES seg16:0x6f6d-0x6f7d]
        }
    }
    out.fine = S;
    return out;
}

// ---- the text (seg16:0x6afe) -------------------------------------------------------------------

HallText hall_text(world::World& w, const TextInputs& in, const Lang& lang, shell::Rng& rng) {
    using briefing::FArg;
    HallText out;
    std::string& t = out.text;
    const auto fmt = [&](int id, int mode, const std::vector<FArg>& a) { briefing::lang_format_into(t, id, mode, a, lang); };
    // The two sums, signed bytes into 16-bit words [BYTES seg16:0x6b2e-0x6b74].
    std::int16_t total = 0, served = 0;
    for (std::size_t c = 0; c < 10; ++c) {
        total = add16(total, sb(w.ba14d[c]));
        served = add16(served, sb(w.ba157[c]));
    }
    // The player's credit += sext(resources), resources = 0 [BYTES seg16:0x6b76-0x6b87].
    static world::ShipRecord scratch{};
    world::ShipRecord& P = (w.player >= 0 && w.player < world::kShipCount) ? w.ship(w.player) : (scratch = {});
    std::int32_t credit = static_cast<std::int32_t>(world::ship_d(P, 0xdc)) + world::ship_w(P, 0xce);
    world::set_ship_d(P, 0xdc, static_cast<std::uint32_t>(credit));
    world::set_ship_w(P, 0xce, 0);
    // P1 and P2 [BYTES seg16:0x6b8d-0x6bf7].
    if (served == 0) {
        fmt(908, 1, {FArg(in.pilot)});
    } else {
        fmt(909, 1, {FArg(ordinal(total, lang)), FArg(int{w.wave}), FArg(2518), FArg(in.pilot)});
    }
    if (served == 0) fmt(911, 0, {});
    // P3, the arrest [BYTES seg16:0x6bfa-0x6d4b].
    const std::int16_t c = sb(w.ba161);
    const std::int16_t e = sb(in.a162);
    if (c >= 0) {
        t += L(lang, c == 8 ? 1097 : 912);
        t += L(lang, c == 9 ? 1093 : 1098 + c);
    }
    // c < 0: string 291 goes into the local buffer, which the officer's name overwrites before any
    // append, so the paragraph starts with ", " [BYTES seg16:0x6c0a-0x6c16 seg16:0x6c87].
    t += ", ";                          // DGROUP:0x9f2
    t += officer_name(w, e, lang);      // seg17:0x8e60(e, buf)
    t += " (";                          // DGROUP:0x117e
    {
        const world::ShipRecord& E = ship_or_scratch(w, e);
        const int n = E.design_index;
        if (n >= 0 && n < world::kShipCount) t += w.design_names[static_cast<std::size_t>(n)];  // DGROUP:0x9742 + 0xad·n
    }
    t += ")";                           // DGROUP:0x1181
    t += L(lang, 913);
    t += hall_clock(in.now);            // seg10:0x97ce: the time now, not the arrest's
    t += ". ";                          // DGROUP:0x9ee
    t += "\r\n\r\n";                    // DGROUP:0x1183
    // P4, the charges [BYTES seg16:0x6d50-0x6e79].
    t += L(lang, 914);
    int k = 0;
    for (int ci = 0; ci < 10; ++ci) {
        const std::size_t cu = static_cast<std::size_t>(ci);
        const std::int16_t d = sub16(sb(w.ba14d[cu]), sb(w.ba157[cu]));
        if (d == 0) continue;
        int A = 0;
        if (k != 0) {
            A = 1;
            for (std::size_t j = cu + 1; j < 10; ++j)
                if (w.ba157[j] != w.ba14d[j]) A = 0;
        }
        const int C = (k != 0 && A == 0) ? 1 : 0;
        fmt(234, 0, {FArg(A), FArg(C), FArg(int{d}), FArg(2519), FArg(1098 + ci)});
        ++k;
    }
    const std::int32_t S = severity(w.ba14d, w.ba157);
    t += ". ";                          // DGROUP:0x9ee
    t += "\r\n \r\n";                   // DGROUP:0x1188: the phase-3 stop
    // P5 [BYTES seg16:0x6e7e-0x6eba].
    t += L(lang, 915);
    t += ". ";
    t += "\r\n\r\n";                    // DGROUP:0x118e
    // The punishment [BYTES seg16:0x6ebf-0x6f94].
    out.s = punish(S, served, credit, world::ship_sb(P, 0xcb));
    out.s.total = total;
    bool conf_ran = false;
    if (out.s.confiscate) {
        out.conf = confiscate(P, 1);
        conf_ran = true;
    }
    credit = static_cast<std::int32_t>(static_cast<std::uint32_t>(credit) - static_cast<std::uint32_t>(out.s.fine));
    world::set_ship_d(P, 0xdc, static_cast<std::uint32_t>(credit));  // `sub/sbb` [BYTES seg16:0x6f82-0x6f94]
    if (out.s.fine == 0 && !conf_ran) {
        // [bp-0xd2] has one writer, seg16:0x6f67; with the fine 0 and no confiscation the original
        // reads a stale stack word. The port reads 0, the defined path's insult (conf <= 0)
        // [DESIGN ADR 0016 decision 1] [BYTES seg16:0x6f67 seg16:0x6fcc seg16:0x70b8].
        out.conf = 0;
        out.conf_unset = true;
        static bool once = false;
        world::guard_todo(once, "seg16:0x6afe's confiscated-weapon word [bp-0xd2] unwritten: 0", "seg16:0x6fcc");
    }
    const std::int16_t a160 = sb(w.ba157[9]);  // DGROUP:0xa160, before this Hall's copy
    // P6, the sentence [BYTES seg16:0x6f98-0x7068].
    fmt(723, 0, {FArg(ordinal(total, lang))});
    if (out.s.fine != 0)
        fmt(725, 0, {FArg(fixed_word(lo16(out.s.fine)))});  // the fine's low word [BYTES seg16:0x6fe2-0x6ff8]
    else if (out.conf >= 1 && out.conf <= 27)
        fmt(726, 0, {FArg(186 + out.conf)});
    else
        fmt(630, 0, {FArg(632 + std::min<int>(a160, 3))});
    t += ". ";
    t += "\r\n\r\n";                    // DGROUP:0x1193
    // P7 [BYTES seg16:0x706d-0x70ab].
    fmt(1158, 0, {FArg(fixed_dword(credit))});
    t += "\r\n \r\n";                   // DGROUP:0x1198: the phase-5 stop
    // P8 [BYTES seg16:0x70b0-0x7162].
    if (out.s.fine != 0 || (out.conf >= 1 && out.conf <= 27))
        t += L(lang, 916);
    else if (a160 < 4)
        t += L(lang, 636);
    else
        t += L(lang, 637);
    t += ", ";
    t += in.pilot;
    t += ". ";
    if (served != 0) {
        const int r = rng.rand();  // seg5:0xdc at seg16:0x7139, the Hall's only draw
        out.drew = true;
        t += L(lang, 860 + r % 4);
    }
    // DGROUP:0xa157 = DGROUP:0xa14d [BYTES seg16:0x7167-0x7182].
    w.ba157 = w.ba14d;
    // seg11:0x3fd4(text, 1) [BYTES seg16:0x7184-0x7190]: the character after each '(' keeps its
    // case [BYTES seg11:0x40ce-0x40da] (review L3).
    t = briefing::case_3fd4(std::move(t), 1);
    return out;
}

// ---- the machine -------------------------------------------------------------------------------

int Hall::s4() const {
    const world::RosterRecord* r = world::roster_of(w_, w_.player);
    const int a = r ? static_cast<std::int8_t>(r->size) : 0;
    const int b = r ? static_cast<std::int8_t>(r->gap) : 0;
    return mul16(add16(lo16(a), idiv16(lo16(b - 6), 2)), 4);  // [BYTES seg15:0x3759-0x3785]
}

void Hall::enter(int W, int H, int play_h) {
    W_ = W;
    H_ = H;
    play_h_ = play_h;
    k_ = Words{};
    text_ = HallText{};
    k_.phase = 0;  // DGROUP:0x929c = 0 [BYTES seg9:0x6c6a]
    frame(0);      // seg16:0x6928(0) [BYTES seg9:0x6c6e]
}

void Hall::frame(int arg) {
    world::VisitState& v = world::visit_state();
    // [BYTES seg16:0x6935-0x6955]
    if (h_.ctrl_held() && v.descent != 1) v.fast = 1;
    if (h_.quick_intro() > 1) v.fast = 1;
    // [BYTES seg16:0x695b-0x6977]
    if (arg != 0 && (k_.phase > 1 || v.descent >= 1)) hall_scene();
    switch (static_cast<std::uint16_t>(k_.phase)) {  // `cmp ax, 0xa; ja` [BYTES seg16:0x6ad3-0x6adb]
    case 0:  // [BYTES seg16:0x697a-0x6988]
        h_.close_hud();
        landing();
        ++k_.phase;
        break;
    case 1:  // [BYTES seg16:0x698b]
        descent();
        break;
    case 2:  // [BYTES seg16:0x6993]
        phase_text();
        ++k_.phase;
        break;
    case 3:
    case 7:  // [BYTES seg16:0x6a14]
        type(0);
        break;
    case 4:
    case 8: {  // 1500 ms inside one frame [BYTES seg16:0x6999-0x69c5 seg16:0x6a1e-0x6a4c]
        const Tick t0 = h_.now();
        h_.pump_until(t0 + 1500);
        ++k_.phase;
        break;
    }
    case 5:  // [BYTES seg16:0x69c7-0x69d8]
        type(0);
        k_.hold = 0;
        break;
    case 6:  // [BYTES seg16:0x69db-0x6a11]
        if (k_.hold == 0) k_.hold = h_.now() + 4000;
        if (h_.now() <= k_.hold) return;
        launch_enter();
        ++k_.phase;
        break;
    case 9:  // [BYTES seg16:0x6a4f]
        launch_frame();
        break;
    case 10: {  // [BYTES seg16:0x6a57-0x6ad1]
        scr_.black(W_, H_);
        const Tick t0 = h_.now();
        v.fast = 0;
        desk_.free_all();  // seg15:0x1328(0)
        scr_.free_scene();
        k_.scene = false;
        // `sub/sbb` from t0, high word 0 and low word < 2000: the port waits to t0 + 2000, the same
        // but at the 49-day wrap of the clock [DESIGN].
        h_.pump_until(t0 + 2000);
        desk_.palette_scheme(8, 0);  // seg10:0x6e76(8, 0)
        desk_.palette_restore(0);    // seg10:0x6dd4(0)
        if (player_game) {
            h_.debriefing();  // seg9:0x6cc4(1)
        } else {
            h_.set_state(1);  // the demo [OPEN: unported]
            w_.w93a4 = 0;
        }
        break;
    }
    default:
        break;
    }
}

// seg15:0x302e in state 8 [BYTES seg15:0x302e-0x358d] (verified claim 10, CORRECTED, and the
// omission "seg15:0x302e's head and tail in state 8").
void Hall::landing() {
    world::VisitState& v = world::visit_state();
    h_.watch_windows(3);  // seg13:0x65f2(3) [BYTES seg15:0x3045]
    h_.music(-2, 1);      // seg10:0x7480(-2, 1), any state but 9
    h_.sound(0x6e);       // seg10:0x744c(0x6e)
    std::string slow;
    const int cal = h_.calibrate();
    // DGROUP:0x93a6 = 0 in state 8: the black and the loading text [BYTES seg15:0x30a0-0x3148].
    if (cal < 850 || (cal < 1600 && h_.depth() < 8)) slow = (W_ > 700 ? h_.lang(2137) : std::string()) + h_.lang(2138);
    // DGROUP:0x92d6 = (22·DGROUP:0x10)/100 (seg5:0x110, 32-bit) and DGROUP:0x92d8 = DGROUP:0x10 - 10.
    v.end_y = lo16(22 * H_ / 100);
    v.start_y = lo16(H_ - 10);
    const std::optional<desk::Rect> slot = scr_.landing(W_, H_, play_h_, slow, v.fast == 0);
    if (slot) {
        k_.scene = true;
        k_.monitor = *slot;          // SetRect(&DGROUP:0x3ca8, out[0] ... out[3]) [BYTES seg15:0x32cc]
        k_.w92de = H_;               // DGROUP:0x92de = DGROUP:0x10
        v.end_y = lo16(slot->t + 20);  // DGROUP:0x92d6 = DGROUP:0x3caa + 20 [BYTES seg15:0x34f9]
    }
    // seg15:0x1a42 (seg15:0x20ca, and seg15:0x28b2 while DGROUP:0x9e5b = 0) [BYTES seg15:0x3547].
    scr_.jet_frames(world::shop().alive == 0, rng_);
    v.descent = 0;  // DGROUP:0x92a2 = 0 [BYTES seg15:0x354b]
    // seg15:0x3caa(1): it returns 1 before its sound and set-up while DGROUP:0x928 is set [BYTES
    // seg15:0x3551 seg15:0x3cb9-0x3cc3 seg15:0x3cf9].
    if (v.fast == 0) {
        h_.sound(0x1c);
        scr_.incoming(1, v.end_y, v.start_y, k_.w92de);
    }
    // The DGROUP:0x9004 shifts by Calibrate at depth >= 8 [BYTES seg15:0x3557-0x3581] change the
    // frame threshold by the machine's speed; the port's scheduler keeps its pace [DESIGN ADR 0009].
}

// seg15:0x3590 in state 8 [BYTES seg15:0x3590-0x374a] (verified claims 11, 12).
void Hall::descent() {
    world::VisitState& v = world::visit_state();
    switch (v.descent) {
    case 0:
        if (v.fast == 0 && scr_.incoming(0, v.end_y, v.start_y, k_.w92de) == 0) return;
        v.descent_due = 0;  // [BYTES seg15:0x35c1-0x35c9]
        v.descent = 1;
        scr_.two_frames();  // seg15:0x1328(1), seg15:0x1eac
        scr_.black(W_, H_); // DGROUP:0x93a6 = 0 [BYTES seg15:0x35d8-0x35f3]
        if (v.fast == 0) {  // [BYTES seg15:0x35f8-0x3616]
            desk_.palette_fade(3, 0xcc);
            desk_.palette_fade(2, 0x89);
        }
        desk_.palette_scheme(-2, 2);  // seg10:0x6e76(-2, 2), no greys, no pad [BYTES seg15:0x36ad-0x36b6]
        descent_step(true);           // its return dropped
        return;
    case 1:
        if (v.descent_due == 0) {
            if (descent_step(false) != 0) {
                h_.sound(9);  // [BYTES seg15:0x36d7]
                v.descent_due = h_.pass_now() + 1000;
            }
        } else if (v.descent_due < h_.pass_now()) {  // unsigned, strictly [BYTES seg15:0x36f5-0x370a]
            v.descent = 2;
        }
        return;
    default:
        ++k_.phase;  // state 8 [BYTES seg15:0x371d-0x3728]
        return;
    }
}

// seg15:0x374c(first) in state 8: the first call's block [BYTES seg15:0x387d-0x38fe] (verified
// claims 13, 14 and the omission "The descent in state 8"), then the steps that state 6 shares
// (world/visit.cpp descent_step).
int Hall::descent_step(bool first) {
    world::VisitState& v = world::visit_state();
    const bool slow = h_.calibrate() < 0x514;
    std::int16_t end = lo16(v.end_y), start = lo16(v.start_y), pos = lo16(v.pos10);
    if (first) {
        scr_.black(W_, H_);  // PatBlt black [BYTES seg15:0x387d]
        v.end_y = -1;        // DGROUP:0x92d6 = -1
        hall_scene();        // seg15:0x760 [BYTES seg15:0x389d]
        // The clip DGROUP:0x3cb0 ... 0x3cb6 = (0, 0, W, DGROUP:0x10) is the screen's whole area.
        v.s4 = s4();
        end = sub16(lo16(k_.w92ba), lo16(v.s4));          // DGROUP:0x92ba - s4
        start = sub16(lo16(k_.monitor.b), lo16(v.s4));    // DGROUP:0x3cae - s4
        if (v.fast != 0) start = add16(end, 1);
        pos = mul16(start, 10);  // DGROUP:0x17fa
        v.flag = 0;              // DGROUP:0x17fc
        v.flag_prev = 0;         // DGROUP:0x17f8
        v.speed = 150;           // DGROUP:0x17f6
    } else if (v.fast != 0) {
        start = add16(end, 1);
        pos = mul16(start, 10);
    }
    if (mul16(sub16(end, 10), 10) < pos)
        v.flag = 1;
    else if (mul16(sub16(end, 60), 10) < pos)
        v.flag = 0;
    else if (mul16(add16(start, 60), 10) < pos)
        v.flag = 1;
    std::int16_t sp = sub16(lo16(v.speed), slow ? 3 : 5);
    if (sp < 0x32) sp = 0x32;
    if (sp > 0x78) sp = 0x78;
    if (v.fast != 0) sp = 0;
    v.speed = sp;
    if (v.flag == 0 && v.flag != v.flag_prev) h_.sound(0x20);
    v.flag_prev = v.flag;
    v.end_y = end;
    v.start_y = start;
    v.pos10 = pos;
    const int y = idiv16(pos, 10);
    scr_.descent_frame(W_, y, v.s4, v.flag, sub16(end, lo16(y)));
    const std::int16_t d = v.flag == 0 ? std::int16_t(slow ? 5 : 6) : std::int16_t(slow ? 3 : 4);
    pos = add16(pos, idiv16(add16(sp, sp), d));
    v.pos10 = pos;
    if (mul16(end, 10) < pos) {
        v.start_y = -1;  // DGROUP:0x92d8 = -1 in state 8 [BYTES seg15:0x3bd4-0x3bef]
        return 1;
    }
    return 0;
}

// seg15:0x760 [BYTES seg15:0x760-0x9e4] (verified claims 15, 16).
void Hall::hall_scene() {
    world::VisitState& v = world::visit_state();
    if (k_.scene) {
        if ((k_.phase > 1 || v.descent > 0) && v.end_y != -1) {
            desk_.scene_to_screen();  // [BYTES seg15:0x775-0x793]
            if ((v.descent == 1 && v.start_y != -1) || k_.phase > 6) return;
            const int s = s4();
            scr_.ship_frame(half16(W_) - half16(s), v.end_y + 5, s);  // [BYTES seg15:0x796-0x860]
            return;
        }
        scr_.free_scene();  // [BYTES seg15:0x863-0x8a0]
        k_.scene = false;
    } else if (k_.phase > 5) {
        return;
    }
    const Screen::HallScene hs = scr_.hall_scene(W_, H_);  // scene 0x73 [BYTES seg15:0x903]
    k_.scene = true;
    k_.monitor = hs.slot0;      // SetRect(&DGROUP:0x3ca8, out[0] ... out[3])
    k_.mon_x = hs.slot0.l;      // DGROUP:0x3ca4 = DGROUP:0x3d48
    k_.mon_y = hs.slot0.t;      // DGROUP:0x3ca6 = DGROUP:0x3d4a
    k_.w92ba = hs.w5;           // DGROUP:0x92ba = out[5]
    k_.w92de = H_;              // DGROUP:0x92de = DGROUP:0x10
    k_.w92d2 = k_.monitor.b - k_.monitor.t - 15;  // seg15:0x8604 [BYTES seg15:0x8611-0x861b]
    desk_.monitor(k_.monitor);  // seg15:0x8604
    desk_.scene_to_screen();    // [BYTES seg15:0x9b0-0x9d0]
}

// seg16:0x6afe [BYTES seg16:0x6b0c seg16:0x7193-0x719a].
void Hall::phase_text() {
    k_.step = 0;  // DGROUP:0x92d4 = 0
    TextInputs in;
    in.pilot = h_.pilot();
    in.now = h_.clock();
    in.a162 = world::law_words().ba162;  // DGROUP:0xa162 (law.h LawWords)
    text_ = hall_text(w_, in, [this](int id) { return h_.lang(id); }, rng_);
    k_.text = text_.text;
    type(1);  // seg15:0x1584(1): the init
}

void Hall::newline() {
    ++k_.line;  // [BYTES seg15:0x19ac]
    k_.x = k_.mon_x + 29;
    const int h = font_h_;
    const std::int16_t lines = (h + 1) != 0 ? idiv16(lo16(k_.w92d2 - 0x3a), lo16(h + 1)) : 0;
    if (lines > k_.line) {
        k_.y += h + 1;  // [BYTES seg15:0x1742]
    } else {
        // SetRect(&r, DGROUP:0x6084 ... 0x608a) = (L + 25, T + 25, R - 25, T + DGROUP:0x92d2 - 25)
        // (seg15:0x8604 [BYTES seg15:0x865d-0x8680]), ScrollDC h + 1 times [BYTES seg15:0x19d9-0x173b].
        const desk::Rect r{k_.monitor.l + 25, k_.monitor.t + 25, k_.monitor.r - 25, k_.monitor.t + k_.w92d2 - 25};
        desk_.scroll_up(r, h + 1);
        k_.cur_y -= h + 1;
    }
}

// seg15:0x1584(arg) in state 8 [BYTES seg15:0x1584-0x1a3f] (verified claims 17-20).
void Hall::type(int arg) {
    // The font from the monitor rectangle [BYTES seg15:0x1592-0x15ae].
    font_h_ = idiv16(lo16(k_.monitor.r - k_.monitor.l - 63), 15);
    font_w_ = idiv16(lo16(8 * font_h_), 21);
    const int h = font_h_;
    if (arg != 0 && k_.step == 0) {
        // The init [BYTES seg15:0x15b1-0x1612]: the CRT flash seg15:0x84b0(W/2, DGROUP:0x3ca6 +
        // DGROUP:0x92d2/2), which plays sound 0x14 outside state 0's step 7.
        const int fx = half16(W_), fy = k_.mon_y + half16(k_.w92d2);
        h_.sound_at(0x14, fx, fy, 4);
        desk_.flash(fx, fy);
        k_.done = 0;
        k_.index = 0;
        k_.line = 0;
        k_.x = k_.cur_x = k_.mon_x + 29;
        k_.y = k_.cur_y = k_.mon_y + 29;
        k_.next = h_.pass_now() + 500;
        return;
    }
    if (arg != 0 && k_.step == 1) {
        // The Space skip of states 0 and 6; state 8 never has DGROUP:0x92d4 = 1 [BYTES
        // seg15:0x1615-0x1662].
        return;
    }
    if (h_.pass_now() < k_.next) return;  // [BYTES seg15:0x1665-0x1672]
    k_.next = h_.pass_now() + 40;
    if (k_.done != 0) return;  // [BYTES seg15:0x168c]
    desk_.text_font(h, font_w_);
    std::string& t = k_.text;
    const auto at = [&t](std::size_t i) -> char { return i < t.size() ? t[i] : '\0'; };
    const int ch = h + idiv16(lo16(10 * h), -13), cw = idiv16(lo16(6 * h), 13), chh = idiv16(lo16(10 * h), 13);
    for (int n = 0; n < 2; ++n) {
        if (k_.index < t.size()) {  // a control byte below 13 other than LF becomes a CR [BYTES seg15:0x18fb-0x1915]
            const signed char sc = static_cast<signed char>(t[k_.index]);
            if (sc < 13 && sc != 0 && sc != '\n') t[k_.index] = '\r';
        }
        bool wrap = false;
        if (at(k_.index) == ' ') {
            // The next word up to its delimiter into [bp-0x5b]; GetTextExtent measures the n bytes
            // from [bp-0x5c], whose first byte no path writes: a space [DESIGN ADR 0016 decision 1]
            // [BYTES seg15:0x192c-0x1974].
            std::string word = " ";
            std::size_t j = k_.index + 1;
            while (at(j) != '\0' && at(j) != ' ' && at(j) != '\r') word += at(j++);
            const int ext = desk_.text_extent(word) + k_.x;
            if (k_.monitor.r - 25 - 9 < ext) wrap = true;  // DGROUP:0x6088 - 9 [BYTES seg15:0x1985-0x1990]
        }
        if (at(k_.index) == '\r' || wrap) {
            if (at(k_.index) == '\r') ++k_.index;  // [BYTES seg15:0x19b9-0x19bf]
            newline();
        } else {
            const char c = at(k_.index);
            if (k_.index != 0) desk_.black(k_.cur_x + 2, k_.cur_y + ch, cw, chh);  // [BYTES seg15:0x174d-0x1784]
            desk_.text_char(k_.x, k_.y, c);                                       // [BYTES seg15:0x17a4]
            k_.x += desk_.text_extent(std::string(1, c));                         // [BYTES seg15:0x17bc-0x17c7]
            desk_.block(k_.x + 2, k_.y + ch, cw, chh, 0x0000FF80);                // [BYTES seg15:0x17cb-0x1815]
            k_.cur_x = k_.x;
            k_.cur_y = k_.y;
        }
        ++k_.index;                      // [BYTES seg15:0x183a]
        h_.sound_at(0x2b, 0, 0, 0);      // seg10:0x754a(0x2b, 0, 0, 0)
        // The checks on the next character [BYTES seg15:0x184e-0x18ea].
        if (arg == 0 && at(k_.index) == '\r' && k_.index > 0 && at(k_.index - 1) == '\n') {
            k_.next = h_.pass_now() + 1900;
            return;
        }
        if (at(k_.index) == '\r' && at(k_.index + 2) == ' ') {
            ++k_.phase;  // state 8 [BYTES seg15:0x1899-0x18a4]
            return;
        }
        if (at(k_.index) == '\0') {
            ++k_.phase;  // [BYTES seg15:0x18c6-0x18cd]
            k_.done = 1; // DGROUP:0x92a = 1 [BYTES seg15:0x18e4]
            return;
        }
    }
}

// seg15:0xcb0 outside state 0 [BYTES seg15:0xcc5-0xcdd seg15:0xd01-0xd08 seg15:0xede seg15:0xee4-0xf20]
// (verified claim 51 and the omission "seg15:0xcb0's head runs in state 8").
void Hall::launch_enter() {
    world::VisitState& v = world::visit_state();
    h_.sound(0x6e);
    // dword DGROUP:0x6e28 = DGROUP:0x1e50 + 90000 has readers only in state 0 and the title
    // (src/desk/desk.cpp enter_visit) [DESIGN].
    k_.step = 5;  // DGROUP:0x92d4 = 5
    h_.sound(0x4b);
    h_.pump_until(h_.now() + 1550);
    v.start_y = v.end_y;  // DGROUP:0x92d8 = DGROUP:0x92d6
    v.end_y = 10;         // DGROUP:0x92d6 = 10
}

// seg15:0x10be from step 5 [BYTES seg15:0x1193-0x1307] (the omission "Phase 9 is the launch step
// machine").
bool Hall::launch_frame() {
    switch (k_.step) {
    case 5:
        // seg15:0x9e6 outside state 0, no copy and no 800 ms [BYTES seg15:0x1193-0x1198 seg15:0x11fe].
        scr_.lend_ship();
        k_.size = desk_.visit_ship(h_.main_ship());
        launch(2);
        k_.wait = h_.now() + 950;  // [BYTES seg15:0x1209-0x1217]
        ++k_.step;
        break;
    case 6:
        if (h_.now() > k_.wait) {  // [BYTES seg15:0x121e-0x1234]
            ++k_.step;
            launch(1);
        }
        break;
    case 7:
        if (launch(0) != 0) {  // [BYTES seg15:0x1245-0x1258]
            h_.pump();
            ++k_.step;
        }
        break;
    case 8:
        h_.pump();
        ++k_.step;
        break;
    case 9:
        h_.pump();          // [BYTES seg15:0x125b]
        desk_.free_all();   // seg15:0x1328(1), seg15:0x14be
        ++k_.phase;         // state 8 [BYTES seg15:0x12f2-0x12f9]
        return true;
    default:
        break;
    }
    return false;
}

// seg15:0x0(arg) in state 8 [BYTES seg15:0x0-0x75b]: the arg-2 block for state 8
// [BYTES seg15:0x104-0x14a], the drawing with the scene part (seg15:0x317) and the end outside
// state 0 [BYTES seg15:0x61e-0x6e0].
int Hall::launch(int arg) {
    world::VisitState& v = world::visit_state();
    const int s4v = 4 * k_.size;
    if (arg == 2) {
        h_.sound(0x20);
        // The pad DC DGROUP:0x92b6 is absent in the Hall, so DGROUP:0x92d8 keeps seg15:0xcb0's
        // value [BYTES seg15:0xb2-0xc3].
        const int cx = k_.monitor.b - k_.monitor.t;
        v.end_y = lo16(idiv16(lo16(cx), 4) + k_.mon_y);  // DGROUP:0x92d6 [BYTES seg15:0x10b-0x121]
        // The monitor area of the screen, the typed text with it, into the scene DC [BYTES seg15:0x124-0x14a].
        desk_.screen_to_scene(k_.monitor.l, k_.monitor.t, k_.monitor.r - k_.monitor.l, cx);
        k_.pos10 = v.start_y * 10;  // DGROUP:0x17c2 [BYTES seg15:0x14f-0x154]
        k_.speed = 150;
        k_.f = 0;
        k_.lum = 100;
        k_.ship = {};
    } else if (arg == 1) {
        h_.sound(0x1c);
    }
    const int f = desk::launch_frame_of(v.start_y, v.end_y, k_.pos10, k_.f);
    if (f > k_.f) {  // the fade [BYTES seg15:0x25d-0x2b0]
        const int old = k_.lum == 0 ? 1 : k_.lum;
        k_.lum = 100 - 100 * f / 40;
        const int p = 100 * k_.lum / old;
        desk_.palette_fade(3, p);
        desk_.palette_fade(2, (p + 200) / 3);
    }
    k_.f = f;
    const bool fast = h_.calibrate() >= 1400;
    k_.speed = desk::launch_speed(f, fast);
    desk::LaunchDraw d;
    d.f = f;
    d.s = k_.size;
    const int y = k_.pos10 / 10;
    d.ship = {W_ / 2 - 2 * k_.size, y, W_ / 2 - 2 * k_.size + s4v, y + s4v + 15};
    d.scene = true;  // state 8 [BYTES seg15:0x317]
    // DGROUP:0x17d2 ... 0x17d8 = (DGROUP:0x3d48, DGROUP:0x3d4a, DGROUP:0x3d48 - DGROUP:0x3ca8 +
    // DGROUP:0x3cac, DGROUP:0x92de) [BYTES seg15:0x95-0xaf].
    d.column = {k_.mon_x, k_.mon_y, k_.mon_x - k_.monitor.l + k_.monitor.r, k_.w92de};
    d.pad = {};  // no pad DC in the Hall
    if (arg == 2) k_.prev_top = y;
    d.state = 8;
    d.y0 = v.start_y;
    d.ye = v.end_y;
    d.pos10 = k_.pos10;
    d.prev_top = k_.prev_top;
    desk_.launch_frame(d);
    k_.prev_top = y;
    k_.ship = d.ship;
    k_.pos10 += desk::launch_advance10(k_.speed, fast);
    if (10 * v.end_y > k_.pos10) {
        h_.sound(0x15);  // [BYTES seg15:0x61e-0x620]
        if (k_.scene) {
            // The ship's last rectangle from the scene DC, then the scene DC freed [BYTES
            // seg15:0x62d-0x699]; step 9's seg15:0x1328(1) frees the rest.
            desk_.scene_to_screen(k_.ship.l, k_.ship.t, k_.ship.r - k_.ship.l, k_.ship.b - k_.ship.t);
            k_.scene = false;
        } else {
            desk_.black(k_.ship.l, k_.ship.t, k_.ship.r - k_.ship.l, k_.ship.b - k_.ship.t);  // [BYTES seg15:0x69e-0x6be]
        }
        const int fx = k_.ship.l + half16(k_.ship.r - k_.ship.l), fy = k_.ship.t + 12;
        h_.sound_at(0x14, fx, fy, 4);  // seg15:0x84b0 [BYTES seg15:0x6c3-0x6db]
        desk_.flash(fx, fy);
        return 1;
    }
    return 0;
}

}  // namespace is::hall
