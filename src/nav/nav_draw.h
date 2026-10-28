// The drawing rules of the Navigation window IS_NAV (docs/re/nav.md "Drawing the tree",
// "Drawing the file rows", "Preview", "Status line"): what the owner-draw routines seg6:0x47e8
// and seg6:0x49c0 put in a row, the status band and the preview frame of seg6:0x5b4c, the
// program-name placeholder seg9:0x352, and the port's own ExtractIcon for listed files (ADR 0012
// decision 5). Portable: the GDI calls are described as Ops that src/nav/nav.cpp executes, so
// tests read them without a display.
#pragma once

#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "formats/icon_source.h"
#include "nav/files.h"

namespace is::assets {
struct IconGroup;
}

namespace is::nav {

// ---- the directory rows (seg6:0x49c0)

// IGRAPHIC.DLL BITMAP FLDR: 15 x 11 cells, the AND mask at x 90 [BYTES IGRAPHIC.DLL BITMAP FLDR]
// [BYTES seg6:0x49c0].
constexpr int kFolderW = 15, kFolderH = 11, kFolderMaskX = 90;
// IGRAPHIC.DLL BITMAP FILES: 11 x 12, the image at x 0 and the mask at x 22 [BYTES IGRAPHIC.DLL
// BITMAP FILES] [BYTES seg6:0x47e8].
constexpr int kFileGlyphW = 11, kFileGlyphH = 12, kFileMaskX = 22;

// One line of the tree, MoveTo(x0, y0) and LineTo(x1, y1) with the DC's current pen.
struct Segment {
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    bool operator==(const Segment& o) const {
        return x0 == o.x0 && y0 == o.y0 && x1 == o.x1 && y1 == o.y1;
    }
};

// What seg6:0x49c0 draws for item `index` of listbox 0x65 with the text `item`, `count` items
// (LB_GETCOUNT) and `next` the text of item index + 1 (read only for depth > 1 and
// index < count - 1), in the item rectangle (left, top, bottom).
struct TreeRowLook {
    int depth = 0, sub = 0, state = 0;  // the suffix digits, read before row 0 loses its space
    int indent = 0;     // I = depth * 16 - 6, 0 for row 0 [BYTES seg6:0x4a4c]
    int glyph_x = 0;    // the FLDR cell: (sub = 1 ? 15 : 0) + {0, 30, 60, 105}[state] [BYTES seg6:0x4aed]
    int glyph_left = 0, glyph_top = 0;  // (left + I + 1, top + 1) [BYTES seg6:0x4b6b seg6:0x4b7f]
    int highlight_left = 0;             // the ODS_SELECTED fill starts at left + I [BYTES seg6:0x4b44]
    int text_x = 0, text_y = 0;         // (left + I + 18, top) [BYTES seg6:0x4d8c]
    std::string text;                   // the name without suffix, seg11:0x3fd4 mode 0
    std::vector<Segment> lines;         // the trunk, the depth line and the tick (rows other than 0)
};
TreeRowLook tree_row(const std::string& item, int index, int count, const std::string& next,
                     int left, int top, int bottom);
// The focus rectangle of seg6:0x4ea8 and seg6:0x4e04 starts at left + depth * 16 - 6 on listbox
// 0x65 (left for row 0) [BYTES seg6:0x4ea8 seg6:0x4e04].
int focus_indent(const std::string& item, int index);

// ---- the file rows (seg6:0x47e8, seg6:0x5cbc)
struct FileRowLook {
    std::string text;  // " n" and a leading space removed, seg11:0x3fd4 mode 0
    int digit = 0;     // the last character - '0'
    // The text colour before the FILES image goes on: 0x00FF0000 for digit 1, 0x000000FF for
    // digit 2, the DC's own otherwise [BYTES seg6:0x47e8].
    std::optional<std::uint32_t> glyph_colour;
    int glyph_left = 0, glyph_top = 0;  // (left + 1, top + 1)
    int text_x = 0, text_y = 0;         // (left + 15, top)
};
FileRowLook file_row(const std::string& item, int left, int top);

// ---- GDI descriptions

// One GDI call or group of calls. Colours are COLORREFs as the original passes them (0x02 in
// the top byte is PALETTERGB).
struct Op {
    enum class Kind {
        kLine,   // MoveTo(x0, y0), LineTo(x1, y1) with a 1-px solid pen `colour`
        kRect,   // Rectangle(x0, y0, x1, y1), pen `colour`, NULL_BRUSH or LTGRAY_BRUSH (`filled`)
        kPixel,  // SetPixel(x0, y0, colour)
        kFill,   // PatBlt(x0, y0, w, h) with a solid brush `colour` (BLACKNESS for 0)
        kText,   // CreateFont(height, width, 0, 0, weight, ..., face), SetTextAlign(align),
                 // SetTextColor(colour), background mode `opaque` with colour `bk`, TextOut
    };
    Kind kind = Kind::kLine;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // kFill: x0, y0, width x1, height y1
    std::uint32_t colour = 0;
    bool filled = false;
    std::string face;
    int height = 0, width = 0, weight = 0;
    unsigned align = 0;  // TA_LEFT 0, TA_RIGHT 2, TA_CENTER 6
    bool opaque = false;
    std::uint32_t bk = 0;
    std::string text;  // cp1252
};
constexpr unsigned kTaLeft = 0, kTaRight = 2, kTaCenter = 6;

// seg11:0x1774(width, l, t, r, b): the bevel; width -1 is 2 filled with LTGRAY_BRUSH [BYTES
// seg11:0x1774-0x1a7b] (shell.md "Screen states", State 3: the same routine as the briefing's).
std::vector<Op> bevel(int width, int l, int t, int r, int b);
// seg9:0x8bec(hdc, x, y, height, width, align, text): Arial weight 600, transparent; from
// height 14 a white copy at (x + 2, y + 2) under the 0x025A5A5A text; height 13 becomes 14
// without it [BYTES seg9:0x8bec-0x8d17].
std::vector<Op> label(int x, int y, int height, int width, unsigned align, const std::string& text);

// seg6:0x5b4c's status band in a client of cw x ch: the bevel (-1, 0, ch - 25, cw, ch) and the
// text cased by seg11:0x3fd4 mode 3 at (5, ch - 20), height 14 [BYTES seg6:0x5bb5 seg6:0x5bc3
// seg6:0x5be2].
std::vector<Op> status_band(int cw, int ch, const std::string& raw_status);
// The preview frame that only WM_PAINT draws: the bevel (4, cw - 96, 85, cw - 24, 157) and
// PatBlt(cw - 92, 89, 64, 64, BLACKNESS) [BYTES seg6:0x5bea seg6:0x5bf8 seg6:0x5c0c].
std::vector<Op> preview_frame(int cw);
// Where the 32 x 32 preview is stretched to: (cw - 92, 89, 64, 64) [BYTES seg6:0x5c2e seg6:0x5c72].
struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
    bool operator==(const Rect& o) const { return x == o.x && y == o.y && w == o.w && h == o.h; }
};
Rect preview_rect(int cw);

// seg9:0x352(hdc, x, flag, size, y, name) [BYTES seg9:0x352]: the program-name placeholder.
// The name loses a 4-character extension ('.' at length - 4, for names longer than 4) [BYTES
// seg9:0x37c-0x3a2]; the cell is blacked; flag 0 draws the frame and the "C:>" prompt (byte 0 +
// the drive offset DGROUP:0x94ac) in Arial 10, green, flag != 0 the boxed variant; both then
// write the name in "Small Fonts" 7, white, TA_CENTER, at (x + 16, y + 26), opaque on black,
// and black the y-wide strips left and right of the cell [BYTES seg9:0x3c2 seg9:0x5ce seg9:0x70e
// seg9:0x7e1 seg9:0x89c]. Navigation calls it with (0, 0, 32, 0) [BYTES seg6:0x455c].
std::vector<Op> placeholder(int x, int flag, int size, int y, const std::string& name, int drive_offset);

// ---- the preview's pixels

// A 32 x 32 cell over black from an ISPACE.DLL GROUP_ICON (105 'S', 106 'R', 107 'D'), as
// LoadIcon and DrawIcon put it into DGROUP:0x94ba [BYTES seg6:0x448c seg6:0x4498]: the member
// nearest 32 x 32 (the highest depth up to 8 bpp on a tie), every pixel its XOR colour, another
// size scaled by nearest neighbour [DESIGN ADR 0012 decision 5]. nullopt for an empty group.
std::optional<fmt::IconCell> group_cell(const assets::IconGroup& group);

// ---- the port's ExtractIcon for listed files (ADR 0012 decision 5)

// IconCounter over fmt::IconSource: count() is ExtractIcon(path, -1), has_icon() whether
// ExtractIcon(path, 0) returns a handle, cell() the icon drawn over black. The last few modules
// stay parsed, so one selection reads each file once [DESIGN].
class IconReader : public IconCounter {
public:
    // Reads the module at a DOS path (a file that cannot be read gives an empty source).
    using Load = std::function<fmt::IconSource(const std::string& dos_path)>;
    explicit IconReader(Load load);
    int count(const std::string& path) override;
    bool has_icon(const std::string& path) override;
    std::optional<fmt::IconCell> cell(const std::string& path);
    // The modules read so far (for tests).
    int loads() const { return loads_; }
    void forget() { cache_.clear(); }

private:
    fmt::IconSource& get(const std::string& path);
    Load load_;
    std::list<std::pair<std::string, fmt::IconSource>> cache_;  // most recent first
    int loads_ = 0;
};

}  // namespace is::nav
