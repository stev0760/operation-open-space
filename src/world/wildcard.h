// WILDCARD DEFENSE (weapon 13, M4 stage 7, track FUEL): seg11:0x9078, which turns every live
// pool-6 record within 650 cells of the user into a fruit (weapon 27). The fruit's end at age 50
// is the pool-6 mover's end kind 5, seg12:0x6a60(6, m) (missile.cpp), whose pool-6 branch is
// pods.cpp pod_destroyed's.
//
// Sources: work/analysis/m4/s7-fuel.md section 5 with s7-fuel-verify.md (claims 36-43 and 46
// confirmed; O10-O12, O14; the key-down rule), and the listing of seg11:0x9078. The manual says
// the weapon "turns the nearest threatening missile into a random harmless fruit" [MANUAL
// Special Ship Systems]; the bytes convert every live pool-6 record in range, whoever launched
// it, the user's own included, and nothing outside pool 6 (O10). core-0009's WILDCARD run had no
// missile in the wave and saw nothing happen [ORACLE core-0009].
//
// The key path: WM_KEYDOWN of Enter or 'Z' calls seg11:0xac86 in `PlayMode` 0 with the selected
// weapon byte 13, and WM_KEYUP calls it for any weapon [BYTES seg11:0x6f58-0x6f74 seg11:0x6e11];
// seg11:0x6b64 reads no lParam, so every auto-repeat WM_KEYDOWN calls it again and one press
// makes at least two calls (key down, key up) (s7-fuel-verify.md "Key-down repeat rule"). The
// port's hud/weapons.h weapon_key carries that rule already: it has no lParam either.
//
// 16-bit arithmetic as the original's (ADR 0013 decision 7). One rand() per fruit picture
// rebuild, in the original's place (rng.md). Portable, no Win32.
#pragma once

#include <array>
#include <cstdint>
#include <functional>

#include "shell/rng.h"
#include "world/collide.h"
#include "world/flight.h"
#include "world/missile.h"
#include "world/records.h"
#include "world/sprite.h"
#include "world/world.h"

namespace is::world {

constexpr int kWeaponWildcard = 13;  // WILDCARD DEFENSE [BYTES seg11:0x90b2]
constexpr int kWeaponShock = 0x17;   // the shock seg11:0x9daa's weapon, also accepted by the gate [BYTES seg11:0x90b9]
constexpr int kWeaponFruit = 27;     // the fruit's weapon block [BYTES seg11:0x90ef seg11:0x910b]
constexpr int kFruitSlot = 0x39;     // image slot DGROUP:0x39e8 + 2·0x39, the last of the 58 [BYTES seg11:0x911e]
constexpr int kFruitGraphic = 0x294; // IGRAPHIC.DLL #4000 660 + rand() % 3 [BYTES seg11:0x91bb]
constexpr std::int16_t kWildcardRange = 0x28a;  // 650 cells of seg8:0x285a = 5200 world units [BYTES seg11:0x9436]
// The fruit pictures' heights: 660 and 662 are 23 × 23, 661 is 27 × 27 [BYTES IGRAPHIC.DLL 4000
// 660 IGRAPHIC.DLL 4000 661 IGRAPHIC.DLL 4000 662] (verified claim 46): all odd.
constexpr std::array<std::int16_t, 3> kFruitHeights = {23, 27, 23};

// The calls out of seg11:0x9078.
class WildcardHost {
public:
    virtual ~WildcardHost() = default;
    // PatBlt(DGROUP:0x13dc, x, y, w, h, BLACKNESS) of the converted missile's square [BYTES
    // seg11:0x943b-0x9458]. The default draws on the window's Blitter (sprite.h wave_blitter),
    // nothing without one.
    virtual void erase(int x, int y, int w, int h);
    // The picture of slot 0x39 [BYTES seg11:0x9146-0x9347] (verified claim 39): the slot's DC and
    // bitmap released when it holds one, graphic `id` loaded (seg10:0x6c02(id, 0, -1)), h its
    // bmHeight (GetObject), a (2g + h) × (2g + h) bitmap blacked with the graphic BitBlt SRCCOPY
    // at (g, g), g = 5 for an odd h else 6, the graphic freed, then seg10:0x6308(0x39, -1, -1, -1)
    // (nothing at `Size` 1). Returns h. The drawing side runs render_fruit_image (below). The
    // default logs once and returns the height of kFruitHeights for 660 ... 662 (0 for any other
    // id) without a picture [DESIGN].
    virtual std::int16_t fruit_image(int id);
};

// The program's links (src/hud/wave_win32.cpp fills fruit_image; an empty slot keeps
// WildcardHost's default).
struct WildcardLinks {
    std::function<void(int x, int y, int w, int h)> erase;  // PatBlt BLACKNESS on DGROUP:0x13dc
    std::function<std::int16_t(int id)> fruit_image;        // seg11:0x9146-0x9347
};
WildcardLinks& wildcard_links();
// The WildcardHost over wildcard_links().
WildcardHost& linked_wildcard_host();

// seg11:0x9078(pool, rec), cdecl [BYTES seg11:0x9078-0x9479] (s7-fuel.md section 5; verified
// claims 36-41): for the ship pool (DGROUP:0x9324) only when a slot 1 ... (int8)+0xcb - 1 holds
// weapon byte 13 or 23; any other pool at once. No fuel, no count, no sound. Then for every m <
// DGROUP:0x9412 (re-read) whose entry age is > 0 and below W(weapon)+0x1e, whose weapon is not
// 27 and whose seg8:0x285a distance from the user is below 650: the square (`scr_x`, `scr_y`,
// `size`, `size`) blacked; e = the highest live weapon-27 entry other than m; entry weapon 27,
// age 1, word +0x0e = W(27)+0x1e (50); the run-time writes W(27)-4 = 0x39 and W(27)+0x24 = 5
// (O12: structs.md's "only run-time write" row misses them); `on_destroy` = seg12:0x6a60. With
// no live fruit (e = -1): the picture of graphic 0x294 + rand() % 3 (the routine's one rand()),
// `size` = its height h, `gap` 5 or 6, `extent` = h << 3, then for an odd h `size` = ((h + 1)
// << `Size`) >> 1 and `gap` = (4 << `Size`) >> 1, for an even h `gap` = (6 << `Size`) >> 1.
// With one: `size` and `gap` copied from e, `extent` kept. `words` is the mutable weapon table
// DGROUP:0x2f54 (missile.h MissileState::words).
void wildcard(World& w, const WaveGlobals& g, WeaponWords& words, shell::Rng& rng, WildcardHost& h,
              int pool, int rec);
// The same on the weapon system's context (c.st.words, c.rng) with linked_wildcard_host(): what
// MissileHost::wildcard's program binding calls.
void wildcard(MissileCtx& c, int pool, int rec);

// The drawing side's part of fruit_image on a Blitter: slot 0x39's old image deleted, then a
// (2g + h) × (2g + h) image blacked (create_image, PatBlt BLACKNESS) and BitBlt(slot, g, g, h, h,
// graphic, 0, 0, SRCCOPY), g = 5 for an odd h else 6, the graphic deleted [BYTES
// seg11:0x9146-0x91a6 seg11:0x925b-0x9338] [DESIGN: the Blitter's create_image stands for
// CreateCompatibleDC + CreateCompatibleBitmap + the SelectObject pairs]. A graphic of kDcNone
// leaves the slot empty. The palette selects of DGROUP:0x920e are the Blitter's (ADR 0007).
void render_fruit_image(Blitter& out, ImageSlotTable& images, DcId graphic, int h);

}  // namespace is::world
