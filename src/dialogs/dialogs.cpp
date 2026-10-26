// The dialog layer of ISPACE.DLL ordinal 10 (see dialogs.h). Every procedure cites the
// ISPACE.DLL routine it ports; the shared rules are in rules.cpp.
#ifdef _WIN32
#include "dialogs/dialogs.h"

#include <exception>

#include "assets/asset_store.h"
#include "settings/settings.h"

namespace is::dialogs {

namespace {

// The LevClass windows paint the frame only while no Order dialog is open (ISPACE.DLL
// DGROUP:0x1f82 = 0) [BYTES ISPACE.DLL seg10:0x30b9].
HWND g_order = nullptr;
// The todo sink of the open dialog, for the LevClass procedure.
std::function<void(const std::string&, const char*)> g_todo;
bool g_logged_bitmap = false, g_logged_logo = false;

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

// The frame of ISPACE.DLL seg10:0x309c's WM_PAINT [BYTES ISPACE.DLL seg10:0x3103-0x3303].
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
        if (!order) {
            // seg10:0x2c68 for the slot: PatBlt BLACKNESS (3, 3, 90, 90) on the slot window's
            // DC [BYTES ISPACE.DLL seg10:0x2b70]. The port blacks its own window only; the
            // original also takes GetDC of slot 1, which these dialogs never create [DESIGN].
            HDC d = GetDC(h);
            PatBlt(d, 3, 3, 90, 90, BLACKNESS);
            ReleaseDC(h, d);
            // The ship image (seg10:0x2aac, ISPACE.DLL seg5:0x0 and the stretch seg10:0x2d3c)
            // is not ported: the preview stays black [OPEN].
            if (!g_logged_bitmap && g_todo) {
                g_logged_bitmap = true;
                g_todo("ship preview bitmap [OPEN]; the LevClass child stays black",
                       "ISPACE.DLL seg10:0x2aac");
            }
        } else if (!g_logged_logo && g_todo) {
            // The Order logo seg10:0x2f4 after seg5:0x446e(palette, 5) [OPEN].
            g_logged_logo = true;
            g_todo("Order logo [OPEN]; the LevClass child stays black", "ISPACE.DLL seg10:0x2f4");
        }
        return 0;
    }
    case WM_NCPAINT: return 0;  // [BYTES ISPACE.DLL seg10:0x3311]
    case WM_COMMAND:            // posted on to the parent [BYTES ISPACE.DLL seg10:0x3335]
        PostMessageW(GetParent(h), WM_COMMAND, w, l);
        return 0;
    default: break;
    }
    return DefWindowProcW(h, msg, w, l);
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
        win16_edit_borders();
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

// A Win32 dialog of a version-4 program draws its WS_BORDER edits with a 3D client edge
// (WS_EX_CLIENTEDGE, 2 pixels); the Win16 dialog manager draws them with a 1-pixel border,
// as the oracle shows (start-0001: Restore's 330, New Game's PILOT, #799's 801, Order's 701).
// The port takes the client edge off again and keeps WS_BORDER [DESIGN].
void Dialog::win16_edit_borders() {
    for (HWND c = GetWindow(hwnd_, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        wchar_t name[16] = L"";
        GetClassNameW(c, name, 16);
        if (lstrcmpiW(name, L"Edit") != 0) continue;
        const LONG_PTR ex = GetWindowLongPtrW(c, GWL_EXSTYLE);
        if (!(ex & WS_EX_CLIENTEDGE)) continue;
        SetWindowLongPtrW(c, GWL_EXSTYLE, ex & ~static_cast<LONG_PTR>(WS_EX_CLIENTEDGE));
        SetWindowLongPtrW(c, GWL_STYLE, GetWindowLongPtrW(c, GWL_STYLE) | WS_BORDER);
        SetWindowPos(c, nullptr, 0, 0, 0, 0,
                     SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
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
    // seg10:0x223c]; the port saves the whole current directory.
    wchar_t cwd[MAX_PATH + 1] = L"";
    const DWORD cwd_len = GetCurrentDirectoryW(MAX_PATH + 1, cwd);
    const DWORD mask = GetLogicalDrives();
    for (int d = 3; d <= 26; ++d) {  // C: to Z: [BYTES ISPACE.DLL seg10:0x20b1]
        if (!(mask & (1u << (d - 1)))) continue;
        Drive dr;
        dr.letter = static_cast<char>('A' + d - 1);
        // _chdrive(d) fails: the drive is skipped [BYTES ISPACE.DLL seg10:0x2104]. The Win32
        // counterpart is SetCurrentDirectory("X:"), which fails for a drive without media,
        // as the oracle's empty CD drive is not listed [ORACLE start-0001] [DESIGN].
        const wchar_t drive_only[] = {static_cast<wchar_t>(dr.letter), L':', 0};
        if (!SetCurrentDirectoryW(drive_only)) continue;
        const wchar_t root[] = {static_cast<wchar_t>(dr.letter), L':', L'\\', 0};
        wchar_t label[MAX_PATH + 1] = L"";
        if (GetVolumeInformationW(root, label, MAX_PATH + 1, nullptr, nullptr, nullptr, nullptr, 0))
            dr.label = label_as_find_first(narrow(label));
        out.push_back(dr);
    }
    if (cwd_len > 0 && cwd_len <= MAX_PATH) SetCurrentDirectoryW(cwd);
    return out;
}

void NewGame::init() {  // ISPACE.DLL seg10:0x1e7c
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
}

void Order::init() {  // ISPACE.DLL seg10:0x0
    g_order = hwnd_;  // DGROUP:0x1f82; the attempt counter DGROUP:0x1f8e is the code check's
    // seg10:0x5dfe(hDlg, 0): the serial of IGRAPHIC.DLL #5500 110 into static 700, then the
    // focus to edit 701. The original reads <LocalPath>IGRAPHIC.DLL and writes the record
    // back; the port reads the asset directory's copy and never writes (ADR 0002).
    std::vector<std::uint8_t> record;
    try {
        if (env_.store) {
            const fmt::NeModule& m = env_.store->module(assets::kGraphicsDll);
            if (const fmt::NeResource* r = m.find(5500, 110))
                record.assign(m.data(*r), m.data(*r) + m.size(*r));
        }
    } catch (const std::exception& x) {
        todo(std::string("IGRAPHIC.DLL: ") + x.what(), "ISPACE.DLL seg10:0x5dfe");
    }
    if (const auto serial = order_serial(record)) {
        set_text(700, *serial);
    } else {
        // A zero serial or a count above 8: the original draws a new serial (ISPACE.DLL
        // seg10:0x492) and writes it into IGRAPHIC.DLL; the port shows the template text
        // [OPEN].
        todo("Order serial generator and its IGRAPHIC.DLL write [OPEN]", "ISPACE.DLL seg10:0x492");
    }
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
    // The logo child at (window width - 161, 70), 146 x 114 [BYTES ISPACE.DLL seg10:0x28b
    // seg10:0x2c6].
    RECT w;
    GetWindowRect(hwnd_, &w);
    logo_ = preview((w.right - w.left) - 161, 70, 146, 114);
    SetFocus(GetDlgItem(hwnd_, 701));
    centre();
}

void Order::cleanup(int result) {  // ISPACE.DLL seg10:0x78e's cleanup
    if (logo_ && IsWindow(logo_)) DestroyWindow(logo_);
    logo_ = nullptr;
    g_order = nullptr;
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
