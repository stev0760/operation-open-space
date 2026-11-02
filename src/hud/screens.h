// What the player sees of the wave's end and of the game's end (M4 stage 1, track SCREENS). The
// geometry and the texts are track END's (src/briefing/debriefing.h: debrief_box,
// game_over_lines, from s1-end.md section 4a and seg9:0x6a12); these functions draw them:
// - the debriefing box of seg9:0x6dec on the briefing's canvas (briefing::Debriefing's box
//   hook), and the client blackout of seg9:0x6cc4;
// - the GAME OVER screen: seg9:0xd6c's blackout and seg9:0x6a12's lines in bitmap font 0 on the
//   HUD canvas (the window DC, ADR 0013).
// Portable.
#pragma once

#include <vector>

#include "briefing/briefing.h"
#include "briefing/debriefing.h"
#include "hud/canvas.h"

namespace is::hud {

// seg9:0x6dec's box, steps 4 and 5 of s1-end.md section 4a: seg11:0x1774(2, L - 6, T - 15, R + 6,
// B + 6) with the unclamped edges, the filled title bar seg11:0x1774(-1, cL, T - 15, cR, T + 10),
// then seg9:0x8bec of the title (3003 with DGROUP:0x91f8; 22 x 10, TA_LEFT) and of 2238 (14 x 0,
// TA_RIGHT), each with its white copy 2 px down and right under the 0x025A5A5A text. The interior
// is not filled [BYTES seg9:0x6e7b-0x6f22 seg9:0x8bec-0x8d17]. The clip that follows is the
// caller's (briefing::Debriefing::frame).
void draw_debrief_box(briefing::Canvas& c, const briefing::DebriefBox& b);

// seg9:0x6cc4's PatBlt(DGROUP:0x13dc, 0, 0, DGROUP:0x90ea, DGROUP:0x10, BLACKNESS): the whole
// client, after the HUD windows are gone [BYTES seg9:0x6d7a].
void debrief_blackout(briefing::Canvas& c, int W, int H);

// seg9:0x6a12: each line through seg10:0x94c4(font 0, align, colour, x, y, text) [BYTES
// seg9:0x6a5c seg9:0x6aa0 seg9:0x6ae0 seg9:0x6b2a seg9:0x6b74 seg9:0x6bb6].
void draw_game_over(Canvas& c, const std::vector<briefing::GameOverLine>& lines);

// seg9:0xd6c's drawing: PatBlt(DGROUP:0x13dc, 0, 0, DGROUP:0x90ea, DGROUP:0x10, BLACKNESS)
// always, then seg9:0x6a12 only while no state request is pending (DGROUP:0x13de = 0) [BYTES
// seg9:0xdcc-0xde9].
void game_over_entry(Canvas& c, int W, int H, bool request_pending,
                     const std::vector<briefing::GameOverLine>& lines);

}  // namespace is::hud
