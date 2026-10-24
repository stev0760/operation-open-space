// isview: a developer viewer for the original game's files, read through the port's asset
// store (src/assets/, ADR 0002) and format readers (src/formats/).
//
// It keeps v0's parts: the menu bar and accelerators are ENG_LANG.DLL's MENU 101 and MAINACCEL
// converted to Win32, and the commands they send show in the status line; the dialogs open
// live from their converted 16-bit templates; the overview draws IGRAPHIC.DLL's sprites with
// the Transformer animated. The tree on the left lists the identification of the directory
// against the table of known files, the ships of MASTER.SHP and MASTER.BOD, and every NE
// module of the directory by resource type and id. The right side shows the selection:
// strings with a search box, dialogs, menus (as a popup), accelerators, bitmaps and sprites
// (zoom, palette, frame grid), icons, WAVs (played from memory), version information, ships
// and hex or text for the rest.
//
//   isview.exe [game directory] [--dialog N] [--select PATH] [--dump MODULE [--out FILE]]
//
// - Without a directory, isview looks in its own directory (ADR 0002, decision 1). A named
//   directory is used as given, whatever it holds, so THEWORKS or a distribution can be
//   inspected (a viewer choice, below).
// - --dialog N opens the language module's DIALOG N at start-up (for screenshots).
// - --select PATH selects a tree node at start-up: overview, identify, ships[/N], bodies[/N]
//   or MODULE[/TYPE[/ID]], such as IGRAPHIC.DLL/4000/60 or ENG_LANG.DLL/MENU/101.
// - --dump MODULE writes a JSON summary of an NE module (resource types, ids, sizes, counts,
//   string count) to standard output, or to FILE with --out, and exits; MODULE * writes every
//   module's. It mirrors work/assets/<module>/index.json, which `isre res` writes; the JSON
//   is built in dump_json.h, which tests/test_isview_dump.cpp compares with index.json.
//   Whenever --dump or --out is on the command line, messages go to standard error and the
//   exit code tells the result: 0 written, 1 failed, 2 usage.
//
// Viewer choices [DESIGN], the viewer's own and not the port's:
// - A named directory is opened as given. ADR 0002 decision 1 has the port try its own
//   directory first and the named one second; the viewer inspects what it is pointed at.
// - Dialogs are created with DS_NOFAILCREATE, so a control whose class is missing does not
//   stop the rest from showing.
// - A sprite whose width is a whole multiple of its height opens with that many frames in
//   its grid; single pictures with such widths get a grid too, and the Frames field corrects
//   it. How the game slices frames is [OPEN] (docs/re/resources.md).
// - Palette index 15 of the 4 bpp sprites is drawn transparent, as v0 did. The bytes are
//   consistent with that; how the game masks sprites is [OPEN] (docs/re/resources.md).
//
// Keys in the tree: Right opens a node, Left closes it or goes to its parent, Enter opens a
// node or acts on the selection (opens the dialog, shows the menu, plays the WAV). Ctrl+C in
// the report copies the selected rows.
//
// The program is one translation unit, because tools/CMakeLists.txt lists only this file; its
// parts are the .inc files below, in dependency order. The Win32-free parts are the headers
// text.h, dump_json.h and wave_columns.h, which tests/ include as well.
#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "assets/asset_store.h"
#include "assets/game_files.h"
#include "assets/known_files.h"
#include "formats/bytes.h"
#include "formats/dib.h"
#include "formats/icon.h"
#include "formats/ne.h"
#include "formats/records.h"
#include "formats/res16.h"
#include "formats/ships.h"
#include "formats/version.h"
#include "formats/wav.h"

#include "dump_json.h"
#include "text.h"
#include "wave_columns.h"

#include "common.inc"
#include "catalog.inc"
#include "dump.inc"
#include "views.inc"
#include "lists.inc"
#include "canvas.inc"

namespace {

// ---- status and layout

void set_status(const std::wstring& s) {
    g.status = s;
    InvalidateRect(g.main, nullptr, FALSE);
}

int header_height() { return 8 + 3 * (g.line_h + 2) + 6; }

void place_label(int& x, int y, int h, const wchar_t* text) {
    HDC dc = GetDC(g.main);
    const HGDIOBJ old = SelectObject(dc, g.font);
    const int w = text_width(dc, text);
    SelectObject(dc, old);
    ReleaseDC(g.main, dc);
    g.labels.push_back({x, y + (h - g.line_h) / 2 + 2, text});
    x += w + 6;
}

void layout() {
    RECT rc;
    GetClientRect(g.main, &rc);
    const int x0 = kTreeWidth, pane_w = std::max(0L, rc.right - x0), height = rc.bottom;
    MoveWindow(g.tree, 0, 0, kTreeWidth, height, TRUE);

    int y = header_height();
    const int ch = g.line_h + 10;  // control height
    int x = x0 + 12;
    bool bar = false;
    g.labels.clear();
    for (HWND h : {g.search, g.button, g.button2, g.zoom, g.frames}) ShowWindow(h, SW_HIDE);
    auto place = [&](HWND h, int w, const wchar_t* label, int drop = 0) {
        if (label) place_label(x, y, ch, label);
        MoveWindow(h, x, y, w, ch + drop, TRUE);
        ShowWindow(h, SW_SHOW);
        x += w + 16;
        bar = true;
    };
    switch (g.view.kind) {
    case ViewKind::kStrings:
        place(g.search, 320, L"Search text or id (#id: exact id):");
        break;
    case ViewKind::kDialog:
        SetWindowTextW(g.button, L"Open dialog");
        place(g.button, 150, nullptr);
        break;
    case ViewKind::kMenu:
        SetWindowTextW(g.button, L"Show as popup");
        place(g.button, 150, nullptr);
        break;
    case ViewKind::kWave:
        SetWindowTextW(g.button, L"Play");
        place(g.button, 100, nullptr);
        place(g.button2, 100, nullptr);
        break;
    default:
        if (uses_picture(g.view.kind) && g.view.img) {
            place(g.zoom, 70, L"Zoom", 220);
            place(g.frames, 60, L"Frames");
        }
        break;
    }
    if (bar) y += ch + 8;

    const bool rep = uses_report(g.view.kind), can = uses_canvas(g.view.kind);
    g.colhdr_y = -1;
    if (rep && !g.view.columns.empty() && !(g.view.columns.size() == 1 && g.view.columns[0].title.empty())) {
        g.colhdr_y = y;
        y += g.line_h + 6;
    }
    const int content_h = std::max(0, height - y);
    const int report_w = rep && can ? pane_w * 11 / 20 : pane_w;
    g.report_x = x0;
    g.report_w = report_w;
    MoveWindow(g.report, x0, y, report_w, content_h, TRUE);
    MoveWindow(g.canvas, rep ? x0 + report_w : x0, y, rep ? pane_w - report_w : pane_w, content_h, TRUE);
    ShowWindow(g.report, rep ? SW_SHOW : SW_HIDE);
    ShowWindow(g.canvas, can ? SW_SHOW : SW_HIDE);
    InvalidateRect(g.main, nullptr, FALSE);
}

void paint_main(HDC dc) {
    RECT rc;
    GetClientRect(g.main, &rc);
    RECT pane{kTreeWidth, 0, rc.right, rc.bottom};
    FillRect(dc, &pane, g.bg_brush);
    SetBkMode(dc, TRANSPARENT);
    const int x = kTreeWidth + 12;
    int y = 8;
    const HGDIOBJ old = SelectObject(dc, g.bold);
    SetTextColor(dc, kText);
    TextOutW(dc, x, y, g.view.title.c_str(), static_cast<int>(g.view.title.size()));
    y += g.line_h + 2;
    SelectObject(dc, g.font);
    SetTextColor(dc, kDim);
    TextOutW(dc, x, y, g.view.subtitle.c_str(), static_cast<int>(g.view.subtitle.size()));
    y += g.line_h + 2;
    SetTextColor(dc, kAccent);
    TextOutW(dc, x, y, g.status.c_str(), static_cast<int>(g.status.size()));
    SetTextColor(dc, kDim);
    for (const Label& l : g.labels) TextOutW(dc, l.x, l.y, l.text.c_str(), static_cast<int>(l.text.size()));
    if (g.colhdr_y >= 0) {
        const int scroll = GetScrollPos(g.report, SB_HORZ);
        const int save = SaveDC(dc);
        IntersectClipRect(dc, g.report_x, g.colhdr_y, g.report_x + g.report_w, g.colhdr_y + g.line_h + 6);
        SelectObject(dc, g.bold);
        for (const Column& c : g.view.columns)
            TextOutW(dc, g.report_x + 2 + col_x(c) - scroll, g.colhdr_y + 2, c.title.c_str(),
                     static_cast<int>(c.title.size()));
        RestoreDC(dc, save);
    }
    SelectObject(dc, old);
}

// ---- views

void filter_strings() {
    wchar_t buf[256] = L"";
    GetWindowTextW(g.search, buf, 256);
    const std::wstring q = lower(buf);
    const bool exact = q.size() > 1 && q[0] == L'#' &&
                       std::all_of(q.begin() + 1, q.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; });
    std::vector<Line> out;
    for (const Line& l : g.view.lines) {
        if (q.empty()) out.push_back(l);
        else if (exact) {
            if (l.text.compare(0, l.text.find(L'\t'), q, 1, std::wstring::npos) == 0) out.push_back(l);
        } else if (lower(l.text).find(q) != std::wstring::npos) {
            out.push_back(l);
        }
    }
    set_lines(out);
    g.view.subtitle = num(static_cast<long long>(out.size())) + L" of " +
                      num(static_cast<long long>(g.view.lines.size())) + L" strings" +
                      (q.empty() ? L"" : L" match \u201C" + std::wstring(buf) + L"\u201D");
    InvalidateRect(g.main, nullptr, FALSE);
}

void apply_view() {
    View& v = g.view;
    g.filling = true;
    SendMessageW(g.report, WM_SETFONT, reinterpret_cast<WPARAM>(v.mono ? g.mono : g.font), FALSE);
    if (v.kind == ViewKind::kStrings) {
        SetWindowTextW(g.search, L"");
        filter_strings();
    } else {
        set_lines(v.lines);
    }
    SendMessageW(g.zoom, CB_SETCURSEL, static_cast<WPARAM>(v.zoom - 1), 0);
    SetWindowTextW(g.frames, num(v.frames).c_str());
    g.filling = false;
    g.scroll_x = g.scroll_y = 0;
    g.ext_w = g.ext_h = 0;
    set_scroll();
    layout();
    InvalidateRect(g.canvas, nullptr, FALSE);
}

void show_node(int n) {
    if (n < 0 || n >= static_cast<int>(g.nodes.size())) return;
    const Node& nd = g.nodes[n];
    View v;
    v.node = n;
    try {
        switch (nd.kind) {
        case NodeKind::kOverview: view_overview(v); break;
        case NodeKind::kIdentify: view_identify(v); break;
        case NodeKind::kShips: view_ships(v); break;
        case NodeKind::kShip: view_ship(v, nd.index); break;
        case NodeKind::kBodies: view_bodies(v); break;
        case NodeKind::kBody: view_body(v, nd.index); break;
        case NodeKind::kModule: view_module(v, g.modules[nd.module], n); break;
        case NodeKind::kType: view_type(v, n); break;
        case NodeKind::kResource: view_resource(v, g.modules[nd.module], *nd.res); break;
        }
    } catch (const std::exception& x) {
        View e;
        e.node = n;
        e.kind = ViewKind::kMessage;
        e.title = v.title.empty() ? nd.label : v.title;
        e.text = {L"Cannot show this: " + message(x.what())};
        v = std::move(e);
    }
    g.view = std::move(v);
    apply_view();
}

void select_node(int n) {
    if (n < 0 || n >= static_cast<int>(g.nodes.size())) return;
    for (int p = g.nodes[n].parent; p >= 0; p = g.nodes[p].parent) g.nodes[p].expanded = true;
    refill_tree(n);
    show_node(n);
}

void toggle(int n) {
    if (n < 0 || !g.nodes[n].has_children) return;
    g.nodes[n].expanded = !g.nodes[n].expanded;
    const int cur = current_node();
    if (cur >= 0 && !shown(cur)) {
        refill_tree(n);
        show_node(n);
    } else {
        refill_tree(cur);
    }
}

// ---- actions

INT_PTR CALLBACK dialog_proc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_INITDIALOG:
        return TRUE;
    case WM_COMMAND:
        // any push button, OK or Cancel (Esc) closes the dialog
        if (HIWORD(w) == BN_CLICKED) {
            bool close = LOWORD(w) == IDOK || LOWORD(w) == IDCANCEL;
            if (!close && l) {
                wchar_t cls[16] = L"";
                GetClassNameW(reinterpret_cast<HWND>(l), cls, 16);
                const auto type = GetWindowLongW(reinterpret_cast<HWND>(l), GWL_STYLE) & BS_TYPEMASK;
                close = lstrcmpiW(cls, L"Button") == 0 && (type == BS_PUSHBUTTON || type == BS_DEFPUSHBUTTON);
            }
            if (close) DestroyWindow(h);
        }
        return TRUE;
    case WM_CLOSE:
        DestroyWindow(h);
        return TRUE;
    case WM_DESTROY:
        if (h == g.modeless) g.modeless = nullptr;
        return FALSE;
    }
    return FALSE;
}

void open_dialog() {
    if (g.view.dialog_tmpl.empty()) return;
    if (g.modeless) DestroyWindow(g.modeless);
    HWND h = CreateDialogIndirectParamW(g.inst, reinterpret_cast<const DLGTEMPLATE*>(g.view.dialog_tmpl.data()),
                                        g.main, dialog_proc, 0);
    if (!h) {
        set_status(L"CreateDialogIndirectParamW failed, error " + num(GetLastError()));
        return;
    }
    g.modeless = h;
    ShowWindow(h, SW_SHOW);
    set_status(L"Opened " + g.view.title + L" modeless; a push button, Esc or Close closes it");
}

HMENU build_popup(const std::vector<MenuItem16>& items) {
    HMENU m = CreatePopupMenu();
    for (const MenuItem16& it : items) {
        const UINT keep = it.flags & (MF_GRAYED | MF_DISABLED | MF_CHECKED | MF_MENUBARBREAK | MF_MENUBREAK);
        const std::wstring text = widen(it.text);
        if (it.flags & kMfPopup)
            AppendMenuW(m, MF_POPUP | MF_STRING | keep, reinterpret_cast<UINT_PTR>(build_popup(it.children)),
                        text.c_str());
        else if (is_separator(it))
            AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        else
            AppendMenuW(m, MF_STRING | keep, it.id, text.c_str());
    }
    return m;
}

void show_popup() {
    const auto& items = g.view.menu;
    if (items.empty()) return;
    // a menu with one popup (a context menu) shows that popup; a menu bar shows its popups
    const bool single = items.size() == 1 && (items[0].flags & kMfPopup);
    HMENU m = build_popup(single ? items[0].children : items);
    RECT r;
    GetWindowRect(g.button, &r);
    SetForegroundWindow(g.main);
    const int cmd = static_cast<int>(TrackPopupMenu(m, TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN | TPM_TOPALIGN,
                                                    r.left, r.bottom, 0, g.main, nullptr));
    DestroyMenu(m);
    set_status(cmd ? L"The popup returned command " + num(cmd) : L"The popup closed without a command");
}

void play_sound() {
    if (!g.view.has_sound) return;
    PlaySoundW(nullptr, nullptr, 0);
    g.playing = g.view.sound.file;
    const BOOL ok = PlaySoundW(reinterpret_cast<LPCWSTR>(g.playing.data()), nullptr,
                               SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
    set_status(ok ? L"Playing " + g.view.title
                  : L"PlaySoundW failed for " + g.view.title + L" (is there a wave output device?)");
}

void stop_sound() {
    PlaySoundW(nullptr, nullptr, 0);
    set_status(L"Stopped");
}

// Enter or a double-click: act on the shown resource, or open and close the node.
void activate(int n) {
    if (n < 0) return;
    if (n == g.view.node) {
        switch (g.view.kind) {
        case ViewKind::kDialog: open_dialog(); return;
        case ViewKind::kMenu: show_popup(); return;
        case ViewKind::kWave: play_sound(); return;
        default: break;
        }
    }
    toggle(n);
}

// ---- subclassed list boxes

LRESULT CALLBACK tree_subclass(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK: {
        const LRESULT hit = SendMessageW(h, LB_ITEMFROMPOINT, 0, l);
        const int row = LOWORD(hit);
        if (HIWORD(hit) == 0 && row < static_cast<int>(g.visible.size())) {
            const Node& n = g.nodes[g.visible[row]];
            const int gx = n.level * kIndent + kGlyphLeft - 2;
            const int x = static_cast<short>(LOWORD(l));
            if (n.has_children && x >= gx && x < gx + kGlyphWidth) {
                if (msg == WM_LBUTTONDBLCLK) return 0;  // the first click toggled already
                const LRESULT r = CallWindowProcW(g.tree_proc, h, msg, w, l);
                PostMessageW(g.main, kMsgToggle, static_cast<WPARAM>(g.visible[row]), 0);
                return r;
            }
        }
        break;
    }
    case WM_KEYDOWN: {
        const int n = current_node();
        if (n < 0) break;
        if (w == VK_RIGHT) {
            if (g.nodes[n].has_children && !g.nodes[n].expanded) toggle(n);
            else if (g.nodes[n].has_children) select_node(g.visible[static_cast<std::size_t>(node_row(n)) + 1]);
            return 0;
        }
        if (w == VK_LEFT) {
            if (g.nodes[n].expanded) toggle(n);
            else if (g.nodes[n].parent >= 0) select_node(g.nodes[n].parent);
            return 0;
        }
        if (w == VK_RETURN) {
            activate(n);
            return 0;
        }
        break;
    }
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS;
    }
    return CallWindowProcW(g.tree_proc, h, msg, w, l);
}

LRESULT CALLBACK report_subclass(HWND h, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_KEYDOWN && (GetKeyState(VK_CONTROL) & 0x8000)) {
        if (w == 'C') {
            copy_report();
            return 0;
        }
        if (w == 'A') {
            SendMessageW(h, LB_SETSEL, TRUE, -1);
            return 0;
        }
    }
    const LRESULT r = CallWindowProcW(g.report_proc, h, msg, w, l);
    if (msg == WM_HSCROLL || msg == WM_KEYDOWN) {
        RECT hdr{g.report_x, g.colhdr_y, g.report_x + g.report_w, g.colhdr_y + g.line_h + 6};
        if (g.colhdr_y >= 0) InvalidateRect(g.main, &hdr, FALSE);
    }
    return r;
}

// ---- the main window

HWND child(const wchar_t* cls, DWORD style, DWORD ex, int id, const wchar_t* text = nullptr) {
    HWND h = CreateWindowExW(ex, cls, text, WS_CHILD | style, 0, 0, 10, 10, g.main,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), g.inst, nullptr);
    SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(g.font), FALSE);
    return h;
}

void create_children() {
    g.tree = child(L"LISTBOX", WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT |
                                   LBS_OWNERDRAWFIXED,
                   0, kIdTree);
    g.report = child(L"LISTBOX", WS_VSCROLL | WS_HSCROLL | WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT |
                                     LBS_OWNERDRAWFIXED | LBS_EXTENDEDSEL,
                     0, kIdReport);
    g.canvas = child(L"ISViewCanvas", WS_VSCROLL | WS_HSCROLL | WS_TABSTOP, 0, kIdCanvas);
    g.search = child(L"EDIT", WS_TABSTOP | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, kIdSearch);
    g.button = child(L"BUTTON", WS_TABSTOP | BS_PUSHBUTTON, 0, kIdButton, L"Open");
    g.button2 = child(L"BUTTON", WS_TABSTOP | BS_PUSHBUTTON, 0, kIdButton2, L"Stop");
    g.zoom = child(L"COMBOBOX", WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, 0, kIdZoom);
    g.frames = child(L"EDIT", WS_TABSTOP | ES_NUMBER, WS_EX_CLIENTEDGE, kIdFrames);
    for (int z = 1; z <= kMaxZoom; ++z)
        SendMessageW(g.zoom, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>((L"\u00D7" + num(z)).c_str()));
    SendMessageW(g.frames, EM_SETLIMITTEXT, 2, 0);
    g.tree_proc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(g.tree, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(tree_subclass)));
    g.report_proc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(g.report, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(report_subclass)));
}

void on_command(WPARAM w, LPARAM l) {
    const HWND from = reinterpret_cast<HWND>(l);
    const int code = HIWORD(w);
    if (!from) {
        // v0: the game's menu bar and accelerators
        wchar_t s[120];
        std::swprintf(s, 120, L"Last %ls command: %u (0x%x)", code ? L"accelerator" : L"menu",
                      static_cast<unsigned>(LOWORD(w)), static_cast<unsigned>(LOWORD(w)));
        set_status(s);
        return;
    }
    if (from == g.tree) {
        if (code == LBN_SELCHANGE) show_node(current_node());
        else if (code == LBN_DBLCLK) activate(current_node());
    } else if (from == g.report && code == LBN_DBLCLK) {
        const LRESULT row = SendMessageW(g.report, LB_GETCARETINDEX, 0, 0);
        if (row >= 0 && row < static_cast<LRESULT>(g.lines.size()) && g.lines[row].node >= 0)
            select_node(g.lines[row].node);
    } else if (from == g.search && code == EN_CHANGE && !g.filling) {
        filter_strings();
    } else if (from == g.button && code == BN_CLICKED) {
        activate(g.view.node);
    } else if (from == g.button2 && code == BN_CLICKED) {
        stop_sound();
    } else if (from == g.zoom && code == CBN_SELCHANGE) {
        g.view.zoom = static_cast<int>(SendMessageW(g.zoom, CB_GETCURSEL, 0, 0)) + 1;
        InvalidateRect(g.canvas, nullptr, FALSE);
    } else if (from == g.frames && code == EN_CHANGE && !g.filling) {
        wchar_t buf[8] = L"";
        GetWindowTextW(g.frames, buf, 8);
        const long n = std::wcstol(buf, nullptr, 10);
        if (n >= 1) {
            g.view.frames = static_cast<int>(std::min<long>(n, kMaxFrames));
            InvalidateRect(g.canvas, nullptr, FALSE);
        }
    }
}

LRESULT CALLBACK wnd_proc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
    case WM_CREATE:
        g.main = h;
        create_children();
        SetTimer(h, kAnimTimer, kAnimMs, nullptr);
        return 0;
    case WM_SIZE:
        layout();
        return 0;
    case WM_SETFOCUS:
        SetFocus(g.tree);
        return 0;
    case WM_MEASUREITEM: {
        auto* mi = reinterpret_cast<MEASUREITEMSTRUCT*>(l);
        mi->itemHeight = static_cast<UINT>(g.line_h);
        return TRUE;
    }
    case WM_DRAWITEM: {
        const auto* d = reinterpret_cast<const DRAWITEMSTRUCT*>(l);
        if (d->CtlID == kIdTree) draw_tree_item(*d);
        else if (d->CtlID == kIdReport) draw_report_item(*d);
        return TRUE;
    }
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLOREDIT: {
        const HWND c = reinterpret_cast<HWND>(l);
        if (c == g.tree || c == g.report || c == g.search || c == g.frames) {
            const HDC dc = reinterpret_cast<HDC>(w);
            SetTextColor(dc, kText);
            SetBkColor(dc, kPanel);
            return reinterpret_cast<LRESULT>(g.panel_brush);
        }
        break;
    }
    case WM_COMMAND:
        on_command(w, l);
        return 0;
    case kMsgToggle:
        toggle(static_cast<int>(w));
        return 0;
    case WM_TIMER:
        g.frame = (g.frame + 1) % 720720;  // a multiple of every frame count up to 16
        if (g.view.kind == ViewKind::kOverview || (uses_picture(g.view.kind) && g.view.frames > 1))
            InvalidateRect(g.canvas, nullptr, FALSE);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        paint_main(dc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(h, kAnimTimer);
        PlaySoundW(nullptr, nullptr, 0);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, w, l);
}

fs::path exe_dir() {
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, &buf[0], static_cast<DWORD>(buf.size()));
        if (n == 0) return {};
        if (n < buf.size()) {
            buf.resize(n);
            return fs::path(buf).parent_path();
        }
        buf.resize(buf.size() * 2);
    }
}

const wchar_t kUsage[] =
    L"Usage: isview [game directory] [--dialog N] [--select PATH] [--dump MODULE [--out FILE]]\n\n"
    L"Without a directory, isview looks for the game in its own directory.\n"
    L"--select PATH: overview, identify, ships[/N], bodies[/N] or MODULE[/TYPE[/ID]].\n"
    L"--dump MODULE: a JSON summary of an NE module (* for every module), then exit.";

void create_fonts() {
    NONCLIENTMETRICSW ncm{};
    ncm.cbSize = sizeof ncm;
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0);
    g.font = CreateFontIndirectW(&ncm.lfMessageFont);
    LOGFONTW lf = ncm.lfMessageFont;
    lf.lfWeight = FW_BOLD;
    g.bold = CreateFontIndirectW(&lf);
    lf = ncm.lfMessageFont;
    lf.lfHeight = lf.lfHeight * 4 / 5;
    g.small = CreateFontIndirectW(&lf);
    lf = ncm.lfMessageFont;
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    std::wcscpy(lf.lfFaceName, L"Courier New");
    g.mono = CreateFontIndirectW(&lf);

    HDC dc = GetDC(nullptr);
    TEXTMETRICW tm{};
    HGDIOBJ old = SelectObject(dc, g.font);
    GetTextMetricsW(dc, &tm);
    g.line_h = tm.tmHeight + 4;
    // the column unit: the mean width of the upper-case letters and digits, which file names,
    // ids and sizes are made of (the average character width is narrower)
    const std::wstring sample = L"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    g.char_w = (text_width(dc, sample) + static_cast<int>(sample.size()) - 1) / static_cast<int>(sample.size());
    SelectObject(dc, g.small);
    GetTextMetricsW(dc, &tm);
    g.small_h = tm.tmHeight;
    SelectObject(dc, g.mono);
    GetTextMetricsW(dc, &tm);
    g.mono_w = tm.tmAveCharWidth;
    SelectObject(dc, old);
    ReleaseDC(nullptr, dc);

    g.bg_brush = CreateSolidBrush(kBg);
    g.panel_brush = CreateSolidBrush(kPanel);
    g.dim_brush = CreateSolidBrush(kDim);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    g.inst = inst;
    std::wstring dir_arg, select, dump, out;
    long start_dialog = -1;
    bool usage = false;
    // a command-line run: --dump or --out appears, with or without a value
    bool cli = false;
    for (int i = 1; i < __argc; ++i) {
        const std::wstring a = __wargv[i];
        auto value = [&]() -> std::wstring {
            if (i + 1 < __argc) return __wargv[++i];
            usage = true;
            return {};
        };
        if (a == L"--dump" || a == L"--out") cli = true;
        if (a == L"--dialog") start_dialog = std::wcstol(value().c_str(), nullptr, 10);
        else if (a == L"--select") select = value();
        else if (a == L"--dump") dump = value();
        else if (a == L"--out") out = value();
        else if (a.size() > 1 && (a[0] == L'-' || a == L"/?")) usage = true;
        else if (dir_arg.empty()) dir_arg = a;
        else usage = true;
    }
    // --out without --dump has nothing to write
    if (cli && dump.empty()) usage = true;
    auto fail = [&](const std::wstring& msg, int code) {
        if (cli) write_error(msg);
        else MessageBoxW(nullptr, msg.c_str(), L"isview", code == 2 ? MB_ICONINFORMATION : MB_ICONERROR);
        return code;
    };
    if (usage) return fail(kUsage, 2);

    if (!dir_arg.empty()) {
        g.dir = fs::path(dir_arg);
    } else {
        const fs::path here = exe_dir();
        const auto found = isa::discover({here});
        if (!found)
            return fail(L"No game directory: " + here.wstring() +
                            L" does not hold INSPACE.EXE, ISPACE.DLL and IGRAPHIC.DLL.\n\n" + kUsage,
                        2);
        g.dir = *found;
    }
    try {
        g.store = std::make_unique<isa::AssetStore>(g.dir);
    } catch (const std::exception& x) {
        return fail(L"Cannot open " + g.dir.wstring() + L": " + message(x.what()), 1);
    }
    if (cli) return run_dump(dump, out);

    load_game();
    build_tree();
    create_fonts();

    WNDCLASSW cc{};
    cc.lpfnWndProc = canvas_proc;
    cc.hInstance = inst;
    cc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    cc.lpszClassName = L"ISViewCanvas";
    RegisterClassW(&cc);
    WNDCLASSW wc{};
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"ISView";
    RegisterClassW(&wc);
    const std::wstring caption = L"isview \u2014 " + (g.title.empty() ? std::wstring(L"Inner Space") : g.title) +
                                 L" \u2014 " + g.dir.wstring();
    CreateWindowExW(0, wc.lpszClassName, caption.c_str(), WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT,
                    CW_USEDEFAULT, 1260, 920, nullptr, g.menu, inst, nullptr);
    if (!g.main) return fail(L"Cannot create the main window", 1);
    ShowWindow(g.main, show);

    int first = 0;
    if (start_dialog >= 0) {
        const int n = find_node(widen(g.store->language_module()) + L"/DIALOG/" + num(start_dialog));
        if (n >= 0) {
            select_node(n);
            open_dialog();
        } else {
            set_status(L"--dialog: no DIALOG " + num(start_dialog) + L" in " + widen(g.store->language_module()));
        }
        first = -1;
    }
    if (!select.empty()) {
        const int n = find_node(select);
        if (n >= 0) first = n;
        else set_status(L"--select: no node " + select);
    }
    if (first >= 0) select_node(first);
    UpdateWindow(g.main);
    SetFocus(g.tree);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        if (g.modeless && IsDialogMessageW(g.modeless, &m)) continue;
        if (m.message == WM_KEYDOWN && m.hwnd == g.search) {
            // the EDIT control has no select-all key; Esc clears the search
            if (m.wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                SendMessageW(g.search, EM_SETSEL, 0, -1);
                continue;
            }
            if (m.wParam == VK_ESCAPE) {
                SetWindowTextW(g.search, L"");
                continue;
            }
        }
        if (g.accel && TranslateAcceleratorW(g.main, g.accel, &m)) continue;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return 0;
}
