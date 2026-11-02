// The target display's texts and pictures and the next-target choice (M3 stage 4, docs/re/world.md
// "Target display", "Icon column"):
// - seg3:0x1542's pool-0 label ("Icon N", N the record index) and seg3:0xf4's icon suffix;
// - seg13:0x2206, the next target of Ctrl+T and of seg13:0x24aa's retarget;
// - seg13:0x5a6a and seg13:0x5c22 for pool 0, the icon picture of the target display
//   (seg13:0x58dc) and of the IS_ICON column (seg7:0x64ac), as a plan of GDI calls.
// Portable.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "hud/canvas.h"

namespace is::hud {

// ---- seg3:0x1542(caps 0, pool 0, rec, dest, mode 0) and seg3:0xf4's suffix

// String 152 ("Icon ^N") with its last 2 characters cut, then seg6:0xf1a(rec): the decimal
// record index [BYTES seg3:0x1640-0x1659] [ORACLE wave-0001 trace LSTRCAT("Icon ","1")].
std::string icon_label(const std::string& string152, int rec);
// The pool-0 suffix by object byte +0x53: 2 -> string 2208 " (VIRUSED)", 0 -> string 2209
// " (FROZEN)", else none [BYTES seg3:0x27f seg3:0x28d seg3:0x2bd]. Returns the string id or 0.
int icon_suffix_id(int b53);

// ---- seg13:0x2206

struct NextTargetHost {
    // The object's screen point (+0x00, +0x02), read directly [BYTES seg13:0x22c0-0x22c4].
    std::function<Point(int pool, int rec)> point;
    // seg8:0x19b0(num, den), Pascal: num = dy, den = dx [BYTES seg13:0x22ef-0x22f4].
    std::function<int(int num, int den)> direction16;
    // seg3:0x0(pool, rec).
    std::function<bool(int pool, int rec)> accept;
    // seg8:0x285a(world position of (pool, rec), world position of the player).
    std::function<int(int pool, int rec)> distance;
    // DGROUP:0x9406 + 2·pool.
    std::function<int(int pool)> in_use;
};

struct NextTargetInputs {
    int heading = 0;    // the player object's +0x28 (0 ... 15) [BYTES seg13:0x223e]
    int ship_pool = 14; // DGROUP:0x9324
    int player = 0;     // DGROUP:0x9646
    // The visible list DGROUP:0x507c (pool bytes) / DGROUP:0x50e0 (record bytes), DGROUP:0x9146
    // entries, sign-extended (cbw) [BYTES seg13:0x2271-0x2283].
    std::vector<std::pair<int, int>> visible;
    // The last contact: DGROUP:0x943e (pool), DGROUP:0x9440 (record), dword DGROUP:0x52d8 (its
    // time) and dword DGROUP:0x1e50 (now) [BYTES seg13:0x2216-0x221f seg13:0x234c-0x2354].
    int contact_pool = 0, contact_rec = 0;
    std::uint32_t contact_time = 0, now = 0;
};

// (pool << 8) | the sign-extended record byte, as the original's ch/al pair: pool 0xff for
// none. When nothing qualifies the original's low byte is an uninitialised local
// ([bp-0xc], seg13:0x23f9); the port returns -1 (0xffff) [DESIGN].
std::int16_t next_target(const NextTargetInputs& in, const NextTargetHost& h);

// ---- seg3:0x0 and seg13:0x371c (core.md "Target validity")
// seg13:0x371c(pool, rec): 0 outside pool 12, 1 for an even pool-12 record, 2 for an odd one
// [BYTES seg13:0x3729-0x373e].
int target_kind_371c(int pool, int rec);
// What seg3:0x0 reads.
struct TargetValidity {
    int ship_pool = 14;  // DGROUP:0x9324
    int player = 0;      // DGROUP:0x9646
    std::function<int(int pool)> allocated;           // DGROUP:0x92ec + 2·pool
    std::function<int(int pool, int rec)> state;      // object word +0x18
    std::function<int(int rec)> ship_147;             // ship byte +0x147 (DGROUP:0x71d7)
    // The defender entry DGROUP:0x52fe + 0x24·rec: words +0x00, +0x06, +0x08.
    std::function<int(int rec)> def_00, def_pool, def_rec;
    // DGROUP:0x3d6c + 0x34·rec = 2 and DGROUP:0x3d5a + 0x34·rec > 0 (the gate post).
    std::function<bool(int rec)> gate_post_live;
};
// seg3:0x0(pool, rec) [BYTES seg3:0xe-0xe9].
bool target_valid(const TargetValidity& v, int pool, int rec);

// ---- seg13:0x5a6a / seg13:0x5c22 for pool 0

// The object record fields the cell reads.
struct CellObject {
    int state = 0;    // +0x18
    int extent = 0;   // +0x20
    int size = 32;    // +0x22
    int gap = 6;      // +0x24
    int src_y = 0;    // +0x12
    int b51 = 0;      // +0x51
};
// The pool-0 icon record DGROUP:0x57a8 + 0x23·rec.
struct CellIcon {
    int w00 = 0;  // DGROUP:0x57a8
    int w02 = 0;  // DGROUP:0x57aa
    int w19 = 0;  // DGROUP:0x57c1
};

struct CellPlan {
    // flag = 1: PatBlt(x >> s, y >> s, 45 >> s, 45 >> s, BLACKNESS) [BYTES seg13:0x5ab0-0x5ad3].
    bool clear = false;
    int clear_x = 0, clear_y = 0, clear_size = 0;
    // The glint (seg13:0x5460 before, seg13:0x54da after the picture) runs [BYTES
    // seg13:0x5a99-0x5b1d seg13:0x5bfe].
    bool glint = false;
    // seg13:0x5c22's StretchBlt(SRCCOPY) of image slot `slot` (13 is the icon strip DGROUP:0x3a02,
    // slot 14 + rec a captured icon's own image, 5 the logo strip of pool 7) after
    // SetStretchBltMode(COLORONCOLOR).
    bool picture = false;
    int slot = 0;
    int dx = 0, dy = 0, dw = 0, dh = 0;
    int sx = 0, sy = 0, sw = 0, sh = 0;
    // seg13:0x371c != 0: graphic 500 instead (seg13:0x5de0-0x5e8f) [OPEN: not drawn by the port].
    bool special = false;
    // Pool 0 with +0x18 != 0 and DGROUP:0x57c1 != 0: the four sub-cells of seg13:0x5f49-0x6057
    // (a shattered icon's quarters) [BYTES seg13:0x5f2c-0x5f46] (s2-display-verify.md claim 33).
    // Sub-cell i sits at ((dx_i + X) >> s, (dy_i + Y) >> s), (dx, dy) = (0, 0), (22, 0), (0, 22),
    // (22, 22), side 20 >> s. `draw`: StretchBlt SRCCOPY from fragment bitmap DGROUP:0x4e94[frag]
    // at (gap, gap) size x size of pool-1 record 0 [BYTES seg13:0x5fb9-0x6011]; else PatBlt
    // BLACKNESS [BYTES seg13:0x6034-0x604b].
    bool sub_cells = false;
    struct SubCell {
        int x = 0, y = 0, side = 0;
        bool draw = false;
        int frag = 0;
    };
    SubCell subs[4];
    int frag_sx = 0, frag_size = 0;
    // A pool other than 0 and 7 [OPEN: M4/M5].
    bool other_pool = false;
};

struct CellInputs {
    int x = 0, y = 0;     // [bp+8], [bp+0xa]
    int pool = 0, rec = 0;
    int flag = 0;         // [bp+0x12]
    int scale = 0;        // [bp+0x14] (VScale for the column, 0 for the target display)
    int kind = 0;         // seg13:0x371c(pool, rec)
    int ship_pool = 14;   // DGROUP:0x9324
    int detail = 3;       // DGROUP:0x9208
    int dg_131c = 0;      // DGROUP:0x131c
    // Pool 7: word +0x10 of pool 7's record 0 (far pointer DGROUP:0x1e14), the source x of a
    // live pool-7 object [BYTES seg13:0x5d2f-0x5d37].
    int pool7_w10 = 0;
    CellObject obj;
    CellIcon icon;
    // seg13:0x5f49's inputs: pool-1 record 0's +0x22 (size) and +0x24 (gap) (far DGROUP:0x1dfc),
    // and for i = 0 ... 3 the signed byte k = DGROUP:0x57c3 + 0x23·rec + i, pool-1 record k's
    // `state` and its icon word DGROUP:0x6038 + 2k. A k outside pool 1 reads `state` 1 [DESIGN].
    int frag_size = 0, frag_gap = 0;
    struct Fragment {
        int index = -1, state = 1, parent = -1;
    };
    Fragment frags[4];
};

// seg13:0x5a6a(hdc, x, y, pool, rec, mark, flag, scale): `glint_counter` is the mark's +0x14
// (DGROUP:0x6e4c + 0x1e·mark), which the call updates.
CellPlan plan_cell(const CellInputs& in, int& glint_counter);

}  // namespace is::hud
