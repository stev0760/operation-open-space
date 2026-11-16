// The ships' attitudes (docs/re/pilots.md "Teams and attitudes"; work/analysis/m5/s3-contact.md
// 2.2, 3.2 with s3-contact-verify.md claims 34, 36-39; s3-tick.md 1.3 with s3-tick-verify.md
// "seg13:0x2714"; the listings of seg13:0x2714, seg13:0x2e60, seg13:0x2da4 and seg13:0x2db8 read
// again for M5 stage 3 part B, track CONTACT, work/world/m5s3/contact-report.md "Readings no
// verifier has checked"):
//  - seg13:0x2688(a, b), ship a's view of ship b: its word A(a, b) plus the team table;
//  - seg4:0x5d9c(a, b), a's strength as a percentage of b's;
//  - seg13:0x2714(a, b, dt, ds, prop, kind), the attitude change, and its callee seg13:0x2e60;
//  - seg4:0x7d36(a, b, d), the fight record of a ship hitting a ship.
// The words: A(a, b) is ship a's word +0x62 + 2b (DGROUP:0x70f2 + 2·(0xad·a + b), pilot.h
// Pilot::attitude); T(x, y) is the team table word DGROUP:0x3824 + 2·(8x + y) (roster_build.h
// RosterWords::team_table, its one home); the hurt bytes are ship a's +0xf6 + 9b + k (pilot.h
// pw::kHurt0 is k = 1); the fight bytes +0x13e + b. Teams are the design word +0x38
// (DGROUP:0x970a + 0xad·n, n = ship word +0x4e).
//
// The original's widths: 16-bit `imul` products cut to their low word where the bytes test only
// AX, 16-bit `idiv` truncating toward zero, 32-bit arithmetic in seg4:0x5d9c (ADR 0013 decision
// 7). None of these routines draws a rand(); seg3:0x6ec's own draws (the formatter's %R) are the
// message display's. Portable, no Win32.
#pragma once

#include <array>
#include <cstdint>
#include <functional>

#include "world/world.h"

namespace is::world {

// ---- the design team
// The team word DGROUP:0x970a + 0xad·n of design n: rating.h DesignRating::team in
// World::design_ratings, which the roster track's seg14:0x1430 writes (roster_build.h
// set_design_team); 0 for a design outside 0 ... 7, as roster_build.h design_team [DESIGN].
std::int16_t design_team_of(const World& w, int design);
// The team of ship i's design (ship word +0x4e), the read `imul bx, [bx + 0x70de], 0xad; mov ax,
// [bx + 0x970a]` [BYTES seg13:0x273b-0x2741].
std::int16_t ship_team(const World& w, int i);

// T(x, y), the team table word DGROUP:0x3824 + 2·(8x + y), held in roster_build.h
// RosterWords::team_table (roster_words(), its one home). x or y outside 0 ... 7 reads and
// writes a scratch word, logged once [DESIGN ADR 0016 decision 2]. DGROUP:0x3894 + 2t is T(7, t)
// and DGROUP:0x3854 + 2t is T(3, t) (0x3894 = 0x3824 + 2·56, 0x3854 = 0x3824 + 2·24).
std::int16_t& team_word(int x, int y);

// DGROUP:0x4ed8 + 2k, the MASTER.SHP index of the Enforcer of class k that seg14:0x0 placed
// (roster_build.h RosterWords::enforcer_shp, its one home); seg4:0x7636 reads it for a dead ship
// without a roster record. k outside 0 ... 3 (the class byte +0x5e of a non-Enforcer:
// DGROUP:0x4ee0 and beyond) reads 0, logged once [DESIGN ADR 0016 decision 2].
std::int16_t enforcer_shp_word(int k);

// The DGROUP words of the contact routines that no other part of the World holds (one set per
// program, as DGROUP is; BSS, 0 at load) [DESIGN: world.h is another track's file; the integrator
// may move them into World]. Each names its address.
struct ContactWords {
    // DGROUP:0x2384, now + 5000 at every ship death (seg4:0x7649), is update.h
    // FrameGlobals::music_due: seg4:0x7636 sets it through CollideHost::music_hold.
    // DGROUP:0x4fd0 + 2·(3t + j): the wave of a revenge slot (seg4:0x7636). Index 24 is the word
    // DGROUP:0x5000 that the no-free-slot store of team 7 reaches (3·7 + 3 = 24) [BYTES
    // seg4:0x7815-0x7856]; what lives there is unknown, so the port holds it here and logs the
    // store once [DESIGN ADR 0016 decision 2].
    std::array<std::int16_t, 25> w4fd0{};
    // DGROUP:0x6b10 + 2·(3t + j) for 3t + j < 24, the dead ship's design (roster word +0x00 or
    // DGROUP:0x4ed8 + 2·class), followed without a gap by DGROUP:0x6b40 + 2t, the player's kills
    // per team (index 24 + t): one array, so the no-free-slot store of team 7 at index 24 lands
    // in the tally of team 0 as the original's does [BYTES seg4:0x7738-0x774a seg4:0x78aa-0x78c5].
    std::array<std::int16_t, 32> w6b10{};
    // DGROUP:0xa147 (dword) and DGROUP:0xa14b: the frame# and the wave of the player's last kill
    // of a classed Enforcer (and of the arrest, seg13:0xa6bc) [BYTES seg4:0x7780-0x778f].
    std::uint32_t da147 = 0;
    std::int16_t wa14b = 0;
    // seg4:0x7d36's tallies of the player's fights: hits by the player DGROUP:0x9680 with their
    // amounts DGROUP:0x8e00, hits on the player DGROUP:0x9682 with DGROUP:0x8e04; read by the
    // assessment text seg3:0x2386-0x2605 [BYTES seg4:0x7dbb-0x7ddc].
    std::int16_t w9680 = 0, w9682 = 0;
    std::uint32_t d8e00 = 0, d8e04 = 0;
    // DGROUP:0x9648, the wave's crime count (seg4:0x6400, seg4:0x6452); pilot.h
    // PilotGlobals::crimes_wave is the same word, joined by the integrator.
    std::int16_t w9648 = 0;
    // DGROUP:0xa163, the last crime (seg4:0x6419 stores 5; seg13:0xb6f2 is stage 4's), read by
    // the call-in (callin.h CallInLinks::dg_a163) [BYTES seg4:0x6419].
    std::uint8_t ba163 = 0;
};
ContactWords& contact_words();
// The kill tally DGROUP:0x6b40 + 2t (t = 0 ... 7) inside ContactWords::w6b10.
inline std::int16_t& kill_tally(ContactWords& c, int t) { return c.w6b10[static_cast<std::size_t>(24 + (t & 7))]; }

// ---- seg13:0x2688(a, b), cdecl [BYTES seg13:0x2688-0x2713] (verified claim 39)
// v = A(a, b) + T(team a, team b) (16-bit); v > 0 adds 20·(3 − sbyte a +0x5d), else 20·(sbyte
// a +0x5e − 3); −300 while DGROUP:0x92e8 != 0 (Demon's Gate), computed after the rest.
std::int16_t relation(const World& w, int a, int b);

// ---- seg4:0x5d9c(a, b), cdecl [BYTES seg4:0x5d9c-0x5e8d] (verified claim 34)
// 100 when a or b is outside 0 ... DGROUP:0x9316 (signed); else, all 32-bit signed, S(x) =
// (E·K) / (E/2 + 50) with E = sext(x +0x26), K = sext(x +0xd8), S(b) = 10 when 0, and the
// result lo16((100·S(a)) / S(b)) (seg5:0x110). A zero E/2 + 50 (E = −100 or −101, which no
// system word reaches) gives 0 for that quotient [DESIGN ADR 0016 decision 4].
std::int16_t strength_ratio(const World& w, int a, int b);

// What seg13:0x2714 calls outside the World.
struct AttitudeOut {
    std::function<void(int recipient, int pool, int rec, int code)> message;  // seg3:0x6ec
    std::function<std::int16_t()> info_mode;  // DGROUP:0xa76
    std::function<void()> info_paint;         // PostMessage(DGROUP:0xa7c, WM_PAINT, 0, 0L)
};
// The program's (bound once, as the other link tables); unbound members do nothing and
// info_mode reads 0 [DESIGN].
AttitudeOut& attitude_links();

// ---- seg13:0x2e60(x, y, k), Pascal `retf 6`, in declaration order [BYTES seg13:0x2e60-0x2f2c]
// k = −1 becomes 9; with y not the player seg13:0x4eec(x, 5·D[k]) and seg13:0x4eec(y, 5·D[k]),
// with y the player 12·D[k] for both (D the word table DGROUP:0x131e = 0, 2, 4, 2, 10, 5, 3, 2,
// 4, 7); then, unless k = 9, or k is 2 or 3 and x and y share a team or seg13:0x2688(x, y) > 70,
// x's hurt byte +0xf6 + 9y + k += lo8(D[k]).
void grudge_note(World& w, int x, int y, int k);

// ---- seg13:0x2714(a, b, dt, ds, prop, kind), cdecl [BYTES seg13:0x2714-0x2da2]
// The attitude change of a toward b (pilots.md; contact-report.md has the reading): nothing in a
// duel (DGROUP:0x9320 = 7) or for a = b; kind 2 forces prop = 0; dt halved, divided by 3, or 0
// as T(ta, tb) or T(tb, ta) leaves ±300, ±450, ±800; ds doubled with dt when a flees b; ds +=
// (3 − c)·ds·10 / 100 with c = sbyte a +0x5e (ds < 0) or b +0x5d; the team pair, the ship pair,
// the spreading loop over the other live ships in play with |T(tk, tb)| < 350 (message 0x51 from
// k to b in one case), the clamps of A(k, b), A(a, b), A(b, a) to ±200 and the info display's
// repaint. seg13:0x2da4 and seg13:0x2db8 have empty bodies (`sub sp, 0x6e` and the epilogue)
// [BYTES seg13:0x2da4-0x2db7 seg13:0x2db8-0x2dcb], so the port calls neither.
void attitude_change(World& w, const AttitudeOut& out, int a, int b, int dt, int ds, int prop, int kind);
// The same with attitude_links().
void attitude_change(World& w, int a, int b, int dt, int ds, int prop, int kind);

// ---- seg4:0x7d36(a, b, d), cdecl [BYTES seg4:0x7d36-0x7e29] (verified claims 36-38)
// When a's goal is (14, b) or b's goal is (14, a) (the literal 14): a's fight byte +0x13e + b =
// min(byte + k, 6), b's +0x13e + a = max(byte − k, −6), k = 2 for d > 30 else 1; then the player's
// tallies (a the player: DGROUP:0x9680 += 1, DGROUP:0x8e00 += sext(d); else b the player:
// DGROUP:0x9682, DGROUP:0x8e04). Whatever the goals, d > 0 with a the player shifts b's hit
// stamps +0x44 ... +0x4a to +0x46 ... +0x4c and stores lo16(frame# / 10) in +0x44.
void fight_record(World& w, ContactWords& cw, int a, int b, std::int16_t d);

}  // namespace is::world
