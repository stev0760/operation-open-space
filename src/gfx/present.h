// The presenter (ADR 0007 decision 3 and Consequences, ADR 0006 decision 2): one isolated step
// from index surface + device palette + window to pixels on screen. It converts the surface
// through a 256-entry lookup into a 32-bit DIB section at the logical size and StretchBlts that
// to the client area by the integer factor with COLORONCOLOR, so every logical pixel is a crisp
// factor x factor block. The lookup fill is portable and host-tested; the blit is Win32.
#pragma once

#include <cstdint>
#include <vector>

#include "gfx/palette.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace is::gfx {

class Surface;

// The 32-bit pixel of a device colour: 0x00RRGGBB, the B, G, R, 0 bytes of a 32-bpp DIB.
inline std::uint32_t bgra(Rgb c) { return c.rgb(); }

// Fills w x h 32-bit pixels (dst_stride in pixels) from w x h index pixels through the palette.
void fill_bgra(const std::uint8_t* src, int src_stride, int w, int h, const DeviceColours& pal,
               std::uint32_t* dst, int dst_stride);

#ifdef _WIN32
class Presenter {
public:
    Presenter() = default;
    Presenter(const Presenter&) = delete;
    Presenter& operator=(const Presenter&) = delete;
    ~Presenter();

    // Converts `surface` with `pal` and draws it to `dst` at (0, 0) scaled by `factor`; the
    // client area right and below the scaled image (client size not a multiple of the factor)
    // is filled black [DESIGN]. Skips the conversion when neither the surface's indices nor the
    // palette changed since the last call, and then also the blit unless `force` (a WM_PAINT)
    // [DESIGN]. Returns true when it drew.
    bool present(HDC dst, int client_w, int client_h, int factor, const Surface& surface,
                 const DeviceColours& pal, std::uint32_t pal_generation, bool force);
    // The 32-bit pixels of the last conversion (top-down, width() per row), for tests.
    const std::uint32_t* pixels() const { return bits_; }
    int width() const { return w_; }
    int height() const { return h_; }
    void invalidate() { valid_ = false; }

private:
    void ensure(int w, int h);
    HDC dc_ = nullptr;
    HBITMAP bmp_ = nullptr;
    HGDIOBJ old_ = nullptr;
    std::uint32_t* bits_ = nullptr;
    int w_ = 0, h_ = 0;
    bool valid_ = false;
    std::uint32_t generation_ = 0;
    std::vector<std::uint8_t> last_;  // the indices of the last conversion
};
#endif

}  // namespace is::gfx
