#include "shell/game_state.h"

namespace is::shell {

namespace {
// The original compares its dword deadlines with the sampled tick unsigned, so a deadline of
// 0 is always passed where the code has no zero test (DGROUP:0x1e78, DGROUP:0x1e7c).
bool passed(Tick now, Tick deadline) { return now >= deadline; }
bool armed_and_passed(Tick now, Tick deadline) { return deadline != 0 && now >= deadline; }
}  // namespace

void GameState::teardown() { run(hooks_.teardown); }  // seg10:0x1fe [BYTES seg2:0x1d7b]

void GameState::state_entry() {
    run(hooks_.play_area);  // seg2:0x1ac8 [BYTES seg2:0x1aea-0x1b5d]
    // seg10:0x0: state 0 calls seg15:0x5786, which clears DGROUP:0x1a [BYTES seg10:0x1a
    // seg15:0x57fc]; state 1 clears it and sets the wave up [BYTES seg10:0x2e].
    if (state_ == kIntro || state_ == kWave) {
        suspended_ = false;
        run(hooks_.enter[state_]);
    }
}

bool GameState::change_step(Tick now) {
    if (!change_pending_) return false;
    now_ = now;
    teardown();
    change_pending_ = false;  // seg2:0x1db0
    state_ = requested_;      // seg2:0x1db9
    state_entry();            // seg2:0x1dbd; seg2:0x1ac8 returns 1 on every path
    if (state_ == kWave) enter_briefing();  // seg2:0x1bce -> seg9:0xc06 -> seg9:0x5cca
    return true;
}

void GameState::start(Tick now) {
    now_ = now;
    state_entry();
    if (state_ == kWave) enter_briefing();  // seg2:0x1bce
}

void GameState::enter_debriefing(bool draw) {
    state_ = kDebriefing;  // seg9:0x6ce5
    deadlines_.debrief_nav = 0;  // seg9:0x6ce1
    if (draw) run(hooks_.enter[kDebriefing]);
}

void GameState::enter_briefing() {
    state_ = kBriefing;   // seg9:0x5d19
    briefing_phase_ = 0;  // seg9:0x8934 starts seg17:0x0 at phase 0 [BYTES seg9:0x8bcb]
    run(hooks_.enter[kBriefing]);
}

void GameState::enter_wave() {
    state_ = kWave;  // seg9:0x60ff
    run(hooks_.enter[kWave]);
}

void GameState::enter_game_over() {
    // seg9:0xd6c: state 4 and its deadline only while no request is pending
    // [BYTES seg9:0xd86 seg9:0xd97 seg9:0xd9b]; seg10:0x22e always sets DGROUP:0x1a
    // [BYTES seg10:0x23c].
    if (!change_pending_) {
        deadlines_.game_over_end = now_ + kGameOverMs;
        state_ = kGameOver;
        run(hooks_.enter[kGameOver]);
    }
    suspended_ = true;
}

void GameState::enter_high_scores(bool from_ending) {
    from_ending_ = from_ending;  // DGROUP:0x94cc
    deadlines_.scores_due = 0;   // seg7:0x1d8
    state_ = kHighScores;        // seg6:0x5dbd
    deadlines_.scores_end = now_ + kHighScoresMs;  // seg6:0x5f6b
    run(hooks_.enter[kHighScores]);
}

void GameState::screen_input(const Input& in) {
    if (in.msg != kWmKeyUp) return;
    switch (state_) {
    case kDebriefing:  // seg11:0x7ba1: ESC opens Navigation, the state stays 2
        if (in.wparam == kVkEscape) run(hooks_.open_navigation);
        break;
    case kBriefing:
        if (in.wparam == kVkEscape) briefing_phase_ = kBriefingLastPhase;  // seg11:0x7bef
        if (in.wparam == kVkSpace) briefing_skip_ = -1;                    // seg11:0x7bf7
        break;
    case kGameOver:  // seg11:0x78e7
        if (in.wparam == kVkEscape) request(kIntro);
        break;
    case kHighScores:  // seg11:0x7b55: as the deadline, without the request
        if (in.wparam == kVkEscape) {
            enter_game_over();
            run(hooks_.save_scores);
            suspended_ = false;  // seg11:0x7b5f
        }
        break;
    default: break;
    }
}

bool GameState::pass(Tick now) {
    if (suspended_) return false;  // seg7:0x10
    now_ = now;                    // seg7:0x1f
    resizable_ = true;             // seg7:0x19

    if (abort_) {  // seg7:0xd6-0xf4: the pass goes on into the state's case
        request(kIntro);
        abort_ = false;
        deadlines_.scores_due = 0;
        enter_game_over();  // with DGROUP:0x13de = 1: no state 4
    }

    // Every deadline branch ends the pass at once (jmp seg7:0x1192): no input handler and no
    // frame run in it, and the pending input (DGROUP:0x8e62) waits for the next pass. Only the
    // handlers consume it (route_pending, screen_pending).
    switch (state_) {
    case kIntro:  // seg7:0x217: seg11:0x7902
        route_pending();
        break;
    case kDebriefing:  // seg7:0x363
        if (armed_and_passed(now, deadlines_.debrief_next)) {  // seg7:0x386
            deadlines_.debrief_next = 0;
            enter_briefing();
            return false;  // seg7:0x394
        }
        if (armed_and_passed(now, deadlines_.debrief_nav)) {  // seg7:0x3ac
            run(hooks_.open_navigation);  // DGROUP:0x1e74 stays set
            return false;                 // seg7:0x3b1 via seg7:0x2c8
        }
        screen_pending();  // seg7:0x3c8
        break;
    case kBriefing:  // seg7:0x2cc
        if (armed_and_passed(now, deadlines_.briefing_end)) {  // seg7:0x2df
            deadlines_.briefing_end = 0;
            enter_wave();  // seg7:0x2e8
            return false;  // seg7:0x323
        }
        screen_pending();  // seg7:0x33a
        break;
    case kGameOver:  // seg7:0x3f1
        if (passed(now, deadlines_.game_over_end)) {  // seg7:0x3fa
            request(kIntro);                         // seg7:0x402
            return false;                            // seg7:0x408
        }
        screen_pending();  // seg7:0x40b-0x421: only while the deadline has not passed
        break;
    case kHighScores:  // seg7:0x42c
        if (passed(now, deadlines_.scores_end)) {  // seg7:0x435
            enter_game_over();                     // seg7:0x437
            run(hooks_.save_scores);
            suspended_ = false;
            request(kGameOver);  // seg7:0x44d
            return false;        // seg7:0x453
        }
        screen_pending();  // seg7:0x46a
        break;
    case kAmbulance:  // seg7:0xff
        resizable_ = false;
        // seg11:0x785e acts on ESC only while DGROUP:0x93a6 = 0; its work is not the shell's.
        route_pending();  // seg7:0x119
        break;
    case kJustice:  // seg7:0x197: no input handler
        resizable_ = false;
        break;
    case kEnding:  // seg7:0x1bf: no input handler
        resizable_ = false;
        if (armed_and_passed(now, deadlines_.scores_due)) {  // seg7:0x1d6
            enter_high_scores(true);                          // seg7:0x1e1
            return false;                                     // seg7:0x1e6
        }
        break;
    default:  // 1, 7 and values outside 0-9: seg7:0x4b0
        if (has_pending_) {
            route_pending();
            return false;  // a pass that routes pending input does nothing else
        }
        break;
    }
    return !change_pending_ && !suspended_;
}

void GameState::route_pending() {
    if (!has_pending_) return;
    // The router seg11:0x6aa8 and the handlers seg11:0x7902 and seg11:0x785e clear
    // DGROUP:0x8e62 on every path [BYTES seg11:0x6b54 seg11:0x7b22 seg11:0x78ae].
    has_pending_ = false;
    if (hooks_.route_input) hooks_.route_input(*this, pending_);
}

void GameState::screen_pending() {
    if (!has_pending_) return;
    // seg11:0x7b7a, seg11:0x7bbc, seg11:0x78bc and seg11:0x7b30 clear DGROUP:0x8e62 on every
    // path [BYTES seg11:0x7bae seg11:0x7c04 seg11:0x78f4 seg11:0x7b6c].
    has_pending_ = false;
    screen_input(pending_);
}

void GameState::frame() {
    if (state_ >= 0 && state_ < kStateCount) {
        run(hooks_.frame[state_]);
    } else {
        run(hooks_.frame[kWave]);
    }
    // The briefing frame seg17:0x0 arms DGROUP:0x5ac when the phase reaches 18 with
    // DGROUP:0x5ac = 0 [BYTES seg17:0x332 seg17:0x33e]. The phases before it are the frame
    // hook's (stage 3); the arming is kept here so that ESC ends the briefing already.
    if (state_ == kBriefing && briefing_phase_ == kBriefingLastPhase &&
        deadlines_.briefing_end == 0)
        deadlines_.briefing_end = now_;
}

void GameState::repaint() {
    switch (state_) {
    case kWave:
    case kHelp: run(hooks_.repaint[kWave]); break;  // both the wave HUD [BYTES seg11:0x15b3]
    case kEnding: break;                            // no entry above 8 [BYTES seg11:0x16b6]
    default:
        if (state_ >= 0 && state_ < kEnding) run(hooks_.repaint[state_]);
        break;
    }
}

}  // namespace is::shell
