// M3 stage 1 fixer cases: ADR 0012 decision 3 for expanded children (a reparse point or an
// over-long path is shown with sub 0 and never probed), decision 2 for the New Game drive list
// (network and unknown drives listed unprobed, rootless drives skipped) and the unique BASE~N
// names of cut long names (decision 4).
#include "doctest.h"
#include "nav_fixture.h"
#include "nav/nav_draw.h"

#include <new>

using namespace is::nav;
using navtest::lines;

namespace {

class CountingFs : public MemFs {
public:
    std::vector<char> probed;
    bool volume_label(char letter, std::string& label) override {
        probed.push_back(letter);
        return MemFs::volume_label(letter, label);
    }
};

}  // namespace

TEST_CASE("nav fixer: an expanded child that is a reparse point shows with sub 0, unprobed") {
    MemFs fs;
    navtest::build_basic(fs);
    fs.add_dir("D:\\DEEP\\JUNC", kAttrDirectory, true);
    navtest::FakeStrings s;
    Records records;
    std::vector<FsCall> calls;
    Walker w(fs);
    w.record(&calls);
    Tree t(w, s, records, Game{0, true, 0});
    t.fill("D:\\");
    REQUIRE(t.path(1) == "D:\\DEEP\\");
    calls.clear();
    CHECK(t.expand(1));
    bool junc = false, a = false;
    for (const std::string& r : t.rows()) {
        junc |= r == "JUNC200";
        a |= r == "A210";
    }
    CHECK(junc);
    CHECK(a);
    for (const std::string& l : lines(calls)) CHECK(l.find("JUNC\\") == std::string::npos);
}

TEST_CASE("nav fixer: a top-level reparse point shows with sub 0 under INI Tree = 1") {
    MemFs fs;
    navtest::build_basic(fs);
    fs.add_dir("D:\\JUNC", kAttrDirectory, true);
    navtest::FakeStrings s;
    Records records;
    Walker w(fs);
    Tree t(w, s, records, Game{0, true, 1});
    t.fill("D:\\");
    bool junc = false;
    for (const std::string& r : t.rows()) junc |= r == "JUNC100";
    CHECK(junc);
}

TEST_CASE("nav fixer: the drive list probes fixed drives only (ADR 0012 decision 2)") {
    CountingFs fs;
    fs.add_drive('C', DriveType::kFixed, "SYSTEM");
    fs.add_drive('D', DriveType::kFixed);
    fs.add_drive('E', DriveType::kNoRoot);
    fs.add_drive('F', DriveType::kRemote, "SHARE");
    fs.add_drive('G', DriveType::kUnknown, "ODD");
    fs.add_drive('H', DriveType::kCdrom, "DISC");
    std::vector<std::string> log;
    const auto l = list_drives(fs, [&](const std::string& s) { log.push_back(s); });
    REQUIRE(l.size() == 5);
    CHECK(l[0].letter == 'C');
    CHECK(l[0].has_label);
    CHECK(l[0].label == "SYSTEM");
    CHECK(l[1].letter == 'D');
    CHECK(l[1].probed);
    CHECK_FALSE(l[1].has_label);
    CHECK(l[2].letter == 'F');
    CHECK_FALSE(l[2].probed);
    CHECK_FALSE(l[2].has_label);
    CHECK(l[3].letter == 'G');
    CHECK_FALSE(l[3].probed);
    CHECK(l[4].letter == 'H');
    CHECK(l[4].label == "DISC");
    CHECK(fs.probed == std::vector<char>{'C', 'D', 'H'});
    REQUIRE(log.size() == 5);
    CHECK(log[0] == R"(drive fs ISPACE.DLL seg10:0x2104 findfirst "C:\*.*" 0x08 -> "SYSTEM")");
    CHECK(log[1] == R"(drive fs ISPACE.DLL seg10:0x2104 findfirst "D:\*.*" 0x08 -> end)");
    CHECK(log[2] == R"(drive fs ISPACE.DLL seg10:0x2104 findfirst "F:\*.*" 0x08 -> not probed (ADR 0012))");
}

TEST_CASE("nav fixer: cut long names get unique BASE~N names per directory") {
    ShortNames n;
    n.reserve("PROGRA~1");  // a fitting name already in the directory
    n.reserve("GAMES");
    CHECK(n.assign("Program Files", short_name("Program Files")) == "PROGRA~2");
    CHECK(n.assign("Program Files (x86)", short_name("Program Files (x86)")) == "PROGRA~3");
    CHECK(n.assign("Program Files", short_name("Program Files")) == "PROGRA~2");  // stable
    CHECK(n.assign("Setup Program.exe", short_name("Setup Program.exe")) == "SETUPP~1.EXE");
    CHECK(n.assign("Setup Program 2.exe", short_name("Setup Program 2.exe")) == "SETUPP~2.EXE");
    ShortNames many;
    for (int i = 1; i <= 10; ++i) many.assign("Microsoft Thing " + std::to_string(i), "MICROSOF");
    CHECK(many.assign("Microsoft Thing 10", "MICROSOF") == "MICRO~10");
}

TEST_CASE("nav fixer: an icon load that throws is ExtractIcon's 0, never an exception") {
    IconReader r([](const std::string&) -> is::fmt::IconSource { throw std::bad_alloc(); });
    CHECK(r.count("D:\\BIG.EXE") == 0);
    CHECK_FALSE(r.has_icon("D:\\BIG.EXE"));
    CHECK_FALSE(r.cell("D:\\BIG.EXE").has_value());
}
