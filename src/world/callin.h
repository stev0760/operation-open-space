// Mid-wave arrivals: the call-in seg13:0xadbe(ship, kind, msg) and its body seg13:0xae68 (docs/re/
// ships.md "Mid-wave arrivals"; work/analysis/m5/s3-roster.md 2.3 with s3-roster-verify.md, whose
// correction 52 and Omissions O30-O34 win; the listing read again for the port, M5 stage 3 part B,
// track ENTRY). A call-in enters an Enforcer that waits off-world, enlists a ship already in play,
// enters an off-world teammate (kind 4) or has seg14:0x0(kind) build a new ship and enters it at a
// gate. Portable, no Win32: the calls out go through CallInHost.
//
// Ruling R1: every rand() is drawn where the original draws it (the gate's rand() % DGROUP:0x9318
// after seg14:0x0, Demon's Gate two more), the 16-bit widths and the original's tests as written.
// Ruling R3: the callers in the law (seg13:0xb6f2, seg13:0xa6bc, seg13:0xb93a) are stage 4; the
// Pirate, Knight and Patrol call-ins of seg4:0x7e2a (track THINK) and every other caller reach the
// same two entry points below.
#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "world/records.h"
#include "world/sprite.h"

namespace is::shell {
class Rng;
}  // namespace is::shell

namespace is::world {

class World;

// The kinds of seg13:0xae68 [BYTES seg13:0xaec7-0xb075] (ships.md "Mid-wave arrivals" step 2).
constexpr int kCallInPirates = 3;        // team 3, activity 4 (seg4:0x8008, seg13:0x32e4; msg 0x69)
constexpr int kCallInHelp = 4;           // team 4 (or 0, 3), activity 1 (seg4:0x824a 0x6a, seg13:0x853b 0x68)
constexpr int kCallInDemon = 0x13;       // the player's team, activity 4 (seg7:0xeb0, seg11:0x9f03; 0xb4)
constexpr int kCallInTerminator = 0x14;  // team 7 class 0, activity 0x17 (seg13:0xacac; 0x58)
constexpr int kCallInInterceptor = 0x15; // team 7 class 1, activity 0x16 (seg4:0x640d 0x3c; law -1)
constexpr int kCallInPatrol = 0x16;      // team 7 class 2, activity 0x14 (seg4:0x8335, seg11:0x7330; 0x6d)

// The request seg13:0xae68 builds before its scans: [bp-0xa] the class (+0x5e, -1 any), [bp-0xe]
// the team, [bp-0xc] the activity (+0x52) [BYTES seg13:0xaecd-0xb070].
struct CallInRequest {
    bool refused = false;
    std::int16_t cls = -1;
    std::int16_t team = 0;
    std::int16_t activity = 0;
};

// What the call-in reaches outside its own words. The defaults do nothing (the sounds and the
// roster build log once through todo()).
class CallInHost {
public:
    virtual ~CallInHost() = default;
    virtual shell::Rng& rng() = 0;                                  // seg5:0xdc
    virtual std::uint32_t now() { return 0; }                       // DGROUP:0x1e50, the pass's time
    virtual std::uint32_t frames() { return 0; }                    // DGROUP:0x1e68, the wave frames
    // The design team word DGROUP:0x970a + 0xad·n (design +0x38); the program binds roster_build.h
    // design_team (track ROSTER).
    virtual std::int16_t team(int design) = 0;
    virtual void play(int id) { (void)id; }                         // seg10:0x744c(id)
    virtual void sound(int id, int pool, int rec, int mode) { (void)id, (void)pool, (void)rec, (void)mode; }  // seg10:0x754a
    virtual void music(int track, int flag) { (void)track, (void)flag; }  // seg10:0x7480(track, flag)
    // seg3:0x6ec(recipient, pool, rec, code), the message dispatcher (hud/messages.h).
    virtual void message(int recipient, int pool, int rec, int code) { (void)recipient, (void)pool, (void)rec, (void)code; }
    virtual void purchases(int ship) { (void)ship; }               // seg16:0x3784(k, 1)
    // seg12:0x2556(14, k) and seg7:0x13b2(2, 14, k): the entry buffer and the materialise effect
    // (entry_effect.h; the frame's UpdateHost::ship_entered draws the same two).
    virtual void entered(int ship) { (void)ship; }
    virtual void designs_begin() {}                                // seg14:0x3eda (MASTER.SHP / .BOD in)
    virtual void designs_end() {}                                  // seg14:0x4074
    // seg14:0x0(mode), the roster builder (roster_build.h build_wave_roster) with mode = the kind,
    // or the team for kind 4 [BYTES seg13:0xb44e-0xb45c]. It sets DGROUP:0x9316 (World::last_ship).
    virtual void build_roster(int mode) { (void)mode; }
    virtual void seg11_238e() {}                                   // seg11:0x238e(DGROUP:0xa7c, 0)
    // seg4:0x8346(ship, pool, rec, code) [OPEN: unread; the call-in passes code 13].
    virtual void seg4_8346(int ship, int pool, int rec, int code) { (void)ship, (void)pool, (void)rec, (void)code; }
    // seg8:0x2bbe(&out, sx, sy) for Demon's Gate: a screen point to the world [OPEN M6]. The
    // default returns the point unchanged and logs.
    virtual Point16 screen_to_world(std::int16_t sx, std::int16_t sy);
    // DGROUP:0x964c, the ships call-ins built this wave (+1 at seg13:0xb644); DGROUP:0x8e2c, the
    // frame# of the last call-in (seg13:0xb6e0), read by seg4:0x7e2a. The defaults keep their own
    // copies (callin_words()) [DESIGN: the integrator joins them with the pilot's PilotGlobals].
    virtual std::int16_t& called_in();
    virtual void set_callin_frame(std::uint32_t frame);
    virtual void todo(const std::string& what, const char* address) { (void)what, (void)address; }
};

// The program's copies of DGROUP:0x964c and DGROUP:0x8e2c (CallInHost's defaults) [DESIGN].
struct CallInWords {
    std::int16_t called_in = 0;     // DGROUP:0x964c
    std::uint32_t callin_frame = 0; // DGROUP:0x8e2c
};
CallInWords& callin_words();

// The refusals and the request of seg13:0xae68 [BYTES seg13:0xae7c-0xb070] (s3-roster-verify.md
// claims 48, 49, 67 and O30): in a race or duel (DGROUP:0x9320 = 6 or 7) only kind 0x15 with byte
// DGROUP:0xa163 = 8; a kind other than 0x15 when DGROUP:0x964c > 2·Detail + 1; any kind when
// DGROUP:0x9316 > Detail + 3; in Demon's Gate any kind but 0x13; kinds 3 and 4 when the requester's
// byte +0xcd >= 0 or a live ship j < DGROUP:0x9316 (strictly below) of the asked team has +0xcd =
// requester; kind 4 also for a requester of team 6, when word DGROUP:0x3824 + 2·(8·T + P) < 0 and
// when the requester's +0x38 = 1 with T = 4 and byte +0x146 in {3, 0xd, 8, 0xa}. An unknown kind
// keeps the request's [bp-0xa] / [bp-0xe] / [bp-0xc] as the stack left them: the port takes 0
// [DESIGN ADR 0016] (no caller passes one).
CallInRequest callin_request(const World& w, CallInHost& h, int ship, int kind,
                             const std::int16_t* team_table, std::int16_t dg_a163);

// seg13:0xae68(ship, kind, msg) [BYTES seg13:0xae68-0xb6ef]: 0 nothing happened, 1 a ship in the
// wave was entered or enlisted, 2 a new ship was built and entered. `team_table` is DGROUP:0x3824
// (the 8 x 8 words, roster_build.h RosterWords::team_table; null reads 0) and `dg_a163` the byte
// DGROUP:0xa163 (the last crime, law's) [DESIGN: both are other tracks' words].
std::int16_t call_in_request(World& w, CallInHost& h, int ship, int kind, int msg,
                             const std::int16_t* team_table = nullptr, std::int16_t dg_a163 = 0);

// seg13:0xadbe(ship, kind, msg) [BYTES seg13:0xadbe-0xae67]: seg13:0xae68, then for a non-zero
// result the event music seg10:0x7480(t, 0) and DGROUP:0x2384 = now + 12000, with t = 2 for kind
// 0x16 and every kind above it or not listed, 7 for kinds 3, 0x13 and 0x14, 2 for 0x15, and for
// kind 4: 8 when the requester is the player, 7 when the player's +0x32 = 14, +0x34 = requester and
// +0x38 = 1, else no music. Returns the AX word the original leaves, which every caller tests with
// `or ax, ax` (pitfalls.md): 0 for seg13:0xae68's 0, lo16(now + 12000) after the music, and with no
// music the player's index (the player's +0x32 != 14) or the requester's. `music_due` is
// DGROUP:0x2384 (FrameGlobals::music_due); null leaves it.
std::int16_t call_in(World& w, CallInHost& h, int ship, int kind, int msg, std::uint32_t* music_due,
                     const std::int16_t* team_table = nullptr, std::int16_t dg_a163 = 0);

// ---- the program's call-in (the slots src/hud fills; an empty slot logs once)

struct CallInLinks {
    std::function<void(int id)> play;                                         // seg10:0x744c
    std::function<void(int id, int pool, int rec, int mode)> sound;           // seg10:0x754a
    std::function<void(int track, int flag)> music;                           // seg10:0x7480
    std::function<void(int recipient, int pool, int rec, int code)> message;  // seg3:0x6ec
    std::function<void(int mode)> build_roster;                               // seg14:0x0(mode) with the bracket
    std::function<std::int16_t(int design)> team;                             // DGROUP:0x970a + 0xad n
    std::function<void()> seg11_238e;
    std::function<void(int ship, int pool, int rec, int code)> seg4_8346;
    std::function<Point16(std::int16_t sx, std::int16_t sy)> screen_to_world; // seg8:0x2bbe
    std::function<std::int16_t*()> team_table;                                // DGROUP:0x3824
    std::function<std::int16_t()> dg_a163;                                    // DGROUP:0xa163
    std::function<std::uint32_t*()> music_due;                                // DGROUP:0x2384
    std::function<std::uint32_t()> now;                                       // DGROUP:0x1e50
    std::function<std::uint32_t()> frames;                                    // DGROUP:0x1e68
    // DGROUP:0x964c and DGROUP:0x8e2c, each one word of the program: the integrator points them at
    // the pilot's PilotGlobals::called_in / ::callin_frame (pilot.h), which seg4:0x7e2a reads;
    // unset (the tests), callin_words() holds them [DESIGN].
    std::function<std::int16_t*()> called_in;
    std::function<std::uint32_t*()> callin_frame;
    std::function<void(const std::string& what, const char* address)> todo;
};
CallInLinks& callin_links();
// The CallInHost over callin_links(), shell::game_rng(), roster.h purchase_pass(w, k, 1) for
// seg16:0x3784(k, 1) and sprite_links().ship_entered for seg12:0x2556 + seg7:0x13b2.
CallInHost& linked_callin_host(World& w);
// seg13:0xadbe on the program's World with linked_callin_host and callin_links()'s words: the one
// entry point for THINK's seg4:0x7e2a and the law's callers.
std::int16_t call_in_program(World& w, int ship, int kind, int msg);

}  // namespace is::world
