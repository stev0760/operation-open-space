// The icon mover's helpers (M4 stage 5, track ICONS): the melt and the burn, the burnt-out end,
// the flame image, the backup slot 32 and its two copies, the ice and virus overlay and the
// pool-5 FIREBALL / SNOWBALL cases of seg7:0x4a38 (work/analysis/m4/s5-icons.md sections 1.1-1.8
// with s5-icons-verify.md: claims 1-29 CONFIRMED, 51 with its additions). The icon mover
// (icon_mover.h) and the briefing entry (setup.h enter_briefing) call them.
//
// The words live in the World: `w00` (DGROUP:0x57a8 + 0x23·i, the effect counter), `w02`
// (DGROUP:0x57aa, the flame frame), the burn deadline (DGROUP:0x57a4), the heat byte +0x2f
// (`flame`) and the capturable byte +0x53 (`b53`) of pool-0 record i. The pictures are plans
// (IconPlan) for the drawing side (src/hud/icon_images.h): the strip DGROUP:0x3a02 (slot 13), the
// icon's own flame image DGROUP:0x3a04 + 2i (slot 14 + i) and the backup DGROUP:0x3a28 (slot 32).
//
// 16-bit arithmetic as the original's (ADR 0013 decision 7). Portable, no Win32.
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <variant>
#include <vector>

#include "world/collide.h"
#include "world/update.h"

namespace is::world {

// ---- image slots and graphics

// DGROUP:0x3a28 = DGROUP:0x39e8 + 2·32: the backup of the bare icons, 32·DGROUP:0x92ec × 32
// (structs.md "Image slots", slot 0x20) [BYTES seg10:0x5079 seg10:0x5093 seg7:0x568c seg7:0x56d3].
constexpr int kIconBackupSlot = 32;
// IGRAPHIC.DLL #4000 230: 160 × 32, five 32 × 32 flame cells (seg7:0x477e) [BYTES seg7:0x47c0].
constexpr int kGraphicIconFlames = 0xe6;
// IGRAPHIC.DLL #4000 360: 64 × 32, cell 0 the green virus slime, cell 1 the ice (verified claim
// 20) [BYTES seg7:0x255f IGRAPHIC.DLL 4000 360].
constexpr int kGraphicIconOverlay = 0x168;

// ---- the plans for the drawing side

// seg7:0x4d86's column copies [BYTES seg7:0x4e14-0x4ed1 seg7:0x4ee3-0x4fbf] (verified claims 6,
// 8): for each column j < count, BitBlt(strip, x[j], gap, width, size, strip, x[j], from_y,
// SRCCOPY); with `own` (the burn, branch B) also, for the five cells k = 1 ... 5 of slot 14 +
// icon, BitBlt(own, x0 + r[j], gap, width, size, own, x0 + r[j], from_y, SRCCOPY) with x0 =
// (size + gap)·(k − 1) + gap, the columns of one cell before the next.
struct IconDripPlan {
    int icon = 0;
    bool own = false;
    std::int16_t size = 0, gap = 0;  // S, g of pool-0 record 0
    std::int16_t width = 0;          // d = (2 << Size) >> 1
    std::int16_t from_y = 0;         // g − d (the melt) or g − d / 2 (the burn)
    int count = 0;                   // 4 (the melt: r1, r2, r3, r4) or 2 (the burn: r1, r3)
    std::array<std::int16_t, 4> r{};  // the column offsets in the cell
    std::array<std::int16_t, 4> x{};  // their strip x: (S + g)·icon + g + r
};
// seg7:0x477e's image [BYTES seg7:0x47ba-0x49a8] (verified claims 2-4): slot 14 + icon, created
// (5S + 6g) × (2g + S) when empty (CreateCompatibleBitmap of DGROUP:0x13dc), PatBlt BLACKNESS
// whole; graphic 230 (seg10:0x6c02(0xe6, 0, −1)) through seg10:0x6308(−1, −1, h, −1) (nothing at
// Size 1) and its mask (seg10:0x6ec6); for k = 0 ... 4 at xk = (S + g)·k + g: BitBlt(own, xk,
// g + h12, S, S − h12, strip, strip_x, g + h12, SRCCOPY), BitBlt(own, xk, g, S, S, mask, S·k, 0,
// SRCAND), BitBlt(own, xk, g, S, S, graphic, S·k, 0, SRCPAINT); the mask and graphic deleted.
struct IconFlamePlan {
    int icon = 0;
    std::int16_t size = 0, gap = 0;  // S, g of pool-0 record 0
    std::int16_t h12 = 0;            // (12 << Size) >> 1
    std::int16_t strip_x = 0;        // (S + g)·icon + g
    std::int16_t level = 1;          // Size DGROUP:0x9156, for the seg10:0x6308 pass
};
// seg7:0x5660 [BYTES seg7:0x566d-0x56a2] (verified claim 14): StretchBlt(strip, strip_x, gap,
// size, size, backup, 32·icon, 0, 32, 32, SRCCOPY), S and g of pool-0 record 0.
struct IconRestorePlan {
    int icon = 0;
    std::int16_t size = 0, gap = 0, strip_x = 0;
};
// seg7:0x56ba [BYTES seg7:0x56c7-0x56fe] (verified claim 15): the reverse, StretchBlt(backup,
// 32·icon, 0, 32, 32, strip, strip_x, gap, size, size, SRCCOPY).
struct IconBackupPlan {
    int icon = 0;
    std::int16_t size = 0, gap = 0, strip_x = 0;
};
// seg10:0x506c [BYTES seg10:0x5083-0x50b5 seg10:0x50bf-0x50f9] (verified claim 12): slot 32
// created 32·count × 32 when empty, then BitBlt(backup, 32k, 0, 32, 32, strip, 38k + 6, 6,
// SRCCOPY) for k < count (DGROUP:0x92ec, 8 in every load: claim 13).
struct IconBackupAllPlan {
    int count = 0;
};
// seg7:0x2512's paste [BYTES seg7:0x252e-0x25ec] (verified claim 18): graphic 360 (seg10:0x6c02
// (0x168, 0, −1)) through seg10:0x6308(−1, −1, h, −1) and its mask; BitBlt(strip, x, gap, size,
// size, mask, size·cell, 0, SRCAND), then the graphic SRCPAINT; S and g of pool-0 record icon.
struct IconOverlayPlan {
    int icon = 0;
    int cell = 0;                    // k: 0 virus, 1 ice
    std::int16_t size = 0, gap = 0;
    std::int16_t x = 0;              // (g + S)·icon + g
    std::int16_t level = 1;          // Size DGROUP:0x9156
};
// seg9:0x5cca's seg10:0x6308(0xd, -1, -1, -1) between the crack loop and seg9:0x94ba [BYTES
// seg9:0x5da6-0x5db3] (hud/icon_images.h IconImages::strip_rescale) (M4 s8 SIZE-SITES).
struct IconStripRescalePlan {
    std::int16_t level = 1;  // Size DGROUP:0x9156
};
// One picture for the drawing side; CrackPlan is seg7:0x233c's (collide.h), deferred with the
// rest in the briefing.
using IconPlan = std::variant<IconDripPlan, IconFlamePlan, IconRestorePlan, IconBackupPlan,
                              IconBackupAllPlan, IconOverlayPlan, CrackPlan, IconStripRescalePlan>;

// The drawing side's calls (src/hud/icon_images.h install_icon_images sets them; an empty slot
// draws nothing, as the host build and the tests without a recorder).
struct IconEffectLinks {
    std::function<void(const IconPlan&)> draw;
    // seg7:0x49b6: the eight DCs DGROUP:0x3a04 + 2i (slots 14 ... 21) released (core.md "Wave
    // end", seg9:0x6cc4 step 7) [BYTES seg9:0x6d61 seg7:0x49b6].
    std::function<void()> release;
};
IconEffectLinks& icon_effect_links();

// A plan to the drawing side, or to the pending list while the briefing defers them.
void icon_draw(const IconPlan& p);
// The briefing entry seg9:0x5cca draws its backup, cracks and overlays into the strip that
// seg10:0x5b30 and seg9:0xb2a made (seg9:0x5d61-0x5db7); the port builds the strip's pixels
// after enter_briefing (world/hooks.cpp build_strip), so enter_briefing defers its plans and the
// strip's builder flushes them in order onto the new strip [DESIGN].
void defer_icon_plans(bool on);
void flush_icon_plans();
const std::vector<IconPlan>& pending_icon_plans();
void clear_icon_plans();

// The burn deadline DGROUP:0x57a4 + 0x23·i (a dword, written only: verified claim 29), now +
// 4000 at a FIREBALL ignition [BYTES seg7:0x4b3f-0x4b49 seg7:0x45e5]. IconRecord::burn_until
// when world.h has it, else a table of this file's [DESIGN].
std::uint32_t icon_burn_until(const World& w, int i);
void set_icon_burn_until(World& w, int i, std::uint32_t t);

// ---- the helpers

// seg7:0x4d86's column offsets from its rand() draws (pure; verified claims 5, 7): r1 =
// ((a % 12 + 10) << Size) >> 1, r2 likewise from b, r3 = (c % 10) << Size, r4 = ((e % 10 + 22)
// << Size) >> 1, d = (2 << Size) >> 1; for the burn (`own`) r3 becomes r4 when bit 0 of the
// fifth draw f is set. The plan for icon i on cells S, g.
IconDripPlan drip_plan(int icon, std::int16_t size, std::int16_t gap, std::int16_t level, bool own,
                       int a, int b, int c, int e, int f);

// seg7:0x4d86(i), cdecl, while DGROUP:0x57a8 + 0x23·i != 0 [BYTES seg7:0x4d86-0x4ffd] (verified
// claims 5-9): four rand() (five for the burn), the plan; the melt (`w02` = 0) decrements `w00`;
// the burn decrements it only while `w02` < 7 and then puffs seg7:0x5b68(0, i, 0) (UpdateHost::
// puff) when `w00` < 50 and `w00` % 5 = 0.
void icon_melt_step(UpdateCtx& c, World& w, int i);
// seg7:0x477e(i), cdecl [BYTES seg7:0x477e-0x49b4] (verified claims 1-4): seg10:0x754a(0x6a, 0,
// i, 1), `w00` = 89 and the flame image (IconFlamePlan). Every caller sets `w02` = 2 first.
void icon_ignite(UpdateCtx& c, World& w, int i);
// seg7:0x4ffe(i), cdecl, from the mover's end of iteration [BYTES seg7:0x4ffe-0x5084] (verified
// claim 11): the square (+0x00, +0x02, side +0x22 of record i) blacked on the window,
// `state` 1, seg7:0x5b68(0, i, 1) twice, seg7:0x5ae8(i, 0) (CollideHost::icon_gone),
// seg7:0x65aa(0, i) and seg8:0x99ce(0, i, −1, −1, 1). `w02` is left as it is.
void icon_burnt_out(UpdateCtx& c, World& w, int i);
// seg7:0x5660(i): the backup's cell back into the strip, then seg7:0x233c(0, i, 0) (damage.h
// icon_damage), which at heat −2 (both callers) only redraws the column row (verified claims
// 14, 16).
void icon_restore(CollideCtx& x, int i);
// seg7:0x56ba(i): the strip's cell into the backup (verified claim 15).
void icon_backup(World& w, int i);
// seg10:0x506c, from the briefing after seg9:0xb2a [BYTES seg9:0x5d6b seg10:0x506c] (verified
// claim 12): the bare strip cells into slot 32.
void icon_backup_all(World& w);
// seg7:0x2512(i, k), cdecl [BYTES seg7:0x2512-0x2648] (verified claims 17-19): with `w00` != 0
// only seg7:0x65aa(0, i); else the overlay cell k on the strip with record i's S and g, the
// directory byte +0x1c8 + i = k + 1, for k = 0 byte +0x53 = 2, then seg7:0x65aa(0, i).
void icon_overlay(CollideCtx& x, int i, int k);
// seg7:0x4a38's pool-5 cases [BYTES seg7:0x4a8a-0x4c4f] (verified claims 23-28; s5-icons.md 1.7):
// R a record of DGROUP:0x9328 (the caller tests the pool) that met icon `icon`. FIREBALL (mass
// 0x78): a frozen icon thaws (four puffs, sound 0x76 below heat −2, heat −2, seg7:0x5660, the
// shooter ship's words +0x36, +0x38 = 2), else an icon not burning ignites (`w02` = 2, the burn
// deadline, seg13:0xb6f2(s, 1) for a ship of literal pool 14 that wants this icon, seg7:0x477e,
// +0x53 = 0). SNOWBALL (mass 0xc8): a burning icon is put out (`w00` = `w02` = 0, heat −2, four
// puffs, +0x53 = 1, sound 0x76, seg7:0x56ba), else an icon with heat >= 0 freezes (seg7:0x2512
// (icon, 1), heat −100, +0x53 = 0).
void icon_pool5_hit(UpdateCtx& c, World& w, int R, int icon);
// seg7:0x49b6 through IconEffectLinks::release [BYTES seg7:0x49b6].
void release_icon_images();

}  // namespace is::world
