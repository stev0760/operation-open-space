// The computer pilot's per-ship words (docs/re/pilots.md "The words a computer ship lives by";
// work/analysis/m5/s3-tick.md 2.1 as corrected by s3-tick-verify.md) and the DGROUP words the
// pilot keeps (M5 stage 3, track THINK).
//
// The words live where the original keeps them: in the 0x15a-byte ship record DGROUP:0x7090 +
// 0x15a·i (records.h ShipRecord). Most of them sit in the record's reserved byte blocks (r98,
// rc2, rf6, r149), so `Pilot` is a view onto the record's bytes at the original's offsets, with a
// getter and a setter per field; nothing is copied, so the tether, the gun, the collision code
// and the HUD, which read ShipRecord's named members (w32, w34, w38, w52 ...), see the same
// words [DESIGN]. Widths are the original's: `w` a signed word, `b` an unsigned byte, `sb` a
// signed byte (read through CBW), `d` a dword (unsigned 32-bit compares with the DGROUP:0x1e50
// clock).
//
//   pilot(ships, i).order()            // word +0x52
//   pilot(ships, i).order(0x16);       // set it
//
// Portable, no Win32.
#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <string>

#include "world/records.h"

namespace is::world {

// --- Raw access to a ship record's bytes at the original's offsets ---------------------------
// ShipRecord is standard layout with every member at its original offset (records.h), so its
// object representation is the original's 0x15a bytes.
inline std::uint8_t ship_b(const ShipRecord& s, int off) {
    return reinterpret_cast<const unsigned char*>(&s)[off];
}
inline std::int8_t ship_sb(const ShipRecord& s, int off) {
    const std::uint8_t v = ship_b(s, off);
    return static_cast<std::int8_t>(v < 0x80 ? v : v - 0x100);
}
inline void set_ship_b(ShipRecord& s, int off, std::uint8_t v) {
    reinterpret_cast<unsigned char*>(&s)[off] = v;
}
inline void set_ship_sb(ShipRecord& s, int off, std::int8_t v) {
    set_ship_b(s, off, static_cast<std::uint8_t>(v));
}
inline std::int16_t ship_w(const ShipRecord& s, int off) {
    std::uint16_t v = 0;
    std::memcpy(&v, reinterpret_cast<const unsigned char*>(&s) + off, 2);
    return to_i16(v);
}
inline void set_ship_w(ShipRecord& s, int off, std::int16_t v) {
    const std::uint16_t u = static_cast<std::uint16_t>(v);
    std::memcpy(reinterpret_cast<unsigned char*>(&s) + off, &u, 2);
}
inline std::uint32_t ship_d(const ShipRecord& s, int off) {
    std::uint16_t lo = 0, hi = 0;
    std::memcpy(&lo, reinterpret_cast<const unsigned char*>(&s) + off, 2);
    std::memcpy(&hi, reinterpret_cast<const unsigned char*>(&s) + off + 2, 2);
    return lo | (static_cast<std::uint32_t>(hi) << 16);
}
inline void set_ship_d(ShipRecord& s, int off, std::uint32_t v) {
    const std::uint16_t lo = static_cast<std::uint16_t>(v), hi = static_cast<std::uint16_t>(v >> 16);
    std::memcpy(reinterpret_cast<unsigned char*>(&s) + off, &lo, 2);
    std::memcpy(reinterpret_cast<unsigned char*>(&s) + off + 2, &hi, 2);
}

// --- The offsets (ship word +N is DGROUP:0x7090 + 0x15a·i + N) --------------------------------
namespace pw {
// The estimate: this think's plan [BYTES seg4:0xb2f-0xb4b seg4:0x5443-0x545f].
constexpr int kEstPool = 0x06;    // w  DGROUP:0x7096 pool of the object (-1 forces a think, seg4:0xcd2)
constexpr int kEstRec = 0x08;     // w  DGROUP:0x7098 record
constexpr int kEstAct = 0x36;     // w  DGROUP:0x70c6 activity: 1 attack, 2 capture/pick up/leave, 3 go to/follow, 4 defend, 5 hold
constexpr int kEstApp = 0x3a;     // w  DGROUP:0x70ca approach: 2 head for, 3/4 veer, 5 away, 7 one think of 3
// The goal: what the ship does (the estimate copied at a goal change, seg4:0x686-0x6eb).
constexpr int kGoalPool = 0x32;   // w  DGROUP:0x70c2
constexpr int kGoalRec = 0x34;    // w  DGROUP:0x70c4
constexpr int kGoalAct = 0x38;    // w  DGROUP:0x70c8
constexpr int kGoalApp = 0x3c;    // w  DGROUP:0x70cc
constexpr int kThinks = 0x30;     // w  DGROUP:0x70c0 the think count (seg4:0x848)
constexpr int kTargetPool = 0x3e; // w  DGROUP:0x70ce the target words seg3:0xf4 writes
constexpr int kTargetRec = 0x40;  // w  DGROUP:0x70d0
constexpr int kHit0 = 0x44;       // w ×5 DGROUP:0x70d4 + 2k: hit stamps frame#/10 (+0x44 ... +0x4c)
// The order words: set by commands, call-ins and the Enforcer code.
constexpr int kOrder = 0x52;      // w  DGROUP:0x70e2: 0 none, -1 re-plan, 1 help, 2-4 commands, 5, 6,
                                  //    7 wait for the ambulance, 9, 10 leave, 0x0e race for a capture,
                                  //    0x14 patrol, 0x15 warn, 0x16 arrest, 0x17 terminate, 0x3e fight back
constexpr int kOrderPool = 0x54;  // w  DGROUP:0x70e4 the order's object
constexpr int kOrderRec = 0x56;   // w  DGROUP:0x70e6
constexpr int kOrderBy = 0x58;    // w  DGROUP:0x70e8 who gave it
constexpr int kAllies = 0x5d;     // sb DGROUP:0x70ed the allies byte (SHP +0x04, seg14:0x158e)
constexpr int kClass = 0x5e;      // sb DGROUP:0x70ee the enemies byte; for team 7 the class: 0 Terminator,
                                  //    1 Intercept, > 1 Patrol (SHP +0x05, seg14:0x1596)
constexpr int kWeaponClass = 0x5f;  // b DGROUP:0x70ef the selected weapon's class 0-3 (seg4:0x6e70)
constexpr int kNoCap = 0x61;      // b  DGROUP:0x70f1 "no speed cap this think" (seg11:0x5c4e)
constexpr int kAttitude0 = 0x62;  // w ×8 DGROUP:0x70f2 + 2j: A(i, j), this ship's view of ship j
constexpr int kRevenge0 = 0x72;   // w ×8 DGROUP:0x7102 + 2j (seg4:0x702e, stage 4)
constexpr int kEntryTime = 0x88;  // d  DGROUP:0x7118 the wave-entry time (seg4:0x54e3)
constexpr int kReplanAt = 0x94;   // d  DGROUP:0x7124 the re-plan deadline (seg4:0x3b1)
constexpr int kOrderUntil = 0x98; // d  DGROUP:0x7128 the order's deadline
constexpr int kD9c = 0x9c;        // d  DGROUP:0x712c 1 becomes now + 5000 at a re-plan (seg4:0x548)
constexpr int kGoalSince = 0xa4;  // d  DGROUP:0x7134 the time of the last goal change (the 19000 ms give-up)
constexpr int kGoalSince2 = 0xa8; // d  DGROUP:0x7138 the same time, a second copy
constexpr int kGrudge0 = 0xc2;    // sb ×8 DGROUP:0x7152 + j: how often j hurt this ship; 0xff = revenge target
constexpr int kWeaponWait = 0xca; // b  DGROUP:0x715a the weapon wait (seg4:0xa4c)
constexpr int kSlots = 0xcb;      // b  DGROUP:0x715b the weapon slots in use + 1
constexpr int kGunWait = 0xcc;    // b  DGROUP:0x715c the gun wait
constexpr int kCalledFor = 0xcd;  // sb DGROUP:0x715d the ship this one was called in for, -1 none
constexpr int kResources = 0xce;  // w  DGROUP:0x715e resources
constexpr int kWait2 = 0xd4;      // w  DGROUP:0x7164 the second weapon wait
constexpr int kSpeedCap = 0xda;   // w  DGROUP:0x716a the computer speed cap (seg11:0x5ce9)
constexpr int kCredit = 0xdc;     // d  DGROUP:0x716c credit
constexpr int kHasCloak = 0xe7;   // b  DGROUP:0x7177
constexpr int kCloaked = 0xe8;    // b  DGROUP:0x7178
constexpr int kHasShield = 0xe9;  // b  DGROUP:0x7179
constexpr int kNoTurn = 0xeb;     // b  DGROUP:0x717b the heading is held while set (seg13:0xd7a)
constexpr int kAlive = 0xec;      // b  DGROUP:0x717c
constexpr int kOneShot = 0xed;    // sb DGROUP:0x717d 1 say the line, 2 second round, -1
constexpr int kEe0 = 0xee;        // sb ×8 DGROUP:0x717e + j (seg4:0x7636, seg4:0x702e)
constexpr int kHurt0 = 0xf7;      // sb ×(9j + k) DGROUP:0x7187 + 9j: hurt bytes (seg4:0x5e8e, seg4:0x7636)
constexpr int kFight0 = 0x13e;    // sb ×8 DGROUP:0x71ce + j: the fight bytes (seg4:0x7d36)
constexpr int kReaction = 0x146;  // sb DGROUP:0x71d6 the last reaction kind seg4:0x8346 stored
constexpr int kCountdown = 0x00;  // w  DGROUP:0x7090 the revenge countdown (seg4:0x707f-0x70e0)
constexpr int kShieldStep = 0x10; // w  DGROUP:0x70a0 the shield's animation step (0 down, 7 up)
constexpr int kWing0 = 0xac;      // b  DGROUP:0x713c wing 0 on (seg4:0x79e3)
constexpr int kClaimPool = 0x148; // sb DGROUP:0x71d8 the capture claim, 0xff none (seg13:0x3302)
constexpr int kClaimRec = 0x149;  // sb DGROUP:0x71d9
constexpr int kGuessX = 0x14a;    // w  DGROUP:0x71da the hunters' guess at this ship while cloaked (seg4:0x83ae)
constexpr int kGuessY = 0x14c;    // w  DGROUP:0x71dc
constexpr int kDue156 = 0x156;    // d  DGROUP:0x71e6 the watch refresh's due time (the player's: the 1500 ms tick)
}  // namespace pw

// --- The view -----------------------------------------------------------------------------------
class Pilot {
public:
    explicit Pilot(ShipRecord& s) : s_(s) {}
    ShipRecord& rec() { return s_; }

#define IS_PILOT_W(name, off)                                                    \
    std::int16_t name() const { return ship_w(s_, off); }                        \
    void name(std::int16_t v) { set_ship_w(s_, off, v); }
#define IS_PILOT_B(name, off)                                                    \
    std::uint8_t name() const { return ship_b(s_, off); }                        \
    void name(std::uint8_t v) { set_ship_b(s_, off, v); }
#define IS_PILOT_SB(name, off)                                                   \
    std::int8_t name() const { return ship_sb(s_, off); }                        \
    void name(std::int8_t v) { set_ship_sb(s_, off, v); }
#define IS_PILOT_D(name, off)                                                    \
    std::uint32_t name() const { return ship_d(s_, off); }                       \
    void name(std::uint32_t v) { set_ship_d(s_, off, v); }

    IS_PILOT_W(est_pool, pw::kEstPool)
    IS_PILOT_W(est_rec, pw::kEstRec)
    IS_PILOT_W(est_act, pw::kEstAct)
    IS_PILOT_W(est_app, pw::kEstApp)
    IS_PILOT_W(goal_pool, pw::kGoalPool)
    IS_PILOT_W(goal_rec, pw::kGoalRec)
    IS_PILOT_W(goal_act, pw::kGoalAct)
    IS_PILOT_W(goal_app, pw::kGoalApp)
    IS_PILOT_W(thinks, pw::kThinks)
    IS_PILOT_W(target_pool, pw::kTargetPool)
    IS_PILOT_W(target_rec, pw::kTargetRec)
    IS_PILOT_W(order, pw::kOrder)
    IS_PILOT_W(order_pool, pw::kOrderPool)
    IS_PILOT_W(order_rec, pw::kOrderRec)
    IS_PILOT_W(order_by, pw::kOrderBy)
    IS_PILOT_SB(allies, pw::kAllies)
    IS_PILOT_SB(cls, pw::kClass)
    IS_PILOT_B(weapon_class, pw::kWeaponClass)
    IS_PILOT_B(no_cap, pw::kNoCap)
    IS_PILOT_D(entry_time, pw::kEntryTime)
    IS_PILOT_D(replan_at, pw::kReplanAt)
    IS_PILOT_D(order_until, pw::kOrderUntil)
    IS_PILOT_D(d9c, pw::kD9c)
    IS_PILOT_D(goal_since, pw::kGoalSince)
    IS_PILOT_D(goal_since2, pw::kGoalSince2)
    IS_PILOT_B(weapon_wait, pw::kWeaponWait)
    IS_PILOT_B(slots, pw::kSlots)
    IS_PILOT_B(gun_wait, pw::kGunWait)
    IS_PILOT_SB(called_for, pw::kCalledFor)
    IS_PILOT_W(resources, pw::kResources)
    IS_PILOT_W(wait2, pw::kWait2)
    IS_PILOT_W(speed_cap, pw::kSpeedCap)
    IS_PILOT_D(credit, pw::kCredit)
    IS_PILOT_B(has_cloak, pw::kHasCloak)
    IS_PILOT_B(cloaked, pw::kCloaked)
    IS_PILOT_B(has_shield, pw::kHasShield)
    IS_PILOT_B(no_turn, pw::kNoTurn)
    IS_PILOT_B(alive, pw::kAlive)
    IS_PILOT_SB(one_shot, pw::kOneShot)
    IS_PILOT_SB(claim_pool, pw::kClaimPool)
    IS_PILOT_SB(claim_rec, pw::kClaimRec)
    IS_PILOT_W(guess_x, pw::kGuessX)
    IS_PILOT_W(guess_y, pw::kGuessY)
    IS_PILOT_D(due156, pw::kDue156)

#undef IS_PILOT_W
#undef IS_PILOT_B
#undef IS_PILOT_SB
#undef IS_PILOT_D

    // Indexed words and bytes, j a ship index 0 ... 7.
    std::int16_t hit(int k) const { return ship_w(s_, pw::kHit0 + 2 * k); }            // +0x44 + 2k
    void hit(int k, std::int16_t v) { set_ship_w(s_, pw::kHit0 + 2 * k, v); }
    std::int16_t attitude(int j) const { return ship_w(s_, pw::kAttitude0 + 2 * j); }  // +0x62 + 2j
    void attitude(int j, std::int16_t v) { set_ship_w(s_, pw::kAttitude0 + 2 * j, v); }
    std::int16_t revenge(int j) const { return ship_w(s_, pw::kRevenge0 + 2 * j); }    // +0x72 + 2j
    void revenge(int j, std::int16_t v) { set_ship_w(s_, pw::kRevenge0 + 2 * j, v); }
    std::int8_t grudge(int j) const { return ship_sb(s_, pw::kGrudge0 + j); }          // +0xc2 + j
    void grudge(int j, std::int8_t v) { set_ship_sb(s_, pw::kGrudge0 + j, v); }
    std::int8_t ee(int j) const { return ship_sb(s_, pw::kEe0 + j); }                  // +0xee + j
    void ee(int j, std::int8_t v) { set_ship_sb(s_, pw::kEe0 + j, v); }
    std::int8_t hurt(int j, int k) const { return ship_sb(s_, pw::kHurt0 + 9 * j + k); }  // +0xf7 + 9j + k
    std::int8_t fight(int j) const { return ship_sb(s_, pw::kFight0 + j); }            // +0x13e + j
    void fight(int j, std::int8_t v) { set_ship_sb(s_, pw::kFight0 + j, v); }

    // The four estimate words copied into the goal words (seg4:0x6bf-0x6eb, seg4:0xb2f-0xb4b).
    bool estimate_is_goal() const {
        return est_pool() == goal_pool() && est_rec() == goal_rec() && est_act() == goal_act() &&
               est_app() == goal_app();
    }
    void copy_estimate_to_goal() {
        goal_pool(est_pool());
        goal_rec(est_rec());
        goal_act(est_act());
        goal_app(est_app());
    }
    // The goal copied into the estimate (seg4:0x5443-0x5464, seg4:0xc5c).
    void copy_goal_to_estimate() {
        est_pool(goal_pool());
        est_rec(goal_rec());
        est_act(goal_act());
        est_app(goal_app());
    }

private:
    ShipRecord& s_;
};

inline Pilot pilot(ShipTable& ships, int i) { return Pilot(ships[static_cast<std::size_t>(i)]); }
inline Pilot pilot(ShipRecord& s) { return Pilot(s); }

// --- The pilot's DGROUP words ------------------------------------------------------------------
// [DESIGN] Until World (world.h) carries them, this struct is their home; the integrator keeps
// one instance per wave beside World and zeroes it at the wave start (all are BSS, 0 at load).
struct PilotGlobals {
    // seg4:0x7e2a's warn-then-call deadlines: the Pirate, the Knight and a third the routine
    // clears (DGROUP:0x1d12) before frame 500 [BYTES seg4:0x7e5a-0x7e70].
    std::uint32_t pirate_due = 0;   // DGROUP:0x1d0a
    std::uint32_t knight_due = 0;   // DGROUP:0x1d0e
    std::uint32_t due_1d12 = 0;     // DGROUP:0x1d12
    // DGROUP:0x8e2c: the frame# of the last call-in (seg13:0xb6e0) and of the wave start
    // (seg9:0x696b): seg4:0x7e2a waits 400 frames after it [BYTES seg4:0x7e48].
    std::uint32_t callin_frame = 0;
    // seg4:0x8626's side outputs, kept from the best candidate by seg4:0x59bc
    // [BYTES seg4:0x5c0e-0x5c78].
    std::int16_t attacked = 0;      // DGROUP:0x9686
    std::int16_t capture_gate = 0;  // DGROUP:0x9688
    std::int16_t w968a = 0;         // DGROUP:0x968a
    // The crime words: DGROUP:0x964a (session) is World::w964a; DGROUP:0x9648 the wave's,
    // DGROUP:0x964c the ships called in this wave (seg13:0xae68) [BYTES seg4:0x8297 seg13:0xae9c].
    std::int16_t crimes_wave = 0;   // DGROUP:0x9648
    std::int16_t called_in = 0;     // DGROUP:0x964c
    // DGROUP:0x967e: the greed level seg4:0x84de writes every 1500 ms (from seg4:0x59bc) and the
    // goal choice reads; 100 at seg9:0x3234 [BYTES seg4:0x84fc-0x8618 seg9:0x3234].
    std::int16_t greed = 100;
    // DGROUP:0x8d38: the revenge taunt seg4:0x702e formats (string 3033) and messages 0x4f /
    // 0x50 split into the two lines [BYTES seg4:0x72e9-0x72f0 seg3:0x1045-0x1052].
    std::string taunt;
    // DGROUP:0x8690 (the Strategy player's re-plan time) is World::w8690 and DGROUP:0x8680 + 2r
    // (the ambulance's served-ship slots) World::w8680.
};

}  // namespace is::world
