// Perspective shifting (docs/re/ships.md "Perspective shifting"; work/analysis/m5/s3-roster.md 4
// with s3-roster-verify.md, whose correction 61 wins: the roster indices are not exchanged; the
// listing read again for the port, M5 stage 3 part B, track ENTRY): WM_KEYUP 'P' latches
// DGROUP:0x1032 (seg11:0x6dc4), the wave pass's seg7:0x7d5 calls seg11:0xaece, which makes the
// next live, in-play ship of the player's design team the player's ship DGROUP:0x9646 and starts
// the pan of the view (seg8:0x65-0x248, view.h ViewInputs::pan). Portable, no Win32: the HUD and
// drawing calls go through ShiftHost.
#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace is::world {

class World;
struct ViewState;
struct WaveGlobals;

// The DGROUP words of the shift that World does not hold [DESIGN: world.h is the set-up's file].
struct ShiftWords {
    // DGROUP:0x1032, the 'P' latch: 1 at seg11:0x6dcd, 0 at seg11:0xaef1, tested at seg7:0x7d5;
    // no other reference [BYTES seg11:0x6dcd seg11:0xaef1 seg7:0x7d5].
    std::int16_t latch = 0;
    // DGROUP:0x9378, the PlayMode the pan restores on arrival (seg8:0x148): written by the
    // placement's tail (seg9:0x698a, = PlayMode) and the F12 toggle (seg11:0x6db7); no other
    // writer [BYTES seg9:0x6987-0x698a seg11:0x6db4-0x6db7 seg8:0x148].
    std::int16_t saved_mode = 0;
};
ShiftWords& shift_words();

// The calls of seg11:0xaece and of the pan out of src/world; the defaults do nothing.
class ShiftHost {
public:
    virtual ~ShiftHost() = default;
    // seg3:0x6ec(recipient, pool, rec, code), the message dispatcher (hud/messages.h).
    virtual void message(int recipient, int pool, int rec, int code) { (void)recipient, (void)pool, (void)rec, (void)code; }
    // seg13:0x2206(): the player's next target, pool in the high byte, record in the low byte
    // (hud/target.h).
    virtual int next_target() { return -1; }
    // seg3:0xf4(ship, pool, rec): (pool, rec) becomes the ship's target (hud).
    virtual void set_target(int ship, int pool, int rec) { (void)ship, (void)pool, (void)rec; }
    // seg11:0x3cc2(ship, 1): the damage panel's lamps of the new ship (hud/hud.h lamp_repaint).
    virtual void lamps(int ship, int arg) { (void)ship, (void)arg; }
    // seg11:0xae94: the weapon slot (hud/weapons.h wave_start_slot; returns at once after a
    // restore) and seg11:0x2f8c, the weapon picture of the new player's weapon.
    virtual void weapon_slot() {}
    virtual void weapon_picture() {}
    // ---- the pan's (seg8:0xcf-0x16b)
    // PatBlt(DGROUP:0x13dc, x, y, w, h, BLACKNESS) over the player's last cell.
    virtual void black_cell(int x, int y, int w, int h) { (void)x, (void)y, (void)w, (void)h; }
    virtual void pin_player() {}   // seg7:0x11cc: the player's object +0x00/+0x02 = the play centre
    virtual void draw_player() {}  // seg7:0x1208
    // DGROUP:0xa76 = mode and PostMessage(DGROUP:0xa7c, WM_PAINT, 0, 0): the left display back to
    // the target (the world's word and the frame's copy, hud/program.h info_mode_119c).
    virtual void info_mode(int mode) { (void)mode; }
    virtual void todo(const std::string& what, const char* address) { (void)what, (void)address; }
};

// seg11:0x6d19 + seg11:0x6dc4, WM_KEYUP 'P' in the key decode: only in a player game
// (DGROUP:0x95bc != 0) and not in edition DGROUP:0x920c = 3, DGROUP:0x1032 = 1 [BYTES
// seg11:0x6d19-0x6d1e seg11:0x6d44-0x6d46 seg11:0x6dc4-0x6dd3]. The key gate lets WM_KEYUP 'P'
// through the lock-out and with the player out of play (input.h key_message).
void shift_key_up(World& w);

// seg11:0xaece() [BYTES seg11:0xaece-0xb09a], 0 or 1:
//  - a race or duel (DGROUP:0x9320 = 6 or 7) returns 0 before the latch is cleared, so the call
//    repeats every pass [BYTES seg11:0xaedc-0xaeec];
//  - DGROUP:0x1032 = 0; 0 when the player's object `state` != 0 or in Demon's Gate;
//  - the search steps DGROUP:0x9646 to the next ship (0 after DGROUP:0x9316) until it is back at
//    the player p or names a ship in `state` 0, alive (+0xec) and of p's design team word (the
//    team test runs for a ship out of play too, and a dead ship fails) [BYTES seg11:0xaf1a-0xaf94];
//    back at p: 0;
//  - s found: DGROUP:0x9646 = p again, words +0xd0 / +0xd2 swapped between s and p,
//    seg3:0x6ec(p, DGROUP:0x9324, s, 0x71); DGROUP:0x9646 = s, s's design word +0x42 = 1 (it
//    captures icons), seg3:0xf4(s, t >> 8, t & 0xff) with t = seg13:0x2206() [BYTES
//    seg11:0xafa0-0xb01f];
//  - seg8:0x4d60(&s +0xd6, &s +0xd6) swaps a word with itself: each ship keeps its roster record,
//    DGROUP:0x931c (World::player_roster) = s's own +0xd6 (correction 61) [BYTES seg11:0xb022-0xb042];
//  - DGROUP:0x1e84 = DGROUP:0x1e80 = 0, PlayMode = 1, DGROUP:0x6b50 = p's object (x, y) when
//    DGROUP:0x951a was 0, DGROUP:0x951a = 1, seg11:0x3cc2(s, 1), seg11:0xae94, seg11:0x2f8c; 1
//    [BYTES seg11:0xb045-0xb090].
// `frame` (nullable) is the frame's copy of the words (WaveGlobals player, play_mode, autopilot),
// written with the World's so the frame's sync_out keeps them [DESIGN].
std::int16_t perspective_shift(World& w, ViewState& v, ShiftHost& h, WaveGlobals* frame = nullptr);

// seg7:0x7d5-0x7dc: the wave pass calls seg11:0xaece while DGROUP:0x1032 != 0 [BYTES
// seg7:0x7d5-0x7dc]. Returns seg11:0xaece's result, or 0 without the call.
std::int16_t shift_pass(World& w, ViewState& v, ShiftHost& h, WaveGlobals* frame = nullptr);

// ---- the program's shift (the slots src/hud fills; an empty slot does nothing)
struct ShiftLinks {
    std::function<void(int recipient, int pool, int rec, int code)> message;  // seg3:0x6ec
    std::function<int()> next_target;                                        // seg13:0x2206
    std::function<void(int ship, int pool, int rec)> set_target;             // seg3:0xf4
    std::function<void(int ship, int arg)> lamps;                            // seg11:0x3cc2
    std::function<void()> weapon_slot;                                       // seg11:0xae94
    std::function<void()> weapon_picture;                                    // seg11:0x2f8c
    std::function<void(int x, int y, int w, int h)> black_cell;             // PatBlt BLACKNESS
    std::function<void()> pin_player;                                        // seg7:0x11cc
    std::function<void()> draw_player;                                       // seg7:0x1208
    std::function<void(int mode)> info_mode;                                 // DGROUP:0xa76 + WM_PAINT
    std::function<void(const std::string& what, const char* address)> todo;
};
ShiftLinks& shift_links();
ShiftHost& linked_shift_host();

}  // namespace is::world
