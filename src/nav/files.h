// The file pane, the icon count, the preview choice and the status line of the Navigation
// window (docs/re/nav.md "File pane and icons", "Status line", "Built-in levels"): the
// selection handler seg6:0x5524 with the file walk seg6:0x420a and the icon routine
// seg6:0x4368. Portable: the icon counts come through IconCounter (the port's own reader, ADR
// 0012 decision 5), the strings through Strings, the filesystem through the Walker.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "nav/fs.h"
#include "nav/tree.h"

namespace is::nav {

// What SHELL.ExtractIcon tells the original about a file [BYTES seg6:0x442c seg6:0x4469].
class IconCounter {
public:
    virtual ~IconCounter() = default;
    // ExtractIcon(path, -1): the number of RT_GROUP_ICON resources, 0 for a file that is not a
    // valid NE or PE module [DESIGN ADR 0012 decision 5].
    virtual int count(const std::string& path) = 0;
    // ExtractIcon(path, 0) returns a handle above 1: the file has an icon to draw.
    virtual bool has_icon(const std::string& path) { return count(path) > 0; }
};

// An in-memory IconCounter for tests: counts by DOS path (case-insensitive), 0 when absent.
class MemIcons : public IconCounter {
public:
    void set(const std::string& path, int count) { counts_[upper(path)] = count; }
    int count(const std::string& path) override;

private:
    std::map<std::string, int> counts_;
};

// The 32x32 preview bitmap DGROUP:0x94ba after a selection [BYTES seg6:0x444e-0x4564].
struct Preview {
    enum class Kind {
        kBlack,        // PatBlt BLACKNESS only
        kIcon,         // ExtractIcon(path, 0) drawn with DrawIcon at (0, 0) over black
        kPlaceholder,  // the program-name placeholder seg9:0x352(dc, 0, 0, 32, 0, name) over black
        kBuiltin,      // LoadIcon(ISPACE.DLL, icon_id) drawn over black
    };
    Kind kind = Kind::kBlack;
    std::string path;  // kIcon: the file's path
    std::string name;  // kPlaceholder: the file's name as found
    int icon_id = 0;   // kBuiltin: ISPACE.DLL GROUP_ICON 105 'S', 106 'R', 107 'D'
    bool operator==(const Preview& o) const {
        return kind == o.kind && path == o.path && name == o.name && icon_id == o.icon_id;
    }
};

// The built-in rows' icon keys and the ISPACE.DLL GROUP_ICON ids seg6:0x43d0-0x43e8 maps them
// to [BYTES seg6:0x43d8] [BYTES ISPACE.DLL GROUP_ICON 105].
int builtin_icon_id(char key);

// What one selection of an ordinary directory lists.
struct FilePane {
    // The items seg6:0x420a LB_ADDSTRINGs into listbox 0x6b (sorted by the listbox, LBS_SORT):
    // "" + name + " " + "0" (DGROUP:0xec2, 0xec3, 0xec5), in find order [BYTES seg6:0x420a
    // seg6:0x426a].
    std::vector<std::string> items;
    std::vector<std::string> paths;  // each listed file's path, as ExtractIcon gets it
    int exes = 0;                    // seg6:0x420a(*.exe)'s count
    int dlls = 0;                    // seg6:0x420a(*.dll)'s count
    // DGROUP:0x94a6: the sum of max(1, ExtractIcon(file, -1)) plus the trailing query, a
    // 16-bit int [BYTES seg6:0x4441 seg6:0x444a seg6:0x589b].
    int icons = 0;
    Preview preview;
};

// seg6:0x5524's walk of an ordinary directory dir (DGROUP:0x6990, with its trailing '\'):
// seg6:0x420a(dir, "*.exe"), seg6:0x420a(dir, "*.dll") with find-first attribute 0, each file
// through seg6:0x4368(dir, name), then seg6:0x4368("", "") [BYTES seg6:0x5877 seg6:0x5889
// seg6:0x589b]. The trailing query ExtractIcon("\", -1) is logged and adds 0 [DESIGN ADR 0012
// decision 5] [ORACLE wave-0001].
FilePane walk_files(Walker& w, IconCounter& icons, const std::string& dir);
// seg6:0x4368(key, "") for a built-in row: ExtractIcon("S\" | "R\" | "D\", -1) logged, adding 0
// [DESIGN ADR 0012 decision 5], and the LoadIcon preview [BYTES seg6:0x4473-0x4498].
FilePane builtin_pane(Walker& w, char key);

// ---- the status line

// The string ids [BYTES ENG_LANG.DLL STRING 48] ids 761-767 [BYTES ENG_LANG.DLL STRING 59] ids
// 938-943 [BYTES ENG_LANG.DLL STRING 129] ids 2055-2056 [BYTES ENG_LANG.DLL STRING 146] id 2323
// [BYTES ENG_LANG.DLL STRING 157] id 2506 [BYTES ENG_LANG.DLL STRING 158] id 2515 [BYTES
// ENG_LANG.DLL STRING 190] ids 3028-3029.
constexpr int kStrGateway = 761, kStrGatewayEnter = 762, kStrRaceTrack = 763, kStrRaceEnter = 764,
              kStrComplete = 765, kStrDuelArena = 766, kStrDuelEnter = 767, kStrNoApps = 938,
              kStrSelectAnother = 939, kStrFile = 941, kStrInThisDir = 942, kStrNotBeen = 943,
              kStrThereIs = 2055, kStrThereAre = 2056, kStrPayFull = 2323, kStrIconWas = 2506,
              kStrIcon = 2515, kStrIconsInFiles = 3028, kStrCapturedDestroyed = 3029;

// The %? and %# directives of the formatter seg10:0xa332 that the status line uses, over int
// arguments in order:
//   - %# takes (n, id): n in decimal (seg6:0xf1a), a space and string id for n = 1, else
//     id + 10000 [BYTES seg10:0xa558 seg10:0xa573 seg10:0xa5d5 seg10:0xa5e7];
//   - %? takes (flag, id0, id1): string id0 when flag is 0, else id1 [ORACLE newgame-0001:
//     "There are 3 icons" with flag 0 and 2056, "There is 1 icon" with flag 1 and 2055]
//     [OPEN: the directive's code in seg10:0xa332].
// Other text is copied as it is.
std::string format(Strings& s, const std::string& fmt, const std::vector<int>& args);

// seg11:0x3fd4 mode 3 as seg6:0x5bc3 cases the status text before drawing it [BYTES
// seg11:0x3fd4 seg6:0x5bc3].
std::string status_display(const std::string& raw);

// seg6:0x5524's classification of DGROUP:0x6990 without its last character, by case-sensitive
// ends-with, testing 760, then 739, then 736 [BYTES seg6:0x5524 seg6:0x55bd seg16:0x5f0e].
enum class RowKind { kDirectory, kDemonsGate, kRace, kDuel };
RowKind classify(const std::string& dir, Strings& s);

enum class EnterButton { kEnabled, kDisabled, kUnchanged };

// The result of one seg6:0x5524 call.
struct Selected {
    RowKind kind = RowKind::kDirectory;
    int box = 0;          // seg6:0x2dba(kind): 0 none, 1 RACE, 2 DUEL [BYTES seg6:0x55df seg6:0x56e9 seg6:0x57c6]
    FilePane pane;        // the file list, icon count and preview
    std::string status;   // DGROUP:0x6a24 as stored (status_display() cases it for drawing)
    EnterButton enter = EnterButton::kEnabled;  // EnableWindow(Enter id 1, ...)
    int record = -1;      // the matching directory record, -1 for none
    // The record's +0x1d0 was set to 2 here: the window rewrites the selected row's state
    // digit to '2' (Tree::set_state) [BYTES seg6:0x59b6-0x5a51].
    bool completed = false;
};

// seg6:0x5524 for DGROUP:0x6990 = dir. `sub_digit` is seg6:0x5260 on the selected row
// (Tree::sub_digit), read only when the directory holds no *.exe. A visited directory that is
// complete gets +0x1d0 = 2 in `records` [BYTES seg6:0x5524 seg6:0x58a1-0x596e seg6:0x598c-0x5b20].
Selected select(Walker& w, IconCounter& icons, Strings& s, Records& records, const Game& g,
                const std::string& dir, int sub_digit);

}  // namespace is::nav
