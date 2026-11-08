// seg12:0x6d64, a weapon into a new slot (weapon_load.h). M4 stage 5, track WEAPONS.
#include "world/weapon_load.h"

#include <utility>

#include "world/missile.h"
#include "world/world.h"

namespace is::world {

namespace {

WeaponImageHook& image_hook() {
    static WeaponImageHook h;
    return h;
}

}  // namespace

WeaponLoad load_weapon_slot(ShipRecord& s, int w) {
    WeaponLoad r;
    // l = 1; `cbw` of +0xcb, `cmp ax, [bp-4]; jle` (signed 16-bit) [BYTES seg12:0x6d72-0x6d89]
    for (int l = 1; sbyte_at(s, 0xcb) > l; ++l) {
        // `cbw` of +0xb8 + l against the word w: held, return with no write [BYTES
        // seg12:0x6d90-0x6d9a]
        if (sbyte_at(s, 0xb8 + l) == w) return r;
    }
    if (sbyte_at(s, 0xcb) > 9) return r;  // `jg`: ten slots [BYTES seg12:0x6da1-0x6da6]
    set_byte_at(s, 0xcb, static_cast<std::uint8_t>(s.slots_cb() + 1));  // byte inc [BYTES seg12:0x6dad]
    const int l = sbyte_at(s, 0xcb) - 1;                                // [BYTES seg12:0x6db1-0x6db7]
    set_byte_at(s, 0xb8 + l, w & 0xff);                                 // lo8(w) [BYTES seg12:0x6dbf]
    set_byte_at(s, 0xae + l, 0x14);                                     // count 20 [BYTES seg12:0x6dc6]
    r.added = true;
    r.slot = l;
    return r;
}

WeaponLoad load_weapon(World& wd, const WeaponWords& W, int ship, int w, const WeaponImageHook& image) {
    if (ship < 0 || ship >= kShipCount) return WeaponLoad{};  // [DESIGN: the original indexes on]
    ShipRecord& s = wd.ship(ship);
    const WeaponLoad r = load_weapon_slot(s, w);
    if (!r.added) return r;
    // W(w)+0x0a with w the word argument, DGROUP:0x2f5e + 0x50·w [BYTES seg12:0x6dcb-0x6dd4];
    // seg10:0x533a(cbw(+0xb8 + l)), cdecl [BYTES seg12:0x6dd6-0x6de1]
    if (W.word(w, 0x0a) != 0 && image) image(sbyte_at(s, 0xb8 + r.slot));
    return r;
}

void set_weapon_image(WeaponImageHook hook) { image_hook() = std::move(hook); }
const WeaponImageHook& weapon_image() { return image_hook(); }

WeaponLoad load_weapon_program(World& wd, int ship, int w) {
    return load_weapon(wd, missile_state().words, ship, w, image_hook());
}

}  // namespace is::world
