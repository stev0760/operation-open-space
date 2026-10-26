// What the game-start flow hands the briefing (Win32 side of src/briefing). The Navigation
// window's Enter (seg6:0x301a) stores the selected directory in DGROUP:0x38c8 before it
// requests state 1; the save loader sets the wave DGROUP:0x91f8. Until those exist, the
// briefing takes the INI Drive root and wave 0.
#pragma once

#include <string>

namespace is::briefing {

// DGROUP:0x38c8: the directory the next wave enters, as the original writes it ("D:\GAMES\",
// cp1252, with a trailing backslash). Empty clears the selection.
void set_selection(const std::string& dos_path);
// DGROUP:0x91f8 before the next briefing entry (0 for a new game).
void set_wave(int wave);
int wave();

}  // namespace is::briefing
