// A computer ship's reactions to the ships around it (docs/re/pilots.md "Reactions";
// work/analysis/m5/s4-commands.md section 2 and 2.1 as corrected by s4-commands-verify.md claims
// 50-56 and the Omissions on seg13:0x7ba2 and seg13:0x9bb8) (M5 stage 4 part B, track COMMANDS):
//  - seg13:0x7ba2(i), cdecl, the think's re-plan step 4c (seg4:0x545): flee, repair, stand,
//    call for help, ask for an attack, call the ambulance, calm down, ask for revenge, go for a
//    virus;
//  - seg13:0x9bb8(i, n), cdecl, "time for the ambulance": 0 or 1, marking the wave in
//    DGROUP:0x1c8e + 2i.
// Widths are the original's; the one direct rand() (seg13:0x8166) is drawn only on its path, and
// the callees' (seg13:0x81c2, seg13:0x8570, seg16:0xb4e) are theirs (ruling R1).
//
// Portable, no Win32.
#pragma once

#include <cstdint>

#include "world/commands.h"

namespace is::world {

void reactions(CommandCtx& c, int i);                      // seg13:0x7ba2
std::int16_t ambulance_time(CommandCtx& c, int i, int n);  // seg13:0x9bb8

// On the program's World (PilotHost::reactions, pilot_think.cpp's step 4c).
void command_reactions(int i);

}  // namespace is::world
