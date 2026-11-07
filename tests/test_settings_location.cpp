// The INI location of ADR 0004 (src/settings/ini_location.h): the resolution order --ini,
// portable, %APPDATA%; the first-run copy from the Windows directory; LocalPath's default; the
// regeneration that follows; and the port-only [Registration] keys of ADR 0008.
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "doctest.h"
#include "settings/ini_location.h"
#include "settings/settings.h"

using namespace is::settings;
namespace fs = std::filesystem;

namespace {

class LocTemp {
public:
    LocTemp() {
        static unsigned counter = 0;
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = fs::temp_directory_path() /
                ("is_iniloc_" + std::to_string(stamp) + "_" + std::to_string(++counter));
        fs::create_directories(path_);
    }
    ~LocTemp() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }
    LocTemp(const LocTemp&) = delete;
    LocTemp& operator=(const LocTemp&) = delete;
    const fs::path& path() const { return path_; }

private:
    fs::path path_;
};

void write_text(const fs::path& p, const std::string& s) {
    fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary);
    f << s;
}

std::string read_text(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), {});
}

}  // namespace

TEST_CASE("INI location: %APPDATA% by default, portable next to the executable, --ini wins") {
    LocTemp t;
    const fs::path exe = t.path() / "bin", appdata = t.path() / "appdata";
    fs::create_directories(exe);

    IniLocation loc = resolve_ini_location(std::nullopt, exe, appdata);
    CHECK(loc.source == IniSource::kAppData);
    CHECK(loc.path == appdata / "Inner Space" / "INSPACE.INI");

    write_text(exe / "INSPACE.INI", "[Inner Space]\r\n");
    loc = resolve_ini_location(std::nullopt, exe, appdata);
    CHECK(loc.source == IniSource::kPortable);
    CHECK(loc.path == exe / "INSPACE.INI");
    // not when the executable sits in the asset directory: that file is the game's own
    // (ADR 0002 decision 5) [DESIGN]
    loc = resolve_ini_location(std::nullopt, exe, appdata, exe);
    CHECK(loc.source == IniSource::kAppData);
    CHECK(resolve_ini_location(std::nullopt, exe, appdata, t.path() / "game").source ==
          IniSource::kPortable);

    // nor when the executable sits in an install set (INSTALL.INI beside it): that INSPACE.INI
    // is the set's own, the install step's source (ADR 0008 decision 1) [DESIGN]
    write_text(exe / "INSTALL.INI", "[Vers]\r\nV=12908\r\n");
    CHECK(resolve_ini_location(std::nullopt, exe, appdata).source == IniSource::kAppData);
    fs::remove(exe / "INSTALL.INI");
    CHECK(resolve_ini_location(std::nullopt, exe, appdata).source == IniSource::kPortable);

    const fs::path named = t.path() / "else" / "my.ini";
    loc = resolve_ini_location(named, exe, appdata);
    CHECK(loc.source == IniSource::kOverride);
    CHECK(loc.path == named);  // need not exist
    CHECK_FALSE(loc.override_refused);
    // not a file in the asset directory, which stays read-only (ADR 0002 decision 5) [DESIGN]
    const fs::path game = t.path() / "game";
    fs::create_directories(game);
    loc = resolve_ini_location(game / "INSPACE.INI", exe, appdata, game);
    CHECK(loc.override_refused);
    CHECK(loc.source == IniSource::kPortable);
    CHECK(loc.path == exe / "INSPACE.INI");
    CHECK(resolve_ini_location(game / "INSPACE.INI", exe, appdata, exe).source ==
          IniSource::kOverride);

    // a directory named INSPACE.INI is not a portable file
    fs::remove(exe / "INSPACE.INI");
    fs::create_directories(exe / "INSPACE.INI");
    CHECK(resolve_ini_location(std::nullopt, exe, appdata).source == IniSource::kAppData);
}

TEST_CASE("INI location: the first run copies <Windows directory>\\INSPACE.INI") {
    LocTemp t;
    const fs::path win = t.path() / "windows" / "INSPACE.INI";
    const fs::path target = t.path() / "appdata" / "Inner Space" / "INSPACE.INI";
    // nothing to copy
    CHECK_FALSE(first_run_copy(target, win));
    CHECK_FALSE(fs::exists(target));
    write_text(win, "[Inner Space]\r\nPath=C:\\SPACE\\\r\n");
    CHECK(first_run_copy(target, win));
    CHECK(read_text(target) == read_text(win));
    // an existing file is never replaced
    write_text(target, "[Inner Space]\r\nPath=D:\\\r\n");
    CHECK_FALSE(first_run_copy(target, win));
    CHECK(read_text(target) == "[Inner Space]\r\nPath=D:\\\r\n");
    CHECK_FALSE(first_run_copy(target, fs::path()));
}

TEST_CASE("INI location: LocalPath defaults to the INI's directory, and regeneration uses it") {
    LocTemp t;
    const fs::path ini_path = t.path() / "Inner Space" / "INSPACE.INI";
    CHECK(ini_directory(ini_path) == t.path() / "Inner Space");
    CHECK(ini_directory(fs::path("INSPACE.INI")) == fs::path("."));

    // the game's file has no Path: the first launch regenerates (smoke-0001)
    write_text(ini_path, "[Inner Space]\r\nMainShip=SKY TALON\r\nThePlayer=Your Name\r\n");
    IniFile ini(ini_path);
    const fs::path assets = t.path() / "game";
    REQUIRE(regenerate(ini, assets, ini_directory(ini_path)));
    ini.save();
    const IniFile back(ini_path);
    const std::string path = back.get_string(kGameSection, "Path", "");
    const std::string local = back.get_string(kGameSection, "LocalPath", "");
    CHECK(path.back() == '\\');
    CHECK(back.get_string(kGameSection, "ShipPath", "") == path + "SHIPS\\");
    CHECK(back.get_string(kGameSection, "SndPath", "") == path + "SOUNDS\\");
    CHECK(back.get_string(kGameSection, "RacePath", "") == path + "GAMES\\");
    CHECK(back.get_string(kGameSection, "MainShip", "") == "TRANSFORMER");
    // the keys turn back into the directories
    CHECK(fs::path(path_from_ini(path)).lexically_normal() ==
          fs::path(assets.string() + "/").lexically_normal());
    CHECK(fs::path(path_from_ini(local)).lexically_normal() ==
          fs::path((t.path() / "Inner Space").string() + "/").lexically_normal());
    CHECK(path_from_ini("").empty());
    // a second launch keeps the keys
    IniFile again(ini_path);
    CHECK_FALSE(regenerate(again, t.path() / "other", t.path()));
}

TEST_CASE("INI location: the original's regeneration message") {
    CHECK(std::string(kRegenerateMessage) ==
          "Some game settings are missing. Will attempt to regenerate INSPACE.INI. For best "
          "results, reinstall from installer.");
}

TEST_CASE("settings: the [Registration] keys are read, never written") {
    LocTemp t;
    const fs::path p = t.path() / "INSPACE.INI";
    write_text(p, "[Inner Space]\r\nPath=C:\\G\\\r\n");
    {
        IniFile ini(p);
        RegistrationSettings reg;
        reg.load(ini);
        CHECK_FALSE(reg.present);
        CHECK(reg.vidsp == 0);
        CHECK(reg.bit32sp == 0);
        CHECK(reg.pdev == 0);
    }
    write_text(p, "[Inner Space]\r\nPath=C:\\G\\\r\n[registration]\r\nvidsp=12\r\n32BitSP=-7\r\n"
                  "PDev=700\r\n");
    IniFile ini(p);
    RegistrationSettings reg;
    reg.load(ini);
    CHECK(reg.present);
    CHECK(reg.vidsp == 12);
    CHECK(reg.bit32sp == -7);
    CHECK(reg.pdev == 700);
    // a section without the three values is absent, so the Windows files are read: an empty
    // one, or one holding only the Order dialog's Serial and SerialCount (ADR 0008 decision 2)
    write_text(p, "[Registration]\r\n");
    IniFile empty(p);
    reg.load(empty);
    CHECK_FALSE(reg.present);
    CHECK(reg.vidsp == 0);
    write_text(p, "[Registration]\r\nSerial=5\r\nSerialCount=1\r\n");
    IniFile serial_only(p);
    reg.load(serial_only);
    CHECK_FALSE(reg.present);
    // one value is enough; the others read 0
    write_text(p, "[Registration]\r\nPDev=3\r\n");
    IniFile one(p);
    reg.load(one);
    CHECK(reg.present);
    CHECK(reg.pdev == 3);
    CHECK(reg.vidsp == 0);
    // GameSettings::store writes [Inner Space] only
    GameSettings s = GameSettings::defaults(640);
    s.load(ini, 640);
    s.store(ini);
    ini.save();
    const IniFile back(p);
    CHECK(back.get_int(kRegistrationSection, "vidsp", 0) == 12);
    CHECK(back.entries(kRegistrationSection).size() == 3);
}
