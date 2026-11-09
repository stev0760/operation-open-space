// The scene renderer, ISPACE.DLL ordinal 32 (seg8:0x0), as a pure function: the #6600 record
// decoder, the projection constants, the layout solver seg8:0xa36, the output slots
// (seg8:0x15ec) and the ordered list of GDI calls the record walk makes on its memory DC
// ISPACE.DLL DGROUP:0x1c66 (src/desk/scene_gdi.* replays it). Sources: docs/re/ispace.md
// "Ordinal notes" ordinal 32, docs/re/formats.md "IGRAPHIC.DLL #6600" and
// work/analysis/m4/s6-scene.md sections 1.1-1.10 with s6-scene-verify.md's corrections.
//
// Every multiply and divide is the original's: the long multiply ISPACE.DLL seg3:0xfe keeps the
// low 32 bits, the long divide seg3:0x130 truncates toward zero, and a 16 x 16 `imul` followed
// by `cwd; idiv` divides the low word of the product. Words the original stores wrap to 16 bits.
//
// Colours: seg8:0x27f0 and seg8:0x5978 build PALETTERGB (0x02BBGGRR) values above 8 bpp and
// PALETTEINDEX values of the command-6 ramps at 8 bpp. The op list carries the PALETTERGB values
// and the GDI side allocates each through command 6 (ADR 0007, "Solid colours ... land on the
// nearest palette entry") [DESIGN].
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
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
    int string_id = 0;                     // +0x19 (signed word)
    int parent = 0, parent2 = 0;           // +0x1b, +0x1c
    int colour = -1;                       // +0x1d (signed)
    int type = 0;                          // +0x1e
    int slot = -1;                         // +0x1f (signed)
    int brightness = 0;                    // +0x20, the raw byte; the level reads it through CBW
    // +0x21 ... +0x25, signed bytes (s6-scene.md 1.4): f21 = 1 bevel (type 1) or cross (type 3),
    // 2 ... 99 vents, 100 METAL, 120 graphic 76; f22 pins; f23 the text size factor; f24 front
    // text; f25 the HIGH VOLTAGE plate.
    int f21 = 0, f22 = 0, f23 = 0, f24 = 0, f25 = 0;
    std::array<std::uint8_t, fmt::Scene6600::kRecordSize> raw{};  // as the resource holds it
};

// The header fields ordinal 32 reads (formats.md "Header colours"). The arithmetic reads the
// words themselves (colour_word, dark_word, divisor_word): the raw bytes of a decoded header, and
// for a header built in code (has_raw false, the unit tests) the typed fields, colour i's
// divisor for i >= 3 being dark colour 0's channel i - 3, the word the bytes overlap there
// [BYTES ISPACE.DLL seg8:0x1537-0x1548] [DESIGN].
struct Header {
    std::array<Rgb, 6> colour{};      // words +6i, i < 6
    int count = 0;                    // +0x24: colours in use (scene 105 has 4)
    std::array<int, 3> divisor{};     // +0x26, +0x28, +0x2a; colours 3-5 read +0x2c ... (dark 0)
    std::array<Rgb, 6> dark{};        // words +0x2c + 6i
    std::array<std::uint8_t, 0x6c> raw{};
    bool has_raw = false;             // raw holds the resource's header (decode, shade_rgb)
    int mode_x = 0, mode_y = 0;       // bytes +0x50, +0x51
    std::int32_t d5c = 0, d60 = 0, d64 = 0, d68 = 0;
};
int colour_word(const Header& h, int i, int channel);   // word +6i + 2ch
int dark_word(const Header& h, int i, int channel);     // word +0x2c + 6i + 2ch
int divisor_word(const Header& h, int i);               // word +0x26 + 2i

struct Scene {
    Header header;
    std::vector<Record> records;
};

Scene decode(const fmt::Scene6600& raw);

// The projection constants of seg8:0x0 (ISPACE.DLL DGROUP:0xd28 ...). d4c, d48 and d44 are the
// dwords DGROUP:0xd4c, 0xd48 and 0xd44.
struct Projection {
    int W = 0, H = 0;        // DGROUP:0xd18, 0xd1a
    int cx = 0, cy = 0;      // SM_CXSCREEN, SM_CYSCREEN (codes 9 and 10)
    int d28 = 0, d2c = 0, d2e = 0, c6c = 0;
    std::int32_t d4c = 0, d48 = 0, d44 = 0;
    // s of a depth Y: q = Y*D3C/1000, s = D4C*D3C/(q + D4C), D3C = sext(d28)
    // [BYTES ISPACE.DLL seg8:0x2920-0x296b].
    std::int32_t scale(std::int32_t y) const;
    // Yb = lo16(s*D48/2000) + d2e [BYTES ISPACE.DLL seg8:0x2971-0x2994].
    int yb(std::int32_t s) const;
    // The screen x of the centre line at Yb: lo16((c6c - Yb)*1000/D44) + trunc((W - d28)/2)
    // [BYTES ISPACE.DLL seg8:0x2a3a-0x2a6f] (a box adds lo16 of X*s/1000).
    int centre(int yb) const;
};
// prev_d2c and prev_d2e stand for DGROUP:0xd2c and 0xd2e as the previous ordinal-32 call left
// them, which a mode byte other than 6, 7 and 8 keeps (0 before the first call; all five scenes
// use modes 6 and 7) [BYTES ISPACE.DLL seg8:0xd0-0x162].
Projection projection(const Header& h, int W, int H, int cx, int cy, int prev_d2c = 0, int prev_d2e = 0);

// What ISPACE.DLL seg8:0x5c86 reads on every ordinal-32 call (s6-scene.md 1.10): the device
// texts the cards and the chip print (string words -10 ... -13) and the condition flags of
// seg8:0x1641. The defaults are the INI defaults with cpu = 4 and no coprocessor flag, the
// port's stand-in for GetWinFlags [DESIGN, ruling R3, docs/re/win16-translation.md].
struct Conditions {
    int a8 = 0;               // DGROUP:0x22a8: 0xff, then 1 (386 + WF_80x87) or 0 (486)
    int c70 = 4;              // DGROUP:0x1c70: the CPU class 2, 3, 4 or 0
    bool async_wave = false;  // DGROUP:0x22aa: waveOutGetNumDevs > 0 and no WAVECAPS_SYNC
    bool stereo = false;      // DGROUP:0x22ab: device 0 has more than one channel
    bool net_found = false;   // DGROUP:0x22a9: both network reads non-empty (no reader here)
    std::string video = "Video Card";  // DGROUP:0x1b34
    std::string sound = "Sound Card";  // DGROUP:0x1b8e
    std::string net;                   // DGROUP:0x1be8
};
bool condition_holds(const Header& h, int v, const Conditions& c);
// DGROUP:0xd1c: itoa(100*cpu + 86, 10) [BYTES ISPACE.DLL seg8:0x5f97-0x5fa5].
std::string cpu_text(int c70);
// The cuts of seg8:0x5c86 on a GetPrivateProfileString result: a space at an index above 16 or
// a '(' ends the video and sound texts [BYTES ISPACE.DLL seg8:0x5ce1-0x5d14 seg8:0x5da0-0x5dd3];
// a sound text starting "Crea" becomes "Sound Blaster" first [BYTES ISPACE.DLL seg8:0x5d70-0x5d94];
// a network text loses a leading '"' and also ends at ',' and '"' [BYTES ISPACE.DLL
// seg8:0x5ec3-0x5f45]; the PROTOCOL.INI key ends at ',' [BYTES ISPACE.DLL seg8:0x5e62-0x5e89].
// Results keep at most 0x59 bytes (the buffers of 0x5a), the key 0x27.
std::string cut_video_text(std::string s);
std::string cut_sound_text(std::string s);
std::string cut_net_key(std::string s);
std::string cut_net_text(std::string s);

// What ISPACE.DLL seg8:0x5c86 asks the system (s6-scene.md 1.10). `profile` is
// GetPrivateProfileString(section, key, default, buffer of `size`, file) and returns what it
// leaves in the buffer; the directories are GetWindowsDirectory's and GetSystemDirectory's.
struct DeviceSource {
    std::function<std::string(const std::string& section, const std::string& key, const std::string& def,
                              int size, const std::string& file)> profile;
    std::string windows_dir, system_dir;
    unsigned wave_devs = 0;            // waveOutGetNumDevs()
    std::uint32_t wave_support = 0;    // waveOutGetDevCaps(0): dwSupport
    unsigned wave_channels = 0;        //                       wChannels
    std::uint32_t win_flags = 0;       // GetWinFlags()
};
// GetWinFlags' WF_CPU486, the port's answer: Win32 has no GetWinFlags, a 486 or later reports
// this bit under Win16, and the oracle's Wine gave 0x6c29 with it set (the "486" label, no
// coprocessor flag read on that branch) [DESIGN, ruling R3, docs/re/win16-translation.md]
// [ORACLE core-0008].
constexpr std::uint32_t kWinFlagsPort = 0x0008;
// seg8:0x5c86 on those answers, in the order of its calls [BYTES ISPACE.DLL seg8:0x5c95-0x5fa5].
Conditions devices_from(const DeviceSource& src);

// seg8:0xa36's layout passes; returns the highest slot byte seen (DGROUP:0x1c68, from -1).
int solve(Scene& s, const Projection& p);

// (left, top, right, bottom + 1), as seg8:0x15ec writes them at out + 8s.
struct Rect {
    int l = 0, t = 0, r = 0, b = 0;
    int w() const { return r - l; }
    int h() const { return b - t; }
    bool operator==(const Rect& o) const { return l == o.l && t == o.t && r == o.r && b == o.b; }
};

// ---- the GDI calls

constexpr std::uint32_t kPatCopy = 0x00F00021;   // PATCOPY
constexpr std::uint32_t kBlackness = 0x00000042; // BLACKNESS

enum class Pattern : std::uint8_t {
    kNone,       // +0x21 in 101 ... 127 other than 120: the unwritten local [bp-0x76], no bitmap
                 // (ADR 0016 decision 1) [DESIGN]; no scene has one
    kMetal,      // +0x21 = 100: ordinal 3 LoadBitmap(IGRAPHIC.DLL, "METAL") [BYTES ISPACE.DLL seg8:0x34cd-0x34d1]
    kGraphic76,  // +0x21 = 120: ordinal 1 (hPal, 76, "", dc, 0, -1) [BYTES ISPACE.DLL seg8:0x34de-0x34ef]
};

// One step of the drawing, in the order the bytes make the GDI calls. Brushes and pens are state:
// a kBrush, kPattern or kStock selects a new brush into the DC (the previous one created is
// deleted when the next is made), a kPen a new pen, and kPatBlt, kMoveTo and kLineTo draw with
// what is selected.
struct Op {
    enum Kind : std::uint8_t {
        kBrush,    // CreateSolidBrush(colour) + SelectObject
        kPattern,  // the bitmap of `pattern`, CreatePatternBrush + SelectObject
        kStock,    // SelectObject(GetStockObject(stock)): LTGRAY_BRUSH 1 or BLACK_BRUSH 4
        kPatBlt,   // PatBlt(dc, x, y, w, h, rop); w and h may be negative
        kPen,      // CreatePen(PS_SOLID, width, colour) + SelectObject
        kMoveTo,   // MoveTo(dc, x, y)
        kLineTo,   // LineTo(dc, x, y)
        kText,     // seg8:0x5978: CreateFont(font_h, font_w, 0, 0, weight, ..., 2, 0x22, "Arial"),
                   // SetBkColor(0), SetBkMode(TRANSPARENT), SetTextAlign(align) unless align < 0,
                   // SetTextColor(colour), TextOut(x, y, text)
        kFrame,    // ordinal 34 (ISPACE.DLL seg8:0x5ac) (dc, x, y, w, h, mode): frame_ops
    };
    Kind kind = kPatBlt;
    int x = 0, y = 0, w = 0, h = 0;
    std::uint32_t rop = kPatCopy;     // kPatBlt
    std::uint32_t colour = 0;         // kBrush, kPen, kText: the COLORREF the bytes build
    int width = 0;                    // kPen
    int stock = 0;                    // kStock
    Pattern pattern = Pattern::kNone; // kPattern
    int mode = 0;                     // kFrame
    int font_h = 0, font_w = 0, weight = 0, align = -1;  // kText
    std::string text;                 // kText, cp1252
    int record = 0;                   // the record that made it; 0 for the clear
    std::uint16_t site = 0;           // ISPACE.DLL seg8 offset after the call (kBrush, kPen)
};
// Ordinal 34's own calls for a kFrame op: the stock brush, PatBlt PATCOPY, the right and bottom
// edges in a 1-px pen of RGB(dark), the top and left in RGB(light) unless mode 0, and for mode 2
// a 0x808080 bottom edge (s6-scene.md 1.9) [BYTES ISPACE.DLL seg8:0x5bb-0x758].
std::vector<Op> frame_ops(const Op& frame);

// [DESIGN] A summary of the op list: each solid PATCOPY PatBlt with positive extents as a fill
// in its brush colour and each LineTo as a line in its pen. tests/test_desk.cpp reads it; the
// drawing replays `ops` (src/desk/scene_gdi.h).
struct Prim {
    enum Kind { kFill, kLine } kind = kFill;
    int x = 0, y = 0, w = 0, h = 0;
    int x1 = 0, y1 = 0;
    int width = 0;
    Rgb c;
    int record = 0;
};

struct Render {
    Projection proj;
    int max_slot = -1;
    std::vector<Rect> slots;        // max_slot + 1 entries
    std::vector<Op> ops;            // in drawing order, the clear first
    std::vector<std::vector<Rgb>> ramps;  // the command-6 shades per header colour (seg8:0x14ac)
    Conditions devices;             // what seg8:0x5c86 gave this call
    std::vector<Prim> prims;        // [DESIGN] the summary (see Prim)
};

// seg8:0x27f0 above 8 bpp: PALETTERGB with the peak channel lo8(min(level, 255)) and each other
// channel lo8(colour[col][j]*level/colour[col][peak]) (32-bit), no lower clamp
// [BYTES ISPACE.DLL seg8:0x27ff-0x280a seg8:0x2879-0x28d1].
std::uint32_t shade_colour(const Header& h, int col, int peak, int level);
// The same for a colour in hand (its peak channel), as Rgb.
Rgb shade_rgb(const Rgb& colour, int level);
// DGROUP:0x1c42[i]: the first channel with the largest word (R before G before B)
// [BYTES ISPACE.DLL seg8:0x14e2-0x151d]; 0 for i at or above the header's count.
int peak_of(const Header& h, int i);
// trunc(lo16(colour[col][peak]*sext8(brightness))/50) [BYTES ISPACE.DLL seg8:0x171c-0x1754].
int level_of(const Header& h, const Record& r);
// The shade ramps seg8:0xa36 allocates through palette command 6 [BYTES ISPACE.DLL
// seg8:0x1465-0x14ac seg8:0x1537-0x1548].
std::vector<std::vector<Rgb>> shade_ramps(const Header& h);

// A face corner: (x, y) words.
struct Point {
    int x = 0, y = 0;
};
// The strip filler ISPACE.DLL seg8:0x1fe6 (s6-scene.md 1.5) for the four points P0 ... P3
// (top-left, top-right, bottom-right, bottom-left), the depths A (at P3's side) and B: rows for
// top and bottom faces, columns for side faces. Appends its kBrush and kPatBlt ops.
void strip_fill(std::vector<Op>& out, const Header& h, const Projection& p, const std::array<Point, 4>& P,
                int col, int peak, int level, int A, int B, bool rows, int record);

// Ordinal 32's inputs beyond the resource: the wrapper seg10:0x9526's arguments
// [BYTES seg10:0x9530-0x955d].
struct Inputs {
    int W = 0, H = 0;      // [bp+0xe], [bp+0xc]
    int cx = 0, cy = 0;    // SM_CXSCREEN, SM_CYSCREEN
    int ox = 0, oy = 0;    // [bp+8], [bp+0xa]: header dwords +0x5c, +0x60 when either is non-zero
    int z_delta = 0;       // [bp+6]: added to body word +0x51 when record 2's slot byte is 0xec
    int depth = 8;         // DGROUP:0x1e9a: text below 8 bpp is white
    int prev_d2c = 0, prev_d2e = 0;  // see projection()
    Conditions devices;    // seg8:0x5c86
    // LoadString(ENG_LANG.DLL, id, buf, 40) for string words above 1 (ISPACE.DLL DGROUP:0x690)
    // [BYTES ISPACE.DLL seg8:0x1b45-0x1b54]; empty when not given.
    std::function<std::string(int)> strings;
};

// Ordinal 32 for one scene: decode, constants, solve, the record walk.
Render render(const fmt::Scene6600& raw, const Inputs& in);
Render render(const fmt::Scene6600& raw, int W, int H, int cx, int cy, const Conditions& c);

}  // namespace is::desk
