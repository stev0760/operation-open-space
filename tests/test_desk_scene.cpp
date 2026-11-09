// src/desk/scene.*: ISPACE.DLL ordinal 32 as an op list (work/analysis/m4/s6-scene.md 1.1-1.10
// with s6-scene-verify.md's corrections). Host-only: the strip filler's PatBlts for hand-made
// quads (expected values from an independent model of s6-scene.md 1.5), the level and shade
// rules, the device texts of seg8:0x5c86 on fake answers, and the host rasteriser's
// negative-extent rule; golden (IS_ORIGINAL_DIR): scene 100's slots, scene 105's slots, texts
// and extras at 1024 x 768 against the bytes and core-0008's TextOut trace. With
// IS_SCENE_DUMP_DIR set, the golden case writes the host rasterisations of both scenes as PPM
// files for the oracle comparison of work/world/m4s6/scene-report.md.
#include <doctest.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "assets/asset_store.h"
#include "desk/scene.h"
#include "desk_scene_raster.h"
#include "formats/ne.h"
#include "formats/records.h"
#include "formats/res16.h"

using namespace is;
using namespace is::desk;
namespace fs = std::filesystem;

namespace {

fs::path original_dir() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !fs::is_directory(v)) return {};
    return v;
}
fs::path find_ci(const fs::path& dir, const std::string& name) {
    auto up = [](std::string s) {
        for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return s;
    };
    if (dir.empty() || !fs::is_directory(dir)) return {};
    for (const auto& e : fs::directory_iterator(dir))
        if (up(e.path().filename().string()) == up(name)) return e.path();
    return {};
}

struct Blt {
    int x, y, w, h;
    bool operator==(const Blt& o) const { return x == o.x && y == o.y && w == o.w && h == o.h; }
};
std::vector<Blt> blts(const std::vector<Op>& ops) {
    std::vector<Blt> v;
    for (const Op& o : ops)
        if (o.kind == Op::kPatBlt) v.push_back({o.x, o.y, o.w, o.h});
    return v;
}
std::vector<int> peaks(const std::vector<Op>& ops) {  // the red channel of each white brush
    std::vector<int> v;
    for (const Op& o : ops)
        if (o.kind == Op::kBrush) v.push_back(static_cast<int>(o.colour & 0xff));
    return v;
}
Header white_header() {
    Header h;
    h.count = 1;
    h.colour[0] = {255, 255, 255};
    return h;
}
Projection strip_projection() {
    Projection p;
    p.d2e = 154;
    p.c6c = 629;
    return p;
}

void write_ppm(const fs::path& f, const scene_raster::Image& img) {
    std::FILE* o = std::fopen(f.string().c_str(), "wb");
    if (!o) return;
    std::fprintf(o, "P6\n%d %d\n255\n", img.w, img.h);
    for (std::uint32_t c : img.px) {
        const unsigned char b[3] = {static_cast<unsigned char>(c >> 16), static_cast<unsigned char>(c >> 8),
                                    static_cast<unsigned char>(c)};
        std::fwrite(b, 1, 3, o);
    }
    std::fclose(o);
}
void write_mask(const fs::path& f, const scene_raster::Image& img) {
    std::FILE* o = std::fopen(f.string().c_str(), "wb");
    if (!o) return;
    std::fprintf(o, "P5\n%d %d\n255\n", img.w, img.h);
    for (std::uint8_t u : img.uncertain) std::fputc(u ? 255 : 0, o);
    std::fclose(o);
}

}  // namespace

// ---- the strip filler, ISPACE.DLL seg8:0x1fe6 (s6-scene.md 1.5) ----

TEST_CASE("scene: the strip filler's rows, the y moving up (a top face)") {
    std::vector<Op> ops;
    const std::array<Point, 4> P = {Point{110, 100}, Point{150, 100}, Point{170, 108}, Point{100, 108}};
    strip_fill(ops, white_header(), strip_projection(), P, 0, 0, 200, 400, 380, true, 7);
    // E = 8 < 20: strips 2 px high; the last strip (c = E) is drawn twice; the staircase PatBlts
    // after the first strip, one of them -1 px wide.
    const std::vector<Blt> want = {{100, 108, 71, 2}, {102, 106, 64, 2}, {102, 107, -1, 1}, {166, 107, 2, 1},
                                   {105, 104, 56, 2}, {105, 105, -1, 1}, {161, 105, 2, 1},  {107, 102, 49, 2},
                                   {107, 103, -1, 1}, {156, 103, 2, 1},  {110, 100, 41, 2}, {110, 101, -1, 1},
                                   {151, 101, 2, 1},  {110, 100, 41, 2}};
    CHECK(blts(ops) == want);
    CHECK(peaks(ops) == std::vector<int>{53, 51, 49, 47, 45, 45});
    for (const Op& o : ops) {
        CHECK(o.record == 7);
        if (o.kind == Op::kPatBlt) CHECK(o.rop == kPatCopy);
    }
}

TEST_CASE("scene: the strip filler's rows, the y moving down (P3 above P0)") {
    std::vector<Op> ops;
    const std::array<Point, 4> P = {Point{105, 220}, Point{160, 220}, Point{170, 213}, Point{100, 213}};
    strip_fill(ops, white_header(), strip_projection(), P, 0, 0, 150, 500, 470, true, 3);
    const std::vector<Blt> want = {{100, 213, 71, 2}, {101, 215, 67, 2}, {101, 215, 0, 1}, {168, 215, 1, 1},
                                   {102, 217, 63, 2}, {102, 217, 0, 1},  {165, 217, 1, 1}, {104, 219, 59, 2},
                                   {104, 219, -1, 1}, {163, 219, 1, 1},  {105, 220, 56, 2}, {105, 220, 0, 1},
                                   {161, 220, 1, 1}};
    CHECK(blts(ops) == want);
    CHECK(peaks(ops) == std::vector<int>{79, 75, 71, 68, 66});
}

TEST_CASE("scene: the strip filler's columns (a side face)") {
    std::vector<Op> ops;
    const std::array<Point, 4> P = {Point{200, 300}, Point{209, 304}, Point{209, 330}, Point{200, 338}};
    strip_fill(ops, white_header(), strip_projection(), P, 0, 0, 180, 450, 420, false, 5);
    const std::vector<Blt> want = {{200, 300, 2, 39}, {202, 300, 2, 37}, {202, 337, 1, 1}, {204, 301, 2, 34},
                                   {204, 301, 1, 0},  {204, 335, 1, 1},  {206, 302, 2, 31}, {206, 302, 1, 0},
                                   {206, 333, 1, 1},  {208, 303, 2, 29}, {208, 303, 1, 0},  {208, 332, 1, 0},
                                   {209, 304, 2, 27}, {209, 304, 1, 0},  {209, 331, 1, 0}};
    CHECK(blts(ops) == want);
    CHECK(peaks(ops) == std::vector<int>{69, 67, 63, 60, 58, 56});
}

// ---- levels and colours ----

TEST_CASE("scene: the level reads the brightness byte through CBW, the box negates and scales it") {
    Header h = white_header();
    h.has_raw = false;
    Record r;
    r.colour = 0;
    r.brightness = 240;  // scene 105's records 2 and 4: -16 after CBW
    CHECK(level_of(h, r) == -81);  // trunc(255 * -16 / 50) [BYTES ISPACE.DLL seg8:0x171c-0x1754]
    r.brightness = 100;
    CHECK(level_of(h, r) == 510);  // 255 * 100 = 25500, / 50
    r.brightness = 127;
    CHECK(level_of(h, r) == 647);
    // seg8:0x27f0: no lower clamp; a negative level gives the low bytes.
    CHECK(shade_colour(h, 0, 0, -3) == 0x02fdfdfdu);
    CHECK(shade_colour(h, 0, 0, 300) == 0x02ffffffu);
    Header y;
    y.count = 1;
    y.colour[0] = {255, 235, 80};
    CHECK(shade_colour(y, 0, 0, 100) == (0x02000000u | (31u << 16) | (92u << 8) | 100u));
}

TEST_CASE("scene: a hand-made header reads its typed fields, a decoded one its words") {
    Header h;
    h.count = 4;
    h.colour = {Rgb{255, 255, 255}, Rgb{0, 255, 0}, Rgb{255, 235, 80}, Rgb{0, 120, 240}};
    h.dark = {Rgb{8, 12, 12}, Rgb{0, 32, 0}, Rgb{60, 54, 19}, Rgb{0, 0, 56}};
    h.divisor = {4, 4, 8};
    // Colour 3's divisor is the word +0x2c, dark colour 0's red [BYTES ISPACE.DLL seg8:0x1537-0x1548].
    CHECK(divisor_word(h, 3) == 8);
    CHECK(peak_of(h, 3) == 2);
    CHECK(peak_of(h, 2) == 0);  // R before G on the tie of none; the first maximal channel
    const auto ramps = shade_ramps(h);
    REQUIRE(ramps.size() == 4);
    CHECK(ramps[0].size() == 61);  // (255 - 8) / 4
    CHECK(ramps[1].size() == 55);  // (255 - 32) / 4
    CHECK(ramps[2].size() == 24);  // (255 - 60) / 8
    CHECK(ramps[3].size() == 23);  // (240 - 56) / 8
    CHECK(ramps[3][0] == Rgb{0, 0, 56});
}

// ---- seg8:0x5c86, the device texts ----

TEST_CASE("scene: the device texts on Wine's answers and on a 1990s machine's") {
    DeviceSource wine;  // no keys: every default; GetWinFlags 0x6c29 [ORACLE core-0008]
    wine.windows_dir = "C:\\windows";
    wine.system_dir = "C:\\windows\\system32";
    wine.wave_devs = 1;
    wine.wave_channels = 2;
    wine.win_flags = 0x6c29;
    std::vector<std::string> asked;
    wine.profile = [&](const std::string& s, const std::string& k, const std::string& d, int n, const std::string& f) {
        asked.push_back(s + "|" + k + "|" + std::to_string(n) + "|" + f);
        return d;
    };
    const Conditions c = devices_from(wine);
    CHECK(c.video == "Video Card");
    CHECK(c.sound == "Sound Card");
    CHECK(c.net.empty());
    CHECK(c.c70 == 4);
    CHECK(c.a8 == 0);
    CHECK(c.async_wave);
    CHECK(c.stereo);
    CHECK_FALSE(c.net_found);
    CHECK(cpu_text(c.c70) == "486");
    const std::vector<std::string> want = {
        "Boot.description|display.drv|90|C:\\windows\\system.ini", "drivers|Wave|40|C:\\windows\\system.ini",
        "drivers.desc||90|C:\\windows\\control.ini", "network.setup|netcard|40|C:\\windows\\protocol.ini",
        "netcard||90|C:\\windows\\system32\\network.inf"};
    CHECK(asked == want);

    DeviceSource old = wine;
    old.win_flags = 0x0004 | 0x0400;  // WF_CPU386 | WF_80x87
    old.wave_support = 0x10;          // WAVECAPS_SYNC
    old.wave_channels = 1;
    old.profile = [](const std::string& s, const std::string& k, const std::string& d, int, const std::string&) -> std::string {
        if (s == "Boot.description") return "S3 Trio32/64 PCI (800x600 256 colors)";
        if (s == "drivers" && k == "Wave") return "sb16snd.drv";
        if (s == "drivers.desc" && k == "sb16snd.drv") return "Creative Labs Sound Blaster 16";
        if (s == "network.setup") return "ne2000,1,NE2000";
        if (s == "netcard" && k == "ne2000") return "\"Novell NE2000 Compatible Adapter\",0,ndis";
        return d;
    };
    const Conditions o = devices_from(old);
    CHECK(o.video == "S3 Trio32/64 PCI ");  // cut at '(' (a space at 16 is kept)
    CHECK(o.sound == "Sound Blaster");      // "Crea..." [BYTES ISPACE.DLL seg8:0x5d70-0x5d94]
    CHECK(o.net == "Novell NE2000 Compatible");  // '"' stripped, cut at the space past 16
    CHECK(o.net_found);
    CHECK(o.c70 == 3);
    CHECK(o.a8 == 1);
    CHECK_FALSE(o.async_wave);
    CHECK_FALSE(o.stereo);
    CHECK(cpu_text(o.c70) == "386");
    DeviceSource none = wine;
    none.wave_devs = 0;
    none.win_flags = 0;
    const Conditions n = devices_from(none);
    CHECK_FALSE(n.async_wave);
    CHECK(n.c70 == 0);
    CHECK(n.a8 == 0xff);
    CHECK(cpu_text(n.c70) == "86");
}

// ---- the host rasteriser ----

TEST_CASE("scene: the host rasteriser follows GDI's negative-extent rule") {
    using scene_raster::patblt_box;
    const auto b = patblt_box(10, 20, -3, 2);
    CHECK(b.l == 8);
    CHECK(b.r == 11);  // covers x 8, 9, 10
    CHECK(b.t == 20);
    CHECK(b.b == 22);
    const auto z = patblt_box(5, 5, 0, -1);
    CHECK(z.l == 5);
    CHECK(z.r == 5);  // nothing across
    CHECK(z.t == 5);
    CHECK(z.b == 6);  // y 5 only
    Render r;
    Op br;
    br.kind = Op::kBrush;
    br.colour = 0x02336699u;
    r.ops.push_back(br);
    Op pb;
    pb.kind = Op::kPatBlt;
    pb.x = 4, pb.y = 4, pb.w = -2, pb.h = -2;
    r.ops.push_back(pb);
    const scene_raster::Image img = scene_raster::rasterise(r, 8, 8);
    CHECK(img.at(3, 3) == 0x996633u);
    CHECK(img.at(4, 4) == 0x996633u);
    CHECK(img.at(2, 2) == 0u);
    CHECK(img.at(5, 5) == 0u);
}

// ---- golden: scenes 100 and 105 at 1024 x 768 ----

TEST_CASE("scene: IGRAPHIC.DLL #6600 100 and 105 at 1024x768 (golden)") {
    const fs::path dir = original_dir();
    const fs::path gf = find_ci(dir, "IGRAPHIC.DLL"), lf = find_ci(dir, "ENG_LANG.DLL");
    if (gf.empty() || lf.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    const fmt::NeModule graphics = fmt::NeModule::load(gf);
    const fmt::NeModule lang = fmt::NeModule::load(lf);
    Inputs in;
    in.W = 1024;
    in.H = 768;
    in.cx = 1024;
    in.cy = 768;
    in.strings = [&](int id) { return fmt::load_string(lang, static_cast<std::uint16_t>(id)).value_or(""); };
    in.devices.async_wave = true;  // the oracle's Wine: device 0 without WAVECAPS_SYNC, 2 channels
    in.devices.stereo = true;

    // Scene 100: ispace.md "Slots of scene 100".
    const Render d = render(fmt::load_scene6600(graphics, 100), in);
    REQUIRE(d.slots.size() == 5);
    CHECK(d.slots[0] == Rect{350, 22, 672, 253});
    CHECK(d.slots[1] == Rect{197, 435, 300, 684});
    CHECK(d.slots[2] == Rect{281, 268, 744, 328});
    CHECK(d.slots[3] == Rect{42, 345, 978, 360});
    CHECK(d.slots[4] == Rect{812, 267, 873, 329});

    // Scene 105, the landing corridor: s6-scene.md section 2's test vector.
    const fmt::Scene6600 raw105 = fmt::load_scene6600(graphics, 105);
    const Render c = render(raw105, in);
    CHECK(c.proj.d28 == 1024);
    CHECK(c.proj.d2c == 512);
    CHECK(c.proj.d2e == 154);
    CHECK(c.proj.d4c == 819);
    CHECK(c.proj.c6c == 629);
    CHECK(c.proj.d48 == 927);
    CHECK(c.proj.d44 == 927);
    REQUIRE(c.slots.size() >= 2);
    CHECK(c.slots[0] == Rect{-255, 618, 1281, 629});  // DGROUP:0x92de = 629
    CHECK(c.slots[1] == Rect{419, 201, 457, 238});    // DGROUP:0x92d6 = 201 - 40
    const Scene s105 = decode(raw105);
    CHECK(s105.header.count == 4);
    CHECK(level_of(s105.header, s105.records[2]) == -81);
    CHECK(level_of(s105.header, s105.records[4]) == -81);

    // core-0008's TextOut trace (x, y after the alignment move; font height x width, weight;
    // the PALETTERGB colour) [ORACLE core-0008].
    struct Want {
        std::string text;
        int x, y, h, w, weight;
        std::uint32_t rgb;
    };
    const std::vector<Want> want = {
        {"HIGH", 103, 221, 21, 14, 700, 0x980000},      {"VOLTAGE", 103, 263, 21, 14, 700, 0x980000},
        {"That's", 415, 302, 2, 5, 0, 0x7f7f7f},        {"A Good", 476, 302, 2, 5, 0, 0x7f7f7f},
        {"Video ", 547, 302, 2, 5, 0, 0x7f7f7f},        {"Card    ", 608, 302, 2, 5, 0, 0x7f7f7f},
        {"Green Card", 639, 147, 6, 4, 0, 0x6e6335},    {"56777", 662, 146, 7, 5, 0, 0x7d703a},
        {"", 693, 145, 9, 6, 0, 0x918141},              {"486", 512, 393, 20, 41, 0, 0xc6c6c6},
        {"Sound Card", 739, 138, 10, 7, 0, 0xb09c4b},   {"Video Card", 816, 131, 14, 9, 0, 0xe3c85c},
    };
    std::vector<const Op*> texts;
    for (const Op& o : c.ops)
        if (o.kind == Op::kText) texts.push_back(&o);
    REQUIRE(texts.size() == want.size());
    for (std::size_t i = 0; i < want.size(); ++i) {
        const Op& o = *texts[i];
        INFO("text ", i, " '", want[i].text, "' record ", o.record);
        CHECK(o.text == want[i].text);
        CHECK(o.x == want[i].x);
        CHECK(o.y == want[i].y);
        CHECK(o.font_h == want[i].h);
        CHECK(o.font_w == want[i].w);
        CHECK(o.weight == want[i].weight);
        CHECK(scene_raster::rgb_of(o.colour) == want[i].rgb);
    }
    // The extras of s6-scene.md 1.4, 1.7: the two pattern fills of records 2 and 4 (graphic 76)
    // and record 9's METAL, the HIGH VOLTAGE plate's two ordinal-34 frames, the crosses of the
    // four capacitors (records 31, 32, 43, 59), and no "80x87" (record 40's condition).
    std::map<int, int> pattern_of;
    std::set<int> frames, framed_records;
    for (const Op& o : c.ops) {
        if (o.kind == Op::kPattern) pattern_of[o.record] = static_cast<int>(o.pattern);
        if (o.kind == Op::kFrame) frames.insert(o.mode), framed_records.insert(o.record);
        CHECK(o.record != 40);
        CHECK(o.record != 39);
    }
    CHECK(pattern_of == std::map<int, int>{{2, int(Pattern::kGraphic76)}, {4, int(Pattern::kGraphic76)},
                                           {9, int(Pattern::kMetal)}});
    CHECK(frames == std::set<int>{2, 3});
    CHECK(framed_records == std::set<int>{9});
    for (int rec : {31, 32, 43, 59}) {
        int fills = 0;
        for (const Op& o : c.ops)
            if (o.record == rec && o.kind == Op::kBrush && (o.colour & 0xff) == ((o.colour >> 8) & 0xff)) ++fills;
        INFO("capacitor record ", rec);
        CHECK(fills == 1);  // the cross's one brush in header colour 0 (white, so grey)
    }

    if (const char* out = std::getenv("IS_SCENE_DUMP_DIR"); out && *out && fs::is_directory(out)) {
        assets::AssetStore store(dir);
        // METAL as LoadBitmap makes it (its own colours); graphic 76, a 4-bpp DIB, in the fixed
        // 16-colour VGA set that the oracle shows for it (its indices 7 and 8 as #C0C0C0 and
        // #808080) [ORACLE core-0008] (ispace.md ordinal 1, DGROUP:0x1ea8).
        static const std::uint32_t vga16[16] = {0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080,
                                                0x008080, 0xc0c0c0, 0x808080, 0xff0000, 0x00ff00, 0xffff00,
                                                0x0000ff, 0xff00ff, 0x00ffff, 0xffffff};
        auto bits = [&](Pattern p) {
            scene_raster::PatternBits b;
            fmt::Dib dib;
            if (p == Pattern::kMetal) dib = store.bitmap("METAL");
            else if (p == Pattern::kGraphic76) dib = store.sprite(76);
            else return b;
            b.w = dib.width, b.h = dib.height;
            for (int y = 0; y < b.h; ++y)
                for (int x = 0; x < b.w; ++x)
                    b.px.push_back(p == Pattern::kGraphic76 && dib.bpp == 4 ? vga16[dib.index_at(x, y) & 15] : dib.rgb_at(x, y));
            return b;
        };
        // newgame-0002's desk ran without a wave device: no speakers (records 36-39).
        Inputs quiet = in;
        quiet.devices.async_wave = quiet.devices.stereo = false;
        const auto i100 = scene_raster::rasterise(render(fmt::load_scene6600(graphics, 100), quiet), 1024, 768, bits);
        const auto i105 = scene_raster::rasterise(c, 1024, 768, bits);
        write_ppm(fs::path(out) / "host-100.ppm", i100);
        write_mask(fs::path(out) / "host-100-unsure.pgm", i100);
        write_ppm(fs::path(out) / "host-105.ppm", i105);
        write_mask(fs::path(out) / "host-105-unsure.pgm", i105);
        MESSAGE("wrote the host rasterisations to ", std::string(out));
    }
}
