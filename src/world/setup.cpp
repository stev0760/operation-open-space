// owner: world set-up (the single writer of this file in later stages).
// The wave set-up (setup.h, docs/re/world.md "Wave set-up"). Each routine cites its original;
// the readings that go beyond world.md are listed in the stage report for a verifier. Portable.
#include "world/setup.h"

#include "world/draw.h"

#include <cstdio>
#include <string>

#include "nav/tree.h"

namespace is::world {

namespace {

using std::int16_t;
using std::int32_t;

int16_t i16(int32_t v) { return lo16(v); }

// rand() % n as the original's `cwd; idiv` computes it. A zero divisor would raise the divide
// fault of the original; the port gives 0 instead [DESIGN].
int16_t rmod(int r, int32_t n) {
    const int16_t d = i16(n);
    if (d == 0) return 0;
    return imod16(i16(r), d);
}

void say(SetupHost& h, const std::string& s) {
    if (h.log) h.log(s);
}

std::uint32_t now_of(SetupHost& h) { return h.now ? h.now() : 0; }

// A ship record's word at byte offset `off` (offsets structs.md leaves unnamed).
int16_t ship_word(const ShipRecord& s, int off) {
    const auto* p = reinterpret_cast<const std::uint8_t*>(&s);
    return static_cast<int16_t>(p[off] | (p[off + 1] << 8));
}
std::uint8_t ship_byte(const ShipRecord& s, int off) {
    return reinterpret_cast<const std::uint8_t*>(&s)[off];
}
void set_ship_byte(ShipRecord& s, int off, std::uint8_t v) { reinterpret_cast<std::uint8_t*>(&s)[off] = v; }
void set_ship_word(ShipRecord& s, int off, int16_t v) {
    auto* p = reinterpret_cast<std::uint8_t*>(&s);
    p[off] = static_cast<std::uint8_t>(v);
    p[off + 1] = static_cast<std::uint8_t>(static_cast<std::uint16_t>(v) >> 8);
}


// (side << Size) >> 1 [BYTES seg10:0xbbd].
int16_t side(const World& w, int c) { return sprite_side(static_cast<int16_t>(c), w.size); }

ObjectRecord& rec(World& w, int pool, int i) { return w.pools.rec(pool, i); }

// The placers' common start: +0x08 = +0x0a = -1000 and the screen position copied from them
// [BYTES seg10:0x8c7 seg10:0x8e4].
void off_screen(ObjectRecord& r) {
    r.old_scr_x = -1000;
    r.scr_x = r.old_scr_x;
    r.old_scr_y = -1000;
    r.scr_y = r.old_scr_y;
}

// (rand() % (cells - 150) + 150) * 8, the placers' coordinate [BYTES seg10:0x8f8 seg10:0x910].
int16_t random_coord(shell::Rng& rng, int16_t cells) {
    return mul16(add16(rmod(rng.rand(), sub16(cells, 150)), 150), 8);
}

bool in_place_range(const SpecialPlace& p, int k) { return p.first <= k && k < p.end; }

// The special place's words by index 0..7 (+0x0 ... +0xe of the 0x10-byte entry).
int16_t& pw(SpecialPlace& p, int i) {
    switch (i) {
    case 0: return p.first;
    case 1: return p.end;
    case 2: return p.radius;
    case 3: return p.w6;
    case 4: return p.w8;
    case 5: return p.x;
    case 6: return p.y;
    default: return p.we;
    }
}
int16_t pw(const SpecialPlace& p, int i) { return pw(const_cast<SpecialPlace&>(p), i); }

constexpr int kDgPool8 = 0x3d50;  // DGROUP:0x3d50, the pool-8 table's base

// The current directory record with the accessors the set-up uses.
struct DirView {
    DirRecord& r;
    std::array<std::uint8_t, kDirRecordSize>& b;
    explicit DirView(DirRecord& rec) : r(rec), b(rec.b) {}
    std::int8_t s8(int off) const { return r.sbyte(static_cast<std::size_t>(off)); }
    int16_t w(int off) const { return r.word(static_cast<std::size_t>(off)); }
    void set_w(int off, int16_t v) { r.set_word(static_cast<std::size_t>(off), v); }
    std::uint8_t& at(int off) { return b[static_cast<std::size_t>(off)]; }
};
DirView dv(World& w) { return DirView(w.dir()); }

// A hazard class entry DGROUP:0x4d90 + 0x14 c.
struct HazardView {
    const std::array<std::uint8_t, 0x14>& b;
    std::int8_t s8(int off) const { return static_cast<std::int8_t>(b[static_cast<std::size_t>(off)]); }
    int16_t w(int off) const {
        return static_cast<int16_t>(b[static_cast<std::size_t>(off)] | (b[static_cast<std::size_t>(off) + 1] << 8));
    }
};
// The hazard class table entry c, or an empty entry for an index outside 0..12 [DESIGN: the
// original reads past the table].
HazardView hazard_class(const World& w, int c) {
    static const std::array<std::uint8_t, 0x14> none{};
    return HazardView{(c >= 0 && c < static_cast<int>(w.classes.size())) ? w.classes[static_cast<std::size_t>(c)] : none};
}

}  // namespace

// ---------------------------------------------------------------- difficulty inputs

void power_on(World& w) {
    w.w986 = {1, 1, 1, 1, 1, 2};
    w.w9346 = 0;
    w.w9432 = 0;
    w.w992 = 0;
}

void read_budget(World& w) {
    // seg6:0x11ec [BYTES seg6:0x1204-0x1256]
    switch (w.complex) {
    case 0: w.budget = 130; w.w9000 = 0; break;
    case 1: w.budget = 220; w.w9000 = 0; break;
    case 2: w.budget = 310; w.w9000 = 1; break;
    case 3: w.budget = 420; w.w9000 = 2; break;
    case 4: w.budget = 2000; w.w9000 = 2; break;
    default: break;  // any other value leaves both words
    }
    // The easy first waves [BYTES seg6:0x1258-0x1284].
    if (w.wave < 10) {
        const int16_t b = w.budget;
        const int16_t v = add16(idiv16(b, 2), idiv16(mul16(w.wave, b), 20));
        if (v <= b) w.budget = v;
    }
}

void level_from_score(World& w) {
    // seg9:0x484c: the thresholds DGROUP:0x9bc + 4k from the file [BYTES DGROUP:0x9bc seg9:0x484c].
    static constexpr std::uint32_t kThreshold[12] = {26000,  40000,  55000,  70000,  90000,  120000,
                                                     150000, 200000, 280000, 400000, 550000, 700000};
    w.score_level = 0;
    const std::int32_t score = static_cast<std::int32_t>(w.rating);
    for (int k = 0; k < 12; ++k)
        if (static_cast<std::int32_t>(kThreshold[k]) < score) w.score_level = static_cast<int16_t>(k + 1);
}

void new_game(World& w) {
    // seg9:0xc06, world.md "New-game reset" [BYTES seg9:0xc06 seg9:0xc56]. The rating, 0xa1cd,
    // 0xa1cf, the bytes 0xa193 ... 0xa196 and the DGROUP:0x4eec table (M3 s3 fixer, C016)
    // [BYTES seg9:0xc3e seg9:0xc44 seg9:0xc4e seg9:0xc52 seg9:0xc7f seg9:0xc97 seg9:0xc9d seg9:0xca5
    // seg9:0xcb0 seg9:0xcc8].
    w.w992 = 0;
    if (w.restored == 0) {
        w.score = 0;
        w.rating = 0;
        w.w9346 = -1;
        w.w4eec.fill(-1);
        w.wave = 0;
        w.w967c = 0;
        w.w94ca = 0;
        w.w93ac = 0;
        w.wa1cd = 0;
        w.w9432 = 100;
        w.wa1cf = 1;
        w.ba193.fill(1);
    }
    w.w986.fill(1);
    w.w986[5] = 2;  // DGROUP:0x990
}

// ---------------------------------------------------------------- the generator seg9:0x3202

void generate(World& w, shell::Rng& rng) {
    // The weights of the five categories, a local table [BYTES seg9:0x3214].
    int16_t weight[5] = {1, 1, 3, 5, 5};
    level_from_score(w);  // seg9:0x484c
    w.w9344 = 0;
    read_budget(w);  // seg6:0x11ec
    ShipRecord& me = w.ships[static_cast<std::size_t>(w.player)];

    // DGROUP:0x992, the share of the budget already spent.
    if (w.budget < w.w992) {
        const int16_t q = idiv16(w.budget, 4);
        w.w992 = idiv16(sub16(w.w992, w.budget), 2);
        if (q < w.w992) w.w992 = q;
    } else {
        w.w992 = 0;
    }
    if (me.w26 != 0) w.w992 = sub16(w.w992, sub16(idiv16(me.w26, 2), 50));
    if (w.w9346 != -1) w.w992 = add16(w.w992, 100);
    w.w994 = 0;
    if (w.added == 0) {
        DirView d = dv(w);
        for (int k = 0; k < 4; ++k) w.w992 = add16(w.w992, mul16(d.s8(0xa0 + k), weight[k]));
        for (int k = 0; k < 2; ++k)
            w.w992 = add16(w.w992, mul16(static_cast<int16_t>(hazard_class(w, d.s8(0xab + k)).s8(5) + 3),
                                           d.s8(0xa7 + k)));
    }
    if (w.w992 < idiv16(mul16(w.budget, 2), 3)) {
        // The two roster blocks (seg9:0x3292-0x3560) read the roster DGROUP:0x6e24 and the table
        // DGROUP:0x4eec; a new game has no roster entry that matches, so they draw no number
        // [OPEN: the roster is M5].
        // The player's ship block [BYTES seg9:0x3550 seg9:0x35ec].
        const int16_t v = ship_word(me, 0xce);
        if (400 < v && w.w992 < sub16(idiv16(mul16(w.budget, 3), 5), mul16(w.w994, 50))) {
            int16_t c = i16(idiv16(v, 25) - idiv16(me.w26, 5) + (w.score_level + 10) * 2 - idiv16(w.w992, 5) -
                            w.w994 * 35 + w.w9000 * 3);
            if (75 < c) c = 75;
            if (rmod(rng.rand(), 100) < c) {
                w.w992 = add16(w.w992, 25);
                if (w.w994 < 8) w.w4f1c[static_cast<std::size_t>(w.w994)] = -100;
                w.w994 = add16(w.w994, 1);
            }
            if (rmod(rng.rand(), 250) < c &&
                w.w992 < sub16(idiv16(mul16(w.budget, 3), 5), mul16(w.w994, 45))) {
                w.w992 = add16(w.w992, 30);
                if (w.w994 < 8) w.w4f1c[static_cast<std::size_t>(w.w994)] = -100;
                w.w994 = add16(w.w994, 1);
            }
        }
    }

    // The wave kind DGROUP:0x9320 and the target [BYTES seg9:0x3611-0x37bc].
    int16_t target = 0;
    auto& k986 = w.w986;  // [0] 0x986, [2] 0x98a, [3] 0x98c, [4] 0x98e, [5] 0x990
    w.kind = -1;
    if (w.score_level == 0 && w.added != 0) {
        if (w.wave == 0) {
            w.kind = 0;
            k986[0] = 0;
            target = add16(rmod(rng.rand(), 20), 30);
            w.w9336 = 0;
            w.opponents = 0;
        } else if (k986[2] != 0 && rmod(rng.rand(), 2) == 1) {
            w.kind = 2;
            k986[2] = 0;
            const int r = rng.rand();
            target = add16(rmod(r, 20), 20);
            w.opponents = 1;
        } else if (k986[4] != 0 && rmod(rng.rand(), 2) == 1) {
            w.kind = 4;
            k986[4] = 0;
            target = add16(rmod(rng.rand(), 20), 50);
            w.opponents = 0;
        } else {
            bool two = false;  // the paths that end in "9338 = rand % 2 + 1" and the target
            if (k986[3] != 0 && rmod(rng.rand(), 2) == 1) {
                w.kind = 3;
                k986[3] = 0;
                two = true;
            } else if (k986[5] != 0) {
                w.kind = 5;
                k986[5] = sub16(k986[5], 1);
                two = true;
            } else if (k986[2] == 0) {
                if (k986[4] != 0) {
                    w.kind = 4;
                    k986[4] = 0;
                    const int r = rng.rand();
                    target = add16(rmod(r, 40), 40);
                    w.opponents = 1;
                } else {
                    w.score_level = 1;
                }
            } else {
                w.kind = 2;
                k986[2] = 0;
                target = add16(rmod(rng.rand(), 15), 25);
                w.opponents = static_cast<int16_t>(rmod(rng.rand(), 2) + 1);
            }
            if (two) {
                w.opponents = static_cast<int16_t>(rmod(rng.rand(), 2) + 1);
                target = add16(rmod(rng.rand(), mul16(sub16(3, w.opponents), 10)), 20);
            }
        }
    }
    if (w.wave < 5 && w.kind == -1) {  // [BYTES seg9:0x37bc-0x37e5]
        w.kind = 5;
        k986[5] = sub16(k986[5], 1);
        target = add16(rmod(rng.rand(), 15), 30);
        w.opponents = static_cast<int16_t>(rmod(rng.rand(), 2) + 1);
    }
    if (w.added == 0 && w.kind != 6) w.kind = -1;
    if (w.score_level != 0 && w.added != 0) {  // [BYTES seg9:0x3821-0x3947]
        w.w9336 = 1;
        const int16_t c = rmod(rng.rand(), 5);
        if (c == 0) {
            w.opponents = static_cast<int16_t>(rmod(rng.rand(), 1) + 2);
            target = i16(rmod(rng.rand(), mul16(sub16(8, w.opponents), 5)) + w.score_level * 2 + 5);
        } else if (c == 1) {
            w.opponents = rmod(rng.rand(), 3);
            target = i16(rmod(rng.rand(), mul16(sub16(6, w.opponents), 5)) + w.score_level * 4 + 9);
            if (target < 36) {
                w.score_level = sub16(w.score_level, 1);
            } else {
                w.score_level = sub16(w.score_level, 2);
                if (w.score_level < 1) w.score_level = 1;
            }
        } else if (c == 2) {
            w.opponents = static_cast<int16_t>(rmod(rng.rand(), 2) + 2);
            target = add16(rmod(rng.rand(), mul16(sub16(7, w.opponents), 5)), 5);
            if ((rng.rand() & 1) == 0)
                w.score_level = sub16(w.score_level, 1);
            else
                w.w9336 = 2;
        } else if (c == 3) {
            target = add16(rmod(rng.rand(), 25), 10);
            w.opponents = 2;
            w.score_level = add16(w.score_level, 6);
        } else {
            w.opponents = static_cast<int16_t>(rmod(rng.rand(), 2) + 2);
            target = i16(rmod(rng.rand(), 18 - w.opponents * 4) + (w.score_level + 4) * 3);
            w.score_level = add16(w.score_level, 1);
        }
        if (w.w9332 < 2 && rmod(rng.rand(), 6) < 4) w.w9336 = sub16(w.w9336, 1);
        w.kind = static_cast<int16_t>(rmod(rng.rand(), 8) - 2);
        if (w.kind == 4) {
            target = add16(target, idiv16(target, 2));
            if (2 < w.opponents) w.opponents = sub16(w.opponents, 1);
        }
    }
    if (2 < w.complex) target = idiv16(mul16(target, w.complex), 2);  // [BYTES seg9:0x3947-0x395a]
    if (sub16(idiv16(shl16(w.budget, 2), 5), 20) < w.w992) w.kind = 0;  // [BYTES seg9:0x395d-0x3972]

    // The race chance [BYTES seg9:0x3978-0x3cc0].
    const bool path_form = w.handover.directory.size() > 1 && w.handover.directory[1] == ':';
    const int16_t flag6 = (w.added == 0 && dv(w).s8(0xad) < 0 && path_form) ? 1 : 0;
    // seg16:0x5f0e(DGROUP:0x6990, DGROUP:0x8f7a): the selected directory ends with the program's
    // directory [BYTES seg9:0x3a05 seg9:0x3a09 seg9:0x3a0c].
    auto own_dir = [&] { return !w.program_path.empty() && nav::ends_with(w.handover.directory, w.program_path); };
    const bool a = w.added != 0 &&
                   (own_dir() || (4 < sub16(w.wave, w.w94ca) && path_form)) && w.w94a6 < 25;
    bool race_now = false;
    if (a || flag6 != 0) {
        if (own_dir()) {
            race_now = true;
        } else {
            const int16_t r = rmod(rng.rand(), 100);
            race_now = (r < i16((w.wave - w.w94ca - 3) * 2 + w.score_level / 2) && 50 < me.w26 && 50 < me.w2a) ||
                       flag6 != 0;
        }
    }
    if (race_now) {
        int16_t course;
        if (flag6 == 0) {
            course = rmod(rng.rand(), 3);
        } else {
            course = static_cast<int16_t>(-(dv(w).s8(0xad) + 1));
            dv(w).at(0xad) = 1;
        }
        w.kind = 6;
        const int16_t racers = static_cast<int16_t>(rmod(rng.rand(), 3) + 1);
        w.handover.laps = static_cast<int16_t>(((rng.rand() & 1) + 1) * 5);
        if (w.handover.laps == 10 && rmod(rng.rand(), 5) == 3) w.handover.laps = 1;
        w.handover.race = static_cast<int16_t>(course * 4 + racers + 1);
        int16_t& aa = w.handover.opponent;  // DGROUP:0x94aa
        const int16_t lv = w.score_level, bl = w.w9000, n = w.w967c;
        if (n == 0) {
            aa = (lv < 5 && bl < 2) ? 5 : 4;
        } else if (n < 3) {
            if (bl < 2 && n == 1 && rmod(rng.rand(), 6) < 2 && lv < 5) aa = 5;
            if ((lv < 9 && bl == 0) || (lv < 7 && bl == 1) || rmod(rng.rand(), 5) == 1)
                aa = 4;
            else if (7 < lv && bl == 2 && rmod(rng.rand(), 6) < 2)
                aa = 1;
            else if (2 < rmod(rng.rand(), 6))
                aa = 3;
            else
                aa = 2;
        } else if (n < 4) {
            if (bl == 2 && 7 < lv && (rng.rand() & 1) == 0)
                aa = 1;
            else if (bl < 2 && rmod(rng.rand(), 6) < 2 && lv < 5)
                aa = 4;
            else if (2 < rmod(rng.rand(), 6))
                aa = 2;
            else
                aa = 3;
        } else {
            aa = ((n < 7 && bl != 2) || 2 < rmod(rng.rand(), 6)) ? 1 : 0;
        }
        w.handover.difficulty = 0;
        if (1 < n && rmod(rng.rand(), 6) < 3 && aa != 0)
            w.handover.difficulty = (racers < 3 && 6 < lv && 3 < n && rmod(rng.rand(), 6) < 4) ? 2 : 1;
    }

    // The opponents [BYTES seg9:0x3cc0-0x3e10].
    int16_t free_icons = 0;
    for (int k = 0; k < kIconRecords && k < w.pools.records(kPoolIcons); ++k)
        if (rec(w, kPoolIcons, k).state == 0) ++free_icons;
    if (600 < w.w9432 && w.score_level != 0 && w.opponents < 3) {
        w.opponents = add16(w.opponents, 1);
        w.score_level = add16(w.score_level, 1);
    }
    if (850 < w.w9432 && w.score_level != 0 && w.opponents < 4) {
        w.opponents = add16(w.opponents, 1);
        w.score_level = add16(w.score_level, 1);
    }
    int16_t opp = w.opponents;
    if (free_icons < 5 && 2 < w.opponents) {
        opp = sub16(w.opponents, 1);
        if (free_icons < 3 && 4 < opp) opp = sub16(w.opponents, 2);
    }
    w.opponents = opp;
    target = idiv16(mul16(mul16(add16(w.detail, 3), target), 20), 100);
    if (w.handover.demons_gate != 0 || w.kind == 7) {
        target = 0;
        if (w.handover.demons_gate != 0) w.kind = 0;
        w.w932c = 0;
    }
    if (w.w932c != 0 && w.wave != 0 && w.opponents < 5) w.opponents = add16(w.opponents, 1);
    if (w.wave == 1) {
        w.opponents = 1;
        w.w9336 = 1;
    }
    if (w.opponents < w.w994) w.opponents = w.w994;
    if (w.handover.race != 0) {  // nav.md "Decoding" [BYTES seg9:0x3db6-0x3e00]
        w.kind = 6;
        w.w932c = 0;
        target = 0;
        const int16_t v = sub16(w.handover.race, 1);
        w.opponents = imod16(v, 4);
        w.course = idiv16(v, 4);
        // seg10:0x320a, the race course set-up, is M6 [OPEN].
        w.w9336 = 0;
    }
    if (w.handover.duel != 0) {  // [BYTES seg9:0x3e00-0x3e10]
        w.kind = 7;
        w.opponents = imod16(sub16(w.handover.duel, 1), 4);
        w.w9336 = 0;
        w.gate[0].width = 1200;
        w.gate[0].w0e = 80;
    }

    // The pool-4 count DGROUP:0x940e [BYTES seg9:0x3e26-0x3ea2].
    int16_t p4 = rmod(rng.rand(), mul16(add16(w.opponents, 1), 4));
    if (p4 < 3) p4 = 3;
    if (w.kind == 4) p4 = add16(p4, add16(rmod(rng.rand(), 15), 9));
    if (rmod(rng.rand(), 25) == 4 && 1 < w.detail) p4 = 17;
    if (w.handover.demons_gate == 0) {
        if (w.kind == 6) p4 = w.race_9ebf[static_cast<std::size_t>(w.course) % 3];
    } else {
        p4 = 1;
    }
    w.pools.set_in_use(kPool4, p4);
    w.pools.set_allocated(kPool4, p4);  // DGROUP:0x92f4
    w.pool4 = p4;

    w.categories.fill(0);
    if (w.kind == 3) w.categories[2] = 5;
    if (w.kind == 2) w.categories[3] = 5;
    if (w.kind == 5) w.categories[4] = 5;

    if (w.added == 0 && w.handover.race == 0) {
        // A revisited directory copies its counts [BYTES seg9:0x3214 seg9:0x3efa].
        DirView d = dv(w);
        for (int k = 0; k < 5; ++k) w.categories[static_cast<std::size_t>(k)] = d.s8(0xa0 + k);
        for (int k = 0; k < 2; ++k) {
            const auto u = static_cast<std::size_t>(k);
            w.w4ee0[u] = d.s8(0xa7 + k);
            w.w4ee8[u] = d.s8(0xa9 + k);
            w.w4ee4[u] = d.s8(0xa5 + k);
            w.hazard_class[u] = d.s8(0xab + k);
        }
        for (int k = 0; k < 10; ++k) {
            if (w.pools.allocated(kPoolGatePosts) < d.b[static_cast<std::size_t>(0x232 + k)]) {
                d.at(0x23c + k) = 0;
                d.at(0x232 + k) = d.b[static_cast<std::size_t>(0x23c + k)];
            }
            SpecialPlace& p = w.places[static_cast<std::size_t>(k)];
            pw(p, 0) = d.s8(0x232 + k);
            pw(p, 1) = d.s8(0x23c + k);
            pw(p, 2) = static_cast<int16_t>(d.s8(0x246 + k) * 80);
            pw(p, 3) = d.s8(0x250 + k);
            pw(p, 4) = d.s8(0x25a + k);
            pw(p, 5) = d.w(0x26f + 4 * k);
            pw(p, 6) = d.w(0x271 + 4 * k);
            pw(p, 7) = d.s8(0x265 + k);
        }
        w.w9344 = d.s8(0x264);
        w.gates = d.s8(0xad);
    } else {
        // The category draw loop [BYTES seg9:0x3efa-0x4079], then the thin categories
        // [BYTES seg9:0x407d-0x40cd] (world.md "Difficulty inputs", Categories "Draw", "Stop",
        // "Thin pass"; M3 s3 fixer, C021-C023); DGROUP:0x994 stays [OPEN DGROUP:0x994].
        bool settled = false;
        while (!settled) {
            int16_t sum = 0;
            do {
                int16_t c = 0;
                bool ok = false;
                while (!ok) {
                    ok = true;
                    c = rmod(rng.rand(), 5);
                    if (add16(w.score_level, 2) < weight[c]) ok = false;
                    if (w.score_level < weight[c] && rmod(rng.rand(), 5) < 3) ok = false;
                    if (w.score_level == 0) ok = true;
                    if ((c == 1 || c == 0) && 120 < add16(w.categories[1], w.categories[0])) ok = false;
                }
                int16_t& n = w.categories[static_cast<std::size_t>(c)];
                n = add16(n, 1);
                if ((w.kind == 1 || w.kind == 4) && (c == 0 || c == 1))
                    n = add16(n, 2);
                else if ((w.kind == 2 && c == 3) || (w.kind == 3 && c == 2) || (w.kind == 5 && c == 4))
                    n = add16(n, 3);
                else if (w.kind == 4)
                    n = add16(n, 1);
                if ((w.kind == 0 && (c == 2 || c == 3 || c == 4)) ||
                    (w.kind != 2 && w.kind != -1 && c == 3 && w.score_level < 3))
                    n = sub16(n, 1);
                sum = 0;
                for (int k = 0; k < 5; ++k) sum = add16(sum, mul16(weight[k], w.categories[static_cast<std::size_t>(k)]));
                const int16_t limit = idiv16(mul16(w.budget, 3), 4);
                const int16_t used = add16(add16(mul16(add16(mul16(w.opponents, 2), w.w994), 15), sum), w.w992);
                if (limit < used) sum = add16(target, 1);
            } while (sum < target);
            settled = true;
            for (int k = 0; k < 5; ++k) {
                int16_t& n = w.categories[static_cast<std::size_t>(k)];
                if (0 < n) {
                    int16_t m = idiv16(target, 12);
                    if (5 < m) m = 5;
                    if (n < m && k != 1) {
                        weight[k] = 100;
                        n = 0;
                        settled = false;
                    }
                }
            }
        }
        w.w4ee0 = {0, 0};
        w.w4ee4 = {0, 0};
        if (w.kind == 6) {  // [BYTES seg9:0x40dc-0x41ae]
            w.categories.fill(0);
            const auto c = static_cast<std::size_t>(w.course) % 3;
            w.w9ea2 = w.race_9eb6[c];
            w.w9ea4 = static_cast<int16_t>(w.race_9eb9[c] - w.race_9eb6[c]);
            w.w9ea6 = (w.w9000 < 2 || w.handover.difficulty < 1) ? int16_t(0) : int16_t(w.race_9ebc[c]);
            w.categories[2] = add16(w.w9ea2, w.w9ea4);
            w.categories[4] = w.w9ea6;
            // The race buffer GlobalAlloc(0x42, ...) DGROUP:0x92e0 is M6 [OPEN].
        }
        if (w.categories[4] != 0) {  // the hazard classes of category 4 [BYTES seg9:0x41ae-0x42e0]
            if (w.categories[4] < 6) {
                w.w4ee0 = {w.categories[4], 0};
            } else {
                const int16_t r = static_cast<int16_t>(rmod(rng.rand(), w.categories[4]) + 1);
                w.w4ee0 = {r, sub16(w.categories[4], r)};
            }
            for (int j = 0; j < 2; ++j) {
                int16_t best = -1000, idx = -1;
                for (int c = 1; c < 13; ++c) {
                    HazardView hc = hazard_class(w, c);
                    if (hc.b[0] != 2) continue;
                    const int r = rng.rand();
                    int16_t v = add16(rmod(r, 90), 50);
                    if (w.score_level == 0) {
                        if (w.w1822[static_cast<std::size_t>(c)] == 0) v = add16(rmod(r, 90), 100);
                        if (5 < hc.s8(5)) v = add16(v, mul16(hc.s8(5), -20));
                    } else if (w.score_level < hc.s8(5)) {
                        v = add16(v, mul16(sub16(hc.s8(5), w.score_level), -35));
                    }
                    // The race's favourite class DGROUP:0x9f39 + difficulty + 2 course is M6 [OPEN].
                    v = add16(v, mul16(hc.s8(7), 5));
                    if (hc.s8(3) == 5 && w.kind == 4) v = add16(v, 35);
                    if (best < v) {
                        best = v;
                        idx = static_cast<int16_t>(c);
                    }
                }
                w.hazard_class[static_cast<std::size_t>(j)] = idx;
                if (idx >= 0) w.w1822[static_cast<std::size_t>(idx)] = 1;
            }
            if (w.hazard_class[1] == w.hazard_class[0]) {
                w.hazard_class[1] = 0;
                w.w4ee0 = {add16(w.w4ee0[0], w.w4ee0[1]), 0};
            }
        }
        w.categories[4] = add16(w.w4ee0[1], w.w4ee0[0]);
        if (w.categories[2] != 0) {  // the hazard classes of category 2 [BYTES seg9:0x42e0-0x43f0]
            if (w.kind == 6) {
                w.w4ee4 = {w.w9ea2, w.w9ea4};
            } else if (5 < w.categories[2]) {
                const int16_t r = static_cast<int16_t>(rmod(rng.rand(), w.categories[2]) + 1);
                w.w4ee4 = {r, sub16(w.categories[2], r)};
            } else {
                w.w4ee4 = {w.categories[2], 0};
            }
            for (int j = 0; j < 2; ++j) {
                if (w.kind == 6) {
                    w.w4ee8 = {3, 4};
                    continue;
                }
                int16_t best = -1000, idx = -1;
                for (int c = 1; c < 13; ++c) {
                    HazardView hc = hazard_class(w, c);
                    if (hc.b[0] != 1) continue;
                    int16_t v = add16(rmod(rng.rand(), 50), 40);
                    if (w.score_level < hc.s8(5)) v = add16(v, mul16(sub16(hc.s8(5), w.score_level), -20));
                    if (w.kind == 4 && hc.s8(3) == 5) v = add16(v, 50);
                    if (c == 3 || c == 4) v = -1500;
                    if (best < v) {
                        best = v;
                        idx = static_cast<int16_t>(c);
                    }
                }
                w.w4ee8[static_cast<std::size_t>(j)] = idx;
            }
        }
        // The record keeps the counts for a revisit [BYTES seg9:0x43f0-0x4490].
        DirView d = dv(w);
        for (int k = 0; k < 5; ++k) d.at(0xa0 + k) = static_cast<std::uint8_t>(w.categories[static_cast<std::size_t>(k)]);
        for (int k = 0; k < 2; ++k) {
            const auto u = static_cast<std::size_t>(k);
            d.at(0xa7 + k) = static_cast<std::uint8_t>(w.w4ee0[u]);
            d.at(0xa9 + k) = static_cast<std::uint8_t>(w.w4ee8[u]);
            d.at(0xa5 + k) = static_cast<std::uint8_t>(w.w4ee4[u]);
            d.at(0xab + k) = static_cast<std::uint8_t>(w.hazard_class[u]);
        }
        // The gate count [BYTES seg9:0x4496-0x44f8].
        w.gates = 1;
        if (1 < w.wave && (rng.rand() & 1) != 0 && w.handover.demons_gate == 0 && w.kind != 6 && w.kind != 7) {
            w.gates = 2;
            if (3 < w.wave && rmod(rng.rand(), 10) == 1) {
                w.gates = 3;
                if (7 < w.wave && rmod(rng.rand(), 5) == 1) w.gates = 4;
            }
        }
    }
    // seg10:0x72d8 [OPEN]; DGROUP:0x94c4 = 0, DGROUP:0x9330 = 1 [BYTES seg9:0x4683].
    if (w.added != 0) {
        for (int k = 0; k < 4; ++k) w.w992 = add16(w.w992, mul16(weight[k], w.categories[static_cast<std::size_t>(k)]));
        for (int k = 0; k < 2; ++k) {
            const auto u = static_cast<std::size_t>(k);
            if (w.w4ee0[u] != 0)
                w.w992 = add16(w.w992, mul16(static_cast<int16_t>(hazard_class(w, w.hazard_class[u]).s8(5) + 3), w.w4ee0[u]));
        }
    }
    // The pool-2 units DGROUP:0x932e [BYTES seg9:0x46f6-0x4775].
    if (w.wave < 1)
        w.pool2_units = 0;
    else
        w.pool2_units = i16(rmod(rng.rand(), 3) + w.score_level / 2 - 1);
    if (w.pool2_units < 0) w.pool2_units = 0;
    if (w.detail < 3 || w.pool2_units < 5) {
        if (w.detail == 2 && 3 < w.pool2_units)
            w.pool2_units = 3;
        else if (w.detail < 2 && 2 < w.pool2_units)
            w.pool2_units = 2;
    } else {
        w.pool2_units = 4;
    }
    if (w.handover.demons_gate != 0 || w.kind == 6) w.pool2_units = 0;
    // The opponents against the budget [BYTES seg9:0x4775-0x4840].
    if (w.kind != 6 && w.kind != 7 && w.handover.demons_gate == 0) {
        const int16_t t = i16(w.budget - w.opponents * 40 - w.w992);
        if (50 < t) {
            const int16_t d = idiv16(add16(t, 60), 50);
            int16_t inc = 2;
            if (rmod(rng.rand(), d) < 3) inc = rmod(rng.rand(), d);
            w.opponents = add16(w.opponents, inc);
        }
        if (5 < w.opponents) w.opponents = 5;
        if (w.budget < add16(mul16(w.opponents, 45), w.w992))
            w.opponents = add16(w.opponents, idiv16(sub16(w.w992, w.budget), 45));
        if (w.opponents < w.w994) w.opponents = w.w994;
    }
}

// ---------------------------------------------------------------- the load seg9:0x22b4

// seg9:0x2828 as the load calls it, for wave 0 [BYTES seg9:0x241e seg9:0x2426], world.md
// "seg9:0x2828" and "DGROUP:0x9346".
void wave0_followups(World& w, SetupHost& h) {
    shell::Rng& rng = h.rng;
    auto type_of = [&h](const ShipRecord& s) -> int16_t {
        return h.design_type ? h.design_type(s.design_index) : int16_t{0};
    };
    // The per-ship seg13:0x2688 reads the roster (M5) [OPEN]. A ship of design type 7 sets the
    // local flag [bp - 0x16] (M3 s3 fixer, C026) [BYTES seg9:0x2839 seg9:0x2855 seg9:0x285b
    // seg9:0x2862].
    bool type7 = false;
    for (int i = 0; i <= w.last_ship && i < kShipCount; ++i)
        if (type_of(w.ship(i)) == 7) type7 = true;
    // The DGROUP:0x9346 rule (M3 s3 fixer, C017) [BYTES seg9:0x29d6 seg9:0x29e3 seg9:0x29ea
    // seg9:0x29f1 seg9:0x29fb seg9:0x2a05 seg9:0x2a09].
    for (int i = 0; i <= w.last_ship && i < kShipCount; ++i) {
        const ShipRecord& s = w.ship(i);
        if (s.bec != 0 && type_of(s) == 7 && ship_byte(s, 0x5e) == 0 && s.w54 == 14 && s.w56 == w.player)
            w.w9346 = ship_word(w.ship(w.player), 0xd6);
    }
    if (w.w9432 < 50)
        w.w9332 = 3;
    else if (w.w9432 < 180)
        w.w9332 = 2;
    else if (w.w9432 < 400)
        w.w9332 = 1;
    else
        w.w9332 = 0;
    w.w932c = 0;
    const int16_t n = w.w964a;
    // [bp - 8]: a negative DGROUP:0x964a reaches seg9:0x2ac1 with it unset; the port draws
    // nothing there [DESIGN] [BYTES seg9:0x2ab5 seg9:0x2aba seg9:0x2ac1].
    int a = 0;
    if (n == 0)
        a = 1;
    else if ((0 < n && n < 5))
        a = 2;
    else if (n < 5 || 8 < n) {
        if (n < 9 || 15 < n) {
            if (n >= 16) a = 4;
        } else if (1 < w.w9332 && !type7) {  // [BYTES seg9:0x2aa6 seg9:0x2aad] (M3 s3 fixer, C026)
            a = 3;
        } else {
            a = 4;
        }
    } else {
        a = 3;
    }
    static constexpr int kChance[5] = {0, 20, 50, 80, 0};
    if (a >= 1 && a <= 3) {
        if (rmod(rng.rand(), 100) < kChance[a]) w.w932c = 1;
    } else if (a == 4) {
        w.w932c = 1;
    }
    if (w.wave == 0) w.w932c = 0;
}

void load(World& w, SetupHost& h) {
    // Roles and resets [BYTES seg9:0x22c2-0x22e0].
    w.roles = wave_roles(false);
    // The DEMO command line's DScale draw (seg9:0x237e) needs the DEMO command line, which the
    // port does not run [DESIGN].
    if (w.wave == 0) wave0_followups(w, h);
    for (ShipRecord& s : w.ships) {  // [BYTES seg9:0x2430-0x2480]
        s.w44 = s.w46 = s.w48 = s.w4a = s.w4c = 0;
        for (int j = 0; j < 8; ++j) {
            set_ship_byte(s, 0x13e + j, 0);
            set_ship_byte(s, 0xc2 + j, 0);
        }
    }
    w.w937a.fill(0);
    generate(w, h.rng);
    // DGROUP:0x9432 [BYTES seg9:0x249e-0x24b2].
    w.w9432 = idiv16(mul16(w.w9432, w.w9432 < 251 ? 9 : 7), 10);
    // The pool counts [BYTES seg9:0x24b5-0x257e].
    Pools& p = w.pools;
    p.set_in_use(0, 8);
    p.set_in_use(1, 0);
    p.set_in_use(2, 0);
    p.set_in_use(w.roles.pool5, 0);
    p.set_allocated(0, 8);
    p.set_allocated(1, 2);
    p.set_allocated(2, shl16(w.pool2_units, 3));
    p.set_in_use(6, 0);
    p.set_in_use(7, 0);
    p.set_in_use(10, 0);
    p.set_in_use(11, 0);
    p.set_in_use(13, 0);
    p.set_allocated(w.roles.pool5, 0);
    p.set_allocated(6, 1);
    p.set_allocated(7, 1);
    p.set_allocated(10, 3);
    p.set_allocated(11, 3);
    p.set_allocated(13, 0);
    int16_t rocks = add16(w.categories[1], w.categories[0]);
    p.set_allocated(kPoolRocks, rocks);
    p.set_in_use(kPoolRocks, rocks);
    p.set_in_use(kPool9, w.categories[3]);
    p.set_allocated(kPool9, w.categories[3]);
    w.small_rocks = w.categories[0];
    if (127 < rocks) {  // the rock clamp [BYTES seg9:0x253c-0x255b]
        p.set_in_use(kPoolRocks, 120);
        p.set_allocated(kPoolRocks, 920);
        w.categories[0] = 60;
        w.categories[1] = 60;
    }
    const int16_t centres = mul16(w.gates, 2);
    p.set_allocated(kPoolGateCentres, centres);
    int16_t posts = add16(add16(w.categories[2], w.categories[4]), centres);
    if (80 < posts) posts = 80;
    p.set_in_use(kPoolGatePosts, posts);
    p.set_allocated(kPoolGatePosts, posts);
    if (w.handover.demons_gate != 0) {  // [BYTES seg9:0x2588-0x2596]
        w.roles.radar_first = 1;
        p.set_in_use(kPoolRocks, 0);
        w.w9336 = 0;
        w.opponents = 0;
    }
    if (w.kind == 6) {  // [BYTES seg9:0x25a3]
        p.set_in_use(2, 0);
        p.set_in_use(kPoolRocks, 0);
        p.set_allocated(2, 0);
        if (w.w9000 == 2 || (w.w9000 == 1 && 8 < w.score_level))
            w.w9348 = 1;
        else if ((w.w9000 == 0 && 3 < w.score_level) || w.score_level < 4)
            w.w9348 = 2;
        else
            w.w9348 = 3;
    }
    p.set_in_use(kPoolGateCentres, centres);
    // GlobalReAlloc of pools 0 .. DGROUP:0x9324 [BYTES seg9:0x25c0-0x2619].
    for (int q = 0; q <= w.roles.ships; ++q) p.realloc(q);
    apply_templates(w);
    // seg10:0x69a and the graphics loads of seg9:0x22b4 are the renderer's [DESIGN].
    w.w94e2 = i16((w.categories[3] + w.categories[4]) * 2 + w.categories[1] + w.categories[2] + w.categories[0]);
    say(h, "world: load seg9:0x22b4 wave " + std::to_string(w.wave) + ": " + describe_counts(w));
}

void apply_templates(World& w) {
    // seg10:0xb94 [BYTES seg10:0xb94-0x145f]
    Pools& p = w.pools;
    for (int i = 0; i < p.allocated(0); ++i) {  // icons [BYTES seg10:0xbe5 seg10:0xc34]
        ObjectRecord& r = p.rec(0, i);
        r.size = side(w, 32);
        r.extent = 256;
        r.gap = side(w, 6);
        r.frame = static_cast<int16_t>(i);
        r.mass = 160;
        r.hit_sound = 0x12;
        r.radar_brush = 2;
        r.radar_draw.set(kRadarDot);
        r.on_damage.set(kDamageIcon);
        r.b53 = 1;
    }
    for (int i = 0; i < p.allocated(1); ++i) {
        ObjectRecord& r = p.rec(1, i);
        r.size = side(w, 20);
        r.gap = side(w, 6);
        r.extent = 160;
        r.mass = 60;
        r.hit_sound = 0x12;
        r.radar_color.set(0x00808000);
        r.max_damage = 14;
        r.damage = 0;
        r.state = 1;
        r.x = -1000;
        r.y = -1000;
        r.on_destroy.set(kDestroyPickup);
        r.radar_draw.set(kRadarSmall);
        r.on_damage.set(kDamageGeneric);
        r.b53 = 0;
        r.blast_size = 1;
    }
    for (int i = 0; i < p.allocated(2); ++i) {
        ObjectRecord& r = p.rec(2, i);
        r.size = side(w, 14);
        r.gap = side(w, 2);
        r.extent = 104;
        r.mass = 55;
        r.blast_size = 0;
        r.hit_sound = 7;
        r.flame = 4;
        r.radar_color.set(0x0000ffff);
        r.radar_draw.set(kRadarSmall);
        r.on_damage.set(kDamagePool2);
        r.b53 = 0;
    }
    const int16_t rocks = p.allocated(kPoolRocks);
    for (int i = 0; i < rocks && i < p.records(kPoolRocks); ++i) {
        ObjectRecord& r = p.rec(kPoolRocks, i);
        if (i < sub16(rocks, w.small_rocks)) {  // big rocks
            r.size = side(w, 44);
            r.extent = 344;
            r.mass = 1400;
            r.radar_draw.set(kRadarBig);
            r.max_damage = 60;
            r.gap = side(w, 4);
        } else {  // small rocks
            r.size = side(w, 22);
            r.extent = 168;
            r.mass = 350;
            r.radar_draw.set(kRadarSmall);
            r.max_damage = 200;
            r.gap = side(w, 4);
            if (w.size == 2) r.gap = 4;
            if (w.size == 1 && w.w740 != 0) r.gap = 2;
            if (w.size == 0 && w.w740 != 0) r.gap = 1;
        }
        r.blast_size = 9;
        r.hit_sound = 8;
        r.radar_brush = 4;
        r.radar_color.set(0x00c0c0c0);
        r.on_damage.set(kDamageRock);
        r.b53 = 0;
        r.b54 = 0;
    }
    const int16_t c4 = w.w9214 == 0 ? 36 : 30, g4 = w.w9214 == 0 ? 6 : 4;
    for (int i = 0; i < p.allocated(kPool4); ++i) {
        ObjectRecord& r = p.rec(kPool4, i);
        r.size = side(w, c4);
        r.extent = shl16(c4, 3);
        r.gap = side(w, g4);
        r.mass = 110;
        r.hit_sound = 0x1e;
        r.radar_color.set(0x000000ff);
        r.blast_size = 10;
        r.radar_brush = 5;
        r.radar_draw.set(kRadarBig);
        r.on_damage.set(kDamagePool4);
        r.b53 = 1;
        r.b54 = 0;
    }
    for (int i = 0; i < p.allocated(5); ++i) {
        ObjectRecord& r = p.rec(5, i);
        r.mass = 120;
        r.hit_sound = 100;
        r.radar_color.set(0x000000ff);
        r.radar_draw.set(kRadarSmall);
        r.on_damage.set(kDamagePool6);
        r.b53 = 0;
        r.b54 = 0;
    }
    for (int i = 0; i < p.allocated(7); ++i) {
        ObjectRecord& r = p.rec(7, i);
        r.mass = 20;
        r.hit_sound = 6;
        r.radar_color.set(0x00007f00);
        r.max_damage = -1;
        r.blast_size = 5;
        r.radar_draw.set(kRadarSmall);
        r.on_damage.set(kDamageGeneric);
        r.b53 = 0;
        r.b54 = 0;
    }
    {  // pool 6, record 0
        ObjectRecord& r = p.rec(6, 0);
        r.size = side(w, 40);
        r.extent = 320;
        r.gap = side(w, 6);
        r.mass = 800;
        r.hit_sound = 8;
        r.state = 1;
        r.max_damage = 800;
        r.radar_brush = 1;
        r.blast_size = 7;
        r.on_destroy.set(kDestroyGeneric);
        r.radar_draw.set(kRadarShip);
        r.on_damage.set(kDamagePool7);
        r.b53 = 0;
        r.b54 = 0;
    }
    for (int i = 0; i < p.allocated(kPoolGatePosts); ++i) {
        ObjectRecord& r = p.rec(kPoolGatePosts, i);
        r.vx = 0;
        r.vy = 0;
        r.b53 = 0;
    }
    for (int i = 0; i < p.allocated(kPool9); ++i) {
        ObjectRecord& r = p.rec(kPool9, i);
        r.size = side(w, 38);
        r.extent = 296;
        r.gap = side(w, 4);
        r.mass = 360;
        r.state = 1;
        r.hit_sound = 0x12;
        r.radar_brush = 6;
        r.max_damage = 80;
        r.blast_size = 4;
        r.radar_draw.set(kRadarDot);
        r.on_damage.set(kDamageGeneric);
        r.on_destroy.set(kDestroyGeneric);
        r.b53 = 0;
        r.b54 = 0;
        r.flame = 7;
    }
    for (int i = 0; i < p.allocated(10); ++i) {
        ObjectRecord& r = p.rec(10, i);
        r.size = side(w, 12);
        r.extent = 96;
        r.gap = side(w, 6);
        r.vx = 0;
        r.vy = 0;
        r.mass = 11;
        r.state = 1;
        r.hit_sound = 6;
        r.radar_brush = 4;
        r.radar_color.set(0x007f7f7f);
        r.max_damage = -1;
        r.blast_size = 0;
        r.on_destroy.set(kDestroyGeneric);
        r.radar_draw.set(kRadarSmall);
        r.on_damage.set(kDamageGeneric);
        r.b53 = 0;
        r.b54 = 0;
    }
    for (int i = 0; i < p.allocated(11); ++i) {
        ObjectRecord& r = p.rec(11, i);
        r.gap = side(w, 4);
        r.mass = 101;
        r.state = 1;
        r.hit_sound = 0x12;
        r.radar_brush = 2;
        r.max_damage = 400;
        r.blast_size = 5;
        r.radar_draw.set(kRadarDot);
        r.on_damage.set(kDamageGeneric);
        r.on_destroy.set(kDestroyPool11);
        r.b53 = 1;
        r.b54 = 0;
    }
    // DGROUP:0x2408 (dword) and DGROUP:0x238c = 0 [BYTES seg10:0x1450].
}

// ---------------------------------------------------------------- programs and icons

namespace {

// seg9:0xac4(n, inc): bit n of the record's bitmap +0x1e2; inc first increments DGROUP:0x9200.
bool captured(World& w, int16_t n, bool inc) {
    if (inc) w.icon_number = add16(w.icon_number, 1);
    const DirView d = dv(w);
    const int16_t byte = idiv16(n, 8);  // sar with the sign fix: n / 8
    if (byte < 0 || 0x1e2 + byte >= static_cast<int>(kDirRecordSize)) return false;
    return (d.b[static_cast<std::size_t>(0x1e2 + byte)] & (1u << (imod16(n, 8) & 7))) != 0;
}

// seg9:0x94c: the first pool-0 record with state != 0, or -1 [BYTES seg9:0x94c seg9:0x979
// seg9:0x99a seg9:0x9a0].
int free_slot(World& w) {
    for (int s = 0; s < w.pools.in_use(kPoolIcons); ++s)
        if (rec(w, kPoolIcons, s).state != 0) return s;
    dv(w).at(0x1d0) = 1;
    w.icon_tries = add16(w.icon_tries, 1);
    return -1;
}

// seg9:0x9b0(s) [BYTES seg9:0x9d1-0xab7].
void activate(World& w, SetupHost& h, int s, const std::string& name) {
    if (captured(w, w.icon_number, true)) return;
    if (w.icon_tries < w.icon_limit) {
        IconRecord& ir = w.icons[static_cast<std::size_t>(s)];
        ir.w00 = 0;
        ir.w02 = 0;
        ir.b0b = 0xff;
        ir.b0a = 0xff;
        ir.w19 = 0;
        DirView d = dv(w);
        if (d.w(0x1d2 + 2 * s) < 0) d.set_w(0x1d2 + 2 * s, 0);
        ObjectRecord& r = rec(w, kPoolIcons, s);
        r.damage = d.w(0x1d2 + 2 * s);
        ir.number = sub16(w.icon_number, 1);
        ir.name = name;
        r.state = 0;
        place_icon(w, h.rng, s);  // seg10:0x736
        r.vx = static_cast<int16_t>(rmod(h.rng.rand(), 60) - 30);
        r.vy = static_cast<int16_t>(rmod(h.rng.rand(), 60) - 30);
        w.icons_placed = add16(w.icons_placed, 1);
    } else {
        dv(w).at(0x1d0) = 1;
    }
    w.icon_tries = add16(w.icon_tries, 1);
}

// seg9:0x278(type, slot) [BYTES seg9:0x278 seg9:0x2f1 seg9:0x2fa seg9:0x307 seg9:0x33c].
void placeholder(World& w, SetupHost& h, int type, int slot, const std::string& name) {
    const int flag = (0x105 < type && static_cast<unsigned>(type - 0x106) < 2) ? 0 : 0x20;
    if (slot == -1) {
        slot = free_slot(w);
        if (slot == -1) return;
    }
    StripCell& c = w.strip[static_cast<std::size_t>(slot)];
    c.kind = StripCell::Kind::kPlaceholder;
    c.flag = flag;
    c.name = name;  // seg9:0x352 cuts the extension when it draws
    activate(w, h, slot, name);
}

// seg9:0x156(path) [BYTES seg9:0x156 seg9:0x176 seg9:0x178 seg9:0x1a5 seg9:0x1d2 seg9:0x1fc
// seg9:0x24f seg9:0x251].
void icons_of_file(World& w, SetupHost& h, const std::string& path, const std::string& name) {
    const int n = h.icons.count(path);
    h.walker.icon_call("seg9:0x156", path, -1, n);
    if (n == 0) {
        const bool any = h.icons.cell(path, 0).has_value();
        h.walker.icon_call("seg9:0x156", path, 0, any ? 2 : 0);
        placeholder(w, h, any ? 0x107 : 0, -1, name);
        return;
    }
    for (int k = 0;; ++k) {
        if (9 < w.icon_tries) return;
        if (n <= k) break;
        if (captured(w, w.icon_number, false)) {
            w.icon_number = add16(w.icon_number, 1);
            continue;
        }
        const int slot = free_slot(w);
        if (slot == -1) return;
        const std::optional<fmt::IconCell> cell = h.icons.cell(path, k);
        h.walker.icon_call("seg9:0x156", path, k, cell ? 2 : 0);
        StripCell& c = w.strip[static_cast<std::size_t>(slot)];
        if (cell) {  // PatBlt BLACKNESS, DrawIcon, DestroyIcon
            c.kind = StripCell::Kind::kIcon;
            c.cell = *cell;
        }
        if (blank_cell(c))
            placeholder(w, h, 0, slot, name);
        else
            activate(w, h, slot, name);
    }
}

}  // namespace

bool blank_cell(const StripCell& c) {
    switch (c.kind) {
    case StripCell::Kind::kBlack: return true;
    case StripCell::Kind::kIcon: return fmt::is_uniform(c.cell, c.cell.at(0, 0));  // GetPixel(0, 0)
    case StripCell::Kind::kPlaceholder: return false;  // the frame and the name are not black
    }
    return false;
}

void fill_icons(World& w, SetupHost& h) {
    // seg9:0xb2a [BYTES seg9:0xb2a seg9:0xbf5 seg9:0xbf7]
    DirView d = dv(w);
    for (int s = 0; s < w.pools.in_use(kPoolIcons); ++s) {
        if (s < kIconRecords) w.icons[static_cast<std::size_t>(s)].number = -1;
        ObjectRecord& r = rec(w, kPoolIcons, s);
        if (w.added != 0) {
            r.damage = -1;
            d.set_w(0x1d2 + 2 * s, r.damage);
        }
        r.state = 1;
    }
    for (int s = 0; s < w.pools.in_use(1); ++s) rec(w, 1, s).state = 1;
    if (w.added != 0) {
        d.at(0x1d1) = 0;
        d.set_w(0x150, 0);
        d.set_w(0x14e, d.w(0x150));
    }
    // seg9:0x0(8) [BYTES seg9:0x0 seg9:0x2a seg9:0x46 seg9:0x58 seg9:0x8e seg9:0xb8 seg9:0x100]
    w.icon_limit = 8;
    w.icons_placed = 0;
    w.icon_number = 0;
    w.icon_tries = 0;
    d.at(0x1d0) = 2;
    const std::string dir = w.handover.directory;
    for (const char* mask : {"*.exe", "*.dll"}) {  // DGROUP:0x71c, DGROUP:0x722
        nav::Walker::Find f = h.walker.find("seg9:0x0", dir + mask, nav::kAttrReadOnly);
        nav::FindEntry e;
        bool ok = f.next(e);
        while (ok && w.icon_tries < 10) {
            icons_of_file(w, h, dir + e.name, e.name);
            ok = f.next(e);
        }
    }
    if (w.icons_placed != 0 && d.b[0x1d0] != 1) d.at(0x1d0) = 0;
    w.icons_a = w.icon_tries < w.icon_limit ? w.icon_tries : w.icon_limit;
    w.icons_b = w.icons_a;
    w.icons_c = w.icons_a;
    w.icon_limit = 8;
    say(h, "world: icons seg9:0x0 " + std::to_string(w.icons_a) + " of " +
               std::to_string(w.icon_tries) + " candidates, " + std::to_string(w.icons_placed) +
               " placed, icon number " + std::to_string(w.icon_number));
}

// ---------------------------------------------------------------- placement

Rect16 centre_exclusion(const World& w) {
    // [BYTES seg10:0x998 seg10:0x99d]: all 16-bit, << DGROUP:0x9150.
    const int16_t cx = idiv16(kWorldCells, 2), cy = idiv16(kWorldCells, 2);
    const int16_t hw = idiv16(w.W, 2), hh = idiv16(w.H, 2);
    return Rect16{shl16(sub16(cx, hw), w.shift), shl16(sub16(cy, hh), w.shift),
                  shl16(add16(cx, hw), w.shift), shl16(add16(cy, hh), w.shift)};
}

void place_rock(World& w, shell::Rng& rng, int i) {
    // seg10:0x864 [BYTES seg10:0x8e4-0x994]
    ObjectRecord& r = rec(w, kPoolRocks, i);
    const Rect16 ex = centre_exclusion(w);
    do {
        r.b50 = 0;
        r.x = random_coord(rng, kWorldCells);
        r.y = random_coord(rng, kWorldCells);
        off_screen(r);
        r.vx = static_cast<int16_t>(-(rmod(rng.rand(), 50) - 25));
        r.vy = static_cast<int16_t>(-(rmod(rng.rand(), 50) - 25));
        if ((r.vx < 0 ? -r.vx : r.vx) < 10 && (r.vy < 0 ? -r.vy : r.vy) < 10) r.vx = add16(r.vx, 20);
        r.state = 0;
        r.flame = 0;
        r.b2e = 0;
        r.d6a.set(0);
        r.angle = rmod(rng.rand(), 1700);
    } while (pt_in_rect(ex, r.x, r.y));
}

void place_pool9(World& w, shell::Rng& rng, int i) {
    // seg10:0xa26 [BYTES seg10:0xa3a-0xaf3]
    ObjectRecord& r = rec(w, kPool9, i);
    const Rect16 ex = centre_exclusion(w);
    do {
        r.x = random_coord(rng, kWorldCells);
        r.y = random_coord(rng, kWorldCells);
        off_screen(r);
        r.vx = static_cast<int16_t>(-(rmod(rng.rand(), 50) - 25));
        r.vy = static_cast<int16_t>(-(rmod(rng.rand(), 50) - 25));
        r.state = 0;
        r.b2e = 0;
        r.angle = rmod(rng.rand(), 900);
    } while (pt_in_rect(ex, r.x, r.y));
}

void place_pool4(World& w, shell::Rng& rng, int i) {
    // seg9:0xdf6 [BYTES seg9:0xdf6 seg9:0xe81 seg9:0xe99 seg9:0xfe1]
    const Rect16 ex = centre_exclusion(w);
    bool again = true;
    while (again) {
        ObjectRecord& r = rec(w, kPool4, i);
        r.x = random_coord(rng, kWorldCells);
        r.y = random_coord(rng, kWorldCells);
        off_screen(r);
        r.vx = static_cast<int16_t>(-(rmod(rng.rand(), 50) - 25));
        r.vy = static_cast<int16_t>(-(rmod(rng.rand(), 50) - 25));
        r.angle = 0;
        if ((w.w9214 == 3 || w.w9214 == 4) && i % 2 == 1) r.angle = 5;
        if (w.w9214 == 1 || w.w9214 == 2 || (w.w9214 == 4 && (i & 1) == 0))
            r.on_destroy.set(kDestroyPickup);
        else
            r.on_destroy.set(kDestroyGeneric);
        r.flame = 0;
        r.damage = 0;
        r.b50 = 0;
        r.race_status = 0;
        r.state = w.kind == 6 ? 1 : 0;
        again = pt_in_rect(ex, r.x, r.y);
        if (w.pools.in_use(kPool4) == 17 && i == 16) {
            // The ring of 17: the offsets DGROUP:0x104c + 4k (y) and 0x104e + 4k (x) from the
            // file, times 208 [BYTES DGROUP:0x104c seg9:0xfe1].
            static constexpr int16_t kOff[17][2] = {{0, 0},   {0, -16},  {6, -15},  {11, -11}, {15, -6},
                                                    {16, 0},  {15, 6},   {11, 11},  {6, 15},   {0, 16},
                                                    {-6, 16}, {-11, 11}, {-15, 6},  {-16, 0},  {-15, -6},
                                                    {-11, -11}, {-6, -15}};
            ObjectRecord& r0 = rec(w, kPool4, 0);
            r0.vy = 0;
            r0.vx = r0.vy;
            for (int k = 1; k < 17; ++k) {
                ObjectRecord& q = rec(w, kPool4, k);
                q.y = imod16(add16(mul16(kOff[k][0], 208), r0.y), shl16(kWorldCells, 3));
                q.x = imod16(add16(mul16(kOff[k][1], 208), r0.x), shl16(kWorldCells, 3));
                q.vy = 0;
                q.vx = q.vy;
            }
        }
    }
}

void place_icon(World& w, shell::Rng& rng, int i) {
    // seg10:0x736 [BYTES seg10:0x736 seg10:0x757 seg10:0x776]
    ObjectRecord& r = rec(w, kPoolIcons, i);
    const Rect16 ex = centre_exclusion(w);
    do {
        r.b54 = 0;
        r.x = random_coord(rng, kWorldCells);
        r.y = random_coord(rng, kWorldCells);
        off_screen(r);
        r.flame = 0;
    } while (pt_in_rect(ex, r.x, r.y));
}

namespace {

// seg9:0x4896 for a wave without special places: it clears the table and stores it in the
// record. With hazards of category 2 or 4 it builds special places, which is M5 [OPEN]
// [BYTES seg9:0x4896].
void special_places(World& w, SetupHost& h) {
    int16_t n = w.categories[2];
    if (w.w4ee0[0] != 0 &&
        (hazard_class(w, w.hazard_class[0]).b[2] != 0 || rmod(h.rng.rand(), 3) < 2))
        n = add16(n, w.w4ee0[0]);
    if (w.w4ee0[1] != 0 && hazard_class(w, w.hazard_class[1]).b[2] != 0) n = add16(n, w.w4ee0[1]);
    for (auto& p : w.places) {
        p.end = 0;
        p.first = 0;
    }
    int16_t level = add16(idiv16(w.score_level, 2), 5);
    if (9 < level) level = 9;
    if (idiv16(mul16(level, n), 9) != 0) {
        say(h, "world: TODO special places seg9:0x4896 (M5)");
    }
    w.w9344 = 0;
    if (40 < w.pools.allocated(kPoolGatePosts)) {
        // The cap of 40 (seg9:0x4896's tail) with its record updates [OPEN].
        w.pools.set_in_use(kPoolGatePosts, 40);
        w.pools.set_allocated(kPoolGatePosts, 40);
    }
    DirView d = dv(w);
    for (int k = 0; k < 10; ++k) {
        const SpecialPlace& p = w.places[static_cast<std::size_t>(k)];
        d.at(0x232 + k) = static_cast<std::uint8_t>(pw(p, 0));
        d.at(0x23c + k) = static_cast<std::uint8_t>(pw(p, 1));
        d.at(0x246 + k) = static_cast<std::uint8_t>(pw(p, 2) / 80);
        d.at(0x250 + k) = static_cast<std::uint8_t>(pw(p, 3));
        d.at(0x25a + k) = static_cast<std::uint8_t>(pw(p, 4));
        d.set_w(0x26f + 4 * k, pw(p, 5));
        d.set_w(0x271 + 4 * k, pw(p, 6));
        d.at(0x265 + k) = static_cast<std::uint8_t>(pw(p, 7));
    }
    d.at(0x264) = 0;
}

}  // namespace

void place_gates(World& w, SetupHost& h) {
    // seg9:0x10d4 [BYTES seg9:0x10d4 seg9:0x11e7 seg9:0x1218 seg9:0x18b8 seg9:0x18cc seg9:0x1aaf
    // seg9:0x1be0]
    shell::Rng& rng = h.rng;
    Pools& p = w.pools;
    DirView d = dv(w);
    const int16_t n_posts = mul16(w.gates, 2);
    const int16_t cells_x = kWorldCells, cells_y = kWorldCells;
    for (int k = 0; k < n_posts; ++k) {
        ObjectRecord& r = p.rec(kPoolGatePosts, k);
        off_screen(r);
        r.damage = w.added == 0 ? d.w(0xae + 2 * k) : int16_t(0);
        int16_t x, y;
        if (w.added == 0) {
            x = d.w(4 * k);
            y = d.w(4 * k + 2);
        } else {  // two numbers drawn and not used [BYTES seg9:0x10d4]
            x = mul16(add16(rmod(rng.rand(), sub16(cells_x, 60)), 30), 8);
            y = mul16(add16(rmod(rng.rand(), sub16(cells_y, 60)), 30), 8);
        }
        r.state = 0;
        r.gap = side(w, 6);
        r.flame = 2;
        r.b54 = 1;
        w.pool8[static_cast<std::size_t>(k)].at(0x3d80 - kDgPool8) = -1;
        r.mass = 0xdac;
        r.size = side(w, 34);
        r.extent = 272;
        r.hit_sound = 8;
        r.radar_brush = 1;
        r.radar_draw.set(kRadarDot);
        r.on_damage.set(kDamageRock);
        w.pool8[static_cast<std::size_t>(k)].at(0x3d6c - kDgPool8) = 3;
        if ((k & 1) != 0) continue;
        const int g = k / 2;
        GateRecord& gr = w.gate[static_cast<std::size_t>(g)];
        gr.width = 1200;
        gr.w0e = 80;
        if (w.kind == 6) {
            const auto c = static_cast<std::size_t>(w.course) % 3;
            x = i16(w.race_a120[2 * c] - idiv16(gr.width, 2) - 160);
            y = w.race_a120[2 * c + 1];
        } else if (w.added != 0) {
            x = shl16(rmod(rng.rand(), sub16(cells_x, 300)), 3);
            y = mul16(add16(rmod(rng.rand(), sub16(cells_y, 200)), 200), 8);
        }
        if (w.added == 0 || w.kind == 6) {
            // Objects under the gate move down by 1600 [BYTES seg9:0x10d4].
            const Rect16 gate = rect16(x - 720, y - 720, x + 2400, y + 1200);
            for (int pool : {0, 3, 4, 9}) {
                for (int a = 0; a < p.in_use(pool); ++a) {
                    ObjectRecord& o = p.rec(pool, a);
                    const Rect16 b = rect16(o.x - 160, o.y - 160, o.x + 320, o.y + 320);
                    if (intersect(gate, b)) o.y = imod16(add16(o.y, 1600), shl16(cells_y, 3));
                }
            }
        } else {
            // Redrawn while the gate meets any in-use object of pools 0-13 [BYTES seg9:0x10d4].
            bool clear = false;
            while (!clear) {
                const Rect16 gate = rect16(x - 720, y - 720, x + 2400, y + 1200);
                clear = true;
                for (int pool = 0; pool < w.roles.ships; ++pool) {
                    for (int a = 0; a < p.in_use(pool); ++a) {
                        const ObjectRecord& o = p.rec(pool, a);
                        const Rect16 b = rect16(o.x - 160, o.y - 160, o.x + 320, o.y + 320);
                        if (intersect(gate, b)) {
                            clear = false;
                            x = shl16(rmod(rng.rand(), sub16(cells_x, 300)), 3);
                            y = mul16(add16(rmod(rng.rand(), sub16(cells_y, 200)), 200), 8);
                            break;
                        }
                    }
                }
            }
        }
        ObjectRecord& post = p.rec(kPoolGatePosts, k);
        ObjectRecord& post2 = p.rec(kPoolGatePosts, k + 1);
        post.x = x;
        post.y = y;
        post2.x = x;
        post2.y = y;
        post2.x = add16(post2.x, add16(post.extent, gr.width));
        gr.post = static_cast<int16_t>(k);
        ObjectRecord& c0 = p.rec(kPoolGateCentres, k);
        ObjectRecord& c1 = p.rec(kPoolGateCentres, k + 1);
        c0.x = add16(x, add16(idiv16(gr.width, 2), 160));
        c0.y = sub16(y, 150);
        c1.x = add16(x, add16(idiv16(gr.width, 2), 160));
        c1.y = y;
        const int16_t bx = add16(post.x, post.extent);
        const int16_t by = add16(idiv16(post.extent, 2), post.y);
        gr.beam = rect16(bx, by - 24, bx + gr.width, by + 24);
        c0.mass = 0;
        c0.size = 0;
        c0.extent = 0;
        c1.mass = 0;
        c1.size = 0;
        if (k == 2) w.gate[1].w12 = 1;
        if (k == 4) w.gate[2].w12 = 2;
    }
    if (w.added != 0 && w.kind != 6) special_places(w, h);

    // The hazards [BYTES seg9:0x10d4 seg9:0x18b8 seg9:0x18cc seg9:0x1aaf seg9:0x1be0].
    for (int k = n_posts; k < p.in_use(kPoolGatePosts); ++k) {
        ObjectRecord& r = p.rec(kPoolGatePosts, k);
        Pool8Entry& e = w.pool8[static_cast<std::size_t>(k)];
        off_screen(r);
        if (w.added == 0) {
            r.damage = d.w(0xae + 2 * k);
            e.at(0x3d7e - kDgPool8) = d.w(0xfe + 2 * k);
        } else {
            r.damage = 0;
        }
        int16_t x = r.x, y = r.y;
        if (w.added == 0 && w.kind != 6) {
            x = d.w(4 * k);
            y = d.w(4 * k + 2);
            e.at(0x3d82 - kDgPool8) = -1;
            for (int q = 0; q < w.w9344; ++q)
                if (in_place_range(w.places[static_cast<std::size_t>(q)], k)) e.at(0x3d82 - kDgPool8) = static_cast<int16_t>(q);
        } else {
            bool found = false;
            for (int q = 0; q < w.w9344; ++q) {
                if (in_place_range(w.places[static_cast<std::size_t>(q)], k)) {
                    found = true;
                    e.at(0x3d82 - kDgPool8) = static_cast<int16_t>(q);
                    r.b54 = 1;
                    break;
                }
            }
            if (!found) {
                e.at(0x3d82 - kDgPool8) = -1;
                if (w.kind != 6) {
                    bool ok = false;
                    while (!ok) {
                        ok = true;
                        x = shl16(rmod(rng.rand(), cells_x), 3);
                        y = shl16(rmod(rng.rand(), cells_y), 3);
                        int16_t dx, dy;
                        for (int g = 0; g < w.gates; ++g) {
                            const ObjectRecord& c = p.rec(kPoolGateCentres, 2 * g);
                            if (distance_cells(c.x, c.y, x, y, dx, dy, cells_x, cells_y) < 150) ok = false;
                        }
                        for (int q = 0; q < w.w9344; ++q) {
                            const SpecialPlace& pl = w.places[static_cast<std::size_t>(q)];
                            if (distance_cells(pw(pl, 5), pw(pl, 6), x, y, dx, dy, cells_x, cells_y) <
                                add16(sar16(pw(pl, 2), 3), 200))
                                ok = false;
                        }
                        for (int q = 0; q < k; ++q) {
                            const ObjectRecord& o = p.rec(kPoolGatePosts, q);
                            if (distance_cells(o.x, o.y, x, y, dx, dy, cells_x, cells_y) < 80) ok = false;
                        }
                    }
                }
                // A race takes seg9:0x225a's position (M6) [OPEN].
            }
        }
        r.x = x;
        r.y = y;
        r.state = 0;
        r.gap = side(w, 5);
        r.b54 = 1;
        if (k < add16(n_posts, w.categories[2])) {
            r.mass = 0xdac;
            r.hit_sound = 8;
            r.radar_brush = 4;
            r.radar_draw.set(kRadarDot);
            r.on_damage.set(kDamageGeneric);
            r.on_destroy.set(kDestroyGeneric);
            const int16_t cls = k < add16(n_posts, w.w4ee4[0]) ? w.w4ee8[0] : w.w4ee8[1];
            e.at(0x3d5a - kDgPool8) = hazard_class(w, cls).s8(3);
            e.at(0x3d80 - kDgPool8) = cls;
            if (cls == 3) {
                r.gap = side(w, 4);
                r.size = side(w, 14);
                r.extent = 0x70;
                r.max_damage = 12000;
                r.blast_size = 0;
            } else {
                if (cls == 4) {
                    r.gap = side(w, 4);
                    r.size = side(w, 20);
                    r.extent = 0x98;
                } else {
                    r.gap = side(w, 6);
                    r.size = side(w, 34);
                    r.extent = 0x110;
                }
                r.max_damage = 120;
                r.blast_size = 8;
            }
            e.at(0x3d6c - kDgPool8) = 1;
            if (e.at(0x3d5a - kDgPool8) != 4) r.flame = 2;
        } else {
            e.at(0x3d6c - kDgPool8) = 2;
            const std::size_t sel = (add16(add16(n_posts, w.categories[2]), w.w4ee0[0]) <= k) ? 1 : 0;
            const int16_t cls = w.hazard_class[sel];
            HazardView hc = hazard_class(w, cls);
            e.at(0x3d70 - kDgPool8) = hc.w(0xe);
            e.at(0x3d72 - kDgPool8) = hc.w(0x10);
            e.at(0x3d7a - kDgPool8) = hc.s8(1);
            e.at(0x3d5a - kDgPool8) = hc.s8(3);
            e.at(0x3d68 - kDgPool8) = hc.s8(4);
            r.mass = hc.w(0xa);
            r.b54 = w.kind == 6 ? 1 : hc.b[2];
            r.blast_size = 9;
            r.max_damage = hc.w(0xc);
            if (r.max_damage < (r.damage < 0 ? -r.damage : r.damage)) r.state = 1;
            if (e.at(0x3d5a - kDgPool8) != 4) r.flame = 5;
            if (w.added != 0 && e.at(0x3d5a - kDgPool8) == -1) {
                if ((rng.rand() & 1) == 0) e.at(0x3d5a - kDgPool8) = -3;
            } else if (w.added == 0 && e.at(0x3d5a - kDgPool8) == -1 && r.damage < 0) {
                r.damage = static_cast<int16_t>(-r.damage);
                e.at(0x3d5a - kDgPool8) = -3;
            }
            e.at(0x3d80 - kDgPool8) = cls;
            r.hit_sound = 6;
            r.radar_brush = 4;
            r.radar_draw.set(kRadarDot);
            r.on_damage.set(kDamageGeneric);
            r.on_destroy.set(kDestroyPickup);
            // seg10:0x18b0 into DGROUP:0x3d6e and seg10:0x533a are M5 [OPEN].
            r.gap = sprite_side(hc.s8(9), w.size);
            r.size = sprite_side(hc.s8(8), w.size);
            r.extent = shl16(hc.s8(8), 3);
        }
        if (e.at(0x3d5a - kDgPool8) == 1)
            e.at(0x3d60 - kDgPool8) = static_cast<int16_t>(rmod(rng.rand(), 7) * 500 + 600);
        else
            e.at(0x3d60 - kDgPool8) = static_cast<int16_t>(rmod(rng.rand(), 7) * 500 + 1200);
        e.at(0x3d50 - kDgPool8) = 0;
        e.at(0x3d52 - kDgPool8) = 0;
        e.at(0x3d5e - kDgPool8) = static_cast<int16_t>(rmod(rng.rand(), mul16(add16(w.wave, 1), 200)) + 20);
        int16_t t = mul16(add16(w.wave, 5), 3);
        e.at(0x3d5c - kDgPool8) = 22 < t ? int16_t(22) : t;
        e.at(0x3d58 - kDgPool8) = rmod(rng.rand(), 2);
        if (e.at(0x3d80 - kDgPool8) != 4 && e.at(0x3d80 - kDgPool8) != 3) {
            r.x = sub16(r.x, idiv16(r.extent, 2));
            r.y = sub16(r.y, idiv16(r.extent, 2));
        }
        for (int q = 0; q < w.w9344; ++q) {
            if (in_place_range(w.places[static_cast<std::size_t>(q)], k)) {
                r.b54 = 1;
                break;
            }
        }
    }
    // seg10:0x1dd8 [OPEN]. Icons at the special places [BYTES seg9:0x10d4].
    int16_t slot = 0;
    for (int q = 0; q < w.w9344; ++q) {
        const SpecialPlace& pl = w.places[static_cast<std::size_t>(q)];
        const bool first = q == 0;
        if (first || (pw(w.places[static_cast<std::size_t>(q - 1)], 5) != pw(pl, 5) &&
                      pw(w.places[static_cast<std::size_t>(q - 1)], 6) != pw(pl, 6))) {
            if (slot < p.records(kPoolIcons)) {
                ObjectRecord& o = p.rec(kPoolIcons, slot);
                o.b54 = 1;
                o.vy = 0;
                o.vx = o.vy;
                o.x = sub16(pw(pl, 5), 0x80);
                o.y = sub16(pw(pl, 6), 0x80);
            }
            slot = add16(slot, 1);
            w.w992 = add16(w.w992, mul16(add16(sub16(pw(pl, 1), pw(pl, 0)), 5), 2));
        }
    }
    if (w.kind == 6 && p.in_use(kPoolIcons) != 0) {  // the race lines the icons up (M6)
        const auto c = static_cast<std::size_t>(w.course) % 3;
        const int16_t y0 = w.race_9ea8[2 * c];
        const int16_t x0 = p.rec(kPoolGateCentres, 0).x;
        const int16_t n = p.in_use(kPoolIcons);
        for (int k = 0; k < n; ++k) {
            ObjectRecord& o = p.rec(kPoolIcons, k);
            o.b54 = 1;
            o.vy = 0;
            o.vx = 0;
            o.x = add16(mul16(i16(idiv16(mul16(sub16(n, 1), 64), 2) - k * 64 - 16), 8), x0);
            o.y = sub16(y0, 0x168);
        }
    }
    d.at(0xad) = static_cast<std::uint8_t>(w.gates);
    if (w.kind == 6) d.at(0xad) = static_cast<std::uint8_t>(-1 - w.course);
    for (int k = 0; k < p.in_use(kPoolGatePosts) && 4 * k + 3 < static_cast<int>(kDirRecordSize); ++k) {
        d.set_w(4 * k, p.rec(kPoolGatePosts, k).x);
        d.set_w(4 * k + 2, p.rec(kPoolGatePosts, k).y);
    }
    // seg9:0x2012 for a race (M6) [OPEN].
}

void ship_setup_minimal(World& w) {
    w.last_ship = 0;
    w.player = 0;
    w.pools.resize(kPoolShips, 1);
    w.pools.set_in_use(kPoolShips, 1);
    ObjectRecord& r = w.pools.rec(kPoolShips, 0);
    r.frame = 0;  // seg14:0x15fa: heading 0 [BYTES seg14:0x15fa]
}

void place(World& w, SetupHost& h) {
    // seg9:0x6624 [BYTES seg9:0x6624 seg9:0x6633 seg9:0x663c]
    if (w.placed != 0) return;
    w.placed = 1;
    // The status dialog 0x65 (CreateDialog) while the state is not 3 is the renderer's [DESIGN].
    Pools& p = w.pools;
    w.frames = 0;
    w.w9510 = 0x65;
    for (int i = 0; i < p.allocated(6); ++i) p.rec(6, i).state = 1;  // DGROUP:0x66ec + 0x1c i = 0x191
    for (int i = 0; i < p.allocated(10); ++i) p.rec(10, i).state = 1;
    // The link slots DGROUP:0x65cc + 0x20 k, k < DGROUP:0x9300: both ends' pools -1 (M3 s4 DRAW)
    // [BYTES seg9:0x66ce seg9:0x66dd seg9:0x66e1].
    for (int k = 0; k < p.allocated(10) && k < kLinkSlots; ++k) {
        w.links[static_cast<std::size_t>(k)].pool_a = -1;
        w.links[static_cast<std::size_t>(k)].pool_b = -1;
    }
    w.w226c.fill(-1);
    w.w9456 = 0;
    w.w9430 = 0;
    for (BeamSlot& b : w.beams) {
        b.index = -1;
        b.limit = 50;
    }
    clear_effects(w.effects);
    w.w58cc.fill(-1);
    for (int i = 0; i < p.allocated(w.roles.pool5); ++i) p.rec(w.roles.pool5, i).state = 1;
    for (World::Particle& q : w.particles) {
        q.w08 = 0;
        q.x = -100;
        q.y = -100;
    }
    w.w9e49 = 10;
    // The beam brush index DGROUP:0x9492 and its step DGROUP:0x9494 (M3 s4 DRAW) [BYTES
    // seg9:0x67f4 seg9:0x67f7 seg9:0x67fa].
    w.beam_step = 1;
    w.beam_index = 1;
    for (int i = 0; i < p.in_use(kPoolRocks); ++i) place_rock(w, h.rng, i);  // [BYTES seg9:0x6807]
    for (int i = 0; i < p.in_use(kPool9); ++i) place_pool9(w, h.rng, i);     // [BYTES seg9:0x6822]
    w.w9600 = 1;
    for (int i = 0; i < p.in_use(kPool4); ++i) place_pool4(w, h.rng, i);     // [BYTES seg9:0x6843]
    place_gates(w, h);                                                         // [BYTES seg9:0x6854]
    w.clock = now_of(h);                                                  // [BYTES seg9:0x6859]
    p.set_in_use(7, 0);
    w.w9e49 = 10;
    p.rec(6, 0).state = 1;
    // seg14:0x3eda, seg14:0x0(-1), seg14:0x4074 [BYTES seg9:0x685e seg9:0x6883 seg9:0x688a seg9:0x6890]
    if (h.ship_setup)
        h.ship_setup(w);
    else
        ship_setup_minimal(w);
    w.w9140 = 1;
    for (int q = 0; q < kStartupPools; ++q) {  // [BYTES seg9:0x6624]
        for (int i = 0; i < p.allocated(q) && i < p.records(q); ++i) {
            ObjectRecord& r = p.rec(q, i);
            r.src_y = r.gap;
            r.b2e = 0;
            r.owner_pool = -1;
            r.w36 = -1;
        }
    }
    // A race's seg9:0x6db0 and its sounds are M6 [OPEN].
    w.w1e6c = w.clock + 10000;
    // seg11:0xae94 and seg11:0x2f8c (the weapon picture) run only for wave 1 or a restored game;
    // the program runs them at the wave start (src/hud/wave_win32.cpp) [BYTES seg9:0x6951
    // seg9:0x6958 seg9:0x695c seg9:0x695e seg9:0x6963] (M3 s4 fixer, review M6).
    w.picture_due = w.wave == 1 || w.restored != 0;
    w.restored = 0;
    say(h, "world: placement seg9:0x6624 wave " + std::to_string(w.wave) + ", gate 0 at (" +
               std::to_string(p.rec(kPoolGatePosts, 0).x) + ", " + std::to_string(p.rec(kPoolGatePosts, 0).y) +
               "), " + std::to_string(p.in_use(kPoolRocks)) + " rocks");
}

// ---------------------------------------------------------------- the entries

void state1_load(World& w, SetupHost& h) {
    // seg10:0x0 -> seg9:0x22b4(0) [BYTES seg10:0xad]
    load(w, h);
}

void enter_briefing(World& w, SetupHost& h) {
    // seg9:0x5cca [BYTES seg9:0x5cca]
    w.placed = 0;  // DGROUP:0x93b0 = 0 [BYTES seg9:0x5d24]
    load(w, h);        // seg9:0x22b4(1) [BYTES seg9:0x5d3c]
    w.pools.rec(6, 0).state = 1;
    w.w9e49 = 10;
    w.w952e = -1;
    // seg10:0x5b30: the strip for N = the pool-0 in-use count, blacked [BYTES seg10:0x5b3d].
    w.strip_slots = w.pools.in_use(kPoolIcons);
    for (StripCell& c : w.strip) c = StripCell{};
    fill_icons(w, h);  // seg9:0xb2a [BYTES seg9:0x5d66]
    // seg10:0x506c and seg7:0x233c(0, k, 0) for k < 8 are the drawing's and M4's [OPEN].
    w.w8e34.fill(-1);
    // seg10:0x6308(0xd, -1, -1, -1) [OPEN]; seg9:0x94ba [BYTES seg9:0x94ba]:
    for (int k = 0; k < kIconRecords; ++k) {
        DirView d = dv(w);
        // seg7:0x2512(k, byte - 1) for a marked slot is M4 [OPEN].
        if (d.b[static_cast<std::size_t>(0x1c8 + k)] == 2 && k < w.pools.records(kPoolIcons))
            w.pools.rec(kPoolIcons, k).flame = 0x9c;
        if (w.kind == 2 && w.score_level != 0) {
            const int r = h.rng.rand();
            (void)r;  // seg7:0x2512(k, 0) when r % 3 == 0 [OPEN]
        }
    }
    w.w9130 = 0;
    w.w913a = 0;
    w.w913c = 0;
    w.w9434 = 0;
    int16_t live = 0;
    for (int k = 0; k < kIconRecords; ++k)
        if (w.pools.rec(kPoolIcons, k).state == 0) ++live;
    w.w937a.fill(0);
    w.w992 = add16(w.w992, mul16(live, 4));
    w.w8680.fill(-1);
    w.wave = add16(w.wave, 1);  // [BYTES seg9:0x5e58]
    place(w, h);                // [BYTES seg9:0x5e5d]
    w.wave = sub16(w.wave, 1);  // [BYTES seg9:0x5e60]
    // The pool-2 rewrite [BYTES seg9:0x5e9c-0x5ecd].
    if (live == 0) {
        w.pool2_units = 0;
    } else if (add16(w.budget, 10) < add16(mul16(mul16(live, w.pool2_units), 4), w.w992)) {
        w.pool2_units = idiv16(add16(sub16(w.budget, w.w992), 20), shl16(live, 2));
        if (w.pool2_units < 0) w.pool2_units = 0;
    }
    if (w.complex == 0) {
        if (1 < w.pool2_units) w.pool2_units = 1;
    } else if (w.complex == 1 && 2 < w.pool2_units) {
        w.pool2_units = 2;
    }
    w.w992 = add16(w.w992, mul16(mul16(live, w.pool2_units), 4));
    w.pools.set_allocated(2, shl16(w.pool2_units, 3));
    w.wave = add16(w.wave, 1);  // [BYTES seg9:0x5fbf]
    if (w.kind == 6) w.w94ca = w.wave;
}

void start_wave(World& w, SetupHost& h) {
    // seg9:0x60bc [BYTES seg9:0x60ff seg9:0x6320]
    w.w0a76 = 2;
    w.w9e49 = 10;
    // seg8:0x190e at seg9:0x6117: the stars, six rand() each before the chance block (M3 s4
    // DRAW) [BYTES seg9:0x6117]. seg13:0x2dcc (the radar brushes, called at seg9:0x611c) is
    // unread and not ported [OPEN seg13:0x2dcc] [BYTES seg9:0x611c].
    init_stars(w, h.rng);
    w.w913e = 0;
    ShipRecord& me = w.ships[static_cast<std::size_t>(w.player)];
    const int16_t since = sub16(w.wave, w.w93ac);
    // world.md "Chance block"; the edition DGROUP:0x920c neither 0 nor 3 (M3 s3 fixer, C029)
    // [BYTES seg9:0x6126 seg9:0x612c].
    if (4 < w.score_level && w.handover.demons_gate == 0 && w.kind != 6 && w.kind != 7 &&
        w.w920c != 0 && w.w920c != 3 && (add16(w.w992, 30) < w.budget || 15 < since)) {
        // The count loop k = 0 ... n - 1 and the chance's second loop k = 1 ... n (one slot later)
        // [BYTES seg9:0x617d seg9:0x61bb seg9:0x61cf seg9:0x61dd seg9:0x6217 seg9:0x6229], read by
        // the M3 s4 input analyst and implementer; an independent verifier is still due [OPEN
        // weapon-slot count].
        int16_t heavy = 0;
        const int n = static_cast<std::int8_t>(ship_byte(me, 0xcb));
        for (int k = 0; k < n; ++k) {
            const std::uint8_t c = ship_byte(me, 0xb8 + k);
            if (c == 0x18 || c == 0x19 || c == 0x17 || c == 0x16) ++heavy;
            if (c == 0x1a) heavy = static_cast<int16_t>(heavy + 2);
        }
        if (4 < since && heavy < 4) {
            int16_t chance = i16(((since - 3) * 2 + w.score_level) * 2);
            for (int k = 1; k <= n; ++k) {
                const std::uint8_t c = ship_byte(me, 0xb8 + k);
                if (0x15 < c && c < 0x1b) chance = idiv16(shl16(chance, 2), 5);
            }
            if (rmod(h.rng.rand(), 100) < chance) w.w913e = 1;
        }
    }
    Pools& p = w.pools;
    const std::uint32_t now = now_of(h);
    for (int i = 0; i <= w.last_ship && i < p.records(kPoolShips); ++i) {
        ObjectRecord& s = p.rec(kPoolShips, i);
        ShipRecord& sr = w.ships[static_cast<std::size_t>(i)];
        s.scr_y = -100;  // [BYTES seg9:0x63ca seg9:0x63d2]
        bool resident = false;
        if (w.added == 0) {
            for (int k = 0; k < 9; ++k)
                if (i != w.player && dv(w).s8(0x1c0 + k) == ship_word(sr, 0xd6)) resident = true;
        }
        if (!resident || w.kind == 6) {
            // Off-world, the player at the first gate's centre [BYTES seg9:0x6320-0x633a].
            s.x = -1000;
            s.y = static_cast<int16_t>((-10 - i) * 100);
            if (i == w.player) {
                const ObjectRecord& c = p.rec(kPoolGateCentres, 0);
                s.x = c.x;
                s.y = c.y;
                if (h.recentre) h.recentre(w);  // seg8:0x0
            }
            s.state = 100;
            sr.d88.set(now + static_cast<std::uint32_t>(i) * 500000u);
            s.vx = 0;
            s.vy = -30;
        } else {
            // A resident ship [BYTES seg9:0x6433-0x6488 seg9:0x628a-0x62f6].
            bool hit = true;
            while (hit) {
                s.x = shl16(rmod(h.rng.rand(), kWorldCells), 3);
                s.y = shl16(rmod(h.rng.rand(), kWorldCells), 3);
                const Rect16 a = rect16(s.x, s.y, s.x + s.extent, s.y + s.extent);
                hit = false;
                for (int q = 0; q < 14; ++q)
                    for (int k = 0; k < p.in_use(q); ++k) {
                        const ObjectRecord& o = p.rec(q, k);
                        if (intersect(a, rect16(o.x, o.y, o.x + o.extent, o.y + o.extent))) hit = true;
                    }
            }
            s.state = 0;
            s.vx = static_cast<int16_t>(rmod(h.rng.rand(), 60) - 30);
            s.vy = static_cast<int16_t>(rmod(h.rng.rand(), 60) - 30);
            sr.d88.set(now);
        }
        // Ship byte +0xcd = 0xff unless its design type is 7 (M5) [OPEN].
    }
    // seg8:0x4d94(&ship[player].+0x88, &ship[0].+0x88) at seg9:0x64a7, after the ship loop:
    // the two dwords swap, so the player's entry time is ship 0's (now + 0) and the gate test
    // seg12:0x1f7a compares that (s4-draw.md 4) (M3 s4 DRAW) [BYTES seg8:0x4da1 seg8:0x4dbe
    // seg8:0x4dce seg9:0x6498 seg9:0x649e seg9:0x64a4 seg9:0x64a7].
    {
        ShipRecord& s0 = w.ships[0];
        const std::uint32_t t = me.d88.get();
        me.d88.set(s0.d88.get());
        s0.d88.set(t);
    }
    // seg3:0x45a(2) and seg11:0x3cc2(player, 0) (the HUD, whose seg11:0xafe seeds rand) follow
    // [BYTES seg9:0x64ae seg9:0x64ba]: hud::wave_start() runs seg3:0x45a(2) (M3 s3 fixer).
    w.w0a76 = 2;
    if (w.wave < 2) {
        set_ship_byte(me, 0x5d, 2);
        set_ship_byte(me, 0x5e, 2);
        set_ship_word(me, 0xd2, 0);
        set_ship_word(me, 0xd0, 0);
    }
    if (w.kind == 7) w.gate[0].w12 = 0;
    w.w934e = 0;
    say(h, "world: wave start seg9:0x60bc wave " + std::to_string(w.wave) + ", player at (" +
               std::to_string(p.rec(kPoolShips, w.player).x) + ", " +
               std::to_string(p.rec(kPoolShips, w.player).y) + ") heading " +
               std::to_string(p.rec(kPoolShips, w.player).frame));
}

std::string describe_counts(const World& w) {
    std::string s = "pools";
    for (int q = 0; q < kStartupPools; ++q)
        s += " " + std::to_string(q) + ":" + std::to_string(w.pools.allocated(q)) + "/" +
             std::to_string(w.pools.in_use(q));
    char buf[160];
    std::snprintf(buf, sizeof buf,
                  "; kind %d, categories %d %d %d %d %d, gates %d, pool-2 units %d, opponents %d, "
                  "budget %d, 0x992 %d",
                  w.kind, w.categories[0], w.categories[1], w.categories[2], w.categories[3], w.categories[4], w.gates,
                  w.pool2_units, w.opponents, w.budget, w.w992);
    return s + buf;
}

}  // namespace is::world
