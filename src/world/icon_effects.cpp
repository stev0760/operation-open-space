// The icon mover's helpers (icon_effects.h). Ported from the listings (isre asm, INSPACE.EXE
// seg7) with work/analysis/m4/s5-icons.md and its verification s5-icons-verify.md.
#include "world/icon_effects.h"

#include <utility>

#include "world/damage.h"

namespace is::world {

namespace {

constexpr std::size_t ix(int k) { return static_cast<std::size_t>(k); }

IconRecord& icon_at(World& w, int i) {
    static IconRecord scratch;
    if (i < 0 || i >= kIconRecords) return scratch = IconRecord{};
    return w.icons[ix(i)];
}

// The cell of pool-0 record 0, as seg7:0x477e, seg7:0x4d86, seg7:0x5660 and seg7:0x56ba read it
// through the far pointer DGROUP:0x1df8 [BYTES seg7:0x479e-0x47ad seg7:0x4d95-0x4da4
// seg7:0x566d-0x5675 seg7:0x56c7-0x56cf].
struct Cell {
    std::int16_t size = 0, gap = 0;
};
Cell cell0(World& w) {
    const ObjectRecord& r = obj(w, kPoolIcons, 0);
    return Cell{r.size, r.gap};
}
// (S + g)·i + g, the strip x of icon i's cell (16-bit `imul`).
std::int16_t strip_x_of(Cell c, int i) {
    return add16(mul16(add16(c.size, c.gap), static_cast<std::int16_t>(i)), c.gap);
}
// `shl r, cl` with cl the byte DGROUP:0x9156: the 286's count is taken mod 32.
int shift_of(std::int16_t level) { return level & 0x1f; }

// ---- the briefing's deferral

struct PlanQueue {
    bool defer = false;
    std::vector<IconPlan> pending;
};
PlanQueue& queue() {
    static PlanQueue q;
    return q;
}

}  // namespace

IconEffectLinks& icon_effect_links() {
    static IconEffectLinks l;
    return l;
}

void icon_draw(const IconPlan& p) {
    PlanQueue& q = queue();
    if (q.defer) {
        q.pending.push_back(p);
        return;
    }
    if (icon_effect_links().draw) icon_effect_links().draw(p);
}

void defer_icon_plans(bool on) { queue().defer = on; }

void flush_icon_plans() {
    PlanQueue& q = queue();
    q.defer = false;
    std::vector<IconPlan> v;
    v.swap(q.pending);
    for (const IconPlan& p : v) icon_draw(p);
}

const std::vector<IconPlan>& pending_icon_plans() { return queue().pending; }

void clear_icon_plans() { queue().pending.clear(); }

// The burn deadline DGROUP:0x57a4 + 0x23·i is world.h IconRecord::burn_until; the word has no
// reader (verified claim 29).
std::uint32_t icon_burn_until(const World& w, int i) {
    if (i < 0 || i >= kIconRecords) return 0;
    return w.icons[ix(i)].burn_until;
}

void set_icon_burn_until(World& w, int i, std::uint32_t t) {
    if (i < 0 || i >= kIconRecords) return;  // [DESIGN] the original writes past the table
    w.icons[ix(i)].burn_until = t;
}

// ---- seg7:0x4d86

IconDripPlan drip_plan(int icon, std::int16_t size, std::int16_t gap, std::int16_t level, bool own,
                       int a, int b, int c, int e, int f) {
    const int sl = shift_of(level);
    // rand() % n by `cwd; idiv` (rand() is 0 ... 32767) [BYTES seg7:0x4da7-0x4e03]
    const std::int16_t r1 = sar16(shl16(add16(imod16(lo16(a), 12), 10), sl), 1);
    const std::int16_t r2 = sar16(shl16(add16(imod16(lo16(b), 12), 10), sl), 1);
    std::int16_t r3 = shl16(imod16(lo16(c), 10), sl);  // no `sar` [BYTES seg7:0x4de4-0x4dea]
    const std::int16_t r4 = sar16(shl16(add16(imod16(lo16(e), 10), 22), sl), 1);
    const std::int16_t d = sar16(shl16(2, sl), 1);  // [bp-0x12] [BYTES seg7:0x4e06-0x4e0d]
    IconDripPlan p;
    p.icon = icon;
    p.own = own;
    p.size = size;
    p.gap = gap;
    p.width = d;
    const std::int16_t cx = add16(mul16(add16(size, gap), static_cast<std::int16_t>(icon)), gap);
    if (!own) {  // the melt [BYTES seg7:0x4e14-0x4ed1]
        p.count = 4;
        p.r = {r1, r2, r3, r4};
        p.from_y = sub16(gap, d);
    } else {  // the burn [BYTES seg7:0x4ed4-0x4fbf]
        if ((f & 1) != 0) r3 = r4;  // the fifth draw [BYTES seg7:0x4ed4-0x4ee0]
        p.count = 2;
        p.r = {r1, r3, 0, 0};
        p.from_y = sub16(gap, idiv16(d, 2));  // `cwd; sub; sar` [BYTES seg7:0x4ee3-0x4ef0]
    }
    for (int j = 0; j < p.count; ++j) p.x[ix(j)] = add16(cx, p.r[ix(j)]);
    return p;
}

void icon_melt_step(UpdateCtx& c, World& w, int i) {
    if (w.pools.records(kPoolIcons) == 0) return;  // [DESIGN] no pool-0 record 0
    const Cell k = cell0(w);
    shell::Rng& rng = c.flight.rng;  // seg5:0xdc, the wave's rand() DGROUP:0x144c
    const int a = rng.rand();        // [BYTES seg7:0x4da7]
    const int b = rng.rand();        // [BYTES seg7:0x4dc0]
    const int d3 = rng.rand();       // [BYTES seg7:0x4dd9]
    const int e = rng.rand();        // [BYTES seg7:0x4ded]
    IconRecord& ic = icon_at(w, i);
    const bool own = ic.w02 != 0;    // DGROUP:0x57aa + 0x23·i [BYTES seg7:0x4e14-0x4e19]
    const int f = own ? rng.rand() : 0;  // [BYTES seg7:0x4ed4]
    icon_draw(drip_plan(i, k.size, k.gap, c.flight.g.size, own, a, b, d3, e, f));
    if (!own) {
        ic.w00 = sub16(ic.w00, 1);  // `dec` DGROUP:0x57a8 + 0x23·i [BYTES seg7:0x4ecd]
        return;
    }
    if (ic.w02 < 7) ic.w00 = sub16(ic.w00, 1);  // signed `jge` [BYTES seg7:0x4fc1-0x4fcc]
    // seg7:0x5b68(0, i, 0) on every pass through here [BYTES seg7:0x4fd0-0x4fef]
    if (ic.w00 < 50 && imod16(ic.w00, 5) == 0) c.host.puff(kPoolIcons, i, 0);
}

// ---- seg7:0x477e

void icon_ignite(UpdateCtx& c, World& w, int i) {
    c.host.sound(0x6a, 0, i, 1);  // seg10:0x754a(0x6a, 0, i, 1) [BYTES seg7:0x478d-0x4796]
    if (w.pools.records(kPoolIcons) == 0) return;  // [DESIGN]
    const Cell k = cell0(w);
    icon_at(w, i).w00 = 0x59;  // 89 [BYTES seg7:0x47b4]
    IconFlamePlan p;
    p.icon = i;
    p.size = k.size;
    p.gap = k.gap;
    p.h12 = sar16(shl16(12, shift_of(c.flight.g.size)), 1);  // [BYTES seg7:0x48da-0x48e3]
    p.strip_x = strip_x_of(k, i);
    p.level = c.flight.g.size;
    icon_draw(p);
}

// ---- seg7:0x4ffe

void icon_burnt_out(UpdateCtx& c, World& w, int i) {
    CollideCtx x = frame_collide_ctx(c, w);
    {
        ObjectRecord& I = obj(w, kPoolIcons, i);
        x.h.erase(I.scr_x, I.scr_y, I.size, I.size);  // PatBlt(DGROUP:0x13dc, ..., BLACKNESS) [BYTES seg7:0x500c-0x5031]
        I.state = 1;                                  // [BYTES seg7:0x503a]
    }
    c.host.puff(kPoolIcons, i, 1);  // seg7:0x5b68(0, i, 1) [BYTES seg7:0x5040-0x504c]
    c.host.puff(kPoolIcons, i, 1);  // [BYTES seg7:0x504e-0x505a]
    x.h.icon_gone(i, 0);            // seg7:0x5ae8(i, 0) [BYTES seg7:0x505c-0x5062]
    x.h.column_row(0, i);           // seg7:0x65aa(0, i) [BYTES seg7:0x5068-0x506e]
    seg8_99ce(x, 0, i, -1, -1, 1);  // [BYTES seg7:0x5074-0x507f]
}

// ---- the backup slot 32: seg7:0x5660, seg7:0x56ba, seg10:0x506c

void icon_restore(CollideCtx& x, int i) {
    if (x.w.pools.records(kPoolIcons) == 0) return;  // [DESIGN]
    const Cell k = cell0(x.w);
    icon_draw(IconRestorePlan{i, k.size, k.gap, strip_x_of(k, i)});  // [BYTES seg7:0x566d-0x56a2]
    icon_damage(x, 0, i, 0);  // seg7:0x233c(0, i, 0) [BYTES seg7:0x56a7-0x56af]
}

void icon_backup(World& w, int i) {
    if (w.pools.records(kPoolIcons) == 0) return;  // [DESIGN]
    const Cell k = cell0(w);
    icon_draw(IconBackupPlan{i, k.size, k.gap, strip_x_of(k, i)});  // [BYTES seg7:0x56c7-0x56fe]
}

void icon_backup_all(World& w) {
    // k < DGROUP:0x92ec, pool 0's allocated count [BYTES seg10:0x50bf-0x50f9 seg9:0x24d3]
    icon_draw(IconBackupAllPlan{w.pools.allocated(kPoolIcons)});
}

// ---- seg7:0x2512

void icon_overlay(CollideCtx& x, int i, int k) {
    World& w = x.w;
    // i outside the 8 records: the port draws and writes nothing [DESIGN].
    if (i >= 0 && i < kIconRecords && icon_at(w, i).w00 == 0) {  // [BYTES seg7:0x2521-0x252a]
        const ObjectRecord& I = obj(w, kPoolIcons, i);
        IconOverlayPlan p;
        p.icon = i;
        p.cell = k;
        p.gap = I.gap;    // record i's, not record 0's [BYTES seg7:0x253c-0x2547]
        p.size = I.size;
        p.x = add16(mul16(add16(p.gap, p.size), static_cast<std::int16_t>(i)), p.gap);
        p.level = x.g.size;
        icon_draw(p);  // [BYTES seg7:0x2558-0x25ec]
        w.dir().set_byte(0x1c8 + ix(i), k + 1);  // [BYTES seg7:0x2612-0x2628]
        if (k == 0) obj(w, kPoolIcons, i).b53 = 2;  // [BYTES seg7:0x262d-0x2637]
    }
    x.h.column_row(0, i);  // seg7:0x65aa(0, i) [BYTES seg7:0x263c-0x2642]
}

// ---- seg7:0x4a38's pool-5 cases

void icon_pool5_hit(UpdateCtx& c, World& w, int R, int icon) {
    WaveGlobals& g = c.flight.g;
    CollideCtx x = frame_collide_ctx(c, w);
    // The pool-5 record's shooter, DGROUP:0x5a50 / 0x5a52 + 0x14·R (the port's table holds 32:
    // a record past it reads none) [DESIGN].
    const Pool5Entry owner = R >= 0 && R < static_cast<int>(x.c.pool5.size()) ? x.c.pool5[ix(R)] : Pool5Entry{};
    const std::int16_t mass = obj(w, g.pool5, R).mass;  // +0x44 [BYTES seg7:0x4aa1 seg7:0x4bb9]
    const auto heat = [&]() { return static_cast<std::int8_t>(obj(w, kPoolIcons, icon).flame); };
    if (mass == 0x78) {  // FIREBALL
        if (heat() < 0) {  // the thaw [BYTES seg7:0x4aaa-0x4ab7]
            for (int k = 0; k < 4; ++k) c.host.puff(kPoolIcons, icon, 1);  // [BYTES seg7:0x4ab9-0x4ad3]
            if (heat() < -2) c.host.sound(0x76, 0, icon, 1);  // [BYTES seg7:0x4ad5-0x4af2]
            obj(w, kPoolIcons, icon).flame = 0xfe;           // [BYTES seg7:0x4afd]
            icon_restore(x, icon);                            // seg7:0x5660 [BYTES seg7:0x4b06]
            if (owner.shooter_pool == g.ship_pool) {          // [BYTES seg7:0x4b0a-0x4b24]
                ShipRecord& s = ship_at(w, owner.shooter_rec);
                s.w36 = 2;  // DGROUP:0x70c6
                s.w38 = 2;  // DGROUP:0x70c8
            }
            return;
        }
        IconRecord& ic = icon_at(w, icon);
        if (ic.w02 != 0) return;  // burning already [BYTES seg7:0x4b2b-0x4b36]
        ic.w02 = 2;               // [BYTES seg7:0x4b39]
        set_icon_burn_until(w, icon, g.now + 4000u);  // [BYTES seg7:0x4b3f-0x4b49]
        const ObjectRecord& I = obj(w, kPoolIcons, icon);
        // A computer ship of the literal pool 14 that wants this icon [BYTES seg7:0x4b4e-0x4b95]
        if (I.damage == 0 && I.b53 == 1 && owner.shooter_pool == 0xe) {
            const ShipRecord& s = ship_at(w, owner.shooter_rec);
            if (s.w32 == 0 && s.w34 == icon && s.w38 == 1) x.h.seg13_b6f2(owner.shooter_rec, 1);
        }
        icon_ignite(c, w, icon);                // seg7:0x477e [BYTES seg7:0x4ba1]
        obj(w, kPoolIcons, icon).b53 = 0;       // [BYTES seg7:0x4ba5 seg7:0x4c4b-0x4c4f]
        return;
    }
    if (mass != 0xc8) return;  // [BYTES seg7:0x4bb9-0x4bbf]
    IconRecord& ic = icon_at(w, icon);
    if (ic.w02 > 0) {  // SNOWBALL on a burning icon: put out [BYTES seg7:0x4bc3-0x4bcc]
        ic.w00 = 0;    // [BYTES seg7:0x4bd0]
        ic.w02 = 0;    // [BYTES seg7:0x4bd4]
        obj(w, kPoolIcons, icon).flame = 0xfe;  // [BYTES seg7:0x4be0]
        for (int k = 0; k < 4; ++k) c.host.puff(kPoolIcons, icon, 1);  // [BYTES seg7:0x4be8-0x4bfd]
        obj(w, kPoolIcons, icon).b53 = 1;       // [BYTES seg7:0x4c07]
        c.host.sound(0x76, 0, icon, 1);         // [BYTES seg7:0x4c0c-0x4c15]
        icon_backup(w, icon);                   // seg7:0x56ba [BYTES seg7:0x4c21]
        return;
    }
    if (heat() < 0) return;  // [BYTES seg7:0x4c27-0x4c34]
    icon_overlay(x, icon, 1);                   // seg7:0x2512(icon, 1) [BYTES seg7:0x4c3c]
    obj(w, kPoolIcons, icon).flame = 0x9c;      // -100 [BYTES seg7:0x4c46]
    obj(w, kPoolIcons, icon).b53 = 0;           // [BYTES seg7:0x4c4b-0x4c4f]
}

void release_icon_images() {
    if (icon_effect_links().release) icon_effect_links().release();
}

}  // namespace is::world
