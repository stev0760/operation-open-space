// The install step (src/install, ADR 0008 decision 1) against docs/re/install.md: the INSTALL.INI
// parser on the ship disk's file, the [Vers] V rule, the record writers and a full install of
// IS_ORIGINAL_DIR's ship disk (original/shipdisk next to work/extracted/shipdisk) into a temp
// directory, compared byte for byte with work/extracted/shipdisk and read back by the edition
// check. The random values are generated at test time and never printed; the name is made up.
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "doctest.h"
#include "formats/bytes.h"
#include "formats/ne.h"
#include "install/install.h"
#include "install/install_ini.h"
#include "settings/ini_location.h"
#include "settings/settings.h"
#include "shell/edition.h"
#include "shell/options.h"

namespace fs = std::filesystem;
using namespace is::install;

namespace {

fs::path env_dir(const char* name) {
    const char* v = std::getenv(name);
    if (!v || !*v) return {};
    fs::path p(v);
    std::error_code ec;
    return fs::is_directory(p, ec) ? p : fs::path();
}

// The extracted ship disk (IS_ORIGINAL_DIR) and original/shipdisk beside the repository's work/.
fs::path extracted_dir() { return env_dir("IS_ORIGINAL_DIR"); }
fs::path shipdisk_dir() {
    const fs::path ex = extracted_dir();
    if (ex.empty()) return {};
    const fs::path p = ex.parent_path().parent_path().parent_path() / "original" / "shipdisk";
    std::error_code ec;
    return fs::is_directory(p, ec) ? p : fs::path();
}

std::vector<std::uint8_t> bytes_of(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(f)),
                                     std::istreambuf_iterator<char>());
}

std::string fold(const std::string& s) { return is::fmt::fold_name(s); }

struct TempDir {
    fs::path path;
    TempDir() {
        path = fs::temp_directory_path() /
               ("is_install_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + "_" +
                std::to_string(std::rand()));
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

std::vector<std::uint8_t> resource(const fs::path& exe, std::uint16_t type, std::uint16_t id,
                                   std::size_t n) {
    const is::fmt::NeModule m = is::fmt::NeModule::load(exe);
    const is::fmt::NeResource* r = m.find(type, id);
    REQUIRE(r != nullptr);
    return std::vector<std::uint8_t>(m.data(*r), m.data(*r) + std::min(n, m.size(*r)));
}

}  // namespace

TEST_CASE("install: [Vers] V rule (INSTALL.EXE seg2:0x4a92)") {
    CHECK(version_number("12908") == 12908);
    CHECK(version_number("1290899") == 12908);  // characters after the fifth weigh 0
    CHECK(version_number("") == 0);
    CHECK(version_number("00000") == 0);
    CHECK(version_number("1") == 10000);
    CHECK(version_number("A") == static_cast<std::int16_t>(17 * 10000));  // no digit check
    CHECK(version_label("12908") == "V1.29.08");
    CHECK(version_label("12") == "V1.2");
}

TEST_CASE("install: speed test thresholds and texts") {
    CHECK(detail_from_polls(350001) == 3);
    CHECK(detail_from_polls(350000) == 2);
    CHECK(detail_from_polls(180001) == 2);
    CHECK(detail_from_polls(180000) == 1);
    CHECK(detail_from_polls(70001) == 1);
    CHECK(detail_from_polls(70000) == 0);
    std::uint32_t t = 5000;
    // One poll per tick: the do-while counts 1000 polls before tick reaches t0 + 1000.
    CHECK(speed_polls([&] { return t++; }) == 1000);
    CHECK(the_player("Jo Bloggs") == "Jo");
    CHECK(the_player("Jo") == "Jo");
    CHECK(cut_text("ABCDEFGHIJKLMNOPQRSTUVWXYZ", 0x12) == "ABCDEFGHIJKLMNOPQ");
    CHECK(name_org("A", "B") == "A - B");
    CHECK(complete_dir("D") == "D:\\");
    CHECK(complete_dir("C:\\SPACE") == "C:\\SPACE\\");
    CHECK(required_kb({true, true, true, true, true, true}) == 1766);
    CHECK(required_kb({false, false, false, false, false, false}) == 46);
}

TEST_CASE("install: record writers (formats.md Resources rewritten in place)") {
    is::shell::Rng rng;
    const std::string text = "Test Person - Test Org";
    const auto rec = name_record(text, rng, 0x1234);
    REQUIRE(rec.size() == 50);
    std::string back;
    CHECK(is::shell::read_name(rec, true, &back) == 1);
    CHECK(back == text);
    const auto ir = install_record(rng, 3);
    REQUIRE(ir.size() == 10);
    const std::uint32_t d = ir[0] | (ir[1] << 8) | (ir[2] << 16) | (static_cast<std::uint32_t>(ir[3]) << 24);
    CHECK(d >= 5432u);
    CHECK(d < 5432u + 7651u);
    CHECK(ir[4] == 3);
    CHECK(ir[6] == 0);
    const RegValues v = ini_values(0x17, record_x(ir), {}, rng, 0x4321);
    CHECK(v.vidsp >= 1987);
    CHECK(v.vidsp <= 12095);
    CHECK(is::shell::record_check(ir, v.vidsp, v.pdev) == 0);
    CHECK(v.bit32sp != static_cast<std::int16_t>(2 * v.vidsp - v.pdev - 1231));
    // Mode 0x54 keeps stored non-zero values.
    const RegValues kept = ini_values(0x54, 100, {5000, 6000, 7000}, rng, 1);
    CHECK(kept.vidsp == 5000);
    CHECK(kept.pdev == 6000);
    CHECK(kept.bit32sp == 7000);
    const RegValues fresh = ini_values(0x54, 100, {}, rng, 1);
    CHECK(fresh.pdev - fresh.vidsp == (100 ^ 0xff));
}

TEST_CASE("install: INSTALL.INI of the ship disk") {
    const fs::path sd = shipdisk_dir();
    if (sd.empty()) return;
    const InstallIni ini = load_install_ini(sd / "INSTALL.INI");
    CHECK(ini.version == 12908);
    CHECK(ini.dirs == std::vector<std::string>{"SHIPS", "SOUNDS", "GAMES", "RETIRED"});
    REQUIRE(ini.groups[0].size() == 5);
    CHECK(ini.groups[0][0].name == "MASTER.BOD");
    CHECK(ini.groups[0][0].code == 1);
    CHECK(ini.groups[0][2].name == "WAVEMIX.DLL");
    CHECK(ini.groups[0][2].code == 8);
    CHECK(ini.groups[1].size() == 8);
    CHECK(ini.groups[1][4].name == "INSTALL.EXE");
    CHECK(ini.groups[5].size() == 4);
    CHECK(ini.groups[6][1].name == "INRUN.AD");
    CHECK(ini.groups[6][1].code == 9);
    REQUIRE(ini.groups[7].size() == 1);
    CHECK(ini.groups[7][0].name == "INSPACE.INI");
}

TEST_CASE("install: a full install of the ship disk registers") {
    const fs::path sd = shipdisk_dir();
    const fs::path ex = extracted_dir();
    if (sd.empty() || ex.empty()) return;
    TempDir tmp;
    std::vector<std::string> logs;
    const OpenResult o = open_source(sd, tmp.path / "work", [&](const std::string& s) { logs.push_back(s); });
    REQUIRE(o.set.has_value());
    const SourceSet& set = *o.set;
    CHECK(set.label == "V1.29.08");
    CHECK(set.new_install);
    CHECK(set.rule == Rule::kRegister);

    const fs::path game = tmp.path / "SPACE";
    const fs::path ini_path = tmp.path / "appdata" / "INSPACE.INI";
    std::vector<std::string> asked;
    std::vector<int> percents;
    std::string done_text;
    std::uint32_t clock = 1000;
    Ui ui;
    ui.message = [&](const std::string& t, unsigned) {
        asked.push_back(t);
        return kIdNo;
    };
    ui.components = [&](Components& c) {
        CHECK(c.checked == std::array<bool, 6>{true, true, true, true, true, true});
        c.dir = is::fmt::path_utf8(game) + "/";
        return true;
    };
    ui.difficulty = [](int& d, int& s) {
        CHECK(d == 144);
        CHECK(s == 138);
    };
    ui.window = [] { return true; };
    ui.personalize = [](std::string& n, std::string& o) {
        n = "Test Person";
        o = "Test Org";
    };
    ui.progress = [&](const std::string&, int p) { percents.push_back(p); };
    ui.done = [&](const std::string& t, const fs::path&) { done_text = t; };
    ui.tick = [&] { return clock += 1; };
    const Result r = run_install(set, ini_path, kDefaultDir, ui);
    CHECK(r.completed);
    CHECK(r.registered);
    CHECK(done_text == kDoneValid);
    REQUIRE(asked.size() == 2);
    CHECK(asked[0] == kMsgExitPrograms);
    CHECK(asked[1] == kMsgScreenSaver);
    CHECK(percents.back() == 100);

    // Every produced file equals work/extracted/shipdisk, except the two records of INSPACE.EXE
    // and INSTALL.EXE's copy BACKUP.EXE.
    std::set<std::string> want = {
        "MASTER.BOD", "MASTER.SHP", "WAVEMIX.DLL", "WAVEMIX.INI", "CTL3D.DLL",  "ISPACE.DLL",
        "ENG_LANG.DLL", "IGRAPHIC.DLL", "INSPACE.EXE", "INSTALL.EXE", "ENG_HELP.HLP",
        "README.TXT", "ORDER.TXT", "FACTORY.EXE", "SND_FX.DLL", "ENG_FX.DLL", "MAIN.MID",
        "RACE.MID", "DEMON.MID", "ANTHEM.MID", "INSPACE.SCR", "INRUN.AD"};
    std::set<std::string> seen;
    for (const auto& e : fs::recursive_directory_iterator(game)) {
        if (!e.is_regular_file()) continue;
        std::string n = fold(e.path().filename().string());
        if (n == "BACKUP.EXE") n = "INSTALL.EXE";
        seen.insert(n);
        fs::path ref;
        for (const auto& x : fs::directory_iterator(ex))
            if (fold(x.path().filename().string()) == n) ref = x.path();
        REQUIRE_MESSAGE(!ref.empty(), n);
        auto got = bytes_of(e.path());
        auto exp = bytes_of(ref);
        if (n == "INSPACE.EXE") {
            REQUIRE(got.size() == exp.size());
            // The records sit at 0x4b840 (10 bytes) and 0x4b860 (50 bytes) [ORACLE install-0001].
            for (std::size_t i = 0; i < got.size(); ++i)
                if ((i >= 0x4b840 && i < 0x4b84a) || (i >= 0x4b860 && i < 0x4b892)) got[i] = exp[i];
        }
        CHECK_MESSAGE(got == exp, n);
    }
    CHECK(seen == want);
    // Layout: SHIPS and SOUNDS hold their groups; GAMES and RETIRED exist.
    CHECK(fs::exists(game / "SHIPS" / "MASTER.BOD"));
    CHECK(fs::exists(game / "SOUNDS" / "ENG_FX.DLL"));
    CHECK(fs::is_directory(game / "GAMES"));
    CHECK(fs::is_directory(game / "RETIRED"));

    // The INI keys of install-0001's list, in its order (SoundSystem and Calibrate come from
    // ISPACE.DLL and the game, not from the installer's own writes).
    is::settings::IniFile ini(ini_path);
    std::vector<std::string> keys;
    for (const auto& kv : ini.entries("Inner Space")) keys.push_back(kv.first);
    const std::vector<std::string> expect_keys = {
        "MainShip", "ThePlayer", "PlayMode", "FrameRateSet", "Language", "LocalPath", "Path",
        "ShipPath", "SndPath", "Games", "RacePath", "Dead", "Complex", "Full", "Detail"};
    CHECK(keys == expect_keys);
    CHECK(ini.get_string("Inner Space", "ThePlayer", "") == "Test");
    CHECK(ini.get_int("Inner Space", "Complex", -1) == 2);
    CHECK(ini.get_int("Inner Space", "FrameRateSet", -1) == 36);
    CHECK(ini.get_int("Inner Space", "Full", -1) == 1);
    CHECK(is::settings::path_from_ini(ini.get_string("Inner Space", "Path", "")) ==
          is::settings::path_from_ini(ini_dir(game)));
    CHECK(ini.get_string("Inner Space", "Games", "") == "GAMES");
    CHECK(ini.get_string("Inner Space", "Dead", "") == "RETIRED");
    CHECK(ini.get_string("Teams", "Team1", "") == "AVENGERS");

    // The edition check reads the new copy as registered (ADR 0008 acceptance).
    is::settings::RegistrationSettings reg;
    reg.load(ini);
    REQUIRE(reg.present);
    is::shell::EditionInputs in;
    const fs::path exe = game / "INSPACE.EXE";
    in.install_record = resource(exe, 5500, 110, 10);
    in.name_record = resource(exe, 5000, 143, 50);
    in.name_found = true;
    in.vidsp = reg.vidsp;
    in.bit32sp = reg.bit32sp;
    in.pdev = reg.pdev;
    in.tm_year = 126;
    in.tm_yday = 295;
    const is::shell::Edition e = is::shell::edition_check(is::shell::Edition{}, 1, in);
    CHECK(e.edition == -1);
    CHECK(e.plus != 0);
    CHECK(e.invalid == 0);
    CHECK(e.name == "Test Person - Test Org");
}

TEST_CASE("install: an unrecognised installer starts a trial") {
    const fs::path sd = shipdisk_dir();
    if (sd.empty()) return;
    TempDir tmp;
    // A copy of the set whose INSTALL.EXE differs in one byte past its resources.
    const fs::path src = tmp.path / "set";
    fs::create_directories(src);
    for (const auto& e : fs::directory_iterator(sd))
        fs::copy_file(e.path(), src / e.path().filename());
    for (const auto& e : fs::directory_iterator(src))
        if (fold(e.path().filename().string()) == "INSTALL.EXE") {
            std::ofstream f(e.path(), std::ios::binary | std::ios::app);
            f.put('\0');
        }
    const OpenResult o = open_source(src, tmp.path / "work", {});
    REQUIRE(o.set.has_value());
    CHECK(o.set->rule == Rule::kTrial);
    const fs::path game = tmp.path / "G";
    const fs::path ini_path = tmp.path / "appdata" / "INSPACE.INI";
    std::uint32_t clock = 0;
    std::string done_text;
    Ui ui;
    ui.message = [](const std::string&, unsigned) { return kIdNo; };
    ui.components = [&](Components& c) {
        c.dir = is::fmt::path_utf8(game) + "/";
        return true;
    };
    ui.personalize = [](std::string& n, std::string& o2) {
        n = "Test";
        o2 = "Org";
    };
    ui.done = [&](const std::string& t, const fs::path&) { done_text = t; };
    ui.tick = [&] { return clock += 7; };
    // The date is injected, so the check below cannot straddle midnight.
    ui.date = [](int& y, int& d) {
        y = 126;
        d = 295;
    };
    const Result r = run_install(*o.set, ini_path, kDefaultDir, ui);
    CHECK(r.completed);
    CHECK(!r.registered);
    CHECK(done_text == kDoneDemo);
    is::settings::IniFile ini(ini_path);
    is::settings::RegistrationSettings reg;
    reg.load(ini);
    REQUIRE(reg.present);
    const fs::path exe = game / "INSPACE.EXE";
    is::shell::EditionInputs in;
    in.install_record = resource(exe, 5500, 110, 10);
    in.name_record = resource(exe, 5000, 143, 50);
    in.name_found = true;
    in.vidsp = reg.vidsp;
    in.bit32sp = reg.bit32sp;
    in.pdev = reg.pdev;
    in.tm_year = 126;
    in.tm_yday = 295;
    const is::shell::Edition e = is::shell::edition_check(is::shell::Edition{}, 1, in);
    CHECK(e.edition == 1);  // the trial, day 1
    CHECK(e.day == 1);
    // The step marks its own game directory [DESIGN].
    CHECK(is::settings::path_from_ini(ini.get_string("Port", kInstalledKey, "")) ==
          is::settings::path_from_ini(ini_dir(game)));
}

namespace {
// A copy of the ship disk whose INSTALL.EXE is not recognised (a trial installer).
fs::path trial_set(const fs::path& sd, const fs::path& to) {
    fs::create_directories(to);
    for (const auto& e : fs::directory_iterator(sd)) fs::copy_file(e.path(), to / e.path().filename());
    for (const auto& e : fs::directory_iterator(to))
        if (fold(e.path().filename().string()) == "INSTALL.EXE") {
            std::ofstream f(e.path(), std::ios::binary | std::ios::app);
            f.put('\0');
        }
    return to;
}

is::shell::Edition edition_of(const fs::path& game, const fs::path& ini_path, int yday) {
    const is::settings::IniFile ini(ini_path);
    is::settings::RegistrationSettings reg;
    reg.load(ini);
    REQUIRE(reg.present);
    const fs::path exe = game / "INSPACE.EXE";
    is::shell::EditionInputs in;
    in.install_record = resource(exe, 5500, 110, 10);
    in.name_record = resource(exe, 5000, 143, 50);
    in.name_found = true;
    in.vidsp = reg.vidsp;
    in.bit32sp = reg.bit32sp;
    in.pdev = reg.pdev;
    in.tm_year = 126;
    in.tm_yday = yday;
    return is::shell::edition_check(is::shell::Edition{}, 1, in);
}
}  // namespace

TEST_CASE("install: a reinstall keeps the trial day; a foreign game directory is refused") {
    const fs::path sd = shipdisk_dir();
    if (sd.empty()) return;
    TempDir tmp;
    const OpenResult o = open_source(trial_set(sd, tmp.path / "set"), tmp.path / "work", {});
    REQUIRE(o.set.has_value());
    const fs::path game = tmp.path / "G";
    const fs::path ini_path = tmp.path / "appdata" / "INSPACE.INI";
    std::uint32_t clock = 0;
    int yday = 290;
    std::vector<std::string> asked;
    std::vector<fs::path> offered;  // what dialog 300 answers, in turn
    Ui ui;
    ui.message = [&](const std::string& t, unsigned) {
        asked.push_back(t);
        // "<dir> already exists ...": Yes; "installed before, keep preferences?": No.
        return t.find(kMsgExistsTail) != std::string::npos ? kIdYes : kIdNo;
    };
    ui.components = [&](Components& c) {
        REQUIRE(!offered.empty());
        c.dir = is::fmt::path_utf8(offered.front()) + "/";
        offered.erase(offered.begin());
        return true;
    };
    ui.personalize = [](std::string& n, std::string& o2) {
        n = "Test";
        o2 = "Org";
    };
    ui.tick = [&] { return clock += 7; };
    ui.date = [&](int& y, int& d) {
        y = 126;
        d = yday;
    };

    offered = {game};
    Result r = run_install(*o.set, ini_path, kDefaultDir, ui);
    REQUIRE(r.completed);
    CHECK(edition_of(game, ini_path, 295).day == 6);

    // A second install into the step's own directory five days later: the trial day goes on
    // (the original's SYSTEM.INI/WIN.INI values survive a reinstall) [DESIGN ADR 0008].
    yday = 295;
    offered = {game};
    r = run_install(*o.set, ini_path, kDefaultDir, ui);
    REQUIRE(r.completed);
    const is::shell::Edition e = edition_of(game, ini_path, 295);
    CHECK(e.edition == 1);
    CHECK(e.day == 6);

    // A directory holding a game the step did not install is refused, and dialog 300 opens
    // again [DESIGN ADR 0008 decision 1].
    const fs::path foreign = tmp.path / "OTHER";
    fs::create_directories(foreign);
    { std::ofstream(foreign / "inspace.exe", std::ios::binary).put('M'); }
    const fs::path fresh = tmp.path / "NEW";
    asked.clear();
    offered = {foreign, fresh};
    r = run_install(*o.set, ini_path, kDefaultDir, ui);
    CHECK(r.completed);
    CHECK(r.game_dir == dir_path(ini_dir(fresh)));
    CHECK(offered.empty());
    CHECK(fs::file_size(foreign / "inspace.exe") == 1);
    bool refused = false;
    for (const std::string& a : asked)
        if (a.find(kMsgForeignGameTail) != std::string::npos) refused = true;
    CHECK(refused);
    // The first game directory is no longer the step's own.
    asked.clear();
    offered = {game, tmp.path / "NEW2"};
    r = run_install(*o.set, ini_path, kDefaultDir, ui);
    CHECK(r.completed);
    CHECK(offered.empty());
}

TEST_CASE("install: --install and --to options") {
    using is::shell::parse_options;
    const auto ok = parse_options({"--install", "D:\\SET", "--to", "C:\\GAME"});
    CHECK(ok.error.empty());
    REQUIRE(ok.options.install.has_value());
    CHECK(ok.options.to.has_value());
    CHECK(!parse_options({"--to", "C:\\GAME"}).error.empty());
    CHECK(!parse_options({"--install", "A", "--game", "B"}).error.empty());
    CHECK(!parse_options({"--install"}).error.empty());
}
