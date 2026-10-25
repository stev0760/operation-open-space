#include "gfx/font.h"

#include <cstring>

#include "gfx/surface.h"

namespace is::gfx {

std::uint8_t font_cell(char ch) {
    const int c = static_cast<signed char>(ch);  // the sign-extended byte
    if (c >= '0' && c <= '9') return static_cast<std::uint8_t>(c - 0x0d);  // 35-44
    if (c >= 'a' && c <= 'z') return static_cast<std::uint8_t>(c - 0x61);  // case folded
    if (c >= 'A' && c <= 'Z') return static_cast<std::uint8_t>(c - 0x41);
    switch (c) {
    case ' ':
    case '_': return kCellBlank;
    case '!': return 30;
    case '%': return 45;
    case '(': return 27;
    case ')': return 28;
    case ',': return 32;
    case '-': return 34;
    case '.': return 33;
    case '/': return 26;  // the backslash glyph
    case ':': return 29;
    case '?': return 31;
    case '\\': return 26;
    default: return kCellSkip;
    }
}

int font_start_x(int align, int x, int len, int cell_w) {
    const int span = len * cell_w;
    switch (align) {
    case 0: return x;
    case 2: return x - span;
    case 6: return x - span / 2;  // signed, truncating
    default: return x;
    }
}

bool font_cell_size(int font, FontCell* out) {
    switch (font) {
    case 0: *out = {18, 20}; return true;
    case 1: *out = {12, 18}; return true;
    case 2: *out = {9, 10}; return true;
    default: return false;
    }
}

std::vector<GlyphPlace> font_layout(const std::string& text, int align, int x, int cell_w) {
    std::vector<GlyphPlace> out;
    int cx = font_start_x(align, x, static_cast<int>(std::strlen(text.c_str())), cell_w);
    for (char c : text) {
        if (c == '\0') break;  // lstrlen ends the text
        const std::uint8_t cell = font_cell(c);
        if (cell == kCellSkip) continue;
        out.push_back({cx, cell});
        cx += cell_w;
    }
    return out;
}

int bar_fill_width(int w, int value) {
    return static_cast<int>(static_cast<long>(value) * w / 100);
}

#ifdef _WIN32
void draw_font_text(HDC hdc, const Surface& alphabet, int cell_w, int cell_h, int align,
                    COLORREF brush, int x, int y, const std::string& text) {
    if (alphabet.empty()) return;
    HBRUSH b = CreateSolidBrush(brush);
    HGDIOBJ old_brush = SelectObject(hdc, b);
    // The original selects ALPHABET into its own memory DC; the port's bitmap stays in its DC.
    HDC mdc = alphabet.hdc();
    SetStretchBltMode(hdc, COLORONCOLOR);  // never restored [BYTES ISPACE.DLL seg8:0x3f2]
    for (const GlyphPlace& g : font_layout(text, align, x, cell_w)) {
        if (g.cell == kCellBlank)
            PatBlt(hdc, g.x, y, cell_w, cell_h, BLACKNESS);
        else
            StretchBlt(hdc, g.x, y, cell_w, cell_h, mdc, kGlyphWidth * g.cell, 0, kGlyphWidth,
                       kGlyphHeight, MERGECOPY);
    }
    DeleteObject(SelectObject(hdc, old_brush));
}

namespace {
void line(HDC hdc, COLORREF c, const POINT* pts, int n) {
    HPEN pen = CreatePen(PS_SOLID, 1, c);
    HGDIOBJ old = SelectObject(hdc, pen);
    MoveToEx(hdc, pts[0].x, pts[0].y, nullptr);
    for (int i = 1; i < n; ++i) LineTo(hdc, pts[i].x, pts[i].y);
    DeleteObject(SelectObject(hdc, old));
}
void fill(HDC hdc, HBRUSH br, int x, int y, int w, int h) {
    HGDIOBJ old = SelectObject(hdc, br);
    PatBlt(hdc, x, y, w, h, PATCOPY);
    SelectObject(hdc, old);
}
}  // namespace

void draw_frame(HDC hdc, int x, int y, int w, int h, int style, ColourFn rgb, const void* ctx) {
    // style: brush, pen 1 grey (right and bottom), pen 2 grey (top and left, -1 none)
    int fill_stock, g1, g2;
    switch (style) {
    case 0: fill_stock = BLACK_BRUSH; g1 = 0xc0; g2 = -1; break;
    case 1:
    case 2: fill_stock = LTGRAY_BRUSH; g1 = 0x00; g2 = 0xff; break;
    case 3: fill_stock = LTGRAY_BRUSH; g1 = 0xff; g2 = 0x80; break;
    default: return;  // the original uses uninitialised objects above 3 [DESIGN]
    }
    // The stock brushes are plain RGB colours: black, and RGB(192, 192, 192).
    const COLORREF fc = fill_stock == BLACK_BRUSH ? RGB(0, 0, 0) : RGB(0xc0, 0xc0, 0xc0);
    HBRUSH br = CreateSolidBrush(rgb(fc, ctx));
    fill(hdc, br, x, y, w, h);
    DeleteObject(br);
    const POINT p1[3] = {{x + w, y}, {x + w, y + h}, {x, y + h}};
    line(hdc, rgb(RGB(g1, g1, g1), ctx), p1, 3);
    if (g2 >= 0) {
        const POINT p2[3] = {{x + w, y}, {x, y}, {x, y + h}};
        line(hdc, rgb(RGB(g2, g2, g2), ctx), p2, 3);
    }
    if (style == 2) {
        const POINT p3[2] = {{x, y + h}, {x + w, y + h}};
        line(hdc, rgb(0x808080, ctx), p3, 2);
    }
}

void draw_bar(HDC hdc, int x, int y, int w, int h, int value, ColourFn rgb, const void* ctx) {
    const int fw = bar_fill_width(w, value);
    PatBlt(hdc, x - 2, y + h + 1, w + 4, 1, WHITENESS);
    PatBlt(hdc, x + w + 1, y - 2, 1, h + 3, WHITENESS);
    HBRUSH dark = CreateSolidBrush(rgb(RGB(0x40, 0x40, 0x40), ctx));  // DKGRAY_BRUSH
    fill(hdc, dark, x - 2, y - 2, 1, h + 3);
    fill(hdc, dark, x - 1, y - 2, w + 2, 1);
    DeleteObject(dark);
    PatBlt(hdc, x + fw, y, w - fw, h, BLACKNESS);
    if (fw == 0) return;
    const COLORREF green = rgb(0x0000FF00, ctx), yellow = rgb(0x0000FFFF, ctx),
                   dkgreen = rgb(0x00008000, ctx);
    HBRUSH b = CreateSolidBrush(green);
    fill(hdc, b, x + 3, y + 3, fw - 6, h - 6);
    DeleteObject(b);
    b = CreateSolidBrush(yellow);
    fill(hdc, b, x, y, fw, 3);
    fill(hdc, b, x, y, 3, h);
    DeleteObject(b);
    b = CreateSolidBrush(dkgreen);
    fill(hdc, b, x, y + h - 3, fw, 3);
    fill(hdc, b, x + fw - 3, y, 3, h);
    DeleteObject(b);
    SetPixel(hdc, x, y + h - 3, yellow);
    SetPixel(hdc, x, y + h - 2, yellow);
    SetPixel(hdc, x + 1, y + h - 3, yellow);
    SetPixel(hdc, x + fw - 3, y, yellow);
    SetPixel(hdc, x + fw - 2, y, yellow);
    SetPixel(hdc, x + fw - 3, y + 1, yellow);
}
#endif

}  // namespace is::gfx
