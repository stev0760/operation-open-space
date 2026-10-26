// The Win32 dialogs and driver of the install step. docs/re/install.md "Dialogs"; the logic is
// install.cpp's. Every dialog is built from the set's own INSTALL.EXE DIALOG resource (its
// resources are not packed) through fmt::parse_dialog16 and fmt::dialog_to_win32, the path of
// assets::AssetStore::dialog_template.
#ifdef _WIN32

#include "install/install_win32.h"

#include <commdlg.h>
#include <shellapi.h>

#include <cstdio>
#include <string>
#include <vector>

#include "assets/game_files.h"
#include "formats/bytes.h"
#include "formats/ne.h"
#include "formats/res16.h"

namespace is::install {

namespace {

namespace fs = std::filesystem;

std::wstring wide(const std::string& cp1252) {
    const std::u16string u = fmt::cp1252_to_utf16(cp1252);
    return std::wstring(u.begin(), u.end());
}

std::string narrow(const std::wstring& w) {
    // cp1252 for the Latin-1 range [DESIGN] (ADR 0003: game text is cp1252).
    std::string s;
    for (wchar_t c : w) s.push_back(c < 0x100 ? static_cast<char>(c) : '?');
    return s;
}

std::string window_text(HWND h, int id, int nmax) {
    std::wstring buf(static_cast<std::size_t>(nmax), L'\0');
    const int n = GetDlgItemTextW(h, id, buf.data(), nmax);
    buf.resize(static_cast<std::size_t>(n > 0 ? n : 0));
    return narrow(buf);
}

struct Ctx {
    const Win32Options* o = nullptr;
    const SourceSet* set = nullptr;
    HWND progress = nullptr;
    Components* comp = nullptr;
    int diff = 144, speed = 138;
    bool full = true;
    std::string name, org;
    std::string done_text;
    void log(const std::string& s) const {
        if (o->log) o->log(s);
    }
};

std::vector<std::uint8_t> dialog_template(const SourceSet& set, std::uint16_t id) {
    const fs::path* exe = set.find("INSTALL.EXE");
    if (!exe) throw std::runtime_error("the set has no INSTALL.EXE for its dialogs");
    const fmt::NeModule m = fmt::NeModule::load(*exe);
    const fmt::NeResource* r = m.find(fmt::RT16_DIALOG, id);
    if (!r) throw std::runtime_error("INSTALL.EXE has no DIALOG " + std::to_string(id));
    return fmt::dialog_to_win32(fmt::parse_dialog16(m.data(*r), m.size(*r)), DS_NOFAILCREATE);
}

INT_PTR run_dialog(Ctx& c, std::uint16_t id, DLGPROC proc) {
    const std::vector<std::uint8_t> t = dialog_template(*c.set, id);
    return DialogBoxIndirectParamW(c.o->instance, reinterpret_cast<const DLGTEMPLATE*>(t.data()),
                                   nullptr, proc, reinterpret_cast<LPARAM>(&c));
}

Ctx* ctx_of(HWND h, UINT msg, LPARAM lp) {
    if (msg == WM_INITDIALOG) SetWindowLongPtrW(h, DWLP_USER, lp);
    return reinterpret_cast<Ctx*>(GetWindowLongPtrW(h, DWLP_USER));
}

// INSTALL.EXE seg2:0x4e9a(hDlg, dy): the window centred on the screen, dy pixels lower;
// both halvings truncate toward zero (cdq, sub, sar), as C's division does [BYTES INSTALL.EXE
// seg2:0x4eaf-0x4f0a]. Every dialog procedure calls it at WM_INITDIALOG: dialog 300's with
// dy = 14, the others with 0 [BYTES INSTALL.EXE seg2:0x762 seg2:0x15bf seg2:0x295c seg2:0x3423
// seg2:0x34e0].
void centre(HWND h, int dy) {
    RECT r{};
    GetWindowRect(h, &r);
    const int x = GetSystemMetrics(SM_CXSCREEN) / 2 + (r.left - r.right) / 2;
    const int y = GetSystemMetrics(SM_CYSCREEN) / 2 + (r.top - r.bottom) / 2 + dy;
    MoveWindow(h, x, y, r.right - r.left, r.bottom - r.top, TRUE);
}

// The Win16 dialog manager draws a WS_BORDER edit with a 1-pixel border, a version-4 Win32
// dialog adds WS_EX_CLIENTEDGE; the port takes the client edge off, as Dialog::
// win16_edit_borders does for the game's dialogs (src/dialogs/dialogs.cpp) [DESIGN].
void win16_edit_borders(HWND h) {
    for (HWND c = GetWindow(h, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
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

void pump() {
    MSG m;
    while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
}

// ---- dialog 300, INSTALL.EXE seg2:0x73c

void show_required(HWND h, const Components& c) {
    // seg2:0xb54 [BYTES INSTALL.EXE seg2:0xb6a]
    std::array<bool, 6> on{};
    for (std::size_t i = 0; i < 6; ++i)
        on[i] = IsDlgButtonChecked(h, kBoxIds[i]) == BST_CHECKED;
    (void)c;
    SetDlgItemTextW(h, 132, (std::to_wstring(required_kb(on)) + L" KB").c_str());
}

bool show_available(HWND h) {
    // seg2:0xc44: the free space of the typed drive, or "N/A" [BYTES INSTALL.EXE seg2:0xc44
    // seg2:0xd1d]. The port asks GetDiskFreeSpaceExW for the typed path's root in 64 bits; the
    // DOS call's 2 GB ceiling is not reproduced [DESIGN].
    wchar_t buf[0x6e] = {};
    GetDlgItemTextW(h, 301, buf, 0x6e);
    std::wstring root(buf);
    bool ok = false;
    ULARGE_INTEGER avail{};
    if (root.size() >= 2 && root[1] == L':') {
        root = root.substr(0, 2) + L"\\";
        ok = GetDiskFreeSpaceExW(root.c_str(), &avail, nullptr, nullptr) != 0;
    }
    SetDlgItemTextW(h, 131,
                    ok ? (std::to_wstring(avail.QuadPart / 1024) + L" KB").c_str() : L"N/A");
    // OK (100) is disabled only for N/A or a Required of 0 [BYTES INSTALL.EXE seg2:0xf16].
    EnableWindow(GetDlgItem(h, 100), ok);
    return ok;
}

INT_PTR CALLBACK proc300(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    Ctx* c = ctx_of(h, msg, lp);
    switch (msg) {
    case WM_INITDIALOG: {
        centre(h, 14);  // [BYTES INSTALL.EXE seg2:0x762]
        win16_edit_borders(h);
        Components& k = *c->comp;
        if (!k.caption.empty()) SetWindowTextW(h, wide(k.caption).c_str());
        for (std::size_t i = 0; i < 6; ++i)
            SetDlgItemTextW(h, kSizeIds[i], (std::to_wstring(kSizesKb[i]) + L" KB").c_str());
        SetDlgItemTextW(h, 301, wide(k.dir).c_str());
        for (std::size_t i = 0; i < 6; ++i) {
            CheckDlgButton(h, kBoxIds[i], k.checked[i] ? BST_CHECKED : BST_UNCHECKED);
            if (!k.enabled[i]) EnableWindow(GetDlgItem(h, kBoxIds[i]), FALSE);
        }
        if (k.factory_hidden) {
            ShowWindow(GetDlgItem(h, 124), SW_HIDE);
            ShowWindow(GetDlgItem(h, 128), SW_HIDE);
        }
        show_required(h, k);
        show_available(h);
        SendDlgItemMessageW(h, 301, EM_SETSEL, 0, 0x7fff);
        SetFocus(GetDlgItem(h, 301));
        c->log("install: sound 0x4d of dialog 300 is not played (INSTALL.EXE seg2:0x4134) [OPEN]");
        return FALSE;
    }
    case WM_COMMAND: {
        const int id = LOWORD(wp);
        if (id == 301 && HIWORD(wp) == EN_CHANGE) {
            show_available(h);  // 0x7ea [BYTES INSTALL.EXE seg2:0xaa9]
            return TRUE;
        }
        for (int b : kBoxIds)
            if (id == b) {
                show_required(h, *c->comp);
                return TRUE;
            }
        if (id == 100 || id == IDOK) {
            // seg2:0xdd6 [BYTES INSTALL.EXE seg2:0xe09 seg2:0xe2a seg2:0xe5c]
            if (!IsWindowEnabled(GetDlgItem(h, 100))) return TRUE;
            const std::string dir = complete_dir(window_text(h, 301, 0x6e));
            if (!dir_valid(dir_path(dir))) {
                MessageBoxW(h, wide(kMsgBadPath).c_str(), wide(kTitle).c_str(), kMbStop);
                return TRUE;
            }
            c->comp->dir = dir;
            for (std::size_t i = 0; i < 6; ++i)
                c->comp->checked[i] = IsDlgButtonChecked(h, kBoxIds[i]) == BST_CHECKED;
            EndDialog(h, 1);
            return TRUE;
        }
        if (id == IDCANCEL) {
            EndDialog(h, 0);
            return TRUE;
        }
        break;
    }
    default:
        break;
    }
    return FALSE;
}

// ---- dialog 1922, INSTALL.EXE seg2:0x34a6; dialog 1774, seg2:0x33fa

INT_PTR CALLBACK proc1922(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    Ctx* c = ctx_of(h, msg, lp);
    if (msg == WM_INITDIALOG) {
        centre(h, 0);  // [BYTES INSTALL.EXE seg2:0x34e0]
        CheckRadioButton(h, 142, 146, 144);  // [BYTES INSTALL.EXE seg2:0x34b9 seg2:0x3503]
        CheckRadioButton(h, 137, 140, 138);
        SetFocus(GetDlgItem(h, IDOK));
        return FALSE;
    }
    if (msg == WM_COMMAND && LOWORD(wp) == IDOK) {
        c->diff = 146;
        for (int i = 142; i <= 145; ++i)
            if (IsDlgButtonChecked(h, i) == BST_CHECKED) c->diff = i;
        c->speed = 140;
        for (int i = 137; i <= 139; ++i)
            if (IsDlgButtonChecked(h, i) == BST_CHECKED) c->speed = i;
        EndDialog(h, 1);
        return TRUE;
    }
    return FALSE;  // IDCANCEL falls through: the dialog cannot be cancelled
}

INT_PTR CALLBACK proc1774(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    Ctx* c = ctx_of(h, msg, lp);
    if (msg == WM_INITDIALOG) {
        CheckRadioButton(h, 1775, 1776, 1775);
        centre(h, 0);  // [BYTES INSTALL.EXE seg2:0x3423]  // [BYTES INSTALL.EXE seg2:0x340d]
        return TRUE;
    }
    if (msg == WM_COMMAND && LOWORD(wp) == IDOK) {
        c->full = IsDlgButtonChecked(h, 1775) == BST_CHECKED;  // [BYTES INSTALL.EXE seg2:0x3480]
        EndDialog(h, 1);
        return TRUE;
    }
    return FALSE;
}

// ---- dialog 135, INSTALL.EXE seg2:0x2942

INT_PTR CALLBACK proc135(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    Ctx* c = ctx_of(h, msg, lp);
    if (msg == WM_INITDIALOG) {
        centre(h, 0);  // [BYTES INSTALL.EXE seg2:0x295c]
        win16_edit_borders(h);
        // [BYTES INSTALL.EXE seg2:0x2964 seg2:0x2997 DGROUP:0x724 DGROUP:0x729]
        SetDlgItemTextW(h, 2, L"name");
        SetDlgItemTextW(h, 3, L"organization");
        SendDlgItemMessageW(h, 2, EM_LIMITTEXT, 0x14, 0);
        SendDlgItemMessageW(h, 3, EM_LIMITTEXT, 0x19, 0);
        SendDlgItemMessageW(h, 2, EM_SETSEL, 0, 0x7fff);
        SetFocus(GetDlgItem(h, 2));
        return FALSE;
    }
    if (msg != WM_COMMAND) return FALSE;
    const int id = LOWORD(wp);
    if (id == IDOK) {
        // Enter moves Name -> Organization -> OK; elsewhere it accepts [BYTES INSTALL.EXE
        // seg2:0x29eb].
        const HWND f = GetFocus();
        if (f == GetDlgItem(h, 2)) {
            SetFocus(GetDlgItem(h, 3));
            SendDlgItemMessageW(h, 3, EM_SETSEL, 0, 0x7fff);
            return TRUE;
        }
        if (f == GetDlgItem(h, 3)) {
            SetFocus(GetDlgItem(h, 10));
            return TRUE;
        }
    }
    if (id == IDOK || id == 10) {
        // GetWindowText(2, 0x12) and (3, 0x16) [BYTES INSTALL.EXE seg2:0x2aae seg2:0x2ac7]. The
        // speed test that follows in the original's procedure runs right after the dialog
        // closes [DESIGN].
        c->name = window_text(h, 2, 0x12);
        c->org = window_text(h, 3, 0x16);
        EndDialog(h, 1);
        return TRUE;
    }
    return FALSE;
}

// ---- dialogs 200 and 100

INT_PTR CALLBACK proc_plain(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    Ctx* c = ctx_of(h, msg, lp);
    if (msg == WM_INITDIALOG) {
        centre(h, 0);  // seg2:0x15a6's procedure [BYTES INSTALL.EXE seg2:0x15bf]
        if (c && !c->done_text.empty()) SetDlgItemTextW(h, 130, wide(c->done_text).c_str());
        return TRUE;
    }
    if (msg == WM_COMMAND && (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL)) {
        EndDialog(h, 1);
        return TRUE;
    }
    return FALSE;
}

INT_PTR CALLBACK proc200(HWND h, UINT msg, WPARAM, LPARAM) {
    // The modeless "Installing..." dialog; its Cancel (control 1) is [OPEN INSTALL.EXE
    // seg2:0x15a6] and does nothing in the port.
    if (msg != WM_INITDIALOG) return FALSE;
    centre(h, 0);  // [BYTES INSTALL.EXE seg2:0x15bf]
    return TRUE;
}

void close_progress(Ctx& c) {
    if (c.progress) DestroyWindow(c.progress);
    c.progress = nullptr;
}

}  // namespace

std::optional<fs::path> run_install_win32(const Win32Options& o) {
    Ctx c;
    c.o = &o;
    const auto box = [&](const std::string& t, unsigned f) {
        return MessageBoxW(nullptr, wide(t).c_str(), wide(kTitle).c_str(), f);
    };
    const OpenResult opened = open_source(o.source, o.work_root, o.log);
    if (!opened.set) {
        if (!opened.message.empty()) box(opened.message, kMbExclamation);
        return std::nullopt;
    }
    const SourceSet& set = *opened.set;
    c.set = &set;
    c.log("install: the splash (INSTALL.EXE seg2:0x2624, seg2:0x2d70) is not shown [OPEN]");
    try {
        Ui ui;
        ui.log = o.log;
        ui.message = [&](const std::string& t, unsigned f) {
            close_progress(c);
            return box(t, f);
        };
        ui.components = [&](Components& k) {
            c.comp = &k;
            close_progress(c);
            return run_dialog(c, 300, proc300) == 1;
        };
        ui.progress = [&](const std::string& file, int pct) {
            if (!c.progress) {
                const std::vector<std::uint8_t> t = dialog_template(set, 200);
                c.progress = CreateDialogIndirectParamW(
                    o.instance, reinterpret_cast<const DLGTEMPLATE*>(t.data()), nullptr, proc200,
                    reinterpret_cast<LPARAM>(&c));
            }
            if (c.progress) {
                SetDlgItemTextW(c.progress, 120, wide(file).c_str());
                // The percent's format is [OPEN INSTALL.EXE seg2:0x15a6].
                SetDlgItemTextW(c.progress, 130, (std::to_wstring(pct) + L"%").c_str());
                UpdateWindow(c.progress);
            }
            pump();
        };
        ui.difficulty = [&](int& d, int& s) {
            close_progress(c);
            run_dialog(c, 1922, proc1922);
            d = c.diff;
            s = c.speed;
        };
        ui.window = [&] {
            run_dialog(c, 1774, proc1774);
            return c.full;
        };
        ui.personalize = [&](std::string& n, std::string& g) {
            run_dialog(c, 135, proc135);
            n = c.name;
            g = c.org;
        };
        ui.tick = [] { return static_cast<std::uint32_t>(GetTickCount()); };
        ui.done = [&](const std::string& text, const fs::path& readme) {
            close_progress(c);
            c.done_text = text;
            // Dialog 100 is modeless in the original (CreateDialog at seg2:0x1346); the port
            // shows it modal, which ends the same way [DESIGN].
            run_dialog(c, 100, proc_plain);
            // WinExec("notepad " + source + "README.TXT", SW_SHOWMAXIMIZED) [BYTES INSTALL.EXE
            // seg2:0x16ab seg2:0x16d9].
            if (o.open_readme && !readme.empty())
                ShellExecuteW(nullptr, L"open", L"notepad.exe",
                              (L"\"" + readme.wstring() + L"\"").c_str(), nullptr,
                              SW_SHOWMAXIMIZED);
        };
        std::string def = kDefaultDir;
        if (o.to) def = narrow(o.to->wstring());
        const Result r = run_install(set, o.ini_path, def, ui);
        close_progress(c);
        if (!r.completed) return std::nullopt;
        return r.game_dir;
    } catch (const std::exception& x) {
        close_progress(c);
        c.log(std::string("install: ") + x.what());
        box(x.what(), kMbExclamation);
        return std::nullopt;
    }
}

bool choose_install_set(HWND owner, fs::path* ini) {
    // [DESIGN] (ADR 0008 decision 1): start-up without a game offers the install step.
    if (MessageBoxW(owner,
                    L"No Inner Space game directory was found.\n\nInstall the game now from its "
                    L"install files (the folder holding INSTALL.INI)?",
                    L"Inner Space", MB_YESNO | MB_ICONQUESTION) != IDYES)
        return false;
    wchar_t file[MAX_PATH] = L"INSTALL.INI";
    OPENFILENAMEW of{};
    of.lStructSize = sizeof(of);
    of.hwndOwner = owner;
    of.lpstrFilter = L"Install set (INSTALL.INI)\0INSTALL.INI\0";
    of.lpstrFile = file;
    of.nMaxFile = MAX_PATH;
    of.lpstrTitle = L"Choose the install set's INSTALL.INI";
    of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameW(&of)) return false;
    *ini = fs::path(file);
    return true;
}

}  // namespace is::install

#endif  // _WIN32
