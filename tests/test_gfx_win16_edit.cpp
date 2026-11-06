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

// A nested scope (the Sell dialog #378 over the Ambulance Options dialog #194, both run by
// Dialog::run_modal) must not subclass a control twice: with two hooks the second subclass made
// the edit's saved procedure nccreate_proc itself and the edit's creation overflowed the stack
// (work/world/m4s4/accept/amb.md D1) (M4 s4 fixer B).
TEST_CASE("win16 edit win32: nested scopes give each dialog's edits one subclass") {
    HINSTANCE inst = GetModuleHandleW(nullptr);
    const DWORD style = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL;
    HWND outer = nullptr, inner = nullptr, e_inner = nullptr, e_outer = nullptr;
    {
        is::gfx::Win16EditScope a;
        outer = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 400, 300, nullptr, nullptr, inst, nullptr);
        {
            is::gfx::Win16EditScope b;
            inner = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 300, 200, outer, nullptr, inst, nullptr);
            e_inner = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", style, 10, 10, 120, 20, inner, nullptr,
                                      inst, nullptr);
        }
        // Back in the outer scope: its own dialog's children change again.
        e_outer = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", style, 10, 10, 120, 20, outer, nullptr,
                                  inst, nullptr);
    }
    REQUIRE(e_inner);
    REQUIRE(e_outer);
    for (HWND e : {e_inner, e_outer}) {
        // (the edit takes WS_BORDER off its window style and draws the border itself)
        CHECK((GetWindowLongPtrW(e, GWL_EXSTYLE) & WS_EX_CLIENTEDGE) == 0);
        SetWindowTextW(e, L"7.5");  // the edit's own procedure answers, once
        wchar_t buf[8] = L"";
        GetWindowTextW(e, buf, 8);
        CHECK(lstrcmpW(buf, L"7.5") == 0);
    }
    DestroyWindow(outer);
}

// The list boxes of #194 and #378 are LISTBOX with WS_BORDER; the oracle draws them with a
// 1-pixel border, as the Win16 dialog manager leaves WS_BORDER (amb.md D4) [ORACLE core-0003]
// [BYTES ENG_LANG.DLL DIALOG 194] (M4 s4 fixer B).
TEST_CASE("win16 edit win32: a scoped client-edge list box keeps a 1-pixel border") {
    HINSTANCE inst = GetModuleHandleW(nullptr);
    HWND parent = nullptr, lb = nullptr;
    {
        is::gfx::Win16EditScope scope;
        parent = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 400, 300, nullptr, nullptr, inst, nullptr);
        lb = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY,
                             10, 10, 150, 100, parent, nullptr, inst, nullptr);
    }
    REQUIRE(lb);
    CHECK((GetWindowLongPtrW(lb, GWL_EXSTYLE) & WS_EX_CLIENTEDGE) == 0);
    CHECK((GetWindowLongPtrW(lb, GWL_STYLE) & WS_BORDER) != 0);
    RECT w{}, c{};
    GetWindowRect(lb, &w);
    GetClientRect(lb, &c);
    POINT o{0, 0};
    ClientToScreen(lb, &o);
    CHECK(o.x - w.left == 1);  // 1 px left border, not the 2 px client edge
    CHECK(o.y - w.top == 1);
    CHECK((w.right - w.left) - c.right == 2);
    DestroyWindow(parent);
}
#endif
