// The screen-state machine: requests through the state-change step, the request-1-becomes-3
// rule, the deadlines and the ESC and SPACE rules of states 2-5 (docs/re/shell.md
// "Screen states").
#include <vector>

#include "doctest.h"
#include "shell/game_state.h"

using namespace is::shell;

namespace {

Input key_up(std::uint32_t vk) { return {kWmKeyUp, vk, 0}; }

}  // namespace

TEST_CASE("state: starts in state 0 with no request") {
    GameState g;
    CHECK(g.state() == kIntro);
    CHECK_FALSE(g.change_pending());
    CHECK_FALSE(g.change_step(100));
    CHECK(g.resizable());
}

TEST_CASE("state: a request applies in the state-change step, not before") {
    GameState g;
    g.set(kGameOver);
    g.request(kIntro);
    CHECK(g.state() == kGameOver);
    CHECK(g.change_pending());
    CHECK_FALSE(g.pass(10));  // a pending request ends the pass early
    std::vector<int> order;
    g.hooks().teardown = [&](GameState& s) { order.push_back(100 + s.state()); };
    g.hooks().play_area = [&](GameState& s) { order.push_back(200 + s.state()); };
    g.hooks().enter[kIntro] = [&](GameState& s) { order.push_back(300 + s.state()); };
    CHECK(g.change_step(20));
    CHECK(g.state() == kIntro);
    CHECK_FALSE(g.change_pending());
    // teardown of the old state, then seg2:0x1ac8 with the new one, then its entry
    CHECK(order == std::vector<int>{104, 200, 300});
}

TEST_CASE("state: a requested 1 becomes 3 in the same state-change step") {
    GameState g;
    bool wave_setup = false, briefing = false;
    g.hooks().enter[kWave] = [&](GameState& s) {
        wave_setup = true;
        CHECK(s.state() == kWave);
    };
    g.hooks().enter[kBriefing] = [&](GameState&) { briefing = true; };
    g.set_suspended(true);
    g.request(kWave);
    CHECK(g.change_step(1000));
    CHECK(wave_setup);
    CHECK(briefing);
    CHECK(g.state() == kBriefing);
    CHECK(g.briefing_phase() == 0);
    CHECK_FALSE(g.suspended());  // seg10:0x2e clears DGROUP:0x1a
}

TEST_CASE("state: the suspended tick does nothing") {
    GameState g;
    g.set(kGameOver);
    g.set_suspended(true);
    CHECK_FALSE(g.pass(1u << 30));
    CHECK_FALSE(g.change_pending());
    CHECK(g.state() == kGameOver);
}

TEST_CASE("state 2: deadlines and ESC") {
    GameState g;
    int nav = 0, briefings = 0;
    g.hooks().open_navigation = [&](GameState&) { ++nav; };
    g.hooks().enter[kBriefing] = [&](GameState&) { ++briefings; };
    g.enter_debriefing(false);
    CHECK(g.state() == kDebriefing);

    // nothing armed: nothing happens
    CHECK(g.pass(1000));
    CHECK(g.state() == kDebriefing);

    // DGROUP:0x1e74 due: Navigation, the state stays 2, the deadline stays set, no frame
    g.deadlines().debrief_nav = 1500;
    CHECK(g.pass(1499));
    CHECK(nav == 0);
    CHECK_FALSE(g.pass(1500));
    CHECK(nav == 1);
    CHECK(g.deadlines().debrief_nav == 1500);
    CHECK(g.state() == kDebriefing);

    // ESC opens Navigation too; other keys and key-down do nothing
    g.deadlines().debrief_nav = 0;
    g.post_input(key_up(kVkEscape));
    g.pass(1600);
    CHECK(nav == 2);
    g.post_input({kWmKeyDown, kVkEscape, 0});
    g.pass(1601);
    g.post_input(key_up(kVkSpace));
    g.pass(1602);
    CHECK(nav == 2);

    // DGROUP:0x1e70 due: state 3, the deadline cleared
    g.deadlines().debrief_next = 1700;
    g.pass(1700);
    CHECK(g.state() == kBriefing);
    CHECK(briefings == 1);
    CHECK(g.deadlines().debrief_next == 0);
}

TEST_CASE("state 3: ESC jumps to phase 18, which arms the end; then state 1") {
    GameState g;
    int waves = 0;
    g.hooks().enter[kWave] = [&](GameState&) { ++waves; };
    g.enter_briefing();
    g.pass(100);
    g.frame();
    CHECK(g.deadlines().briefing_end == 0);  // phase 0: not armed

    g.post_input(key_up(kVkSpace));
    g.pass(110);
    CHECK(g.briefing_skip() == -1);
    CHECK(g.briefing_phase() == 0);

    g.post_input(key_up(kVkEscape));
    g.pass(120);
    CHECK(g.briefing_phase() == kBriefingLastPhase);
    CHECK(g.state() == kBriefing);
    g.frame();  // the frame at phase 18 arms DGROUP:0x5ac = DGROUP:0x1e50
    CHECK(g.deadlines().briefing_end == 120);
    g.pass(121);  // due: state 1
    CHECK(g.state() == kWave);
    CHECK(waves == 1);
    CHECK(g.deadlines().briefing_end == 0);
}

TEST_CASE("state 4: the 25 s deadline or ESC request state 0") {
    GameState g;
    g.pass(10000);
    g.enter_game_over();
    CHECK(g.state() == kGameOver);
    CHECK(g.deadlines().game_over_end == 35000);
    CHECK(g.suspended());  // seg10:0x22e sets DGROUP:0x1a
    g.set_suspended(false);

    CHECK(g.pass(34999));
    CHECK_FALSE(g.change_pending());
    CHECK_FALSE(g.pass(35000));
    CHECK(g.change_pending());
    CHECK(g.requested() == kIntro);
    g.change_step(35001);
    CHECK(g.state() == kIntro);

    GameState h;
    h.enter_game_over();
    h.set_suspended(false);
    h.deadlines().game_over_end = 1u << 30;
    h.post_input(key_up(kVkEscape));
    CHECK_FALSE(h.pass(100));
    CHECK(h.requested() == kIntro);
}

TEST_CASE("state 4: seg9:0xd6c with a request pending gives no state 4") {
    GameState g;
    g.set(kWave);
    g.request(kIntro);
    g.enter_game_over();
    CHECK(g.state() == kWave);
    CHECK(g.suspended());
    CHECK(g.deadlines().game_over_end == 0);
}

TEST_CASE("state 5: 34 s, then state 4 with a request for 4; ESC without the request") {
    GameState g;
    int saved = 0;
    g.hooks().save_scores = [&](GameState&) { ++saved; };
    g.pass(2000);
    g.enter_high_scores(false);
    CHECK(g.state() == kHighScores);
    CHECK(g.deadlines().scores_end == 36000);
    CHECK(g.pass(35999));
    CHECK_FALSE(g.pass(36000));
    CHECK(g.state() == kGameOver);  // seg9:0xd6c in the same pass
    CHECK(saved == 1);
    CHECK_FALSE(g.suspended());  // seg7:0x441
    CHECK(g.change_pending());
    CHECK(g.requested() == kGameOver);
    CHECK(g.deadlines().game_over_end == 36000 + kGameOverMs);

    GameState h;
    h.pass(0);
    h.enter_high_scores(false);
    h.post_input(key_up(kVkEscape));
    h.pass(10);
    CHECK(h.state() == kGameOver);
    CHECK_FALSE(h.change_pending());
    CHECK_FALSE(h.suspended());
}

TEST_CASE("state 9: DGROUP:0x1e84 leads to state 5; states 6, 8, 9 pin the window size") {
    GameState g;
    g.set(kEnding);
    CHECK(g.pass(100));
    CHECK_FALSE(g.resizable());
    g.deadlines().scores_due = 200;
    g.pass(200);
    CHECK(g.state() == kHighScores);
    CHECK(g.deadlines().scores_due == 0);
    CHECK(g.deadlines().scores_end == 200 + kHighScoresMs);
    g.pass(201);
    CHECK(g.resizable());

    for (int s : {kAmbulance, kJustice}) {
        GameState h;
        h.set(s);
        h.pass(5);
        CHECK_FALSE(h.resizable());
    }
}

TEST_CASE("state 9: DGROUP:0x1e84 is one word with the wave's (bind_scores_due, triage D-24)") {
    // seg7:0x1c5 (state 9) and seg7:0x6ac (the wave) read the same DGROUP:0x1e84: bound to the
    // World's word (world/hooks.cpp), a write on either side is the other's read (M4 s5 fixer).
    Tick word = 0;
    GameState g;
    g.bind_scores_due(&word);
    g.set(kEnding);
    word = 300;  // the wave's death path arms it (seg12:0x2ac1)
    CHECK(g.scores_due() == 300);
    CHECK(g.deadlines().scores_due == 0);  // the unbound store is not used
    g.pass(299);
    CHECK(g.state() == kEnding);
    g.pass(300);
    CHECK(g.state() == kHighScores);
    CHECK(word == 0);  // seg7:0x1d8 cleared the bound word
    // Unbound (null) again: the GameState's own deadline.
    g.bind_scores_due(nullptr);
    g.deadlines().scores_due = 77;
    CHECK(g.scores_due() == 77);
    word = 5;
    CHECK(g.scores_due() == 77);
}

TEST_CASE("state 1: a pass that routes pending input does nothing else") {
    GameState g;
    g.set(kWave);
    int routed = 0;
    g.hooks().route_input = [&](GameState&, const Input& in) {
        ++routed;
        CHECK(in.wparam == 'W');
    };
    g.post_input(key_up('W'));
    CHECK_FALSE(g.pass(10));
    CHECK(routed == 1);
    CHECK(g.pass(11));
}

TEST_CASE("state: the abort flag requests state 0") {
    GameState g;
    g.set(kWave);
    g.set_abort(true);
    CHECK_FALSE(g.pass(10));
    CHECK_FALSE(g.abort_pending());
    CHECK(g.requested() == kIntro);
    CHECK(g.state() == kWave);  // seg9:0xd6c with DGROUP:0x13de = 1
    CHECK(g.suspended());
    g.change_step(11);
    CHECK(g.state() == kIntro);
    CHECK_FALSE(g.suspended());  // the state-0 entry clears DGROUP:0x1a
}

TEST_CASE("state: frame and repaint hooks by state") {
    GameState g;
    std::vector<int> frames, repaints;
    for (int s = 0; s < kStateCount; ++s) {
        g.hooks().frame[s] = [&frames, s](GameState&) { frames.push_back(s); };
        g.hooks().repaint[s] = [&repaints, s](GameState&) { repaints.push_back(s); };
    }
    for (int s = 0; s < kStateCount; ++s) {
        g.set(s);
        g.frame();
        g.repaint();
    }
    g.set(12);
    g.frame();
    g.repaint();
    CHECK(frames == std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 1});
    // 7 repaints the wave HUD; 9 and values above 8 repaint nothing
    CHECK(repaints == std::vector<int>{0, 1, 2, 3, 4, 5, 6, 1, 8});
}

TEST_CASE("deadlines end the pass: state 5's deadline with ESC pending requests 4, then ESC") {
    GameState g;
    int saved = 0;
    g.hooks().save_scores = [&](GameState&) { ++saved; };
    g.pass(1000);
    g.enter_high_scores(false);
    g.post_input(key_up(kVkEscape));
    CHECK_FALSE(g.pass(1000 + kHighScoresMs));  // seg7:0x453: no handler, no frame
    CHECK(saved == 1);
    CHECK(g.state() == kGameOver);
    CHECK(g.requested() == kGameOver);  // seg7:0x44d, not overwritten by the ESC
    CHECK(g.has_pending_input());       // DGROUP:0x8e62 waits for the next pass
    CHECK(g.change_step(1000 + kHighScoresMs + 1));
    CHECK(g.state() == kGameOver);
    CHECK_FALSE(g.pass(1000 + kHighScoresMs + 2));  // state 4 handles the ESC
    CHECK_FALSE(g.has_pending_input());
    CHECK(g.requested() == kIntro);
}

TEST_CASE("deadlines end the pass: state 3 -> 1 keeps the input for the state-1 router") {
    GameState g;
    std::vector<int> routed_in;
    g.hooks().route_input = [&](GameState& s, const Input&) { routed_in.push_back(s.state()); };
    g.enter_briefing();
    g.deadlines().briefing_end = 50;
    g.post_input(key_up(kVkSpace));
    CHECK_FALSE(g.pass(60));  // seg7:0x323: the wave frame does not run in this pass
    CHECK(g.state() == kWave);
    CHECK(g.briefing_skip() == 0);  // the briefing handler did not see the SPACE
    CHECK(g.has_pending_input());
    CHECK_FALSE(g.pass(61));  // the router takes it
    CHECK(routed_in == std::vector<int>{kWave});
    CHECK_FALSE(g.has_pending_input());
    CHECK(g.pass(62));
}

TEST_CASE("deadlines end the pass: state 2's deadlines keep the input") {
    GameState g;
    int nav = 0;
    g.hooks().open_navigation = [&](GameState&) { ++nav; };
    g.enter_debriefing(false);
    g.deadlines().debrief_nav = 100;
    g.post_input(key_up(kVkEscape));
    CHECK_FALSE(g.pass(100));  // seg7:0x3b1: the Navigation deadline alone
    CHECK(nav == 1);
    CHECK(g.has_pending_input());

    GameState h;
    h.enter_debriefing(false);
    h.deadlines().debrief_next = 100;
    h.post_input(key_up(kVkSpace));
    CHECK_FALSE(h.pass(100));  // seg7:0x394: no state-3 frame in this pass
    CHECK(h.state() == kBriefing);
    CHECK(h.briefing_skip() == 0);
    CHECK(h.has_pending_input());
    CHECK(h.pass(101));  // the briefing handler takes the SPACE; no end armed yet
    CHECK(h.briefing_skip() == -1);
    CHECK_FALSE(h.has_pending_input());
}

TEST_CASE("deadlines end the pass: state 4 dispatches input only before its deadline") {
    GameState g;
    int routed = 0;
    g.hooks().route_input = [&](GameState&, const Input&) { ++routed; };
    g.pass(0);
    g.enter_game_over();
    g.set_suspended(false);
    g.post_input(key_up('A'));
    CHECK_FALSE(g.pass(kGameOverMs));  // seg7:0x408
    CHECK(g.requested() == kIntro);
    CHECK(g.has_pending_input());  // seg7:0x40b not reached
    g.change_step(kGameOverMs + 1);
    CHECK(g.state() == kIntro);
    g.pass(kGameOverMs + 2);  // the state-0 handler takes it
    CHECK(routed == 1);
    CHECK_FALSE(g.has_pending_input());
}

TEST_CASE("deadlines end the pass: state 9 -> 5 runs no state-5 frame in the same pass") {
    GameState g;
    g.set(kEnding);
    g.deadlines().scores_due = 200;
    CHECK_FALSE(g.pass(200));  // seg7:0x1e6
    CHECK(g.state() == kHighScores);
    CHECK(g.pass(201));
}

TEST_CASE("states 8 and 9 leave input pending: they have no handler") {
    for (int s : {kJustice, kEnding}) {
        GameState g;
        g.set(s);
        g.post_input(key_up(kVkEscape));
        g.pass(10);
        CHECK(g.has_pending_input());
    }
}
