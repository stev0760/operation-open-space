// The screen-state machine of docs/re/shell.md "Screen states" and docs/re/globals.md
// "Game tick": the state DGROUP:0x90f4, the request pair DGROUP:0x968e / DGROUP:0x13de, the
// state-change step seg2:0x1d58, and the per-pass work of the tick seg7:0x0 that the shell
// owns (deadlines, the screen handlers' ESC and SPACE rules). The frame, repaint and entry
// work of each state is a hook; the stubs do nothing until later stages fill them. Portable.
#pragma once

#include <array>
#include <cstdint>
#include <functional>

#include "shell/frame_scheduler.h"

namespace is::shell {

// DGROUP:0x90f4 values (shell.md "Screen states").
enum StateId : int {
    kIntro = 0,       // intro, title and demo attract
    kWave = 1,        // the wave
    kDebriefing = 2,  // wave debriefing
    kBriefing = 3,    // wave mission briefing
    kGameOver = 4,    // GAME OVER
    kHighScores = 5,  // high-score table
    kAmbulance = 6,   // ambulance visit
    kHelp = 7,        // Online Help window
    kJustice = 8,     // Hall of Justice
    kEnding = 9,      // game-completion ending
};
constexpr int kStateCount = 10;

// Window message numbers that the state handlers test; Win16 and Win32 share them.
constexpr std::uint32_t kWmKeyDown = 0x100;
constexpr std::uint32_t kWmKeyUp = 0x101;
constexpr std::uint32_t kVkEscape = 0x1b;
constexpr std::uint32_t kVkSpace = 0x20;

// Pending input: DGROUP:0x8e60 = 1 with msg DGROUP:0x8e62, wParam DGROUP:0x8e64 and lParam
// DGROUP:0x1d38 [BYTES seg2:0x783 seg2:0x78c seg2:0x792 seg2:0x799].
struct Input {
    std::uint32_t msg = 0;
    std::uint32_t wparam = 0;
    std::int32_t lparam = 0;
};

// The wall-clock deadlines the shell's states test (0 = unset where the original tests for 0).
struct Deadlines {
    Tick debrief_next = 0;   // DGROUP:0x1e70: state 2 -> 3
    Tick debrief_nav = 0;    // DGROUP:0x1e74: state 2 opens Navigation
    Tick briefing_end = 0;   // DGROUP:0x5ac: state 3 -> 1
    Tick game_over_end = 0;  // DGROUP:0x1e78: state 4 -> request 0 (no zero test)
    Tick scores_end = 0;     // DGROUP:0x1e7c: state 5 -> 4 (no zero test)
    Tick scores_due = 0;     // DGROUP:0x1e84: state 9 (and the wave) -> 5
};

// The briefing's last phase, at which the frame arms DGROUP:0x5ac [BYTES seg17:0x33e].
constexpr int kBriefingLastPhase = 18;
// seg9:0xd6c: DGROUP:0x1e78 = now + 25000 [BYTES seg9:0xd91].
constexpr Tick kGameOverMs = 25000;
// seg6:0x5d58: DGROUP:0x1e7c = now + 34000 at its end [BYTES seg6:0x5f6b].
constexpr Tick kHighScoresMs = 34000;

class GameState {
public:
    using Hook = std::function<void(GameState&)>;
    using InputHook = std::function<void(GameState&, const Input&)>;

    // The work that later stages fill. Empty hooks are stubs that do nothing.
    struct Hooks {
        std::array<Hook, kStateCount> frame;    // the state's frame function (timing.md)
        std::array<Hook, kStateCount> repaint;  // seg11:0xafe's table entry
        // The state entry seg10:0x0 for states 0 and 1 (seg15:0x5786, the wave set-up),
        // and the drawing parts of the entry routines of states 2-5 (seg9:0x6cc4,
        // seg9:0x5cca, seg9:0xd6c, seg6:0x5d58).
        std::array<Hook, kStateCount> enter;
        Hook teardown;          // seg10:0x1fe, before a state change [BYTES seg2:0x1d7b]
        Hook play_area;         // the play-area and clip recomputation of seg2:0x1ac8
        Hook open_navigation;   // seg6:0x1c72(1): create or show IS_NAV
        Hook save_scores;       // seg6:0x626c, at the end of state 5 [BYTES seg7:0x43c]
        // The input handlers the shell does not own: state 0 seg11:0x7902, state 1 the router
        // seg11:0x6aa8, state 6 seg11:0x785e [BYTES seg11:0x6ab2-0x6b60].
        InputHook route_input;
    };

    GameState() = default;

    Hooks& hooks() { return hooks_; }

    // DGROUP:0x90f4.
    int state() const { return state_; }
    // A direct write of DGROUP:0x90f4, as most changes do (globals.md "Game state globals").
    void set(int s) { state_ = s; }
    // A request: DGROUP:0x968e = s, DGROUP:0x13de = 1 (globals.md "Game tick"). It applies in
    // the next state-change step, before the next tick.
    void request(int s) {
        requested_ = s;
        change_pending_ = true;
    }
    bool change_pending() const { return change_pending_; }
    int requested() const { return requested_; }

    // DGROUP:0x1a: while set, the tick returns at once [BYTES seg7:0x10].
    bool suspended() const { return suspended_; }
    void set_suspended(bool s) { suspended_ = s; }
    // DGROUP:0x22: while clear, WM_GETMINMAXINFO pins the window size and a double click
    // cannot switch modes (shell.md "Mode switching").
    bool resizable() const { return resizable_; }
    // DGROUP:0x8f6c: the 0x991 abort asks the next tick to request state 0.
    void set_abort(bool a) { abort_ = a; }
    bool abort_pending() const { return abort_; }

    // DGROUP:0x1e50, the tick sampled by the last pass.
    Tick now() const { return now_; }
    Deadlines& deadlines() { return deadlines_; }
    const Deadlines& deadlines() const { return deadlines_; }
    // DGROUP:0x93aa, the briefing phase.
    int briefing_phase() const { return briefing_phase_; }
    void set_briefing_phase(int p) { briefing_phase_ = p; }
    // DGROUP:0x9462: WM_KEYUP VK_SPACE in state 3 sets it to -1 [BYTES seg11:0x7bf7].
    int briefing_skip() const { return briefing_skip_; }
    // DGROUP:0x93a6: the Inner Demon visit flag; state 6's ESC needs it 0.
    bool demon_visit() const { return demon_visit_; }

    // The idle step's state-change step seg2:0x1d58: when a request is pending it tears the
    // old state down, takes the requested state, runs seg2:0x1ac8 (play area, state entry)
    // and seg2:0x1b96, which turns a new state 1 into state 3 (seg9:0xc06 -> seg9:0x5cca).
    // Returns true when a change ran; the idle step then runs no pass.
    bool change_step(Tick now);
    // seg6:0x1b3c, the 0x9d0 handler's start: seg2:0x1ac8 (play area, state entry) and
    // seg2:0x1b96 for the current state [BYTES seg6:0x1b46 seg6:0x1b56].
    void start(Tick now);
    // seg2:0x1d58 without a pending request: the teardown alone (exit path, step 2 of
    // shell.md "Exit" teardown).
    void teardown();

    // One tick pass without the frame (seg7:0x0): samples `now` into DGROUP:0x1e50 and runs
    // the abort check, the state's deadlines and its pending input. Returns false when the
    // pass ends early (suspended, a request made, a routed input in state 1, or a deadline
    // that fired) so that the caller runs no frame in it. A deadline that fires ends the pass
    // before the input handler; the input then waits for the next pass.
    bool pass(Tick now);
    // The state's frame function, when the scheduler says a frame is due.
    void frame();
    // The repaint routine seg11:0xafe: its table has no entry above 8 [BYTES seg11:0x16b6].
    void repaint();

    // Stores pending input for the next pass (WndProc with no frame running).
    void post_input(const Input& in) {
        pending_ = in;
        has_pending_ = true;
    }
    bool has_pending_input() const { return has_pending_; }

    // ---- the state entry routines (the writes and deadlines; their drawing is a hook)
    void enter_debriefing(bool draw);  // seg9:0x6cc4
    void enter_briefing();             // seg9:0x5cca
    void enter_wave();                 // seg9:0x60bc
    void enter_game_over();            // seg9:0xd6c
    void enter_high_scores(bool from_ending);  // seg6:0x5d58

private:
    void run(const Hook& h) {
        if (h) h(*this);
    }
    void state_entry();  // seg2:0x1ac8 and seg10:0x0
    // The screen handlers of states 2-5 (seg11:0x7b7a, seg11:0x7bbc, seg11:0x78bc,
    // seg11:0x7b30): WM_KEYUP with VK_ESCAPE, and VK_SPACE in state 3.
    void screen_input(const Input& in);
    // A pending input to the handler that consumes it: route_input, or screen_input.
    void route_pending();
    void screen_pending();

    Hooks hooks_;
    int state_ = kIntro;  // seg6:0x18f0 writes 0 in WM_CREATE
    int requested_ = 0;
    bool change_pending_ = false;
    bool suspended_ = false;
    bool resizable_ = true;  // DGROUP:0x22 starts as 1 [BYTES DGROUP:0x22]
    bool abort_ = false;
    Tick now_ = 0;
    Deadlines deadlines_;
    int briefing_phase_ = 0;
    int briefing_skip_ = 0;
    bool demon_visit_ = false;
    Input pending_;
    bool has_pending_ = false;
    bool from_ending_ = false;  // DGROUP:0x94cc
};

}  // namespace is::shell
