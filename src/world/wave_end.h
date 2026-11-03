// The wave's end (docs/re/core.md "The wave end", docs/re/shell.md "Wave deadlines and the demo",
// work/analysis/m4/s1-end.md sections 2 and 3 with s1-end-verify.md claims 19-39):
//   - the tick's deadline block seg7:0x5ce-0x707 (DGROUP:0x1e88, DGROUP:0x1e80, DGROUP:0x1e84),
//   - seg9:0x6bc2 and seg9:0x6cc4(arg), the way into state 2,
//   - seg9:0x2828's part for a wave that has been played (DGROUP:0x91f8 > 0).
// Portable, no Win32. Everything outside the World goes through WaveEndHost.
//
// ADR 0001: the original tests the deadlines on every tick pass; the port's state-1 frame hook
// runs once per frame, so wave_deadlines() runs at the top of that hook, before the wave pass,
// and a deadline that fires ends the frame there: the test is late by at most one frame
// [DESIGN ADR 0001]. Wiring (flight_hooks.cpp, the update's owner): in wave_hook, before
// sync_in, `if (wave_deadlines(ctx) == DeadlineResult::kEndPass) return;`.
#pragma once

#include <cstdint>

#include "shell/game_state.h"
#include "world/ambulance.h"
#include "world/setup.h"
#include "world/world.h"

namespace is::world {

// The routines of other parts the wave end calls; the defaults do nothing.
class WaveEndHost {
public:
    virtual ~WaveEndHost() = default;
    // seg10:0x7480(code, arg): the music wrapper; code -1 is the hard pause (sound.md).
    virtual void music(int code, int arg) { (void)code, (void)arg; }
    // seg10:0x7db8: ISPACE.DLL ordinal 7 command 2 (audio::reset_slots).
    virtual void reset_sounds() {}
    // seg13:0x65f2(1): seg13:0x5850 (window DGROUP:0x95fc), seg7:0x63f2 (the icon column
    // DGROUP:0x9458) and seg13:0x6362 (correction 25) [BYTES seg13:0x669b].
    virtual void close_watch_windows() {}
    // DGROUP:0x92e4 = 0 in the program's other copies of it (the game-start flow's session).
    virtual void clear_restored() {}
    // seg9:0x1092: DeleteObject of the 32 words DGROUP:0x4e94 [BYTES seg9:0x1092].
    virtual void delete_wave_objects() {}
    // seg12:0x26a4(pool, rec), a record's private buffer.
    virtual void free_buffer(int pool, int rec) { (void)pool, (void)rec; }
    // seg11:0x2f0, the HUD windows; then seg7:0x49b6, the 8 DCs DGROUP:0x3a04 + 2i.
    virtual void close_hud() {}
    virtual void release_wave_dcs() {}
    // PatBlt(DGROUP:0x13dc, 0, 0, W, H, BLACKNESS) [BYTES seg9:0x6d7a].
    virtual void black_client() {}
    // seg6:0x7266: the blocks DGROUP:0x94da / DGROUP:0x94dc freed [OPEN: their content].
    virtual void free_blocks() {}
    // seg11:0xb09c(0, i): ship i's flame off.
    virtual void flame_off(int ship) { (void)ship; }
    // seg10:0x9672 then seg10:0x552(4, 1) when the meal changed, and seg10:0x565a (the meal
    // slot, shell.md "Meal slot") [BYTES seg9:0x2af0-0x2b0f].
    virtual void meal_check() {}
    // seg6:0x885c(0, 0): LASTGAME's save, stage 3 [DESIGN ADR 0014: the save writer is not
    // ported in stage 1].
    virtual void save_lastgame() {}
    // The state-2 screen's start, after seg9:0x6cc4 (the debriefing's page and box).
    virtual void debriefing_started() {}
    // seg16:0x68d6(main, 2328, 2329): the modal "I'm afraid you're history" MessageBox,
    // caption "Inner Space", flags 0x30 (MB_ICONEXCLAMATION), the title "PAUSED.." while it is
    // up [BYTES seg7:0x6c8 seg7:0x6d2 seg16:0x68ed seg16:0x6904 seg16:0x691b] [ORACLE core-0001].
    virtual void history_box() {}
    // State 8, the Hall of Justice: seg9:0x6c62's writes and seg16:0x6928(0) (M6) [OPEN].
    virtual void hall_of_justice() {}
    // The race wave's credit loop of seg9:0x6bc2 (kind 6) [OPEN M6] [BYTES seg9:0x6be3-0x6c59].
    virtual void race_credit() {}
    // seg9:0x6db0 (kind 6): every ship's fuel to its tank [OPEN M6] [BYTES seg9:0x6dc5].
    virtual void race_refuel() {}
    // The ambulance: DGROUP:0x1e88 due with the player served and DGROUP:0x951a = 0: the
    // seg7:0x5fa path, DGROUP:0x90f4 = 6, DGROUP:0x914a = 0, push 1, jmp seg7:0x18f =
    // seg16:0xcfe(1) once in this tick (the VISIT track's state-6 entry) [BYTES seg7:0x5fa
    // seg7:0x608] (s3-ambflight-verify.md claim 52a).
    virtual void ambulance_enter() {}
    // The demo's end of seg7:0x62b-0x69a: seg6:0x92b8, state 8, seg9:0xd6c [OPEN: the demo].
    virtual void demo_seg6_92b8() {}
    // The roster rating loop seg9:0x2e97-0x2f74 (word +0x06 by the class/DGROUP:0x9334 rule,
    // seg14:0x3eda / seg14:0x4074) [OPEN] [BYTES seg9:0x2e97 seg9:0x2f2a].
    virtual void roster_ratings() {}
};

struct WaveEndCtx {
    World& w;
    shell::GameState& s;
    WaveEndHost& host;
    // seg9:0x2828's head (seg13:0x2688, DGROUP:0x9346, DGROUP:0x9332, DGROUP:0x932c and its
    // rand()) runs through the set-up's wave0_followups with this host; null skips it [DESIGN].
    SetupHost* setup = nullptr;
    // The ambulance's words and calls for seg16:0x32a0; null: the program's ambulance_state()
    // and linked_ambulance_host() (ambulance.h) [DESIGN].
    AmbulanceState* amb = nullptr;
    AmbulanceHost* amb_host = nullptr;
};

enum class DeadlineResult { kContinue, kEndPass };

// seg7:0x5ce-0x707 [BYTES seg7:0x5ce-0x707]: with now = s.now() (DGROUP:0x1e50),
//   - DGROUP:0x1e88 (armed, not above now): cleared; with the player served and DGROUP:0x951a
//     = 0 the visit (WaveEndHost::ambulance_enter) and the pass ends, else seg16:0x32a0
//     (ambulance.h ambulance_drop) and the block goes on (s3-ambflight-verify.md claim 52);
//   - DGROUP:0x1e80 (armed, not above now, unsigned `ja` at seg7:0x627): in a player game
//     PlayMode = 1 when > 1, DGROUP:0x1e84 = now when the player's `alive` (+0xec) = 0, else
//     seg9:0x6bc2; then DGROUP:0x1e80 = 0 and the pass ends. The demo path (DGROUP:0x95bc = 0)
//     runs seg9:0x2828, seg6:0x885c(0, 0), seg9:0xd6c and requests state 0 [OPEN: the demo is
//     not ported; its calls are the host's];
//   - else DGROUP:0x1e84 (armed, not above now): in the edition DGROUP:0x920c = 0 the history
//     MessageBox, then DGROUP:0x1e84 = 0 and seg6:0x5d58 (state 5) [BYTES seg7:0x1d8 seg7:0x1e1],
//     and the pass ends.
DeadlineResult wave_deadlines(WaveEndCtx& c);

// seg9:0x6bc2 [BYTES seg9:0x6bc2-0x6cc0]: the music's hard pause, DGROUP:0x1e74 = 0, the race
// credit (kind 6), then state 8 (DGROUP:0x93a4 != 0) or seg9:0x6cc4(0).
void wave_over(WaveEndCtx& c);

// seg9:0x6cc4(arg) [BYTES seg9:0x6cc4-0x6dab], the 17 steps of s1-end.md 3: the sounds reset,
// seg13:0x65f2(1), state 2 (GameState::enter_debriefing(false)), DGROUP:0x92e4 = 0, the race
// refuel, the wave's GDI objects and buffers, the HUD, the black client, DGROUP:0x93b2,
// seg9:0x2828, seg6:0x7266, DGROUP:0x94c4 = 1, DGROUP:0x9330 = 0. With arg != 0 the box is drawn
// at once (seg9:0x6dec(1), the Hall of Justice's way); with 0 the first state-2 frame draws it.
void debrief_entry(WaveEndCtx& c, int arg);

// seg9:0x2828 for DGROUP:0x91f8 > 0, after its head [BYTES seg9:0x2af0-0x31f0]: the meal
// re-check, the flames off and the roster writes per ship, DGROUP:0x931c, the remaining icons
// into the directory record, the rock / hazard / gate counts and LASTGAME.
void wave_into_directory(WaveEndCtx& c);

// The remaining icons' packing alone (seg9:0x2f82-0x307d): the pool-0 records 0 ... 7 in
// `state` 0, in order, into slots s = 0, 1, ...: word +0x1d2 + 2s = `damage`, byte +0x1c8 + s =
// 2 for heat +0x2f < 0, 1 for byte +0x53 = 2 (1 wins), left as it was otherwise; the slots after
// the last zeroed (word and byte).
void pack_remaining_icons(World& w);

}  // namespace is::world
