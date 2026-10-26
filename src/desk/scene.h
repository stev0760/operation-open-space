// The scene renderer, ISPACE.DLL ordinal 32 (seg8:0x0), as a portable model: the #6600 record
// decoder, the projection constants, the layout solver seg8:0xa36, the output slots
// (seg8:0x15ec) and the primitive list the draw loop seg8:0x1afb hands to GDI. Sources:
// docs/re/ispace.md "Ordinal notes" ordinal 32 and docs/re/formats.md "IGRAPHIC.DLL #6600".
//
// Every multiply and divide is the original's long arithmetic (seg3:0xfe, seg3:0x130), which
// truncates toward zero; results that the original stores in words wrap to 16 bits.
//
// What the docs leave [OPEN] is approximated and marked: the strip faces of seg8:0x1fe6, the
// line shades of seg8:0x5632 and the type-3 primitive seg8:0x3874.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "formats/records.h"

namespace is::desk {

struct Rgb {
    std::uint8_t r = 0, g = 0, b = 0;
    bool operator==(const Rgb& o) const { return r == o.r && g == o.g && b == o.b; }
};

// One 0x26-byte record of the body (formats.md "Records").
struct Record {
    bool resolved = false;                 // +0x00
    std::array<int, 3> pos{};              // +0x01 X, +0x03 Y (depth), +0x05 Z (height)
    std::array<int, 3> size{};             // +0x07 SX, +0x09 SY, +0x0b SZ (a line's end point)
    std::array<int, 3> pos_code{};         // +0x0d, +0x0f, +0x11
    std::array<int, 3> size_code{};        // +0x13, +0x15, +0x17
    int string_id = 0;                     // +0x19
    int parent = 0, parent2 = 0;           // +0x1b, +0x1c
    int colour = -1;                       // +0x1d (signed)
    int type = 0;                          // +0x1e
    int slot = -1;                         // +0x1f (signed)
    int brightness = 0;                    // +0x20
    int pattern = 0;                       // +0x21
};

// The header fields ordinal 32 reads (formats.md "Header colours").
struct Header {
    std::array<Rgb, 3> colour{};      // words +0x00-+0x11
    int count = 0;                    // +0x24
    std::array<int, 3> divisor{};     // +0x26
    std::array<Rgb, 3> dark{};        // words +0x2c-+0x3d
    std::array<std::uint8_t, 0x6c> raw{};
    int mode_x = 0, mode_y = 0;       // bytes +0x50, +0x51
    std::int32_t d5c = 0, d60 = 0, d64 = 0, d68 = 0;
};

struct Scene {
    Header header;
    std::vector<Record> records;
};

Scene decode(const fmt::Scene6600& raw);

// The projection constants of seg8:0x0 (ISPACE.DLL DGROUP:0xd28 ...).
struct Projection {
    int W = 0, H = 0;        // DGROUP:0xd18, 0xd1a
    int cx = 0, cy = 0;      // SM_CXSCREEN, SM_CYSCREEN (codes 9 and 10)
    int d28 = 0, d2c = 0, d2e = 0, d4c = 0, c6c = 0, d48 = 0, d44 = 0;
    // s and Yb of a depth Y [BYTES ISPACE.DLL seg8:0x2920 seg8:0x2966].
    int scale(int y) const;
    int yb(int s) const;
    // The screen x of the centre line at Yb, (c6c - Yb)*1000/d44 + (W - d28)/2.
    int centre(int yb) const;
};
Projection projection(const Header& h, int W, int H, int cx, int cy);

// The conditions of seg8:0x1641 (ISPACE.DLL DGROUP:0x22a8, 0x1c70, 0x22aa, 0x22ab).
struct Conditions {
    int a8 = 0;        // DGROUP:0x22a8
    int c70 = 0;       // DGROUP:0x1c70
    bool async_wave = false;  // DGROUP:0x22aa: waveOutGetNumDevs > 0 and no WAVECAPS_SYNC
    bool stereo = false;      // DGROUP:0x22ab: device 0 has more than one channel
};
bool condition_holds(const Header& h, int v, const Conditions& c);

// seg8:0xa36's layout passes; returns the highest slot byte seen (DGROUP:0x1c68, from -1).
int solve(Scene& s, const Projection& p);

// A projected face of a box record (seg8:0x28de).
struct Face {
    int left = 0, top = 0, width = 0, height = 0, yb = 0;
    int right() const { return left + width; }
};
Face front_face(const Record& r, const Projection& p);
Face back_face(const Record& r, const Projection& p);

// (left, top, right, bottom + 1), as seg8:0x15ec writes them at out + 8s.
struct Rect {
    int l = 0, t = 0, r = 0, b = 0;
    int w() const { return r - l; }
    int h() const { return b - t; }
    bool operator==(const Rect& o) const { return l == o.l && t == o.t && r == o.r && b == o.b; }
};

// What GDI receives: PatBlt PATCOPY with a solid brush, or a 2-px pen from MoveTo to LineTo.
struct Prim {
    enum Kind { kFill, kLine } kind = kFill;
    int x = 0, y = 0, w = 0, h = 0;  // kFill; kLine: (x, y) -> (x1, y1)
    int x1 = 0, y1 = 0;
    int width = 0;                   // kLine pen width
    Rgb c;
    int record = 0;                  // the record that drew it
};

struct Render {
    Projection proj;
    int max_slot = -1;
    std::vector<Rect> slots;        // max_slot + 1 entries
    std::vector<Prim> prims;        // in drawing order
    std::vector<std::vector<Rgb>> ramps;  // the command-6 shades per header colour (seg8:0x14ac)
};

// seg8:0x27f0 above 8 bpp: PALETTERGB with the peak channel = level (clamped to 255) and the
// others colour[k] * level / peak.
Rgb shade_rgb(const Rgb& colour, int level);
// The shade ramps seg8:0xa36 allocates through palette command 6 [BYTES ISPACE.DLL
// seg8:0x1465-0x14ac seg8:0x1537].
std::vector<std::vector<Rgb>> shade_ramps(const Header& h);

// Ordinal 32 for one scene: decode, constants, solve, draw loop.
Render render(const fmt::Scene6600& raw, int W, int H, int cx, int cy, const Conditions& c);

}  // namespace is::desk
