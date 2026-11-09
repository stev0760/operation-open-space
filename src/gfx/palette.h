// The game palette: ISPACE.DLL's palette manager (ordinal 4, commands 0-8, and ordinal 14) and
// its colour tables, ported as docs/re/shell.md "Palette" and docs/re/palette-tables.md
// describe them, over a software palette with the original's layout (ADR 0007 decision 1).
// The device the manager talks to is the port's fixed 256-colour display (ADR 0007 decision 4):
// BITSPIXEL 8, SIZEPALETTE 256, NUMRESERVED 20. "Publishing" stands for AnimatePalette and the
// realisation of a new palette: it copies working entries to the device palette, which the
// surface's colour table and the presenter mirror (src/gfx/gfx.h). Portable: no Win32 calls.
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

namespace is::fmt {
struct Dib;
}

namespace is::gfx {

struct Rgb {
    std::uint8_t r = 0, g = 0, b = 0;
    bool operator==(const Rgb& o) const { return r == o.r && g == o.g && b == o.b; }
    bool operator!=(const Rgb& o) const { return !(*this == o); }
    bool black() const { return r == 0 && g == 0 && b == 0; }
    static constexpr Rgb of(std::uint32_t rrggbb) {
        return {static_cast<std::uint8_t>(rrggbb >> 16), static_cast<std::uint8_t>(rrggbb >> 8),
                static_cast<std::uint8_t>(rrggbb)};
    }
    std::uint32_t rgb() const { return std::uint32_t{r} << 16 | std::uint32_t{g} << 8 | b; }
};

// A PALETTEENTRY: peRed, peGreen, peBlue, peFlags.
struct PalEntry {
    std::uint8_t r = 0, g = 0, b = 0, flags = 0;
    Rgb rgb() const { return {r, g, b}; }
    void set(Rgb c) {  // bytes 0-2 only, as every writer in seg5 does
        r = c.r;
        g = c.g;
        b = c.b;
    }
    bool black() const { return r == 0 && g == 0 && b == 0; }
    bool operator==(const PalEntry& o) const {
        return r == o.r && g == o.g && b == o.b && flags == o.flags;
    }
};
using PaletteArray = std::array<PalEntry, 256>;
using DeviceColours = std::array<Rgb, 256>;

constexpr std::uint8_t kPcReserved = 0x01;

// The port's device answers (ADR 0007 decision 4) [DESIGN].
constexpr int kBitsPixel = 8, kSizePalette = 256, kNumReserved = 20;
// Entries 10-245 are the game's: AnimatePalette(hPal, 10, 236, ...) [BYTES ISPACE.DLL seg5:0x441a].
constexpr int kFirstGameEntry = 10, kGameEntries = 236;

// The 20 static colours of a 256-colour Windows display, entries 0-9 and 246-255 (shell.md
// "Palette", GetSystemPaletteEntries bullet). [REF Windows SDK, default palette static colours]
extern const std::array<std::uint32_t, 20> kStaticColours;  // 0x00RRGGBB, 0-9 then 246-255
// The device palette before the game's first palette: the statics and black [DESIGN].
DeviceColours default_device_colours();
bool is_static_entry(int i);

// ISPACE.DLL DGROUP:0x117c (18 rows of 10) and DGROUP:0x1cdc (18 rows of 5) as seg5:0x5846
// leaves them (palette-tables.md). [BYTES ISPACE.DLL seg5:0x5846]
constexpr int kSchemeRows = 18;
using Scheme10 = std::array<Rgb, 10>;
using Scheme5 = std::array<Rgb, 5>;
extern const std::array<Scheme10, kSchemeRows> kSchemes10;
extern const std::array<Scheme5, kSchemeRows> kSchemes5;
// Command 7's switch from c to a 0x117c row, -1 where the original leaves the row unset
// [BYTES ISPACE.DLL seg5:0x6292].
int ramp_row(int c);

// ISPACE.DLL #5500 100: 92 RGB triples for entries 11-102 (formats.md).
using Colours5500 = std::array<Rgb, 92>;

// Nearest entry by squared RGB distance in [first, last]; ties go to the lowest index, as GDI's
// GetNearestPaletteIndex and Wine's colour-table match resolve them [DESIGN].
int nearest_index(const DeviceColours& pal, Rgb c, int first = 0, int last = 255);
// A COLORREF as the 8-bit display resolved it to a palette index [DESIGN, after REF Win32 SDK
// "Palettes"]: PALETTEINDEX(n) (high byte 1) is n; PALETTERGB (high byte 2) is the nearest of
// all 256 entries; a plain RGB (high byte 0) is the nearest of the 20 static colours, which the
// display kept for colours the palette does not name (brushes are not dithered: [DESIGN], the
// dithering question of ispace.md ordinal 33 is [OPEN]); DIBINDEX(n) (0x10FFxxxx) is n.
int colour_index(std::uint32_t colorref, const DeviceColours& device);

using PalHandle = std::uint16_t;

// The palette manager of ISPACE.DLL seg5. Handles stand for HPALETTEs: non-zero, unique per
// creation [DESIGN]. Every command finds its slot by handle and does nothing when the handle is
// not found [BYTES ISPACE.DLL seg5:0x35b4]; where the original would go on with slot 3 and
// read past its tables, the port stops [DESIGN].
class PaletteManager {
public:
    static constexpr int kSlots = 3;     // DGROUP:0x1cd6[3]
    static constexpr int kModes = 3;     // the modes INSPACE passes: 0, 1, 2
    static constexpr int kRecords = 19;  // ramp records per mode
    static constexpr int kSchemeIds = 25;

    PaletteManager();

    // ISPACE.DLL #5500 100, which command 1 reads with FindResource every time; the port reads
    // it once (src/gfx/gfx.h) and hands it in [DESIGN]. Until then the triples are black.
    void set_colours_5500(const Colours5500& c) { res5500_ = c; }

    // Called after every publish with the device range that changed.
    using PublishFn = std::function<void(int first, int count)>;
    void on_publish(PublishFn f) { on_publish_ = std::move(f); }
    // Called when command 0 needs the font bitmap ALPHABET (true: load) and when command 2
    // releases the last palette (false: delete) [BYTES ISPACE.DLL seg5:0x3712 seg5:0x3b95].
    using AlphabetFn = std::function<void(bool load)>;
    void on_alphabet(AlphabetFn f) { on_alphabet_ = std::move(f); }

    // ---- ordinal 4 (ISPACE.DLL seg5:0x34c8)
    // The dispatcher: hPal, a, b, c and cmd as INSPACE pushes them (hwnd and hdc are the
    // device's, which the port does not need). Command 0 returns the handle, command 6 the
    // entry or -1, the others 0.
    int command(PalHandle pal, int a, int b, int c, int cmd);

    PalHandle create(int kind);                           // 0: seg5:0x3596(a)
    void load_scheme(PalHandle pal, int c, int mode);     // 1: seg5:0x44ac(hPal, c, b)
    void destroy(PalHandle pal);                          // 2: seg5:0x3aae(hPal)
    void animate(PalHandle pal);                          // 3: seg5:0x4378(hPal, hwnd, hdc)
    void save_set(PalHandle pal, int set, int blank);     // 4: seg5:0x484a(hPal, hdc, c, b)
    void restore_set(PalHandle pal, int set);             // 5: seg5:0x474c(hPal, hdc, b)
    int allocate_colour(PalHandle pal, int r, int g, int b);  // 6: seg5:0x3dee(hPal, a, b, c)
    void allocate_ramp(PalHandle pal, int c);             // 7: seg5:0x446e -> seg5:0x61b2
    void swap_black(PalHandle pal, int c);                // 8: seg5:0x3bae(hPal, c)

    // ---- ordinal 14 (ISPACE.DLL seg5:0x50cc): scale or blend by percent, then command 3.
    void scale(PalHandle pal, int mode, int percent);

    // ---- seg5:0x26a2: the per-graphic recolour pass of seg5:0x1b8e (shell.md "Graphic colour
    // tables"), on a DIB colour table (0x00RRGGBB, padded to 16 entries for the pass). d is the
    // load mode (3 for ordinal 1 at 8 bpp, 0 for ordinal 12, 2 allocates every colour with
    // command 6), a and b the scheme rows (12 or -1: keep), `last` the last argument
    // (non-zero: record a and b), `ordinal12` DGROUP:0x188 (set by ordinal 12's seg7:0x12d8).
    // seg5:0x1b8e's command 3 for d 0 and 2 is included.
    // Returns ISPACE.DLL DGROUP:0x1ea8: true when the fixed 16-colour set replaced the table, after
    // which the loader calls seg6:0x7fc8 (fixed_set_remap) [BYTES ISPACE.DLL seg5:0x26d8 seg5:0x272b
    // seg5:0x571-0x57c seg6:0x1f5-0x200 seg6:0x5630-0x563b seg7:0x13aa-0x13b8].
    bool recolour_graphic(PalHandle pal, int id, std::vector<std::uint32_t>& table, int d, int a,
                          int b, int last, bool ordinal12);

    // ---- state
    bool valid(PalHandle pal) const { return slot_of(pal) >= 0; }
    const PaletteArray* working(PalHandle pal) const;   // DGROUP:0x116c[slot]
    // The same entries for the DLL's own writers outside the manager: ordinal 12's ramp builder
    // ISPACE.DLL seg7:0x4ee8 writes them through DGROUP:0x1168 = DGROUP:0x116c[slot] + 4 [BYTES
    // ISPACE.DLL seg7:0x4f36-0x4f4a seg7:0x50aa seg7:0x543d] (gfx/smooth_render.h). Null when
    // the handle is not found.
    PaletteArray* working_entries(PalHandle pal);
    const PaletteArray* stored(PalHandle pal, int set) const;  // DGROUP:0x14a4[slot*2+set]
    int kind(PalHandle pal) const;                      // DGROUP:0x1e92[slot], -1 unknown
    int mode(PalHandle pal) const;                      // DGROUP:0x1e9c[slot], -1 unknown
    // The device palette: what the display shows, and what the surface's colour table holds.
    const DeviceColours& device() const { return device_; }
    // Incremented on every publish.
    std::uint32_t generation() const { return generation_; }
    // The colour tables as they stand (row 12 is written per graphic).
    const std::array<Scheme10, kSchemeRows>& schemes10() const { return schemes10_; }
    const std::array<Scheme5, kSchemeRows>& schemes5() const { return schemes5_; }
    // DGROUP:0x1398 + 4*(mode*19 + record): the first and last entry of a ramp record.
    struct Ramp {
        int first = 0, last = 0;
    };
    Ramp ramp_record(int mode, int record) const;
    // DGROUP:0x1dea[mode*25 + c]: the record of scheme c, -1 when none.
    int ramp_of(int mode, int c) const;
    // DGROUP:0x1eb8[mode*19 + record]: the record's use count.
    int ramp_uses(int mode, int record) const;
    // DGROUP:0x1f3c: palettes created and not deleted (the ALPHABET reference count).
    int alphabet_refs() const { return alphabet_refs_; }
    // DGROUP:0x1eaa and DGROUP:0x1eac, the ramp flags of seg5:0x26a2 (BSS: 0 until the first
    // write). recolour_graphic writes them only on its d = 0 path at depth >= 8 other than a = 4,
    // b = -1: each is 1, cleared when a colour of its range (0-9, 10-14) is brighter than the one
    // before, then forced to 1 when its scheme byte is not 12; they keep their values across calls
    // [BYTES ISPACE.DLL seg5:0x2ec2 seg5:0x2fde seg5:0x2fe4 seg5:0x3100 seg5:0x3106-0x3118].
    // Readers: ordinal 12's seg7:0x12d8, which sets b1 (b2) to 100 when DGROUP:0x1eaa (0x1eac) is
    // not 0 [BYTES ISPACE.DLL seg7:0x1541-0x1551], and ordinal 2 on kind-1 palettes [BYTES
    // ISPACE.DLL seg6:0xd4e-0xd78] (work/analysis/m4/s6-ord12.md 2.2, verified claims 13 and 14).
    int dg_1eaa() const { return dg_1eaa_; }
    int dg_1eac() const { return dg_1eac_; }
    // DGROUP:0x1e80 + 6 slot + 2k (k = 0 ... 2), ordinal 14's blend rows on kind-1 palettes:
    // ordinal 12 writes -1 to k = 0 and 1 before its load [BYTES ISPACE.DLL seg7:0x1328-0x134a].
    void set_blend_from(PalHandle pal, int k, int row);
    int blend_from(PalHandle pal, int k) const;

    // ISPACE.DLL seg5:0x64f8(slot, r, g, b, s1, s2), cdecl: 0 in palette mode 2 (`xor al, al`
    // at seg5:0x6512; the callers clear AH); otherwise the scheme ids s1 then s2, each looked
    // up in DGROUP:0x1dea[mode 25 + s] with -1 skipped, are scanned over their ramp records'
    // entries (DGROUP:0x1398, first ... last inclusive) of the working palette DGROUP:0x1168
    // with distance |dr| + |dg| + |db| and a best of 1000 at first: a strictly smaller distance
    // becomes the best and goes to DGROUP:0x1a6, and the scan returns DGROUP:0x1a6 >> 2 at the
    // first distance below 14 or at its end. With no entry scanned DGROUP:0x1a6 keeps its last
    // value, which `last` carries in and out (as entries, the port's divided records) [BYTES
    // ISPACE.DLL seg5:0x64f8 seg5:0x6512 seg5:0x6517-0x655a seg5:0x655d-0x66df] (docs/re/
    // ispace.md "Nearest colour"; gfx/ship_flame.h nearest_flame_entry is the one-scheme case).
    // A scheme id without a record reads DGROUP:0x1398 + 4 (19 mode - 1) in the original
    // [OPEN]; the port skips it [DESIGN].
    int nearest_scheme_entry(PalHandle pal, Rgb c, int s1, int s2, int& last) const;

private:
    struct Slot {
        PalHandle handle = 0;
        int kind = 0;
        int mode = 0;
        PaletteArray working{};
        std::array<PaletteArray, 2> stored{};
        std::array<int, 3> blend_from{};  // DGROUP:0x1e80 + 6*slot
        std::array<int, 3> blend_to{};    // DGROUP:0x1f2a + 6*slot
    };
    int slot_of(PalHandle pal) const;
    void ramp(int slot, int c);  // seg5:0x61b2
    void publish(int slot, int first, int count);
    static int luminance(Rgb c);  // the brightness test of seg5:0x26a2

    std::array<Slot, kSlots> slots_{};
    DeviceColours device_;
    std::uint32_t generation_ = 0;
    PalHandle next_handle_ = 0;
    Colours5500 res5500_{};
    std::array<Scheme10, kSchemeRows> schemes10_;
    std::array<Scheme5, kSchemeRows> schemes5_;
    std::array<std::array<int, kSchemeIds>, kModes> scheme_record_{};   // DGROUP:0x1dea
    std::array<std::array<int, kRecords>, kModes> record_uses_{};      // DGROUP:0x1eb8
    std::array<std::array<Ramp, kRecords>, kModes> records_{};         // DGROUP:0x1398
    int alphabet_refs_ = 0;  // DGROUP:0x1f3c
    int dg_1eaa_ = 0, dg_1eac_ = 0;  // DGROUP:0x1eaa, DGROUP:0x1eac
    PublishFn on_publish_;
    AlphabetFn on_alphabet_;
};

// ---- ISPACE.DLL ordinal 17's colour map (work/analysis/m4/s5-sheet.md 4.4, verified claims 26,
// 27 and 31 CORRECTED)

// The word table DGROUP:0x1484[0 ... 14]: the palette index ordinal 17 writes for source nibble
// v is the low byte of entry v (DGROUP:0x1484 + 2v) [BYTES ISPACE.DLL seg6:0x58c4-0x58cc
// seg6:0x5a99-0x5aa3]; nibble 15 is transparent.
using QuickColourMap = std::array<std::uint8_t, 15>;

// The DGROUP:0x15c pass of seg5:0x26a2, which runs only inside ordinal 17's load (seg5:0x1b8e
// clears DGROUP:0x15c after it [BYTES ISPACE.DLL seg5:0x1c04]): for k = 0 ... 14, with (r, g, b)
// colour-table entry k after the recolour (`table`, 0x00RRGGBB: bytes +0x2a, +0x29, +0x28 of the
// BITMAPINFO entry) [BYTES ISPACE.DLL seg5:0x2a69-0x2bb7]:
//  - (k < 10 and b1 != 12) or (k >= 10 and b2 != 12): seg5:0x64f8(slot, r, g, b, id_a, id_b),
//    id_a and id_b ordinal 17's DGROUP:0x147c and DGROUP:0x1480 (seg5:0x6f58 of b1 and b2, set
//    only for a scheme other than 12, else left from the last call) [BYTES ISPACE.DLL
//    seg5:0x2a80-0x2ac5];
//  - else id = seg5:0x66ea(r, g, b) (colour_ramp_id, gfx/ship_sheet.h), seg5:0x61b2(slot, id)
//    when no earlier entry of this case had the same id, then seg5:0x64f8(slot, r, g, b, id, -1)
//    [BYTES ISPACE.DLL seg5:0x2ac8-0x2b65]. The earlier ids are stack bytes [bp - 0x5c + j]
//    written only by this case; an entry of the first case leaves its byte as the stack held it
//    [OPEN]; the port counts only this case's ids, so a ramp is allocated once per distinct id
//    [DESIGN ADR 0016 decision 1];
//  - the result's low byte (`sub ah, ah`) [BYTES ISPACE.DLL seg5:0x2b61 seg5:0x2b90].
// The path ported is DGROUP:0x1e9a >= 8 with palette kind 0 (the port's 8-bpp device and the
// game palette): below 8 bpp (seg5:0x4b40) and kind 1 (seg5:0x4e96) are not [DESIGN], and the
// map stays 0 there. `dg_1a6` is DGROUP:0x1a6 (seg5:0x64f8's last entry).
QuickColourMap quick_colour_map(PaletteManager& pm, PalHandle pal, const std::vector<std::uint32_t>& table,
                                int b1, int b2, int id_a, int id_b, int& dg_1a6);

// ISPACE.DLL seg6:0x7fc8(hPal, hDib), which ordinals 1, 2 and 12 and the quick cells' loader call
// when DGROUP:0x1ea8 is set (recolour_graphic returned true): a fresh 4-bpp BITMAPINFOHEADER over
// the DIB's (seg5:0x22e6) and every pixel's nibble v replaced by (15, 5, 1, 2, 9, 6, 10, 13, 14,
// 4, 0, 3, 8, 7, 11, 12)[v], so that under the fixed set each pixel keeps the VGA colour of its
// own index (index 8 grey, 7 silver, 0 black) [BYTES ISPACE.DLL seg6:0x7fef-0x8042 seg6:0x8050-0x81fc].
// Its four class counters and the words DGROUP:0x1670 / 0x1672 it leaves have no reader in
// ISPACE.DLL [BYTES ISPACE.DLL seg6:0x8162-0x8174 seg6:0x81fe-0x8252]; they are not kept. A DIB of
// another depth is left alone [DESIGN: the bytes rewrite it as 4 bpp; no graphic reaches that].
// core-0008's far boxes of the landing corridor show the colours this gives [ORACLE core-0008];
// the reading is the M4 s6 fixer's [OPEN: unchecked by a verifier; docs/re keeps seg6:0x7fc8
// open until a verify round on seg6:0x7fc8-0x8264].
void fixed_set_remap(fmt::Dib& dib);

}  // namespace is::gfx
