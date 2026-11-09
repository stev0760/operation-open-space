// owner: M4 stage 6, track PAD.
// The landing pad (src/gfx/pad.h): seg15:0x482e's geometry and seg15:0x4a2e's picture
// (work/analysis/m4/s6-pad.md section 1 with s6-pad-verify.md claims 1-15). The goldens compare
// the picture with the oracle's pad at 1024 x 768, newgame-0002's desk (intro-04.png) and
// core-0003's visit (landing-01-scene.png), both at (433, 642) [ORACLE newgame-0002] [ORACLE
// core-0003]; they read work/oracle beside IS_ORIGINAL_DIR and skip without it.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include "doctest.h"
#include "gfx/pad.h"
#include "png_decode.h"

using is::gfx::pad_geometry;
using is::gfx::pad_greys;
using is::gfx::pad_mask;
using is::gfx::pad_picture;
using is::gfx::PadGeometry;
using is::gfx::PadPicture;

TEST_CASE("pad: seg15:0x482e's geometry at the three test sizes") {
    // s6-pad.md 1.1's vectors [BYTES seg15:0x483d-0x4975] (verified claims 1-5).
    const PadGeometry a = pad_geometry(683, 768);
    CHECK(a.q == 910);
    CHECK(a.k == -190);
    CHECK(a.top == 131);
    CHECK(a.pw == 159);
    CHECK(a.pc == 74);
    CHECK(a.height() == 126);
    CHECK(a.poly[0].x == 14);
    CHECK(a.poly[0].y == 1);
    CHECK(a.poly[1].x == 144);
    CHECK(a.poly[1].y == 1);
    CHECK(a.poly[2].x == 159);
    CHECK(a.poly[2].y == 74);
    CHECK(a.poly[3].x == 0);
    CHECK(a.poly[3].y == 74);
    CHECK(a.m1 == -521);
    CHECK(a.m2 == 486);
    CHECK(a.x_left == 376);
    CHECK(a.vx == 452);
    CHECK(a.vy == 321);
    CHECK(a.d92dc == 200);
    // On screen: (W/2 - half(pw), client - pc - 0x34) = (433, 642).
    CHECK(1024 / 2 - a.pw / 2 == 433);
    CHECK(768 - a.pc - 0x34 == 642);

    const PadGeometry b = pad_geometry(500, 600);
    CHECK(b.top == 120);
    CHECK(b.pw == 152);
    CHECK(b.pc == 66);
    CHECK(b.poly[0].x == 16);
    CHECK(b.poly[1].x == 136);

    const PadGeometry c = pad_geometry(900, 1080);
    CHECK(c.top == 143);
    CHECK(c.pw == 166);
    CHECK(c.pc == 83);
    CHECK(c.poly[0].x == 12);
    CHECK(c.poly[1].x == 154);
}

TEST_CASE("pad: the 16-bit shift of seg15:0x483d wraps a play height above 8191") {
    // shl ax, 2 keeps the low word: 8192 << 2 = 0x8000, -32768 / 3 = -10922 [BYTES seg15:0x483d].
    const PadGeometry g = pad_geometry(8192, 9000);
    CHECK(g.q == -10922);
    CHECK(g.k == -12022);
}

TEST_CASE("pad: seg15:0x4a2e's picture at 1024 x 768, s6-pad.md 1.3's table") {
    const PadGeometry g = pad_geometry(683, 768);
    const PadPicture p = pad_picture(g);
    REQUIRE(p.w == 159);
    REQUIRE(p.h == 126);
    // row:first-last(row)/first-last(row+1)=grey, rows 2 ... 70 [ORACLE newgame-0002].
    const char* table =
        "2:15-143/15-143=49 4:14-144/14-144=53 6:14-144/14-144=58 8:14-144/13-145=62 10:13-145/13-145=67 "
        "12:13-145/13-145=71 14:13-146/12-146=75 16:12-146/12-146=80 18:12-146/12-147=84 20:11-147/11-147=89 "
        "22:11-147/11-148=93 24:11-148/10-148=97 26:10-148/10-148=102 28:10-149/10-149=106 30:9-149/9-149=111 "
        "32:9-149/9-150=115 34:9-150/8-150=119 36:8-150/8-150=124 38:8-151/8-151=128 40:8-151/7-151=133 "
        "42:7-151/7-152=137 44:7-152/7-152=141 46:6-152/6-152=146 48:6-153/6-153=150 50:6-153/5-153=155 "
        "52:5-153/5-154=159 54:5-154/5-154=163 56:4-154/4-155=168 58:4-155/4-155=172 60:4-155/3-155=177 "
        "62:3-156/3-156=181 64:3-156/3-156=185 66:3-156/2-157=190 68:2-157/2-157=194 70:2-157/2-157=199";
    std::istringstream in(table);
    std::string tok;
    int rows = 0, bad = 0;
    const auto row_ok = [&](int y, int first, int last, int grey) {
        for (int x = 0; x < p.w; ++x) {
            const int want = x >= first && x <= last ? grey : 0;
            if (p.at(x, y) != want) return false;
        }
        return true;
    };
    while (in >> tok) {
        int y = 0, f0 = 0, l0 = 0, f1 = 0, l1 = 0, v = 0;
        REQUIRE(std::sscanf(tok.c_str(), "%d:%d-%d/%d-%d=%d", &y, &f0, &l0, &f1, &l1, &v) == 6);
        if (!row_ok(y, f0, l0, v)) ++bad;
        if (!row_ok(y + 1, f1, l1, v)) ++bad;
        rows += 2;
    }
    CHECK(rows == 70);
    CHECK(bad == 0);
    for (const int y : {0, 1, 72, 73}) CHECK(row_ok(y, 1, 0, 0));
    for (int y = 74; y < 84; ++y) CHECK(row_ok(y, 1, 157, 128));
    // The column, rows 84 ... 125, x 61 ... 96 [ORACLE newgame-0002].
    const int col[36] = {45, 45, 67, 67, 90, 90, 113, 113, 135, 135, 158, 158, 181, 181, 203, 203, 226, 226,
                         226, 226, 203, 203, 181, 181, 158, 158, 135, 135, 113, 113, 90, 90, 67, 67, 45, 45};
    int col_bad = 0;
    for (int y = 84; y < 126; ++y)
        for (int x = 0; x < p.w; ++x) {
            const int want = x >= 61 && x <= 96 ? col[x - 61] : 0;
            if (p.at(x, y) != want) ++col_bad;
        }
    CHECK(col_bad == 0);
    // The brushes' greys in creation order: 36 bands, 128, then 45 ... 226.
    const std::vector<int> greys = pad_greys(g);
    // 128 is band 19's and 45, 67 and 181 are bands' too: 36 + 6.
    REQUIRE(greys.size() == 42);
    CHECK(greys.front() == 45);
    CHECK(greys[1] == 49);
    CHECK(greys[35] == 199);
    CHECK(greys[36] == 90);
    CHECK(greys[41] == 226);
}

TEST_CASE("pad: the mask's rows 0 and 1 are black and each lit row is one run") {
    for (const int H : {300, 480, 600, 683, 768, 900, 1200}) {
        const PadGeometry g = pad_geometry(H, H + 85);
        const std::vector<std::uint8_t> m = pad_mask(g);
        REQUIRE(m.size() == static_cast<std::size_t>(g.pw) * g.pc);
        int runs_bad = 0;
        for (int y = 0; y < g.pc; ++y) {
            int runs = 0;
            bool prev = false;
            for (int x = 0; x < g.pw; ++x) {
                const bool w = m[static_cast<std::size_t>(y) * g.pw + x] != 0;
                if (w && !prev) ++runs;
                prev = w;
            }
            if (y < 2 ? runs != 0 : runs != 1) ++runs_bad;
        }
        CHECK_MESSAGE(runs_bad == 0, "H " << H);
    }
}

namespace {

std::filesystem::path oracle_dir(const char* run) {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    std::filesystem::path base = v;
    if (!base.has_filename()) base = base.parent_path();
    const auto d = base.parent_path().parent_path() / "oracle" / run;
    return std::filesystem::is_directory(d) ? d : std::filesystem::path();
}

// The pad's grey levels against the oracle's 24-bpp pixels at (433, 642): the count of pixels
// whose (v, v, v) differs, outside the rectangle `skip` (screen coordinates, l t r b).
int pad_diff(const pngtest::Image& img, const PadPicture& p, int x0, int y0, const int (&skip)[4]) {
    int bad = 0;
    for (int y = 0; y < p.h; ++y)
        for (int x = 0; x < p.w; ++x) {
            const int sx = x0 + x, sy = y0 + y;
            if (sx >= skip[0] && sx < skip[2] && sy >= skip[1] && sy < skip[3]) continue;
            const std::uint32_t v = p.at(x, y);
            if (img.at(sx, sy) != ((v << 16) | (v << 8) | v)) ++bad;
        }
    return bad;
}

}  // namespace

TEST_CASE("pad golden: the desk's pad of newgame-0002 and the visit's of core-0003, 0 px") {
    const PadPicture p = pad_picture(pad_geometry(683, 768));
    struct Shot {
        const char* run;
        const char* file;
        int skip[4];  // the landed ship's square (phase 2's frame 1 at (446, 576), 132 x 132)
    };
    for (const Shot& s : {Shot{"newgame-0002-intro", "intro-04.png", {0, 0, 0, 0}},
                          Shot{"core-0003", "picture/landing-01-scene.png", {446, 576, 446 + 132, 576 + 132}}}) {
        const char* run = s.run;
        const char* file = s.file;
        const auto d = oracle_dir(run);
        if (d.empty() || !std::filesystem::exists(d / file)) {
            MESSAGE("skipped: work/oracle/" << std::string(run) << "/" << file << " not available");
            continue;
        }
        const pngtest::Image img = pngtest::read((d / file).string());
        REQUIRE(img.w == 1024);
        REQUIRE(img.h == 768);
        const int bad = pad_diff(img, p, 433, 642, s.skip);
        MESSAGE(std::string(run) << "/" << file << ": " << bad << " pad pixels differ outside the ship");
        CHECK(bad == 0);
    }
}
