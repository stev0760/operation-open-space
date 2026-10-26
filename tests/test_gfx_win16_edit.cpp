// The Win16 look of WS_BORDER edits (src/gfx/win16_edit.h): under Win32, the formatting
// rectangle of a client-edge edit created inside a Win16EditScope against that of an edit
// created self-bordered (WS_BORDER without WS_EX_CLIENTEDGE, as the Win16 dialog manager
// creates it). Win32 only.
#include "gfx/win16_edit.h"

#include <initializer_list>

#include "doctest.h"

#ifdef _WIN32
namespace {

RECT text_rect_in_window(HWND edit) {
    RECT f{};
    SendMessageW(edit, EM_GETRECT, 0, reinterpret_cast<LPARAM>(&f));
    POINT o{0, 0};
    ClientToScreen(edit, &o);
    RECT w{};
    GetWindowRect(edit, &w);
    OffsetRect(&f, o.x - w.left, o.y - w.top);
    return f;
}

}  // namespace

TEST_CASE("win16 edit win32: a scoped client-edge edit places its text as a self-bordered one") {
    HFONT font = CreateFontW(14, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0x22, L"Arial");
    for (const int h : {20, 19, 16, 120}) {
        const DWORD style = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL |
                            (h > 40 ? ES_MULTILINE : 0);
        HWND ref_parent = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 400, 300, nullptr,
                                          nullptr, GetModuleHandleW(nullptr), nullptr);
        HWND ref = CreateWindowExW(0, L"EDIT", L"text", style, 10, 10, 200, h, ref_parent,
                                   nullptr, GetModuleHandleW(nullptr), nullptr);
        HWND parent = nullptr, port = nullptr, other = nullptr;
        {
            is::gfx::Win16EditScope scope;
            parent = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 400, 300, nullptr, nullptr,
                                     GetModuleHandleW(nullptr), nullptr);
            port = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"text", style, 10, 10, 200, h,
                                   parent, nullptr, GetModuleHandleW(nullptr), nullptr);
            // only the scope's dialog's children change
            other = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"text", style, 10, 10, 200, h,
                                    ref_parent, nullptr, GetModuleHandleW(nullptr), nullptr);
        }
        REQUIRE(ref);
        REQUIRE(port);
        REQUIRE(other);
        SendMessageW(ref, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
        SendMessageW(port, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
        const RECT a = text_rect_in_window(ref), b = text_rect_in_window(port);
        INFO("height " << h << ": self-bordered " << a.left << "," << a.top << " scoped "
                       << b.left << "," << b.top);
        CHECK(a.left == b.left);
        CHECK(a.top == b.top);
        CHECK((GetWindowLongPtrW(port, GWL_EXSTYLE) & WS_EX_CLIENTEDGE) == 0);
        CHECK((GetWindowLongPtrW(other, GWL_EXSTYLE) & WS_EX_CLIENTEDGE) != 0);
        DestroyWindow(parent);
        DestroyWindow(ref_parent);
    }
    DeleteObject(font);
}
#endif
