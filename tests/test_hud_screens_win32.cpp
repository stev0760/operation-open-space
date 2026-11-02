// M4 stage 1 SCREENS on GDI (Wine in CTest), with the original's files (IS_ORIGINAL_DIR): the
// message display, the debriefing box and the GAME OVER screen drawn at 1024 x 768 and compared
// pixel by pixel with the oracle's core-0001 screenshots (work/oracle/core-0001). The comparison
// reports the differing pixels per region; it does not require a match (the wave's palette and
// the parts other tracks draw differ) [DESIGN].
#ifdef _WIN32
#include <windows.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "assets/asset_store.h"
#include "briefing/debriefing.h"
#include "briefing/gdi_canvas.h"
#include "doctest.h"
#include "gfx/gfx.h"
#include "help/png.h"
#include "hud/gdi_canvas.h"
#include "hud/hud.h"
#include "hud/messages.h"
#include "hud/screens.h"
#include "hud_fakes.h"
#include "png_decode.h"
#include "shell/geometry.h"

using namespace is::hud;
using hudtest::FakeHost;

namespace {

std::filesystem::path original_dir() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    return v;
}

std::filesystem::path oracle_dir() {
    const auto o = original_dir();
    if (o.empty()) return {};
    std::filesystem::path base = o;
    if (!base.has_filename()) base = base.parent_path();
    const auto d = base.parent_path().parent_path() / "oracle" / "core-0001";
    return std::filesystem::is_directory(d) ? d : std::filesystem::path();
}

std::uint32_t rgb_of(const is::gfx::Gfx& g, int x, int y) {
    const auto& c = g.palettes().device()[g.surface().at(x, y)];
    return (static_cast<std::uint32_t>(c.r) << 16) | (c.g << 8) | c.b;
}

struct Region {
    const char* name;
    int x, y, w, h;
};

int diff(const is::gfx::Gfx& g, const pngtest::Image& o, const Region& r, int* wide, int tol = 12) {
    int n = 0;
    *wide = 0;
    for (int y = r.y; y < r.y + r.h; ++y)
        for (int x = r.x; x < r.x + r.w; ++x) {
            const std::uint32_t a = rgb_of(g, x, y), b = o.at(x, y);
            if (a == b) continue;
            ++n;
            for (int sh = 0; sh <= 16; sh += 8) {
                const int d = static_cast<int>((a >> sh) & 0xff) - static_cast<int>((b >> sh) & 0xff);
                if (d > tol || d < -tol) {
                    ++*wide;
                    break;
                }
            }
        }
    return n;
}

void report(const is::gfx::Gfx& g, const char* shot, const std::vector<Region>& regions) {
    const auto od = oracle_dir();
    if (od.empty()) {
        MESSAGE("work/oracle/core-0001 not found: the comparison is skipped");
        return;
    }
    const pngtest::Image o = pngtest::read((od / shot).string());
    REQUIRE(o.w == 1024);
    REQUIRE(o.h == 768);
    for (const Region& r : regions) {
        int wide = 0;
        const int d = diff(g, o, r, &wide);
        MESSAGE(shot, " ", std::string(r.name), ": ", d, " of ", r.w * r.h, " pixels differ, ", wide,
                " by more than 12 levels");
    }
}

// IS_M4_DUMP names a directory for the port's pictures, for a look [DESIGN].
void dump(const is::gfx::Gfx& g, const char* name) {
    const char* dir = std::getenv("IS_M4_DUMP");
    if (!dir || !*dir) return;
    is::fmt::Dib dib;
    dib.width = 1024;
    dib.height = 768;
    dib.bpp = 8;
    dib.bottom_up = false;
    for (const auto& c : g.palettes().device())
        dib.palette.push_back((static_cast<std::uint32_t>(c.r) << 16) | (c.g << 8) | c.b);
    dib.bits.resize(dib.stride() * 768);
    for (int y = 0; y < 768; ++y)
        for (int x = 0; x < 1024; ++x) dib.bits[dib.stride() * y + x] = g.surface().at(x, y);
    const auto png = is::help::png_from_dib(dib);
    std::ofstream(std::filesystem::path(dir) / name, std::ios::binary)
        .write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
}

struct Setup {
    is::assets::AssetStore store;
    is::gfx::Gfx g;
    explicit Setup(const std::filesystem::path& dir) : store(dir), g(&store) {
        g.set_client_size(1024, 768, 1);
        g.palette_create();
        g.palette_scheme(8, 0);
        g.blacken();
    }
    std::string lang(int id) {
        if (auto s = store.find_string(static_cast<std::uint16_t>(id))) return *s;
        return {};
    }
};

// The message display's texts from the dispatcher.
class MsgHost : public FakeHost {
public:
    MessageState* m = nullptr;
    Setup* s = nullptr;
    std::string lang(int id) override { return s->lang(id); }
    std::string text_8740() const override { return m->sender; }
    std::string text_8768() const override { return m->line1; }
    std::string text_87b8() const override { return m->line2; }
};

class Dispatch : public MessageHost {
public:
    Setup* s = nullptr;
    std::string pilot() const override { return "Your Name"; }
    std::string lang(int id) override { return s->lang(id); }
    std::string name(int pool, int) override { return pool == 7 ? cut_name_code(s->lang(347)) : std::string(); }
};

}  // namespace

TEST_CASE("hud screens win32: golden: the message display against core-0001") {
    const auto dir = original_dir();
    if (dir.empty()) {
        MESSAGE("IS_ORIGINAL_DIR not set: skipped");
        return;
    }
    Setup s(dir);
    HFONT font = CreateFontW(14, 0, 0, 0, 0, 0, 0, 0, ANSI_CHARSET, 0, 0, PROOF_QUALITY,
                             VARIABLE_PITCH | FF_SWISS, L"Arial");
    GdiCanvas cv(s.g, [font] { return font; });
    std::string why;
    REQUIRE_MESSAGE(cv.load(&why), why);
    cv.set_image(Img::kLogoStrip, s.g.logo_strip());
    MessageState m;
    Dispatch d;
    d.s = &s;
    dispatch_message(m, d, 0, 7, 0, 83);  // "THRUSTER SYSTEM OK / REPAIRS COMPLETE"
    CHECK(m.line1 == "THRUSTER SYSTEM OK ");
    MsgHost h;
    h.m = &m;
    h.s = &s;
    h.ship_.fuel_max = 660;
    h.ship_.fuel = 660;
    // The ambulance's object: a dead pool-7 record; extent 0x140, size 40 and gap 6 are the
    // logo cell's [OPEN: the pool-7 record's values come from the world's set-up].
    CellInputs c;
    c.obj.state = 1;
    c.obj.extent = 0x140;
    c.obj.size = 40;
    c.obj.gap = 6;
    h.cells_[{7, 0}] = c;
    Hud hud(cv, h);
    hud.loaded();
    hud.repaint();
    hud.info().mark(1).pool = 7;
    hud.info().mark(1).rec = 0;
    hud.info().set_mode_119c(kModeMessage);
    hud.info().flush();
    s.g.surface().flush();
    dump(s.g, "message-83.png");
    report(s.g, "hit/24-d2-shot.png",
           {{"message display (147,688 273x76)", 147, 688, 273, 76},
            {"message title (147,688 120x14)", 147, 688, 120, 14},
            {"message sender (147,706 120x14)", 147, 706, 120, 14},
            {"message lines (147,734 273x30)", 147, 734, 273, 30},
            {"logo (360,688 60x50)", 360, 688, 60, 50}});
    DeleteObject(font);
}

TEST_CASE("hud screens win32: golden: the debriefing box against core-0001") {
    const auto dir = original_dir();
    if (dir.empty()) {
        MESSAGE("IS_ORIGINAL_DIR not set: skipped");
        return;
    }
    Setup s(dir);
    is::briefing::GdiCanvas c(s.g);
    debrief_blackout(c, 1024, 768);
    const auto b = is::briefing::debrief_box(1024, 768, 1, [&s](int id) { return s.lang(id); });
    draw_debrief_box(c, b);
    s.g.surface().flush();
    dump(s.g, "debriefing.png");
    report(s.g, "hit/43-debriefing.png",
           {{"title bar (216,196 593x26)", 216, 196, 593, 26},
            {"frame left (216,196 8x369)", 216, 196, 8, 369},
            {"frame right (800,196 9x369)", 800, 196, 9, 369},
            {"frame bottom (216,556 593x9)", 216, 556, 593, 9},
            {"outside the box, top (0,0 1024x190)", 0, 0, 1024, 190}});
}

TEST_CASE("hud screens win32: golden: GAME OVER against core-0001") {
    const auto dir = original_dir();
    if (dir.empty()) {
        MESSAGE("IS_ORIGINAL_DIR not set: skipped");
        return;
    }
    Setup s(dir);
    GdiCanvas cv(s.g, [] { return HFONT(nullptr); });
    const auto lines =
        is::briefing::game_over_lines(1024, 768, 3, 1, 0, 4900, true, [&s](int id) { return s.lang(id); });
    REQUIRE(lines.size() == 6u);
    CHECK(lines[5].text == "SCORE: 4900");
    game_over_entry(cv, 1024, 768, false, lines);
    s.g.surface().flush();
    dump(s.g, "game-over.png");
    report(s.g, "death/after-c0004.png",
           {{"frame", 0, 0, 1024, 768},
            {"GAME OVER (402,199 220x30)", 402, 199, 220, 30},
            {"LAST WAVE / ICONS (262,259 520x110)", 262, 259, 520, 110},
            {"SCORE (402,439 220x30)", 402, 439, 220, 30}});
}
#endif
