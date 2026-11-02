// The IS_ICON column window (icon_column.h). Win32 only: the host build compiles this file
// empty.
//
// The column draws into its own 8-bit index image with the GDI calls of seg7:0x64ac and
// seg13:0x5c22 (the strip DGROUP:0x3a02 is an index image too, ADR 0007 decision 2), and the
// image is converted through the game palette onto the window, as the main surface is (ADR
// 0007 decision 3) [DESIGN]. The window keeps the original's pixel sizes; the main window's
// ADR 0006 factor is not applied to it [DESIGN].
#ifdef _WIN32
#include "hud/icon_column.h"
#include "hud/wave.h"

#include <array>
#include <string>
#include <memory>

#include "app/app.h"
#include "gfx/present.h"
#include "gfx/surface.h"
#include "hud/target.h"
#include "shell/game_state.h"
#include "world/hooks.h"
#include "world/world.h"

namespace is::hud {

namespace {

// SetActiveWindow(main) as the original calls it after the column's creation, move and scale
// (seg7:0x649b, seg7:0x6273). On the original's Windows that call returned the input focus to
// main. Under a window manager (Wine on a desktop) the new owned window keeps the X focus,
// GetForegroundWindow stays on the column and the timer's focus test (seg2:0x1327) pauses the
// game until the player clicks main; so the port also restores the foreground window
// [DESIGN ADR 0013 decision 3]. A host without a window manager (headless Wine) leaves main
// active and both calls do nothing.
void reactivate_main() {
    if (!app::g.main) return;
    SetActiveWindow(app::g.main);
    if (GetForegroundWindow() != app::g.main) SetForegroundWindow(app::g.main);
}

}  // namespace

namespace {

using app::g;

constexpr const wchar_t* kClass = L"IS_ICON";  // DGROUP:0x52d / DGROUP:0xb99
constexpr UINT kScaleItem = 0xf0;               // the appended "1/2x Scale" item
constexpr int kRowPitch = 0x2e;                 // 46 [BYTES seg7:0x6525]

bool g_registered = false;
HWND g_wnd = nullptr;         // DGROUP:0x9458
HMENU g_sys = nullptr;        // DGROUP:0x945a
int g_left = 0, g_top = 0;    // DGROUP:0x1de4 / 0x1de6 (IconLeft, IconTop)
bool g_pos_read = false;
bool g_pos_dirty = false;     // DGROUP:0x8fee
// The row marks DGROUP:0x6e38 + 0x1e·(i + 3): +0x14, the glint counter of seg13:0x5a6a.
std::array<int, world::kIconRecords> g_glint{};
gfx::ImagePtr g_image;
std::unique_ptr<gfx::Presenter> g_presenter;

world::World& w() { return world::current(); }
int vscale() { return g.keys.vscale; }  // DGROUP:0xb6e

void read_position() {
    if (g_pos_read) return;
    g_pos_read = true;
    // seg6:0x205-0x245: IconLeft (screen width - 48) and IconTop (0) [BYTES seg6:0x205
    // seg6:0x245].
    g_left = g.screen_cx - 48;
    g_top = 0;
    if (g.ini) {
        g_left = g.ini->get_int(settings::kGameSection, "IconLeft", g_left);
        g_top = g.ini->get_int(settings::kGameSection, "IconTop", 0);
    }
}

// seg7:0x63b6(menu, item, check, text): the "1/2x Scale" check.
void check_scale() {
    if (!g_sys) return;
    ModifyMenuW(g_sys, kScaleItem, MF_BYCOMMAND | MF_STRING | (vscale() ? MF_CHECKED : MF_UNCHECKED),
                kScaleItem, L"1/2x Scale");
}

// One cell: seg13:0x5a6a(hdc, 0, 46i + 4, 0, i, i + 3, flag, VScale) on the column's image.
void draw_row(HDC dc, int i, int flag) {
    world::World& wd = w();
    if (i < 0 || i >= world::kIconRecords || i >= wd.pools.records(0)) return;
    const world::ObjectRecord& o = wd.pools.rec(0, i);
    const world::IconRecord& ic = wd.icons[static_cast<std::size_t>(i)];
    CellInputs in;
    in.x = 0;
    in.y = kRowPitch * i + 4;
    in.pool = 0;
    in.rec = i;
    in.flag = flag;
    in.scale = vscale();
    in.ship_pool = wd.roles.ships;
    in.detail = wd.detail;
    in.obj.state = o.state;
    in.obj.extent = o.extent;
    in.obj.size = o.size;
    in.obj.gap = o.gap;
    in.obj.src_y = o.src_y;
    in.obj.b51 = o.race_status;
    in.icon.w00 = ic.w00;
    in.icon.w02 = ic.w02;
    in.icon.w19 = ic.w19;
    in.kind = target_kind_371c(0, i);  // seg13:0x371c: 0 for pool 0 (M4 s2 DISPLAY)
    fill_fragment_inputs(i, in);       // seg13:0x5f49's inputs (hud/wave.h)
    const CellPlan p = plan_cell(in, g_glint[static_cast<std::size_t>(i)]);
    if (p.clear) PatBlt(dc, p.clear_x, p.clear_y, p.clear_size, p.clear_size, BLACKNESS);
    // The glint (seg13:0x5460, seg13:0x54da) is not drawn [OPEN]; with flag 1 it never runs.
    gfx::ImagePtr strip = world::icon_strip();
    if (p.picture && p.slot == 0xd && strip) {
        SetStretchBltMode(dc, COLORONCOLOR);  // seg13:0x5c98
        StretchBlt(dc, p.dx, p.dy, p.dw, p.dh, strip->hdc(), p.sx, p.sy, p.sw, p.sh, SRCCOPY);
    }
    // A shattered icon's four quarters [BYTES seg13:0x5f49-0x6057] (M4 s2 DISPLAY).
    if (p.sub_cells) {
        for (const CellPlan::SubCell& q : p.subs) {
            if (!q.draw) {
                PatBlt(dc, q.x, q.y, q.side, q.side, BLACKNESS);  // seg13:0x604b
            } else if (gfx::ImagePtr f = fragment_image(q.frag)) {
                SetStretchBltMode(dc, COLORONCOLOR);
                StretchBlt(dc, q.x, q.y, q.side, q.side, f->hdc(), p.frag_sx, p.frag_sx, p.frag_size, p.frag_size,
                           SRCCOPY);  // seg13:0x5ffe
            }
        }
    }
}

// Converts the image onto the window (ADR 0007 decision 3).
void present(HDC wdc) {
    if (!g_image || !g.gfx) return;
    if (!g_presenter) g_presenter = std::make_unique<gfx::Presenter>();
    RECT rc;
    GetClientRect(g_wnd, &rc);
    g_presenter->present(wdc, rc.right, rc.bottom, 1, *g_image, g.gfx->palettes().device(),
                         g.gfx->palettes().generation(), true);
}

bool ensure_image() {
    if (!g.gfx || !g_wnd) return false;
    RECT rc;
    GetClientRect(g_wnd, &rc);
    const int cx = rc.right > 0 ? rc.right : 1, cy = rc.bottom > 0 ? rc.bottom : 1;
    if (!g_image || g_image->width() != cx || g_image->height() != cy) g_image = g.gfx->create_image(cx, cy);
    return g_image != nullptr;
}

// seg7:0x64ac(hdc, flag, row) on the image: every row for row = -1. Returns 0 when the wave
// has no icon column (DGROUP:0x91fc = 0) [BYTES seg7:0x64b9-0x65a3].
int paint_rows(int flag, int row) {
    if (!g_wnd) return 1;  // seg7:0x64b9-0x64c3
    if (ensure_image()) {
        HDC dc = g_image->hdc();
        if (row == -1) {
            for (int i = 0; i < w().icons_c; ++i) draw_row(dc, i, flag);  // seg7:0x6500-0x653a
        } else {
            draw_row(dc, row, flag);  // seg7:0x653c-0x6561
        }
    }
    return w().icons_b != 0 ? 1 : 0;  // seg7:0x658e
}

LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:  // seg7:0x61bf-0x6210
        g_wnd = hwnd;
        g_sys = GetSystemMenu(hwnd, FALSE);
        AppendMenuW(g_sys, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(g_sys, MF_STRING, kScaleItem, L"1/2x Scale");  // DGROUP:0xb80
        check_scale();
        DrawMenuBar(hwnd);
        return 0;
    case WM_MOVE: {  // seg7:0x62a1-0x62d3
        RECT r;
        GetWindowRect(hwnd, &r);
        if (r.left != g_left || r.top != g_top) {
            // CopyRect into DGROUP:0x1de4 and DGROUP:0x8fee = 1; the INI write-back of IconLeft
            // and IconTop is not ported [OPEN].
            g_left = r.left;
            g_top = r.top;
            g_pos_dirty = true;
        }
        // seg7:0x6273: SetActiveWindow(main). The main window's reactivation runs the emulated
        // WM_QUERYNEWPALETTE (ADR 0007 decision 5) [BYTES seg7:0x6273 seg2:0x5cb].
        reactivate_main();
        return 0;
    }
    case WM_SETFOCUS:  // seg7:0x6298: DGROUP:0x8f68 = 1
        g.running = true;
        return 0;
    case WM_PAINT: {  // seg7:0x632b-0x6392
        InvalidateRect(hwnd, nullptr, TRUE);
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        int more = 1;
        if (ensure_image()) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            PatBlt(g_image->hdc(), 0, 0, rc.right, rc.bottom, BLACKNESS);  // seg7:0x6365
            more = paint_rows(1, -1);                                      // seg7:0x6372
            present(dc);
        }
        EndPaint(hwnd, &ps);
        if (more == 0) icon_column_destroy();  // seg7:0x638f
        return 0;
    }
    case WM_ERASEBKGND:  // the class brush BLACK_BRUSH; the paint covers the client [DESIGN]
        return 1;
    case WM_CLOSE:  // seg7:0x6394-0x63a4
        icon_column_destroy();
        return 0;
    case WM_GETMINMAXINFO: {  // seg7:0x62d5-0x6329: a fixed size
        auto* mm = reinterpret_cast<MINMAXINFO*>(lp);
        const int width = 0x32 >> vscale();
        const int rows = w().icons_c;
        const int height = vscale() ? GetSystemMetrics(SM_CYCAPTION) + 0x17 * rows + 3
                                    : GetSystemMetrics(SM_CYCAPTION) + 0x2e * rows + 6;
        mm->ptMinTrackSize.x = mm->ptMaxTrackSize.x = width;
        mm->ptMinTrackSize.y = mm->ptMaxTrackSize.y = height;
        return 0;
    }
    case WM_SYSCOMMAND:  // seg7:0x6213-0x627f
        if ((wp & 0xfff0) == kScaleItem) {
            g.keys.vscale = g.keys.vscale < 1 ? 1 : 0;  // seg7:0x622a-0x6233: DGROUP:0xb6e
            check_scale();
            RECT r;
            GetWindowRect(hwnd, &r);
            MoveWindow(hwnd, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
            reactivate_main();  // seg7:0x6273
            return 0;
        }
        if ((wp & 0xfff0) == SC_CLOSE) g.keys.icon_wnd = 0;  // seg7:0x627f: DGROUP:0x8f72 = 0
        return DefWindowProcW(hwnd, msg, wp, lp);
    case WM_DESTROY:
        if (g_wnd == hwnd) g_wnd = nullptr;
        g_sys = nullptr;
        return 0;
    default: break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

// seg7:0x641a [BYTES seg7:0x6427-0x649f].
void icon_column_create() {
    if (g_wnd || w().icons_b == 0 || !g.main) {  // seg7:0x6427-0x643a (DGROUP:0xa7c: the HUD's)
        if (!g_wnd) app::log("hud: seg7:0x641a: no icon column (DGROUP:0x91fc " + std::to_string(w().icons_b) +
                 (g_wnd ? ", it exists" : "") + ")");
        reactivate_main();
        return;
    }
    register_icon_column(g.instance);
    // seg7:0x643c-0x646c: the rows' marks {2, 46i + 2, i, 0} (origin, record, pool 0); the
    // column's cells take their origin from seg7:0x64ac's arguments, so only the glint counters
    // are kept [DESIGN].
    g_glint.fill(0);
    read_position();
    // CreateWindow("IS_ICON", "", 0x10CC0000, IconLeft, IconTop, 50, 150, main, 0) [BYTES
    // seg7:0x646e-0x6496]: visible, captioned, a system menu and a sizing frame; an owned
    // top-level window. Its activation takes it from main, and seg7:0x649b gives it back
    // [BYTES seg7:0x649b].
    HWND h = CreateWindowExW(0, kClass, L"", WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME, g_left,
                             g_top, 0x32, 0x96, g.main, nullptr, g.instance, nullptr);
    app::log(h ? "hud: seg7:0x641a: IS_ICON at (" + std::to_string(g_left) + ", " + std::to_string(g_top) +
                     "), " + std::to_string(w().icons_c) + " rows"
               : "hud: seg7:0x641a: CreateWindow(IS_ICON) failed, error " + std::to_string(GetLastError()));
    reactivate_main();  // seg7:0x649b
    // On the original's Windows the visible window's creation activated it and this call gave
    // the activation back to main. On a 256-colour display that reactivation brought
    // WM_QUERYNEWPALETTE, whose handler erased the client; the Wine oracle shows that erase
    // at every activation (wave-0001 shots 05-12, the bare display), the real machine does not
    // (win98-0001, the full display from the wave start), and the port draws nothing on an
    // activation (ADR 0007 decision 5, ADR 0013 decision 3) [ORACLE win98-0001] [ORACLE
    // wave-0001] [DESIGN]. The status display seg11:0x3cc2 drew before the post stands.
}

void register_icon_column(HINSTANCE instance) {
    if (g_registered) return;
    // The program registers the seven classes at start-up with DefWindowProc for the windows
    // that come later (src/app/main.cpp register_classes); the column registers IS_ICON again
    // with its procedure, as src/nav does for IS_NAV [DESIGN].
    UnregisterClassW(kClass, instance);
    // seg6:0x152b-0x1579: style 3, seg7:0x6184, LoadIcon("GAME"), no cursor, BLACK_BRUSH.
    WNDCLASSW wc{};
    wc.style = CS_VREDRAW | CS_HREDRAW;
    wc.lpfnWndProc = wndproc;
    wc.hInstance = instance;
    wc.hIcon = g.icon;
    wc.hCursor = nullptr;
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kClass;
    g_registered = RegisterClassW(&wc) != 0;
    if (!g_registered) app::log("hud: RegisterClass(IS_ICON) failed, error " + std::to_string(GetLastError()));
}

bool icon_column_wave_start() {
    // seg13:0x66af-0x66ca: IconWnd != 0 and no column: PostMessage(main, 0x9aa, 0, 0), whose
    // handler seg2:0x4f0 (src/app/wndproc.cpp, app::icon_watch) runs seg7:0x641a.
    if (g.keys.icon_wnd == 0 || g_wnd || !g.main) return false;
    PostMessageW(g.main, app::kMsgIconWatch, 0, 0);
    return true;
}

void icon_column_row(int rec) {
    if (!g_wnd) return;
    // seg7:0x64ac(0, 1, rec): GetDC(DGROUP:0x9458), the row, ReleaseDC; DGROUP:0x91fc = 0 then
    // destroys the window (seg7:0x659e).
    const int more = paint_rows(1, rec);
    if (HDC dc = GetDC(g_wnd)) {
        present(dc);
        ReleaseDC(g_wnd, dc);
    }
    if (more == 0) icon_column_destroy();
}

void icon_column_rows_tick() {
    if (!g_wnd) return;
    const int more = paint_rows(0, -1);
    if (HDC dc = GetDC(g_wnd)) {
        present(dc);
        ReleaseDC(g_wnd, dc);
    }
    if (more == 0) icon_column_destroy();
}

void icon_column_destroy() {
    if (!g_wnd) return;  // seg7:0x63fc
    HWND h = g_wnd;
    g_wnd = nullptr;  // seg7:0x640c
    DestroyWindow(h);
}

bool icon_column_exists() { return g_wnd != nullptr; }

}  // namespace is::hud
#endif
