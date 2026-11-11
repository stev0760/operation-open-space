# Random numbers

INSPACE draws every random number from one runtime-library `rand()` with a 32-bit state. ISPACE.DLL and FACTORY.EXE carry their own copies of the same routine with separate state. The generator is the linear congruential one of the Microsoft C runtime, not the Borland one. The seed is the low word of `GetCurrentTime`. [BYTES seg5:0xdc] [BYTES seg5:0xc4]

## The generator
- `rand()` is INSPACE seg5:0xdc, a far cdecl routine with no arguments. It multiplies the dword state at DGROUP:0x144c by 0x000343FD (214013) through the RTL's 32 × 32-bit multiply seg5:0x314, which keeps the low 32 bits. It then adds 0x00269EC3 (2531011), stores the state and returns bits 16–30 of it in AX. [BYTES seg5:0xe1] [BYTES seg5:0xf6] [BYTES seg5:0x105] [BYTES seg5:0x314]
- `srand(seed)` is INSPACE seg5:0xc4. It stores the 16-bit argument in the low word of the state and clears the high word. [BYTES seg5:0xcc] [BYTES seg5:0xcf]
- The state starts at 1 in the initialised data. [BYTES DGROUP:0x144c]
- The same constants and output bits define `rand()` in the Microsoft C runtime. Borland C++ multiplies by 0x015A4E35 and adds 1. [REF Microsoft C runtime rand.c] [REF Borland C++ 3.1 runtime library rand.c]
- Only seg5:0xc4 and seg5:0xdc write or read DGROUP:0x144c. [BYTES seg5:0xcc] [BYTES seg5:0xfc]

A C++ model that reproduces the sequence bit for bit: [DESIGN]

```cpp
#include <cstdint>

struct OriginalRand {                    // INSPACE seg5:0xc4 / seg5:0xdc
    std::uint32_t state = 1;             // DGROUP:0x144c, initialised data
    void srand(std::uint16_t seed) { state = seed; }
    int rand() {                         // 0..32767
        state = state * 214013u + 2531011u;    // mod 2^32
        return static_cast<int>((state >> 16) & 0x7FFFu);
    }
};
```

## Seeds
INSPACE calls `srand` from three places. Each passes AX of `GetCurrentTime` (USER.15), the low 16 bits of Windows' millisecond count:

| Call | Function | When it runs |
|---|---|---|
| seg2:0x1b6b | seg2:0x1ac8 | On every screen-state change, after the play area is recomputed. The idle step reaches it through seg2:0x1d58 ([timing.md](timing.md)), and start-up reaches it from seg6:0x1b4d |
| seg11:0xb28 | seg11:0xafe | When the wave HUD is built (the `IS_INFO` panels and the scrollbar). Callers: seg2:0x9b0, seg11:0x3e48, seg16:0x1e57, seg18:0x74d |
| seg11:0xa4bb | seg11:0xa4a6 | At the start of a routine with 14 `rand()` call sites, most of them inside loops, so the number of draws varies. Callers: seg11:0xa371, seg15:0x80ce and four sites in seg18 |

[BYTES seg2:0x1b65] [BYTES seg11:0xb22] [BYTES seg11:0xa4b5] [BYTES seg2:0x1dbd] [BYTES seg11:0xa559-0xa7d1]

- seg11:0xa4a6 generates the lightning bolt's points ([shell.md](shell.md#intro-and-title-screen), the bolt shape; [core.md](core.md#lightning-seg110x9fdc-seg110xa234-seg110xa9ea)). Its srand(low word of GetCurrentTime()) (seg5:0xc4) runs before anything else, also on its early exits, so every strike in a wave reseeds the generator and the sequence after a bolt depends on the clock. [BYTES seg11:0xa4b5-0xa4bb]
- Which of the three seeds the wave's draws start from is [OPEN]. The oracle places the last seed before wave 1 about 0.1–0.2 s before the wave's first frame, at the 36 frames per second the dumps show (see below). [ORACLE probe-0001]

## Oracle check
The probe run's raw memory dumps confirm the generator and the seed source. Stepping the LCG backwards from DGROUP:0x144c reaches a state below 0x10000, a possible `srand` value, in every dump. [ORACLE probe-0001]

| Dump | DGROUP:0x144c | 16-bit seed | Draws since the seed |
|---|---|---|---|
| title | 0x25da5ef6 | 0x3375 | 891 |
| navigation | 0x25da5ef6 | 0x3375 | 891 |
| wave | 0xe43e04a1 | 0xb411 | 2512 |
| move0 | 0x1dae3fcb | 0xb411 | 2522 |
| move1 | 0x732657ca | 0xb411 | 2627 |
| move2 | 0x507159d6 | 0xb411 | 2799 |

- The four wave dumps lie on one orbit. Stepping forward from the wave value reaches the move0 value after exactly 10 draws, the move1 value after 105 more and the move2 value after 172 more. [ORACLE probe-0001]
- The wave dump's `GetCurrentTime` sample (DGROUP:0x1e50) is 0x4799d4d2. The last tick before it with low word 0xb411 is 0x4799b411, 8385 ms earlier. The dump came 8.5 s after ESC on the briefing, with DGROUP:0x1e68 at 298 wave frames, which is 8.28 s at 36 frames per second. So the seed is the clock's low word at the start of the wave. [ORACLE probe-0001]
- The title dump's seed tick lies 26.8 s before the dump, which was taken 28 s after launch. The title and Navigation dumps hold the same state, so those screens drew no random numbers between the two dumps. [ORACLE probe-0001]

## Callers and range mapping
- INSPACE has 382 call sites of `rand()` in 101 functions. By segment there are 107 in seg9, 41 in seg12, 39 in seg6, 28 each in seg4 and seg8, 24 in seg15, 22 in seg18, 21 in seg14, 19 in seg11, 16 in seg13, 14 in seg7, 13 in seg10, 8 in seg16, and one each in seg2 and seg3. [BYTES seg5:0xdc]
- The sound wrappers seg10:0x754a and seg10:0x7944 draw none. So the sound settings do not change INSPACE's sequence. [BYTES seg10:0x754a] [BYTES seg10:0x7944]
- The results are used in these ways:

  | Use | Sites | Code |
  |---|---|---|
  | `rand() % n` with a constant n, right after the call | 270 | `mov cx, n; cwd; idiv cx`, remainder in DX |
  | Bit tests | 16 | `test al, 1`, `test al, 3`, `test al, 7` or `and ax, 1` |
  | Other forms | 96 | Mostly `rand() % n` with n a global, a field or a computed value (`cwd; idiv`). Seven sites take a constant modulus after an intervening `les`: seg4:0x847e (% 4000) and seg9:0xe81, seg9:0xe99, seg10:0x8f8, seg10:0x910, seg10:0xab1 and seg10:0xac9 (% 50). Two sites go through RTL 32-bit helpers (seg9:0x4c65, seg9:0x4caf) |

  [BYTES seg4:0x1267] [BYTES seg2:0xdc0] [BYTES seg4:0x1293] [BYTES seg12:0x60d3] [BYTES seg4:0x847e] [BYTES seg9:0xe81] [BYTES seg9:0x4c65]
  - In the 270 sites of the first row, the moduli range from 1 to 26000. The most common are 3 (37 sites), 2 (28), 100 (18), 4 and 6 (14 each) and 10 (13). [BYTES seg4:0x1267]
  - `rand()` lies in 0–32767 and `cwd` sets DX to 0, so the signed `idiv` yields the plain remainder `r % n` for every positive n. [BYTES seg5:0x105]
- The text formatter seg10:0xa332 draws one number for its `R` directive (0x52). `R` reads two arguments n and base, computes `rand() % n + base` and loads that string id from the language DLL (DGROUP:0x9212) with `LoadString`. So `R` picks one of n consecutive strings. Its callers include the "RESOURCE ALERT" message (seg7:0x62) and messages 0x5b, 0x5e and 0x70, whose strings 687, 692 and 1064 pick one of 688 … 691 ([core.md](core.md#rand-sites)). Which other strings use `R` is [OPEN]. [BYTES seg10:0xa406] [BYTES seg10:0xa7c4] [BYTES seg10:0xa7e2] [BYTES seg10:0xa7f3]
- Wave sites whose draws depend on game state ([world.md](world.md)):

  | Site | Draws | Evidence |
  |---|---|---|
  | seg9:0x94ba | one `rand()` per icon slot (8 slots) at seg9:0x952c, only when DGROUP:0x9320 = 2 and DGROUP:0x9334 ≠ 0; a result ≡ 0 mod 3 puts the virus overlay on that icon. Its callee seg7:0x2512 calls seg7:0x65aa(0, i), whose only path to `rand()` needs the ship pool DGROUP:0x9324, so it draws none for pool 0 | [BYTES seg9:0x94ba seg9:0x952c seg9:0x951e-0x9545 seg7:0x2512 seg7:0x65aa] |
  | seg9:0x3900 | one `rand()` % 6 in the wave generator only while DGROUP:0x9332 < 2 (signed); a result below 4 lowers DGROUP:0x9336 by 1. So a different violence class shifts the generator's later draws | [BYTES seg9:0x3900 seg9:0x3905 seg9:0x3907 seg9:0x3912 seg9:0x3915 seg9:0x3917] |
  | seg7:0x342e, seg8:0x9694 | 25 `rand()` at frame 1 of every type-6 (break-up) and type-7 (spark, defender burst) effect entry, in step 7, one per piece with i outer and j inner; seg8:0x9694 takes each % 32 and then % 8 ([world.md](world.md#effect-frame-seg80x3fbe), Pieces) | [BYTES seg7:0x3730 seg8:0x9998-0x99ae seg8:0x425a-0x4289] |
  | seg7:0x5b68, the smoke start | at `Detail` 0 … 2 one `rand()` % 100 first; then, only after taking a free record and for a pool other than the ships, `rand()` % `extent` for x, the same for y and `rand()` % 16 for the heading. A ship's puff draws only the `Detail` one. seg7:0x5d6a draws its `Detail` one the same way ([world.md](world.md#smoke-seg70x5b68-and-seg70x5e04)) | [BYTES seg7:0x5b8e-0x5ba5 seg7:0x5bc5-0x5c1b seg7:0x5ca3-0x5d59 seg7:0x5d8e-0x5da5] |
  | seg7:0x4d86, the icon melt and burn | four `rand()` per call of a melt and five per call of a burn, the fifth a bit test, before the burn's own seg7:0x5b68 draws ([core.md](core.md#icon-effects)) | [BYTES seg7:0x4da7-0x4e0d seg7:0x4ed4-0x4ee0 seg7:0x4fd0-0x4fef] |
  | seg8:0x4e14 | one `rand()` at the end of every visibility pass. When `rand()` % 9 = 0, each listed object with byte +0x50 ≠ 0 outside pool DGROUP:0x9324 calls seg11:0x9fdc(pool, rec) once, provided another listed object j ≠ i has `state` 0; the strike behind it reseeds with srand(GetCurrentTime()) ([core.md](core.md#lightning-seg110x9fdc-seg110xa234-seg110xa9ea)) | [BYTES seg8:0x4ffb seg8:0x5006 seg8:0x5012-0x5058 seg8:0x5060-0x509d seg8:0x50a0] |
  | seg12:0x3148, the pool-10 mover | two `rand()` % 128 per in-flight hook per pass, r1 (blue = 128 + r1) then r2 (red), on or off screen ([core.md](core.md#the-tether-weapons-seg110x947a)) | [BYTES seg12:0x3265 seg12:0x3273] |
  | seg12:0x64aa, fire from a missile | one `rand()` % 3 per qualifying pass (age ≥ 13, age & 3 = 0) of a W+0x16 = 2 missile (FIRE DEMONS), after the steer's two of the same pass | [BYTES seg12:0x64f4 seg12:0x64b9-0x64cc] |
  | seg11:0xa4a6, seg11:0xa234 | the bolt's point draws after its srand; a virus source adds one for its amount after them | [BYTES seg11:0xa4b5-0xa4bb seg11:0xa37d] |
  | seg12:0x532c, the virus mover | `rand()` % 100 per target of pools other than 0 and 6 when seg11:0xa234's strike is considered | [BYTES seg12:0x55f5-0x5604 seg12:0x560b-0x5629] |
  | seg11:0x9078, WILDCARD | one `rand()` % 3 per fruit rebuild, on the first conversion of a call that finds no live fruit | [BYTES seg11:0x91ac-0x91c8] |
  | seg11:0x7c12, weapon 22 | one `rand()` at seg11:0x80c5 and, with a free particle slot, one at seg6:0x8569 | [BYTES seg11:0x80c5 seg6:0x8569] |
  | seg13:0x0, damage block | `rand()` % (2w + 1) for a ship whose word +0x2c < 50 under byte +0xe8 or +0xea, on frames picked by the DGROUP:0x134e cycle with the `Detail`-dependent divisors 10 − max(2·`Detail`, 5) and 18 − 4·`Detail` (`Detail` = DGROUP:0x9208) | [BYTES seg13:0x3e3-0x4d8 seg13:0x449 seg13:0x47b] |
  | seg13:0x0, tail smoke | `rand()` % (ship word +0x26 + 30), below 2 calling seg7:0x5b68(DGROUP:0x9324, i, 1), only for a ship on screen (+0x1a ≠ 0) with +0x26 < 50, and not for ship DGROUP:0x952e while DGROUP:0x9e49 = 4. core-0006's PUFF counts, 63 and 23 against 63.3 and 21.6 expected at 36 frames per second, fit one draw per frame [ORACLE core-0006] | [BYTES seg13:0x68d-0x6e2] |
  | seg8:0x0, shake | under DGROUP:0x238c ≠ 0: seg10:0x744c(0x69), then seg10:0x754a(8, `rand()` % DGROUP:0x90ea, `rand()` % DGROUP:0x90ec, 4) with the DGROUP:0x90ec draw first, then `rand()` % 3 − 2 for x and then for y (−2 … 0) | [BYTES seg8:0x251-0x2a3] |
  | seg10:0x9a2a, reload path | the Size rescale draws only through seg9:0x94ba: 8 `rand()` (one per icon k = 0 … 7 at seg9:0x952c, ascending) in a kind-2 wave with DGROUP:0x9334 ≠ 0; every other callee it reaches is excluded by DGROUP:0x92e4 = 1 or by the pool ([shell.md](shell.md#rescale-seg100x9a2a)) | [BYTES seg9:0x94e2-0x9545 seg9:0x952c seg7:0x23ea seg7:0x24a6] |
  | seg9:0x22b4, the load | for a DEMO or PLAY command line (DGROUP:0x90f8 ≠ 0, DGROUP:0x8e5e ≠ 0) with INI `DScale` = 0 the Size pick draws one `rand()` (a bit test) and, when that is odd or Size = 2, a second one, % 3 ([shell.md](shell.md#size-entry-points)) | [BYTES seg9:0x2345-0x23e4] |
  | the launched weapon's contact | seg7:0x2e02 (outside pool 1) and seg12:0x129e draw none; seg8:0x5ab0 and seg7:0x2652 call none directly but draw through seg8:0x5df2 (seg8:0x5f9a: `rand()` % 12 + 30 against the amount for a target with `owner_pool` ≠ −1, and its callees) and through `on_destroy` seg12:0x274a (per fragment, a full effect table draws none) | [BYTES seg7:0x2e02-0x3099 seg8:0x5ab0-0x5dee seg12:0x129e-0x1774 seg7:0x2652-0x26c3 seg8:0x5f9a seg12:0x2a4b-0x2a64] |

## ISPACE.DLL and FACTORY.EXE
- ISPACE.DLL has the same pair: `srand` at ISPACE.DLL seg3:0x650 and `rand` at seg3:0x668, with the dword state at ISPACE.DLL DGROUP:0xb8c (initialised to 1). It uses the multiply helper seg3:0xfe. [BYTES ISPACE.DLL seg3:0x650] [BYTES ISPACE.DLL seg3:0x66d] [BYTES ISPACE.DLL DGROUP:0xb8c]
  - It seeds from `GetCurrentTime` in ISPACE.DLL seg10:0x492, seg10:0x4dba and seg10:0x5b24. The last of these is ordinal 10 command 9 and a subroutine of ordinal 24. [BYTES ISPACE.DLL seg10:0x4a6] [BYTES ISPACE.DLL seg10:0x4ddb] [BYTES ISPACE.DLL seg10:0x5b3f]
  - It has 20 `rand()` call sites: 2 in ordinal 9, which picks one of three sound variants (ISPACE.DLL seg9:0xcca, seg9:0xd0e), 10 in seg10:0x492, 5 in seg10:0x5b24, 1 in seg10:0x4dba, and one each in seg10:0x4b1a and seg10:0x5266. The last two are subroutines of ordinal 24 that do not seed. The purpose of the seg10 routines is [OPEN]. [BYTES ISPACE.DLL seg9:0xcca] [BYTES ISPACE.DLL seg10:0x4ac] [BYTES ISPACE.DLL seg10:0x5b45] [BYTES ISPACE.DLL seg10:0x4deb] [BYTES ISPACE.DLL seg10:0x4bcb] [BYTES ISPACE.DLL seg10:0x52de]
  - It never touches INSPACE's state. In all six probe dumps, with sound off, ISPACE.DLL DGROUP:0xb8c still holds 1. [ORACLE probe-0001]
- FACTORY.EXE (game) and theworks/FACTORY.EXE each carry the pair at seg3:0xc4 and seg3:0xdc. The state is at FACTORY.EXE DGROUP:0x72c in the game build and at theworks/FACTORY.EXE DGROUP:0x9d2. [BYTES FACTORY.EXE seg3:0xe1] [BYTES FACTORY.EXE seg3:0xcc] [BYTES theworks/FACTORY.EXE seg3:0xe1] [BYTES theworks/FACTORY.EXE seg3:0xcc]

## Other generators
- No segment of the four modules holds 0x015A4E35 (Borland), 0x41C64E6D (ANSI C example) or 0x000343FD as a 32-bit immediate. No `mov r16, imm16` loads 0x4E35, 0x41C6, 0x4E6D or 0x015A, and the only one that loads 0x43FD is the one in each `rand()` copy above. [BYTES seg5:0xe1] [BYTES ISPACE.DLL seg3:0x66d] [BYTES FACTORY.EXE seg3:0xe1] [BYTES theworks/FACTORY.EXE seg3:0xe1]
  - 0x015A occurs hundreds of times as an operand in INSPACE, as the stride of its 0x15a-byte records. [BYTES seg7:0xb12]
  - The byte patterns that look like `mov r16, 0x015A` at INSPACE seg4:0x73b5, seg4:0x7409, seg4:0x7433, seg10:0xa270, seg15:0x55b6 and seg16:0x7652 decode as parts of `imul bx, [bp − 0x42], 0x15a` and `cmp word ptr es:[bx + 0x15a], 0`. [BYTES seg4:0x73b3] [BYTES seg4:0x7407] [BYTES seg4:0x7431] [BYTES seg10:0xa26e] [BYTES seg15:0x55b4] [BYTES seg16:0x7650]
- No `GetCurrentTime` or `timeGetTime` result, and no read of DGROUP:0x1e50, feeds a modulo or mask within the next three instructions. So the game does not use the clock as a random source. [BYTES seg7:0x1f]
- Whether a table-driven generator exists is [OPEN].

## Reproducing sequences
To reproduce a run bit for bit, the port needs three things. [DESIGN]
- The same `srand` calls at the same three points, with the same 16-bit seeds. A test harness injects the seeds, or the `GetCurrentTime` values they come from.
- The same order of draws, call site for call site, including draws on paths that look cosmetic, such as the formatter's `R` directive.
- ISPACE.DLL's generator as a separate instance, which only ISPACE.DLL's own code consumes: ordinal 9's sound variants and the seg10 routines above.
