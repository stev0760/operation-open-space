// State 2, the wave debriefing, and the GAME OVER screen's lines (docs/re/shell.md "State 2,
// debriefing", "State 4, GAME OVER"; docs/re/core.md "The debriefing";
// work/analysis/m4/s1-end.md section 4 with s1-end-verify.md). Portable.
//
// - seg9:0x6fee: the page text of page kind 2 (debrief_text), line by line.
// - seg9:0x6dec(arg): the box, its title bar and labels, the page origin and clip, the page load
//   seg17:0x30b0(11, 3500) on the first frame, the typer step and DGROUP:0x1e74 (Debriefing).
// - seg9:0x6a12: the six GAME OVER lines (game_over_lines).
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "briefing/briefing.h"
#include "briefing/lang_format.h"
#include "shell/game_state.h"

namespace is::briefing {

// String ids of the page [BYTES ENG_LANG.DLL STRING 20] id 318 [BYTES ENG_LANG.DLL STRING 35]
// id 545 [BYTES ENG_LANG.DLL STRING 140] ids 2238-2239 [BYTES ENG_LANG.DLL STRING 135] id 2146
// [BYTES ENG_LANG.DLL STRING 188] ids 3003-3007 [BYTES ENG_LANG.DLL STRING 189] ids 3010-3017.
constexpr int kStrDirectory = 318, kStrColon = 545, kStrRaceTrack = 2239, kStrDuel = 2146;
constexpr int kStrDebriefTitle = 3003, kStrEscContinue = 2238;
constexpr int kStrScore = 3004, kStrFinished = 3005, kStrOfIts = 3006, kStrThusFar = 3007;
constexpr int kStrPlayedFor = 3010, kStrCapturing = 3012, kStrRevenge = 3013, kStrKilled = 3014;
constexpr int kStrRelation = 3017, kStrWasKilledBy = 879, kStrYou = 885, kStrNoChange = 789;
constexpr int kStrDodgeCity = 629;

// One ship of the page's ship paragraphs (steps 10 and 11), as the program reads it from the
// ship records; the port's waves have no other ships yet, so the list is usually the player's
// alone [OPEN: the computer ships are M5].
struct DebriefShip {
    bool alive = true;            // byte +0xec != 0
    bool team7 = false;           // design word DGROUP:0x970a + 0xad·(+0x4e) = 7
    bool same_team = false;       // that word equals the player's
    std::string name;             // seg3:0x1542(0, 14, i)
    int killer_pool = -1, killer_rec = -1;  // words +0x06 / +0x08
    std::string killer_name;      // seg3:0x1542(1, +0x06, +0x08, ..., 1 or 2)
    std::uint8_t relation = 0;    // byte +0xee + player (DGROUP:0x717e + 0x15a·i + player)
    std::array<std::int8_t, 8> towards{};  // bytes +0xf6 + 9·player + k (DGROUP:0x7186)
    std::string team_name;        // seg13:0x218a(team)
};

// The values seg9:0x6fee reads.
struct DebriefInputs {
    int kind = 0;                 // DGROUP:0x9320 (6 race, 7 duel)
    std::string directory;        // DGROUP:0x38c8
    int race = 0;                 // DGROUP:0x94e
    std::uint32_t score = 0;      // DGROUP:0x1e58
    int icons = 0;                // DGROUP:0x94a6, the directory's icons
    int destroyed = 0;            // directory word +0x14e
    int captured = 0;             // directory word +0x150
    int dir_state = 0;            // directory byte +0x1d0 (2: finished)
    std::string pilot;            // DGROUP:0x380c
    int seconds = 0;              // DGROUP:0x934e
    int cap = 0;                  // player word +0xd2 - DGROUP:0x9370
    int des = 0;                  // player word +0xd0 - DGROUP:0x9372
    int player = 0;               // DGROUP:0x9646
    std::vector<DebriefShip> ships;  // 0 ... DGROUP:0x9316
    // Step 9's team-7 revenge (seg9:0x8016-0x8091) needs the tables DGROUP:0x4f16/0x4ffa; the
    // program passes whether it applies and the team name [OPEN].
    bool revenge = false;
    std::string revenge_team;
    // Step 13: DGROUP:0x93b6 != -1 and the name seg3:0x1542(1, 14, seg14:0x4520(...)).
    bool dodge_city = false;
    std::string dodge_name;
};

// seg17:0x42ea(1): " \r\n" [BYTES seg17:0x42ea DGROUP:0xbb5 DGROUP:0xbb7].
constexpr const char* kPageBreak = " \r\n";

// seg9:0x6fee for a wave that is not a race (kind 6) or a duel (kind 7): steps 1 ... 13 of
// s1-end.md 4c. The race block (seg9:0x71dc-0x7db5) and the duel block (seg9:0x7e17-0x7fa0) are
// not ported [OPEN]; the team paragraph (seg9:0x8455-0x87d9) writes 789 only, its other rules
// are [OPEN]. `rand` is rand(), drawn once per dead ship that killed itself (seg9:0x81c4).
// `phase` is DGROUP:0x93aa for %N.
std::string debrief_text(const DebriefInputs& in, const LangLookup& lang, int phase,
                         const std::function<int()>& rand);

// seg9:0x6dec's rectangles for W = DGROUP:0x90ea, H = DGROUP:0x10 [BYTES seg9:0x6e29-0x6f73].
struct DebriefBox {
    int L = 0, T = 0, R = 0, B = 0;  // DGROUP:0x6084 ... 0x608a: centre ± (290, 173)
    int cL = 0, cR = 0;              // max(L - 6, 0), min(R + 6, W)
    Bevel frame{};                   // seg11:0x1774(2, L - 6, T - 15, R + 6, B + 6), unclamped
    Bevel title_bar{};               // seg11:0x1774(-1, cL, T - 15, cR, T + 10)
    Label title{};                   // 3003 at (cL + 5, T - 13), 22 x 10, TA_LEFT
    Label esc{};                     // 2238 at (cR - 7, T - 6), 14 x 0, TA_RIGHT
    int ox = 0, oy = 0;              // DGROUP:0x5000 = L + 14, DGROUP:0x5002 = T + 18
    int clip_l = 0, clip_t = 0, clip_r = 0, clip_b = 0;  // DGROUP:0x93ae (L, T + 18, R, B)
};
DebriefBox debrief_box(int W, int H, int wave, const LangLookup& lang);

// seg9:0x6a12's line: seg10:0x94c4(font 0, align, colour, x, y, text).
struct GameOverLine {
    int align = 0;  // 0 left, 2 right, 6 centre
    std::uint32_t colour = 0;
    int x = 0, y = 0;
    std::string text;
};
// The six lines for W, H, the wave DGROUP:0x91f8, the player's words +0xd2 and +0xd0 and the
// score; none in the demo (DGROUP:0x95bc = 0) [BYTES seg9:0x6a20-0x6bb6].
std::vector<GameOverLine> game_over_lines(int W, int H, int wave, int captured, int destroyed,
                                          std::uint32_t score, bool player_game,
                                          const LangLookup& lang);

// The state-2 screen: seg9:0x6dec(0) per frame, seg9:0x6dec(1) on a repaint.
class Debriefing {
public:
    Debriefing(Canvas& c, Host& h) : c_(c), h_(h), typer_(c, h) {}

    // What the frame reads besides the page text: the play area, the wave for the title,
    // PlayMode DGROUP:0x9134 and the demo flag DGROUP:0x95bc.
    struct Setup {
        int W = 0, H = 0;
        int wave = 0;
        int play_mode = 0;
        bool player_game = true;
    };
    // seg9:0x6cc4's part: the next frame finds no page (DGROUP:0x6078 = 0) and loads it from
    // `text` [DESIGN: the text is built at the wave end, from the values seg9:0x6fee would read
    // at the first frame; the wave's values do not change between the two].
    void start(const Setup& s, std::string text);
    // seg9:0x6dec(arg) [BYTES seg9:0x6dec-0x6fec]. A box hook, when set, draws the box instead
    // of the canvas (src/hud's); otherwise the canvas draws it with seg11:0x1774 and seg9:0x8bec.
    void frame(shell::GameState& s, int arg);
    // The page block freed (seg17:0x306e), as the briefing's next load does.
    void clear() { loaded_ = false; }

    using BoxHook = std::function<void(const DebriefBox&)>;
    void set_box_hook(BoxHook h) { box_hook_ = std::move(h); }

    const DebriefBox& box() const { return box_; }
    const PageTyper& typer() const { return typer_; }
    const std::string& text() const { return text_; }
    bool loaded() const { return loaded_; }

private:
    Canvas& c_;
    Host& h_;
    PageTyper typer_;
    Setup setup_;
    DebriefBox box_;
    std::string text_;
    bool loaded_ = false;  // DGROUP:0x6078 != 0
    BoxHook box_hook_;
};

}  // namespace is::briefing
