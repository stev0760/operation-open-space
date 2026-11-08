// M4 stage 5, track WEAPONS, on GDI (Wine in CTest): seg10:0x4412's animation-4 sheet of weapon 10
// (REAR-DEPLOYED MINES, IGRAPHIC 150, six frames) drawn through the GdiBlitter in COLORONCOLOR,
// then seg11:0x2ff6's picture from frame 5 and the weapon display, compared with core-0006's crop
// "20 - REAR-DEPLOYED MINES" (work/oracle/core-0006/picture/tf-weapon-slot1.png, 352 x 84 from
// (670, 684)) [ORACLE core-0006]. Every pixel of the crop has the oracle's shape: the differences
// are palette-level (160/159, 96/99, 224/219 grey, 156 pixels inside the mine), from the test's
// scheme 8 against the wave's palette [OPEN], so the shrink keeps the oracle's rows and columns.
// The check is that no pixel differs by more than 12 in a channel. IS_HUD_DUMP_DIR names a
// directory for the port's crop [DESIGN].
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
#include "world/sprite.h"

using namespace is::hud;

namespace {

std::filesystem::path original_dir_a4() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    return v;
}

std::filesystem::path picture_dir_a4() {
    std::filesystem::path base = original_dir_a4();
    if (base.empty()) return {};
    if (!base.has_filename()) base = base.parent_path();
    const auto d = base.parent_path().parent_path() / "oracle" / "core-0006" / "picture";
    return std::filesystem::is_directory(d) ? d : std::filesystem::path();
}

std::uint32_t rgb_a4(const is::gfx::Gfx& g, int x, int y) {
    const auto& c = g.palettes().device()[g.surface().at(x, y)];
    return (static_cast<std::uint32_t>(c.r) << 16) | (c.g << 8) | c.b;
}

void dump_a4(const is::gfx::Gfx& g, int x0, int y0, int w, int h, const std::string& name) {
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

struct LangHostA4 : hudtest::FakeHost {
    is::assets::AssetStore* store = nullptr;
    std::string lang(int id) override {
        if (auto t = store->find_string(static_cast<std::uint16_t>(id))) return *t;
        return {};
    }
};

}  // namespace

TEST_CASE("hud win32: golden: weapon 10's animation-4 sheet and picture against core-0006") {
    const auto dir = original_dir_a4();
    if (dir.empty()) {
        MESSAGE("IS_ORIGINAL_DIR not set: skipped");
        return;
    }
    is::assets::AssetStore store(dir);
    is::gfx::Gfx g(&store);
    g.set_client_size(1024, 768, 1);
    g.palette_create();
    g.palette_scheme(8, 0);  // as the weapon-8 golden test [OPEN: the wave's palette]
    g.blacken();
    HFONT font = CreateFontW(14, 0, 0, 0, 0, 0, 0, 0, ANSI_CHARSET, 0, 0, PROOF_QUALITY,
                             VARIABLE_PITCH | FF_SWISS, L"Arial");
    GdiCanvas cv(g, [font] { return font; });
    std::string why;
    REQUIRE_MESSAGE(cv.load(&why), why);

    // seg10:0x4412(k, 6, 150, 6): seg10:0x6c02(150, 3, -1), bmHeight, the plan, the GDI calls.
    is::world::GdiBlitter b(g);
    const is::world::DcId graphic = b.load_graphic(150, 3, -1);
    REQUIRE(graphic != is::world::kDcNone);
    const int h = b.image(graphic)->height();
    CHECK(h == 22);
    const Anim4Sheet plan = anim4_sheet_plan(h, 6);
    const is::world::DcId k = render_anim4_sheet(b, graphic, plan);
    REQUIRE(k != is::world::kDcNone);
    CHECK(b.image(k)->width() == 174);
    CHECK(b.image(k)->height() == 34);
    // Frame 5 (side 20 at (146, 6)) is lit, the 2-px margin right of it and the gap row black
    int lit = 0;
    for (int y = 6; y < 26; ++y)
        for (int x = 146; x < 166; ++x) lit += b.image(k)->at(x, y) != 0 ? 1 : 0;
    MESSAGE("frame 5: ", lit, " of 400 lit");
    CHECK(lit > 0);
    for (int y = 0; y < 34; ++y) CHECK(b.image(k)->at(167, y) == 0);

    // seg11:0x2ff6(10): the 22 x 22 cell at (146, 6) stretched to 29 x 29, no overlay
    const WeaponTable t(0);
    WeaponPictureInputs in;
    in.size_level = 1;
    in.sheet_slot = 0x31;
    const WeaponPicture p = weapon_picture(t, 10, in);
    CHECK(p.sx == 146);
    CHECK(p.side_plain == 29);
    cv.set_image(Img::kWeaponSheet, b.image(k));
    compose_weapon_picture(cv, p);
    cv.set_image(Img::kWeaponSheet, nullptr);
    CHECK(cv.width(Img::kWeapon) == 29);

    LangHostA4 host;
    host.store = &store;
    host.ship_.fuel_max = 660;
    host.ship_.fuel = 660;
    host.weapon_.name_id = 196;  // REAR-DEPLOYED MINES
    host.weapon_.counted = true;
    host.weapon_.no_count = false;
    host.weapon_.count = 20;
    Hud hud(cv, host);
    hud.loaded();
    hud.info().set_mode(kModeTarget);
    hud.repaint();
    hud.info().flush();
    g.surface().flush();
    dump_a4(g, 670, 684, 352, 84, "port-weapon-10-anim4");
    const auto pd = picture_dir_a4();
    if (pd.empty()) {
        MESSAGE("work/oracle/core-0006/picture not found: the comparison is skipped");
        DeleteObject(font);
        return;
    }
    const pngtest::Image o = pngtest::read((pd / "tf-weapon-slot1.png").string());
    REQUIRE(o.w == 352);
    int differ = 0, wide = 0, port_lit = 0, oracle_lit = 0;
    int bx0 = 9999, by0 = 9999, bx1 = -1, by1 = -1, ox0 = 9999, oy0 = 9999, ox1 = -1, oy1 = -1;
    for (int y = 24; y < 84; ++y)  // below the two text lines
        for (int x = 100; x < 250; ++x) {
            const std::uint32_t a = rgb_a4(g, 670 + x, 684 + y), c = o.at(x, y);
            if (a != 0) ++port_lit, bx0 = std::min(bx0, x), by0 = std::min(by0, y), bx1 = std::max(bx1, x), by1 = std::max(by1, y);
            if (c != 0) ++oracle_lit, ox0 = std::min(ox0, x), oy0 = std::min(oy0, y), ox1 = std::max(ox1, x), oy1 = std::max(oy1, y);
            if (a == c) continue;
            ++differ;
            for (int sh = 0; sh <= 16; sh += 8) {
                const int d = static_cast<int>((a >> sh) & 0xff) - static_cast<int>((c >> sh) & 0xff);
                if (d > 12 || d < -12) {
                    ++wide;
                    break;
                }
            }
        }
    MESSAGE("tf-weapon-slot1.png picture area: ", differ, " differ (", wide, " by more than 12); lit port ", port_lit, " box (", bx0, ", ", by0,
            ") - (", bx1, ", ", by1, "), oracle ", oracle_lit, " box (", ox0, ", ", oy0, ") - (", ox1, ", ", oy1, ")");
    CHECK(port_lit > 0);
    CHECK(wide == 0);
    DeleteObject(font);
}
#endif
