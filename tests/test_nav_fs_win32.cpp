// Win32Fs (src/nav/fs_win32.cpp) under Win32 (Wine in CTest): the walk of the fixture drive
// `basic` makes the same calls and gives the same rows as the in-memory replica, and long names
// come back as 8.3 names that still reach their files (ADR 0012 decisions 1 and 4). The fixture
// is oracle/fixtures/make_fixtures.py's tree, found by tests/fixture_dir.h (IS_FIXTURE_DIR or
// work/fixtures/basic); the golden skips when it is absent.
#ifdef _WIN32
#include <windows.h>

#include <cstdlib>
#include <string>
#include <vector>

#include "doctest.h"
#include "fixture_dir.h"
#include "nav_fixture.h"

using namespace is::nav;

namespace {

const std::string kBase = "D:\\FIX\\";

// The shared fixture lookup of fixture_dir.h (IS_FIXTURE_DIR), as a DOS path with a trailing
// backslash (M3 s1 fixer: one variable for every golden).
std::string fixture_root() {
    std::string d = fixture_dir().string();
    for (char& c : d)
        if (c == '/') c = '\\';
    if (d.empty()) return {};
    if (d.back() != '\\') d += '\\';
    return d;
}

// The lines with the fixture's prefix written as the replica's.
std::vector<std::string> as_d(const std::vector<FsCall>& calls, const std::string& root) {
    std::vector<std::string> v;
    for (FsCall c : calls) {
        if (c.path.compare(0, root.size(), root) == 0) c.path = kBase + c.path.substr(root.size());
        v.push_back(describe(c));
    }
    return v;
}

class OneIcon : public IconCounter {
public:
    int count(const std::string&) override { return 1; }
};

}  // namespace

TEST_CASE("nav fs win32: the fixture drive walks as its in-memory replica") {
    const std::string root = fixture_root();
    if (root.empty() || !path_fits(root + "DEEP\\A\\B")) {
        MESSAGE("fixture basic absent (IS_FIXTURE_DIR or work/fixtures/basic); skipped");
        return;
    }
    // The fixture is a directory, not a drive root, so its own enumeration starts with "." and
    // "..": the replica sits in D:\FIX\ to match.
    navtest::FakeStrings s;
    Records records;
    const Game game{0, true, 0};

    Win32Fs wfs;
    std::vector<FsCall> wcalls;
    Walker ww(wfs);
    ww.record(&wcalls);
    Tree wt(ww, s, records, game);

    MemFs mfs;
    navtest::build_basic(mfs, kBase);
    std::vector<FsCall> mcalls;
    Walker mw(mfs);
    mw.record(&mcalls);
    Tree mt(mw, s, records, game);

    wt.fill(root);
    mt.fill(kBase);
    CHECK(as_d(wcalls, root) == navtest::lines(mcalls));
    REQUIRE(wt.rows().size() == mt.rows().size());
    CHECK(parse_row(wt.rows()[0]).state == parse_row(mt.rows()[0]).state);
    CHECK(parse_row(wt.rows()[0]).sub == parse_row(mt.rows()[0]).sub);
    for (std::size_t i = 1; i < wt.rows().size(); ++i) CHECK(wt.rows()[i] == mt.rows()[i]);

    // expand deep, then a
    wcalls.clear();
    mcalls.clear();
    CHECK(wt.expand(1));
    CHECK(mt.expand(1));
    CHECK(wt.expand(2));
    CHECK(mt.expand(2));
    CHECK(as_d(wcalls, root) == navtest::lines(mcalls));
    CHECK(wt.rows()[3] == "B300");

    // the file walk of games and tools
    OneIcon one;
    for (const char* dir : {"GAMES\\", "TOOLS\\"}) {
        wcalls.clear();
        mcalls.clear();
        const FilePane wp = walk_files(ww, one, root + dir);
        const FilePane mp = walk_files(mw, one, kBase + dir);
        CHECK(wp.items == mp.items);
        CHECK(as_d(wcalls, root) == navtest::lines(mcalls));
    }

    // the drive list holds the system drive, C:
    bool c_drive = false;
    for (const Drive& d : wfs.drives())
        if (d.letter == 'C') c_drive = true;
    CHECK(c_drive);
}

TEST_CASE("nav fs win32: long names become 8.3 names that reach their files") {
    wchar_t tmp[MAX_PATH] = {};
    REQUIRE(GetTempPathW(MAX_PATH, tmp) > 0);
    const std::wstring base = std::wstring(tmp) + L"isnav_" + std::to_wstring(GetCurrentProcessId());
    const std::wstring sub = base + L"\\Long Directory Name";
    REQUIRE(CreateDirectoryW(base.c_str(), nullptr));
    REQUIRE(CreateDirectoryW(sub.c_str(), nullptr));
    const std::wstring file = sub + L"\\program.exe";
    HANDLE h = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    REQUIRE(h != INVALID_HANDLE_VALUE);
    CloseHandle(h);

    std::string dos;
    for (wchar_t c : base) dos += static_cast<char>(c);  // the temp path is ASCII here
    if (path_fits(dos + "\\LONGDIRE")) {
        Win32Fs fs;
        Walker w(fs);
        Walker::Find f = w.find("test", dos + "\\*.*", kTreeAttr);
        FindEntry e;
        std::string name;
        while (f.next(e))
            if (is_subdir(e)) name = e.name;
        CHECK(name.size() <= 12);
        CHECK(name == upper(name));
        CHECK(name.find(' ') == std::string::npos);
        CHECK(w.has_exe(dos + "\\" + name));
        MESSAGE("8.3 name of \"Long Directory Name\": " << name);
    }

    DeleteFileW(file.c_str());
    RemoveDirectoryW(sub.c_str());
    RemoveDirectoryW(base.c_str());
}

TEST_CASE("nav fs win32: a cut name maps back to its long name") {
    Win32Fs fs;
    CHECK(fs.native("C:\\X\\LONGDIRE\\*.EXE") == L"C:\\X\\LONGDIRE\\*.EXE");
    fs.remember("C:\\X\\LONGDIRE", L"C:\\X\\Long Dir Name");
    CHECK(fs.native("C:\\X\\LONGDIRE\\*.EXE") == L"C:\\X\\Long Dir Name\\*.EXE");
    CHECK(fs.native("c:\\x\\longdire") == L"C:\\X\\Long Dir Name");
    CHECK(fs.native("C:\\X\\LONGDIRECTORY") == L"C:\\X\\LONGDIRECTORY");
    fs.remember("C:\\X\\LONGDIRE\\SUBDIREC", L"C:\\X\\Long Dir Name\\Sub Directory");
    CHECK(fs.native("C:\\X\\LONGDIRE\\SUBDIREC\\A.EXE") == L"C:\\X\\Long Dir Name\\Sub Directory\\A.EXE");
}
#endif
