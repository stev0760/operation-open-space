// The tethers: GRAPPLING HOOK (weapon 2) and HANDCUFFS (weapon 15) (work/analysis/m4/s7-tether.md
// with s7-tether-verify.md, whose CORRECTED claims 19 and 49 and Omissions A-Q complete it;
// core.md "Hit dispatcher" rule 5; world.md "Link lines"). M4 s7 TETHER.
//
//   tether_use     seg11:0x947a(P, R, w): fire a hook from object (P, R), or release what it holds
//   hook_meets     seg12:0x3498(Tp, Tr, k): hook k meets object (Tp, Tr): attach, steal or miss
//   move_hooks     seg12:0x3148: the pool-10 mover, the hooks in flight with their ropes
//   tether_follow  seg8:0x1e32(pool, rec): a held object, each time its mover integrates it
//   tether_throw   seg13:0x6e5-0xa57: a ship lets go of a swung object heading at a heavy one
//
// The link table DGROUP:0x65cc (World::links, world.h LinkSlot) pairs slot k with pool-10 record
// k; DGROUP:0x9300 = 3 slots [BYTES seg9:0x2508] (ruling R3). The ends: A (+0x00/+0x02) is 10
// with the hook record while it flies, then the held object; B (+0x04/+0x06) is the holder. The
// held object T keeps its holder in +0x32/+0x34 (`owner_pool`/`owner_rec`) and its slot in byte
// +0x30; the holder O keeps what it holds in +0x36/+0x38 (10 and k while the hook flies) and its
// own slot in byte +0x31 [BYTES seg11:0x95f2 seg12:0x36ec seg11:0x9a55 seg12:0x3703].
//
// Portable, no Win32: the GDI calls go through the Blitter (sprite.h pens and lines, line.h
// seg12:0x7256) on the window DC DGROUP:0x13dc (kDcWindow).
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <utility>

#include "world/draw.h"
#include "world/flight.h"
#include "world/player_sheet.h"
#include "world/sprite.h"
#include "world/world.h"

namespace is::world {

struct UpdateCtx;
struct VisibilityGlobals;

constexpr int kWeaponHook = 2;     // GRAPPLING HOOK [BYTES seg11:0xad6b]
constexpr int kWeaponCuffs = 0xf;  // HANDCUFFS [BYTES seg11:0xad7e]
constexpr int kPoolHooks = 10;     // the hook records DGROUP:0x1e20 [BYTES seg11:0x954b]
constexpr int kHookImageSlot = 3;  // DGROUP:0x39ee = DGROUP:0x39e8 + 2·3 [BYTES seg12:0x33ff]

// The two bytes of ObjectRecord::w30: +0x30, the slot of the tether holding the object
// [BYTES seg12:0x36ec seg8:0x1e60], and +0x31, the slot of the object's own tether
// [BYTES seg11:0x95f2 seg11:0x9cd5]; both read signed (`cbw`).
inline std::int8_t held_slot(const ObjectRecord& o) {
    return static_cast<std::int8_t>(static_cast<std::uint16_t>(o.w30) & 0xff);
}
inline std::int8_t own_slot(const ObjectRecord& o) {
    return static_cast<std::int8_t>(static_cast<std::uint16_t>(o.w30) >> 8);
}
inline void set_held_slot(ObjectRecord& o, int k) {
    o.w30 = static_cast<std::int16_t>((static_cast<std::uint16_t>(o.w30) & 0xff00u) |
                                      (static_cast<unsigned>(k) & 0xffu));
}
inline void set_own_slot(ObjectRecord& o, int k) {
    o.w30 = static_cast<std::int16_t>((static_cast<std::uint16_t>(o.w30) & 0x00ffu) |
                                      ((static_cast<unsigned>(k) & 0xffu) << 8));
}

// The DGROUP words of the tethers that outlive a wave: zero at program start, never reset
// (not at a wave start) [BYTES seg8:0x1e41-0x1e4c seg8:0x238d seg8:0x2393].
struct TetherState {
    // DGROUP:0x915a: the attached rope's pen 0 ... 3, + 1 on every seg8:0x1e32 call and 0 above
    // 3 [BYTES seg8:0x1e41-0x1e4c]. The race mover seg12:0x3e98 advances it too (M6, Omission M).
    std::int16_t pen_phase = 0;
    // DGROUP:0x17bc / DGROUP:0x17be: the last HANDCUFFS impulses aT and aH, one pair for every
    // rope (Omission L) [BYTES seg8:0x2301 seg8:0x234c seg8:0x238d seg8:0x2393].
    std::int16_t impulse_t = 0, impulse_h = 0;
    // DGROUP:0x3b44 + 2n: the attached rope's four pens, CreatePen(PS_SOLID, 1, c) for c =
    // 0x000000ff, 0x007f007f, 0x007f00ff, 0x00ff00ff [BYTES seg10:0x4db2-0x4df3] (make_rope_pens).
    std::array<PenId, 4> pens{};
};
TetherState& tether_state();

// The four rope pens' COLORREFs in the order of DGROUP:0x3b44 ... 0x3b4a (verified claim 53).
constexpr std::array<std::uint32_t, 4> kRopePenColours = {0x000000ffu, 0x007f007fu, 0x007f00ffu,
                                                          0x00ff00ffu};
// seg10:0x4b2e's tail: DeleteObject of the old pens (seg10:0x4d87-0x4db0), then the four
// CreatePen calls [BYTES seg10:0x4d87-0x4df3]. The program calls it where it makes the wave
// brushes (rock_images.h WaveGraphics).
void make_rope_pens(Blitter& out, TetherState& st);

// seg10:0x561a: when DGROUP:0x9300 != 0 and image slot 3 is empty, the hook sheet
// seg10:0x6ee6(3, 180, "", 0, 0x63, 3, 0, 1, 0, 12, 12, 0, 0) through the weapon-sheet renderer
// (IGRAPHIC 4000/180, 12 x 12, four directions filled to 16) [BYTES seg10:0x5624-0x5650]. The
// callers are the load seg9:0x2722 and the `Size` rebuild seg10:0x9641. True when it filled the
// slot.
bool load_hook_sheet(Blitter& out, ImageSlotTable& images, const Pools& pools, int size_level,
                     const SheetRenderer& render);

// The calls the tethers make outside the world's records. Every default draws or plays
// nothing; weapon_word, team, muzzle and aim read the program's tables.
class TetherHost {
public:
    virtual ~TetherHost() = default;
    // seg10:0x754a(id, pool, rec, mode).
    virtual void sound(int id, int pool, int rec, int mode) { (void)id, (void)pool, (void)rec, (void)mode; }
    // seg10:0x744c(id), the sound command (0x69: the reset of the priority, no sound).
    virtual void command(int id) { (void)id; }
    // seg8:0x29ea: a world point to the screen.
    virtual Point16 to_screen(std::int16_t x, std::int16_t y) { return (void)x, (void)y, Point16{}; }
    // The Blitter of the window DC DGROUP:0x13dc; null draws nothing.
    virtual Blitter* blitter() { return nullptr; }
    // seg7:0x1208, the player's sprite.
    virtual void player_sprite() {}
    // W(w)+off, the weapon record DGROUP:0x2f54 + 0x50·w (collide.h weapon_word).
    virtual std::int16_t weapon_word(int w, int off);
    // The design word +0x38 (team, DGROUP:0x970a + 0xad·n) (collide.h design_from_world).
    virtual std::int16_t team(const World& w, int design);
    // The muzzle word DGROUP:0x7b60 + 4·(64n + f) + 2·xy (shots.h ShotState::muzzle).
    virtual std::int16_t muzzle(int n, int f, int xy);
    // seg8:0x348e(P, R, TP, TR, S), the aim (missile.h aim_at, ruling R1): false when it
    // stored nothing.
    virtual bool aim(World& w, int P, int R, int TP, int TR, std::int16_t S, int shift);
};

// What the routines touch. play_w / play_h are W and H, DGROUP:0x90ea and DGROUP:0x90ec.
struct TetherCtx {
    World& w;
    WaveGlobals& g;
    TetherHost& h;
    std::int16_t play_w;
    std::int16_t play_h;
    TetherState& st;
};

// seg11:0x947a(P, R, w), cdecl [BYTES seg11:0x947a-0x9da7] (s7-tether.md section 2, verified
// claims 1-24, Omissions A, C-F): object (P, R) uses weapon w (2 or 15).
//  - Nothing while O is off screen. For the ship pool DGROUP:0x9324: some slot 1 ... byte
//    +0xcb - 1 must hold weapon 2 or 15 (either), and W(w)+0x2e (25 or 20) is taken from `fuel`
//    whenever it does not exceed it, before the slot test is applied; more than `fuel` refuses.
//  - O holding something (+0x36 != -1): the release (no sound): a held ship gets the control
//    lock-out now + 800, object +0x26 = 1 and `tumbling`; the held object's `owner_pool` -1;
//    O +0x36 -1; slot +0x00/+0x04 -1; hook record `state` 1; the last line erased and the hook
//    record's square blacked; seg7:0x1208 for the player while DGROUP:0x951a = 0.
//  - Otherwise the fire: the first slot k < DGROUP:0x9300 whose record has `state` != 0 and
//    +0x00 = -1; SLING (0x1d) at O; the target: a team-7 ship with w = 15 takes ship words
//    +0x54/+0x56, every other user searches (pools DGROUP:0x9326 ... DGROUP:0x9324 but 2, 5,
//    12; < 300 cells; the score of claims 11-12 with the literal-14 multipliers); with no target
//    or in Demon's Gate the straight shot (7 x the heading + O's velocity), else the aimed shot
//    (seg8:0x348e at 160 for a ship target, 110 otherwise, the fallback of Omission E).
// Both end with seg7:0x2080(10, -1). The pool-8 hazard caller seg12:0x6dea (M5) and the race
// mover seg12:0x3e98 (M6) reach the same routine (Omission A).
void tether_use(TetherCtx& c, int P, int R, int w);

// seg12:0x3498(Tp, Tr, k), cdecl, 0 or 1 [BYTES seg12:0x3498-0x3777] (section 5, claims 31-37):
// 0 at once for the hook's own holder or another hook; else the in-flight line erased, record k
// `state` 1 and its square blacked; a ship holder's hook meeting a ship other than its goal, a
// `fixed` object outside pool 4 and Tp = -1 (the timeout) miss (holder +0x36 -1, slot pools -1,
// seg7:0x2080(10, -1), silent); a pool-6 target's age / 4 first; else CLUNK (6) at the hook, T
// unfixed, a previous tether of T freed (the steal), T held by the slot's holder. +0x08 = 0.
// Callers: move_hooks (the contact and the timeout) and the hit dispatcher seg8:0x641d for an
// attacker in pool 10 (Omission B, collide.cpp).
int hook_meets(TetherCtx& c, int Tp, int Tr, int k);

// seg8:0x1e32(pool, rec), Pascal `retf 4` [BYTES seg8:0x1e32-0x2655] (section 6, claims 38-54,
// Omissions J-L): called by the integrator seg8:0x1b54 for a record outside pool 10 whose
// `owner_pool` is not -1, before its own move (flight.cpp integrate, FlightHost::follow_owner).
// The pen phase, the position less the velocity, the holding cost of a ship holder (3 fuel every
// max(mass / -200 + 8, 3) frames; fuel <= 0 makes d 1000), the break beyond 350 cells (cuffs) or
// 450 (hook), on the holder's `state` or for the ambulance (both lines erased, seg7:0x2080,
// seg10:0x744c(0x69), STATIC 0x14 at T; the forces still run), the hook's pull (capturable) or
// swing beyond 40 cells, the cuffs' rope beyond 300 cells, the position plus the velocity,
// seg7:0x1208, the old line erased and, while held, the new one drawn.
void tether_follow(TetherCtx& c, int pool, int rec);

// seg12:0x3148, the pool-10 mover at seg7:0xd16 [BYTES seg12:0x3148-0x3496] (section 4, claims
// 25-30, Omissions H, I): for each record k below DGROUP:0x941a with `state` 0: `collided` 0,
// the frame from the last pass's line, seg8:0x1b54(0x0a00 | k, 1), the cell, seg8:0xb76 from
// image slot 3, + 50 when the hook is farther than (H << 3) / 2 cells from its holder, the
// points, the old line erased, two rand() for the pen RGB(r2, 0, 128 + r1), the new line, the
// roll, + 1, the contact (seg8:0x50aa then hook_meets) while on screen, the miss above 41, and
// seg7:0x1208 for the player's hook while DGROUP:0x951a = 0.
void move_hooks(UpdateCtx& c, World& w);

// seg13:0x6e5-0xa57, ship i's throw [BYTES seg13:0x6e5-0xa54] (section 9, claims 60-61): with
// `Detail` > 1, i on screen, ship word +0x38 = 1, object +0x36 >= 0, not the player in Action
// mode, not while the hook flies and only for slot weapon 2: when the held object G is i's goal
// (ship words +0x32/+0x34), the first visible object of mass > 500 other than G with 120 < d <
// 500 cells and (t << 7) / |G.v| > 125 (t = G's speed toward it), else the goal object, makes
// i let go: seg11:0x947a(DGROUP:0x9324, i, 2). `vis` is the visibility list (null: empty).
void tether_throw(TetherCtx& c, const VisibilityGlobals* vis, int i);

// seg12:0x3778's two calls out on the Blitter (draw.h LinkHooks, Omission O): the erase is
// seg12:0x7256 between SelectObject(DGROUP:0x13dc, GetStockObject(BLACK_PEN)) and the old pen
// back [BYTES seg12:0x38a6-0x38c0]; seg7:0x2080(10, -1) is missile.h recount_in_use.
class TetherLinkHooks : public LinkHooks {
public:
    Blitter* out = nullptr;
    Pools* pools = nullptr;
    std::int16_t play_w = 0, play_h = 0;
    void line(DcId dc, std::int16_t x0, std::int16_t y0, std::int16_t x1, std::int16_t y1) override;
    void seg7_2080(int pool, int rec) override;
};

// ---- the program's links (the integrator fills them in src/world/flight_hooks.cpp; an empty
// slot makes the TetherHost default)
struct TetherLinks {
    std::function<World*()> world;              // the program's World
    std::function<WaveGlobals*()> globals;      // the program's wave globals
    std::function<std::pair<std::int16_t, std::int16_t>()> play_size;  // DGROUP:0x90ea, 0x90ec
    std::function<void(int id, int pool, int rec, int mode)> sound;    // seg10:0x754a
    std::function<void(int id)> command;                               // seg10:0x744c
    std::function<Point16(std::int16_t x, std::int16_t y)> to_screen;  // seg8:0x29ea
    std::function<Blitter*()> blitter;                                 // DGROUP:0x13dc
    std::function<void()> player_sprite;                               // seg7:0x1208
    std::function<std::int16_t(int w, int off)> weapon_word;           // W(w)+off
    std::function<const VisibilityGlobals*()> visible;                 // DGROUP:0x507c / 0x50e0
};
TetherLinks& tether_links();
// The TetherHost over tether_links().
TetherHost& linked_tether_host();
// The routines on the program's links: nothing without a World or wave globals (one log).
void tether_use_program(int P, int R, int w);
int hook_meets_program(int Tp, int Tr, int k);
void tether_follow_program(int pool, int rec);
void tether_throw_program(int ship);
// seg12:0x3778's hooks on the program's links (UpdateHost::link_hooks).
LinkHooks* program_link_hooks();

}  // namespace is::world
