// seg11:0x1bcc (status.h). (M3 s2 fixer)
#include "hud/status.h"

namespace is::hud {

void status_tick(world::World& w, std::uint32_t now, StatusState& s, const StatusHooks& h) {
    using world::add16;
    using world::sub16;
    auto update = [&h](int arg) {
        if (h.update) h.update(arg);
    };
    // The score [BYTES seg11:0x1bda-0x1beb].
    if (h.score && h.score_shown && h.score_shown() != h.score()) update(0);
    // The lamp blink every 300 ms [BYTES seg11:0x1bec-0x1c1a].
    if (s.blink_due < now) {
        s.blink_due = now + 300u;
        s.blink = s.blink == 0 ? 1 : 0;  // cmp 1; sbb; neg
        update(1);
    }
    // The clock: seg5:0x2b4(DGROUP:0x1e68, 36), unsigned, against DGROUP:0x934e [BYTES
    // seg11:0x1c1b-0x1c35].
    if (static_cast<std::int16_t>(w.frames / 36u) != w.w934e) update(7);
    // Every 1000 ms: fuel and mass [BYTES seg11:0x1c36-0x1d6b].
    if (s.fuel_due < now) {
        s.fuel_due = now + 1000u;
        for (int i = 0; i <= w.last_ship && i < world::kShipCount; ++i) {
            world::ShipRecord& r = w.ship(i);
            // al = 5 · byte +0xe8 (imul byte) [BYTES seg11:0x1c5c-0x1c63 seg11:0x1d43-0x1d45]
            const std::int16_t burn = static_cast<std::int16_t>(5 * static_cast<std::int8_t>(r.be8));
            std::int16_t f;
            if (r.thrusting == 0) {
                f = sub16(r.fuel, burn);  // [BYTES seg11:0x1c67-0x1c6b]
            } else {
                // +0x0c / -40 (idiv) - burn + fuel [BYTES seg11:0x1d4b-0x1d59]
                f = add16(sub16(static_cast<std::int16_t>(r.thrust / -40), burn), r.fuel);
            }
            if (f < 0) f = 0;
            r.fuel = f;
            if (r.be8 != 0) {  // [BYTES seg11:0x1c7c-0x1c9e]
                r.fuel = sub16(r.fuel, 3);
                if (r.fuel < 0x78) {
                    if (h.seg11_4c72) h.seg11_4c72(i);
                    if (h.seg3_332e) h.seg3_332e(0x4090418u);
                }
            }
            // Weapon kind 3 in the current slot: object byte +0x50 moves toward 0x6d, by half
            // for the player and a fifth for the others [BYTES seg11:0x1ca3-0x1cfa].
            if (w.pools.records(w.roles.ships) > i) {
                world::ObjectRecord& o = w.ship_object(i);
                if (world::sbyte_at(r, 0xb8 + r.weapon_slot) == 3) {
                    const std::int16_t d =
                        static_cast<std::int16_t>(0x6d - static_cast<std::int8_t>(o.b50));
                    const std::int16_t step = i == w.player ? static_cast<std::int16_t>(d / 2)
                                                            : static_cast<std::int16_t>(d / 5);
                    o.b50 = static_cast<std::uint8_t>(o.b50 + static_cast<std::uint8_t>(step));
                }
                // +0x44 = fuel / 20 + the design mass [BYTES seg11:0x1cfe-0x1d26]
                const std::int16_t dm = h.design_mass ? h.design_mass(r.design_index) : 0;
                o.mass = add16(static_cast<std::int16_t>(r.fuel / 20), dm);
            }
        }
        update(6);                         // [BYTES seg11:0x1d64]
        if (h.seg13_2dcc) h.seg13_2dcc();  // [BYTES seg11:0x1d6b]
    }
    if (h.seg11_1d7c) h.seg11_1d7c();  // [BYTES seg11:0x1d71]
}

}  // namespace is::hud
