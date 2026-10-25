// The intro's texts and layout rules (docs/re/shell.md "Intro and title screen"): the string
// rules of the title, the version and trial lines, the text pages of phase 4, the key list of
// phase 5 and the title's positions relative to W/2, H/2, D and C. Portable and host-tested;
// the strings themselves come from the caller (cp1252, as the original stores them).
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace is::intro {

// LoadString into a buffer of `max` bytes: at most max - 1 bytes of the string [REF Win16 SDK
// LoadString].
std::string load_limit(const std::string& s, int max);

// seg11:0x3fd4(buf, mode): mode 0 lowers A-Z except the character right after a '(' (the name
// line); mode 1 raises a-z, mode 2 raises the first letter after a space, mode 3 the first
// after '.', '!', CR or LF; " I " keeps its capital. The compares are signed, so bytes from 0x80
// stay as they are [BYTES seg11:0x3fef seg11:0x4019 seg11:0x402b seg11:0x40d4].
std::string case_rule(std::string s, int mode);

// The title's name line: string 2338 (DGROUP:0x920c = 0) or 2337, at most 44 bytes, through
// seg11:0x3fd4 mode 0, plus DGROUP:0x1285 (0x99, the trade mark sign) [BYTES seg15:0x6e17
// seg15:0x6e41 seg15:0x6e52 DGROUP:0x1285].
std::string title_name_line(const std::string& name);

// seg15:0x681c: the template DGROUP:0x1277 "V0.00.00" with bytes 1, 3, 4 from INSPACE.EXE string
// 167 and bytes 6, 7 from string 233, cut after byte 7 [BYTES seg15:0x682c seg15:0x6840
// seg15:0x6848 seg15:0x6863 seg15:0x6872]. Missing bytes keep the template's.
std::string version_string(const std::string& s167, const std::string& s233);

// The edition line: the edition text (2339 or 2340, or empty for DGROUP:0x920c = 3) + " - "
// (DGROUP:0x1288) + the version + " " (DGROUP:0x128c) + INSPACE.EXE string 607 (at most 19 bytes)
// [BYTES seg15:0x6ece-0x6f52].
std::string edition_line(const std::string& edition, const std::string& version,
                         const std::string& s607);

// seg10:0x885e's day value: (tm_year - 93) * 365 + tm_yday [BYTES seg10:0x8a26 seg10:0x8a52].
int day_value(int tm_year, int tm_yday);
// DGROUP:0x9224: k = (P - V) with its low byte inverted, day = dayval - k + 1, 200 when that is
// below 1 [BYTES seg10:0x8a87 seg10:0x8a91 seg10:0x8a97 seg10:0x8aa0 seg10:0x8aa8]. With
// V = P = 0 it is dayval - 254.
int trial_day(int dayval, int vidsp, int pdev);

// wsprintf of a format with one %d (strings 2341 and 2342) [DESIGN: the port replaces the first
// "%d" and leaves any other text as it is].
std::string format_day(const std::string& format, int day);

// The title's trial line (shell.md "Trial line") for the edition DGROUP:0x920c, DGROUP:0x922a
// and the day DGROUP:0x9224, from the strings 2341, 2342 and 2343. `order_line` is the
// local [bp-0x12] that draws the Press 'R' line. `none` for DGROUP:0x920c = 3; -1 (the CD and
// registered-user line) is not reached by the port before stage 3 and gives `none` as well.
struct TrialLine {
    bool none = false;
    std::string text;
    bool order_line = false;
};
TrialLine trial_line(int edition, int invalid, int day, const std::string& s2341,
                     const std::string& s2342, const std::string& s2343);

// The copyright DGROUP:0x12d2, 56 bytes, cp1252 (0xA9 is the copyright sign) [BYTES
// DGROUP:0x12d2].
extern const char* const kCopyright;

// ---- phase 4: the text pages (seg15:0x6890)
// The lines of page `page` (DGROUP:0x95d6) of ENG_LANG.DLL #5000 136 or 200: the resource is
// cut at its first NUL, every CR ends a line and the next line starts two bytes after it (CR
// LF), the page starts at line 11 * page and shows 11 lines [BYTES seg15:0x6967 seg15:0x6972
// seg15:0x698e seg15:0x69ad seg15:0x69f3]. Lines past the end are empty [DESIGN: the original
// reads on past the resource there].
constexpr int kPageLines = 11;
std::array<std::string, kPageLines> text_page_lines(const std::vector<std::uint8_t>& res, int page);
// The resource id: 136 while DGROUP:0x920c is 0 or 1, else 200 [BYTES seg15:0x6939].
int text_page_resource(int edition);

// ---- phase 5: Basic Key Commands (seg15:0x6b52)
struct KeyRow {
    int key_id = 0;        // the key string, 1223 + i except rows 8-10
    int desc_id = 0;       // the description, 1323 + i except rows 8-10
    std::uint32_t colour;  // RGB(255 - 13 i, 0, 12 i + 11), a plain COLORREF
    int dy = 0;            // y relative to H/2: 30 (i - 5)
};
constexpr int kKeyRows = 11;
std::array<KeyRow, kKeyRows> key_rows();

// ---- phase 2: the title's positions (seg15:0x6cdc), with W = DGROUP:0x90ea, H = DGROUP:0x10,
// D = DGROUP:0x95de and C = DGROUP:0x95ce; every text is TA_CENTER | TA_TOP.
struct Point {
    int x = 0, y = 0;
    bool operator==(const Point& o) const { return x == o.x && y == o.y; }
};
struct TitleLayout {
    Point operation;  // 2121 "OPERATION:" (-90, D - 245)
    Point name;       // the name line (0, D - 220)
    Point edition;    // the edition line (0, D - 186)
    Point trial;      // the trial line (0, C + 125)
    Point order;      // Press 'R' (0, C + 150)
    Point hint;       // the 256-colour hint (0, C + 86), never drawn by the port (ADR 0007)
    Point copyright;  // (0, C + 103)
    Point esc;        // 2542 (0, C + 185)
    Point picture;    // the 276-wide picture's top left (-138, D - 155)
};
TitleLayout title_layout(int w, int h, int d, int c);
// DGROUP:0x95de at the title's entry: 40, 30 or 0 for H < 440, < 500 or larger [BYTES
// seg15:0x5c45 seg15:0x5c55 seg15:0x5c5d].
int title_offset(int h);

}  // namespace is::intro
