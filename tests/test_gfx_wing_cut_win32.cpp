// The wing mask's polygon raster (src/gfx/wing_cut.h wing_mask) against GDI under Win32 (Wine in
// CTest): ISPACE.DLL ordinal 4 command 0's PatBlts and Polygons drawn again into a 450 x 90 1-bpp
// bitmap, compared bit for bit [BYTES ISPACE.DLL seg5:0x3724-0x386e]. The port's table is what
// Wine's GDI draws (work/analysis/m4/s5-sheet.md 2.2, its open item) [DESIGN]. The bit-for-bit
// check runs only under Wine: on real Windows (M9/M10) GDI's polygon edge rule may differ, and
// the test then reports the count without failing [OPEN: Windows' Polygon raster against the
// original's, which settles whether the table should follow it] (M4 s5 fixer, review L10).
#ifdef _WIN32
#include <windows.h>

#include <string>
#include <vector>

#include "doctest.h"
#include "gfx/wing_cut.h"

TEST_CASE("wing cut win32: GDI draws the mask DGROUP:0x184 bit for bit as the port's table") {
    HDC screen = GetDC(nullptr);
    REQUIRE(screen);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP bm = CreateBitmap(450, 90, 1, 1, nullptr);  // [BYTES ISPACE.DLL seg5:0x372d]
    REQUIRE(dc);
    REQUIRE(bm);
    HGDIOBJ old = SelectObject(dc, bm);
    PatBlt(dc, 0, 0, 450, 90, BLACKNESS);   // [BYTES ISPACE.DLL seg5:0x3751-0x3762]
    PatBlt(dc, 45, 0, 46, 90, WHITENESS);   // [BYTES ISPACE.DLL seg5:0x3767-0x3778]
    PatBlt(dc, 360, 45, 90, 46, WHITENESS); // [BYTES ISPACE.DLL seg5:0x377d-0x378f]
    HGDIOBJ op = SelectObject(dc, GetStockObject(WHITE_PEN));    // [BYTES ISPACE.DLL seg5:0x3794-0x37b9]
    HGDIOBJ ob = SelectObject(dc, GetStockObject(WHITE_BRUSH));
    POINT p1[3] = {{180, 89}, {269, 89}, {269, 0}};               // [BYTES ISPACE.DLL seg5:0x37bc-0x37e8]
    POINT p2[4] = {{112, 89}, {179, 89}, {179, 0}, {157, 0}};     // [BYTES ISPACE.DLL seg5:0x37ed-0x381c]
    POINT p3[4] = {{270, 68}, {270, 89}, {359, 89}, {359, 21}};   // [BYTES ISPACE.DLL seg5:0x3821-0x3851]
    Polygon(dc, p1, 3);
    Polygon(dc, p2, 4);
    Polygon(dc, p3, 4);
    SelectObject(dc, op);
    SelectObject(dc, ob);
    SelectObject(dc, old);
    struct {
        BITMAPINFOHEADER h;
        RGBQUAD c[2];
    } bi{};
    bi.h.biSize = sizeof(BITMAPINFOHEADER);
    bi.h.biWidth = 450;
    bi.h.biHeight = -90;
    bi.h.biPlanes = 1;
    bi.h.biBitCount = 1;
    bi.h.biCompression = BI_RGB;
    const int stride = ((450 + 31) / 32) * 4;
    std::vector<unsigned char> bits(static_cast<std::size_t>(stride) * 90);
    const int lines = GetDIBits(dc, bm, 0, 90, bits.data(), reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS);
    DeleteObject(bm);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
    REQUIRE(lines == 90);
    // Colour 1 is white (the mono bitmap's background), colour 0 black.
    const bool one_white = bi.c[1].rgbRed == 0xff && bi.c[1].rgbGreen == 0xff && bi.c[1].rgbBlue == 0xff;
    REQUIRE(one_white);
    const is::gfx::WingMask& m = is::gfx::wing_mask();
    int differ = 0;
    std::string first;
    for (int y = 0; y < 90; ++y)
        for (int x = 0; x < 450; ++x) {
            const bool gdi = ((bits[static_cast<std::size_t>(y) * stride + x / 8] >> (7 - x % 8)) & 1) != 0;
            if (gdi != m.at(x, y)) {
                if (differ == 0) first = "(" + std::to_string(x) + ", " + std::to_string(y) + ")";
                ++differ;
            }
        }
    MESSAGE("the wing mask: " << differ << " bits differ from GDI" << (differ ? ", first at " + first : std::string()));
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    const bool wine = ntdll && GetProcAddress(ntdll, "wine_get_version") != nullptr;
    if (wine) CHECK(differ == 0);
}
#endif
