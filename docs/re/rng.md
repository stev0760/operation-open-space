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

- What seg11:0xa4a6 generates is [OPEN].
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
- The text formatter seg10:0xa332 draws one number for its `R` directive (0x52). `R` reads two arguments n and base, computes `rand() % n + base` and loads that string id from the language DLL (DGROUP:0x9212) with `LoadString`. So `R` picks one of n consecutive strings. Its callers include the "RESOURCE ALERT" message (seg7:0x62). Which strings use `R` is [OPEN]. [BYTES seg10:0xa406] [BYTES seg10:0xa7c4] [BYTES seg10:0xa7e2] [BYTES seg10:0xa7f3]

## ISPACE.DLL and FACTORY.EXE
- ISPACE.DLL has the same pair: `srand` at ISPACE.DLL seg3:0x650 and `rand` at seg3:0x668, with the dword state at ISPACE.DLL DGROUP:0xb8c (initialised to 1). It uses the multiply helper seg3:0xfe. [BYTES ISPACE.DLL seg3:0x650] [BYTES ISPACE.DLL seg3:0x66d] [BYTES ISPACE.DLL DGROUP:0xb8c]
  - It seeds from `GetCurrentTime` in ISPACE.DLL seg10:0x492, seg10:0x4dba and seg10:0x5b24. The last of these is ordinal 10 command 9 and a subroutine of ordinal 24. [BYTES ISPACE.DLL seg10:0x4a6] [BYTES ISPACE.DLL seg10:0x4ddb] [BYTES ISPACE.DLL seg10:0x5b3f]
  - It has 20 `rand()` call sites: 2 in ordinal 9, which picks one of three sound variants (ISPACE.DLL seg9:0xcca, seg9:0xd0e), 10 in seg10:0x492, 5 in seg10:0x5b24, 1 in seg10:0x4dba, and one each in seg10:0x4b1a and seg10:0x5266. The last two are subroutines of ordinal 24 that do not seed. The purpose of the seg10 routines is [OPEN]. [BYTES ISPACE.DLL seg9:0xcca] [BYTES ISPACE.DLL seg10:0x4ac] [BYTES ISPACE.DLL seg10:0x5b45] [BYTES ISPACE.DLL seg10:0x4deb] [BYTES ISPACE.DLL seg10:0x4bcb] [BYTES ISPACE.DLL seg10:0x52de]
  - It never touches INSPACE's state. In all six probe dumps, with sound off, ISPACE.DLL DGROUP:0xb8c still holds 1. [ORACLE probe-0001]
- FACTORY.EXE (shipdisk) and theworks/FACTORY.EXE each carry the pair at seg3:0xc4 and seg3:0xdc. The state is at FACTORY.EXE DGROUP:0x72c in the shipdisk build and at theworks/FACTORY.EXE DGROUP:0x9d2. [BYTES FACTORY.EXE seg3:0xe1] [BYTES FACTORY.EXE seg3:0xcc] [BYTES theworks/FACTORY.EXE seg3:0xe1] [BYTES theworks/FACTORY.EXE seg3:0xcc]

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
