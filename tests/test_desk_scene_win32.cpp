// src/desk/scene_gdi.* under Win32 (Wine in CTest): draw_scene replays ordinal 32's op list on a
// 32-bpp DIB section with the colours as the bytes build them, and the pixels match the host
// rasteriser of tests/desk_scene_raster.h wherever it is certain (every pixel but the pen lines
// and texts); a hand-made list with negative extents, a pattern brush and an ordinal-34 frame;
// golden (IS_ORIGINAL_DIR): scenes 100 and 105 at 1024 x 768, and the port's form on the 8-bit
// surface (palette command 6, METAL and graphic 76 from the gfx loaders). With
// IS_SCENE_DUMP_DIR set, the golden case writes the Wine renders as PPM files for the oracle
// comparison of work/world/m4s6/scene-report.md.
#ifdef _WIN32
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "assets/asset_store.h"
#include "desk/scene.h"
#include "desk/scene_gdi.h"
#include "desk_scene_raster.h"
#include "doctest.h"
#include "formats/ne.h"
#include "formats/records.h"
#include "formats/res16.h"
#include "gfx/gfx.h"

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

// A top-down 32-bpp DIB section with its own DC, black.
class Dib32 {
public:
    Dib32(int w, int h) : w_(w), h_(h) {
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = w;
        bi.bmiHeader.biHeight = -h;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* p = nullptr;
        bmp_ = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &p, nullptr, 0);
        bits_ = static_cast<std::uint32_t*>(p);
        dc_ = CreateCompatibleDC(nullptr);
        old_ = SelectObject(dc_, bmp_);
        PatBlt(dc_, 0, 0, w, h, BLACKNESS);
        GdiFlush();
    }
    ~Dib32() {
        SelectObject(dc_, old_);
        DeleteDC(dc_);
        DeleteObject(bmp_);
    }
    Dib32(const Dib32&) = delete;
    Dib32& operator=(const Dib32&) = delete;
    HDC dc() const { return dc_; }
    HBITMAP bitmap() const { return bmp_; }
    std::uint32_t at(int x, int y) const { return bits_[static_cast<std::size_t>(y) * static_cast<std::size_t>(w_) + static_cast<std::size_t>(x)] & 0xffffffu; }
    std::uint32_t* bits() { return bits_; }
    int w() const { return w_; }
    int h() const { return h_; }

private:
    int w_, h_;
    HBITMAP bmp_ = nullptr;
    std::uint32_t* bits_ = nullptr;
    HDC dc_ = nullptr;
    HGDIOBJ old_ = nullptr;
};

// A pattern's bits as a 32-bpp DIB section (not selected into a DC while the brush is made).
struct PatternBitmap {
    HBITMAP bmp = nullptr;
    explicit PatternBitmap(const scene_raster::PatternBits& b) {
        if (b.w <= 0 || b.h <= 0) return;
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = b.w;
        bi.bmiHeader.biHeight = -b.h;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* p = nullptr;
        bmp = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &p, nullptr, 0);
        if (bmp && p) std::copy(b.px.begin(), b.px.end(), static_cast<std::uint32_t*>(p));
    }
    ~PatternBitmap() {
        if (bmp) DeleteObject(bmp);
    }
    PatternBitmap(const PatternBitmap&) = delete;
    PatternBitmap& operator=(const PatternBitmap&) = delete;
};

// COLORREF 0x02BBGGRR as the DIB section's 0x00RRGGBB is scene_raster::rgb_of; the replay hands
// GDI the plain RGB (a true-colour DC ignores the PALETTERGB flag).
SceneGdi plain(std::function<HBITMAP(Pattern)> pattern = {}) {
    SceneGdi w;
    w.colour = [](std::uint32_t c) { return static_cast<COLORREF>(c & 0x00ffffffu); };
    w.pattern = std::move(pattern);
    return w;
}

struct Compared {
    long same = 0, differ = 0, unsure = 0;
};
Compared compare(const Dib32& d, const scene_raster::Image& host) {
    Compared c;
    for (int y = 0; y < host.h; ++y)
        for (int x = 0; x < host.w; ++x) {
            if (host.unsure(x, y)) {
                ++c.unsure;
                continue;
            }
            if (d.at(x, y) == host.at(x, y)) ++c.same;
            else ++c.differ;
        }
    return c;
}

void write_ppm(const fs::path& f, const Dib32& d) {
    std::FILE* o = std::fopen(f.string().c_str(), "wb");
    if (!o) return;
    std::fprintf(o, "P6\n%d %d\n255\n", d.w(), d.h());
    for (int y = 0; y < d.h(); ++y)
        for (int x = 0; x < d.w(); ++x) {
            const std::uint32_t c = d.at(x, y);
            const unsigned char b[3] = {static_cast<unsigned char>(c >> 16), static_cast<unsigned char>(c >> 8),
                                        static_cast<unsigned char>(c)};
            std::fwrite(b, 1, 3, o);
        }
    std::fclose(o);
}

Op op(Op::Kind k) {
    Op o;
    o.kind = k;
    return o;
}

}  // namespace

TEST_CASE("scene win32: draw_scene matches the host rasteriser on a hand-made op list") {
    Render r;
    Op clear = op(Op::kPatBlt);
    clear.w = 64, clear.h = 48, clear.rop = kBlackness;
    r.ops.push_back(clear);
    Op b = op(Op::kBrush);
    b.colour = 0x02204080u;
    r.ops.push_back(b);
    for (const auto& e : std::vector<std::array<int, 4>>{{10, 10, 5, 3}, {20, 10, -4, 3}, {30, 20, 3, -5}, {40, 30, -2, -2},
                                                        {50, 5, 0, 4}, {5, 40, 6, 0}, {2, 2, 1, 1}}) {
        Op p = op(Op::kPatBlt);
        p.x = e[0], p.y = e[1], p.w = e[2], p.h = e[3];
        r.ops.push_back(p);
    }
    Op pat = op(Op::kPattern);
    pat.pattern = Pattern::kMetal;
    r.ops.push_back(pat);
    Op pp = op(Op::kPatBlt);
    pp.x = 11, pp.y = 25, pp.w = -9, pp.h = 13;  // the origin of the brush at the DC's (0, 0)
    r.ops.push_back(pp);
    Op bl = op(Op::kPatBlt);
    bl.x = 13, bl.y = 27, bl.w = 3, bl.h = 3, bl.rop = kBlackness;
    r.ops.push_back(bl);
    Op fr = op(Op::kFrame);
    fr.x = 30, fr.y = 34, fr.w = 20, fr.h = 10, fr.mode = 2;
    r.ops.push_back(fr);

    scene_raster::PatternBits bits;
    bits.w = bits.h = 8;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) bits.px.push_back(((x + y) & 1) ? 0xc0c0c0u : (x == 0 ? 0x000000u : 0x808080u));
    PatternBitmap pb(bits);
    REQUIRE(pb.bmp != nullptr);
    Dib32 d(64, 48);
    draw_scene(d.dc(), r, plain([&](Pattern p) { return p == Pattern::kMetal ? pb.bmp : nullptr; }));
    const scene_raster::Image host = scene_raster::rasterise(r, 64, 48, [&](Pattern) { return bits; });
    const Compared c = compare(d, host);
    CHECK(c.differ == 0);
    CHECK(c.same > 64 * 48 - 200);
    // Negative extents: x 17..20 of the second PatBlt, y 16..20 of the third.
    CHECK(d.at(17, 10) == 0x804020u);
    CHECK(d.at(20, 12) == 0x804020u);
    CHECK(d.at(21, 10) == 0u);
    CHECK(d.at(30, 16) == 0x804020u);
    CHECK(d.at(30, 20) == 0x804020u);
    CHECK(d.at(30, 21) == 0u);
    // The frame's light-grey fill and its mode-2 bottom line in 0x808080 (ordinal 34).
    CHECK(d.at(40, 38) == 0xc0c0c0u);
    CHECK(d.at(40, 44) == 0x808080u);
}

TEST_CASE("scene win32: IGRAPHIC.DLL #6600 100 and 105 replayed at 1024x768 (golden)") {
    const fs::path dir = original_dir();
    const fs::path gf = find_ci(dir, "IGRAPHIC.DLL"), lf = find_ci(dir, "ENG_LANG.DLL");
    if (gf.empty() || lf.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    const fmt::NeModule graphics = fmt::NeModule::load(gf);
    const fmt::NeModule lang = fmt::NeModule::load(lf);
    assets::AssetStore store(dir);
    Inputs in;
    in.W = 1024;
    in.H = 768;
    in.cx = 1024;
    in.cy = 768;
    in.strings = [&](int id) { return fmt::load_string(lang, static_cast<std::uint16_t>(id)).value_or(""); };

    // The patterns as the oracle shows them: METAL with its own colours, graphic 76's 4-bpp
    // indices in the 16-colour VGA set [ORACLE core-0008].
    static const std::uint32_t vga16[16] = {0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080,
                                            0x008080, 0xc0c0c0, 0x808080, 0xff0000, 0x00ff00, 0xffff00,
                                            0x0000ff, 0xff00ff, 0x00ffff, 0xffffff};
    auto bits_of = [&](Pattern p) {
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
    const scene_raster::PatternBits metal_bits = bits_of(Pattern::kMetal), g76_bits = bits_of(Pattern::kGraphic76);
    PatternBitmap metal(metal_bits), g76(g76_bits);
    auto pattern = [&](Pattern p) -> HBITMAP {
        return p == Pattern::kMetal ? metal.bmp : p == Pattern::kGraphic76 ? g76.bmp : nullptr;
    };

    const char* dump = std::getenv("IS_SCENE_DUMP_DIR");
    const bool dumping = dump && *dump && fs::is_directory(dump);
    for (int id : {100, 105}) {
        Inputs i = in;
        // The oracle runs: newgame-0002's desk without a wave device, core-0008's corridor with one.
        i.devices.async_wave = i.devices.stereo = id == 105;
        const Render r = render(fmt::load_scene6600(graphics, static_cast<std::uint16_t>(id)), i);
        Dib32 d(1024, 768);
        draw_scene(d.dc(), r, plain(pattern));
        const scene_raster::Image host = scene_raster::rasterise(r, 1024, 768, bits_of);
        const Compared c = compare(d, host);
        INFO("scene ", id, ": ", c.same, " same, ", c.differ, " differ, ", c.unsure, " uncertain");
        CHECK(c.differ == 0);
        CHECK(c.same > 700000);
        if (dumping) write_ppm(fs::path(dump) / ("wine-" + std::to_string(id) + ".ppm"), d);
    }

    // The port's form on the 8-bit renderer: the scene's colours allocated through command 6
    // and published, the patterns from the gfx loaders. Graphic 76's dots and ground show the
    // oracle's #C0C0C0 and #808080 entries (ADR 0007 decision 2: the pixels are indices).
    gfx::Gfx g(&store);
    g.set_client_size(1024, 768, 0);
    g.palette_create();
    g.palette_save(0, 1);  // seg10:0x6da6(0, 1): entries 10-245 black before the render [BYTES seg15:0x3099-0x309d]
    Inputs i = in;
    i.devices.async_wave = i.devices.stereo = true;
    const Render r = render(fmt::load_scene6600(graphics, 105), i);
    gfx::ImagePtr img = g.create_image(1024, 768);
    std::vector<std::string> todos;
    draw_scene(g, img->hdc(), r, [&](const std::string& w, const char*) { todos.push_back(w); });
    img->flush();
    CHECK(todos.empty());
    const auto& dev = g.palettes().device();
    auto rgb = [&](int x, int y) {
        const auto& e = dev[img->at(x, y)];
        return (static_cast<std::uint32_t>(e.r) << 16) | (static_cast<std::uint32_t>(e.g) << 8) | e.b;
    };
    if (dumping) {
        Dib32 out(1024, 768);
        for (int y = 0; y < 768; ++y)
            for (int x = 0; x < 1024; ++x) out.bits()[static_cast<std::size_t>(y) * 1024 + static_cast<std::size_t>(x)] = rgb(x, y);
        write_ppm(fs::path(dump) / "gfx-105.ppm", out);
    }
    // Record 2's front, the left far box, x 427 ... 470 at y 240 ... 250: core-0008 shows a
    // #808080 ground with #000000 holes and #C0C0C0 dots. Ordinal 1 gives graphic 76 the fixed
    // 16-colour table (entry 15 is not black) and seg6:0x7fc8 maps its pixels so that each keeps
    // its VGA colour (gfx/palette.h fixed_set_remap) [BYTES ISPACE.DLL seg5:0x571-0x57c
    // seg6:0x80bc-0x815b] [ORACLE core-0008] (review H4).
    std::map<std::uint32_t, int> box;
    for (int y = 240; y < 250; ++y)
        for (int x = 427; x < 470; ++x) ++box[rgb(x, y)];
    INFO("box colours: ", box.size());
    CHECK(box.count(0x808080u) == 1);
    CHECK(box.count(0x000000u) == 1);
    CHECK(box.count(0x0000ffu) == 0);
    CHECK(box.count(0xff00ffu) == 0);
    // The solid fills land on allocated or nearest entries: the board's green (record 1's top)
    // is a green.
    const std::uint32_t board = rgb(300, 600);
    CHECK(((board >> 8) & 0xff) > ((board >> 16) & 0xff));
}

TEST_CASE("scene win32: render_scene, the program's ordinal 32 (golden)") {
    const fs::path dir = original_dir();
    if (dir.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    assets::AssetStore store(dir);
    const Render r = render_scene(store, 105, 1024, 768, 1024, 768, [&](int id) {
        return store.find_string(static_cast<std::uint16_t>(id)).value_or("");
    });
    REQUIRE(r.slots.size() >= 2);
    CHECK(r.slots[0] == Rect{-255, 618, 1281, 629});
    CHECK(r.slots[1] == Rect{419, 201, 457, 238});
    int cpu = 0, plate = 0;
    for (const Op& o : r.ops) {
        if (o.kind == Op::kText && o.text == "486") ++cpu;
        if (o.kind == Op::kText && (o.text == "HIGH" || o.text == "VOLTAGE")) ++plate;
    }
    CHECK(cpu == 1);
    CHECK(plate == 2);
}

TEST_CASE("scene win32: read_devices answers as the system does") {
    const Conditions c = read_devices();
    // Wine and Windows 10/11 have no [Boot.description] display.drv: the default label.
    CHECK(!c.video.empty());
    CHECK(c.c70 == 4);
    CHECK(c.a8 == 0);
    CHECK(cpu_text(c.c70) == "486");
}
#endif
