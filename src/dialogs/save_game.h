// The saved game on the program's state (docs/re/formats.md "Saved games", ADR 0014): the writer
// seg6:0x885c(named, kind) with its gates, the purge seg6:0x7042, the names and the free-name
// loop, the header seg6:0x8eea from the World; the reader seg6:0x930a, read and validated into
// new blocks and installed only on success (ADR 0014 decision 2); the score list seg6:0x6576
// and the state-5 deadline of seg6:0x5d58; and the DGROUP words a saved game carries that the
// port has no other home for (SaveGlobals). The byte layout is src/formats/savegame.h; the
// files, the INI and the message boxes go through SaveHost (save_game_win32.cpp is the
// program's). Portable (M4 stage 3, track SAVE).
//
// The Save Game dialog #167 itself (ISPACE.DLL seg10:0xdc0: the combo, the 8-character cut,
// the LASTGAME refusal and the INI Restore write) is dialogs.cpp SaveGame; the Always Ask for
// Save Name box (control 670 of the Display options, INI SavePrompt) is read by save_step
// through rules.h save_dialog_opens.
#pragma once

#include <array>
#include <cstdint>
#include <ctime>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "formats/savegame.h"

namespace is::world {
class World;
}  // namespace is::world
namespace is::nav {
class Records;
}  // namespace is::nav

namespace is::dialogs::save {

using fmt::save::DirBytes;
using fmt::save::RosterBytes;

// The DGROUP words of the header that no other part of the port holds yet; a restore loads
// them and a save writes them back, so a file round-trips until their writers are ported
// (s3-save.md section 7). Their homes in src/world and src/hud are the fixer's requests.
struct SaveGlobals {
    std::array<std::int16_t, 24> w4fd0{};  // DGROUP:0x4fd0, 8 x 3 (header +0xe6)
    std::array<std::int16_t, 24> w6b10{};  // DGROUP:0x6b10, 8 x 3 (header +0x116)
    std::array<std::int16_t, 8> w6b40{};   // DGROUP:0x6b40, 8 words (header +0x146; seg4:0x774a adds)
    // DGROUP:0xa14d / 0xa157 (header +0x174, +0x17e) and DGROUP:0xa161 (+0x188) are the World's
    // (ba14d, ba157, ba161, M4 s3 ROSTER).
    std::uint8_t a162 = 0;                 // DGROUP:0xa162 (header +0x189)
    std::int16_t w92e2 = 0;                // DGROUP:0x92e2: 1 after a restore changed Size
    std::int16_t w951c = 0, w104c = 0, w104e = 0;  // cleared by a restore [BYTES seg6:0x98b9-0x98c4]
    // The granularity the game's file was read with; a new game's is 32 (ADR 0014 decision 1).
    fmt::save::Granularity granularity = fmt::save::kNewGameGranularity;
    // DGROUP:0x93a2, the running game's place in the score list (0 in a fresh process).
    std::int16_t rank = 0;
    // DGROUP:0x6af8 / DGROUP:0x94c6: the kept score list; null (not built) until seg6:0x6576.
    std::optional<fmt::save::ScoreList> scores;
    // State 5: the table DGROUP:0x94d8 (slots), its deadline DGROUP:0x6b00 and the drawn flag
    // DGROUP:0x1c3c.
    std::vector<fmt::save::TableSlot> table;
    std::uint32_t table_due = 0;
    bool table_drawn = false;
};
SaveGlobals& globals();
// A new game's values (seg10:0x82a2's new-game path): the granularity back to 32 [DESIGN ADR
// 0014 decision 1].
void new_game(SaveGlobals& g);

// The game-start session's words a saved game holds (dialogs/hooks.h Session, the program's
// PlayMode): the glue copies them in and out.
struct SessionValues {
    std::uint32_t game_id = 0;     // DGROUP:0x939a
    std::uint32_t game_date = 0;   // DGROUP:0x939e (low word) and DGROUP:0x93a0 (high word)
    std::string pilot;             // DGROUP:0x380c
    std::array<std::array<std::int16_t, 8>, 8> table{};  // DGROUP:0x3824
    int size = 1;                  // DGROUP:0x9156
    int drive = 0;                 // DGROUP:0x94ac
    int play_mode = 0;             // DGROUP:0x9134
    int wave = 0;                  // DGROUP:0x91f8
    int restored = 0;              // DGROUP:0x92e4
};

// The files, the INI and the user. Paths are relative to LocalPath, '\' separated, cp1252.
class SaveHost {
public:
    virtual ~SaveHost() = default;
    // At most `max` bytes of the file; null when it does not open.
    virtual std::optional<std::vector<std::uint8_t>> read(const std::string& path, std::size_t max) = 0;
    // ADR 0014 decision 1 "Writing is atomic": a temporary file beside the target, then
    // MoveFileExW(MOVEFILE_REPLACE_EXISTING); false leaves the old file whole and no temporary
    // behind (decision 9). The directory is created when missing [DESIGN].
    virtual bool write_atomic(const std::string& path, const std::vector<std::uint8_t>& bytes) = 0;
    virtual bool remove(const std::string& path) = 0;  // OpenFile(name, OF_DELETE)
    // findfirst/findnext attribute 1 over dir + pattern: the found names, in find order.
    virtual std::vector<std::string> find(const std::string& dir, const std::string& pattern) = 0;
    virtual std::string ini(const char* key, const char* def, std::size_t n) = 0;
    virtual void ini_set_int(const char* key, int v) = 0;
    // seg16:0x68d6(DGROUP:0x16, id, -1): MessageBox(string id, "Inner Space", 0x30).
    virtual void message(int id) = 0;
    virtual void log(const std::string& s) = 0;
    virtual std::tm now() = 0;   // seg10:0x804c's localtime
    virtual std::uint32_t time() = 0;  // seg10:0x804c(1), time()
    // MASTER.SHP's record count DGROUP:0x95aa, seg14:0x455c's refusal and record +0x0a's name.
    virtual int shp_count() = 0;
    virtual bool ship_ok(int shp) = 0;
    virtual std::string ship_name(int shp) = 0;
};

struct Ctx {
    world::World& w;
    nav::Records& nav;
    std::uint32_t& frames;   // DGROUP:0x5a8 (world::flight_globals().frames)
    SessionValues& s;
    SaveGlobals& g;
    SaveHost& host;
    int edition = 0;       // DGROUP:0x920c
    int player_game = 1;   // DGROUP:0x95bc
};

// The directories: LocalPath + Games + '\' and LocalPath + Dead + '\' (seg6:0x6d8: each at
// most 10 characters with '\' appended) [BYTES seg6:0x978 seg6:0x9a0].
std::string games_dir(SaveHost& h);
std::string dead_dir(SaveHost& h);

// ---- the directory records: one byte store (World::records), Navigation's view synced
// World → Navigation: the counters +0x14e/+0x150 and the state +0x1d0 the wave wrote, and the
// path when the bytes hold one (at Navigation's open).
void sync_to_nav(const world::World& w, nav::Records& nav);
// Navigation → World: the records Enter added and their paths, and a state 2 Navigation set
// (seg6:0x59b6) (at Enter and before a save).
void sync_from_nav(const nav::Records& nav, world::World& w);

// ---- the roster block: World::roster's records in index order through world::roster_to_bytes
// and roster_from_bytes (src/world/roster.h, the ROSTER track's).
std::vector<RosterBytes> roster_out(const world::World& w);
void roster_in(world::World& w, const std::vector<RosterBytes>& r);

// seg6:0x8eea on the program's state. Caps the player's credit dword +0xdc in place, as the
// original does [BYTES seg6:0x8fff].
fmt::save::Header gather_header(Ctx& c, const std::vector<RosterBytes>& roster, int dir_count);

// seg6:0x885c(named, kind): 1 when a file was written.
int save_game(Ctx& c, int named, int kind);
// seg6:0x99b6(): the save-time roster update (world::roster_from_ships_for_save, the ROSTER
// track's loop), then seg6:0x885c(1, 0) at seg6:0x9e56; its result [BYTES seg6:0x99b6-0x9e56].
int roster_update_and_save(Ctx& c);
// seg6:0x930a: 1 when a game was restored. The trial's refusal (2325) is the caller's
// (intro::GameStart::restore), as the original's callers test before the call.
int restore_game(Ctx& c);
// The install of a parsed .sav (steps 6 to 10 of formats.md "Reading"), after the roster check.
void install(Ctx& c, fmt::save::Parsed& p);
// Step 8 with ADR 0014 decision 7: records k < DGROUP:0x931a: byte +0x64 = 7 → 0; word +0x00
// not below the SHP count → 1, and record 0's when seg14:0x455c refuses it; the last record gets
// the bounds part only [BYTES seg6:0x986d-0x98b2] [DESIGN decision 7].
void roster_check(std::vector<RosterBytes>& r, int shp_count, const std::function<bool(int)>& ship_ok);

// seg6:0x6576's scan (passes 0, 1, 2) and lists, kept in SaveGlobals::scores until
// seg6:0x717a frees them (free_scores); sets SaveGlobals::rank when the list sets it.
const fmt::save::ScoreList& score_list(Ctx& c);
void free_scores(SaveGlobals& g);  // seg6:0x626c: seg6:0x717a and the table's free
// seg6:0x5d58's DGROUP:0x6b00: from state 9 now + 11000; rank > 10 now; rank 1 now + 9800;
// else now + (12 - rank) x 600 + 1000 (verify claim 30) [BYTES seg6:0x5e49-0x5f39].
std::uint32_t table_deadline(std::uint32_t now, int rank, bool from_ending);
// seg6:0x5f7a's draw test: strictly now > DGROUP:0x6b00 (verify claim 43, CORRECTED), the
// movers done (the bursts are not ported, so always [OPEN]) and DGROUP:0x1c3c = 0 [BYTES
// seg6:0x6235-0x6260].
bool table_due(std::uint32_t now, std::uint32_t due, bool drawn);

}  // namespace is::dialogs::save

#ifdef _WIN32
// The program's saved game (save_game_win32.cpp): the Win32 host over LocalPath, INSPACE.INI and
// the main window, and the program's state (dialogs::session(), app::g, world::current(),
// nav::records(), world::flight_globals()).
namespace is::dialogs {
// seg6:0x885c(named, kind); `player` is DGROUP:0x95bc.
int program_save(int named, int kind, int player);
// seg6:0x99b6 then seg6:0x885c(1, 0) (the save step's arguments 1 to 3).
int program_roster_save(int player);
// seg6:0x930a after its callers' trial test (intro::GameStart::restore).
int program_restore(int player);
// LASTGAME at the wave end: seg9:0x31f0's seg6:0x885c(0, 0) (wave_end.cpp:52's hook; the
// world's WaveEndLinks needs a save_lastgame slot that calls this, the fixer's request).
void program_save_lastgame();
// The directory records' sync at Navigation's open and at Enter (save::sync_to_nav,
// save::sync_from_nav on the program's World and nav::records()).
void sync_records_to_nav();
void sync_records_from_nav();
// A new game (New Game OK): the granularity back to 32 (ADR 0014 decision 1).
void save_new_game();
// briefing::high_score_links() bound to the score list, the .ded save and the table.
void install_high_scores();
}  // namespace is::dialogs
#endif
