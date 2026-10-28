// The Navigation window (see nav.h).
#ifdef _WIN32
#include "nav/nav.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <string>
#include <vector>

#include "assets/asset_store.h"
#include "gfx/surface.h"
#include "nav/files.h"
#include "nav/nav_rules.h"
#include "settings/settings.h"

namespace is::nav {

namespace {

class HostStrings : public Strings {
public:
    std::string lang(int id) override;
};

// The window's parts that live from WM_CREATE to the close.
struct Live {
    std::shared_ptr<Fs> fs;
    std::unique_ptr<Walker> walker;
    std::unique_ptr<Tree> tree;
    std::unique_ptr<IconReader> icons;
    Game game;
    HBITMAP fldr = nullptr;     // DGROUP:0x6aec, IGRAPHIC.DLL BITMAP FLDR
    HBITMAP files = nullptr;    // DGROUP:0x6aee, IGRAPHIC.DLL BITMAP FILES
    HBITMAP preview = nullptr;  // DGROUP:0x94ba, 32 x 32
    std::uint32_t* preview_bits = nullptr;
};

Host g_host;
Parts g_parts;
HostStrings g_strings;
Records g_own_records;
Live g_live;
Selected g_selected;
HWND g_nav = nullptr;          // DGROUP:0xd1e
int g_arg = 0;                 // DGROUP:0xd20: Enter is accepted only when set
int g_drive = 0;               // DGROUP:0x94ac at open; DGROUP:0xd18 = 'C' + it
int g_sel = 0;                 // DGROUP:0x94b4, the directory selection
int g_accepted = 0;            // DGROUP:0xd1c, the accepted selection
std::string g_dir;             // DGROUP:0x6990
std::string g_status;          // DGROUP:0x6a24
bool g_demo_named = false;     // DGROUP:0x94be
RECT g_rect{kDefaultLeft, kDefaultTop, kDefaultLeft + kDefaultRight, kDefaultTop + kDefaultBottom};  // DGROUP:0x6af0
bool g_logged_glyphs = false;
Accepted g_last_accepted;      // DGROUP:0x94a2 and DGROUP:0x94b0 of the last Enter (M3 s2 fixer)

constexpr const wchar_t* kClass = L"IS_NAV";

std::string HostStrings::lang(int id) { return g_host.lang ? g_host.lang(id) : std::string(); }

std::wstring wide(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(1252, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(1252, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::string narrow(const std::wstring& w) {
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(1252, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(1252, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

std::string lang(int id) { return g_strings.lang(id); }
void todo(const std::string& what, const char* address) {
    if (g_host.todo) g_host.todo(what, address);
}
void log(const std::string& s) {
    if (g_host.log) g_host.log(s);
}

Records& recs() { return g_host.records ? *g_host.records : g_own_records; }

int ini_int(const char* key, int def) {
    return g_host.ini ? g_host.ini->get_int(settings::kGameSection, key, def) : def;
}

void place(HWND parent, int id, const Box& b) {
    if (HWND c = GetDlgItem(parent, id)) MoveWindow(c, b.x, b.y, b.w, b.h, TRUE);
}

// The children of seg6:0x23d4 that the window builds itself. Enter, Help and the lists get no
// WM_SETFONT: seg6:0x23d4 sends DGROUP:0x90fe only to the box controls (shell.md item 11); the
// owner-draw routines select it into the item DC. The call after each child, seg2:0x1a7a, is the
// Ctl3dSubclassCtl wrapper, which does nothing without CTL3D, as in the port [BYTES seg2:0x1a84].
HWND child(HWND parent, const wchar_t* cls, const std::string& text, DWORD style, const Box& b, int id) {
    return CreateWindowExW(0, cls, wide(text).c_str(), style, b.x, b.y, b.w, b.h, parent,
                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g_host.instance, nullptr);
}

HWND dir_list() { return g_nav ? GetDlgItem(g_nav, kDirListId) : nullptr; }
HWND file_list() { return g_nav ? GetDlgItem(g_nav, kFileListId) : nullptr; }

// ---- GDI

COLORREF colour(std::uint32_t c) { return static_cast<COLORREF>(c); }

void run(HDC dc, const std::vector<Op>& ops) {
    for (const Op& o : ops) {
        switch (o.kind) {
        case Op::Kind::kLine: {
            HPEN pen = CreatePen(PS_SOLID, 1, colour(o.colour));
            HGDIOBJ old = SelectObject(dc, pen);
            MoveToEx(dc, o.x0, o.y0, nullptr);
            LineTo(dc, o.x1, o.y1);
            SelectObject(dc, old);
            DeleteObject(pen);
            break;
        }
        case Op::Kind::kRect: {
            HPEN pen = CreatePen(PS_SOLID, 1, colour(o.colour));
            HGDIOBJ old_pen = SelectObject(dc, pen);
            HGDIOBJ old_brush = SelectObject(dc, GetStockObject(o.filled ? LTGRAY_BRUSH : NULL_BRUSH));
            Rectangle(dc, o.x0, o.y0, o.x1, o.y1);
            SelectObject(dc, old_brush);
            SelectObject(dc, old_pen);
            DeleteObject(pen);
            break;
        }
        case Op::Kind::kPixel:
            SetPixel(dc, o.x0, o.y0, colour(o.colour));
            break;
        case Op::Kind::kFill:
            if ((o.colour & 0xFFFFFFu) == 0) {
                PatBlt(dc, o.x0, o.y0, o.x1, o.y1, BLACKNESS);
            } else {
                HBRUSH b = CreateSolidBrush(colour(o.colour));
                HGDIOBJ old = SelectObject(dc, b);
                PatBlt(dc, o.x0, o.y0, o.x1, o.y1, PATCOPY);
                SelectObject(dc, old);
                DeleteObject(b);
            }
            break;
        case Op::Kind::kText: {
            HFONT f = CreateFontA(o.height, o.width, 0, 0, o.weight, 0, 0, 0, 0, 0, 0, 2, 0x22, o.face.c_str());
            HGDIOBJ old = SelectObject(dc, f);
            const UINT align = SetTextAlign(dc, o.align);
            const COLORREF tc = SetTextColor(dc, colour(o.colour));
            const COLORREF bk = SetBkColor(dc, colour(o.bk));
            const int mode = SetBkMode(dc, o.opaque ? OPAQUE : TRANSPARENT);
            const std::wstring w = wide(o.text);
            TextOutW(dc, o.x0, o.y0, w.c_str(), static_cast<int>(w.size()));
            SetBkMode(dc, mode);
            SetBkColor(dc, bk);
            SetTextColor(dc, tc);
            SetTextAlign(dc, align);
            SelectObject(dc, old);
            DeleteObject(f);
            break;
        }
        }
    }
}

// A colour bitmap of a DIB with its own colours [DESIGN: LoadBitmap made a device bitmap; the
// window draws on a true-colour desktop].
HBITMAP colour_bitmap(const fmt::Dib& d) {
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = d.width;
    bi.bmiHeader.biHeight = -d.height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP h = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!h || !bits) return h;
    const std::vector<std::uint32_t> px = fmt::to_bgra(d);
    std::copy(px.begin(), px.end(), static_cast<std::uint32_t*>(bits));
    return h;
}

// seg10:0x6d38 (ISPACE ordinal 3), LoadBitmap from IGRAPHIC.DLL [BYTES seg6:0x2401 seg6:0x2404]:
// a black-and-white DIB becomes a monochrome bitmap, as LoadBitmap made it, so its blits take
// the DC's text and background colours [REF Win16 SDK LoadBitmap].
HBITMAP load_bitmap(const char* name) {
    const assets::AssetStore* store = g_host.store ? g_host.store() : nullptr;
    if (!store) return nullptr;
    try {
        const fmt::Dib d = store->bitmap(name);
        if (gfx::is_black_and_white(d)) {
            const std::vector<std::uint8_t> bits = gfx::dib_mono_bits(d);
            return CreateBitmap(d.width, d.height, 1, 1, bits.data());
        }
        return colour_bitmap(d);
    } catch (const std::exception& x) {
        log(std::string("navigation: IGRAPHIC.DLL BITMAP ") + name + ": " + x.what());
        return nullptr;
    }
}

void free_gdi() {
    // seg6:0x2ebc deletes DGROUP:0x6aec[0 ... 1] and DGROUP:0x94ba [BYTES seg6:0x2fa0 seg6:0x2fbf].
    for (HBITMAP* b : {&g_live.fldr, &g_live.files, &g_live.preview})
        if (*b) {
            DeleteObject(*b);
            *b = nullptr;
        }
    g_live.preview_bits = nullptr;
}

void ensure_preview() {
    if (g_live.preview) return;
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = fmt::kIconCell;
    bi.bmiHeader.biHeight = -fmt::kIconCell;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    g_live.preview = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    g_live.preview_bits = static_cast<std::uint32_t*>(bits);
    if (g_live.preview_bits)
        std::fill(g_live.preview_bits, g_live.preview_bits + fmt::kIconCell * fmt::kIconCell, 0xFF000000u);
}

void preview_cell(const fmt::IconCell& c) {
    GdiFlush();
    std::copy(c.bgra.begin(), c.bgra.end(), g_live.preview_bits);
}

// DGROUP:0x94ba after seg6:0x4368's calls (nav.md "Preview").
void render_preview(const Preview& p) {
    ensure_preview();
    if (!g_live.preview_bits) return;
    fmt::IconCell black;
    black.bgra.fill(0xFF000000u);
    preview_cell(black);  // PatBlt BLACKNESS [BYTES seg6:0x44cb]
    switch (p.kind) {
    case Preview::Kind::kBlack:
        break;
    case Preview::Kind::kIcon:  // DrawIcon(ExtractIcon(path, 0)) [BYTES seg6:0x44ec]
        if (g_live.icons)
            if (auto c = g_live.icons->cell(p.path)) preview_cell(*c);
        break;
    case Preview::Kind::kBuiltin: {  // LoadIcon(ISPACE.DLL, id) [BYTES seg6:0x448c seg6:0x4498]
        const assets::AssetStore* store = g_host.store ? g_host.store() : nullptr;
        if (!store) break;
        try {
            if (auto c = group_cell(store->group_icon(assets::kGameDll, static_cast<std::uint16_t>(p.icon_id))))
                preview_cell(*c);
        } catch (const std::exception& x) {
            log("navigation: ISPACE.DLL GROUP_ICON " + std::to_string(p.icon_id) + ": " + x.what());
        }
        break;
    }
    case Preview::Kind::kPlaceholder: {  // seg9:0x352(dc, 0, 0, 32, 0, name) [BYTES seg6:0x455c]
        HDC mem = CreateCompatibleDC(nullptr);
        HGDIOBJ old = SelectObject(mem, g_live.preview);
        run(mem, placeholder(0, 0, fmt::kIconCell, 0, p.name, g_drive));
        SelectObject(mem, old);
        DeleteDC(mem);
        GdiFlush();
        break;
    }
    }
}

// seg6:0x5b4c(dc, paint): the status band, on WM_PAINT the preview frame, and the preview
// stretched 2x [BYTES seg6:0x5b4c].
void draw_status(HDC dc, bool paint) {
    RECT c;
    GetClientRect(g_nav, &c);
    run(dc, status_band(c.right, c.bottom, g_status));
    if (paint) run(dc, preview_frame(c.right));
    ensure_preview();
    if (!g_live.preview) return;
    HDC mem = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(mem, g_live.preview);
    const Rect r = preview_rect(c.right);
    const int mode = SetStretchBltMode(dc, COLORONCOLOR);
    StretchBlt(dc, r.x, r.y, r.w, r.h, mem, 0, 0, fmt::kIconCell, fmt::kIconCell, SRCCOPY);
    SetStretchBltMode(dc, mode);
    SelectObject(mem, old);
    DeleteDC(mem);
}

void draw_status_now() {  // seg6:0x5b4c(0, 0): GetDC of the window
    if (!g_nav) return;
    if (HDC dc = GetDC(g_nav)) {
        draw_status(dc, false);
        ReleaseDC(g_nav, dc);
    }
}

// ---- the lists

// Replays the tree's edits into listbox 0x65 (LB_RESETCONTENT, LB_INSERTSTRING at the index the
// original's sorted LB_ADDSTRING or its LB_INSERTSTRING gave, LB_DELETESTRING) [DESIGN].
void replay() {
    HWND l = dir_list();
    if (!g_live.tree) return;
    for (const ListEdit& e : g_live.tree->take_edits()) {
        if (!l) continue;
        switch (e.op) {
        case ListEdit::Op::kReset: SendMessageW(l, LB_RESETCONTENT, 0, 0); break;
        case ListEdit::Op::kInsert:
            SendMessageW(l, LB_INSERTSTRING, static_cast<WPARAM>(e.index),
                         reinterpret_cast<LPARAM>(wide(e.text).c_str()));
            break;
        case ListEdit::Op::kDelete: SendMessageW(l, LB_DELETESTRING, static_cast<WPARAM>(e.index), 0); break;
        }
    }
}

int cursel() {
    HWND l = dir_list();
    return l ? static_cast<int>(SendMessageW(l, LB_GETCURSEL, 0, 0)) : -1;
}

std::string item_text(HWND list, int index) {
    const LRESULT n = SendMessageW(list, LB_GETTEXTLEN, static_cast<WPARAM>(index), 0);
    if (n <= 0 || n > 4096) return {};
    std::wstring w(static_cast<std::size_t>(n) + 1, L'\0');
    const LRESULT got = SendMessageW(list, LB_GETTEXT, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(w.data()));
    w.resize(got > 0 ? static_cast<std::size_t>(got) : 0);
    return narrow(w);
}

// seg6:0x5524 for DGROUP:0x6990 [BYTES seg6:0x5524].
void select_dir(int sel) {
    if (!g_live.walker || !g_live.tree) return;
    const int sub = g_live.tree->sub_digit(sel);
    log("navigation select " + g_dir);
    g_selected = nav::select(*g_live.walker, *g_live.icons, g_strings, recs(), g_live.game, g_dir, sub);
    // seg6:0x2dba(kind) [BYTES seg6:0x55df seg6:0x56e9 seg6:0x57c6 seg6:0x586a]
    if (g_parts.show_box) g_parts.show_box(g_nav, g_selected.box);
    if (HWND f = file_list())  // LB_ADDSTRING into the sorted list [BYTES seg6:0x426a]
        for (const std::string& item : g_selected.pane.items)
            SendMessageW(f, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide(item).c_str()));
    if (g_selected.completed) {  // LB_DELETESTRING, LB_INSERTSTRING [BYTES seg6:0x5a32 seg6:0x5a51]
        g_live.tree->set_state(sel, kStateComplete);
        replay();
    }
    if (g_selected.enter != EnterButton::kUnchanged)
        if (HWND e = GetDlgItem(g_nav, kEnterId)) EnableWindow(e, g_selected.enter == EnterButton::kEnabled);
    g_status = g_selected.status;
    render_preview(g_selected.pane.preview);
    draw_status_now();
}

// seg6:0x5318: the selected path into DGROUP:0x6990 and seg6:0x5524 [BYTES seg6:0x5318].
// WM_SETREDRAW FALSE for the life of the guard; TRUE and a repaint when it ends, also when the
// selection throws (M3 s1 fixer) [DESIGN].
class RedrawOff {
public:
    explicit RedrawOff(HWND h) : h_(h) {
        if (h_) SendMessageW(h_, WM_SETREDRAW, FALSE, 0);
    }
    ~RedrawOff() {
        if (h_) {
            SendMessageW(h_, WM_SETREDRAW, TRUE, 0);
            InvalidateRect(h_, nullptr, TRUE);
        }
    }
    RedrawOff(const RedrawOff&) = delete;
    RedrawOff& operator=(const RedrawOff&) = delete;

private:
    HWND h_;
};

void select_current() {
    HWND f = file_list();
    {
        RedrawOff off(f);
        if (f) SendMessageW(f, LB_RESETCONTENT, 0, 0);
        const int sel = cursel();
        // LB_GETCURSEL is the row; no selection reads row -1, which the port takes as row 0 [DESIGN].
        if (g_live.tree) g_dir = g_live.tree->path(sel < 0 ? 0 : sel);
        select_dir(sel < 0 ? 0 : sel);
    }
    if (HWND l = dir_list()) SetFocus(l);
}

// seg6:0x37ca: the tree and the initial selection [BYTES seg6:0x37ca].
void fill_tree() {
    HWND l = dir_list();
    {
        RedrawOff off(l);  // [BYTES seg6:0x37ca] (M3 s1 fixer)
        g_live.tree->fill(drive_root(g_drive));
        replay();
    }
    InitialInputs in;
    in.wave = g_host.wave ? g_host.wave() : 0;
    in.command_line = g_host.command_line && g_host.command_line();
    in.demo_type = g_host.demo_type ? g_host.demo_type() : 519;
    if (in.command_line && (in.demo_type == 521 || in.demo_type == 522) && g_host.ini)
        in.track = g_host.ini->get_string(settings::kGameSection, "Track", "", 0x6d);  // [BYTES seg6:0x3c84]
    in.current = g_dir;
    in.demo_named = g_demo_named;
    const InitialSelection s = initial_selection(*g_live.tree, g_strings, recs(), g_live.game, in);
    replay();
    g_dir = s.dir;
    g_demo_named = s.demo_named;
    g_sel = s.index;
    if (l) SendMessageW(l, LB_SETCURSEL, static_cast<WPARAM>(s.index), 0);
    if (g_parts.initial) g_parts.initial(s);
}

// ---- WM_CREATE

void build(HWND h) {
    // seg6:0x23d4 starts with EnableWindow(main, FALSE) [BYTES seg6:0x23f4]; seg6:0x2ebc
    // enables it again.
    if (g_host.main) EnableWindow(g_host.main, FALSE);
    if (!g_live.fldr) g_live.fldr = load_bitmap("FLDR");  // DGROUP:0xd56 [BYTES seg6:0x2401]
    if (!g_live.files) g_live.files = load_bitmap("FILES");  // DGROUP:0xd5b [BYTES seg6:0x2417]
    if ((!g_live.fldr || !g_live.files) && !g_logged_glyphs) {
        g_logged_glyphs = true;
        todo("Navigation glyphs: IGRAPHIC.DLL BITMAP FLDR or FILES is not available", "seg6:0x2401");
    }
    RECT c;
    GetClientRect(h, &c);
    child(h, L"button", load(g_strings, kEnterString, 0x28), kEnterStyle, enter_create_box(c.right, c.bottom), kEnterId);
    child(h, L"button", load(g_strings, kHelpString, 0x28), kHelpStyle, help_create_box(c.right, c.bottom), kHelpId);
    if (HWND l = child(h, L"listbox", "", kDirListStyle, dir_list_box(c.right, c.bottom), kDirListId))
        SendMessageW(l, LB_SETHORIZONTALEXTENT, kDirExtent, 0);  // Win16 0x415 [BYTES seg6:0x2507]
    if (HWND f = child(h, L"listbox", "", kFileListStyle, file_list_create_box(c.right, c.bottom), kFileListId))
        SendMessageW(f, LB_SETCOLUMNWIDTH, kFileColumn, 0);  // Win16 0x416 [BYTES seg6:0x2527]
    if (g_parts.create_boxes) g_parts.create_boxes(h, c.right, c.bottom, g_host.font);

    // The walk's world [DESIGN ADR 0012].
    g_live.fs = g_host.fs ? g_host.fs : std::shared_ptr<Fs>(std::make_shared<Win32Fs>());
    g_live.walker = std::make_unique<Walker>(*g_live.fs, g_host.log);
    IconReader::Load read = g_host.read_module;
    if (!read) {
        std::weak_ptr<Fs> weak = g_live.fs;
        read = [weak](const std::string& dos) {
            if (auto fs = weak.lock())
                if (auto* w = dynamic_cast<Win32Fs*>(fs.get()))
                    return fmt::IconSource::load(std::filesystem::path(w->native(dos)));
            return fmt::IconSource(std::vector<std::uint8_t>{});
        };
    }
    g_live.icons = std::make_unique<IconReader>(std::move(read));
    g_live.game.edition = g_host.edition ? g_host.edition() : 0;
    g_live.game.player = g_host.player ? g_host.player() != 0 : true;
    g_live.game.tree_key = ini_int("Tree", 0);  // DGROUP:0x94bc [BYTES seg6:0x37ef]
    g_live.tree = std::make_unique<Tree>(*g_live.walker, g_strings, recs(), g_live.game);

    fill_tree();       // seg6:0x37ca [BYTES seg6:0x23d4]
    select_current();  // seg6:0x5318 [BYTES seg6:0x2d96]
    // Sounds 0x6e and 0x4d while DGROUP:0xd20 != 0 [BYTES seg6:0x2d99 seg6:0x2da2 seg6:0x2daa].
    if (g_arg != 0 && g_host.sound) {
        g_host.sound(0x6e);
        g_host.sound(0x4d);
    }
}

// ---- WM_DRAWITEM

// seg6:0x49c0: a row of listbox 0x65 [BYTES seg6:0x49c0].
void draw_tree_row(const DRAWITEMSTRUCT* d) {
    HDC dc = d->hDC;
    const int index = static_cast<int>(d->itemID);
    const std::string item = item_text(d->hwndItem, index);
    const int count = static_cast<int>(SendMessageW(d->hwndItem, LB_GETCOUNT, 0, 0));
    const std::string next = index + 1 < count ? item_text(d->hwndItem, index + 1) : std::string();
    const RECT& r = d->rcItem;
    const TreeRowLook look = tree_row(item, index, count, next, r.left, r.top, r.bottom);
    const bool sel = (d->itemState & ODS_SELECTED) != 0;
    const COLORREF old_text = GetTextColor(dc);
    FillRect(dc, &r, GetSysColorBrush(COLOR_WINDOW));
    if (sel) {
        RECT h = r;
        h.left = look.highlight_left;
        FillRect(dc, &h, GetSysColorBrush(COLOR_HIGHLIGHT));
    }
    HGDIOBJ old_font = g_host.font ? SelectObject(dc, g_host.font) : nullptr;
    for (const Segment& s : look.lines) {  // the DC's current pen [BYTES seg6:0x4ba5]
        MoveToEx(dc, s.x0, s.y0, nullptr);
        LineTo(dc, s.x1, s.y1);
    }
    if (g_live.fldr) {
        HDC mem = CreateCompatibleDC(dc);
        HGDIOBJ old = SelectObject(mem, g_live.fldr);
        BitBlt(dc, look.glyph_left, look.glyph_top, kFolderW, kFolderH, mem, kFolderMaskX, 0, SRCAND);
        BitBlt(dc, look.glyph_left, look.glyph_top, kFolderW, kFolderH, mem, look.glyph_x, 0, SRCPAINT);
        SelectObject(mem, old);
        DeleteDC(mem);
    }
    SetTextColor(dc, GetSysColor(sel ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
    SetBkMode(dc, TRANSPARENT);
    const std::wstring w = wide(look.text);
    TextOutW(dc, look.text_x, look.text_y, w.c_str(), static_cast<int>(w.size()));
    if (old_font) SelectObject(dc, old_font);
    SetTextColor(dc, old_text);
}

// seg6:0x47e8: a row of listbox 0x6b, or seg6:0x49c0 for listbox 0x65 [BYTES seg6:0x47e8].
void draw_row(const DRAWITEMSTRUCT* d) {
    if (d->CtlID == static_cast<UINT>(kDirListId)) {
        draw_tree_row(d);
        return;
    }
    HDC dc = d->hDC;
    const bool sel = (d->itemState & ODS_SELECTED) != 0;
    const COLORREF old_text = GetTextColor(dc);
    const RECT& r = d->rcItem;
    const FileRowLook look = file_row(item_text(d->hwndItem, static_cast<int>(d->itemID)), r.left, r.top);
    FillRect(dc, &r, GetSysColorBrush(sel ? COLOR_HIGHLIGHT : COLOR_WINDOW));
    HGDIOBJ old_font = g_host.font ? SelectObject(dc, g_host.font) : nullptr;
    if (g_live.files) {
        HDC mem = CreateCompatibleDC(dc);
        HGDIOBJ old = SelectObject(mem, g_live.files);
        BitBlt(dc, look.glyph_left, look.glyph_top, kFileGlyphW, kFileGlyphH, mem, kFileMaskX, 0, SRCAND);
        if (look.glyph_colour) SetTextColor(dc, colour(*look.glyph_colour));
        BitBlt(dc, look.glyph_left, look.glyph_top, kFileGlyphW, kFileGlyphH, mem, 0, 0, SRCPAINT);
        SelectObject(mem, old);
        DeleteDC(mem);
    }
    SetTextColor(dc, GetSysColor(sel ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
    SetBkMode(dc, TRANSPARENT);
    const std::wstring w = wide(look.text);
    TextOutW(dc, look.text_x, look.text_y, w.c_str(), static_cast<int>(w.size()));
    if (old_font) SelectObject(dc, old_font);
    SetTextColor(dc, old_text);
}

RECT focus_rect(const DRAWITEMSTRUCT* d) {
    RECT r = d->rcItem;
    if (d->CtlID == static_cast<UINT>(kDirListId) && d->itemID != static_cast<UINT>(-1))
        r.left += focus_indent(item_text(d->hwndItem, static_cast<int>(d->itemID)), static_cast<int>(d->itemID));
    return r;
}

// seg6:0x4ea8: the row, then the focus rectangle [BYTES seg6:0x4ea8].
void draw_entire(const DRAWITEMSTRUCT* d) {
    draw_row(d);
    if (d->itemState & ODS_FOCUS) {
        const RECT r = focus_rect(d);
        DrawFocusRect(d->hDC, &r);
    }
}

// WM_DRAWITEM: itemID -1 and ODA_FOCUS go to seg6:0x4e04, ODA_DRAWENTIRE to seg6:0x4ea8,
// ODA_SELECT to seg6:0x47e8 [BYTES seg6:0x1f22 seg6:0x4e04].
void draw_item(const DRAWITEMSTRUCT* d) {
    if (d->itemID == static_cast<UINT>(-1) || d->itemAction == ODA_FOCUS) {
        if (d->itemID == static_cast<UINT>(-1) || (d->itemState & ODS_FOCUS)) {
            const RECT r = focus_rect(d);
            DrawFocusRect(d->hDC, &r);
        } else {
            draw_entire(d);
        }
    } else if (d->itemAction == ODA_DRAWENTIRE) {
        draw_entire(d);
    } else if (d->itemAction == ODA_SELECT) {
        draw_row(d);
    }
}

// ---- the procedure

LRESULT handle(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_CREATE:  // [BYTES seg6:0x1f35 seg6:0x1f82]
        g_nav = h;
        build(h);
        return 0;
    case WM_MOVE:  // GetWindowRect into DGROUP:0x6af0
        GetWindowRect(h, &g_rect);
        return 0;
    case WM_SIZE: {  // [BYTES seg6:0x1f22]
        GetWindowRect(h, &g_rect);
        RECT c;
        GetClientRect(h, &c);
        place(h, kEnterId, enter_box(c.right, c.bottom));
        place(h, kHelpId, help_box(c.right, c.bottom));
        place(h, kDirListId, dir_list_box(c.right, c.bottom));
        place(h, kFileListId, file_list_box(c.right, c.bottom));
        if (g_parts.layout_boxes) g_parts.layout_boxes(h, c.right, c.bottom);
        return 0;
    }
    case WM_GETMINMAXINFO: {
        auto* mm = reinterpret_cast<MINMAXINFO*>(l);
        mm->ptMinTrackSize = {kMinTrackW, kMinTrackH};
        return 0;
    }
    case WM_MEASUREITEM: {
        // tmHeight + tmExternalLeading + 3 of DGROUP:0x90fe, at least 14 [BYTES seg6:0x2016
        // seg6:0x2021 seg6:0x2034].
        auto* m = reinterpret_cast<MEASUREITEMSTRUCT*>(l);
        int height = 14;
        if (HDC dc = GetDC(g_host.main)) {
            HGDIOBJ old = g_host.font ? SelectObject(dc, g_host.font) : nullptr;
            TEXTMETRICW tm{};
            GetTextMetricsW(dc, &tm);
            if (old) SelectObject(dc, old);
            ReleaseDC(g_host.main, dc);
            height = tm.tmHeight + tm.tmExternalLeading + 3;
            if (height < 14) height = 14;
        }
        m->itemHeight = static_cast<UINT>(height);
        return TRUE;
    }
    case WM_DRAWITEM:
        draw_item(reinterpret_cast<const DRAWITEMSTRUCT*>(l));
        return TRUE;
    case WM_COMMAND: {
        const int id = LOWORD(w), code = HIWORD(w);
        if (id == kHelpId) {  // seg2:0x190a(5, 0) [BYTES seg6:0x2305]
            if (g_host.help) g_host.help();
        } else if (id == kEnterId) {  // seg6:0x301a [BYTES seg6:0x2384]
            if (enter() == 0) expand_or_collapse();
        } else if (id == kDirListId) {
            if (code == LBN_SELCHANGE) {  // DGROUP:0x94b4 = LB_GETCURSEL, seg6:0x5318
                const int sel = cursel();
                if (sel != g_sel) {
                    g_sel = sel;
                    select_current();
                }
            } else if (code == LBN_DBLCLK) {
                expand_or_collapse();
            }
        }
        return 0;
    }
    case WM_SYSCOMMAND:  // SC_CLOSE goes to the main window [BYTES seg6:0x23a5 seg6:0x23b8]
        if ((w & 0xfff0) == SC_CLOSE) {
            PostMessageW(g_host.main, WM_SYSCOMMAND, SC_CLOSE, 0);
            return 0;
        }
        break;
    case WM_PAINT: {  // InvalidateRect(h, NULL, TRUE), BeginPaint, seg6:0x5b4c(dc, 1) [BYTES seg6:0x22cf]
        InvalidateRect(h, nullptr, TRUE);
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        if (dc) draw_status(dc, true);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_NCDESTROY:  // a window destroyed with its owner [DESIGN]
        if (g_nav == h) {
            g_nav = nullptr;
            free_gdi();
        }
        break;
    default: break;
    }
    return DefWindowProcW(h, msg, w, l);
}

LRESULT CALLBACK nav_proc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    // No exception may unwind through user32's frames [DESIGN].
    try {
        return handle(h, msg, w, l);
    } catch (const std::exception& x) {
        log(std::string("navigation: ") + x.what());
    } catch (...) {
    }
    return DefWindowProcW(h, msg, w, l);
}

}  // namespace

std::vector<std::string> unset_inputs(const Host& h) {
    std::vector<std::string> v;
    if (!h.lang) v.push_back("lang");
    if (!h.edition) v.push_back("edition");
    if (!h.player) v.push_back("player");
    if (!h.store) v.push_back("store");
    if (!h.command_line) v.push_back("command_line");
    if (!h.demo_type) v.push_back("demo_type");
    if (!h.wave) v.push_back("wave");
    if (!h.restored) v.push_back("restored");
    if (!h.entered) v.push_back("entered");
    return v;
}

bool install(const Host& host) {
    g_host = host;
    // An input the game leaves unset falls back to a default that is wrong for some copies
    // (edition 0 locks the built-in levels of a registered copy) (M3 s1 fixer).
    for (const std::string& n : unset_inputs(host))
        if (host.log) host.log("IS_NAV: Host::" + n + " is not set");
    UnregisterClassW(kClass, host.instance);
    WNDCLASSW wc{};
    wc.style = 0xb;  // CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS, as start-up step 12
    wc.lpfnWndProc = nav_proc;
    wc.hInstance = host.instance;
    wc.hIcon = host.icon;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(LTGRAY_BRUSH));
    wc.lpszClassName = kClass;
    return RegisterClassW(&wc) != 0;
}

void attach(HWND main, HFONT font) {
    g_host.main = main;
    g_host.font = font;
}

void set_parts(const Parts& parts) { g_parts = parts; }

HWND window() { return g_nav; }

void open(int arg, int drive_offset) {  // seg6:0x1c72
    // DGROUP:0xd24 = 1 (read by M3's paths).
    if (g_nav && IsWindow(g_nav)) {
        ShowWindow(g_nav, SW_SHOW);
        return;
    }
    g_arg = arg;
    g_sel = g_accepted;  // DGROUP:0x94b4 = DGROUP:0xd1c
    g_drive = drive_offset;
    const int x = ini_int("NavLeft", kDefaultLeft), y = ini_int("NavTop", kDefaultTop);
    const int cw = ini_int("NavRight", kDefaultRight), ch = ini_int("NavBottom", kDefaultBottom);
    g_rect = {x, y, x + cw, y + ch};
    CreateWindowExW(0, kClass, wide(lang(kTitleString)).c_str(), kStyle, x, y, cw, ch, g_host.main,
                    nullptr, g_host.instance, nullptr);
    log("navigation window " + std::string(g_nav ? "open" : "failed") + " at " + std::to_string(x) +
        "," + std::to_string(y) + " " + std::to_string(cw) + "x" + std::to_string(ch));
    // The play area goes black unless the state is 2 [BYTES seg6:0x1d45 seg6:0x1d4a seg6:0x1d51].
    if (!(g_host.state && g_host.state() == 2) && g_host.black_play_area) g_host.black_play_area();
    // seg6:0x1e68(1), the demo pilot, for PlayMode != 0 in a demo [BYTES seg6:0x1d6c seg6:0x1d82].
    if (g_host.play_mode && g_host.player && g_host.play_mode() != 0 && g_host.player() == 0) {
        if (g_parts.demo_pilot) g_parts.demo_pilot(1);
        else todo("Navigation demo pilot", "seg6:0x1e68");
    }
}

void close() {  // seg6:0x2ebc
    if (!g_nav) return;
    // DGROUP:0x1d40 = now + 1000 [BYTES seg6:0x2ede]: no instruction reads that dword
    // (shell.md "DEMO and PLAY"), so the port does not keep it.
    if (g_host.ini) {
        try {
            for (const auto& kv : close_writes(g_rect.left, g_rect.top, g_rect.right, g_rect.bottom))
                g_host.ini->set_int(settings::kGameSection, kv.first, kv.second);
            if (g_host.ini_changed) g_host.ini_changed();
        } catch (const std::exception& x) {
            log(std::string("Nav keys: ") + x.what());
        }
    }
    const bool torn = g_host.torn_down && g_host.torn_down();
    // Outside the teardown, seg10:0x7db8 (ordinal 7 command 2) purges the deferred sound slots
    // before the main window is enabled [BYTES seg6:0x2fca seg6:0x2fd1 seg6:0x2fdc] (M7 s3
    // fixer).
    if (!torn && g_host.reset_sounds) g_host.reset_sounds();
    if (!torn && g_host.main) EnableWindow(g_host.main, TRUE);
    HWND h = g_nav;
    g_nav = nullptr;  // [BYTES seg6:0x2ff1]
    if (IsWindow(h)) DestroyWindow(h);
    free_gdi();  // [BYTES seg6:0x2fa0 seg6:0x2fbf]
    if (!torn && g_host.main) {
        ShowWindow(g_host.main, SW_SHOW);
        SetActiveWindow(g_host.main);
    }
}

int enter() {  // seg6:0x301a
    if (g_arg == 0) return 0;  // DGROUP:0xd20 [BYTES seg6:0x302e]
    if (g_host.state && g_host.state() == 1) return 0;  // DGROUP:0x90f4 [BYTES seg6:0x3035]
    if (g_parts.enter) return g_parts.enter();
    if (!g_live.walker) return 0;
    // Step 1: DGROUP:0x38c8 = DGROUP:0x6990 [BYTES seg6:0x3044]. The built-in rows are Parts'
    // (steps 2 to 6); without them the port refuses them.
    const std::string label = g_dir;
    if (classify(g_dir, g_strings) != RowKind::kDirectory) {
        todo("Navigation Enter on a built-in level (Demon's Gate, races, duels)", "seg6:0x3049");
        return 0;
    }
    // Step 7: seg6:0x4190(DGROUP:0x38c8) and seg6:0x3f4c(DGROUP:0x6990) < 2 [BYTES seg6:0x3666
    // seg6:0x366e seg6:0x3675 seg6:0x367e].
    if (!g_live.walker->has_exe(label, "seg6:0x301a via seg6:0x4190")) return 0;  // (M3 s1 fixer)
    if (state_of(g_dir) >= kStateComplete) return 0;
    return accept(label);
}

int accept(const std::string& label) {  // seg6:0x3686
    const EnterAction a = enter_action(true, g_host.wave ? g_host.wave() : 0,
                                       g_host.restored ? g_host.restored() : 0);
    bool added = false;
    const int r = recs().enter(g_dir, &added);  // seg6:0x36b6
    if (r < 0) {
        // The 99th record would end past the 64 KB block [BYTES seg6:0x3775]: the port refuses
        // Enter, so the wave's +0x1d0 and counters never land on another directory's record
        // (M3 s1 fixer) [OPEN: ADR 0012 line owed].
        log("navigation Enter: refused, no room for a directory record of " + g_dir + " [OPEN]");
        return 0;
    }
    log("navigation Enter: record " + std::to_string(r) + (added ? " added for " : " found for ") + g_dir);
    ++g_last_accepted.serial;  // (M3 s2 fixer)
    g_last_accepted.record = r;
    g_last_accepted.added = added;
    g_last_accepted.dir = g_dir;
    g_accepted = g_sel;  // DGROUP:0xd1c = DGROUP:0x94b4
    if (g_host.entered) g_host.entered(label);
    if (a == EnterAction::kRequestWave) {
        log("navigation Enter: state request 1");
        if (g_host.request_state) g_host.request_state(1);
    } else if (g_host.arm_debriefing) {
        g_host.arm_debriefing();
    }
    close();
    return 1;
}

const Accepted& last_accepted() { return g_last_accepted; }

bool record_room() {
    return recs().find(g_dir) >= 0 || static_cast<int>(recs().list.size()) < Records::kMax;
}

int selected() { return g_sel; }
int row_count() { return g_live.tree ? g_live.tree->count() : 0; }
std::string& directory() { return g_dir; }
int drive_offset() { return g_drive; }
bool armed() { return g_arg != 0; }
Tree* tree() { return g_nav ? g_live.tree.get() : nullptr; }
Walker* walker() { return g_nav ? g_live.walker.get() : nullptr; }
Records& records() { return recs(); }
Strings& strings() { return g_strings; }
Game game() { return g_live.game; }
const Selected& selection() { return g_selected; }

int state_of(const std::string& path) { return state_digit(path, g_live.game, recs(), g_strings); }

void select_row(int index) {
    if (HWND l = dir_list()) SendMessageW(l, LB_SETCURSEL, static_cast<WPARAM>(index), 0);
    g_sel = index;
    select_current();
}

void expand_or_collapse() {
    HWND l = dir_list();
    if (!l || !g_live.tree) return;
    const int sel = cursel();
    if (sel < 0) return;
    const int sub = g_live.tree->sub_digit(sel);  // seg6:0x5260
    if (sub != kSubCollapsed && sub != kSubExpanded) return;
    RedrawOff off(l);  // (M3 s1 fixer)
    if (sub == kSubCollapsed) g_live.tree->expand(sel, g_dir);  // seg6:0x4f46 on DGROUP:0x6990
    else g_live.tree->collapse(sel);                             // seg6:0x511c
    replay();
    SendMessageW(l, LB_SETCURSEL, static_cast<WPARAM>(sel), 0);  // [BYTES seg6:0x4f46 seg6:0x511c]
}

}  // namespace is::nav
#endif
