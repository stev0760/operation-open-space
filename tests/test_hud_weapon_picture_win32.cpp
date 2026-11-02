// M4 stage 2, track DISPLAY, on GDI (Wine in CTest): the weapon display with weapon 8's composed
// picture (seg11:0x2ff6 from the HUD's seg10:0x533a sheet of graphic 370) and the score panel,
// drawn by the HUD's repaint at 1024 x 768 with the original's files (IS_ORIGINAL_DIR) and
// compared pixel by pixel with the oracle's crops of core-0002 (work/oracle/core-0002/picture):
// the weapon display at 20, 19 and 16 (352 x 84 from (670, 684)), the score panel at 000, 900,
// 4000 and 8000 (144 x 84 from (0, 684)). The comparison reports; it requires only the picture's
// box, where the drawing is the composed one. IS_HUD_DUMP_DIR names a directory for the port's
// crops [DESIGN].
#ifdef _WIN32
#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "assets/asset_store.h"
#include "doctest.h"
#include "gfx/gfx.h"
#include "help/png.h"
#include "hud/gdi_canvas.h"
#include "hud/hud.h"
#include "hud/weapons.h"
#include "hud_fakes.h"
#include "png_decode.h"

using namespace is::hud;
using hudtest::FakeHost;

namespace {

std::filesystem::path original_dir() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    return v;
}

// work/oracle/core-0002/picture beside IS_ORIGINAL_DIR (work/extracted/shipdisk).
std::filesystem::path picture_dir() {
    std::filesystem::path base = original_dir();
    if (base.empty()) return {};
    if (!base.has_filename()) base = base.parent_path();
    const auto d = base.parent_path().parent_path() / "oracle" / "core-0002" / "picture";
    return std::filesystem::is_directory(d) ? d : std::filesystem::path();
}

std::uint32_t rgb_of(const is::gfx::Gfx& g, int x, int y) {
    const auto& c = g.palettes().device()[g.surface().at(x, y)];
    return (static_cast<std::uint32_t>(c.r) << 16) | (c.g << 8) | c.b;
}

void dump(const is::gfx::Gfx& g, int x0, int y0, int w, int h, const std::string& name) {
    const char* dir = std::getenv("IS_HUD_DUMP_DIR");
    if (!dir || !*dir) return;
    is::fmt::Dib dib;
    dib.width = w;
    dib.height = h;
    dib.bpp = 8;
    dib.bottom_up = false;
    for (const auto& c : g.palettes().device())
        dib.palette.push_back((static_cast<std::uint32_t>(c.r) << 16) | (c.g << 8) | c.b);
    dib.bits.resize(dib.stride() * static_cast<std::size_t>(h));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) dib.bits[dib.stride() * y + x] = g.surface().at(x0 + x, y0 + y);
    const auto png = is::help::png_from_dib(dib);
    std::ofstream(std::filesystem::path(dir) / (name + ".png"), std::ios::binary)
        .write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
}

// The fake host with the ENG_LANG strings of the original's files.
struct LangHost : FakeHost {
    is::assets::AssetStore* store = nullptr;
    std::string lang(int id) override {
        if (auto t = store->find_string(static_cast<std::uint16_t>(id))) return *t;
        return {};
    }
};

struct Count {
    int differ = 0, wide = 0, port_lit = 0, oracle_lit = 0;
    int bx0 = 9999, by0 = 9999, bx1 = -1, by1 = -1;  // the port's non-black box (screen)
};

// The crop at (x0, y0) of the oracle's picture against the surface, over the sub-box (bx, by,
// bw, bh) in crop coordinates.
Count compare(const is::gfx::Gfx& g, const pngtest::Image& o, int x0, int y0, int bx, int by, int bw, int bh) {
    Count c;
    for (int y = by; y < by + bh; ++y)
        for (int x = bx; x < bx + bw; ++x) {
            const std::uint32_t a = rgb_of(g, x0 + x, y0 + y), b = o.at(x, y);
            if (a != 0) {
                ++c.port_lit;
                c.bx0 = std::min(c.bx0, x0 + x), c.by0 = std::min(c.by0, y0 + y);
                c.bx1 = std::max(c.bx1, x0 + x), c.by1 = std::max(c.by1, y0 + y);
            }
            if (b != 0) ++c.oracle_lit;
            if (a == b) continue;
            ++c.differ;
            for (int sh = 0; sh <= 16; sh += 8) {
                const int d = static_cast<int>((a >> sh) & 0xff) - static_cast<int>((b >> sh) & 0xff);
                if (d > 12 || d < -12) {
                    ++c.wide;
                    break;
                }
            }
        }
    return c;
}

}  // namespace

TEST_CASE("hud win32: golden: the weapon display and the score panel against core-0002") {
    const auto dir = original_dir();
    if (dir.empty()) {
        MESSAGE("IS_ORIGINAL_DIR not set: skipped");
        return;
    }
    is::assets::AssetStore store(dir);
    is::gfx::Gfx g(&store);
    g.set_client_size(1024, 768, 1);
    g.palette_create();
    g.palette_scheme(8, 0);  // as the wave-0001 golden test [OPEN: the wave's palette]
    g.blacken();
    HFONT font = CreateFontW(14, 0, 0, 0, 0, 0, 0, 0, ANSI_CHARSET, 0, 0, PROOF_QUALITY,
                             VARIABLE_PITCH | FF_SWISS, L"Arial");
    GdiCanvas cv(g, [font] { return font; });
    std::string why;
    REQUIRE_MESSAGE(cv.load(&why), why);

    // seg11:0x2ff6(8) at Size 1 from the sheet of graphic 370.
    const WeaponTable t(0);
    WeaponPictureInputs in;
    in.size_level = 1;
    in.sheet_slot = 0x30;
    const WeaponPicture p = weapon_picture(t, 8, in);
    cv.set_image(Img::kWeaponSheet, render_weapon_sheet(g, store.sprite(370), 370, 0));
    REQUIRE(cv.has(Img::kWeaponSheet));
    MESSAGE("the sheet of graphic 370: ", cv.width(Img::kWeaponSheet), " x ", cv.height(Img::kWeaponSheet));
    compose_weapon_picture(cv, p);
    cv.set_image(Img::kWeaponSheet, nullptr);
    CHECK(cv.width(Img::kWeapon) == 46);
    CHECK(cv.height(Img::kWeapon) == 46);

    const auto pd = picture_dir();
    const struct {
        int count;
        std::uint32_t score;
        const char* weapon_png;
        const char* score_png;
    } cases[] = {{20, 0, "weapon-20-start.png", "score-000.png"},
                 {19, 4000, "weapon-19.png", "score-4000.png"},
                 {16, 900, "weapon-16-tab2.png", "score-900-kill.png"},
                 {20, 8000, nullptr, "score-8000.png"}};
    for (const auto& k : cases) {
        LangHost h;
        h.store = &store;
        h.ship_.fuel_max = 660;
        h.ship_.fuel = 660;
        h.weapon_.name_id = 194;
        h.weapon_.counted = true;
        h.weapon_.no_count = false;
        h.weapon_.count = k.count;
        h.score_ = k.score;
        Hud hud(cv, h);
        hud.loaded();
        hud.info().set_mode(kModeTarget);
        hud.repaint();
        hud.info().flush();
        g.surface().flush();
        const std::string tag = std::to_string(k.count) + "-" + std::to_string(k.score);
        dump(g, 670, 684, 352, 84, "port-weapon-" + tag);
        dump(g, 0, 684, 144, 84, "port-score-" + tag);
        if (pd.empty()) {
            MESSAGE("work/oracle/core-0002/picture not found: the comparison is skipped");
            continue;
        }
        if (k.weapon_png) {
            const pngtest::Image o = pngtest::read((pd / k.weapon_png).string());
            REQUIRE(o.w == 352);
            const Count all = compare(g, o, 670, 684, 0, 0, 352, 84);
            // the picture's bitmap: x 826 ... 871, y 716 ... 761 on screen (s2-display.md 1)
            const Count pic = compare(g, o, 670, 684, 156, 32, 46, 46);
            MESSAGE(std::string(k.weapon_png), ": ", all.differ, " of ", 352 * 84, " differ (", all.wide,
                    " by more than 12); picture box: ", pic.differ, " of ", 46 * 46, " differ (", pic.wide,
                    "), lit port ", pic.port_lit, " oracle ", pic.oracle_lit, ", the port's lit box (",
                    pic.bx0, ", ", pic.by0, ") - (", pic.bx1, ", ", pic.by1, ")");
            // the oracle's non-black box is (828, 728) - (871, 751) [ORACLE core-0002]
            CHECK(pic.port_lit > 0);
        }
        const pngtest::Image so = pngtest::read((pd / k.score_png).string());
        REQUIRE(so.w == 144);
        const Count sc = compare(g, so, 0, 684, 0, 0, 144, 84);
        const Count dg = compare(g, so, 0, 684, 0, 40, 144, 44);  // the digits' rows
        MESSAGE(std::string(k.score_png), ": ", sc.differ, " of ", 144 * 84, " differ (", sc.wide, " by more than 12); digit rows ",
                dg.differ, " of ", 144 * 44, " (", dg.wide, ")");
    }
    DeleteObject(font);
}
#endif
