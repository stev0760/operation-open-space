// owner: world set-up (the single writer of this file in later stages).
// The wave set-up's hook points (hooks.cpp, Win32 only): the state-1 entry seg10:0x0 (the load),
// the briefing entry's world part seg9:0xc06 / seg9:0x5cca (the icon fill and the placement), the
// wave start seg9:0x60bc (the ships) and the teardown. The program's World lives here.
#pragma once

#include <cstdint>
#include <functional>

#include "shell/game_state.h"
#include "world/entry_effect.h"
#include "world/wave_end.h"
#include "world/world.h"

#ifdef _WIN32
#include <filesystem>

#include "gfx/gfx.h"
#endif

namespace is::app {
struct App;
// Installs the hooks (main.cpp, after install_briefing_hooks, whose state-1 and state-3 entries
// it wraps).
void install_world_hooks(App& app);
}  // namespace is::app

namespace is::world {

// The program's World (DGROUP), one for the whole run.
World& current();

#ifdef _WIN32
// The icon strip DGROUP:0x3a02 the briefing entry built: 38 N + 6 x 44 pixels, N = the pool-0
// in-use count, black, slot s's 32 x 32 cell at (38 s + 6, 6) (world.md "Extraction"). Null
// before the first briefing entry.
gfx::ImagePtr icon_strip();
// seg10:0x5b30 again on the program's World, the strip unscaled and blacked with each slot's
// icon or placeholder, for the rescale's reload path [BYTES seg10:0x9f7f-0x9f83] (M4 s8 integrate).
void rebuild_icon_strip();
// The last placement's seg14:0x0(-1) ran on the drawing side's RosterHost (on_roster_build), so
// every ship's sheet is made; false: the player-only set-up, whose sheet the placement hook builds.
bool roster_placed();
// INI ShipPath as ADR 0004 decision 4 resolves it (the directory of MASTER.SHP and MASTER.BOD).
std::filesystem::path program_ship_dir();
#endif

// seg9:0x60bc's calls after the ships (seg11:0x3cc2(player) and the HUD repaint): src/hud sets
// it; the world hooks run it after start_wave (M3 s2 fixer).
void on_wave_start(std::function<void(World&)> f);
// The image work of the placement seg9:0x6624 at the briefing entry: seg14:0x0(-1)'s sheets
// (seg14:0x11a8 into slot 0x28 + i) and flame strip (seg10:0x5d3c, slot 0x22), which phase 11
// of the briefing draws from [BYTES seg9:0x688a seg14:0xc7e seg14:0xd32]. src/hud sets it; the
// world hooks run it after enter_briefing's placement (M5 s2 BRIEFING).
void on_briefing_placement(std::function<void(World&)> f);
// The design mass DGROUP:0x9704 + 0xad·design of the ship set-up, for seg11:0x1bcc: design word
// +0x32 of World::design_ratings, which seg14:0x1b84 fills for every ship (roster_build.h
// design_fill_portable) (0 for a design outside 0 ... 7) (M3 s2 fixer; M5 s3 integrate, STEER B4).
std::int16_t design_mass(int design);
// seg14:0x0(mode) with the program's RosterHost (roster_build.h build_wave_roster), bracketed by
// seg14:0x3eda / seg14:0x4074: mode -1 at the placement seg9:0x6624 [BYTES seg9:0x6883
// seg9:0x688a seg9:0x6890], a call-in's kind or team from seg13:0xae68 [BYTES seg13:0xb3fa
// seg13:0xb45c seg13:0xb467]. src/hud (wave_win32.cpp) sets it, since the RosterHost builds the
// ships' sheets; unset, the placement runs the player-only set-up (M5 s3 integrate).
void on_roster_build(std::function<void(World&, int mode)> f);
// Runs it; false when no drawing side set it.
bool run_roster_build(World& w, int mode);

// ---- M4 stage 1, track END: the gate exit and the wave end on the program's World
// The calls to other parts these need; an empty slot logs a TODO once. The program fills them
// (the fixer's wiring, see work/world/m4s1/end-report.md).
struct WaveEndLinks {
    std::function<void()> clear_restored;       // DGROUP:0x92e4 = 0 in the game-start session
    std::function<void()> close_hud;            // seg11:0x2f0 (src/hud close_hud)
    std::function<void()> close_watch_windows;  // seg13:0x65f2(1)
    std::function<void()> history_box;          // seg16:0x68d6(main, 2328, 2329), modal
    std::function<void(int pool, int rec)> column_row;  // seg7:0x65aa
    std::function<void(int ship, int id)> message;      // seg3:0x6ec(ship, 7, 0, id)
    std::function<void(int rec, int arg)> icon_counted; // seg7:0x5ae8(rec, arg)
    // seg13:0x65f2(m), the three watch windows (src/hud/watch.h watch_windows), for the Hall of
    // Justice's landing seg15:0x302e (m = 3) (M5 s4 integrate).
    std::function<int(int m)> watch_windows;
    // seg6:0x885c(0, 0), LASTGAME at the wave end (src/dialogs program_save_lastgame) [BYTES
    // seg9:0x31f0 seg7:0x687] [DESIGN ADR 0014] (M4 s3 fixer).
    std::function<void()> save_lastgame;
};
WaveEndLinks& wave_end_links();
// The entry effect's context of the wave's drawing side (src/hud wave_win32.cpp entry_context),
// for seg12:0x2556, seg7:0x13b2 and seg12:0x26a4 (SCREENS sets it in install_wave).
void set_entry_context(std::function<EntryContext()> f);
// The state-2 screen's start, set by the briefing (src/briefing/hooks.cpp): called after
// seg9:0x6cc4 with the World whose values the page reads.
void on_debriefing(std::function<void(World&)> f);

// UpdateHost::gate_passed(pool, rec): seg12:0x2288 on the program's World (gate_exit.h).
void run_gate_exit(int pool, int rec, std::uint32_t now);
// EntryHooks::vanished(pool, rec): the end of a mode-1 vanish (gate_exit.h vanish_end).
void run_vanish_end(int pool, int rec, std::uint32_t now);
// The state-1 frame hook's first step: the tick's deadline block seg7:0x5ce-0x707 on the
// program's World (wave_end.h); kEndPass: run no wave pass and frame [DESIGN ADR 0001].
DeadlineResult run_wave_deadlines(shell::GameState& s);

}  // namespace is::world
