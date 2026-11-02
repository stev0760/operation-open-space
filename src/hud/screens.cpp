// The debriefing box and the GAME OVER screen (screens.h). M4 stage 1, track SCREENS.
#include "hud/screens.h"

namespace is::hud {

void draw_debrief_box(briefing::Canvas& c, const briefing::DebriefBox& b) {
    // [BYTES seg9:0x6e7b-0x6e8d]
    briefing::draw_bevel(c, b.frame.width, b.frame.l, b.frame.t, b.frame.r, b.frame.b);
    // [BYTES seg9:0x6e95-0x6ead]
    briefing::draw_bevel(c, b.title_bar.width, b.title_bar.l, b.title_bar.t, b.title_bar.r,
                         b.title_bar.b);
    briefing::draw_label(c, b.title);  // [BYTES seg9:0x6eb5-0x6eec]
    briefing::draw_label(c, b.esc);    // [BYTES seg9:0x6ef2-0x6f22]
}

void debrief_blackout(briefing::Canvas& c, int W, int H) {
    c.black(0, 0, W, H);  // [BYTES seg9:0x6d7a]
}

void draw_game_over(Canvas& c, const std::vector<briefing::GameOverLine>& lines) {
    for (const briefing::GameOverLine& l : lines)
        c.font_text(0, l.align, l.colour, l.x, l.y, l.text);
}

void game_over_entry(Canvas& c, int W, int H, bool request_pending,
                     const std::vector<briefing::GameOverLine>& lines) {
    c.pat_blt(Img::kScreen, 0, 0, W, H, kBlackness, Brush::none());  // [BYTES seg9:0xdcc-0xddd]
    if (!request_pending) draw_game_over(c, lines);                 // [BYTES seg9:0xde2-0xde9]
}

}  // namespace is::hud
