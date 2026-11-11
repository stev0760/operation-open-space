// The player's 1500 ms tick seg7:0xdaf-0x117d at the end of the wave frame (docs/re/world.md
// "The 1500 ms tick"; work/world/m4s8/tick-report.md, "Readings no verifier has checked").
//
// The frame's branch seg7:0xd7d-0xdac (update.cpp) draws the player's sprite while DGROUP:0x58a,
// DGROUP:0x588 or ship byte +0xeb is set and DGROUP:0x951a is 0; every other frame comes here.
// The tick is due once ship dword +0x156 (DGROUP:0x71e6 + 0x15a·p) is at or below now; with
// DGROUP:0x951a = 0 it re-arms +0x156 = now + 1500 and calls seg13:0x6716(player), which redraws
// the player through seg7:0x1208 unless the screen shakes. That redraw is what shows a cloaked
// ship at rest: the vanish's end leaves object byte +0x2e = 0xfe (gate_exit.cpp vanish_end), and
// seg8:0xb76's pinned-player path blits the cell and masks it with the DGROUP:0x5ea8 brush
// (sprite.cpp), the dithered ghost of core-0010. The rest of the body is the tick's bookkeeping
// for the ambulance, the pools and the other ships.
//
// The routines of the body that the port does not have (computer ships and their goals, M5;
// Demon's Gate and the duel, M6; the Strategy-mode command line, M8) are slots of
// player_tick_links(): an empty slot logs once through world::guard_todo, naming the address
// (ruling R3). The inline stores of the body are ported.
//
// Portable, no Win32. 16-bit arithmetic as the original's (ADR 0013 decision 7).
#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace is::world {

struct UpdateCtx;

// The DGROUP words of the tick body that no other part of the port holds.
struct PlayerTickState {
    // DGROUP:0x596, the pacing count of the seg13:0x2714 call at seg7:0x101e (0 in the image)
    // [BYTES seg7:0xfe9 seg7:0x1026 seg7:0x1042 DGROUP:0x596].
    std::int16_t dg_596 = 0;
    // dword DGROUP:0x8714 = frame# while the player's goal is pool 9 with word +0x38 = 1; its reader
    // is the computer ships' seg13:0x864e (M5) [BYTES seg7:0xf43-0xf4b seg13:0x864e seg9:0x5d2d].
    std::uint32_t dg_8714 = 0;
};
// The program's PlayerTickState, one for the run.
PlayerTickState& player_tick_state();

// The routines the tick calls that belong to later stages. Each empty slot logs once.
struct PlayerTickLinks {
    std::function<void()> seg18_5cec;       // while DGROUP:0x913e != 0 [BYTES seg7:0xdef-0xdf6]
    std::function<void()> seg12_515e;       // a kind-7 wave's respawn (M6) [BYTES seg7:0xe71-0xe78]
    // seg7:0xea7-0xefb, Demon's Gate with the player alone (M6): seg13:0xadbe(player, 0x13, 0xb4),
    // seg12:0x6d64(1, w) for each weapon w < 22 of the player's slots 1 ... +0xcb - 1, and ship 1's
    // words +0x0a and +0x24 = the player's +0x0a.
    std::function<void(int player)> demons_gate_mirror;
    std::function<std::int16_t()> seg4_59bc;  // the player's goal estimate (M5); AX unused [BYTES seg7:0xf0c]
    std::function<void(int ship)> seg13_7b40;  // (M5) [BYTES seg7:0xfae-0xfb4]
    // seg13:0x2714(a, b, c, d, e, f), cdecl, the attitude change (M5) [BYTES seg7:0x101e].
    std::function<void(int a, int b, int c, int d, int e, int f)> seg13_2714;
    std::function<void(int ship)> seg4_6700;  // (M5) [BYTES seg7:0x1070-0x1079]
    std::function<void()> seg8_9ab8;          // pool 2 near its gate (M5) [BYTES seg7:0x107a]
    std::function<void()> seg13_3302;         // the goal reports, message 0xb8 (M5) [BYTES seg7:0x107f]
    std::function<void()> seg4_7e2a;          // (M5) [BYTES seg7:0x116a]
    std::function<void()> seg4_83ae;          // the hunters' fix on a cloaked ship (M5) [BYTES seg7:0x116f]
    std::function<void()> seg8_9b4e;          // pool 2's goals (M5) [BYTES seg7:0x1174]
    // seg3:0x1d0a(a, ship, mode): the ship activity text, which it writes to DGROUP:0x8630 (the
    // Strategy-mode command line, M8); the slot returns the new text [BYTES seg13:0x2fb6].
    std::function<std::string(int a, int ship, int mode)> activity_text;
};
PlayerTickLinks& player_tick_links();

// seg7:0xdaf-0x117d, the frame's tick path: nothing until ship dword +0x156 <= now (unsigned);
// with DGROUP:0x951a != 0 only seg13:0x2f30(player), without the re-arm, so it runs on every
// frame of the autopilot; otherwise +0x156 = now + 1500, seg13:0x6716(player) and the body.
void player_tick(UpdateCtx& c);

// seg13:0x2f30(ship), Pascal (`retf 2`): nothing unless the ship is the player; a negative word
// +0x06 becomes 0 and ends the call; otherwise the activity text seg3:0x1d0a(100, ship, mode) with
// mode = 1 in Strategy mode (PlayMode != 0), 0 otherwise, rewrites DGROUP:0x8630, and a changed
// text sets DGROUP:0x58c = 1 [BYTES seg13:0x2f30-0x3005].
void ship_goal_text(UpdateCtx& c, int ship);

}  // namespace is::world
