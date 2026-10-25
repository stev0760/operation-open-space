// 8-bit index bitmaps (ADR 0006 decision 2, ADR 0007 decision 2): the drawing surface at the
// logical size and the game's images, plus 1-bit monochrome bitmaps for LoadBitmap's
// black-and-white resources and ordinal 6's masks.
//
// On Win32 an index bitmap is a top-down 8-bpp DIB section selected into its own memory DC, so
// the game draws with the GDI calls the original makes. Every index bitmap carries the same
// fixed colour table (index_table): it stands for ADR 0007 decision 2's table of palette indices
// (DIB_PAL_COLORS). Its 256 colours are distinct, so a blit between two index bitmaps maps each
// index to itself under any GDI, and it never changes, so a palette change touches no bitmap:
// only the presenter's lookup table carries the palette's colours (decision 3). Colours reach
// GDI as DIBINDEX(n) (Gfx::dc_colour). A monochrome bitmap is a 1-bpp DDB
// (CreateBitmap), so GDI maps its bits through the destination's text and background colours
// as the original's LoadBitmap results were. On the host the same class holds a plain buffer
// and does no drawing. Portable header; the Win32 members are guarded.
#pragma once

#include <cstdint>
#include <vector>

#include "formats/dib.h"
#include "gfx/palette.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace is::gfx {

class Surface {
public:
    enum class Format { kIndex8, kMono1 };

    Surface() = default;
    // An index bitmap of w x h, all index 0, with index_table() as its colour table.
    static Surface index8(int w, int h);
    // A monochrome bitmap from word-aligned rows, MSB first, bit 1 = white (CreateBitmap).
    static Surface mono(int w, int h, std::vector<std::uint8_t> bits);

    Surface(Surface&& o) noexcept;
    Surface& operator=(Surface&& o) noexcept;
    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;
    ~Surface();

    bool empty() const { return w_ == 0 || h_ == 0; }
    Format format() const { return format_; }
    int width() const { return w_; }
    int height() const { return h_; }
    // Bytes per row: (w + 3) & ~3 for index bitmaps (DIB rows), word-aligned for mono.
    int stride() const { return stride_; }
    // Top-down rows. For an index bitmap these are the DIB section's bits on Win32: call flush()
    // before reading what GDI drew. A mono bitmap's bits are the ones it was made from.
    std::uint8_t* pixels() { return bits_; }
    const std::uint8_t* pixels() const { return bits_; }
    std::uint8_t at(int x, int y) const;  // index, or 0/1 for mono

    // GdiFlush on Win32: GDI batches drawing into DIB sections.
    void flush() const;

#ifdef _WIN32
    // The memory DC with the bitmap selected; its text colour is DIBINDEX(0) and background
    // colour DIBINDEX(255), the black and white the original's DCs started with. The bitmap
    // stays selected for the surface's life: draw through this DC, not another one.
    HDC hdc() const { return dc_; }
    HBITMAP bitmap() const { return bmp_; }
#endif

private:
    void release();

    Format format_ = Format::kIndex8;
    int w_ = 0, h_ = 0, stride_ = 0;
    std::uint8_t* bits_ = nullptr;
    std::vector<std::uint8_t> store_;  // the host's index bits, and every mono bitmap's bits
#ifdef _WIN32
    HDC dc_ = nullptr;
    HBITMAP bmp_ = nullptr;
    HGDIOBJ old_ = nullptr;
#endif
};

// The colour table of every index bitmap [DESIGN, ADR 0007 decision 2]: entries 0-9 and
// 246-255 are the 20 static colours (default_device_colours), which never change, so stock
// objects, BLACKNESS (0) and WHITENESS (255) keep their meaning; entries 10-245 are
// RGB(i, 255 - i, 0x80), distinct from each other and from the statics. They are not the
// palette's colours and are never shown.
const DeviceColours& index_table();

// ---- conversions of the original's DIBs (portable)

// ISPACE.DLL seg5:0x2080, CreateDIBitmap at 8 bpp (shell.md "Palette", Bitmaps bullet): an 8-bit
// DIB's pixels are palette indices as they are (DIB_PAL_COLORS with the indices 0-255); a 1-,
// 4- or 24-bit DIB goes through DIB_RGB_COLORS, so each colour becomes the nearest entry of the
// palette realised at the time, the device palette (nearest_index) [DESIGN]. Top-down, w*h.
std::vector<std::uint8_t> dib_indices(const fmt::Dib& dib, const DeviceColours& device);
// A 1-bpp DIB whose two colours are black and white: LoadBitmap made a monochrome bitmap of it
// [REF Win16 SDK LoadBitmap]. Its bits as CreateBitmap rows, 1 where the colour is nearer white.
bool is_black_and_white(const fmt::Dib& dib);
std::vector<std::uint8_t> dib_mono_bits(const fmt::Dib& dib);
// Ordinal 6, ISPACE.DLL seg5:0x650 (ispace.md): a monochrome bitmap of the source's size, 1
// where the source pixel equals the background colour RGB(0, 0, 0). A plain RGB black resolves
// to static entry 0 (colour_index), so an index source gives 1 for index 0 only, as the 8-bit
// display did; a mono source gives 1 for its black (0) bits [DESIGN].
std::vector<std::uint8_t> mask_bits(const Surface& src);
// Rows of a w-pixel monochrome bitmap: word aligned (CreateBitmap).
inline int mono_stride(int w) { return ((w + 15) / 16) * 2; }

}  // namespace is::gfx
