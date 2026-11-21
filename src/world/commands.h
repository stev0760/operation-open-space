// The player's orders to other ships and the ships' orders to each other (docs/re/pilots.md
// "Commands"; work/analysis/m5/s4-commands.md sections 0, 1, 1.9, 1.11 and 5 as corrected by
// s4-commands-verify.md, whose CORRECTED verdicts and Omissions are followed): the key handlers
// of seg11:0x6b64 for A, C, D, E, F, G, H, S [BYTES seg11:0x6f20-0x7130], the command routines
// seg13:0x81c2 (help), 0x8570 (attack), 0x8e98 (defend), 0x915c (challenge), 0x9810
// (self-preserve), 0x9ce0 (capture), 0x9f3e (exit), 0xa01c (get gas), seg13:0x910a, seg13:0x9970,
// the defenders' seg13:0x9a18 (Alt+A), seg13:0x9ae8 (Alt+H) and the Alt+E loop seg11:0x70bb-0x7113
// with the pool-2 mover's state 15 (seg8:0x8f5b-0x9000, seg8:0x8300-0x847e), the request record
// of a computer ship to the Strategy player (seg13:0xa5c2, seg13:0xa65a), the Strategy keys
// seg13:0xa186 ... 0xa568 and Ctrl+Alt+S seg13:0x4ac4 (M5 stage 4 part B, track COMMANDS).
//
// Notation: P = DGROUP:0x9646 (the player), now = DGROUP:0x1e50, "ship word +N" = DGROUP:0x7090
// + 0x15a·i + N (pilot.h), team(i) = design word +0x38 of ship i (DGROUP:0x970a + 0xad·n),
// msg(r, p, s, m) = seg3:0x6ec(r, p, s, m). Widths are the original's (records.h helpers: every
// `imul byte` is AX = sext(AL)·sext(byte), every `cdq; idiv r16` a 16-bit signed divide), and
// every rand() is drawn from PilotCtx::rng in the original's order (ruling R1).
//
// Portable, no Win32.
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

#include "world/collide.h"
#include "world/pilot_think.h"

namespace is::world {

// --- The DGROUP words of the commands ---------------------------------------------------------
// [DESIGN] Their one home until the integrator joins them into World; all BSS (0 at load).
struct CommandWords {
    // The request a computer ship made of the Strategy player (s4-commands.md 1.11): the order
    // (1 help, 4 attack, 5 defend, 0x0b duel), its object, the requester, the time it was
    // recorded and the deferred reply's time [BYTES seg13:0x84d1-0x84f2 seg13:0x8bc9-0x8bec
    // seg13:0x90af-0x90cb seg13:0x955e-0x9577 seg13:0xa69d-0xa6b0].
    std::int16_t req_order = 0;     // DGROUP:0x964e
    std::int16_t req_pool = 0;      // DGROUP:0x9650
    std::int16_t req_rec = 0;       // DGROUP:0x9652
    std::int16_t req_by = 0;        // DGROUP:0x9654
    std::uint32_t req_time = 0;     // dword DGROUP:0x8858
    std::uint32_t reply_due = 0;    // dword DGROUP:0x885c
    // dword DGROUP:0x8710: frame# (DGROUP:0x1e68) of the last pool-9 attack a computer ship got
    // obeyed (seg13:0x8570) [BYTES seg13:0x8623-0x8662]. Dword DGROUP:0x8714, frame# of the
    // player's last virus attack goal, has its one home in player_tick.h PlayerTickState::dg_8714,
    // which seg7:0xf4b writes (M5 s4 integrate, COMMANDS binding B8).
    std::uint32_t virus_ask = 0;    // DGROUP:0x8710
    // DGROUP:0x1c8e + 2i: the wave (DGROUP:0x91f8) of ship i's last "time for the ambulance"
    // (seg13:0x9bb8) [BYTES seg13:0x9c36-0x9c66].
    std::array<std::int16_t, 8> ambulance_wave{};
    // The pool-2 defender words below the entry base DGROUP:0x52fe + 0x24·k (s4-commands.md 5):
    // the orbit offset from the owner (DGROUP:0x52ec / 0x52ee + 0x24k, seg13:0x9ae8) and the
    // homing flag (DGROUP:0x52f6 + 0x24k, seg13:0x9a18, the mover's state 15). Index k as the
    // defender table's (collide.h CollideGlobals::defenders); grown on demand [DESIGN].
    struct DefenderWords {
        std::int16_t dx = 0;      // -0x12 DGROUP:0x52ec
        std::int16_t dy = 0;      // -0x10 DGROUP:0x52ee
        std::int16_t homing = 0;  // -0x08 DGROUP:0x52f6
        std::int16_t w02 = 0;     // +0x02 DGROUP:0x5300 (seg8:0x9028 clears it for state 5)
    };
    std::vector<DefenderWords> defender;
    DefenderWords& def(int k);
    // The entry's dword +0x0e (DGROUP:0x530c + 0x24k), the mover's timer, read by state 15
    // [BYTES seg8:0x844e-0x847e]; collide.h DefenderEntry has no home for it [DESIGN].
    std::vector<std::uint32_t> timer;
    std::uint32_t& def_timer(int k);
    // The pool-2 mover's head (seg8:0x8277-0x82c0): DGROUP:0x9452, 1 on a steering frame; its
    // deadline dword DGROUP:0x186c; the 0 ... 4 counter DGROUP:0x1874.
    std::int16_t steer = 0;          // DGROUP:0x9452
    std::uint32_t steer_due = 0;     // DGROUP:0x186c
    std::int16_t w1874 = 0;          // DGROUP:0x1874
};
CommandWords& command_words();  // the program's (one set, as DGROUP is)

// --- Calls out of the commands that PilotHost does not carry ------------------------------------
// Every default logs once through world::guard_todo, names the address and does nothing (ruling R3).
class CommandHost {
public:
    virtual ~CommandHost() = default;
    // seg13:0x4eec(s, v), cdecl: the capture count of ship word +0x82.
    virtual void capture_count(int ship, int v);
    // The default calls missile.h missile_links().capture_count, the program's binding.
    // seg13:0x2714(a, b, dt, ds, prop, kind), cdecl: the default is relations.h attitude_change
    // with the program's attitude_links().
    virtual void attitude_change(World& w, int a, int b, int dt, int ds, int prop, int kind);
    // seg3:0x33b4(n), the Strategy player's order notice (M8).
    virtual void order_notice(int n);
    // The pool-2 defender table DGROUP:0x52fe + 0x24·k (collide.h CollideGlobals::defenders);
    // null while unbound.
    virtual std::vector<DefenderEntry>* defenders();
    // seg13:0x498e(s, flag), cdecl: the resource pack (death.h drop_pod).
    virtual void drop_pod(int s, int flag);
};

// What the commands touch: the pilot's context (pools, ships, the flight globals, the World, the
// PilotGlobals, rand() and the PilotHost), the host above and the words.
struct CommandCtx {
    PilotCtx& p;
    CommandHost& h;
    CommandWords& cw;
};

// --- The command routines (cdecl; c the asking ship; (tp, tr) = c's target words +0x3e/+0x40) --
std::int16_t help_command(CommandCtx& c, int asker);           // seg13:0x81c2: -1, 0 or 1
std::int16_t attack_command(CommandCtx& c, int asker);         // seg13:0x8570: -1, 0 or 1
void defend_command(CommandCtx& c, int asker);                 // seg13:0x8e98
std::int16_t no_other_ship(CommandCtx& c, int asker);          // seg13:0x910a: 1 when no other ship is in play and alive
void challenge_command(CommandCtx& c, int asker);              // seg13:0x915c
void preserve_command(CommandCtx& c, int asker);               // seg13:0x9810
std::int16_t ask_attack(CommandCtx& c, int asker);             // seg13:0x9970: -1, 0, or seg13:0x8570's
std::int16_t capture_command(CommandCtx& c, int asker);        // seg13:0x9ce0: 0 or 1
void exit_command(CommandCtx& c, int asker);                   // seg13:0x9f3e
void gas_command(CommandCtx& c, int asker);                    // seg13:0xa01c
void send_defenders(CommandCtx& c, int i, int pool, int rec);  // seg13:0x9a18
// seg8:0x9028(k, s, pool, rec), cdecl: defender k's new state (raised to s; 5 clears word +0x02,
// 15 the homing flag) and target, unless the target is its owner or a defender of the same owner.
void defender_order(CommandCtx& c, int k, int s, int pool, int rec);
void defenders_return(CommandCtx& c, int i);                   // seg13:0x9ae8
void defenders_leave(CommandCtx& c);                           // seg11:0x70bb-0x7113 (Alt+E)
void arrest_command(CommandCtx& c, int s);                     // seg13:0x4ac4 (Ctrl+Alt+S)
// The request record (s4-commands.md 1.11).
void request_answered(CommandCtx& c);                          // seg13:0xa5c2
void request_reply(CommandCtx& c);                             // seg13:0xa65a (seg4:0x681)
// The Strategy keys (each returns at once while DGROUP:0x9134 = 0).
void strategy_attack(CommandCtx& c);                           // seg13:0xa186 (A)
void strategy_capture(CommandCtx& c);                          // seg13:0xa234 (C)
void strategy_exit(CommandCtx& c);                             // seg13:0xa3c8 (E)
void strategy_gas(CommandCtx& c);                              // seg13:0xa47c (G)
void strategy_help(CommandCtx& c);                             // seg13:0xa4f4 (D)
void strategy_needs(CommandCtx& c);                            // seg13:0xa568 (S)
// The pool-2 mover seg8:0x8256's state 15 for defender k (s4-commands.md 5): the (12, 1)
// fallbacks, then the spin, or, on a frame whose DGROUP:0x9452 the mover's head set (steer_frame)
// with the entry's timer dword +0x0e passed, the homing on the target. The integrator's mover
// calls it for an entry in state 15 and goes on with its tail (seg8:0x8484); false when the entry
// is not in state 15.
bool defender_attack_step(CommandCtx& c, int k, bool steer_frame);
// The pool-2 mover's head, once per pass before its loop [BYTES seg8:0x8277-0x82c0]: DGROUP:0x9452
// = 0; once dword DGROUP:0x186c < now (unsigned) it becomes now + sext(max(4·(55 - DGROUP:0x91f8),
// 110)) (16-bit `shl 2`) and DGROUP:0x9452 = 1; DGROUP:0x1874 counts 0 ... 4. Returns
// DGROUP:0x9452, the steer_frame of defender_attack_step.
bool defender_mover_head(CommandCtx& c);

// --- The key handlers (seg11:0x6b64's WM_KEYDOWN letters) ---------------------------------------
// ctrl and alt are the decode's flags [bp-0x1c] and [bp-0x1a]: GetKeyState(VK_CONTROL) /
// GetKeyState(VK_MENU) bit 15, or the menu messages 0x99e / 0x99b [BYTES seg11:0x6cd5-0x6d13].
// Each calls the routines in the bytes' order; the decode's gate (the player's lock-out, the demo)
// is input.cpp's.
void key_a(CommandCtx& c, bool ctrl, bool alt);  // seg11:0x7065
void key_c(CommandCtx& c, bool ctrl, bool alt);  // seg11:0x7019
void key_d(CommandCtx& c, bool ctrl, bool alt);  // seg11:0x6f20
void key_e(CommandCtx& c, bool ctrl, bool alt);  // seg11:0x70a5
void key_f(CommandCtx& c, bool ctrl, bool alt);  // seg11:0x6feb
void key_g(CommandCtx& c, bool ctrl, bool alt);  // seg11:0x6fff
void key_s(CommandCtx& c, bool ctrl, bool alt);  // seg11:0x7033
// H's decode is ambulance.h ambulance_key (seg11:0x6f7a-0x6fe8); its Ctrl and Alt calls are
// help_command(P) and defenders_return(P).

// --- The program's entry points (the integrator binds `with_ctx`) -------------------------------
// The program builds the frame's CommandCtx (program_pilot_ctx, its CommandHost, command_words())
// and runs f in it; unbound, every entry point logs once and does nothing [DESIGN].
struct CommandLinks {
    std::function<void(const std::function<void(CommandCtx&)>& f)> with_ctx;
};
CommandLinks& command_links();

// The keys on the program's World, for track KEYS (KeyActions).
void command_key_a(bool ctrl, bool alt);
void command_key_c(bool ctrl, bool alt);
void command_key_d(bool ctrl, bool alt);
void command_key_e(bool ctrl, bool alt);
void command_key_f(bool ctrl, bool alt);
void command_key_g(bool ctrl, bool alt);
void command_key_s(bool ctrl, bool alt);
void command_ctrl_h(int ship);  // ambulance.h AmbulanceHost::ctrl_h: seg13:0x81c2(ship)
void command_alt_h(int ship);   // ambulance.h AmbulanceHost::alt_h: seg13:0x9ae8(ship)
// The other callers' entries on the program's World.
void command_challenge(int ship);              // seg13:0x915c (CollideHost::seg13_915c, seg4:0x66e3)
void command_capture(int ship);                // seg13:0x9ce0 (PilotHost::seg13_9ce0, seg4:0x446e)
void command_defenders_return(int ship);       // seg13:0x9ae8 (PilotHost::seg13_9ae8)
void command_request_reply();                  // seg13:0xa65a (PilotHost::command_report)
void command_send_defenders(int i, int pool, int rec);  // seg13:0x9a18 (PilotHost::send_defenders)

}  // namespace is::world
