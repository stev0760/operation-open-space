// isview: shows the original game's assets straight from its Win16 DLLs, through the
// port's own readers. The menu bar and accelerators are ENG_LANG.DLL's MENU 101 and
// MAINACCEL, converted to Win32; the list opens any of its 22 dialogs, converted from
// the 16-bit templates; the right side draws IGRAPHIC.DLL's sprite DIBs.
//
//   isview.exe <directory with ENG_LANG.DLL and IGRAPHIC.DLL> [--dialog <id>]
//
// --dialog opens that dialog modeless at start-up (for screenshots).
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "formats/dib.h"
#include "formats/res16.h"

using namespace is::fmt;
namespace fs = std::filesystem;

namespace {

constexpr COLORREF kBg = RGB(0x10, 0x18, 0x28);
constexpr std::uint32_t kBgRgb = 0x101828;
constexpr COLORREF kText = RGB(0xD8, 0xE0, 0xF0);
constexpr COLORREF kDim = RGB(0x80, 0x90, 0xA8);
constexpr int kListWidth = 250;
constexpr int kXformFrames = 4;  // IGRAPHIC #4000 60 is a 4-frame strip [docs/re/resources.md]
constexpr UINT_PTR kAnimTimer = 1;

struct Sprite {
    std::uint16_t id = 0;
    int w = 0, h = 0;
    HBITMAP bmp = nullptr;
};

struct DialogEntry {
    std::uint16_t id = 0;
    std::wstring caption;
    std::vector<std::uint8_t> tmpl;  // Win32 DLGTEMPLATE
};

struct App {
    HINSTANCE inst = nullptr;
    HWND main = nullptr, list = nullptr, modeless = nullptr;
    HFONT font = nullptr, small = nullptr;
    HMENU menu = nullptr;
    HACCEL accel = nullptr;
    std::wstring dir, title, status = L"Menu and accelerator commands appear here.";
    std::unique_ptr<NeModule> lang, gfx;
    std::vector<DialogEntry> dialogs;
    std::vector<Sprite> sprites;
    const Sprite* xform = nullptr;
    int frame = 0;
} g;

std::wstring widen(const std::string& s) {
    const std::u16string u = cp1252_to_utf16(s);
    return std::wstring(u.begin(), u.end());
}

HBITMAP make_bitmap(const Dib& d) {
    const auto px = to_bgra(d, d.bpp == 4 ? 15 : -1, kBgRgb);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = d.width;
    bi.bmiHeader.biHeight = -d.height;  // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP h = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (h && bits) std::memcpy(bits, px.data(), px.size() * sizeof px[0]);
    return h;
}

bool load(std::wstring& err) {
    try {
        const fs::path dir(g.dir);
        g.lang = std::make_unique<NeModule>(NeModule::load(dir / L"ENG_LANG.DLL"));
        g.gfx = std::make_unique<NeModule>(NeModule::load(dir / L"IGRAPHIC.DLL"));
        if (auto t = load_string(*g.lang, 2338)) g.title = widen(*t);

        for (const NeResource* r : g.lang->all(RT16_DIALOG)) {
            Dialog16 d = parse_dialog16(g.lang->data(*r), g.lang->size(*r));
            g.dialogs.push_back({r->id, widen(d.caption), dialog_to_win32(d)});
        }
        std::sort(g.dialogs.begin(), g.dialogs.end(),
                  [](const DialogEntry& a, const DialogEntry& b) { return a.id < b.id; });

        if (const NeResource* r = g.lang->find(RT16_MENU, 101)) {
            const auto t = menu_to_win32(parse_menu16(g.lang->data(*r), g.lang->size(*r)));
            g.menu = LoadMenuIndirectW(reinterpret_cast<const MENUTEMPLATEW*>(t.data()));
        }
        if (const NeResource* r = g.lang->find(RT16_ACCELERATOR, "MAINACCEL")) {
            std::vector<ACCEL> a;
            for (const Accel16& e : parse_accel16(g.lang->data(*r), g.lang->size(*r)))
                a.push_back(ACCEL{static_cast<BYTE>(e.flags), e.key, e.cmd});
            g.accel = CreateAcceleratorTableW(a.data(), static_cast<int>(a.size()));
        }

        for (const NeResource* r : g.gfx->all(4000)) {
            Dib d = parse_dib(g.gfx->data(*r), g.gfx->size(*r));
            g.sprites.push_back({r->id, d.width, d.height, make_bitmap(d)});
        }
        std::sort(g.sprites.begin(), g.sprites.end(),
                  [](const Sprite& a, const Sprite& b) { return a.id < b.id; });
        for (const Sprite& s : g.sprites)
            if (s.id == 60) g.xform = &s;
    } catch (const std::exception& e) {
        err = widen(e.what());
        return false;
    }
    return true;
}

void close_dialog(HWND h) {
    if (GetWindowLongPtrW(h, DWLP_USER)) {
        DestroyWindow(h);
    } else {
        EndDialog(h, 0);
    }
}

// Shows the converted dialog as is; any push button closes it.
INT_PTR CALLBACK dialog_proc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_INITDIALOG:
        SetWindowLongPtrW(h, DWLP_USER, l);
        return TRUE;
    case WM_COMMAND:
        if (HIWORD(w) == BN_CLICKED) {
            const auto type = GetWindowLongW(reinterpret_cast<HWND>(l), GWL_STYLE) & BS_TYPEMASK;
            if (LOWORD(w) == IDOK || LOWORD(w) == IDCANCEL || type == BS_PUSHBUTTON ||
                type == BS_DEFPUSHBUTTON)
                close_dialog(h);
        }
        return TRUE;
    case WM_CLOSE:
        close_dialog(h);
        return TRUE;
    case WM_DESTROY:
        if (h == g.modeless) g.modeless = nullptr;
        return FALSE;
    }
    return FALSE;
}

HWND open_dialog(std::size_t i, bool modeless) {
    const auto* t = reinterpret_cast<const DLGTEMPLATE*>(g.dialogs[i].tmpl.data());
    if (modeless) return CreateDialogIndirectParamW(g.inst, t, g.main, dialog_proc, 1);
    DialogBoxIndirectParamW(g.inst, t, g.main, dialog_proc, 0);
    return nullptr;
}

void text(HDC dc, int x, int y, const std::wstring& s, COLORREF c) {
    SetTextColor(dc, c);
    TextOutW(dc, x, y, s.c_str(), static_cast<int>(s.size()));
}

void blit(HDC dst, HDC src, const Sprite& s, int x, int y, int w, int h, int sx, int sw) {
    SelectObject(src, s.bmp);
    StretchBlt(dst, x, y, w, h, src, sx, 0, sw, s.h, SRCCOPY);
}

void paint(HDC dc, const RECT& rc) {
    const int w = rc.right - kListWidth, h = rc.bottom;
    if (w <= 0 || h <= 0) return;
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP buf = CreateCompatibleBitmap(dc, w, h);
    HGDIOBJ old_buf = SelectObject(mem, buf);
    RECT all{0, 0, w, h};
    HBRUSH bg = CreateSolidBrush(kBg);
    FillRect(mem, &all, bg);
    DeleteObject(bg);
    SetBkMode(mem, TRANSPARENT);
    SetStretchBltMode(mem, COLORONCOLOR);
    HGDIOBJ old_font = SelectObject(mem, g.font);
    HDC src = CreateCompatibleDC(dc);
    HGDIOBJ old_src = SelectObject(src, GetStockObject(BLACK_PEN));

    const int x0 = 16;
    int y = 12;
    text(mem, x0, y, L"Operation: Inner Space, read from the original Win16 DLLs by the port's C++ readers", kText);
    y += 20;
    text(mem, x0, y, L"Source: " + g.dir, kDim);
    y += 18;
    text(mem, x0, y, L"Main window title, ENG_LANG.DLL string 2338: “" + g.title + L"”", kDim);
    y += 18;
    text(mem, x0, y, g.status, kDim);
    y += 32;

    if (g.xform) {
        text(mem, x0, y, L"IGRAPHIC.DLL #4000 60: the Transformer, 4 animation frames (animated, ×4), and the strip (×2)", kText);
        y += 22;
        const int fw = g.xform->w / kXformFrames;
        blit(mem, src, *g.xform, x0, y, fw * 4, g.xform->h * 4, g.frame * fw, fw);
        blit(mem, src, *g.xform, x0 + fw * 4 + 32, y + g.xform->h, g.xform->w * 2, g.xform->h * 2, 0,
             g.xform->w);
        y += g.xform->h * 4 + 24;
    }

    text(mem, x0, y,
         L"IGRAPHIC.DLL #4000: all " + std::to_wstring(g.sprites.size()) +
             L" sprite DIBs, 4 bpp with their own palettes, index 15 drawn transparent",
         kText);
    y += 22;
    SelectObject(mem, g.small);
    const int cell = 60, box_w = 54, box_h = 42;
    const int per_row = std::max(1, (w - 2 * x0) / cell);
    for (std::size_t i = 0; i < g.sprites.size(); ++i) {
        const Sprite& s = g.sprites[i];
        const int cx = x0 + static_cast<int>(i % per_row) * cell;
        const int cy = y + static_cast<int>(i / per_row) * (box_h + 16);
        double k = std::min(static_cast<double>(box_w) / s.w, static_cast<double>(box_h) / s.h);
        if (k >= 1) k = std::floor(k);
        const int dw = std::max(1, static_cast<int>(s.w * k)), dh = std::max(1, static_cast<int>(s.h * k));
        blit(mem, src, s, cx + (box_w - dw) / 2, cy + (box_h - dh) / 2, dw, dh, 0, s.w);
        text(mem, cx + 2, cy + box_h + 1, std::to_wstring(s.id), kDim);
    }

    SelectObject(src, old_src);
    DeleteDC(src);
    SelectObject(mem, old_font);
    BitBlt(dc, kListWidth, 0, w, h, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old_buf);
    DeleteObject(buf);
    DeleteDC(mem);
}

LRESULT CALLBACK wnd_proc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_CREATE: {
        g.list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", nullptr,
                                 WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                                 0, 0, kListWidth, 100, h, nullptr, g.inst, nullptr);
        SendMessageW(g.list, WM_SETFONT, reinterpret_cast<WPARAM>(g.font), FALSE);
        for (const auto& d : g.dialogs) {
            wchar_t line[160];
            std::swprintf(line, 160, L"%4u  %ls", d.id, d.caption.c_str());
            SendMessageW(g.list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line));
        }
        SetTimer(h, kAnimTimer, 160, nullptr);
        return 0;
    }
    case WM_SIZE:
        MoveWindow(g.list, 0, 0, kListWidth, HIWORD(l), TRUE);
        return 0;
    case WM_COMMAND:
        if (reinterpret_cast<HWND>(l) == g.list) {
            if (HIWORD(w) == LBN_DBLCLK) {
                const LRESULT i = SendMessageW(g.list, LB_GETCURSEL, 0, 0);
                if (i >= 0 && static_cast<std::size_t>(i) < g.dialogs.size())
                    open_dialog(static_cast<std::size_t>(i), false);
            }
        } else if (l == 0) {
            wchar_t s[120];
            std::swprintf(s, 120, L"Last %ls command: %u (0x%x)", HIWORD(w) ? L"accelerator" : L"menu",
                          LOWORD(w), LOWORD(w));
            g.status = s;
            InvalidateRect(h, nullptr, FALSE);
        }
        return 0;
    case WM_TIMER:
        g.frame = (g.frame + 1) % kXformFrames;
        InvalidateRect(h, nullptr, FALSE);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        paint(dc, rc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(h, kAnimTimer);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, w, l);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    g.inst = inst;
    int start_dialog = -1;
    for (int i = 1; i < __argc; ++i) {
        const std::wstring a = __wargv[i];
        if (a == L"--dialog" && i + 1 < __argc) {
            start_dialog = std::wcstol(__wargv[++i], nullptr, 10);
        } else if (g.dir.empty()) {
            g.dir = a;
        }
    }
    if (g.dir.empty()) {
        MessageBoxW(nullptr, L"Usage: isview <game directory> [--dialog <id>]", L"isview", MB_ICONINFORMATION);
        return 2;
    }
    std::wstring err;
    if (!load(err)) {
        MessageBoxW(nullptr, (L"Cannot read the original files:\n" + err).c_str(), L"isview", MB_ICONERROR);
        return 1;
    }

    NONCLIENTMETRICSW ncm{};
    ncm.cbSize = sizeof ncm;
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0);
    g.font = CreateFontIndirectW(&ncm.lfMessageFont);
    LOGFONTW lf = ncm.lfMessageFont;
    lf.lfHeight = lf.lfHeight * 4 / 5;
    g.small = CreateFontIndirectW(&lf);

    WNDCLASSW wc{};
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    wc.lpszClassName = L"ISView";
    RegisterClassW(&wc);
    g.main = CreateWindowExW(0, wc.lpszClassName, (L"isview — " + g.title).c_str(),
                             WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 1180,
                             900, nullptr, g.menu, inst, nullptr);
    ShowWindow(g.main, show);
    UpdateWindow(g.main);

    if (start_dialog >= 0)
        for (std::size_t i = 0; i < g.dialogs.size(); ++i)
            if (g.dialogs[i].id == start_dialog) g.modeless = open_dialog(i, true);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        if (g.modeless && IsDialogMessageW(g.modeless, &m)) continue;
        if (g.accel && TranslateAcceleratorW(g.main, g.accel, &m)) continue;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return 0;
}
