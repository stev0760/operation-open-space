// The computer pilot's orders and reactions (M5 stage 3 part B, fixer): the order executor
// seg13:0x6900 with the Enforcers' seg13:0xa6bc, the revenge recall seg4:0x702e, the cloak and
// shield decision seg4:0x79ae and the reaction word seg4:0x8346 with its defenders' order
// seg13:0x9a18. Read from the listings for the fix (work/world/m5s3/fix-report.md, "Readings no
// verifier has checked", which gives each routine's reading with its addresses); the review's
// findings M1-M4 (work/world/m5s3/review.md).
//
// Widths are the original's (records.h helpers); the routines draw rand() in the order written
// (ruling R1). The law's arrest branch seg13:0xa98a-0xac34 (M5 stage 4 part B, track LAW) follows
// docs/re/law.md "The Enforcer's executor" step 4 (work/analysis/m5/s4-crime.md section 4 with
// s4-crime-verify.md claims 41-49); the Strategy player's activity text of seg13:0x69ad-0x69e0
// is M8's.
//
// Portable, no Win32.
#pragma once

#include <cstdint>

#include "world/pilot_think.h"

namespace is::world {

// seg4:0x8346(i, pool, rec, kind), cdecl [BYTES seg4:0x8346-0x83ac]: kind 3 against i's own
// attack goal (+0x38 = 1, goal (pool, rec)) becomes sbyte +0x146 (the previous reaction); unless i
// is the player while DGROUP:0x95bc != 0, seg13:0x9a18(i, pool, rec) sends i's defenders; byte
// +0x146 = lo8(kind); returns 1 always.
std::int16_t ship_reaction(PilotCtx& c, int i, int pool, int rec, int kind);

// seg13:0x7b40(k), cdecl [BYTES seg13:0x7b40-0x7ba1]: the player did ship k's bidding.
// seg3:0x6ec(P, 14, k, 3) (message 3); P's order +0x52 = 0 in Action mode (DGROUP:0x9134 = 0);
// then seg13:0x2714(k, P, 10, 0x16, 0, 6) when P's +0x54 = 9 (the bidding was against a virus),
// else seg13:0x2714(k, P, 15, 0x41, 1, 8). P = DGROUP:0x9646. No rand().
void bidding_report(PilotCtx& c, int k);

// seg4:0x79ae(i), cdecl [BYTES seg4:0x79ae-0x7d34]: the cloak and the shield of a computer ship
// (and of the player in Strategy mode), run at the end of each re-plan. Nothing without
// `has_cloak` +0xe7 and `has_shield` +0xe9, for a shield ship whose wing byte +0xac is 0, or for
// a cloak ship with fuel +0x1e < 100. u = the system is down (shield step +0x10 = 0 and not
// cloaked +0xe8); h = the last ship j (ascending, j != i, in play) hunting i (goal (14, i), +0x38
// = 1) within 500 cells, else -1. The toggles go through seg11:0x4b00 (has_shield) or
// seg11:0x4c72; the player's notice seg3:0x332e(a·65536 + b) with a = 0x406 ... 0x409 (the toggle
// that took) and b = 0x410 ... 0x415 (the reason).
void cloak_shield(PilotCtx& c, int i);

// seg4:0x702e(i), cdecl [BYTES seg4:0x702e-0x7633]: the revenge recall; 1 diverts the think to
// its step f. Entry: i's memory of the player (byte +0xee + P and word +0x72 + 2P) not 0, outside
// Demon's Gate, duels and races; word +0x00 counts down below 4 (message 0x88 at 4); the player
// only while DGROUP:0x95bc != 0; on screen, Engine >= 45, not a Knight, after 7 s of the wave.
// Then the first on-screen ship j < DGROUP:0x9316 with i's revenge word +0x72 + 2j > 5 and i not
// an Enforcer: of another team, or of i's team with memory byte +0xee + j above 8 or = 4, is
// taunted (the text DGROUP:0x8d38, messages 0x4f / 0x50) and, for a memory of at most 5,
// attacked; a teammate with another memory is forgotten (returns 0).
std::int16_t revenge_recall(PilotCtx& c, int i);

// seg13:0x6900(i), Pascal (`retf 2`) [BYTES seg13:0x6900-0x78b4]: +0x94 = now + 4000; team 7
// goes to seg13:0xa6bc; a RENEGADE (team 6) whose order is someone else's drops it (message
// 0x83); else the order +0x52: 1 help, 2-4 the commands, 5 follow, 7 hold near the nearest ship,
// 9 run from the hunters, 10 leave, 0x0c fetch fuel, 0x0d the needs, 0x0e the race for a
// capture; any other order is dropped (the Strategy player instead holds, order 7).
void order_executor(PilotCtx& c, int i);

// seg13:0xa6bc(e), Pascal (`retf 2`) [BYTES seg13:0xa6bc-0xadba]: +0x94 = now + 750
// [BYTES seg13:0xa6cb-0xa6e1]; the doughnut grab [BYTES seg13:0xa6e5-0xa78b]; order 0x14 the
// patrol [BYTES seg13:0xa78e-0xa921], 0x15 the warning [BYTES seg13:0xa924-0xa987], 0x16 the
// arrest [BYTES seg13:0xa98a-0xac34], anything else the terminate order 0x17 [BYTES
// seg13:0xac37-0xadac]. The arrest: with t = +0x56 gone (object `state` in pool +0x54 != 0, or
// dead) the estimate (12, 0); a prisoner attacking e after e's +0x98 with more than one of e's
// hit stamps +0x44 ... +0x4c set resists (crime 7 for the player, DGROUP:0xa154 += 1; order
// 0x17, +0xed 2 or 1, +0x98 = now + 20000, +0x94 = now); while +0xed > 0 e follows t and within
// 250 cells says 552 (+0xed = 1: the arrest record DGROUP:0xa147, 0xa14b, 0xa161, 0xa162) or 578
// (+0xed = -1), makes a computer prisoner fight (teams 3, 6, 1: order 2 on e) or leave (order
// 10), and throws the HANDCUFFS (seg4:0x584c on the first weapon-15 slot); with +0xed <= 0 the
// estimate (12, 0), the escort to gate 0; last, +0xed = 1 again unless the hook flies or holds t.
void enforcer_orders(PilotCtx& c, int e);

}  // namespace is::world
