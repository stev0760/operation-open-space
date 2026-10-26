// Small rules of the original that more than one part of the port applies: the time of day of
// the greeting and the desk (seg10:0x9764), and the drive label texts of the briefing's
// greeting, the DRIVE list and the Analysing/Entering Drive lines (seg15:0x6558, ISPACE.DLL
// seg10:0x1e7c). One copy, so the parts cannot diverge. Portable.
#pragma once

#include <optional>
#include <string>

namespace is::shell {

// seg10:0x9764(0): 0 for hours 3-11, 1 for 12-17, 2 for 18-23, 3 for 0-2 [BYTES seg10:0x9790
// seg10:0x9797 seg10:0x979d seg10:0x97a7 seg10:0x97b2].
int day_part(int hour);

// The '.' rule of both label readers: when the name is longer than 7 characters and its
// character 8 is '.', that '.' is removed and the rest moves down one [BYTES ISPACE.DLL
// seg10:0x2197 seg10:0x219f seg10:0x21a9 seg10:0x20b8-0x20d4] [BYTES seg15:0x65cc seg15:0x65da].
std::string drop_label_dot(std::string name);
// seg15:0x6558's label (Analysing Drive, Entering Drive): a failed or empty find-first result
// is "Unknown_HD", then the '.' rule [BYTES seg15:0x65cc seg15:0x65da seg15:0x6602].
std::string label_text(const std::optional<std::string>& find_first_name);
// A Win32 volume label (GetVolumeInformation, up to 32 characters) as the Win16 find-first
// gives it: the first 8 characters, a '.', the next 3 [DESIGN].
std::optional<std::string> label_as_find_first(const std::string& win32_label);

}  // namespace is::shell
