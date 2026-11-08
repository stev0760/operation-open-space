// The icon effects' pictures (icon_images.h). Each plan's blits are the original's, in its order
// (work/analysis/m4/s5-icons.md sections 1.2-1.6, verified claims 2-8, 12, 14, 15, 18).
#include "hud/icon_images.h"

#include <memory>
#include <type_traits>

#include "world/icon_mover.h"

namespace is::hud {

namespace {

constexpr std::size_t ix(int k) { return static_cast<std::size_t>(k); }

}  // namespace

world::DcId IconImages::own(int icon) const {
    if (icon < 0 || icon >= world::kIconRecords) return world::kDcNone;
    return images_[ix(world::kIconOwnSlot + icon)];
}

void IconImages::draw(const world::IconPlan& p) {
    std::visit(
        [this](const auto& q) {
            using T = std::decay_t<decltype(q)>;
            if constexpr (std::is_same_v<T, world::IconDripPlan>)
                drip(q);
            else if constexpr (std::is_same_v<T, world::IconFlamePlan>)
                flame(q);
            else if constexpr (std::is_same_v<T, world::IconRestorePlan>)
                restore(q);
            else if constexpr (std::is_same_v<T, world::IconBackupPlan>)
                backup_one(q);
            else if constexpr (std::is_same_v<T, world::IconBackupAllPlan>)
                backup_all(q);
            else if constexpr (std::is_same_v<T, world::IconOverlayPlan>)
                overlay(q);
            else if (crack)
                crack(q);  // world::CrackPlan
        },
        p);
}

// seg10:0x6308(-1, -1, h, -1) returns at once at Size 1 (ispace.md "seg10:0x6308"); the other
// sizes' rescale is not ported [OPEN].
void IconImages::rescale(std::int16_t level, const char* what) {
    if (level != 1 && todo) todo(std::string("seg10:0x6308 on ") + what, "seg10:0x6308");
}

// seg7:0x4d86 [BYTES seg7:0x4e14-0x4ed1 seg7:0x4ee3-0x4fbf]
void IconImages::drip(const world::IconDripPlan& p) {
    const world::DcId s = strip_dc();
    if (s != world::kDcNone)
        for (int j = 0; j < p.count; ++j)
            out_.bit_blt(s, p.x[ix(j)], p.gap, p.width, p.size, s, p.x[ix(j)], p.from_y, world::kSrcCopy);
    if (!p.own) return;
    const world::DcId o = own(p.icon);
    if (o == world::kDcNone) return;  // no image made: nothing to melt [DESIGN]
    for (int k = 1; k <= 5; ++k) {  // [BYTES seg7:0x4f44-0x4fbf]
        const int x0 = (p.size + p.gap) * (k - 1) + p.gap;
        for (int j = 0; j < p.count; ++j)
            out_.bit_blt(o, x0 + p.r[ix(j)], p.gap, p.width, p.size, o, x0 + p.r[ix(j)], p.from_y,
                         world::kSrcCopy);
    }
}

// seg7:0x477e [BYTES seg7:0x47ba-0x49a8]
void IconImages::flame(const world::IconFlamePlan& p) {
    if (p.icon < 0 || p.icon >= world::kIconRecords) return;  // [DESIGN] the table's 8 slots
    const world::DcId gr = out_.load_graphic(world::kGraphicIconFlames, 0, -1);  // [BYTES seg7:0x47ba-0x47c3]
    rescale(p.level, "the icon flames");
    const world::DcId mask = gr != world::kDcNone ? out_.create_mask(gr) : world::kDcNone;  // seg10:0x6ec6 [BYTES seg7:0x47dc]
    const int w = 5 * p.size + 6 * p.gap, h = 2 * p.gap + p.size;
    world::DcId& o = images_[ix(world::kIconOwnSlot + p.icon)];
    if (o == world::kDcNone) o = out_.create_image(w, h);  // DGROUP:0x3a04 + 2i == 0 [BYTES seg7:0x4832-0x4865]
    if (o != world::kDcNone) out_.pat_blt(o, 0, 0, w, h, world::kBlackness);  // [BYTES seg7:0x4896-0x48b6]
    const world::DcId s = o != world::kDcNone ? strip_dc() : world::kDcNone;
    for (int k = 0; k < 5; ++k) {  // [BYTES seg7:0x48bb-0x4962]
        const int xk = (p.size + p.gap) * k + p.gap;
        if (o == world::kDcNone) break;
        if (s != world::kDcNone)
            out_.bit_blt(o, xk, p.gap + p.h12, p.size, p.size - p.h12, s, p.strip_x, p.gap + p.h12, world::kSrcCopy);
        if (mask != world::kDcNone) out_.bit_blt(o, xk, p.gap, p.size, p.size, mask, p.size * k, 0, world::kSrcAnd);
        if (gr != world::kDcNone) out_.bit_blt(o, xk, p.gap, p.size, p.size, gr, p.size * k, 0, world::kSrcPaint);
    }
    // DeleteDC twice, DeleteObject(mask), DeleteObject(graphic) [BYTES seg7:0x4966-0x49a8]
    if (mask != world::kDcNone) out_.delete_image(mask);
    if (gr != world::kDcNone) out_.delete_image(gr);
}

// seg7:0x5660 [BYTES seg7:0x566d-0x56a2]
void IconImages::restore(const world::IconRestorePlan& p) {
    const world::DcId s = strip_dc(), b = backup();
    if (s == world::kDcNone || b == world::kDcNone) return;
    out_.stretch_blt(s, p.strip_x, p.gap, p.size, p.size, b, p.icon * 32, 0, 32, 32, world::kSrcCopy);
}

// seg7:0x56ba [BYTES seg7:0x56c7-0x56fe]
void IconImages::backup_one(const world::IconBackupPlan& p) {
    const world::DcId s = strip_dc(), b = backup();
    if (s == world::kDcNone || b == world::kDcNone) return;
    out_.stretch_blt(b, p.icon * 32, 0, 32, 32, s, p.strip_x, p.gap, p.size, p.size, world::kSrcCopy);
}

// seg10:0x506c [BYTES seg10:0x5079-0x50b5 seg10:0x50bf-0x50f9]
void IconImages::backup_all(const world::IconBackupAllPlan& p) {
    world::DcId& b = images_[ix(world::kIconBackupSlot)];
    if (b == world::kDcNone) b = out_.create_image(32 * p.count, 32);  // DGROUP:0x3a28 == 0 [BYTES seg10:0x5079-0x50b5]
    const world::DcId s = strip_dc();
    if (b == world::kDcNone || s == world::kDcNone) return;
    for (int k = 0; k < p.count; ++k) out_.bit_blt(b, 32 * k, 0, 32, 32, s, 38 * k + 6, 6, world::kSrcCopy);
}

// seg7:0x2512 [BYTES seg7:0x252e-0x25ec]
void IconImages::overlay(const world::IconOverlayPlan& p) {
    const world::DcId s = strip_dc();
    if (s == world::kDcNone) return;
    const world::DcId gr = out_.load_graphic(world::kGraphicIconOverlay, 0, -1);  // [BYTES seg7:0x255f-0x2562]
    if (gr == world::kDcNone) return;
    rescale(p.level, "the icon overlay");
    const world::DcId mask = out_.create_mask(gr);
    if (mask != world::kDcNone) out_.bit_blt(s, p.x, p.gap, p.size, p.size, mask, p.size * p.cell, 0, world::kSrcAnd);  // [BYTES seg7:0x2584-0x25c0]
    out_.bit_blt(s, p.x, p.gap, p.size, p.size, gr, p.size * p.cell, 0, world::kSrcPaint);  // [BYTES seg7:0x25c5-0x25ec]
    if (mask != world::kDcNone) out_.delete_image(mask);
    out_.delete_image(gr);
}

void IconImages::release() {
    for (int i = 0; i < world::kIconRecords; ++i) {  // [BYTES seg7:0x49b6]
        world::DcId& o = images_[ix(world::kIconOwnSlot + i)];
        if (o == world::kDcNone) continue;
        out_.delete_image(o);
        o = world::kDcNone;
    }
}

void install_icon_images(world::Blitter& out, world::ImageSlotTable& images,
                         std::function<world::DcId()> strip,
                         std::function<void(const world::CrackPlan&)> crack, std::function<bool()> ready,
                         std::function<void(const std::string&, const char*)> todo) {
    auto ii = std::make_shared<IconImages>(out, images);
    ii->strip = std::move(strip);
    ii->crack = std::move(crack);
    ii->todo = std::move(todo);
    world::IconEffectLinks& l = world::icon_effect_links();
    l.draw = [ii, ready](const world::IconPlan& p) {
        if (!ready || ready()) ii->draw(p);
    };
    l.release = [ii, ready]() {
        if (!ready || ready()) ii->release();
    };
}

}  // namespace is::hud
