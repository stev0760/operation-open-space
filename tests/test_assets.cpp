// The asset store: name restoration, GameFiles over synthetic installed and distribution
// directories, discovery, the known-file table and identify(), and AssetStore's typed
// accessors. The golden tests open the installed copy (IS_ORIGINAL_DIR, work/extracted/
// shipdisk) and the distribution (original/shipdisk), plus the THEWORKS trees next to them,
// and check the generated table against original.sha256. ctest passes IS_ORIGINAL_DIR and
// IS_ASSETS_DIR only; the distribution is IS_DIST_DIR when that is set, else original/
// shipdisk of the repository above IS_ASSETS_DIR or IS_ORIGINAL_DIR (original_root()).
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "assets/asset_store.h"
#include "assets/known_files.h"
#include "doctest.h"
#include "formats/sha256.h"
#include "formats/szdd.h"
#include "ne_builder.h"

using namespace is::assets;
using namespace is::fmt;
namespace fs = std::filesystem;

namespace {

using Bytes = std::vector<std::uint8_t>;

void app16(Bytes& b, unsigned v) {
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
}
void app32(Bytes& b, std::uint32_t v) {
    app16(b, v & 0xFFFF);
    app16(b, v >> 16);
}
void apps(Bytes& b, const std::string& s, bool nul = true) {
    b.insert(b.end(), s.begin(), s.end());
    if (nul) b.push_back(0);
}
Bytes bytes_of(const std::string& s) { return {s.begin(), s.end()}; }

// SZDD with an all-literal stream: a 0xFF flag byte before every 8 bytes.
Bytes szdd_literal(const Bytes& data, char last = 0) {
    Bytes b = {'S', 'Z', 'D', 'D', 0x88, 0xF0, 0x27, 0x33, 'A', static_cast<std::uint8_t>(last)};
    app32(b, static_cast<std::uint32_t>(data.size()));
    for (std::size_t i = 0; i < data.size(); i += 8) {
        b.push_back(0xFF);
        b.insert(b.end(), data.begin() + static_cast<std::ptrdiff_t>(i),
                 data.begin() + static_cast<std::ptrdiff_t>(std::min(i + 8, data.size())));
    }
    return b;
}

// A 4 bpp DIB, w x h, with or without the BITMAPFILEHEADER.
Bytes make_dib(int w, int h, bool file_header) {
    const std::uint32_t stride = static_cast<std::uint32_t>(((w * 4 + 31) / 32) * 4);
    const std::uint32_t bits = stride * static_cast<std::uint32_t>(h);
    const std::uint32_t info = 40 + 16 * 4;
    Bytes b;
    if (file_header) {
        apps(b, "BM", false);
        app32(b, 14 + info + bits);
        app32(b, 0);
        app32(b, 14 + info);
    }
    app32(b, 40);
    app32(b, static_cast<std::uint32_t>(w));
    app32(b, static_cast<std::uint32_t>(h));
    app16(b, 1);
    app16(b, 4);
    app32(b, 0);
    app32(b, bits);
    app32(b, 0);
    app32(b, 0);
    app32(b, 16);
    app32(b, 0);
    for (unsigned i = 0; i < 16; ++i) app32(b, i * 0x00111111u);
    for (std::uint32_t i = 0; i < bits; ++i) b.push_back(static_cast<std::uint8_t>(i * 0x31));
    return b;
}

// PCM 11,025 Hz, 8-bit, mono, four samples.
Bytes make_wav() {
    Bytes b;
    apps(b, "RIFF", false);
    app32(b, 4 + 8 + 16 + 8 + 4);
    apps(b, "WAVEfmt ", false);
    app32(b, 16);
    app16(b, 1);
    app16(b, 1);
    app32(b, 11025);
    app32(b, 11025);
    app16(b, 1);
    app16(b, 8);
    apps(b, "data", false);
    app32(b, 4);
    for (std::uint8_t s : {0x80, 0x90, 0x70, 0x80}) b.push_back(s);
    return b;
}

// A 1 bpp 32 x 1 ICON (40 + 8 + 4 + 4 bytes), and a GROUP_ICON whose one entry it is.
Bytes make_icon() {
    Bytes b;
    app32(b, 40);
    app32(b, 32);
    app32(b, 2);  // XOR and AND rows
    app16(b, 1);
    app16(b, 1);
    for (int i = 0; i < 4; ++i) app32(b, 0);
    app32(b, 2);
    app32(b, 0);
    app32(b, 0x000000);
    app32(b, 0xFFFFFF);
    app32(b, 0xFFFF0000);
    app32(b, 0x0000FFFF);
    return b;
}
Bytes make_group_icon(std::uint16_t icon_id) {
    Bytes g;
    app16(g, 0);
    app16(g, 1);
    app16(g, 1);
    for (std::uint8_t x : {32, 1, 2, 0}) g.push_back(x);
    app16(g, 1);
    app16(g, 1);
    app32(g, static_cast<std::uint32_t>(make_icon().size()));
    app16(g, icon_id);
    return g;
}

// The New Game dialog's head with one button (tests/test_res16.cpp has the same layout).
Bytes make_dialog() {
    Bytes d = {0xC0, 0x00, 0xC0, 0x90, 1, 62, 0, 18, 0, 198, 0, 117, 0, 0, 0};
    apps(d, "New Game");
    app16(d, 8);
    apps(d, "Helv");
    for (int x : {146, 0, 7, 0, 47, 0, 14, 0, 1, 0, 0x01, 0x00, 0x01, 0x50, 0x80})
        d.push_back(static_cast<std::uint8_t>(x));
    apps(d, "&OK");
    d.push_back(0);
    return d;
}

Bytes make_menu(const std::string& text, std::uint16_t id) {
    Bytes m;
    app16(m, 0);
    app16(m, 0);
    app16(m, kMfEnd);
    app16(m, id);
    apps(m, text);
    return m;
}

Bytes lang_dll(const std::string& title) {
    std::vector<std::string> slots(16);
    slots[2] = title;  // 2338 = block 147, slot 2
    const Bytes accel = {0x81, 0x70, 0x00, 0x09, 0x00};  // last | FVIRTKEY, VK_F1 -> 9
    return build_ne("IN_DLOG", {{RT16_STRING, 147, "", string_block(slots)},
                                {RT16_DIALOG, 321, "", make_dialog()},
                                {RT16_MENU, 101, "", make_menu("&Go", 7)},
                                {RT16_MENU, 0, "POPUPMENU", make_menu("&Pop", 8)},
                                {RT16_ACCELERATOR, 0, "MAINACCEL", accel}});
}

Bytes graphic_dll() {
    return build_ne("INGRAPHIC", {{kSpriteType, 60, "", make_dib(12, 3, true)},
                                  {RT16_BITMAP, 0, "ALPHABET", make_dib(5, 2, false)}});
}

Bytes ispace_dll() {
    Bytes pal(288, 0), table(128, 0), b5000(96, 0);
    for (std::size_t i = 0; i < 0x114; ++i) pal[i] = static_cast<std::uint8_t>(i);
    table[2] = 0xFF;  // word 1 = -1
    table[3] = 0xFF;
    for (std::size_t i = 0; i < 88; ++i) b5000[i] = static_cast<std::uint8_t>(i + 1);
    return build_ne("ISPACE", {{Palette5500::kType, Palette5500::kId, "", pal},
                               {Table5600::kType, Table5600::kId, "", table},
                               {Bytes5000::kType, Bytes5000::kId, "", b5000},
                               {RT16_ICON, 7, "", make_icon()},
                               {RT16_GROUP_ICON, 105, "", make_group_icon(7)}});
}

Bytes master_bod(const std::string& body) {
    BodyRecord b;
    b.name = body;
    b.bitmap_id = 60;
    MasterBod f;
    f.records = {BodyRecord{}, b};
    return write_master_bod(f);
}

Bytes master_shp(const std::string& ship) {
    ShipRecord s;
    s.name = ship;
    s.body = 1;
    MasterShp f;
    f.records = {s};
    f.tail.assign(3, 0);
    return write_master_shp(f);
}

class TempDir {
public:
    TempDir() {
        static unsigned counter = 0;
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = fs::temp_directory_path() /
                ("is_assets_" + std::to_string(stamp) + "_" + std::to_string(++counter));
        fs::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    fs::path sub(const std::string& name) const {
        const fs::path p = path_ / name;
        fs::create_directories(p);
        return p;
    }

private:
    fs::path path_;
};

void write(const fs::path& p, const Bytes& b) {
    std::ofstream f(p, std::ios::binary);
    f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
    REQUIRE(f);
}

// An installed copy in mixed case, with an unknown file and a second language.
fs::path make_installed(const TempDir& t) {
    const fs::path d = t.sub("installed");
    write(d / "eng_lang.dll", lang_dll("Limited Inner Space"));
    write(d / "FRA_LANG.DLL", lang_dll("Inner Space limit\xE9"));
    write(d / "IGRAPHIC.DLL", graphic_dll());
    write(d / "Ispace.Dll", ispace_dll());
    write(d / "INSPACE.EXE", build_ne("INSPACE", {}));
    write(d / "SND_FX.DLL", build_ne("ISND_FX", {{kWavResourceType, 3, "", make_wav()}}));
    write(d / "MASTER.BOD", master_bod("TESTBODY"));
    write(d / "master.shp", master_shp("TESTSHIP"));
    write(d / "FOO.BAR", bytes_of("not a game file"));
    return d;
}

// The same files as the distribution ships them: SZDD under changed names, INSPACE.ex_
// stored plain, and two names ending in '_'.
fs::path make_distribution(const TempDir& t) {
    const fs::path d = t.sub("distribution");
    write(d / "ENG_LANG.dl1", szdd_literal(lang_dll("Limited Inner Space")));
    write(d / "FRA_LANG.DL1", szdd_literal(lang_dll("Inner Space limit\xE9")));
    write(d / "IGRAPHIC.DL1", szdd_literal(graphic_dll()));
    write(d / "ISPACE.dll", ispace_dll());
    write(d / "INSPACE.ex_", build_ne("INSPACE", {}));
    write(d / "SND_FX.DLL", build_ne("ISND_FX", {{kWavResourceType, 3, "", make_wav()}}));
    write(d / "MASTER.BO1", szdd_literal(master_bod("TESTBODY")));
    write(d / "MASTER.SH1", szdd_literal(master_shp("TESTSHIP")));
    write(d / "README.TX_", szdd_literal(bytes_of("hello"), 'T'));
    write(d / "ODD.TX_", bytes_of("plain"));
    return d;
}

std::vector<std::string> names_of(const GameFiles& g) {
    std::vector<std::string> v;
    for (const auto& e : g.entries()) v.push_back(e.name.u8string());
    return v;
}

fs::path env_dir(const char* name) {
    const char* v = std::getenv(name);
    if (!v || !*v || !fs::is_directory(v)) return {};
    fs::path p = fs::path(v).lexically_normal();
    if (p.filename().empty()) p = p.parent_path();
    return p;
}

// The repository that holds original.sha256 and original/shipdisk: the grandparent of
// IS_DIST_DIR when that is set, else the nearest of the three directories above IS_ASSETS_DIR
// or IS_ORIGINAL_DIR (work/assets, work/extracted/shipdisk) that holds both. Empty when none.
fs::path original_root() {
    const fs::path dist = env_dir("IS_DIST_DIR");
    if (!dist.empty()) return dist.parent_path().parent_path();
    for (const char* var : {"IS_ASSETS_DIR", "IS_ORIGINAL_DIR"}) {
        fs::path p = env_dir(var);
        for (int up = 0; up < 3 && !p.empty() && p.has_relative_path(); ++up) {
            p = p.parent_path();
            if (fs::is_regular_file(p / "original.sha256") &&
                fs::is_directory(p / "original" / "shipdisk"))
                return p;
        }
    }
    return {};
}

// The distribution as shipped: IS_DIST_DIR, else original/shipdisk under original_root().
fs::path dist_dir() {
    if (fs::path d = env_dir("IS_DIST_DIR"); !d.empty()) return d;
    const fs::path root = original_root();
    return root.empty() ? fs::path() : root / "original" / "shipdisk";
}

// THEWORKS next to a shipdisk tree (work/extracted/theworks, original/theworks).
fs::path theworks_beside(const fs::path& shipdisk) {
    if (shipdisk.empty()) return {};
    const fs::path p = shipdisk.parent_path() / "theworks";
    return fs::is_directory(p) ? p : fs::path();
}

void check_shipdisk_store(const AssetStore& a) {
    CHECK(a.string(2338) == "Limited Inner Space");
    CHECK_FALSE(a.find_string(2336));  // an empty slot of block 147
    CHECK_THROWS_AS(a.string(2336), NotFound);
    const Dialog16 d = a.dialog(321);
    CHECK(d.caption == "New Game");
    CHECK(d.cx == 198);
    CHECK(d.cy == 117);
    CHECK(a.dialog_template(321) == dialog_to_win32(d));
    CHECK(a.menu(101).size() == 5);
    CHECK_FALSE(a.menu("PopupMenu").empty());
    CHECK(a.menu_template(101) == menu_to_win32(a.menu(101)));
    CHECK(a.accelerators("MAINACCEL").size() == 26);
    CHECK(a.accelerators("factaccel").size() == 29);

    const Dib s = a.sprite(60);
    CHECK(s.width == 132);
    CHECK(s.height == 33);
    CHECK_NOTHROW(a.bitmap("ALPHABET"));
    CHECK_NOTHROW(a.bitmap("SD_LOGO", kGameDll));
    // ISPACE.DLL's three sprites (work/assets/ispace.dll/4000)
    struct SpriteSize {
        std::uint16_t id;
        int width, height;
    };
    for (const SpriteSize& z : {SpriteSize{220, 40, 40}, {221, 360, 40}, {300, 138, 138}}) {
        CAPTURE(z.id);
        const Dib d = a.sprite(z.id, kGameDll);
        CHECK(d.width == z.width);
        CHECK(d.height == z.height);
        CHECK(d.bpp == 4);
    }
    CHECK_THROWS_AS(a.sprite(60, kGameDll), NotFound);
    // the language DLL's integer-id menus do not answer an empty name
    CHECK_THROWS_AS(a.menu(""), NotFound);
    CHECK_THROWS_AS(a.group_icon(kGameDll, ""), NotFound);

    const Ships sh = a.ships();
    CHECK(sh.bodies.records.size() == 52);
    CHECK(sh.ships.records.size() == 118);
    const ShipRecord* t = sh.find("TRANSFORMER");
    REQUIRE(t);
    CHECK(t == &sh.ships.records[1]);
    CHECK(t->body == 33);
    const BodyRecord* b = sh.body(*t);
    REQUIRE(b);
    CHECK(b->name == "TRANSFORMER");
    CHECK(b->bitmap_id == 60);
    CHECK(a.sprite(b->bitmap_id).width == 132);

    const VersionInfo v = a.version(kMainProgram);
    REQUIRE(v.find_string("FileVersion"));
    CHECK(*v.find_string("FileVersion") == "1.29.08");
    const IconGroup g = a.group_icon(kMainProgram, "game");
    REQUIRE(g.entries.size() == 2);
    REQUIRE(g.images.size() == 2);
    CHECK(std::set<int>{g.images[0].image.width, g.images[1].image.width} == std::set<int>{16, 32});
    CHECK(g.images[0].image.bpp == 4);
    REQUIRE(g.ico.size() > 6);
    CHECK(g.ico[4] == 2);
    // ISPACE.DLL's icon groups have integer ids (105, 106, 107), one 32 x 32 entry each
    for (std::uint16_t id : {105, 106, 107}) {
        CAPTURE(id);
        const IconGroup ig = a.group_icon(kGameDll, id);
        REQUIRE(ig.images.size() == 1);
        CHECK(ig.images[0].image.width == 32);
        CHECK(ig.images[0].image.height == 32);
        const fs::path assets = env_dir("IS_ASSETS_DIR");
        const fs::path ref = assets / "ispace.dll" / "GROUP_ICON" / (std::to_string(id) + ".ico");
        if (!assets.empty() && fs::is_regular_file(ref)) CHECK(ig.ico == read_file(ref));
    }
    CHECK_THROWS_AS(a.group_icon(kGameDll, 108), NotFound);

    // the shipdisk keeps its sound modules in the game directory, the default sound directory
    CHECK(&a.sound_files() == &a.files());
    const auto waves = a.sound_module(kSoundDll).all(kWavResourceType);
    CHECK(waves.size() == 46);
    REQUIRE_FALSE(waves.empty());
    const Sound snd = a.wav("snd_fx.dll", waves.front()->id);
    CHECK(snd.wave.rate == 11025);
    CHECK(snd.wave.bits == 8);
    CHECK(snd.wave.channels == 1);
    CHECK(snd.file.size() == snd.wave.file_size);
    CHECK(a.fx_module() == "ENG_FX.DLL");
    const auto voices = a.sound_module(a.fx_module()).all(kWavResourceType);
    CHECK(voices.size() == 32);  // work/assets/eng_fx.dll/100
    REQUIRE_FALSE(voices.empty());
    CHECK(a.wav(a.fx_module(), voices.back()->id).file.size() > 44);

    CHECK_NOTHROW(a.palette5500());
    CHECK_NOTHROW(a.table5600());
    CHECK(a.bytes5000().length == 88);
    for (std::uint16_t id : Scene6600::kIds) CHECK_NOTHROW(a.scene6600(id));

    CHECK_THROWS_AS(a.dialog(9999), NotFound);
    CHECK_THROWS_AS(a.version("ENG_LANG.DLL"), NotFound);
}

void check_shipdisk_identified(const GameFiles& g, bool distribution) {
    const Identification id = identify(g);
    CHECK(id.form == (distribution ? DirForm::kDistribution : DirForm::kInstalled));
    CHECK(id.files.size() == 25);
    CHECK(id.all_known());
    CHECK(id.packages == std::vector<Package>{Package::kShipdisk});
    for (const FileReport& f : id.files) {
        REQUIRE_MESSAGE(f.known, f.path.u8string() << ": " << f.error);
        CHECK(f.known->package == Package::kShipdisk);
        CHECK(f.name_known);
        CHECK(f.error.empty());
        // the file on disk carries the table's distribution or installed name exactly
        const char* stored = distribution ? f.known->dist_name : f.known->name;
        CHECK_MESSAGE(f.path.filename() == path_from_cp1252(stored), f.path.u8string());
    }
}

}  // namespace

TEST_CASE("assets: restored_name follows re/isre/szdd.py restored_name") {
    CHECK(restored_name("MASTER.BO1") == "MASTER.BOD");
    CHECK(restored_name("ENG_LANG.dl1") == "ENG_LANG.DLL");
    CHECK(restored_name("Eng_help.hl1") == "Eng_help.HLP");
    CHECK(restored_name("INRUN.A1") == "INRUN.AD");
    CHECK(restored_name("INSPACE.sc1") == "INSPACE.SCR");
    CHECK(restored_name("WAVEMIX.IN1") == "WAVEMIX.INI");
    CHECK(restored_name("INSPACE.ex_", 'X') == "INSPACE.EXE");  // the table comes first
    CHECK(restored_name("a.b.Mi1") == "a.b.MID");
    CHECK(restored_name(".sh1") == ".SHP");
    CHECK(restored_name("README.TX_", 'T') == "README.TXT");
    CHECK(restored_name("NOEXT_", '1') == "NOEXT1");
    CHECK(restored_name("README.TX_") == "README.TX_");
    CHECK(restored_name("README.TX_", ' ') == "README.TX_");
    CHECK(restored_name("README.TX_", '\x7F') == "README.TX_");
    CHECK(restored_name("README.TXT", 'T') == "README.TXT");
    CHECK(restored_name("ISPACE.dll") == "ISPACE.dll");
    CHECK(restored_name("FILE.dl") == "FILE.dl");
    CHECK(restored_name("FILE.dl12") == "FILE.dl12");
}

TEST_CASE("assets: GameFiles over an installed copy in mixed case") {
    TempDir t;
    const fs::path dir = make_installed(t);
    GameFiles g(dir);
    CHECK(g.form() == DirForm::kInstalled);
    CHECK(g.shadowed().empty());
    CHECK(names_of(g) == std::vector<std::string>{"eng_lang.dll", "FOO.BAR", "FRA_LANG.DLL",
                                                  "IGRAPHIC.DLL", "INSPACE.EXE", "Ispace.Dll",
                                                  "MASTER.BOD", "master.shp", "SND_FX.DLL"});
    const GameFiles::Entry* e = g.find("ENG_LANG.DLL");
    REQUIRE(e);
    CHECK(e->path == dir / "eng_lang.dll");
    CHECK(e->name == "eng_lang.dll");
    CHECK_FALSE(e->distribution_name);
    CHECK(g.find("Eng_Lang.Dll") == e);
    CHECK(g.has("ispace.dll"));
    CHECK(g.has("MASTER.SHP"));
    CHECK_FALSE(g.has("SND_FX"));
    CHECK_FALSE(g.has("ENG_LANG.dl1"));

    const Bytes& b = g.bytes("eng_lang.DLL");
    CHECK(b == lang_dll("Limited Inner Space"));
    CHECK(&g.bytes("ENG_LANG.DLL") == &b);  // cached
    CHECK(g.read("ENG_LANG.DLL") == b);
    // the file changes on disk: bytes() keeps the cached copy until forget(), read() never caches
    write(dir / "eng_lang.dll", bytes_of("rewritten"));
    CHECK(g.bytes("ENG_LANG.DLL") == lang_dll("Limited Inner Space"));
    CHECK(g.read("ENG_LANG.DLL") == bytes_of("rewritten"));
    g.forget("Eng_Lang.Dll");
    CHECK(g.bytes("ENG_LANG.DLL") == bytes_of("rewritten"));
    g.forget("NOT_CACHED.DLL");  // no effect
    CHECK_THROWS_AS(g.bytes("ENG_FX.DLL"), NotFound);
    CHECK_THROWS_AS(g.read("ENG_FX.DLL"), NotFound);
    CHECK_THROWS_AS(GameFiles(dir / "missing"), NotFound);
    CHECK_THROWS_AS(GameFiles(dir / "FOO.BAR"), NotFound);
    CHECK(holds_game(dir));
}

TEST_CASE("assets: names fold the case of cp1252's letters and keep their other characters") {
    TempDir t;
    const fs::path dir = t.sub("names");
    const fs::path cafe = dir / path_from_cp1252("CAF\xC9.DIB");  // cp1252 E acute
    write(cafe, bytes_of("cafe"));
    // s caron, oe, z caron, y diaeresis: their upper-case letters are 0x8A 0x8C 0x8E 0x9F
    const fs::path lower = dir / path_from_cp1252("\x9A\x9C\x9E\xFF.wav");
    write(lower, bytes_of("lower"));
    // sharp s and micro have no upper-case letter in cp1252; division is no letter
    write(dir / path_from_cp1252("\xDF\xB5\xF7.txt"), bytes_of("sharp"));
    GameFiles g(dir);
    REQUIRE(g.find("caf\xE9.dib"));  // e acute finds E acute
    CHECK(g.find("caf\xE9.dib")->path == cafe);
    CHECK(g.bytes("Caf\xC9.Dib") == bytes_of("cafe"));
    CHECK_FALSE(g.has("CAFE.DIB"));
    REQUIRE(g.find("\x8A\x8C\x8E\x9F.WAV"));
    CHECK(g.find("\x8A\x8C\x8E\x9F.WAV")->path == lower);
    CHECK(g.find("\x9A\x8C\x9E\x9F.Wav")->path == lower);
    CHECK(g.has("\xDF\xB5\xF7.TXT"));
    CHECK_FALSE(g.has("SS\xB5\xF7.TXT"));
    CHECK_FALSE(g.has("\xDF\xB5\xD7.TXT"));  // division sign is no times sign
    CHECK(names_of(g).size() == 3);

    // every lower-case letter of Latin-1 and its upper-case partner; the rest stay
    for (int c = 0xE0; c <= 0xFE; ++c) {
        CAPTURE(c);
        const std::string lo(1, static_cast<char>(c)), up(1, static_cast<char>(c - 0x20));
        CHECK((name_key(path_from_cp1252(lo)) == name_key(path_from_cp1252(up))) == (c != 0xF7));
    }
    CHECK(name_key(path_from_cp1252("\xFF")) == name_key(path_from_cp1252("\x9F")));
    CHECK(name_key(fs::path(u"\u00b5\u00df\u0142\u0101")) ==
          fs::path(u"\u00b5\u00df\u0142\u0101").native());  // no partner in cp1252: kept
    CHECK(name_key(fs::path(u"a\u00e9z")) == fs::path(u"A\u00c9Z").native());
#ifndef _WIN32
    // native UTF-8: bytes that are no complete two-byte letter stay as they are
    CHECK(name_key(fs::path(std::string("a\xC3"))) == std::string("A\xC3"));
    CHECK(name_key(fs::path(std::string("\xC3(\xC5"))) == std::string("\xC3(\xC5"));
    CHECK(name_key(fs::path(std::string("\xA9\xC3\xA9"))) == std::string("\xA9\xC3\x89"));
#endif
}

TEST_CASE("assets: a file that cannot be opened or read throws NotFound") {
    TempDir t;
    const fs::path dir = t.sub("unreadable");
    write(dir / "GONE.DLL", bytes_of("gone"));
    GameFiles g(dir);
    fs::remove(dir / "GONE.DLL");  // removed after the listing
    CHECK_THROWS_AS(g.read("GONE.DLL"), NotFound);
    CHECK_THROWS_AS(g.bytes("GONE.DLL"), NotFound);
    const Identification id = identify(g);
    REQUIRE(id.files.size() == 1);
    CHECK_FALSE(id.files[0].error.empty());
    // a directory opens on POSIX and then fails to read; libstdc++'s filebuf throws
    // std::ios_base::failure there, which read() turns into NotFound
    const fs::path sub = t.sub("unreadable/ADIR.DLL");
    CHECK_THROWS_AS(g.read(GameFiles::Entry{sub, "ADIR.DLL", false}), NotFound);
}

TEST_CASE("assets: GameFiles over a distribution expands SZDD and restores the names") {
    TempDir t;
    const fs::path dir = make_distribution(t);
    GameFiles g(dir);
    CHECK(g.form() == DirForm::kDistribution);
    CHECK(names_of(g) == std::vector<std::string>{"ENG_LANG.DLL", "FRA_LANG.DLL", "IGRAPHIC.DLL",
                                                  "INSPACE.EXE", "ISPACE.dll", "MASTER.BOD",
                                                  "MASTER.SHP", "ODD.TX_", "README.TXT",
                                                  "SND_FX.DLL"});
    const GameFiles::Entry* e = g.find("eng_lang.dll");
    REQUIRE(e);
    CHECK(e->path == dir / "ENG_LANG.dl1");
    CHECK(e->distribution_name);
    CHECK(g.bytes("ENG_LANG.DLL") == lang_dll("Limited Inner Space"));
    CHECK(g.bytes("INSPACE.EXE") == build_ne("INSPACE", {}));  // INSPACE.ex_ is stored plain
    CHECK(g.find("INSPACE.EXE")->distribution_name);
    CHECK_FALSE(g.find("ISPACE.DLL")->distribution_name);
    CHECK(g.bytes("readme.txt") == bytes_of("hello"));  // '_' became the stored 'T'
    CHECK(g.bytes("ODD.TX_") == bytes_of("plain"));     // not SZDD: the name stays
    CHECK_FALSE(g.has("ENG_LANG.dl1"));
    CHECK(holds_game(dir));
}

TEST_CASE("assets: a file under its installed name shadows its distribution twin") {
    TempDir t;
    const fs::path dir = t.sub("both");
    write(dir / "ENG_LANG.dl1", szdd_literal(bytes_of("compressed")));
    write(dir / "ENG_LANG.DLL", bytes_of("installed"));
    write(dir / "B.DL1", bytes_of("first"));
    write(dir / "b.dll", bytes_of("second"));
    GameFiles g(dir);
    CHECK(g.bytes("ENG_LANG.DLL") == bytes_of("installed"));
    CHECK(g.bytes("B.DLL") == bytes_of("second"));
    CHECK(std::set<fs::path>(g.shadowed().begin(), g.shadowed().end()) ==
          std::set<fs::path>{dir / "ENG_LANG.dl1", dir / "B.DL1"});
    CHECK(g.form() == DirForm::kInstalled);  // only the files in use count
    const Identification id = identify(g);
    CHECK(id.files.size() == 2);
    CHECK(id.shadowed == g.shadowed());  // passed on for the report
}

TEST_CASE("assets: a bad SZDD stream throws FormatError and identify() reports it") {
    TempDir t;
    const fs::path dir = t.sub("bad");
    Bytes cut = szdd_literal(bytes_of("0123456789"));
    cut.resize(cut.size() - 3);
    write(dir / "BROKEN.DL1", cut);
    write(dir / "FINE.TXT", bytes_of("fine"));
    GameFiles g(dir);
    CHECK_THROWS_AS(g.bytes("BROKEN.DLL"), FormatError);
    const Identification id = identify(g);
    REQUIRE(id.files.size() == 2);
    CHECK(id.files[0].name == "BROKEN.DLL");
    CHECK(id.files[0].known == nullptr);
    CHECK_FALSE(id.files[0].error.empty());
    CHECK(id.files[0].sha256.empty());
    CHECK(id.files[1].error.empty());
    CHECK(id.files[1].sha256 == sha256_hex(bytes_of("fine").data(), 4));
    CHECK(id.unknown_count() == 2);
    CHECK(id.packages.empty());
}

TEST_CASE("assets: identify() reports unknown files and still serves them") {
    TempDir t;
    const fs::path dir = make_installed(t);
    GameFiles g(dir);
    const Identification id = identify(g);
    CHECK(id.files.size() == g.entries().size());
    CHECK_FALSE(id.all_known());
    CHECK(id.unknown_count() == id.files.size());
    for (const FileReport& f : id.files) {
        CHECK(f.known == nullptr);
        CHECK(f.error.empty());
        CHECK(f.sha256.size() == 64);
        const std::string n = f.name.u8string();
        // the shipdisk has these names, with other content; FOO.BAR and FRA_LANG.DLL it has not
        const bool shipped = n != "FOO.BAR" && n != "FRA_LANG.DLL";
        CHECK_MESSAGE(f.name_known == shipped, n);
    }
    CHECK(AssetStore(std::move(g)).string(2338) == "Limited Inner Space");
}

TEST_CASE("assets: discovery takes the first candidate that holds the game") {
    TempDir t;
    const fs::path installed = make_installed(t);
    const fs::path distribution = make_distribution(t);
    const fs::path empty = t.sub("empty");
    const fs::path addon = t.sub("addon");  // THEWORKS-like: no game modules
    write(addon / "FACTORY.EXE", build_ne("IFACTORY", {}));
    write(addon / "XFORM.DIB", make_dib(4, 4, true));
    const fs::path partial = t.sub("partial");  // no IGRAPHIC.DLL
    write(partial / "INSPACE.EXE", build_ne("INSPACE", {}));
    write(partial / "ISPACE.DLL", ispace_dll());

    CHECK_FALSE(holds_game(empty));
    CHECK_FALSE(holds_game(addon));
    CHECK_FALSE(holds_game(partial));
    CHECK_FALSE(holds_game(empty / "nowhere"));
    CHECK(discover({fs::path(), empty / "nowhere", empty, addon, partial, distribution, installed}) ==
          distribution);
    CHECK(discover({installed, distribution}) == installed);
    CHECK(discover({empty, addon}) == std::nullopt);
    CHECK(discover({}) == std::nullopt);
}

TEST_CASE("assets: AssetStore reads the same assets from both forms") {
    TempDir t;
    const fs::path ship_dir = t.sub("ships");
    write(ship_dir / "master.bod", master_bod("OTHERBODY"));
    write(ship_dir / "Master.Shp", master_shp("OTHERSHIP"));
    for (const fs::path& dir : {make_installed(t), make_distribution(t)}) {
        CAPTURE(dir.u8string());
        AssetStore a(dir);
        CHECK(a.language() == "ENG");
        CHECK(a.language_module() == "ENG_LANG.DLL");
        CHECK(a.language_dll().module_name() == "IN_DLOG");
        CHECK(&a.module("eng_lang.dll") == &a.language_dll());  // loaded once

        CHECK(a.string(2338) == "Limited Inner Space");
        CHECK_FALSE(a.find_string(2339));
        CHECK_THROWS_AS(a.string(2339), NotFound);
        CHECK_THROWS_AS(a.string(5), NotFound);

        const Dialog16 d = a.dialog(321);
        CHECK(d.caption == "New Game");
        CHECK(d.face == "Helv");
        CHECK(a.dialog_template(321) == dialog_to_win32(d));
        Dialog16 mapped = d;
        mapped.face = "MS Sans Serif";
        CHECK(a.dialog_template(321, {"MS Sans Serif", 0}) == dialog_to_win32(mapped));
        mapped.point_size = 9;
        CHECK(a.dialog_template(321, {"MS Sans Serif", 9}, 0x10) == dialog_to_win32(mapped, 0x10));
        CHECK_THROWS_AS(a.dialog(322), NotFound);

        REQUIRE(a.menu(101).size() == 1);
        CHECK(a.menu(101)[0].text == "&Go");
        CHECK(a.menu("popupmenu")[0].text == "&Pop");
        CHECK(a.menu_template("POPUPMENU") == menu_to_win32(a.menu("POPUPMENU")));
        CHECK_THROWS_AS(a.menu(102), NotFound);
        const auto acc = a.accelerators("MainAccel");
        REQUIRE(acc.size() == 1);
        CHECK(acc[0].key == 0x70);
        CHECK(acc[0].cmd == 9);
        CHECK_THROWS_AS(a.accelerators("FACTACCEL"), NotFound);
        // an empty name names nothing; MENU 101, with an integer id, must not answer it
        CHECK_THROWS_AS(a.menu(""), NotFound);
        CHECK_THROWS_AS(a.menu_template(""), NotFound);
        CHECK_THROWS_AS(a.accelerators(""), NotFound);

        CHECK(a.sprite(60).width == 12);
        CHECK(a.sprite(60).height == 3);
        CHECK(a.bitmap("alphabet").width == 5);
        CHECK_THROWS_AS(a.sprite(61), NotFound);
        CHECK_THROWS_AS(a.sprite(60, "ISPACE.DLL"), NotFound);
        CHECK_THROWS_AS(a.bitmap(""), NotFound);
        CHECK_THROWS_AS(a.group_icon(kMainProgram, "GAME"), NotFound);
        CHECK_THROWS_AS(a.version(kMainProgram), NotFound);
        const IconGroup ig = a.group_icon(kGameDll, 105);
        REQUIRE(ig.entries.size() == 1);
        CHECK(ig.entries[0].id == 7);
        REQUIRE(ig.images.size() == 1);
        CHECK(ig.images[0].image.width == 32);
        CHECK(ig.images[0].image.bpp == 1);
        CHECK(ig.ico.size() == 6 + 16 + make_icon().size());
        CHECK_THROWS_AS(a.group_icon(kGameDll, 106), NotFound);
        CHECK_THROWS_AS(a.group_icon(kGameDll, ""), NotFound);  // 105 has an integer id

        const Sound s = a.wav("SND_FX.DLL", 3);
        CHECK(s.file == make_wav());
        CHECK(s.wave.rate == 11025);
        CHECK(s.wave.frames() == 4);
        CHECK_THROWS_AS(a.wav("SND_FX.DLL", 4), NotFound);
        CHECK_THROWS_AS(a.wav("ENG_FX.DLL", 3), NotFound);

        CHECK(a.palette5500().colours[1].r == 3);
        CHECK(a.table5600().rows[0][1] == -1);
        CHECK(a.bytes5000().length == 88);
        CHECK_THROWS_AS(a.scene6600(100), NotFound);

        const Ships sh = a.ships();
        REQUIRE(sh.find("TESTSHIP"));
        CHECK(sh.find("TESTSHIP")->body == 1);
        REQUIRE(sh.body(*sh.find("TESTSHIP")));
        CHECK(sh.body(*sh.find("TESTSHIP"))->name == "TESTBODY");
        CHECK(sh.ships.tail.size() == 3);
        CHECK(sh.find("TRANSFORMER") == nullptr);
        CHECK(a.ships(ship_dir).find("OTHERSHIP"));
        CHECK_THROWS_AS(a.ships(ship_dir / "nowhere"), NotFound);
        ShipRecord stray = *sh.find("TESTSHIP");
        stray.body = static_cast<std::uint16_t>(sh.bodies.records.size());  // past the last body
        CHECK(sh.body(stray) == nullptr);

        a.set_language("FRA");
        CHECK(a.language_module() == "FRA_LANG.DLL");
        CHECK(a.string(2338) == "Inner Space limit\xE9");  // cp1252, unconverted
        a.set_language("DEU");
        CHECK_THROWS_AS(a.string(2338), NotFound);
        a.set_language(kDefaultLanguage);
        CHECK(a.string(2338) == "Limited Inner Space");
        CHECK_THROWS_AS(a.module("NOPE.DLL"), NotFound);

        // The Ship Factory (M8) writes MASTER.BOD and MASTER.SHP under their installed names:
        // over the installed files, or beside the distribution's MASTER.BO1 and MASTER.SH1.
        // The next ships() reads what it wrote.
        const bool shipped = a.files().form() == DirForm::kDistribution;
        write(shipped ? dir / kBodyFile : a.files().find(kBodyFile)->path, master_bod("NEWBODY"));
        write(shipped ? dir / kShipFile : a.files().find(kShipFile)->path, master_shp("NEWSHIP"));
        const Ships fresh = a.ships();
        CHECK(fresh.find("TESTSHIP") == nullptr);
        REQUIRE(fresh.find("NEWSHIP"));
        REQUIRE(fresh.body(*fresh.find("NEWSHIP")));
        CHECK(fresh.body(*fresh.find("NEWSHIP"))->name == "NEWBODY");
        CHECK(a.ships(dir).find("NEWSHIP"));
    }
}

TEST_CASE("assets: sound modules come from the sound directory (SndPath)") {
    TempDir t;
    const fs::path game = make_installed(t);  // SND_FX.DLL with WAV 3
    // the layout of INI regeneration and INSTALL.EXE: <game>\SOUNDS\ holds the sound modules
    const fs::path sounds = t.sub("installed/SOUNDS");
    write(sounds / "snd_fx.dll", build_ne("ISND_FX", {{kWavResourceType, 4, "", make_wav()}}));
    write(sounds / "ENG_FX.DL1",
          szdd_literal(build_ne("IENG_FX", {{kWavResourceType, 9, "", make_wav()}})));
    write(sounds / "FRA_FX.DLL", build_ne("IFRA_FX", {{kWavResourceType, 10, "", make_wav()}}));

    AssetStore a(game);
    CHECK(a.sound_dir().empty());
    CHECK(&a.sound_files() == &a.files());  // the default: the game directory
    CHECK(a.fx_module() == "ENG_FX.DLL");
    CHECK(a.wav(kSoundDll, 3).file == make_wav());
    CHECK_THROWS_AS(a.wav(kSoundDll, 4), NotFound);
    CHECK_THROWS_AS(a.wav(a.fx_module(), 9), NotFound);
    CHECK(a.files().find("SOUNDS") == nullptr);  // a subdirectory is not listed

    a.set_sound_dir(sounds);
    CHECK(a.sound_dir() == sounds);
    CHECK(a.sound_files().dir() == sounds);
    CHECK(a.sound_files().form() == DirForm::kDistribution);
    CHECK(a.wav(kSoundDll, 4).file == make_wav());
    CHECK_THROWS_AS(a.wav(kSoundDll, 3), NotFound);  // the game directory's SND_FX.DLL is not used
    CHECK(&a.sound_module(kSoundDll) != &a.module(kSoundDll));  // two files, two modules
    CHECK(a.module(kSoundDll).find(kWavResourceType, 3));
    CHECK(a.sound_module("eng_fx.dll").module_name() == "IENG_FX");
    CHECK(a.wav(a.fx_module(), 9).wave.rate == 11025);
    CHECK_THROWS_AS(a.wav(a.fx_module(), 10), NotFound);
    CHECK_THROWS_AS(a.module(a.fx_module()), NotFound);  // module() is the game directory

    a.set_language("FRA");
    CHECK(a.fx_module() == "FRA_FX.DLL");
    CHECK(a.wav(a.fx_module(), 10).file == make_wav());
    CHECK(a.string(2338) == "Inner Space limit\xE9");  // the language DLL stays in the game dir

    a.set_sound_dir(sounds / "nowhere");
    CHECK_THROWS_AS(a.sound_files(), NotFound);
    CHECK_THROWS_AS(a.wav(kSoundDll, 4), NotFound);
    CHECK_THROWS_AS(a.sound_module(kSoundDll), NotFound);

    a.set_sound_dir({});
    CHECK(&a.sound_files() == &a.files());
    CHECK(a.wav(kSoundDll, 3).file == make_wav());
}

TEST_CASE("assets: malformed modules and records throw FormatError") {
    TempDir t;
    const fs::path dir = t.sub("malformed");
    write(dir / "ENG_LANG.DLL", bytes_of("MZ but nothing else"));
    write(dir / "ISPACE.DLL",
          build_ne("ISPACE", {{Palette5500::kType, Palette5500::kId, "", Bytes(10, 1)}}));
    write(dir / "MASTER.BOD", master_bod("B"));
    write(dir / "MASTER.SHP", master_shp("S"));
    write(dir / "IGRAPHIC.DLL", build_ne("INGRAPHIC", {{kSpriteType, 1, "", bytes_of("no DIB")}}));
    AssetStore a(dir);
    CHECK_THROWS_AS(a.string(1), FormatError);
    CHECK_THROWS_AS(a.palette5500(), FormatError);  // 10 bytes of 288
    CHECK_THROWS_AS(a.sprite(1), FormatError);
    CHECK_NOTHROW(a.ships());
}

TEST_CASE("assets: the known-file table is consistent") {
    const KnownFiles table = known_files();
    CHECK(table.size() == 166);
    std::size_t shipdisk = 0, theworks = 0;
    std::set<std::pair<int, fs::path::string_type>> seen;
    const KnownFile* prev = nullptr;
    for (const KnownFile& k : table) {
        CAPTURE(k.name);
        (k.package == Package::kShipdisk ? shipdisk : theworks) += 1;
        CHECK(std::string(k.sha256).size() == 64);
        CHECK(std::string(k.dist_sha256).size() == 64);
        CHECK(std::string(k.sha256).find_first_not_of("0123456789abcdef") == std::string::npos);
        // every renamed file of the table restores by its extension alone
        CHECK(restored_name(path_from_cp1252(k.dist_name)) == path_from_cp1252(k.name));
        if (std::string(k.name) == k.dist_name) {
            CHECK(k.size == k.dist_size);
            CHECK(std::string(k.sha256) == k.dist_sha256);
        }
        CHECK(seen.insert({static_cast<int>(k.package), name_key(path_from_cp1252(k.name))}).second);
        if (prev) CHECK(prev->package <= k.package);
        prev = &k;
        CHECK(find_known(path_from_cp1252(k.name), k.size, k.sha256) == &k);
    }
    CHECK(shipdisk == 25);
    CHECK(theworks == 141);
    CHECK(known_name("eng_lang.dll"));
    CHECK_FALSE(known_name("FRA_LANG.DLL"));
    const KnownFile* inspace = find_known("inspace.exe", 310112,
        "fcc51839146d312209ba4123043d8a874dd2418344401ceed2afbdc888fbb66f");
    REQUIRE(inspace);
    CHECK(inspace->package == Package::kShipdisk);
    CHECK(std::string(inspace->dist_name) == "INSPACE.ex_");
    CHECK(find_known("INSPACE.EXE", 310111,
                     "fcc51839146d312209ba4123043d8a874dd2418344401ceed2afbdc888fbb66f") == nullptr);
    CHECK(std::string(package_label(Package::kShipdisk)) == "shipdisk v1.29.08");
    CHECK(std::string(package_label(Package::kTheWorks)) == "theworks");
}

TEST_CASE("assets golden: the generated table equals original.sha256 and original/") {
    const fs::path repo = original_root();
    const fs::path manifest = repo.empty() ? fs::path() : repo / "original.sha256";
    if (manifest.empty() || !fs::is_regular_file(manifest)) {
        MESSAGE("skipped: original.sha256 and original/shipdisk not found from IS_DIST_DIR, "
                "IS_ASSETS_DIR or IS_ORIGINAL_DIR");
        return;
    }
    using Row = std::tuple<int, std::string, std::uint32_t, std::string, std::string,
                           std::uint32_t, std::string>;
    std::set<Row> want, have;
    for (const KnownFile& k : known_files())
        have.insert({static_cast<int>(k.package), k.name, k.size, k.sha256, k.dist_name,
                     k.dist_size, k.dist_sha256});

    std::ifstream in(manifest, std::ios::binary);
    std::string line;
    std::size_t archives = 0;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        REQUIRE(line.size() > 66);
        const std::string digest = line.substr(0, 64);
        const std::string rel = line.substr(66);
        const auto slash = rel.rfind('/');
        const std::string folder = rel.substr(0, slash), name = rel.substr(slash + 1);
        if (folder == "original/archives") {
            ++archives;
            continue;
        }
        REQUIRE_MESSAGE((folder == "original/shipdisk" || folder == "original/theworks"), rel);
        const Package pkg = folder == "original/shipdisk" ? Package::kShipdisk : Package::kTheWorks;
        const Bytes raw = read_file(repo / path_from_cp1252(rel));
        REQUIRE_MESSAGE(sha256_hex(raw.data(), raw.size()) == digest, rel);
        Bytes data = raw;
        char last = 0;
        if (is_szdd(raw.data(), raw.size())) {
            last = parse_szdd_header(raw.data(), raw.size()).last_char;
            data = szdd_expand(raw.data(), raw.size()).data;
        }
        want.insert({static_cast<int>(pkg), restored_name(path_from_cp1252(name), last).u8string(),
                     static_cast<std::uint32_t>(data.size()), sha256_hex(data.data(), data.size()),
                     name, static_cast<std::uint32_t>(raw.size()), digest});
    }
    CHECK(archives == 2);
    CHECK(want.size() == 166);
    for (const Row& r : want)
        CHECK_MESSAGE(have.count(r), "missing or stale row: " << std::get<1>(r)
                                     << " (re-run tools/gen_known_files.py)");
    for (const Row& r : have)
        CHECK_MESSAGE(want.count(r), "row not in the manifest: " << std::get<1>(r));
}

TEST_CASE("assets golden: the installed copy and the distribution give the same assets") {
    const fs::path installed = env_dir("IS_ORIGINAL_DIR");
    const fs::path dist = dist_dir();
    if (installed.empty() && dist.empty()) {
        MESSAGE("skipped: neither IS_ORIGINAL_DIR nor original/shipdisk available");
        return;
    }
    if (installed.empty()) MESSAGE("installed part skipped: IS_ORIGINAL_DIR unset");
    if (dist.empty()) MESSAGE("distribution part skipped: original/shipdisk not found");
    std::optional<AssetStore> si, sd;
    if (!installed.empty()) {
        si.emplace(installed);
        CHECK(si->files().form() == DirForm::kInstalled);
        check_shipdisk_store(*si);
        check_shipdisk_identified(si->files(), false);
        CHECK(holds_game(installed));
    }
    if (!dist.empty()) {
        sd.emplace(dist);
        CHECK(sd->files().form() == DirForm::kDistribution);
        CHECK(sd->files().shadowed().empty());
        check_shipdisk_store(*sd);
        check_shipdisk_identified(sd->files(), true);
        CHECK(holds_game(dist));
    }
    if (si && sd) {
        const GameFiles& gi = si->files();
        const GameFiles& gd = sd->files();
        REQUIRE(gi.entries().size() == gd.entries().size());
        for (std::size_t i = 0; i < gi.entries().size(); ++i) {
            const auto& ei = gi.entries()[i];
            const auto& ed = gd.entries()[i];
            CHECK(ei.name == ed.name);  // isre extract wrote the restored names
            CHECK_MESSAGE(gi.read(ei) == gd.read(ed), ei.name.u8string());
        }
        CHECK(si->dialog_template(321) == sd->dialog_template(321));
        CHECK(si->menu_template(101) == sd->menu_template(101));
        CHECK(si->string(2338) == sd->string(2338));
        CHECK(discover({dist, installed}) == dist);
        CHECK(discover({installed, dist}) == installed);
    }
}

TEST_CASE("assets golden: THEWORKS files are known and are not a game directory") {
    const fs::path installed = env_dir("IS_ORIGINAL_DIR");
    const fs::path dist = dist_dir();
    std::size_t checked = 0;
    for (const fs::path& shipdisk : {installed, dist}) {
        const fs::path tw = theworks_beside(shipdisk);
        if (tw.empty()) continue;
        CAPTURE(tw.u8string());
        ++checked;
        const GameFiles g(tw);
        CHECK(g.form() == DirForm::kInstalled);  // THEWORKS ships its files expanded
        const Identification id = identify(g);
        CHECK(id.files.size() == 141);
        CHECK(id.all_known());
        CHECK(id.packages == std::vector<Package>{Package::kTheWorks});
        for (const FileReport& f : id.files)
            CHECK_MESSAGE((f.known && f.known->package == Package::kTheWorks), f.path.u8string());
        CHECK_FALSE(holds_game(tw));
        CHECK(discover({tw, shipdisk}) == shipdisk);
    }
    if (checked == 0)
        MESSAGE("skipped: no theworks directory beside IS_ORIGINAL_DIR or original/shipdisk");
    if (checked == 1) MESSAGE("one theworks directory checked of two");
}

TEST_CASE("assets golden: identify() over a directory that mixes both packages") {
    const fs::path shipdisk = env_dir("IS_ORIGINAL_DIR");
    const fs::path tw = theworks_beside(shipdisk);
    if (tw.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR or the theworks directory beside it not available");
        return;
    }
    TempDir t;
    const fs::path dir = t.sub("mixed");
    // ATTACK.WAV (theworks) sorts before WAVEMIX.INI (shipdisk), so the packages are found in
    // the order theworks, shipdisk and must come back in enum order. FACTORY.EXE is a name
    // of both packages, with other content in each.
    fs::copy_file(tw / "ATTACK.WAV", dir / "ATTACK.WAV");
    fs::copy_file(tw / "FACTORY.EXE", dir / "FACTORY.EXE");
    fs::copy_file(shipdisk / "WAVEMIX.INI", dir / "WAVEMIX.INI");
    write(dir / "ZZZ.TXT", bytes_of("not shipped"));
    const Identification id = identify(GameFiles(dir));
    CHECK(id.packages == std::vector<Package>{Package::kShipdisk, Package::kTheWorks});
    REQUIRE(id.files.size() == 4);
    const Package want[] = {Package::kTheWorks, Package::kTheWorks, Package::kShipdisk};
    for (std::size_t i = 0; i < 3; ++i) {
        CAPTURE(id.files[i].name.u8string());
        REQUIRE(id.files[i].known);
        CHECK(id.files[i].known->package == want[i]);
    }
    CHECK(id.files[1].known->size == 43488);  // theworks/FACTORY.EXE, not the shipdisk's 39536
    CHECK(id.files[3].known == nullptr);
    CHECK_FALSE(id.files[3].name_known);
    CHECK(id.unknown_count() == 1);
    CHECK(id.shadowed.empty());

    fs::copy_file(shipdisk / "FACTORY.EXE", dir / "FACTORY.EXE",
                  fs::copy_options::overwrite_existing);
    const Identification again = identify(GameFiles(dir));
    REQUIRE(again.files.size() == 4);
    REQUIRE(again.files[1].known);
    CHECK(again.files[1].known->package == Package::kShipdisk);
    CHECK(again.files[1].known->size == 39536);
}
