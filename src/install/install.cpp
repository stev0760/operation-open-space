#include "install/install.h"

#include <algorithm>
#include <cstring>
#include <ctime>
#include <fstream>
#include <system_error>

#include "assets/game_files.h"
#include "assets/known_files.h"
#include "formats/bytes.h"
#include "formats/ne.h"
#include "formats/sha256.h"
#include "formats/szdd.h"
#include "settings/ini_location.h"
#include "settings/settings.h"
#include "shell/edition.h"

namespace is::install {

namespace {

constexpr const char* kGame = "Inner Space";  // the INI section

std::vector<std::uint8_t> read_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("cannot read " + fmt::path_utf8(p));
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(f)),
                                     std::istreambuf_iterator<char>());
}

void write_file(const fs::path& p, const std::vector<std::uint8_t>& data) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f) throw std::runtime_error("cannot create " + fmt::path_utf8(p));
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!f) throw std::runtime_error("cannot write " + fmt::path_utf8(p));
}

bool all_zero(const std::vector<std::uint8_t>& b) {
    return std::all_of(b.begin(), b.end(), [](std::uint8_t c) { return c == 0; });
}

// The first n bytes of resource type/id of an NE file; empty when missing.
std::vector<std::uint8_t> ne_record(const fs::path& path, std::uint16_t type, std::uint16_t id,
                                    std::size_t n) {
    try {
        const fmt::NeModule m = fmt::NeModule::load(path);
        const fmt::NeResource* r = m.find(type, id);
        if (!r) return {};
        const std::size_t len = std::min(n, m.size(*r));
        return std::vector<std::uint8_t>(m.data(*r), m.data(*r) + len);
    } catch (const std::exception&) {
        return {};
    }
}

bool same_name(const std::string& a, const std::string& b) {
    return fmt::fold_name(a) == fmt::fold_name(b);
}

// INSTALL.EXE seg2:0x22d2(dir): 1 when the directory exists, 2 when it was created, 0 on
// failure [BYTES INSTALL.EXE seg2:0x22d2] (its exact helpers are [OPEN] in install.md).
int ensure_dir(const fs::path& dir) {
    std::error_code ec;
    if (fs::is_directory(dir, ec)) return 1;
    if (fs::create_directory(dir, ec) && !ec) return 2;
    return 0;
}

// A directory holding an installed game: INSPACE.EXE in it, any case (ADR 0002 decision 3).
bool holds_game(const fs::path& dir) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return false;
    const auto want = assets::name_key(fs::path("INSPACE.EXE"));
    for (const auto& e : fs::directory_iterator(dir, ec))
        if (assets::name_key(e.path().filename()) == want) return true;
    return false;
}

bool same_dir(const fs::path& a, const fs::path& b) {
    if (a.empty() || b.empty()) return false;
    std::error_code ec;
    const bool eq = fs::equivalent(a, b, ec);
    return eq && !ec;
}

}  // namespace

const fs::path* SourceSet::find(const std::string& name) const {
    const auto it = files.find(assets::name_key(assets::path_from_cp1252(name)));
    return it == files.end() ? nullptr : &it->second;
}

OpenResult open_source(const fs::path& given_in, const fs::path& work_root, const Log& log) {
    OpenResult out;
    SourceSet s;
    s.given = given_in;
    std::error_code ec;
    fs::path src = given_in;
    if (fs::is_regular_file(src, ec)) {
        std::string ext = fmt::fold_name(fmt::path_utf8(src.extension()));
        if (ext == ".ZIP") {
            // [OPEN] the zip reader is not written yet.
            out.message = "A .zip install set must be unpacked first: " + fmt::path_utf8(src);
            return out;
        }
        src = src.parent_path();  // INSTALL.INI itself
    }
    if (!fs::is_directory(src, ec)) {
        out.message = "No install set at " + fmt::path_utf8(given_in);
        return out;
    }
    // INSTALL.EXE seg2:0x3b38: the source directory is the installer's own directory. The port
    // copies the set into a working directory of its own first, so the write test and
    // SECURE.DAT never touch the user's files [DESIGN].
    fs::path work = work_root / (src.filename().empty() ? fs::path("SET") : src.filename());
    if (fs::exists(work, ec) && fs::equivalent(work, src, ec)) {
        s.dir = src;
    } else {
        fs::remove_all(work, ec);
        fs::create_directories(work, ec);
        if (ec) {
            out.message = "Cannot create " + fmt::path_utf8(work) + ": " + ec.message();
            return out;
        }
        for (const auto& e : fs::directory_iterator(src, ec)) {
            if (!e.is_regular_file()) continue;
            fs::copy_file(e.path(), work / e.path().filename(), fs::copy_options::overwrite_existing,
                          ec);
            if (ec) {
                out.message = "Cannot copy " + fmt::path_utf8(e.path()) + ": " + ec.message();
                return out;
            }
        }
        s.dir = work;
        if (log) log("install: set " + fmt::path_utf8(src) + " copied to " + fmt::path_utf8(work));
    }
    for (const auto& e : fs::directory_iterator(s.dir, ec))
        if (e.is_regular_file()) s.files[assets::name_key(e.path().filename())] = e.path();

    // Validation, version: [Vers] V [BYTES INSTALL.EXE seg2:0x4a92 seg2:0x3d28].
    const fs::path* ini = s.find("INSTALL.INI");
    s.ini = ini ? load_install_ini(*ini) : InstallIni{};
    s.label = version_label(s.ini.v);
    if (s.ini.version == 0) {
        if (log) log("install: INSTALL.INI [Vers] V is missing or 0; the installer exits");
        return out;  // no message, as the original
    }
    // Write test: LZOpenFile(source + "SECURE.DAT", OF_CREATE) [BYTES INSTALL.EXE seg2:0x38cc].
    {
        std::ofstream f(s.dir / "SECURE.DAT", std::ios::binary | std::ios::trunc);
        if (!f) {
            out.message = kMsgWriteProtected;
            return out;
        }
    }
    s.files[assets::name_key(fs::path("SECURE.DAT"))] = s.dir / "SECURE.DAT";
    // ISPACE.DLL: OF_EXIST, then LoadLibrary [BYTES INSTALL.EXE seg2:0x3e79 seg2:0x3e9d]. The
    // port loads no Win16 DLL; an NE header that does not parse stands for "faulty" [DESIGN].
    const fs::path* dll = s.find("ISPACE.DLL");
    if (!dll) {
        out.message = kMsgNoDll;
        return out;
    }
    try {
        (void)fmt::NeModule::load(*dll);
    } catch (const std::exception&) {
        out.message = kMsgDllFaulty;
        return out;
    }
    // Install set: ordinal 24 command 1 reads the source INSTALL.EXE's #5500 120 (0x78 bytes)
    // and #5500 110 (0x10 bytes) [BYTES ISPACE.DLL seg10:0x5416 seg10:0x546c seg10:0x558a].
    const fs::path* exe = s.find("INSTALL.EXE");
    if (exe) {
        s.rec120 = ne_record(*exe, 5500, 120, 0x78);
        s.rec110 = ne_record(*exe, 5500, 110, 0x10);
    }
    s.new_install = all_zero(s.rec120) && all_zero(s.rec110);
    // The rule: the installer the set ships, by SHA-256 (ADR 0002 decision 4, ADR 0008).
    s.rule = Rule::kTrial;
    s.rule_why = "no INSTALL.EXE: trial";
    if (exe) {
        try {
            const std::vector<std::uint8_t> b = read_file(*exe);
            const std::string h = fmt::sha256_hex(b.data(), b.size());
            const assets::KnownFile* k = assets::find_known(fs::path("INSTALL.EXE"), b.size(), h);
            if (k && k->package == assets::Package::kShipdisk) {
                s.rule = Rule::kRegister;
                s.rule_why = std::string("INSTALL.EXE of ") + assets::package_label(k->package) +
                             ": registers";
            } else {
                s.rule_why = "INSTALL.EXE not recognised (" + h.substr(0, 16) + "...): trial";
            }
        } catch (const std::exception& x) {
            s.rule_why = std::string("INSTALL.EXE unreadable: ") + x.what() + ": trial";
        }
    } else if (s.find("SETUP.EXE")) {
        // v1.29.07 SETUP.EXE with Limited != 0 takes ordinal 10 command 9 [BYTES
        // v1.29.07/SETUP.EXE seg2:0x3fda]; its hash is not in the table (not in original/).
        s.rule_why = "SETUP.EXE (free installer): trial";
    }
    if (log) log("install: " + s.label + ", " + s.rule_why);
    out.set = std::move(s);
    return out;
}

// ---- dialog 300

Components initial_components(const SourceSet& set) {
    Components c;
    if (!set.new_install) c.caption = kCaptionReinstall;  // DGROUP:0x7b6 = 1
    // [BYTES INSTALL.EXE seg2:0x955 seg2:0x993 seg2:0x9c1 seg2:0x9ef seg2:0xa1b]
    if (!set.find("FACTORY.EX_")) {
        c.checked[1] = false;
        c.factory_hidden = true;
    }
    const auto off = [&](const char* name, std::size_t i) {
        if (!set.find(name)) {
            c.checked[i] = false;
            c.enabled[i] = false;
        }
    };
    off("INSPACE.SC1", 5);
    off("MAIN.MI1", 4);
    off("SND_FX.DLL", 2);
    off("ENG_FX.DLL", 3);
    return c;
}

int required_kb(const std::array<bool, 6>& checked) {
    // seg2:0xb54: 46 + 771 (119), 37 (124), 336 (135), 316 (125), 239 (306), 21 (304).
    int kb = kBaseKb;
    for (std::size_t i = 0; i < checked.size(); ++i)
        if (checked[i]) kb += kSizesKb[i];
    return kb;
}

std::string complete_dir(std::string t) {
    if (t.size() == 1) t += ':';
    if (t.empty() || (t.back() != '\\' && t.back() != '/')) t += '\\';
    return t;
}

fs::path dir_path(const std::string& cp1252) {
    std::string s = cp1252;
#ifndef _WIN32
    for (char& c : s)
        if (c == '\\') c = '/';
#endif
    return assets::path_from_cp1252(s);
}

std::string ini_dir(const fs::path& dir) {
    std::string s;
    for (char16_t c : dir.u16string()) {
        // cp1252 for the Latin-1 range; anything else as '?' (ADR 0003 leaves no other choice
        // for a Win16 INI value).
        s.push_back(c < 0x100 ? static_cast<char>(c) : '?');
    }
    for (char& c : s)
        if (c == '/') c = '\\';
    if (s.empty() || s.back() != '\\') s.push_back('\\');
    return s;
}

bool dir_valid(const fs::path& dir) {
    // seg2:0x38ee: chdir, else mkdir and chdir; then rmdir (an empty directory goes) and the
    // compare with _getdcwd [BYTES INSTALL.EXE seg2:0x3952 seg2:0x3961 seg2:0x3995 seg2:0x39ad
    // seg2:0x39be].
    std::error_code ec;
    fs::path p = dir;
    if (!p.has_filename()) p = p.parent_path();
    if (p.empty() || !p.is_absolute()) return false;
    const bool existed = fs::is_directory(p, ec);
    if (!existed) {
        if (!fs::create_directory(p, ec) || ec) return false;
    }
    const fs::path abs = fs::absolute(p, ec);
    const bool same = !ec && fs::equivalent(abs, p, ec) && !ec;
    if (!existed) fs::remove(p, ec);  // rmdir: only the empty directory just made
    return same;
}

// ---- the copy

std::optional<std::pair<fs::path, bool>> source_for(const SourceSet& set, const std::string& key) {
    // [BYTES INSTALL.EXE seg2:0x36f4 seg2:0x371f seg2:0x3747 seg2:0x378a]
    std::optional<std::pair<fs::path, bool>> r;
    bool lz = false;
    if (const fs::path* p = set.find(probe_name(key, '1'))) {
        r = std::make_pair(*p, true);
        lz = true;
    }
    if (const fs::path* p = set.find(probe_name(key, '_'))) r = std::make_pair(*p, lz);
    if (!r) {
        if (const fs::path* p = set.find(key)) r = std::make_pair(*p, false);
    }
    return r;
}

std::vector<std::uint8_t> read_source(const fs::path& path, bool lz) {
    std::vector<std::uint8_t> b = read_file(path);
    if (lz && fmt::is_szdd(b.data(), b.size())) {
        fmt::SzddFile f = fmt::szdd_expand(b.data(), b.size());
        return std::move(f.data);
    }
    return b;
}

// ---- dialogs 1922 and 1774

void write_difficulty(settings::IniFile& ini, int d, int sp) {
    const int complex = d == 142 ? 0 : d == 143 ? 1 : d == 144 ? 2 : d == 145 ? 3 : 4;
    ini.set_int(kGame, "Complex", complex);
    if (d == 142 || d == 143) ini.set(kGame, "Braking", "0");
    const int rate = sp == 137 ? 20 : sp == 138 ? 36 : sp == 139 ? 54 : 72;
    ini.set_int(kGame, "FrameRateSet", rate);
}

void write_full(settings::IniFile& ini, bool full) { ini.set_int(kGame, "Full", full ? 1 : 0); }

// ---- dialog 135 and the speed test

std::string cut_text(const std::string& s, std::size_t nmax) {
    std::string r = s.substr(0, s.find('\0'));
    if (nmax > 0 && r.size() > nmax - 1) r.resize(nmax - 1);
    return r;
}

std::string name_org(const std::string& name, const std::string& org) {
    return name + " - " + org;
}

std::string the_player(const std::string& name) {
    std::string r = name;
    for (std::size_t i = 0; i < 20 && i < r.size(); ++i)
        if (r[i] == ' ') {
            r.resize(i);
            break;
        }
    return r;
}

std::uint32_t speed_polls(const std::function<std::uint32_t()>& tick) {
    std::uint32_t n = 0;
    const std::uint32_t t0 = tick();
    const std::uint32_t end = t0 + 1000u;  // add/adc: 32-bit
    do {
        ++n;
    } while (tick() < end);
    return n;
}

int detail_from_polls(std::uint32_t n) {
    if (n > 0x55730u) return 3;
    if (n > 0x2bf20u) return 2;
    if (n > 0x11170u) return 1;
    return 0;
}

// ---- registration

namespace {
shell::Rng* g_scramble_rng = nullptr;
unsigned scramble_draw(int) { return static_cast<unsigned>(g_scramble_rng->rand()); }
}  // namespace

std::vector<std::uint8_t> name_record(const std::string& text, shell::Rng& rng, std::uint16_t seed) {
    // ISPACE.DLL seg10:0x4dba: srand(GetCurrentTime()) in both modes [BYTES ISPACE.DLL
    // seg10:0x4dc9].
    std::vector<std::uint8_t> b(50, 0);
    for (std::size_t i = 0; i < text.size() && i < 50; ++i)
        b[i] = static_cast<std::uint8_t>(text[i]);
    rng.srand(seed);
    g_scramble_rng = &rng;
    b = shell::scramble_name(std::move(b), scramble_draw);
    g_scramble_rng = nullptr;
    return b;
}

std::vector<std::uint8_t> install_record(shell::Rng& rng, int drive) {
    std::vector<std::uint8_t> r(10, 0);  // GlobalAlloc(0x42, 10): zeroed
    const std::int16_t d = static_cast<std::int16_t>(rng.rand() % 7651 + 5432);  // seg10:0x4bcb
    const std::int32_t dd = d;  // cdq
    const auto u = static_cast<std::uint32_t>(dd);
    r[0] = static_cast<std::uint8_t>(u);
    r[1] = static_cast<std::uint8_t>(u >> 8);
    r[2] = static_cast<std::uint8_t>(u >> 16);
    r[3] = static_cast<std::uint8_t>(u >> 24);
    const auto w = static_cast<std::uint16_t>(drive);
    r[4] = static_cast<std::uint8_t>(w);
    r[5] = static_cast<std::uint8_t>(w >> 8);
    return r;
}

int record_x(const std::vector<std::uint8_t>& rec) {
    const unsigned lo = rec.size() >= 2 ? static_cast<unsigned>(rec[0] | (rec[1] << 8)) : 0u;
    return static_cast<int>(static_cast<std::uint16_t>(lo % 653u + 654u));
}

RegValues ini_values(int mode, int x, const RegValues& stored, shell::Rng& rng, std::uint16_t seed) {
    const auto w16 = [](int v) { return static_cast<int>(static_cast<std::int16_t>(v)); };
    rng.srand(seed);  // seg10:0x5b3f
    const int r1 = rng.rand() % 1111;
    const int r2 = rng.rand() % 8999;
    int s = w16(r1 + r2 + 1987);  // [bp-4]
    int pdev = 0;                 // [bp-6]
    if (mode == 0x17 || mode == 0x38)
        pdev = w16(s + x);  // seg10:0x5b95
    else if (mode == 0x54 || mode == 0x5b || mode == 0xbe)
        pdev = w16((x ^ 0xff) + s);  // seg10:0x5c41: xor al, 0xff
    RegValues out;
    // vidsp [BYTES ISPACE.DLL seg10:0x5c81-0x5cce]
    if (stored.vidsp == 0 || mode == 0x17 || mode == 0x38 || mode == 0x75 || mode == 0x5b)
        out.vidsp = s;
    else
        s = out.vidsp = w16(stored.vidsp);
    // PDev [BYTES ISPACE.DLL seg10:0x5cdb-0x5d35]
    if (stored.pdev == 0 || mode == 0x17 || mode == 0x38 || mode == 0x75 || mode == 0x9d ||
        mode == 0x5b) {
        if (mode == 0x9d) pdev = w16(-stored.pdev);
        out.pdev = pdev;
    } else {
        out.pdev = pdev = w16(stored.pdev);
    }
    // 32BitSP [BYTES ISPACE.DLL seg10:0x5d38-0x5dc3]
    const int xx = w16(2 * s - pdev - 0x4cf);
    int v32;
    if (mode == 0x5b) {
        v32 = xx;
    } else {
        v32 = rng.rand();
        if (v32 == xx) --v32;
    }
    if (mode == 0x54 && stored.bit32sp != 0)
        out.bit32sp = stored.bit32sp;
    else
        out.bit32sp = v32;
    return out;
}

bool write_resource(const fs::path& path, std::uint16_t type, std::uint16_t id,
                    const std::vector<std::uint8_t>& bytes, std::string* why) {
    try {
        std::error_code ec;
        const auto when = fs::last_write_time(path, ec);
        const fmt::NeModule m = fmt::NeModule::load(path);
        const fmt::NeResource* r = m.find(type, id);
        if (!r || m.size(*r) < bytes.size()) {
            if (why) *why = "resource " + std::to_string(type) + " " + std::to_string(id) + " missing";
            return false;
        }
        {
            std::fstream f(path, std::ios::binary | std::ios::in | std::ios::out);
            if (!f) throw std::runtime_error("cannot open " + fmt::path_utf8(path));
            f.seekp(static_cast<std::streamoff>(r->offset));
            f.write(reinterpret_cast<const char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
            if (!f) throw std::runtime_error("cannot write " + fmt::path_utf8(path));
        }
        if (!ec) fs::last_write_time(path, when, ec);  // int 21h 5701h in the original
        return true;
    } catch (const std::exception& x) {
        if (why) *why = x.what();
        return false;
    }
}

// ---- the whole step

namespace {

struct Step {
    const SourceSet& set;
    Ui& ui;
    fs::path ini_path;
    fs::path game;
    std::string game_text;  // DGROUP:0x1100 as the INI writes it
    bool keep_ini = false;     // DGROUP:0x13ec
    bool keep_master = false;  // DGROUP:0x13ee
    int percent = 0;           // DGROUP:0x1414
    bool logged_8 = false;
    // What went wrong without ending the step, shown before dialog 100 [DESIGN].
    std::vector<std::string> problems;

    void log(const std::string& s) const {
        if (ui.log) ui.log("install: " + s);
    }
    fs::path bak() const { return ini_path.parent_path() / "INSPACE.BAK"; }

    // seg2:0x3376: INSPACE.BAK back over INSPACE.INI [BYTES INSTALL.EXE seg2:0x3376].
    void restore() {
        std::error_code ec;
        if (fs::exists(bak(), ec))
            fs::copy_file(bak(), ini_path, fs::copy_options::overwrite_existing, ec);
    }

    fs::path target_dir(int code) {
        // seg2:0x1950 [BYTES INSTALL.EXE seg2:0x1950 seg2:0x19ab seg2:0x1a18 seg2:0x1a3d].
        if (code >= 1 && code <= 7) {
            const std::size_t n = static_cast<std::size_t>(code - 1);
            if (n < set.ini.dirs.size()) return game / assets::path_from_cp1252(set.ini.dirs[n]);
            return game;
        }
        if (code == 8 || code == 9) {
            // The Windows directory (8) and After Dark's directory (9): the port needs none
            // of these files (CTL3D.DLL, WAVEMIX.DLL/INI, INSPACE.SCR, INRUN.AD) and puts them
            // in the game directory [DESIGN] (ADR 0008 decision 1).
            if (!logged_8) log("destination codes 8 and 9 go to the game directory [DESIGN]");
            logged_8 = true;
        }
        return game;
    }

    // seg2:0x1950 + seg2:0x36c2 + seg2:0x37be for one group.
    bool copy_group(int g, std::size_t& done, std::size_t total, const fs::path* to_dir) {
        const auto& entries = set.ini.groups[static_cast<std::size_t>(g)];
        for (std::size_t i = 0; i < entries.size(); ++i) {
            const IniEntry& e = entries[i];
            CopyItem it;
            it.group = g;
            it.index = i;
            it.key = e.name;
            const fs::path dir = to_dir ? *to_dir : target_dir(e.code);
            std::string name = e.name;
            // Outside backup mode INSTALL.EXE becomes BACKUP.EXE [BYTES INSTALL.EXE seg2:0x1b25].
            if (same_name(name, "INSTALL.EXE")) name = "BACKUP.EXE";
            it.target = to_dir ? ini_path : dir / assets::path_from_cp1252(name);
            // Skips [BYTES INSTALL.EXE seg2:0x1aa9 seg2:0x1aed seg2:0x1b03 seg2:0x1b18].
            std::error_code ec;
            if (g == 0 && i <= 1 && keep_master) {
                if (fs::exists(it.target, ec))
                    it.skip = true;
                else
                    keep_master = false;
            }
            if (g == 7 && i == 0 && keep_ini) it.skip = true;
            if (!it.skip) {
                const auto src = source_for(set, e.name);
                if (!src) {
                    // OpenFile fails; what the original does then is [OPEN].
                    log("source missing for " + e.name + " [OPEN]");
                    problems.push_back("missing in the install set: " + e.name);
                } else {
                    it.source = src->first;
                    it.lz = src->second;
                    try {
                        fs::create_directories(it.target.parent_path(), ec);
                        write_file(it.target, read_source(it.source, it.lz));
                    } catch (const std::exception& x) {
                        log(x.what());
                        return false;
                    }
                }
            }
            ++done;
            // Dialog 200: control 120 the file name, 130 the percent. The percent formula is
            // [OPEN INSTALL.EXE seg2:0x15a6]; the port counts files.
            percent = total ? static_cast<int>(done * 100 / total) : 100;
            if (ui.progress) ui.progress(e.name, percent);
        }
        return true;
    }
};

}  // namespace

Result run_install(const SourceSet& set, const fs::path& ini_path, const std::string& default_dir,
                   Ui& ui) {
    Result res;
    res.ini_path = ini_path;
    res.rule = set.rule;
    Step st{set, ui, ini_path, {}, {}, false, false, 0, false, {}};
    const auto ask = [&](const std::string& t, unsigned f) { return ui.message ? ui.message(t, f) : kIdNo; };

    // Start-up step 3 [BYTES INSTALL.EXE seg2:0x3d2e].
    ask(kMsgExitPrograms, kMbExclamation);

    // Installed before [BYTES INSTALL.EXE seg2:0x27da seg2:0x289e seg2:0x28b6 seg2:0x2904
    // seg2:0x2918]: the port's INSPACE.INI stands for <Windows directory>\INSPACE.INI
    // (ADR 0008 decision 1).
    int kept_fx = 0, kept_voice = 0, kept_music = 0;
    // The registration values as they stand before the step. The original keeps them in
    // SYSTEM.INI and WIN.INI, which the INSPACE.INI copy below never touches, so a reinstall
    // keeps a trial's day (seg10:0x5b24 keeps non-zero stored values, formats.md "The INI
    // values"); the port's [Registration] lives in the INI that group 7 replaces, so the step
    // carries the values across [DESIGN ADR 0008 decision 1].
    settings::RegistrationSettings prior;
    // The game directory a previous run of this step installed ([Port] Installed): the only
    // existing game directory the step writes into [DESIGN ADR 0008 decision 1].
    fs::path own_previous;
    std::error_code ec;
    if (fs::exists(ini_path, ec)) {
        settings::IniFile old(ini_path);
        prior.load(old);
        own_previous = settings::path_from_ini(old.get_string(settings::kPortSection, kInstalledKey, "", 0x6e));
        kept_fx = old.get_int(kGame, "SoundFX", 0);
        kept_voice = old.get_int(kGame, "VoiceFX", 0);
        kept_music = old.get_int(kGame, "Music", 0);
        fs::copy_file(ini_path, st.bak(), fs::copy_options::overwrite_existing, ec);
        if (ask(kMsgInstalledBefore, kMbYesNoQuestionDefNo) == kIdYes)
            st.keep_ini = st.keep_master = true;
    }

    // The first phase seg2:0xf5a(1): group [7] into the INI's directory [BYTES INSTALL.EXE
    // seg2:0x2935 seg2:0x1b11]. Destination 0 of the first phase is the Windows directory;
    // the port's is the INI's own path [DESIGN].
    std::size_t done = 0;
    {
        const fs::path dir = ini_path.parent_path();
        fs::create_directories(dir, ec);
        if (!st.copy_group(7, done, set.ini.groups[7].size(), &dir)) return res;
    }
    // seg2:0x100e(1) with DGROUP:0x4e = 1: LocalPath, Path and ShipPath = the Windows directory,
    // SndPath = the source [BYTES INSTALL.EXE seg2:0x1351]. The port writes the INI's directory
    // there; the second phase overwrites all four. The SYSTEM.INI [drivers] line is not written
    // (ADR 0008 consequences) [DESIGN]. Ordinal 4 command 0 (INSTALL.EXE seg2:0x4098) is [OPEN];
    // the oracle's SoundSystem=3 comes from that ISPACE.DLL call and is not written.
    {
        settings::IniFile ini(ini_path);
        const std::string wd = ini_dir(ini_path.parent_path());
        ini.set(kGame, "LocalPath", wd);
        ini.set(kGame, "Path", wd);
        ini.set(kGame, "ShipPath", wd);
        ini.set(kGame, "SndPath", ini_dir(set.dir));
        ini.save();
    }
    if (!set.new_install)
        st.log("the set's INSTALL.EXE records name a previous install; the reinstall and demo "
               "questions of seg2:0x46af are [OPEN]; installing as new");

    // Dialog 300.
    Components c = initial_components(set);
    c.dir = default_dir;
    for (;;) {
        if (!ui.components || !ui.components(c)) {
            // Cancel: 0x7f2 -> seg2:0x3376 and DestroyWindow [BYTES INSTALL.EXE seg2:0xe6c].
            st.restore();
            st.log("cancelled in dialog 300");
            return res;
        }
        st.game_text = complete_dir(c.dir);
        st.game = dir_path(st.game_text);
        // The port never writes into a game directory it did not install: a directory that
        // holds INSPACE.EXE is refused unless it is [Port] Installed, and dialog 300 opens
        // again. The original overwrites it (and asks nothing for C:\SPACE\) [DESIGN ADR 0008
        // decision 1].
        if (holds_game(st.game) && !same_dir(st.game, own_previous)) {
            ask(st.game_text + kMsgForeignGameTail, kMbStop);
            st.log("refused: " + fmt::path_utf8(st.game) + " holds a game this step did not install");
            continue;
        }
        break;
    }
    res.game_dir = st.game;

    // Directory and screen saver: seg2:0x1c0c [BYTES INSTALL.EXE seg2:0x1c0c seg2:0x1c75].
    const int made = ensure_dir(st.game);
    if (made == 1 && st.game_text != kDefaultDir) {
        if (ask(st.game_text + kMsgExistsTail, kMbYesNoQuestion) == kIdNo) {
            st.restore();
            return res;
        }
    }
    if (made == 0) {
        st.log("cannot create " + fmt::path_utf8(st.game));
        st.restore();
        return res;
    }
    // An empty first [Dirs] key ends the installer: seg2:0x1c0c returns 0 [BYTES INSTALL.EXE
    // seg2:0x1cac seg2:0x1cb1].
    if (set.ini.dirs.empty() || set.ini.dirs.front().empty()) {
        st.log("INSTALL.INI [Dirs] is empty; the installer ends");
        st.restore();
        return res;
    }
    for (const std::string& d : set.ini.dirs) {
        if (ensure_dir(st.game / assets::path_from_cp1252(d)) == 0) {
            st.restore();
            return res;
        }
    }
    if (c.checked[5]) {
        // DGROUP:0x50; its SYSTEM.INI [boot] SCRNSAVE.EXE write is not ported (ADR 0008
        // decision 1: the port writes no Windows file) [DESIGN].
        const int a = ask(kMsgScreenSaver, kMbYesNoQuestion);
        st.log(std::string("screen saver answer ") + (a == kIdNo ? "No" : "Yes") +
               "; SYSTEM.INI [boot] is not written [DESIGN]");
    }

    // The copy, groups 0-6 by the flags [BYTES INSTALL.EXE seg2:0xdd6 seg2:0xe7d-0xef8].
    std::array<bool, kGroupCount> flag{};
    flag[0] = true;
    for (std::size_t i = 0; i < 6; ++i) flag[i + 1] = c.checked[i];
    std::size_t total = 0;
    for (int g = 0; g < 7; ++g)
        if (flag[static_cast<std::size_t>(g)]) total += set.ini.groups[static_cast<std::size_t>(g)].size();
    done = 0;
    st.percent = 0;
    for (int g = 0; g < 7; ++g)
        if (flag[static_cast<std::size_t>(g)] && !st.copy_group(g, done, total, nullptr)) return res;
    if (total == 0) st.percent = 100;

    // INSPACE.INI after the copy: seg2:0x100e(0) [BYTES INSTALL.EXE seg2:0x10f0-0x1302].
    settings::IniFile ini(ini_path);
    if (kept_fx) ini.set(kGame, "SoundFX", "1");
    if (kept_voice) ini.set(kGame, "VoiceFX", "1");
    if (kept_music) ini.set(kGame, "Music", "1");
    // LocalPath is the INI's directory (ADR 0004 decision 5) [DESIGN]; the original writes the
    // install directory.
    ini.set(kGame, "LocalPath", ini_dir(ini_path.parent_path()));
    ini.set(kGame, "Path", ini_dir(st.game));
    const auto key = [&](std::size_t n) { return n < set.ini.dirs.size() ? set.ini.dirs[n] : std::string(); };
    ini.set(kGame, "ShipPath", ini_dir(st.game) + key(0) + "\\");
    ini.set(kGame, "SndPath", ini_dir(st.game) + key(1) + "\\");
    ini.set(kGame, "Games", key(2));
    ini.set(kGame, "RacePath", ini_dir(st.game) + key(2) + "\\");
    ini.set(kGame, "Dead", key(3));
    ini.save();

    // Dialogs 1922, 1774 and 135.
    int diff = 144, speed = 138;
    if (ui.difficulty) ui.difficulty(diff, speed);
    write_difficulty(ini, diff, speed);
    ini.save();
    write_full(ini, ui.window ? ui.window() : true);
    ini.save();
    std::string name, org;
    if (ui.personalize) ui.personalize(name, org);
    name = cut_text(name, 0x12);
    org = cut_text(org, 0x16);

    // ISPACE.DLL's own rand() state, 1 at load (ISPACE.DLL DGROUP:0xb8c, rng.md) [DESIGN].
    shell::Rng rng;
    const auto seed = [&] { return static_cast<std::uint16_t>(ui.tick ? ui.tick() : 0); };
    const fs::path exe = st.game / "INSPACE.EXE";
    std::string why;
    // Ordinal 24 command 9 (seg2:0x43a4): the name record into the new INSPACE.EXE.
    bool record_failed = false;
    if (!write_resource(exe, 5000, 143, name_record(name_org(name, org), rng, seed()), &why)) {
        st.log("#5000 143: " + why);
        st.problems.push_back("INSPACE.EXE #5000 143 not written: " + why);
        record_failed = true;
    }
    ini.set(kGame, "ThePlayer", the_player(name));
    ini.save();
    const int detail = detail_from_polls(speed_polls(ui.tick ? ui.tick : [] {
        static std::uint32_t t = 0;
        return t += 1;
    }));
    ini.set_int(kGame, "Detail", detail);
    ini.save();

    // Registration: seg2:0x130b -> seg2:0x43da [BYTES INSTALL.EXE seg2:0x43da seg2:0x4433
    // seg2:0x4445]. The three values go to the port INI's [Registration] (ADR 0008 decision 1);
    // the original resets the SYSTEM.INI and WIN.INI dates, which the port does not touch.
    // The stored values are those from before the step (see `prior` above) [DESIGN ADR 0008].
    const RegValues stored{prior.vidsp, prior.pdev, prior.bit32sp};
    int day = 0;
    if (ui.date) {
        int year = 0, yday = 0;
        ui.date(year, yday);
        day = shell::edition_day_value(year, yday);
    } else {
        const std::time_t t = std::time(nullptr);
        if (const std::tm* tm = std::localtime(&t)) day = shell::edition_day_value(tm->tm_year, tm->tm_yday);
    }
    RegValues v;
    if (set.rule == Rule::kRegister) {
        // Command 0: seg10:0x5612 (nothing off A: and B:), then seg10:0x4b1a(b, 0). The drive
        // word is the game directory's letter - 0x40 (LocalPath in the original) [DESIGN].
        const char letter = st.game_text.empty() ? 0 : st.game_text[0];
        const int drive = static_cast<int>(static_cast<signed char>(
                              (letter >= 'a' && letter <= 'z') ? letter - 0x20 : letter)) - 0x40;
        const std::vector<std::uint8_t> rec = install_record(rng, drive);
        if (!write_resource(exe, 5500, 110, rec, &why)) {
            st.log("#5500 110: " + why);
            st.problems.push_back("INSPACE.EXE #5500 110 not written: " + why);
            record_failed = true;
        }
        v = ini_values(0x17, record_x(rec), stored, rng, seed());
        res.registered = true;  // seg2:0x43da returns 1 -> DGROUP:0x13f6
    } else {
        // Ordinal 10 (0, 0, 0, day, 0x54, 0, 9) -> seg10:0x5b24(0x54, day) [BYTES ISPACE.DLL
        // seg9:0x14da].
        v = ini_values(0x54, day, stored, rng, seed());
    }
    ini.set_int(settings::kRegistrationSection, "vidsp", v.vidsp);
    ini.set_int(settings::kRegistrationSection, "32BitSP", v.bit32sp);
    ini.set_int(settings::kRegistrationSection, "PDev", v.pdev);
    // The marker of the port's own install (see the refusal after dialog 300) [DESIGN].
    ini.set(settings::kPortSection, kInstalledKey, ini_dir(st.game));
    ini.save();
    st.log(std::string("registration: ") + (res.registered ? "command 0 (registered)" : "trial start"));

    // Dialog 100 [BYTES INSTALL.EXE seg2:0x1627 seg2:0x165f seg2:0x1673]. The original reports
    // success whatever its record writes did; the port lists what went wrong first, and a
    // failed record write counts as an unfinished install, so the game does not start on a
    // copy whose records and values disagree [DESIGN].
    if (!st.problems.empty()) {
        std::string text = "The installation is incomplete:";
        for (const std::string& p : st.problems) text += "\n" + p;
        ask(text, kMbExclamation);
    }
    res.completed = st.percent == 100 && !record_failed;
    const char* text = !res.completed ? kDoneCancelled : res.registered ? kDoneValid : kDoneDemo;
    if (ui.done) {
        const fs::path* readme = set.find("README.TXT");
        ui.done(text, readme ? *readme : fs::path());
    }
    return res;
}

}  // namespace is::install
