#include "hud/target.h"

namespace is::hud {

namespace {

int w16(int v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }
// cwd; sub ax, dx; sar ax, 1: a signed halving toward zero.
int half(int v) { return w16(v) / 2; }
int abs16(int v) { return w16(v) < 0 ? w16(-w16(v)) : w16(v); }  // seg9:0x88ba

// ch = the pool byte, cl = 0; al = the record byte, cbw; or ax, cx [BYTES seg13:0x236d-0x2377
// seg13:0x23f9-0x2400].
std::int16_t pack(int pool, int rec) {
    const std::uint16_t lo = static_cast<std::uint16_t>(static_cast<std::int16_t>(static_cast<std::int8_t>(rec & 0xff)));
    return static_cast<std::int16_t>(static_cast<std::uint16_t>((pool & 0xff) << 8) | lo);
}

}  // namespace

std::string icon_label(const std::string& string152, int rec) {
    std::string s = string152;
    // seg3:0x1640-0x1659: the last character is the gender mark (^M, ^F, ^N); the routine cuts
    // the last 2 characters and appends seg6:0xf1a([bp+0xa]).
    s.erase(s.size() >= 2 ? s.size() - 2 : 0);
    return s + std::to_string(w16(rec));
}

int icon_suffix_id(int b53) {
    if (b53 == 2) return 0x8a0;  // seg3:0x27f: 2208
    if (b53 == 0) return 0x8a1;  // seg3:0x28d: 2209
    return 0;
}

std::int16_t next_target(const NextTargetInputs& in, const NextTargetHost& h) {
    const int heading = w16(in.heading);
    const Point me = h.point(in.ship_pool, in.player);
    // seg13:0x224a-0x2320: the candidates in the heading's cone. The original keeps them in two
    // 20-word stack arrays ([bp-0x54], [bp-0x7c]) without a bound; the port keeps them all
    // [DESIGN].
    std::vector<std::pair<int, int>> cand;
    for (const auto& e : in.visible) {
        const int p = e.first, r = e.second;
        // seg13:0x229e-0x22a8: the entry's POOL byte compared with DGROUP:0x9324 and then with
        // DGROUP:0x9646, so only a pool equal to both is skipped [BYTES seg13:0x229e seg13:0x22a4].
        if (p == in.ship_pool && p == in.player) continue;
        const Point t = h.point(p, r);
        const int dx = w16(t.x - me.x), dy = w16(t.y - me.y);  // seg13:0x22dc, seg13:0x22e7
        const int a = w16(h.direction16(dy, dx));               // seg13:0x22f4
        bool in_cone;
        if (heading >= 0xe)  // seg13:0x22fc -> 0x225d: h - a < 3, else a = 0
            in_cone = w16(heading - a) < 3 || a == 0;
        else if (heading <= 2)  // seg13:0x2304 -> 0x224f: a - h < 3, else a = 15
            in_cone = w16(a - heading) < 3 || a == 0xf;
        else  // seg13:0x230c-0x231c: abs(h - a) < 3
            in_cone = abs16(heading - a) < 3;
        if (in_cone) cand.emplace_back(p, r);
    }
    int best = 0x4e20, bp = -1, br = 0;  // [bp-0xe], [bp-0xa], [bp-0xc]
    if (!cand.empty()) {
        // seg13:0x232b-0x2379: the last contact within 5000 ms (unsigned) when it is a candidate.
        if (in.now - in.contact_time < 0x1388u) {
            for (const auto& c : cand)
                if (c.first == w16(in.contact_pool) && c.second == w16(in.contact_rec) &&
                    h.accept(in.contact_pool, in.contact_rec))
                    return pack(in.contact_pool, in.contact_rec);
        }
        // seg13:0x237c-0x23f1: the nearest accepted candidate.
        for (const auto& c : cand) {
            if (!h.accept(c.first, c.second)) continue;
            const int d = w16(h.distance(c.first, c.second));
            if (d < best) {
                best = d;
                bp = c.first;
                br = c.second;
            }
        }
        if (bp > -1) return pack(bp, br);  // seg13:0x23f3-0x23f7: jle
    }
    // seg13:0x2403-0x2499: the nearest accepted object of pools 0 ... DGROUP:0x9324, not the
    // player.
    for (int p = 0; p <= in.ship_pool; ++p)
        for (int r = 0; r < h.in_use(p); ++r) {
            if (p == in.ship_pool && r == in.player) continue;  // seg13:0x241c-0x2429
            if (!h.accept(p, r)) continue;
            const int d = w16(h.distance(p, r));
            if (d < best) {
                best = d;
                bp = p;
                br = r;
            }
        }
    if (bp == -1) return -1;  // the uninitialised low byte [DESIGN]
    return pack(bp, br);
}

CellPlan plan_cell(const CellInputs& in, int& glint_counter) {
    CellPlan c;
    const int s = in.scale;
    // seg13:0x5a78-0x5aab: the glint counter, reset by a flag, else 0, 1, 2, 0, ...
    if (in.flag != 0) {
        glint_counter = 0;
    } else {
        glint_counter = w16(glint_counter + 1);
        if (glint_counter > 2) glint_counter = 0;
    }
    c.glint = glint_counter == 0;
    if (in.flag == 1) {  // seg13:0x5ab0-0x5ad3
        c.clear = true;
        c.clear_x = w16(in.x) >> s;
        c.clear_y = w16(in.y) >> s;
        c.clear_size = 0x2d >> s;
    }
    // seg13:0x5ad8-0x5b14: no glint for a dead object, pool 2, a flagged call, Detail < 2, or the
    // ship pool while DGROUP:0x131c = 0.
    if (in.obj.state != 0 || in.pool == 2 || in.flag != 0 || in.detail < 2) c.glint = false;
    if (in.pool == in.ship_pool && in.dg_131c == 0) c.glint = false;
    // seg13:0x5b35-0x5bdd: an unflagged call skips some objects.
    bool draw = true;
    if (in.flag == 0) {
        if (in.pool == 7 && in.obj.state != 0) draw = false;
        if (in.pool == 1) draw = false;
        if (in.kind != 0) draw = false;
        if (in.pool == 0 && in.icon.w00 == 0 && in.obj.b51 == 0) draw = false;
        if ((in.pool == 0 || in.pool == 0xb) && in.obj.state != 0 && in.obj.b51 == 0) draw = false;
        if (in.pool == in.ship_pool) draw = false;
    }
    if (!draw) return c;
    // seg13:0x5c22.
    if (in.kind != 0) {  // seg13:0x5dcf-0x5ddc
        c.special = true;
        return c;
    }
    int a = in.obj.extent >> 3;                   // seg13:0x5c40-0x5c47: sar 3
    const int size = in.obj.size, gap = in.obj.gap;
    if (in.pool == 7) {
        // seg13:0x5dac case 7 [BYTES seg13:0x5d05-0x5d4b]: slot 5, the logo strip (shell.md "Image
        // slots"); a dead record (+0x18 != 0) shows the cell at 6·gap + 5·size, y gap, a live one
        // the source x of record 0's +0x10 with its own +0x12; the extent is halved (idiv 2).
        // Then seg13:0x5dcf (seg13:0x371c = 0 here, tested above) and seg13:0x5f1e-0x5f46 /
        // seg13:0x6070-0x6074 lead every pool-7 call to the draw at seg13:0x6096 (M4 s1 SCREENS).
        c.slot = 5;
        if (in.obj.state != 0) {
            c.sx = w16(w16(gap * 6) + w16(size * 5));
            c.sy = gap;
        } else {
            c.sx = in.pool7_w10;
            c.sy = in.obj.src_y;
        }
        a = a / 2;
        c.sw = c.sh = size;
        const int off = half(0x2b - a);  // seg13:0x6096-0x60e1
        a = a >> s;
        c.dx = w16(off + in.x) >> s;
        c.dy = w16(off + in.y) >> s;
        c.dw = c.dh = a;
        c.picture = true;
        return c;
    }
    if (in.pool != 0) {
        c.other_pool = true;
        return c;
    }
    if (in.obj.state != 0) {                      // seg13:0x5f1e-0x5f46
        if (in.icon.w19 != 0) {
            c.sub_cells = true;
            return c;
        }
        if (in.obj.b51 == 0) return c;            // seg13:0x6070-0x6092
    }
    // seg13:0x5d5b-0x5d9b: slot 13 at (gap + size)·rec + gap, or slot rec + 14 at
    // (w02 / 2 - 1)·(gap + size) + gap.
    if (in.icon.w02 == 0) {
        c.slot = 0xd;
        c.sx = w16(w16((gap + size) * in.rec) + gap);
    } else {
        c.slot = w16(in.rec + 0xe);
        c.sx = w16(w16((half(in.icon.w02) - 1) * (gap + size)) + gap);
    }
    c.sy = in.obj.src_y;  // seg13:0x5c58
    c.sw = c.sh = size;
    // seg13:0x6096-0x60e1: centred in a 43-pixel cell, all shifted right by the scale.
    const int off = half(0x2b - a);
    a = a >> s;
    c.dx = w16(off + in.x) >> s;
    c.dy = w16(off + in.y) >> s;
    c.dw = c.dh = a;
    c.picture = true;
    return c;
}

}  // namespace is::hud
