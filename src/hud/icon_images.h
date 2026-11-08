// The drawing side of the icon effects (world/icon_effects.h IconPlan; M4 stage 5, track ICONS):
// the strip DGROUP:0x3a02 (slot 13), the icons' own flame images DGROUP:0x3a04 + 2i (slots
// 14 ... 21, (5S + 6g) × (2g + S)) and the backup DGROUP:0x3a28 (slot 32, 32·DGROUP:0x92ec ×
// 32), on the wave's Blitter and the image table DGROUP:0x39e8 (src/hud/wave_win32.cpp g_blit,
// g_images). The images live in the table's slots, so the icon mover's sprite (seg8:0xb76 from
// slot 14 + i, icon_mover.cpp) and the column and target pictures (world.h slot_image) find them.
//
// Graphics 230 (the flames) and 360 (the overlays) load through Blitter::load_graphic(id, 0, −1)
// (seg10:0x6c02) with their masks (Blitter::create_mask, seg10:0x6ec6) on each call and are
// deleted after it, as the original does. Portable: the Blitter is the only drawing interface.
#pragma once

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
    // A part not ported (seg10:0x6308 at Size != 1), logged once by the caller's facility.
    std::function<void(const std::string& what, const char* address)> todo;

    void draw(const world::IconPlan& p);
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
    void rescale(std::int16_t level, const char* what);

    world::Blitter& out_;
    world::ImageSlotTable& images_;
};

// The program's IconImages on `out` and `images` (wave_win32.cpp install_wave): sets
// world::icon_effect_links() to it; `ready` gates every call (wave_win32.cpp ready()).
void install_icon_images(world::Blitter& out, world::ImageSlotTable& images,
                         std::function<world::DcId()> strip,
                         std::function<void(const world::CrackPlan&)> crack, std::function<bool()> ready,
                         std::function<void(const std::string&, const char*)> todo);

}  // namespace is::hud
