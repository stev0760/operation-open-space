// The watch windows' portable logic (watch.h). The host build's program entry points are here
// too: without Win32 there is no window, so they do nothing.
#include "hud/watch.h"

namespace is::hud {

namespace {
constexpr Colour kTitleFriend = 0x02FFB900, kTitleFoe = 0x02006EFF, kTitleOther = 0x02AAAAAA;  // [BYTES seg13:0x5337 seg13:0x5356 seg13:0x5361]
constexpr Colour kTitleDefender = 0x00008000;  // [BYTES seg13:0x536c-0x536e]
constexpr Colour kLineColour = 0x0000FFFF, kLineMono = 0x00FFFFFF;  // [BYTES seg13:0x53c1-0x53ce]
constexpr int kTextX = 0x30;  // [BYTES seg13:0x523e seg13:0x53a4]
}  // namespace

int watch_key_563c(WatchHost& h, bool ctrl, bool alt) {
    Mark& m = h.mark();
    const int ship_pool = h.ship_pool(), P = h.player();
    if (!h.window() && !ctrl && !alt) {  // seg13:0x564b-0x5670
        m.pool = ship_pool;
        m.rec = 0;
        m.ox = m.oy = 2;
    }
    if (ctrl && !alt && h.window()) {  // seg13:0x5673-0x568c
        h.destroy();
        return 0;
    }
    int p = h.player_target_pool(), r = h.player_target_rec();  // seg13:0x568f-0x56a1
    if (alt && p != 2) {  // seg13:0x56a5-0x56f8: the player's first defender in play
        for (int k = 0; k < h.defenders(); ++k) {
            const DefenderWords d = h.defender(k);
            if (d.owner_pool != ship_pool || d.owner_rec != P) continue;
            if (h.object_state(2, k) != 0) continue;  // far DGROUP:0x1e00 + 0x70k, +0x18
            p = 2;
            r = k;
            break;
        }
    }
    int f = 0;  // [bp+6], the Ctrl word reused
    if (ctrl || alt || !h.window()) {  // seg13:0x56fc-0x574c
        if (ctrl && alt) {  // watch yourself
            p = ship_pool;
            r = P;
        }
        if (p == ship_pool || p == 2) {
            m.pool = p;
            m.rec = r;
            f = 1;
        }
    }
    if (f == 0) {  // seg13:0x5751-0x576d
        h.next_mark();
        if (m.pool != ship_pool && m.pool != 2) return 0;
    }
    if (!h.window()) {  // seg13:0x5771-0x57a1
        h.post_create();
    } else {
        h.post_paint();
        h.set_title(watch_title(h));  // seg13:0x57ac
    }
    return 1;
}

std::string watch_title(WatchHost& h) {
    const Mark& m = h.mark();
    std::string t = case_3fd4(h.name(0, m.pool, m.rec, 0), 2);  // seg13:0x57ba-0x57ed
    if (m.pool == h.ship_pool())                                // seg13:0x57f2-0x5835
        t += " - " + case_3fd4(h.pilot_name(m.rec), 2);         // DGROUP:0x137c
    return t;
}

void draw_cell(Canvas& c, WatchHost& h, const CellPlan& p) {
    if (p.clear) c.pat_blt(Img::kScreen, p.clear_x, p.clear_y, p.clear_size, p.clear_size, kBlackness, Brush::none());
    if (p.picture && p.slot == 0xd)
        c.stretch_blt(Img::kScreen, p.dx, p.dy, p.dw, p.dh, Img::kIconStrip, p.sx, p.sy, p.sw, p.sh, kSrcCopy);
    if (p.picture && p.ship) {  // the ship's sheet, slot +0x50 + 0x28, and the cloak's checkerboard
        c.stretch_blt(Img::kScreen, p.dx, p.dy, p.dw, p.dh, Img::kShipSprite, p.sx, p.sy, p.sw, p.sh, kSrcCopy);
        if (p.cloak) c.pat_blt(Img::kScreen, p.dx, p.dy, p.dw, p.dh, CellPlan::kCloakRop, Brush::checker());
    }
    if (p.picture && p.slot >= 14 && p.slot < 22)  // a burning icon's own image, slot 14 + r
        c.stretch_blt(Img::kScreen, p.dx, p.dy, p.dw, p.dh, Img::kIconOwn, p.sx, p.sy, p.sw, p.sh, kSrcCopy);
    if (p.sub_cells) {
        for (const CellPlan::SubCell& q : p.subs) {
            if (q.draw && h.fragment(q.frag))
                c.stretch_blt(Img::kScreen, q.x, q.y, q.side, q.side, Img::kFragment, p.frag_sx, p.frag_sx, p.frag_size,
                              p.frag_size, kSrcCopy);
            else if (!q.draw)
                c.pat_blt(Img::kScreen, q.x, q.y, q.side, q.side, kBlackness, Brush::none());
        }
    }
    if (p.special && h.gate_picture())
        c.stretch_blt(Img::kScreen, p.dx, p.dy, p.dw, p.dh, Img::kGatePicture, p.sx, p.sy, p.sw, p.sh, kSrcCopy);
    if (p.picture && p.slot == 5 && c.has(Img::kLogoStrip))
        c.stretch_blt(Img::kScreen, p.dx, p.dy, p.dw, p.dh, Img::kLogoStrip, p.sx, p.sy, p.sw, p.sh, kSrcCopy);
}

namespace {
// seg13:0x5a6a(hdc, x, y, pool, rec, mark, flag, scale) with the mark's glint counter.
void cell_at(WatchHost& h, Canvas& c, int x, int y, int pool, int rec, int flag, int scale, int& glint) {
    CellInputs in;
    if (!h.cell(pool, rec, in)) return;  // no such record in the port's pools [DESIGN]
    in.x = x;
    in.y = y;
    in.pool = pool;
    in.rec = rec;
    in.flag = flag;
    in.scale = scale;
    draw_cell(c, h, plan_cell(in, glint));
}
}  // namespace

void watch_paint(WatchHost& h, WatchState& ws, Canvas& c, int mode) {
    if (!h.window()) return;  // seg13:0x50be-0x50c3
    Mark& m = h.mark();
    // The mark is latched here: a move to the next ship below leaves this call drawing the old
    // one (pitfalls: a value latched at entry) [BYTES seg13:0x50c7-0x50d6].
    const int p = m.pool, r = m.rec;
    const int ship_pool = h.ship_pool(), P = h.player();
    const bool visible = (p == ship_pool && r == P) || h.target_ok(p, r);  // seg13:0x50d9-0x50f8
    if (!visible) {  // seg13:0x50fb-0x5114
        if (watch_key_563c(h, false, false) == 0) {
            h.destroy();
            return;
        }
    }
    // The cell at DGROUP:0x6e8a / 0x6e8c, mark 2, flag `mode`, scale 0 [BYTES seg13:0x514b-0x5162].
    cell_at(h, c, m.ox, m.oy, p, r, mode, 0, m.glint);
    // The text, every 500 ms or when line 1 is empty [BYTES seg13:0x5165-0x5220].
    const std::uint32_t now = h.now();
    bool changed = false;
    if (!(ws.text_due > now) || h.line1().empty()) {
        const std::string old1 = h.line1(), old2 = h.line2();
        if (p == ship_pool)
            h.activity(r == P ? 0x64 : static_cast<std::int8_t>(P), r);  // `mov al, [0x9646]; cbw`
        else
            h.defender_activity(r);
        changed = old1 != h.line1() || old2 != h.line2();
    }
    if (!(ws.text_due > now)) ws.text_due = now + 500;  // `add ax, 0x1f4; adc dx, 0`
    if (!changed) return;
    // [BYTES seg13:0x522c-0x542a]
    c.pat_blt(Img::kScreen, kTextX, 0, h.client_width(), 0x30, kBlackness, Brush::none());
    std::string title;
    Colour colour;
    if (p == ship_pool) {
        const int v = h.attitude(r, P);  // r's view of the player [BYTES seg13:0x5289-0x5297]
        if (r == P) {
            title = h.lang(0x7f9);  // 2041 "REALITY CHECK"
        } else {
            title = h.team(P) == h.team(r) ? h.lang(0x7fa) : h.team_name(h.team(r));  // 2042 "TEAMMATE"
            title += " - " + h.attitude_words(v);  // DGROUP:0x1378, DGROUP:0x93e0
        }
        colour = v > 0x50 ? kTitleFriend : v < -0x50 ? kTitleFoe : kTitleOther;
    } else {
        // 2531 "%D %D" with (2, r, code 0) and (the owner, code 6) [BYTES seg13:0x537e-0x5399].
        colour = kTitleDefender;
        const DefenderWords d = h.defender(r);
        const auto lang = [&h](int id) { return h.lang(id); };
        title = briefing::lang_format(
            h.lang(0x9e3),
            {briefing::FArg(h.name(1, p, r, 0)), briefing::FArg(0), briefing::FArg(0),
             briefing::FArg(h.name(0, d.owner_pool, d.owner_rec, 6)), briefing::FArg(0), briefing::FArg(0)},
            lang);
    }
    c.text_out(Img::kScreen, kTextX, 0, kTaLeft, colour, title);  // seg13:0x53b5
    const Colour lc = h.monochrome() ? kLineMono : kLineColour;
    c.text_out(Img::kScreen, kTextX, 0x10, kTaLeft, lc, h.line1());  // seg13:0x53ea
    c.text_out(Img::kScreen, kTextX, 0x1e, kTaLeft, lc, h.line2());  // seg13:0x5404
}

int smart_rows(WatchHost& h, WatchState& ws, Canvas* c, int mode, int i) {
    const int ship_pool = h.ship_pool(), P = h.player();
    int n = 0;  // [bp-0xa] [BYTES seg13:0x649a-0x64e6]
    for (int k = 0; k <= h.last_ship(); ++k) {
        if (k == P || !h.target_ok(ship_pool, k)) continue;
        if (n < static_cast<int>(ws.row.size())) {
            WatchState::Row& w = ws.row[static_cast<std::size_t>(n)];
            w.rec = k;
            w.x = 2;
            w.y = 0x2e * n + 2;
        }
        ++n;
    }
    if (n != ws.rows) {  // seg13:0x64e8-0x652e
        ws.rows = n;
        return n == 0 ? 1 : 2;
    }
    if (!c) return 0;
    const auto row_glint = [&](int k) -> int& {
        static int scratch = 0;
        return k >= 0 && k < static_cast<int>(ws.row.size()) ? ws.row[static_cast<std::size_t>(k)].glint
                                                               : (scratch = 0);
    };
    if (i == -1) {  // seg13:0x6531-0x6577: every row
        for (int k = 0; k < ws.rows && k < static_cast<int>(ws.row.size()); ++k)
            cell_at(h, *c, 0, 0x2e * k + 4, ship_pool, ws.row[static_cast<std::size_t>(k)].rec, mode, ws.half,
                    row_glint(k));
        return 0;
    }
    int k = 0;  // seg13:0x6579-0x65bb: ship i's row, or row n when it has none
    while (k < ws.rows && !(k < static_cast<int>(ws.row.size()) && ws.row[static_cast<std::size_t>(k)].rec == i)) ++k;
    cell_at(h, *c, 0, 0x2e * k + 4, ship_pool, i, mode, ws.half, row_glint(k));
    return 0;
}

int smart_width(int half) { return 0x32 >> half; }  // [BYTES seg13:0x628d-0x62a6]
int smart_height(int caption, int rows, int half) {
    return half != 0 ? caption + 0x17 * rows + 3 : caption + 0x2e * rows + 6;  // [BYTES seg13:0x62a9-0x62e1]
}

WatchLinks& watch_links() {
    static WatchLinks l;
    return l;
}

#ifndef _WIN32
int watch_key(bool, bool) { return 0; }
void watch_create() {}
void watch_destroy() {}
bool watch_exists() { return false; }
void smart_create() {}
void smart_destroy() {}
bool smart_exists() { return false; }
void watch_ship(int) {}
void watch_tail() {}
void smart_row(int, int) {}
int watch_windows(int) { return 0; }
void all_ships_notice(int) {}
void watch_mark2() {}
WatchKeys take_watch_keys() { return {}; }
std::string program_object_name(int, int, int, int) { return {}; }
#endif

}  // namespace is::hud
