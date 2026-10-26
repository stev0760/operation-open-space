// The LevClass frame as GDI draws it (src/dialogs/dialogs.cpp paint_frame) against the computed
// picture of src/dialogs/preview.h, pixel for pixel on a 32 bpp DIB section. Win32 only.
#ifdef _WIN32
#include <windows.h>

#include "dialogs/dialogs.h"
#include "dialogs/preview.h"
#include "doctest.h"

TEST_CASE("dialogs preview win32: GDI's LevClass frame equals lev_frame") {
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = 96;
    bi.bmiHeader.biHeight = -96;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bm = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    REQUIRE(bm);
    HGDIOBJ old = SelectObject(dc, bm);
    PatBlt(dc, 0, 0, 96, 96, BLACKNESS);
    RECT r{0, 0, 96, 96};
    is::dialogs::paint_frame(dc, r);
    GdiFlush();
    const is::dialogs::Image want = is::dialogs::lev_frame(96, 96);
    const auto* px = static_cast<const std::uint32_t*>(bits);
    int differ = 0;
    for (int i = 0; i < 96 * 96; ++i) differ += (px[i] & 0xFFFFFF) != want.px[static_cast<std::size_t>(i)];
    CHECK(differ == 0);
    SelectObject(dc, old);
    DeleteObject(bm);
    DeleteDC(dc);
}
#endif
