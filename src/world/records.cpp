// Layout checks of src/world/records.h: every field of the object record, the effect entry and
// the ship record at the offset structs.md gives it (docs/re/structs.md "Object record (0x70
// bytes)", "Effect table", "Ship records"). The API itself is header-only.
#include "world/records.h"

#include <cstddef>
#include <type_traits>

namespace is::world {
namespace {

static_assert(std::is_standard_layout<ObjectRecord>::value, "offsetof needs standard layout");
static_assert(std::is_standard_layout<EffectEntry>::value, "offsetof needs standard layout");
static_assert(std::is_standard_layout<ShipRecord>::value, "offsetof needs standard layout");

#define IS_AT(T, f, off) static_assert(offsetof(T, f) == (off), #T "::" #f " at " #off)

// Object record [BYTES see records.h per field].
IS_AT(ObjectRecord, scr_x, 0x0);
IS_AT(ObjectRecord, scr_y, 0x2);
IS_AT(ObjectRecord, x, 0x4);
IS_AT(ObjectRecord, y, 0x6);
IS_AT(ObjectRecord, old_scr_x, 0x8);
IS_AT(ObjectRecord, old_scr_y, 0xa);
IS_AT(ObjectRecord, vx, 0xc);
IS_AT(ObjectRecord, vy, 0xe);
IS_AT(ObjectRecord, src_x, 0x10);
IS_AT(ObjectRecord, src_y, 0x12);
IS_AT(ObjectRecord, w14, 0x14);
IS_AT(ObjectRecord, w16, 0x16);
IS_AT(ObjectRecord, state, 0x18);
IS_AT(ObjectRecord, on_screen, 0x1a);
IS_AT(ObjectRecord, w1c, 0x1c);
IS_AT(ObjectRecord, damage, 0x1e);
IS_AT(ObjectRecord, extent, 0x20);
IS_AT(ObjectRecord, size, 0x22);
IS_AT(ObjectRecord, gap, 0x24);
IS_AT(ObjectRecord, angle, 0x26);
IS_AT(ObjectRecord, frame, 0x28);
IS_AT(ObjectRecord, w2a, 0x2a);
IS_AT(ObjectRecord, max_damage, 0x2c);
IS_AT(ObjectRecord, b2e, 0x2e);
IS_AT(ObjectRecord, flame, 0x2f);
IS_AT(ObjectRecord, w30, 0x30);
IS_AT(ObjectRecord, owner_pool, 0x32);
IS_AT(ObjectRecord, owner_rec, 0x34);
IS_AT(ObjectRecord, w36, 0x36);
IS_AT(ObjectRecord, w38, 0x38);
IS_AT(ObjectRecord, w3a, 0x3a);
IS_AT(ObjectRecord, alt_dc, 0x3c);
IS_AT(ObjectRecord, w3e, 0x3e);
IS_AT(ObjectRecord, w40, 0x40);
IS_AT(ObjectRecord, hit_sound, 0x42);
IS_AT(ObjectRecord, mass, 0x44);
IS_AT(ObjectRecord, radar_brush, 0x46);
IS_AT(ObjectRecord, blast_size, 0x48);
IS_AT(ObjectRecord, w4a, 0x4a);
IS_AT(ObjectRecord, radar_color, 0x4c);
IS_AT(ObjectRecord, b50, 0x50);
IS_AT(ObjectRecord, race_status, 0x51);
IS_AT(ObjectRecord, b52, 0x52);
IS_AT(ObjectRecord, b53, 0x53);
IS_AT(ObjectRecord, b54, 0x54);
IS_AT(ObjectRecord, b55, 0x55);
IS_AT(ObjectRecord, r56, 0x56);
IS_AT(ObjectRecord, on_damage, 0x5e);
IS_AT(ObjectRecord, radar_draw, 0x62);
IS_AT(ObjectRecord, on_destroy, 0x66);
IS_AT(ObjectRecord, d6a, 0x6a);
IS_AT(ObjectRecord, w6e, 0x6e);

// Effect entry.
IS_AT(EffectEntry, kind, 0x0);
IS_AT(EffectEntry, w02, 0x2);
IS_AT(EffectEntry, w04, 0x4);
IS_AT(EffectEntry, pool, 0x6);
IS_AT(EffectEntry, rec, 0x8);
IS_AT(EffectEntry, w0a, 0xa);
IS_AT(EffectEntry, w0c, 0xc);
IS_AT(EffectEntry, w0e, 0xe);
IS_AT(EffectEntry, time, 0x10);

// Ship record; DGROUP:0x7090 + offset for record 0.
IS_AT(ShipRecord, w00, 0x0);
IS_AT(ShipRecord, thrusting, 0x2);
IS_AT(ShipRecord, w04, 0x4);
IS_AT(ShipRecord, w06, 0x6);
IS_AT(ShipRecord, w08, 0x8);
IS_AT(ShipRecord, w0a, 0xa);
IS_AT(ShipRecord, thrust, 0xc);
IS_AT(ShipRecord, drag, 0xe);
IS_AT(ShipRecord, type, 0x10);
IS_AT(ShipRecord, spin, 0x12);
IS_AT(ShipRecord, turn, 0x14);
IS_AT(ShipRecord, turn_acc, 0x16);
IS_AT(ShipRecord, weapon_slot, 0x18);
IS_AT(ShipRecord, w1a, 0x1a);
IS_AT(ShipRecord, thrust_mult, 0x1c);
IS_AT(ShipRecord, fuel, 0x1e);
IS_AT(ShipRecord, w20, 0x20);
IS_AT(ShipRecord, w22, 0x22);
IS_AT(ShipRecord, w24, 0x24);
IS_AT(ShipRecord, w26, 0x26);
IS_AT(ShipRecord, w28, 0x28);
IS_AT(ShipRecord, w2a, 0x2a);
IS_AT(ShipRecord, w2c, 0x2c);
IS_AT(ShipRecord, w2e, 0x2e);
IS_AT(ShipRecord, w30, 0x30);
IS_AT(ShipRecord, w32, 0x32);
IS_AT(ShipRecord, w34, 0x34);
IS_AT(ShipRecord, w36, 0x36);
IS_AT(ShipRecord, w38, 0x38);
IS_AT(ShipRecord, w3a, 0x3a);
IS_AT(ShipRecord, w3c, 0x3c);
IS_AT(ShipRecord, target_pool, 0x3e);
IS_AT(ShipRecord, target_rec, 0x40);
IS_AT(ShipRecord, w42, 0x42);
IS_AT(ShipRecord, w44, 0x44);
IS_AT(ShipRecord, w46, 0x46);
IS_AT(ShipRecord, w48, 0x48);
IS_AT(ShipRecord, w4a, 0x4a);
IS_AT(ShipRecord, w4c, 0x4c);
IS_AT(ShipRecord, design_index, 0x4e);
IS_AT(ShipRecord, sprite_slot, 0x50);
IS_AT(ShipRecord, w52, 0x52);
IS_AT(ShipRecord, w54, 0x54);
IS_AT(ShipRecord, w56, 0x56);
IS_AT(ShipRecord, w58, 0x58);
IS_AT(ShipRecord, anim_step, 0x5a);
IS_AT(ShipRecord, anim_last, 0x5b);
IS_AT(ShipRecord, anim_frame, 0x5c);
IS_AT(ShipRecord, b5d, 0x5d);
IS_AT(ShipRecord, w5e, 0x5e);
IS_AT(ShipRecord, overthrust_cool, 0x60);
IS_AT(ShipRecord, b61, 0x61);
IS_AT(ShipRecord, r62, 0x62);
IS_AT(ShipRecord, w72, 0x72);
IS_AT(ShipRecord, w82, 0x82);
IS_AT(ShipRecord, lockout, 0x84);
IS_AT(ShipRecord, d88, 0x88);
IS_AT(ShipRecord, d8c, 0x8c);
IS_AT(ShipRecord, flame_until, 0x90);
IS_AT(ShipRecord, d94, 0x94);
IS_AT(ShipRecord, r98, 0x98);
IS_AT(ShipRecord, weapon_count, 0xae);
IS_AT(ShipRecord, weapon, 0xb8);
IS_AT(ShipRecord, rc2, 0xc2);
IS_AT(ShipRecord, roster_index, 0xd6);
IS_AT(ShipRecord, kill_value, 0xd8);
IS_AT(ShipRecord, laps, 0xda);
IS_AT(ShipRecord, wdc, 0xdc);
IS_AT(ShipRecord, wde, 0xde);
IS_AT(ShipRecord, we0, 0xe0);
IS_AT(ShipRecord, be2, 0xe2);
IS_AT(ShipRecord, be3, 0xe3);
IS_AT(ShipRecord, has_flame, 0xe4);
IS_AT(ShipRecord, be5, 0xe5);
IS_AT(ShipRecord, anim_wrap, 0xe6);
IS_AT(ShipRecord, be7, 0xe7);
IS_AT(ShipRecord, be8, 0xe8);
IS_AT(ShipRecord, be9, 0xe9);
IS_AT(ShipRecord, bea, 0xea);
IS_AT(ShipRecord, beb, 0xeb);
IS_AT(ShipRecord, bec, 0xec);
IS_AT(ShipRecord, bed, 0xed);
IS_AT(ShipRecord, bee, 0xee);
IS_AT(ShipRecord, rf6, 0xf6);
IS_AT(ShipRecord, radar_hide, 0x147);
IS_AT(ShipRecord, checkpoint, 0x148);
IS_AT(ShipRecord, r149, 0x149);
IS_AT(ShipRecord, due156, 0x156);

#undef IS_AT

}  // namespace
}  // namespace is::world
