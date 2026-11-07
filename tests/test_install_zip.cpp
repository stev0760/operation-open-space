// The install step on a .zip install set (ADR 0008 decision 1: a zip is unpacked into a
// directory of its own first). A zip built from original/game (stored and deflated
// entries, the set inside one folder) opens and installs exactly as the directory set does:
// the same set, the same game directory byte for byte, the same INI. Skips without original/.
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "doctest.h"
#include "formats/bytes.h"
#include "install/install.h"
#include "settings/settings.h"
#include "test_zip_builder.h"

namespace fs = std::filesystem;
using namespace is::install;

namespace {

fs::path game() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v) return {};
    const fs::path p = fs::path(v).parent_path().parent_path().parent_path() / "original" / "game";
    std::error_code ec;
    return fs::is_directory(p, ec) ? p : fs::path();
}

struct Tmp {
    fs::path path;
    Tmp() {
        path = fs::temp_directory_path() /
               ("is_install_zip_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + "_" +
                std::to_string(std::rand()));
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~Tmp() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void write_bytes(const fs::path& p, const std::vector<std::uint8_t>& b) {
    std::ofstream f(p, std::ios::binary);
    f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}

// One deterministic run: every answer and the clock fixed, so two runs write the same bytes.
Result install_once(const SourceSet& set, const fs::path& game, const fs::path& ini_path) {
    std::uint32_t clock = 5000;
    Ui ui;
    ui.message = [](const std::string&, unsigned) { return kIdNo; };
    ui.components = [&](Components& c) {
        c.dir = is::fmt::path_utf8(game) + "/";
        return true;
    };
    ui.difficulty = [](int&, int&) {};
    ui.window = [] { return false; };
    ui.personalize = [](std::string& n, std::string& o) {
        n = "Zip Tester";
        o = "Test Org";
    };
    ui.done = [](const std::string&, const fs::path&) {};
    ui.tick = [&] { return clock += 3; };
    ui.date = [](int& y, int& d) {
        y = 126;
        d = 300;
    };
    return run_install(set, ini_path, kDefaultDir, ui);
}

std::map<std::string, std::vector<std::uint8_t>> tree(const fs::path& root) {
    std::map<std::string, std::vector<std::uint8_t>> m;
    for (const auto& e : fs::recursive_directory_iterator(root))
        m[e.path().lexically_relative(root).generic_string() + (e.is_directory() ? "/" : "")] =
            e.is_regular_file() ? is::fmt::read_file(e.path()) : std::vector<std::uint8_t>{};
    return m;
}

}  // namespace

TEST_CASE("install: a zipped install set installs as the directory set does") {
    const fs::path sd = game();
    if (sd.empty()) return;
    Tmp tmp;
    // The zip: the set's files in a folder GAME/, small ones deflated, large ones stored.
    std::vector<is_test::ZipItem> items;
    items.push_back({"GAME/", {}, false});
    for (const auto& e : fs::directory_iterator(sd)) {
        if (!e.is_regular_file()) continue;
        std::vector<std::uint8_t> b = is::fmt::read_file(e.path());
        const bool small = b.size() < 50000;
        items.push_back({"GAME/" + e.path().filename().string(), std::move(b), small});
    }
    const fs::path zip = tmp.path / "thegame.zip";
    write_bytes(zip, is_test::build_zip(items));

    std::vector<std::string> logs;
    const OpenResult oz = open_source(zip, tmp.path / "INSTALL", [&](const std::string& s) { logs.push_back(s); });
    REQUIRE_MESSAGE(oz.set.has_value(), oz.message);
    const OpenResult od = open_source(sd, tmp.path / "INSTALL2", {});
    REQUIRE(od.set.has_value());
    const SourceSet& z = *oz.set;
    const SourceSet& d = *od.set;
    CHECK(z.dir == tmp.path / "INSTALL" / "thegame" / "GAME");
    CHECK(z.label == d.label);
    CHECK(z.rule == d.rule);
    CHECK(z.rule == Rule::kRegister);
    CHECK(z.new_install == d.new_install);
    CHECK(z.rec120 == d.rec120);
    CHECK(z.rec110 == d.rec110);
    REQUIRE(z.files.size() == d.files.size());
    for (const auto& kv : d.files) {
        const auto it = z.files.find(kv.first);
        REQUIRE(it != z.files.end());
        CHECK(is::fmt::read_file(it->second) == is::fmt::read_file(kv.second));
    }
    bool logged = false;
    for (const std::string& s : logs) logged |= s.find("unpacked to") != std::string::npos;
    CHECK(logged);
    // The user's zip is left as it was.
    CHECK(fs::is_regular_file(zip));

    const Result rz = install_once(z, tmp.path / "GZ", tmp.path / "az" / "INSPACE.INI");
    const Result rd = install_once(d, tmp.path / "GD", tmp.path / "ad" / "INSPACE.INI");
    CHECK(rz.completed);
    CHECK(rz.registered);
    CHECK(rd.completed);
    CHECK(tree(tmp.path / "GZ") == tree(tmp.path / "GD"));
    // The INIs differ only where they name the game directory or the INI's own.
    const is::settings::IniFile iz(rz.ini_path), id(rd.ini_path);
    const auto ez = iz.entries("Inner Space"), ed = id.entries("Inner Space");
    REQUIRE(ez.size() == ed.size());
    for (std::size_t i = 0; i < ez.size(); ++i) {
        CHECK(ez[i].first == ed[i].first);
        const bool names_dir = ez[i].second.find("\\GZ\\") != std::string::npos ||
                               ez[i].second.find("\\az\\") != std::string::npos;
        if (!names_dir) CHECK_MESSAGE(ez[i].second == ed[i].second, ez[i].first);
    }
}

TEST_CASE("install: a zip without INSTALL.INI or with an unsafe name is refused") {
    Tmp tmp;
    const fs::path z1 = tmp.path / "none.zip";
    write_bytes(z1, is_test::build_zip({{"README.TXT", {'h', 'i'}, false}}));
    OpenResult o = open_source(z1, tmp.path / "INSTALL", {});
    CHECK(!o.set.has_value());
    CHECK(o.message.find("holds no INSTALL.INI") != std::string::npos);

    const fs::path z2 = tmp.path / "evil.zip";
    write_bytes(z2, is_test::build_zip({{"../INSTALL.INI", {'x'}, false}}));
    o = open_source(z2, tmp.path / "INSTALL", {});
    CHECK(!o.set.has_value());
    CHECK(o.message.find("unsafe path") != std::string::npos);
    CHECK(!fs::exists(tmp.path / "INSTALL.INI"));

    const fs::path z3 = tmp.path / "broken.zip";
    write_bytes(z3, {'P', 'K', 3, 4, 0, 0});
    o = open_source(z3, tmp.path / "INSTALL", {});
    CHECK(!o.set.has_value());
    CHECK(o.message.find("Cannot read the zip") != std::string::npos);
}

TEST_CASE("install: a zip's set folder never names the working root (\"..zip\")") {
    Tmp tmp;
    const fs::path work = tmp.path / "INSTALL";
    fs::create_directories(work / "Keep");
    write_bytes(work / "Keep" / "KEEP.TXT", {'k'});
    const fs::path z = tmp.path / "..zip";
    write_bytes(z, is_test::build_zip({{"INSTALL.INI", {'[', 'V', ']'}, false}}));
    std::string why;
    const auto dir = unpack_zip(z, work, &why);
    REQUIRE_MESSAGE(dir.has_value(), why);
    CHECK(dir->filename() == fs::path("SET"));
    CHECK(fs::exists(work / "Keep" / "KEEP.TXT"));  // the root was not emptied
}

TEST_CASE("install: a refused zip writes nothing and keeps the earlier unpack") {
    Tmp tmp;
    const fs::path work = tmp.path / "INSTALL";
    const fs::path z = tmp.path / "Set.zip";
    write_bytes(z, is_test::build_zip({{"INSTALL.INI", {'a'}, false}}));
    std::string why;
    REQUIRE(unpack_zip(z, work, &why).has_value());
    CHECK(fs::exists(work / "Set" / "INSTALL.INI"));
    // Entry 0 is good, entry 1 is not: the check runs over every name first.
    for (const char* bad : {"CON", "a/NUL.TXT", "b./x", "c /x", "d\x01", "e?f"}) {
        write_bytes(z, is_test::build_zip({{"NEW.TXT", {'n'}, false}, {bad, {'x'}, false}}));
        CHECK_FALSE(unpack_zip(z, work, &why).has_value());
        CHECK(why.find("unsafe path") != std::string::npos);
        CHECK(fs::exists(work / "Set" / "INSTALL.INI"));
        CHECK_FALSE(fs::exists(work / "Set" / "NEW.TXT"));
    }
    // Two names that differ only in case are one file on Windows: refused.
    write_bytes(z, is_test::build_zip({{"INSTALL.INI", {'a'}, false}, {"install.ini", {'b'}, false}}));
    CHECK_FALSE(unpack_zip(z, work, &why).has_value());
    CHECK(why.find("twice") != std::string::npos);
    CHECK(fs::exists(work / "Set" / "INSTALL.INI"));
}

TEST_CASE("install: a zip stating more than the unpack bound is refused before writing") {
    Tmp tmp;
    const fs::path work = tmp.path / "INSTALL";
    std::vector<std::uint8_t> z = is_test::build_zip({{"INSTALL.INI", {'a'}, false}, {"B.BIN", {'b'}, false}});
    // The central directory's usize (offset 24 in each 46 + name byte entry) of both entries
    // set to 0x90000000: together past kZipMaxBytes.
    std::size_t cd = 0;
    for (std::size_t i = 0; i + 4 <= z.size(); ++i)
        if (z[i] == 0x50 && z[i + 1] == 0x4b && z[i + 2] == 1 && z[i + 3] == 2) {
            if (!cd) cd = i;
            z[i + 24] = 0, z[i + 25] = 0, z[i + 26] = 0, z[i + 27] = 0x90;
        }
    REQUIRE(cd != 0);
    const fs::path p = tmp.path / "Big.zip";
    write_bytes(p, z);
    std::string why;
    CHECK_FALSE(unpack_zip(p, work, &why).has_value());
    CHECK(why.find("more than") != std::string::npos);
    CHECK_FALSE(fs::exists(work / "Big"));
}
