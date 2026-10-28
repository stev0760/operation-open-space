// The Navigation walk's filesystem view (src/nav/fs.h) against docs/re/nav.md "Probes" and
// "Filesystem calls", ADR 0012's bounds, and the oracle's int 21h trace of newgame-0002 on
// fixture `basic` (work/oracle/newgame-0002/trace.log).
#include "doctest.h"
#include "nav_fixture.h"

using namespace is::nav;
using navtest::lines;

TEST_CASE("nav fs: the DOS attribute filter and wildcard matching") {
    // attribute 0x15 admits directories, system and normal files, not hidden ones
    CHECK(dos_attr_match(kAttrDirectory, kTreeAttr));
    CHECK(dos_attr_match(kAttrArchive, kTreeAttr));
    CHECK(dos_attr_match(kAttrReadOnly, kTreeAttr));
    CHECK(dos_attr_match(kAttrSystem | kAttrDirectory, kTreeAttr));
    CHECK_FALSE(dos_attr_match(kAttrHidden | kAttrDirectory, kTreeAttr));
    // attribute 0 admits normal files only
    CHECK(dos_attr_match(kAttrArchive, kFileAttr));
    CHECK(dos_attr_match(kAttrReadOnly | kAttrArchive, kFileAttr));
    CHECK_FALSE(dos_attr_match(kAttrDirectory, kFileAttr));
    CHECK_FALSE(dos_attr_match(kAttrSystem, kFileAttr));
    CHECK_FALSE(dos_attr_match(kAttrHidden, kFileAttr));
    CHECK(dos_attr_match(kAttrHidden, kAttrHidden));

    CHECK(dos_name_match("ALPHA.EXE", "*.exe"));
    CHECK(dos_name_match("alpha.exe", "*.EXE"));
    CHECK_FALSE(dos_name_match("ALPHA.DLL", "*.exe"));
    CHECK_FALSE(dos_name_match("GAMES", "*.exe"));
    CHECK(dos_name_match("GAMES", "*.*"));
    CHECK(dos_name_match(".", "*.*"));
    CHECK_FALSE(dos_name_match(".", "*.exe"));
    CHECK(dos_name_match("A.B", "?.B"));
    CHECK_FALSE(dos_name_match("AB.B", "?.B"));
}

TEST_CASE("nav fs: 8.3 names cut from long names (ADR 0012 decision 4)") {
    CHECK(short_name("games") == "GAMES");
    CHECK(short_name("Program Files") == "PROGRAMF");
    CHECK(short_name("a+b;c") == "A_B_C");
    CHECK(short_name("setup.exe") == "SETUP.EXE");
    CHECK(short_name("archive.tar.gz") == "ARCHIVET.GZ");
    CHECK(short_name("verylongname.html") == "VERYLONG.HTM");
    CHECK(short_name(".") == ".");
    CHECK(short_name("..") == "..");
}

TEST_CASE("nav fs: paths, the 100-byte bound and describe()") {
    CHECK(with_slash("D:\\GAMES") == "D:\\GAMES\\");
    CHECK(with_slash("D:\\") == "D:\\");
    CHECK(pattern_dir("D:\\GAMES\\*.exe") == "D:\\GAMES\\");
    CHECK(pattern_dir("D:\\*.*") == "D:\\");
    // DGROUP:0x6990 holds the path, its '\' and the NUL in 100 bytes
    CHECK(path_fits("D:\\" + std::string(95, 'X')));    // 98 + '\' + NUL = 100
    CHECK_FALSE(path_fits("D:\\" + std::string(96, 'X')));
    CHECK(probe_label_on_listing(DriveType::kFixed));
    CHECK(probe_label_on_listing(DriveType::kRemovable));
    CHECK_FALSE(probe_label_on_listing(DriveType::kRemote));
    CHECK_FALSE(probe_label_on_listing(DriveType::kUnknown));
    CHECK_FALSE(probe_label_on_listing(DriveType::kNoRoot));

    FsCall c;
    c.routine = "seg6:0x4748";
    c.path = "D:\\DEEP\\*.*";
    c.arg = 0x15;
    c.ok = true;
    c.name = ".";
    CHECK(describe(c) == R"(seg6:0x4748 findfirst "D:\DEEP\*.*" 0x15 -> ".")");
    c.op = FsCall::Op::kFindNext;
    c.ok = false;
    CHECK(describe(c) == "seg6:0x4748 findnext -> end");
    FsCall i;
    i.routine = "seg6:0x4368";
    i.op = FsCall::Op::kExtractIcon;
    i.path = "D:\\TOOLS\\ECHO.EXE";
    i.arg = -1;
    i.value = 3;
    CHECK(describe(i) == R"(seg6:0x4368 ExtractIcon "D:\TOOLS\ECHO.EXE" -1 -> 3)");
}

TEST_CASE("nav fs: MemFs enumerates as DOS does") {
    MemFs fs;
    navtest::build_basic(fs);
    fs.add_dir("D:\\HIDDEN", kAttrDirectory | kAttrHidden);
    std::vector<std::string> names;
    FindEntry e;
    auto s = fs.find("D:\\*.*", kTreeAttr);
    while (s->next(e)) names.push_back(e.name);
    // the root has no "." entries; the hidden directory is left out
    CHECK(names == std::vector<std::string>{"DEEP", "EMPTY", "GAMES", "TOOLS"});
    names.clear();
    s = fs.find("D:\\GAMES\\*.*", kTreeAttr);
    while (s->next(e)) names.push_back(e.name);
    CHECK(names == std::vector<std::string>{".", "..", "ALPHA.EXE", "BRAVO.EXE", "CHARLIE.EXE"});
    names.clear();
    s = fs.find("D:\\GAMES\\*.exe", kFileAttr);
    while (s->next(e)) names.push_back(e.name);
    CHECK(names.size() == 3);
    s = fs.find("D:\\NOPE\\*.*", kTreeAttr);
    CHECK_FALSE(s->next(e));
    CHECK(fs.drives().size() == 1);
}

TEST_CASE("nav fs: the root row's probes in the oracle's order") {
    MemFs fs;
    navtest::build_basic(fs);
    std::vector<FsCall> calls;
    std::vector<std::string> log;
    Walker w(fs, [&](const std::string& l) { log.push_back(l); });
    w.record(&calls);
    // seg6:0x37ca's root row: seg6:0x45be, then seg6:0x4748 && seg6:0x4650 (newgame-0002 trace:
    // D:\*.exe, one D:\*.* call, then D:\*.* with a <entry>\*.exe probe after each entry until
    // GAMES finds ALPHA.EXE)
    CHECK_FALSE(w.passes("D:\\", "D:\\"));
    CHECK(w.has_subdir("D:\\"));
    CHECK(w.subdir_has_exe("D:\\"));
    const std::vector<std::string> want = {
        R"(seg6:0x4190 findfirst "D:\*.exe" 0x00 -> end)",
        R"(seg6:0x4748 findfirst "D:\*.*" 0x15 -> "DEEP")",
        R"(seg6:0x4650 findfirst "D:\*.*" 0x15 -> "DEEP")",
        R"(seg6:0x4190 findfirst "D:\DEEP\*.exe" 0x00 -> end)",
        R"(seg6:0x4650 findnext -> "EMPTY")",
        R"(seg6:0x4190 findfirst "D:\EMPTY\*.exe" 0x00 -> end)",
        R"(seg6:0x4650 findnext -> "GAMES")",
        R"(seg6:0x4190 findfirst "D:\GAMES\*.exe" 0x00 -> "ALPHA.EXE")",
    };
    CHECK(lines(calls) == want);
    REQUIRE(log.size() == want.size());
    CHECK(log[0] == "nav fs " + want[0]);
}

TEST_CASE("nav fs: seg6:0x45be on a child, the one-level-ahead look") {
    MemFs fs;
    navtest::build_basic(fs);
    std::vector<FsCall> calls;
    Walker w(fs);
    w.record(&calls);
    // D:\DEEP\A: no *.exe, a subdirectory B whose *.exe exists (newgame-0002 expand of deep)
    CHECK(w.passes("D:\\DEEP\\A", "D:\\"));
    CHECK(lines(calls) == std::vector<std::string>{
                              R"(seg6:0x4190 findfirst "D:\DEEP\A\*.exe" 0x00 -> end)",
                              R"(seg6:0x4748 findfirst "D:\DEEP\A\*.*" 0x15 -> ".")",
                              R"(seg6:0x4748 findnext -> "..")",
                              R"(seg6:0x4748 findnext -> "B")",
                              R"(seg6:0x4650 findfirst "D:\DEEP\A\*.*" 0x15 -> ".")",
                              R"(seg6:0x4650 findnext -> "..")",
                              R"(seg6:0x4650 findnext -> "B")",
                              R"(seg6:0x4190 findfirst "D:\DEEP\A\B\*.exe" 0x00 -> "FOXTROT.EXE")",
                          });
    calls.clear();
    // EMPTY: nothing; DEEP: A holds no *.exe, so the look one level ahead fails
    CHECK_FALSE(w.passes("D:\\EMPTY", "D:\\"));
    CHECK_FALSE(w.passes("D:\\DEEP", "D:\\"));
    CHECK(w.passes("D:\\GAMES", "D:\\"));
}

TEST_CASE("nav fs: a failed find-first still runs the find-next (seg6:0x4748, seg6:0x4650)") {
    MemFs fs;
    navtest::build_basic(fs);
    std::vector<FsCall> calls;
    Walker w(fs);
    w.record(&calls);
    CHECK_FALSE(w.has_subdir("D:\\NOPE"));
    CHECK_FALSE(w.subdir_has_exe("D:\\NOPE"));
    CHECK(lines(calls) == std::vector<std::string>{
                              R"(seg6:0x4748 findfirst "D:\NOPE\*.*" 0x15 -> end)",
                              "seg6:0x4748 findnext -> end",
                              R"(seg6:0x4650 findfirst "D:\NOPE\*.*" 0x15 -> end)",
                              "seg6:0x4650 findnext -> end",
                          });
}

TEST_CASE("nav fs: ADR 0012 bounds: path length, reparse points, entry count") {
    MemFs fs;
    navtest::build_basic(fs);
    // a junction under GAMES and a deep chain past the 100-byte buffer
    fs.add_dir("D:\\GAMES\\LINK", kAttrDirectory, true);
    std::string deep = "D:\\";
    for (int i = 0; i < 12; ++i) {
        deep += "DIRECTRY";  // 8 bytes + '\' per level
        fs.add_dir(deep);
        deep += "\\";
    }
    fs.add_file(deep + "X.EXE");
    std::vector<FsCall> calls;
    Walker w(fs);
    w.record(&calls);

    // the reparse point is reported by the enumeration and never entered afterwards
    CHECK(w.has_subdir("D:\\GAMES"));
    CHECK(w.is_reparse("D:\\GAMES\\LINK"));
    CHECK(w.is_reparse("D:\\GAMES\\LINK\\SUB\\"));
    CHECK_FALSE(w.is_reparse("D:\\GAMES"));
    calls.clear();
    CHECK_FALSE(w.has_exe("D:\\GAMES\\LINK"));
    REQUIRE(calls.size() == 1);
    CHECK(calls[0].refused);
    CHECK(describe(calls[0]) == R"(seg6:0x4190 findfirst "D:\GAMES\LINK\*.exe" 0x00 -> refused (ADR 0012))");

    // a directory whose path does not fit DGROUP:0x6990 is never searched
    calls.clear();
    CHECK_FALSE(w.enterable(deep));
    CHECK_FALSE(w.has_exe(deep));
    CHECK(calls.size() == 1);
    CHECK(calls[0].refused);

    // a search ends after 4096 entries
    MemFs big;
    big.add_drive('E');
    big.add_dir("E:\\BIG");
    for (int i = 0; i < kMaxEntries + 10; ++i) big.add_file("E:\\BIG\\F" + std::to_string(i) + ".EXE");
    Walker wb(big);
    std::vector<FsCall> bc;
    wb.record(&bc);
    Walker::Find f = wb.find("seg6:0x420a", "E:\\BIG\\*.exe", kFileAttr);
    FindEntry e;
    int n = 0;
    while (f.next(e)) ++n;
    CHECK(n == kMaxEntries);
    CHECK(bc.back().refused);
    CHECK_FALSE(f.next(e));
}
