// What the player sees of the wave's end and of the game's end (M4 stage 1, track SCREENS). The
// geometry and the texts are track END's (src/briefing/debriefing.h: debrief_box,
// game_over_lines, from s1-end.md section 4a and seg9:0x6a12); these functions draw them:
// - the debriefing box of seg9:0x6dec on the briefing's canvas (briefing::Debriefing's box
//   hook), and the client blackout of seg9:0x6cc4;
// - the GAME OVER screen: seg9:0xd6c's blackout and seg9:0x6a12's lines in bitmap font 0 on the
//   HUD canvas (the window DC, ADR 0013).
// Portable.
#pragma once

#include <cstdint>
#include <string>
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

// ---- state 5: the champions table seg6:0x62b6 (M4 s3 SAVE)
// One text of the table, drawn through seg10:0x94c4(font, align, colour, x, y, text).
struct TableText {
    int font = 0;
    int align = 0;
    Colour colour = 0;
    int x = 0, y = 0;
    std::string text;
    bool operator==(const TableText& o) const {
        return font == o.font && align == o.align && colour == o.colour && x == o.x && y == o.y && text == o.text;
    }
};
// A slot of the table DGROUP:0x94d8 (formats/savegame.h TableSlot): the text +0x00, the score
// +0x28 and the place +0x2c.
struct ChampionRow {
    std::string text;
    std::int32_t score = 0;
    int place = 0;
};
// seg6:0x62b6's texts [BYTES seg6:0x62c5-0x6564]: W = DGROUP:0x90ea and H = DGROUP:0x90ec,
// halved toward zero; hw = min(W/2 - 20, 360); cols = min((hw - 33) * 2 / 18, 39) (signed
// idiv); 10 rows, or client_h / 47 rows 15 px lower when the client height DGROUP:0x10 is below
// 480. The title (string 925 cut to 39 bytes) in 0x000000FF at (W/2, H/2 + dy - 175), align 6;
// row i at y = 30 i + H/2 + dy - 130, stopping at the first empty slot: the place (align 2, x =
// W/2 - hw + 7), the text cut to cols characters (align 0, W/2 - hw + 46) and the score (align 2,
// W/2 + hw - 30), all in 0x02FFFFFF for the running score's row, else 0x0200gg FF with g = 0xFF
// for i < 3 and rank > 7, else (i x 0xff01, 16-bit) / 10 + 230; the footer `your` + `score` +
// ": " + the running score in 0x02FFFFFF at (W/2, y - dy/2 + 50), y being the row the loop
// stopped at. Font 0 throughout.
std::vector<TableText> champion_texts(int W, int H, int client_h, const std::string& title,
                                      const std::vector<ChampionRow>& rows, int rank, std::int32_t score,
                                      const std::string& your, const std::string& score_word);
void draw_champions(Canvas& c, const std::vector<TableText>& texts);

}  // namespace is::hud
