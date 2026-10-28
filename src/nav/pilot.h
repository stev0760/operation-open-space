// The demo pilot seg6:0x1e68 (docs/re/nav.md "Demo pilot"; docs/re/shell.md "Game-start
// dialogs" item 12, **Demo pilot**): in the demo (DGROUP:0x95bc = 0) with PlayMode
// (DGROUP:0x9134) set, it walks the Navigation tree one row per pass and tries Enter on each,
// and the demo's end deletes demogame.sav. Portable: the window is a PilotTarget.
//
// Wiring (the contract with src/nav/nav.cpp):
//   - nav.cpp implements PilotTarget over its tree and Enter, and calls set_pilot_target() once
//     with an object that outlives every window (Enter may close the window inside step());
//   - seg6:0x1c72 calls pilot_open(now, tick) after creating the window, where the original
//     calls seg6:0x1e68(1) (PlayMode != 0 and DGROUP:0x95bc = 0) [BYTES seg6:0x1d6c seg6:0x1d82];
//   - the state-0 frame calls pilot_frame(tick) instead of the intro's frame while the window
//     exists, PlayMode != 0 and DGROUP:0x95bc = 0 [BYTES seg7:0x233 seg7:0x248]; src/dialogs/
//     hooks.cpp wraps the intro's frame hook for that.
// The DEMO and PLAY command lines' DemoType table belongs to the initial selection
// (tree.h initial_selection(), enter.h kDemoType*).
#pragma once

#include <cstdint>
#include <string>

namespace is::nav {

// What seg6:0x1e68 reads and does in the window.
class PilotTarget {
public:
    virtual ~PilotTarget() = default;
    // seg6:0x301a: 1 when Enter accepted the selection (and closed the window).
    virtual int enter() = 0;
    // DGROUP:0x94b6, the row count (never decremented by a collapse).
    virtual int count() = 0;
    // DGROUP:0x94b4, the selected row.
    virtual int selection() = 0;
    // seg6:0x5260 on the selected row: its sub digit (1 collapsed with subdirectories).
    virtual int sub_digit() = 0;
    // seg6:0x4f46(1): expands the selected row.
    virtual void expand() = 0;
    // DGROUP:0x94b4 = row, LB_SETCURSEL (0x407 in Win16) on listbox 0x65 and seg6:0x5318.
    virtual void select(int row) = 0;
};

class Pilot {
public:
    // seg6:0x1e68(arg) with `now` = GetCurrentTime() and `tick` = DGROUP:0x1e50:
    //   1. arg != 0 sets DGROUP:0x1c34 = now + 1200 [BYTES seg6:0x1e7b];
    //   2. DGROUP:0xd26 = 0 [BYTES seg6:0x1e8d];
    //   3. when DGROUP:0x1c34 < tick (unsigned 32 bits), DGROUP:0x1c34 = tick + 600 and Enter
    //      [BYTES seg6:0x1e9b seg6:0x1ea2 seg6:0x1eb0];
    //   4. when Enter fails and the selection is below the count (signed), it expands the
    //      selected row when its sub digit is 1 and selects the next row; otherwise
    //      DGROUP:0xd26 = 1 [BYTES seg6:0x1ecb seg6:0x1ed8 seg6:0x1ede seg6:0x1f04];
    //   5. after the attempt, DGROUP:0xd26 = 1 when count - 1 <= selection [BYTES seg6:0x1f0a
    //      seg6:0x1f14].
    void step(bool arg, std::uint32_t now, std::uint32_t tick, PilotTarget& t);

    // DGROUP:0xd26: the pilot reached the last row (read by the demo's end).
    bool done() const { return done_; }
    // DGROUP:0x1c34: the tick of the next attempt.
    std::uint32_t next() const { return next_; }

private:
    std::uint32_t next_ = 0;
    bool done_ = false;
};

// The program's pilot and its window.
Pilot& pilot();
void set_pilot_target(PilotTarget* t);
// seg6:0x1e68(1) from seg6:0x1c72. False when no target is set.
bool pilot_open(std::uint32_t now, std::uint32_t tick);
// seg6:0x1e68(0) from the state-0 frame. False when no target is set (the caller then runs
// the intro's frame) [DESIGN].
bool pilot_frame(std::uint32_t tick);

// ---- the demo's end (seg7:0x610-0x64d, in the state-0 tick)

// The file seg6:0x92b8 deletes: LocalPath (DGROUP:0x9078) + Games (DGROUP:0x96b8, with the
// '\' seg6:0x6d8 appends) + "demogame.sav" (DGROUP:0xfed), OpenFile(OF_DELETE) [BYTES
// seg6:0x92b8 seg6:0x92fe DGROUP:0xfed]. The port's LocalPath is ADR 0004 decision 5's
// directory, and the delete stays inside it [DESIGN ADR 0012 decision 6].
constexpr const char* kDemoSave = "demogame.sav";
std::string demo_save_path(const std::string& local_path, const std::string& games);

// When the demo timer DGROUP:0x1e80 expires (non-zero and <= the tick): in the demo
// (DGROUP:0x95bc = 0), with the current directory record complete (+0x1d0 = 2) and the pilot
// done (DGROUP:0xd26 != 0), seg6:0x92b8 deletes demogame.sav [BYTES seg7:0x610 seg7:0x622 seg7:0x62b seg7:0x63e seg7:0x646
// seg7:0x64d].
struct DemoEnd {
    std::uint32_t timer = 0;   // DGROUP:0x1e80
    std::uint32_t tick = 0;    // DGROUP:0x1e50
    bool player = false;       // DGROUP:0x95bc != 0
    bool record_complete = false;
    bool pilot_done = false;   // DGROUP:0xd26
};
bool deletes_demo_save(const DemoEnd& e);

}  // namespace is::nav
