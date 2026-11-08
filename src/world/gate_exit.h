// An object goes through a gate: seg12:0x2288(pool, rec) and the end of its vanish, the mode-1
// branch of seg7:0x158e (seg7:0x17dc-0x1a02) (docs/re/core.md "seg12:0x2288",
// work/analysis/m4/s1-end.md sections 1, 1b, 1c with s1-end-verify.md claims 1-18, the
// verifier's corrections 10, 12, 13, 15 and 18 followed). Portable, no Win32.
//
// Wiring (the update's owner, src/world/update.cpp and flight_hooks.cpp): UpdateHost::gate_passed
// (seg12:0x1776's call at seg12:0x1e17 / seg12:0x1e45) calls gate_exit(), and EntryHooks::vanished
// (entry_effect.cpp finish(), after its seg7:0x1819 PatBlt) calls vanish_end().
#pragma once

#include <cstdint>
#include <string>

#include "world/entry_effect.h"
#include "world/world.h"

namespace is::world {

// The routines seg12:0x2288 and the vanish's end call outside this file; the defaults do
// nothing (and design_type() reads 0).
class GateExitHost {
public:
    virtual ~GateExitHost() = default;
    // seg10:0x754a(id, pool, rec, arg).
    virtual void sound(int id, int pool, int rec, int arg) { (void)id, (void)pool, (void)rec, (void)arg; }
    // seg12:0x274a(pool, rec): the destroy handler (the death track's) [BYTES seg12:0x22ff].
    virtual void destroy(int pool, int rec) { (void)pool, (void)rec; }
    // The pool-5 (DGROUP:0x9328) slot words: word DGROUP:0x5a4c + 0x14·rec = word DGROUP:0x5a4e +
    // 0x14·rec - 7 [BYTES seg12:0x22cf seg12:0x22d3 seg12:0x22d6]; what the pair means is [OPEN].
    virtual void pool5_slot(int rec) { (void)rec; }
    // seg7:0x2080(pool, rec), an off-screen object's removal (a no-op for pools 0 and 14,
    // s1-end.md 1c.6) [OPEN: unread for the other pools].
    virtual void removed(int pool, int rec) { (void)pool, (void)rec; }
    // seg7:0x5ae8(rec, arg): the icon counters (core.md "Counters") [BYTES seg12:0x24e2].
    virtual void icon_counted(int rec, int arg) { (void)rec, (void)arg; }
    // The design word DGROUP:0x970a + 0xad·design: the team (7 for the law) [OPEN: the design
    // table is M5].
    virtual std::int16_t design_type(int design) { return (void)design, 0; }
    // seg7:0x65aa(pool, rec): the icon column's row, or the target display (core.md "Icon column").
    virtual void column_row(int pool, int rec) { (void)pool, (void)rec; }
    // seg3:0x6ec(ship, 7, 0, id): a status message.
    virtual void message(int ship, int id) { (void)ship, (void)id; }
    // seg12:0x3778, the link lines [BYTES seg7:0x19d6].
    virtual void lines() {}
    // seg2:0x170e, the menu items' state [BYTES seg7:0x19fa].
    virtual void menu_refresh() {}
    // Word +0x1e of the weapon table DGROUP:0x2f54 + 0x50·weapon (DGROUP:0x2f72) [BYTES
    // seg7:0x1988]; the program reads collide.h weapon_word.
    virtual std::int16_t weapon_word_1e(int weapon) { return (void)weapon, 0; }
    // LoadString for seg10:0xa332(DGROUP:0x8630, 1422, 1) (no directive in 1422).
    virtual std::string lang(int id) { return (void)id, std::string(); }
};

struct GateExitCtx {
    World& w;
    std::uint32_t now;            // DGROUP:0x1e50
    GateExitHost& host;
    EntryContext* entry = nullptr;  // seg12:0x2556 / seg7:0x13b2 / seg12:0x26a4; null: none [DESIGN]
};

// seg12:0x2288(pool, rec) [BYTES seg12:0x2288-0x2554]: the 11 steps of s1-end.md 1. The caller
// (seg12:0x1776) arms DGROUP:0x1e80 for the player afterwards, and PatBlts and plays 0x27 for
// anything else (update.cpp move_gate).
void gate_exit(GateExitCtx& c, int pool, int rec);

// The end of a mode-1 vanish after its PatBlt at seg7:0x1819 [BYTES seg7:0x181e-0x19ff]: the ship
// whose word +0x2e = 2 gets byte +0x2e = 0xfe (and the PlayMode message, correction 13); anything
// else `state` 1, seg7:0x65aa, the 800 award, the pool-6 weapon reset or +0x2e = 0xff with
// seg12:0x3778, then seg12:0x26a4, seg7:0x2080 and seg2:0x170e. The slot's common tail
// (seg7:0x1acb) stays entry_effect.cpp's end_slot. For the ambulance (pool 7, the vanish of
// seg16:0xa2's states 3/4 and of the park seg16:0x8a0) this is where its `state` becomes 1 and
// byte +0x2e 0xff, which stops the frame's calls of seg16:0xa2 and lets seg16:0xb4e re-place it
// (s3-ambflight.md 2.2, 2.6) [OPEN: the verifier left what seg7:0x13b2(1, 7, 0) does to A.state
// unchecked; this is the port's reading of seg7:0x18b7 and seg7:0x19d1] (M4 s3 AMBFLIGHT).
void vanish_end(GateExitCtx& c, int pool, int rec);

// The award's search of the 8-entry hit history [BYTES seg7:0x18f1-0x1945]: the first k whose
// time window fails ends the search; the first k whose pool and record match wins. Returns the
// entry index, or -1. The window is 0 <= DGROUP:0x1e68 / 10 - sx(word DGROUP:0x937a + 2k) < 20,
// 32-bit (high word 0, low word below 20 unsigned) [BYTES seg7:0x190a seg7:0x191d seg7:0x1923
// seg7:0x1928] (correction 15: at most one award). With d frames between the push and the
// vanish's end, an entry counts always for d <= 190 and never for d >= 200 (191 ... 199: when
// the push frame mod 10 <= 199 - d); pool and record compare after `cbw` of the stored bytes, so
// a record >= 128 never matches, and from frame 327,680 on (F / 10 >= 0x8000) no entry does
// (s5-weapons.md 9.2, s5-weapons-verify.md claim 24 as corrected; tests/test_world_award_window.cpp).
int award_entry(const World& w, int pool, int rec);

// seg8:0x81f8(pool, rec): the history shifted down, entry 0 = (DGROUP:0x1e68 / 10, pool, rec)
// [BYTES seg8:0x820f seg8:0x8240 seg8:0x8246 seg8:0x824c] (for the hit dispatcher's port).
void push_hit(World& w, int pool, int rec);

// The vanish's length in frames for a cell side and Size: n = entry_blocks(size, Size), and the
// step blacks 10 blocks a frame and ends on the pick that reaches n², so ceil(n² / 10) frames
// [BYTES seg7:0x1427 seg7:0x1435 seg7:0x17d0 seg7:0x17d8].
int vanish_frames(std::int16_t size, std::int16_t size_level);

}  // namespace is::world
