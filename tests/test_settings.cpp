// The INI layer: IniFile's profile semantics over synthetic files, GameSettings' keys and
// defaults (checked against docs/re/formats.md's tables), the seg6:0x6d8 regeneration, and a
// golden read of the shipped INSPACE.INI (IS_ORIGINAL_DIR, work/extracted/shipdisk).
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <set>
#include <string>
#include <vector>

#include "doctest.h"
#include "settings/settings.h"

using namespace is::settings;
namespace fs = std::filesystem;

namespace {

class SettingsTemp {
public:
    SettingsTemp() {
        static unsigned counter = 0;
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = fs::temp_directory_path() /
                ("is_settings_" + std::to_string(stamp) + "_" + std::to_string(++counter));
        fs::create_directories(path_);
    }
    ~SettingsTemp() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }
    SettingsTemp(const SettingsTemp&) = delete;
    SettingsTemp& operator=(const SettingsTemp&) = delete;
    const fs::path& path() const { return path_; }

private:
    fs::path path_;
};

void write_bytes(const fs::path& p, const std::string& s) {
    std::ofstream f(p, std::ios::binary);
    f.write(s.data(), static_cast<std::streamsize>(s.size()));
}

std::string read_bytes(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

IniFile ini_of(const SettingsTemp& t, const std::string& text) {
    const fs::path p = t.path() / "TEST.INI";
    write_bytes(p, text);
    return IniFile(p);
}

fs::path settings_env_dir(const char* name) {
    const char* v = std::getenv(name);
    if (!v || !*v) return {};
    fs::path p(v);
    std::error_code ec;
    return fs::is_directory(p, ec) ? p : fs::path{};
}

// The repository: the nearest directory holding docs/re/formats.md above IS_ASSETS_DIR,
// IS_ORIGINAL_DIR or this source file. Empty when none.
fs::path repo_root() {
    const std::vector<fs::path> starts = {settings_env_dir("IS_ASSETS_DIR"),
                                          settings_env_dir("IS_ORIGINAL_DIR"),
                                          fs::path(__FILE__).parent_path()};
    for (fs::path p : starts) {
        for (int up = 0; up < 4 && !p.empty(); ++up) {
            std::error_code ec;
            if (fs::is_regular_file(p / "docs" / "re" / "formats.md", ec)) return p;
            if (!p.has_relative_path()) break;
            p = p.parent_path();
        }
    }
    return {};
}

GameSettings changed() {
    GameSettings s = GameSettings::defaults(800);
    s.path = "D:\\GAME\\";
    s.the_player = "Pilot \xe9";  // cp1252 e-acute
    s.restore = "SAVE1";
    s.watch_left = 17;
    s.watch_right = 999;
    s.icon_left = 5;
    s.sound_system = -1;
    s.jbtn = {3, 2, 1, 0};
    s.tea = 1600;
    s.track = "LAZYLOOP";
    s.ship2 = "EAGLE";
    s.nav_bottom = -4;
    return s;
}

}  // namespace

TEST_CASE("IniFile: sections, keys, trimming, quotes and case") {
    SettingsTemp t;
    IniFile ini = ini_of(t,
                         "top=1\r\n"
                         "[Inner Space]\r\n"
                         "  Path =  C:\\INSPACE\\  \r\n"
                         "Quoted=\"  spaced  \"\r\n"
                         "Single='x'\r\n"
                         "Mixed=\"x'\r\n"
                         "Lone=\"\r\n"
                         "; comment=not a key\r\n"
                         "novalue\r\n"
                         "=nokey\r\n"
                         "Empty=\r\n"
                         "Dup=first\r\n"
                         "Dup=second\r\n"
                         "[\xe9t\xe9]\r\n"
                         "K=\xe0\r\n"
                         "[inner space]\r\n"
                         "Later=yes\r\n"
                         "Dup=third\n");
    CHECK(ini.existed());
    CHECK(ini.get("", "top") == std::optional<std::string>("1"));
    CHECK(ini.get("Inner Space", "Path") == std::optional<std::string>("C:\\INSPACE\\"));
    CHECK(ini.get("INNER SPACE", "path") == std::optional<std::string>("C:\\INSPACE\\"));
    CHECK(*ini.get("Inner Space", "Quoted") == "  spaced  ");
    CHECK(*ini.get("Inner Space", "Single") == "x");
    CHECK(*ini.get("Inner Space", "Mixed") == "\"x'");
    CHECK(*ini.get("Inner Space", "Lone") == "\"");
    CHECK(!ini.get("Inner Space", "; comment"));
    CHECK(!ini.get("Inner Space", "novalue"));
    CHECK(!ini.get("Inner Space", ""));
    CHECK(*ini.get("Inner Space", "Empty") == "");
    CHECK(*ini.get("Inner Space", "Dup") == "first");
    CHECK(*ini.get("Inner Space", "Later") == "yes");  // a second section of the same name
    CHECK(*ini.get("\xc9T\xc9", "k") == "\xe0");       // cp1252 fold: e-acute = E-acute
    CHECK(!ini.get("Missing", "Path"));
    CHECK(!ini.get("Inner Space", "Missing"));
    CHECK(ini.sections() ==
          std::vector<std::string>{"", "Inner Space", "\xe9t\xe9", "inner space"});
    const auto e = ini.entries("Inner Space");
    REQUIRE(e.size() == 10);
    CHECK(e.front() == std::pair<std::string, std::string>("Path", "C:\\INSPACE\\"));
    CHECK(e.back() == std::pair<std::string, std::string>("Dup", "third"));
}

TEST_CASE("IniFile: nsize truncates to nsize - 1 bytes") {
    SettingsTemp t;
    IniFile ini = ini_of(t, "[S]\r\nRestore=NEW GAME!\r\nQ=\"abcdef\"\r\n");
    CHECK(*ini.get("S", "Restore", 9) == "NEW GAME");
    CHECK(*ini.get("S", "Restore", 1) == "");
    CHECK(*ini.get("S", "Restore", 0) == "NEW GAME!");
    CHECK(*ini.get("S", "Q", 4) == "abc");  // quotes go first
    CHECK(ini.get_string("S", "Missing", "DEFAULTVALUE", 5) == "DEFA");
    CHECK(ini.get_string("S", "Restore", "x") == "NEW GAME!");
    CHECK(ini.get_string("S", "Missing", "x") == "x");
}

TEST_CASE("IniFile: GetPrivateProfileInt parsing") {
    SettingsTemp t;
    IniFile ini = ini_of(t,
                         "[S]\r\n"
                         "a=42\r\nb=-17\r\nc=+8\r\nd=12abc\r\ne=abc\r\nf=\r\ng=   \r\n"
                         "h=\"77\"\r\ni=- 5\r\nj=0x10\r\nk=1 2\r\nl=\t-3x\r\nm=4294967295\r\n");
    CHECK(ini.get_int("S", "a", 9) == 42);
    CHECK(ini.get_int("S", "b", 9) == -17);
    CHECK(ini.get_int("S", "c", 9) == 8);
    CHECK(ini.get_int("S", "d", 9) == 12);
    CHECK(ini.get_int("S", "e", 9) == 0);
    CHECK(ini.get_int("S", "f", 9) == 9);  // empty: the default
    CHECK(ini.get_int("S", "g", 9) == 9);  // trimmed to empty
    CHECK(ini.get_int("S", "h", 9) == 77);
    CHECK(ini.get_int("S", "i", 9) == 0);
    CHECK(ini.get_int("S", "j", 9) == 0);
    CHECK(ini.get_int("S", "k", 9) == 1);
    CHECK(ini.get_int("S", "l", 9) == -3);
    CHECK(ini.get_int("S", "m", 9) == -1);  // 32-bit wrap
    CHECK(ini.get_int("S", "absent", -5) == -5);
    CHECK(ini.get_int("Nope", "a", 7) == 7);
}

TEST_CASE("IniFile: a missing file, set, remove and save") {
    SettingsTemp t;
    const fs::path p = t.path() / "new" / "deeper" / "INSPACE.INI";
    IniFile ini(p);
    CHECK_FALSE(ini.existed());
    CHECK(ini.sections().empty());
    ini.set("Inner Space", "Path", "  C:\\X\\ ");
    ini.set_int("Inner Space", "Size", -2);
    ini.set("Teams", "Team1", "AVENGERS");
    ini.set("inner space", "path", "C:\\Y\\");  // replaces, keeps the key's spelling
    CHECK(*ini.get("Inner Space", "Path") == "C:\\Y\\");
    CHECK(ini.get_int("Inner Space", "Size", 0) == -2);
    ini.save();
    CHECK(read_bytes(p) ==
          "[Inner Space]\r\nPath=C:\\Y\\\r\nSize=-2\r\n\r\n[Teams]\r\nTeam1=AVENGERS\r\n");
    ini.remove("INNER SPACE", "SIZE");
    ini.remove("Inner Space", "Absent");
    ini.remove("Absent", "Path");
    ini.save();
    IniFile again(p);
    CHECK(again.existed());
    CHECK(again.entries("Inner Space") ==
          std::vector<std::pair<std::string, std::string>>{{"Path", "C:\\Y\\"}});
    CHECK(*again.get("Teams", "Team1") == "AVENGERS");
}

TEST_CASE("IniFile: unknown content survives in order") {
    SettingsTemp t;
    const std::string text =
        "; leading comment\r\n"
        "[Ship Factory]\r\n"
        "Ship=DRAGON\r\n"
        "\r\n"
        "[Inner Space]\r\n"
        "MainShip=SKY TALON\r\n"
        "; a note\r\n"
        "Odd line\r\n"
        "Language=ENG\r\n"
        "\r\n"
        "[Teams]\r\n"
        "Team1=AVENGERS\r\n";
    IniFile ini = ini_of(t, text);
    ini.save();
    CHECK(read_bytes(ini.path()) == text);  // unchanged content: byte for byte

    ini.set("Inner Space", "MainShip", "EAGLE");
    ini.set("Inner Space", "Size", "1");     // appended after the last key, before the blank
    ini.remove("Inner Space", "Language");  // the note and the odd line stay
    ini.set("Ship Factory", "ShowLeft", "20");
    ini.set("New", "K", "V");
    ini.save();
    CHECK(read_bytes(ini.path()) ==
          "; leading comment\r\n"
          "[Ship Factory]\r\n"
          "Ship=DRAGON\r\n"
          "ShowLeft=20\r\n"
          "\r\n"
          "[Inner Space]\r\n"
          "MainShip=EAGLE\r\n"
          "; a note\r\n"
          "Odd line\r\n"
          "Size=1\r\n"
          "\r\n"
          "[Teams]\r\n"
          "Team1=AVENGERS\r\n"
          "\r\n"
          "[New]\r\n"
          "K=V\r\n");
}

TEST_CASE("IniFile: save into an unwritable place throws") {
    SettingsTemp t;
    const fs::path blocker = t.path() / "file";
    write_bytes(blocker, "x");
    IniFile ini(blocker / "INSPACE.INI");  // its directory is a file
    ini.set("S", "K", "V");
    CHECK_THROWS_AS(ini.save(), std::runtime_error);
}

TEST_CASE("GameSettings: every default") {
    const GameSettings d = GameSettings::defaults(1024);
    // Paths, identity and game choice
    CHECK(d.path.empty());
    CHECK(d.local_path.empty());
    CHECK(d.ship_path.empty());
    CHECK(d.snd_path.empty());
    CHECK(d.race_path.empty());
    CHECK(d.games.empty());
    CHECK(d.dead.empty());
    CHECK(d.language.empty());
    CHECK(d.the_player.empty());
    CHECK(d.main_ship == "EAGLE");
    CHECK(d.restore.empty());
    CHECK(d.play_mode == 0);
    CHECK(d.drive == "C");
    CHECK(d.size == 1);
    CHECK(d.set_size == 1);
    CHECK(d.strat_ask == 0);
    CHECK(d.save_prompt == 1);
    CHECK(d.accept == 0);
    CHECK(d.plus_count == 13);
    // Window state and positions
    CHECK(d.full == 1);
    CHECK(d.on_top == 1);
    CHECK(d.show_left == 0);
    CHECK(d.show_top == 0);
    CHECK(d.show_right == 0);
    CHECK(d.show_bottom == 0);
    CHECK(d.icon_wnd == 1);
    CHECK(d.vscale == 0);
    CHECK(d.sm_wnd == 0);
    CHECK(d.sscale == 0);
    CHECK(d.show_launch == 2);
    CHECK(d.menu == 0);
    CHECK(d.radar_left == 0);
    CHECK(d.radar_top == 0);
    CHECK(d.watch_left == 0);
    CHECK(d.watch_top == 0);
    CHECK(d.watch_right == 280);
    CHECK(d.icon_left == 976);  // newgame-0001 at 1024 pixels
    CHECK(d.icon_top == 0);
    CHECK(d.sm_left == 0);
    CHECK(d.sm_top == 0);
    CHECK(d.help_left == 0);
    CHECK(d.help_top == 0);
    CHECK(d.nav_left == 30);
    CHECK(d.nav_top == 30);
    CHECK(d.nav_right == 560);
    CHECK(d.nav_bottom == 350);
    // Settings
    CHECK(d.game_speed == 20000);
    CHECK(d.frame_rate_set == 36);
    CHECK(d.calibrate == 0);
    CHECK(d.detail == 3);
    CHECK(d.quick == 0);
    CHECK(d.enhance == 1);
    CHECK(d.complex == 0);
    CHECK(d.braking == 0);
    CHECK(d.mix_channels == 1);
    CHECK(d.bad_video == 1);
    CHECK(d.quick_intro == 0);
    CHECK(d.ship_anim == 1);
    CHECK(d.border == 1);
    CHECK(d.bright == 0);
    CHECK(d.ignore_res == 0);
    CHECK(d.aux == 0);
    CHECK(d.rate == 0);
    CHECK(d.joystick == 0);
    CHECK(d.four_btns == 0);
    CHECK(d.jmin_x == 0);
    CHECK(d.jmin_y == 0);
    CHECK(d.jrange_x == 0);
    CHECK(d.jrange_y == 0);
    CHECK(d.jbtn == std::array<int, 4>{0, 1, 2, 3});
    // Sound
    CHECK(d.sound_fx == 1);
    CHECK(d.voice_fx == 1);
    CHECK(d.music == 1);
    CHECK(d.sound_volume == 100);
    CHECK(d.fx_music == 50);
    CHECK(d.stereo_effect == 2);
    CHECK(d.purge_sound == 1);
    CHECK(d.stereo_swap == 0);
    CHECK(d.sound_system == -1);
    CHECK(d.allow_sound == 0);
    CHECK(d.force_vol == 0);
    CHECK(d.saver_volume == 0);
    // Special times
    CHECK(d.breakfast == 800);
    CHECK(d.coffee == 1000);
    CHECK(d.lunch == 1200);
    CHECK(d.tea == 1530);
    // Race, duel and demo options
    CHECK(d.racers == 1);
    CHECK(d.race_diff == 0);
    CHECK(d.race_laps == 1);
    CHECK(d.race_opp == 2);
    CHECK(d.duelers == 0);
    CHECK(d.duel_diff == 0);
    CHECK(d.duel_opp == 2);
    CHECK(d.dracers == 0);
    CHECK(d.ddiff == 0);
    CHECK(d.dlaps == 0);
    CHECK(d.demo_type == 519);
    CHECK(d.dscale == 0);
    CHECK(d.track.empty());
    CHECK(d.ship1.empty());
    CHECK(d.ship2.empty());
    CHECK(d.tree == 0);

    // An empty INI loads exactly the defaults.
    SettingsTemp t;
    IniFile empty(t.path() / "NONE.INI");
    GameSettings loaded = changed();
    loaded.load(empty, 1024);
    IniFile a(t.path() / "A.INI"), b(t.path() / "B.INI");
    loaded.store(a);
    d.store(b);
    CHECK(a.entries(kGameSection) == b.entries(kGameSection));
}

TEST_CASE("GameSettings: derived defaults") {
    CHECK(GameSettings::default_icon_left(640) == 592);
    CHECK(GameSettings::default_watch_right(100) == 380);
    SettingsTemp t;
    IniFile ini = ini_of(t, "[Inner Space]\r\nWatchLeft=100\r\n");
    GameSettings s;
    s.load(ini, 800);
    CHECK(s.watch_right == 380);
    CHECK(s.icon_left == 752);
    IniFile ini2 = ini_of(t, "[Inner Space]\r\nWatchLeft=100\r\nWatchRight=7\r\nIconLeft=3\r\n");
    s.load(ini2, 800);
    CHECK(s.watch_right == 7);
    CHECK(s.icon_left == 3);
}

TEST_CASE("GameSettings: read sizes truncate as INSPACE reads") {
    SettingsTemp t;
    IniFile ini = ini_of(t,
                         "[Inner Space]\r\nRestore=ABCDEFGHIJ\r\nLanguage=ENGLISH\r\n"
                         "Dead=0123456789ABC\r\nThePlayer=ABCDEFGHIJKLMNOPQRSTUVWXYZ\r\n");
    GameSettings s;
    s.load(ini, 640);
    CHECK(s.restore == "ABCDEFGH");
    CHECK(s.language == "ENGL");
    CHECK(s.dead == "0123456789");
    CHECK(s.the_player == "ABCDEFGHIJKLMNOPQRS");
}

TEST_CASE("GameSettings: store and load round trip") {
    SettingsTemp t;
    const GameSettings s = changed();
    IniFile ini(t.path() / "RT.INI");
    s.store(ini);
    ini.save();
    IniFile back(t.path() / "RT.INI");
    GameSettings r;
    r.load(back, 1024);
    IniFile x(t.path() / "X.INI"), y(t.path() / "Y.INI");
    s.store(x);
    r.store(y);
    CHECK(x.entries(kGameSection) == y.entries(kGameSection));
    CHECK(r.the_player == "Pilot \xe9");
    CHECK(r.watch_right == 999);
    CHECK(r.jbtn == std::array<int, 4>{3, 2, 1, 0});
    CHECK(back.get_int(kGameSection, "SoundSystem", 0) == -1);
    CHECK(*back.get(kGameSection, "4Btns") == "0");
    CHECK(back.entries(kGameSection).size() == GameSettings::keys().size());
}

TEST_CASE("GameSettings: the key list equals formats.md's [Inner Space] tables") {
    const fs::path root = repo_root();
    if (root.empty()) {
        MESSAGE("skipped: docs/re/formats.md not found");
        return;
    }
    const std::string doc = read_bytes(root / "docs" / "re" / "formats.md");
    const std::string head = "### INSPACE.INI [Inner Space]";
    const std::size_t begin = doc.find(head);
    REQUIRE(begin != std::string::npos);
    const std::size_t end = doc.find("\n### ", begin + head.size());
    const std::string section = doc.substr(begin, end - begin);

    // The backticked names in the first cell of every table row; `A0`…`A3` is a range.
    std::vector<std::string> doc_keys;
    const std::regex name("`([^`]+)`");
    const std::regex range("^`([A-Za-z]+)([0-9]+)`\xe2\x80\xa6`\\1([0-9]+)`$");
    std::size_t pos = 0;
    while (pos < section.size()) {
        std::size_t nl = section.find('\n', pos);
        if (nl == std::string::npos) nl = section.size();
        const std::string line = section.substr(pos, nl - pos);
        pos = nl + 1;
        if (line.rfind("| ", 0) != 0) continue;
        const std::size_t bar = line.find('|', 1);
        const std::string cell = line.substr(1, bar - 1);
        std::string trimmed = cell;
        trimmed.erase(0, trimmed.find_first_not_of(' '));
        trimmed.erase(trimmed.find_last_not_of(' ') + 1);
        std::smatch m;
        if (std::regex_match(trimmed, m, range)) {
            for (int n = std::stoi(m[2]); n <= std::stoi(m[3]); ++n)
                doc_keys.push_back(m[1].str() + std::to_string(n));
            continue;
        }
        for (std::sregex_iterator it(cell.begin(), cell.end(), name), stop; it != stop; ++it)
            doc_keys.push_back((*it)[1].str());
    }
    std::vector<std::string> code_keys;
    for (std::string_view k : GameSettings::keys()) code_keys.emplace_back(k);
    CHECK(doc_keys.size() > 100);
    CHECK(doc_keys == code_keys);
    CHECK(std::set<std::string>(code_keys.begin(), code_keys.end()).size() == code_keys.size());
}

TEST_CASE("regenerate: an empty Path writes the ten keys") {
    SettingsTemp t;
    IniFile ini(t.path() / "INSPACE.INI");
    ini.set(kGameSection, "Path", "");
    ini.set(kGameSection, "FrameRateSet", "36");
#ifdef _WIN32
    const fs::path asset = L"C:\\INSPACE";
    const fs::path local = L"C:\\Users\\p\\Saved Games\\INSPACE\\";
#else
    const fs::path asset = "C:/INSPACE";
    const fs::path local = "C:/Users/p/Saved Games/INSPACE/";
#endif
    CHECK(regenerate(ini, asset, local));
    CHECK(ini.entries(kGameSection) ==
          std::vector<std::pair<std::string, std::string>>{
              {"Path", "C:\\INSPACE\\"},
              {"FrameRateSet", "36"},
              {"LocalPath", "C:\\Users\\p\\Saved Games\\INSPACE\\"},
              {"ShipPath", "C:\\INSPACE\\SHIPS\\"},
              {"SndPath", "C:\\INSPACE\\SOUNDS\\"},
              {"RacePath", "C:\\INSPACE\\GAMES\\"},
              {"Dead", "RETIRED"},
              {"Games", "GAMES"},
              {"Language", "ENG"},
              {"MainShip", "TRANSFORMER"},
              {"ThePlayer", "your name"},
          });

    // An absent Path counts as empty; a cp1252 letter survives as one byte.
    IniFile fresh(t.path() / "FRESH.INI");
    CHECK(regenerate(fresh, fs::path(u"C:\\Jeux\\\u00e9t\u00e9"), fs::path(u"C:\\L")));
    CHECK(*fresh.get(kGameSection, "Path") == "C:\\Jeux\\\xe9t\xe9\\");
    CHECK(*fresh.get(kGameSection, "LocalPath") == "C:\\L\\");
    CHECK(fresh.entries(kGameSection).size() == 10);

    // A directory cp1252 cannot hold is refused, not stored lossily.
    IniFile bad(t.path() / "BAD.INI");
    CHECK_THROWS_AS(regenerate(bad, fs::path(u"C:\\\u4e2d"), fs::path(u"C:\\L")),
                    std::runtime_error);
}

TEST_CASE("regenerate: a non-empty Path changes nothing") {
    SettingsTemp t;
    const std::string text = "[Inner Space]\r\nPath=C:\\GAME\\\r\nMainShip=EAGLE\r\n";
    IniFile ini = ini_of(t, text);
    CHECK_FALSE(regenerate(ini, fs::path("C:/OTHER"), fs::path("C:/LOCAL")));
    CHECK(ini.entries(kGameSection) ==
          std::vector<std::pair<std::string, std::string>>{{"Path", "C:\\GAME\\"},
                                                           {"MainShip", "EAGLE"}});
    ini.save();
    CHECK(read_bytes(ini.path()) == text);
}

TEST_CASE("golden: the shipped INSPACE.INI") {
    const fs::path dir = settings_env_dir("IS_ORIGINAL_DIR");
    if (dir.empty() || !fs::is_regular_file(dir / "INSPACE.INI")) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    const IniFile ini(dir / "INSPACE.INI");
    REQUIRE(ini.existed());
    CHECK(ini.sections() == std::vector<std::string>{"Ship Factory", "Inner Space", "Teams"});
    // timing.md: the shipped INSPACE.INI sets FrameRateSet=36.
    CHECK(ini.get_int(kGameSection, "FrameRateSet", 0) == 36);
    CHECK(*ini.get(kGameSection, "Language") == "ENG");
    CHECK(*ini.get(kGameSection, "MainShip") == "SKY TALON");
    CHECK(*ini.get(kGameSection, "ThePlayer") == "Your Name");
    CHECK(ini.get_int(kGameSection, "PlayMode", 9) == 0);
    CHECK(*ini.get("Ship Factory", "Ship") == "DRAGON");
    // formats.md [Teams]: the shipped INI names the eight teams.
    const auto teams = ini.entries("Teams");
    REQUIRE(teams.size() == 8);
    for (std::size_t i = 0; i < 8; ++i) {
        CHECK(teams[i].first == "Team" + std::to_string(i + 1));
        CHECK(!teams[i].second.empty());
        CHECK(teams[i].second.size() <= 19);
    }
    CHECK(teams[0].second == "AVENGERS");
    // It has no Path, so the first launch regenerates (smoke-0001).
    GameSettings s;
    s.load(ini, 1024);
    CHECK(s.path.empty());
    CHECK(s.frame_rate_set == 36);
    CHECK(s.main_ship == "SKY TALON");

    // Saving it unchanged reproduces its bytes.
    SettingsTemp t;
    IniFile copy(t.path() / "INSPACE.INI");
    write_bytes(copy.path(), read_bytes(dir / "INSPACE.INI"));
    IniFile reread(copy.path());
    reread.save();
    CHECK(read_bytes(copy.path()) == read_bytes(dir / "INSPACE.INI"));
}
