// The program's saved game (save_game.h, the _WIN32 part): the Win32 SaveHost over LocalPath
// (ADR 0004 decision 5, ADR 0014 decision 1), the copies of the session's words in and out, and
// the state-5 links. Win32 only: the host build compiles this file empty.
#ifdef _WIN32
#include "dialogs/save_game.h"

#include <windows.h>

#include <ctime>
#include <exception>
#include <filesystem>
#include <memory>
#include <set>

#include "app/app.h"
#include "assets/asset_store.h"
#include "briefing/hooks.h"
#include "dialogs/dialogs.h"
#include "dialogs/hooks.h"
#include "hud/screens.h"
#include "nav/nav.h"
#include "settings/settings.h"
#include "world/flight_hooks.h"
#include "world/hooks.h"
#include "world/roster.h"
#include "world/world.h"

namespace is::dialogs {

namespace {

using app::g;

std::set<std::string> g_logged;
void todo_once(const std::string& what, const char* address) {
    if (g_logged.insert(what).second) app::log_todo(what, address);
}

std::string lang(int id) {
    try {
        if (g.store)
            if (auto s = g.store->find_string(static_cast<std::uint16_t>(id))) return *s;
    } catch (const std::exception& x) {
        app::log("string " + std::to_string(id) + ": " + x.what());
    }
    return {};
}

std::filesystem::path local(const std::string& rel) { return g.local_path / std::filesystem::path(wide(rel)); }

class Win32SaveHost final : public save::SaveHost {
public:
    std::optional<std::vector<std::uint8_t>> read(const std::string& path, std::size_t max) override {
        const auto p = local(path);
        HANDLE f = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE) return std::nullopt;
        std::vector<std::uint8_t> out;
        std::uint8_t buf[0x4000];
        while (out.size() < max) {
            const DWORD want = static_cast<DWORD>(std::min<std::size_t>(sizeof buf, max - out.size()));
            DWORD got = 0;
            if (!ReadFile(f, buf, want, &got, nullptr) || got == 0) break;
            out.insert(out.end(), buf, buf + got);
        }
        CloseHandle(f);
        return out;
    }
    bool write_atomic(const std::string& path, const std::vector<std::uint8_t>& bytes) override {
        // ADR 0014 decision 1 and 9 [DESIGN]: a temporary file beside the target, MoveFileExW
        // over it; every failure path closes the file and removes the temporary.
        const auto target = local(path);
        std::error_code ec;
        // The target directory created when missing [DESIGN ADR 0014 decision 11].
        if (std::filesystem::create_directories(target.parent_path(), ec))
            todo_once("the save directory created, where OpenFile OF_CREATE fails", "seg6:0x885c");
        const std::filesystem::path tmp = target.wstring() + L".tmp";
        HANDLE f = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE) {
            app::log("save: " + fmt::path_utf8(tmp) + " does not open for writing");
            return false;
        }
        DWORD put = 0;
        const bool wrote = bytes.empty() ||
                           (WriteFile(f, bytes.data(), static_cast<DWORD>(bytes.size()), &put, nullptr) &&
                            put == bytes.size());
        const bool flushed = wrote && FlushFileBuffers(f);
        CloseHandle(f);
        if (!flushed || !MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            app::log("save: the write of " + fmt::path_utf8(target) + " failed (" + std::to_string(GetLastError()) +
                     "), the old file kept");
            DeleteFileW(tmp.c_str());
            return false;
        }
        return true;
    }
    bool remove(const std::string& path) override { return DeleteFileW(local(path).c_str()) != 0; }
    std::vector<std::string> find(const std::string& dir, const std::string& pattern) override {
        // findfirst attribute 1: plain and read-only files [BYTES seg6:0x7075 seg6:0x6836].
        std::vector<std::string> out;
        WIN32_FIND_DATAW d;
        const auto p = local(dir + pattern);
        HANDLE h = FindFirstFileW(p.c_str(), &d);
        if (h == INVALID_HANDLE_VALUE) return out;
        do {
            if (d.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) continue;
            out.push_back(narrow(d.cFileName));
        } while (FindNextFileW(h, &d));
        FindClose(h);
        return out;
    }
    std::string ini(const char* key, const char* def, std::size_t n) override {
        return g.ini ? g.ini->get_string(settings::kGameSection, key, def, n) : std::string(def);
    }
    void ini_set_int(const char* key, int v) override {
        if (!g.ini) return;
        g.ini->set_int(settings::kGameSection, key, v);
        try {
            g.ini->save();
        } catch (const std::exception& x) {
            app::log(std::string("INSPACE.INI: ") + x.what());
        }
    }
    void message(int id) override {
        // seg16:0x68d6: MessageBox(hwnd, string, "Inner Space" DGROUP:0x9690, 0x30) [BYTES seg16:0x6920]
        MessageBoxW(g.main, wide(lang(id)).c_str(), L"Inner Space", MB_ICONEXCLAMATION);
    }
    void log(const std::string& s) override { app::log(s); }
    std::tm now() override {
        const std::time_t t = std::time(nullptr);
        const std::tm* tm = std::localtime(&t);
        return tm ? *tm : std::tm{};
    }
    std::uint32_t time() override { return static_cast<std::uint32_t>(std::time(nullptr)); }
    int shp_count() override { return ships() ? static_cast<int>(ships_->ships.records.size()) : 0; }
    bool ship_ok(int v) override {
        return ships() ? world::ship_accepted(world::current(), ships_->ships, ships_->bodies, v) : true;
    }
    std::string ship_name(int v) override {
        if (!ships() || v < 0 || static_cast<std::size_t>(v) >= ships_->ships.records.size()) return {};
        return ships_->ships.records[static_cast<std::size_t>(v)].name;
    }

private:
    // seg14:0x3eda: MASTER.BOD and MASTER.SHP from ShipPath, as hooks.cpp ship_dir resolves it
    // (ADR 0004 decision 4), once per operation.
    const assets::Ships* ships() {
        if (tried_) return ships_.get();
        tried_ = true;
        try {
            if (!g.store) return nullptr;
            std::error_code ec;
            std::filesystem::path dir = g.asset_dir;
            const std::string s = ini("ShipPath", "", 0x6d);
            if (!s.empty() && std::filesystem::exists(std::filesystem::path(wide(s)) / L"MASTER.SHP", ec))
                dir = std::filesystem::path(wide(s));
            else if (std::filesystem::exists(g.asset_dir / L"SHIPS" / L"MASTER.SHP", ec))
                dir = g.asset_dir / L"SHIPS";
            ships_ = std::make_unique<assets::Ships>(g.store->ships(dir));
        } catch (const std::exception& x) {
            app::log(std::string("save: MASTER.SHP: ") + x.what());
        }
        return ships_.get();
    }
    bool tried_ = false;
    std::unique_ptr<assets::Ships> ships_;
};

// The session's words in and out of one operation.
struct ProgramCtx {
    Win32SaveHost host;
    save::SessionValues s;
    save::Ctx c;
    explicit ProgramCtx(int player)
        : c{world::current(), nav::records(), world::flight_globals().frames, s, save::globals(), host,
            g.edition.edition, player} {
        const Session& q = session();
        s.game_id = q.game_id;
        s.game_date = q.game_date;
        s.pilot = q.pilot;
        s.table = q.table;
        s.size = q.size;
        s.drive = q.drive;
        s.play_mode = g.play_mode;
        s.wave = briefing::wave();  // DGROUP:0x91f8's one home (M4 s1 fixer)
        s.restored = q.restored;
    }
    void write_back() {
        Session& q = session();
        q.game_id = s.game_id;
        q.game_date = s.game_date;
        q.pilot = s.pilot;
        q.table = s.table;
        q.size = s.size;
        q.drive = s.drive;
        g.play_mode = s.play_mode;
        q.wave = s.wave;
        briefing::set_wave(s.wave);
        q.restored = s.restored;
    }
};

int session_player() { return session().player; }

}  // namespace

int program_save(int named, int kind, int player) {
    ProgramCtx p(player);
    const int r = save::save_game(p.c, named, kind);
    p.write_back();  // the credit cap and the ship records changed in place
    return r;
}

int program_roster_save(int player) {
    ProgramCtx p(player);
    const int r = save::roster_update_and_save(p.c);
    p.write_back();
    return r;
}

int program_restore(int player) {
    ProgramCtx p(player);
    const int r = save::restore_game(p.c);
    p.write_back();  // step 1's id and date also on a failure (decision 2)
    return r;
}

void program_save_lastgame() { program_save(0, 0, session_player()); }

void sync_records_to_nav() { save::sync_to_nav(world::current(), nav::records()); }
void sync_records_from_nav() { save::sync_from_nav(nav::records(), world::current()); }

void save_new_game() { save::new_game(save::globals()); }

void install_high_scores() {
    briefing::HighScoreLinks& l = briefing::high_score_links();
    l.build_list = [] {
        // seg6:0x5d58 steps 4 and 5: the table block and seg6:0x6576 [BYTES seg6:0x5ddc-0x5df9].
        ProgramCtx p(session_player());
        save::SaveGlobals& sg = save::globals();
        const fmt::save::ScoreList& list = save::score_list(p.c);
        sg.table = fmt::save::champion_slots(list, [&](int v) { return p.host.ship_name(v); });
        app::log("score list seg6:0x6576: " + std::to_string(list.list.size()) + " entries, rank " +
                 std::to_string(sg.rank));
        return static_cast<int>(sg.rank);
    };
    l.after_music = [](bool from_ending) {
        save::SaveGlobals& sg = save::globals();
        // Step 7: seg6:0x885c(1, 1) unless DGROUP:0x94cc != 0 [BYTES seg6:0x5e2d-0x5e39].
        if (!from_ending) program_save(1, 1, session_player());
        // Step 8: DGROUP:0x6b00; the particle bursts seg6:0x72d8 / seg6:0x84fc are not ported
        // [OPEN], so the table is drawn at the deadline [BYTES seg6:0x5e49-0x5f4e].
        sg.table_due = save::table_deadline(static_cast<std::uint32_t>(g.game.now()), sg.rank, from_ending);
        todo_once("the state-5 particle bursts", "seg6:0x72d8");
        // Step 9: DGROUP:0x94c4 = 0, seg6:0x5f7a(1) [BYTES seg6:0x5f51-0x5f5d].
        sg.table_drawn = false;
        briefing::high_score_links().frame(true);
    };
    l.frame = [](bool first) {
        save::SaveGlobals& sg = save::globals();
        if (first) sg.table_drawn = false;  // DGROUP:0x1c3c = 0 [BYTES seg6:0x5f8d]
        const std::uint32_t now = static_cast<std::uint32_t>(g.game.now());
        if (!save::table_due(now, sg.table_due, sg.table_drawn)) return;
        sg.table_drawn = true;  // DGROUP:0x94c4 = DGROUP:0x1c3c = 1 [BYTES seg6:0x6248-0x6260]
        if (!g.gfx || g.gfx->surface().empty()) return;
        std::vector<hud::ChampionRow> rows;
        for (const fmt::save::TableSlot& t : sg.table) rows.push_back({t.text, t.score, t.place});
        const auto texts = hud::champion_texts(g.area.w, g.area.play_h, g.area.h, lang(925), rows, sg.rank,
                                               static_cast<std::int32_t>(world::current().score), lang(308),
                                               lang(2063));
        for (const hud::TableText& t : texts) g.gfx->font_text(t.font, t.align, t.colour, t.x, t.y, t.text);
        app::present_frame();
        app::log("high scores: seg6:0x62b6 drew " + std::to_string(rows.size()) + " rows");
    };
    l.free = [] { save::free_scores(save::globals()); };
}

}  // namespace is::dialogs
#endif
