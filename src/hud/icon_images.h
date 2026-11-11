// The drawing side of the icon effects (world/icon_effects.h IconPlan; M4 stage 5, track ICONS):
// the strip DGROUP:0x3a02 (slot 13), the icons' own flame images DGROUP:0x3a04 + 2i (slots
// 14 ... 21, (5S + 6g) × (2g + S)) and the backup DGROUP:0x3a28 (slot 32, 32·DGROUP:0x92ec ×
// 32), on the wave's Blitter and the image table DGROUP:0x39e8 (src/hud/wave_win32.cpp g_blit,
// g_images). The images live in the table's slots, so the icon mover's sprite (seg8:0xb76 from
// slot 14 + i, icon_mover.cpp) and the column and target pictures (world.h slot_image) find them.
//
// Graphics 230 (the flames) and 360 (the overlays) load through Blitter::load_graphic(id, 0, −1)
// (seg10:0x6c02), go through seg10:0x6308(−1, −1, bitmap, −1) at the plan's Size
// (world/rock_images.h rescale_bitmap), get their masks (Blitter::create_mask, seg10:0x6ec6) on
// each call and are deleted after it, as the original does [BYTES seg7:0x47ba-0x47dc
// seg7:0x255d-0x257a] (M4 s8 SIZE-SITES). Portable: the Blitter is the only drawing interface.
#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "world/icon_effects.h"
#include "world/sprite.h"

namespace is::hud {

class IconImages {
public:
    IconImages(world::Blitter& out, world::ImageSlotTable& images) : out_(out), images_(images) {}

    // The strip DC DGROUP:0x3a02 (wave_win32.cpp strip_dc, which binds the strip the briefing
    // entry built); kDcNone draws nothing on the strip.
    std::function<world::DcId()> strip;
    // seg7:0x233c's crack pictures (the briefing's deferred CrackPlans): wave_win32.cpp icon_crack.
    std::function<void(const world::CrackPlan&)> crack;
    // A part not ported, logged once by the caller's facility.
    std::function<void(const std::string& what, const char* address)> todo;
    // The strip's new DC after strip_rescale (the program's copies of DGROUP:0x3a02: wave_win32.cpp
    // g_draw.strip_dc and the strip image the column reads) [DESIGN ADR 0016 decision 3]; unset,
    // only the table's slot 13 holds it.
    std::function<void(world::DcId)> strip_rescaled;

    void draw(const world::IconPlan& p);
    // seg9:0x5cca's seg10:0x6308(0xd, −1, −1, −1) [BYTES seg9:0x5da6-0x5db3] (verified claim 41):
    // the briefing's strip slot 13, built unscaled (38N + 6 x 44) with its backup and cracks, is
    // rescaled at `level` before the overlays seg9:0x94ba paste at each record's scaled cell, so
    // the cells are (32 << Size) sar 1 with gaps (6 << Size) sar 1 (76k + 12 at Size 2, 19k + 3
    // at Size 0) (s8-size.md 2.3). The strip DC comes from `strip`; the new one goes to slot 13
    // and to strip_rescaled. Nothing at Size 1 (rescale.h) (M4 s8 SIZE-SITES).
    void strip_rescale(std::int16_t level);
    // seg7:0x49b6 [BYTES seg7:0x49b6] (core.md "Wave end", seg9:0x6cc4 step 7): each non-empty
    // DC DGROUP:0x3a04 + 2i, i < 8, released and its word zeroed.
    void release();

    world::DcId own(int icon) const;  // slot 14 + icon, kDcNone when empty
    world::DcId backup() const { return images_[world::kIconBackupSlot]; }

private:
    void drip(const world::IconDripPlan& p);
    void flame(const world::IconFlamePlan& p);
    void restore(const world::IconRestorePlan& p);
    void backup_one(const world::IconBackupPlan& p);
    void backup_all(const world::IconBackupAllPlan& p);
    void overlay(const world::IconOverlayPlan& p);
    world::DcId strip_dc() const { return strip ? strip() : world::kDcNone; }

    world::Blitter& out_;
    world::ImageSlotTable& images_;
};

// The program's IconImages on `out` and `images` (wave_win32.cpp install_wave): sets
// world::icon_effect_links() to it; `ready` gates every call (wave_win32.cpp ready()).
// `strip_rescaled` is IconImages::strip_rescaled (M4 s8 SIZE-SITES).
void install_icon_images(world::Blitter& out, world::ImageSlotTable& images,
                         std::function<world::DcId()> strip,
                         std::function<void(const world::CrackPlan&)> crack, std::function<bool()> ready,
                         std::function<void(const std::string&, const char*)> todo,
                         std::function<void(world::DcId)> strip_rescaled = {});

}  // namespace is::hud
