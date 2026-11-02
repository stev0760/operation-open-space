// The wave's records (src/world/records.h; docs/re/structs.md, docs/re/world.md "Notation",
// "World coordinates"): field offsets, the documented DGROUP addresses of ship record 0, the
// 16-bit helpers of ADR 0013 decision 7, the pool store and roles, the effect table and the
// image-slot rule.
#include <cstddef>
#include <cstdint>

#include "doctest.h"
#include "world/records.h"
// The offsetof checks of every field; records.cpp defines no symbols, so including it here
// builds them even before src/world has a library of its own.
#include "world/records.cpp"  // NOLINT(bugprone-suspicious-include)

using namespace is::world;

TEST_CASE("world records: sizes") {
    CHECK(sizeof(ObjectRecord) == 0x70);
    CHECK(sizeof(EffectEntry) == 0x14);
    CHECK(sizeof(ShipRecord) == 0x15a);
    CHECK(sizeof(EffectTable) == 45 * 0x14);
    CHECK(kDgEffectTable + sizeof(EffectTable) == 0x1e90 + 0x384);
    CHECK(sizeof(ShipTable) == 8 * 0x15a);
}

TEST_CASE("world records: object fields quoted in world.md") {
    CHECK(offsetof(ObjectRecord, x) == 0x04);
    CHECK(offsetof(ObjectRecord, y) == 0x06);
    CHECK(offsetof(ObjectRecord, old_scr_x) == 0x08);
    CHECK(offsetof(ObjectRecord, vx) == 0x0c);
    CHECK(offsetof(ObjectRecord, vy) == 0x0e);
    CHECK(offsetof(ObjectRecord, src_x) == 0x10);
    CHECK(offsetof(ObjectRecord, src_y) == 0x12);
    CHECK(offsetof(ObjectRecord, state) == 0x18);
    CHECK(offsetof(ObjectRecord, on_screen) == 0x1a);
    CHECK(offsetof(ObjectRecord, w1c) == 0x1c);
    CHECK(offsetof(ObjectRecord, extent) == 0x20);
    CHECK(offsetof(ObjectRecord, size) == 0x22);
    CHECK(offsetof(ObjectRecord, gap) == 0x24);
    CHECK(offsetof(ObjectRecord, angle) == 0x26);
    CHECK(offsetof(ObjectRecord, frame) == 0x28);
    CHECK(offsetof(ObjectRecord, max_damage) == 0x2c);
    CHECK(offsetof(ObjectRecord, b2e) == 0x2e);
    CHECK(offsetof(ObjectRecord, flame) == 0x2f);
    CHECK(offsetof(ObjectRecord, owner_pool) == 0x32);
    CHECK(offsetof(ObjectRecord, owner_rec) == 0x34);
    CHECK(offsetof(ObjectRecord, w36) == 0x36);
    CHECK(offsetof(ObjectRecord, alt_dc) == 0x3c);
    CHECK(offsetof(ObjectRecord, hit_sound) == 0x42);
    CHECK(offsetof(ObjectRecord, mass) == 0x44);
    CHECK(offsetof(ObjectRecord, radar_brush) == 0x46);
    CHECK(offsetof(ObjectRecord, blast_size) == 0x48);
    CHECK(offsetof(ObjectRecord, radar_color) == 0x4c);
    CHECK(offsetof(ObjectRecord, b50) == 0x50);
    CHECK(offsetof(ObjectRecord, race_status) == 0x51);
    CHECK(offsetof(ObjectRecord, b53) == 0x53);
    CHECK(offsetof(ObjectRecord, b54) == 0x54);
    CHECK(offsetof(ObjectRecord, on_damage) == 0x5e);
    CHECK(offsetof(ObjectRecord, radar_draw) == 0x62);
    CHECK(offsetof(ObjectRecord, on_destroy) == 0x66);
    CHECK(offsetof(ObjectRecord, d6a) == 0x6a);
}

TEST_CASE("world records: effect entry fields") {
    // +0x00 is the frame counter and +0x0a the type (s1-death.md section 0, verified claims 1-3).
    CHECK(offsetof(EffectEntry, frame) == 0x00);
    CHECK(offsetof(EffectEntry, end) == 0x02);
    CHECK(offsetof(EffectEntry, pool) == 0x06);
    CHECK(offsetof(EffectEntry, rec) == 0x08);
    CHECK(offsetof(EffectEntry, type) == 0x0a);
    CHECK(offsetof(EffectEntry, start) == 0x10);
    CHECK(kEffectTypeFF06 == static_cast<std::int16_t>(0xff06));
    EffectTable t{};
    clear_effects(t);
    for (const EffectEntry& e : t) CHECK(e.frame == 0xffff);
}

TEST_CASE("world records: ship record 0 at the DGROUP addresses of structs.md") {
    auto at = [](std::size_t off) { return ship_field_address(0, static_cast<std::uint16_t>(off)); };
    CHECK(at(offsetof(ShipRecord, w00)) == 0x7090);
    CHECK(at(offsetof(ShipRecord, thrusting)) == 0x7092);
    CHECK(at(offsetof(ShipRecord, thrust)) == 0x709c);
    CHECK(at(offsetof(ShipRecord, drag)) == 0x709e);
    CHECK(at(offsetof(ShipRecord, type)) == 0x70a0);
    CHECK(at(offsetof(ShipRecord, spin)) == 0x70a2);
    CHECK(at(offsetof(ShipRecord, turn)) == 0x70a4);
    CHECK(at(offsetof(ShipRecord, turn_acc)) == 0x70a6);
    CHECK(at(offsetof(ShipRecord, weapon_slot)) == 0x70a8);
    CHECK(at(offsetof(ShipRecord, thrust_mult)) == 0x70ac);
    CHECK(at(offsetof(ShipRecord, fuel)) == 0x70ae);
    CHECK(at(offsetof(ShipRecord, w26)) == 0x70b6);
    CHECK(offsetof(ShipRecord, w2a) == 0x2a);
    CHECK(at(offsetof(ShipRecord, w32)) == 0x70c2);
    CHECK(at(offsetof(ShipRecord, w34)) == 0x70c4);
    CHECK(at(offsetof(ShipRecord, w3a)) == 0x70ca);
    CHECK(at(offsetof(ShipRecord, target_pool)) == 0x70ce);
    CHECK(at(offsetof(ShipRecord, target_rec)) == 0x70d0);
    CHECK(at(offsetof(ShipRecord, design_index)) == 0x70de);
    CHECK(at(offsetof(ShipRecord, sprite_slot)) == 0x70e0);
    CHECK(at(offsetof(ShipRecord, anim_step)) == 0x70ea);
    CHECK(at(offsetof(ShipRecord, anim_last)) == 0x70eb);
    CHECK(at(offsetof(ShipRecord, anim_frame)) == 0x70ec);
    CHECK(at(offsetof(ShipRecord, overthrust_cool)) == 0x70f0);
    CHECK(at(offsetof(ShipRecord, w72)) == 0x7102);
    CHECK(at(offsetof(ShipRecord, lockout)) == 0x7114);
    CHECK(at(offsetof(ShipRecord, d88)) == 0x7118);
    CHECK(at(offsetof(ShipRecord, flame_until)) == 0x7120);
    CHECK(at(offsetof(ShipRecord, weapon_count)) == 0x713e);
    CHECK(at(offsetof(ShipRecord, weapon)) == 0x7148);
    CHECK(at(offsetof(ShipRecord, roster_index)) == 0x7166);
    CHECK(at(offsetof(ShipRecord, kill_value)) == 0x7168);
    CHECK(at(offsetof(ShipRecord, laps)) == 0x716a);
    CHECK(at(offsetof(ShipRecord, be2)) == 0x7172);
    CHECK(at(offsetof(ShipRecord, has_flame)) == 0x7174);
    CHECK(at(offsetof(ShipRecord, anim_wrap)) == 0x7176);
    CHECK(at(offsetof(ShipRecord, be7)) == 0x7177);
    CHECK(at(offsetof(ShipRecord, be9)) == 0x7179);
    CHECK(at(offsetof(ShipRecord, beb)) == 0x717b);
    CHECK(at(offsetof(ShipRecord, bec)) == 0x717c);
    CHECK(at(offsetof(ShipRecord, bee)) == 0x717e);
    CHECK(at(offsetof(ShipRecord, radar_hide)) == 0x71d7);
    CHECK(at(offsetof(ShipRecord, checkpoint)) == 0x71d8);
    CHECK(at(offsetof(ShipRecord, due156)) == 0x71e6);
    // structs.md: the highest operand is the word at DGROUP:0x71e8 (+0x158), record 1 starts at
    // DGROUP:0x71ea, and record 1's +0x94 is DGROUP:0x727e.
    CHECK(at(offsetof(ShipRecord, due156) + 2) == 0x71e8);
    CHECK(ship_field_address(1, 0) == 0x71ea);
    CHECK(ship_field_address(1, offsetof(ShipRecord, d94)) == 0x727e);
    CHECK(ship_field_address(8, 0) == 0x7b60);
    ShipRecord s{};
    s.roster_word(2) = 35;
    CHECK(s.w2a == 35);
}

TEST_CASE("world records: 16-bit helpers") {
    CHECK(mul16(200, 200) == static_cast<std::int16_t>(40000 - 65536));
    CHECK(mul16(-3, 7) == -21);
    CHECK(mul16(-32768, -1) == -32768);
    CHECK(lo16(0x12345) == 0x2345);
    CHECK(lo16(-1) == -1);
    CHECK(add16(32767, 1) == -32768);
    CHECK(add16(-32768, -1) == 32767);
    CHECK(sub16(-32768, 1) == 32767);
    CHECK(shl16(0x4000, 1) == -32768);
    CHECK(shl16(1600, 4) == 25600);
    CHECK(shl16(3000, 4) == static_cast<std::int16_t>(48000 - 65536));
    CHECK(sar32(-1, 3) == -1);
    CHECK(sar32(-9, 3) == -2);
    CHECK(sar32(9, 3) == 1);
    CHECK(sar16(-32768, 15) == -1);
    CHECK(sar16(-7, 1) == -4);
    CHECK(idiv16(-7, 2) == -3);
    CHECK(imod16(-7, 2) == -1);
    CHECK(div32(-70000, 3) == -23333);
    Dword d;
    d.set(0xfffffffeu);
    CHECK(d.lo == 0xfffe);
    CHECK(d.hi == 0xffff);
    CHECK(d.get_signed() == -2);
    d.set(0x80000000u);
    CHECK(d.get_signed() == INT32_MIN);
}

TEST_CASE("world records: world constants") {
    CHECK(kWorldCells == 3200);
    CHECK(kWorldUnits == 25600);
    CHECK(pixel_shift(1) == 3);
    CHECK(pixel_units(1) == 8);
    CHECK(pixel_units(0) == 16);
    CHECK(pixel_units(2) == 4);
    // world.md Record templates: icons (32 << Size) >> 1, rocks 44 and 22, gaps 6 and 4.
    CHECK(sprite_side(32, 1) == 32);
    CHECK(sprite_side(32, 2) == 64);
    CHECK(sprite_side(32, 0) == 16);
    CHECK(sprite_side(44, 1) == 44);
    CHECK(sprite_side(6, 0) == 3);
    CHECK(sprite_side(4, 2) == 8);
}

TEST_CASE("world records: pool store") {
    Pools p;
    for (int k = 0; k < kStartupPools; ++k) {
        CHECK(p.allocated(k) == 10);
        CHECK(p.records(k) == 10);
        CHECK(p.in_use(k) == 0);
        CHECK(p.rec(k, 9).x == 0);
    }
    for (int k = kSeg18FirstPool; k < kPoolCount; ++k) CHECK_FALSE(p.exists(k));
    p.create_seg18();
    for (int k = kSeg18FirstPool; k < kPoolCount; ++k) CHECK(p.records(k) == 1);
    p.free_seg18();
    CHECK_FALSE(p.exists(20));

    // seg9:0x22b4 reallocates to max(count, 1); the leading records survive, new ones are zero.
    p.rec(kPoolRocks, 0).x = 1234;
    p.resize(kPoolRocks, 0);
    CHECK(p.allocated(kPoolRocks) == 0);
    CHECK(p.records(kPoolRocks) == 1);
    CHECK(p.rec(kPoolRocks, 0).x == 1234);
    p.resize(kPoolRocks, 40);
    CHECK(p.records(kPoolRocks) == 40);
    CHECK(p.rec(kPoolRocks, 39).mass == 0);

    // seg7:0x52c7 grows to index + 1 only when the index is at or above the count.
    p.grow_to(kPoolRocks, 39);
    CHECK(p.allocated(kPoolRocks) == 40);
    p.grow_to(kPoolRocks, 44);
    CHECK(p.allocated(kPoolRocks) == 45);
    CHECK(p.records(kPoolRocks) == 45);

    // seg7:0x1090 clamps negative or excessive in-use counts to the allocated count.
    p.set_in_use(kPoolRocks, 60);
    p.set_in_use(kPoolIcons, -1);
    p.set_in_use(kPool4, 5);
    p.clamp_in_use(kPoolShips);
    CHECK(p.in_use(kPoolRocks) == 45);
    CHECK(p.in_use(kPoolIcons) == 10);
    CHECK(p.in_use(kPool4) == 5);

    // seg12:0x274a's index test: the count rises for every index at or above in_use, also when
    // the routine goes on [BYTES seg12:0x2765 seg12:0x2767].
    p.resize(kPoolShips, 4);
    p.set_in_use(kPoolShips, 2);
    CHECK(p.destroy_index_ok(kPoolShips, 1, 14, 3));
    CHECK(p.bad_index_count == 0);
    CHECK(p.destroy_index_ok(kPoolShips, 3, 14, 3));
    CHECK(p.bad_index_count == 1);
    CHECK_FALSE(p.destroy_index_ok(kPoolShips, 4, 14, 3));
    CHECK(p.bad_index_count == 2);
    p.resize(kPoolShips, 8);
    CHECK_FALSE(p.destroy_index_ok(kPoolShips, 5, 14, 3));
    CHECK(p.bad_index_count == 3);
}

TEST_CASE("world records: roles, operands and image slots") {
    CHECK(wave_roles(false).ships == 14);
    CHECK(wave_roles(false).radar_first == 0);
    CHECK(wave_roles(true).radar_first == 1);
    CHECK(wave_roles(false).pool5 == 5);
    CHECK(wave_roles(false).rocks == 3);
    CHECK(seg18_roles().ships == 20);
    CHECK(seg18_roles().radar_first == 15);
    CHECK(seg18_roles().pool5 == 17);
    CHECK(seg18_roles().rocks == 19);

    CHECK(pool_rec(14, 0) == 0x0e00);
    CHECK(pool_of(pool_rec(14, 3)) == 14);
    CHECK(rec_of(pool_rec(14, 3)) == 3);
    CHECK(pool_of(pool_rec(20, 255)) == 20);

    CHECK(kImageSlots == 58);
    ShipRecord s{};
    s.has_flame = 1;
    s.sprite_slot = 5;
    CHECK(ship_image_slot(2, s) == 0x2a);
    s.has_flame = 0;
    CHECK(ship_image_slot(2, s) == 0x2d);
    CHECK(type_cells_by_heading(0));
    CHECK(type_cells_by_heading(7));
    CHECK_FALSE(type_cells_by_heading(3));
}
