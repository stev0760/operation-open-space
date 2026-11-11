// Pool 11, the dropped pods (work/analysis/m4/s3-fuel.md section 5 with s3-fuel-verify.md claims
// 47-56): the mover seg12:0x6b1e, the expiry's on_destroy seg12:0x6a60, the pickup dissolve
// seg12:0x129e and the pod image seg10:0x42ee. The pod record DGROUP:0x8560 + 0x34·i is effects.h
// PodRecord: its dword +0x04 holds the age (low word, DGROUP:0x8564) and the lifetime (high word,
// DGROUP:0x8566); the fill seg13:0x4ba0 writes 0x01f40000, age 0 and 500 frames (verified claim
// 53) [BYTES seg13:0x4bb3]. What the pickup gives the ship (the touch's pool-11 branch) is M5.
// M4 stage 3, track FUEL.
//
// 16-bit arithmetic as the original's (ADR 0013 decision 7). Portable, no Win32.
#pragma once

#include <cstdint>
#include <functional>

#include "world/effects.h"
#include "world/tank.h"
#include "world/view.h"

namespace is::world {

struct UpdateCtx;

// The pod's image slot, object word +0x4a (fill_pod writes 0x1d) [BYTES seg13:0x4bb3-0x4cbe].
constexpr int kSlotPod = 0x1d;
// The mass seg12:0x6b1e passes to the bounce: the constant 0x6e, not the record's 101 (verified
// claim 51) [BYTES seg12:0x6b61].
constexpr std::int16_t kPodBounceMass = 0x6e;

// The age and the lifetime words of a pod record.
inline std::int16_t pod_age(const PodRecord& p) { return static_cast<std::int16_t>(p.age.lo); }
inline std::int16_t pod_lifetime(const PodRecord& p) { return static_cast<std::int16_t>(p.age.hi); }

// seg12:0x6b1e [BYTES seg12:0x6b1e-0x6bd8 seg12:0x6be8-0x6ce5] (verified claims 48-52, 55): per
// record below DGROUP:0x941c: a taken pod (`state` != 0) only continues its dissolve
// seg12:0x129e(11, i) while +0x51 != 0; else `collided` cleared, `capturable` +0x53 = 1 once the
// age is above 2, one step then drag 0x28 (on or off screen), the cell (kind - 1)·(size + gap) +
// gap of the slot +0x4a drawn with kind 3, the age + 1; on screen and not `collided` the pair
// finder: a ship with no shield and no cloak touches it, anything else bounces it with mass 0x6e
// and dispatches with the pod as T; an age above the lifetime outside Demon's Gate
// (DGROUP:0x92e8 = 0) calls its on_destroy and seg7:0x2080(11, -1).
void move_pods(UpdateCtx& c, World& w);

// What seg12:0x6a60 calls.
struct PodDestroyOut {
    std::function<void(int x, int y, int w, int h)> erase;     // PatBlt(DGROUP:0x13dc, ..., BLACKNESS)
    std::function<void(int pool, int rec, int arg)> puff;       // seg7:0x5b68
    std::function<ScreenPoint(std::int16_t x, std::int16_t y)> to_screen;  // seg8:0x29ea
};
// seg12:0x6a60(pool, rec), cdecl [BYTES seg12:0x6a60-0x6b1c] (verified claim 54): the square
// blacked when seg8:0x7ba8 accepts the record's world point on screen, for pool 6 the missile
// entry's age = W(weapon)+0x1e and weapon -1 [BYTES seg12:0x6ac5-0x6adc], five smoke puffs
// seg7:0x5b68(pool, rec, 1), `state` = 1.
void pod_destroyed(World& w, int pool, int rec, const PodDestroyOut& d);

// seg12:0x129e(pool, rec), cdecl [BYTES seg12:0x129e-0x1774]: the pickup dissolve of a pod (pool
// 11) or a pool-6 record, seg12:0xe5c's twin: the flag, the step and the phase as there; the
// image slot +0x4a (pool 11, x offset 0) or W(w)-4 of the record's weapon w = DGROUP:0x66e4 +
// 0x1c·rec (pool 6, x offset max(0, W(w)+0x26 - 1)·(size + gap)); a slot outside 0 ... 100 counts
// DGROUP:0x9498, writes 0x10 into the weapon word DGROUP:0x66e4 + 0x1c·rec (for a pod too) and
// takes the slot DGROUP:0x3450 (verified claim 56), weapon 16's word W(16)-4
// [DESIGN ADR 0016 decision 3]; the player's sprite on odd phases for +0x48 = the player at
// `Detail` > 1; off screen nothing more; then the ten cases (margins u(2) and u(4) where
// seg12:0xe5c has u(3) and u(6)), phase 9 with seg7:0x2080(pool, -1) and the player's sprite,
// and the frame (its bottom strip at y + size - 3, a literal 3). Returns the phase. The weapon
// words (pool 6's and W(16)-4) are read through `weapon_word` (W(w)+off); null reads 0.
std::int16_t pod_dissolve(World& w, int pool, int rec, const DissolveOut& d,
                          const std::function<std::int16_t(int w, int off)>& weapon_word = {});

// seg10:0x42ee [BYTES seg10:0x42ee-0x440a]: unless slot 0x1d (DGROUP:0x3a22) holds an image, an 89
// x 33 image blacked, then graphic 690 + k (seg10:0x6c02(690 + k, 3, -1)) cell (0, 0, 23 x 23) at
// (28k + 5, 5) for k = 0, 1, 2. The rescale seg10:0x6308(0x1d, -1, -1, -1) at Size != 1 is [OPEN].
void build_pod_image(Blitter& out, ImageSlotTable& images);

}  // namespace is::world
