// seg12:0x6d64(s, w): a weapon into a new slot of ship s (work/analysis/m4/s5-weapons.md
// section 3, verified claims 19-21). M4 stage 5, track WEAPONS. Portable, no Win32.
//
// seg12:0x6d64 is cdecl (`retf`, every caller pops 4 bytes): [bp+6] the ship, [bp+8] the weapon,
// one local [bp-4] (l) [BYTES seg12:0x6d72 seg12:0x6de9 seg13:0x1e53 seg16:0x39e7]. Its callers
// [BYTES seg13:0x1e4e seg16:0x39e2 seg16:0x4021 seg14:0x1764 seg7:0xed3 seg11:0x9f26
// seg11:0x7362 seg11:0x7370 seg11:0x737e seg18:0x5251]: the kind-1 pod (capture.cpp
// pod_weapons, which then writes the pod's count over the 20), a bought weapon not held and
// UNIFY FORCES (purchases.cpp apply_purchases, unify_forces), the placement's weapon 13 when
// DGROUP:0x104e != 0 (roster.cpp), and paths of M6 (seg7:0xed3, seg11:0x9f26, seg18) and the
// cheat keys (seg11:0x72e0).
//
// What it does not do: no sound, no WM_PAINT, no seg11:0x2f8c (the weapon picture), no change of
// `weapon_slot` (+0x18). Its only call is seg10:0x533a, the weapon's image slot and sheet
// [BYTES seg12:0x6d64-0x6de9].
#pragma once

#include <functional>

#include "world/records.h"

namespace is::world {

class World;

// What seg12:0x6d64 did: whether a slot was added, and its index l.
struct WeaponLoad {
    bool added = false;
    int slot = -1;
};

// seg12:0x6d64's slot part on the ship record [BYTES seg12:0x6d72-0x6dc6]:
//  - for l = 1 while sext8(+0xcb) > l (signed 16-bit compare): a slot whose sext8(+0xb8 + l)
//    equals w returns with no write, so the held weapon is not reloaded; slot 0 is never
//    searched [BYTES seg12:0x6d72-0x6d9a];
//  - (int8) +0xcb > 9 returns: ten slots are full [BYTES seg12:0x6da1-0x6da6];
//  - else +0xcb += 1 (a byte inc), l = sext8(+0xcb) - 1, +0xb8 + l = lo8(w) and +0xae + l =
//    0x14, the count 20 [BYTES seg12:0x6dad-0x6dc6].
WeaponLoad load_weapon_slot(ShipRecord& s, int w);

// seg10:0x533a(w): the image slot and sheet of a launched weapon (player_sheet.h
// build_weapon_sheets' loop body). The program binds it (set_weapon_image); empty: none.
using WeaponImageHook = std::function<void(int w)>;

// seg12:0x6d64(ship, w) whole: load_weapon_slot, then, when a slot was added and W(w)+0x0a
// (DGROUP:0x2f5e + 0x50·w, w the word argument) != 0, image(sext8(+0xb8 + l)) [BYTES
// seg12:0x6dcb-0x6de1]. A ship index outside the table changes nothing [DESIGN].
WeaponLoad load_weapon(World& wd, const WeaponWords& W, int ship, int w, const WeaponImageHook& image);

// The program's seg10:0x533a for load_weapon's callers that have no hook of their own
// (capture.h CaptureHost::seg12_6d64, purchases.h WorldPurchaseHost::load_weapon): the HUD sets
// it (src/hud/wave_win32.cpp) [DESIGN].
void set_weapon_image(WeaponImageHook hook);
const WeaponImageHook& weapon_image();

// seg12:0x6d64 on the program's state: the World given, the weapon words of missile_state()
// and the hook of set_weapon_image.
WeaponLoad load_weapon_program(World& wd, int ship, int w);

}  // namespace is::world
