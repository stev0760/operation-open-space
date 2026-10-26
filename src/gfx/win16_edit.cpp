#include "gfx/win16_edit.h"

#ifdef _WIN32
namespace is::gfx {

namespace {

thread_local HWND t_dialog = nullptr;  // the scope's dialog, once created
thread_local bool t_active = false;    // a scope waits for or holds its dialog
thread_local WNDPROC t_edit_proc = nullptr;

bool is_edit(HWND h) {
    wchar_t name[16] = L"";
    GetClassNameW(h, name, 16);
    return lstrcmpiW(name, L"Edit") == 0;
}

// Sees only the edit's WM_NCCREATE: the CREATESTRUCT the edit reads there and the window's
// styles lose WS_EX_CLIENTEDGE and keep WS_BORDER; then the edit's own procedure returns.
LRESULT CALLBACK nccreate_proc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    const WNDPROC edit = t_edit_proc;
    SetWindowLongPtrW(h, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(edit));
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(l);
        cs->dwExStyle &= ~static_cast<DWORD>(WS_EX_CLIENTEDGE);
        cs->style |= WS_BORDER;
        SetWindowLongPtrW(h, GWL_EXSTYLE,
                          GetWindowLongPtrW(h, GWL_EXSTYLE) & ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE));
        SetWindowLongPtrW(h, GWL_STYLE, GetWindowLongPtrW(h, GWL_STYLE) | WS_BORDER);
    }
    return CallWindowProcW(edit, h, msg, w, l);
}

LRESULT CALLBACK cbt_proc(int code, WPARAM w, LPARAM l) {
    if (code == HCBT_CREATEWND && t_active) {
        const CREATESTRUCTW* cs = reinterpret_cast<CBT_CREATEWNDW*>(l)->lpcs;
        const HWND h = reinterpret_cast<HWND>(w);
        if (!t_dialog) {
            t_dialog = h;  // the first window the scope sees is the dialog itself
        } else if (cs->hwndParent == t_dialog && (cs->dwExStyle & WS_EX_CLIENTEDGE) && is_edit(h)) {
            // The window exists and has had no message yet [REF Win32 SDK CBTProc].
            t_edit_proc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(h, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&nccreate_proc)));
        }
    }
    return CallNextHookEx(nullptr, code, w, l);
}

}  // namespace

Win16EditScope::Win16EditScope() : saved_dialog_(t_dialog), saved_active_(t_active) {
    t_dialog = nullptr;
    t_active = true;
    hook_ = SetWindowsHookExW(WH_CBT, cbt_proc, nullptr, GetCurrentThreadId());
}

Win16EditScope::~Win16EditScope() {
    if (hook_) UnhookWindowsHookEx(hook_);
    t_dialog = saved_dialog_;
    t_active = saved_active_;
}

}  // namespace is::gfx
#endif
