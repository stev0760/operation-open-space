// The explosion's fireballs against the core-0001 oracle (golden): seg8:0x3fbe's BitBlt from
// slot 10 (IGRAPHIC.DLL #4000 170 at DGROUP:0xb70's offsets, side 6f + 10 at Size 1) as the
// port issues it, cropped from the graphic, and searched for in the death run's screenshots
// work/oracle/core-0001/death/death-c0705.png and death-c0707.png (the player's explosion,
// 0.15 s apart) [ORACLE core-0001]. The graphic is compared in its own DIB colours: the
// wave palette's recolour pass (seg5:0x26a2) is not applied here, so a pixel matches when its
// colour is within a small distance [DESIGN]. Reads IS_ORIGINAL_DIR and work/oracle beside it,
// and skips without them.
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "assets/asset_store.h"
#include "doctest.h"
#include "formats/dib.h"
#include "png_decode.h"
#include "world/effects.h"

using namespace is::world;

namespace {

std::filesystem::path original_dir() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    return v;
}

std::filesystem::path oracle_death() {
    const auto o = original_dir();
    if (o.empty()) return {};
    std::filesystem::path base = o;
    if (!base.has_filename()) base = base.parent_path();
    const auto d = base.parent_path().parent_path() / "oracle" / "core-0001" / "death";
    return std::filesystem::is_directory(d) ? d : std::filesystem::path();
}

int dist2(std::uint32_t a, std::uint32_t b) {
    const int dr = static_cast<int>((a >> 16) & 0xff) - static_cast<int>((b >> 16) & 0xff);
    const int dg = static_cast<int>((a >> 8) & 0xff) - static_cast<int>((b >> 8) & 0xff);
    const int db = static_cast<int>(a & 0xff) - static_cast<int>(b & 0xff);
    return dr * dr + dg * dg + db * db;
}

struct Best {
    int x = 0, y = 0, lit = 0, miss = 1 << 30;
};

}  // namespace

TEST_CASE("effects: the fireballs of slot 10 against the core-0001 death (golden)") {
    const auto dir = original_dir();
    if (dir.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    const auto od = oracle_death();
    if (od.empty() || !std::filesystem::exists(od / "death-c0705.png")) {
        MESSAGE("skipped: work/oracle/core-0001/death not found");
        return;
    }
    is::assets::AssetStore store(dir);
    const is::fmt::Dib dib = store.sprite(static_cast<std::uint16_t>(kGraphicExplosion));
    REQUIRE(dib.width == 248);
    REQUIRE(dib.height == 52);
    const std::vector<std::uint32_t> px = is::fmt::to_bgra(dib);

    // The source squares as seg8:0x3fbe issues them for +0x00 = 1 ... 8 at Size 1.
    struct Cell {
        int sx, side;
    };
    std::vector<Cell> cells;
    {
        World w;
        clear_effects(w.effects);
        w.W = 1024;
        w.H = 683;
        w.size = 1;
        EffectState st;
        EffectWords in;
        in.dg_1318 = 7;
        EffectHost host;
        is::shell::Rng rng;
        RecordingBlitter out;
        ImageSlotTable images{};
        images[kSlotExplosion] = 0x50;
        EffectCtx c{w, st, in, host, rng, &out, &images, nullptr, nullptr, nullptr};
        EffectEntry& e = w.effects[0];
        e.frame = 1;
        e.end = 10;
        e.type = 4;
        st.sides.centre[0] = EffectPoint{300, 300};
        for (int f = 0; f < 8; ++f) {
            out.calls.clear();
            effect_frame(c, 0);
            REQUIRE(out.calls.size() == 1);
            CHECK(out.calls[0].src == 0x50);
            cells.push_back({out.calls[0].sx, out.calls[0].w});
            // Each cell lies inside the graphic: the last ends at its right edge.
            CHECK(out.calls[0].sx + out.calls[0].w <= 248);
        }
        CHECK(cells[7].sx + cells[7].side == 248);
    }

    // Two shots where a whole fireball stands clear of the smoke: the search over the 240 x 200
    // around the ship's last place (530, 355 in death-c0703.png) finds it pixel for pixel. A
    // search of all eight shots and the sides 22 ... 52 also finds side 22 and a second side 40
    // with a few smoke pixels over them [ORACLE core-0001].
    const int kTol = 48 * 48;  // squared RGB distance for a match [DESIGN]
    struct Case {
        const char* shot;
        int frame;
    };
    for (const Case& cs : {Case{"death-c0705.png", 4}, Case{"death-c0707.png", 5}}) {
        const std::string shot = cs.shot;
        CAPTURE(shot);
        if (!std::filesystem::exists(od / cs.shot)) continue;
        const pngtest::Image img = pngtest::read((od / cs.shot).string());
        REQUIRE(img.w >= 700);
        REQUIRE(img.h >= 500);
        const Cell& cl = cells[static_cast<std::size_t>(cs.frame)];
        Best b;
        for (int y0 = 250; y0 + cl.side < 450; ++y0)
            for (int x0 = 410; x0 + cl.side < 650; ++x0) {
                int lit = 0, miss = 0;
                for (int y = 0; y < cl.side && miss < b.miss; ++y)
                    for (int x = 0; x < cl.side; ++x) {
                        const std::uint32_t g = px[static_cast<std::size_t>(y) * 248 + cl.sx + x] & 0xffffff;
                        const std::uint32_t sc = img.at(x0 + x, y0 + y) & 0xffffff;
                        if (g != 0) ++lit;
                        if (dist2(g, sc) > kTol) ++miss;
                    }
                if (miss < b.miss) {
                    b.miss = miss;
                    b.x = x0;
                    b.y = y0;
                    b.lit = lit;
                }
            }
        MESSAGE(shot << ": fireball " << cs.frame << " (side " << cl.side << ", source x " << cl.sx
                        << ") at (" << b.x << ", " << b.y << "), " << b.miss << " of " << b.lit
                        << " lit pixels differ");
        CHECK(b.lit > 500);
        CHECK(b.miss == 0);
    }
}
