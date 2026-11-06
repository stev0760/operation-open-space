// State 2 and the GAME OVER lines (debriefing.h). The page's steps follow s1-end.md 4c with the
// verifier's corrections (s1-end-verify.md 52-64).
#include "briefing/debriefing.h"

#include <algorithm>

namespace is::briefing {

namespace {

// seg9:0x95ba(id): LoadString(id) appended to the page [BYTES seg9:0x95ba].
void append_id(std::string& page, int id, const LangLookup& lang) {
    if (lang) page += lang(id);
}

// W / 2 as cdq; sub ax, dx; sar ax, 1: truncated toward zero [BYTES seg9:0x6e29].
int half(int v) { return v / 2; }

}  // namespace

std::string debrief_text(const DebriefInputs& in, const LangLookup& lang, int phase,
                         const std::function<int()>& rand) {
    std::string page;  // lstrcpy(DGROUP:0x6078, "") [BYTES seg9:0x7007]
    auto fmt = [&](int id, const std::vector<FArg>& a) { lang_format_into(page, id, 0, a, lang, phase, rand); };
    const bool disk = in.directory.size() > 1 && in.directory[1] == ':';  // DGROUP:0x38c9 = ':'
    const bool race = in.kind == 6;  // [bp-0xda]
    // 2. The directory line [BYTES seg9:0x7035 seg9:0x7043 seg9:0x705b seg9:0x706b seg9:0x7086
    // seg9:0x7097 seg9:0x709e].
    if (in.kind == 6 && !disk)
        append_id(page, kStrRaceTrack, lang);
    else if (in.kind == 7 && !disk)
        append_id(page, kStrDuel, lang);
    else
        append_id(page, kStrDirectory, lang);
    append_id(page, kStrColon, lang);
    page += in.directory;
    page += kPageBreak;
    // 3. 3004 with (DGROUP:0x94e = 0, the score) [BYTES seg9:0x70a4-0x70bd].
    fmt(kStrScore, {FArg(in.race == 0 ? 1 : 0), FArg(static_cast<unsigned long>(in.score))});
    // 4. The directory's counts [BYTES seg9:0x70e7-0x71cd].
    if (disk && in.kind != 6) {
        const int r = static_cast<std::int16_t>(in.icons - in.destroyed - in.captured);
        if (in.dir_state == 2) {
            fmt(kStrFinished, {FArg(in.directory), FArg(in.pilot), FArg(in.icons > 20 ? 1 : 0),
                               FArg(in.icons < 50 ? 1 : 0), FArg(2242), FArg(2243)});
            fmt(kStrOfIts, {FArg(in.icons > 1 ? 1 : 0), FArg(in.icons), FArg(2515), FArg(in.captured),
                            FArg(2506), FArg(in.destroyed), FArg(2506)});
        } else {
            fmt(kStrThusFar, {FArg(in.captured), FArg(2507), FArg(in.destroyed), FArg(2507),
                              FArg(r == 1 ? 1 : 0), FArg(2509), FArg(2508), FArg(r), FArg(2515)});
        }
        page += kPageBreak;
        page += kPageBreak;
    }
    // 5. The race block seg9:0x71dc-0x7db5 [OPEN M6] [BYTES seg9:0x71d3 seg9:0x71d8].
    if (!race) {
        // 6. 3010: the play time [BYTES seg9:0x7db8-0x7e06].
        const int s = in.seconds;
        fmt(kStrPlayedFor, {FArg(in.kind == 7 ? 1 : 0), FArg(870), FArg(1997), FArg(s >= 60 ? 1 : 0),
                            FArg(s / 60), FArg(2510), FArg(s % 60), FArg(2511)});
    }
    if (in.kind == 7) {
        // 7. The duel block seg9:0x7e17-0x7fa0 [OPEN M6] [BYTES seg9:0x7e13].
    } else {
        // 8. 3012 with (c, c, 2515, d, d, 2515, c = 0 and d = 0) [BYTES seg9:0x7fa3-0x7fdd].
        fmt(kStrCapturing, {FArg(in.cap), FArg(in.cap), FArg(2515), FArg(in.des), FArg(in.des), FArg(2515),
                            FArg(in.cap == 0 && in.des == 0 ? 1 : 0)});
    }
    // 9. The team-7 revenge [BYTES seg9:0x8016-0x8091]; its test is the program's [OPEN].
    if (in.revenge) fmt(kStrRevenge, {FArg(in.revenge_team)});
    // 10. The dead ships [BYTES seg9:0x80b0-0x8150].
    for (std::size_t i = 0; i < in.ships.size(); ++i) {
        const DebriefShip& sh = in.ships[i];
        if (sh.alive || sh.team7) continue;
        fmt(kStrKilled, {FArg(sh.same_team ? 1 : 0), FArg(sh.name), FArg(sh.same_team ? 1 : 0)});
        if (sh.killer_pool == 14 && sh.killer_rec == static_cast<int>(i)) {
            // seg9:0x95ba(rand() % 3 + 2311): ENG_LANG.DLL has no strings 2311-2313 (claim 60), so
            // only the ". " shows [BYTES seg9:0x81c4 seg9:0x81cf seg9:0x81d5] [BYTES ENG_LANG.DLL
            // STRING 145] ids 2310, 2314.
            const int r = rand ? rand() : 0;
            append_id(page, static_cast<std::int16_t>(r) % 3 + 2311, lang);
            page += ". ";
        } else {
            std::string t = lang ? lang(kStrWasKilledBy) : std::string();
            if (sh.killer_pool == 14 && sh.killer_rec == in.player)
                t += lang ? lang(kStrYou) : std::string();
            else
                t += sh.killer_name;
            t += ". ";
            page += t;  // [BYTES seg9:0x80da seg9:0x80fb seg9:0x8117-0x8142]
        }
    }
    // 11. Up to three ships the player dealt with most: per slot the largest positive sum of the
    // eight signed bytes +0xf6 + 9·player + j, the first on ties, the filled slots a prefix
    // (core.md The debriefing, step 7, The ranking; C078) [BYTES seg9:0x81e5-0x8445].
    {
        std::array<int, 3> sel{-1, -1, -1};
        std::array<int, 3> best{};
        int count = 0;
        for (int s = 0; s < 3; ++s) {
            best[static_cast<std::size_t>(s)] = 0;
            for (int i = 0; i < static_cast<int>(in.ships.size()); ++i) {
                if (i == in.player) continue;
                const DebriefShip& sh = in.ships[static_cast<std::size_t>(i)];
                if (sh.team7) continue;
                int sum = 0;
                for (std::int8_t b : sh.towards) sum += b;
                if (!(best[static_cast<std::size_t>(s)] < sum)) continue;
                if (sel[0] == i || sel[1] == i) continue;
                if (sh.relation == 3 || sh.relation == 0) continue;
                // The exclusion list [bp-0x42] holds -1 eight times and is never written
                // (claim 63) [BYTES seg9:0x7fef seg9:0x8262].
                best[static_cast<std::size_t>(s)] = sum;
                sel[static_cast<std::size_t>(s)] = i;
                count = s + 1;
            }
        }
        int verb = 1;  // [bp-0x130] keeps its last value for a byte outside 1 ... 8
        for (int j = 0; j < count; ++j) {
            const int i = sel[static_cast<std::size_t>(j)];
            if (i < 0) continue;  // a slot that found no ship [DESIGN]
            const DebriefShip& sh = in.ships[static_cast<std::size_t>(i)];
            switch (sh.relation) {  // the table at seg9:0x8366
            case 1:
                verb = (!sh.alive && sh.killer_pool == 14 && sh.killer_rec == in.player) ? 893 : 900;
                break;
            case 2: verb = 906; break;
            case 3: verb = 903; break;
            case 4: verb = 904; break;
            case 5: verb = 902; break;
            case 6: verb = 905; break;
            case 7: verb = 907; break;
            case 8: verb = 901; break;
            default: break;
            }
            fmt(kStrRelation, {FArg(verb), FArg(sh.same_team ? 1 : 0), FArg(sh.name),
                               FArg(sh.same_team ? 0 : 1), FArg(sh.team_name)});
            // seg9:0x8838(i) when word DGROUP:0x7102 + 2(0xad·i + player) > 5 [OPEN: unread].
        }
    }
    // 12. Two breaks, then the team paragraph: 789 when no team qualifies [BYTES seg9:0x8447
    // seg9:0x844f seg9:0x862d seg9:0x87d2]; the paragraph for a team whose relation
    // DGROUP:0x3824 moved from its wave-start copy DGROUP:0x9350 is [OPEN].
    page += kPageBreak;
    page += kPageBreak;
    append_id(page, kStrNoChange, lang);
    // 13. Dodge City [BYTES seg9:0x87da-0x8826].
    if (in.dodge_city) {
        page += kPageBreak;
        page += kPageBreak;
        page += in.dodge_name;
        append_id(page, kStrDodgeCity, lang);
    }
    // The block DGROUP:0x6078 holds 2000 bytes [BYTES seg17:0x310a].
    if (page.size() > 1999) page.resize(1999);
    return page;
}

DebriefBox debrief_box(int W, int H, int wave, const LangLookup& lang) {
    DebriefBox b;
    // SetRect(DGROUP:0x6084, W/2 - 0x122, H/2 - 0xad, W/2 + 0x122, H/2 + 0xad) [BYTES
    // seg9:0x6e29-0x6e4f].
    b.L = half(W) - 0x122;
    b.T = half(H) - 0xad;
    b.R = half(W) + 0x122;
    b.B = half(H) + 0xad;
    b.cL = std::max(b.L - 6, 0);   // [BYTES seg9:0x6e5e]
    b.cR = std::min(b.R + 6, W);   // [BYTES seg9:0x6e72]
    b.frame = Bevel{2, b.L - 6, b.T - 15, b.R + 6, b.B + 6};       // [BYTES seg9:0x6e8d]
    b.title_bar = Bevel{-1, b.cL, b.T - 15, b.cR, b.T + 10};       // [BYTES seg9:0x6ead]
    std::string title;
    lang_format_into(title, kStrDebriefTitle, 1, {FArg(wave)}, lang);  // [BYTES seg9:0x6ec4]
    b.title = Label{b.cL + 5, b.T - 13, 22, 10, kTaLeft, title};       // [BYTES seg9:0x6eec]
    b.esc = Label{b.cR - 7, b.T - 6, 14, 0, kTaRight, lang ? lang(kStrEscContinue) : std::string()};  // [BYTES seg9:0x6f22]
    b.ox = b.L + 14;  // DGROUP:0x5000 [BYTES seg9:0x6f2e]
    b.oy = b.T + 18;  // DGROUP:0x5002 [BYTES seg9:0x6f37]
    b.clip_l = b.L;   // CreateRectRgn(L, T + 18, R, B) [BYTES seg9:0x6f60]
    b.clip_t = b.T + 18;
    b.clip_r = b.R;
    b.clip_b = b.B;
    return b;
}

std::vector<GameOverLine> game_over_lines(int W, int H, int wave, int captured, int destroyed,
                                          std::uint32_t score, bool player_game,
                                          const LangLookup& lang) {
    std::vector<GameOverLine> v;
    if (!player_game) return v;  // [BYTES seg9:0x6a20 seg9:0x6a25]
    auto text = [&](int id, const std::vector<FArg>& a) {
        std::string s;
        lang_format_into(s, id, 1, a, lang);
        return s;
    };
    const int cx = half(W), cy = half(H);
    // [BYTES seg9:0x6a32 seg9:0x6a5c] 325 "GAME OVER", centred, 0x020080ff.
    v.push_back({6, 0x020080ffu, cx, cy - 0xaf, lang ? lang(325) : std::string()});
    // [BYTES seg9:0x6a72 seg9:0x6aa0] 2532 "LAST WAVE: %d" with DGROUP:0x91f8.
    v.push_back({0, 0x00008000u, cx - 0xf0, cy - 0x73, text(2532, {FArg(wave)})});
    // [BYTES seg9:0x6ab2 seg9:0x6ae0] 2533 "ICONS:".
    v.push_back({0, 0x00008000u, cx - 0xf0, cy - 0x55, text(2533, {})});
    // [BYTES seg9:0x6afc seg9:0x6b2a] 2534 "CAPTURED: %d" with the player's word +0xd2.
    v.push_back({2, 0x00008000u, cx + 0x7c, cy - 0x55, text(2534, {FArg(captured)})});
    // [BYTES seg9:0x6b46 seg9:0x6b74] 2535 "DESTROYED: %d" with the player's word +0xd0.
    v.push_back({2, 0x00008000u, cx + 0x7c, cy - 0x37, text(2535, {FArg(destroyed)})});
    // [BYTES seg9:0x6b8b seg9:0x6bb6] 2536 "SCORE: %l" with DGROUP:0x1e58, red.
    v.push_back({6, 0x000000ffu, cx, cy + 0x41, text(2536, {FArg(static_cast<unsigned long>(score))})});
    return v;
}

void Debriefing::start(const Setup& s, std::string text) {
    setup_ = s;
    text_ = std::move(text);
    loaded_ = false;  // DGROUP:0x6078 = 0: the briefing's last phase freed it (s1-end.md 4a.2)
}

void Debriefing::frame(shell::GameState& s, int arg) {
    // 1. Not in the demo, not while now = DGROUP:0x1e70 [BYTES seg9:0x6e00 seg9:0x6e10].
    if (!setup_.player_game || s.now() == s.deadlines().debrief_next) return;
    const int phase = s.briefing_phase();
    const bool just_made = arg != 0 || !loaded_;
    if (just_made) {  // [BYTES seg9:0x6e15-0x6e21]
        box_ = debrief_box(setup_.W, setup_.H, setup_.wave,
                           [this](int id) { return h_.lang(id); });
        if (box_hook_) {
            box_hook_(box_);
        } else {
            draw_bevel(c_, box_.frame.width, box_.frame.l, box_.frame.t, box_.frame.r, box_.frame.b);
            draw_bevel(c_, box_.title_bar.width, box_.title_bar.l, box_.title_bar.t, box_.title_bar.r,
                       box_.title_bar.b);
            draw_label(c_, box_.title);
            draw_label(c_, box_.esc);
        }
        // The box's inside is not filled: it is seg9:0x6cc4's black [BYTES seg9:0x6d7a].
        c_.clip(true, box_.clip_l, box_.clip_t, box_.clip_r, box_.clip_b);  // [BYTES seg9:0x6f73]
        typer_.set_origin(box_.ox, box_.oy, box_.R);  // DGROUP:0x6088 = R, the wrap edge
        if (!loaded_) {
            // seg17:0x30b0(11, 3500) in state 2: wrap on, kind 2, the 2000-byte block, the hold,
            // then seg17:0x2c5a(1, 0) with the CRT flash [BYTES seg9:0x6f80 seg9:0x6f87
            // seg17:0x30c3 seg17:0x30ca seg17:0x30d0 seg17:0x318e seg17:0x32c4 seg17:0x32cd].
            typer_.set_page(text_, true, 3500);
            loaded_ = true;
            typer_.step(s.now(), 1, 0, phase);
        } else {
            typer_.step(s.now(), 1, 1, phase);  // seg17:0x2c5a(1, 1) [BYTES seg9:0x6f95]
        }
    } else {
        c_.clip(true, box_.clip_l, box_.clip_t, box_.clip_r, box_.clip_b);  // [BYTES seg9:0x6fac]
    }
    // 8. The step and DGROUP:0x1e74 [BYTES seg9:0x6fbd seg9:0x6fc5 seg9:0x6fc9 seg9:0x6fd0
    // seg9:0x6fe1].
    const int r = typer_.step(s.now(), 0, 0, phase);
    if (setup_.play_mode != 0 && r == 1) s.deadlines().debrief_nav = s.now() + 3000;
    c_.clip(false, 0, 0, 0, 0);
}

}  // namespace is::briefing
