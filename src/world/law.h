// The law (docs/re/law.md; work/analysis/m5/s4-crime.md sections 3 and 5 as verified by
// s4-crime-verify.md claims 17-24 and 58-61, M5 stage 4 part B, track LAW): the crime routine
// seg13:0xb6f2(s, c) and the warrant seg13:0xb93a(). The arrest (seg13:0xa6bc's order 0x16) is
// pilot_orders.cpp's, the prisoner's gate passage gate_exit.cpp's, the Hall of Justice src/hall's.
//
// The words. The law's DGROUP words live where the port already holds them, one home each:
//  - DGROUP:0x964a, the session crime level: World::w964a;
//  - DGROUP:0xa14d + c and DGROUP:0xa157 + c (c = 0 ... 9), the player's crimes and the served
//    ones: World::ba14d[c] and World::ba157[c] (so DGROUP:0xa152 is ba14d[5], DGROUP:0xa154
//    ba14d[7]);
//  - DGROUP:0xa161, the arrest's crime: World::ba161;
//  - DGROUP:0x9648, the wave's crime level, and DGROUP:0xa163, the crime being acted on:
//    relations.h ContactWords::w9648 and ::ba163 (contact_words());
//  - dword DGROUP:0xa147 and word DGROUP:0xa14b, the arrest record: ContactWords::da147 / ::wa14b;
//  - DGROUP:0x104a, F6's force flag: cheats.h CheatState::w104a (cheat_state(), track KEYS);
//  - DGROUP:0xa162, the arresting ship: LawWords below.
//
// Ruling R1: the routines draw no rand() of their own; the call-in's draws (seg13:0xae68) and the
// message display's (seg3:0x6ec case seg3:0xf61) happen in the callees, in the original's order.
// The original's widths: 16-bit adds to the crime words, 8-bit increments of the crime bytes,
// signed 16-bit compares. Portable, no Win32.
#pragma once

#include <cstdint>

namespace is::world {

class World;

// The law's DGROUP words that no other part of the port holds (one set per program, as DGROUP is;
// BSS, 0 at load) [DESIGN: the integrator may move them into World].
struct LawWords {
    // DGROUP:0xa162: lo8 of the arresting ship's index, written with line 552 [BYTES seg13:0xab07];
    // the restore loads header +0x189 into it [BYTES seg6:0x9704] (dialogs/save_game.h
    // SaveGlobals::a162 is the restore's copy: the integrator points it here). The Hall reads it
    // [BYTES seg16:0x6bfa-0x6ccb].
    std::uint8_t ba162 = 0;
};
LawWords& law_words();

// What the law reaches outside its words. The defaults log once and return the neutral value.
class LawHost {
public:
    virtual ~LawHost() = default;
    // seg13:0xadbe(ship, kind, msg), cdecl (callin.h call_in): returns the AX the callers test
    // with `or ax, ax` (pitfalls.md); the default returns 0 (refused).
    virtual std::int16_t call_in(int ship, int kind, int msg);
    // seg3:0x6ec(recipient, pool, rec, code), cdecl: the message display (hud/messages.h).
    virtual void message(int recipient, int pool, int rec, int code);
    // The design team of ship i: word DGROUP:0x970a + 0xad·(ship word +0x4e) (relations.h
    // ship_team on the World).
    virtual std::int16_t team(const World& w, int ship);
};

// Everything the law's routines touch. World holds DGROUP:0x9646 (player), 0x9316 (last_ship),
// 0x9320 (kind), the pools (pool 14 is DGROUP:0x1e30's), the ship records, DGROUP:0x964a and the
// crime bytes; the references are the words of other homes (above).
struct LawCtx {
    World& w;
    LawHost& host;
    std::int16_t& w9648;  // DGROUP:0x9648 (ContactWords::w9648)
    std::uint8_t& ba163;  // DGROUP:0xa163 (ContactWords::ba163)
    std::int16_t& w104a;  // DGROUP:0x104a (CheatState::w104a): read at seg13:0xb7b9, cleared at seg13:0xb801
};

// The crime codes (string 1098 + c is the Hall's charge) [BYTES ENG_LANG.DLL STRING 69] ids
// 1098-1103, [BYTES ENG_LANG.DLL STRING 70] ids 1104-1107.
constexpr int kCrimeMurder = 0;         // " Murdering An Enforcer." (seg4:0x7636 counts it)
constexpr int kCrimeIcon = 1;           // " Destroying An Icon"
constexpr int kCrimeEvidence = 2;       // " Removing Evidence"
constexpr int kCrimeDefenseless = 3;    // " Attacking The Defenseless"
constexpr int kCrimeDoughnut = 4;       // " Taking An Enforcer's Doughnut"
constexpr int kCrimeOfficer = 5;        // " Firing Upon An Officer" (seg4:0x6356, seg13:0x4ac4)
constexpr int kCrimeDispensor = 6;      // " Destroying Doughnut Dispensors"
constexpr int kCrimeResisting = 7;      // " Resisting Arrest." (seg13:0xaa22 counts it)
constexpr int kCrimeRace = 8;           // " Unsportsmanlike Race Conduct"
constexpr int kCrimeReckless = 9;       // " Being An Idiot."

// The warning message code m of seg13:0xb6f2's switch for crime c [BYTES seg13:0xb711-0xb757]
// (table seg13:0xb761): 0x45, 0x46, 0x5d, 0x65, 0x72, 0x86, -, 0xb9, 0xc0 for c = 1 ... 9.
// Crimes 0, 7 and c > 9 (and c < 0: `dec ax; cmp ax, 8; ja` is unsigned) leave [bp-0xc]
// unwritten; `written` is false for them and the result is the port's 0 [DESIGN ADR 0016
// decision 1].
std::int16_t crime_message(int c, bool& written);

// seg13:0xb6f2(s, c), cdecl [BYTES seg13:0xb6f2-0xb939]: nothing in a duel (DGROUP:0x9320 = 7) or
// for c = 5 while DGROUP:0xa163 = 5; both crime words +1 for s = P and c != 9, then +1 for c = 6
// or +3 for any other c, then +4 more for c = 2; with c = 8 or DGROUP:0x104a != 0 the forced
// path (+2, DGROUP:0xa163 = c, seg13:0xadbe(s, 0x15, -1) whatever it returns, the warning from
// ship DGROUP:0x9316, the player's byte DGROUP:0xa14d + c += 1 for s = P, DGROUP:0x104a = 0);
// else the first Enforcer k = 0 ... DGROUP:0x9316 in `state` 0, not arresting or terminating
// (+0x52 not 0x16, 0x17), within 0x960 cells of s (and for c = 6 only while the player's
// unserved count of 6 is not above the served one) calls an Interceptor: refused, message 0x44
// from k; accepted, the warning from k, DGROUP:0xa163 = c and the player's count; nobody seeing
// it with 4 < DGROUP:0x9648 < 1000: message 0x6e (707) to the player and DGROUP:0x9648 = 1001.
void crime_report(LawCtx& c, int s, int code);

// seg13:0xb93a(), cdecl [BYTES seg13:0xb93a-0xba03], every 300 frames (update.cpp, seg7:0xd78):
// at the first ship k = 0 ... DGROUP:0x9316 alive (+0xec), in `state` 0, of team 7, of class
// sbyte +0x5e > 1 (PATROLMAN, CONSTABLE) and on screen (object +0x1a != 0), when the player's
// unserved crimes (sum of sext DGROUP:0xa14d + c, c = 0 ... 8) exceed the served ones (the same
// sum of DGROUP:0xa157), seg13:0xadbe(P, 0x15, -1) and, when it returns non-zero, message 0xae
// (1940 "Hey! I Have An Outstanding Warrant For Your Arrest!") from k and DGROUP:0xa163 =
// DGROUP:0xa161; then it returns whatever the call-in returned.
void warrant_check(LawCtx& c);

// The program's LawHost: callin.h call_in_program, callin_links().message (seg3:0x6ec) and
// relations.h ship_team.
LawHost& program_law_host();
// A LawCtx on the program's words (contact_words(), cheat_state()) for World w.
LawCtx program_law_ctx(World& w);

// The entry points on the program's World (the other tracks bind these).
// seg13:0xb6f2(s, c): the crime sites' CollideHost / CaptureHost seg13_b6f2.
void crime_report_program(World& w, int s, int code);
// F6 (cheats.cpp, seg11:0x733b) sets CheatState::w104a = 1 and calls this through
// cheat_links().crime.
// seg13:0xb93a(): UpdateHost::seg13_b93a.
void warrant_check_program(World& w);

}  // namespace is::world
