// The Navigation tree model (src/nav/tree.h) against docs/re/nav.md "Tree walk", "Built-in
// levels" and "Directory records", the rows of newgame-0002 and its int 21h trace on fixture
// `basic` (work/oracle/newgame-0002/trace.log, lines 210473-212171 and 219573-220260).
#include "doctest.h"
#include "nav_fixture.h"

using namespace is::nav;
using navtest::lines;

namespace {

struct World {
    MemFs fs;
    navtest::FakeStrings s;
    Records records;
    std::vector<FsCall> calls;
    Walker w{fs};
    World() {
        navtest::build_basic(fs);
        w.record(&calls);
    }
};

}  // namespace

TEST_CASE("nav tree: the row suffix seg6:0x4118") {
    CHECK(row_text("GAMES", 1, 0, 0) == "GAMES100");
    CHECK(row_text(" D:\\", 1, 1, 2) == " D:\\112");
    CHECK(row_text("B", 3, 0, 0) == "B300");
    const RowDigits d = parse_row("DEMON'S GATE103");
    CHECK(d.name == "DEMON'S GATE");
    CHECK(d.depth == 1);
    CHECK(d.sub == 0);
    CHECK(d.state == 3);
}

TEST_CASE("nav tree: the fill of fixture basic in a player game (newgame-0002)") {
    World x;
    Tree t(x.w, x.s, x.records, Game{0, true, 0});
    t.fill("D:\\");
    // listbox order: the root first (leading space), then sorted by the whole text
    CHECK(t.rows() == std::vector<std::string>{
                          " D:\\112", "DEEP110", "DEMON'S GATE103", "Dodge City - DUEL103",
                          "EMPTY100", "GAMES100", "Grand Prix - RACE103", "Lazy Loop - RACE100",
                          "SDi Speedway - RACE103", "TOOLS100"});
    CHECK(t.count() == 10);
    CHECK_FALSE(t.all_complete());  // DGROUP:0x920c = 0
    // the calls, as the oracle's trace shows them
    CHECK(lines(x.calls) == std::vector<std::string>{
                                R"(seg6:0x4190 findfirst "D:\*.exe" 0x00 -> end)",
                                R"(seg6:0x4748 findfirst "D:\*.*" 0x15 -> "DEEP")",
                                R"(seg6:0x4650 findfirst "D:\*.*" 0x15 -> "DEEP")",
                                R"(seg6:0x4190 findfirst "D:\DEEP\*.exe" 0x00 -> end)",
                                R"(seg6:0x4650 findnext -> "EMPTY")",
                                R"(seg6:0x4190 findfirst "D:\EMPTY\*.exe" 0x00 -> end)",
                                R"(seg6:0x4650 findnext -> "GAMES")",
                                R"(seg6:0x4190 findfirst "D:\GAMES\*.exe" 0x00 -> "ALPHA.EXE")",
                                R"(seg6:0x37ca findfirst "D:\*.*" 0x15 -> "DEEP")",
                                R"(seg6:0x4748 findfirst "D:\DEEP\*.*" 0x15 -> ".")",
                                R"(seg6:0x4748 findnext -> "..")",
                                R"(seg6:0x4748 findnext -> "A")",
                                R"(seg6:0x37ca findnext -> "EMPTY")",
                                R"(seg6:0x4748 findfirst "D:\EMPTY\*.*" 0x15 -> ".")",
                                R"(seg6:0x4748 findnext -> "..")",
                                "seg6:0x4748 findnext -> end",
                                R"(seg6:0x37ca findnext -> "GAMES")",
                                R"(seg6:0x4748 findfirst "D:\GAMES\*.*" 0x15 -> ".")",
                                R"(seg6:0x4748 findnext -> "..")",
                                R"(seg6:0x4748 findnext -> "ALPHA.EXE")",
                                R"(seg6:0x4748 findnext -> "BRAVO.EXE")",
                                R"(seg6:0x4748 findnext -> "CHARLIE.EXE")",
                                "seg6:0x4748 findnext -> end",
                                R"(seg6:0x37ca findnext -> "TOOLS")",
                                R"(seg6:0x4748 findfirst "D:\TOOLS\*.*" 0x15 -> ".")",
                                R"(seg6:0x4748 findnext -> "..")",
                                R"(seg6:0x4748 findnext -> "DELTA.EXE")",
                                R"(seg6:0x4748 findnext -> "ECHO.EXE")",
                                "seg6:0x4748 findnext -> end",
                                "seg6:0x37ca findnext -> end",
                            });
    const auto edits = t.take_edits();
    REQUIRE(edits.size() == 11);
    CHECK(edits[0].op == ListEdit::Op::kReset);
    CHECK(edits[1] == ListEdit{ListEdit::Op::kInsert, 0, " D:\\112"});
    CHECK(edits[2] == ListEdit{ListEdit::Op::kInsert, 1, "DEEP110"});
    CHECK(t.take_edits().empty());
}

TEST_CASE("nav tree: the built-in rows' lock rules by edition") {
    World x;
    SUBCASE("registered (-1): nothing locked, all-complete flag kept while every row is complete") {
        Tree t(x.w, x.s, x.records, Game{-1, true, 0});
        t.fill("D:\\");
        CHECK(t.rows()[2] == "DEMON'S GATE100");
        CHECK(t.rows()[3] == "Dodge City - DUEL100");
        CHECK(t.rows()[6] == "Grand Prix - RACE100");
        CHECK_FALSE(t.all_complete());  // the root is complete (no *.exe), the others are new
    }
    SUBCASE("edition 3: one race, Demon's Gate locked, the duel open") {
        Tree t(x.w, x.s, x.records, Game{3, true, 0});
        t.fill("D:\\");
        CHECK(t.rows() == std::vector<std::string>{" D:\\112", "DEEP110", "DEMON'S GATE103",
                                                   "Dodge City - DUEL100", "EMPTY100", "GAMES100",
                                                   "Lazy Loop - RACE100", "TOOLS100"});
        CHECK(t.count() == 8);
    }
    SUBCASE("edition 0 in the demo: only Demon's Gate locked") {
        Tree t(x.w, x.s, x.records, Game{0, false, 0});
        t.fill("D:\\");
        CHECK(t.rows()[2] == "DEMON'S GATE103");
        CHECK(t.rows()[3] == "Dodge City - DUEL100");
        CHECK(t.rows()[6] == "Grand Prix - RACE100");
        CHECK(t.rows()[8] == "SDi Speedway - RACE100");
    }
    SUBCASE("edition 2 with every directory complete keeps DGROUP:0x94b2") {
        MemFs fs;
        fs.add_drive('E');
        fs.add_dir("E:\\GAMES");
        fs.add_file("E:\\GAMES\\X.EXE");
        x.records.enter("E:\\GAMES\\");
        x.records.list[0].state = 2;
        Walker w(fs);
        Tree t(w, x.s, x.records, Game{2, true, 0});
        t.fill("E:\\");
        CHECK(t.rows()[3] == "GAMES102");  // after DEMON'S GATE and Dodge City
        CHECK(t.all_complete());
    }
}

TEST_CASE("nav tree: INI Tree = 1 filters the top-level rows through seg6:0x45be") {
    World x;
    Tree t(x.w, x.s, x.records, Game{0, true, 1});
    t.fill("D:\\");
    // DEEP fails (its subdirectory A holds no *.exe), EMPTY fails, GAMES and TOOLS pass
    CHECK(t.rows() == std::vector<std::string>{" D:\\112", "DEMON'S GATE103", "Dodge City - DUEL103",
                                               "GAMES100", "Grand Prix - RACE103", "Lazy Loop - RACE100",
                                               "SDi Speedway - RACE103", "TOOLS100"});
}

TEST_CASE("nav tree: expand and collapse (newgame-0002 expands deep, then a)") {
    World x;
    Tree t(x.w, x.s, x.records, Game{0, true, 0});
    t.fill("D:\\");
    t.take_edits();
    x.calls.clear();
    CHECK(t.path(1) == "D:\\DEEP\\");
    CHECK(t.sub_digit(1) == 1);
    CHECK(t.sub_digit(0) == 10);  // ':' - '0': the root never expands
    CHECK(t.expand(1));
    CHECK(lines(x.calls) == std::vector<std::string>{
                                R"(seg6:0x4f46 findfirst "D:\DEEP\*.*" 0x15 -> ".")",
                                R"(seg6:0x4f46 findnext -> "..")",
                                R"(seg6:0x4f46 findnext -> "A")",
                                R"(seg6:0x4190 findfirst "D:\DEEP\A\*.exe" 0x00 -> end)",
                                R"(seg6:0x4748 findfirst "D:\DEEP\A\*.*" 0x15 -> ".")",
                                R"(seg6:0x4748 findnext -> "..")",
                                R"(seg6:0x4748 findnext -> "B")",
                                R"(seg6:0x4650 findfirst "D:\DEEP\A\*.*" 0x15 -> ".")",
                                R"(seg6:0x4650 findnext -> "..")",
                                R"(seg6:0x4650 findnext -> "B")",
                                R"(seg6:0x4190 findfirst "D:\DEEP\A\B\*.exe" 0x00 -> "FOXTROT.EXE")",
                                R"(seg6:0x4748 findfirst "D:\DEEP\A\*.*" 0x15 -> ".")",
                                R"(seg6:0x4748 findnext -> "..")",
                                R"(seg6:0x4748 findnext -> "B")",
                                R"(seg6:0x4650 findfirst "D:\DEEP\A\*.*" 0x15 -> ".")",
                                R"(seg6:0x4650 findnext -> "..")",
                                R"(seg6:0x4650 findnext -> "B")",
                                R"(seg6:0x4190 findfirst "D:\DEEP\A\B\*.exe" 0x00 -> "FOXTROT.EXE")",
                                "seg6:0x4f46 findnext -> end",
                            });
    CHECK(t.rows()[1] == "DEEP120");
    CHECK(t.rows()[2] == "A210");
    CHECK(t.path(2) == "D:\\DEEP\\A\\");
    const auto e1 = t.take_edits();
    REQUIRE(e1.size() == 3);
    CHECK(e1[0] == ListEdit{ListEdit::Op::kDelete, 1, {}});
    CHECK(e1[1] == ListEdit{ListEdit::Op::kInsert, 1, "DEEP120"});
    CHECK(e1[2] == ListEdit{ListEdit::Op::kInsert, 2, "A210"});

    x.calls.clear();
    CHECK(t.expand(2));
    CHECK(lines(x.calls) == std::vector<std::string>{
                                R"(seg6:0x4f46 findfirst "D:\DEEP\A\*.*" 0x15 -> ".")",
                                R"(seg6:0x4f46 findnext -> "..")",
                                R"(seg6:0x4f46 findnext -> "B")",
                                R"(seg6:0x4190 findfirst "D:\DEEP\A\B\*.exe" 0x00 -> "FOXTROT.EXE")",
                                R"(seg6:0x4748 findfirst "D:\DEEP\A\B\*.*" 0x15 -> ".")",
                                R"(seg6:0x4748 findnext -> "..")",
                                R"(seg6:0x4748 findnext -> "FOXTROT.EXE")",
                                "seg6:0x4748 findnext -> end",
                                "seg6:0x4f46 findnext -> end",
                            });
    CHECK(t.rows()[2] == "A220");
    CHECK(t.rows()[3] == "B300");
    CHECK(t.path(3) == "D:\\DEEP\\A\\B\\");
    CHECK(t.path(4) == "D:\\DEMON'S GATE\\");
    CHECK(t.path(0) == "D:\\");
    CHECK(t.count() == 12);

    // collapse deep: its descendants go, the count stays
    t.collapse(1);
    CHECK(t.rows()[1] == "DEEP110");
    CHECK(t.rows()[2] == "DEMON'S GATE103");
    CHECK(t.rows().size() == 10);
    CHECK(t.count() == 12);
}

TEST_CASE("nav tree: siblings enter in the reverse of the find order") {
    MemFs fs;
    fs.add_drive('E');
    fs.add_dir("E:\\TOP");
    for (const char* n : {"ONE", "TWO", "THREE"}) {
        fs.add_dir(std::string("E:\\TOP\\") + n);
        fs.add_file(std::string("E:\\TOP\\") + n + "\\P.EXE");
    }
    navtest::FakeStrings s;
    Records r;
    Walker w(fs);
    Tree t(w, s, r, Game{-1, true, 0});
    t.fill("E:\\");
    const int top = t.find_exact(-1, "TOP110");
    REQUIRE(top == static_cast<int>(t.rows().size()) - 1);  // sorted after the built-in rows
    CHECK(t.expand(top));
    CHECK(t.rows()[top + 1] == "THREE200");
    CHECK(t.rows()[top + 2] == "TWO200");
    CHECK(t.rows()[top + 3] == "ONE200");
    CHECK(t.path(top + 3) == "E:\\TOP\\ONE\\");
}

TEST_CASE("nav tree: the state digit seg6:0x3f4c and the records") {
    navtest::FakeStrings s;
    Records r;
    const Game player{0, true, 0};
    CHECK(state_digit("D:\\GAMES", player, r, s) == kStateNew);
    bool added = false;
    CHECK(r.enter("D:\\GAMES\\", &added) == 0);
    CHECK(added);
    CHECK(r.current == 0);
    CHECK(state_digit("D:\\GAMES", player, r, s) == kStateVisited);
    CHECK(state_digit("D:\\games", player, r, s) == kStateNew);  // lstrcmp is case-sensitive
    r.list[0].state = 2;
    CHECK(state_digit("D:\\GAMES\\", player, r, s) == kStateComplete);
    CHECK(r.enter("D:\\TOOLS\\", &added) == 1);
    CHECK(r.enter("D:\\GAMES\\", &added) == 0);
    CHECK_FALSE(added);
    CHECK(r.current == 0);
    // the races and the duel, in a player game of edition 0
    CHECK(state_digit("D \\Grand Prix - RACE\\", player, r, s) == kStateLocked);
    CHECK(state_digit("D \\Lazy Loop - RACE\\", player, r, s) == kStateNew);
    CHECK(state_digit("D \\Dodge City - DUEL", player, r, s) == kStateLocked);
    CHECK(state_digit("D \\DEMON'S GATE\\", player, r, s) == kStateLocked);
    // in the demo or a registered game they are open; Demon's Gate needs edition -1 or 2
    CHECK(state_digit("D \\Grand Prix - RACE\\", Game{0, false, 0}, r, s) == kStateNew);
    CHECK(state_digit("D \\DEMON'S GATE\\", Game{0, false, 0}, r, s) == kStateLocked);
    CHECK(state_digit("D \\DEMON'S GATE\\", Game{-1, true, 0}, r, s) == kStateNew);
    CHECK(state_digit("D \\DEMON'S GATE\\", Game{2, true, 0}, r, s) == kStateNew);
    CHECK(state_digit("D \\Grand Prix - RACE\\", Game{-1, true, 0}, r, s) == kStateNew);
    // ends-with is case-sensitive
    CHECK(state_digit("D \\GRAND PRIX - race\\", player, r, s) == kStateNew);
    // 98 records fit
    Records full;
    for (int i = 0; i < Records::kMax; ++i) CHECK(full.enter("D:\\D" + std::to_string(i) + "\\") == i);
    CHECK(full.enter("D:\\ONE MORE\\", &added) == -1);
    CHECK_FALSE(added);
}

TEST_CASE("nav tree: list searches LB_FINDSTRING and LB_FINDSTRINGEXACT") {
    World x;
    Tree t(x.w, x.s, x.records, Game{0, true, 0});
    t.fill("D:\\");
    CHECK(t.find_exact(-1, "DEEP") == -1);  // the suffix is part of the text
    CHECK(t.find_exact(-1, "deep110") == 1);
    CHECK(t.find_prefix(-1, "games") == 5);
    CHECK(t.find_prefix(5, "G") == 6);   // after the start, wrapping
    CHECK(t.find_prefix(6, "G") == 5);
    CHECK(t.find_prefix(-1, "NOPE") == -1);
}

TEST_CASE("nav tree: the initial selection") {
    World x;
    SUBCASE("a new game selects row 1; the demo row 0") {
        Tree t(x.w, x.s, x.records, Game{0, true, 0});
        t.fill("D:\\");
        const auto sel = initial_selection(t, x.s, x.records, Game{0, true, 0}, {});
        CHECK(sel.index == 1);
        CHECK(sel.dir == "D:\\");
        Tree d(x.w, x.s, x.records, Game{0, false, 0});
        d.fill("D:\\");
        CHECK(initial_selection(d, x.s, x.records, Game{0, false, 0}, {}).index == 0);
    }
    SUBCASE("a later wave reopens the current record's path, expanding it") {
        x.records.enter("D:\\DEEP\\A\\B\\");
        Tree t(x.w, x.s, x.records, Game{0, true, 0});
        t.fill("D:\\");
        InitialInputs in;
        in.wave = 3;
        const auto sel = initial_selection(t, x.s, x.records, Game{0, true, 0}, in);
        CHECK(t.rows()[1] == "DEEP120");
        CHECK(t.rows()[2] == "A220");
        CHECK(t.rows()[3] == "B301");
        CHECK(sel.index == 3);
        CHECK(sel.dir == "D:\\DEEP\\A\\B\\");
    }
    SUBCASE("a one-component path selects without expanding") {
        x.records.enter("D:\\GAMES\\");
        Tree t(x.w, x.s, x.records, Game{0, true, 0});
        t.fill("D:\\");
        InitialInputs in;
        in.wave = 1;
        const auto sel = initial_selection(t, x.s, x.records, Game{0, true, 0}, in);
        CHECK(sel.index == 5);
        CHECK(t.rows()[5] == "GAMES101");
    }
    SUBCASE("a path that matches no row selects row 0 and appends the root") {
        x.records.enter("D:\\GONE\\");
        x.records.enter("D:\\XYZ\\");
        Tree t(x.w, x.s, x.records, Game{0, true, 0});
        t.fill("D:\\");
        InitialInputs in;
        in.wave = 2;
        const auto sel = initial_selection(t, x.s, x.records, Game{0, true, 0}, in);
        CHECK(sel.index == 0);
        CHECK(sel.dir == "D:\\XYZ\\D:\\");
    }
    SUBCASE("Demon's Gate's record (no drive) gets the root in front") {
        x.records.enter("DEMON'S GATE");
        Tree t(x.w, x.s, x.records, Game{-1, true, 0});
        t.fill("D:\\");
        InitialInputs in;
        in.wave = 4;
        const auto sel = initial_selection(t, x.s, x.records, Game{-1, true, 0}, in);
        CHECK(sel.dir == "D:\\DEMON'S GATE");
        CHECK(sel.index == 2);
    }
    SUBCASE("DEMO 521 selects the INI Track race; 522 Dodge City") {
        Tree t(x.w, x.s, x.records, Game{0, false, 0});
        t.fill("D:\\");
        InitialInputs in;
        in.command_line = true;
        in.demo_type = 521;
        in.track = "Grand Prix";
        auto sel = initial_selection(t, x.s, x.records, Game{0, false, 0}, in);
        CHECK(sel.race == 9);
        CHECK(sel.demo_named);
        CHECK(sel.dir == "Grand Prix - RACE");
        CHECK(sel.index == 6);
        in.demo_type = 522;
        sel = initial_selection(t, x.s, x.records, Game{0, false, 0}, in);
        CHECK(sel.duel == 1);
        CHECK(sel.index == 3);
        in.demo_type = 520;
        sel = initial_selection(t, x.s, x.records, Game{0, false, 0}, in);
        CHECK(sel.dir == "D:\\");
        CHECK_FALSE(sel.demo_named);
        CHECK(sel.index == 0);
        in.demo_type = 519;
        in.current = "D:\\TOOLS\\";
        sel = initial_selection(t, x.s, x.records, Game{0, false, 0}, in);
        CHECK(sel.dir == "D:\\TOOLS\\");
        CHECK(sel.index == 9);
    }
}
