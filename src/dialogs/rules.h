// The portable rules of the game-start and exit dialogs that ISPACE.DLL ordinal 10 opens for
// INSPACE.EXE (docs/re/shell.md "Game-start dialogs" and "Exit", docs/re/ispace.md ordinal 10,
// docs/re/formats.md "Saved games" and "INI files", ADR 0008 decision 3): the list fills, the
// ship availability, the result mapping, the INI writes of OK in their order and the small
// arithmetic of the procedures. No Win32: src/dialogs/dialogs.cpp applies these to the
// controls and the tests check them on the host.
#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "formats/ships.h"
#include "shell/rng.h"
#include "shell/shared_rules.h"

namespace is::dialogs {

// ---- ordinal 10's commands (shell.md "Game-start dialogs", the command table)
enum Command : int {
    kCmdOptions = 0,
    kCmdGameStart = 1,  // Restore Game, then New Game
    kCmdSaveGame = 3,
    kCmdNewDemo = 4,
    kCmdExit = 5,       // the exit dialog from SC_CLOSE
    kCmdAmbulance = 6,
    kCmdAbort = 7,      // the exit dialog from 0x991
    kCmdOrder = 8,
    kCmdImportant = 11,
    kCmdBenefits = 12,
    kCmdPlusPack = 14,
    kCmdDate = 15,
};

// The ENG_LANG.DLL templates [BYTES ENG_LANG.DLL DIALOG].
enum Template : std::uint16_t {
    kDlgSaveGame = 167,      // #00a7
    kDlgExit = 244,          // #00f4
    kDlgRestoreGame = 267,   // #010b
    kDlgNewGame = 321,       // #0141
    kDlgNewDemo = 589,       // #024d
    kDlgOrder = 693,         // #02b5
    kDlgBenefits = 799,      // #031f
    kDlgImportant = 988,     // #03dc
    kDlgPlusPack = 1872,     // #0750
};

// The results of the exit dialog #244 [BYTES ISPACE.DLL seg10:0x105c seg10:0x1066 seg10:0x1078].
constexpr int kExitCancel = 0, kExitExit = 1, kExitSave = 0x251;

// x5 of commands 1, 4 and 6: 1 when DGROUP:0x920c = 0, 3 when it is 3, else 0
// [BYTES seg10:0x836c seg10:0x8372]. ISPACE.DLL keeps it in its DGROUP:0x1aa.
int x5_for_edition(int edition);

// seg10:0x82a2's date number: (tm_year - 93) * 365 + tm_yday as a 16-bit word
// [BYTES seg10:0x8300].
int date_number(int tm_year, int tm_yday);

// ---- Restore Game #267 (ISPACE.DLL seg10:0xec2, init seg10:0x1438)

// " NEW GAME" (leading space) [BYTES ISPACE.DLL seg10:0x1535].
inline constexpr const char* kNewGameEntry = " NEW GAME";

// The saved games the GAME combo adds after " NEW GAME", in find order: each `*.SAV` name
// without its last 4 characters, skipping exactly "DEMOGAME" (lstrcmp) [BYTES ISPACE.DLL
// seg10:0x154f seg10:0x15af seg10:0x15cf seg10:0x15d7]. `found` is the number of *.SAV files
// (DEMOGAME counted), which sizes the combo.
std::vector<std::string> restore_game_names(const std::vector<std::string>& sav_files,
                                            int* found = nullptr);
// The Save Game #167 list: the same cut, skipping "LASTGAME" (DEMOGAME is listed)
// [BYTES ISPACE.DLL seg10:0xc93]. `added` counts the names added.
std::vector<std::string> save_game_names(const std::vector<std::string>& sav_files,
                                         int* added = nullptr);

// CBS_SORT's order as the combo box keeps it (lstrcmpi, stable for equal names). The dialog
// inserts with CB_ADDSTRING and lets the control sort; the tests use this to check the list
// that the user sees [DESIGN].
std::vector<std::string> sorted_like_combo(std::vector<std::string> items);

// A combo sized to h * n + 30, h the height of "Test" in the dialog's normal font
// [BYTES ISPACE.DLL seg10:0x15f9 seg10:0x1638 seg10:0x21fd]; PLAY gets 2 (h + 15)
// [BYTES ISPACE.DLL seg10:0x223c].
int combo_height(int h, int n);
int play_combo_height(int h);

// OK maps entry 0 (NEW GAME) to 1 and any other entry to 2; Cancel ends with 0
// [BYTES ISPACE.DLL seg10:0x1e57 seg10:0x1e62 seg10:0x1e69 seg10:0x1e6e].
int restore_game_result(bool ok, int index);

// The header fields of a .sav that the field fill seg10:0x1a80 reads (formats.md "Header").
struct SaveHeader {
    std::string pilot;           // +0x04, 20 bytes
    std::int32_t score = 0;      // +0x1e
    int ship = 0;                // +0x22, the SHP index (word)
    int wave = 0;                // +0x24 (DGROUP:0x91f8 on restore)
    std::uint8_t preview = 0;    // +0x170, the preview flags for ISPACE.DLL seg10:0x2aac
};
// The bytes seg10:0x1a80 reads [BYTES ISPACE.DLL seg10:0x1b72].
constexpr std::size_t kSaveHeaderRead = 0x1a9;
// Null when fewer than 0x1a9 bytes came (the failed read: MessageBeep and no change).
std::optional<SaveHeader> parse_save_header(const std::vector<std::uint8_t>& bytes);

// What OK writes, in order: Restore = the combo text and, when a save's ship is known
// (ISPACE.DLL DGROUP:0x1f8c >= 0), MainShip = that record's name and ThePlayer = the pilot
// [BYTES ISPACE.DLL seg10:0x1d96 seg10:0x1da7 seg10:0x1dcf seg10:0x1de4].
struct IniWrite {
    std::string key;
    std::string value;  // cp1252
    bool operator==(const IniWrite& o) const { return key == o.key && value == o.value; }
};
std::vector<IniWrite> restore_game_writes(const std::string& combo_text, int ship_index,
                                          const std::string& ship_name,
                                          const std::string& pilot);

// ---- ship availability seg10:0x611c (shell.md "Ship availability")

// The MASTER.BOD bitmap ids (body word +0x21) that the limited version refuses
// [BYTES ISPACE.DLL seg10:0x61da seg10:0x6247].
bool limited_body(int body_word);

// The cache ISPACE.DLL DGROUP:0x1ac after its first computation with CDDrive empty (ADR 0008
// decision 4): 0 when ordinal 24 command 2 (seg10:0x4f9c) answered non-zero, else command 10
// (seg10:0x4c68)'s result [BYTES ISPACE.DLL seg10:0x6160 seg10:0x6163 seg10:0x6175].
int availability_cache(int command2, int command10);

// seg10:0x611c(ship): with the cache non-zero every ship unless x5 (DGROUP:0x1aa) is 3, which
// refuses every ship but 1; with the cache 0, a ship whose body's word +0x21 is in the
// limited list is refused [BYTES ISPACE.DLL seg10:0x619c-0x6247]. A ship or body index outside
// the tables reads as body word 0 [DESIGN].
struct Availability {
    int cache = 0;
    int x5 = 0;
    const fmt::MasterShp* ships = nullptr;
    const fmt::MasterBod* bodies = nullptr;
};
bool ship_allowed(const Availability& a, int ship);

// The SHIP combo's entries in record order (the combo sorts them): every record whose name is
// non-empty, whose team byte +0x03 is not 7 and that the rule allows [BYTES ISPACE.DLL
// seg10:0x2019 seg10:0x2033 seg10:0x203a seg10:0x2047].
std::vector<int> new_game_ships(const Availability& a);
// Restore Game's ship for a save's header word +0x22: 0 when above count - 1, then 1 when the
// rule refuses it [BYTES ISPACE.DLL seg10:0x1c0f seg10:0x1c1b].
int restore_ship_index(int ship_word, const Availability& a);

inline constexpr const char* kTransformer = "TRANSFORMER";
// The ship name the init selects (shell.md "Defaults"): x5 != 0 gives TRANSFORMER without an
// INI write; otherwise a MainShip that names a record (exact compare, first match) whose team
// is 7 or that the rule refuses gives TRANSFORMER with MainShip = TRANSFORMER written.
struct DefaultShip {
    std::string name;
    bool write_main_ship = false;
    int record = -1;  // the exactly matching record for the preview and TEAM, else -1
};
DefaultShip default_ship(const std::string& main_ship, const Availability& a);

// OK's refusal: while x5 != 0 any ship but TRANSFORMER shows strings 2326 + 2327 and keeps the
// dialog open [BYTES ISPACE.DLL seg10:0x2532 seg10:0x2585].
bool limited_ship_refused(int x5, const std::string& ship);

// ---- New Game #321 and New Demo #589 (ISPACE.DLL seg10:0xf26, init seg10:0x1e7c)

// ZOOM LEVEL lists 2x, 1x, 1/2x; the init selects 2 - SetSize and OK writes Size = SetSize =
// 2 - index [BYTES ISPACE.DLL seg10:0x1ff7 seg10:0x2625-0x265b]. An index outside 0-2 selects
// nothing (CB_SETCURSEL fails) [DESIGN: noted, as the control does].
int zoom_index(int set_size);
int size_for_zoom(int index);

// drop_label_dot, label_text and label_as_find_first live in shell/shared_rules.h.
using shell::drop_label_dot;
using shell::label_as_find_first;
using shell::label_text;
// The New Game DRIVE list's label (ISPACE.DLL seg10:0x1e7c): "Unknown_HD" (DGROUP:0x320,
// exact case) only when the find-first fails (null); a found name is kept as found, an empty
// one too; then the '.' rule, which leaves "Unknown_HD" as it is [BYTES ISPACE.DLL seg10:0x2138
// seg10:0x2145 seg10:0x214c seg10:0x2187 seg10:0x2197].
std::string drive_label_text(const std::optional<std::string>& find_first_name);
// "C: " (DGROUP:0x32b) with its letter raised to the drive's, + drive_label_text
// [BYTES ISPACE.DLL seg10:0x216d seg10:0x2178 seg10:0x217a seg10:0x20e1].
std::string drive_entry(char letter, const std::optional<std::string>& find_first_name);
// The selection: the index of the entry whose letter equals Drive's first byte (a
// case-sensitive byte compare, made for every listed drive, so the last equal entry wins),
// else 0 [BYTES ISPACE.DLL seg10:0x20ab seg10:0x2154 seg10:0x2157 seg10:0x215a seg10:0x2164].
int drive_index(const std::vector<std::string>& entries, const std::string& drive_key);
// seg15:0x6558: 'C' + offset, string 937 ':', the label text of drive 'C' + offset.
std::string drive_text(int offset, const std::string& colon,
                       const std::optional<std::string>& find_first_name);

// What OK writes, in order: MainShip and Drive (the first character of the DRIVE entry), and
// for New Game also Size, SetSize, ThePlayer and PlayMode [BYTES ISPACE.DLL seg10:0x259f
// seg10:0x25f7 seg10:0x2646 seg10:0x265b seg10:0x2687 seg10:0x26c8] [ORACLE newgame-0001].
std::vector<IniWrite> new_game_writes(bool demo, const std::string& ship,
                                      const std::string& drive_entry_text, int zoom,
                                      const std::string& pilot, int play);
// After those writes, Command mode with StratAsk 0 or 10 asks 2850-2853 (Yes writes
// StratAsk + 1) [BYTES ISPACE.DLL seg10:0x26ab seg10:0x26cd seg10:0x26f5].
bool strat_ask(int play, int strat_ask_key);

// ---- the exit dialog #244 (ISPACE.DLL seg10:0x103c, init seg10:0x109a)

// Save & Exit is enabled only when x5 != 0 and x4 is 1, 2 or 3 [BYTES ISPACE.DLL seg10:0x11d7
// seg10:0x11f8].
bool save_exit_enabled(int x4, int x5);
// seg2:0xe5d: seg10:0x8568(-1) in state 2, (1) otherwise [BYTES seg2:0xe57 seg2:0xe5b].
int save_exit_argument(int state);
// Exit opens #799 (command 12) when DGROUP:0x920c is 0 or 1 [BYTES seg2:0xe1f seg2:0xe26].
bool benefits_on_exit(int edition);
// seg10:0x87de: acts while DGROUP:0x11aa = 0 and DGROUP:0x920c is -1 or 3; PlusCount + 1,
// above 12 stored as 0 with dialog #1872 (command 14) [BYTES seg10:0x87eb seg10:0x87f2
// seg10:0x880e seg10:0x8816 seg10:0x881f seg10:0x882d seg10:0x8852].
struct PlusCountStep {
    bool acts = false;
    int count = 0;
    bool plus_pack = false;
};
PlusCountStep plus_count_step(int plus, int edition, int plus_count);

// ---- Save Game #167 and seg10:0x8568

// seg10:0x8568(arg): for 1, -1 and 3 the dialog opens only when SavePrompt != 0, or when it
// is 0 and Restore (read into 9 bytes) is empty or has a space at index 3; other arguments
// always open it [BYTES seg10:0x857f-0x85ee].
bool save_dialog_opens(int arg, int save_prompt, const std::string& restore9);
// &Save: up to 8 characters of the combo cut at the first space; empty or "LASTGAME" is
// refused (the field is cleared and the dialog stays) [BYTES ISPACE.DLL seg10:0xdf7
// seg10:0xe38 seg10:0xe63 seg10:0xe72].
std::optional<std::string> save_name(const std::string& combo_text);
// seg10:0x8568 after a non-zero result: the save (seg6:0x885c(1, 0) for -1 and 0, the roster
// update seg6:0x99b6 for 1, 2 and 3), and the main window destroyed for -1 and 1
// [BYTES seg10:0x8632 seg10:0x8636 seg10:0x863d seg10:0x8648].
bool save_destroys_window(int arg);

// ---- Order #693 (ISPACE.DLL seg10:0x664, init seg10:0x0)

// Static 710: 2344 for x4 = 0, 2341 with x4 for x4 > 0, 2342 with -x4 for x4 < 0
// [BYTES ISPACE.DLL seg10:0x153 seg10:0x16b seg10:0x187].
struct DayText {
    int string_id = 0;
    std::optional<int> value;  // the %d argument
};
DayText order_day_text(int x4);
// x4 of command 8: DGROUP:0x9224 when DGROUP:0x922a = 0, else 0 [BYTES seg10:0x8380].
int order_x4(int invalid, int day);
// Static 704 becomes 2545 only when x5 (the date number) < 0x4c0 [BYTES ISPACE.DLL
// seg10:0x1bc]; 694 is hidden when x4 < 3 or x4 > 50 [BYTES ISPACE.DLL seg10:0x222].
bool order_tshirt(int x5);
bool order_phone_hidden(int x4);
// wsprintf of a template text with %d conversions only (static 712's "%d/%d/%d" with tm_mon +
// 1, tm_mday and tm_year) [BYTES ISPACE.DLL seg10:0x259].
std::string format_ints(const std::string& fmt, const std::vector<int>& args);
// The serial record IGRAPHIC.DLL #5500 110 (formats.md "Resources rewritten in place"), as
// ISPACE.DLL seg10:0x5dfe reads it: dword +0 the volume serial (not used by the port), signed
// word +4 the attempt counter, dword +6 the serial [BYTES ISPACE.DLL seg10:0x5e8c seg10:0x5e90
// seg10:0x5ea1]. Null when the resource is shorter than 10 bytes.
struct SerialRecord {
    std::int32_t serial = 0;  // +6, DGROUP:0x154c
    int count = 0;            // +4, signed 16-bit
    bool operator==(const SerialRecord& o) const { return serial == o.serial && count == o.count; }
};
std::optional<SerialRecord> serial_record(const std::vector<std::uint8_t>& resource);
// ISPACE.DLL seg10:0x492 after srand: n = rand() % 10 + 3 rounds, each computing v afresh from
// the 16-bit terms, D * (rand() % 17 + 813) with D the date number (x5 of command 8, ISPACE.DLL
// DGROUP:0x213a) sign-extended, the v < 21400 top-up, labs, the 600000 < v < 700000 fold and
// (rand() % 6) * 100000 [BYTES ISPACE.DLL seg10:0x4ac-0x618]. `r` is ISPACE.DLL's own copy of
// the Microsoft C generator (seg3:0x650 srand, seg3:0x668 rand).
std::int32_t generate_serial(shell::Rng& r, int date_number);
// The same with srand(seed) first; the original seeds with GetCurrentTime's low word
// [BYTES ISPACE.DLL seg10:0x4a0 seg10:0x4a6].
std::int32_t generate_serial(std::uint16_t seed, int date_number);
// seg10:0x5dfe(hDlg, bump) on a record read: a counter above 8 (signed) or a zero serial draws
// a new serial and sets the counter to 0; otherwise a non-zero bump adds 1 to the counter
// (16-bit). `changed` says the record differs from `in` (the original writes it back either
// way) [BYTES ISPACE.DLL seg10:0x5ea1 seg10:0x5ea8 seg10:0x5ec1 seg10:0x5ed0 seg10:0x5edc
// seg10:0x5ef8 seg10:0x5f02].
struct SerialStep {
    SerialRecord record;
    bool regenerated = false;
    bool changed = false;
};
SerialStep serial_step(const SerialRecord& in, bool bump,
                       const std::function<std::int32_t()>& generate);
// ltoa(v, 10) into control 700 [BYTES ISPACE.DLL seg10:0x5f5c seg10:0x62e].
std::string serial_text(std::int32_t v);
// The port's copy of the record (ADR 0008 decision 1: the port never writes a game file): keys
// Serial and SerialCount of the port INI's [Registration] section, read and written only
// there [DESIGN, for ADR 0008].
inline constexpr const char* kSerialKey = "Serial";
inline constexpr const char* kSerialCountKey = "SerialCount";
// seg10:0x80a8 opens Order at most 20 times per run (counter DGROUP:0x921e) [BYTES seg10:0x80b5
// seg10:0x80bc].
constexpr int kOrderLimit = 20;

// ---- Important Information #988 and the ESC rule

// ESC at the title with DGROUP:0x920c = 2 and Accept != 42 opens Important Information
// instead of Restore Game [BYTES seg11:0x7a28 seg11:0x7a2d seg11:0x7a34].
bool needs_important(int edition, int accept);
inline constexpr int kAccepted = 42;

// seg6:0x930a refuses while DGROUP:0x920c = 0 and DGROUP:0x95bc != 0 (string 2325)
// [BYTES seg6:0x9319 seg6:0x9320 seg6:0x9329].
bool restore_refused(int edition, int player);

// ---- common

// The MoveWindow of every procedure: (SM_CXSCREEN / 2 + (left - right) / 2, SM_CYSCREEN / 2 +
// (top - bottom) / 2) with the window size, C division truncating [BYTES ISPACE.DLL
// seg10:0x172d seg10:0x2308].
struct Placement {
    int x = 0, y = 0, w = 0, h = 0;
};
Placement centred(int left, int top, int right, int bottom, int screen_cx, int screen_cy);

// seg10:0x602a: the bold font goes to "Button" children whose style's low byte is
// BS_PUSHBUTTON or BS_DEFPUSHBUTTON, the normal one to every other child [BYTES ISPACE.DLL
// seg10:0x6052 seg10:0x606e seg10:0x607c].
bool bold_child(const std::string& class_name, std::uint32_t style);

// A #5000 text resource as SetWindowText sees it: up to the first NUL.
std::string resource_text(const std::vector<std::uint8_t>& bytes);

// seg10:0x41cc's key: "Team" + itoa(team + 1) in section "Team" + "s" (string 2279 + "s").
std::string team_key(int team);

}  // namespace is::dialogs
