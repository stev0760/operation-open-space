// The drawing calls of the HUD (ADR 0013 decisions 1 and 4): the GDI calls seg11:0xafe, its
// panel routines and the IS_INFO paint seg11:0x238e make, on the window DC DGROUP:0x13dc (the
// ADR 0006 surface) and on the memory bitmaps of seg11:0x0 and seg11:0x344. The HUD logic is
// portable and draws only through this interface; src/hud/gdi_canvas_win32.cpp runs it on the
// surface, and the host tests record it. Portable.
#pragma once

#include <cstdint>
#include <string>

namespace is::hud {

// A COLORREF exactly as the original passes it: 0x02bbggrr is PALETTERGB, 0x00bbggrr a plain
// RGB. The Win32 canvas resolves it through gfx::Gfx::dc_colour (ADR 0007).
using Colour = std::uint32_t;

// The bitmaps the HUD draws to or from, by the DGROUP word that holds the original's handle.
enum class Img : int {
    kScreen = 0,  // DGROUP:0x13dc, the window DC (the surface)
    kMetal,       // DGROUP:0x5032: BITMAP METAL, 8 x 8 (the strip's pattern brush DGROUP:0x500a)
    kLamps,       // DGROUP:0x5034: graphic 250, 27 x 18, three 6-px lamp rows
    kButton,      // DGROUP:0x5036: graphic 240, 40 x 20, two 20 x 20 button cells
    kTiles,       // DGROUP:0x5044: graphic 45, 174 x 58, three 58 x 58 border tiles
    kFuelBar,     // DGROUP:0x503a: graphic 260, 14 x 60, the fuel column
    kDigital,     // DGROUP:0x5038: BITMAP DIGITAL, 140 x 10, 1 bpp, 10 x 10 glyphs
    kSystems,     // DGROUP:0x5030: BITMAP SYSTEMS, 32 x 77, seven 11-px label rows
    kDamage,      // DGROUP:0x503c: the damage panel, 84 x 61, composed by seg11:0x344
    kFuelPanel,   // DGROUP:0x503e: the fuel gauge background, 30 x 85, composed by seg11:0x344
    kWeapon,      // DGROUP:0x5040: the weapon picture of the weapon display
    // DGROUP:0x3a02 (image slot 13 of DGROUP:0x39e8): the icon strip the briefing entry built,
    // the source of seg13:0x5c22's pool-0 pictures (M3 stage 4).
    kIconStrip,
    // DGROUP:0x39e8[5]: the logo strip of seg10:0x39a8, the source of seg13:0x5c22's pool-7
    // pictures (the SD logo of "MESSAGE from / Ambulance") (M4 s1 SCREENS).
    kLogoStrip,
    // M4 stage 2 (DISPLAY): seg11:0x2ff6's sources, the weapon's sheet DGROUP:0x39e8[W(w)-4]
    // and the overlay graphic G (seg10:0x6c02(0xbf/0xc4/0xc5/0xbd, 0, -1)).
    kWeaponSheet,
    kWeaponOverlay,
    // seg13:0x5f49's source: the icon fragment's bitmap DGROUP:0x4e94[k] of the sub-cell being
    // drawn (Host::fragment loads it).
    kFragment,
    // seg13:0x5e05's graphic 500, the exit gate's picture of seg13:0x5c22 (Host::gate_picture
    // loads it) (M4 s4 fixer B).
    kGatePicture,
    // DGROUP:0x3a04 + 2r (image slot 14 + r of DGROUP:0x39e8): a burning icon's own image, the
    // source of seg13:0x5c22's pool-0 picture for slots 14 ... 21 (hud/icon_images.h) [BYTES
    // seg13:0x5d5b-0x5d9b] (M4 s5 ICONS).
    kIconOwn,
    kCount
};

// A brush selected for a PatBlt or a ROP that uses the pattern.
struct Brush {
    enum class Kind { kNone, kSolid, kStock, kMetal };
    Kind kind = Kind::kNone;
    Colour colour = 0;  // kSolid: CreateSolidBrush(colour)
    int stock = 0;      // kStock: GetStockObject(stock): LTGRAY_BRUSH 1, NULL_BRUSH 5
    static Brush none() { return {}; }
    static Brush solid(Colour c) { return {Kind::kSolid, c, 0}; }
    static Brush stock_object(int s) { return {Kind::kStock, 0, s}; }
    static Brush metal() { return {Kind::kMetal, 0, 0}; }  // DGROUP:0x500a
};

// A 1-pixel pen: CreatePen(PS_SOLID, 1, colour), or a stock pen (WHITE_PEN 6, BLACK_PEN 7).
struct Pen {
    bool is_stock = false;
    Colour colour = 0;
    int stock = 0;
    static Pen solid(Colour c) { return {false, c, 0}; }
    static Pen stock_object(int s) { return {true, 0, s}; }
};

struct Point {
    int x = 0, y = 0;
};

// Raster operations the HUD passes.
constexpr std::uint32_t kSrcCopy = 0x00CC0020;
constexpr std::uint32_t kSrcPaint = 0x00EE0086;
constexpr std::uint32_t kMergeCopy = 0x00C000CA;
// seg11:0x2154 and seg11:0x1ef6 pass 0x00C00746: ROP3 index 0xC0, MERGECOPY, with a non-standard
// low word that GDI ignores [BYTES seg11:0x2086 seg11:0x2272] [REF Win32 SDK, Ternary Raster
// Operations].
constexpr std::uint32_t kMergeCopy746 = 0x00C00746;
constexpr std::uint32_t kPatCopy = 0x00F00021;
constexpr std::uint32_t kBlackness = 0x00000042;
constexpr std::uint32_t kWhiteness = 0x00FF0062;

// Stock objects (wingdi.h).
constexpr int kLtGrayBrush = 1, kNullBrush = 5, kWhitePen = 6, kBlackPen = 7;
// SetTextAlign values the HUD passes.
constexpr unsigned kTaLeft = 0, kTaRight = 2;

class Canvas {
public:
    virtual ~Canvas() = default;

    // Whether the bitmap exists (its handle is non-zero), and its size (GetObject).
    virtual bool has(Img img) const = 0;
    virtual int width(Img img) const = 0;
    virtual int height(Img img) const = 0;
    // CreateCompatibleBitmap(DGROUP:0x13dc, w, h): a new black bitmap in `img`.
    virtual void create(Img img, int w, int h) = 0;

    // PatBlt with `brush` selected for the call.
    virtual void pat_blt(Img dst, int x, int y, int w, int h, std::uint32_t rop,
                         const Brush& brush) = 0;
    // BitBlt with `brush` selected in the destination for the call (MERGECOPY uses it).
    virtual void bit_blt(Img dst, int x, int y, int w, int h, Img src, int sx, int sy,
                         std::uint32_t rop, const Brush& brush = Brush::none()) = 0;
    // MoveTo(p[0]) and LineTo(p[1]) ... LineTo(p[n-1]) with `pen` selected: each LineTo
    // excludes its end pixel.
    virtual void lines(Img dst, const Pen& pen, const Point* p, int n) = 0;
    // Rectangle(l, t, r, b) with `pen` and `brush` selected.
    virtual void rectangle(Img dst, const Pen& pen, const Brush& brush, int l, int t, int r,
                           int b) = 0;
    virtual void set_pixel(Img dst, int x, int y, Colour c) = 0;
    // SetStretchBltMode(dst, COLORONCOLOR) and StretchBlt(dst, x, y, w, h, src, sx, sy, sw, sh,
    // rop): seg13:0x5c22's picture [BYTES seg13:0x5c93-0x5c98 seg13:0x60e1]. A canvas without
    // the call draws nothing (the default).
    virtual void stretch_blt(Img /*dst*/, int /*x*/, int /*y*/, int /*w*/, int /*h*/, Img /*src*/,
                             int /*sx*/, int /*sy*/, int /*sw*/, int /*sh*/, std::uint32_t /*rop*/) {}
    // seg10:0x949c, ISPACE.DLL ordinal 34: the bevelled frame of `style`.
    virtual void frame(Img dst, int x, int y, int w, int h, int style) = 0;
    // seg10:0x94c4, ISPACE.DLL ordinal 33: the bitmap font on the window DC.
    virtual void font_text(int font, int align, Colour c, int x, int y,
                           const std::string& text) = 0;
    // TextOut with font DGROUP:0x90fe, TRANSPARENT, the alignment and the text colour.
    virtual void text_out(Img dst, int x, int y, unsigned align, Colour c,
                          const std::string& text) = 0;

    // An IS_INFO panel drawn on the surface (ADR 0013 decision 4): until clear_view(), the
    // window DC's coordinates start at (x, y) and its drawing is clipped to w x h, as the child
    // window's client area was.
    virtual void set_view(int x, int y, int w, int h) = 0;
    virtual void clear_view() = 0;
};

}  // namespace is::hud
