// The program's HUD (src/hud/hooks.cpp): the one Hud on the surface, the scrollbar, and the slots through which the other modules hand the HUD their data and calls.
// The repaint hook of states 1 and 7, the HUD's part of the state-1 entry (seg11:0x0), the
// panels' flush after each wave frame and seg11:0x2f0 at the end of the wave states are
// installed by install_hud_hooks (src/app/app.h). Portable header.
#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "hud/host.h"
#include "hud/hud.h"

namespace is::hud {

// What other modules provide. An empty slot gives hud::Host's default.
struct Links {
    // The wave's flags (the set-up, implementer A; the world, B and F).
    std::function<bool()> race;        // DGROUP:0x94e
    std::function<bool()> demo_gate;   // DGROUP:0x9130
    std::function<bool()> alt_view;    // DGROUP:0x951a
    std::function<int()> player;       // DGROUP:0x9646
    std::function<int()> ship_pool;    // DGROUP:0x9324
    std::function<int()> watch_ship;   // DGROUP:0x952e
    std::function<int()> view_mode;    // DGROUP:0x9e49
    std::function<bool()> blink;       // DGROUP:0x93fe
    // The panels' data.
    std::function<std::uint32_t()> score;       // DGROUP:0x1e58
    std::function<bool()> score_alt;            // DGROUP:0x951c
    std::function<bool()> score_alt_hi;         // DGROUP:0x104c
    std::function<std::uint32_t()> wave_ticks;  // DGROUP:0x1e68
    std::function<ShipView(int)> ship;
    std::function<WeaponView()> weapon;
    std::function<void(int, int)> message;      // seg3:0x6ec(ship, 7, 0, id)
    std::function<int()> frame_rate_set;        // DGROUP:0x59c
    std::function<int()> frame_rate_actual;     // DGROUP:0x59a
    // The target and message displays.
    std::function<int(int, int)> attitude;      // seg13:0x2688
    std::function<bool(int, int)> target_ok;    // seg3:0x0
    std::function<int(int, int)> target_kind;   // seg13:0x371c
    std::function<void()> bump_bad_index;       // DGROUP:0x9498
    std::function<std::string()> text_9628, text_8718, text_960a, text_8740, text_8768,
        text_87b8;
    std::function<bool(int, int, CellInputs&)> cell;  // seg13:0x5a6a's record fields
    std::function<int()> game_mode;                   // DGROUP:0x9320
    std::function<void(Canvas&)> draw_race;         // seg11:0x41bc(hdc, 1, 1)
    std::function<int()> player_target_pool, player_target_rec;  // ship +0x3e, +0x40
    std::function<int()> next_target;                          // seg13:0x2206
    std::function<void(int, int, int)> set_target;             // seg3:0xf4
    std::function<Point(int, int)> screen_point;
    std::function<Point()> player_point;
    std::function<int(int, int)> direction16;                  // seg8:0x19b0
    std::function<int(int)> in_use;                            // DGROUP:0x9406
    std::function<int(int, int, int, int)> distance;           // seg8:0x285a
    std::function<bool(int)> gate_post_live;                   // DGROUP:0x3d6c / 0x3d5a
    std::function<int()> record_bound_9422;
    std::function<bool()> mark2_window;                        // DGROUP:0x95fc
    std::function<void()> draw_mark2;
    // The state-1 screen of seg11:0x15b9 and the radar (implementer D).
    std::function<void()> demo_mark, pin_player, draw_player, set_58c, draw_1f38, radar;
    std::function<void()> radar_centre;  // seg11:0xb0c-0xb1f (Radar::set_centre)
    // The radar's zoom (src/hud/radar.h): 600 - DGROUP:0xa6c for SetScrollPos, and the
    // WM_VSCROLL handler seg11:0x7798(code, pos) (Radar::scroll).
    std::function<int()> scroll_pos;
    std::function<void(int, int)> vscroll;
    // The zoom keys' tail for a zoom the key switch computed (Radar::zoom_changed): stores it,
    // SetScrollPos(600 - zoom) and the radar [BYTES seg11:0x7233-0x7247].
    std::function<void(std::int32_t)> zoom_changed;
};

// The slots; fill them before the wave starts.
Links& links();
// The program's HUD; null before install_hud_hooks and in host builds.
Hud* hud();
// seg9:0x60bc's HUD part at the wave start: DGROUP:0xa76 = 2 [BYTES seg9:0x60bc].
void wave_start();
// WM_VSCROLL from the HUD scrollbar: seg11:0x7798(code, pos) [BYTES seg2:0x6a7-0x6ae], through
// links().vscroll.
void vscroll(int code, int pos);
// SetScrollPos(DGROUP:0xa7a, SB_CTL, pos, TRUE): Radar::set_scroll_pos's target.
void set_scroll_pos(int pos);
// seg11:0x2f0: the HUD's windows go (the scrollbar control and the IS_INFO panels).
void close();

// ---- M4 stage 1, track SCREENS: bound by src/hud/hooks.cpp (Win32); without the binding (the
// host build) they do nothing. src/world cannot link src/hud (ishud links isworld), so the world
// reaches them through a world-side slot that install_wave fills.
// seg3:0x6ec(recipient, pool, rec, code): the message dispatcher (hud/messages.h) on the
// program's left IS_INFO panel. The damage, death, capture and repair paths call it: 0xd (the
// OverThruster ram, string 350, sound 0x67 at most every 4 s), 0x5b / 0x5e (strings 687 / 692),
// 0x70 (string 1064), 0x54 (the ambulance's "Repairs Complete", string 138). The lamp messages
// 0xa, 0xb, 0xc, 0x52, 0x53 come from the damage panel itself (Panels::lamps -> Host::message).
void post_message(int recipient, int pool, int rec, int code);
// seg7:0x119c(mode, 0): DGROUP:0xa76 = mode (the World's word and the flight role's frame copy)
// and a repaint of the left panel. The frame's step 16 calls it with 2 when the message's end
// time DGROUP:0x870c has passed and DGROUP:0x9608 = 0 (world/update.cpp, UpdateHost::
// speed_display_end_7_119c) [BYTES seg7:0xbe3-0xbe7 seg7:0x11a6-0x11bf].
void info_mode_119c(int mode);
void bind_messages(std::function<void(int, int, int, int)> post, std::function<void(int)> mode);

// src/hud/hooks.cpp binds the program's HUD and its scrollbar calls here.
void bind_program(Hud* hud, std::function<void(int)> scroll_pos, std::function<void()> close);

}  // namespace is::hud
