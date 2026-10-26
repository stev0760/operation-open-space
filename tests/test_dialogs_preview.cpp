// The LevClass pictures (src/dialogs/preview.h) against docs/re/shell.md "Game-start dialogs"
// (Ship previews, Logo): the frame's pixels on the 96 × 96 client, the slot paint, the ship
// bitmap of TRANSFORMER and the Order logo. The goldens read IS_ORIGINAL_DIR and skip without it.
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>

#include "assets/asset_store.h"
#include "dialogs/preview.h"
#include "doctest.h"
#include "gfx/palette.h"

using namespace is::dialogs;

namespace {

std::filesystem::path original_dir() {
    const char* v = std::getenv("IS_ORIGINAL_DIR");
    if (!v || !*v || !std::filesystem::is_directory(v)) return {};
    return v;
}

}  // namespace

TEST_CASE("dialogs preview: the LevClass frame on the 96 x 96 client (ISPACE.DLL seg10:0x309c)") {
    const Image f = lev_frame(96, 96);
    // Column 0 rows 0-94 and row 0 columns 0-94 end white; column 95 and row 95 black.
    for (int i = 0; i <= 94; ++i) {
        CHECK(f.at(0, i) == 0xFFFFFF);
        CHECK(f.at(i, 0) == 0xFFFFFF);
    }
    for (int i = 0; i < 96; ++i) {
        CHECK(f.at(95, i) == 0);
        CHECK(f.at(i, 95) == 0);
    }
    // The light strips (0, 0, 3, 96) and (0, 0, 96, 3) inside the white edge.
    CHECK(f.at(1, 50) == kLtGray);
    CHECK(f.at(2, 50) == kLtGray);
    CHECK(f.at(50, 1) == kLtGray);
    CHECK(f.at(50, 2) == kLtGray);
    // The dark strips (2, 93, 96, 96) and (93, 1, 96, 95): 0x404040.
    CHECK(f.at(50, 93) == kDkGray);
    CHECK(f.at(50, 94) == kDkGray);
    CHECK(f.at(93, 50) == kDkGray);
    CHECK(f.at(94, 50) == kDkGray);
    // The corner pixels (94 - i, i), (95 - i, i), (2 - i, 93 + i), (3 - i, 93 + i), where the
    // lines leave them.
    CHECK(f.at(93, 1) == kLtGray);
    CHECK(f.at(94, 1) == kGray);
    CHECK(f.at(92, 2) == kLtGray);
    CHECK(f.at(93, 2) == kGray);
    CHECK(f.at(2, 93) == kLtGray);
    CHECK(f.at(1, 94) == kLtGray);
    CHECK(f.at(3, 93) == 0);
    CHECK(f.at(2, 94) == 0);
    // The inside stays the class background.
    CHECK(f.at(3, 3) == 0);
    CHECK(f.at(48, 48) == 0);
    const Image g = lev_frame(96, 96, 0x123456);
    CHECK(g.at(48, 48) == 0x123456);
}

TEST_CASE("dialogs preview: seg10:0x2c68 blacks (3, 3, 90, 90) and doubles the w x w cell") {
    Image client = lev_frame(96, 96, 0x111111);
    ShipBitmap bm;
    bm.w = 2;
    bm.image = Image(8, 2, 0);
    bm.image.set(0, 0, 0xFF0000);
    bm.image.set(1, 1, 0x00FF00);
    bm.image.set(2, 0, 0x0000FF);  // outside the w × w cell
    paint_slot(client, &bm);
    CHECK(client.at(3, 3) == 0);
    CHECK(client.at(92, 92) == 0);
    CHECK(client.at(46, 46) == 0xFF0000);
    CHECK(client.at(47, 47) == 0xFF0000);
    CHECK(client.at(48, 48) == 0x00FF00);
    CHECK(client.at(49, 49) == 0x00FF00);
    CHECK(client.at(50, 46) == 0);
    CHECK(client.at(0, 0) == 0xFFFFFF);  // the frame stays
    Image empty = lev_frame(96, 96, 0x111111);
    paint_slot(empty, nullptr);
    CHECK(empty.at(48, 48) == 0);
}

TEST_CASE("dialogs preview: the flags of seg10:0x2aac black part of the bitmap") {
    is::fmt::Dib d;
    d.width = 8;
    d.height = 4;
    d.bpp = 8;
    d.palette = {0, 0xFFFFFF};
    d.bits.assign(d.stride() * 4, 1);
    // d = trunc((4 - 2)/2) = 1: flags 2 blacks the column 3, flags 1 the column 0.
    const ShipBitmap a = ship_bitmap(d, 100, 12, 12, 2, 2);
    CHECK(a.w == 4);
    CHECK(a.image.at(3, 0) == 0);
    CHECK(a.image.at(2, 0) != 0);
    CHECK(a.image.at(0, 0) != 0);
    const ShipBitmap b = ship_bitmap(d, 100, 12, 12, 2, 3);
    CHECK(b.image.at(3, 1) == 0);
    CHECK(b.image.at(0, 1) == 0);
    CHECK(b.image.at(1, 1) != 0);
}

TEST_CASE("dialogs preview: TRANSFORMER's preview (golden)") {
    const auto dir = original_dir();
    if (dir.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    is::assets::AssetStore store(dir);
    const is::assets::Ships ships = store.ships(dir);
    const is::fmt::ShipRecord* r = ships.find("TRANSFORMER");
    REQUIRE(r);
    CHECK(r->byte_07 == 0);
    CHECK(r->byte_08 == 10);
    const is::fmt::BodyRecord* b = ships.body(*r);
    REQUIRE(b);
    CHECK(b->bitmap_id == 60);
    const is::fmt::Dib body = store.sprite(b->bitmap_id);
    CHECK(body.width == 132);
    CHECK(body.height == 33);
    const ShipBitmap bm = ship_bitmap(body, b->bitmap_id, r->byte_07, r->byte_08, b->byte_1a, 0);
    CHECK(bm.w == 33);
    // Every colour of the cell is black or from DGROUP:0x117c row 0 or DGROUP:0x1cdc row 10.
    std::set<std::uint32_t> allowed{0};
    for (const auto& c : is::gfx::kSchemes10[0]) allowed.insert(c.rgb());
    for (const auto& c : is::gfx::kSchemes5[10]) allowed.insert(c.rgb());
    int coloured = 0, outside = 0;
    for (int y = 0; y < 33; ++y)
        for (int x = 0; x < 33; ++x) {
            const std::uint32_t c = bm.image.at(x, y);
            if (c) ++coloured;
            if (!allowed.count(c)) ++outside;
        }
    CHECK(coloured > 100);
    CHECK(outside == 0);
    Image client = lev_frame(96, 96);
    paint_slot(client, &bm);
    // The doubled cell sits at (15, 15) - (81, 81).
    CHECK(client.at(15 + 2 * 16, 15 + 2 * 16) == bm.image.at(16, 16));
}

TEST_CASE("dialogs preview: the Order logo, ISPACE.DLL #4000 300 (golden)") {
    const auto dir = original_dir();
    if (dir.empty()) {
        MESSAGE("skipped: IS_ORIGINAL_DIR not available");
        return;
    }
    is::assets::AssetStore store(dir);
    const Logo lg = order_logo(store.sprite(300, is::assets::kGameDll));
    CHECK(lg.w == 138);
    CHECK(lg.h == 138);
    // Rows 106-137 are never drawn and hold no non-black pixel.
    int lower = 0;
    for (int y = 106; y < 138; ++y)
        for (int x = 0; x < 138; ++x) lower += lg.image.at(x, y) != 0;
    CHECK(lower == 0);
    // The patch: (125, 29, 2, 2) black; the mask is white only where the image is black.
    CHECK_FALSE(lg.white(125, 29));
    CHECK_FALSE(lg.white(126, 30));
    CHECK_FALSE(lg.white(35, 47));
    CHECK_FALSE(lg.white(105, 48));
    int bad = 0;
    for (int y = 0; y < 138; ++y)
        for (int x = 0; x < 138; ++x)
            if (lg.white(x, y) && lg.image.at(x, y) != 0) ++bad;
    CHECK(bad == 0);
    // Painting keeps the background where the mask is white and shows the image elsewhere.
    Image client(146, 114, 0xC0C0C0);
    paint_logo(client, lg);
    CHECK(client.at(0, 0) == 0xC0C0C0);
    CHECK(client.at(4 + 125, 4 + 29) == lg.image.at(125, 29));
    int shown = 0;
    for (int y = 0; y < kLogoH; ++y)
        for (int x = 0; x < kLogoW; ++x)
            if (lg.image.at(x, y)) shown += client.at(4 + x, 4 + y) == lg.image.at(x, y);
    MESSAGE("logo pixels shown: " << shown);
    CHECK(shown == 5388);
}
