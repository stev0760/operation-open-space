// The Navigation file pane, icon count, preview and status line (src/nav/files.h) against
// docs/re/nav.md "File pane and icons" and "Status line", the status texts of newgame-0001,
// the icon counts of wave-0001 and the int 21h and SHELL.EXTRACTICON trace of newgame-0002
// (work/oracle/newgame-0002/trace.log lines 215709-216031).
#include "doctest.h"
#include "nav_fixture.h"

using namespace is::nav;
using navtest::lines;

namespace {

struct World {
    MemFs fs;
    MemIcons icons;
    navtest::FakeStrings s;
    Records records;
    std::vector<FsCall> calls;
    Walker w{fs};
    Game game{0, true, 0};
    World() {
        navtest::build_basic(fs);
        navtest::basic_icons(icons);
        w.record(&calls);
    }
    Selected pick(const std::string& dir, int sub = 0) {
        return select(w, icons, s, records, game, dir, sub);
    }
};

// ExtractIcon(-1) returning 0, as the unpatched Wine runtime does (newgame-0001).
class ZeroIcons : public IconCounter {
public:
    int count(const std::string&) override { return 0; }
    bool has_icon(const std::string&) override { return true; }
};

}  // namespace

TEST_CASE("nav files: the formatter's %? and %#") {
    navtest::FakeStrings s;
    CHECK(format(s, "%? %# IN %#.", {0, 2056, 2055, 3, 2515, 3, 941}) == "THERE ARE 3 ICONS IN 3 FILES.");
    CHECK(format(s, "%? %# IN %#.", {1, 2056, 2055, 1, 2515, 1, 941}) == "THERE IS 1 ICON IN 1 FILE.");
    CHECK(format(s, "%# CAPTURED AND %# DESTROYED.", {1, 2506, 0, 2506}) ==
          "1 ICON WAS CAPTURED AND 0 ICONS WERE DESTROYED.");
    CHECK(format(s, "100% sure", {}) == "100% sure");
}

TEST_CASE("nav files: status_display is seg11:0x3fd4 mode 3") {
    CHECK(status_display("THERE ARE 3 ICONS IN 3 FILES.YOU HAVE NOT BEEN HERE BEFORE. ") ==
          "There are 3 icons in 3 files.You have not been here before. ");
    CHECK(status_display("No apps IN THIS DIRECTORY. Select another directory to enter.") ==
          "No apps in this directory. Select another directory to enter.");
}

TEST_CASE("nav files: selecting games (newgame-0001, newgame-0002)") {
    World x;
    const Selected sel = x.pick("D:\\GAMES\\");
    CHECK(sel.kind == RowKind::kDirectory);
    CHECK(sel.box == 0);
    CHECK(sel.pane.items == std::vector<std::string>{"ALPHA.EXE 0", "BRAVO.EXE 0", "CHARLIE.EXE 0"});
    CHECK(sel.pane.paths[0] == "D:\\GAMES\\ALPHA.EXE");
    CHECK(sel.pane.exes == 3);
    CHECK(sel.pane.dlls == 0);
    CHECK(sel.pane.icons == 3);
    CHECK(sel.pane.preview == Preview{Preview::Kind::kIcon, "D:\\GAMES\\ALPHA.EXE", "", 0});
    CHECK(sel.enter == EnterButton::kEnabled);
    CHECK(status_display(sel.status) == "There are 3 icons in 3 files.You have not been here before. ");
    CHECK(lines(x.calls) == std::vector<std::string>{
                                R"(seg6:0x420a findfirst "D:\GAMES\*.exe" 0x00 -> "ALPHA.EXE")",
                                R"(seg6:0x4368 ExtractIcon "D:\GAMES\ALPHA.EXE" -1 -> 1)",
                                R"(seg6:0x4368 ExtractIcon "D:\GAMES\ALPHA.EXE" 0 -> icon)",
                                R"(seg6:0x420a findnext -> "BRAVO.EXE")",
                                R"(seg6:0x4368 ExtractIcon "D:\GAMES\BRAVO.EXE" -1 -> 1)",
                                R"(seg6:0x420a findnext -> "CHARLIE.EXE")",
                                R"(seg6:0x4368 ExtractIcon "D:\GAMES\CHARLIE.EXE" -1 -> 1)",
                                "seg6:0x420a findnext -> end",
                                R"(seg6:0x420a findfirst "D:\GAMES\*.dll" 0x00 -> end)",
                                R"(seg6:0x4368 ExtractIcon "\" -1 -> 0)",
                            });
}

TEST_CASE("nav files: the icon count is the sum of max(1, n) (wave-0001, newgame-0001)") {
    World x;
    // tools: ECHO 3 + DELTA 1 under the patched runtime and the port's reader
    Selected t = x.pick("D:\\TOOLS\\");
    CHECK(t.pane.icons == 4);
    CHECK(status_display(t.status) == "There are 4 icons in 2 files.You have not been here before. ");
    // b: one file, one icon
    Selected b = x.pick("D:\\DEEP\\A\\B\\");
    CHECK(status_display(b.status) == "There is 1 icon in 1 file.You have not been here before. ");
    // ExtractIcon(-1) = 0 still counts each file once (newgame-0001's "2 icons in 2 files")
    ZeroIcons zero;
    Selected z = select(x.w, zero, x.s, x.records, x.game, "D:\\TOOLS\\", 0);
    CHECK(z.pane.icons == 2);
    CHECK(status_display(z.status) == "There are 2 icons in 2 files.You have not been here before. ");
    // DLLs count in the files and the icons, not in the no-exe test
    MemFs fs;
    fs.add_drive('E');
    fs.add_file("E:\\LIB\\A.DLL");
    fs.add_file("E:\\MIX\\A.EXE");
    fs.add_file("E:\\MIX\\B.DLL");
    Walker w(fs);
    MemIcons none;
    Selected lib = select(w, none, x.s, x.records, x.game, "E:\\LIB\\", 0);
    CHECK(lib.pane.dlls == 1);
    CHECK(lib.pane.items == std::vector<std::string>{"A.DLL 0"});
    CHECK(lib.enter == EnterButton::kDisabled);
    Selected mix = select(w, none, x.s, x.records, x.game, "E:\\MIX\\", 0);
    CHECK(status_display(mix.status) == "There are 2 icons in 2 files.You have not been here before. ");
}

TEST_CASE("nav files: a directory without *.exe") {
    World x;
    Selected e = x.pick("D:\\EMPTY\\", 0);
    CHECK(status_display(e.status) == "No apps in this directory. Select another directory to enter.");
    CHECK(e.enter == EnterButton::kDisabled);
    CHECK(e.pane.preview.kind == Preview::Kind::kBlack);
    CHECK(e.pane.icons == 0);
    // deep (sub digit 1) and an expanded row (2) leave Enter as it was; row 0 reads 10
    CHECK(x.pick("D:\\DEEP\\", 1).enter == EnterButton::kUnchanged);
    CHECK(x.pick("D:\\DEEP\\", 2).enter == EnterButton::kUnchanged);
    CHECK(x.pick("D:\\", 10).enter == EnterButton::kDisabled);
}

TEST_CASE("nav files: the preview choice of seg6:0x4368") {
    MemFs fs;
    fs.add_drive('E');
    fs.add_file("E:\\P\\NOICON.EXE");
    fs.add_file("E:\\P\\NOICON2.EXE");
    fs.add_file("E:\\P\\ICON.EXE");
    fs.add_file("E:\\P\\LATE.EXE");
    MemIcons icons;
    icons.set("E:\\P\\ICON.EXE", 2);
    icons.set("E:\\P\\LATE.EXE", 1);
    std::vector<FsCall> calls;
    Walker w(fs);
    w.record(&calls);
    FilePane p = walk_files(w, icons, "E:\\P\\");
    // the first file with an icon wins; the placeholder of NOICON.EXE is replaced
    CHECK(p.preview == Preview{Preview::Kind::kIcon, "E:\\P\\ICON.EXE", "", 0});
    CHECK(p.icons == 1 + 1 + 2 + 1);
    // ExtractIcon(path, 0) runs until a file yields an icon
    int zero_calls = 0;
    for (const auto& c : calls)
        if (c.op == FsCall::Op::kExtractIcon && c.arg == 0) ++zero_calls;
    CHECK(zero_calls == 3);

    // no icon anywhere: the first program's name
    MemIcons none;
    FilePane q = walk_files(w, none, "E:\\P\\");
    CHECK(q.preview == Preview{Preview::Kind::kPlaceholder, "", "NOICON.EXE", 0});
}

TEST_CASE("nav files: the built-in rows (newgame-0002 screens 19 to 23)") {
    World x;
    SUBCASE("Demon's Gate locked in a player game of edition 0") {
        Selected d = x.pick("D:\\DEMON'S GATE\\");
        CHECK(d.kind == RowKind::kDemonsGate);
        CHECK(d.box == 0);
        CHECK(d.enter == EnterButton::kEnabled);
        CHECK(d.status == "Gateway to the ultimate challenge! To enter, you must pay for the full game version.");
        CHECK(d.pane.preview == Preview{Preview::Kind::kBuiltin, "", "", 105});
        CHECK(lines(x.calls) == std::vector<std::string>{R"(seg6:0x4368 ExtractIcon "S\" -1 -> 0)"});
    }
    SUBCASE("Grand Prix locked, Lazy Loop open") {
        Selected g = x.pick("D:\\Grand Prix - RACE\\");
        CHECK(g.kind == RowKind::kRace);
        CHECK(g.box == 1);
        // 21-nav-grandprix.png: "This is an inner space race track. To enter, you must pay for
        // the full game version."
        CHECK(status_display(g.status) ==
              "This is an inner space race track. To enter, you must pay for the full game version.");
        CHECK(g.pane.preview.icon_id == 106);
        Selected l = x.pick("D:\\Lazy Loop - RACE\\");
        CHECK(l.status == "This is an Inner Space race track. Enter here for a thrill!");
    }
    SUBCASE("Dodge City") {
        Selected d = x.pick("D:\\Dodge City - DUEL\\");
        CHECK(d.kind == RowKind::kDuel);
        CHECK(d.box == 2);
        CHECK(d.status == "This is the arena for Inner Space duels. To enter, you must pay for the full game version.");
        CHECK(d.pane.preview.icon_id == 107);
        x.game.edition = -1;
        CHECK(x.pick("D:\\Dodge City - DUEL\\").status ==
              "This is the arena for Inner Space duels. If you have a dispute, take him here!");
        CHECK(x.pick("D:\\DEMON'S GATE\\").status ==
              "Gateway to the ultimate challenge! Enter when you have what it takes..");
    }
    CHECK(builtin_icon_id('X') == 0);
}

TEST_CASE("nav files: visited and complete directories") {
    World x;
    const int i = x.records.enter("D:\\GAMES\\");
    SUBCASE("visited") {
        x.records.list[static_cast<std::size_t>(i)].captured = 1;
        Selected s = x.pick("D:\\GAMES\\");
        CHECK(s.record == i);
        CHECK(s.enter == EnterButton::kEnabled);
        CHECK_FALSE(s.completed);
        CHECK(s.status == "THERE ARE 3 ICONS IN 3 FILES.1 ICON WAS CAPTURED AND 0 ICONS WERE DESTROYED.");
    }
    SUBCASE("complete by the count: captured + destroyed above the icons") {
        x.records.list[static_cast<std::size_t>(i)].captured = 2;
        x.records.list[static_cast<std::size_t>(i)].destroyed = 2;
        Selected s = x.pick("D:\\GAMES\\");
        CHECK(s.completed);
        CHECK(x.records.list[static_cast<std::size_t>(i)].state == 2);
        CHECK(s.enter == EnterButton::kDisabled);
        CHECK(status_display(s.status) ==
              "This directory is complete. 2 icons were captured and 2 icons were destroyed.");
        // the second visit finds +0x1d0 = 2 already
        Selected again = x.pick("D:\\GAMES\\");
        CHECK_FALSE(again.completed);
        CHECK(again.enter == EnterButton::kDisabled);
    }
    SUBCASE("equal to the icons is not complete") {
        x.records.list[static_cast<std::size_t>(i)].captured = 3;
        CHECK_FALSE(x.pick("D:\\GAMES\\").completed);
    }
}

TEST_CASE("nav files: classify by the strings' ends") {
    navtest::FakeStrings s;
    CHECK(classify("D:\\DEMON'S GATE\\", s) == RowKind::kDemonsGate);
    CHECK(classify("D:\\Lazy Loop - RACE\\", s) == RowKind::kRace);
    CHECK(classify("D:\\Dodge City - DUEL\\", s) == RowKind::kDuel);
    CHECK(classify("D:\\GAMES\\", s) == RowKind::kDirectory);
    CHECK(classify("D:\\X - race\\", s) == RowKind::kDirectory);  // case-sensitive
    CHECK(classify("", s) == RowKind::kDirectory);
}
