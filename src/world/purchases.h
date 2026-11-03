// owner: M4 stage 3, track VISIT.
// The purchases of the ambulance (docs/re/core.md "The ambulance", "Credit and resources";
// work/analysis/m4/s3-visit.md section 4 with the verdicts of s3-visit-verify.md): the shop's
// DGROUP words (the to-do flags DGROUP:0x953e ... 0x957f and the totals DGROUP:0x9e55 ...
// 0x9e5d), the purchase-applier seg16:0x3784(ship, mode) with every helper (seg16:0x3468 the
// repair, seg16:0x3730 the reload, seg16:0x3e80 the refuel, seg16:0x3ece DOUBLE GUNS,
// seg16:0x3f88 UNIFY FORCES, seg16:0x4030 a powerup) and the computer ships' shop seg16:0x3b18.
// Portable, no Win32.
//
// The original keeps these words in DGROUP; the port keeps them in one Shop (shop(), one per
// program as DGROUP is) beside the World, because world.h is another track's file [DESIGN].
// The calls out (the design words the World does not hold yet, the roster bytes, seg12:0x6d64,
// seg13:0x33d6, seg14:0x11a8, the INI) go through PurchaseHost; WorldPurchaseHost answers them
// from the World where it can and logs a TODO where it cannot.
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>

#include "world/records.h"

namespace is::world {

class World;

// ---- the shop's DGROUP words ----------------------------------------------------------------
// The layout follows DGROUP: 0x9542 + w (w = 1 ... 26) ends at 0x955c, the six powerup bytes
// 0x955d ... 0x9562, the new-body byte 0x9563, the sold bytes 0x9564 + w end at 0x957e, then
// UNIFY 0x957f [BYTES seg16:0x5b24 seg16:0x3784].
constexpr int kShopWeapons = 26;   // w = 1 ... 26 (0x1a) [BYTES seg16:0x39f9 seg16:0x5b24]
constexpr int kPowerups = 6;       // k = 0 ... 5 [BYTES seg16:0x3784 seg16:0x4bd8]
struct Shop {
    std::uint8_t basic = 0;        // DGROUP:0x953e BASIC REPAIRS
    std::uint8_t wing = 0;         // DGROUP:0x953f the wing (shield, cloak) repair
    std::uint8_t refuel = 0;       // DGROUP:0x9540 REFUEL
    std::uint8_t double_guns = 0;  // DGROUP:0x9541 DOUBLE GUNS
    std::array<std::uint8_t, kShopWeapons + 1> bought{};  // DGROUP:0x9542 + w, w = 1 ... 26
    std::array<std::uint8_t, kPowerups> powerup{};        // DGROUP:0x955d + k: a count, read by
                                                          // seg16:0x3784's repeat loop
    std::uint8_t new_body = 0;     // DGROUP:0x9563 NEW SHIP BODY
    std::array<std::uint8_t, kShopWeapons + 1> sold{};    // DGROUP:0x9564 + w
    std::uint8_t unify = 0;        // DGROUP:0x957f UNIFY FORCES

    std::int16_t cost = 0;         // DGROUP:0x9e55 the bill (TOTAL COST OF WORK)
    std::int32_t credit = 0;       // DGROUP:0x9e57 (dword) the shopping credit, "Remaining"
    // DGROUP:0x9e5b, the served ship's `alive` at the pickup (seg16:0x441; the AMBFLIGHT track's
    // writer), and DGROUP:0x9e5d, the Sell total. seg16:0x3784's dword write sets both
    // (1, 0) [BYTES seg16:0x3b07] (verified claim 82).
    std::int16_t alive = 1;        // DGROUP:0x9e5b
    std::int16_t sold_total = 0;   // DGROUP:0x9e5d
    std::int16_t weapons = 0;      // DGROUP:0x958e the weapon count the 10-weapon test reads
    // DGROUP:0x9532, the Options dialog's window while it is open (also 1 around the dead ship's
    // chooser, seg16:0x1365), and DGROUP:0x959c, the Sell dialog's: the visit returns at once
    // while 0x9532 != 0 (verified claim 1). The port keeps the HWND as an integer [DESIGN].
    std::uintptr_t dlg = 0;        // DGROUP:0x9532
    std::uintptr_t sell_dlg = 0;   // DGROUP:0x959c

    // The to-do flags: bytes 0x953e ... 0x9541, 0x9542 + w, 0x9564 + w, 0x955d + k, 0x9563 and
    // 0x957f, as seg16:0x5b24 and seg16:0x3784's end clear them [BYTES seg16:0x5b24
    // seg16:0x3a99-0x3b02].
    void clear_flags();
    bool any_repair() const { return basic != 0 || wing != 0 || new_body != 0; }
};
// The program's Shop (DGROUP), zero-initialised as BSS is, alive = 1 [DESIGN: the pickup's
// writer sets it before every visit].
Shop& shop();

// ---- the ship words the shop reads, by the original's offsets --------------------------------
std::int32_t credit_of(const ShipRecord& s);              // dword +0xdc [BYTES seg16:0x11f8]
void set_credit(ShipRecord& s, std::int32_t v);
// The 16-bit result of a helper subtracted from the dword: `cdq; sub; sbb` [BYTES seg16:0x37f8].
void charge(ShipRecord& s, std::int16_t cost);
std::int16_t& system_word(ShipRecord& s, int k);          // +0x26 + 2k [BYTES seg16:0x34f1]

// ---- the calls out -----------------------------------------------------------------------------
class PurchaseHost {
public:
    virtual ~PurchaseHost() = default;
    // The design word DGROUP:0x96d2 + 0xad·n + off: +0x1e the tank DGROUP:0x96f0, +0x20 D
    // DGROUP:0x96f2, +0x22 + 2k lamp row k DGROUP:0x96f4, +0x2a DGROUP:0x96fc, +0x32 the mass
    // DGROUP:0x9704.
    virtual std::int16_t design_word(int design, int off) = 0;
    // The design name DGROUP:0x9742 + 0xad·n (design +0x70).
    virtual std::string design_name(int design) = 0;
    // A byte of roster record `index` (far DGROUP:0x6e24 + 0x7c·index + off); index -1 reads and
    // writes the bytes before the base in the original [OPEN].
    virtual int roster_byte(int index, int off) = 0;
    virtual void set_roster_byte(int index, int off, int v) = 0;
    virtual void load_weapon(int ship, int w) = 0;   // seg12:0x6d64(ship, w)
    virtual void rate(int ship) = 0;                 // seg13:0x33d6(ship)
    virtual void build_sheet(int ship) = 0;          // seg14:0x11a8(ship)
    virtual bool sheet_dc(int ship) = 0;             // DGROUP:0x3a38 + 2·ship != 0
    virtual std::int16_t sheet_dc_word(int ship) = 0;  // the word itself, for object +0x3a
    virtual void designs_begin() = 0;                // seg14:0x3eda
    virtual void designs_end() = 0;                  // seg14:0x4074
    // GetPrivateProfileString("Inner Space", "MainShip", "EAGLE", buf, 20, "INSPACE.INI").
    virtual std::string ini_main_ship() = 0;
    // The first of the DGROUP:0x95aa body records (0x41 bytes at far DGROUP:0x6e20, name at
    // +0x0a) named `name`, else DGROUP:0x95aa [BYTES seg16:0x3595-0x35c4].
    virtual int body_index(const std::string& name) = 0;
    virtual void set_body(int ship, int index) = 0;  // seg16:0x5f78(ship, index)
    // PostMessage(DGROUP:0xa7e, WM_PAINT) and PostMessage(DGROUP:0xa7c, WM_PAINT), each when
    // its window exists [BYTES seg16:0x3a90-0x3abb].
    virtual void post_info_paint() = 0;
    virtual int game_state() = 0;                    // DGROUP:0x90f4
    virtual int rand() = 0;                          // seg5:0xdc
    virtual void todo(const std::string& what, const char* address) = 0;
};

// The default host on the World: the design words the World keeps (World::design_ratings:
// the tank and lamp row 3; rows 0 ... 2 are 0, 1, 2 for every design [BYTES seg14:0x1d3b
// seg14:0x1d44]; +0x2a as collide.h design_from_world reads it [OPEN]; the mass through
// on_design_mass), the roster bytes the RosterRecord keeps, and TODOs for the rest. The calls it
// cannot make log once through `todo_log`.
class WorldPurchaseHost : public PurchaseHost {
public:
    explicit WorldPurchaseHost(World& w) : w_(w) {}
    std::int16_t design_word(int design, int off) override;
    std::string design_name(int design) override;
    int roster_byte(int index, int off) override;
    void set_roster_byte(int index, int off, int v) override;
    void load_weapon(int ship, int w) override;
    void rate(int ship) override;
    void build_sheet(int ship) override;
    bool sheet_dc(int ship) override;
    std::int16_t sheet_dc_word(int ship) override;
    void designs_begin() override;
    void designs_end() override;
    std::string ini_main_ship() override;
    int body_index(const std::string& name) override;
    void set_body(int ship, int index) override;
    void post_info_paint() override;
    int game_state() override;
    int rand() override;
    void todo(const std::string& what, const char* address) override;

    // The slots the program fills (src/hud/hooks.cpp); an empty slot keeps the TODO.
    std::function<void(int, int)> on_load_weapon;
    std::function<void(int)> on_build_sheet;
    std::function<bool(int)> on_sheet_dc;
    std::function<std::string()> on_ini_main_ship;
    std::function<void()> on_post_info_paint;
    std::function<int()> on_game_state;
    std::function<std::string(int)> on_design_name;
    // DGROUP:0x9704 + 0xad·design, the program's world::design_mass (world/hooks.h, whose object
    // the tests do not link) [DESIGN].
    std::function<std::int16_t(int)> on_design_mass;
    std::function<void(const std::string&, const char*)> todo_log;

protected:
    World& w_;
};

struct PurchaseCtx {
    World& w;
    Shop& s;
    const WeaponWords& W;  // DGROUP:0x2f54 (missile_state().words in the program)
    PurchaseHost& h;
    bool player_game = true;   // DGROUP:0x95bc
    std::int16_t restored = 0; // DGROUP:0x92e4
    std::int16_t plus = 0;     // DGROUP:0x11aa
};

// ---- the helpers -------------------------------------------------------------------------------
// seg16:0x3468(ship, k): repair of system k; returns ((100 - old)/10)·10 (16-bit idiv) or 0.
std::int16_t repair_system(PurchaseCtx& c, int ship, int k);
// seg16:0x3730(ship, slot): the reload; 0 when the slot's ammo byte is 0xff.
std::int16_t reload_slot(PurchaseCtx& c, int ship, int slot);
// seg16:0x3e80(ship): the refuel to the ship's own tank; returns ((tank - fuel)/70)·10.
std::int16_t refuel_ship(PurchaseCtx& c, int ship);
// seg16:0x3ece(ship): DOUBLE GUNS; 250, or 0 when word +0x0a is already 2.
std::int16_t double_guns(PurchaseCtx& c, int ship);
// seg16:0x3f88(ship): UNIFY FORCES on the PLAYER, whatever ship is served; 250.
std::int16_t unify_forces(PurchaseCtx& c, int ship);
// seg16:0x4030(ship, k): powerup k; the listed price, (b + 3)·100 or (b + 1)·100.
std::int16_t powerup(PurchaseCtx& c, int ship, int k);
// seg16:0x3b18(ship, mode): the computer ships' shop (sets the to-do flags and the bill).
void computer_shop(PurchaseCtx& c, int ship, int mode);
// The slot shift of a removed weapon: slot j and the later slots move down one, slot 9
// (+0xc1, +0xb7) is zeroed [BYTES seg16:0x391c-0x3974 seg16:0x3f9c-0x3fca].
void remove_slot(ShipRecord& s, int j);

// seg16:0x3784(ship, mode): applies the to-do flags to ship `ship`. mode 0 is the visit's
// (seg16:0x872), mode 1 the wave set-up's (seg14:0xd32) [BYTES seg16:0x3784-0x3b17].
void apply_purchases(PurchaseCtx& c, int ship, int mode);

// seg14:0xd32's call seg16:0x3784(i, 1) on the program's Shop: the wave-1 (and fresh-ship and
// restore) repair and refuel pass, for the ROSTER track's port of seg14:0x0 [BYTES seg14:0xd16
// seg14:0xd32]. `h` gives the calls out; the program passes its WorldPurchaseHost.
void wave_setup_purchases(World& w, PurchaseHost& h, const WeaponWords& W, int ship,
                          bool player_game, std::int16_t restored, std::int16_t plus);

}  // namespace is::world
