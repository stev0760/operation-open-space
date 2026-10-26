// The edition check (src/shell/edition.h) against docs/re/shell.md "Edition check" and
// "Editions", docs/re/ispace.md ordinal 24 command 2 and docs/re/formats.md "Resources
// rewritten in place"; the intro's readers of the flags (title lines and colours, text pages,
// the Press 'R' box); and golden runs on the user's INSPACE.EXE. The registered golden reads
// IS_REGISTERED_DIR (INSPACE.EXE and an INSPACE.INI with a [Registration] section) and skips
// when it is unset. No registered record, name or value appears in this file (ADR 0008 d. 6).
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "doctest.h"
#include "formats/ne.h"
#include "intro/intro.h"
#include "intro_fakes.h"
#include "settings/settings.h"
#include "shell/edition.h"

using is::shell::Edition;
using is::shell::EditionInputs;
using is::shell::edition_check;
using is::shell::record_check;
namespace fs = std::filesystem;

namespace {

// 2026-10-23: tm_year 126, tm_yday 295; dayval = 33 * 365 + 295 = 12340.
constexpr int kYear = 126, kYday = 295, kDayval = 12340;

std::vector<std::uint8_t> install_record(std::uint32_t d, std::uint16_t drive = 3) {
    std::vector<std::uint8_t> r(10, 0);
    for (int i = 0; i < 4; ++i) r[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(d >> (8 * i));
    r[4] = static_cast<std::uint8_t>(drive);
    return r;
}

unsigned fixed_rand(int i) { return static_cast<unsigned>(i * 7919 + 13); }

std::vector<std::uint8_t> name_record(const std::string& text) {
    std::vector<std::uint8_t> b(text.begin(), text.end());
    b.resize(50, 0);
    return is::shell::scramble_name(b, fixed_rand);
}

EditionInputs inputs(int v, int s, int p) {
    EditionInputs in;
    in.vidsp = v;
    in.bit32sp = s;
    in.pdev = p;
    in.tm_year = kYear;
    in.tm_yday = kYday;
    return in;
}

// A synthetic registered install: D, and PDev - vidsp = (D & 0xFFFF) mod 653 + 654.
constexpr std::uint32_t kD = 0x00C0FFEE;
constexpr int kV = 3000;
constexpr int kP = kV + static_cast<int>((kD & 0xFFFF) % 653 + 654);

fs::path env_dir(const char* name) {
    const char* v = std::getenv(name);
    if (!v || !*v) return {};
    fs::path p(v);
#ifdef _WIN32
    // Under Wine a Unix path reaches the test as is; drive Z: is the root.
    if (!fs::is_directory(p) && v[0] == '/') p = fs::path(std::string("Z:") + v);
#endif
    return fs::is_directory(p) ? p : fs::path();
}

EditionInputs inputs_from_exe(const fs::path& exe) {
    EditionInputs in;
    const is::fmt::NeModule m = is::fmt::NeModule::load(exe);
    if (const auto* r = m.find(5500, 110))
        in.install_record.assign(m.data(*r), m.data(*r) + std::min<std::size_t>(10, m.size(*r)));
    if (const auto* r = m.find(5000, 143)) {
        in.name_found = true;
        in.name_record.assign(m.data(*r), m.data(*r) + std::min<std::size_t>(50, m.size(*r)));
    }
    in.tm_year = kYear;
    in.tm_yday = kYday;
    return in;
}

}  // namespace

TEST_CASE("edition: the day value is a 16-bit word") {
    CHECK(is::shell::edition_day_value(126, 295) == kDayval);
    CHECK(is::shell::edition_day_value(93, 0) == 0);
    // (283 - 93) * 365 = 69350 wraps to 3814
    CHECK(is::shell::edition_day_value(283, 0) == 3814);
}

TEST_CASE("edition: ordinal 24 command 2 on the NT branch") {
    const auto rec = install_record(kD);
    CHECK(record_check(rec, kV, kV - 1) == 2);  // PDev < vidsp
    CHECK(record_check(install_record(0, 3), 0, 0) == 3);  // D = 0, whatever the drive
    CHECK(record_check({}, 5, 9) == 3);                    // missing record reads as zeros
    CHECK(record_check(rec, kV, kV) == 1);                 // PDev = vidsp
    CHECK(record_check(rec, kV, kP) == 0);                 // the installer's relation
    CHECK(record_check(rec, kV, kP + 1) == 1000);          // mismatch
    // D's high word alone is non-zero: not 3; the relation uses only the low word
    const auto hi = install_record(0x00010000);
    CHECK(record_check(hi, 100, 100 + 0 % 653 + 654) == 0);
}

TEST_CASE("edition: the name record's scramble and command 10") {
    const std::string text = "Test User - Test Org";
    const auto rec = name_record(text);
    for (std::uint8_t b : std::vector<std::uint8_t>(rec.begin() + 20, rec.end()))
        CHECK(b >= 0x80);  // every NUL became -(r % 96 + 2)
    std::string name;
    CHECK(is::shell::read_name(rec, true, &name) == 1);
    CHECK(name == text);
    // a decoded zero in bytes 0-2 returns 0 and moves bytes 3-49 down
    std::vector<std::uint8_t> raw(50, 0);
    const std::string tail = "ABCDEFG";
    for (std::size_t i = 0; i < tail.size(); ++i) raw[3 + i] = static_cast<std::uint8_t>(tail[i]);
    raw[0] = 'x';  // bytes 1 and 2 stay 0
    CHECK(is::shell::read_name(is::shell::scramble_name(raw, fixed_rand), true, &name) == 0);
    CHECK(name == "ABCDEFG");
    // the text stops at +0x30: at most 48 bytes
    CHECK(is::shell::read_name(name_record(std::string(60, 'n')), true, &name) == 1);
    CHECK(name == std::string(48, 'n'));
    // bytes >= 0x80 decode to 0: a cp1252 letter ends the name
    CHECK(is::shell::read_name(name_record("Ren\xe9 X"), true, &name) == 1);
    CHECK(name == "Ren");
    CHECK(is::shell::read_name({}, false, &name) == 0);
    CHECK(name.empty());
}

TEST_CASE("edition: the unregistered shipdisk (all zero, V = P = 0)") {
    EditionInputs in = inputs(0, 0, 0);
    in.install_record = std::vector<std::uint8_t>(10, 0);
    const std::string clear = "Unregistered Copy   ILLEGAL";
    in.name_record.assign(clear.begin(), clear.end());
    in.name_record.resize(50, ' ');
    in.name_found = true;
    int ret = -5;
    const Edition e = edition_check(Edition{}, 1, in, &ret);
    CHECK(e.edition == 0);
    CHECK(e.plus == 0);
    CHECK(e.invalid == 0);
    CHECK(e.checked == 1);
    CHECK(e.record_result == 3);
    CHECK(e.day == kDayval - 254);  // 12086, the oracle's day on 2026-10-23 (smoke-0001)
    CHECK(e.name.empty());
    CHECK(ret == 1);
}

TEST_CASE("edition: a registered record with matching values") {
    EditionInputs in = inputs(kV, 1234, kP);
    in.install_record = install_record(kD);
    in.name_record = name_record("Test User - Test Org");
    in.name_found = true;
    int ret = -5;
    Edition prev;
    prev.day = 77;
    const Edition e = edition_check(prev, 1, in, &ret);
    CHECK(e.edition == -1);
    CHECK(e.plus == 1);
    CHECK(e.invalid == 0);
    CHECK(e.record_result == 0);
    CHECK(e.name == "Test User - Test Org");
    CHECK(e.day == 77);  // the day block does not run
    CHECK(ret == 0);
    // DGROUP:0x920c = -1: the next call returns 1 at once and changes nothing
    EditionInputs other = inputs(0, 0, 0);
    const Edition again = edition_check(e, 0, other, &ret);
    CHECK(ret == 1);
    CHECK(again.edition == -1);
    CHECK(again.plus == 1);
    CHECK(again.name == e.name);
    // a name record that fails the three-byte test: registered, but no Plus
    in.name_record = name_record(std::string("\0\0\0Late", 7));
    const Edition np = edition_check(Edition{}, 0, in, &ret);
    CHECK(np.edition == -1);
    CHECK(np.plus == 0);
    CHECK(np.name == "Late");
    CHECK(ret == 0);
}

TEST_CASE("edition: a mismatch gives DGROUP:0x922a = 1 with the Limited edition") {
    EditionInputs in = inputs(kV, 0, kP + 5);
    in.install_record = install_record(kD);
    int ret = -5;
    const Edition e = edition_check(Edition{}, 0, in, &ret);
    CHECK(e.record_result == 1000);
    CHECK(e.edition == 0);
    CHECK(e.invalid == 1);
    CHECK(e.plus == 0);
    CHECK(ret == 1);
    // k = (P - V) ^ 0xff, dd = dayval - k, day = dd + 1
    const int k = ((kP + 5 - kV) ^ 0xff);
    CHECK(e.day == kDayval - k + 1);
}

TEST_CASE("edition: the 35-day form and the 14-day trial") {
    // P - V = k ^ 0xff with k = dayval - dd; 32BitSP = low16(2 V - P - 1231).
    const auto make = [](int dd, bool code) {
        const int v = 2000;
        const int p = v + ((kDayval - dd) ^ 0xff);
        const int s = code ? static_cast<std::int16_t>(2 * v - p - 1231) : 0;
        return inputs(v, s, p);
    };
    int ret = -5;
    for (int dd : {0, 5, 34}) {
        CAPTURE(dd);
        const Edition e = edition_check(Edition{}, 0, make(dd, true), &ret);
        CHECK(e.record_result == 3);
        CHECK(e.edition == 2);
        CHECK(e.day == -1 - dd);
        CHECK(e.invalid == 0);
        CHECK(ret == 0);
    }
    // expired code: day -1 - dd, Limited, no 922a
    Edition e = edition_check(Edition{}, 0, make(35, true), &ret);
    CHECK(e.edition == 0);
    CHECK(e.day == -36);
    CHECK(e.invalid == 0);
    CHECK(ret == 1);
    // no code: days 0-13 are the trial, edition 1, day dd + 1
    e = edition_check(Edition{}, 0, make(13, false), &ret);
    CHECK(e.edition == 1);
    CHECK(e.day == 14);
    CHECK(ret == 0);
    e = edition_check(Edition{}, 0, make(14, false), &ret);
    CHECK(e.edition == 0);
    CHECK(e.day == 15);
    // before the start date: dd + 1 < 1 gives 200
    e = edition_check(Edition{}, 0, make(-3, false), &ret);
    CHECK(e.edition == 0);
    CHECK(e.day == 200);
    // PDev < vidsp (command 2 = 2) runs the day block as well
    e = edition_check(Edition{}, 0, inputs(500, 0, 400), &ret);
    CHECK(e.record_result == 2);
    CHECK(e.day == static_cast<std::int16_t>(kDayval - ((400 - 500) ^ 0xff) + 1));
}

// ---- the intro's readers of the flags

namespace {

struct EditionHost : intro_test::FakeHost {
    Edition e;
    std::vector<std::uint8_t> page200;
    int checks = 0;
    EditionHost() {
        strings[2124] = "Reg User: ";
        strings[2344] = "invalid order text";
        std::string page;
        for (int i = 0; i < 21; ++i) page += "plus " + std::to_string(i) + "\r\n";
        page200.assign(page.begin(), page.end());
        page200.push_back(0);
    }
    Edition edition_check() override {
        ++checks;
        return e;
    }
    std::vector<std::uint8_t> lang_resource(int type, int id) override {
        if (type == 5000 && id == 200) return page200;
        return FakeHost::lang_resource(type, id);
    }
};

struct EditionRig {
    intro_test::FakeCanvas canvas;
    EditionHost host;
    is::shell::Rng rng;
    std::unique_ptr<is::intro::Intro> intro;
    explicit EditionRig(const Edition& e) {
        host.e = e;
        intro = std::make_unique<is::intro::Intro>(canvas, host, rng);
        intro->enter(host.t);
    }
    std::vector<std::string> title() {
        intro_test::run_until(*intro, host, [&] { return intro->phase() == 2; });
        canvas.ops.clear();
        host.t += 13;
        intro->frame(host.t, false);
        return canvas.with_prefix("text ");
    }
};

Edition registered() {
    Edition e;
    e.edition = -1;
    e.plus = 1;
    e.checked = 1;
    e.name = "Test User - Test Org";
    return e;
}

bool contains(const std::vector<std::string>& v, const std::string& part) {
    for (const auto& s : v)
        if (s.find(part) != std::string::npos) return true;
    return false;
}

}  // namespace

TEST_CASE("edition: the registered title (Plus, gold, Reg User, no Press 'R')") {
    EditionRig r(registered());
    CHECK(r.host.checks == 1);  // the state-0 entry
    const auto t = r.title();
    CHECK(r.host.checks == 2);  // title step 0
    // gold emboss: shadow PALETTERGB(0x74, 0x53, 0x19), main PALETTERGB(0xD8, 0xBC, 0x56)
    CHECK(contains(t, "text 02195374 423 140 6 1 OPERATION:"));
    CHECK(contains(t, "text 0256bcd8 422 139 6 1 OPERATION:"));
    CHECK(contains(t, "text 0256bcd8 512 164 6 1 inner space\x99"));
    CHECK(contains(t, "text 0256bcd8 512 198 6 1  Plus Edition - V1.29.08 Aug 9/2000"));
    CHECK(contains(t, "text 0256bcd8 512 509 6 1 Reg User: Test User - Test Org"));
    CHECK_FALSE(contains(t, "Press 'R'"));
    CHECK_FALSE(contains(t, "trial period"));
    CHECK_FALSE(contains(t, "02ff60a0"));
    CHECK(r.intro->edition() == -1);
    CHECK(r.intro->plus() == 1);
}

TEST_CASE("edition: the registered text pages are #5000 200 with the shorter deadlines") {
    EditionRig r(registered());
    intro_test::run_until(*r.intro, r.host, [&] { return r.intro->phase() == 4; });
    const intro_test::Tick entered = r.host.t;
    CHECK(r.intro->deadline() == entered + 18000);
    CHECK(contains(r.canvas.with_prefix("text "), "plus 0"));
    CHECK(contains(r.canvas.ops, "Inner Space V1.29.08"));
    intro_test::run_until(*r.intro, r.host, [&] { return r.intro->page() == 1; });
    CHECK(r.intro->deadline() - r.host.t == 17000);
    CHECK(contains(r.canvas.with_prefix("text "), "plus 11"));
}

TEST_CASE("edition: the 'isn't valid' title and the 35-day line") {
    Edition bad;
    bad.edition = 0;
    bad.invalid = 1;
    bad.day = 5;
    EditionRig r(bad);
    auto t = r.title();
    CHECK(contains(t, "512 164 6 1 limited inner space\x99"));
    CHECK(contains(t, "512 198 6 1  Standard Edition"));
    CHECK(contains(t, "512 509 6 1 The game isn't valid on this hard drive."));
    CHECK(contains(t, "text 00000000 512 534 6 2 invalid order text"));

    Edition code;
    code.edition = 2;
    code.day = -6;
    EditionRig r2(code);
    t = r2.title();
    CHECK(contains(t, "512 164 6 1 inner space\x99"));  // 2337 for every edition but 0
    CHECK(contains(t, "You are on day 6 of your 35 day demo registration."));
    CHECK_FALSE(contains(t, "Press 'R'"));
}

TEST_CASE("edition: the default host check is the unregistered shipdisk") {
    intro_test::FakeHost h;  // 2026-10-25
    const Edition e = h.is::intro::Host::edition_check();
    CHECK(e.edition == 0);
    CHECK(e.plus == 0);
    CHECK(e.day == 12088);
}

// ---- golden: the user's INSPACE.EXE

TEST_CASE("golden: the shipdisk's INSPACE.EXE gives the Limited edition") {
    const fs::path dir = env_dir("IS_ORIGINAL_DIR");
    if (dir.empty() || !fs::is_regular_file(dir / "INSPACE.EXE")) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    EditionInputs in = inputs_from_exe(dir / "INSPACE.EXE");
    REQUIRE(in.install_record.size() == 10);
    CHECK(in.name_found);
    int ret = -5;
    const Edition e = edition_check(Edition{}, 1, in, &ret);
    CHECK(e.record_result == 3);
    CHECK(e.edition == 0);
    CHECK(e.plus == 0);
    CHECK(e.invalid == 0);
    CHECK(e.day == kDayval - 254);
    CHECK(ret == 1);
}

TEST_CASE("golden: a registered install (IS_REGISTERED_DIR)") {
    const fs::path dir = env_dir("IS_REGISTERED_DIR");
    if (dir.empty()) {
        MESSAGE("skipped: IS_REGISTERED_DIR not set");
        return;
    }
    REQUIRE(fs::is_regular_file(dir / "INSPACE.EXE"));
    EditionInputs in = inputs_from_exe(dir / "INSPACE.EXE");
    const is::settings::IniFile ini(dir / "INSPACE.INI");
    is::settings::RegistrationSettings reg;
    reg.load(ini);
    REQUIRE(reg.present);
    in.vidsp = reg.vidsp;
    in.bit32sp = reg.bit32sp;
    in.pdev = reg.pdev;
    int ret = -5;
    const Edition e = edition_check(Edition{}, 1, in, &ret);
    CHECK(e.record_result == 0);
    CHECK(e.edition == -1);
    CHECK(e.plus == 1);
    CHECK_FALSE(e.name.empty());
    CHECK(ret == 0);
}
