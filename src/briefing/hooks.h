// What the game-start flow hands the briefing (Win32 side of src/briefing). The Navigation
// window's Enter (seg6:0x301a) stores the selected directory in DGROUP:0x38c8 before it
// requests state 1; the save loader sets the wave DGROUP:0x91f8. Until those exist, the
// briefing takes the INI Drive root and wave 0.
#pragma once

#include <functional>
#include <string>

namespace is::briefing {

// DGROUP:0x38c8: the directory the next wave enters, as the original writes it ("D:\GAMES\",
// cp1252, with a trailing backslash). Empty clears the selection.
void set_selection(const std::string& dos_path);
// DGROUP:0x92e8 (Demon's Gate), DGROUP:0x94e (race) and DGROUP:0x950 (duel) as the Navigation
// window's Enter left them (src/nav/enter.h Handover); the briefing entry picks its text by
// them (text_resource, seg9:0x5ee7-0x5f46). All 0 for an ordinary directory.
void set_handover(int demons_gate, int race, int duel);
// DGROUP:0x91f8 before the next briefing entry (0 for a new game).
void set_wave(int wave);
int wave();

struct DebriefBox;
// The debriefing box's drawing (seg9:0x6dec steps 4 and 5): src/hud sets it (hud::draw_debrief_box
// on the briefing's canvas); unset, the briefing's canvas draws it with the same routines
// seg11:0x1774 and seg9:0x8bec (M4 s1 END).
void set_debrief_box_hook(std::function<void(const DebriefBox&)> f);

}  // namespace is::briefing
