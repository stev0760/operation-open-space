// poc_wave: proof-of-concept sandbox for the game's central idea, "a directory is a wave".
//
// NOT PORT BEHAVIOUR. Movement, capture, world size and layout are placeholders until the
// flight model and the wave rules are reverse-engineered (checklist FLT, NAV, WPN). What is
// grounded: the wave's files are the directory's *.exe then *.dll [docs/re/oracle.md], each
// program contributes its icons, the ship is IGRAPHIC.DLL #4000 60 read by the port's DIB
// reader, positions come from the game's rand() [docs/re/rng.md], and frames run at the
// default FrameRateSet of 36 per second as ADR 0001 proposes.
//
//   poc_wave.exe <game directory with IGRAPHIC.DLL> <wave directory>
//
// Arrow keys move the ship; touching an icon removes it; Esc quits.
#include <windows.h>
#include <mmsystem.h>
#include <shellapi.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "formats/dib.h"
#include "formats/ne.h"

using namespace is::fmt;
namespace fs = std::filesystem;

namespace {

constexpr int kViewW = 1024, kViewH = 768;
constexpr int kWorldW = kViewW * 2, kWorldH = kViewH * 2;  // placeholder size, wraps
constexpr int kFrameRate = 36;                             // FrameRateSet default
constexpr int kShipFrames = 4;

// The game's generator: Microsoft C rand() (docs/re/rng.md).
struct OriginalRand {
    std::uint32_t state = 1;
    void srand(std::uint16_t seed) { state = seed; }
    int rand() {
        state = state * 214013u + 2531011u;
        return static_cast<int>((state >> 16) & 0x7FFF);
    }
};

struct Icon {
    HICON icon = nullptr;
    std::wstring file;
    int x = 0, y = 0;
    bool alive = true;
};

struct Game {
    HWND wnd = nullptr;
    HFONT font = nullptr;
    HBITMAP ship = nullptr;
    int ship_w = 0, ship_h = 0;
    std::wstring dir;
    std::vector<Icon> icons;
    std::size_t files = 0;
    double x = kWorldW / 2.0, y = kWorldH / 2.0;
    std::uint32_t frame = 0;
    int captured = 0;
} g;

HBITMAP make_bitmap(const Dib& d) {
    const auto px = to_bgra(d, 15, 0x000000);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = d.width;
    bi.bmiHeader.biHeight = -d.height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP h = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (h && bits) std::memcpy(bits, px.data(), px.size() * sizeof px[0]);
    return h;
}

void list_wave(const std::wstring& dir) {
    OriginalRand rng;
    rng.srand(static_cast<std::uint16_t>(GetTickCount()));
    for (const wchar_t* pattern : {L"*.exe", L"*.dll"}) {
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW((fs::path(dir) / pattern).c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            ++g.files;
            const std::wstring path = (fs::path(dir) / fd.cFileName).wstring();
            const UINT n = ExtractIconExW(path.c_str(), -1, nullptr, nullptr, 0);
            for (UINT i = 0; i < n && g.icons.size() < 400; ++i) {
                HICON big = nullptr;
                if (ExtractIconExW(path.c_str(), static_cast<int>(i), &big, nullptr, 1) == 1 && big) {
                    Icon ic;
                    ic.icon = big;
                    ic.file = fd.cFileName;
                    ic.x = rng.rand() % (kWorldW - 64) + 32;
                    ic.y = rng.rand() % (kWorldH - 64) + 32;
                    g.icons.push_back(ic);
                }
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
}

double wrap(double v, double size) {
    v = std::fmod(v, size);
    return v < 0 ? v + size : v;
}

// One frame of placeholder logic.
void step() {
    const double speed = 6.0;
    if (GetAsyncKeyState(VK_LEFT) & 0x8000) g.x -= speed;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) g.x += speed;
    if (GetAsyncKeyState(VK_UP) & 0x8000) g.y -= speed;
    if (GetAsyncKeyState(VK_DOWN) & 0x8000) g.y += speed;
    g.x = wrap(g.x, kWorldW);
    g.y = wrap(g.y, kWorldH);
    for (Icon& ic : g.icons) {
        if (!ic.alive) continue;
        double dx = std::fabs(ic.x + 16 - g.x), dy = std::fabs(ic.y + 16 - g.y);
        dx = std::fmin(dx, kWorldW - dx);
        dy = std::fmin(dy, kWorldH - dy);
        if (dx < 24 && dy < 24) {
            ic.alive = false;
            ++g.captured;
        }
    }
    ++g.frame;
}

void draw(HDC dc) {
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP buf = CreateCompatibleBitmap(dc, kViewW, kViewH);
    HGDIOBJ old = SelectObject(mem, buf);
    RECT all{0, 0, kViewW, kViewH};
    FillRect(mem, &all, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

    // camera centred on the ship; draw each icon at its nearest wrapped position
    const double cx = g.x - kViewW / 2.0, cy = g.y - kViewH / 2.0;
    for (const Icon& ic : g.icons) {
        if (!ic.alive) continue;
        const double sx = wrap(ic.x - cx + kWorldW / 2.0, kWorldW) - kWorldW / 2.0;
        const double sy = wrap(ic.y - cy + kWorldH / 2.0, kWorldH) - kWorldH / 2.0;
        if (sx > -32 && sx < kViewW && sy > -32 && sy < kViewH)
            DrawIconEx(mem, static_cast<int>(sx), static_cast<int>(sy), ic.icon, 32, 32, 0, nullptr, DI_NORMAL);
    }

    if (g.ship) {
        HDC src = CreateCompatibleDC(dc);
        HGDIOBJ o = SelectObject(src, g.ship);
        const int fw = g.ship_w / kShipFrames;
        const int f = static_cast<int>((g.frame / 4) % kShipFrames);
        TransparentBlt(mem, kViewW / 2 - fw, kViewH / 2 - g.ship_h, fw * 2, g.ship_h * 2, src, f * fw, 0, fw,
                       g.ship_h, RGB(0, 0, 0));
        SelectObject(src, o);
        DeleteDC(src);
    }

    SetBkMode(mem, TRANSPARENT);
    SelectObject(mem, g.font);
    SetTextColor(mem, RGB(0x60, 0xFF, 0x60));
    wchar_t line[400];
    int alive = 0;
    for (const Icon& ic : g.icons) alive += ic.alive;
    std::swprintf(line, 400, L"WAVE: %ls   %zu icons in %zu files   left %d   taken %d   frame %u",
                  g.dir.c_str(), g.icons.size(), g.files, alive, g.captured, g.frame);
    TextOutW(mem, 8, 6, line, static_cast<int>(std::wcslen(line)));
    SetTextColor(mem, RGB(0x80, 0x80, 0x80));
    const wchar_t* note = L"Proof-of-concept sandbox: movement and capture are placeholders, not the original's rules.";
    TextOutW(mem, 8, kViewH - 22, note, static_cast<int>(std::wcslen(note)));

    BitBlt(dc, 0, 0, kViewW, kViewH, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(buf);
    DeleteDC(mem);
}

LRESULT CALLBACK wnd_proc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_KEYDOWN:
        if (w == VK_ESCAPE) DestroyWindow(h);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        draw(dc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, w, l);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    if (__argc < 3) {
        MessageBoxW(nullptr, L"Usage: poc_wave <game directory> <wave directory>", L"poc_wave", MB_ICONINFORMATION);
        return 2;
    }
    try {
        NeModule gfx = NeModule::load(fs::path(__wargv[1]) / L"IGRAPHIC.DLL");
        const NeResource* r = gfx.find(4000, 60);
        if (!r) throw FormatError("IGRAPHIC.DLL has no #4000 60");
        Dib d = parse_dib(gfx.data(*r), gfx.size(*r));
        g.ship = make_bitmap(d);
        g.ship_w = d.width;
        g.ship_h = d.height;
    } catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "poc_wave", MB_ICONERROR);
        return 1;
    }
    g.dir = __wargv[2];
    list_wave(g.dir);

    g.font = CreateFontW(-14, 0, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET, 0, 0, NONANTIALIASED_QUALITY,
                         FIXED_PITCH, L"Courier New");
    WNDCLASSW wc{};
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    wc.lpszClassName = L"PocWave";
    RegisterClassW(&wc);
    RECT rc{0, 0, kViewW, kViewH};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g.wnd = CreateWindowExW(0, wc.lpszClassName, L"poc_wave — a directory as a wave", WS_OVERLAPPEDWINDOW,
                            CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr,
                            nullptr, inst, nullptr);
    ShowWindow(g.wnd, show);

    // Fixed frame schedule, no catch-up (ADR 0001, Proposed).
    timeBeginPeriod(1);
    LARGE_INTEGER freq, t0, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);
    const double period = 1000.0 / kFrameRate;
    double due = 0;  // ms after t0 when the next frame runs
    MSG m;
    for (;;) {
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
            if (m.message == WM_QUIT) {
                timeEndPeriod(1);
                return 0;
            }
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
        QueryPerformanceCounter(&now);
        const double ms = static_cast<double>(now.QuadPart - t0.QuadPart) * 1000.0 / static_cast<double>(freq.QuadPart);
        if (ms >= due) {
            step();
            InvalidateRect(g.wnd, nullptr, FALSE);
            UpdateWindow(g.wnd);
            due += period;
            if (ms > due) due = ms + period;  // late: restart the schedule, no catch-up
        } else {
            Sleep(static_cast<DWORD>(std::min(due - ms, 5.0)));
        }
    }
}
