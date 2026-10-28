// The wave's sprite routine seg8:0xb76 (docs/re/world.md "Sprites") and the drawing interface the
// wave's portable code draws through (ADR 0013 decision 1: the surface is the window, the
// original's calls in the original's order).
//
// Blitter stands for the GDI calls the wave makes on the window DC DGROUP:0x13dc and on its
// memory DCs (the image slots DGROUP:0x39e8, the icon strip DGROUP:0x3a02, the radar bitmap
// DGROUP:0x93ce). DCs and brushes are 16-bit words as the original stores them (object +0x3c,
// the image-slot table, the brush table DGROUP:0x5004): the port keeps its own ids there and the
// Win32 Blitter (sprite_win32.cpp) maps them to HDCs and HBRUSHes [DESIGN ADR 0013]. A test
// drives the routines with RecordingBlitter and compares the call list.
//
// Portable, no Win32.
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "world/records.h"
#include "world/world.h"  // Rect16, pt_in_rect

#ifdef _WIN32
#include <windows.h>

namespace is::gfx {
class Gfx;
class Surface;
}  // namespace is::gfx
#endif

namespace is::world {

// A DC word: 0 is none. kDcWindow is the window DC DGROUP:0x13dc, which the port's surface
// stands for (ADR 0006 decision 2) [DESIGN ADR 0013].
using DcId = std::uint16_t;
constexpr DcId kDcNone = 0;
constexpr DcId kDcWindow = 1;
// The same window DC as the HUD's routines (the radar's blit to the strip) draw on it: the
// wave pass's clip (GdiBlitter::set_window_clip) does not apply [DESIGN ADR 0013] [OPEN Status
// display at wave start] (M3 s2 fixer).
constexpr DcId kDcHudWindow = 2;
// A brush word: 0 is none. Stock objects have fixed ids (GetStockObject(n) = kBrushStock + n).
using BrushId = std::uint16_t;
constexpr BrushId kBrushNone = 0;
constexpr BrushId kBrushStock = 0xff00;
constexpr BrushId kBrushWhite = kBrushStock + 0;  // GetStockObject(WHITE_BRUSH) [BYTES seg11:0x3866]
constexpr BrushId kBrushBlack = kBrushStock + 4;  // BLACK_BRUSH

// The raster operations the wave passes [REF Win16 SDK, ternary raster operations].
constexpr std::uint32_t kSrcCopy = 0x00CC0020;    // SRCCOPY
constexpr std::uint32_t kMergeCopy = 0x00C000CA;  // MERGECOPY: source AND brush
constexpr std::uint32_t kSrcPaint = 0x00EE0086;   // SRCPAINT: source OR destination
constexpr std::uint32_t kBlackness = 0x00000042;  // BLACKNESS
constexpr std::uint32_t kWhiteness = 0x00FF0062;  // WHITENESS
constexpr std::uint32_t kPatCopy = 0x00F00021;    // PATCOPY
constexpr std::uint32_t kDpa = 0x00A000C9;        // DPa: destination AND brush (the player mask)
constexpr int kColorOnColor = 3;                  // SetStretchBltMode COLORONCOLOR

// The GDI calls of the wave's drawing, with the original's arguments. Colours are COLORREFs as
// the original passes them (PALETTERGB 0x02xxxxxx included); the Win32 Blitter resolves them
// through Gfx::dc_colour (ADR 0007). The resource calls stand for the original's
// CreateCompatibleBitmap + CreateCompatibleDC pairs, LoadBitmap (seg10:0x6d38), the #4000
// loader (seg10:0x6c02) and CreateSolidBrush; each returns a new id, 0 when it fails.
class Blitter {
public:
    virtual ~Blitter() = default;
    virtual void bit_blt(DcId dst, int x, int y, int w, int h, DcId src, int sx, int sy,
                         std::uint32_t rop) = 0;
    virtual void pat_blt(DcId dst, int x, int y, int w, int h, std::uint32_t rop) = 0;
    virtual void stretch_blt(DcId dst, int x, int y, int w, int h, DcId src, int sx, int sy,
                             int sw, int sh, std::uint32_t rop) = 0;
    virtual void set_stretch_mode(DcId dc, int mode) = 0;
    // SelectObject(dc, brush): returns the brush that was selected.
    virtual BrushId select_brush(DcId dc, BrushId brush) = 0;
    virtual void set_pixel(DcId dc, int x, int y, std::uint32_t colorref) = 0;
    virtual void frame_rect(DcId dc, const Rect16& r, BrushId brush) = 0;

    // A black memory image of w x h with its own DC (CreateCompatibleBitmap(DGROUP:0x13dc)).
    virtual DcId create_image(int w, int h) = 0;
    // seg10:0x6d38 (BITMAP name) and seg10:0x6c02 (#4000 id, mode, arg), each in its own DC.
    virtual DcId load_bitmap(const std::string& name) = 0;
    virtual DcId load_graphic(int id, int mode, int arg) = 0;
    virtual BrushId create_solid_brush(std::uint32_t colorref) = 0;
};

// One recorded call (RecordingBlitter). Unused fields stay 0.
struct BlitCall {
    enum Op { kBitBlt, kPatBlt, kStretchBlt, kStretchMode, kSelectBrush, kSetPixel, kFrameRect };
    Op op = kBitBlt;
    DcId dst = 0;
    int x = 0, y = 0, w = 0, h = 0;
    DcId src = 0;
    int sx = 0, sy = 0, sw = 0, sh = 0;
    std::uint32_t rop = 0;   // ROP; the mode for kStretchMode; the colour for kSetPixel
    BrushId brush = 0;       // kSelectBrush, kFrameRect
    bool operator==(const BlitCall& o) const {
        return op == o.op && dst == o.dst && x == o.x && y == o.y && w == o.w && h == o.h &&
               src == o.src && sx == o.sx && sy == o.sy && sw == o.sw && sh == o.sh &&
               rop == o.rop && brush == o.brush;
    }
};
// Shorthands for expected calls.
inline BlitCall bit_blt_call(DcId dst, int x, int y, int w, int h, DcId src, int sx, int sy,
                             std::uint32_t rop) {
    BlitCall c;
    c.op = BlitCall::kBitBlt, c.dst = dst, c.x = x, c.y = y, c.w = w, c.h = h;
    c.src = src, c.sx = sx, c.sy = sy, c.rop = rop;
    return c;
}
inline BlitCall pat_blt_call(DcId dst, int x, int y, int w, int h, std::uint32_t rop) {
    BlitCall c;
    c.op = BlitCall::kPatBlt, c.dst = dst, c.x = x, c.y = y, c.w = w, c.h = h, c.rop = rop;
    return c;
}

// A Blitter that draws nothing and records every drawing call, for host tests. Resources get
// ids counting up from 0x100 (brushes from 0x200); select_brush tracks each DC's brush.
class RecordingBlitter : public Blitter {
public:
    std::vector<BlitCall> calls;

    void bit_blt(DcId dst, int x, int y, int w, int h, DcId src, int sx, int sy,
                 std::uint32_t rop) override;
    void pat_blt(DcId dst, int x, int y, int w, int h, std::uint32_t rop) override;
    void stretch_blt(DcId dst, int x, int y, int w, int h, DcId src, int sx, int sy, int sw,
                     int sh, std::uint32_t rop) override;
    void set_stretch_mode(DcId dc, int mode) override;
    BrushId select_brush(DcId dc, BrushId brush) override;
    void set_pixel(DcId dc, int x, int y, std::uint32_t colorref) override;
    void frame_rect(DcId dc, const Rect16& r, BrushId brush) override;
    DcId create_image(int w, int h) override;
    DcId load_bitmap(const std::string& name) override;
    DcId load_graphic(int id, int mode, int arg) override;
    BrushId create_solid_brush(std::uint32_t colorref) override;

    // What the resource calls were given, by id.
    struct Image { DcId id; int w, h; std::string name; int graphic; };
    std::vector<Image> images;
    struct Brush { BrushId id; std::uint32_t colorref; };
    std::vector<Brush> brushes;

private:
    DcId next_dc_ = 0x100;
    BrushId next_brush_ = 0x200;
    std::vector<std::pair<DcId, BrushId>> selected_;
};

// A point in screen pixels.
struct Point16 {
    std::int16_t x = 0, y = 0;
};
// seg8:0x29ea: a world point (dword +0x04: x, y) to the screen ([View](world.md#view)); the
// view owner provides it.
using ToScreen = std::function<Point16(std::int16_t x, std::int16_t y)>;

// The DGROUP words seg8:0xb76 reads and writes. Names follow world.md where it gives one; the
// others are named by address.
struct SpriteGlobals {
    DcId window_dc = kDcWindow;          // DGROUP:0x13dc
    std::int16_t ship_pool = kPoolShips; // DGROUP:0x9324
    std::int16_t player = 0;             // DGROUP:0x9646
    std::int16_t play_w = 0;             // W, DGROUP:0x90ea
    std::int16_t play_h = 0;             // H, DGROUP:0x90ec
    std::int16_t dg_951a = 0;            // DGROUP:0x951a: not 0 lets the player move on screen [OPEN]
    std::int16_t dg_238c = 0;            // DGROUP:0x238c: screen shake on [BYTES seg8:0x0]
    std::int16_t dg_238e = 0;            // DGROUP:0x238e: re-pin the player once; cleared here
    std::int16_t dg_952e = 0;            // DGROUP:0x952e: a ship index of the kind-3/6 modes [OPEN]
    std::int16_t dg_9e47 = 0;            // DGROUP:0x9e47 [OPEN]
    std::int16_t dg_9e49 = 0;            // DGROUP:0x9e49: special-wave kind [OPEN]
    std::int16_t dg_913a = 0;            // DGROUP:0x913a [OPEN]
    std::int16_t dg_913c = 0;            // DGROUP:0x913c [OPEN]
    BrushId player_mask_brush = 0;       // DGROUP:0x5ea8, the 0x00A000C9 PatBlt's brush [OPEN]
};

// seg8:0xb76(pool*256 + rec, kind, srcDC, masked) [BYTES seg8:0xb76]: draws one object's cell on
// the window DC, erasing what its last cell uncovered. Returns the routine's result (0 for the
// first early exit, 1 otherwise). Kinds: 4 and 5 move the screen position without drawing
// (5 only off screen), 6 blits SRCPAINT without an erase, 7 draws a 3 x 3 WHITENESS dot; every
// other kind (the callers pass 1) erases and blits. `masked` picks MERGECOPY over SRCCOPY:
// MERGECOPY uses whatever brush the caller selected into the window DC.
int draw_sprite(Pools& pools, std::int16_t pool_rec, std::int16_t kind, DcId src_dc,
                std::int16_t masked, SpriteGlobals& g, const ToScreen& to_screen, Blitter& out);

// The erase that seg8:0xb76 makes before the blit (seg8:0xdfd-0x1015), separated so the
// geometry is testable on its own: dx, dy are the move (+0x00 - +0x08, +0x02 - +0x0a), the
// cell is at (x, y) now and was at (old_x, old_y); size and gap are +0x22 and +0x24.
// Returns true when the gap case replaces the erase and the blit by one larger blit (made
// here); false when the caller still blits the cell (after the strips or the old cell were
// blacked here).
bool erase_and_merge(Blitter& out, DcId dst, DcId src, std::int16_t x, std::int16_t y,
                     std::int16_t old_x, std::int16_t old_y, std::int16_t size,
                     std::int16_t gap, std::int16_t src_x, std::int16_t src_y,
                     std::uint32_t rop);

#ifdef _WIN32
// The Blitter on GDI (sprite_win32.cpp): kDcWindow is the renderer's surface DC (Gfx::dc(),
// looked up on every call because a resize makes a new surface); every other DC is an index
// image of the renderer (Gfx::create_image, load_bitmap, load_graphic) or one the caller binds.
// Colours go through Gfx::dc_colour (ADR 0007). Brushes it creates are deleted with it; stock
// brushes are GetStockObject(id - kBrushStock).
class GdiBlitter : public Blitter {
public:
    explicit GdiBlitter(gfx::Gfx& gfx);
    ~GdiBlitter() override;
    GdiBlitter(const GdiBlitter&) = delete;
    GdiBlitter& operator=(const GdiBlitter&) = delete;

    // An image the caller made gets a DC id; the blitter keeps it alive.
    DcId bind(std::shared_ptr<gfx::Surface> image);
    // A brush the caller made (the HUD's METAL pattern brush): an id, not deleted here.
    BrushId bind_brush(HBRUSH brush);
    HDC hdc(DcId id) const;
    HBRUSH hbrush(BrushId id) const;
    // The image behind a DC id, null for the window and unknown ids.
    std::shared_ptr<gfx::Surface> image(DcId id) const;
    // SelectClipRgn(DGROUP:0x13dc, DGROUP:0x12) of the wave pass (seg7:0x84a): the wave's draws
    // on the window DC are clipped to the play rectangle from now on. The port clips the wave's
    // own blits only, not the HUD canvas that shares the surface [DESIGN ADR 0013] [OPEN Status
    // display at wave start] (M3 s2 fixer).
    void set_window_clip(const Rect16& r) {
        clip_ = r;
        clipped_ = true;
    }

    void bit_blt(DcId dst, int x, int y, int w, int h, DcId src, int sx, int sy,
                 std::uint32_t rop) override;
    void pat_blt(DcId dst, int x, int y, int w, int h, std::uint32_t rop) override;
    void stretch_blt(DcId dst, int x, int y, int w, int h, DcId src, int sx, int sy, int sw,
                     int sh, std::uint32_t rop) override;
    void set_stretch_mode(DcId dc, int mode) override;
    BrushId select_brush(DcId dc, BrushId brush) override;
    void set_pixel(DcId dc, int x, int y, std::uint32_t colorref) override;
    void frame_rect(DcId dc, const Rect16& r, BrushId brush) override;
    DcId create_image(int w, int h) override;
    DcId load_bitmap(const std::string& name) override;
    DcId load_graphic(int id, int mode, int arg) override;
    BrushId create_solid_brush(std::uint32_t colorref) override;

private:
    BrushId brush_id(HBRUSH h);
    // The window DC with the pass's clip applied (SaveDC + IntersectClipRect), undone by
    // unclip() (RestoreDC) [DESIGN ADR 0013].
    HDC clip(DcId id, HDC d) const;
    void unclip(DcId id, HDC d) const;
    Rect16 clip_{};
    bool clipped_ = false;
    gfx::Gfx& gfx_;
    DcId next_dc_ = 0x100;
    BrushId next_brush_ = 0x200;
    std::map<DcId, std::shared_ptr<gfx::Surface>> images_;
    struct Brush {
        HBRUSH h;
        bool owned;
    };
    std::map<BrushId, Brush> brushes_;
};
#endif

}  // namespace is::world
