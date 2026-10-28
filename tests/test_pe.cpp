// The PE reader (src/formats/pe): synthetic images, malformed ones, and the fixture drive's
// programs (oracle/fixtures/make_fixtures.py), which skip when the fixture is absent.
#include <string>

#include "doctest.h"
#include "fixture_dir.h"
#include "formats/icon.h"
#include "formats/pe.h"
#include "pe_builder.h"

using namespace is::fmt;
namespace fs = std::filesystem;

namespace {
std::vector<std::uint8_t> bytes(std::initializer_list<int> v) {
    std::vector<std::uint8_t> b;
    for (int x : v) b.push_back(static_cast<std::uint8_t>(x));
    return b;
}
std::vector<PeFakeRes> sample() {
    return {{RTPE_GROUP_ICON, 0, u"MAIN", 0x409, bytes({1, 2, 3})},
            {RTPE_GROUP_ICON, 2, u"", 0x409, bytes({4, 5})},
            {RTPE_GROUP_ICON, 2, u"", 0x407, bytes({6})},
            {RTPE_ICON, 1, u"", 0x409, bytes({7, 8, 9, 10})},
            {RTPE_ICON, 5, u"", 0, bytes({11})},
            {24, 1, u"", 0x409, bytes({12})}};
}
}  // namespace

TEST_CASE("pe: the resource tree in directory order, with names, languages and data") {
    for (bool plus : {false, true}) {
        INFO("PE32+ " << plus);
        const auto img = build_pe(sample(), plus);
        CHECK(module_kind(img.data(), img.size()) == ModuleKind::kPe);
        PeModule m(img);
        CHECK(m.pe32_plus() == plus);
        CHECK(m.machine() == (plus ? 0x8664 : 0x14C));
        REQUIRE(m.sections().size() == 1);
        CHECK(m.sections()[0].name == ".rsrc");
        CHECK(m.sections()[0].virtual_address == 0x1000);
        REQUIRE(m.resources().size() == 6);
        CHECK(m.resources()[0].name == u"MAIN");
        CHECK(m.resources()[0].id == 0);
        CHECK(m.resources()[1].id == 2);
        CHECK(m.resources()[1].lang == 0x409);
        CHECK(m.resources()[2].lang == 0x407);
        CHECK(m.resources()[5].type == 24);

        // all(): one entry per name, first language
        const auto groups = m.all(RTPE_GROUP_ICON);
        REQUIRE(groups.size() == 2);
        CHECK(groups[0]->name == u"MAIN");
        CHECK(groups[1]->id == 2);
        CHECK(groups[1]->lang == 0x409);
        CHECK(m.all(RTPE_ICON).size() == 2);

        const PeResource* r = m.find(RTPE_ICON, 1);
        REQUIRE(r);
        REQUIRE(m.size(*r) == 4);
        CHECK(m.data(*r)[0] == 7);
        CHECK(m.data(*r)[3] == 10);
        const PeResource* g = m.find(RTPE_GROUP_ICON, 2);
        REQUIRE(g);
        CHECK(m.data(*g)[0] == 4);
        REQUIRE(m.find(RTPE_GROUP_ICON, u"MAIN"));
        CHECK(m.data(*m.find(RTPE_GROUP_ICON, u"MAIN"))[2] == 3);
        CHECK_FALSE(m.find(RTPE_ICON, 2));
        CHECK_FALSE(m.find(RTPE_GROUP_ICON, u"OTHER"));
    }
}

TEST_CASE("pe: an image without resources lists none") {
    auto img = build_pe({});
    // clear the resource data directory
    const std::size_t dir = 0x44 + 20 + 92 + 4 + 2 * 8;
    for (int k = 0; k < 8; ++k) img[dir + k] = 0;
    PeModule m(img);
    CHECK(m.resources().empty());
    CHECK(m.all(RTPE_GROUP_ICON).empty());
}

TEST_CASE("pe: module kinds") {
    CHECK(module_kind(nullptr, 0) == ModuleKind::kNone);
    const auto txt = bytes({'h', 'e', 'l', 'l', 'o'});
    CHECK(module_kind(txt.data(), txt.size()) == ModuleKind::kNone);
    std::vector<std::uint8_t> mz(0x80, 0);
    mz[0] = 'M'; mz[1] = 'Z'; mz[0x3C] = 0x40;
    CHECK(module_kind(mz.data(), mz.size()) == ModuleKind::kNone);  // plain DOS program
    mz[0x40] = 'N'; mz[0x41] = 'E';
    CHECK(module_kind(mz.data(), mz.size()) == ModuleKind::kNe);
    mz[0x3C] = 0xFF; mz[0x3D] = 0xFF;  // e_lfanew past the end
    CHECK(module_kind(mz.data(), mz.size()) == ModuleKind::kNone);
}

TEST_CASE("pe: malformed images throw FormatError") {
    const auto good = build_pe(sample());
    auto bad = good;
    bad[0] = 'X';
    CHECK_THROWS_AS(PeModule{bad}, FormatError);
    bad = good;
    bad[0x42] = 'X';  // "PX\0\0"
    CHECK_THROWS_AS(PeModule{bad}, FormatError);
    bad = good;
    bad[0x44 + 20] = 0x07;  // optional header magic
    CHECK_THROWS_AS(PeModule{bad}, FormatError);
    bad = good;
    bad[0x44 + 2] = 200;  // more sections than the limit
    CHECK_THROWS_AS(PeModule{bad}, FormatError);
    // truncated inside the resource section
    bad.assign(good.begin(), good.begin() + 0x210);
    CHECK_THROWS_AS(PeModule{bad}, FormatError);
    bad.assign(good.begin(), good.begin() + 0x100);  // inside the section table
    CHECK_THROWS_AS(PeModule{bad}, FormatError);
    // the root directory's entry count runs past the section
    bad = good;
    bad[0x200 + 14] = 0xFF;
    bad[0x200 + 15] = 0xFF;
    CHECK_THROWS_AS(PeModule{bad}, FormatError);
    // a type directory offset past the section
    bad = good;
    bad[0x200 + 20] = 0xFF;
    bad[0x200 + 21] = 0xFF;
    bad[0x200 + 22] = 0x0F;
    bad[0x200 + 23] = 0x80;
    CHECK_THROWS_AS(PeModule{bad}, FormatError);
}

TEST_CASE("pe: a data entry outside the file parses but its bytes throw") {
    auto img = build_pe(sample());
    PeModule ok(img);
    // the data entry of the first resource: point its RVA far away
    const PeResource& r0 = ok.resources()[0];
    // find the data entry holding r0's RVA and size, and move the RVA out of every section
    for (std::size_t k = 0x200; k + 8 <= img.size(); k += 4) {
        const std::uint32_t v = img[k] | img[k + 1] << 8 | img[k + 2] << 16 | std::uint32_t{img[k + 3]} << 24;
        const std::uint32_t s = img[k + 4] | img[k + 5] << 8 | img[k + 6] << 16 | std::uint32_t{img[k + 7]} << 24;
        if (v == r0.rva && s == r0.size) {
            img[k + 3] = 0x40;
            break;
        }
    }
    PeModule m(img);
    REQUIRE(m.resources().size() == 6);
    CHECK_THROWS_AS(m.data(m.resources()[0]), FormatError);
    CHECK(m.data(m.resources()[1])[0] == 4);
}

TEST_CASE("pe: a directory that points back at its root is not followed past three levels") {
    auto img = build_pe(sample());
    // the first type's subdirectory entry -> the root (offset 0, subdirectory bit)
    img[0x200 + 20] = 0;
    img[0x200 + 21] = 0;
    img[0x200 + 22] = 0;
    img[0x200 + 23] = 0x80;
    PeModule m(img);  // terminates; the looping branch yields no leaves of its own
    CHECK(m.all(RTPE_ICON).size() == 2);
}

// ---- golden: the fixture drive

TEST_CASE("pe: fixture programs list one RT_GROUP_ICON per icon") {
    const fs::path dir = fixture_dir();
    if (dir.empty()) {
        MESSAGE("skipped: fixture drive not available (IS_FIXTURE_DIR)");
        return;
    }
    struct Case { const char* path; std::size_t icons; };
    const Case cases[] = {{"GAMES/ALPHA.EXE", 1}, {"GAMES/BRAVO.EXE", 1}, {"GAMES/CHARLIE.EXE", 1},
                          {"TOOLS/DELTA.EXE", 1}, {"TOOLS/ECHO.EXE", 3},  {"DEEP/A/B/FOXTROT.EXE", 1}};
    for (const Case& c : cases) {
        INFO(c.path);
        PeModule m = PeModule::load(dir / c.path);
        CHECK(m.machine() == 0x14C);
        const auto groups = m.all(RTPE_GROUP_ICON);
        REQUIRE(groups.size() == c.icons);
        REQUIRE(m.all(RTPE_ICON).size() == c.icons);
        for (std::size_t k = 0; k < groups.size(); ++k) {
            CHECK(groups[k]->id == k + 1);  // make_fixtures.py: "k ICON" with k = 1, 2, ...
            const auto es = parse_group_icon(m.data(*groups[k]), m.size(*groups[k]));
            REQUIRE(es.size() == 1);
            CHECK(es[0].width == 32);
            CHECK(es[0].bpp == 4);
            const PeResource* r = m.find(RTPE_ICON, es[0].id);
            REQUIRE(r);
            const Icon ic = parse_icon(m.data(*r), m.size(*r));
            CHECK(ic.image.width == 32);
            CHECK(ic.image.height == 32);
            CHECK(ic.image.bpp == 4);
        }
    }
}
