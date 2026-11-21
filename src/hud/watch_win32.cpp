// The watch windows IS_WATCH and IS_SMART (watch.h). Win32 only: the host build compiles this
// file empty.
//
// As the IS_ICON column does (icon_column_win32.cpp), each window draws into its own 8-bit index
// image with the original's GDI calls and the image is converted through the game palette onto
// the window (ADR 0007 decisions 2 and 3); the windows keep the original's pixel sizes, without
// the main window's ADR 0006 factor [DESIGN]. The palette selection of seg13:0x50b0 and
// seg13:0x6450 (SelectPalette(DGROUP:0x920e)) is that conversion's.
#ifdef _WIN32
#include "hud/watch.h"

#include <windows.h>

#include <array>
#include <memory>
#include <set>
#include <string>

#include "app/app.h"
#include "formats/bytes.h"
#include "gfx/gfx.h"
#include "gfx/launch_ship.h"
#include "gfx/present.h"
#include "gfx/surface.h"
#include "hud/icon_column.h"
#include "hud/program.h"
#include "hud/wave.h"
#include "settings/settings.h"
#include "shell/game_state.h"
#include "world/flight_hooks.h"
#include "world/hooks.h"
#include "world/icon_mover.h"
#include "world/missile.h"
#include "world/relations.h"
#include "world/world.h"

namespace is::hud {

namespace {

using app::g;

constexpr const wchar_t* kWatchClass = L"IS_WATCH";  // DGROUP:0x138d
constexpr const wchar_t* kSmartClass = L"IS_SMART";  // DGROUP:0x1397
constexpr UINT kScaleItem = 0xf0;                    // "1/2x Scale" (DGROUP:0xb80)
constexpr int kWatchExtra = 0x36;                    // SM_CYCAPTION + 54 [BYTES seg13:0x58b4]

world::World& w() { return world::current(); }

std::set<std::string> g_logged;
void todo_once(const std::string& what, const char* address) {
    if (g_logged.insert(std::string(address) + what).second) app::log_todo(what, address);
}

// SetActiveWindow(main), and the foreground window back to main under a window manager, as the
// icon column does [DESIGN ADR 0013 decision 3].
void reactivate_main() {
    if (!app::g.main) return;
    SetActiveWindow(app::g.main);
    if (GetForegroundWindow() != app::g.main) SetForegroundWindow(app::g.main);
}

// ---- the DGROUP words of the two windows
HWND g_watch = nullptr;  // DGROUP:0x95fc
HWND g_smart = nullptr;  // DGROUP:0x95fe
HMENU g_smart_sys = nullptr;
WatchState g_ws;
ActivityState g_text;   // DGROUP:0x8694 / 0x86d0 and the other text buffers
Mark g_mark_fallback;   // the watch mark before the HUD exists [DESIGN]
// DGROUP:0x1ddc ... 0x1de2 (WatchLeft, WatchTop, WatchRight, the bottom CopyRect stores) and
// DGROUP:0x1dec / 0x1dee (SmLeft, SmTop), read from the INI at first use [BYTES seg6:0x1c2-0x2a4].
RECT g_watch_rect{};
int g_sm_left = 0, g_sm_top = 0;
bool g_keys_read = false;
WatchKeys g_keys;
bool g_watch_registered = false, g_smart_registered = false;

void read_keys() {
    if (g_keys_read) return;
    g_keys_read = true;
    int l = 0, t = 0, r = 280, sl = 0, st = 0;
    if (g.ini) {
        l = g.ini->get_int(settings::kGameSection, "WatchLeft", 0);    // seg6:0x1c2
        t = g.ini->get_int(settings::kGameSection, "WatchTop", 0);     // seg6:0x1d8
        r = g.ini->get_int(settings::kGameSection, "WatchRight", l + 280);  // seg6:0x1e8: WatchLeft + 280
        sl = g.ini->get_int(settings::kGameSection, "SmLeft", 0);      // seg6:0x28e
        st = g.ini->get_int(settings::kGameSection, "SmTop", 0);       // seg6:0x2a4
    }
    g_watch_rect = RECT{l, t, r, 0};
    g_sm_left = sl;
    g_sm_top = st;
    g_ws.half = g.keys.sscale;  // DGROUP:0x95f8, INI SScale
}

// ---- the window's canvas: hud::Canvas on one 8-bit image (Img::kScreen) and the source images
class ImageCanvas final : public Canvas {
public:
    gfx::ImagePtr screen;
    std::array<gfx::ImagePtr, static_cast<std::size_t>(Img::kCount)> src{};

    HDC dc(Img i) const {
        if (i == Img::kScreen) return screen ? screen->hdc() : nullptr;
        const auto& p = src[static_cast<std::size_t>(i)];
        return p ? p->hdc() : nullptr;
    }
    bool has(Img i) const override { return dc(i) != nullptr; }
    int width(Img i) const override {
        const auto& p = i == Img::kScreen ? screen : src[static_cast<std::size_t>(i)];
        return p ? p->width() : 0;
    }
    int height(Img i) const override {
        const auto& p = i == Img::kScreen ? screen : src[static_cast<std::size_t>(i)];
        return p ? p->height() : 0;
    }
    void create(Img, int, int) override {}
    HBRUSH brush(const Brush& b, bool* owned) const {
        *owned = false;
        if (b.kind == Brush::Kind::kSolid && g.gfx) {
            *owned = true;
            return CreateSolidBrush(g.gfx->dc_colour(b.colour));
        }
        if (b.kind == Brush::Kind::kChecker) {
            *owned = true;
            return gfx::checker_brush();
        }
        return nullptr;
    }
    void pat_blt(Img d, int x, int y, int cx, int cy, std::uint32_t rop, const Brush& b) override {
        HDC h = dc(d);
        if (!h) return;
        bool owned = false;
        HBRUSH br = brush(b, &owned);
        HGDIOBJ old = br ? SelectObject(h, br) : nullptr;
        PatBlt(h, x, y, cx, cy, rop);
        if (old) SelectObject(h, old);
        if (owned && br) DeleteObject(br);
    }
    void bit_blt(Img d, int x, int y, int cx, int cy, Img s, int sx, int sy, std::uint32_t rop,
                 const Brush&) override {
        HDC a = dc(d), b = dc(s);
        if (a && b) BitBlt(a, x, y, cx, cy, b, sx, sy, rop);
    }
    void lines(Img d, const Pen& pen, const Point* p, int n) override {
        HDC h = dc(d);
        if (!h || n < 1 || !g.gfx) return;
        const COLORREF c = pen.is_stock ? (pen.stock == kWhitePen ? RGB(255, 255, 255) : RGB(0, 0, 0)) : pen.colour;
        HPEN hp = CreatePen(PS_SOLID, 1, g.gfx->dc_colour(c));
        HGDIOBJ old = SelectObject(h, hp);
        MoveToEx(h, p[0].x, p[0].y, nullptr);
        for (int i = 1; i < n; ++i) LineTo(h, p[i].x, p[i].y);
        SelectObject(h, old);
        DeleteObject(hp);
    }
    void rectangle(Img, const Pen&, const Brush&, int, int, int, int) override {}
    void set_pixel(Img d, int x, int y, Colour c) override {
        if (HDC h = dc(d); h && g.gfx) SetPixel(h, x, y, g.gfx->dc_colour(c));
    }
    void stretch_blt(Img d, int x, int y, int cx, int cy, Img s, int sx, int sy, int sw, int sh,
                     std::uint32_t rop) override {
        HDC a = dc(d), b = dc(s);
        if (!a || !b) return;
        const int mode = SetStretchBltMode(a, COLORONCOLOR);  // seg13:0x5c98
        StretchBlt(a, x, y, cx, cy, b, sx, sy, sw, sh, rop);
        SetStretchBltMode(a, mode);
    }
    void frame(Img, int, int, int, int, int) override {}
    void font_text(int, int, Colour, int, int, const std::string&) override {}
    // TextOut in the HUD font DGROUP:0x90fe with SetTextAlign(0) and SetBkColor(0) on a DC whose
    // background mode stays OPAQUE [BYTES seg13:0x5251-0x5277]: black cells over the black strip
    // the painter has just blacked, so the port draws transparent [DESIGN].
    void text_out(Img d, int x, int y, unsigned align, Colour c, const std::string& text) override {
        HDC h = dc(d);
        if (!h || text.empty() || !g.gfx) return;
        const int saved = SaveDC(h);
        if (g.font) SelectObject(h, g.font);
        SetBkMode(h, TRANSPARENT);
        SetTextAlign(h, align);
        SetTextColor(h, g.gfx->dc_colour(c));
        const std::u16string u = fmt::cp1252_to_utf16(text);
        TextOutW(h, x, y, reinterpret_cast<const wchar_t*>(u.c_str()), static_cast<int>(u.size()));
        RestoreDC(h, saved);
    }
    void set_view(int, int, int, int) override {}
    void clear_view() override {}
};

struct Surface {
    ImageCanvas canvas;
    std::unique_ptr<gfx::Presenter> presenter;

    bool ensure(HWND hwnd) {
        if (!g.gfx || !hwnd) return false;
        RECT rc;
        GetClientRect(hwnd, &rc);
        const int cx = rc.right > 0 ? rc.right : 1, cy = rc.bottom > 0 ? rc.bottom : 1;
        if (!canvas.screen || canvas.screen->width() != cx || canvas.screen->height() != cy) {
            canvas.screen = g.gfx->create_image(cx, cy);  // black
        }
        return canvas.screen != nullptr;
    }
    void present(HWND hwnd, HDC dc, bool force) {
        if (!canvas.screen || !g.gfx) return;
        if (!presenter) presenter = std::make_unique<gfx::Presenter>();
        RECT rc;
        GetClientRect(hwnd, &rc);
        presenter->present(dc, rc.right, rc.bottom, 1, *canvas.screen, g.gfx->palettes().device(),
                           g.gfx->palettes().generation(), force);
    }
};
Surface g_watch_surface, g_smart_surface;

// ---- the program's hosts

std::string lang_string(int id) {
    try {
        if (g.store)
            if (auto s = g.store->find_string(static_cast<std::uint16_t>(id))) return *s;
    } catch (const std::exception& x) {
        app::log("watch: string " + std::to_string(id) + ": " + x.what());
    }
    return {};
}

class ProgramActivityHost final : public ActivityHost {
public:
    bool load(int id, std::string& buf, int cap) override {
        try {
            if (g.store)
                if (auto s = g.store->find_string(static_cast<std::uint16_t>(id))) {
                    buf = *s;
                    if (cap > 0 && static_cast<int>(buf.size()) > cap - 1) buf.resize(static_cast<std::size_t>(cap - 1));
                    return true;
                }
        } catch (const std::exception&) {
        }
        // Win16 LoadString with a missing string block returns 0 and leaves the buffer; the
        // oracle shot that would show it is owed (s4-commands-verify.md claim 68) [DESIGN].
        return false;
    }
    std::uint32_t now() const override { return g.game.now(); }
    int design_flags44(int design) const override {
        if (watch_links().design_flags44) return watch_links().design_flags44(design);
        todo_once("the design byte +0x44 (body byte +9 bit 0x20) of the ships' names", "seg14:0x1efd");
        return 0;
    }
    int w967a() const override {  // [BYTES seg6:0x17ad-0x17cc]
        const std::string s = lang_string(0xb56);
        return !s.empty() && s[0] == '2' ? 1 : 0;
    }
    int missile_weapon(int rec) const override { return w().pools.missile(rec).weapon; }
    int weapon_name(int wpn) const override { return world::missile_state().words.word(wpn, 0); }
    void think(int s) override {
        if (watch_links().think) return watch_links().think(s);
        todo_once("seg4:0x78 from the demo's give-up in seg3:0x1d0a", "seg3:0x2226");
    }
    void todo(const std::string& what, const char* address) override { todo_once(what, address); }
};
ProgramActivityHost g_act_host;

class ProgramWatchHost final : public WatchHost {
public:
    ImageCanvas* canvas = nullptr;  // the canvas the cell's images go to

    bool window() const override { return g_watch != nullptr; }
    int ship_pool() const override { return w().roles.ships; }
    int player() const override { return w().player; }
    int last_ship() const override { return w().last_ship; }
    int player_target_pool() const override {
        return w().player >= 0 && w().player < world::kShipCount ? w().ship(w().player).target_pool : -1;
    }
    int player_target_rec() const override {
        return w().player >= 0 && w().player < world::kShipCount ? w().ship(w().player).target_rec : 0;
    }
    int defenders() const override { return w().pools.in_use(2); }  // DGROUP:0x940a
    DefenderWords defender(int k) const override {
        DefenderWords d;
        const auto& v = world::collide_globals().defenders;
        if (k < 0 || k >= static_cast<int>(v.size())) {  // past the table: 0 [DESIGN ADR 0016 decision 2]
            todo_once("the watch window's defender entry past DGROUP:0x52fe's table reads 0", "seg13:0x537e");
            return d;
        }
        const world::DefenderEntry& e = v[static_cast<std::size_t>(k)];
        d.state = e.w00;
        d.owner_pool = e.owner_pool;
        d.owner_rec = e.owner_rec;
        d.target_pool = e.killer_pool;
        d.target_rec = e.killer_rec;
        return d;
    }
    int object_state(int p, int r) const override {
        return p >= 0 && p < world::kPoolCount && r >= 0 && r < w().pools.records(p) ? int{w().pools.rec(p, r).state} : 1;
    }
    bool target_ok(int p, int r) const override { return target_valid(validity(), p, r); }
    Mark& mark() override { return hud() ? hud()->info().mark(2) : g_mark_fallback; }
    void next_mark() override {
        if (hud()) hud()->info().select_target(6);  // seg3:0x45a(6)
    }
    void post_create() override {
        if (g.main) PostMessageW(g.main, app::kMsgShipWatch, 0, 0);  // seg13:0x5778-0x5785
    }
    void post_paint() override {
        if (g_watch) InvalidateRect(g_watch, nullptr, FALSE);  // PostMessage(WM_PAINT) [DESIGN]
    }
    void set_title(const std::string& t) override {
        if (!g_watch) return;
        const std::u16string u = fmt::cp1252_to_utf16(t);
        SetWindowTextW(g_watch, reinterpret_cast<const wchar_t*>(u.c_str()));
    }
    void destroy() override { watch_destroy(); }
    std::uint32_t now() const override { return g.game.now(); }
    int attitude(int a, int b) const override { return world::relation(w(), a, b); }
    int team(int s) const override { return world::ship_team(w(), s); }
    std::string team_name(int team) override {  // seg13:0x218a: [Teams] Team<team + 1>, 20 bytes
        try {
            if (g.ini) return g.ini->get_string("Teams", "Team" + std::to_string(team + 1), "", 20);
        } catch (const std::exception&) {
        }
        return {};
    }
    std::string attitude_words(int v) override {
        if (!hud()) return {};
        hud()->info().attitude_text(v);  // seg11:0x3f2e into DGROUP:0x93e0
        return hud()->info().text_93e0();
    }
    std::string lang(int id) override { return lang_string(id); }
    bool monochrome() const override { return false; }  // DGROUP:0x90ee: the 8-bit surface [DESIGN ADR 0007]
    void activity(int a, int r) override {
        const int before = g_text.bad_index;
        activity_text(w(), g_act_host, g_text, a, r, 3);
        for (int k = before; k < g_text.bad_index; ++k) w().pools.bad_index_count = world::add16(w().pools.bad_index_count, 1);
    }
    void defender_activity(int r) override {
        const int before = g_text.bad_index;
        defender_text(w(), g_act_host, g_text, w().player, defender(r), 3);
        for (int k = before; k < g_text.bad_index; ++k) w().pools.bad_index_count = world::add16(w().pools.bad_index_count, 1);
    }
    const std::string& line1() const override { return g_text.w8694; }
    const std::string& line2() const override { return g_text.w86d0; }
    void clear_lines() override {
        g_text.w8694.clear();
        g_text.w86d0.clear();
    }
    std::string name(int caps, int pool, int rec, int mode) override {
        return object_name(w(), g_act_host, caps, pool, rec, mode);
    }
    std::string pilot_name(int s) override { return pilot_name_8e60(w(), g_act_host, s); }
    bool cell(int p, int r, CellInputs& out) override {
        if (p < 0 || p >= world::kPoolCount || r < 0 || r >= w().pools.records(p)) return false;
        const world::ObjectRecord& o = w().pools.rec(p, r);
        out = CellInputs{};
        out.kind = target_kind_371c(p, r);
        out.ship_pool = w().roles.ships;
        out.detail = w().detail;
        // DGROUP:0x131c, INI ShipAnim (default 1): a ship's cell turns with its heading and
        // glints [BYTES seg6:0xd0a seg13:0x5b0d seg13:0x5c67] (fixer: was 0).
        out.dg_131c = g.ini ? g.ini->get_int(settings::kGameSection, "ShipAnim", 1) : 1;
        out.obj.state = o.state;
        out.obj.extent = o.extent;
        out.obj.size = o.size;
        out.obj.gap = o.gap;
        out.obj.src_y = o.src_y;
        out.obj.b51 = o.race_status;
        if (p == 0 && r < world::kIconRecords) {
            const world::IconRecord& ic = w().icons[static_cast<std::size_t>(r)];
            out.icon.w00 = ic.w00;
            out.icon.w02 = ic.w02;
            out.icon.w19 = ic.w19;
            fill_fragment_inputs(r, out);
        }
        if (p == 7 && w().pools.records(7) > 0) out.pool7_w10 = world::word_at(w().pools.rec(7, 0), 0x10);
        const bool ship = p == w().roles.ships && r < world::kShipCount;
        if (ship) {
            const world::ShipRecord& s = w().ships[static_cast<std::size_t>(r)];
            out.ship_slot = s.sprite_slot;
            out.cloaked = s.be8;
            out.obj_w28 = world::word_at(o, 0x28);
        }
        if (canvas) {  // the sources as the HUD's cell() sets them on its canvas
            canvas->src[static_cast<std::size_t>(Img::kIconStrip)] = world::icon_strip();
            if (p == 0 && r < world::kIconRecords)
                canvas->src[static_cast<std::size_t>(Img::kIconOwn)] = world::slot_image(world::kIconOwnSlot + r);
            if (ship) canvas->src[static_cast<std::size_t>(Img::kShipSprite)] = world::slot_image(out.ship_slot + 0x28);
            if (p == 7) {
                gfx::ImagePtr strip = world::slot_image(5);
                if (!strip && g.gfx) strip = g.gfx->logo_strip();
                canvas->src[static_cast<std::size_t>(Img::kLogoStrip)] = strip;
            }
        }
        return true;
    }
    bool fragment(int k) override {
        if (!canvas) return false;
        gfx::ImagePtr im = fragment_image(k);
        canvas->src[static_cast<std::size_t>(Img::kFragment)] = im;
        return im != nullptr;
    }
    int client_width() const override {
        RECT rc{};
        if (g_watch) GetClientRect(g_watch, &rc);
        return rc.right;
    }

private:
    static TargetValidity validity() {  // seg3:0x0 on the World, as src/hud/hooks.cpp builds it
        TargetValidity v;
        v.ship_pool = w().roles.ships;
        v.player = w().player;
        v.allocated = [](int p) { return p >= 0 && p < world::kPoolCount ? int{w().pools.allocated(p)} : 0; };
        v.state = [](int p, int r) {
            return p >= 0 && p < world::kPoolCount && r >= 0 && r < w().pools.records(p) ? int{w().pools.rec(p, r).state}
                                                                                         : 1;
        };
        v.ship_147 = [](int r) { return r >= 0 && r < world::kShipCount ? int{w().ship(r).radar_hide} : 0; };
        const auto def = [](int r) -> const world::DefenderEntry* {
            const auto& d = world::collide_globals().defenders;
            return r >= 0 && r < static_cast<int>(d.size()) ? &d[static_cast<std::size_t>(r)] : nullptr;
        };
        v.def_00 = [def](int r) { return def(r) ? int{def(r)->w00} : 0; };
        v.def_pool = [def](int r) { return def(r) ? int{def(r)->owner_pool} : -1; };
        v.def_rec = [def](int r) { return def(r) ? int{def(r)->owner_rec} : 0; };
        v.gate_post_live = [](int r) { return world::gate_post_live(w(), r); };
        return v;
    }
};
ProgramWatchHost g_host;

// The watch mark's direction mark is drawn into this window: Info's slots (watch_create).
void bind_mark2() {
    links().mark2_window = [] { return g_watch != nullptr; };
    links().draw_mark2 = [] { watch_mark2(); };
}

// seg13:0x50b0(hdc, mode) on the watch's image, then the image onto `dc` (or a GetDC).
void paint_watch(HDC dc, int mode, bool force) {
    if (!g_watch || !g_watch_surface.ensure(g_watch)) return;
    g_host.canvas = &g_watch_surface.canvas;
    watch_paint(g_host, g_ws, g_watch_surface.canvas, mode);
    g_host.canvas = nullptr;
    if (!g_watch) return;  // the painter closed it
    if (dc) {
        g_watch_surface.present(g_watch, dc, force);
    } else if (HDC d = GetDC(g_watch)) {
        g_watch_surface.present(g_watch, d, force);
        ReleaseDC(g_watch, d);
    }
}

LRESULT CALLBACK watch_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:  // seg13:0x4f92-0x4f99
        g_watch = hwnd;
        g_host.set_title(watch_title(g_host));
        return 0;
    case WM_MOVE: {  // seg13:0x4fa8-0x4fde
        RECT r;
        GetWindowRect(hwnd, &r);
        if (r.left != g_watch_rect.left || r.top != g_watch_rect.top) {
            g_watch_rect = r;          // CopyRect into DGROUP:0x1ddc
            g_keys.watch_dirty = true;  // DGROUP:0x8fea
        }
        reactivate_main();  // seg13:0x4fde
        return 0;
    }
    case WM_SETFOCUS:  // seg13:0x4f9f: DGROUP:0x8f68 = 1
        g.running = true;
        return 0;
    case WM_GETMINMAXINFO: {  // seg13:0x4fe6-0x5013: minimum width 54, a fixed height
        auto* mm = reinterpret_cast<MINMAXINFO*>(lp);
        const int h = GetSystemMetrics(SM_CYCAPTION) + kWatchExtra;
        mm->ptMinTrackSize.x = kWatchExtra;
        mm->ptMinTrackSize.y = h;
        mm->ptMaxTrackSize.y = h;
        return 0;
    }
    case WM_PAINT: {  // seg13:0x501a-0x508c
        InvalidateRect(hwnd, nullptr, TRUE);
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (g_watch_surface.ensure(hwnd)) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            PatBlt(g_watch_surface.canvas.screen->hdc(), 0, 0, rc.right, rc.bottom, BLACKNESS);  // seg13:0x5054
            g_host.clear_lines();  // seg13:0x5059-0x506e: both lines emptied
            paint_watch(dc, 1, true);  // seg13:0x5079: seg13:0x50b0(hdc, 1)
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:  // the paint covers the client [DESIGN]
        return 1;
    case WM_CLOSE:  // seg13:0x508e-0x509e
        watch_destroy();
        return 0;
    case WM_DESTROY:
        if (g_watch == hwnd) g_watch = nullptr;
        return 0;
    default: break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void register_class(const wchar_t* name, WNDPROC proc, bool& done) {
    if (done) return;
    // The program registers the classes at start-up with DefWindowProc (src/app/main.cpp); the
    // window registers its own again with its procedure, as IS_ICON does [DESIGN].
    UnregisterClassW(name, g.instance);
    WNDCLASSW wc{};
    wc.style = CS_VREDRAW | CS_HREDRAW;
    wc.lpfnWndProc = proc;
    wc.hInstance = g.instance;
    wc.hIcon = g.icon;
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = name;
    done = RegisterClassW(&wc) != 0;
    if (!done) app::log("watch: RegisterClass failed, error " + std::to_string(GetLastError()));
}

// ---- IS_SMART

void check_scale() {
    if (!g_smart_sys) return;
    ModifyMenuW(g_smart_sys, kScaleItem, MF_BYCOMMAND | MF_STRING | (g_ws.half ? MF_CHECKED : MF_UNCHECKED), kScaleItem,
                L"1/2x Scale");  // seg7:0x63b6
}

void move_to_own_rect(HWND hwnd) {
    RECT r;
    GetWindowRect(hwnd, &r);
    MoveWindow(hwnd, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
}

// seg13:0x6450(hdc, mode, i) on the IS_SMART image.
void paint_smart(HDC dc, int mode, int i, bool force) {
    if (!g_smart || !g_smart_surface.ensure(g_smart)) return;
    g_host.canvas = &g_smart_surface.canvas;
    const int r = smart_rows(g_host, g_ws, &g_smart_surface.canvas, mode, i);
    g_host.canvas = nullptr;
    if (r == 1) {  // seg13:0x64fb: seg13:0x6362
        smart_destroy();
        return;
    }
    if (r == 2) {  // seg13:0x6501-0x6529
        move_to_own_rect(g_smart);
        return;
    }
    if (dc) {
        g_smart_surface.present(g_smart, dc, force);
    } else if (HDC d = GetDC(g_smart)) {
        g_smart_surface.present(g_smart, d, force);
        ReleaseDC(g_smart, d);
    }
}

LRESULT CALLBACK smart_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:  // seg13:0x6175-0x61c0
        g_smart = hwnd;
        g_smart_sys = GetSystemMenu(hwnd, FALSE);
        AppendMenuW(g_smart_sys, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(g_smart_sys, MF_STRING, kScaleItem, L"1/2x Scale");
        check_scale();
        DrawMenuBar(hwnd);
        return 0;
    case WM_SYSCOMMAND:
        if ((wp & 0xfff0) == kScaleItem) {  // DGROUP:0x95f8 toggled (cmp 1; sbb; neg)
            g_ws.half = g_ws.half == 0 ? 1 : 0;
            g.keys.sscale = g_ws.half;
            check_scale();
            move_to_own_rect(hwnd);
            reactivate_main();
            return 0;
        }
        if ((wp & 0xfff0) == SC_CLOSE) g.keys.sm_wnd = 0;  // SmWnd DGROUP:0x8f74 = 0
        return DefWindowProcW(hwnd, msg, wp, lp);
    case WM_MOVE: {
        RECT r;
        GetWindowRect(hwnd, &r);
        if (r.left != g_sm_left || r.top != g_sm_top) {
            g_sm_left = r.left;
            g_sm_top = r.top;
            g_keys.sm_dirty = true;  // DGROUP:0x8ff0
        }
        reactivate_main();
        return 0;
    }
    case WM_SETFOCUS:
        g.running = true;
        return 0;
    case WM_GETMINMAXINFO: {  // seg13:0x628d-0x62e1
        auto* mm = reinterpret_cast<MINMAXINFO*>(lp);
        const int cx = smart_width(g_ws.half), cy = smart_height(GetSystemMetrics(SM_CYCAPTION), g_ws.rows, g_ws.half);
        mm->ptMinTrackSize.x = mm->ptMaxTrackSize.x = cx;
        mm->ptMinTrackSize.y = mm->ptMaxTrackSize.y = cy;
        return 0;
    }
    case WM_PAINT: {
        InvalidateRect(hwnd, nullptr, TRUE);
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (g_smart_surface.ensure(hwnd)) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            PatBlt(g_smart_surface.canvas.screen->hdc(), 0, 0, rc.right, rc.bottom, BLACKNESS);
            paint_smart(dc, 1, -1, true);  // seg13:0x6450(hdc, 1, -1)
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_CLOSE:
        smart_destroy();
        return 0;
    case WM_DESTROY:
        if (g_smart == hwnd) g_smart = nullptr;
        g_smart_sys = nullptr;
        return 0;
    default: break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

std::string program_object_name(int caps, int pool, int rec, int mode) {
    return object_name(world::current(), g_act_host, caps, pool, rec, mode);
}

int watch_key(bool ctrl, bool alt) {
    read_keys();
    bind_mark2();
    return watch_key_563c(g_host, ctrl, alt);
}

void watch_create() {
    read_keys();
    bind_mark2();
    // seg13:0x5882-0x58c6: only with the left IS_INFO display and no watch.
    if (hud() && hud()->info().exists() && !g_watch && g.main) {
        register_class(kWatchClass, watch_proc, g_watch_registered);
        const int cx = g_watch_rect.right - g_watch_rect.left;
        const int cy = GetSystemMetrics(SM_CYCAPTION) + kWatchExtra;
        // CreateWindow("IS_WATCH", "Ship - Pilot", 0x10CC0000, WatchLeft, WatchTop, WatchRight -
        // WatchLeft, SM_CYCAPTION + 54, main, 0) (DGROUP:0x1380).
        HWND h = CreateWindowExW(0, kWatchClass, L"Ship - Pilot", WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
                                 g_watch_rect.left, g_watch_rect.top, cx, cy, g.main, nullptr, g.instance, nullptr);
        if (!h) app::log("watch: CreateWindow(IS_WATCH) failed, error " + std::to_string(GetLastError()));
    }
    reactivate_main();  // seg13:0x58cb: either way
}

void watch_destroy() {
    if (!g_watch) return;  // seg13:0x585a
    HWND h = g_watch;
    g_watch = nullptr;  // seg13:0x586a
    DestroyWindow(h);
}

bool watch_exists() { return g_watch != nullptr; }

void smart_create() {
    read_keys();
    // seg13:0x6397-0x63a3: only with the left IS_INFO display and no IS_SMART; otherwise just
    // SetActiveWindow(main) [BYTES seg13:0x63a5 seg13:0x643f].
    if (!(hud() && hud()->info().exists()) || g_smart || !g.main) {
        reactivate_main();
        return;
    }
    // seg13:0x63a8-0x63da: DGROUP:0x95fa = the ships k <= DGROUP:0x9316, k != P, with seg3:0x0.
    g_ws.rows = 0;
    for (int k = 0; k <= w().last_ship; ++k)
        if (k != w().player && g_host.target_ok(w().roles.ships, k)) ++g_ws.rows;
    if (g_ws.rows == 0) return;  // seg13:0x63dc-0x63e1: no window and no SetActiveWindow
    for (int k = 0; k < 8; ++k) {  // seg13:0x63e3-0x6410: entries 11 ... 18
        WatchState::Row& r = g_ws.row[static_cast<std::size_t>(k)];
        r.pool = w().roles.ships;
        r.rec = k;
        r.x = 2;
        r.y = 0x2e * k + 2;
    }
    register_class(kSmartClass, smart_proc, g_smart_registered);
    // CreateWindow("IS_SMART", "", 0x10CC0000, SmLeft, SmTop, 50, 150, main, 0) [BYTES seg13:0x6412-0x643a].
    HWND h = CreateWindowExW(0, kSmartClass, L"", WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME, g_sm_left,
                             g_sm_top, 0x32, 0x96, g.main, nullptr, g.instance, nullptr);
    if (!h) app::log("watch: CreateWindow(IS_SMART) failed, error " + std::to_string(GetLastError()));
    reactivate_main();  // seg13:0x643f
}

void smart_destroy() {
    if (!g_smart) return;
    HWND h = g_smart;
    g_smart = nullptr;
    DestroyWindow(h);
}

bool smart_exists() { return g_smart != nullptr; }

void watch_ship(int i) {
    // seg13:0x6890(i) [BYTES seg13:0x6890-0x68da]
    if (g_smart && i != w().player) paint_smart(nullptr, 2, i, false);  // seg13:0x6450(0, 2, i)
    const Mark& m = g_host.mark();
    if (g_watch && m.pool == w().roles.ships && m.rec == i) paint_watch(nullptr, 2, false);  // seg13:0x50b0(0, 2)
}

void smart_row(int i, int mode) {
    if (g_smart) paint_smart(nullptr, mode, i, false);
}

void watch_tail() {
    if (g_watch) paint_watch(nullptr, 0, false);  // seg13:0x50b0(0, 0) [BYTES seg13:0x67f2-0x67fa]
}

int watch_windows(int m) {
    // seg13:0x65f2(m) [BYTES seg13:0x65f2-0x6715]
    switch (m) {
    case 0: {
        int posted = 0;
        if (g.keys.icon_wnd != 0 && !icon_column_exists() && g.main) {
            PostMessageW(g.main, app::kMsgIconWatch, 0, 0);
            posted = 1;
        }
        if (g.keys.sm_wnd != 0 && !g_smart && g.main) {
            PostMessageW(g.main, app::kMsgAllShips, 0, 0);
            posted = 1;
        }
        return posted;
    }
    case 1:
        watch_destroy();         // seg13:0x5850
        icon_column_destroy();   // seg7:0x63f2
        smart_destroy();         // seg13:0x6362
        return 0;
    case 2: {  // ShowWindow(SW_SHOW) on each, SetActiveWindow(main) when any
        bool any = false;
        for (HWND h : {g_watch, g_smart})
            if (h) {
                ShowWindow(h, SW_SHOW);
                any = true;
            }
        if (icon_column_exists()) todo_once("seg13:0x65f2 modes 2 and 3 on the IS_ICON column", "seg13:0x65f2");
        if (any) reactivate_main();
        return 0;
    }
    case 3:  // ShowWindow(SW_HIDE) on each
        for (HWND h : {g_watch, g_smart})
            if (h) ShowWindow(h, SW_HIDE);
        if (icon_column_exists()) todo_once("seg13:0x65f2 modes 2 and 3 on the IS_ICON column", "seg13:0x65f2");
        return 0;
    default: return 0;
    }
}

void all_ships_notice(int n) {
    g_ws.rows = n;  // DGROUP:0x95fa
    if (g.main) PostMessageW(g.main, app::kMsgAllShips, 0, 0);
}

void watch_mark2() {
    // seg11:0x3ed8(2): GetDC(DGROUP:0x95fc), seg11:0x2ce6(hdc, 2) [BYTES seg11:0x3f01-0x3f15]: the
    // frame origin (2, 2), the old direction erased in BLACK_PEN, the new one in the mark's pen
    // or black (seg11:0x2dac).
    if (!g_watch || !hud() || !g_watch_surface.ensure(g_watch)) return;
    Mark& m = hud()->info().mark(2);
    m.ox = 2;
    m.oy = 2;
    ImageCanvas& c = g_watch_surface.canvas;
    const auto segment = [&](int dir, bool pen) {
        Point p[3];
        const int n = mark_segment(dir, p);
        if (n == 0) return;
        for (int i = 0; i < n; ++i) {
            p[i].x += m.ox;
            p[i].y += m.oy;
        }
        c.lines(Img::kScreen, pen ? Pen::solid(m.colour) : Pen::stock_object(kBlackPen), p, n);
    };
    if (m.dir != m.shown) {
        segment(m.shown, false);
        m.shown = m.dir;
    }
    segment(m.dir, m.lit != 0);
    if (HDC d = GetDC(g_watch)) {
        g_watch_surface.present(g_watch, d, false);
        ReleaseDC(g_watch, d);
    }
}

WatchKeys take_watch_keys() {
    WatchKeys k = g_keys;
    k.watch_left = g_watch_rect.left;
    k.watch_top = g_watch_rect.top;
    k.watch_right = g_watch_rect.right;
    k.sm_left = g_sm_left;
    k.sm_top = g_sm_top;
    g_keys.watch_dirty = g_keys.sm_dirty = false;
    return k;
}

}  // namespace is::hud
#endif
