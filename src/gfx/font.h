// The game's bitmap font, ISPACE.DLL ordinal 33 (seg8:0x37a), with INSPACE's wrapper
// seg10:0x94c4, and the two drawing helpers ordinal 34 (frame, seg8:0x5ac) and ordinal 35 (bar,
// seg8:0x768), as docs/re/ispace.md "Ordinal notes" describes them. The character map, the
// advance and the start x are portable and host-tested; the drawing is Win32.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace is::gfx {

class Surface;

// IGRAPHIC.DLL BITMAP ALPHABET: one row of 46 cells of 18 x 20 [BYTES IGRAPHIC.DLL BITMAP
// ALPHABET].
constexpr int kGlyphWidth = 18, kGlyphHeight = 20, kGlyphCount = 46;
constexpr std::uint8_t kCellBlank = 0x84;  // ' ' and '_': a black box
constexpr std::uint8_t kCellSkip = 0xff;   // drawn as nothing, no advance

// seg8:0x42a-0x548: the cell of a byte of text [BYTES ISPACE.DLL seg8:0x548 seg8:0x54e
// seg8:0x562 seg8:0x42a seg8:0x434 seg8:0x444 seg8:0x44a seg8:0x47c seg8:0x48a seg8:0x491
// seg8:0x4c1 seg8:0x500].
std::uint8_t font_cell(char c);

// The text's alignment: 0 left, 2 right, 6 centred [BYTES ISPACE.DLL seg8:0x388 seg8:0x39c
// seg8:0x3a5 seg8:0x3ff seg8:0x410 seg8:0x414 seg8:0x422].
// len counts every byte (lstrlen), skipped ones included. Any other align leaves the
// original's x uninitialised; the port takes x [DESIGN].
int font_start_x(int align, int x, int len, int cell_w);

// The cell size of the wrapper seg10:0x94c4's fonts 0, 1 and 2: 18 x 20, 12 x 18, 9 x 10
// [BYTES seg10:0x94d6 seg10:0x94e0 seg10:0x94ea]. False for any other font, whose cell the
// original leaves uninitialised; the port then draws nothing [DESIGN].
struct FontCell {
    int w = 0, h = 0;
};
bool font_cell_size(int font, FontCell* out);

// One drawn cell: x, and the glyph cell or kCellBlank.
struct GlyphPlace {
    int x = 0;
    std::uint8_t cell = 0;
};
// The cells ordinal 33 draws for a cp1252 text, left to right.
std::vector<GlyphPlace> font_layout(const std::string& text, int align, int x, int cell_w);

// Ordinal 35's filled width fw = value * w / 100 (signed, long) [BYTES ISPACE.DLL seg8:0x783].
int bar_fill_width(int w, int value);

#ifdef _WIN32
// Ordinal 33 on `hdc` with the font bitmap `alphabet` (a monochrome Surface; nothing is drawn
// when it is empty, where the original reads a zero handle): per cell a StretchBlt of glyph
// (18 * cell, 0, 18, 20) to (cx, y, cell_w, cell_h) with MERGECOPY and a solid brush of `brush`
// (a COLORREF the DC resolves; pass it through Gfx::dc_colour), PatBlt BLACKNESS for blanks,
// SetStretchBltMode(hdc, COLORONCOLOR) left set [BYTES ISPACE.DLL seg8:0x3b1-0x59d].
void draw_font_text(HDC hdc, const Surface& alphabet, int cell_w, int cell_h, int align,
                    COLORREF brush, int x, int y, const std::string& text);
// Ordinal 34: the bevelled frame of style 0-3 [BYTES ISPACE.DLL seg8:0x5ac-0x71a]. `rgb`
// resolves a plain RGB COLORREF for the DC (Gfx::dc_colour).
using ColourFn = COLORREF (*)(COLORREF, const void* ctx);
void draw_frame(HDC hdc, int x, int y, int w, int h, int style, ColourFn rgb, const void* ctx);
// Ordinal 35: the bevelled bar filled to value percent [BYTES ISPACE.DLL seg8:0x768-0xa26].
void draw_bar(HDC hdc, int x, int y, int w, int h, int value, ColourFn rgb, const void* ctx);
#endif

}  // namespace is::gfx
