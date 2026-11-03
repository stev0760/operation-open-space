// Fuel, the cloak and the shield (M4 s3 FUEL): the status tick's fuel block seg11:0x1bcc
// (src/hud/status.h), the toggles seg11:0x4c72 and seg11:0x4b00, seg3:0x332e and the CapsLock key
// (src/world/input.h), against work/analysis/m4/s3-fuel.md with s3-fuel-verify.md and the
// oracle's burn of core-0003.
#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

#include "doctest.h"
#include "hud/status.h"
#include "world/collide.h"
#include "world/input.h"
#include "world/world.h"

using namespace is::world;

namespace {

struct Rig {
    World w;
    is::hud::StatusState st;
    is::hud::StatusHooks h;
    std::vector<int> cloaks;
    std::vector<std::uint32_t> notices;
    Rig() {
        w.last_ship = 0;
        w.player = 0;
        w.pools.set_in_use(kPoolShips, 1);
        ShipRecord& s = w.ship(0);
        s.fuel = 1320;
        s.thrust = 205;  // TRANSFORMER: 5 x SHP thruster 41
        h.design_mass = [](int) -> std::int16_t { return 259; };
        h.seg11_4c72 = [this](int i) { cloaks.push_back(i); };
        h.seg3_332e = [this](std::uint32_t ids) { notices.push_back(ids); };
    }
};

class RecSystems : public SystemsHost {
public:
    std::vector<std::tuple<int, int, int>> vanish;  // (mode, pool, ship); mode 0 = the buffer
    std::vector<std::tuple<int, int, int, int>> sounds;
    std::vector<std::pair<int, int>> flames;
    int renders = 0;
    std::string line;
    void vanish_buffer(int pool, int ship) override { vanish.emplace_back(0, pool, ship); }
    void vanish_start(int mode, int pool, int ship) override { vanish.emplace_back(mode, pool, ship); }
    void shield_render(int) override { ++renders; }
    void flame(int on, int ship) override { flames.emplace_back(on, ship); }
    void sound(int id, int pool, int rec, int mode) override { sounds.emplace_back(id, pool, rec, mode); }
    std::string lang(int id) override {
        return id == 1033 ? "Uncloaking " : id == 1048 ? "Because I'm Low On Gas." : id == 1047 ? "Due To System Damage." : "";
    }
    std::string pilot() override { return "PILOT"; }
    void strategy_line(const std::string& t) override { line = t; }
};

struct ShipRig {
    World w;
    RecSystems h;
    ShipRig() {
        w.player = 0;
        w.last_ship = 1;
        w.pools.set_in_use(kPoolShips, 2);
        for (int i = 0; i < 2; ++i) {
            ShipRecord& s = w.ship(i);
            s.fuel = 500;
            s.w2c = 100;
            ObjectRecord& o = w.ship_object(i);
            o.size = 33;
            o.x = 1200;
            o.y = -300;
        }
    }
    SystemsCtx ctx(std::uint32_t frames, std::uint32_t now = 9000) { return SystemsCtx{w, now, frames, h}; }
};

}  // namespace

TEST_CASE("fuel tick: the TRANSFORMER burns 5 a second thrusting, 13 cloaked [BYTES seg11:0x1c36-0x1d6b] [ORACLE core-0003]") {
    Rig r;
    ShipRecord& s = r.w.ship(0);
    s.thrusting = 1;
    // core-0003 "Burn while thrusting": 1320 -> 1315, then -5 every second.
    is::hud::status_tick(r.w, 1001, r.st, r.h);
    CHECK(s.fuel == 1315);
    is::hud::status_tick(r.w, 2002, r.st, r.h);
    CHECK(s.fuel == 1310);
    CHECK(r.w.ship_object(0).mass == 1310 / 20 + 259);
    // Cloaked and thrusting: 205 / -40 = -5, 5 for the cloak, then the cloak's 3: 13.
    s.be8 = 1;
    is::hud::status_tick(r.w, 3003, r.st, r.h);
    CHECK(s.fuel == 1297);
    CHECK(r.cloaks.empty());
    // Cloaked and idle: 5 + 3.
    s.thrusting = 0;
    is::hud::status_tick(r.w, 4004, r.st, r.h);
    CHECK(s.fuel == 1289);
}

TEST_CASE("fuel tick: the floor comes before the cloak's 3, the auto-off below 120 whatever it returns [BYTES seg11:0x1c6d-0x1c9e]") {
    Rig r;
    ShipRecord& s = r.w.ship(0);
    s.be8 = 1;
    s.fuel = 4;
    is::hud::status_tick(r.w, 1001, r.st, r.h);
    // 4 - 5 floors at 0, then -3 without a floor (verified claim 1).
    CHECK(s.fuel == -3);
    CHECK(r.cloaks == std::vector<int>{0});
    CHECK(r.notices == std::vector<std::uint32_t>{0x04090418u});
    // Still cloaked (the stub did not toggle): it goes on every tick, and the fuel goes on down.
    is::hud::status_tick(r.w, 2002, r.st, r.h);
    CHECK(s.fuel == -3);  // -3 - 5 floors at 0, then the unfloored -3 again
    CHECK(r.cloaks.size() == 2);
}

TEST_CASE("cloak: seg11:0x4c72 on and off, the 150-frame wait of a computer ship [BYTES seg11:0x4c72-0x4d16]") {
    ShipRig r;
    ShipRecord& s = r.w.ship(1);
    // No cloak: nothing.
    SystemsCtx c = r.ctx(1000);
    CHECK(cloak_toggle(c, 1) == 0);
    s.be7 = 1;
    set_ship_dword(s, 0x152, 900);
    // A computer ship within 150 frames of its last toggle (unsigned 32-bit) waits.
    c.frames = 1049;
    CHECK(cloak_toggle(c, 1) == 0);
    // On: system 3 above 40, fuel at least 120.
    c.frames = 1050;
    CHECK(cloak_toggle(c, 1) == 1);
    CHECK(s.be8 == 1);
    CHECK(s.w2e == 2);
    CHECK(ship_dword(s, 0x152) == 1050u);
    CHECK(ship_dword(s, 0x14e) == 9000u);
    CHECK(ship_dword(s, 0x14a) == (1200u | (static_cast<std::uint32_t>(static_cast<std::uint16_t>(-300)) << 16)));
    CHECK(r.h.vanish == std::vector<std::tuple<int, int, int>>{{0, kPoolShips, 1}, {1, kPoolShips, 1}});
    // Off too early, then at 150 frames.
    c.frames = 1199;
    CHECK(cloak_toggle(c, 1) == 0);
    c.frames = 1200;
    CHECK(cloak_toggle(c, 1) == 1);
    CHECK(s.be8 == 0);
    CHECK(std::get<0>(r.h.vanish.back()) == 2);
    // Byte +0xeb (an effect running) blocks; a computer ship needs system 3 above 40 to cloak.
    s.beb = 1;
    c.frames = 5000;
    CHECK(cloak_toggle(c, 1) == 0);
    s.beb = 0;
    s.w2c = 40;
    CHECK(cloak_toggle(c, 1) == 0);
    // The player in action mode skips the wait and the 40; system 3 must not be 0; fuel >= 120.
    ShipRecord& p = r.w.ship(0);
    p.be7 = 1;
    p.w2c = 1;
    set_ship_dword(p, 0x152, 4999);
    c.frames = 5000;
    CHECK(cloak_toggle(c, 0) == 1);
    CHECK(cloak_toggle(c, 0) == 1);  // and straight back off
    p.fuel = 119;
    CHECK(cloak_toggle(c, 0) == 0);
    p.fuel = 120;
    p.w2c = 0;
    CHECK(cloak_toggle(c, 0) == 0);
}

TEST_CASE("shield: seg11:0x4b00 up and down, the animation word, SHIELD.WAV, the corrected wait [BYTES seg11:0x4b00-0x4c66]") {
    ShipRig r;
    ShipRecord& s = r.w.ship(1);
    set_ship_dword(s, 0x152, 0);
    s.thrusting = 1;
    // A computer ship within 150 frames waits (s3-fuel-verify.md claim 11, CORRECTED).
    SystemsCtx c = r.ctx(149);
    CHECK(shield_toggle(c, 1) == 0);
    c.frames = 150;
    CHECK(shield_toggle(c, 1) == 1);
    CHECK(s.bea == 1);
    CHECK(s.type == 1);
    CHECK(s.w20 == 33 - 12);
    CHECK(s.w22 == 7);
    CHECK(dg_9522() == 1);
    CHECK(r.h.renders == 1);
    CHECK(r.h.flames == std::vector<std::pair<int, int>>{{1, 1}});
    CHECK(r.h.sounds == std::vector<std::tuple<int, int, int, int>>{{5, kPoolShips, 1, 1}});
    // Mid-animation (+0x10 in 1 ... 6) nothing; at 7 (raised) it lowers.
    c.frames = 400;
    CHECK(shield_toggle(c, 1) == 0);
    s.type = 7;
    CHECK(shield_toggle(c, 1) == 1);
    CHECK(s.bea == 0);
    CHECK(s.type == 6);
    CHECK(s.w20 == 31);
    CHECK(s.w22 == 1);
    CHECK(dg_9522() == -1);
    // No raise below 50 of system 3, but a lowering is allowed.
    s.type = 0;
    s.w2c = 49;
    c.frames = 800;
    CHECK(shield_toggle(c, 1) == 0);
}

TEST_CASE("seg3:0x332e: the strategy line's text and words [BYTES seg3:0x332e-0x33b1]") {
    ShipRig r;
    SystemsCtx c = r.ctx(0, 7000);
    strategy_notice(c, 0x04090418u);
    CHECK(r.w.w8630 == "PILOT: Uncloaking Because I'm Low On Gas.");
    CHECK(r.w.w058c == 1);
    CHECK(r.w.w05c0 == 8200u);
    CHECK(r.h.line == r.w.w8630);
    strategy_notice(c, 0x04090417u);
    CHECK(r.w.w8630 == "PILOT: Uncloaking Due To System Damage.");
}

TEST_CASE("CapsLock: WM_KEYUP 0x14 toggles the shield, else the cloak, in action mode [BYTES seg11:0x6d19 seg11:0x6dd6-0x6e0a]") {
    CHECK(capslock_action(false, 1, 0, 1, 1) == CapsAction::kShield);
    CHECK(capslock_action(false, 1, 0, 0, 1) == CapsAction::kCloak);
    CHECK(capslock_action(false, 1, 0, 0, 0) == CapsAction::kNone);
    CHECK(capslock_action(false, 1, 1, 1, 1) == CapsAction::kNone);  // PlayMode != 0
    CHECK(capslock_action(false, 0, 0, 1, 1) == CapsAction::kNone);  // the demo
    CHECK(capslock_action(true, 1, 0, 1, 1) == CapsAction::kNone);   // the switch's gate
}
