// The dialogs that ISPACE.DLL ordinal 10 opens (docs/re/shell.md "Game-start dialogs" and
// "Exit", docs/re/ispace.md ordinal 10): Restore Game #267, New Game #321, New Demo #589, the
// exit dialog #244, Registration Benefits #799, the Plus Pack #1872, Important Information
// #988, Order #693 and Save Game #167, each built from its ENG_LANG.DLL template
// (AssetStore::dialog_template) and run with its ported procedure. The rules they apply are in
// src/dialogs/rules.h.
//
// This layer knows nothing of the main program: Env carries the files, the INI, the strings
// and the side effects, so the tests can open the dialogs modeless on their own. The wrapper
// seg10:0x82a2 that sits around them (music, the result tail) is src/dialogs/hooks.cpp.
// Win32 only.
#pragma once

#ifdef _WIN32
#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "dialogs/rules.h"

namespace is::assets {
class AssetStore;
}
namespace is::settings {
class IniFile;
}
namespace is::nav {
class Fs;
}

namespace is::dialogs {

// Everything a dialog reads or does outside its own controls.
struct Env {
    HINSTANCE instance = nullptr;  // the instance the templates are created with
    HWND owner = nullptr;          // hwnd of ordinal 10 (DGROUP:0x16)
    const assets::AssetStore* store = nullptr;
    settings::IniFile* ini = nullptr;
    // LocalPath (DGROUP:0x2056 in ISPACE.DLL): the GAMES\ directory of the saved games.
    std::filesystem::path local_path;
    // Path (DGROUP:0x20c4): ORDER.TXT and README.TXT. The asset directory (ADR 0004 decision 4).
    std::filesystem::path text_dir;
    // ShipPath with the fallback of ADR 0004 decision 4: MASTER.BOD and MASTER.SHP.
    std::filesystem::path ship_dir;
    HICON game_icon = nullptr;  // INSPACE.EXE GROUP_ICON GAME
    int screen_cx = 0, screen_cy = 0;  // SM_CXSCREEN, SM_CYSCREEN

    // The arguments of the call (shell.md's command table) and the date of command 15.
    int x4 = 0, x5 = 0;
    int tm_mday = 0, tm_mon = 0, tm_year = 0;
    // ISPACE.DLL DGROUP:0x1ac, the availability cache (rules.h availability_cache).
    int availability_cache = 0;

    // Strings, cp1252: ENG_LANG.DLL and INSPACE.EXE; empty when missing.
    std::function<std::string(int)> lang;
    std::function<std::string(int)> exe_string;
    // seg10:0x744c, the sound wrapper (is::sound::play).
    std::function<void(int)> sound;
    // log_todo: an action that a later stage ports.
    std::function<void(const std::string&, const char*)> todo;
    // The INI changed: persist it (the original's WritePrivateProfileString writes at once).
    std::function<void()> ini_changed;
    // MessageBox(owner, text, caption, flags); the tests answer it.
    std::function<int(HWND, const std::string& text, const std::string& caption, UINT flags)>
        message_box;
    // ShellExecute "open" of a text file (ADR 0008 decision 3: the system's text editor).
    std::function<void(const std::filesystem::path&)> open_text;
    // GetCurrentTime of the serial generator's srand (ISPACE.DLL seg10:0x4a0); null means
    // GetTickCount, the milliseconds since boot as GetCurrentTime counts them. The tests fix it.
    std::function<std::uint32_t()> current_time;
};

// The INI writes of the dialogs, in order, as they happened (for the logs and the tests).
using WriteLog = std::vector<IniWrite>;

class Dialog {
public:
    Dialog(Env& env, std::uint16_t template_id) : env_(env), template_(template_id) {}
    virtual ~Dialog();
    Dialog(const Dialog&) = delete;
    Dialog& operator=(const Dialog&) = delete;

    std::uint16_t template_id() const { return template_; }
    HWND hwnd() const { return hwnd_; }
    // The EndDialog result, and whether the procedure ended the dialog.
    int result() const { return result_; }
    bool ended() const { return ended_; }
    const WriteLog& writes() const { return writes_; }

    // DialogBoxIndirectParam with the template; returns the result (-1 when it cannot open).
    int run_modal();
    // CreateDialogIndirectParam with the same procedure (the tests); null on failure.
    HWND create_modeless();

    // The procedure's dispatch, for the static DLGPROC.
    INT_PTR handle(HWND h, UINT msg, WPARAM w, LPARAM l);

protected:
    virtual void init() = 0;
    virtual void command(int id, int code) { (void)id, (void)code; }

    void end(int result);
    // seg10:0x602a(hDlg, 0): the bold font to push buttons, the normal one to the rest, and
    // every child shown.
    void set_fonts();
    void bold(int id);
    // The MoveWindow centring of every procedure, from the window rectangle taken at init.
    void centre();
    // The height of "Test" in the normal font, for the combo sizes.
    int text_height();
    void size_combo(int id, int height);
    // STM_SETICON of the GAME icon on the SS_ICON control 591.
    void set_icon();
    void set_text(int id, const std::string& cp1252);
    std::string text(int id, int max_chars) const;
    void add(int id, const std::string& cp1252);  // CB_ADDSTRING
    int find(int id, const std::string& cp1252);  // CB_FINDSTRING(-1, ...)
    void select(int id, int index);
    int selection(int id) const;
    std::string selected_text(int id) const;
    void write(const std::string& key, const std::string& value);
    std::string ini_string(const std::string& key, const std::string& def, std::size_t nsize);
    int ini_int(const std::string& key, int def);
    std::string lang(int id) const { return env_.lang ? env_.lang(id) : std::string(); }
    void sound(int id) const {
        if (env_.sound) env_.sound(id);
    }
    void todo(const std::string& what, const char* address) const {
        if (env_.todo) env_.todo(what, address);
    }
    int box(const std::string& text, UINT flags);
    std::vector<std::uint8_t> lang_resource(int type, int id) const;
    // The LevClass child (ISPACE.DLL seg10:0x4250 and the CreateWindow of each procedure).
    HWND preview(int x, int y, int w, int h);

    Env& env_;
    std::uint16_t template_;
    HWND hwnd_ = nullptr;
    RECT start_rect_{};
    HFONT normal_ = nullptr, bold_ = nullptr;
    int result_ = 0;
    bool ended_ = false;
    bool modal_ = false;
    WriteLog writes_;
};

// The saved games of <LocalPath>GAMES\*.SAV in find order (attribute 1: files, not
// directories) [BYTES ISPACE.DLL seg10:0x154f seg10:0x15af].
std::vector<std::string> list_saves(const std::filesystem::path& local_path);

// Restore Game #267 (ISPACE.DLL seg10:0xec2): 1 for NEW GAME, 2 for a save, 0 for Cancel.
class RestoreGame final : public Dialog {
public:
    RestoreGame(Env& env, std::vector<std::string> saves);
    int ship_index() const { return ship_; }  // ISPACE.DLL DGROUP:0x1f8c
    int found() const { return found_; }

protected:
    void init() override;
    void command(int id, int code) override;

private:
    void fill(int index);  // seg10:0x1a80

    std::vector<std::string> saves_;
    fmt::MasterShp ships_;
    fmt::MasterBod bodies_;
    int ship_ = -1;
    int found_ = 0;
    std::string pilot_;  // DGROUP:0x1568
};

// New Game #321 and New Demo #589 (ISPACE.DLL seg10:0xf26): 1 for OK, 0 for Cancel.
class NewGame final : public Dialog {
public:
    // A drive the DRIVE combo lists: its letter and the label as the find-first gives it.
    struct Drive {
        char letter = 'C';
        std::optional<std::string> label;
    };
    NewGame(Env& env, bool demo, std::vector<Drive> drives);
    // The drives C: to Z: that exist and can be made current, with their labels
    // (GetLogicalDrives, SetCurrentDirectory("X:") and GetVolumeInformation in place of
    // _chdrive and the find-first of attribute 8) [DESIGN].
    static std::vector<Drive> system_drives();
    // The list from a filesystem (system_drives() passes Win32Fs; tests pass a MemFs).
    static std::vector<Drive> drives_from(nav::Fs& fs, const std::function<void(const std::string&)>& log);
    bool demo() const { return demo_; }

protected:
    void init() override;
    void command(int id, int code) override;

private:
    void show_ship(const std::string& name);  // the preview and TEAM (seg10:0x2978)
    void ok();                                // seg10:0x250c(hDlg, 1)

    bool demo_;
    std::vector<Drive> drives_;
    fmt::MasterShp ships_;
    fmt::MasterBod bodies_;
    std::string ship_;  // DGROUP:0x1554
};

// The exit dialog #244 (ISPACE.DLL seg10:0x103c): kExitCancel, kExitExit or kExitSave.
class ExitDialog final : public Dialog {
public:
    ExitDialog(Env& env, int cmd) : Dialog(env, kDlgExit), cmd_(cmd) {}

protected:
    void init() override;
    void command(int id, int code) override;

private:
    int cmd_;
};

// Registration Benefits #799 (command 12) and the Plus Pack #1872 (command 14), procedure
// ISPACE.DLL seg10:0x5f96.
class Benefits final : public Dialog {
public:
    Benefits(Env& env, int cmd)
        : Dialog(env, cmd == kCmdPlusPack ? kDlgPlusPack : kDlgBenefits), cmd_(cmd) {}

protected:
    void init() override;
    void command(int id, int code) override;

private:
    int cmd_;
};

// Important Information #988 (ISPACE ordinal 39, ISPACE.DLL seg10:0x124a).
class Important final : public Dialog {
public:
    explicit Important(Env& env) : Dialog(env, kDlgImportant) {}

protected:
    void init() override;
    void command(int id, int code) override;
};

// Order #693 (ISPACE.DLL seg10:0x664), with Register disabled and its edit read-only (ADR 0008
// decision 3).
class Order final : public Dialog {
public:
    explicit Order(Env& env) : Dialog(env, kDlgOrder) {}
    ~Order() override;

protected:
    void init() override;
    void command(int id, int code) override;

private:
    void cleanup(int result);  // seg10:0x78e's cleanup
    void show_serial(bool bump);  // seg10:0x5dfe(hDlg, bump)
    HWND logo_ = nullptr;
};

// Save Game #167 (ISPACE.DLL seg10:0xdc0): 1 with INI Restore written, 0 for Cancel.
class SaveGame final : public Dialog {
public:
    SaveGame(Env& env, std::vector<std::string> saves)
        : Dialog(env, kDlgSaveGame), saves_(std::move(saves)) {}

protected:
    void init() override;
    void command(int id, int code) override;

private:
    std::vector<std::string> saves_;
};

// ISPACE ordinal 10's dispatch for the commands the shell reaches: 1 (Restore Game, then New
// Game on 1), 3, 4, 5, 7, 8 (and Important Information when Order returns 1), 11, 12 and 14
// [BYTES ISPACE.DLL seg10:0x436f-0x4426 seg10:0x434c seg10:0x4355]. Other commands log and
// return 0. Command 4 and the second half of 1 enumerate the drives.
int ordinal10(Env& env, int cmd);

// The LevClass frame of ISPACE.DLL seg10:0x309c on the client rectangle `r`, with GDI.
void paint_frame(HDC dc, const RECT& r);

// The window class "LevClass" (ISPACE.DLL seg10:0x4250(0)): registered once per instance.
bool register_lev_class(HINSTANCE instance);

// cp1252 <-> UTF-16 at the Win32 calls (ADR 0003).
std::wstring wide(const std::string& cp1252);
std::string narrow(const std::wstring& utf16);

}  // namespace is::dialogs
#endif
