// The flight role's hook points (flight_hooks.cpp, Win32 only): the state-1 frame hook that runs
// the wave pass and frame of update.h, state 1's route_input (the router seg11:0x6aa8 and the
// key switch seg11:0x6b64), and the key latch fed from the main window's WM_KEYDOWN / WM_KEYUP
// (ADR 0013 decision 6). The other roles bind their parts here; until a World is bound the
// frame hook only runs the previous state-1 frame hook.
#pragma once

#include <cstdint>
#include <functional>

#include "world/damage.h"
#include "world/input.h"
#include "world/update.h"
#include "world/view.h"

namespace is::app {
struct App;
// Installs the hooks (main.cpp, after install_briefing_hooks).
void install_flight_hooks(App& app);
}  // namespace is::app

namespace is::world {

class World;

// The wave's World (the set-up's owner binds it; nullptr unbinds). Binding copies the wave's
// counts into the flight globals at the next frame.
void bind_flight_world(World* w);
// The routines of other parts the update calls (nullptr: the logging defaults). The host's
// sound_frame replaces the previous state-1 frame hook's call at seg7:0xcaf.
void set_update_host(UpdateHost* host);
// The HUD's target codes, Ctrl+T and zoom tail (KeyActions::target_code, toggle_manual_target,
// zoom_changed); the switch's posts, speed and rescale steps stay the flight role's.
void set_key_actions(KeyActions* actions);

// The calls of the default UpdateHost that other parts of the port implement (src/hud's
// integration fills them, M3 s2 fixer); an empty slot keeps the logging stub. Each names the
// original's routine.
struct WaveLinks {
    std::function<void(std::int16_t pool_rec, int kind, int slot, int masked)> draw_sprite;  // seg8:0xb76
    std::function<void()> player_sprite;   // seg7:0x1208
    std::function<void()> visibility;      // seg8:0x4e14
    std::function<void(int)> dissolve;     // seg7:0x3a42(i)
    std::function<void()> radar_refresh;   // seg11:0x3396
    std::function<void()> fuel_mass;       // seg11:0x1bcc
    std::function<void()> turn_followup;   // seg11:0x894
    std::function<void(int)> direction_mark;  // seg13:0x24aa(d)
    std::function<void(int)> stars;        // seg8:0x158a(arg)
    std::function<void()> lines;           // seg12:0x3778
    std::function<void()> select_clip;     // SelectClipRgn(DGROUP:0x13dc, DGROUP:0x12) at seg7:0x84a
    // M4 s1 fixer: the image table DGROUP:0x39e8 of the effects' and the debris' drawing
    // (seg8:0x3fbe, seg13:0x447a, seg13:0x4710); null draws nothing.
    std::function<const ImageSlotTable*()> images;
    // seg7:0x119c(2, 0), the information display back to the target at the end of the speed or
    // message display [BYTES seg7:0xbe5].
    std::function<void()> info_target;
    // seg13:0x675e's HUD part at the ship mover's head (hud/wave.h pictures_tick) [BYTES
    // seg13:0x15] (M4 s2 fixer).
    std::function<void()> pictures;
};
WaveLinks& wave_links();
// DGROUP:0xa6c's owner (the radar, src/hud/radar.h): the key switch reads the zoom through it,
// so KeyState::zoom only carries the value into seg11:0x6b64 (M3 s2 fixer).
void set_zoom_source(std::function<std::int32_t()> zoom);
// The wave start (seg9:0x60bc): the latch's `pressed` bits are cleared (KeyLatch::clear_pressed).
void wave_started();

// DGROUP:0x59a: the wave frames of the last full second (the wave tick's count, seg7:0xadc), for
// the B display's ACTUAL line (src/hud links HudLinks::frame_rate_actual to it).
int frame_rate_actual();

// The collisions' calls into the HUD (M4 s1 COLLIDE): the message display seg3:0x6ec and the
// in-use counts seg7:0x2080; an empty slot logs once. The destroy handlers (seg12:0x274a,
// seg16:0x8e4) and the wing debris (DGROUP:0x9176) are the world's own (effects.h, debris.h) and
// run on the program's World without a link (M4 s1 fixer).
struct CollideLinks {
    std::function<void(int ship, int pool, int rec, int code)> message;  // seg3:0x6ec
    std::function<void(int pool, int rec)> in_use;                       // seg7:0x2080
    // M4 s2 fixer: seg7:0x233c's crack and seg7:0x2768's fragment bitmaps and pictures on the
    // strip DC DGROUP:0x3a02 (src/hud/wave_win32.cpp) (collide.h CrackPlan, FragmentPlan).
    std::function<void(const CrackPlan&)> icon_crack;
    std::function<void(int r, std::int16_t side)> fragment_bitmap;
    std::function<void(const FragmentPlan&)> fragment_pictures;
};
CollideLinks& collide_links();
// seg12:0x274a(pool, rec), the generic destroy handler (effects.h object_destroyed), on the
// program's World with the flight role's words (the gate exit's dead ship, M4 s1 fixer).
void destroy_object(int pool, int rec);
// The collision DGROUP words (collide.h CollideGlobals) and the auto-repair's words of the
// program. wave_started() (seg9:0x60bc) resets the pair orientation (ADR 0015 decision 1) and
// the repair deadline, which the placement seg9:0x6624 clears with the frame count
// [BYTES seg9:0x6689 seg9:0x668d].
CollideGlobals& collide_globals();
RepairState& repair_state();
// seg11:0x1d7c on the bound World, for the status tick's StatusHooks::seg11_1d7c (src/hud).
void repair_tick();

// The flight role's state, for the other roles' hooks.
WaveGlobals& flight_globals();
FrameGlobals& frame_globals();
ViewState& flight_view();
KeyLatch& key_latch();
KeyState& key_state();

// seg8:0x0 for the set-up's SetupHost::recentre: the view from the World's player.
void recentre_view(World& w);
// seg8:0x29ea with the current view, for the sprite routine's ToScreen.
ScreenPoint flight_to_screen(std::int16_t x, std::int16_t y);

// The latch feed (wndproc.cpp): WM_KEYDOWN, WM_KEYUP, WM_SYSKEYDOWN and WM_SYSKEYUP with their
// wParam and lParam (bit 30: the key was down before) [DESIGN ADR 0013 decision 6].
void feed_key(std::uint32_t msg, std::uintptr_t wparam, std::intptr_t lparam);

}  // namespace is::world
