// The violence score DGROUP:0x9432 and its 10-second decay seg7:0x2128 (M4 stage 5, track ICONS,
// triage M-2; work/analysis/m4/s5-icons.md section 3 with s5-icons-verify.md claims 39-44).
//
// [MANUAL Dealing With Other Ships]: "If you are violent, the game responds with violence, but
// if you don't try to kill anyone, the game gets less violent." The player's shots, weapon uses,
// hits and kills raise World::w9432 (shots.cpp, missile.cpp, collide.cpp); the wave load scales
// it (setup.cpp load); seg9:0x2828 turns it into the class DGROUP:0x9332 of later waves. The
// wave pass calls seg7:0x2128 every 10 s while the player's `state` is 0 (update.cpp, the timer
// FrameGlobals::dg_5b8, which runs on across waves: DGROUP:0x5b8 has no other writer than the
// two timers, verified claim 40).
//
// 16-bit arithmetic as the original's (ADR 0013 decision 7). Portable, no Win32.
#pragma once

#include <cstdint>

#include "world/world.h"

namespace is::world {

// One step of seg7:0x2128 [BYTES seg7:0x2173-0x218a seg7:0x21e8-0x21f9]: d = A / div (`cwd;
// idiv`), at least `floor`; A -= d (16-bit `sub`), and 0 when the result is negative (`jns`).
std::int16_t violence_step(std::int16_t a, std::int16_t div, std::int16_t floor);

// seg7:0x2128(), cdecl, on A = World::w9432 (DGROUP:0x9432) [BYTES seg7:0x2128-0x2205] (verified
// claims 41-43):
//  - no other ship (DGROUP:0x9316 = World::last_ship = 0): one step (50, 2), no time test;
//  - else the ships k = 0 ... DGROUP:0x9316 − 1 (the last record is not tested) are searched for
//    one hunting the player: words +0x32 = 14 (literal), +0x34 = DGROUP:0x9646 and +0x38 = 1. The
//    first one gives a step (18, 4), then (14, 4) when the player's last shot or weapon use
//    (DGROUP:0x8e28 = World::w8e28) lies 15 s back: (u32)(w8e28 + 15000) < now;
//  - none hunting: a step (40, 3), then (36, 2) under the same time test.
// `now` is DGROUP:0x1e50, the pass's GetCurrentTime (WaveGlobals::now). No rand(), no sound.
void decay_violence(World& w, std::uint32_t now);

}  // namespace is::world
