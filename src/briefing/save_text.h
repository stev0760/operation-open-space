// The texts that describe a saved game from its header (docs/re/formats.md "Header", shell.md
// "Game-start dialogs" item 3): INSPACE.EXE export 13, seg10:0xa118(dst, header), which fills the
// Restore Game dialog's edit 330, and the desk monitor's progress lines of seg15:0x52e4 after a
// restore. Both are pure functions of a copy of the 0x1a9 header bytes: export 13 writes its
// quotients back into its own buffer, which ISPACE re-reads from the file at each selection, so
// the write-back is not visible [DESIGN]. Portable (M4 stage 3, track SAVE).
#pragma once

#include <cstdint>
#include <string>

#include "briefing/lang_format.h"

namespace is::briefing {

// seg10:0xa118: strings 3000 (mode 1), 3001 (mode 0, when +0x15a or +0x15c is not 0), 3002 and,
// with the Online Help window open (DGROUP:0x9460 != 0), 3027. `header` holds 0x1a9 bytes;
// `pilot` is DGROUP:0x380c, compared with +0x04 for 3027 [BYTES seg10:0xa118-0xa321].
std::string restore_description(const std::uint8_t* header, const std::string& pilot,
                                const LangLookup& lang, bool help_window);

// The "COMPLETING ..." text of both: +0x160 = +0x32 → 2503 "IT" when +0x32 = 1, else 2501 "ALL
// OF THEM"; else +0x160 = 0 → 2502 "NONE OF THEM"; else 2500 "%d OF THEM" [BYTES
// seg10:0xa1ef-0xa219]. The desk's variant gives 2529 "SOME OF IT" for +0x160 = 0 with
// +0x32 <= 1 (verify claim 51, CORRECTED) [BYTES seg15:0x5521 seg15:0x5545-0x555d].
std::string completion_text(std::int16_t completed, std::int16_t dirs, const LangLookup& lang, bool desk);

// seg15:0x52e4's lines after the greeting of a restored game: 3000 with the %< flag 0 (mode 0),
// 3001 as export 13, then 3031 with (+0x164 > 400 signed, 311 + shape) (mode 0). The shape is
// reduced by 100 when >= 100, as export 13 [BYTES seg15:0x54c0-0x5631]. For the desk's port
// (src/desk/desk.cpp): the caller opens LocalPath + Games + Restore + ".sav" and
// passes its first 0x1a9 bytes; seg15:0x54af does not test the read [OPEN: a short file's
// stale buffer].
std::string desk_progress(const std::uint8_t* header, const LangLookup& lang);

}  // namespace is::briefing
