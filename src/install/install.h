// The port's install step (ADR 0008 decision 1): what INSTALL.EXE of the v1.29.08 game
// does, run from the install set the user names. docs/re/install.md is the specification;
// formats.md "Resources rewritten in place" gives the records and ispace.md ordinal 24 the
// ISPACE.DLL calls the installer makes. Portable: no Win32 calls. The Win32 dialogs and the
// driver are in install_win32.cpp; the questions go through the Ui callbacks below, so the
// whole step runs on the host for the tests.
//
// Entry points [DESIGN] (for ADR 0008):
//   inspace.exe --install <source> [--to <dir>]  runs the step and exits;
//   start-up that finds no asset directory (ADR 0002 decision 1) offers to choose the install
//   set's INSTALL.INI with the Open dialog, runs the step and continues into the game.
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "install/install_ini.h"
#include "shell/rng.h"

namespace is::settings {
class IniFile;
}

namespace is::install {

namespace fs = std::filesystem;

using Log = std::function<void(const std::string& utf8)>;

// ---- the original's texts (cp1252) [BYTES INSTALL.EXE DGROUP addresses as named]

inline constexpr const char* kTitle = "IS Install";  // DGROUP:0x10
// DGROUP:0x7ef, MessageBox 0x30 [BYTES INSTALL.EXE seg2:0x3d2e]
inline constexpr const char* kMsgExitPrograms =
    "Make sure any running programs, (especially email programs) are exited before continuing.";
// DGROUP:0x77c [BYTES INSTALL.EXE seg2:0x3d4c]
inline constexpr const char* kMsgWriteProtected =
    "Installer cannot run if install disk is write-protected. \nClose write-protect tab on back "
    "of disk and run install again.";
// DGROUP:0x8cf, 0x8e7, 0x8fe [BYTES INSTALL.EXE seg2:0x3e79 seg2:0x3e9d seg2:0x3ebd]
inline constexpr const char* kMsgNoDll = "Can't find ISPACE.DLL!!";
inline constexpr const char* kMsgDllFaulty = "ISPACE.DLL is faulty!!";
// DGROUP:0x6b2, MessageBox 0x124 [BYTES INSTALL.EXE seg2:0x2918]
inline constexpr const char* kMsgInstalledBefore =
    "Inner Space has been installed before. Should old preferences be kept? (Answer No if "
    "installing to fix a problem)";
// DGROUP:0x21b, MessageBox 0x10 [BYTES INSTALL.EXE seg2:0xe5c]
inline constexpr const char* kMsgBadPath =
    "The install path you typed in is too long or invalid. \nMake sure directory names are 8 "
    "characters long or shorter and do not include spaces.";
// "<dir> already exists.  Do you wish to install to that directory anyway?" (0x24)
// [BYTES INSTALL.EXE seg2:0x1c75]
inline constexpr const char* kMsgExistsTail =
    " already exists.  Do you wish to install to that directory anyway?";
// The port's own refusal of a game directory it did not install, "<dir>" + this (MB 0x10)
// [DESIGN ADR 0008 decision 1].
inline constexpr const char* kMsgForeignGameTail =
    " holds an Inner Space game that this install did not create. The install does not write "
    "into it; choose another directory.";
// The port INI's [Port] key naming the game directory this step installed [DESIGN].
inline constexpr const char* kInstalledKey = "Installed";
// (0x24) [BYTES INSTALL.EXE seg2:0xfaa]
inline constexpr const char* kMsgScreenSaver =
    "Do you wish Inner Space to become the active screen saver?";
// Dialog 100 control 130 [BYTES INSTALL.EXE seg2:0x1627 seg2:0x165f seg2:0x1673 DGROUP:0x40f]
inline constexpr const char* kDoneCancelled = "Installation Cancelled.";
inline constexpr const char* kDoneValid = "Inner Space is now Validated and Installed!";
inline constexpr const char* kDoneDemo = "Inner Space Demo Installation Complete!";
// Dialog 300's captions [BYTES INSTALL.EXE seg2:0x76a seg2:0x793]
inline constexpr const char* kCaptionDemo = "Inner Space Demo Install";
inline constexpr const char* kCaptionReinstall = "Inner Space Valid ReInstall";
// The default directory when the set names none [BYTES INSTALL.EXE seg2:0x462d]
inline constexpr const char* kDefaultDir = "C:\\SPACE\\";

// MessageBox results and flags the Ui sees (winuser.h values).
constexpr int kIdOk = 1, kIdCancel = 2, kIdYes = 6, kIdNo = 7;
constexpr unsigned kMbYesNoQuestionDefNo = 0x124, kMbYesNoQuestion = 0x24, kMbExclamation = 0x30,
                   kMbStop = 0x10;

// ---- the install set

// Which rule the set's installer applies (ADR 0008 decision 1 "Which rule applies").
enum class Rule {
    kRegister,  // INSTALL.EXE of v1.29.08: ordinal 24 command 0 [BYTES INSTALL.EXE seg2:0x43da]
    kTrial,     // anything else: ordinal 10 command 9, seg10:0x5b24(0x54, day)
};

struct SourceSet {
    fs::path given;  // what the user named
    fs::path dir;    // the port's working copy, which the step reads (and writes SECURE.DAT in)
    InstallIni ini;
    std::string label;  // version_label(ini.v), "V1.29.08"
    // The set's files by name_key (case-insensitive, ADR 0002 decision 3).
    std::map<fs::path::string_type, fs::path> files;
    // INSTALL.EXE #5500 120 (0x78 bytes read) and #5500 110 (0x10 bytes read): empty when the
    // module or the resource is missing [BYTES ISPACE.DLL seg10:0x5416 seg10:0x546c seg10:0x558a].
    std::vector<std::uint8_t> rec120, rec110;
    bool new_install = true;  // both records zero: ordinal 24 command 1 answers 0
    Rule rule = Rule::kTrial;
    std::string rule_why;  // which installer was recognised, for the log

    // The set's file with this name (cp1252, case-insensitive), or nullptr.
    const fs::path* find(const std::string& name) const;
};

struct OpenResult {
    std::optional<SourceSet> set;
    // When `set` is empty: the original's message box text (cp1252), or empty when the
    // original exits without a message (a [Vers] V of 0) [BYTES INSTALL.EXE seg2:0x3d28].
    std::string message;
};

// Start-up and validation (install.md "Start-up" steps 2-4 and "Validation"). `given` is a
// directory holding INSTALL.INI, or that INSTALL.INI itself. The set is copied into
// `work_root`/<its directory name> first (the port never writes into the user's files; the
// original's write test then creates SECURE.DAT in that copy) [DESIGN]. `given` may also be a
// .zip install set: it is unpacked with unpack_zip and the step goes on from the unpacked
// directory (ADR 0008 decision 1) [DESIGN].
OpenResult open_source(const fs::path& given, const fs::path& work_root, const Log& log);

// Unpacks the zip install set `zip` into `work_root`/<its file name without .zip> (emptied
// first) and returns the directory holding its INSTALL.INI: that directory, or the one
// subdirectory that holds it. Empty with *why (a message for the user) when the zip cannot be
// read, names a path outside the target, or holds no INSTALL.INI. The files keep the names
// the zip stores; their times are the unpack time, as a directory set's copy has [DESIGN].
std::optional<fs::path> unpack_zip(const fs::path& zip, const fs::path& work_root, std::string* why);

// ---- dialog 300 (install.md "Dialogs")

// Boxes of dialog 300 in group order 1-6: 119 Inner Space, 124 Ship Factory, 125 Sound
// Effects, 135 Voice Effects, 306 Music Soundtrack, 304 Screen Savers [BYTES INSTALL.EXE
// seg2:0xe7d-0xef8].
constexpr std::array<int, 6> kBoxIds = {119, 124, 125, 135, 306, 304};
// Their size labels (controls 127, 128, 129, 130, 308, 305) and sizes in KB [BYTES INSTALL.EXE
// seg2:0x7c7-0x8d8].
constexpr std::array<int, 6> kSizeIds = {127, 128, 129, 130, 308, 305};
constexpr std::array<int, 6> kSizesKb = {771, 37, 316, 336, 239, 21};
// Required = 46 + the checked boxes' sizes [BYTES INSTALL.EXE seg2:0xb6a].
constexpr int kBaseKb = 46;

struct Components {
    std::string caption;           // dialog 300's caption; empty keeps the template's
    std::string dir = kDefaultDir;  // edit 301 (cp1252)
    std::array<bool, 6> checked = {true, true, true, true, true, true};
    std::array<bool, 6> enabled = {true, true, true, true, true, true};
    bool factory_hidden = false;  // FACTORY.EX_ missing hides 124 and 128
};

// The init's checks of the source files [BYTES INSTALL.EXE seg2:0x955 seg2:0x993 seg2:0x9c1
// seg2:0x9ef seg2:0xa1b]: FACTORY.EX_ unchecks and hides 124, INSPACE.SC1 unchecks and
// disables 304, MAIN.MI1 306, SND_FX.DLL 125 and ENG_FX.DLL 135.
Components initial_components(const SourceSet& set);
int required_kb(const std::array<bool, 6>& checked);
// The text of edit 301 on OK [BYTES INSTALL.EXE seg2:0xe09 seg2:0xe2a]: a one-character text
// gets ':', and a missing trailing separator is added ('\\'; a text that already ends in '/'
// keeps it, for the host).
std::string complete_dir(std::string typed);
// INSTALL.EXE seg2:0x38ee's test of a typed directory, on the port's file system: valid when it
// exists or can be created and its absolute form names the same directory. A directory the test
// created is removed again, as the original's rmdir does [DESIGN: Win32 accepts long names].
bool dir_valid(const fs::path& dir);
// A directory string (cp1252, as edit 301 holds it) as a path, and back as the INI holds it
// (cp1252, backslashes, trailing backslash).
fs::path dir_path(const std::string& cp1252);
std::string ini_dir(const fs::path& dir);

// ---- the copy (install.md "INSTALL.INI")

struct CopyItem {
    int group = 0;
    std::size_t index = 0;  // key index within the group
    std::string key;        // the INI's name, which is the target's name
    fs::path source;        // empty when skipped or missing
    bool lz = false;        // read through LZOpenFile (the '1' probe hit): SZDD expands
    fs::path target;
    bool skip = false;      // INSTALL.EXE DGROUP:0x13f8
};

// INSTALL.EXE seg2:0x36c2's source for key K: K with its last character '1', then '_', then K
// itself; lz is set by the '1' probe alone.
std::optional<std::pair<fs::path, bool>> source_for(const SourceSet& set, const std::string& key);

// Reads a source as the copy does: through LZ (an SZDD stream expands; any other file reads as
// it is) or raw [BYTES INSTALL.EXE seg2:0x379b seg2:0x37ae seg2:0x37be].
std::vector<std::uint8_t> read_source(const fs::path& path, bool lz);

// ---- dialogs 1922 and 1774 (install.md "Dialogs")

// Radio 142-146 (difficulty) and 137-140 (speed): Complex = 0-3 for 142-145, else 4; Braking
// = 0 only for 142 and 143; FrameRateSet = 20, 36, 54, else 72 [BYTES INSTALL.EXE seg2:0x358f
// seg2:0x35bc seg2:0x35c1 seg2:0x362a].
void write_difficulty(settings::IniFile& ini, int difficulty_id, int speed_id);
// Full = 1 when 1775 is checked [BYTES INSTALL.EXE seg2:0x3480].
void write_full(settings::IniFile& ini, bool full_screen);

// ---- dialog 135 and the speed test

// GetWindowText(2, 0x12) and (3, 0x16): at most 17 and 21 bytes [BYTES INSTALL.EXE seg2:0x2aae
// seg2:0x2ac7].
std::string cut_text(const std::string& s, std::size_t nmax);
// "" + Name + " - " + Org, ordinal 24 command 9's a [BYTES INSTALL.EXE seg2:0x2ad7-0x2b06].
std::string name_org(const std::string& name, const std::string& org);
// ThePlayer: Name cut at its first space within 20 bytes [BYTES INSTALL.EXE seg2:0x2b19
// seg2:0x2b37].
std::string the_player(const std::string& name);

// INSTALL.EXE seg2:0x2b53: n = 0; t0 = tick(); do ++n; while (tick() < t0 + 1000) (32-bit
// unsigned) [BYTES INSTALL.EXE seg2:0x2b53-0x2b8a].
std::uint32_t speed_polls(const std::function<std::uint32_t()>& tick);
// Detail = 3 above 350000 polls, 2 above 180000, 1 above 70000, else 0 [BYTES INSTALL.EXE
// seg2:0x2b8c seg2:0x2b9e seg2:0x2bb0].
int detail_from_polls(std::uint32_t polls);

// ---- registration (install.md "Registration", formats.md "Resources rewritten in place")

// ISPACE.DLL seg10:0x4dba(buf, 50, 1) after srand(seed): the 50-byte #5000 143 record of
// `text` (bytes after the text are 0, which the scramble turns into random bytes; the
// original's buffer holds stack bytes there [OPEN INSTALL.EXE seg2:0x2b11]).
std::vector<std::uint8_t> name_record(const std::string& text, shell::Rng& rng, std::uint16_t seed);

// ISPACE.DLL seg10:0x4b1a's record on the NT branch: D = rand() % 7651 + 5432 (seg10:0x4440
// returns 0), the drive word at +4, zero +6..+9 [BYTES ISPACE.DLL seg10:0x4b99-0x4bf1].
std::vector<std::uint8_t> install_record(shell::Rng& rng, int drive);
// x = (D & 0xFFFF) mod 653 + 654 [BYTES ISPACE.DLL seg10:0x4c02-0x4c10].
int record_x(const std::vector<std::uint8_t>& record);

struct RegValues {
    int vidsp = 0, pdev = 0, bit32sp = 0;
};
// ISPACE.DLL seg10:0x5b24(mode, x) for modes 0x17 and 0x54 after srand(seed), with the stored
// values (0 when absent). Returns the values the three keys hold afterwards [BYTES ISPACE.DLL
// seg10:0x5b39-0x5dc3].
RegValues ini_values(int mode, int x, const RegValues& stored, shell::Rng& rng, std::uint16_t seed);

// Writes `bytes` over the start of resource `type` `id` of the NE file `path` in place, as
// ISPACE.DLL seg10:0x4636 does, and restores the file's modification time (the original keeps
// the DOS date and time). False with *why when the resource is missing or shorter.
bool write_resource(const fs::path& path, std::uint16_t type, std::uint16_t id,
                    const std::vector<std::uint8_t>& bytes, std::string* why);

// ---- the whole step

struct Ui {
    // MessageBox(text, kTitle, flags): kIdYes/kIdNo/kIdOk.
    std::function<int(const std::string& text, unsigned flags)> message;
    // Dialog 300: false for Cancel. The callback validates the path itself with dir_valid and
    // kMsgBadPath, as the dialog procedure does.
    std::function<bool(Components&)> components;
    // Dialog 1922: the checked difficulty (142-146) and speed (137-140) radio ids.
    std::function<void(int& difficulty_id, int& speed_id)> difficulty;
    // Dialog 1774: true for Full Screen (1775).
    std::function<bool()> window;
    // Dialog 135: Name and Organization as typed (cp1252).
    std::function<void(std::string& name, std::string& org)> personalize;
    // Dialog 200: control 120 the file name, 130 the percent.
    std::function<void(const std::string& file, int percent)> progress;
    // Dialog 100: control 130's text; the README path the original opens on OK.
    std::function<void(const std::string& text, const fs::path& readme)> done;
    // GetCurrentTime.
    std::function<std::uint32_t()> tick;
    // The local date as struct tm's tm_year and tm_yday, for the trial start; empty reads
    // localtime [DESIGN: injectable for the tests].
    std::function<void(int& tm_year, int& tm_yday)> date;
    Log log;
};

struct Result {
    bool completed = false;  // the copy ran to 100 % and the dialogs were answered
    fs::path game_dir;
    fs::path ini_path;
    Rule rule = Rule::kTrial;
    bool registered = false;  // INSTALL.EXE DGROUP:0x13f6: the ordinal 24 call ran
};
// completed is false also when a record write failed (the step then says so) [DESIGN].

// Runs install.md from the "Installed before" rule to dialog 100. `ini_path` is the port's
// INSPACE.INI for the new game directory (ADR 0004); `default_dir` is edit 301's text
// (kDefaultDir unless --to names one).
Result run_install(const SourceSet& set, const fs::path& ini_path, const std::string& default_dir,
                   Ui& ui);

}  // namespace is::install
