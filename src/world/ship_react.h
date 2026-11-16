// What a ship does about being hit and what a ship's death leaves behind (docs/re/ship-contact.md;
// docs/re/pilots.md "The attack reaction"; work/analysis/m5/s3-contact.md 2.2, 3.2, 3.3 with
// s3-contact-verify.md claims 17-33 and Omissions 1 and 10; the body of seg4:0x5e8e read from the
// listing for M5 stage 3 part B, track CONTACT, work/world/m5s3/contact-report.md "Readings no
// verifier has checked"):
//  - seg4:0x5e8e(v, a, d, kind), the attack reaction of ship v hit by ship a;
//  - seg4:0x7636(ship, cp, cr, kill), the killer words, the team tallies, the revenge slots and
//    the death line of a destroyed ship;
//  - seg8:0x7ce0(T pool, T rec, a, res), the player's assist score;
//  - seg8:0x2d96-0x2e5b, the wing taunts of a ship that lost a wing.
// The calls out (messages, sounds, the crime report, the call-in, the target, seg13:0x915c, the
// music's hold) go through the collision context's CollideHost; the law is stage 4's (ruling R3):
// seg13:0xb6f2 is a host call the program logs once.
//
// The original's 16/32-bit widths (ADR 0013 decision 7). seg4:0x5e8e draws its one rand() at
// seg4:0x5fbc in the original's place (ruling R1); its callees' draws (the formatter's %R of
// seg3:0x6ec, seg13:0xadbe's) are theirs. seg4:0x7636 and seg8:0x7ce0 draw none. Portable, no
// Win32.
#pragma once

#include <cstdint>

#include "world/collide.h"
#include "world/relations.h"

namespace is::world {

// The AttitudeOut of a collision context: seg3:0x6ec and the info display through x.h.
AttitudeOut attitude_out(CollideCtx& x);

// ---- seg4:0x5e8e(v, a, d, kind), cdecl [BYTES seg4:0x5e8e-0x66ff]
// v the ship hit, a the ship that hit it, d the amount, kind 1 (seg8:0x45e0's shot hit, the
// dispatcher's ship credit) or 2 (the rivals seg13:0x3008):
//  1. d = 90 becomes 50; seg13:0x2714(v, a, −(d/4 + 4), −8 − d, 0, −1).
//  2. Kind 1 by the player on a ship the player does not hunt with v's grudge byte +0xc2 + a = 0:
//     returns unless d >= 33 and 0 <= frame#/10 − v's +0x46 <= 80 with +0x46 != 0.
//  3. Returns in Demon's Gate (DGROUP:0x92e8) or a duel (DGROUP:0x9320 = 7). Kind 1 with v's
//     hurt byte +0xf7 + 9a below 9: seg13:0x2714(v, a, −8, −10 − d, 0, 1).
//  4. A fleeing v (+0x38 = 4): with T(7, team v) > −100 and T(7, team a) < 100, crime 3 for a
//     when d > 10, rand() % 4 <= 1 and v's engine < 40, else message 0x5c to a from the first
//     Enforcer j < DGROUP:0x9316; then seg13:0x2714(v, a, −13, −d, 0, kind); return.
//  5. The early returns of an unhurt v (engine < 41 unless kind 1), of kind 2 (a teammate, v's
//     order 9, attitude > 100, > 130), of a = v, of a v under a's command, of a grudge >= 20
//     (reset to 0), of a v hunting a (not an Enforcer); the low-fuel plea 0xbe to the player
//     with the law at grudge 4 (sound 0x3d otherwise), the grudge set to 4.
//  6. seg13:0x2714(v, a, −(d/2 + 7), −10 − d, 1, kind) unless kind 1 with hurt byte <= 8; the
//     player v in Action mode or a player game returns; the strength test seg4:0x5d9c; the first
//     hurt's sound 0x3d; grudge += 1; the violence DGROUP:0x9432 += d/2 for the player's kind 1;
//     the weight w 0 ... 8 from v's engine after d / design +0x10, d, the attitude and the class.
//  7. The Enforcer branch (team 7 hit by another team): a Patrol (+0x5e > 1) warns (0x3b, order
//     0x15, DGROUP:0x964a += 1, DGROUP:0x9648 += 3) or calls an Interceptor seg13:0xadbe(a, 0x15,
//     0x3c) (DGROUP:0xa163 = 5, or message 0x43), the crime bytes and order 0x14; an Interceptor or
//     Terminator fights back (order 0x3e) or says 0x41.
//  8. Other ships answer 0x29/0x2a (liking), 0x2d/0x2e (not), 0x2b/0x2f with sound 0x3e, or turn
//     on a: seg3:0xf4(v, 14, a) and seg13:0x915c(v), a FUZZY ONE handing the fight to the last
//     live AVENGER in play with engine > 40 (v replaced).
//  9. v's grudge byte +0xc2 + a += w (seg4:0x66e9), on every path that reaches step 6.
void attack_reaction(CollideCtx& x, int v, int a, int d, int kind);

// ---- seg4:0x7636(ship, cp, cr, kill), cdecl [BYTES seg4:0x7636-0x79ac] (verified claims 17-29)
// DGROUP:0x2384 = now + 5000; the credit (cp, cr) moved to a ship that hurt the dead one most
// (its hurt bytes +0xf7 + 9j, > 7, +5 and +100 for the credited ship); the killer words +0x06 /
// +0x08; the player's kill tally DGROUP:0x6b40 + 2t; an Enforcer's death bookkeeping
// (DGROUP:0x9346, the crime byte DGROUP:0xa14d and the arrest record, or the snapshot
// DGROUP:0xa157); the revenge slot DGROUP:0x4eec / 0x4fd0 / 0x6b10 + 2(3t + j) with the index-3t+3
// overrun of a full team kept inside ContactWords; the death lines 0x5f ... 0x63, 0x89 sent to the
// player. Every pool test is the literal 14 (pitfalls.md).
void ship_killed(CollideCtx& x, int ship, int cp, int cr, int kill);

// ---- seg8:0x7ce0(T pool, T rec, a, res), near [BYTES seg8:0x7ce0-0x7d66] (verified claims 30, 31)
// n = Σ over the 8 hit-history entries (World::w937a, hit_pool, hit_rec) naming (T pool, T rec)
// of 1 (word 0) and 3 (frame#/10 − sext(word) with high word 0 and low word below 20); n > 6
// calls the kill scorer seg8:0x7be8(T pool, T rec, a, res).
void assist_score(CollideCtx& x, int tp, int tr, std::int16_t a, std::int16_t res);

// ---- seg8:0x2d96-0x2e5b, the taunts of seg8:0x2cd8 (verified claims 32, 33, 66)
// For each j <= DGROUP:0x9316 whose goal is (DGROUP:0x9324, ship) with +0x38 = 1 (no alive test,
// no j != ship test): seg3:0x6ec(j, DGROUP:0x9324, ship, code), code 0x1b when j's wing bytes
// +0xac/+0xad equal the torn ship's, else 0x1e (the torn ship keeps a wing, j has none), 0x1c or
// 0x19 (both keep one: seg4:0x5d9c(ship, j) > 95 or not), 0x1d or 0x1a (the torn ship has none).
// The caller runs it only while DGROUP:0x92e4 = 0.
void wing_taunts(CollideCtx& x, int ship);

}  // namespace is::world
