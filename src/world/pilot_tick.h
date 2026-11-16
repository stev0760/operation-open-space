// The player's 1500 ms tick calls that are the computer pilots' (work/analysis/m5/s3-tick.md
// 1.4, 1.6-1.8, 1.10 as corrected by s3-tick-verify.md claims 1-5, 10-11, 14-19 and its
// Omissions; work/world/m5s3/think-report.md "Readings" for the parts read again from the
// listing): the rivals seg4:0x6700, the capture-claim reset seg13:0x3302, the call-ins seg4:0x7e2a,
// the cloaked ship's guessed position seg4:0x83ae, and seg13:0x675e's ship loop (M5 stage 3,
// track THINK).
//
// Each routine keeps the original's widths and its rand() draws in the order written (ruling R1).
// Calls into other tracks and later stages go through PilotHost (pilot_think.h).
//
// Portable, no Win32.
#pragma once

#include "world/pilot_think.h"

namespace is::world {

// seg4:0x6700(s), cdecl [BYTES seg4:0x6700-0x6a77]: with s's estimate G = (+0x06, +0x08) in
// play, for each ship j = 0 ... DGROUP:0x9316 in ascending order:
//  - the claim: j in play, alive, +0x52 != 0x0e, goal pool G.pool, not a race wave, j != s, goal
//    rec G.rec, G capturable (+0x53 != 0) and j's claim bytes not already G: seg3:0x6ec(j, 14, s,
//    0x66), 0xbf for a team-7 j, then j's claim bytes = G;
//  - the rival (while none is found yet, [bp-0x10] = 0): j of team 2 or 5, +0x52 != 0x0e, in play,
//    alive, j != s, not the player while DGROUP:0x95bc != 0, +0x52 != 2 unless team 2, and
//    seg13:0x2688(j, s) < 160 or j's grudge byte +0xc2 + s > 0; of another team than s, not
//    already after G, and not team 2 against an s of team 0: j's goal and estimate = G; within
//    1900 cells +0x36 = +0x38 = 2 and seg3:0x6ec(s, 14, j, 0x17), else 3; +0x3a = +0x3c = 2,
//    +0x52 = +0x54 = 0x0e, +0x56 = s, +0x94 = now + 500; for the player j the activity text. A j
//    whose goal is G (taken now or before) ends the rival search.
void rivals(PilotCtx& c, int s);

// seg13:0x3302(), cdecl [BYTES seg13:0x3302-0x33d5]: nothing in a race (DGROUP:0x9320 = 6); for
// every ship i = 0 ... DGROUP:0x9316 except one whose +0x38 = 2 and whose claim bytes equal its
// goal: the friend's notice seg3:0x6ec(player, 14, i, 0xb8) when the player's +0x38 != 1, i's
// claim is the player's goal, i != player and seg13:0x2688(i, player) > 0 or i is the player's
// teammate; then both claim bytes = 0xff.
void claim_reset(PilotCtx& c);

// seg4:0x7e2a(), cdecl [BYTES seg4:0x7e2a-0x8344]: the Pirate, Knight and Patrolman call-ins.
// The first two warn (code 0x6b, 0x6c) and set their deadline DGROUP:0x1d0a / 0x1d0e = now +
// 7000; a later pass after the deadline calls seg13:0xadbe(player, 3, 0x69) / (player, 4, 0x6a)
// and clears it. The Patrolman calls seg13:0xadbe(player, 0x16, 0x6d) with no warning. At most
// one rand() per block, in block order; a block that acts returns.
void callins(PilotCtx& c);

// seg4:0x83ae(), cdecl [BYTES seg4:0x83ae-0x84dd]: for each alive cloaked ship c and each other
// alive ship j (ascending) whose goal is (14, c): when the guess +0x14a is more than 350 cells from
// c, x = rand() % 4000 + c.x - 2000, then y likewise (16-bit), each wrapped once by 8·DGROUP:0x5a0
// / 8·DGROUP:0x5a2.
void cloak_guess(PilotCtx& c);

// seg13:0x675e(), cdecl [BYTES seg13:0x675e-0x688d]: the ship loop (ships i = 0 ... DGROUP:0x9316:
// with INI ShipAnim DGROUP:0x131c = 0 only once +0x156 + 10000 < now; then, when (+0x156 < now and
// i != player) or the object's `state` != 0, +0x156 = now + 5000 and seg13:0x6890(i)), the calls
// seg13:0x50b0(0, 0), seg13:0x58dc(0, 0) and seg7:0x64ac(0, 0, -1) through PilotHost::watch_tail,
// and the All Ships notice: with no IS_SMART window, DGROUP:0x9316 != 0 and SmWnd DGROUP:0x8f74 !=
// 0, DGROUP:0x95fa = the ships i != player with seg3:0x0(ship pool, i) != 0, posted when not 0.
void watch_refresh(PilotCtx& c);

// The player-in-Action-mode part of seg13:0x6890(i) and the post-loop calls are host calls.

}  // namespace is::world
