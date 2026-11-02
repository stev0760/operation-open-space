// The string formatter seg10:0xa332(buf, id, mode, ...) over an ENG_LANG.DLL string, with every
// directive the listing has [BYTES seg10:0xa332-0xa852] (read for M4 stage 1 from
// work/analysis/m4/s1-end-impl/seg10_0xa332.md). Portable.
//
// The directives, each taking its arguments in order whether or not its group is on:
//   %<  takes a flag; the group is on while it is non-zero. An enabled %< looks ahead for the
//       next "%<" or "%>" (other directives in between are passed over): before a following
//       "%<" it writes ", ", before "%>" string 797 " AND" and a space, but only when an
//       enabled group of the same run came before (the count c > 0); then c + 1
//       [BYTES seg10:0xa5f8-0xa6a0].
//   %>  ends the run: on again, c = 0 [BYTES seg10:0xa5ec].
//   %#  takes (n, id): n in decimal (seg6:0xf1a), a space, string id for n = 1, else id + 10000
//       [BYTES seg10:0xa558-0xa5e7]. %d takes n alone.
//   %?  takes (flag, id0, id1): string id1 when flag >= 1, else id0 [BYTES seg10:0xa77c-0xa7a4].
//   %S %A %V take a string id; a trailing '^' is dropped [BYTES seg10:0xa4c9].
//   %s %a %v take a string; %l a dword in decimal (seg6:0xeb8); %% a '%'.
//   %N %n   CR LF, except in briefing phase 11 (DGROUP:0x93aa = 0xb) [BYTES seg10:0xa6e3-0xa701].
//   %R  takes (m, base): string rand() % m + base [BYTES seg10:0xa7b4].
//   %D  takes (pool, rec, type): the object name of seg3:0x1542 (the caller supplies it as text,
//       FArg::s, since the name tables are the world's) [BYTES seg10:0xa754].
//   Any other %x skips three characters and takes nothing [BYTES seg10:0xa42f].
//   '^' before P, F, M or N is dropped with that letter (the caret codes) [BYTES seg10:0xa3d9].
// Mode 0 appends to `buf`, modes 1 and 2 overwrite it; mode 2's split into the two message lines
// (seg3:0x3498) is the caller's [BYTES seg10:0xa37b seg10:0xa831].
//
// The loop of the original runs one character past the string's end when the last directive
// ends exactly there, writing the string's NUL into the buffer, so the text that follows it in
// the buffer is invisible to lstrlen; the port stops at the end [DESIGN].
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace is::briefing {

// One vararg word, dword or far string.
struct FArg {
    long v = 0;
    std::string s;
    bool is_string = false;
    FArg(int x) : v(x) {}
    FArg(long x) : v(x) {}
    FArg(unsigned long x) : v(static_cast<long>(x)) {}
    FArg(std::string x) : s(std::move(x)), is_string(true) {}
    FArg(const char* x) : s(x), is_string(true) {}
};

using LangLookup = std::function<std::string(int)>;
// `phase` is DGROUP:0x93aa, `rand` the game's rand() for %R (unset: 0).
std::string lang_format(const std::string& fmt, const std::vector<FArg>& args, const LangLookup& lang,
                        int phase = 0, const std::function<int()>& rand = {});
// seg10:0xa332(buf, id, mode, ...): LoadString(id) formatted, appended (mode 0) or replacing.
void lang_format_into(std::string& buf, int id, int mode, const std::vector<FArg>& args,
                      const LangLookup& lang, int phase = 0, const std::function<int()>& rand = {});

}  // namespace is::briefing
