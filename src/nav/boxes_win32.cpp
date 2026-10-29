// The RACE and DUEL boxes: the window side (see boxes.h). Win32 only.
#ifdef _WIN32
#include "nav/boxes.h"

#include <string>

namespace is::nav {

namespace {

std::wstring wide(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(1252, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(1252, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

// LoadString(id, buf, 0x28): at most 39 bytes [BYTES seg6:0x2441] [REF Win16 SDK LoadString].
std::string load(const std::function<std::string(int)>& lang, int id) {
    std::string s = lang ? lang(id) : std::string();
    if (s.size() > static_cast<std::size_t>(kBoxStringMax - 1)) s.resize(kBoxStringMax - 1);
    return s;
}

HWND make(HWND nav, HINSTANCE instance, const wchar_t* cls, const std::string& text,
          unsigned long style, const Box& b, int id) {
    return CreateWindowExW(0, cls, wide(text).c_str(), style, b.x, b.y, b.w, b.h, nav,
                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
}

}  // namespace

int create_boxes(HWND nav, HINSTANCE instance, HFONT font,
                 const std::function<std::string(int)>& lang, const settings::IniFile* ini) {
    RECT c{};
    GetClientRect(nav, &c);
    // The group boxes: WM_SETFONT with lParam 1 [BYTES seg6:0x25b6 seg6:0x25c1 seg6:0x2614
    // seg6:0x261f].
    for (const auto& g : {std::make_pair(kRaceBoxId, kStrRaceBox), std::make_pair(kDuelBoxId, kStrDuelBox)}) {
        HWND h = make(nav, instance, L"button", load(lang, g.second), kGroupBoxStyle,
                      create_box(g.first, c.right, 0), g.first);
        if (h && font) SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
    // DGROUP:0x94b8: GetTextExtent("Test", 4)'s height in DGROUP:0x90fe on a DC of the window,
    // released to the main window's handle [BYTES seg6:0x2642 seg6:0x2658]. The port releases it
    // to the window it came from [DESIGN].
    int ext = 0;
    if (HDC dc = GetDC(nav)) {
        HGDIOBJ old = font ? SelectObject(dc, font) : nullptr;
        SIZE sz{};
        if (GetTextExtentPoint32A(dc, kExtentText, 4, &sz)) ext = sz.cy;
        if (old) SelectObject(dc, old);
        ReleaseDC(nav, dc);
    }
    for (const auto& cb : combos()) {
        HWND h = make(nav, instance, L"combobox", "", kComboStyle, create_box(cb.id, c.right, ext), cb.id);
        if (!h) continue;
        // The combos get WM_SETFONT with lParam 0, the group boxes lParam 1 (shell.md
        // "Game-start dialogs" item 11; M3 s3 fixer, C012) [BYTES seg6:0x26b6].
        if (font) SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
        for (int id : cb.strings)
            SendMessageW(h, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide(load(lang, id)).c_str()));
        SendMessageW(h, CB_SETCURSEL, static_cast<WPARAM>(initial_index(ini, cb)), 0);
    }
    return ext;
}

void place_boxes(HWND nav, int cw, int ch, int ext) {
    for (int id : {kRaceBoxId, kDuelBoxId, kRacersId, kDuelersId, kRaceDiffId, kDuelDiffId,
                   kRaceLapsId, kRaceOppId, kDuelOppId}) {
        if (HWND h = GetDlgItem(nav, id)) {
            const Box b = size_box(id, cw, ch, ext);
            MoveWindow(h, b.x, b.y, b.w, b.h, TRUE);
        }
    }
}

void show_boxes(HWND nav, BoxKind kind) {
    // seg6:0x2dba's order: 0x8c-0x90, then 0x93, 0x91, 0x92, 0x94 [BYTES seg6:0x2dba].
    for (int id : {kRaceBoxId, kRacersId, kRaceDiffId, kRaceLapsId, kRaceOppId, kDuelBoxId,
                   kDuelersId, kDuelDiffId, kDuelOppId})
        if (HWND h = GetDlgItem(nav, id)) ShowWindow(h, show_command(kind, id));
}

BoxValues read_boxes(HWND nav) {
    BoxValues v;
    for (const auto& cb : combos()) {
        HWND h = GetDlgItem(nav, cb.id);
        // CB_GETCURSEL's WORD result, CB_ERR = -1 [BYTES seg6:0x32fa-0x34aa].
        v.at(cb.id) = h ? static_cast<int>(SendMessageW(h, CB_GETCURSEL, 0, 0)) : -1;
    }
    return v;
}

}  // namespace is::nav
#endif
