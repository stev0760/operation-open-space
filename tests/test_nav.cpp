// The Navigation window's portable rules (src/nav/nav_rules.h, src/nav/nav_draw.h) against
// docs/re/shell.md "Game-start dialogs" items 11 and 12, docs/re/nav.md "Drawing the tree",
// "Drawing the file rows", "Preview" and "Status line", and the oracle's window of newgame-0001
// and newgame-0002.
#include <filesystem>
#include <string>
#include <vector>

#include "assets/asset_store.h"
#include "doctest.h"
#include "fixture_dir.h"
#include "formats/icon_source.h"
#include "nav/nav_draw.h"
#include "nav/nav_rules.h"

using namespace is::nav;

TEST_CASE("nav: the defaults, the style and the key writes of seg6:0x2ebc") {
    const Placement p;
    CHECK(p.x == 30);
    CHECK(p.y == 30);
    CHECK(p.w == 560);
    CHECK(p.h == 350);
    CHECK(kStyle == 0x10cc0000ul);
    CHECK(kTitleString == 2140);
    // newgame-0001: a 560 x 350 window at 30,30 writes NavLeft=30 NavRight=560 NavTop=30
    // NavBottom=350, in this order
    const auto w = close_writes(30, 30, 590, 380);
    REQUIRE(w.size() == 4);
    CHECK(w[0] == std::make_pair(std::string("NavLeft"), 30));
    CHECK(w[1] == std::make_pair(std::string("NavRight"), 560));
    CHECK(w[2] == std::make_pair(std::string("NavTop"), 30));
    CHECK(w[3] == std::make_pair(std::string("NavBottom"), 350));
    // a moved and resized window keeps x, y, width and height
    const auto m = close_writes(100, 50, 800, 500);
    CHECK(m[1].second == 700);
    CHECK(m[3].second == 450);
}

TEST_CASE("nav: the children's ids, styles and layout") {
    // newgame-0001: a 552 x 316 client; Enter 447,5 90 x 26, Help 447,40, the directory list
    // 0,0 160 x 290, the file list 164,0 268 x 290
    CHECK(enter_box(552, 316) == Box{447, 5, 90, 26});
    CHECK(help_box(552, 316) == Box{447, 40, 90, 26});
    CHECK(dir_list_box(552, 316) == Box{0, 0, 160, 290});
    CHECK(file_list_box(552, 316) == Box{164, 0, 268, 290});
    // seg6:0x23d4's creation boxes, before the first WM_SIZE
    CHECK(enter_create_box(552, 316) == Box{462, 15, 90, 26});
    CHECK(help_create_box(552, 316) == Box{462, 51, 90, 26});
    CHECK(file_list_create_box(552, 316) == Box{164, 0, 288, 290});
    CHECK(kEnterId == 1);
    CHECK(kHelpId == 0x70);
    CHECK(kDirListId == 0x65);
    CHECK(kFileListId == 0x6b);
    CHECK(kDirListStyle == 0x50b30153ul);
    CHECK(kFileListStyle == 0x50a30b53ul);
    CHECK(kDirExtent == 200);
    CHECK(kFileColumn == 95);
    CHECK(kEnterString == 2143);
    CHECK(kHelpString == 2144);
}

TEST_CASE("nav: drive roots and Enter's rule seg6:0x301a") {
    CHECK(drive_root(0) == "C:\\");
    CHECK(drive_root(1) == "D:\\");  // DGROUP:0xd18 for the fixture drive
    CHECK(enter_action(false, 0, 0) == EnterAction::kRefused);
    CHECK(enter_action(true, 0, 0) == EnterAction::kRequestWave);  // a new game
    CHECK(enter_action(true, 5, 1) == EnterAction::kRequestWave);  // a restored game
    CHECK(enter_action(true, 5, 0) == EnterAction::kArmDebriefing);
}

TEST_CASE("nav draw: the tree rows of seg6:0x49c0") {
    // row 0: the space goes, I = 0, the green folder with '+' (sub 1, state 2: 15 + 60), no lines
    TreeRowLook r = tree_row(" D:\\112", 0, 10, "DEEP110", 0, 0, 17);
    CHECK(r.indent == 0);
    CHECK(r.glyph_x == 75);
    CHECK(r.text == "d:\\");
    CHECK(r.text_x == 18);
    CHECK(r.glyph_left == 1);
    CHECK(r.glyph_top == 1);
    CHECK(r.lines.empty());
    // a top-level row with subdirectories: yellow with '+', I = 10, the trunk and the depth line
    // coincide at x 4, the tick to x 9
    r = tree_row("DEEP110", 1, 10, "DEMON'S GATE103", 0, 17, 34);
    CHECK(r.depth == 1);
    CHECK(r.indent == 10);
    CHECK(r.glyph_x == 15);
    CHECK(r.text == "deep");
    CHECK(r.text_x == 28);
    CHECK(r.glyph_left == 11);
    CHECK(r.highlight_left == 10);
    CHECK(r.lines == std::vector<Segment>{{4, 17, 4, 34}, {4, 17, 4, 34}, {4, 23, 9, 23}});
    // locked built-in rows are red (105), lower case
    r = tree_row("DEMON'S GATE103", 2, 10, "x", 0, 34, 51);
    CHECK(r.glyph_x == 105);
    CHECK(r.text == "demon's gate");
    r = tree_row("Lazy Loop - RACE100", 7, 10, "x", 0, 0, 17);
    CHECK(r.glyph_x == 0);
    CHECK(r.text == "lazy loop - race");
    // visited (30), visited with '+' (45), complete (60), red with '+' (120)
    CHECK(tree_row("GAMES101", 3, 10, "", 0, 0, 17).glyph_x == 30);
    CHECK(tree_row("GAMES111", 3, 10, "", 0, 0, 17).glyph_x == 45);
    CHECK(tree_row("GAMES102", 3, 10, "", 0, 0, 17).glyph_x == 60);
    CHECK(tree_row("GAMES113", 3, 10, "", 0, 0, 17).glyph_x == 120);
    // expanded (sub 2) adds nothing
    CHECK(tree_row("DEEP120", 1, 11, "A210", 0, 0, 17).glyph_x == 0);
    // the last row: the trunk and the depth line stop at top + 6
    r = tree_row("TOOLS100", 9, 10, "", 0, 100, 117);
    CHECK(r.lines == std::vector<Segment>{{4, 100, 4, 106}, {4, 100, 4, 106}, {4, 106, 9, 106}});
    // depth 2 before a deeper row: the depth line x = 14 * 2 - 10 = 18 stops at top + 6
    r = tree_row("A220", 2, 12, "B300", 0, 34, 51);
    CHECK(r.indent == 26);
    CHECK(r.text_x == 44);
    CHECK(r.lines == std::vector<Segment>{{4, 34, 4, 51}, {18, 34, 18, 40}, {18, 40, 23, 40}});
    // depth 2 before a row of the same depth: the depth line runs to the bottom
    r = tree_row("A210", 2, 12, "C200", 0, 34, 51);
    CHECK(r.lines[1] == Segment{18, 34, 18, 51});
    // depth 3, x = 32
    r = tree_row("B300", 3, 12, "DEMON'S GATE103", 0, 51, 68);
    CHECK(r.indent == 42);
    CHECK(r.lines == std::vector<Segment>{{4, 51, 4, 68}, {32, 51, 32, 57}, {32, 57, 37, 57}});
    // the rectangle's left shifts every position
    r = tree_row("GAMES100", 5, 10, "", 3, 0, 17);
    CHECK(r.glyph_left == 14);
    CHECK(r.text_x == 31);
    CHECK(r.lines[0] == Segment{7, 0, 7, 17});
    CHECK(focus_indent(" D:\\112", 0) == 0);
    CHECK(focus_indent("A220", 2) == 26);
}

TEST_CASE("nav draw: the file rows of seg6:0x47e8 and seg6:0x5cbc") {
    FileRowLook f = file_row("ALPHA.EXE 0", 0, 0);
    CHECK(f.text == "alpha.exe");
    CHECK(f.digit == 0);
    CHECK_FALSE(f.glyph_colour.has_value());
    CHECK(f.glyph_left == 1);
    CHECK(f.glyph_top == 1);
    CHECK(f.text_x == 15);
    f = file_row(" ECHO.EXE 1", 95, 17);
    CHECK(f.text == "echo.exe");  // the leading space goes
    CHECK(f.glyph_colour == 0x00FF0000u);
    CHECK(f.glyph_left == 96);
    CHECK(f.text_x == 110);
    CHECK(f.text_y == 17);
    CHECK(file_row("X.DLL 2", 0, 0).glyph_colour == 0x000000FFu);
}

TEST_CASE("nav draw: the status band, the preview frame and seg11:0x1774") {
    // the band of a 552 x 316 client: bevel(-1, 0, 291, 552, 316), then the cased text at
    // (5, 296) with its white copy at (7, 298)
    const std::vector<Op> v = status_band(552, 316, "THERE ARE 3 ICONS IN 3 FILES.YOU HAVE NOT BEEN HERE BEFORE. ");
    const std::vector<Op> b = bevel(-1, 0, 291, 552, 316);
    REQUIRE(v.size() == b.size() + 2);
    CHECK(v[0].kind == Op::Kind::kLine);
    CHECK(v[0].x0 == 0);
    CHECK(v[0].y0 == 316);
    CHECK(v[0].x1 == 552);
    const Op& shadow = v[v.size() - 2];
    const Op& text = v.back();
    CHECK(shadow.kind == Op::Kind::kText);
    CHECK(shadow.colour == 0x02FFFFFFu);
    CHECK(shadow.x0 == 7);
    CHECK(shadow.y0 == 298);
    CHECK(text.colour == 0x025A5A5Au);
    CHECK(text.x0 == 5);
    CHECK(text.y0 == 296);
    CHECK(text.face == "Arial");
    CHECK(text.height == 14);
    CHECK(text.weight == 600);
    CHECK_FALSE(text.opaque);
    // newgame-0001's text, seg11:0x3fd4 mode 3
    CHECK(text.text == "There are 3 icons in 3 files.You have not been here before. ");
    // the filled bevel: two LTGRAY rectangles
    int rects = 0;
    for (const Op& o : b)
        if (o.kind == Op::Kind::kRect) {
            ++rects;
            CHECK(o.filled);
        }
    CHECK(rects == 2);
    // the preview frame: bevel(4, W - 96, 85, W - 24, 157), four empty rectangles and a black
    // 64 x 64 at (W - 92, 89)
    const std::vector<Op> fr = preview_frame(552);
    CHECK(fr.back().kind == Op::Kind::kFill);
    CHECK(fr.back().x0 == 460);
    CHECK(fr.back().y0 == 89);
    CHECK(fr.back().x1 == 64);
    CHECK(fr.back().y1 == 64);
    CHECK(fr.back().colour == 0u);
    rects = 0;
    for (const Op& o : fr)
        if (o.kind == Op::Kind::kRect) {
            ++rects;
            CHECK_FALSE(o.filled);
        }
    CHECK(rects == 4);
    CHECK(preview_rect(552) == Rect{460, 89, 64, 64});
    // seg9:0x8bec at height 13 draws 14 without the white copy
    CHECK(label(0, 0, 13, 0, kTaLeft, "x").size() == 1);
    CHECK(label(0, 0, 13, 0, kTaLeft, "x")[0].height == 14);
}

TEST_CASE("nav draw: the program-name placeholder seg9:0x352") {
    // Navigation's call (0, 0, 32, 0, name) for drive D:
    const std::vector<Op> v = placeholder(0, 0, 32, 0, "ALPHA.EXE", 1);
    REQUIRE(v.size() > 4);
    CHECK(v[0].kind == Op::Kind::kFill);  // the cell blacked
    CHECK(v[0].x1 == 32);
    CHECK(v[0].y1 == 32);
    CHECK(v[0].colour == 0u);
    // the frame: white left and top, grey bottom and right, black corners
    CHECK(v[1].kind == Op::Kind::kLine);
    CHECK(v[1].colour == 0x00FFFFFFu);
    CHECK(v[1].y0 == 20);
    CHECK(v[3].colour == 0x00808080u);
    CHECK(v[3].x1 == 31);
    std::vector<const Op*> texts;
    for (const Op& o : v)
        if (o.kind == Op::Kind::kText) texts.push_back(&o);
    REQUIRE(texts.size() == 2);
    // "C:>" + DGROUP:0x94ac in green Arial 10 at (2, 2)
    CHECK(texts[0]->text == "D:>");
    CHECK(texts[0]->face == "Arial");
    CHECK(texts[0]->height == 10);
    CHECK(texts[0]->colour == 0x0000FF00u);
    CHECK(texts[0]->x0 == 2);
    CHECK(texts[0]->y0 == 2);
    CHECK(texts[0]->opaque);
    // the name without its extension, Small Fonts 7, white, centred at (16, 26)
    CHECK(texts[1]->text == "ALPHA");
    CHECK(texts[1]->face == "Small Fonts");
    CHECK(texts[1]->height == 7);
    CHECK(texts[1]->colour == 0x00FFFFFFu);
    CHECK(texts[1]->align == kTaCenter);
    CHECK(texts[1]->x0 == 16);
    CHECK(texts[1]->y0 == 26);
    // the side strips are y wide: none for y = 0
    CHECK(v.back().kind == Op::Kind::kFill);
    CHECK(v.back().x1 == 0);
    // a name of four characters or with no '.' at length - 4 keeps its text
    const auto name_of = [](const std::vector<Op>& ops) { return ops[ops.size() - 3].text; };
    CHECK(name_of(placeholder(0, 0, 32, 0, ".EXE", 0)) == ".EXE");
    CHECK(name_of(placeholder(0, 0, 32, 0, "README", 0)) == "README");
    // the wave strip's call (38 * slot + 6, flag, 32, 6): the strips black 6 px either side
    const std::vector<Op> w = placeholder(44, 0x20, 32, 6, "B.EXE", 0);
    CHECK(w[w.size() - 2].x0 == 38);
    CHECK(w[w.size() - 2].x1 == 6);
    CHECK(w.back().x0 == 76);
    // flag != 0: the grey box with the blue bar, no prompt
    int boxes = 0;
    for (const Op& o : w) {
        CHECK_FALSE(o.text == "C:>");
        if (o.kind == Op::Kind::kFill && o.colour == 0x00800000u) ++boxes;
    }
    CHECK(boxes == 1);
}

TEST_CASE("nav draw: the built-in preview cell from a GROUP_ICON") {
    is::assets::IconGroup g;
    CHECK_FALSE(group_cell(g).has_value());
    const auto image = [](int w, int h, std::uint32_t rgb) {
        is::fmt::Icon ic;
        ic.image.width = w;
        ic.image.height = h;
        ic.image.bpp = 24;
        ic.image.bottom_up = false;
        ic.image.bits.assign(ic.image.stride() * static_cast<std::size_t>(h), 0);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const std::size_t at = static_cast<std::size_t>(y) * ic.image.stride() + static_cast<std::size_t>(x) * 3;
                ic.image.bits[at] = static_cast<std::uint8_t>(rgb);
                ic.image.bits[at + 1] = static_cast<std::uint8_t>(rgb >> 8);
                ic.image.bits[at + 2] = static_cast<std::uint8_t>(rgb >> 16);
            }
        return ic;
    };
    g.images.push_back(image(16, 16, 0x112233));
    auto c = group_cell(g);
    REQUIRE(c.has_value());
    CHECK(c->at(0, 0) == 0xFF112233u);  // scaled from 16 x 16
    CHECK(c->at(31, 31) == 0xFF112233u);
    g.images.push_back(image(32, 32, 0x445566));
    c = group_cell(g);
    REQUIRE(c.has_value());
    CHECK(c->at(5, 7) == 0xFF445566u);  // the 32 x 32 member wins
}

TEST_CASE("nav draw: IconReader caches each module and counts as ExtractIcon") {
    int calls = 0;
    IconReader r([&](const std::string&) {
        ++calls;
        return is::fmt::IconSource(std::vector<std::uint8_t>{'M', 'Z'});
    });
    CHECK(r.count("D:\\GAMES\\ALPHA.EXE") == 0);  // not a module: ExtractIcon(-1) = 0
    CHECK_FALSE(r.has_icon("d:\\games\\alpha.exe"));
    CHECK_FALSE(r.cell("D:\\GAMES\\ALPHA.EXE").has_value());
    CHECK(calls == 1);
    for (int i = 0; i < 9; ++i) r.count("D:\\F" + std::to_string(i));
    CHECK(calls == 10);
    r.count("D:\\GAMES\\ALPHA.EXE");  // dropped after eight newer modules
    CHECK(calls == 11);
    CHECK(r.loads() == 11);
}

TEST_CASE("nav draw: IconReader on the fixture drive's programs") {
    const std::filesystem::path dir = fixture_dir();
    if (dir.empty()) return;  // oracle/fixtures/make_fixtures.py basic is not built
    IconReader r([&](const std::string& dos) {
        // "D:\GAMES\ALPHA.EXE" -> <fixture>/GAMES/ALPHA.EXE
        std::string rel = dos.substr(3);
        for (char& ch : rel)
            if (ch == '\\') ch = '/';
        return is::fmt::IconSource::load(dir / rel);
    });
    CHECK(r.count("D:\\GAMES\\ALPHA.EXE") == 1);
    CHECK(r.has_icon("D:\\GAMES\\ALPHA.EXE"));
    CHECK(r.count("D:\\TOOLS\\ECHO.EXE") == 3);
    CHECK(r.count("D:\\GAMES\\NOFILE.EXE") == 0);
    CHECK_FALSE(r.has_icon("D:\\GAMES\\NOFILE.EXE"));
    // ALPHA's icon is a red square on black (newgame-0001's preview)
    const auto c = r.cell("D:\\GAMES\\ALPHA.EXE");
    REQUIRE(c.has_value());
    bool red = false;
    for (std::uint32_t px : c->bgra)
        if ((px & 0xFFFFFFu) == 0xFF0000u) red = true;
    CHECK(red);
}
