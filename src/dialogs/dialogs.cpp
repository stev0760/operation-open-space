// The dialog layer of ISPACE.DLL ordinal 10 (see dialogs.h). Every procedure cites the
// ISPACE.DLL routine it ports; the shared rules are in rules.cpp.
#ifdef _WIN32
#include "dialogs/dialogs.h"

#include <cstring>
#include <exception>

#include "assets/asset_store.h"
#include "dialogs/preview.h"
#include "gfx/win16_edit.h"
#include "settings/settings.h"

namespace is::dialogs {

namespace {

// The LevClass windows paint the frame only while no Order dialog is open (ISPACE.DLL
// DGROUP:0x1f82 = 0) [BYTES ISPACE.DLL seg10:0x30b9].
HWND g_order = nullptr;
// The todo sink of the open dialog, for the LevClass procedure.
std::function<void(const std::string&, const char*)> g_todo;
// The slot-0 bitmap of the open dialog (ISPACE.DLL DGROUP:0x157c[0] in DGROUP:0x1588[0]) and the
// Order logo with its mask (DGROUP:0x1584, DGROUP:0x1586), loaded on the logo's first paint.
std::optional<ShipBitmap> g_slot;
std::optional<Logo> g_logo;
bool g_logo_tried = false;
const assets::AssetStore* g_store = nullptr;

constexpr const wchar_t* kLevClass = L"LevClass";
constexpr int kPreviewId = 100;

INT_PTR CALLBACK dialog_proc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    Dialog* d = nullptr;
    if (msg == WM_INITDIALOG) {
        d = reinterpret_cast<Dialog*>(l);
        SetWindowLongPtrW(h, DWLP_USER, l);
    } else {
        d = reinterpret_cast<Dialog*>(GetWindowLongPtrW(h, DWLP_USER));
    }
    if (!d) return FALSE;
    // No exception may unwind through user32's frames [DESIGN].
    try {
        return d->handle(h, msg, w, l);
    } catch (const std::exception& x) {
        if (g_todo) g_todo(std::string("dialog exception: ") + x.what(), "ISPACE.DLL seg10");
    } catch (...) {
    }
    return FALSE;
}

}  // namespace

// The frame of ISPACE.DLL seg10:0x309c's WM_PAINT [BYTES ISPACE.DLL seg10:0x3103-0x3303]
// (preview.h lev_frame is the same picture, computed).
void paint_frame(HDC dc, const RECT& r) {
    const int l = r.left, t = r.top, rr = r.right, b = r.bottom;
    const auto fill = [&](int x0, int y0, int x1, int y1, int stock) {
        RECT f{x0, y0, x1, y1};
        FillRect(dc, &f, static_cast<HBRUSH>(GetStockObject(stock)));
    };
    fill(l, t, l + 3, b, LTGRAY_BRUSH);
    fill(l, t, rr, t + 3, LTGRAY_BRUSH);
    fill(l + 2, b - 3, rr, b, DKGRAY_BRUSH);
    fill(rr - 3, t + 1, rr, b - 1, DKGRAY_BRUSH);
    for (int i = 0; i < 3; ++i) {
        SetPixel(dc, rr - i - 2, i + t, RGB(0xc0, 0xc0, 0xc0));
        SetPixel(dc, rr - i - 1, i + t, RGB(0x80, 0x80, 0x80));
        SetPixel(dc, l - i + 2, i + b - 3, RGB(0xc0, 0xc0, 0xc0));
        SetPixel(dc, l - i + 3, i + b - 3, RGB(0, 0, 0));
    }
    HGDIOBJ old = SelectObject(dc, GetStockObject(WHITE_PEN));
    MoveToEx(dc, l, b - 1, nullptr);
    LineTo(dc, l, t);
    LineTo(dc, rr - 1, t);
    SelectObject(dc, GetStockObject(BLACK_PEN));
    LineTo(dc, rr - 1, b - 1);
    LineTo(dc, l - 1, b - 1);
    SelectObject(dc, old);
}

namespace {

// An Image as a 32 bpp DIB section selected into a new memory DC: the port's stand-in for the
// display-compatible memory DCs of ISPACE.DLL (CreateCompatibleDC(DGROUP:0x2012), ispace.md
// ordinal 10).
struct MemImage {
    HDC dc = nullptr;
    HBITMAP bm = nullptr;
    HGDIOBJ old = nullptr;
    MemImage(HDC ref, const Image& im) {
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
        bi.bmiHeader.biWidth = im.w;
        bi.bmiHeader.biHeight = -im.h;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        void* bits = nullptr;
        dc = CreateCompatibleDC(ref);
        bm = CreateDIBSection(ref, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (bits) std::memcpy(bits, im.px.data(), im.px.size() * 4);
        if (dc && bm) old = SelectObject(dc, bm);
    }
    ~MemImage() {
        if (dc && old) SelectObject(dc, old);
        if (bm) DeleteObject(bm);
        if (dc) DeleteDC(dc);
    }
    MemImage(const MemImage&) = delete;
    MemImage& operator=(const MemImage&) = delete;
};

// ISPACE.DLL seg10:0x2c68(slot) [BYTES ISPACE.DLL seg10:0x2c7b seg10:0x2c8e seg10:0x2c9c
// seg10:0x2cd5 seg10:0x2ce1 seg10:0x2cfb seg10:0x2d09 seg10:0x2d1d seg10:0x2d2e]. The stretch
// helper seg10:0x2d3c takes StretchBlt for every true-colour source; the port's dialogs draw at
// the display's depth, so it is always StretchBlt, in COLORONCOLOR, never HALFTONE (shell.md
// "Ship previews") [DESIGN].
void paint_slot_window(HWND h) {
    HDC d = GetDC(h);
    PatBlt(d, 3, 3, 90, 90, BLACKNESS);
    if (g_slot) {
        const int w = g_slot->w;
        MemImage m(d, g_slot->image);
        SetStretchBltMode(d, COLORONCOLOR);
        StretchBlt(d, 48 - w, 48 - w, 2 * w, 2 * w, m.dc, 0, 0, w, w, SRCCOPY);
    }
    ReleaseDC(h, d);
}

// ISPACE.DLL seg10:0x2f4, the Order logo (shell.md "Game-start dialogs", Logo).
void paint_logo_window(HWND h) {
    if (!g_logo && !g_logo_tried) {  // the first paint, while DGROUP:0x1584 is 0
        g_logo_tried = true;
        try {
            if (g_store) g_logo = order_logo(g_store->sprite(300, assets::kGameDll));
        } catch (const std::exception& x) {
            if (g_todo) g_todo(std::string("Order logo: ") + x.what(), "ISPACE.DLL seg10:0x30e");
        }
    }
    if (!g_logo) return;
    HDC d = GetDC(h);
    // The mask as the monochrome bitmap of ordinal 6, rows padded to 16 bits [REF Win32 SDK
    // CreateBitmap].
    const int stride = ((g_logo->w + 15) / 16) * 2;
    std::vector<std::uint8_t> bits(static_cast<std::size_t>(stride) * g_logo->h, 0);
    for (int y = 0; y < g_logo->h; ++y)
        for (int x = 0; x < g_logo->w; ++x)
            if (g_logo->white(x, y))
                bits[static_cast<std::size_t>(y) * stride + x / 8] |= static_cast<std::uint8_t>(0x80 >> (x % 8));
    HBITMAP mono = CreateBitmap(g_logo->w, g_logo->h, 1, 1, bits.data());
    HDC md = CreateCompatibleDC(d);
    HGDIOBJ mold = SelectObject(md, mono);
    MemImage im(d, g_logo->image);
    SetBkColor(d, RGB(255, 255, 255));
    SetTextColor(d, RGB(0, 0, 0));
    BitBlt(d, kLogoX, kLogoY, kLogoW, kLogoH, md, 0, 0, SRCAND);
    BitBlt(d, kLogoX, kLogoY, kLogoW, kLogoH, im.dc, 0, 0, SRCPAINT);
    SelectObject(md, mold);
    DeleteDC(md);
    DeleteObject(mono);
    ReleaseDC(h, d);
}

// ISPACE.DLL seg10:0x309c, the LevClass procedure (ordinal 21).
LRESULT CALLBACK lev_proc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_PAINT: {
        InvalidateRect(h, nullptr, TRUE);  // [BYTES ISPACE.DLL seg10:0x30ac]
        RECT r;
        GetClientRect(h, &r);
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        const bool order = g_order != nullptr;
        if (!order) paint_frame(dc, r);
        EndPaint(h, &ps);
        // seg10:0x2c68 for slot 0; the original also runs it for slot 1, whose window these
        // dialogs never create, so it returns at once [BYTES ISPACE.DLL seg10:0x2c7b].
        if (!order) paint_slot_window(h);
        else paint_logo_window(h);
        return 0;
    }
    case WM_ERASEBKGND:
        // The Order dialog registers LevClass with NULL_BRUSH (seg10:0x4250(1)), so the logo
        // window erases nothing [BYTES ISPACE.DLL seg10:0x290 seg10:0x293].
        if (g_order) return 1;
        break;
    case WM_NCPAINT: return 0;  // [BYTES ISPACE.DLL seg10:0x3311]
    case WM_COMMAND:            // posted on to the parent [BYTES ISPACE.DLL seg10:0x3335]
        PostMessageW(GetParent(h), WM_COMMAND, w, l);
        return 0;
    default: break;
    }
    return DefWindowProcW(h, msg, w, l);
}

// ISPACE.DLL seg10:0x2aac(ship, 0, flags) for slot 0: the slot's bitmap freed, then built for a
// ship at or above 0 (ispace.md ordinal 10, Ship previews) [BYTES ISPACE.DLL seg10:0x2b0c
// seg10:0x2b2a].
void build_preview(const Env& env, const fmt::MasterShp& ships, const fmt::MasterBod& bodies,
                   int ship, int flags) {
    g_slot.reset();
    if (ship < 0 || static_cast<std::size_t>(ship) >= ships.records.size() || !env.store) return;
    const fmt::ShipRecord& s = ships.records[static_cast<std::size_t>(ship)];
    if (s.body >= bodies.records.size()) return;
    const fmt::BodyRecord& b = bodies.records[s.body];
    if (b.bitmap_id == fmt::kBodyBitmapFromFile) {
        // The BOD + 0x23 file of an imported ship is not read yet [OPEN ISPACE.DLL seg5:0x1c16].
        if (env.todo) env.todo("ship preview from a .DIB file (" + b.bitmap_file + ")", "ISPACE.DLL seg5:0x1c16");
        return;
    }
    try {
        g_slot = ship_bitmap(env.store->sprite(b.bitmap_id), b.bitmap_id, s.byte_07, s.byte_08,
                             b.byte_1a, flags);
    } catch (const std::exception& x) {
        if (env.todo) env.todo(std::string("ship preview: ") + x.what(), "ISPACE.DLL seg5:0x0");
    }
}

std::string class_of(HWND h) {
    wchar_t buf[64] = L"";
    GetClassNameW(h, buf, 64);
    return narrow(buf);
}

void load_ships(const Env& env, fmt::MasterShp* ships, fmt::MasterBod* bodies) {
    // seg10:0x406a: ShipPath + MASTER.SHP, count = length / 0x41 [BYTES ISPACE.DLL
    // seg10:0x40c4 seg10:0x4102 seg10:0x4124].
    try {
        if (!env.store) return;
        const assets::Ships s = env.store->ships(env.ship_dir);
        *ships = s.ships;
        *bodies = s.bodies;
    } catch (const std::exception& x) {
        if (env.todo) env.todo(std::string("MASTER.SHP: ") + x.what(), "ISPACE.DLL seg10:0x406a");
    }
}

std::vector<std::uint8_t> read_prefix(const std::filesystem::path& p, std::size_t n, bool* opened) {
    *opened = false;
    std::vector<std::uint8_t> out;
    HANDLE f = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return out;
    *opened = true;
    out.resize(n);
    DWORD got = 0;
    if (!ReadFile(f, out.data(), static_cast<DWORD>(n), &got, nullptr)) got = 0;
    out.resize(got);
    CloseHandle(f);
    return out;
}

}  // namespace

std::wstring wide(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(1252, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(1252, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::string narrow(const std::wstring& w) {
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(1252, 0, w.data(), static_cast<int>(w.size()), nullptr, 0,
                                      nullptr, nullptr);
    std::string s(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(1252, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr,
                        nullptr);
    return s;
}

bool register_lev_class(HINSTANCE instance) {
    WNDCLASSW existing{};
    if (GetClassInfoW(instance, kLevClass, &existing)) return true;
    // ISPACE.DLL seg10:0x4250(0): CS_VREDRAW | CS_HREDRAW, IDC_ARROW, BLACK_BRUSH
    // [BYTES ISPACE.DLL seg10:0x425a seg10:0x4260 seg10:0x4287 seg10:0x429b]. The original
    // registers it for each dialog and unregisters it at the end; the port keeps it [DESIGN].
    WNDCLASSW wc{};
    wc.style = CS_VREDRAW | CS_HREDRAW;
    wc.lpfnWndProc = lev_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kLevClass;
    return RegisterClassW(&wc) != 0;
}

std::vector<std::string> list_saves(const std::filesystem::path& local_path) {
    std::vector<std::string> out;
    const std::filesystem::path pattern = local_path / L"GAMES" / L"*.SAV";
    WIN32_FIND_DATAW fd;
    HANDLE f = FindFirstFileW(pattern.c_str(), &fd);
    if (f == INVALID_HANDLE_VALUE) return out;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        // _dos_findfirst (ISPACE.DLL seg10:0x15af) returns the upper-case 8.3 name, and the
        // DEMOGAME/LASTGAME skips compare it exactly, while the game itself writes the
        // names in lower case (formats.md "Names and triggers"). The port takes the short
        // name when the file system has one, else the long name, and upper-cases it [DESIGN].
        std::string name = narrow(fd.cAlternateFileName[0] ? fd.cAlternateFileName : fd.cFileName);
        for (char& c : name)
            if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        out.push_back(name);
    } while (FindNextFileW(f, &fd));
    FindClose(f);
    return out;
}

// ---------------------------------------------------------------- Dialog

Dialog::~Dialog() {
    if (hwnd_ && IsWindow(hwnd_) && !modal_) {
        SetWindowLongPtrW(hwnd_, DWLP_USER, 0);  // no dispatch into a half-destroyed object
        DestroyWindow(hwnd_);
    }
    if (normal_) DeleteObject(normal_);
    if (bold_) DeleteObject(bold_);
}

int Dialog::run_modal() {
    if (!env_.store) return -1;
    std::vector<std::uint8_t> t;
    try {
        // DS_NOFAILCREATE: the SS_ICON controls name icon 246, which ENG_LANG.DLL lacks
        // (shell.md "ISPACE dialogs in common") [DESIGN].
        t = env_.store->dialog_template(template_, {}, DS_NOFAILCREATE);
    } catch (const std::exception& x) {
        todo(std::string("dialog template: ") + x.what(), "ISPACE.DLL seg9:0x145e");
        return -1;
    }
    modal_ = true;
    const gfx::Win16EditScope edits;  // the Win16 edit borders (gfx/win16_edit.h) [DESIGN]
    const INT_PTR r = DialogBoxIndirectParamW(env_.instance,
                                              reinterpret_cast<const DLGTEMPLATE*>(t.data()),
                                              env_.owner, dialog_proc, reinterpret_cast<LPARAM>(this));
    hwnd_ = nullptr;
    return static_cast<int>(r);
}

HWND Dialog::create_modeless() {
    if (!env_.store) return nullptr;
    const auto t = env_.store->dialog_template(template_, {}, DS_NOFAILCREATE);
    modal_ = false;
    const gfx::Win16EditScope edits;  // the Win16 edit borders (gfx/win16_edit.h) [DESIGN]
    return CreateDialogIndirectParamW(env_.instance, reinterpret_cast<const DLGTEMPLATE*>(t.data()),
                                      env_.owner, dialog_proc, reinterpret_cast<LPARAM>(this));
}

INT_PTR Dialog::handle(HWND h, UINT msg, WPARAM w, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG:
        hwnd_ = h;
        g_todo = env_.todo;
        GetWindowRect(h, &start_rect_);  // taken at init start [BYTES ISPACE.DLL seg10:0x172d]
        // seg9:0x29c0: CreateFont(14, 6, 0, 0, weight, 0, 0, 0, 0, 0, 0, 2, 0x22, "Arial") with
        // weights 0 and 730 [BYTES ISPACE.DLL seg9:0x29e8 seg9:0x2a0f].
        normal_ = CreateFontW(14, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0x22, L"Arial");
        bold_ = CreateFontW(14, 6, 0, 0, 730, 0, 0, 0, 0, 0, 0, 2, 0x22, L"Arial");
        init();
        return FALSE;  // every procedure sets the focus itself
    case WM_COMMAND:
        command(LOWORD(w), HIWORD(w));
        return TRUE;
    default: break;
    }
    return FALSE;
}

void Dialog::end(int result) {
    result_ = result;
    ended_ = true;
    if (modal_) EndDialog(hwnd_, result);
}

void Dialog::set_fonts() {  // ISPACE.DLL seg10:0x602a(hDlg, 0)
    for (HWND c = GetWindow(hwnd_, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        const auto style = static_cast<std::uint32_t>(GetWindowLongW(c, GWL_STYLE));
        SendMessageW(c, WM_SETFONT,
                     reinterpret_cast<WPARAM>(bold_child(class_of(c), style) ? bold_ : normal_),
                     TRUE);
        ShowWindow(c, SW_SHOW);
    }
}

void Dialog::bold(int id) {
    SendDlgItemMessageW(hwnd_, id, WM_SETFONT, reinterpret_cast<WPARAM>(bold_), TRUE);
}

void Dialog::centre() {
    const Placement p = centred(start_rect_.left, start_rect_.top, start_rect_.right,
                                start_rect_.bottom, env_.screen_cx, env_.screen_cy);
    MoveWindow(hwnd_, p.x, p.y, p.w, p.h, TRUE);
}

int Dialog::text_height() {
    HDC dc = GetDC(hwnd_);
    HGDIOBJ old = SelectObject(dc, normal_);
    SIZE s{};
    GetTextExtentPoint32W(dc, L"Test", 4, &s);
    SelectObject(dc, old);
    ReleaseDC(hwnd_, dc);
    return s.cy;
}

void Dialog::size_combo(int id, int height) {
    HWND c = GetDlgItem(hwnd_, id);
    if (!c) return;
    RECT r;
    GetWindowRect(c, &r);
    MapWindowPoints(nullptr, hwnd_, reinterpret_cast<POINT*>(&r), 2);
    MoveWindow(c, r.left, r.top, r.right - r.left, height, TRUE);
}

void Dialog::set_icon() {
    SendDlgItemMessageW(hwnd_, 591, STM_SETICON, reinterpret_cast<WPARAM>(env_.game_icon), 0);
}

void Dialog::set_text(int id, const std::string& s) {
    SetDlgItemTextW(hwnd_, id, wide(s).c_str());
}

std::string Dialog::text(int id, int max_chars) const {
    std::wstring buf(static_cast<std::size_t>(max_chars) + 1, L'\0');
    const int n = GetDlgItemTextW(hwnd_, id, buf.data(), max_chars + 1);
    buf.resize(static_cast<std::size_t>(n > 0 ? n : 0));
    return narrow(buf);
}

void Dialog::add(int id, const std::string& s) {
    SendDlgItemMessageW(hwnd_, id, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide(s).c_str()));
}

int Dialog::find(int id, const std::string& s) {
    return static_cast<int>(SendDlgItemMessageW(hwnd_, id, CB_FINDSTRING, static_cast<WPARAM>(-1),
                                                reinterpret_cast<LPARAM>(wide(s).c_str())));
}

void Dialog::select(int id, int index) {
    SendDlgItemMessageW(hwnd_, id, CB_SETCURSEL, static_cast<WPARAM>(index), 0);
}

int Dialog::selection(int id) const {
    // A missing control answers 0, as SendDlgItemMessage to no window does.
    if (!GetDlgItem(hwnd_, id)) return 0;
    return static_cast<int>(SendDlgItemMessageW(hwnd_, id, CB_GETCURSEL, 0, 0));
}

std::string Dialog::selected_text(int id) const {
    const int i = selection(id);
    if (i < 0 || !GetDlgItem(hwnd_, id)) return {};
    const auto n = SendDlgItemMessageW(hwnd_, id, CB_GETLBTEXTLEN, static_cast<WPARAM>(i), 0);
    if (n <= 0) return {};
    std::wstring buf(static_cast<std::size_t>(n) + 1, L'\0');
    SendDlgItemMessageW(hwnd_, id, CB_GETLBTEXT, static_cast<WPARAM>(i),
                        reinterpret_cast<LPARAM>(buf.data()));
    buf.resize(static_cast<std::size_t>(n));
    return narrow(buf);
}

void Dialog::write(const std::string& key, const std::string& value) {
    writes_.push_back({key, value});
    if (!env_.ini) return;
    env_.ini->set(settings::kGameSection, key, value);
    if (env_.ini_changed) env_.ini_changed();
}

std::string Dialog::ini_string(const std::string& key, const std::string& def, std::size_t nsize) {
    return env_.ini ? env_.ini->get_string(settings::kGameSection, key, def, nsize) : def;
}

int Dialog::ini_int(const std::string& key, int def) {
    return env_.ini ? env_.ini->get_int(settings::kGameSection, key, def) : def;
}

int Dialog::box(const std::string& text, UINT flags) {
    if (env_.message_box) return env_.message_box(hwnd_, text, "Inner Space", flags);
    return MessageBoxW(hwnd_, wide(text).c_str(), L"Inner Space", flags);
}

std::vector<std::uint8_t> Dialog::lang_resource(int type, int id) const {
    try {
        if (env_.store) {
            const fmt::NeModule& m = env_.store->language_dll();
            if (const fmt::NeResource* r = m.find(static_cast<std::uint16_t>(type),
                                                  static_cast<std::uint16_t>(id)))
                return std::vector<std::uint8_t>(m.data(*r), m.data(*r) + m.size(*r));
        }
    } catch (const std::exception& x) {
        todo(std::string("resource: ") + x.what(), "ISPACE.DLL seg10");
    }
    return {};
}

HWND Dialog::preview(int x, int y, int w, int h) {
    register_lev_class(env_.instance);
    return CreateWindowExW(0, kLevClass, nullptr, WS_CHILD | WS_VISIBLE, x, y, w, h, hwnd_,
                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(kPreviewId)),
                           env_.instance, nullptr);
}

// ---------------------------------------------------------------- Restore Game #267

RestoreGame::RestoreGame(Env& env, std::vector<std::string> saves)
    : Dialog(env, kDlgRestoreGame), saves_(std::move(saves)) {}

void RestoreGame::init() {  // ISPACE.DLL seg10:0x1438
    g_slot.reset();
    set_fonts();
    load_ships(env_, &ships_, &bodies_);
    add(332, kNewGameEntry);  // [BYTES ISPACE.DLL seg10:0x1535]
    for (const std::string& n : restore_game_names(saves_, &found_)) add(332, n);
    select(332, 0);
    size_combo(332, combo_height(text_height(), 1 + found_));  // [BYTES ISPACE.DLL seg10:0x1641]
    const std::string restore = ini_string("Restore", "", 20);  // [BYTES ISPACE.DLL seg10:0x1698]
    (void)ini_int("BadVideo", 1);  // DGROUP:0x1f7e: only the preview's stretch reads it
    int index = find(332, restore);
    if (index < 0) index = 0;
    select(332, index);
    // The preview at (4, client bottom - 113) [BYTES ISPACE.DLL seg10:0x1649 seg10:0x167d].
    RECT c;
    GetClientRect(hwnd_, &c);
    preview(4, c.bottom - 113, 96, 96);
    fill(index);
    SetFocus(GetDlgItem(hwnd_, 332));
    centre();
}

void RestoreGame::fill(int index) {  // ISPACE.DLL seg10:0x1a80
    if (index == 0) {
        set_text(331, "?");
        set_text(329, "?");
        set_text(269, "?");
        set_text(330, lang(1149));  // "NEW GAME, STARTING AT WAVE 1."
        ship_ = -1;
        build_preview(env_, ships_, bodies_, -1, 0);  // seg10:0x1b0a
        InvalidateRect(GetDlgItem(hwnd_, kPreviewId), nullptr, TRUE);
        return;
    }
    const std::string name = selected_text(332);
    bool opened = false;
    const auto bytes = read_prefix(env_.local_path / L"GAMES" / wide(name + ".sav"),
                                   kSaveHeaderRead, &opened);
    if (!opened) return;  // OpenFile failed: nothing changes [BYTES ISPACE.DLL seg10:0x1b0a]
    const auto h = parse_save_header(bytes);
    if (!h) {  // a short read: MessageBeep(0) [BYTES ISPACE.DLL seg10:0x1b72]
        MessageBeep(0);
        return;
    }
    Availability a{env_.availability_cache, env_.x5, &ships_, &bodies_};
    ship_ = restore_ship_index(h->ship, a);
    pilot_ = h->pilot;
    build_preview(env_, ships_, bodies_, ship_, h->preview);  // seg10:0x1c57, header +0x170
    set_text(331, h->pilot);
    std::string ship_name;
    if (ship_ >= 0 && static_cast<std::size_t>(ship_) < ships_.records.size())
        ship_name = ships_.records[static_cast<std::size_t>(ship_)].name;
    set_text(329, ship_name);
    set_text(269, std::to_string(h->score));  // ltoa
    // Edit 330: INSPACE.EXE export 13 (seg10:0xa118) formats strings 3000-3002 and 3027 from
    // the header with the formatter seg10:0xa332, whose %A conversion is not documented
    // [OPEN]. The port leaves the edit empty.
    set_text(330, "");
    todo("saved-game description (INSPACE export 13) [OPEN]", "seg10:0xa118");
    InvalidateRect(GetDlgItem(hwnd_, kPreviewId), nullptr, TRUE);
}

void RestoreGame::command(int id, int code) {  // ISPACE.DLL seg10:0xec2
    switch (id) {
    case 1: {  // seg10:0x1d36(hDlg, 1)
        const int index = selection(332);
        const std::string ship_name =
            ship_ >= 0 && static_cast<std::size_t>(ship_) < ships_.records.size()
                ? ships_.records[static_cast<std::size_t>(ship_)].name
                : std::string();
        for (const IniWrite& w : restore_game_writes(selected_text(332), ship_, ship_name, pilot_))
            write(w.key, w.value);
        end(restore_game_result(true, index));
        break;
    }
    case 333: end(restore_game_result(false, 0)); break;  // seg10:0x1d36(hDlg, 0)
    case 332:                                              // seg10:0x1a38
        if (code == CBN_SELCHANGE) fill(selection(332));  // the index goes to DGROUP:0xcea
        break;
    default: break;
    }
}

// ---------------------------------------------------------------- New Game #321 / New Demo #589

NewGame::NewGame(Env& env, bool demo, std::vector<Drive> drives)
    : Dialog(env, demo ? kDlgNewDemo : kDlgNewGame), demo_(demo), drives_(std::move(drives)) {}

std::vector<NewGame::Drive> NewGame::system_drives() {
    // SetErrorMode(1) without a restore [BYTES ISPACE.DLL seg10:0x2087].
    SetErrorMode(SEM_FAILCRITICALERRORS);
    std::vector<Drive> out;
    // _getdrive() is saved and restored around the loop [BYTES ISPACE.DLL seg10:0x207d
    // seg10:0x223c]; the port saves the whole current directory, of any length.
    std::wstring cwd(GetCurrentDirectoryW(0, nullptr), L'\0');
    if (!cwd.empty()) {
        const DWORD n = GetCurrentDirectoryW(static_cast<DWORD>(cwd.size()), cwd.data());
        cwd.resize(n < cwd.size() ? n : 0);
    }
    const DWORD mask = GetLogicalDrives();
    for (int d = 3; d <= 26; ++d) {  // C: to Z: [BYTES ISPACE.DLL seg10:0x20b1]
        if (!(mask & (1u << (d - 1)))) continue;
        Drive dr;
        dr.letter = static_cast<char>('A' + d - 1);
        // _chdrive(d) fails: the drive is skipped, and only then [BYTES ISPACE.DLL
        // seg10:0x2110 seg10:0x2118]. The Win32 counterpart is SetCurrentDirectory("X:")
        // [DESIGN]. It also fails on a drive without media, which the original's DOS
        // select-disk call lists as "X: Unknown_HD" on Windows 9x (shell.md: the oracle skipped
        // E: because its prefix maps no E: directory) [OPEN ISPACE.DLL seg10:0x2110].
        const wchar_t drive_only[] = {static_cast<wchar_t>(dr.letter), L':', 0};
        if (!SetCurrentDirectoryW(drive_only)) continue;
        const wchar_t root[] = {static_cast<wchar_t>(dr.letter), L':', L'\\', 0};
        // The volume-label find-first on "X:\*.*" with attribute 8 [BYTES ISPACE.DLL
        // seg10:0x211f-0x2138]: GetVolumeInformation's label in its 8.3 form; a failed call or
        // an empty label (DOS has no label entry to find) is the failed search, null, which
        // the list shows as "Unknown_HD" [DESIGN].
        wchar_t label[MAX_PATH + 1] = L"";
        if (GetVolumeInformationW(root, label, MAX_PATH + 1, nullptr, nullptr, nullptr, nullptr, 0))
            dr.label = label_as_find_first(narrow(label));
        out.push_back(dr);
    }
    if (!cwd.empty()) SetCurrentDirectoryW(cwd.c_str());
    return out;
}

void NewGame::init() {  // ISPACE.DLL seg10:0x1e7c
    g_slot.reset();
    set_fonts();
    load_ships(env_, &ships_, &bodies_);
    const Availability a{env_.availability_cache, env_.x5, &ships_, &bodies_};
    // ZOOM LEVEL 332: 2230-2232, 2 - SetSize [BYTES ISPACE.DLL seg10:0x1f67-0x1ff7]. New Demo
    // has no control 332 or 325, so those sends do nothing there.
    if (GetDlgItem(hwnd_, 332)) {
        add(332, lang(2230));
        add(332, lang(2231));
        add(332, lang(2232));
        select(332, zoom_index(ini_int("SetSize", 1)));
    }
    for (int i : new_game_ships(a)) add(324, ships_.records[static_cast<std::size_t>(i)].name);
    // DRIVE 330 [BYTES ISPACE.DLL seg10:0x207d-0x21b4].
    std::vector<std::string> entries;
    for (const Drive& d : drives_) {
        entries.push_back(drive_entry(d.letter, d.label));
        add(330, entries.back());
    }
    const std::string drive = ini_string("Drive", "C", 10);
    if (!demo_) {  // PLAY 325 [BYTES ISPACE.DLL seg10:0x2249 seg10:0x225f]
        add(325, lang(2275));
        add(325, lang(2276));
    }
    // The preview at (client right - 104, client bottom - 103) [BYTES ISPACE.DLL seg10:0x22c8
    // seg10:0x22cf seg10:0x22e7], then the centring at seg10:0x2308.
    RECT c;
    GetClientRect(hwnd_, &c);
    preview(c.right - 104, c.bottom - 103, 96, 96);
    centre();
    select(330, drive_index(entries, drive));
    const int h = text_height();
    size_combo(330, combo_height(h, static_cast<int>(entries.size())));
    size_combo(325, play_combo_height(h));
    // Defaults [BYTES ISPACE.DLL seg10:0x233e-0x24ed].
    if (!demo_) set_text(322, ini_string("ThePlayer", "AutoPilot", 20));
    const DefaultShip d = default_ship(ini_string("MainShip", "EAGLE", 20), a);
    if (d.write_main_ship) write("MainShip", kTransformer);
    ship_ = d.name;
    if (!demo_) select(325, ini_int("PlayMode", 0));
    int index = find(324, d.name);
    if (index < 0) index = 0;
    select(324, index);
    if (d.record >= 0) show_ship(d.name);
    SetFocus(GetDlgItem(hwnd_, 324));
}

void NewGame::show_ship(const std::string& name) {
    for (std::size_t i = 0; i < ships_.records.size(); ++i) {
        if (ships_.records[i].name != name) continue;
        // TEAM: seg10:0x41cc, [Teams] Team<n + 1> (string 2279 "Team" + "s"), 20 bytes
        // [BYTES ISPACE.DLL seg10:0x41e7 seg10:0x4204 seg10:0x4213].
        std::string team;
        if (env_.ini)
            team = env_.ini->get_string(lang(2279) + "s", team_key(ships_.records[i].team), "", 20);
        set_text(326, team);
        build_preview(env_, ships_, bodies_, static_cast<int>(i), 0);
        InvalidateRect(GetDlgItem(hwnd_, kPreviewId), nullptr, TRUE);
        return;
    }
}

void NewGame::ok() {  // ISPACE.DLL seg10:0x250c(hDlg, 1)
    if (limited_ship_refused(env_.x5, ship_)) {
        box(lang(2326) + lang(2327), MB_ICONEXCLAMATION);  // 0x30 [BYTES ISPACE.DLL seg10:0x2585]
        return;
    }
    const int play = selection(325);
    for (const IniWrite& w : new_game_writes(demo_, ship_, selected_text(330), selection(332),
                                             text(322, 19), play))
        write(w.key, w.value);
    if (!demo_) {
        const int strat = ini_int("StratAsk", 0);
        if (strat_ask(play, strat)) {  // [BYTES ISPACE.DLL seg10:0x26cd seg10:0x26f5]
            const int r = box(lang(2850) + lang(2851) + lang(2852) + lang(2853),
                              MB_YESNO | MB_ICONEXCLAMATION);  // 0x34
            if (r != IDYES) return;
            write("StratAsk", std::to_string(strat + 1));  // [BYTES ISPACE.DLL seg10:0x27d9]
        }
    }
    end(1);  // DGROUP:0x1f7a cleared first [BYTES ISPACE.DLL seg10:0x2846]
}

void NewGame::command(int id, int code) {  // ISPACE.DLL seg10:0xf26
    switch (id) {
    case 1: ok(); break;
    case 333: end(0); break;
    case 324:  // seg10:0x2978: the selected name into DGROUP:0x1554
        if (code == CBN_SELCHANGE) {
            ship_ = selected_text(324);
            show_ship(ship_);
        }
        break;
    default: break;
    }
}

// ---------------------------------------------------------------- the exit dialog #244

void ExitDialog::init() {  // ISPACE.DLL seg10:0x109a
    set_icon();
    set_fonts();
    // Static 590: 180 + INSPACE.EXE 1800 + "?" (command 5) or 174 + "?" (command 7)
    // [BYTES ISPACE.DLL seg10:0x1134 seg10:0x1149 seg10:0x115d].
    std::string s;
    if (cmd_ == kCmdAbort) {
        s = lang(174) + "?";
    } else {
        s = lang(180) + (env_.exe_string ? env_.exe_string(1800) : std::string()) + "?";
    }
    set_text(590, s);
    // Save & Exit starts disabled and is enabled for x5 != 0 with x4 1-3; x6 = 1 (never
    // passed) would hide it [BYTES ISPACE.DLL seg10:0x11bd seg10:0x11d7 seg10:0x11f8].
    EnableWindow(GetDlgItem(hwnd_, 593), save_exit_enabled(env_.x4, env_.x5) ? TRUE : FALSE);
    SetFocus(GetDlgItem(hwnd_, 333));
    centre();
}

void ExitDialog::command(int id, int) {  // ISPACE.DLL seg10:0x103c
    switch (id) {
    case 1: end(kExitExit); break;
    case 333: end(kExitCancel); break;
    case 593: end(kExitSave); break;
    default: break;
    }
}

// ---------------------------------------------------------------- #799 and #1872

void Benefits::init() {  // ISPACE.DLL seg10:0x3af6
    sound(0x6e);
    sound(0x51);
    set_icon();
    set_fonts();
    bold(800);
    set_text(801, resource_text(lang_resource(5000, cmd_ == kCmdPlusPack ? 138 : 330)));
    SetFocus(GetDlgItem(hwnd_, 2));
    centre();
}

void Benefits::command(int id, int) {  // ISPACE.DLL seg10:0x5f96
    switch (id) {
    case 2: end(0); break;
    case 1:
        if (cmd_ == kCmdBenefits) {
            end(1);
        } else if (env_.open_text) {
            // WinExec("NOTEPAD " + Path + "ORDER.TXT", SW_SHOW); the dialog stays open
            // [BYTES ISPACE.DLL seg10:0x5ffa seg10:0x6006] (ADR 0008 decision 3).
            env_.open_text(env_.text_dir / L"ORDER.TXT");
        }
        break;
    default: break;
    }
}

// ---------------------------------------------------------------- Important Information #988

void Important::init() {  // ISPACE.DLL seg10:0x12be
    set_icon();
    set_fonts();
    bold(247);
    bold(989);
    set_text(245, resource_text(lang_resource(5000, 105)));
    SetFocus(GetDlgItem(hwnd_, 1));
    centre();
}

void Important::command(int id, int) {  // ISPACE.DLL seg10:0x124a
    switch (id) {
    case 1:  // I Accept: Accept = "42" [BYTES ISPACE.DLL seg10:0x1372 seg10:0x138d]
        write("Accept", std::to_string(kAccepted));
        end(1);
        break;
    case 2: end(0); break;
    default: break;
    }
}

// ---------------------------------------------------------------- Order #693

Order::~Order() {
    if (g_order == hwnd_) g_order = nullptr;
    g_logo.reset();
    g_logo_tried = false;
}

void Order::init() {  // ISPACE.DLL seg10:0x0
    g_order = hwnd_;
    g_store = env_.store;
    g_logo.reset();  // DGROUP:0x1584 starts at 0: the first paint loads the logo
    g_logo_tried = false;  // DGROUP:0x1f82; the attempt counter DGROUP:0x1f8e is the code check's
    // seg10:0x5dfe(hDlg, 0): the serial record, regenerated by seg10:0x492 when its counter is
    // above 8 or its serial 0, shown with ltoa in static 700, then the focus to edit 701.
    show_serial(false);
    set_fonts();
    for (int id : {694, 695, 710, 704, 696, 706}) bold(id);  // [BYTES ISPACE.DLL seg10:0x74]
    // 0x401 to edit 701 is EM_SETSEL in Win16: no effect on the empty edit, not sent [DESIGN].
    // Register stays disabled and the edit read-only: the code check is not ported (ADR 0008
    // decision 3) [DESIGN].
    SendDlgItemMessageW(hwnd_, 701, EM_SETREADONLY, TRUE, 0);
    EnableWindow(GetDlgItem(hwnd_, 1), FALSE);
    const DayText day = order_day_text(env_.x4);
    std::string s = lang(day.string_id);
    if (day.value) s = format_ints(s, {*day.value});
    set_text(710, s);
    if (order_tshirt(env_.x5)) set_text(704, lang(2545));
    set_text(712, format_ints(text(712, 255), {env_.tm_mon + 1, env_.tm_mday, env_.tm_year}));
    if (order_phone_hidden(env_.x4)) ShowWindow(GetDlgItem(hwnd_, 694), SW_HIDE);
    // seg5:0x446e(hPal, 5), the gold ramp of DGROUP:0x117c row 1, changes nothing visible at
    // the dialogs' true colour and is not ported [DESIGN] [BYTES ISPACE.DLL seg10:0x285
    // seg5:0x446e]. The logo child at (window width - 161, 70), 146 x 114 [BYTES ISPACE.DLL
    // seg10:0x28b seg10:0x2c6].
    RECT w;
    GetWindowRect(hwnd_, &w);
    logo_ = preview((w.right - w.left) - 161, 70, 146, 114);
    SetFocus(GetDlgItem(hwnd_, 701));
    centre();
}

void Order::show_serial(bool bump) {  // ISPACE.DLL seg10:0x5dfe(hDlg, bump)
    // The record: the original reads <LocalPath>IGRAPHIC.DLL #5500 110 and writes it back
    // (seg10:0x4666, seg10:0x4636). The port never writes a game file (ADR 0008 decision 1): its
    // copy of the record lives in the port INI's [Registration] Serial and SerialCount, and
    // until it exists the asset directory's IGRAPHIC.DLL record is read [DESIGN, for ADR 0008].
    // Dword +0, the volume serial seg10:0x4440 stores on a regeneration, is not kept: nothing
    // the port runs reads it (the drive-serial branch, ADR 0008 decision 2) [DESIGN].
    SerialRecord rec;
    bool from_ini = false;
    if (env_.ini && env_.ini->get(settings::kRegistrationSection, kSerialKey)) {
        rec.serial = env_.ini->get_int(settings::kRegistrationSection, kSerialKey, 0);
        rec.count = env_.ini->get_int(settings::kRegistrationSection, kSerialCountKey, 0);
        from_ini = true;
    }
    if (!from_ini) {
        try {
            if (env_.store) {
                const fmt::NeModule& m = env_.store->module(assets::kGraphicsDll);
                if (const fmt::NeResource* r = m.find(5500, 110)) {
                    const std::vector<std::uint8_t> bytes(m.data(*r), m.data(*r) + m.size(*r));
                    if (const auto read = serial_record(bytes)) rec = *read;
                }
            }
        } catch (const std::exception& x) {
            todo(std::string("IGRAPHIC.DLL: ") + x.what(), "ISPACE.DLL seg10:0x5dfe");
        }
    }
    // seg10:0x492: srand(GetCurrentTime() low word) and D = x5 of command 8 (the date number,
    // ISPACE.DLL DGROUP:0x213a) [BYTES ISPACE.DLL seg10:0x4a0 seg10:0x532].
    const auto generate = [this] {
        const std::uint32_t t = env_.current_time ? env_.current_time() : GetTickCount();
        return generate_serial(static_cast<std::uint16_t>(t & 0xffff), env_.x5);
    };
    const SerialStep step = serial_step(rec, bump, generate);
    // Written only when it changed: an unchanged write-back changes nothing [DESIGN].
    if (env_.ini && step.changed) {
        env_.ini->set_int(settings::kRegistrationSection, kSerialKey, step.record.serial);
        env_.ini->set_int(settings::kRegistrationSection, kSerialCountKey, step.record.count);
        if (env_.ini_changed) env_.ini_changed();
    }
    set_text(700, serial_text(step.record.serial));  // ltoa(DGROUP:0x154c, 10) into DGROUP:0x1674
    SetFocus(GetDlgItem(hwnd_, 701));
}

void Order::cleanup(int result) {  // ISPACE.DLL seg10:0x78e's cleanup
    if (logo_ && IsWindow(logo_)) DestroyWindow(logo_);
    logo_ = nullptr;
    g_order = nullptr;
    g_logo.reset();
    g_logo_tried = false;
    end(result);
}

void Order::command(int id, int) {  // ISPACE.DLL seg10:0x664
    switch (id) {
    case 709:  // WinExec("NOTEPAD " + Path + "ORDER.TXT") [BYTES ISPACE.DLL seg10:0x6c3]
        if (env_.open_text) env_.open_text(env_.text_dir / L"ORDER.TXT");
        break;
    case 711:  // README.TXT [BYTES ISPACE.DLL seg10:0x6ea]
        if (env_.open_text) env_.open_text(env_.text_dir / L"README.TXT");
        break;
    case 333: cleanup(0); break;
    case 1:  // disabled (ADR 0008 decision 3)
        todo("Order Register code check (ADR 0008 decision 3)", "ISPACE.DLL seg10:0x78e");
        break;
    default: break;
    }
}

// ---------------------------------------------------------------- Save Game #167

void SaveGame::init() {  // ISPACE.DLL seg10:0xb0e
    // The GAME combo is a run-time child [BYTES ISPACE.DLL seg10:0xc0b].
    CreateWindowExW(0, L"combobox", L"", 0x50210102, 65, 10, 140, 30, hwnd_,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(332)), env_.instance, nullptr);
    set_fonts();
    int added = 0;
    for (const std::string& n : save_game_names(saves_, &added)) add(332, n);
    size_combo(332, combo_height(text_height(), added));
    const int index = find(332, ini_string("Restore", "", 20));
    if (index >= 0) select(332, index);
    SendDlgItemMessageW(hwnd_, 332, CB_LIMITTEXT, 8, 0);
    SetFocus(GetDlgItem(hwnd_, 332));
    centre();
}

void SaveGame::command(int id, int) {  // ISPACE.DLL seg10:0xdc0
    switch (id) {
    case 1: {
        if (const auto name = save_name(text(332, 8))) {
            write("Restore", *name);
            end(1);  // the ship table is freed (seg10:0x4168)
        } else {
            set_text(332, "");
        }
        break;
    }
    case 333: end(0); break;
    default: break;
    }
}

// ---------------------------------------------------------------- ordinal 10

int ordinal10(Env& env, int cmd) {
    switch (cmd) {
    case kCmdGameStart: {  // [BYTES ISPACE.DLL seg10:0x436f-0x43aa]
        RestoreGame restore(env, list_saves(env.local_path));
        const int r = restore.run_modal();
        if (r != 1) return r;
        NewGame game(env, false, NewGame::system_drives());  // DGROUP:0x1f7a = 0
        return game.run_modal();
    }
    case kCmdNewDemo: {  // DGROUP:0x1f7a = 1 [BYTES ISPACE.DLL seg10:0x43b6]
        NewGame demo(env, true, NewGame::system_drives());
        return demo.run_modal();
    }
    case kCmdSaveGame: {
        SaveGame save(env, list_saves(env.local_path));
        return save.run_modal();
    }
    case kCmdExit:
    case kCmdAbort: {
        ExitDialog d(env, cmd);
        return d.run_modal();
    }
    case kCmdOrder: {  // Important Information follows a 1 [BYTES ISPACE.DLL seg10:0x434c]
        Order o(env);
        const int r = o.run_modal();
        if (r != 1) return r;
        Important i(env);
        return i.run_modal();
    }
    case kCmdImportant: {
        Important i(env);
        return i.run_modal();
    }
    case kCmdBenefits:
    case kCmdPlusPack: {
        Benefits b(env, cmd);
        return b.run_modal();
    }
    default:
        if (env.todo) env.todo("ordinal 10 command " + std::to_string(cmd), "ISPACE.DLL seg9:0x145e");
        return 0;
    }
}

}  // namespace is::dialogs
#endif
