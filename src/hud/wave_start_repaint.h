// The wave start's extra repaint of the status display (ADR 0013 decision 3, "The full status
// display from the first frame of the wave"): the post of 0x9aa at the wave start
// (seg13:0x66af, hud/icon_column.h icon_column_wave_start) marks it due; the IS_ICON creation
// that post causes takes the mark and repaints after the reactivation's erase when the column
// was made and the game is in state 1 or 7 [DESIGN ADR 0013 decision 3] [ORACLE win98-0001].
// Any other end of the column (a destroy, a post that made no column) drops the mark, so a later
// Alt+I creation keeps the erase alone [DESIGN]. Portable.
#pragma once

#include "shell/game_state.h"

namespace is::hud {

class WaveStartRepaint {
public:
    // seg13:0x66af posted 0x9aa.
    void posted() { due_ = true; }
    // The column's creation: whether to run the paint routine once more. Clears the mark.
    bool take(bool created, int state) {
        const bool due = due_;
        due_ = false;
        return due && created && (state == shell::kWave || state == shell::kHelp);
    }
    // The column destroyed, or the wave states left.
    void drop() { due_ = false; }
    bool due() const { return due_; }

private:
    bool due_ = false;
};

}  // namespace is::hud
