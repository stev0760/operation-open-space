// owner: M4 stage 6, track PAD.
// The pad's polygon raster (src/gfx/pad.h pad_mask) and its whole picture (pad_picture) against
// GDI under Win32 (Wine in CTest): seg15:0x4a2e's calls drawn again, the mask into a 1-bpp bitmap
// compatible with a fresh memory DC for every play height 1 ... 3000, and the whole pad for 683
// (1024 x 768) into a 24-bpp DIB section with the PALETTERGB brushes and MERGECOPY [BYTES
// seg15:0x4a3b-0x4c6c]. The port's raster is Wine's GDI's [DESIGN]; the bit-for-bit check runs
// only under Wine, as test_gfx_wing_cut_win32.cpp's: on real Windows the counts are reported
// [OPEN: Windows' Polygon raster against the original's, which settles whether the port should
// follow it].
#ifdef _WIN32
#include <windows.h>

#include <algorithm>
#include <string>
#include <vector>

#include "doctest.h"
#include "gfx/pad.h"

namespace {

bool under_wine() {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    return ntdll && GetProcAddress(ntdll, "wine_get_version") != nullptr;
}

// seg15:0x4a2e's steps 1-3 [BYTES seg15:0x4a3b-0x4aa1]: GDI's bits, top-down, 1 = white.
std::vector<std::uint8_t> gdi_mask(const is::gfx::PadGeometry& g) {
    HDC screen = GetDC(nullptr);
    HDC m = CreateCompatibleDC(screen);
    HBITMAP bm = CreateCompatibleBitmap(m, g.pw, g.pc);  // 1 bpp: the fresh DC's default bitmap
    HGDIOBJ old = SelectObject(m, bm);
    PatBlt(m, 0, 0, g.pw, g.pc, BLACKNESS);
    HGDIOBJ ob = SelectObject(m, GetStockObject(WHITE_BRUSH));
    POINT p[4];
    for (int i = 0; i < 4; ++i) p[i] = {g.poly[static_cast<std::size_t>(i)].x, g.poly[static_cast<std::size_t>(i)].y};
    Polygon(m, p, 4);
    SelectObject(m, ob);
    SelectObject(m, old);
    struct {
        BITMAPINFOHEADER h;
        RGBQUAD c[2];
    } bi{};
    bi.h.biSize = sizeof(BITMAPINFOHEADER);
    bi.h.biWidth = g.pw;
    bi.h.biHeight = -g.pc;
    bi.h.biPlanes = 1;
    bi.h.biBitCount = 1;
    bi.h.biCompression = BI_RGB;
    const int stride = ((g.pw + 31) / 32) * 4;
    std::vector<unsigned char> bits(static_cast<std::size_t>(stride) * g.pc);
    GetDIBits(m, bm, 0, static_cast<UINT>(g.pc), bits.data(), reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);
    DeleteObject(bm);
    DeleteDC(m);
    ReleaseDC(nullptr, screen);
    std::vector<std::uint8_t> out(static_cast<std::size_t>(g.pw) * g.pc);
    for (int y = 0; y < g.pc; ++y)
        for (int x = 0; x < g.pw; ++x)
            out[static_cast<std::size_t>(y) * g.pw + x] = (bits[static_cast<std::size_t>(y) * stride + x / 8] >> (7 - x % 8)) & 1;
    return out;
}

}  // namespace

TEST_CASE("pad win32: GDI's Polygon raster of seg15:0x4a2e equals pad_mask for every play height") {
    int sizes = 0, differ = 0;
    std::string first;
    for (int H = 1; H <= 3000; ++H) {
        const is::gfx::PadGeometry g = is::gfx::pad_geometry(H, H + 85);
        if (g.pw <= 2 || g.pc <= 2) continue;
        ++sizes;
        if (gdi_mask(g) != is::gfx::pad_mask(g)) {
            if (differ == 0) first = std::to_string(H);
            ++differ;
        }
    }
    MESSAGE("the pad mask: " << differ << " of " << sizes << " play heights differ from GDI"
                             << (differ ? ", first " + first : std::string()));
    if (under_wine()) CHECK(differ == 0);
}

TEST_CASE("pad win32: seg15:0x4a2e drawn with GDI at 1024 x 768 equals pad_picture") {
    const is::gfx::PadGeometry g = is::gfx::pad_geometry(683, 768);
    const is::gfx::PadPicture p = is::gfx::pad_picture(g);
    const int w = g.pw, h = g.height();
    HDC screen = GetDC(nullptr);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 24;
    void* bits = nullptr;
    HBITMAP pad = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    REQUIRE(pad);
    HDC d = CreateCompatibleDC(screen);
    HGDIOBJ old_pad = SelectObject(d, pad);
    PatBlt(d, 0, 0, w, h, BLACKNESS);  // seg15:0x482e [BYTES seg15:0x4a0a-0x4a1c]
    HDC m = CreateCompatibleDC(d);
    HBITMAP mono = CreateCompatibleBitmap(m, g.pw, g.pc);
    HGDIOBJ old_m = SelectObject(m, mono);
    PatBlt(m, 0, 0, g.pw, g.pc, BLACKNESS);
    HGDIOBJ ob = SelectObject(m, GetStockObject(WHITE_BRUSH));
    POINT pts[4];
    for (int i = 0; i < 4; ++i) pts[i] = {g.poly[static_cast<std::size_t>(i)].x, g.poly[static_cast<std::size_t>(i)].y};
    Polygon(m, pts, 4);
    SelectObject(m, ob);
    const auto brush = [&](int v, auto draw) {
        HBRUSH b = CreateSolidBrush(0x02000000u | RGB(v, v, v));
        HGDIOBJ o = SelectObject(d, b);
        draw();
        SelectObject(d, o);
        DeleteObject(b);
    };
    const int step = 1700 / g.pc;
    int val = 450;
    for (int y = 0; g.pc - 2 > y; y += 2) {
        brush(val / 10, [&] { BitBlt(d, 0, y, g.pw, 2, m, 0, y, MERGECOPY); });
        val += step;
        val = val > 2550 ? 2550 : val + step;
    }
    SelectObject(m, old_m);
    DeleteObject(mono);
    DeleteDC(m);
    brush(0x80, [&] { PatBlt(d, 1, g.pc, g.pw - 2, 10, PATCOPY); });
    val = 450;
    for (int i = 0; i < 18; i += 2) {
        brush(val / 10, [&] {
            PatBlt(d, g.pw / 2 + i - 18, g.pc + 10, 2, 42, PATCOPY);
            PatBlt(d, g.pw / 2 - i + 16, g.pc + 10, 2, 42, PATCOPY);
        });
        val = std::min(val + 227, 2550);
    }
    GdiFlush();
    const int stride = ((w * 3 + 3) / 4) * 4;
    int differ = 0;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const auto* px = static_cast<const unsigned char*>(bits) + static_cast<std::size_t>(y) * stride + 3 * x;
            const int v = p.at(x, y);
            if (px[0] != v || px[1] != v || px[2] != v) ++differ;
        }
    SelectObject(d, old_pad);
    DeleteObject(pad);
    DeleteDC(d);
    ReleaseDC(nullptr, screen);
    MESSAGE("the pad picture: " << differ << " of " << w * h << " pixels differ from GDI");
    if (under_wine()) CHECK(differ == 0);
}
#endif
