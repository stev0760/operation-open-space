// LIGHTNING (work/analysis/m4/s7-ring.md section 2 with s7-ring-verify.md: claims 10-22, 24-27
// CONFIRMED, claims 23 and 28 CORRECTED; Omissions O5-O14): the search seg11:0x9fdc, the strike
// seg11:0xa234, the bolt seg11:0xa4a6 (its points are src/shell/bolt.h make_bolt) and the bolt's
// life seg11:0xa9ea. M4 stage 7, track RING.
//
// The charge is an object's byte +0x50 (`b50`, signed): the 1000-ms tick seg11:0x1bcc raises a
// ship's while its selected weapon is 3 (src/hud/status.cpp), the strike moves it from the
// source to the target, and the visibility pass seg8:0x4e14 lets any charged object outside the
// ship pool discharge through seg11:0x9fdc on one frame in nine (visibility.h; O7).
//
// Callers of seg11:0x9fdc: the weapon system's case 2 with (DGROUP:0x9324, DGROUP:0x9646), the
// visibility pass, BATTERIES' fire seg12:0x66e3 (pool 6, a missile whose signed +0x50 > 25), the
// computer pilots' seg4:0x594f (M5) and the pool-8 weapon dispatcher's seg12:0x6ea1 (O5). Callers
// of seg11:0xa234: seg11:0x9fdc, the virus mover seg12:0x53fc (pool 9) and the tether helper
// seg8:0x22a8, the last two with no fee and no search (O6).
//
// The bolt's words live in BoltState (bolt_state(), the program's one): DGROUP:0x1016 (`pending`,
// N + 1 after a drawn bolt, negated by each seg11:0xa9ea call, 0 when done), DGROUP:0x1036
// (`flashed`), DGROUP:0x951e / 0x9520 (the source) and the point tables. DGROUP:0x1016 is also
// cleared by the briefing entry seg9:0x5cca (seg9:0x5ddc), the state-1 load seg10:0x0
// (seg10:0x31) and the intro (seg15:0x80fb); DGROUP:0x1036 only by seg11:0xa9ea, so a bolt cut
// between its second and third frame call leaves `flashed` = 1 and the next bolt ends after one
// call, an erase with no flash and no second thunder (O13) [BYTES seg9:0x5ddc seg10:0x31
// seg15:0x80fb seg11:0xac6e-0xac7b]. The intro keeps its own copy of both words (src/intro/
// intro.h), so a title bolt does not carry into the wave [DESIGN].
//
// The drawing goes to the window DC DGROUP:0x13dc through the Blitter (CreatePen, SelectObject,
// the clipped line seg12:0x7256 of src/world/line.h, SelectObject back, DeleteObject); the pens'
// RGB colours reach the 8-bit surface through ADR 0007 as the effect brushes do (ruling R6).
//
// 16-bit arithmetic as the original's (ADR 0013 decision 7). Portable, no Win32.
#pragma once

#include <cstdint>
#include <functional>

#include "shell/bolt.h"
#include "shell/rng.h"
#include "world/records.h"
#include "world/sprite.h"
#include "world/world.h"

namespace is::world {

struct ShotState;

// ---- constants [BYTES seg11:0xa049 seg11:0xa06f seg11:0xa1b6 seg11:0xa1bc seg11:0xa284
// seg11:0xa3eb seg11:0xa411 seg11:0xa415 seg11:0xa43d seg11:0xa471 seg10:0x20bf]
constexpr int kWeaponLightning = 3;             // LIGHTNING [BYTES ENG_LANG.DLL STRING 12] id 189
constexpr int kSoundBolt = 0x22;                // BOLT.WAV (re/artifacts/sounds.tsv)
constexpr std::int16_t kLightningPlayerRange = 150;  // the player's R floor
constexpr std::int16_t kLightningAmountBase = 2000;  // seg8:0x5df2's amount A / 2 + 2000
constexpr int kLightningTeam = 7;               // the design team word that zeroes both charges
constexpr int kWordLightningCost = 0x2e;        // W(3)+0x2e = DGROUP:0x3072, 20 [BYTES seg10:0x20bf]
// The pens [BYTES seg11:0xa8ad-0xa8bf seg11:0xa91e-0xa930 seg11:0xaa0d-0xaa13 seg11:0xaa52-0xaa65
// seg11:0xaacb-0xaad1 seg11:0xaae6-0xaaf9 seg11:0xabbd]: CreatePen(PS_SOLID, width, colour),
// width 2 for the main line and 1 for the branches.
constexpr std::uint32_t kBoltStrikePen = 0x00ffffc3;  // RGB(0xc3, 0xff, 0xff): the strike, any pool but 9
constexpr std::uint32_t kBoltVirusPen = 0x0000ff00;   // RGB(0, 0xff, 0): a pool-9 source (strike and flash)
constexpr std::uint32_t kBoltFlashPen = 0x00ffff7f;   // RGB(0x7f, 0xff, 0xff): the flash
constexpr std::uint32_t kBoltErasePen = 0x00000000;   // black

// ---- the bolt's words
struct BoltState {
    std::int16_t pending = 0;   // DGROUP:0x1016
    std::int16_t flashed = 0;   // DGROUP:0x1036
    std::int16_t src_pool = 0;  // DGROUP:0x951e
    std::int16_t src_rec = 0;   // DGROUP:0x9520
    // DGROUP:0x6b5c (P0 ... PN), DGROUP:0x6bbc + 20b (count DGROUP:0x9526) and DGROUP:0x6cfc +
    // 12s (count DGROUP:0x9528): the screen points of the last drawn bolt.
    shell::BasicBolt<shell::BoltPoint> shape;
};
// The program's one BoltState (DGROUP lives once).
BoltState& bolt_state();

// ---- the calls out
class LightningHost {
public:
    virtual ~LightningHost() = default;
    // seg10:0x754a(id, a, b, mode): mode 1 at object (a, b), mode 4 at the screen point (a, b).
    // The default logs once.
    virtual void sound(int id, int a, int b, int mode);
    // seg8:0x5df2(T pool, T rec, X pool, X rec, amount), the hit dispatcher (collide.h dispatch,
    // COLLIDE's), with DGROUP:0x943a / 0x943c already set; its result is dropped. The default
    // logs once and returns 0.
    virtual std::int16_t dispatch(int tp, int tr, int xp, int xr, std::int16_t amount);
    // seg7:0x2512(icon, k), the overlay (icon_effects.h icon_overlay, ICONS'); the default logs once.
    virtual void icon_overlay(int icon, int k);
    // seg8:0x29ea(x, y), a world point to the screen; the default logs once and returns (0, 0).
    virtual Point16 to_screen(std::int16_t x, std::int16_t y);
    // USER.15 GetCurrentTime, which seg11:0xa4a6 seeds rand() with [BYTES seg11:0xa4b5]. The
    // default is the pass's DGROUP:0x1e50 [DESIGN: the program binds its tick].
    virtual std::uint32_t current_time(std::uint32_t pass_now) { return pass_now; }
    // SetBkMode(DGROUP:0x13dc, mode) [BYTES seg11:0xa9f7-0xa9fd]: the bolt's pens are solid, so
    // it changes no pixel of the bolt; the default does nothing [DESIGN].
    virtual void bk_mode(int mode) { (void)mode; }
    // The design word +0x38 (DGROUP:0x970a + 0xad·n), the team step 9 of seg11:0xa234 compares
    // with 7 [BYTES seg11:0xa466-0xa471]; the default reads collide.h design_from_world, as the
    // dispatcher's CollideHost::design does [OPEN: the word's writer].
    virtual std::int16_t team(const World& w, int design);
};

// What the routines touch.
struct LightningCtx {
    World& w;                // the pools, ships, roles, player, W, H, DGROUP:0x1318, DGROUP:0x9498
    BoltState& bolt;
    LightningHost& h;
    shell::Rng& rng;          // the game's rand() DGROUP:0x144c
    const WeaponWords& weapons;  // DGROUP:0x2f54: W(3)+0x2e, W(id)+0x1e
    std::uint32_t now = 0;    // DGROUP:0x1e50
    Blitter* out = nullptr;   // the window DC DGROUP:0x13dc; null draws nothing
    std::int16_t* dg_943a = nullptr;  // DGROUP:0x943a / 0x943c, the colliding party the strike
    std::int16_t* dg_943c = nullptr;  //   sets before its dispatcher call; null writes nothing
    ShotState* shots = nullptr;       // the muzzle tables DGROUP:0x7b60; null: shots.h shot_state()
};

// ---- seg11:0x9fdc

struct SearchResult {
    bool acted = false;        // the gate passed
    std::int16_t range = 0;    // R
    int pool = -1, rec = 0;    // the pick ([bp-4], [bp-6]); pool -1 for none
    bool paid = false;         // the pool-14 fee was taken
};
// R = min(4·sext8(+0x50) (16-bit `shl`), W / 2 (`cwd; sub ax, dx; sar 1`)), then at least 150 for
// the player's ship (claim 11) [BYTES seg11:0xa036-0xa078].
std::int16_t lightning_range(std::int8_t charge, std::int16_t W, bool player);
// seg11:0x9fdc(pool, rec), cdecl [BYTES seg11:0x9fdc-0xa233] (claims 10-15):
//  1. Gate: for the ship pool some slot l in 1 ... sext8(+0xcb) - 1 holding 3 while fuel >
//     W(3)+0x2e (signed); a slot 3 with too little fuel lets the search go on; another pool
//     always proceeds (claim 10).
//  2. R (lightning_range).
//  3. The pick: pools DGROUP:0x9326 ... DGROUP:0x9324 (signed) except 5 and 12, records below
//     the in-use count with `state` 0, not the user; d = seg8:0x285a(candidate, user) < R
//     (signed, cells, candidate - user, top-left points, no `on_screen` test); h =
//     seg8:0x19b0(den = dx, num = dy) with h - 1 <= user `frame` (+0x28) <= h + 1, signed, no
//     wrap; the largest `mass` strictly above the best so far (from 0) wins (claims 12, 13). The
//     goal-weighted score (R - d)·mass·(4 for a pool-14 user going for it) / R is stored in a
//     local and overwritten before any read [BYTES seg11:0xa120-0xa16b]: not ported, it has no
//     effect [DESIGN].
//  4. With a pick: a user of the literal pool 14 pays fuel = max(0, fuel - W(3)+0x2e) (`jns`),
//     before seg11:0xa234, which may refuse (a bolt pending: O9) (claim 14).
//  5. A user of the literal pool 6 whose missile entry's launcher (DGROUP:0x66e8 / 0x66ea +
//     0x1c·rec) is the pick, in the ship pool, gets the entry's age DGROUP:0x66ec = W(id)+0x1e -
//     1, whether or not the strike happened (claim 15).
SearchResult lightning_search(LightningCtx& c, int pool, int rec);

// ---- seg11:0xa234

enum class StrikeOutcome : std::uint8_t {
    kPending,    // DGROUP:0x1016 != 0: nothing at all (claim 16)
    kBadRecord,  // rec not below the source pool's in-use count: DGROUP:0x9498 + 1 (claim 17)
    kStruck,     // the sound, the bolt, the dispatch and the charges
    kGuarded,    // a virus with charge -1, 0 or 1: the division the original faults on skipped
};
// The amount A of a source with charge c (claim 21): sext8(c), or for a virus source (pool 9)
// r % sext8(c / 2) + sext8(c / 3) with r its rand() and both quotients 8-bit `idiv`s (the %
// 16-bit). `ok` is false when c / 2 is 0 (c in -1 ... 1): the original divides by zero at
// seg11:0xa3a3 and faults; the port computes nothing then (A = 0) [DESIGN ADR 0016 decision 4] (ruling R1)
// [BYTES seg11:0xa37d-0xa3c7].
std::int16_t strike_amount(int pool, std::int8_t charge, int r, bool* ok);
// seg11:0xa234(pool, rec, tpool, trec), cdecl [BYTES seg11:0xa234-0xa4a4] (claims 16-25, 23 as
// corrected):
//  1. DGROUP:0x1016 != 0: return. The locals DGROUP:0x2400 / 0x2402 sar 3 ([bp-0x20],
//     [bp-0x1e]) are never read (O12): not ported.
//  2. DGROUP:0x9520 = rec, DGROUP:0x951e = pool; rec >= the in-use count (signed): DGROUP:0x9498
//     + 1, return.
//  3. seg10:0x754a(0x22, pool, rec, 1).
//  4. The source point: a ship's one-barrel muzzle (x + 8·mx(n, frame), y + 8·my(n, frame)),
//     n = design_index, the table DGROUP:0x7b60 + 4(64n + frame); another source's centre (x +
//     extent / 2, y + extent / 2); the target's centre the same; both through seg8:0x29ea; then
//     the bolt seg11:0xa4a6(s0, s1, pool) (bolt_draw) (claim 18).
//  5. A (strike_amount; a pool-9 source draws its rand() here, after the bolt's).
//  6. A pool-9 source on an icon (tpool 0): seg7:0x2512(trec, 0) (claim 22).
//  7. DGROUP:0x943c = rec, DGROUP:0x943a = pool, seg8:0x5df2(tpool, trec, pool, rec, A / 2 +
//     2000): the dispatcher deals A / 2 with its flag for 2 <= A <= 1999, a plain 2000 for A in
//     -1 ... 1, 2000 + A / 2 for A <= -2, and 1 for a ship source of team 7 (claim 23 as
//     corrected; the dispatcher's rule, collide.h).
//  8. A target outside the ship pool: +0x50 = lo8(3A / 5), overwriting what the dispatcher left
//     (a tank's wobble rate: O11); the player's ship as source: A = A / 3; the source's +0x50 -=
//     lo8(A) (claim 24).
//  9. A ship source whose design team word (DGROUP:0x970a + 0xad·n) is 7: both charges 0, the
//     target's in any pool (claim 25).
// With the virus's division guarded (kGuarded) the steps that take A (the dispatch and both
// charge stores) are skipped and logged once; the others run [DESIGN ADR 0016 decision 4] (ruling R1).
StrikeOutcome lightning_strike(LightningCtx& c, int pool, int rec, int tpool, int trec);

// ---- seg11:0xa4a6 and seg11:0xa9ea

// seg11:0xa4a6(x1, y1, x2, y2, pool), cdecl [BYTES seg11:0xa4a6-0xa9e9] (claims 19, 20): srand
// with the low word of GetCurrentTime (seg5:0xc4) first, on every path; the points (make_bolt);
// when it draws: CreatePen(0, 2, 0x0000ff00 for pool 9, else 0x00ffffc3) for P(i-1) -> P(i), i =
// 1 ... N, then the same colour at width 1 for the branches (4 lines each) and the sub-branches
// (2 lines each), every line through seg12:0x7256, each pen selected out and deleted; then
// DGROUP:0x1016 = N + 1. The two early exits (L / 50 > 22, a 17th branch) draw nothing and leave
// DGROUP:0x1016 as it is. True when it drew.
bool bolt_draw(LightningCtx& c, int x1, int y1, int x2, int y2, int pool);
// seg11:0xa9ea(arg), cdecl [BYTES seg11:0xa9ea-0xac82] (claim 28 as corrected): SetBkMode(2);
// the main line i = 1 ... |D| - 1 (seg9:0x88ba) at width 2 and the branches at width 1, black for
// D = DGROUP:0x1016 > 0; for D <= 0 with arg != 0 first seg10:0x754a(0x22, DGROUP:0x951e,
// DGROUP:0x9520, 1) when DGROUP:0x1318 <= 1, else seg10:0x754a(0x22, P0.x, P0.y, 4) at
// DGROUP:0x6b5c, and the pens green (0x0000ff00) for a pool-9 source with arg != 0, else
// 0x00ffff7f. Then: D > 0 with DGROUP:0x1036 = 0: the branches again with the stock BLACK_PEN,
// D = -D; otherwise DGROUP:0x1036 = 0 sets it to 1 and D = -D, DGROUP:0x1036 != 0 clears both.
// The wave frame calls it with 1 while DGROUP:0x1016 != 0 (seg7:0xca0-0xca9); the intro's
// seg11:0xa9ea(0) is src/intro Intro::bolt_pass.
void bolt_frame(LightningCtx& c, int arg);

// ---- the program's links (the glue, src/world/flight_hooks.cpp, fills them; an empty slot keeps
// LightningHost's default)
struct LightningLinks {
    std::function<void(int id, int a, int b, int mode)> sound;                             // seg10:0x754a
    std::function<std::int16_t(int tp, int tr, int xp, int xr, std::int16_t a)> dispatch;  // seg8:0x5df2
    std::function<void(int icon, int k)> icon_overlay;                                     // seg7:0x2512
    std::function<Point16(std::int16_t x, std::int16_t y)> to_screen;                      // seg8:0x29ea
    std::function<std::uint32_t()> current_time;                                           // USER.15
    std::function<void(int mode)> bk_mode;                                                 // SetBkMode
    std::function<std::int16_t(int design)> team;                                          // DGROUP:0x970a + 0xad·n
    // DGROUP:0x943a / 0x943c of the program (FrameGlobals::dg_943a / dg_943c); null writes nothing.
    std::int16_t* dg_943a = nullptr;
    std::int16_t* dg_943c = nullptr;
};
LightningLinks& lightning_links();
// The LightningHost over lightning_links().
LightningHost& linked_lightning_host();
// The routines on the program's state: World w, bolt_state(), linked_lightning_host(),
// shell::game_rng(), missile.h missile_state().words, the window Blitter wave_blitter(), the
// links' DGROUP:0x943a / 0x943c and `now` (DGROUP:0x1e50).
SearchResult lightning_search_on(World& w, std::uint32_t now, int pool, int rec);
StrikeOutcome lightning_strike_on(World& w, std::uint32_t now, int pool, int rec, int tpool, int trec);
void bolt_frame_on(World& w, std::uint32_t now, int arg);

}  // namespace is::world
