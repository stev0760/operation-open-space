#include "hud/hud.h"

#include <string>

namespace is::hud {

namespace {

int w16(int v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }

constexpr int kTile = 0x3a;  // 58

}  // namespace

// ---- the border

int TileCounter::next(int reset) {
    if (reset != 0) counter_ = -1;  // seg11:0x1717-0x171d
    ++counter_;                     // seg11:0x1723
    if (counter_ > 6) counter_ = 0;  // seg11:0x1727-0x172e: jle
    static constexpr int kMap[7] = {0, 1, 2, 1, 0, 2, 1};  // seg11:0x175b
    return kMap[counter_];
}

int tile_offset(int w) {
    const int r = w16(w) % kTile;  // seg11:0xdf2-0xdf3: cwd; idiv 58
    return w16(kTile - r) / 2;     // seg11:0xdf5-0xdfc
}

std::vector<Tile> border_tiles(int w, int hud_top, TileCounter& counter) {
    std::vector<Tile> out;
    w = w16(w);
    const int off = tile_offset(w);  // [bp-0x42]
    int n = w / kTile;               // [bp-0x44], seg11:0xe08-0xe0b
    if (off != 0) ++n;               // seg11:0xe0e-0xe13
    counter.next(1);                 // seg11:0xe19: the result is discarded
    for (int i = 0; i < n + 1; ++i) {  // seg11:0xe3d-0xe77: i = 0 .. n
        const int t = counter.next(0);
        out.push_back({w16(i * kTile - off), 0, kTile, kTile, t});  // seg11:0xe68
    }
    const int rows = w16(hud_top) / kTile;  // [bp-0x44], seg11:0xe79-0xe82
    const int rest = w16(hud_top) % kTile;  // [bp-0x46], seg11:0xe85-0xe8b
    int i = 1;
    for (; i < rows; ++i) {  // seg11:0xe95-0xecf: i = 1 .. rows - 1
        const int t = counter.next(0);
        out.push_back({w16(-off), w16(i * kTile), kTile, kTile, t});  // seg11:0xec1
    }
    {
        const int t = counter.next(0);  // seg11:0xed1-0xefe: the partial tile at the last i
        out.push_back({w16(-off), w16(i * kTile), kTile, rest, t});
    }
    const int rx = w16(off + w - kTile);  // seg11:0xf18-0xf1f
    i = 1;
    for (; i < rows; ++i) {  // seg11:0xf0a-0xf49
        const int t = counter.next(0);
        out.push_back({rx, w16(i * kTile), kTile, kTile, t});  // seg11:0xf3b
    }
    {
        const int t = counter.next(0);  // seg11:0xf4b-0xf7d
        out.push_back({rx, w16(i * kTile), kTile, rest, t});
    }
    return out;
}

std::vector<Band> bevel_bands(int w, int hud_top) {
    std::vector<Band> out;
    const int off = tile_offset(w);
    // PALETTERGB(v, v, v): the 8-bit products of seg11:0xf9e-0xfb7 (mul cl on al).
    const auto grey = [](int v) {
        const Colour b = static_cast<Colour>(v & 0xff);
        return 0x02000000u | (b << 16) | (b << 8) | b;
    };
    for (int i = 0; i < 10; ++i)  // top, seg11:0xf9a-0x1013
        out.push_back({w16(2 * i + 0x3a - off), w16(2 * i + 0x3a),
                       w16(w + 2 * off - 0x74 - 4 * i), 2, grey((0xe - i) * 0xa)});
    for (int i = 0; i < 10; ++i)  // bottom, seg11:0x101a-0x109c
        out.push_back({w16(2 * i + 0x3a - off), w16(hud_top - 2 - 2 * i),
                       w16(w + 2 * off - 0x74 - 4 * i), 2, grey((0xc - i) * 0x14)});
    for (int i = 0; i < 10; ++i)  // left, seg11:0x10a5-0x111b
        out.push_back({w16(2 * i + 0x3a - off), w16(2 * i + 0x3a), 2,
                       w16(hud_top - 0x3a - 4 * i), grey((0xe - i) * 0xa)});
    for (int i = 0; i < 10; ++i)  // right, seg11:0x1122-0x11a2
        out.push_back({w16(w + off - 0x3c - 2 * i), w16(2 * i + 0x3a), 2,
                       w16(hud_top - 0x3a - 4 * i), grey((0xc - i) * 0x14)});
    return out;
}

// ---- the repaint

Hud::Hud(Canvas& canvas, Host& host, Children* children)
    : c_(canvas), h_(host), children_(children), panels_(canvas, host), info_(canvas, host) {}

void Hud::loaded() {
    loaded_ = true;            // DGROUP:0x500a = CreatePatternBrush(METAL) [BYTES seg11:0xe9]
    panels_.compose();         // seg11:0x1cf: seg11:0x344
    panels_.state().button = 1;  // seg11:0x1d2: DGROUP:0x93d8 = 1
}

bool Hud::gate() const {
    // seg11:0x2120 [BYTES seg11:0x212a-0x214a]
    if (!loaded_ || h_.demo_gate()) return false;
    const int s = h_.state();
    return s == 1 || s == 7;
}

void Hud::repaint() {
    h_.radar_centre();  // seg11:0xb0c-0xb1f: DGROUP:0x5050 = 43, DGROUP:0x5052 = 35 (M3 s2 fixer)
    h_.seed_rng();  // seg11:0xb22-0xb28: srand(GetCurrentTime())
    // seg11:0xb2e-0xb68: the DC and the palette are the surface's (ADR 0006, ADR 0007).
    if (gate()) {  // seg11:0xb6e-0xb73
        const shell::PlayArea a = h_.area();
        const bool race = h_.race();
        values_ = shell::hud_values(a.w, race);  // seg11:0xb77-0xc58
        layout_ = shell::hud_layout(a, shell::hud_inputs(values_));
        panels_.set_layout(values_, a.hud_top, a.w);
        // seg11:0xc5d-0xda4: the scrollbar (not in a race) and the two IS_INFO windows.
        if (children_) children_->layout(layout_, race);
        info_.layout(layout_, values_);
        // seg11:0xda7-0xdd8: the strip with the METAL pattern brush DGROUP:0x500a.
        c_.pat_blt(Img::kScreen, 0, a.hud_top, a.w, 0x55, kPatCopy, Brush::metal());
        if (h_.border()) border(a);  // seg11:0xddd-0xde2: DGROUP:0xa78
        // seg11:0x11a6-0x11ef: the HUD line in silver.
        const Point line[2] = {{0, a.hud_top}, {a.w, a.hud_top}};
        c_.lines(Img::kScreen, Pen::solid(0x00C0C0C0), line, 2);
        score_box(a);
        const shell::HudValues& v = values_;
        const int top = a.hud_top;
        // seg11:0x13ed-0x140a: the target display's frame.
        panels_.panel_frame(2, v.target_x, top, w16(v.target_w + v.target_x), w16(top + 0x54));
        // seg11:0x140d-0x1461: the fuel gauge background, then the level.
        c_.bit_blt(Img::kScreen, v.fuel_x, top, 0x1e, 0x55, Img::kFuelPanel, 0, 0, kSrcCopy);
        panels_.state().fuel_shown = -1;  // seg11:0x145a
        panels_.fuel();
        // seg11:0x1464-0x1480: the radar's frame.
        panels_.panel_frame(4, v.radar_x, top, w16(v.radar_x + 0x63), w16(top + 0x54));
        // seg11:0x1483-0x14ef: the damage panel, its lamps, and the red button.
        c_.bit_blt(Img::kScreen, v.damage_x, w16(top + 0x18), 0x54, 0x3d, Img::kDamage, 0, 0,
                   kSrcCopy);
        panels_.lamps(1);  // seg11:0x14d6
        PanelState& ps = panels_.state();
        ps.button_due = h_.now();                    // seg11:0x14da-0x14de: DGROUP:0x1e50
        ps.button_shown = ps.button == 0 ? 1 : 0;  // seg11:0x14e2-0x14eb
        panels_.button();                            // seg11:0x14ef
        // seg11:0x14f2-0x158f: the clock box, its colon from DIGITAL (100, 0) through a green
        // brush, and the digits.
        c_.frame(Img::kScreen, v.clock_x, w16(top + 1), 0x37, 0x11, 0);
        c_.bit_blt(Img::kScreen, w16(v.clock_x + 0x17), w16(top + 4), 10, 10, Img::kDigital,
                   0x64, 0, kMergeCopy, Brush::solid(0x0000FF00));
        panels_.clock(1, gate());
        // seg11:0x1593-0x15b0: the weapon display's frame.
        panels_.panel_frame(2, v.weapon_x, top, w16(v.weapon_w + v.weapon_x), w16(top + 0x54));
    }
    dispatch();  // seg11:0x15b3
}

void Hud::border(const shell::PlayArea& a) {
    // seg11:0xde6-0xf90: the tiles from graphic 45 (DGROUP:0x5044).
    for (const Tile& t : border_tiles(a.w, a.hud_top, tiles_))
        c_.bit_blt(Img::kScreen, t.x, t.y, t.w, t.h, Img::kTiles, t.t * kTile, 0, kSrcCopy);
    // seg11:0xf95-0x11a2: the bevel.
    for (const Band& b : bevel_bands(a.w, a.hud_top))
        c_.pat_blt(Img::kScreen, b.x, b.y, b.w, b.h, kPatCopy, Brush::solid(b.colour));
}

void Hud::score_box(const shell::PlayArea& a) {
    // seg11:0x11f4-0x1202: LoadString(2063) into a 6-byte buffer, at most 5 characters.
    std::string score = h_.lang(0x80f);
    if (score.size() > 5) score.resize(5);
    const shell::HudValues& v = values_;
    const int top = a.hud_top;
    int h;
    const auto letter = [&](int i) {
        return i < static_cast<int>(score.size()) ? score.substr(static_cast<std::size_t>(i), 1)
                                                  : std::string();
    };
    if (a.w < 0x294) {  // seg11:0x1211-0x1295
        h = 0x2d;
        c_.pat_blt(Img::kScreen, v.left, w16(top + 0xc), v.left_w, h, kBlackness, Brush::none());
        for (int i = 0; i < 5; ++i)
            panels_.small_text(0, 0x000000FF, w16(i * 0xe + v.left + 6), w16(top + 0x10),
                               letter(i));
    } else if (a.w < 0x2f8) {  // seg11:0x1297-0x1302: font 0, x = 18i + L
        h = 0x3c;
        c_.pat_blt(Img::kScreen, v.left, w16(top + 0xc), v.left_w, h, kBlackness, Brush::none());
        for (int i = 0; i < 5; ++i)
            c_.font_text(0, 0, 0x000000FF, w16(i * 0x12 + v.left), w16(top + 0x10), letter(i));
    } else {  // seg11:0x1304-0x1364: font 0, x = 22i + L + 25
        h = 0x3c;
        c_.pat_blt(Img::kScreen, v.left, w16(top + 0xc), v.left_w, h, kBlackness, Brush::none());
        for (int i = 0; i < 5; ++i)
            c_.font_text(0, 0, 0x000000FF, w16(i * 0x16 + v.left + 0x19), w16(top + 0x10),
                         letter(i));
    }
    panels_.state().score_shown = 0xffffffffu;  // seg11:0x1366
    panels_.score_digits();                       // seg11:0x1370
    // seg11:0x1373-0x13e8: the silver edge right and below the box.
    const Point edge[3] = {{w16(v.left_w + v.left), w16(top + 0xc)},
                           {w16(v.left_w + v.left), w16(h + top + 0xc)},
                           {v.left, w16(h + top + 0xc)}};
    c_.lines(Img::kScreen, Pen::solid(0x00C0C0C0), edge, 3);
}

void Hud::dispatch() {
    // seg11:0x15b3-0x16be: the table entry for states 1 and 7 is seg11:0x15b9. The other
    // states' entries are their modules' repaint hooks (shell::GameState::repaint).
    const int s = h_.state();
    if (s != 1 && s != 7) return;
    if (h_.alt_view()) return;  // seg11:0x15b9-0x15be: DGROUP:0x951a
    if (h_.demo_gate()) {       // seg11:0x15c2-0x15dd
        h_.demo_mark();
        return;
    }
    h_.pin_player();   // seg11:0x15e0: seg7:0x11cc
    h_.draw_player();  // seg11:0x15e5: seg7:0x1208
    h_.set_58c();      // seg11:0x15ea: DGROUP:0x58c = 1
    h_.draw_1f38();    // seg11:0x15f0: seg7:0x1f38
    h_.radar();        // seg11:0x15f6: seg11:0x33cc
}

bool Hud::lamp_repaint(int ship) {
    panels_.compose_lamps(ship);  // seg11:0x3cd0-0x3de4
    if (h_.alt_view()) {          // seg11:0x3de9-0x3e45
        const shell::PlayArea a = h_.area();
        c_.bit_blt(Img::kScreen, values_.damage_x, w16(a.hud_top + 0x18), 0x54, 0x3d,
                   Img::kDamage, 0, 0, kSrcCopy);
        panels_.lamps(1);
        return false;
    }
    repaint();  // seg11:0x3e48
    return true;
}

}  // namespace is::hud
