// The music backend choice (src/audio/music_select.h, ADR 0011 decision 3): the [Port] Music
// values, the SoundFont resolution, the default rule and the Wine default, on fake backends.
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "audio/music_select.h"
#include "doctest.h"
#include "settings/settings.h"

namespace m = is::audio::music;
namespace fs = std::filesystem;

namespace {

struct Calls {
    std::vector<std::string> log;      // backend calls, "<name> <call>"
    std::vector<std::string> lines;    // select's log lines
    int synth_made = 0;
    int mci_made = 0;
};

struct FakeBackend final : m::Backend {
    std::string name;
    Calls* c;
    bool open_ok;
    FakeBackend(std::string n, Calls* calls, bool ok) : name(std::move(n)), c(calls), open_ok(ok) {}
    bool open(const fs::path&) override {
        c->log.push_back(name + " open");
        return open_ok;
    }
    bool seek_ms(unsigned, bool) override {
        c->log.push_back(name + " seek");
        return true;
    }
    bool play_to_ms(unsigned) override {
        c->log.push_back(name + " play");
        return true;
    }
    void pause() override { c->log.push_back(name + " pause"); }
    void resume() override { c->log.push_back(name + " resume"); }
    void close() override { c->log.push_back(name + " close"); }
    void set_volume(int v) override { c->log.push_back(name + " volume " + std::to_string(v)); }
};

struct Env {
    Calls calls;
    bool mci_ok = true;
    bool synth_ok = true;      // soundfont
    bool opl3_built = false;
    bool opl3_ok = true;       // a built-in opl3 synth opens
    std::vector<m::Kind> asked;  // make_synth's kinds, in order
    bool default_sf2 = true;   // Z:\usr\share\soundfonts\default.sf2 exists
    fs::path last_sf2;
    m::SelectEnv make(bool wine) {
        m::SelectEnv e;
        e.wine = wine;
        e.make_mci = [this] {
            ++calls.mci_made;
            return std::make_unique<FakeBackend>("mci", &calls, mci_ok);
        };
        e.make_synth = [this](m::Kind k, const fs::path& sf2) -> std::unique_ptr<m::Backend> {
            last_sf2 = sf2;
            asked.push_back(k);
            if (k == m::Kind::kOpl3 && (!opl3_built || !opl3_ok)) return nullptr;
            if (k == m::Kind::kSoundFont && (!synth_ok || sf2.empty())) return nullptr;
            ++calls.synth_made;
            return std::make_unique<FakeBackend>(m::kind_name(k), &calls, true);
        };
        e.exists = [this](const fs::path&) { return default_sf2; };
        e.log = [this](const std::string& s) { calls.lines.push_back(s); };
        return e;
    }
};

}  // namespace

TEST_CASE("music select: the [Port] Music values") {
    bool known = false;
    CHECK(m::parse_kind("", &known) == m::Kind::kMci);
    CHECK(known);
    CHECK(m::parse_kind("mci", &known) == m::Kind::kMci);
    CHECK(m::parse_kind(" SoundFont ", &known) == m::Kind::kSoundFont);
    CHECK(known);
    CHECK(m::parse_kind("OPL3", &known) == m::Kind::kOpl3);
    CHECK(m::parse_kind("fluidsynth", &known) == m::Kind::kMci);
    CHECK_FALSE(known);
    CHECK(std::string(m::kind_name(m::Kind::kSoundFont)) == "soundfont");
    CHECK(std::string(m::kind_name(m::Kind::kOpl3)) == "opl3");
    CHECK(std::string(m::kind_name(m::Kind::kMci)) == "mci");
}

TEST_CASE("music select: the SoundFont resolution") {
    auto yes = [](const fs::path&) { return true; };
    auto no = [](const fs::path&) { return false; };
    CHECK(m::resolve_soundfont("my.sf2", false, no) == fs::path("my.sf2"));
    CHECK(m::resolve_soundfont("my.sf2", true, no) == fs::path("my.sf2"));
    CHECK(m::resolve_soundfont("", false, yes).empty());
    CHECK(m::resolve_soundfont("", true, yes) == fs::path(std::string(m::kWineSoundFont)));
    CHECK(m::resolve_soundfont("", true, no).empty());
}

TEST_CASE("music select: the default is mci; off Wine nothing falls back") {
    Env env;
    env.mci_ok = false;
    auto b = m::select_backend("", "my.sf2", env.make(false));
    REQUIRE(b);
    CHECK_FALSE(b->open("x.mid"));
    CHECK_FALSE(b->open("x.mid"));
    CHECK(env.calls.log == std::vector<std::string>{"mci open", "mci open"});
    CHECK(env.calls.synth_made == 0);
}

TEST_CASE("music select: under Wine an empty key with a SoundFont takes soundfont outright") {
    Env env;
    auto b = m::select_backend("", "", env.make(true));
    REQUIRE(b);
    CHECK(b->open("x.mid"));
    CHECK(env.last_sf2 == fs::path(std::string(m::kWineSoundFont)));
    CHECK(env.calls.synth_made == 1);
    CHECK(env.calls.mci_made == 0);
    CHECK(env.calls.log == std::vector<std::string>{"soundfont open"});
    int default_lines = 0;
    for (const auto& l : env.calls.lines) default_lines += l.find("the Wine default") != std::string::npos;
    CHECK(default_lines == 1);

    Env bad;  // the SoundFont does not open: mci
    bad.synth_ok = false;
    auto c = m::select_backend("", "", bad.make(true));
    CHECK(c->open("x.mid"));
    CHECK(bad.calls.synth_made == 0);
    CHECK(bad.calls.log == std::vector<std::string>{"mci open"});
}

TEST_CASE("music select: under Wine an empty key takes opl3 first when it is built in") {
    Env env;
    env.opl3_built = true;
    auto b = m::select_backend("", "", env.make(true));
    REQUIRE(b);
    CHECK(b->open("x.mid"));
    CHECK(env.calls.log == std::vector<std::string>{"opl3 open"});
    CHECK(env.asked == std::vector<m::Kind>{m::Kind::kOpl3});
    CHECK(env.calls.mci_made == 0);
    REQUIRE_FALSE(env.calls.lines.empty());
    CHECK(env.calls.lines.back() == "music: backend opl3, the Wine default");

    // opl3 built in but not opening: soundfont, then mci.
    Env sf;
    sf.opl3_built = true;
    sf.opl3_ok = false;
    auto c = m::select_backend("", "", sf.make(true));
    REQUIRE(c);  // M7 s2b fixer
    CHECK(c->open("x.mid"));
    CHECK(sf.calls.log == std::vector<std::string>{"soundfont open"});
    CHECK(sf.asked == std::vector<m::Kind>{m::Kind::kOpl3, m::Kind::kSoundFont});

    Env none;  // opl3 not built in, no SoundFont: mci without asking for soundfont
    none.default_sf2 = false;
    auto d = m::select_backend("", "", none.make(true));
    REQUIRE(d);  // M7 s2b fixer
    CHECK(d->open("x.mid"));
    CHECK(none.calls.log == std::vector<std::string>{"mci open"});
    CHECK(none.asked == std::vector<m::Kind>{m::Kind::kOpl3});
    CHECK(none.calls.lines.back() == "music: backend mci");

    // Off Wine the empty key stays mci, built in or not; an explicit soundfont under Wine stays
    // soundfont, and an explicit mci stays mci.
    Env win;
    win.opl3_built = true;
    auto e = m::select_backend("", "", win.make(false));
    REQUIRE(e);  // M7 s2b fixer
    CHECK(e->open("x.mid"));
    CHECK(win.calls.log == std::vector<std::string>{"mci open"});
    CHECK(win.asked.empty());

    Env named;
    named.opl3_built = true;
    auto f = m::select_backend("soundfont", "", named.make(true));
    REQUIRE(f);  // M7 s2b fixer
    CHECK(f->open("x.mid"));
    CHECK(named.calls.log == std::vector<std::string>{"soundfont open"});
    CHECK(named.asked == std::vector<m::Kind>{m::Kind::kSoundFont});

    Env mci;
    mci.opl3_built = true;
    auto g = m::select_backend("mci", "", mci.make(true));
    REQUIRE(g);  // M7 s2b fixer
    CHECK(g->open("x.mid"));
    CHECK(mci.calls.log == std::vector<std::string>{"mci open"});
    CHECK(mci.asked.empty());
}

TEST_CASE("music select: under Wine an explicit mci is mci, and no SoundFont means mci") {
    Env env;
    env.mci_ok = false;
    auto b = m::select_backend("mci", "", env.make(true));
    CHECK_FALSE(b->open("x.mid"));
    CHECK_FALSE(b->open("x.mid"));
    CHECK(env.calls.synth_made == 0);
    CHECK(env.calls.log == std::vector<std::string>{"mci open", "mci open"});

    Env none;
    none.default_sf2 = false;
    auto c = m::select_backend("", "", none.make(true));
    CHECK(c->open("x.mid"));
    CHECK(none.calls.synth_made == 0);
    CHECK(none.calls.log == std::vector<std::string>{"mci open"});
}

TEST_CASE("music select: soundfont and opl3 by name, and their fallbacks") {
    Env env;
    auto b = m::select_backend("soundfont", "gm.sf2", env.make(false));
    CHECK(b->open("x.mid"));
    CHECK(env.calls.log == std::vector<std::string>{"soundfont open"});
    CHECK(env.last_sf2 == fs::path("gm.sf2"));
    CHECK(env.calls.mci_made == 0);

    // A SoundFont that does not load: mci.
    Env bad;
    bad.synth_ok = false;
    bad.mci_ok = false;
    auto c = m::select_backend("soundfont", "broken.sf2", bad.make(true));
    CHECK_FALSE(c->open("x.mid"));
    CHECK(bad.calls.log == std::vector<std::string>{"mci open"});

    // opl3 not built in: mci (an explicit opl3 is not the empty key, so no Wine default).
    Env o;
    o.mci_ok = false;
    auto d = m::select_backend("opl3", "", o.make(true));
    CHECK_FALSE(d->open("x.mid"));
    CHECK(o.calls.log == std::vector<std::string>{"mci open"});

    Env o2;
    o2.opl3_built = true;
    auto e = m::select_backend("opl3", "", o2.make(false));
    CHECK(e->open("x.mid"));
    CHECK(o2.calls.log == std::vector<std::string>{"opl3 open"});
}

TEST_CASE("music select: an unknown value logs and takes the default") {
    Env env;
    auto b = m::select_backend("fluidsynth", "", env.make(false));
    CHECK(b->open("x.mid"));
    CHECK(env.calls.log == std::vector<std::string>{"mci open"});
    REQUIRE_FALSE(env.calls.lines.empty());
    CHECK(env.calls.lines[0].find("fluidsynth") != std::string::npos);
}

TEST_CASE("music select: PortSettings keeps Music and SoundFont as written") {
    is::settings::IniFile ini(fs::temp_directory_path() / "is_test_music_select_absent.ini");
    ini.set("Port", "Music", "SoundFont");
    ini.set("Port", "SoundFont", "C:\\sf\\GeneralUser GS.sf2");
    is::settings::PortSettings p;
    p.load(ini);
    CHECK(p.music == "SoundFont");
    CHECK(p.soundfont == "C:\\sf\\GeneralUser GS.sf2");
    is::settings::IniFile empty(fs::temp_directory_path() / "is_test_music_select_absent2.ini");
    is::settings::PortSettings q;
    q.load(empty);
    CHECK(q.music.empty());
    CHECK(q.soundfont.empty());
}
