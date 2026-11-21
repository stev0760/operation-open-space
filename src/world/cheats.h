// The cheat keys (docs/re/law.md "Cheat keys"; work/analysis/m5/s4-crime.md section 1 with
// s4-crime-verify.md claims 1-16 and 69; M5 stage 4 part B, track KEYS): the gate seg11:0x6ee7
// (DELETE) and the per-key table seg11:0x72e0(vk) with its switch at seg11:0x749e.
//
// The gate of the key-up switch (seg11:0x6d58, input.cpp key_message) lets '1' ... '9' and F3 ...
// F9 reach cheat_key only while DGROUP:0x951c is not 0. Every key makes exactly the stores the
// bytes make; the routines of other parts (the call-in, the crime routine, the weapon loader, the
// race finish) go through CheatHost, whose defaults do nothing.
//
// Ruling R3: '1' calls the race finish seg4:0x2da2 as a logged host call (races are out of the
// stage); '5' sets what the bytes set and no more (Demon's Gate is out of the stage).
// Portable, no Win32.
#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace is::shell {
class Rng;
}  // namespace is::shell

namespace is::world {

class World;

// The cheat words of the program: one copy, cheat_state(). A restore and a new game clear w951c and
// w104e [BYTES seg6:0x98bb seg6:0x98c1 seg10:0x8505 seg10:0x850b].
struct CheatState {
    std::int16_t w951c = 0;  // DGROUP:0x951c, the cheat word: DELETE toggles it (0 in the image)
    std::int16_t w104e = 0;  // DGROUP:0x104e, '7': the placement seg14:0x1758 gives weapon 13
    // DGROUP:0x104a, F6's force flag: seg13:0xb6f2 takes its forced path while it is set and clears
    // it there (seg13:0xb801); a path that returns first (a duel, crime 5 repeated) leaves it set
    // [BYTES seg11:0x733b seg13:0xb7b3 seg13:0xb801].
    std::int16_t w104a = 0;
};

// The routines the keys call in other parts. The defaults do nothing.
class CheatHost {
public:
    virtual ~CheatHost() = default;
    virtual shell::Rng& rng() = 0;                                            // seg5:0xdc
    // seg13:0xadbe(ship, kind, msg), the call-in (callin.h call_in); the result is ignored.
    virtual std::int16_t call_in(int ship, int kind, int msg) { (void)ship, (void)kind, (void)msg; return 0; }
    virtual void play(int id) { (void)id; }                                   // seg10:0x744c(id)
    virtual void sound(int id, int pool, int rec, int mode) { (void)id, (void)pool, (void)rec, (void)mode; }  // seg10:0x754a
    // seg3:0x6ec(recipient, pool, rec, code), the message dispatcher.
    virtual void message(int recipient, int pool, int rec, int code) { (void)recipient, (void)pool, (void)rec, (void)code; }
    // seg4:0x2da2(), the race finish (the race bonus to the score and the rating, the place
    // sounds, the finish clock): races are out of stage 4 (R3), the default logs once.
    virtual void race_finish() {}
    // seg13:0xb6f2(ship, code), the crime routine (law.h); DGROUP:0x104a is set before the call.
    virtual void crime(int ship, int code) { (void)ship, (void)code; }
    // seg12:0x6d64(ship, w), a weapon into a new slot (weapon_load.h load_weapon_program).
    virtual void load_weapon(int ship, int w) { (void)ship, (void)w; }
    // The dword DGROUP:0x5018 = 0xffffffff: the score panel draws its digits again (the HUD's
    // PanelState::score_shown).
    virtual void score_redraw() {}
    virtual void todo(const std::string& what, const char* address) { (void)what, (void)address; }
};

CheatState& cheat_state();

// What the keys read and write: the World, the cheat words, and the two DGROUP words of other
// parts whose one copy the keys store to: DGROUP:0x104c (god mode, CollideGlobals::w104c, read by
// damage.cpp) and DGROUP:0x1048 (FrameGlobals::dg_1048, read by the pass's 10 s block).
struct CheatCtx {
    World& w;
    CheatState& st;
    std::int16_t& god;      // DGROUP:0x104c
    std::int16_t& dg_1048;  // DGROUP:0x1048
    CheatHost& h;
};

// The virtual keys of the key-up switch seg11:0x6d19 that reach seg11:0x6d58: '1' ... '9' and
// F3 ... F9 [BYTES seg11:0x6d3a-0x6d56].
constexpr bool is_cheat_vk(std::uint16_t vk) { return (vk >= 0x31 && vk <= 0x39) || (vk >= 0x72 && vk <= 0x78); }
constexpr std::uint16_t kVkDelete = 0x2e;
constexpr std::uint16_t kVkF3 = 0x72, kVkF4 = 0x73, kVkF5 = 0x74, kVkF6 = 0x75, kVkF7 = 0x76, kVkF8 = 0x77, kVkF9 = 0x78;

// The sound ids the keys play [BYTES seg11:0x6f0c seg11:0x7309 seg11:0x7388 seg11:0x745f seg11:0x7488].
constexpr int kSoundCheatOn = 0x49;       // DELETE on
constexpr int kSoundPowerup = 0x20;       // '9', F3
constexpr int kSoundRefuel = 0x71;        // '6'
constexpr int kSoundResources = 0x23;     // '8'
constexpr int kSoundWeapons = 0x25;       // F7
// The call-in messages of '3', '4' and F5 [BYTES seg11:0x73e7 seg11:0x73f5 seg11:0x7328].
constexpr int kMsgKnight = 0x6a, kMsgPirate = 0x69, kMsgPatrolman = 0x6d;

// seg11:0x6ee7, the gate (WM_KEYDOWN 0x2e, 0x99b, 0x99e) [BYTES seg11:0x6ee7-0x6f1d]: nothing
// unless the edition DGROUP:0x920c is 2 or -1 (World::w920c); then DGROUP:0x951c = (it == 0)
// (`cmp 1; sbb; neg`: 0 gives 1, anything else 0). Off: DGROUP:0x104c = 0. On: sound 0x49. Both
// end with DGROUP:0x5018 = -1. Returns whether the edition let the key through.
bool cheat_gate(CheatCtx& c);

// seg11:0x72e0(vk), cdecl, switch at seg11:0x749e [BYTES seg11:0x72e0-0x7502]. The caller's gate
// (seg11:0x6d58) has tested DGROUP:0x951c; a VK outside '1' ... '9', F3 ... F9 does nothing.
void cheat_key(CheatCtx& c, std::uint16_t vk);

// ---- the program's keys (the slots the integrator fills; an empty slot logs once)
struct CheatLinks {
    std::function<void(int id)> play;                                         // seg10:0x744c
    std::function<void(int id, int pool, int rec, int mode)> sound;           // seg10:0x754a
    std::function<void(int recipient, int pool, int rec, int code)> message;  // seg3:0x6ec
    std::function<void()> race_finish;                                        // seg4:0x2da2
    std::function<void(int ship, int code)> crime;                            // seg13:0xb6f2
    std::function<void()> score_redraw;                                       // DGROUP:0x5018
};
CheatLinks& cheat_links();
// The CheatHost over cheat_links(), shell::game_rng(), callin.h call_in_program and weapon_load.h
// load_weapon_program on `w`; a slot not filled falls back to callin_links()'s play, sound and
// message, else logs once.
CheatHost& linked_cheat_host(World& w);
// seg11:0x6ee7 and seg11:0x72e0 on the program's World and cheat_state() with linked_cheat_host;
// `god` and `dg_1048` are the one copies the program holds (collide_globals().w104c,
// FrameGlobals::dg_1048).
bool cheat_gate_program(World& w, std::int16_t& god, std::int16_t& dg_1048);
void cheat_key_program(World& w, std::int16_t& god, std::int16_t& dg_1048, std::uint16_t vk);

}  // namespace is::world
