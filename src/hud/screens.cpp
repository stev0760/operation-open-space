// The debriefing box and the GAME OVER screen (screens.h). M4 stage 1, track SCREENS.
#include "hud/screens.h"

#include <algorithm>

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

// ---- state 5 (M4 s3 SAVE)

std::vector<TableText> champion_texts(int W, int H, int client_h, const std::string& title,
                                      const std::vector<ChampionRow>& rows, int rank, std::int32_t score,
                                      const std::string& your, const std::string& score_word) {
    std::vector<TableText> out;
    const int w2 = static_cast<std::int16_t>(W) / 2, h2 = static_cast<std::int16_t>(H) / 2;  // cdq; sub; sar
    int hw = w2 - 20;  // [BYTES seg6:0x62c5-0x62dc]
    if (hw > 360) hw = 360;
    int cols = static_cast<std::int16_t>((hw - 33) * 2) / 18;  // idiv [BYTES seg6:0x62e0-0x62f6]
    if (cols > 39) cols = 39;
    int n = 10, dy = 0;
    if (client_h < 480) {  // DGROUP:0x10 [BYTES seg6:0x62fb-0x6311]
        dy = 15;
        n = static_cast<std::int16_t>(client_h) / 47;
    }
    // LoadString(925, 40 bytes) [BYTES seg6:0x6322-0x634d]
    out.push_back({0, 6, 0x000000FFu, w2, h2 + dy - 175, title.substr(0, std::min<std::size_t>(title.size(), 39))});
    int y = h2 + dy - 130;  // [bp-8] before any row [DESIGN: the loop always sets it first]
    for (int i = 0; i < n; ++i) {
        y = 30 * i + h2 + dy - 130;  // [BYTES seg6:0x6463-0x6477]
        if (static_cast<std::size_t>(i) >= rows.size() || rows[static_cast<std::size_t>(i)].text.empty())
            break;  // [BYTES seg6:0x6497-0x649b]
        const ChampionRow& r = rows[static_cast<std::size_t>(i)];
        Colour colour;
        if (r.score == score) {
            colour = 0x02FFFFFFu;  // [BYTES seg6:0x64ba-0x64c0]
        } else {
            int g;
            if (i < 3 && rank > 7)  // [BYTES seg6:0x635d-0x636a]
                g = 0xff;
            else  // imul 0xff01; cdq; idiv 10; + 0xe6 [BYTES seg6:0x6371-0x637f]
                g = static_cast<std::int16_t>(static_cast<std::int16_t>(i * 0xff01) / 10 + 0xe6);
            colour = 0x02000000u | (static_cast<Colour>(g & 0xff) << 8) | 0xffu;
        }
        out.push_back({0, 2, colour, w2 - hw + 7, y, std::to_string(r.place)});  // seg6:0xf1a
        std::string t = r.text;
        if (cols >= 0 && t.size() > static_cast<std::size_t>(cols)) t.resize(static_cast<std::size_t>(cols));
        out.push_back({0, 0, colour, w2 - hw + 46, y, t});                  // [BYTES seg6:0x63d4-0x640b]
        out.push_back({0, 2, colour, w2 + hw - 30, y, std::to_string(r.score)});  // seg6:0xeb8
    }
    // 308 + 2063 + ": " (DGROUP:0xf5b) + ltoa(score) [BYTES seg6:0x64c6-0x6564]
    out.push_back({0, 6, 0x02FFFFFFu, w2, y - dy / 2 + 50, your + score_word + ": " + std::to_string(score)});
    return out;
}

void draw_champions(Canvas& c, const std::vector<TableText>& texts) {
    for (const TableText& t : texts) c.font_text(t.font, t.align, t.colour, t.x, t.y, t.text);
}

}  // namespace is::hud
