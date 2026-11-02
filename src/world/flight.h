// The player's flight model (docs/re/world.md "Flight model", "Ships (seg13:0x0)", "Turning",
// "Go flag", "Thrust", "Brake", "Drift and drag", "Helpers", "Body thrust") and the integrator
// seg8:0x1b54 ("World coordinates"). Every product, sum, shift and compare the original makes
// in 16 bits is made in 16 bits here (ADR 0013 decision 7), through records.h's helpers.
//
// The polled keys come from the KeyLatch of input.h with the original's GetAsyncKeyState masks
// (ADR 0013 decision 6). Calls into routines other parts of the port own (the player's sprite,
// the flame's drawing tail, sounds, the computer pilots of M5) go through FlightHost, whose
// defaults do nothing; each names the original's address.
//
// Portable, no Win32.
#pragma once

#include <array>
#include <cstdint>

#include "shell/rng.h"
#include "world/records.h"

namespace is::world {

class KeyLatch;

// --- Virtual keys the wave polls (Win16 and Win32 share them) --------------------------------
constexpr std::uint16_t kVkShift = 0x10;
constexpr std::uint16_t kVkControl = 0x11;
constexpr std::uint16_t kVkMenu = 0x12;  // Alt
constexpr std::uint16_t kVkSpace = 0x20;
constexpr std::uint16_t kVkLeft = 0x25;
constexpr std::uint16_t kVkUp = 0x26;
constexpr std::uint16_t kVkRight = 0x27;
constexpr std::uint16_t kVkDown = 0x28;

// --- Tables (world.md "Heading table", "Helpers") ---------------------------------------------
// DGROUP:0x1050: 16 (dx, dy) word pairs [BYTES DGROUP:0x1050].
constexpr std::array<std::int16_t, 32> kHeadingTable = {
    0, -16, 6, -15, 11, -11, 15, -6, 16, 0, 15, 6, 11, 11, 6, 15,
    0, 16, -6, 16, -11, 11, -15, 6, -16, 0, -15, -6, -11, -11, -6, -15};
constexpr std::int16_t heading_dx(int h) { return kHeadingTable[static_cast<std::size_t>(h) * 2]; }
constexpr std::int16_t heading_dy(int h) {
    return kHeadingTable[static_cast<std::size_t>(h) * 2 + 1];
}
// DGROUP:0xf00 ... 0xf5a: 91 bytes for 0 ... 90 degrees [BYTES DGROUP:0xf00].
extern const std::array<std::uint8_t, 91> kSineTable;

// seg9:0x88ba: 16-bit abs (abs(-32768) stays -32768) [BYTES seg9:0x88ba].
constexpr std::int16_t abs16(std::int16_t v) { return v < 0 ? sub16(0, v) : v; }
// seg9:0x88dc: 32-bit abs [BYTES seg9:0x88dc].
constexpr std::int32_t abs32(std::int32_t v) {
    return v < 0 ? static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(v)) : v;
}
// seg6:0x71b2 sin128 and seg6:0x720c cos128 of a in degrees: r = (a + 360) % 360 (16-bit add,
// `idiv`), folded through kSineTable [BYTES seg6:0x71b2 seg6:0x720c]. A negative r reads bytes
// before the table in the original; the port returns 0 there [OPEN] (flight never passes one).
std::int16_t sin128(std::int16_t a);
std::int16_t cos128(std::int16_t a);
// seg8:0x35d0(a, b): one Newton step of sqrt(a*a + b*b), 1 when g = max + min/2 is 0; the low
// word of the 32-bit result [BYTES seg8:0x35e3-0x367b].
std::int16_t magnitude(std::int32_t a, std::int32_t b);
// seg8:0x19b0(num, den): the 16-sector direction of num * 100 / den [BYTES seg8:0x19bf-0x1b46].
std::int16_t direction16(std::int16_t num, std::int16_t den);

// --- The DGROUP words the flight model and the update loop read and write ---------------------
// Names follow world.md and globals.md where they give one; the others are named by address.
// [DESIGN] Until the wave's own globals (world.h) carry them, this struct is their home; the
// hooks keep one instance per wave.
struct BurstEntry {   // DGROUP:0x5b2c + 0x16 k, 40 entries; only the words the OverThruster
    std::int16_t w0 = 0;  // resets are kept [OPEN] (world.md Open questions "Thrust table")
    std::int16_t w2 = 0;
    std::int16_t w8 = 0;
};

struct WaveGlobals {
    std::uint32_t now = 0;           // DGROUP:0x1e50, the pass's GetCurrentTime
    std::int16_t player = 0;         // DGROUP:0x9646
    std::int16_t ship_pool = kPoolShips;  // DGROUP:0x9324
    std::int16_t rock_pool = kPoolRocks;  // DGROUP:0x934a
    std::int16_t pool5 = kPool5;     // DGROUP:0x9328
    std::int16_t last_ship = 0;      // DGROUP:0x9316
    std::int16_t gates = 1;          // DGROUP:0x9318
    std::int16_t play_mode = 0;      // DGROUP:0x9134 `PlayMode`: 0 = action mode
    std::int16_t demo = 1;           // DGROUP:0x95bc: 0 in the demo
    std::int16_t wave_kind = 0;      // DGROUP:0x9320
    std::uint8_t dg_9eb4 = 0;        // DGROUP:0x9eb4 (race)
    std::uint8_t dg_9eb5 = 0;        // DGROUP:0x9eb5 (race)
    std::int16_t dg_1034 = 1;        // DGROUP:0x1034, holds 1, never written [BYTES DGROUP:0x1034]
    std::int16_t cycle = 0;          // DGROUP:0x95f6, the 0 ... 8 cycle
    std::int16_t dg_134e = 0;        // DGROUP:0x134e, a 0 ... 100 cycle of seg13:0x0 [OPEN]
    std::uint32_t dg_1350 = 0;       // DGROUP:0x1350, seg13:0x0's 9000 ms deadline [OPEN]
    std::int16_t dg_9130 = 0;        // DGROUP:0x9130 seg18 mode
    std::int16_t dg_9006 = 7;        // DGROUP:0x9006 (timing.md "Calibrate": 7 ... 10)
    std::int16_t autopilot = 0;      // DGROUP:0x951a [OPEN]
    std::int16_t dg_952e = 0;        // DGROUP:0x952e [OPEN]
    std::int16_t dg_9e49 = 0;        // DGROUP:0x9e49 [OPEN]
    std::int16_t dg_9e5b = 0;        // DGROUP:0x9e5b [OPEN]
    std::int16_t dg_913a = 0;        // DGROUP:0x913a [OPEN]
    std::int16_t dg_913c = 0;        // DGROUP:0x913c [OPEN]
    std::int16_t dg_952a = 0;        // DGROUP:0x952a, the OverThruster request
    std::int16_t turned = 0;         // DGROUP:0x9138, the turn follow-up flag
    std::int16_t dg_238c = 0;        // DGROUP:0x238c screen shake
    std::int16_t dg_238e = 0;        // DGROUP:0x238e
    std::int16_t braking = 0;        // DGROUP:0x8ff6 INI `Braking`
    std::int16_t detail = 3;         // DGROUP:0x9208 INI `Detail`
    std::int16_t size = 1;           // DGROUP:0x9156 INI `Size`
    std::int16_t demons_gate = 0;    // DGROUP:0x92e8
    std::int16_t dg_94e2 = 0;        // DGROUP:0x94e2 [OPEN] (computer ships' speed cap)
    std::int16_t joystick = 0;       // DGROUP:0x8ff4 INI `JoyStick`
    std::int16_t joy_overthrust = 0; // DGROUP:0x103e
    std::int16_t joy_thrust = 0;     // DGROUP:0x1040
    std::int16_t joy_fire = 0;       // DGROUP:0x1042
    std::int16_t joy_left = 0;       // DGROUP:0x1044
    std::int16_t joy_right = 0;      // DGROUP:0x1046
    std::uint32_t race_timer = 0;    // DGROUP:0x1c3e
    std::int16_t burst_count = 50;   // DGROUP:0x9510, the particle burst's step
    std::int16_t burst_pool = 0;     // DGROUP:0x9512
    std::int16_t burst_rec = 0;      // DGROUP:0x9514
    std::array<BurstEntry, 40> burst{};  // DGROUP:0x5b2c
    std::array<std::int16_t, 8> body_anim{};  // DGROUP:0x1c72 + 2i, the body-thrust counter
    std::uint32_t game_frames = 0;   // DGROUP:0x1e68, wave frames (the HUD clock)
    std::uint32_t frames = 0;        // DGROUP:0x5a8, wave frames
};

// --- Calls out of the flight model -----------------------------------------------------------
class FlightHost {
public:
    virtual ~FlightHost() = default;
    // seg7:0x1208, the player's sprite.
    virtual void player_sprite() {}
    // seg11:0xb105 to the end of seg11:0xb09c: the paste of strip row 2 ship + on into the ship's
    // sheet after +0x2f (world/player_sheet.h flame_pastes, paste_flame) [BYTES seg11:0xb105-0xb302].
    virtual void flame_tail(int on, int ship) { (void)on, (void)ship; }
    // seg10:0x754a(id, pool, rec, arg): a sound (M7's engine; not wired in this stage).
    virtual void sound(int id, int pool, int rec, int arg) { (void)id, (void)pool, (void)rec, (void)arg; }
    // The design record word +0x18 (DGROUP:0x970a + 0xad n) [OPEN].
    virtual std::int16_t design_kind(int design) { return (void)design, 0; }
    // seg13:0x59e8(ship, 0x0e) [OPEN].
    virtual bool seg13_59e8(int ship) { return (void)ship, false; }
    // Command mode's "drifting" line (seg11:0x6550-0x65a2) [OPEN].
    virtual void command_mode_line() {}
    // The computer pilots and the special paths of seg13:0x0, M5 [OPEN]; each gets the ship.
    virtual void seg13_3748(int ship) { (void)ship; }   // the move of ship DGROUP:0x952e
    virtual void seg13_3b88() {}                        // the alternate player move
    virtual void seg12_274a(int pool, int ship) { (void)pool, (void)ship; }
    virtual void seg4_277a() {}
    virtual void seg4_144c(int ship, int arg) { (void)ship, (void)arg; }
    virtual void seg4_bde(int ship) { (void)ship; }
    virtual void seg4_78(int ship) { (void)ship; }
    virtual void seg13_tail(int ship) { (void)ship; }   // seg13:0x6e5-0xcf9: the rest of the ship step
    virtual void seg11_ai(int ship) { (void)ship; }     // seg11:0x5cd7-0x5f11, 0x62fc-0x63a8
    virtual void seg13_head_tail(int ship) { (void)ship; }  // seg13:0x3e3-0x533 [OPEN]
    // seg8:0x1e32(rec, pool): an owned object follows its owner [OPEN].
    virtual void follow_owner(int pool, int rec) { (void)pool, (void)rec; }
    // The signed byte +0x33 of roster record n (far DGROUP:0x6e24 + 0x7c n) [BYTES seg11:0x6644].
    virtual std::int16_t roster_byte_33(int roster) { return (void)roster, 0; }
    // seg8:0xb76(pool * 256 + rec, kind, DGROUP:0x39e8[slot], masked): the sprite routine.
    virtual void draw_sprite(std::int16_t pool_rec, int kind, int slot, int masked) {
        (void)pool_rec, (void)kind, (void)slot, (void)masked;
    }
    // seg13:0x6e5-0xa57 (Detail > 1: the ships' distance work, seg11:0x947a) [OPEN] M5.
    virtual void seg13_6e5(int ship) { (void)ship; }
    // seg13:0xb5a-0xcf9: an on-screen ship's collisions, touches and weapon tail (update.h).
    virtual void ship_collide(int ship) { (void)ship; }
    // seg7:0x5b68(pool, rec, arg): a smoke or fragment puff [OPEN] M4.
    virtual void puff(int pool, int rec, int arg) { (void)pool, (void)rec, (void)arg; }
    // seg11:0x4c72(ship), seg11:0x4b00(ship) (damage effects, M4) and seg3:0x332e(msg) [OPEN].
    virtual std::int16_t seg11_4c72(int ship) { return (void)ship, 0; }
    virtual std::int16_t seg11_4b00(int ship) { return (void)ship, 0; }
    virtual void seg3_332e(std::uint32_t arg) { (void)arg; }
    virtual void seg4_6e70(int ship) { (void)ship; }
    virtual void seg11_4108() {}
    virtual void race_step(int ship) { (void)ship; }   // seg13:0x545-0x614 (kind-6 waves) M6
    virtual void race_thrust(int ship) { (void)ship; } // seg11:0x674d-0x6846 (kind-6 waves) M6
    virtual void seg13_675e() {}                       // seg13:0x675e at the head of seg13:0x0
};

// Everything the flight routines touch.
struct FlightCtx {
    Pools& pools;
    ShipTable& ships;
    WaveGlobals& g;
    KeyLatch& keys;
    FlightHost& host;
    shell::Rng& rng;

    ObjectRecord& ship_obj(int i) { return pools.rec(g.ship_pool, i); }
    // "The player in action mode": DGROUP:0x9134 = 0 and the ship is DGROUP:0x9646.
    bool action_player(int i) const { return g.play_mode == 0 && i == g.player; }
};

// --- The routines -----------------------------------------------------------------------------
// seg8:0x1b54(steps, pool * 256 + rec): moves by v * steps (16-bit) and wraps at 25600.
void integrate(FlightCtx& c, std::int16_t steps, std::int16_t pool_rec);
// seg8:0x1d54(limit, pool * 256 + rec): when |vx| + |vy| >= limit, each non-zero axis steps one
// unit toward 0 [BYTES seg8:0x1d54-0x1e2e].
void limit_speed(Pools& pools, std::int16_t limit, std::int16_t pool_rec);
// seg13:0x6716(ship): the player's sprite, or ship +0x156 = now - 1 [BYTES seg13:0x6716].
void ship_touched(FlightCtx& c, int ship);
// seg11:0xb09c(on, ship): +0x2f = ship +0x0c / 20 or 0, then the tail [BYTES seg11:0xb0db-0xb100].
void flame(FlightCtx& c, int on, int ship);
// seg4:0x0(ship): the Alt spin [BYTES seg4:0x0-0x74].
void spin(FlightCtx& c, int ship);
// seg11:0x592a: the player's turn keys [BYTES seg11:0x592a-0x5ab4].
void turn_input(FlightCtx& c);
// seg11:0x5ab6(mode, ship): go flag, brake, thrust, drift and drag [BYTES seg11:0x5ab6-0x63b0].
void thrust_step(FlightCtx& c, std::int16_t mode, int ship);
// seg11:0x63b4(mode, ship): the thrust [BYTES seg11:0x63b4-0x6871].
void thrust(FlightCtx& c, std::int16_t mode, int ship);
// seg11:0x6874(ship): the brake [BYTES seg11:0x6874-0x6aa6].
void brake(FlightCtx& c, int ship);
// The go flag of seg11:0x5ab6 for its inputs, separated for tests [BYTES seg11:0x5b30-0x5c1c].
bool go_flag(const ShipRecord& s, const WaveGlobals& g, std::int16_t mode, bool action_player);
// The thrust force T of seg11:0x63b4 (steps 1, 3, 4; the roster scaling is passed in as
// `roster_byte`, -128 for none) [BYTES seg11:0x6630-0x66a9].
std::int16_t thrust_force(const ShipRecord& s, std::int16_t roster_byte);
// The velocity change of one thrust (seg11:0x66b3-0x6713), all 16-bit.
struct Dv {
    std::int16_t dx = 0, dy = 0;
};
Dv thrust_dv(std::int16_t heading, std::int16_t t, std::int16_t k, std::int16_t drag,
             std::int16_t mass, std::int16_t vx, std::int16_t vy);
// The Detail >= 3 drag of seg11:0x6117-0x6282.
Dv drift_dv(std::int16_t heading, std::int16_t drag, std::int16_t mass, std::int16_t vx,
            std::int16_t vy, bool autopilot);
// seg13:0x0: the ship mover, once per wave frame [BYTES seg13:0x0-0xd67]. A dead ship
// (`alive` +0xec = 0) with `tumbling` +0xe2 set gets the drift (seg8:0x1b54), object +0x26 = 1
// and a heading step through seg13:0x100c every second frame (ship word +0x16 as the counter),
// then skips the input and flight block (seg13:0x224) to the smoke, the sprite draw and the
// collisions; once the lock-out +0x84 passes it explodes through seg12:0x274a
// (FlightHost::seg12_274a) (s1-death.md section 3, verified claims 30-32) [BYTES seg13:0xd7-0x189
// seg13:0x68d seg13:0xa57].
void move_ships(FlightCtx& c);

// seg13:0x100c(h, ship) [BYTES seg13:0x100c-0x1074] (verified claim 31): h + object word +0x26,
// >= 16 -> 0 and <= 0 -> 15 (the test on the sum: a step of -1 from 1 gives 15), when ship byte
// +0xeb = 0 and ship word +0x10 (`type`) is < 1 or > 6; else h unchanged.
std::int16_t turn_step(const ShipRecord& s, const ObjectRecord& o, std::int16_t h);

// The control lock-out +0x84 (`cmp dword, now; jb`): passed when it lies before now, unsigned.
// seg16:0x8e4 sets now + 6000 at death; the tumble ends, the turn keys and the fire poll act
// only once it has passed [BYTES seg13:0x14b seg11:0x5938 seg11:0x4dba seg16:0xa2d]. The
// collision tumble of the damage track sets it through set_lockout.
inline bool lockout_passed(const ShipRecord& s, std::uint32_t now) { return s.lockout.get() < now; }
inline void set_lockout(ShipRecord& s, std::uint32_t until) { s.lockout.set(until); }

// --- The flight values of the ship set-up (world.md "Mass, thrust and drag", "Turn word") ----
// The design record's words (DGROUP:0x96f2 + 0xad n) that flight reads, from the ship's MASTER.SHP
// and MASTER.BOD records. The rest of the design and the ship set-up seg14 is the set-up's
// (M5) [OPEN]; these are the parts a flying player needs.
struct FlightDesign {
    std::int16_t thrust = 0;     // DGROUP:0x9710: lo16(5 * signed SHP byte +1) [BYTES seg14:0x1c8c-0x1c94]
    std::int16_t drag = 0;       // DGROUP:0x9712: signed body byte +0x1c [BYTES seg14:0x1ca1-0x1caa]
    std::int16_t turn_base = 0;  // DGROUP:0x96f2: body byte +0x1d (world.md "Turn word") [OPEN: its writer]
    std::int16_t mass = 0;       // DGROUP:0x9704 [BYTES seg14:0x1e35-0x1e8f]
    std::uint8_t has_flame = 1;  // DGROUP:0x9718: 0 for a Body Thrust body, 1 otherwise [BYTES seg14:0x1f41-0x1f5a]
    std::uint8_t cloak = 0;   // DGROUP:0x971c: SHP +9 bit 0x08 [BYTES seg14:0x1fd6 seg14:0x1fe2 seg14:0x1fef]
    std::uint8_t shield = 0;  // DGROUP:0x9779: BOD +0x20 bit 0x02 [BYTES seg14:0x1f9f seg14:0x1fab seg14:0x1fc8]
};
// SHP bytes +1 (Thruster) and +2 (Armor), body bytes +0x1b, +0x1c, +0x1d and the flags +0x20,
// and the SHP flags +9.
FlightDesign flight_design(std::uint8_t shp_thruster, std::uint8_t shp_armor,
                           std::int8_t body_strength, std::int8_t body_1c, std::int8_t body_1d,
                           std::uint8_t body_flags, std::uint8_t shp_flags = 0);
// seg14:0x21a0-0x22f2's flight fields of ship record `s` and the object's mass (seg14:0x20bc):
// +0xec = 1, +0x0c, +0x0e, +0x36 = 0, +0x1c = 1, the lock-out +0x84 = now, +0xe4, the animation
// step 1 and frame 0, +0xe7 (cloak) and +0xe9 (shield) [BYTES seg14:0x21ea-0x21f6], the turn word (3/4 of it for the player while DGROUP:0x95bc != 0).
// `roster_byte` is the signed roster byte +0x33 when +0xd6 != -1.
void apply_flight_design(ShipRecord& s, ObjectRecord& o, const FlightDesign& d,
                         bool player_game_player, std::uint32_t now, std::int16_t roster_byte);

}  // namespace is::world
