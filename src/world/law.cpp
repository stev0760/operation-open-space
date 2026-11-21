// The law (law.h). 16-bit arithmetic as the original's (ADR 0013 decision 7).
#include "world/law.h"

#include "world/callin.h"
#include "world/cheats.h"
#include "world/pilot_think.h"
#include "world/relations.h"
#include "world/world.h"

namespace is::world {

namespace {

struct Once {
    bool message = false, ship = false, last = false, crime = false, call = false, say = false;
} g_once;

bool ship_ok(int i) { return i >= 0 && i < kShipCount; }

// DGROUP:0x9316 as a loop bound over the ship table [DESIGN ADR 0016 decision 2].
int last_ship(const World& w, const char* address) {
    if (w.last_ship < kShipCount) return w.last_ship;
    guard_todo(g_once.last, "DGROUP:0x9316 past the ship table", address);
    return kShipCount - 1;
}

// Record i of the literal pool 14 (DGROUP:0x1e30); outside the storage a zeroed scratch record
// [DESIGN ADR 0016 decision 2].
const ObjectRecord& obj14(const World& w, int i, const char* address) {
    if (i >= 0 && i < w.pools.records(kPoolShips)) return w.pools.rec(kPoolShips, i);
    static ObjectRecord zero;
    zero = ObjectRecord{};
    guard_todo(g_once.ship, "a ship index outside the pool-14 records", address);
    return zero;
}

// The crime byte DGROUP:0xa14d + c (`inc byte [bx + 0xa14d]`, 8-bit wrap); c outside 0 ... 9
// reaches the bytes after the array, which the port does not keep [DESIGN ADR 0016 decision 2].
void count_crime(World& w, int code, const char* address) {
    if (code >= 0 && code < static_cast<int>(w.ba14d.size())) {
        auto& b = w.ba14d[static_cast<std::size_t>(code)];
        b = static_cast<std::uint8_t>(b + 1);
        return;
    }
    guard_todo(g_once.crime, "a crime byte DGROUP:0xa14d + c past the ten counts", address);
}

// Both crime words, DGROUP:0x964a and DGROUP:0x9648, 16-bit adds (`inc` / `add word, imm`).
void raise(LawCtx& c, std::int16_t n) {
    c.w.w964a = add16(c.w.w964a, n);
    c.w9648 = add16(c.w9648, n);
}

}  // namespace

LawWords& law_words() {
    static LawWords w;
    return w;
}

// --- LawHost defaults ---------------------------------------------------------------------------
std::int16_t LawHost::call_in(int, int, int) {
    guard_todo(g_once.call, "the law's call-in unbound", "seg13:0xadbe");
    return 0;
}
void LawHost::message(int, int, int, int) { guard_todo(g_once.say, "the law's message unbound", "seg3:0x6ec"); }
std::int16_t LawHost::team(const World& w, int ship) { return ship_ok(ship) ? ship_team(w, ship) : std::int16_t{0}; }

// --- seg13:0xb6f2 -------------------------------------------------------------------------------
std::int16_t crime_message(int c, bool& written) {
    written = true;
    switch (static_cast<std::uint16_t>(c - 1)) {  // `dec ax; cmp ax, 8; ja` [BYTES seg13:0xb753-0xb75c]
    case 0: return 0x45;  // 570 "Stop That! You Just Broke a Good Icon!" [BYTES seg13:0xb711]
    case 1: return 0x46;  // 572 "That Was Evidence! It's Mine To Take." [BYTES seg13:0xb718]
    case 2: return 0x5d;  // 960 "You're Charged With Using Excessive Force." [BYTES seg13:0xb71f]
    case 3: return 0x65;  // 961 "Rule #1 ...Donut Mess With My Doughnuts!!" [BYTES seg13:0xb726]
    case 4: return 0x72;  // the shared tail seg3:0x14d6, no line of its own [BYTES seg13:0xb737]
    case 5: return 0x86;  // 962 "Donut Deploying Devices Are Not To Be Disturbed!" [BYTES seg13:0xb73e]
    case 7: return 0xb9;  // 963 "The Prize Is For The Winner You Loser!" [BYTES seg13:0xb745]
    case 8: return 0xc0;  // 964 "You're Guilty Of Reckless Thrusting!!" [BYTES seg13:0xb74c]
    default: break;       // case 6 (c = 7) goes to seg13:0xb773 with no store [BYTES seg13:0xb76d]
    }
    written = false;
    return 0;
}

void crime_report(LawCtx& c, int s, int code) {
    World& w = c.w;
    // [bp-0xe] = 1 at seg13:0xb700 and 0 for a team-7 ship in `state` 0 at seg13:0xb847 has no
    // reader, nor has the distance [bp-0xa] [BYTES seg13:0xb700 seg13:0xb847 seg13:0xb8ad]; the
    // port keeps neither.
    if (w.kind == 7) return;                        // a duel [BYTES seg13:0xb705-0xb70a]
    if (code == 5 && c.ba163 == 5) return;          // crime 5 while 5 is acted on [BYTES seg13:0xb72d-0xb734]
    bool written = false;
    const std::int16_t m = crime_message(code, written);  // [bp-0xc]
    if (!written)
        guard_todo(g_once.message,
                   "seg13:0xb6f2's message word [bp-0xc] unwritten for crime 0, 7 or above 9 (0) "
                   "[DESIGN ADR 0016 decision 1]",
                   "seg13:0xb753");
    const int player = w.player;  // DGROUP:0x9646
    // The crime words [BYTES seg13:0xb773-0xb7ae].
    if (s == player && code != 9) raise(c, 1);
    raise(c, code == 6 ? std::int16_t{1} : std::int16_t{3});
    if (code == 2) raise(c, 4);
    // The forced path: crime 8 or F6 [BYTES seg13:0xb7b3-0xb807].
    if (code == 8 || c.w104a != 0) {
        raise(c, 2);
        c.ba163 = static_cast<std::uint8_t>(code & 0xff);  // `mov al, [bp+8]` [BYTES seg13:0xb7cd]
        (void)c.host.call_in(s, kCallInInterceptor, -1);    // seg13:0xadbe(s, 0x15, -1), AX unread [BYTES seg13:0xb7d8]
        // The speaker is DGROUP:0x9316 as the call-in left it, the new Interceptor when one was
        // built [BYTES seg13:0xb7e1].
        c.host.message(s, kPoolShips, w.last_ship, m);  // seg3:0x6ec(s, 0xe, DGROUP:0x9316, m) [BYTES seg13:0xb7ea]
        if (s == player) count_crime(w, code, "seg13:0xb7fd");
        c.w104a = 0;  // [BYTES seg13:0xb801]
        return;
    }
    // The witness: the first Enforcer k in `state` 0 (pool 14, DGROUP:0x1e30, no `alive` test),
    // not arresting or terminating, within 0x960 cells of s [BYTES seg13:0xb80a-0xb8c9].
    const std::int16_t sx = obj14(w, s, "seg13:0xb8a3").x, sy = obj14(w, s, "seg13:0xb8a3").y;
    const int last = last_ship(w, "seg13:0xb84f");
    for (int k = 0; k <= last; ++k) {  // signed `jg` [BYTES seg13:0xb84f-0xb855]
        if (c.host.team(w, k) != 7) continue;                        // [BYTES seg13:0xb859-0xb869]
        if (obj14(w, k, "seg13:0xb875").state != 0) continue;       // [BYTES seg13:0xb86b-0xb87a]
        const ShipRecord& K = w.ships[static_cast<std::size_t>(k)];
        if (K.w52 == 0x16 || K.w52 == 0x17) continue;               // [BYTES seg13:0xb87e-0xb88a]
        std::int16_t dx = 0, dy = 0;  // [bp-6], [bp-8]
        // seg8:0x285a(&[bp-6], &[bp-8], k's point, s's point) [BYTES seg13:0xb88c-0xb8a8]
        const std::int16_t d = cells_between(obj14(w, k, "seg13:0xb89a"), sx, sy, dx, dy);
        if (!(d < 0x960)) continue;                                  // signed `jl` [BYTES seg13:0xb8b0-0xb8b3]
        // Crime 6 is passed over while the player's count of it is above the served one, signed
        // bytes [BYTES seg13:0xb8b8-0xb8c9].
        if (code == 6 && static_cast<std::int8_t>(w.ba14d[6]) > static_cast<std::int8_t>(w.ba157[6])) continue;
        if (c.host.call_in(s, kCallInInterceptor, -1) == 0) {  // `or ax, ax` [BYTES seg13:0xb8cd-0xb8dd]
            c.host.message(s, kPoolShips, k, 0x44);             // 559, absent: the voice alone [BYTES seg13:0xb811-0xb81b]
            return;
        }
        c.host.message(s, kPoolShips, k, m);                    // [BYTES seg13:0xb8e1-0xb8ec]
        c.ba163 = static_cast<std::uint8_t>(code & 0xff);       // [BYTES seg13:0xb8f7]
        if (s == player) count_crime(w, code, "seg13:0xb905");  // [BYTES seg13:0xb8fa-0xb905]
        return;
    }
    // Nobody saw it: line 707 to the player, whoever s is, once a wave [BYTES seg13:0xb90b-0xb92c].
    if (c.w9648 > 4 && c.w9648 < 1000) {
        c.host.message(player, 7, 0, 0x6e);  // seg3:0x6ec(DGROUP:0x9646, 7, 0, 0x6e) [BYTES seg13:0xb924]
        c.w9648 = 1001;                      // [BYTES seg13:0xb92c]
    }
}

// --- seg13:0xb93a -------------------------------------------------------------------------------
void warrant_check(LawCtx& c) {
    World& w = c.w;
    const int last = last_ship(w, "seg13:0xb9ac");
    for (int k = 0; k <= last; ++k) {  // signed `jg` [BYTES seg13:0xb9ac-0xb9b2]
        const ShipRecord& K = w.ships[static_cast<std::size_t>(k)];
        if (K.bec == 0) continue;                                         // [BYTES seg13:0xb9b4-0xb9be]
        const ObjectRecord& O = obj14(w, k, "seg13:0xb9cc");
        if (O.state != 0) continue;                                       // [BYTES seg13:0xb9c0-0xb9d1]
        if (c.host.team(w, k) != 7) continue;                             // [BYTES seg13:0xb9d3-0xb9de]
        if (ship_sb(K, pw::kClass) <= 1) continue;                        // `jle` [BYTES seg13:0xb9e0-0xb9e5]
        if (O.on_screen == 0) continue;                                   // [BYTES seg13:0xb9e7-0xb9ec]
        // The sums over c = 0 ... 9 but 9, `cbw` of each byte, 16-bit adds [BYTES seg13:0xb9ee-0xb970].
        std::int16_t unserved = 0, served = 0;  // [bp-8], [bp-0xa]
        for (int j = 0; j < 10; ++j) {
            if (j == 9) continue;
            unserved = add16(unserved, static_cast<std::int8_t>(w.ba14d[static_cast<std::size_t>(j)]));
            served = add16(served, static_cast<std::int8_t>(w.ba157[static_cast<std::size_t>(j)]));
        }
        if (served >= unserved) continue;  // signed `jge` [BYTES seg13:0xb972-0xb978]
        if (c.host.call_in(w.player, kCallInInterceptor, -1) != 0) {  // [BYTES seg13:0xb97a-0xb98b]
            c.host.message(w.player, kPoolShips, k, 0xae);  // 1940 [BYTES seg13:0xb98d-0xb999]
            c.ba163 = w.ba161;                              // [BYTES seg13:0xb9a1-0xb9a4]
        }
        return;  // the first acting k, whatever the call-in returned [BYTES seg13:0xb98b seg13:0xb9a7]
    }
}

// --- the program --------------------------------------------------------------------------------
namespace {

class ProgramLawHost final : public LawHost {
public:
    World* w = nullptr;
    std::int16_t call_in(int ship, int kind, int msg) override {
        return w ? call_in_program(*w, ship, kind, msg) : std::int16_t{0};  // seg13:0xadbe (callin.h)
    }
    void message(int recipient, int pool, int rec, int code) override {  // seg3:0x6ec
        if (callin_links().message) return callin_links().message(recipient, pool, rec, code);
        LawHost::message(recipient, pool, rec, code);
    }
};
ProgramLawHost g_law_host;

}  // namespace

LawHost& program_law_host() { return g_law_host; }

LawCtx program_law_ctx(World& w) {
    g_law_host.w = &w;
    ContactWords& cw = contact_words();
    return LawCtx{w, g_law_host, cw.w9648, cw.ba163, cheat_state().w104a};
}

void crime_report_program(World& w, int s, int code) {
    LawCtx c = program_law_ctx(w);
    crime_report(c, s, code);
}

void warrant_check_program(World& w) {
    LawCtx c = program_law_ctx(w);
    warrant_check(c);
}

}  // namespace is::world
