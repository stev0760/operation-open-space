// The pool-5 balls, SNOWBALLS (weapon 4) and FIREBALLS (weapon 5), and the FIRE DEMONS' flames
// (M4 stage 7, track FUEL): the player's launcher seg13:0x1318, the other creator seg12:0x2db6,
// the pool-5 mover seg7:0x40c4 with its trail, its heating copies and its end, and the pool's
// on_damage seg7:0x570a with the SNOWBALL split.
//
// Sources: work/analysis/m4/s7-fuel.md sections 0-4 and its verification s7-fuel-verify.md (47
// claims confirmed; claim 20 CORRECTED: the player-box test reads the raw square of trail
// element 5; Omissions O1-O16), and the listings of every routine named. Where core.md differs,
// the analysis is followed (core.md:664's reversed argument order of seg13:0x1318).
//
// The tables: the entry E(r) (DGROUP:0x5a4c + 0x14·r) and the trail T(r) (DGROUP:0x5eac +
// 0x24·r) are collide.h Pool5Entry, CollideGlobals::pool5[r]. The original holds 11 of each
// (kPool5Table); the port keeps 11 and refuses the ball that would pass them [DESIGN ADR 0016 decisions 2 and 3]
// (stage 7 ruling R2: a Proposed line of ADR 0016). The pool-5 records are Pools' pool
// DGROUP:0x9328 (5 in a wave; seg13:0x1318 names the literal 5).
//
// The in-use count DGROUP:0x9410 the cap reads is seg7:0x2080's recount (missile.h
// recount_in_use), which every launch and every ball's end calls: it drops to the highest live
// record + 1, and to 0 once every ball ended, as core-0009's pool-5 watch shows (p5 = 1 at the
// launch, 0 after 0.93 - 0.97 s) [ORACLE core-0009] [BYTES seg7:0x209f seg7:0x20d4-0x211b
// seg7:0x472b seg13:0x1697]. s7-fuel-verify.md O1 ("ending a ball never lowers it") misses
// that recount; the fuel is spent before the cap test, as O1 says.
//
// 16-bit arithmetic as the original's (ADR 0013 decision 7): every `cdq; idiv` of the listings
// is a 16-bit `cwd; idiv`. None of these routines draws rand(); the mover's only draws are its
// callees' (the puffs, the dispatcher's tethered-target rule).
//
// Portable, no Win32.
#pragma once

#include <cstdint>
#include <functional>
#include <utility>

#include "world/collide.h"
#include "world/flight.h"
#include "world/records.h"
#include "world/update.h"
#include "world/world.h"

namespace is::world {

// ---- the tables and their words

// The entry and trail tables hold 11 entries: DGROUP:0x5a4c + 11·0x14 = DGROUP:0x5b28 and
// DGROUP:0x5eac + 11·0x24 = DGROUP:0x6038 begin the next tables [BYTES seg8:0x870 seg8:0x88b
// seg7:0x3922] (s7-fuel.md 0.1, 0.2).
constexpr int kPool5Table = 11;
constexpr int kPool5Trail = 6;  // Tx, Ty, Ts[0 ... 5] [BYTES seg7:0x410d seg7:0x46df]
constexpr std::int16_t kMassFireball = 0x78;  // 120 [BYTES seg13:0x14da seg7:0x4213]
constexpr std::int16_t kMassSnowball = 0xc8;  // 200 [BYTES seg13:0x14c4 seg7:0x4230]
constexpr std::int16_t kBallSnow = 4;         // the kind of weapon 4 [BYTES seg11:0xad56]
constexpr std::int16_t kBallFire = 5;         // the kind of weapon 5 [BYTES seg11:0xad42]
// The weapon system's life argument (0x1e) [BYTES seg11:0xad40 seg11:0xad54].
constexpr std::int16_t kBallLife = 0x1e;
// The trail's start word -100 (0xff9c) [BYTES seg13:0x1506 seg7:0x58df seg12:0x2fde].
constexpr std::int16_t kTrailStart = -100;
// seg13:0x1318's fuel floor: a launch from a ship needs `fuel` > 40 [BYTES seg13:0x135e].
constexpr std::int16_t kBallFuelFloor = 0x28;
// The sounds: 0x1b FIREBALL.WAV and 0x19 FIRE1.WAV [BYTES seg13:0x1674 seg13:0x1687].
constexpr int kSoundFireball = 0x1b;
constexpr int kSoundFire1 = 0x19;
// The end's square, 54 × 54 at the uncentred corner [BYTES seg7:0x4691 seg7:0x4693].
constexpr int kBallEndSquare = 0x36;

// Entry and trail r of the tables, guarded: an index outside the 11 entries reads and writes a
// scratch entry, reset at each access, and logs once [DESIGN ADR 0016 decision 2].
Pool5Entry& pool5_entry(CollideGlobals& c, int r);

// The grown-record template of all three creators (s7-fuel.md 0.4, verified claim 5): the
// allocated count of `pool` = r + 1 with the block grown (GlobalReAlloc GMEM_MOVEABLE |
// GMEM_ZEROINIT: the new record is zero, a kept one keeps its bytes), then `mass` 0x78,
// `hit_sound` 0x64 (no slot: plays nothing), dword +0x4c = 0xff (red), `radar_draw`
// seg11:0x3914, `on_damage` seg7:0x570a, bytes +0x53 and +0x54 = 0, `owner_pool` -1, `src_y` =
// `gap`, byte +0x2e = 0; E(r)+0 = -1 and the in-use count = r + 1 [BYTES seg13:0x13c1-0x1488
// seg7:0x575b-0x5889 seg12:0x2e27-0x2f58]. A record reference into the pool is stale after it.
void pool5_grow(Pools& p, CollideGlobals& c, int pool, int r);

// The free search of the three creators [BYTES seg13:0x13aa-0x1495 seg13:0x168b-0x168f
// seg7:0x573d-0x5896 seg12:0x2e08-0x2f67]: r = 0, 1, ...; at r = the allocated count the record
// is grown (pool5_grow); the first r with E(r)+0 = -1 is returned. An r that reaches
// kPool5Table returns -1 with no write past the tables and logs once: no ball is made [DESIGN
// ADR 0016 decisions 2 and 3] (ruling R2).
int pool5_free_record(Pools& p, CollideGlobals& c, int pool);

// seg13:0x1318's cap: 2·(`Detail` + 2), 16-bit [BYTES seg13:0x139a-0x139f].
std::int16_t launch_cap(std::int16_t detail);
// seg12:0x2db6's cap: min(10, lo16((`Detail` + 2)·2 · (min(DGROUP:0x9334, 10) + 10)) / 20), the
// product's high word dropped by `cwd` before the `idiv` [BYTES seg12:0x2dd3-0x2df6] (verified
// claim 34a).
std::int16_t spawn_cap(std::int16_t detail, std::int16_t score_level);

// The picture by age t (s7-fuel.md 2.2, verified claim 13): t < 28: frame t / 5, side 6f + 10,
// offset 3f; t = 28, 29: frame 7, side 52, offset 21; t >= 30: all 0. The side is unscaled
// here; the offset is never scaled [BYTES seg7:0x474f-0x4771 seg7:0x40db-0x40fc].
struct BallFrame {
    std::int16_t f = 0, side = 0, off = 0;
};
BallFrame ball_frame(std::int16_t age);

// ---- the creators

// The calls out of the creators.
class FuelHost {
public:
    virtual ~FuelHost() = default;
    // seg10:0x754a(id, pool, rec, mode). The default logs once.
    virtual void sound(int id, int pool, int rec, int mode);
    // seg10:0x744c(0x69): the effect-priority reset before the player's launch sound, as
    // missile.h MissileHost::sound_reset: seg10:0x754a(0x69, 0, 0, 0) alone [BYTES
    // seg13:0x165f-0x1666] (O16).
    virtual void sound_reset() { sound(0x69, 0, 0, 0); }
    // W(w)+off of the weapon table DGROUP:0x2f54: the program's words (collide.h weapon_word).
    virtual std::int16_t weapon_word(int w, int off) { return ::is::world::weapon_word(w, off); }
};
// A FuelHost over a sound function: the program's binding (seg10:0x754a through the frame
// host's sound()) [DESIGN].
class SoundFuelHost final : public FuelHost {
public:
    explicit SoundFuelHost(std::function<void(int id, int pool, int rec, int mode)> f) : f_(std::move(f)) {}
    void sound(int id, int pool, int rec, int mode) override {
        if (f_) f_(id, pool, rec, mode);
    }

private:
    std::function<void(int, int, int, int)> f_;
};

// What the creators touch: the World (pools, ships, `Detail` DGROUP:0x9208, DGROUP:0x9334), the
// roles, the player and the time, the pool-5 tables.
struct FuelCtx {
    World& w;
    WaveGlobals& g;
    CollideGlobals& c;
    FuelHost& h;
};

// seg13:0x1318(pool, rec, life, kind), Pascal `retf 8` in declaration order (verified claim 1;
// core.md:664 writes it reversed) [BYTES seg13:0x1318-0x16a7] (s7-fuel.md section 1, claims
// 2-11): a ship launcher (pool = DGROUP:0x9324) needs a slot 1 ... (int8)+0xcb - 1 holding `kind`
// (the first one) and `fuel` > 40, then pays W(kind)+0x2e (8 for both weapons), floored at 0;
// any other launcher goes on at once. With the fuel spent, an `on_screen` 0 launcher or a full
// pool (cap 2·(`Detail` + 2) not above DGROUP:0x9410) makes no ball (O1: the fuel stays spent).
// The ball takes the lowest free record of the literal pool 5 (grown at the allocated count):
// E+0 = 0, +0x10 = 0, the last hit = the launcher, `mass` 200 (kind 4) or 120 (kind 5),
// `state` 0, split 0, byte +0x2e 0, Tx[0] = Ty[0] = -100 only (O4: the loop counter is not in
// the index), the shooter, `vx`/`vy` = lo16(6·heading(frame)), the corner lo16(heading ·
// (E/32 + 6)) + E/2 + the launcher's x/y - 32, E being its `extent`, the life, the launcher's
// velocity added; seg10:0x744c(0x69) for the player's ball, seg10:0x754a(0x1b when life > 24,
// else 0x19, 5, r, 1) and seg7:0x2080(5, -1). True when a ball was made. No rand().
bool launch_fuel_ball(FuelCtx& c, int pool, int rec, std::int16_t life, std::int16_t kind);

// seg12:0x2db6(pool, rec, heading, life, kind), Pascal `retf 0xa` [BYTES seg12:0x2db6-0x3145]
// (s7-fuel.md section 4, verified claims 34, 34a, 34b, O13): life capped at 24; the cap
// spawn_cap() against the in-use count of DGROUP:0x9328; the free search on DGROUP:0x9328; E+0
// = 0, +0x10 = 0, `state` 0, split 0, byte +0x2e 0, `mass` 0x78 (kind 5) or 0xc8 (kind 4),
// Tx/Ty[0 ... 4] = -100; the corner with E/32 + 4 heading steps of `heading`; the shooter and
// the last hit = (pool, rec); `vx`/`vy` = lo16(6·heading) without the launcher's velocity;
// seg10:0x754a(0x1b when life > 25, else 0x19, DGROUP:0x9328, r, 1) (with the cap it is always
// 0x19); the life; seg7:0x2080(DGROUP:0x9328, -1). The FIRE DEMONS' fire (seg12:0x64aa, the
// LAUNCHED track's) calls it as (6, m, frame, 0xb, 5 or 4). True when a ball was made.
bool spawn_fuel_ball(FuelCtx& c, int pool, int rec, std::int16_t heading, std::int16_t life,
                     std::int16_t kind);

// ---- the mover and the on_damage

// seg7:0x40c4, the pool-5 mover, once per wave frame between the fragments and pool 6
// [BYTES seg7:0x40c4-0x477c seg7:0xcf6] (s7-fuel.md section 2; claims 12-28, 20 as corrected;
// O6-O9): for r < the in-use count of DGROUP:0x9328 (re-read) with E(r)+0 > -1: the frame by
// age, the side (c << `Size`) >> 1, the trail shifted up, `x`/`y` - 8·offset, seg8:0x29ea into
// `scr_x`/`scr_y` and trail element 0, `size`, `extent` = side << DGROUP:0x9150, `src_x` =
// (DGROUP:0xb70[f] << `Size`) >> 1, `src_y` 0; the draw seg8:0xb76(P5·256 + r, 6, slot 10 for
// `mass` 0x78 or 11 for 0xc8, 0) (UpdateHost::draw_sprite; another mass keeps the previous
// record's slot, and before any the port draws nothing [DESIGN ADR 0016 decision 1]); `x`/`y`
// back; on the window DC (UpdateHost::blitter) PatBlt BLACKNESS of trail element 5 at (x - 1,
// y - 1) with side + 2; the player's ship dword +0x156 = now - 1 when W/2 - s5 < x5, H/2 - s5 <
// y5, x5 < W/2 + B, y5 < H/2 + B (B = (40 << `Size`) >> 1, signed, the raw element-5 square:
// claim 20 as corrected); PatBlt 0x00A000C9 of elements 4 and 3 at (x - 1, y - 1) with side + 2
// under the brushes DGROUP:0x5ea8 and DGROUP:0x5eaa (rock_images.h WaveGraphics::mask,
// checker), each SelectObject undone; E+0x0c/+0x0e = `vx`/`vy`, seg8:0x1b54(P5·256 + r, 1);
// with `on_screen` != 0 or a race wave the pair finder, seg8:0x546c(hit, &vx, &vy, extent + 2,
// mass) and seg8:0x5df2(P5, r, hit pool, hit rec, a), then the heating copies (the rock pool:
// FIREBALL heat + 1 up to 9 and +0x6a = now + 5000, SNOWBALL heat = -(5·heat) for heat > 0;
// pool 8 with `mass` 0xdac the same; any `mass` 0xa0: the icon words of the hit record, which
// logs once); age + 1, and past life + 4 the end: the 54 × 54 BLACKNESS square when seg8:0x7ba8
// accepts (`scr_x`, `scr_y`), six squares of `size` + 2 at the trail, E+0 = -1, `state` 1,
// seg18:0x5350 in seg18 mode [OPEN M6: logged], seg7:0x2080(P5, r).
void move_pool5(UpdateCtx& c, World& w);

// seg7:0x570a(pool, rec, a), the pool-5 `on_damage`, cdecl [BYTES seg7:0x570a-0x5a3e] (s7-fuel.md
// section 3, verified claims 29-33): `a` is never read and 0 is returned. A split ball or one
// whose `mass` is not 0xc8 gets `vx`/`vy` back from E+0x0c/+0x0e. A SNOWBALL with split 0
// splits: a record from the free search of DGROUP:0x9328 (no cap: the count can pass 2·(`Detail`
// + 2)) gets E+0 = 0, +0x10 = 0, `state` 0, split 1, byte +0x2e 0, `mass` 0xc8, Tx/Ty[0 ... 4] =
// -100, the parent's `x`/`y`, shooter and last hit, `vx` = svy/3 + svx, `vy` = svx/(-3) + svy,
// life 16; seg7:0x2080(DGROUP:0x9328, -1); the parent `vx` = svy/(-3) + svx, `vy` = svx/3 +
// svy, split 1, life 16. When the search would pass the 11 table entries the new half is not
// made and the parent's half runs alone [DESIGN ADR 0016 decisions 2 and 3] (ruling R2). The object writes go to
// the `pool` argument's records, the counts and the search to DGROUP:0x9328, as the bytes do
// (the same pool in a wave). The dispatcher's handler call (collide.h call_on_damage) reaches it
// for kDamagePool5.
std::int16_t pool5_on_damage(CollideCtx& x, int pool, int rec, std::int16_t a);

// seg12:0x2288's pool-5 words (gate_exit.h GateExitHost::pool5_slot): E(rec)+0 = E(rec)+2 - 7
// [BYTES seg12:0x22cf-0x22d6] (verified claim 44): the ball, its byte +0x2e now 0xff, draws and
// collides no more and ends 12 passes later while its trail fades. The sound 0x27 and the byte
// are gate_exit.cpp's.
void pool5_gate(CollideGlobals& c, int rec);

// seg9:0x6624's pool-5 part [BYTES seg9:0x677b-0x67af] (verified claim 45): E(r)+0 = -1 and
// `state` 1 for every allocated record of DGROUP:0x9328. With no tables (null) only the states
// are written.
void pool5_wave_start(Pools& p, int pool5, CollideGlobals* c);

// The program's pool-5 tables for the routines that run outside a frame host (the set-up's
// placement): the program registers its collision words (flight_hooks.h collide_globals());
// unregistered, null [DESIGN].
void set_pool5_tables(std::function<CollideGlobals*()> f);
CollideGlobals* pool5_tables();

}  // namespace is::world
