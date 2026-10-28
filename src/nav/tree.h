// The directory tree of the Navigation window (docs/re/nav.md "Tree walk", "Built-in levels",
// "Directory records"): the model of listbox 0x65 that seg6:0x37ca fills, seg6:0x4f46 expands
// and seg6:0x511c collapses, the row text and its three-digit suffix, the built-in rows and
// their lock rules, the state digit seg6:0x3f4c, the directory records and the initial
// selection. Portable; the window (src/nav/nav.cpp) mirrors the rows into the real listbox.
//
// The model is the listbox: rows() holds the item texts in listbox order, and every change is
// also queued as a ListEdit (take_edits()) at the index the original's LB_ADDSTRING (sorted) or
// LB_INSERTSTRING put it, so the window can replay the edits with LB_INSERTSTRING and
// LB_DELETESTRING [DESIGN].
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "nav/fs.h"

namespace is::nav {

// ENG_LANG.DLL strings by id, cp1252.
class Strings {
public:
    virtual ~Strings() = default;
    virtual std::string lang(int id) = 0;
};
// LoadString(id, buf, max): at most max - 1 bytes [REF Win16 SDK LoadString].
std::string load(Strings& s, int id, int max);

// The string ids of the tree [BYTES ENG_LANG.DLL STRING 47] ids 736-750 [BYTES ENG_LANG.DLL
// STRING 48] id 760.
constexpr int kStrDuelSuffix = 736;  // " - DUEL"
constexpr int kStrRaceSuffix = 739;  // " - RACE"
constexpr int kStrRace0 = 740;       // "Lazy Loop", then 741 "SDi Speedway", 742 "Grand Prix"
constexpr int kStrDuel0 = 750;       // "Dodge City"
constexpr int kStrDemonsGate = 760;  // "DEMON'S GATE"

// What the tree reads from the game.
struct Game {
    int edition = 0;     // DGROUP:0x920c (-1, 0, 2 or 3; shell.md "Edition check")
    bool player = true;  // DGROUP:0x95bc != 0: a player game (0 in the demo)
    int tree_key = 0;    // INI `Tree`, DGROUP:0x94bc, default 0 [BYTES seg6:0x37ef]
};

// ---- rows

// seg6:0x4118(buf, depth, sub, state): name + '0' + depth, '0' + sub, '0' + state, by byte
// arithmetic on the three "0" strings DGROUP:0xeb2, 0xeb4, 0xeb6 [BYTES seg6:0x4118].
std::string row_text(const std::string& name, int depth, int sub, int state);
// The digits back: the last three bytes minus '0'; the name is the text without them.
struct RowDigits {
    std::string name;
    int depth = 0, sub = 0, state = 0;
};
RowDigits parse_row(const std::string& text);

// The sub digit's values: 0 no expandable subdirectory, 1 collapsed with subdirectories,
// 2 expanded [BYTES seg6:0x4f46 seg6:0x511c].
constexpr int kSubNone = 0, kSubCollapsed = 1, kSubExpanded = 2;
// The state digit's values: 0 new, 1 visited, 2 complete, 3 locked [BYTES seg6:0x3f4c].
constexpr int kStateNew = 0, kStateVisited = 1, kStateComplete = 2, kStateLocked = 3;

// ---- directory records (nav.md "Directory records"; formats.md "Directory records")

// The fields of one 0x297-byte record that Navigation reads and writes.
struct DirRecord {
    std::string path;            // +0x152, 110 bytes, DGROUP:0x6990 with its trailing '\'
    std::int16_t destroyed = 0;  // +0x14e, "%# DESTROYED" [BYTES seg6:0x5b20]
    std::int16_t captured = 0;   // +0x150, "%# CAPTURED" [BYTES seg6:0x5b20]
    std::uint8_t state = 0;      // +0x1d0, 2 = complete [BYTES seg6:0x598c]
};

class Records {
public:
    // 0x297-byte records addressed with a 16-bit product: 98 fit [BYTES seg6:0x3775].
    static constexpr int kMax = 98;

    std::vector<DirRecord> list;  // the block behind DGROUP:0x69f4, count DGROUP:0x94a4
    int current = 0;              // DGROUP:0x94a2

    // The first record whose +0x152 equals path (lstrcmp, case-sensitive), or -1 [BYTES
    // seg6:0x36ed seg6:0x40ff].
    int find(const std::string& path) const;
    // seg6:0x36b6, Enter's acceptance: finds or appends the record of DGROUP:0x6990 and makes
    // it current. `added` is DGROUP:0x94b0. A new record copies the path (at most 109 bytes)
    // and has +0x1c0..+0x1c7 = 0xFF (the caller's full record) [BYTES seg6:0x36b6 seg6:0x36e4
    // seg6:0x3760 seg6:0x3788 seg6:0x3791]. At kMax records the port refuses to add one and
    // returns -1, where the original writes past its 64 KB block [DESIGN] [OPEN].
    int enter(const std::string& path, bool* added = nullptr);
};

// seg16:0x5f0e: whether s ends with tail, case-sensitive; a tail longer than s is false
// [BYTES seg16:0x5f0e] [DESIGN: the original has no length guard].
bool ends_with(const std::string& s, const std::string& tail);

// seg6:0x3f4c(path): the state digit of a directory [BYTES seg6:0x3f4c seg6:0x3f8e seg6:0x3ff3
// seg6:0x400f seg6:0x404e seg6:0x406b seg6:0x40ff seg6:0x4107].
int state_digit(const std::string& path, const Game& g, const Records& r, Strings& s);

// ---- the tree

struct ListEdit {
    enum class Op { kReset, kInsert, kDelete };  // LB_RESETCONTENT, LB_INSERTSTRING, LB_DELETESTRING
    Op op = Op::kInsert;
    int index = 0;
    std::string text;  // kInsert only
    bool operator==(const ListEdit& o) const {
        return op == o.op && index == o.index && text == o.text;
    }
};

// The sorted listbox's order (LBS_SORT, no WM_COMPAREITEM): the whole item text, suffix
// included, compared case-insensitively byte by byte [BYTES seg6:0x24d1] [OPEN: Win16 USER's
// exact comparison for apostrophes and hyphens, nav.md "Open questions"].
bool list_less(const std::string& a, const std::string& b);

class Tree {
public:
    Tree(Walker& walker, Strings& strings, const Records& records, Game game);

    // seg6:0x37ca steps 1 to 6: resets the list and fills the root row, the top-level rows and
    // the built-in rows for the drive root DGROUP:0xd18 ("D:\", upper case with '\') [BYTES
    // seg6:0x37ca]. Also clears DGROUP:0x94e and DGROUP:0x950 [BYTES seg6:0x37ca].
    void fill(const std::string& root);

    const std::vector<std::string>& rows() const { return rows_; }
    // DGROUP:0x94b6: incremented for every row added, never decremented by a collapse [BYTES
    // seg6:0x390e seg6:0x50e0 seg6:0x3813].
    int count() const { return count_; }
    // DGROUP:0x94b2: 1 when DGROUP:0x920c is -1 or 2 and no root or top-level row has a
    // state below 2 [BYTES seg6:0x37f7 seg6:0x38c1 seg6:0x39ce].
    bool all_complete() const { return all_complete_; }
    const std::string& root() const { return root_; }

    // seg6:0x5260: the sub digit of a row (row 0 reads ':' - '0' = 10) [BYTES seg6:0x5260].
    int sub_digit(int index) const;
    // The row's name without the suffix (row 0: the drive root) [BYTES seg6:0x5260].
    std::string name(int index) const;
    // seg6:0x5318's path for a row, as it sets DGROUP:0x6990: the drive root for row 0, else
    // the root + the names of the row's ancestors (each the nearest earlier row one level up)
    // + the row's name + '\' [BYTES seg6:0x5318].
    std::string path(int index) const;

    // seg6:0x4f46 on row `index` with DGROUP:0x6990 = dir: rewrites the row's sub digit to
    // '2', enumerates dir + "*.*" attribute 0x15 and inserts each child directory that passes
    // seg6:0x45be at index + 1, so siblings show in the reverse of the find order. True when
    // it added a row [BYTES seg6:0x4f46 seg6:0x4ffe seg6:0x5020 seg6:0x5065 seg6:0x50db].
    bool expand(int index, const std::string& dir);
    bool expand(int index) { return expand(index, path(index)); }
    // seg6:0x511c: rewrites the sub digit to '1' and deletes the following rows while their
    // depth is greater than the row's [BYTES seg6:0x511c seg6:0x5182].
    void collapse(int index);
    // The completion rewrite of seg6:0x5524: the row's last digit becomes '0' + state [BYTES
    // seg6:0x5a32 seg6:0x5a51].
    void set_state(int index, int state);

    // LB_FINDSTRINGEXACT and LB_FINDSTRING (case-insensitive; the search starts after `start`
    // and wraps; -1 searches from the top); -1 when nothing matches [REF Win16 SDK LB_FINDSTRING].
    int find_exact(int start, const std::string& s) const;
    int find_prefix(int start, const std::string& s) const;

    // The edits since the last call, in order.
    std::vector<ListEdit> take_edits();

    // seg6:0x37ca's buffer bp-0x80 as the fill leaves it: the last built-in row's text (the
    // duel row), which the initial selection searches when it skips the path split [BYTES
    // seg6:0x3b04 seg6:0x3e29].
    const std::string& fill_tail() const { return fill_tail_; }

private:
    void add_sorted(const std::string& text);  // LB_ADDSTRING
    void insert(int index, const std::string& text);  // LB_INSERTSTRING
    void erase(int index);  // LB_DELETESTRING
    Walker& w_;
    Strings& s_;
    const Records& r_;
    Game g_;
    std::string root_ = "C:\\";
    std::vector<std::string> rows_;
    std::vector<ListEdit> edits_;
    int count_ = 0;
    bool all_complete_ = false;
    std::string fill_tail_;
};

// ---- the initial selection (nav.md "Initial selection", "DEMO and PLAY command lines")

struct InitialInputs {
    int wave = 0;                  // DGROUP:0x91f8
    bool command_line = false;     // DGROUP:0x90f8 != 0: a DEMO or PLAY command line
    int demo_type = 519;           // DemoType DGROUP:0x95ba
    std::string track;             // INI `Track`, read when DemoType is 521 or 522
    std::string current;           // DGROUP:0x6990 before the call
    bool demo_named = false;       // DGROUP:0x94be before the call
};
struct InitialSelection {
    int index = 0;            // DGROUP:0x94b4, LB_SETCURSEL
    std::string dir;          // DGROUP:0x6990 afterwards
    bool demo_named = false;  // DGROUP:0x94be
    int race = 0;             // DGROUP:0x94e
    int duel = 0;             // DGROUP:0x950
};
// The end of seg6:0x37ca after the fill: picks the row of the current record (expanding the
// rows of its path), the DEMO or PLAY row or the default row 1 (row 0 in the demo) [BYTES
// seg6:0x3be3 seg6:0x3c06 seg6:0x3c43 seg6:0x3c84 seg6:0x3d93 seg6:0x3e17 seg6:0x3e29
// seg6:0x3e6e-0x3f31].
InitialSelection initial_selection(Tree& tree, Strings& s, const Records& r, const Game& g,
                                   const InitialInputs& in);

}  // namespace is::nav
