// M4 stage 2, track DISPLAY: seg11:0x2ff6's composed weapon picture (src/hud/weapons.h
// weapon_picture, compose_weapon_picture), the selection seg11:0xadf0 / seg11:0xae94 and the
// weapon keys of seg11:0x6b64 (weapon_key). work/analysis/m4/s2-display.md with its -verify.md.
#include <array>
#include <vector>

#include "doctest.h"
#include "hud/weapons.h"
#include "hud_fakes.h"

using namespace is::hud;
using hudtest::Call;
using hudtest::RecordingCanvas;

TEST_CASE("hud weapon picture: weapon 8 at Size 0, 1 and 2") {
    const WeaponTable t(0);
    WeaponPictureInputs in;
    in.size_level = 1;
    in.sheet_slot = 0x30;
    const WeaponPicture p = weapon_picture(t, 8, in);
    REQUIRE(p.kind == WeaponPicture::Kind::kComposed);
    CHECK(p.name == 194);
    CHECK(p.overlay == 0);      // +0x14 = 0, w != 0x18
    CHECK(p.side_plain == 46);  // 2·23 (claim 10)
    CHECK(p.cell == 24);        // 23 odd: 24 with gap 4
    CHECK(p.gap == 4);
    CHECK(p.sx == 116);         // 4·24 + 5·4: frame 4 [BYTES seg11:0x3297]
    CHECK(p.sy == 4);
    CHECK(p.sheet_slot == 0x30);
    in.size_level = 0;
    const WeaponPicture q = weapon_picture(t, 8, in);
    CHECK(q.cell == 12);
    CHECK(q.gap == 2);
    CHECK(q.sx == 58);
    CHECK(q.side_plain == 46);  // the destination does not scale with Size
    in.size_level = 2;
    const WeaponPicture r = weapon_picture(t, 8, in);
    CHECK(r.cell == 48);
    CHECK(r.gap == 8);
    CHECK(r.sx == 232);
    // without the run-time slot the table's W(8)-4 = -1
    CHECK(weapon_picture(t, 8).sheet_slot == -1);
}

TEST_CASE("hud weapon picture: the overlay, the names and W+0x06") {
    const WeaponTable t(0);
    // w9 DEATH HEADS: +0x14 = 4 -> 0xbf (191); A = 21 -> 22, gap 4; name 195 = 0xc3 -> sx = gap
    WeaponPicture p = weapon_picture(t, 9);
    CHECK(p.overlay == 0xbf);
    CHECK(p.side_overlay == 42);
    CHECK(p.name == 0xc3);
    CHECK(p.sx == 4);
    CHECK(p.side_plain == 28);  // (21 << 2) / 3
    // w10 REAR-DEPLOYED MINES: name 0xc4, W(10)-8 = 22 (block 9's +0x48): side 88 / 3 = 29;
    // +0x06 = 4 with +0x26 = 6: (6 - 1)·(22 + 6) + 6
    p = weapon_picture(t, 10);
    CHECK(p.name == 0xc4);
    CHECK(p.overlay == 0);
    CHECK(p.side_plain == 29);
    CHECK(p.sx == 5 * 28 + 6);
    // w12 FIRE DEMONS: name 0xc6 -> sx = sy = gap; +0x14 = 1 -> 0xbf
    p = weapon_picture(t, 12);
    CHECK(p.name == 0xc6);
    CHECK(p.overlay == 0xbf);
    CHECK(p.sx == p.gap);
    // w14: +0x14 = 3 -> 0xc5
    CHECK(weapon_picture(t, 14).overlay == 0xc5);
    // w24 (0x18): the roster bytes, overlay 0xbd, the player's sheet 0x28 + player, S = A
    WeaponPictureInputs in;
    in.player = 2;
    in.roster_7a = 31;
    p = weapon_picture(t, 0x18, in);
    CHECK(p.overlay == 0xbd);
    CHECK(p.a_raw == 31);
    CHECK(p.side_overlay == 31);
    CHECK(p.cell == 32);
    CHECK(p.sheet_slot == 0x2a);
}

TEST_CASE("hud weapon picture: seg11:0x30d0's calls for weapon 8") {
    const WeaponTable t(0);
    WeaponPictureInputs in;
    in.sheet_slot = 0x30;
    const WeaponPicture p = weapon_picture(t, 8, in);
    RecordingCanvas c;
    c.sizes[Img::kWeaponSheet] = {476, 33};
    compose_weapon_picture(c, p);
    REQUIRE(c.calls.size() == 3);
    CHECK(c.calls[0].op == Call::kCreate);
    CHECK(c.calls[0].dst == Img::kWeapon);
    CHECK(c.calls[0].w == 46);
    CHECK(c.calls[0].h == 46);
    CHECK(c.calls[1].op == Call::kPat);
    CHECK(c.calls[1].rop == kBlackness);
    CHECK(c.calls[2].op == Call::kStretch);
    CHECK(c.calls[2].x == 0);
    CHECK(c.calls[2].y == 0);
    CHECK(c.calls[2].w == 46);
    CHECK(c.calls[2].h == 46);
    CHECK(c.calls[2].src == Img::kWeaponSheet);
    CHECK(c.calls[2].sx == 116);
    CHECK(c.calls[2].sy == 4);
    CHECK(c.calls[2].sw == 24);
    CHECK(c.calls[2].sh == 24);
    CHECK(c.calls[2].rop == kSrcCopy);
}

TEST_CASE("hud weapon picture: the overlay's placement") {
    const WeaponTable t(0);
    const WeaponPicture p = weapon_picture(t, 9);  // S = 42
    RecordingCanvas c;
    c.sizes[Img::kWeaponSheet] = {300, 30};
    c.sizes[Img::kWeaponOverlay] = {57, 29};  // graphic 191 (s2-display.md section 1)
    compose_weapon_picture(c, p);
    REQUIRE(c.calls.size() == 4);
    CHECK(c.calls[0].w == 99);  // gw + S
    CHECK(c.calls[0].h == 42);  // max(gh, S)
    CHECK(c.calls[2].y == 0);   // H = S: the cell at the top
    CHECK(c.calls[3].op == Call::kBlt);
    CHECK(c.calls[3].x == 42);
    CHECK(c.calls[3].y == 6);   // (42 - 29) / 2
    CHECK(c.calls[3].w == 57);
    CHECK(c.calls[3].src == Img::kWeaponOverlay);
    // a taller overlay centres the cell instead
    RecordingCanvas d;
    d.sizes[Img::kWeaponSheet] = {300, 30};
    d.sizes[Img::kWeaponOverlay] = {20, 51};
    compose_weapon_picture(d, p);
    CHECK(d.calls[0].h == 51);
    CHECK(d.calls[2].y == 4);  // (51 - 42) / 2
    CHECK(d.calls[3].y == 0);
    // a failed overlay load (G = 0) takes the square path
    RecordingCanvas e;
    compose_weapon_picture(e, p);
    CHECK(e.calls[0].w == 28);  // name 0xc3: (W-8 << 2) / 3
    CHECK(e.calls.size() == 2);  // no sheet: no stretch
}

TEST_CASE("hud weapons: seg11:0xadf0 Tab and seg11:0xae94") {
    // core-0002: +0xb9 = 8, +0xcb = 2 at the wave start; Tab twice changes nothing visible
    std::array<int, 12> wb{0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    const auto byte = [&wb](int j) { return j >= 0 && j < 12 ? wb[static_cast<std::size_t>(j)] : 0; };
    TabSelection s = tab_select(1, 2, byte, 0);
    CHECK(s.slot == 1);  // 2, then weapon 0 skipped to 3, past +0xcb: Action mode wraps to 1
    CHECK(s.dg_9524 == 1);
    CHECK(!s.clear_b50);
    s = tab_select(1, 2, byte, 1);
    CHECK(s.slot == 0);  // Strategy mode wraps to 0
    s = tab_select(1, 1, byte, 0);
    CHECK(s.slot == 0);  // one slot or fewer
    CHECK(s.dg_9524 == 1);
    // three slots: 1 -> 2 -> wrap; a weapon-0 slot skipped once
    wb = {0, 8, 3, 5, 0, 0, 0, 0, 0, 0, 0, 0};
    CHECK(tab_select(1, 4, byte, 0).slot == 2);
    s = tab_select(2, 4, byte, 0);
    CHECK(s.slot == 3);
    CHECK(s.clear_b50);  // leaving weapon 3 (LIGHTNING)
    wb = {0, 8, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0};
    CHECK(tab_select(1, 4, byte, 0).slot == 3);
    CHECK(wave_start_slot(0, 2, 0) == 1);
    CHECK(wave_start_slot(0, 1, 0) == 0);
    CHECK(wave_start_slot(3, 2, 1) == 3);  // DGROUP:0x92e4 != 0
}

TEST_CASE("hud weapons: the weapon keys of seg11:0x6b64") {
    WeaponKeyState s;
    CHECK(weapon_key(0x101, 'Z', s) == WeaponKey::kFire);
    CHECK(weapon_key(0x101, 0x0d, s) == WeaponKey::kFire);
    CHECK(weapon_key(0x101, 0x1b, s) == WeaponKey::kNone);  // ESC goes on
    CHECK(weapon_key(0x100, 'Z', s) == WeaponKey::kIgnored);  // weapon 8: a press does nothing
    CHECK(weapon_key(0x100, 0x09, s) == WeaponKey::kTab);
    CHECK(weapon_key(0x100, 'T', s) == WeaponKey::kNone);
    s.weapon = 13;
    CHECK(weapon_key(0x100, 'Z', s) == WeaponKey::kFire);  // the WILDCARD
    s.play_mode = 1;
    CHECK(weapon_key(0x100, 'Z', s) == WeaponKey::kIgnored);
    CHECK(weapon_key(0x101, 'Z', s) == WeaponKey::kIgnored);
    CHECK(weapon_key(0x100, 0x09, s) == WeaponKey::kTab);  // no PlayMode test
    s.play_mode = 0;
    s.gated = true;
    CHECK(weapon_key(0x101, 'Z', s) == WeaponKey::kIgnored);
    CHECK(weapon_key(0x100, 0x09, s) == WeaponKey::kIgnored);
    s.gated = false;
    s.demo = 0;
    CHECK(weapon_key(0x101, 'Z', s) == WeaponKey::kIgnored);
    CHECK(weapon_key(0x100, 0x09, s) == WeaponKey::kIgnored);
}
