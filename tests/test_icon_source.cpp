// The port's icon reader (src/formats/icon_source, ADR 0012 decision 5): counts and 32x32
// cells over black from synthetic NE and PE modules, the member selection, the 8 bpp index
// image and the blank-cell test; then the fixture drive's programs against the icons
// oracle/fixtures/make_fixtures.py draws, and INSPACE.EXE's GAME group through the NE path.
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <string>

#include "doctest.h"
#include "fixture_dir.h"
#include "formats/icon.h"
#include "formats/icon_source.h"
#include "formats/ne.h"
#include "gfx/palette.h"
#include "ne_builder.h"
#include "pe_builder.h"

using namespace is::fmt;
namespace fs = std::filesystem;

namespace {
void a16(std::vector<std::uint8_t>& b, unsigned v) {
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
}
void a32(std::vector<std::uint8_t>& b, std::uint32_t v) {
    a16(b, v & 0xFFFF);
    a16(b, v >> 16);
}

// A paletted icon image (1, 4 or 8 bpp): the palette, an index per pixel (top-down) and the
// AND bit per pixel.
std::vector<std::uint8_t> make_icon(int w, int h, int bpp, const std::vector<std::uint32_t>& pal,
                                    const std::function<int(int, int)>& index,
                                    const std::function<bool(int, int)>& mask = {}) {
    std::vector<std::uint8_t> b;
    a32(b, 40); a32(b, static_cast<std::uint32_t>(w)); a32(b, static_cast<std::uint32_t>(2 * h));
    a16(b, 1); a16(b, static_cast<unsigned>(bpp)); a32(b, 0);
    a32(b, 0); a32(b, 0); a32(b, 0); a32(b, static_cast<std::uint32_t>(pal.size())); a32(b, 0);
    for (std::uint32_t c : pal) a32(b, c);
    const std::size_t stride = ((static_cast<std::size_t>(w) * bpp + 31) / 32) * 4;
    for (int y = h - 1; y >= 0; --y) {
        std::vector<std::uint8_t> row(stride, 0);
        for (int x = 0; x < w; ++x) {
            const int v = index(x, y);
            const int bit = x * bpp;
            row[bit / 8] |= static_cast<std::uint8_t>(v << (8 - bpp - bit % 8));
        }
        b.insert(b.end(), row.begin(), row.end());
    }
    const std::size_t mstride = ((static_cast<std::size_t>(w) + 31) / 32) * 4;
    for (int y = h - 1; y >= 0; --y) {
        std::vector<std::uint8_t> row(mstride, 0);
        for (int x = 0; x < w; ++x)
            if (mask && mask(x, y)) row[x / 8] |= static_cast<std::uint8_t>(0x80 >> (x % 8));
        b.insert(b.end(), row.begin(), row.end());
    }
    return b;
}

std::vector<std::uint8_t> make_icon32(int w, int h, const std::function<std::uint32_t(int, int)>& argb) {
    std::vector<std::uint8_t> b;
    a32(b, 40); a32(b, static_cast<std::uint32_t>(w)); a32(b, static_cast<std::uint32_t>(2 * h));
    a16(b, 1); a16(b, 32); a32(b, 0);
    a32(b, 0); a32(b, 0); a32(b, 0); a32(b, 0); a32(b, 0);
    for (int y = h - 1; y >= 0; --y)
        for (int x = 0; x < w; ++x) a32(b, argb(x, y));
    b.insert(b.end(), static_cast<std::size_t>((w + 31) / 32 * 4 * h), 0);
    return b;
}

struct Member { std::uint8_t w, h, colors; std::uint16_t bpp; std::uint16_t id; std::size_t bytes; };
std::vector<std::uint8_t> make_group(const std::vector<Member>& ms) {
    std::vector<std::uint8_t> b;
    a16(b, 0); a16(b, 1); a16(b, static_cast<unsigned>(ms.size()));
    for (const Member& m : ms) {
        b.push_back(m.w); b.push_back(m.h); b.push_back(m.colors); b.push_back(0);
        a16(b, 1); a16(b, m.bpp); a32(b, static_cast<std::uint32_t>(m.bytes)); a16(b, m.id);
    }
    return b;
}

// make_fixtures.py's VGA16 and icon_pixels(), as the expected cells over black.
constexpr std::array<std::uint32_t, 16> kVga16 = {
    0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080, 0x008080, 0xC0C0C0,
    0x808080, 0xFF0000, 0x00FF00, 0xFFFF00, 0x0000FF, 0xFF00FF, 0x00FFFF, 0xFFFFFF};
struct Style { int bg, fg; const char* shape; };
constexpr Style kStyles[] = {{9, 15, "square"},   {10, 0, "diamond"}, {12, 15, "cross"},
                             {11, 0, "circle"},   {13, 15, "triangle"}, {14, 0, "xmark"},
                             {3, 15, "ring"},     {5, 15, "bars"},    {2, 15, "square"},
                             {4, 15, "diamond"}};

std::array<std::array<int, 32>, 32> fixture_pixels(int n) {
    const Style& s = kStyles[n % 10];
    const int back = n == 6 ? -1 : s.bg;  // TRANSPARENT_BG = {6}
    const std::string shape = s.shape;
    std::array<std::array<int, 32>, 32> px;
    for (auto& row : px) row.fill(back);
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x) {
            const double dx = x - 15.5, dy = y - 13.5;
            bool on = false;
            if (shape == "square") on = std::fabs(dx) <= 8 && std::fabs(dy) <= 8;
            else if (shape == "diamond") on = std::fabs(dx) + std::fabs(dy) <= 11;
            else if (shape == "cross") on = std::fabs(dx) <= 3 || std::fabs(dy) <= 3;
            else if (shape == "circle") on = dx * dx + dy * dy <= 100;
            else if (shape == "triangle") on = 4 <= y && y <= 24 && std::fabs(dx) <= (y - 4) * 0.6;
            else if (shape == "xmark") on = std::fabs(std::fabs(dx) - std::fabs(dy)) <= 2.5 && std::fabs(dx) <= 11;
            else if (shape == "ring") on = 49 <= dx * dx + dy * dy && dx * dx + dy * dy <= 121;
            else if (shape == "bars") on = x % 8 < 4 && 3 <= y && y <= 24;
            if (on && y <= 26) px[y][x] = s.fg;
        }
    for (int y = 28; y < 32; ++y)
        for (int x = 0; x < 32; ++x) px[y][x] = 0;
    for (int k = 0; k < n + 1; ++k) {
        const int x0 = 1 + 3 * k;
        for (int y : {29, 30})
            for (int x : {x0, x0 + 1})
                if (x < 32) px[y][x] = 15;
    }
    if (back != -1)
        for (int i = 0; i < 32; ++i) px[0][i] = px[31][i] = px[i][0] = px[i][31] = 0;
    return px;
}

// DrawIcon over black: a transparent pixel (AND 1, XOR 0) stays black.
IconCell fixture_cell(int n) {
    const auto px = fixture_pixels(n);
    IconCell c;
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x)
            c.bgra[static_cast<std::size_t>(y) * 32 + x] = 0xFF000000u | kVga16[std::max(px[y][x], 0)];
    return c;
}

int differing(const IconCell& a, const IconCell& b) {
    int n = 0;
    for (std::size_t i = 0; i < a.bgra.size(); ++i) n += a.bgra[i] != b.bgra[i];
    return n;
}

const std::vector<std::uint32_t> kPal4 = {0x000000, 0xFF0000, 0x00FF00, 0x0000FF};
}  // namespace

TEST_CASE("icon source: files that are not NE or PE modules count 0") {
    CHECK(IconSource(std::vector<std::uint8_t>{}).count() == 0);
    const std::string txt = "This is a text file, not a program.\r\n";
    IconSource t(std::vector<std::uint8_t>(txt.begin(), txt.end()));
    CHECK(t.count() == 0);
    CHECK(t.kind() == ModuleKind::kNone);
    CHECK_FALSE(t.cell(0));
    std::vector<std::uint8_t> dos(0x80, 0);  // a DOS program: MZ without NE or PE
    dos[0] = 'M'; dos[1] = 'Z'; dos[0x3C] = 0x40;
    CHECK(IconSource(dos).count() == 0);
    // an NE header whose resource table runs past the end
    std::vector<std::uint8_t> ne(0x80, 0);
    ne[0] = 'M'; ne[1] = 'Z'; ne[0x3C] = 0x40; ne[0x40] = 'N'; ne[0x41] = 'E';
    ne[0x64] = 0xF0; ne[0x66] = 0xF8;
    CHECK(IconSource(ne).count() == 0);
    CHECK(IconSource::load(fs::path("no such dir") / "NOFILE.EXE").count() == 0);
}

TEST_CASE("icon source: an NE module counts its groups in table order and draws them over black") {
    const auto red = make_icon(32, 32, 4, kPal4, [](int x, int) { return x < 16 ? 1 : 2; },
                               [](int x, int y) { return x == 0 && y == 0; });
    const auto blue = make_icon(32, 32, 4, kPal4, [](int, int) { return 3; });
    const auto mod = build_ne("TEST", {{RT16_ICON, 1, "", red},
                                       {RT16_ICON, 2, "", blue},
                                       {RT16_GROUP_ICON, 0, "SECOND", make_group({{32, 32, 16, 4, 2, blue.size()}})},
                                       {RT16_GROUP_ICON, 7, "", make_group({{32, 32, 16, 4, 1, red.size()}})},
                                       {RT16_GROUP_ICON, 9, "", make_group({{32, 32, 16, 4, 99, 0}})}});
    IconSource s(mod);
    CHECK(s.kind() == ModuleKind::kNe);
    CHECK(s.count() == 3);  // a group whose member is missing still counts
    auto c0 = s.cell(0);
    REQUIRE(c0);
    CHECK(c0->at(5, 5) == 0xFF0000FF);
    auto c1 = s.cell(1);
    REQUIRE(c1);
    // the masked pixel at (0, 0) has XOR colour red: over black DrawIcon leaves red
    CHECK(c1->at(0, 0) == 0xFFFF0000);
    CHECK(c1->at(15, 31) == 0xFFFF0000);
    CHECK(c1->at(16, 0) == 0xFF00FF00);
    CHECK_FALSE(s.cell(2));
    CHECK_FALSE(s.cell(3));
    CHECK_FALSE(s.cell(-1));
}

TEST_CASE("icon source: a masked black pixel is black, so a fully transparent icon is a uniform cell") {
    const auto clear = make_icon(32, 32, 4, kPal4, [](int, int) { return 0; }, [](int, int) { return true; });
    IconSource s(build_ne("T", {{RT16_ICON, 1, "", clear},
                                {RT16_GROUP_ICON, 1, "", make_group({{32, 32, 16, 4, 1, clear.size()}})}}));
    REQUIRE(s.count() == 1);
    auto c = s.cell(0);
    REQUIRE(c);
    CHECK(is_uniform(*c));
    CHECK_FALSE(is_uniform(*c, 0xFFFFFFFFu));
    IconCell white;
    white.bgra.fill(0xFFFFFFFFu);
    CHECK_FALSE(is_uniform(white));  // compared with the black strip pixel, not with itself
    CHECK(is_uniform(white, 0xFFFFFFFFu));
    std::vector<std::uint8_t> idx(1024, 0);
    CHECK(is_uniform(idx));
    idx[1023] = 7;
    CHECK_FALSE(is_uniform(idx));
}

TEST_CASE("icon source: the member nearest 32x32 at the best depth for the display") {
    const auto i16 = make_icon(16, 16, 4, kPal4, [](int x, int y) { return (x + y) % 4; });
    const auto i32x1 = make_icon(32, 32, 1, {0x000000, 0xFFFFFF}, [](int, int) { return 1; });
    const auto i32x4 = make_icon(32, 32, 4, kPal4, [](int, int) { return 2; });
    std::vector<std::uint32_t> pal8(256, 0);
    pal8[200] = 0x123456;
    const auto i32x8 = make_icon(32, 32, 8, pal8, [](int, int) { return 200; });
    const auto i32x32 = make_icon32(32, 32, [](int, int) { return 0xFF00FFFFu; });
    const auto mod = build_ne("T", {{RT16_ICON, 1, "", i16},
                                    {RT16_ICON, 2, "", i32x1},
                                    {RT16_ICON, 3, "", i32x4},
                                    {RT16_ICON, 4, "", i32x8},
                                    {RT16_ICON, 5, "", i32x32},
                                    {RT16_GROUP_ICON, 1, "", make_group({{16, 16, 16, 4, 1, i16.size()},
                                                                        {32, 32, 2, 1, 2, i32x1.size()},
                                                                        {32, 32, 16, 4, 3, i32x4.size()},
                                                                        {32, 32, 0, 32, 5, i32x32.size()},
                                                                        {32, 32, 0, 8, 4, i32x8.size()}})}});
    IconSource s(mod);
    REQUIRE(s.count() == 1);
    CHECK(s.cell(0)->at(3, 3) == 0xFF123456);            // 8 bpp display: the 8 bpp member
    CHECK(s.cell(0, 4)->at(3, 3) == 0xFF00FF00);         // 4 bpp: the 4 bpp member
    CHECK(s.cell(0, 1)->at(3, 3) == 0xFFFFFFFF);         // 1 bpp
    CHECK(s.cell(0, 32)->at(3, 3) == 0xFF00FFFF);        // true colour: the 32 bpp member
}

TEST_CASE("icon source: another size is scaled to 32x32 by nearest neighbour") {
    const auto i16 = make_icon(16, 16, 4, kPal4, [](int x, int y) { return (x + 2 * y) % 4; });
    const auto i64 = make_icon(64, 64, 4, kPal4, [](int x, int) { return x % 2 ? 3 : 1; });
    SUBCASE("16x16 doubles every pixel") {
        IconSource s(build_ne("T", {{RT16_ICON, 1, "", i16},
                                    {RT16_GROUP_ICON, 1, "", make_group({{16, 16, 16, 4, 1, i16.size()}})}}));
        auto c = s.cell(0);
        REQUIRE(c);
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 32; ++x) {
                INFO(x << "," << y);
                CHECK(c->at(x, y) == (0xFF000000u | kPal4[static_cast<std::size_t>((x / 2 + 2 * (y / 2)) % 4)]));
            }
    }
    SUBCASE("at equal distance the larger size wins, and 64x64 keeps the even columns") {
        const auto i48 = make_icon(48, 48, 4, kPal4, [](int, int) { return 2; });
        IconSource s(build_ne("T", {{RT16_ICON, 1, "", i16},
                                    {RT16_ICON, 2, "", i48},
                                    {RT16_ICON, 3, "", i64},
                                    {RT16_GROUP_ICON, 1, "", make_group({{16, 16, 16, 4, 1, i16.size()},
                                                                        {48, 48, 16, 4, 2, i48.size()}})},
                                    {RT16_GROUP_ICON, 2, "", make_group({{64, 64, 16, 4, 3, i64.size()}})}}));
        CHECK(s.cell(0)->at(0, 0) == 0xFF00FF00);
        auto c = s.cell(1);
        REQUIRE(c);
        CHECK(std::all_of(c->bgra.begin(), c->bgra.end(), [](std::uint32_t v) { return v == 0xFFFF0000u; }));
    }
}

TEST_CASE("icon source: 32 bpp members blend by alpha; PNG and broken members are skipped") {
    const auto half = make_icon32(32, 32, [](int x, int) { return x < 16 ? 0x80FF8040u : 0x00FFFFFFu; });
    const auto noalpha = make_icon32(32, 32, [](int, int) { return 0x00204060u; });
    std::vector<std::uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 0};
    const auto blue = make_icon(32, 32, 4, kPal4, [](int, int) { return 3; });
    auto broken = blue;
    broken.resize(100);
    const auto mod = build_ne("T", {{RT16_ICON, 1, "", half},
                                    {RT16_ICON, 2, "", noalpha},
                                    {RT16_ICON, 3, "", png},
                                    {RT16_ICON, 4, "", blue},
                                    {RT16_ICON, 5, "", broken},
                                    {RT16_GROUP_ICON, 1, "", make_group({{32, 32, 0, 32, 1, half.size()}})},
                                    {RT16_GROUP_ICON, 2, "", make_group({{32, 32, 0, 32, 2, noalpha.size()}})},
                                    {RT16_GROUP_ICON, 3, "", make_group({{0, 0, 0, 32, 3, png.size()},
                                                                        {32, 32, 0, 4, 5, broken.size()},
                                                                        {16, 16, 16, 4, 4, blue.size()}})},
                                    {RT16_GROUP_ICON, 4, "", make_group({{0, 0, 0, 32, 3, png.size()}})}});
    IconSource s(mod);
    REQUIRE(s.count() == 4);
    auto a = s.cell(0);
    REQUIRE(a);
    CHECK(a->at(0, 0) == 0xFF804020u);  // 0xFF, 0x80, 0x40 times 128/255, rounded
    CHECK(a->at(31, 0) == 0xFF000000u);
    CHECK(s.cell(1)->at(9, 9) == 0xFF204060u);  // no alpha anywhere: the colour as the mask leaves it
    auto b = s.cell(2);
    REQUIRE(b);
    CHECK(b->at(0, 0) == 0xFF0000FFu);  // the 4 bpp member (32x32 at 4 bpp, the one listed as 16x16)
    CHECK_FALSE(s.cell(3));
}

TEST_CASE("icon source: a PE module counts its group names in directory order") {
    const auto red = make_icon(32, 32, 4, kPal4, [](int, int) { return 1; });
    const auto green = make_icon(32, 32, 4, kPal4, [](int, int) { return 2; });
    const auto img = build_pe({{RTPE_GROUP_ICON, 0, u"MAIN", 0x409, make_group({{32, 32, 16, 4, 2, green.size()}})},
                               {RTPE_GROUP_ICON, 3, u"", 0x409, make_group({{32, 32, 16, 4, 1, red.size()}})},
                               {RTPE_GROUP_ICON, 3, u"", 0x407, make_group({{32, 32, 16, 4, 2, green.size()}})},
                               {RTPE_ICON, 1, u"", 0x409, red},
                               {RTPE_ICON, 2, u"", 0x409, green}});
    IconSource s(img);
    CHECK(s.kind() == ModuleKind::kPe);
    REQUIRE(s.count() == 2);  // two names; the second language of id 3 is not another icon
    CHECK(s.cell(0)->at(0, 0) == 0xFF00FF00u);
    CHECK(s.cell(1)->at(0, 0) == 0xFFFF0000u);

    auto cut = img;
    cut.resize(0x210);
    CHECK(IconSource(cut).count() == 0);
}

TEST_CASE("icon source: the 8 bpp index image through the palette's nearest entry") {
    namespace gfx = is::gfx;
    const gfx::DeviceColours dev = gfx::default_device_colours();
    auto nearest = [&](std::uint32_t rgb) { return gfx::nearest_index(dev, gfx::Rgb::of(rgb)); };
    IconCell c;
    for (std::size_t i = 0; i < c.bgra.size(); ++i) c.bgra[i] = 0xFF000000u | kVga16[i % 16];
    const auto idx = cell_to_index8(c, nearest);
    REQUIRE(idx.size() == 1024);
    // the VGA colours are the static colours: 0-7 at 0-7, grey 0x808080 at 248, 9-15 at 249-255
    const int expect[16] = {0, 1, 2, 3, 4, 5, 6, 7, 248, 249, 250, 251, 252, 253, 254, 255};
    for (int i = 0; i < 16; ++i) {
        INFO("VGA " << i);
        CHECK(idx[static_cast<std::size_t>(i)] == expect[i]);
    }
    CHECK(idx[16 + 9] == 249);
}

// ---- golden

TEST_CASE("icon source: the fixture drive's programs, counted and drawn") {
    const fs::path dir = fixture_dir();
    if (dir.empty()) {
        MESSAGE("skipped: fixture drive not available (IS_FIXTURE_DIR)");
        return;
    }
    struct Case { const char* path; std::vector<int> icons; };
    const Case cases[] = {{"GAMES/ALPHA.EXE", {0}}, {"GAMES/BRAVO.EXE", {1}}, {"GAMES/CHARLIE.EXE", {2}},
                          {"TOOLS/DELTA.EXE", {3}}, {"TOOLS/ECHO.EXE", {4, 5, 6}},
                          {"DEEP/A/B/FOXTROT.EXE", {7}}};
    const is::gfx::DeviceColours dev = is::gfx::default_device_colours();
    for (const Case& k : cases) {
        INFO(k.path);
        const IconSource s = IconSource::load(dir / k.path);
        CHECK(s.kind() == ModuleKind::kPe);
        REQUIRE(s.count() == static_cast<int>(k.icons.size()));
        for (std::size_t i = 0; i < k.icons.size(); ++i) {
            INFO("index " << i << " icon " << k.icons[i]);
            const auto c = s.cell(static_cast<int>(i));
            REQUIRE(c);
            CHECK(differing(*c, fixture_cell(k.icons[i])) == 0);
            CHECK_FALSE(is_uniform(*c));
            const auto idx = cell_to_index8(*c, [&](std::uint32_t rgb) {
                return is::gfx::nearest_index(dev, is::gfx::Rgb::of(rgb));
            });
            // every fixture colour is a static colour, so the index image maps back exactly
            for (std::size_t p = 0; p < idx.size(); ++p)
                if (dev[idx[p]].rgb() != (c->bgra[p] & 0xFFFFFF)) {
                    FAIL_CHECK("pixel " << p << " maps to index " << int(idx[p]));
                    break;
                }
        }
        CHECK_FALSE(s.cell(static_cast<int>(k.icons.size())));
    }
    // icon 6 has a transparent background: the shape over black
    const auto echo = IconSource::load(dir / "TOOLS/ECHO.EXE").cell(2);
    REQUIRE(echo);
    CHECK(echo->at(0, 0) == 0xFF000000u);
    CHECK(echo->at(15 + 9, 13) == 0xFFFFFFFFu);  // on the ring: dx 8.5, dy -0.5

    // a truncated program counts 0, as does a text file
    auto bytes = read_file(dir / "GAMES/ALPHA.EXE");
    bytes.resize(bytes.size() / 2);
    CHECK(IconSource(bytes).count() == 0);
    const std::string txt = "README\r\n";
    CHECK(IconSource(std::vector<std::uint8_t>(txt.begin(), txt.end())).count() == 0);
}

namespace {
fs::path find_upper(const fs::path& dir, const std::string& name) {
    std::error_code ec;
    if (dir.empty() || !fs::is_directory(dir, ec)) return {};
    for (const auto& e : fs::directory_iterator(dir)) {
        std::string f = e.path().filename().string();
        for (char& ch : f) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        if (f == name) return e.path();
    }
    return {};
}
}  // namespace

TEST_CASE("icon source: INSPACE.EXE's GAME group through the NE path") {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    const fs::path exe = v && *v ? find_upper(fs::path(v), "INSPACE.EXE") : fs::path();
    if (exe.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    const IconSource s = IconSource::load(exe);
    CHECK(s.kind() == ModuleKind::kNe);
    REQUIRE(s.count() == 1);
    const auto c = s.cell(0);
    REQUIRE(c);
    // the 32x32 member (ICON 1), each pixel its XOR colour
    const NeModule m = NeModule::load(exe);
    const NeResource* r = m.find(RT16_ICON, 1);
    REQUIRE(r);
    const Icon ic = parse_icon(m.data(*r), m.size(*r));
    REQUIRE(ic.image.width == 32);
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x)
            if (c->at(x, y) != (0xFF000000u | ic.image.rgb_at(x, y))) {
                FAIL_CHECK("pixel " << x << "," << y);
                return;
            }
    CHECK_FALSE(is_uniform(*c));
    CHECK_FALSE(s.cell(1));  // one group
}

// ---- M3 stage 1 fixer: the bounded reader (ADR 0012 decision 5)

TEST_CASE("icon source: read() takes the headers and the resource section only") {
    const auto red = make_icon(32, 32, 4, kPal4, [](int, int) { return 1; });
    const auto img = build_pe({{RTPE_GROUP_ICON, 1, u"", 0x409, make_group({{32, 32, 16, 4, 1, red.size()}})},
                               {RTPE_ICON, 1, u"", 0x409, red}});
    std::uint64_t asked = 0, largest = 0;
    // the image at the start of a 4 GB file of zeros
    const std::uint64_t size = 4ull << 30;
    const auto at = [&](std::uint64_t off, std::size_t n, std::uint8_t* out) {
        asked += n;
        largest = std::max<std::uint64_t>(largest, off + n);
        for (std::size_t i = 0; i < n; ++i) out[i] = off + i < img.size() ? img[off + i] : 0;
        return off + n <= size;
    };
    const IconSource s = IconSource::read(size, at);
    CHECK(s.kind() == ModuleKind::kPe);
    REQUIRE(s.count() == 1);
    CHECK(s.cell(0)->at(0, 0) == 0xFFFF0000u);
    CHECK(asked <= img.size() + 0x100);
    CHECK(largest <= img.size());
    // the same answers as the whole image
    const IconSource whole(img);
    CHECK(differing(*whole.cell(0), *s.cell(0)) == 0);

    // a file that is no module is answered from its first 64 bytes
    asked = 0;
    std::vector<std::uint8_t> junk(0x40, 'x');
    CHECK(IconSource::read(size, [&](std::uint64_t off, std::size_t n, std::uint8_t* out) {
              asked += n;
              for (std::size_t i = 0; i < n; ++i) out[i] = off + i < junk.size() ? junk[off + i] : 0;
              return true;
          }).count() == 0);
    CHECK(asked == 0x40);
}

TEST_CASE("icon source: an NE module above the cap counts 0 unread") {
    std::vector<std::uint8_t> mz(0x80, 0);
    mz[0] = 'M'; mz[1] = 'Z'; mz[0x3C] = 0x40; mz[0x40] = 'N'; mz[0x41] = 'E';
    std::uint64_t asked = 0;
    const IconSource s = IconSource::read(IconSource::kMaxNeImage + 1,
                                          [&](std::uint64_t off, std::size_t n, std::uint8_t* out) {
                                              asked += n;
                                              for (std::size_t i = 0; i < n; ++i)
                                                  out[i] = off + i < mz.size() ? mz[off + i] : 0;
                                              return true;
                                          });
    CHECK(s.count() == 0);
    CHECK(asked < 0x100);
}

TEST_CASE("icon source: resource names amplified past the cap are refused") {
    using namespace pe_builder;
    auto img = build_pe({{RTPE_ICON, 1, u"", 0x409, {0, 0, 0, 0}}});
    img.resize(0x200);
    // one type, 2000 named entries that all name one 65535-unit string
    const std::size_t kN = 2000;
    std::vector<std::uint8_t> d(24 + 16 + 8 * kN, 0);
    w16(d, 14, 1);
    w32(d, 16, RTPE_GROUP_ICON);
    w32(d, 20, 0x80000000u | 24);
    w16(d, 24 + 12, static_cast<std::uint32_t>(kN));
    const std::size_t name_at = d.size();
    for (std::size_t i = 0; i < kN; ++i) {
        w32(d, 24 + 16 + 8 * i, 0x80000000u | static_cast<std::uint32_t>(name_at));
        w32(d, 24 + 16 + 8 * i + 4, 0x80000000u);  // back at the root
    }
    w16(d, name_at, 0xFFFF);
    d.resize(name_at + 2 + 2 * 0xFFFF, 'A');
    const std::size_t sec = 0x44 + 20 + 0xE0;
    w32(img, sec + 8, static_cast<std::uint32_t>(d.size()));
    w32(img, sec + 16, static_cast<std::uint32_t>(d.size()));
    img.insert(img.end(), d.begin(), d.end());
    CHECK_THROWS_AS(PeModule{img}, FormatError);
    CHECK(IconSource(img).count() == 0);
}

TEST_CASE("icon source: members above 256 a side are skipped; V4 headers carry their masks") {
    const auto big = make_icon(300, 300, 1, {0x000000, 0xFFFFFF}, [](int, int) { return 1; });
    const auto red = make_icon(32, 32, 4, kPal4, [](int, int) { return 1; });
    {
        const auto img = build_pe({{RTPE_GROUP_ICON, 1, u"", 0x409, make_group({{0, 0, 2, 1, 1, big.size()}})},
                                   {RTPE_ICON, 1, u"", 0x409, big}});
        IconSource s(img);
        CHECK(s.count() == 1);
        CHECK_FALSE(s.cell(0).has_value());
    }
    {
        const auto img = build_pe({{RTPE_GROUP_ICON, 1, u"", 0x409,
                                    make_group({{0, 0, 2, 1, 1, big.size()}, {32, 32, 16, 4, 2, red.size()}})},
                                   {RTPE_ICON, 1, u"", 0x409, big},
                                   {RTPE_ICON, 2, u"", 0x409, red}});
        CHECK(IconSource(img).cell(0)->at(0, 0) == 0xFFFF0000u);
    }
    // a 32 bpp BI_BITFIELDS member with a 108-byte BITMAPV4HEADER: the bits follow the header
    std::vector<std::uint8_t> v4;
    a32(v4, 108); a32(v4, 32); a32(v4, 64); a16(v4, 1); a16(v4, 32); a32(v4, 3);
    a32(v4, 0); a32(v4, 0); a32(v4, 0); a32(v4, 0); a32(v4, 0);
    a32(v4, 0x00FF0000); a32(v4, 0x0000FF00); a32(v4, 0x000000FF); a32(v4, 0xFF000000);
    v4.resize(108, 0);
    for (int i = 0; i < 32 * 32; ++i) a32(v4, 0xFF00FF00u);  // opaque green
    v4.insert(v4.end(), 4 * 32, 0);
    const auto img = build_pe({{RTPE_GROUP_ICON, 1, u"", 0x409, make_group({{32, 32, 0, 32, 1, v4.size()}})},
                               {RTPE_ICON, 1, u"", 0x409, v4}});
    const auto c = IconSource(img).cell(0);
    REQUIRE(c);
    CHECK(c->at(0, 0) == 0xFF00FF00u);
    CHECK(c->at(31, 31) == 0xFF00FF00u);
}
