# The wave: set-up, world, flight and drawing

This page covers the wave in state 1: how the load and the briefing entry build the wave from the chosen directory, the world's coordinates, the per-frame update, the player's flight model and input, the view, the sprite drawing, the play-area frame and status display, the IS_INFO displays, the radar scrollbar, the radar and the icons. Addresses without a module name are in INSPACE.EXE, whose DGROUP is seg19. The screen states, the load screen seg10:0x0 and the briefing are in [shell.md](shell.md#screen-states), the frame gate in [timing.md](timing.md#the-frame-gate), the pools and the object and ship records in [structs.md](structs.md), the tick and the game-state globals in [globals.md](globals.md), `rand()` in [rng.md](rng.md), the INI keys in [formats.md](formats.md#ini-files) and the running original in [oracle.md](oracle.md). [DESIGN]

## Notation
- A `/` is a signed division that truncates toward zero: `idiv` for 16-bit operands, the RTL long division seg5:0x110 for 32-bit ones. `>>` is an arithmetic shift (`sar`). `lo16(a·b)` is a 16-bit `imul` whose high word is dropped, as when `cwd` follows it. The translation rules are in [win16-translation.md](win16-translation.md). [BYTES seg5:0x110 seg11:0x66e6 seg11:0x60ec]
- Symbols used below:

  | Symbol | Global | Meaning | Evidence |
  |---|---|---|---|
  | W | DGROUP:0x90ea | play width in pixels; 1024 at 1024×768 | [BYTES seg11:0xb77 seg8:0x0] [ORACLE newgame-0002] |
  | H | DGROUP:0x90ec | play height: client height − 85 | [BYTES seg8:0xb76] |
  | H' | DGROUP:0x8e5a | top of the HUD strip: DGROUP:0x10 − 85, 683 at 1024×768 | [BYTES seg2:0xc4e] |
  | s | DGROUP:0x9150 | 4 − `Size` (DGROUP:0x9156); one pixel is `1 << s` world units | [BYTES seg9:0x2408 seg6:0xafd seg6:0xb07] |
  | player | pool DGROUP:0x9324, record DGROUP:0x9646 | the player's ship in the ship pool ([structs.md](structs.md#pool-roles)) | [BYTES seg8:0x14 seg8:0x1b] |
  | now | DGROUP:0x1e50 | the `GetCurrentTime` sample of the pass ([timing.md](timing.md#per-pass-work-in-seg70x0)) | [BYTES seg7:0x1f] |

## Wave set-up

### Order
1. **Load from the state change.** A state-1 request runs seg2:0x1ac8, which calls seg10:0x0 ([shell.md](shell.md#game-start-dialogs), item 13, Load screen). Its load seg9:0x22b4 at seg10:0xad runs the wave generator seg9:0x3202 once. [BYTES seg2:0x1b71 seg10:0xad seg9:0x2495]
2. **Briefing entry.** seg2:0x1b96 → seg9:0xc06 → seg9:0x5cca, or the debriefing timer DGROUP:0x1e70 (seg7:0x38f → seg9:0x5cca), enters the briefing. seg9:0x5cca runs the load seg9:0x22b4 at seg9:0x5d3c and the icon fill seg9:0xb2a at seg9:0x5d66 ([Programs and icons](#programs-and-icons)). [BYTES seg9:0x5d3c seg9:0x5d66 seg7:0x38f]
   - **New-game reset.** seg9:0xc06 sets DGROUP:0x992 = 0. When DGROUP:0x92e4 = 0 it also clears the score DGROUP:0x1e58 and the rating DGROUP:0x5078 (dwords), DGROUP:0x91f8, DGROUP:0x967c, DGROUP:0x94ca, DGROUP:0x93ac and DGROUP:0xa1cd, sets DGROUP:0x9346 and all 8 × 3 words of DGROUP:0x4eec to −1, DGROUP:0x9432 to 100, DGROUP:0xa1cf to 1 and the four bytes DGROUP:0xa193 … DGROUP:0xa196 to 1. It then always sets the words DGROUP:0x986 … DGROUP:0x98e to 1 and DGROUP:0x990 to 2. [BYTES seg9:0xc3e seg9:0xc44 seg9:0xc4e seg9:0xc52 seg9:0xc56 seg9:0xc7f seg9:0xc97 seg9:0xc9d seg9:0xca5 seg9:0xcb0 seg9:0xcc8 seg9:0xcd7]
3. **Generator runs.** A wave started through a state-1 request runs the generator twice: seg10:0xad, then seg9:0x5d3c. A wave started by the debriefing timer runs it once. Each run draws fresh `rand()` values, and only the last run's counts reach the placement seg9:0x6624. [BYTES seg9:0x2495 seg10:0xad seg9:0x5d3c seg9:0x5e5d]
   - The first run's writes to persistent state remain in effect for the second: DGROUP:0x98a, DGROUP:0x98c and DGROUP:0x98e cleared, DGROUP:0x990 decremented, DGROUP:0x9334 changed, the bytes DGROUP:0x1822 + k set, and the directory-record bytes +0xa0 … +0xac written. [BYTES seg9:0x2495 seg10:0xad seg9:0x5d3c]
   - **DGROUP:0x9346.** It is 0 in the DGROUP image. The [new-game reset](#order) sets it to −1, and before each generator run seg9:0x22b4 calls seg9:0x2828 when DGROUP:0x91f8 = 0, which sets it to the player record's +0xd6 when a ship i has byte +0xec ≠ 0, kind 7, byte +0x5e = 0, +0x54 = 14 and +0x56 = DGROUP:0x9646. In the first new game after start-up without such a ship, the first run therefore sees 0 and the second −1; a later game starts from what the previous one left. [BYTES seg9:0xc44 seg9:0xc56 seg9:0x241e seg9:0x2426 seg9:0x2a09 seg10:0xad seg9:0x5d3c seg9:0x3292 seg9:0x3299] The probe's navigation dump holds 0 and its wave dump −1. [ORACLE probe-0001]
4. **Placement.** seg9:0x5cca runs the placement seg9:0x6624 at seg9:0x5e5d between an increment and a decrement of the wave number DGROUP:0x91f8 (seg9:0x5e58, seg9:0x5e60). No branch between seg9:0x5d70 and seg9:0x5e5d skips it. [BYTES seg9:0x5e58 seg9:0x5e5d seg9:0x5e60]
   - Before the placement seg9:0x5cca adds 4 to DGROUP:0x992 for every pool-0 record 0 … 7 with `state` 0, and after it seg9:0x5eda adds that count · DGROUP:0x932e · 4. [BYTES seg9:0x5e0d seg9:0x5e23 seg9:0x5e26 seg9:0x5e5d seg9:0x5eda] The probe's wave holds DGROUP:0x992 = 50 with DGROUP:0x932e = 0. [ORACLE probe-0001]
5. **Pool-2 rewrite.** After the placement, seg9:0x5e9c–0x5ecd rewrite DGROUP:0x932e ([Difficulty inputs](#difficulty-inputs)). [BYTES seg9:0x5e9c seg9:0x5ea1 seg9:0x5ecd]
6. **Wave number.** seg9:0x5fbf increments DGROUP:0x91f8 for good. [BYTES seg9:0x5fbf]
7. **Wave start.** The briefing (state 3) ends in seg9:0x60bc, which sets state 1 ([shell.md](shell.md#screen-states)) and places the ships ([Ships at wave start](#ships-at-wave-start)). [BYTES seg9:0x60ff seg9:0x6320]

### Wave number
- DGROUP:0x91f8 holds the number of completed waves during the whole set-up, so it is 0 while wave 1 is built. The placement alone sees it one higher. [BYTES seg9:0x5d3c seg9:0x5d66 seg9:0x5e58 seg9:0x5e5d seg9:0x5e60 seg9:0x5fbf]

### Difficulty inputs
- **Budget.** seg6:0x11ec reads INSPACE.INI `Complex` (DGROUP:0x495, default 0) into DGROUP:0x8ff8 and sets the wave budget DGROUP:0x932a and DGROUP:0x9000. Any other `Complex` value leaves both words unchanged. [BYTES seg6:0x1204 seg6:0x1209 seg6:0x120e seg6:0x1214 seg6:0x121c seg6:0x1224 seg6:0x122a seg6:0x1232 seg6:0x1238 seg6:0x1240 seg6:0x1248 seg6:0x1256]

  | `Complex` | DGROUP:0x932a | DGROUP:0x9000 | Evidence |
  |---|---|---|---|
  | 0 | 130 | 0 | [BYTES seg6:0x1204–0x1256] |
  | 1 | 220 | 0 | [BYTES seg6:0x1204–0x1256] |
  | 2 | 310 | 1 | [BYTES seg6:0x1204–0x1256] |
  | 3 | 420 | 2 | [BYTES seg6:0x1204–0x1256] |
  | 4 | 2000 | 2 | [BYTES seg6:0x1204–0x1256] |

- **Easy first waves.** While DGROUP:0x91f8 < 10, seg6:0x11ec replaces the budget B by `min(B, B/2 + lo16(DGROUP:0x91f8·B)/20)`, all signed 16-bit. Wave 1 gets half the `Complex` budget, and the full budget is reached at DGROUP:0x91f8 = 10. [BYTES seg6:0x1258 seg6:0x125d seg6:0x1262 seg6:0x1265 seg6:0x126c seg6:0x1276 seg6:0x127a seg6:0x1284]
- **DGROUP:0x9432.** The load scales the word DGROUP:0x9432 ([globals.md](globals.md#global-table)) by `lo16(r·9)/10` when it is below 251 and by `lo16(r·7)/10` otherwise, after the generator and before the pool counts. [BYTES seg9:0x249e seg9:0x24a0 seg9:0x24a7 seg9:0x24ac seg9:0x24b0 seg9:0x24b2]
- **seg9:0x2828.** seg9:0x22b4 calls seg9:0x2828 only when DGROUP:0x91f8 = 0. seg9:0x2828 sets DGROUP:0x9332 from DGROUP:0x9432 (signed: below 50 gives 3, below 180 gives 2, below 400 gives 1, else 0). It clears DGROUP:0x932c and sets it to 1 with a chance picked by DGROUP:0x964a: 0 gives 20 %, 1 … 4 give 50 %, 5 … 8 give 80 %, 9 … 15 give 80 % when DGROUP:0x9332 ≥ 2 and no ship's design type is 7 and certainty otherwise, and above 15 certainty. Each percentage case draws one `rand()` % 100. At wave 0 (DGROUP:0x91f8 = 0) it then clears DGROUP:0x932c again. [BYTES seg9:0x241e seg9:0x2426 seg9:0x2a17 seg9:0x2a4c seg9:0x2a52 seg9:0x2a75 seg9:0x2a83 seg9:0x2a98 seg9:0x2aa6 seg9:0x2ac1 seg9:0x2ae3 seg9:0x2b25 seg9:0x2b3d seg9:0x2b4d]
- **DGROUP:0x992.** The generator seg9:0x3202 starts DGROUP:0x992 as follows, all signed 16-bit: [BYTES seg9:0x323f seg9:0x3262 seg9:0x3273 seg9:0x3292 seg9:0x32a4 seg9:0x32c5 seg9:0x32f5 seg9:0x3307 seg9:0x3311]
  1. when the budget B = DGROUP:0x932a is below DGROUP:0x992, DGROUP:0x992 becomes min((DGROUP:0x992 − B)/2, B/4), else 0;
  2. it subtracts (player ship word +0x26)/2 − 50 when that word is non-zero;
  3. it adds 100 when DGROUP:0x9346 ≠ −1;
  4. for a revisited directory (DGROUP:0x94b0 = 0) it adds the directory record's signed bytes +0xa0 … +0xa3 times 1, 1, 3 and 5, and for j = 0, 1 the signed byte +0xa7 + j times (byte DGROUP:0x4d95 + 0x14·(byte +0xab + j) + 3).
  - For a new directory (DGROUP:0x94b0 ≠ 0) it adds, after the draw loop, lo16(w[k]·count[k]) for categories k = 0 … 3, with the local weights w (any weight raised to 100 by the thin pass), and for k = 0, 1 lo16((byte DGROUP:0x4d95 + 0x14·DGROUP:0x4ed4[k] + 3)·DGROUP:0x4ee0[k]) when DGROUP:0x4ee0[k] ≠ 0. DGROUP:0x4d95 is byte +5 of the class table DGROUP:0x4d90. [BYTES seg9:0x3211 seg9:0x40b2 seg9:0x4683 seg9:0x469b]
- **First wave.** For a new game's first wave (DGROUP:0x9334 = 0, DGROUP:0x94b0 ≠ 0, DGROUP:0x91f8 = 0) the generator sets the kind DGROUP:0x9320 = 0, DGROUP:0x986 = 0, DGROUP:0x9336 = 0, DGROUP:0x9338 = 0 and the target T = `rand()` % 20 + 30, and skips the block that depends on DGROUP:0x9334. [BYTES seg9:0x3654 seg9:0x365d seg9:0x3666 seg9:0x366f seg9:0x3675 seg9:0x3688 seg9:0x3809 seg9:0x3947]
- **Target.** Every path then reaches seg9:0x3947. With INI `Complex` DGROUP:0x8ff8 ≥ 3, T becomes lo16(T·Complex)/2. When lo16(B << 2)/5 − 20 < DGROUP:0x992 the kind becomes 0. [BYTES seg9:0x3947 seg9:0x394e seg9:0x395d seg9:0x3972] The opponent adjustments seg9:0x3ce5–0x3d1d need DGROUP:0x9334 ≠ 0. After them T = lo16(lo16((`Detail` + 3)·T)·20)/100. T is 0 for Demon's Gate (DGROUP:0x92e8 ≠ 0), kind 7 or a race (DGROUP:0x94e ≠ 0), and DGROUP:0x9338 is raised to DGROUP:0x994 when that is larger. [BYTES seg9:0x3ce5 seg9:0x3d43 seg9:0x3d58 seg9:0x3d66 seg9:0x3da7 seg9:0x3db6 seg9:0x3dc8]
- **Race chance.** The generator calls seg16:0x5f0e(DGROUP:0x6990, DGROUP:0x8f7a), a case-sensitive ends-with test. When the chosen directory ends with INI `Path` (the program's own directory, "C:\INSPACE\" in the probe dump) in a new directory (DGROUP:0x94b0 ≠ 0) with DGROUP:0x94a6 < 25, the wave becomes a race (kind 6) without the `rand()` % 100 chance roll; seg9:0x3ab6 still draws `rand()` % 3 for the race variant. [BYTES seg9:0x3a05 seg9:0x3a0c seg9:0x3a2b seg9:0x3a42 seg9:0x3a4c seg9:0x3ab6 seg9:0x3ac4 seg16:0x5f0e] [ORACLE probe-0001]
- **Categories.** The generator seg9:0x3202 fills five category counts DGROUP:0x933a, 0x933c, 0x933e, 0x9340 and 0x9342, with weights w = 1, 1, 3, 5 and 5 in its local table at [bp − 0x28]. [BYTES seg9:0x3211 seg9:0x3214 seg9:0x2513 seg9:0x2520 seg9:0x2530]
  - **Draw.** Each step draws c = `rand()` % 5 and rejects it when DGROUP:0x9334 + 2 < w[c]. When DGROUP:0x9334 < w[c] it draws a second `rand()` % 5, even for a c already rejected, and a value below 3 rejects c; DGROUP:0x9334 = 0 accepts c again. c = 0 or 1 is then rejected while DGROUP:0x933a + DGROUP:0x933c > 120, whatever DGROUP:0x9334. [BYTES seg9:0x3efa seg9:0x3f74]
  - **Increments.** An accepted c gains 1. In kinds 1 and 4, c = 0 or 1 gains 2 more and nothing else. c gains 3 more for (kind 2, c = 3), (kind 3, c = 2) and (kind 5, c = 4), and any other c gains 1 more in kind 4. c = 2, 3 or 4 loses 1 in kind 0, and c = 3 loses 1 when the kind is neither 2 nor −1 and DGROUP:0x9334 < 3. The compares are signed. [BYTES seg9:0x3efa seg9:0x3fcb]
  - **Stop.** The loop runs while T > S = Σ lo16(w[i]·count[i]). It ends early, by setting S = T + 1, when (3·DGROUP:0x932a)/4 < (2·DGROUP:0x9338 + DGROUP:0x994)·15 + S + DGROUP:0x992. [BYTES seg9:0x402e seg9:0x4046 seg9:0x4068 seg9:0x4079]
  - **Thin pass.** After the loop, every category k other than 1 with 0 < count[k] < min(T/12, 5) gets w[k] = 100 and count 0, and the draw loop runs again without resetting the other counts. Loop and pass repeat until a pass thins nothing. [BYTES seg9:0x3eaf seg9:0x407d seg9:0x40b2]
  - Categories 0 and 1 feed pool 3 (small and big rocks), categories 2 and 4 pool 8, and category 3 pool 9. [BYTES seg9:0x2513 seg9:0x2520 seg9:0x2530]
  - A revisited directory outside a race copies the counts from its record's bytes +0xa0 … +0xa4 instead of drawing them. [BYTES seg9:0x3214]
- **Gate count.** DGROUP:0x9318 starts at 1. In a short-circuit chain it gains 1 when DGROUP:0x91f8 > 1, `rand()` is odd, DGROUP:0x92e8 = 0 and DGROUP:0x9320 is neither 6 nor 7; 1 more when also DGROUP:0x91f8 > 3 and `rand()` % 10 = 1; and 1 more when also DGROUP:0x91f8 > 7 and `rand()` % 5 = 1. [BYTES seg9:0x4496 seg9:0x449c seg9:0x44a5 seg9:0x44ae seg9:0x44b5 seg9:0x44bc seg9:0x44c3 seg9:0x44c7 seg9:0x44ce seg9:0x44dc seg9:0x44e0 seg9:0x44e7 seg9:0x44f8]
  - A revisited directory that is not a race (DGROUP:0x94b0 = 0 and DGROUP:0x94e = 0) instead takes the signed byte +0xad of its 0x297-byte record, at far DGROUP:0x69f4 + DGROUP:0x94a2·0x297 ([formats.md](formats.md#directory-records-0x297-bytes)). [BYTES seg9:0x4680]
- **Pool-4 count.** DGROUP:0x940e, copied to DGROUP:0x92f4, is `rand()` % ((DGROUP:0x9338 + 1)·4), raised to at least 3. DGROUP:0x9320 = 4 adds `rand()` % 15 + 9. A further `rand()` % 25 = 4 with `Detail` DGROUP:0x9208 > 1 replaces it by 17; DGROUP:0x92e8 ≠ 0 replaces it by 1; and a race (DGROUP:0x9320 = 6) by the byte DGROUP:0x9ebf + DGROUP:0x9322. [BYTES seg9:0x3e36 seg9:0x3e3f seg9:0x3e5a seg9:0x3e78 seg9:0x3e80 seg9:0x3e9f seg9:0x3ea2]
- **Pool-2 units.** DGROUP:0x932e (pool 2 holds 8 records per unit) is 0 when DGROUP:0x91f8 < 1, and otherwise `rand()` % 3 + DGROUP:0x9334/2 − 1, floored at 0 and capped at 4 when `Detail` ≥ 3, 3 when `Detail` = 2 and 2 when `Detail` < 2. It is 0 for Demon's Gate (DGROUP:0x92e8) and in a race. seg9:0x5e9c–0x5ecd rewrite it after the placement. [BYTES seg9:0x46f6 seg9:0x4716 seg9:0x471a seg9:0x4721 seg9:0x472e seg9:0x4735 seg9:0x4744 seg9:0x474b seg9:0x475a seg9:0x4761 seg9:0x4775 seg9:0x5e9c seg9:0x5ea1 seg9:0x5ecd]

### Pool counts
seg9:0x22b4 sets the allocated counts (DGROUP:0x92ec + 2p) and the in-use counts (DGROUP:0x9406 + 2p, [structs.md](structs.md#pool-table)) after the generator. [BYTES seg9:0x24b5 seg9:0x24bd seg9:0x24c0 seg9:0x24c9 seg9:0x24cd seg9:0x24d3 seg9:0x24e3 seg9:0x24e7 seg9:0x24ea seg9:0x24ed seg9:0x24f0 seg9:0x24f3 seg9:0x24f6 seg9:0x24fd seg9:0x2501 seg9:0x2508 seg9:0x250c seg9:0x2510]

| Pool | Allocated | In use | Evidence |
|---|---|---|---|
| 0 (icons) | 8 | 8 | [BYTES seg9:0x24b5–0x2510] |
| 1 | 2 | 0 | [BYTES seg9:0x24b5–0x2510] |
| 2 | 8 · DGROUP:0x932e | 0 | [BYTES seg9:0x24b5–0x2510] |
| 3 (rocks) | DGROUP:0x933c + DGROUP:0x933a | the same | [BYTES seg9:0x2513 seg9:0x2516 seg9:0x251a seg9:0x251d] |
| 4 | DGROUP:0x92f4 = DGROUP:0x940e | DGROUP:0x940e | [BYTES seg9:0x3ea2] |
| 5 (DGROUP:0x9328) | 0 | 0 | [BYTES seg9:0x24b5–0x2510] |
| 6, 7 | 1 | 0 | [BYTES seg9:0x24b5–0x2510] |
| 8 | DGROUP:0x933e + DGROUP:0x9342 + 2·DGROUP:0x9318, capped at 80 | the same | [BYTES seg9:0x2520 seg9:0x2524 seg9:0x2528 seg9:0x252c seg9:0x2563 seg9:0x256d seg9:0x2571 seg9:0x257b seg9:0x257e] |
| 9 | DGROUP:0x9340 | DGROUP:0x9340 | [BYTES seg9:0x2530 seg9:0x2534 seg9:0x2538] |
| 10, 11 | 3 | 0 | [BYTES seg9:0x24b5–0x2510] |
| 12 (gate centres) | 2·DGROUP:0x9318 | 2·DGROUP:0x9318 | [BYTES seg9:0x2567 seg9:0x256a] |
| 13 | 0 | 0 | [BYTES seg9:0x24b5–0x2510] |

- **Small and big rocks.** DGROUP:0x92ea is first set to 14 and then to DGROUP:0x933a, the small-rock count. [BYTES seg9:0x24d3 seg9:0x2540]
- **Rock clamp.** When DGROUP:0x933c + DGROUP:0x933a exceeds 127, the in-use count becomes 120, the allocated count 920 (0x398), and DGROUP:0x933a and DGROUP:0x933c both become 60, while DGROUP:0x92ea keeps the unclamped DGROUP:0x933a. [BYTES seg9:0x253c seg9:0x2540 seg9:0x2544 seg9:0x2549 seg9:0x254f seg9:0x2555 seg9:0x2558 seg9:0x255b]
- **No rocks.** Demon's Gate (DGROUP:0x92e8 ≠ 0, seg9:0x2590) and a race (DGROUP:0x9320 = 6, seg9:0x25a3) then set the pool-3 in-use count to 0. [BYTES seg9:0x2590 seg9:0x25a3]
- The ship pool's counts come from seg14:0x0 ([structs.md](structs.md#allocation)). [BYTES seg14:0xa23]

### Record templates
seg10:0xb94 fills the per-pool constants after the counts. The radar fields per pool are in [Radar colours](#radar-colours). [BYTES seg10:0xb94]
- **Icons (pool 0).** +0x22 = `(32 << Size) >> 1`, +0x20 = 256, +0x24 = `(6 << Size) >> 1`, +0x28 = the record index, +0x44 = 160, `hit_sound` 0x12, `radar_brush` 2, `radar_draw` seg11:0x39d8, `on_damage` seg7:0x233c and byte +0x53 = 1. [BYTES seg10:0xb94 seg10:0xbe5 seg10:0xc34]
- **Rocks (pool 3).** The first DGROUP:0x92f2 − DGROUP:0x92ea records are big rocks: +0x22 = `(44 << Size) >> 1`, +0x20 = 344, +0x44 = 1400, `max_damage` 60, `radar_draw` seg11:0x3bae. The rest are small rocks: +0x22 = `(22 << Size) >> 1`, +0x20 = 168, +0x44 = 350, `max_damage` 200, `radar_draw` seg11:0x3914. [BYTES seg10:0xb94]
  - All rocks get +0x24 = `(4 << Size) >> 1`, `blast_size` 9, `hit_sound` 8, `radar_brush` 4, `radar_color` 0x00C0C0C0 and `on_damage` seg7:0x5090. Small rocks' +0x24 is set to 4, 2 or 1 by `Size` and DGROUP:0x740. [BYTES seg10:0xb94]
- **Extent and side.** Word +0x20 is the object's extent in world units, about 8 × its `Size`-1 sprite side: icons 256 for 32 px, pool 1 160 for 20 px and gate posts 272 for 34 px, but rocks 344 (8·43) for 44 px and 168 (8·21) for 22 px, pool 9 296 for 38 px and pool 2 104 for 14 px. +0x22 is the on-screen side in pixels, `(side << Size) >> 1`. [BYTES seg10:0xb94 seg9:0x6264 seg9:0x6268 seg9:0x6271 seg9:0x6276 seg9:0x10d4]

### Placement (seg9:0x6624)
1. A latch DGROUP:0x93b0, cleared only at seg9:0x5d24, makes it run once per wave. The five calls from seg17 are therefore no-ops after seg9:0x5e5d has run in the same wave. [BYTES seg9:0x6633 seg9:0x663c seg9:0x5d24]
2. It sets the 9 beam slots to DGROUP:0x610e + 0x94k = −1 and DGROUP:0x610c + 0x94k = 50, and the 45 effect entries ([structs.md](structs.md#effect-table-45--0x14-bytes-at-dgroup0x1e90)) and the 20 entries at DGROUP:0x58cc to −1. [BYTES seg9:0x6624]
3. It places the rocks with seg10:0x864 (seg9:0x6807), pool 9 with seg10:0xa26 (seg9:0x6822), pool 4 with seg9:0xdf6 (seg9:0x6843), and the gates and hazards with seg9:0x10d4 (seg9:0x6854). [BYTES seg9:0x6807 seg9:0x6822 seg9:0x6843 seg9:0x6854]
4. It starts the wave clock DGROUP:0x1e50 with `GetCurrentTime` (seg9:0x6859). [BYTES seg9:0x6859]
5. It runs the ship set-up: seg14:0x3eda, seg14:0x0(−1) and seg14:0x4074. [BYTES seg9:0x685e seg9:0x6883 seg9:0x688a seg9:0x6890]
6. For every allocated record of pools 0–14 it then sets +0x12 = +0x24, byte +0x2e = 0 and +0x32 = +0x36 = −1. [BYTES seg9:0x6624]

### Random positions
- **Centre exclusion.** Every random placement of rocks (seg10:0x864), pool 9 (seg10:0xa26), pool 4 (seg9:0xdf6) and icons (seg10:0x736) redraws the whole record while `PtInRect(((1600 − W/2) << s, (1600 − H/2) << s, (1600 + W/2) << s, (1600 + H/2) << s), (x, y))` holds, all signed 16-bit. [BYTES seg10:0x998 seg10:0x99d seg10:0xa26 seg9:0xdf6 seg10:0x736]
  - The excluded screen-sized area is centred on (1600 << s, 1600 << s): the world centre (12800, 12800) at `Size` 1, (6400, 6400) at `Size` 2 and (25600, 25600) at `Size` 0, where the right and bottom edges overflow 16 bits once W/2 or H/2 exceeds 447. [BYTES seg10:0x998 seg10:0x99d]

### Rocks
- seg10:0x864(i) places rock i for i < the pool-3 in-use count. It draws, in this `rand()` order: [BYTES seg10:0x8e4 seg10:0x8f8 seg10:0x901 seg10:0x907 seg10:0x90a seg10:0x90c seg10:0x910 seg10:0x924 seg10:0x930 seg10:0x936 seg10:0x943 seg10:0x952 seg10:0x95f seg10:0x969 seg10:0x972 seg10:0x97d seg10:0x981 seg10:0x985 seg10:0x98a seg10:0x994]
  1. x = (`rand()` % 3050 + 150)·8;
  2. y the same with DGROUP:0x5a2;
  3. vx = 25 − `rand()` % 50;
  4. vy the same;
  5. +0x26 = `rand()` % 1700.
- When |vx| < 10 and |vy| < 10 it adds 20 to vx (seg9:0x88ba is `abs`). It sets `state` 0, +0x00, +0x02, +0x08 and +0x0a to −1000, bytes +0x2e and +0x2f to 0 and dword +0x6a to 0. [BYTES seg10:0x952 seg10:0x95f seg10:0x969 seg10:0x972 seg10:0x97d seg10:0x981]

### Pool 9 and pool 4
- seg10:0xa26 places pool-9 records with the rock draws and +0x26 = `rand()` % 900. [BYTES seg10:0xa26]
- seg9:0xdf6 places pool-4 records with the same x, y, vx and vy draws but without the |v| < 10 boost. [BYTES seg9:0xdf6]
  - +0x26 = 0, or 5 for odd records when DGROUP:0x9214 is 3 or 4. [BYTES seg9:0xdf6]
  - `on_destroy` is seg7:0x2e02 when DGROUP:0x9214 is 1 or 2, or 4 with an even index, and seg12:0x274a otherwise. [BYTES seg9:0xdf6]
  - +0x1e and bytes +0x2f, +0x50 and +0x51 are 0. `state` is 0, or 1 in a race. [BYTES seg9:0xdf6]
  - When DGROUP:0x940e = 17, placing record 16 zeroes the velocity of records 0–16 and sets record k (1–16) to y = (word DGROUP:0x104c + 4k · 208 + y0) % 25600 and x = (word DGROUP:0x104e + 4k · 208 + x0) % 25600, where (x0, y0) is record 0. The remainder is signed, so a negative sum stays negative. [BYTES seg9:0xdf6 seg9:0xfe1]

### Gates
seg9:0x10d4 places the gates and the hazards. [BYTES seg9:0x10d4]
- **Posts.** Pool-8 records 0 … 2·DGROUP:0x9318 − 1 are pairs of posts. Post 2p sits at the gate position (x, y) and post 2p + 1 at (x + 272 + 1200, y): the post's +0x20 plus the width word DGROUP:0x6682 + 0x1c·p, which is set to 1200. [BYTES seg9:0x10d4]
- **Post fields.** Both posts get +0x22 = `(34 << Size) >> 1`, +0x20 = 272, +0x24 = `(6 << Size) >> 1`, +0x44 = 3500, `hit_sound` 8, `radar_brush` 1, `radar_draw` seg11:0x39d8, `on_damage` seg7:0x5090, `state` 0, byte +0x2f = 2, byte +0x54 = 1, word DGROUP:0x3d80 + 0x34·k = −1 and kind word DGROUP:0x3d6c + 0x34·k = 3. [BYTES seg9:0x10d4 seg9:0x11e7 seg9:0x1218]
- **Position in a new directory.** With DGROUP:0x94b0 ≠ 0 each post iteration first draws two unused values (`rand()` % 3140 + 30)·8. For gate p outside a race the position is then x = (`rand()` % 2900) << 3 and y = (`rand()` % 3000 + 200)·8, x drawn first. It is redrawn until the rectangle (x − 720, y − 720, x + 2400, y + 1200) meets no rectangle (ox − 160, oy − 160, ox + 320, oy + 320) of any in-use record of pools 0–13. [BYTES seg9:0x10d4]
- **Revisited directory and race.** A revisited directory reuses the dword at directory record + 8p. A race takes x = word DGROUP:0xa120 + 4·DGROUP:0x9322 − 760 and y = word DGROUP:0xa122 + 4·DGROUP:0x9322. In those two cases every in-use object of pools 0, 3, 4 and 9 whose rectangle overlaps the gate rectangle gets y = (y + 1600) % 25600, signed. [BYTES seg9:0x10d4]
- **Beam and centres.** Each gate p sets its beam rectangle DGROUP:0x6678 + 0x1c·p = (x + 272, y + 136 − 24, x + 272 + 1200, y + 136 + 24) and DGROUP:0x6688 + 0x1c·p = 2p. The word DGROUP:0x668a + 0x1c·p becomes 1 for p = 1 and 2 for p = 2. [BYTES seg9:0x10d4]
- **Pool-12 centres.** Record 2p goes to (x + 760, y − 150) and record 2p + 1 to (x + 760, y). Record 2p gets +0x20, +0x22 and +0x44 = 0, and record 2p + 1 gets +0x22 and +0x44 = 0. [BYTES seg9:0x10d4]

### Hazards
- Pool-8 records 2·DGROUP:0x9318 … in-use − 1 are hazards. Their behaviour belongs to M5. [BYTES seg9:0x10d4]
- **Position.** Outside special-place ranges, a new directory outside a race draws ((`rand()` % 3200) << 3, (`rand()` % 3200) << 3) until the point is, by seg8:0x285a in 1/8-unit cells, at least 150 cells (1200 units) from every pool-12 gate centre record 2p, at least (DGROUP:0x4f30 entry >> 3) + 200 cells from every special place DGROUP:0x4f36, and at least 80 cells (640 units) from every earlier pool-8 record. [BYTES seg9:0x10d4]
- **Templates.** Records below 2·DGROUP:0x9318 + DGROUP:0x933e take fixed templates. The rest copy the class table DGROUP:0x4d90 entry DGROUP:0x4ed4[0 or 1]. Classes other than 3 and 4 are then shifted by −(+0x20)/2 in x and y. [BYTES seg9:0x10d4]

  | Class | Side | Extent +0x20 | `max_damage` | `blast_size` | Evidence |
  |---|---|---|---|---|---|
  | 3 | 14 px | 112 | 12000 | 0 | [BYTES seg9:0x10d4] |
  | 4 | 20 px | 152 | 120 | 8 | [BYTES seg9:0x10d4] |
  | other | 34 px | 272 | 120 | 8 | [BYTES seg9:0x10d4] |

- **Radar fields.** Pool-8 objects get `radar_draw` seg11:0x39d8 with brush 1 (yellow) at seg9:0x1218 (+0x44 = 0xdac at seg9:0x11e7), brush 4 (grey) at seg9:0x18cc (+0x44 = 0xdac at seg9:0x18b8), and brush 4 at seg9:0x1be0, where +0x44 comes from the table word DGROUP:0x4d9a + bx (seg9:0x1aaf). [BYTES seg9:0x11e7 seg9:0x1218 seg9:0x18b8 seg9:0x18cc seg9:0x1aaf seg9:0x1be0]

### Programs and icons
The wave's objects of pool 0 are the icons of the directory's programs. The ExtractIcon calls and the icon bitmap are in [Icons](#icons). [BYTES seg9:0x0 seg9:0x156]
1. **Reset.** At the briefing entry, seg9:0xb2a sets `state` 1 for every in-use pool-0 record and every in-use pool-1 record (there are none after the load), then calls seg9:0x0(8), its only call. seg9:0x0 stores the limit 8 in DGROUP:0x9204 and sets the directory record's byte +0x1d0 = 2. [BYTES seg9:0xbf5 seg9:0xbf7 seg9:0x0 seg9:0x2a]
2. **Enumeration.** seg9:0x0 enumerates DGROUP:0x38c8 + "*.exe" (DGROUP:0x71c) and then DGROUP:0x38c8 + "*.dll" (DGROUP:0x722) with the RTL find-first seg5:0x364 (attribute 1) and find-next seg5:0x352. It calls seg9:0x156(dir + name) per file while DGROUP:0x9206 ≤ 9, and leaves a pattern once DGROUP:0x9206 reaches 10. [BYTES seg9:0x0 DGROUP:0x71c DGROUP:0x722 seg9:0x46 seg9:0x58 seg9:0x62 seg9:0x8e seg9:0xb8 seg9:0xca seg9:0xd4 seg9:0x100]
3. **Icons of a file.** Every icon index of every exe or dll is a separate candidate. Each index consumes one global icon number DGROUP:0x9200. [BYTES seg9:0x178 seg9:0x1a5 seg9:0x251 seg9:0x156]
4. **Captured icons.** An icon whose number DGROUP:0x9200 has its bit set in the directory record's bitmap (byte +0x1e2 + n/8, bit n % 8; record DGROUP:0x69f4 + 0x297·DGROUP:0x94a2) is skipped without extraction and only advances DGROUP:0x9200. [BYTES seg9:0x22e seg9:0x235 seg9:0x241 seg9:0xad2 seg9:0xaee seg9:0xb0b seg9:0xb15]
5. **Slot.** A non-skipped icon takes the first pool-0 record with `state` ≠ 0 (seg9:0x94c). With none free, seg9:0x94c sets directory byte +0x1d0 = 1, increments DGROUP:0x9206 and returns −1, which ends that file's loop. [BYTES seg9:0x18f seg9:0x196 seg9:0x979 seg9:0x99a seg9:0x9a0]
6. **Activation.** seg9:0x9b0(s) first calls seg9:0xac4(DGROUP:0x9200, 1), which increments DGROUP:0x9200 and returns at once when the old number's bit is set. When DGROUP:0x9206 < DGROUP:0x9204 (8) it: [BYTES seg9:0x9d1 seg9:0x9d8 seg9:0xa11 seg9:0xa43 seg9:0xa4f seg9:0xa5d seg9:0xa66 seg9:0xa75 seg9:0xa87 seg9:0xa9d seg9:0xab1]
   - clears DGROUP:0x57a8, DGROUP:0x57aa and DGROUP:0x57c1 + 0x23·s, sets the bytes DGROUP:0x57b2 and DGROUP:0x57b3 + 0x23·s to 0xff, and clamps the directory word +0x1d2 + 2s to 0 when it is negative;
   - stores that word in +0x1e, the number DGROUP:0x9200 − 1 in DGROUP:0x57ae + 0x23·s, the found file name DGROUP:0x3966 in DGROUP:0x57b4 + 0x23·s, and `state` 0;
   - calls seg10:0x736(s), sets vx = `rand()` % 60 − 30 and vy = `rand()` % 60 − 30 (−30 … 29), and increments DGROUP:0x9202.
   - Otherwise it sets directory byte +0x1d0 = 1. In both cases it increments DGROUP:0x9206 (seg9:0xab7). [BYTES seg9:0xab7]
7. **Position.** seg10:0x736 places the icon at x = (`rand()` % 3050 + 150)·8, y likewise, sets +0x00, +0x02, +0x08 and +0x0a to −1000 and bytes +0x2f and +0x54 to 0, and redraws inside the [centre exclusion](#random-positions). [BYTES seg10:0x736]
8. **Result.** seg9:0x0 ends by storing min(DGROUP:0x9206, 8) in DGROUP:0x91fa, DGROUP:0x91fc and DGROUP:0x91fe and resetting DGROUP:0x9204 to 8. A wave holds at most 8 icon objects. [BYTES seg9:0x0 seg9:0x114 seg9:0x125 seg9:0x12d seg9:0x133 seg9:0x13f seg9:0x148]
   - Directory byte +0x1d0 stays 2 when no icon was placed, becomes 0 when icons were placed and none were left over, and stays 1 when icons remain. [BYTES seg9:0x2a seg9:0x114 seg9:0x125 seg9:0x12d]

### Ships at wave start
seg9:0x60bc places the ships after the calls that [shell.md](shell.md#screen-states) lists (state 3, briefing). [BYTES seg9:0x6320]
- **Chance block.** Before its ship loop seg9:0x60bc clears DGROUP:0x913e, and sets it to 1 when one `rand()` % 100 falls below a chance. It draws only when DGROUP:0x9334 > 4, DGROUP:0x92e8 = 0, the kind is neither 6 nor 7, the edition DGROUP:0x920c is neither 0 nor 3, DGROUP:0x992 + 30 is below the budget or DGROUP:0x91f8 − DGROUP:0x93ac > 15, DGROUP:0x91f8 − DGROUP:0x93ac > 4, and the player's weapon slots hold fewer than 4 weapons of ids 0x16 … 0x1a, id 0x1a counting twice. [BYTES seg9:0x6126 seg9:0x612c seg9:0x6219 seg9:0x6229 seg9:0x63c0]
- **Every ship.** Each ship i = 0 … DGROUP:0x9316 of pool 14 gets +0x02 = −100. [BYTES seg9:0x63ca seg9:0x63d2]
- **Off-world ships.** In a new directory (DGROUP:0x94b0 ≠ 0) every ship goes off-world. In a revisited directory the player, every ship in a race and every ship whose roster index (ship record +0xd6) is absent from the directory record bytes +0x1c0 … +0x1c8 go off-world. Off-world means x = −1000, y = (−10 − i)·100, `state` 100, vx = 0, vy = −30 and ship-record dword +0x88 = now + i·500000. [BYTES seg9:0x63e9 seg9:0x63ff seg9:0x640f seg9:0x6421 seg9:0x6429 seg9:0x6309 seg9:0x6319 seg9:0x631c seg9:0x6347 seg9:0x6367 seg9:0x636b seg9:0x637e seg9:0x638a seg9:0x6394]
- **Player.** The player's ship then takes x and y from pool-12 record 0, the first gate's centre (dword +4 of the record at far DGROUP:0x1e28), and seg8:0x0 recentres the view. The player therefore starts at (gate x + 760, gate y − 150) in `state` 100 with vy = −30, until the [ship entry](#ship-entry-seg70x1d0c) places it at a gate. [BYTES seg9:0x6320 seg9:0x6326 seg9:0x6328 seg9:0x632c seg9:0x6335 seg9:0x633a]
- **Resident ships.** A ship listed in the record, not the player and not in a race, goes to ((`rand()` % 3200) << 3, (`rand()` % 3200) << 3), redrawn while its square (x, y, x + w, y + w), w = +0x20, intersects any in-use record square of pools 0–13. It then gets `state` 0, vx = `rand()` % 60 − 30, vy = `rand()` % 60 − 30 and ship-record dword +0x88 = now. [BYTES seg9:0x6433 seg9:0x6439 seg9:0x643d seg9:0x6452 seg9:0x6456 seg9:0x6488 seg9:0x628a seg9:0x62ae seg9:0x62ba seg9:0x62c0 seg9:0x62d2 seg9:0x62e8 seg9:0x62f6]
- **Heading.** The ship set-up seg14:0x1430 writes the heading word +0x28 = 0 (seg14:0x15fa), and nothing else in it writes the heading. seg14:0x170a writes the roster record's byte +0x28 + i instead (far DGROUP:0x6e24 + 0x7c·n), replacing it by byte DGROUP:0x2f8a + 0x50·k when that word is not −1 and DGROUP:0x11aa = 0. [BYTES seg14:0x15bc seg14:0x15fa seg14:0x16fd seg14:0x170a]
- The heading at wave start differs between runs. [ORACLE wave-0001] The ship set-up does not produce that difference. [BYTES seg14:0x15fa]
- **Radar brushes.** seg13:0x2dcc, called from seg9:0x611c, sets the ships' radar brushes ([Radar colours](#radar-colours)). [BYTES seg9:0x611c]

### Ship entry (seg7:0x1d0c)
The wave frame calls seg7:0x1d0c at seg7:0xb99 while DGROUP:0x9140 ≠ 0. Each call enters at most one waiting ship. [BYTES seg7:0xb91 seg7:0xb99]
1. **Player sprite.** It first calls seg7:0x1208 when the player's `state` is 100. [BYTES seg7:0x1d24 seg7:0x1d2c]
2. **Choice.** It takes the first ship i = 0 … DGROUP:0x9316 whose ship dword +0x88 is below now (unsigned 32-bit). When i's `state` is 100, the first later ship j > i with `state` 100 gets +0x88 = now + 5000, and no other ship does. A ship i whose `state` is not 100, or whose design word DGROUP:0x970a + 0xad·(ship word +0x4e) is 7 while ship byte +0x5e = 1, is passed over and the search goes on. [BYTES seg7:0x1efe seg7:0x1f0e seg7:0x1f16 seg7:0x1f18 seg7:0x1d4c seg7:0x1d57 seg7:0x1d70 seg7:0x1d78 seg7:0x1d80 seg7:0x1d89 seg7:0x1d94]
3. **Gate.** It draws g = `rand()` % DGROUP:0x9318 and then uses g = 0 for the player, so the draw happens for the player too. [BYTES seg7:0x1d9d seg7:0x1da3 seg7:0x1daa seg7:0x1db2]
4. **Position.** With post0 and post1 the pool-8 records 2g and 2g + 1 (DGROUP:0x1e18, stride 0xe0), it sets x = post0.x + ((post1.x − post0.x)/2 − (object +0x20)/2) + (pool-8 record 0 +0x20)/2 and y = post0.y, all signed 16-bit. [BYTES seg7:0x1dc0 seg7:0x1dd4 seg7:0x1dea seg7:0x1e2b]
5. **Entry.** It sets `state` 0, sets DGROUP:0x9714 + 0xad·design = 1 for the player, calls seg8:0x0, plays sound 0x27 through seg10:0x754a(0x27, 14, i, 1), and calls seg12:0x2556(14, i) and seg7:0x13b2(2, 14, i). [BYTES seg7:0x1e35 seg7:0x1e4e seg7:0x1e54 seg7:0x1e62 seg7:0x1e6f seg7:0x1e7f]
6. **Pending flag.** It recomputes DGROUP:0x9140, non-zero while some ship in `state` 100 has byte +0xec ≠ 0, and sets DGROUP:0x668a = 1 in a kind-7 wave once none remain. [BYTES seg7:0x1e85 seg7:0x1ea3 seg7:0x1ebb seg7:0x1ec9]
- When the player is entered this way, the post position replaces the set-up's start at the gate centre. [BYTES seg7:0x1dd0 seg9:0x6320]

### Probe wave
The probe's wave dump holds DGROUP:0x9320 = 0, one gate (DGROUP:0x9318 = 1) with beam DGROUP:0x6678 = (6424, 14032, 7624, 14080), categories DGROUP:0x933a = 24 and DGROUP:0x933c = 14 with 38 rocks in use (DGROUP:0x940c), pool-4 count DGROUP:0x940e = 3, pools 8 and 12 with 2 records in use each, DGROUP:0x992 = 50, DGROUP:0x94e2 = 38 and DGROUP:0x9332 = 2. Its `rand()` state leads back to the wave-start seed 0xb411 ([rng.md](rng.md#oracle-check)). The object positions live in the pool blocks, outside the DGROUP image. [ORACLE probe-0001]

## World coordinates
- **Extent.** The world is 25600 × 25600 units. DGROUP:0x5a0 and DGROUP:0x5a2 both hold 3200 (bytes 80 0c 80 0c), and no instruction in INSPACE.EXE writes them. [BYTES DGROUP:0x5a0 DGROUP:0x5a2]
- **Two scales.** Users either multiply 3200 by 8 to get world units (`shl 3` or `imul 8`), or use 3200 directly as the world size in 1/8-unit cells, for example seg8:0x285a, seg12:0x51de and seg11:0x1b18. [BYTES seg8:0x1be0 seg9:0x643d seg9:0x6456 seg8:0x285a seg12:0x51de seg11:0x1b18]
- **Units.** Positions (+0x04, +0x06) are plain signed 16-bit world units with no fraction bits. Velocities (+0x0c, +0x0e) are world units per step. [BYTES seg8:0x1b54 seg8:0x1bdc seg8:0x1c02]
- **Pixels.** One pixel is `1 << s` world units, so 8 at the default `Size` 1. [BYTES seg8:0x0 seg9:0x2408]
- **Integrator.** seg8:0x1b54(steps, pool·256 + rec), Pascal (`retf 4`), adds vx·steps to x and vy·steps to y in 16 bits; steps = 1 skips the multiply. Unless DGROUP:0x9130 ≠ 0 it then wraps each axis: x > 25600 becomes 0 and x < 0 becomes 25600 (signed compares), the same for y. [BYTES seg8:0x1b54 seg8:0x1bdc seg8:0x1be0 seg8:0x1c02 seg8:0x1c09 seg8:0x1c1a seg8:0x1c41 seg8:0x1c62 seg8:0x1c80 seg8:0x1ca1]
  - Before moving, it returns at once for a pool-14 record whose byte +0x54 ≠ 0 while DGROUP:0x92e8 ≠ 0 and DGROUP:0x9e49 = 10. Otherwise it calls seg8:0x1e32(rec, pool) when `owner_pool` ≠ −1 and the pool is not 10. [BYTES seg8:0x1b54]
  - For pool DGROUP:0x9328 it saves DGROUP:0x9006 and sets it to 10, and restores it only when DGROUP:0x9130 = 0: with DGROUP:0x9130 ≠ 0 the jump at seg8:0x1c23 skips the restore and DGROUP:0x9006 stays 10. [BYTES seg8:0x1b54 seg8:0x1c23]
- **Distance.** seg8:0x285a (Pascal, `retf 0x10`) measures the distance of two world points in 1/8-unit cells. It shifts each coordinate right by 3 and wraps each axis delta by DGROUP:0x5a0 or DGROUP:0x5a2 (3200) when its magnitude exceeds half of that (1600). It stores the wrapped dx and dy through its two far-pointer arguments and returns one Newton step of the square root as a 32-bit value: ((dx² + dy²)/g + g) >> 1 with g = max + min/2 of the absolute deltas, or 1 when g = 0. In seg18 mode (DGROUP:0x9130 ≠ 0) it maps both points through seg8:0x29ea instead and neither shifts nor wraps. [BYTES seg8:0x2869 seg8:0x2872 seg8:0x2892 seg8:0x28cb seg8:0x28f3 seg8:0x2951 seg8:0x2987 seg8:0x2990 seg8:0x29c8 seg8:0x29db]
- **Start position and heading.** The ship set-up puts the player at the first gate's centre with heading 0 ([Ships at wave start](#ships-at-wave-start)), and the [ship entry](#ship-entry-seg70x1d0c) moves it to a gate position. [BYTES seg9:0x6320 seg14:0x15fa seg7:0x1dd0]
- **View origin.** The view origin follows the player ([View](#view)). [BYTES seg8:0x0]

## Update loop
The frame gate and the pass structure are in [timing.md](timing.md#the-frame-gate). In address order, a wave pass and a wave frame do this: [BYTES seg7:0x58b seg7:0x92d]
1. **Turn follow-up and radar.** When DGROUP:0x9138 ≠ 0 (then cleared), or DGROUP:0x8f76 ≠ 0 and DGROUP:0x9144 ≥ DGROUP:0x8f76, or DGROUP:0x8f76 = 0 and dword DGROUP:0x5a8 % 3 = 0, the pass calls seg11:0x1bcc (fuel and mass, [Mass](#mass-thrust-and-drag)), seg11:0x894 and seg11:0x3396 (the radar, [Refresh](#refresh)). [BYTES seg7:0x7fe seg7:0x82d seg7:0x833 seg7:0x83d]
2. **Clip region.** The pass selects the clip region DGROUP:0x12 into DC DGROUP:0x13dc at seg7:0x842–0x84a ([Clip region and message line](#clip-region-and-message-line)) unless it has left earlier: a pass with DGROUP:0x9130 ≠ 0 jumps from seg7:0x594 past it, and the wave-ending branches return before it. [BYTES seg7:0x594 seg7:0x842 seg7:0x84a]
3. **Pending input.** seg7:0x863 delivers a key message left in DGROUP:0x8e62 ([Input](#input)). [BYTES seg7:0x863]
4. **Fire poll.** seg11:0x4db0 runs on every pass ([Input](#input)). [BYTES seg7:0x871 seg7:0x87a seg7:0x881]
5. **Timers.** While DGROUP:0x1048 ≠ 0 the pass runs the DGROUP:0x1e6c block: once now ≥ DGROUP:0x1e6c with DGROUP:0x92f0 ≠ 0 it sets DGROUP:0x1e6c = now + 10000, calls seg8:0x9464 and clears DGROUP:0x914e, and within 500 ms of DGROUP:0x1e6c with DGROUP:0x914e = 0 it calls seg8:0x9564. While the player's ship is in `state` 0 it moves DGROUP:0x5a4 to now + 50 once now reaches it, and every 10 s (DGROUP:0x5b8) calls seg7:0x2128. [BYTES seg7:0x886 seg7:0x891 seg7:0x898 seg7:0x8a9 seg7:0x8ad seg7:0x8b2 seg7:0x8bc seg7:0x8c9 seg7:0x8d0 seg7:0x8e5 seg7:0x8f0 seg7:0x901 seg7:0x90d seg7:0x91a seg7:0x922]
6. **Beam slots.** When a frame is due (DGROUP:0x9144 ≥ DGROUP:0x8f76), seg7:0x938–0x970 calls seg8:0x3aec for i = 0 … 4 (outer) and j = 0 … DGROUP:0x594 + DGROUP:0x598 − 1 (inner). The callee (Pascal, `retf 4`) gets i at [bp+6] and the slot j at [bp+8]. Each result goes to DGROUP:0x9136, and each outer pass increments dword DGROUP:0x5cc. [BYTES seg7:0x938 seg7:0x93d seg7:0x942 seg7:0x946 seg7:0x94b seg7:0x94e seg7:0x951 seg7:0x954 seg7:0x95b seg7:0x95d seg7:0x962 seg7:0x965 seg7:0x96b seg8:0x3afb seg8:0x3cf6 seg8:0x3fba]
   - seg8:0x3aec is the update of beam slot j of the 0x94-byte table at DGROUP:0x6098, active while 0 ≤ DGROUP:0x610e + 0x94j < DGROUP:0x610c + 0x94j. It moves the slot's pixels by owner vx·i / −50 and vy·i / −50 and wraps at 25600. Weapons belong to M4. [BYTES seg8:0x3afb seg8:0x3b00 seg8:0x3b0b seg8:0x3cf6 seg8:0x3cfc seg8:0x3cff seg8:0x3d01]
   - DGROUP:0x594 = 3 and DGROUP:0x598 = 2 are slot counts. seg11:0x4e19 sets DGROUP:0x594 = 7, and DGROUP:0x598 has no writer, so a frame normally makes 25 calls. [BYTES DGROUP:0x594 DGROUP:0x598 seg11:0x4e19]
7. **Effects.** seg8:0x3fbe(k) runs only when DGROUP:0x9430 > 0, for k = 0 … DGROUP:0x9434 inclusive, and animates entry k of the effect table DGROUP:0x1e90. seg7:0x375a then runs unconditionally and animates the 10 × 4 particle burst at DGROUP:0x5b28 while DGROUP:0x9510 < 50. Neither moves pool objects. [BYTES seg7:0x972 seg7:0x977 seg7:0x980 seg7:0x983 seg7:0x98b seg7:0x991 seg7:0x993 seg8:0x3fbe seg7:0x375a]
8. **Frame counters.** The frame counts DGROUP:0x9142 through 0 … 54 (seg7:0x9dc) and, at seg7:0xb2d and seg7:0xb32, increments the dwords DGROUP:0x5a8 and DGROUP:0x1e68 once per frame. [BYTES seg7:0x9d6 seg7:0x9dc seg7:0x9e0 seg7:0x9e7 seg7:0xb2d seg7:0xb32]
9. **OverThruster re-arm.** Once every 8 frames (counter DGROUP:0x17b6) the frame decrements byte +0x60 of every ship whose byte is not 0. [BYTES seg7:0xafd seg7:0xb01 seg7:0xb08 seg7:0xb17 seg7:0xb1e]
10. **Joystick.** While INI `JoyStick` DGROUP:0x8ff4 ≠ 0 the frame calls seg11:0x7504 and, when the deadline DGROUP:0x2388 is below now (unsigned 32-bit), sets it to now + 50 and calls seg11:0x72b2 ([Joystick](#joystick)). [BYTES seg7:0xb37 seg7:0xb46 seg7:0xb4d seg7:0xb5a seg7:0xb5f]
11. **seg18 mode.** With DGROUP:0x9130 ≠ 0 the frame calls seg16:0x64fa and skips to seg7:0x1192, past steps 12 … 22. [BYTES seg7:0xb64 seg7:0xb6b]
12. **Before the view.** It calls seg10:0x7f88 when DGROUP:0x103c ≠ 0, then seg8:0x158a(0) at seg7:0xb87. [BYTES seg7:0xb70 seg7:0xb7a seg7:0xb87]
13. **View.** seg8:0x0 recentres the view (seg7:0xb8c), before the ships move. [BYTES seg7:0xb8c]
14. **Ship entry.** seg7:0x1d0c runs at seg7:0xb99 when DGROUP:0x9140 ≠ 0 ([Ship entry](#ship-entry-seg70x1d0c)). [BYTES seg7:0xb91 seg7:0xb99]
15. **Visibility.** The frame sets DGROUP:0x9146 = 0 and runs the visibility pass seg8:0x4e14 at seg7:0xba2 ([Visibility pass](#visibility-pass-seg80x4e14)). [BYTES seg7:0xb9c seg7:0xba2]
16. **Speed display end.** When the dword DGROUP:0x870c ≠ 0 and now is past it (unsigned), it calls seg3:0x6ec when DGROUP:0x9608 ≠ 0, else seg7:0x119c. [BYTES seg7:0xbad seg7:0xbb8 seg7:0xbd9 seg7:0xbe7]
17. **seg12:0x3778** runs at seg7:0xbed. [BYTES seg7:0xbed]
18. **Icon dissolve.** For i = 0 while i < DGROUP:0x9406 (read again on every iteration), it calls seg7:0x3a42(i) when pool-0 byte +0x51 ≠ 0 ([Icon dissolve](#icon-dissolve-seg70x3a42)). [BYTES seg7:0xbf2 seg7:0xc0b seg7:0xc14 seg7:0xc1a]
19. **Information displays.** When now is past the deadline dword DGROUP:0x17b8 (unsigned, strictly greater), the frame sets the deadline to now + 280 and calls seg13:0x24aa(0) in target mode or seg13:0x24aa(1) in message mode, and seg13:0x24aa(2) when DGROUP:0x95fc ≠ 0 ([IS_INFO displays](#is_info-displays)). [BYTES seg7:0xc24 seg7:0xc2b seg7:0xc38 seg7:0xc4c seg7:0xc5b]
20. **Before the movers.** It calls seg16:0xa2 when pool 7 record 0 has `state` 0, else seg16:0x33a2 when DGROUP:0x6da8 ≠ 0 and now > DGROUP:0x6da8 (unsigned), then seg16:0x3306 when that returns non-zero. It sets DGROUP:0x5d2 = (DGROUP:0x5d2 = 0), calls seg11:0xa9ea(1) when DGROUP:0x1016 ≠ 0, calls seg10:0x7b48, calls seg12:0x4d8e in a kind-6 wave, and calls seg18:0xd94 when DGROUP:0x913a ≠ 0 or DGROUP:0x913c ≠ 0. [BYTES seg7:0xc61 seg7:0xc6c seg7:0xc73 seg7:0xc86 seg7:0xc8f seg7:0xc94 seg7:0xca0 seg7:0xcaf seg7:0xcb4 seg7:0xcc0 seg7:0xcce]
21. **Movers.** The pool objects move in this call order: seg13:0x0 (pool 14), seg7:0x3bae (pool DGROUP:0x934a = 3), seg12:0x532c (9), seg12:0x633e (8), seg8:0x8256 (2), seg7:0x309c, seg7:0x32d0, seg7:0x40c4 (DGROUP:0x9328 = 5), seg12:0x0 (6), seg6:0x73f6, seg6:0x7c14, seg7:0x5e04 (when DGROUP:0x9456 > 0), seg12:0x3148 (10), seg12:0x6b1e (11), seg12:0xae4 (4, when DGROUP:0x9600 ≠ 0) and seg12:0x3e98(0). [BYTES seg7:0xcd3 seg7:0xcd8 seg7:0xcdd seg7:0xce2 seg7:0xce7 seg7:0xcec seg7:0xcf1 seg7:0xcf6 seg7:0xcfb seg7:0xd00 seg7:0xd05 seg7:0xd0a seg7:0xd11 seg7:0xd16 seg7:0xd1b seg7:0xd20 seg7:0xd27 seg7:0xd2e]
    - seg13:0x0, seg7:0x3bae, seg12:0x532c, seg8:0x8256, seg7:0x309c, seg7:0x32d0, seg7:0x40c4, seg12:0x0, seg12:0x3148, seg12:0x6b1e and seg12:0xae4 call the integrator seg8:0x1b54 directly. [BYTES seg7:0xcd3 seg7:0xd2e]
22. **After the movers.** The frame calls seg13:0xba04 once now has passed DGROUP:0x2384 (unsigned) while DGROUP:0x950, DGROUP:0x94e, DGROUP:0x92e8 and DGROUP:0x9130 are 0, and seg13:0xb93a when the frame counter DGROUP:0x1e68 is a multiple of 300. It then calls the player's sprite seg7:0x1208 (at seg7:0xda9) when DGROUP:0x58a, DGROUP:0x588 or ship byte +0xeb is set while DGROUP:0x951a and DGROUP:0x238c are 0, and skips both the sprite and the next block while DGROUP:0x238c ≠ 0. Otherwise, once ship dword +0x156 has passed, it runs the 1500 ms block seg7:0xdce–0x117d, which with DGROUP:0x951a ≠ 0 calls only seg13:0x2f30 and leaves +0x156 unchanged. It ends with seg7:0x158e while DGROUP:0x9148 > 0 (signed) and seg13:0x447a. [BYTES seg7:0xd38 seg7:0xd3f-0xd59 seg7:0xd6d seg7:0xd7d-0xda9 seg7:0xdbc-0xddd seg7:0x1179 seg7:0x1182 seg7:0x118d]

### Ships (seg13:0x0)
- seg13:0x0 runs once per wave frame (seg7:0xcd3). It first calls seg13:0x675e, then increments DGROUP:0x95f6 and resets it to 0 when it exceeds 8, a 0 … 8 cycle. [BYTES seg7:0xcd3 seg13:0x18 seg13:0x1c seg13:0x23]
- **Head.** It then cycles DGROUP:0x134e through 0 … 100, sets pool 14's in-use and allocated counts to DGROUP:0x9316 + 1, and, when the dword DGROUP:0x1350 is below now (unsigned), sets a local flag and DGROUP:0x1350 = now + 9000. [BYTES seg13:0x29 seg13:0x6d]
- It loops i = 0 … DGROUP:0x9316 and skips ships whose `state` ≠ 0. For each other ship it moves the ship and then runs that ship's input and physics, so the steps interleave per ship. [BYTES seg13:0xd37 seg13:0x9f]
  - The move is seg13:0x3b88 for the player when DGROUP:0x913a or DGROUP:0x913c = 2, seg13:0x3748 for ship DGROUP:0x952e when DGROUP:0x9e49 is 1 … 6, and seg8:0x1b54(1, pool·256 + i) otherwise. [BYTES seg13:0x9f seg13:0xa1]
  - When ship byte +0xec (DGROUP:0x717c) = 0 and byte +0xe2 (DGROUP:0x7172) = 0, seg13:0xe1 returns from seg13:0x0, and the remaining ships are skipped that frame. [BYTES seg13:0x9f seg13:0xa1]
  - **Tumbling.** The path at seg13:0xd7 takes a ship with byte +0xec = 0, and ship DGROUP:0x952e while DGROUP:0x9e5b = 0 and DGROUP:0x9e49 = 2. It returns from seg13:0x0 when byte +0xe2 = 0, as above, and otherwise tumbles the ship. With byte +0xec = 0, once dword +0x84 < now (unsigned), it clears +0xe2 and the object's +0x26 and calls seg12:0x274a(DGROUP:0x9324, i). [BYTES seg13:0xb5 seg13:0xc2 seg13:0xcb seg13:0xd2 seg13:0xdc seg13:0x14b seg13:0x156 seg13:0x160 seg13:0x174 seg13:0x181]
- **Drawing.** seg13:0x0 draws ship i with seg8:0xb76(0, DGROUP:0x39e8[i + 0x28], 1, (pool << 8) | i) when its word +0x1a ≠ 0, except the player's ship while DGROUP:0x951a = 0 and ship DGROUP:0x952e while DGROUP:0x9e49 is 3 … 6. Before the draw it sets `src_x` and `src_y` from ship-record word +0x10, read at the top of that ship's iteration. [BYTES seg13:0xa66 seg13:0xa77 seg13:0xa80 seg13:0xa8e seg13:0xa9b seg13:0xaa1 seg13:0xaef seg13:0xb37 seg13:0xb4f seg13:0xb55 seg13:0xd28]
- The flight steps it runs are in [Flight model](#flight-model). [BYTES seg13:0x224 seg13:0x616]

### Rocks (seg7:0x3bae)
- Only rocks in `state` 0 move. [BYTES seg7:0x3bae]
- **Off screen.** A rock with word +0x1a = 0 moves only on frames with DGROUP:0x9142 % 3 = 0, by seg8:0x1d54(0x28, …) and then seg8:0x1b54(3, …). [BYTES seg7:0x3bae]
- **Speed limit.** seg8:0x1d54 (`retf 4`; the caller pushes pool·256 + rec first and the limit second, so the limit is at [bp+6]) moves each non-zero velocity axis one unit toward 0 when \|vx\| + \|vy\| ≥ the limit, a 16-bit signed test. [BYTES seg8:0x1d54 seg8:0x1d72 seg8:0x1e27 seg7:0x3103 seg7:0x3108]
- **On screen.** A rock with +0x1a ≠ 0 calls seg8:0x1d54(0x28, …) and seg8:0x1b54(1, …) each frame. It then sets the frame +0x28 = +0x26/100 (0 … 17) from the current +0x26 and the source x +0x10 = (+0x22 + +0x24)·frame + +0x24, and draws. [BYTES seg7:0x3bae]
- **Spin.** After drawing it adds (|vx| + |vy|) to +0x26 with the sign of vx, wrapping to 0 above 1799 and to 1799 below 0. [BYTES seg7:0x3bae]
- **Collisions.** It tests collisions with seg8:0x50aa when +0x1a ≠ 0 and +0x1c = 0. [BYTES seg7:0x3bae]
- **Image slot.** For an in-play on-screen rock seg7:0x3bae picks the image slot from the heat byte +0x2f: above 0 slot 6, at or below 0 slot 7 (0x24 and 0x23 while DGROUP:0x9130 ≠ 0). A small rock adds 2 to the slot: while DGROUP:0x9130 = 0 when word +0x22 < (30 << `Size`) >> 1, and while DGROUP:0x9130 ≠ 0 when +0x22 < 12 at `Size` 0 or +0x22 < 30 at `Size` 1 (`Size` = DGROUP:0x9156; signed compares). [BYTES seg7:0x40a0 seg7:0x40a5 seg7:0x40ae seg7:0x40b2 seg7:0x3ca6 seg7:0x3cb4 seg7:0x3cb6 seg7:0x3cc7 seg7:0x3ccd seg7:0x3cd8 seg7:0x3cee seg7:0x3cf5 seg7:0x3d0b seg7:0x3d12]
- **Hot rock.** While the heat is above 0, it selects brush DGROUP:0x3b74[heat] into DGROUP:0x13dc and draws with MERGECOPY. It lowers the heat by 1 when dword +0x6a has passed (unsigned) and sets +0x6a = now + 3000. It restores the previous brush only when the heat is still above 0 after the decrement, so the drop to 0 leaves the heat brush selected. [BYTES seg7:0x3bc5 seg7:0x3be6 seg7:0x3bf4 seg7:0x3c0d-0x3c3f seg7:0x3c1f seg7:0x3c3b seg7:0x3d9f seg8:0xd5b]
- **Cold rock.** A negative heat first calls seg7:0x5b68(rock pool, i, 0) when `rand()` % 7 = 2, and then rises by 1 every frame. [BYTES seg7:0x3c54 seg7:0x3c5b seg7:0x3c66 seg7:0x3c75 seg7:0x3c8a seg7:0x3c95 seg7:0x3c9c]
- **Heating.** A rock hit by a DGROUP:0x9328 object of mass 0x78 gets heat + 1, at most 9, and +0x6a = now + 5000. A hit by one of mass 0xc8, while the heat is above 0 (signed), turns the heat into −5·heat (8-bit) and calls seg7:0x5b68(DGROUP:0x934a, rock, 0). [BYTES seg7:0x3f4c seg7:0x3f65 seg7:0x3f7d seg7:0x3f90 seg7:0x3fc3 seg7:0x3fda seg7:0x3fe3 seg7:0x3fe7 seg7:0x3ff7]

### Collisions
- Collision tests are in screen space. seg8:0x50aa(pool·256 + rec) refreshes the screen position +0x00/+0x02 of the candidates through seg8:0x29ea, builds squares (`scr_x`, `scr_y`, `scr_x` + +0x22, `scr_y` + +0x22) and tests pairs with seg8:0x6afa. [BYTES seg8:0x50aa seg8:0x29ea seg8:0x6afa]
- It returns the hit object's pool·256 + rec, 0x1000 for the pool-16 record (seg8:0x5454, after seg18:0x2c1e), or 0xFFFF. [BYTES seg8:0x50aa seg8:0x6afa]

### Visibility pass (seg8:0x4e14)
seg8:0x4e14 writes the on-screen word +0x1a and the word +0x1c. It walks pools DGROUP:0x9326 … DGROUP:0x9324 and appends records to the byte lists DGROUP:0x507c (pool) and DGROUP:0x50e0 (record) at index DGROUP:0x9146, then incremented. [BYTES seg8:0x4e28 seg8:0x4fad seg8:0x4fc0]
- **Normal path (DGROUP:0x9130 = 0).** It visits only records with `state` 0 and `mass` +0x44 ≠ 0, skips a pool-8 record whose word DGROUP:0x3d80 + 0x34·rec is 4, and, while 3 ≤ DGROUP:0x9e49 ≤ 6, skips pool 7 and, from pool 14 on, the record DGROUP:0x952e. [BYTES seg8:0x4e32 seg8:0x4f30 seg8:0x4f37 seg8:0x4f44 seg8:0x4f54 seg8:0x4f75 seg8:0x4f7b seg8:0x4f81 seg8:0x4f8d]
  - A visited record whose (+0x04, +0x06) lies in one of the DGROUP:0x9158 [activity rectangles](#view) at DGROUP:0x23e0 (PtInRect, first match) gets +0x1c = 0 and +0x1a = 1 and is appended. [BYTES seg8:0x4e7c seg8:0x4e89-0x4ea1 seg8:0x4ebe]
  - A visited record outside them gets +0x1a = −1 when its +0x1a was above 0, and otherwise +0x1a = 0 and (+0x00, +0x02) = (−1000, −1000). [BYTES seg8:0x4ed4-0x4f0a]
- **seg18 mode (DGROUP:0x9130 ≠ 0).** It appends every in-use record of those pools, with no state, mass or rectangle test and no skips, and sets each one's +0x1c = 0 and +0x1a = 1. [BYTES seg8:0x4f9a seg8:0x4fcd seg8:0x4fd1 seg8:0x4fd5 seg8:0x4fe1 seg8:0x4fec]
- Both paths end in one `rand()` draw, whose use is in [rng.md](rng.md#callers-and-range-mapping). [BYTES seg8:0x4ffb]

## Object record
The fields of the object record that the wave uses (+0x08 … +0x6a: screen and source positions, `on_screen`, `extent`, `size`, `gap`, `frame`, `mass` and the rest) are in [structs.md](structs.md#object-record-0x70-bytes). [BYTES seg10:0xb94 seg8:0xb76]

## Flight model
The player's ship moves through seg13:0x0 ([Ships](#ships-seg130x0)). Headings index a 16-entry vector table, and every formula below keeps the original's 16- and 32-bit truncation. Object fields are in [structs.md](structs.md#object-record-0x70-bytes) and ship-record fields (DGROUP:0x7090 + 0x15a·i) in [structs.md](structs.md#ship-records-8--0x15a-bytes-at-dgroup0x7090). [BYTES seg13:0x0 DGROUP:0x1050]

### Order per frame
- For the player in action mode with byte +0xe2 = 0, seg13:0x0 calls seg4:0x0 (spin) every frame and seg11:0x5ab6(4, i) (thrust, brake and drag) only when DGROUP:0x95f6 is even, on 5 of every 9 frames (DGROUP:0x95f6 = 0, 2, 4, 6, 8). Other ships get mode 3 on the same frames. This path needs ship byte +0xec ≠ 0. [BYTES seg13:0x224 seg13:0x233 seg13:0x236 seg13:0x23b seg13:0x240 seg13:0x245 seg13:0x255]
- Every ship has moved by its velocity in seg13:0x0 before its own input and physics of that frame run ([Ships](#ships-seg130x0)). [BYTES seg13:0x9f]

### Heading table
- DGROUP:0x1050 holds 16 (dx, dy) word pairs, one per heading: [BYTES DGROUP:0x1050]

  | h | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
  |---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
  | dx | 0 | 6 | 11 | 15 | 16 | 15 | 11 | 6 | 0 | −6 | −11 | −15 | −16 | −15 | −11 | −6 |
  | dy | −16 | −15 | −11 | −6 | 0 | 6 | 11 | 15 | 16 | 16 | 11 | 6 | 0 | −6 | −11 | −15 |

- Entry 9 is (−6, 16), not the mirror (−6, 15) of entry 7. [BYTES DGROUP:0x1050]
- Heading 0 points up (screen y decreasing), and each step of +1 is 22.5° clockwise. [BYTES seg11:0x59dc DGROUP:0x1050]

### Turning
- **Router guards.** seg11:0x592a, the player's turn input, does nothing while `PlayMode` DGROUP:0x9134 ≠ 0, while DGROUP:0x9320 = 6 with DGROUP:0x9eb4 = 0, while ship byte +0xeb ≠ 0, or while the lock-out dword +0x84 ≥ now (32-bit unsigned). It also tests DGROUP:0x1034, which holds 1 and is never written. [BYTES seg11:0x5938 seg11:0x5941 seg11:0x5948 seg11:0x594f seg11:0x595c seg11:0x5967 seg11:0x596c DGROUP:0x1034]
- **Keys.** It tests VK_RIGHT first and VK_LEFT second, each by `GetAsyncKeyState` bit 0x8000 alone or by the joystick flags DGROUP:0x1046 (right) and DGROUP:0x1044 (left). Both blocks can run in one call, so holding both keys leaves the heading unchanged. [BYTES seg11:0x5971 seg11:0x5978 seg11:0x597d seg11:0x5a0f seg11:0x5a16 seg11:0x5a1b]
- **Step.** VK_RIGHT adds 1 to the heading +0x28 and VK_LEFT subtracts 1, wrapping above 15 to 0 and below 0 to 15. [BYTES seg11:0x59dc seg11:0x59f0 seg11:0x59f7 seg11:0x5a7a seg11:0x5a8e seg11:0x5a95]
- **Plain arrow.** With Alt up (`GetAsyncKeyState(VK_MENU)`) an arrow steps the heading by one, sets object +0x26 = 0 and spin +0x12 = 0, calls seg7:0x1208 (the player's sprite) when DGROUP:0x238e = 0, and sets DGROUP:0x9138 = 1, which makes the next wave pass call seg11:0x1bcc, seg11:0x894 and seg11:0x3396. [BYTES seg11:0x59aa seg11:0x59ba seg11:0x59c6 seg11:0x59fd seg11:0x5a04 seg11:0x5a09 seg7:0x7fe seg7:0x82d seg7:0x833]
- **Alt spin.** Alt+Right sets spin +0x12 to min(+0x12 + 1, 1) and Alt+Left to max(+0x12 − 1, −1). seg4:0x0, run every frame for the player in action mode, adds +0x12 to the heading with the 0 … 15 wrap and calls seg7:0x1208. The ship spins one step per frame until a plain arrow or the start of thrust (seg11:0x6437) zeroes +0x12. [BYTES seg11:0x5985 seg11:0x5997 seg11:0x59a2 seg11:0x5a23 seg11:0x5a35 seg11:0x5a40 seg4:0x0 seg13:0x236 seg11:0x6437]
  - seg4:0x0(i) (`retf 2`) reads pool 14 through DGROUP:0x1e30 and first calls seg7:0x1208 when record i's word +0x26 ≠ 0 and DGROUP:0x238c = 0. [BYTES seg4:0xb seg4:0xf seg4:0x13 seg4:0x1a seg4:0x21]
- **Cadence.** Each frame a ship on the normal flight path reaches seg13:0x616, unless the wave is kind 6 (DGROUP:0x9320 = 6) with DGROUP:0x9eb4 ≠ 1, which takes seg13:0x545 instead. At seg13:0x616: [BYTES seg13:0x61b seg13:0x620 seg13:0x631 seg13:0x638 seg13:0x644 seg13:0x64f seg13:0x654 seg13:0x667 seg13:0x688]
  - if the accumulator +0x16 < 10, it adds the turn word +0x14, times 3 when object +0x1a = 0 and DGROUP:0x9320 ≠ 6, then calls seg11:0x592a for the player in action mode or seg4:0xbde for any other ship;
  - otherwise it subtracts 10 from +0x16 and reads no turn input that frame.
- **Rate.** With turn word r and a key held, the player gets on average 10/(r + 10) heading steps per frame, 1350/(r + 10) RPM at 36 frames per second. [BYTES seg13:0x61b seg13:0x654 seg13:0x688]
  - The help page prints TURN RATE as 1350/r (seg17:0x3771: 0x546 `idiv` ship word +0x14), so its 64 RPM means r = 21. The real rate is then about 1350/31 = 43.5 RPM, about 3.5 steps (79°) in 0.3 s, which agrees with the 4 steps (90°) read from the sprite. [BYTES seg17:0x3771] [ORACLE wave-0001]
- **Turn word.** +0x14 = (1200 × body byte +0x1d) / T' in 32 bits (seg5:0x110), the body byte kept in the design record at DGROUP:0x96f2. T' is the design thrust DGROUP:0x9710 (= SHP thruster × 5); for a ship with a roster record (+0xd6 ≠ −1) it is ((signed roster byte +0x33 + 10)·10·thrust)/100 in 32 bits. When DGROUP:0x95bc ≠ 0, seg14:0x22e4 sets the player's turn word to (3·r)/4, truncated toward zero. More thrust therefore turns faster. [BYTES seg14:0x22a2 seg14:0x22b9 seg14:0x22c2 seg14:0x22cc seg14:0x22e4 seg14:0x22ea seg14:0x22ef seg14:0x22f2] The MASTER.BOD and MASTER.SHP fields are in [formats.md](formats.md#mastershp-ships). [DESIGN]

### Go flag
seg11:0x5ab6 computes a go flag that the brake and the thrust need. [BYTES seg11:0x5b35]
1. It starts as (ship word +0x2a ≠ 0). [BYTES seg11:0x5b35]
2. It is cleared when +0x2a < 50 and (DGROUP:0x9320 is not 6 or 7, or fuel +0x1e > 50) and DGROUP:0x95f6 < 4. [BYTES seg11:0x5b4d seg11:0x5b54 seg11:0x5b67 seg11:0x5b6e seg11:0x5b7f]
3. When fuel < 50 it is recomputed as (max(fuel/12, 2) ≥ DGROUP:0x95f6/2, or the mode argument = 1), dropping the +0x2a test. For a ship other than the player in action mode it is also set when ship word +0x32 = 4 and +0x38 = 2. [BYTES seg11:0x5b8f seg11:0x5b95 seg11:0x5ba5 seg11:0x5ba7 seg11:0x5bb6]
4. It is cleared while the lock-out dword +0x84 > now (unsigned), or in a kind-6 wave while +0x84 + 400 > now. [BYTES seg11:0x5bee seg11:0x5c0b seg11:0x5c1c]

### Thrust
- **Early returns.** seg11:0x5ab6 returns at once in a kind-6 wave (DGROUP:0x9320 = 6) while byte DGROUP:0x9eb4 = 0, and for ship DGROUP:0x952e while 3 ≤ DGROUP:0x9e49 ≤ 6 and DGROUP:0x9130 = 0. seg11:0x63b4 returns at once for ship DGROUP:0x952e while 1 ≤ DGROUP:0x9e49 ≤ 6 and DGROUP:0x9130 = 0. [BYTES seg11:0x5aca seg11:0x5ad6 seg11:0x5b11 seg11:0x5b2c seg11:0x63a8 seg11:0x63c3 seg11:0x63de seg11:0x6869]
- **Autopilot flag.** seg11:0x5ab6 sets its local autopilot flag [bp − 0x1c] for ship DGROUP:0x952e while DGROUP:0x9e49 = 0, unless that ship is the player with `PlayMode` DGROUP:0x9134 = 0, and for the player while DGROUP:0x951a ≠ 0. [BYTES seg11:0x5ada seg11:0x5af8 seg11:0x5afd seg11:0x5b0c]
- **Autopilot step.** With the flag set it sets ship word +0xda = min(+0xda/2, 40), sets the thrust block [bp − 0x1a] = 1, and sets the mode argument to 1 when \|vx\| or \|vy\| exceeds 30. [BYTES seg11:0x5f16 seg11:0x5f1c-0x5f36 seg11:0x5f4a-0x5f75]
- **Mode 2.** Mode 2 sets DGROUP:0x952a = 1 and becomes mode 1; every other mode sets DGROUP:0x952a = 0. [BYTES seg11:0x5c37 seg11:0x5c40 seg11:0x5c48]
- **Multiplier decay.** Every seg11:0x5ab6 call sets the thrust multiplier k (+0x1c) to max(k − 1, 1) after the go flag and before the key tests. seg14:0x21bb initialises it to 1. [BYTES seg11:0x5c26 seg11:0x5c2a seg11:0x5c30 seg11:0x5c33 seg14:0x21bb]
- **Brake or thrust.** Brake has priority. With the go flag set, seg11:0x5ab6 calls the brake seg11:0x6874, and skips thrust and drag, for the player's ship while `GetAsyncKeyState(VK_DOWN)` & 0x8001 and `PlayMode` DGROUP:0x9134 = 0, or in mode 5. Otherwise, with the go flag set and the thrust block [bp − 0x1a] clear, it calls the thrust seg11:0x63b4(mode, i) in mode 4 while `GetAsyncKeyState(VK_UP)` & 0x8001 or the joystick flag DGROUP:0x1040 holds with DGROUP:0x9134 = 0, in mode 1, or in mode 3 with ship word +0x02 ≠ 0. Otherwise it applies drift. The thrust block is cleared at seg11:0x5c4e and set by the autopilot step and at seg11:0x5f0a. [BYTES seg11:0x5c4e seg11:0x5f0a seg11:0x5f9b seg11:0x5fee seg11:0x5ffb seg11:0x6007] [BYTES seg11:0x5f7a seg11:0x5f81 seg11:0x5f86 seg11:0x5f95 seg11:0x5fab seg11:0x5fb2 seg11:0x5fb9 seg11:0x5fbe seg11:0x5fcc seg11:0x5fd2 seg11:0x5fd8 seg11:0x5fe3]
- **Flame on.** When now > ship dword +0x90 (32-bit unsigned), seg11:0x63b4 sets +0x90 = now + 250. If +0x02 is 0 it sets it to 1, calls seg11:0xb09c(1, i) (flame on), sets object +0x26 = 0 and spin +0x12 = 0 and calls seg13:0x6716. The flame therefore appears at most once per 250 ms window. [BYTES seg11:0x63e2 seg11:0x63ee seg11:0x63f6 seg11:0x63fc seg11:0x6402 seg11:0x640a seg11:0x6417 seg11:0x641f seg11:0x6431 seg11:0x6437 seg11:0x6440]
- **OverThruster.** During thrust, when ship byte +0x60 < 4, (`GetAsyncKeyState(VK_CONTROL)` bit 0x8000 in action mode for the player, or DGROUP:0x952a ≠ 0, which the player's joystick flag DGROUP:0x103e sets) and fuel ≥ 30: [BYTES seg11:0x6454 seg11:0x6456 seg11:0x6466 seg11:0x646d seg11:0x6478 seg11:0x6481 seg11:0x648c seg11:0x6498 seg11:0x64b1 seg11:0x64c5 seg11:0x65ab seg11:0x65c6 seg11:0x65d2 seg11:0x65dc seg11:0x65e2]
  - if object +0x1a ≠ 0 it plays sound 0x29 through seg10:0x754a(0x29, pool, i, 1) ([sound.md](sound.md)), sets the 40 entries (stride 0x16) at DGROUP:0x5b2c to words −100, −100 with word +8 = 0, writes DGROUP:0x9512 = DGROUP:0x9324, DGROUP:0x9514 = i and DGROUP:0x9510 = 0, and sets +0x60 = 0x1c when +0x60 was not 0 and the ship is not the player in action mode, else 8 [BYTES seg11:0x650a-0x6516];
  - in every case it then sets k = +0x1c = 5 and subtracts 25 from fuel.
- **Burst.** A burst gives k = 5 on its own call. k then falls by 1 on each later seg11:0x5ab6 call, which runs on 5 of every 9 frames whether or not thrust is applied, down to 1. With +0x60 = 8 the next burst is possible after 5 decrements of +0x60, 33 to 40 frames ([Update loop](#update-loop), step 9). [BYTES seg7:0xafd seg7:0xb01 seg7:0xb08 seg7:0xb17 seg7:0xb1e seg11:0x6478 seg11:0x5c26]
- **Force.** seg11:0x63b4 computes the thrust force T: [BYTES seg11:0x6630 seg11:0x6637 seg11:0x664d seg11:0x6655 seg11:0x6658 seg11:0x665c seg11:0x6669 seg11:0x6674 seg11:0x668c seg11:0x6696 seg11:0x669a seg11:0x669d seg11:0x66a0 seg11:0x66a9 seg14:0x21a9]
  1. T = ship word +0x0c;
  2. with a roster record (+0xd6 ≠ −1), T = (lo16((b + 10)·10)·T)/100 in 32 bits, b the signed roster byte +0x33;
  3. T = T/3 when +0x2a = 0 or fuel = 0; otherwise, when +0x2a < 35, T = T − (lo16((40 − +0x2a)·T) >> 6);
  4. T = max(T, 1).
- **Velocity change.** With h = heading +0x28, k = +0x1c (plus 2 when k > 1 and ship word +0x3a = 5), D = ship word +0x0e and m = object +0x44, thrust adds to each axis, all 16-bit: [BYTES seg11:0x6600 seg11:0x6607 seg11:0x6613 seg11:0x661f seg11:0x6626 seg11:0x66b3 seg11:0x66d4 seg11:0x66dd seg11:0x66e6 seg11:0x66e9 seg11:0x66ec seg11:0x66ef seg11:0x66f0 seg11:0x6707 seg11:0x670a seg11:0x6710 seg11:0x6716 seg11:0x672c]

  ```
  dvx = (lo16(lo16(tabx[h] * T) * k) - lo16(D * vx)) / m
  dvy = (lo16(lo16(taby[h] * T) * k) - lo16(D * vy)) / m
  ```

  Drag is built in, and each axis tops out near 16·T·k/D. [BYTES seg11:0x66e6 seg11:0x672c]
- **Flame length.** seg11:0x63b4 ends by storing object byte +0x2f = lo16(T·k)/20. seg11:0xb09c stores +0x2f = ship +0x0c / 20 when it turns the flame on and 0 when it turns it off. [BYTES seg11:0x684a seg11:0x684d seg11:0x6854 seg11:0x6865 seg11:0xb0eb]
  - In mode 1 seg11:0x63b4 skips the velocity change at seg11:0x65e7 and reaches the store at seg11:0x684a with T ([bp − 0x14]) and k ([bp − 0x10]) never written in that call, so +0x2f takes whatever those stack words hold. [BYTES seg11:0x65e7 seg11:0x6617 seg11:0x66a9 seg11:0x684a]

### Brake
1. seg11:0x6874 runs only when vx or vy ≠ 0. It sets object +0x26 = 0. [BYTES seg11:0x68ed seg11:0x68f3]
2. When now > +0x90 it sets +0x90 = now + 250, and if +0x02 ≠ 0 it clears it and calls seg11:0xb09c(0, i). [BYTES seg11:0x6917 seg11:0x691b seg11:0x6921]
3. With T0 = ship +0x0c, it computes B = m/30 + T0/8 + D + 30 (T0/8 truncated toward zero) and `dv = −((B·v) / m)` per axis, the product in 32 bits and the division by seg5:0x110. [BYTES seg11:0x6933 seg11:0x6935 seg11:0x6937 seg11:0x693b seg11:0x693d seg11:0x6949 seg11:0x694e seg11:0x696d seg11:0x6973]
4. It clamps each change to the velocity: |v| < |dv| gives dv = −v (|·| by seg9:0x88ba). When both changes are 0 it uses dv = −sign(v) per axis, so braking always reaches exactly 0. [BYTES seg11:0x6874 seg9:0x88ba]

### Drift and drag
1. **Flame off.** When neither brake nor thrust runs, seg11:0x5ab6 returns if ship byte +0xe2 ≠ 0 and DGROUP:0x9130 = 0. Otherwise, if +0x02 ≠ 0, it clears it and calls seg11:0xb09c(0, i) and seg13:0x6716, with no 250 ms wait, and then applies one drag variant. [BYTES seg11:0x600d seg11:0x6012 seg11:0x6019 seg11:0x6027 seg11:0x6033 seg11:0x6039 seg11:0x603f]
2. **DGROUP:0x9130 ≠ 0.** `dv = −(lo16(v·D) / (m << 2))`, tested before the auto-brake and `Detail`. [BYTES seg11:0x606d seg11:0x6076]
3. **Auto-brake.** With INI `Braking` (DGROUP:0x8ff6) ≠ 0, the player and action mode: c = m/(−60) − (+0x02 / 8) − D − 30, where +0x02 is already 0, and `dv = lo16(c·v) / m` per axis. [BYTES seg11:0x608e seg11:0x6097 seg11:0x609f seg11:0x60ab seg11:0x60b5 seg11:0x60c9 seg11:0x60e8 seg11:0x60ea seg11:0x60ec seg11:0x60ef seg11:0x60f2 seg11:0x60f6 seg11:0x60f7 seg11:0x610d seg11:0x6112]
4. **`Detail` ≤ 2.** `dv = −(lo16(v·D) / (m << 3))` per axis. [BYTES seg11:0x6117 seg11:0x611c seg11:0x6295 seg11:0x6299 seg11:0x62a0 seg11:0x62a3 seg11:0x62a4 seg11:0x62a6 seg11:0x62b9 seg11:0x62bc]
5. **`Detail` ≥ 3 (default 3).** With h = +0x28 and 32-bit products divided by seg5:0x110 and kept as 16 bits: [BYTES seg11:0x6143 seg11:0x6167 seg11:0x6181 seg11:0x6184 seg11:0x6186 seg11:0x6189 seg11:0x61a0 seg11:0x61b9 seg11:0x61d0 seg11:0x61d8 seg11:0x61f7 seg11:0x620e seg11:0x6241 seg11:0x6249 seg11:0x6252 seg11:0x625d seg11:0x6260 seg11:0x6262 seg11:0x6266 seg11:0x627a seg11:0x6280]

   ```
   sp     = seg8:0x35d0(vx, vy)
   d      = seg8:0x19b0(num = vy, den = vx)
   a      = (d - h) * 23 + 360
   along  = (cos128(a) * D * sp) / (m << 9)
   across = (sin128(a) * D * sp) / (m * 178)
   dvx = (lo16(tabx[h] * along) + lo16(tabx[(h + 4) % 16] * across)) / -20
   dvy = (lo16(taby[h] * along) + lo16(taby[(h + 4) % 16] * across)) / -20
   ```

   When the autopilot flag is set (DGROUP:0x951a for the player, or an AI ship), along and across are first scaled by 4/3. [BYTES seg11:0x6143 seg11:0x6280]

### Helpers
- **Sine table.** The bytes DGROUP:0xf00 … 0xf5a are 91 values for 0 … 90° (0, 2, 4, 6, 9, …, 127, 128, 128, 128). They only approximate sin·128 and differ from round(sin·128) at 57 of the 91 entries (25° is 55, not 54), so no rounding of sin·128 reproduces them. seg6:0x71b2 (sin128) and seg6:0x720c (cos128) take a = (a + 360) % 360 (C `%`, `idiv`) and fold it into 0 … 90 through the table, negating for the lower quadrants. [BYTES DGROUP:0xf00 seg6:0x71b2 seg6:0x720c]
- **Magnitude seg8:0x35d0(a, b).** With 32-bit arguments it returns 1 when g = max(|a|, |b|) + (min(|a|, |b|) >> 1) is 0, and otherwise the low word of ((a·a + b·b) / g + g) >> 1 in signed 32-bit arithmetic, one Newton step of the square root. [BYTES seg8:0x35e3 seg8:0x35f5 seg8:0x3603 seg8:0x360f seg8:0x361b seg8:0x3638 seg8:0x3646 seg8:0x3657 seg8:0x3668 seg8:0x366b seg8:0x366f seg8:0x367b]
- **Direction seg8:0x19b0(num, den).** Pascal with num at [bp+6] and den at [bp+8]. It computes r = num·100 / den in 32 bits, den = 0 counting as 1, and returns a 16-sector direction (first value when num > 0, second otherwise): [BYTES seg8:0x19bf seg8:0x19d8 seg8:0x19f2 seg8:0x19fe seg8:0x1a09 seg8:0x1a14 seg8:0x1a28 seg8:0x1a3b seg8:0x1a45 seg11:0x615f seg11:0x6163]

  | r | Result | Evidence |
  |---|---|---|
  | \|r\| > 500 | 8 / 0 | [BYTES seg8:0x19bf–0x1b46] |
  | 155 … 500 | 7 / 15 | [BYTES seg8:0x19bf–0x1b46] |
  | 66 … 154 | 6 / 14 | [BYTES seg8:0x19bf–0x1b46] |
  | 20 … 65 | 5 / 13 | [BYTES seg8:0x19bf–0x1b46] |
  | −19 … 19 | 4 when den > 0, else 12 | [BYTES seg8:0x19bf–0x1b46] |
  | −64 … −20 | 11 / 3 | [BYTES seg8:0x19bf–0x1b46] |
  | −153 … −65 | 10 / 2 | [BYTES seg8:0x19bf–0x1b46] |
  | −500 … −154 | 9 / 1 | [BYTES seg8:0x19bf–0x1b46] |

### Mass, thrust and drag
- Thrust and drag come only from the ship's design: ship +0x0c (T0) = signed SHP byte +1 × 5 (seg14:0x1c8c, stored at seg14:0x21a9), and ship +0x0e (D) = body byte +0x1c (seg14:0x1ca1, stored at seg14:0x21b1). [BYTES seg14:0x1c8c seg14:0x21a9 seg14:0x1ca1 seg14:0x21b1]
- The design mass DGROUP:0x9704 = lo16(10 · body byte +0x1b) · (lo8(cbw(SHP byte +2)/4) + 100) / 100, the first product an 8 × 8-bit `imul`, the second product 32-bit and the division seg5:0x110 (seg14:0x1e35 … 0x1e8f). It goes to object +0x44 at the ship set-up (seg14:0x20bc). The design thrust is an 8-bit `imul` of the SHP thruster byte by 5 (seg14:0x1c8c). [BYTES seg14:0x1e35 seg14:0x1e50 seg14:0x1e5d seg14:0x1e8a seg14:0x1c8c seg14:0x20bc]
- **Ship flight fields.** seg14:0x21a0–0x22f2 writes, for ship s of design n: byte +0xec = 1; words +0x0c and +0x0e = the design words DGROUP:0x9710 and DGROUP:0x9712 (+0xad·n); word +0x36 = 0; word +0x1c = 1; dword +0x84 = `GetCurrentTime`; byte +0xe3 = DGROUP:0x9716 & 1; bytes +0xe4, +0xe6, +0xe7 and +0xe9 from DGROUP:0x9718, 0x9774, 0x971c and 0x9779; word +0x0a = 1 unless DGROUP:0x91f8 ≠ 1 and s is the player; byte +0x5a = 1; 0 in bytes +0x5c, +0xed and +0x5f and words +0x52, +0x54, +0xa0 and +0xa2; byte +0xe5 = (DGROUP:0x9775 ≠ 0); and the turn word +0x14 ([Turning](#turning)). [BYTES seg14:0x21a0 seg14:0x21a9 seg14:0x21b1 seg14:0x21b5 seg14:0x21bb seg14:0x21c8 seg14:0x21d6 seg14:0x21de seg14:0x220e seg14:0x2219 seg14:0x2241 seg14:0x2284 seg14:0x2294 seg14:0x22a2 seg14:0x22c2 seg14:0x22e4]
- **Fuel and mass tick.** Every 1000 ms (when now passes DGROUP:0x5074, which then becomes now + 1000) seg11:0x1bcc runs for ships i = 0 … DGROUP:0x9316: [BYTES seg11:0x1c3e seg11:0x1c47 seg11:0x1cfe seg11:0x1d26]
  1. fuel +0x1e −= 5 · byte +0xe8 (8-bit signed `imul`), plus +0x0c / −40 while +0x02 ≠ 0, floored at 0; [BYTES seg11:0x1c5c-0x1c73 seg11:0x1c7c seg11:0x1c83 seg11:0x1c88]
  2. with byte +0xe8 ≠ 0, fuel −= 3 more without a floor, and while fuel < 120 (signed) it calls seg11:0x4c72(i) and seg3:0x332e(0x4090418); [BYTES seg11:0x1c92 seg11:0x1c9e seg11:0x1d2d-0x1d61]
  3. a ship whose current weapon slot (ship byte +0xb8 + word +0x18) holds weapon kind 3 moves object byte +0x50 = b toward 0x6d, by (0x6d − b)/2 for the player and (0x6d − b)/5 for the others; [BYTES seg11:0x1ca8 seg11:0x1cac seg11:0x1ccf seg11:0x1cd7 seg11:0x1cf8]
  4. the mass becomes +0x44 = fuel/20 + the design word DGROUP:0x9704 + 0xad·(ship word +0x4e). [BYTES seg11:0x1d07 seg11:0x1d13 seg11:0x1d26]
  - Fuel therefore enters m and slows the thrust and brake response as fuel is added. [BYTES seg11:0x1d26]

### Body thrust
- **Animation.** A body-thrust ship (ship byte +0xe4 = 0, the body's flag bit 0x01 set) animates frames instead of drawing a flame. While +0x02 ≠ 0, object byte +0x2e = 0 and (object +0x1a ≠ 0 or seg13:0x59e8 ≠ 0), every second frame (counter DGROUP:0x1c72 + 2i > 1) the ship-record frame byte +0x5c += the step byte +0x5a. It bounces between 0 and the limit byte +0x5b when ship byte +0xe6 = 0, and wraps to 0 past the limit otherwise. Object +0x12 = frame·(+0x24 + +0x22) + +0x24. [BYTES seg13:0x25f seg13:0x268 seg13:0x280 seg13:0x28a seg13:0x297 seg13:0x2fa seg13:0x311 seg14:0x21de seg14:0x21e6 seg11:0xb09c]
- **Reset.** When a body-thrust ship stops thrusting with frame +0x5c ≠ 0 and object +0x1a ≠ 0, seg13:0x0 sets object +0x12 = +0x24 (frame 0), +0x5c = 0 and the step +0x5a = 1. [BYTES seg13:0x341 seg13:0x378]

### Frame-rate scaling
- Thrust, brake, drag and turning step per wave frame, thrust, brake and drag on 5 of every 9 frames. [BYTES seg7:0xcd3 seg13:0x255]
- The wall-clock tests that touch flight are the 250 ms flame window (+0x90), the control lock-out (+0x84, +400 in kind-6 waves), the 1000 ms fuel and mass tick of seg11:0x1bcc (DGROUP:0x5074) and the race timer DGROUP:0x1c3e in seg11:0x6730. [BYTES seg11:0x63ee seg11:0x5bee seg11:0x1cfe]
- Flight speed in world units per real second therefore scales with the achieved frame rate (`FrameRateSet`, [timing.md](timing.md#rate-settings)), while mass and fuel change per real second. [BYTES seg7:0xcd3 seg13:0x255 seg11:0x63ee seg11:0x5bee]

## Input

### Router
1. **WndProc.** When DGROUP:0x90f8 or DGROUP:0x8e5e is 0, WndProc passes key input on only when DGROUP:0x1212 ≠ 0, DGROUP:0x8f68 ≠ 0 and DGROUP:0x3b94 ≤ now. [BYTES seg2:0x74a seg2:0x751 seg2:0x75c seg2:0x766]
2. **During a pass.** With DGROUP:0x968c ≠ 0 (a game pass is already running) it sets DGROUP:0x8e62 = msg and calls seg11:0x6aa8(msg, wParam, lParam) directly. [BYTES seg2:0x77b]
3. **Otherwise** it stores msg, wParam and lParam in DGROUP:0x8e62, DGROUP:0x8e64 and dword DGROUP:0x1d38, sets DGROUP:0x8e60 = 1 and calls the game pass seg2:0x1c08 at once. That pass sets DGROUP:0x968c and enters seg7:0x0, whose state-1/7 case sees DGROUP:0x8e60 at seg7:0x4e6, calls seg11:0x6aa8 at seg7:0x50e, clears DGROUP:0x8e60 and ends the pass ([globals.md](globals.md#game-tick-seg70x0)). Keys are therefore handled synchronously. [BYTES seg2:0x783 seg2:0x78c seg2:0x792 seg2:0x799]
4. seg7:0x863 delivers only a message still left in DGROUP:0x8e62 on a regular pass. [BYTES seg7:0x84f seg7:0x863]
5. **seg11:0x6aa8** returns at once when DGROUP:0x8e62 = 0 and sends input to seg18:0xd4e while DGROUP:0x9130 ≠ 0. It maps the wParam of WM_KEYDOWN and WM_KEYUP through seg11:0xb306: unless DGROUP:0x94fa = 'Y', a byte found at DGROUP:0x94fa + j (j = 1 … 21) is replaced by byte DGROUP:0x94e4 + j. It then dispatches state 1 to seg11:0x6b64 and clears DGROUP:0x8e62. [BYTES seg11:0x6aa8 seg11:0xb306]
   - **Key map.** seg6:0x1785–0x17a4 loads string 2900 into DGROUP:0x94e4 and then string 2901 into DGROUP:0x94fa, each with cchMax 0x28, although the buffers are only 0x16 bytes apart, so 2901 overwrites the tail of 2900. seg11:0xb306 compares sign-extended bytes. The English strings "YHADTGSCDVPBWGXSIEFSCM" (2900) and "NQADTGSCDVPBWGXSIEFSCM" (2901) differ only at j = 1, so the one mapping is Q → H. [BYTES seg6:0x1785 seg6:0x1789 seg6:0x1790 seg6:0x179b seg6:0x17a2 seg11:0xb313-0xb341] [BYTES ENG_LANG.DLL STRING 182] ids 2900–2901 The oracle's Q acts as H. [ORACLE wave-0001]

### Polled keys
- The arrow keys, Ctrl, Shift and Space are not handled as messages: the key-down and key-up switches of seg11:0x6b64 have no case for them. Thrust, brake and OverThruster are polled in seg11:0x5ab6 and seg11:0x63b4 on thrust frames, and turning in seg11:0x592a ([Flight model](#flight-model)). [BYTES seg11:0x6b64 seg11:0x5f7a seg11:0x5971]
- **Fire.** seg11:0x4db0 runs on every wave pass, not only on frames. It fires (seg11:0x4e0a) on `GetAsyncKeyState(VK_SHIFT or VK_SPACE)` bit 0x8000 or DGROUP:0x1042, gated by DGROUP:0x9134 = 0, the +0x84 lock-out and player `state` = 0. [BYTES seg7:0x871 seg7:0x87a seg7:0x881 seg11:0x4dba seg11:0x4dce seg11:0x4dde seg11:0x4dea seg11:0x4df4 seg11:0x4e00]
- **Hold behaviour.** Up and Down are tested with mask 0x8001, so a press shorter than the polling interval still acts once through the "pressed since the last call" bit. The turn keys use only 0x8000. [BYTES seg11:0x5f81 seg11:0x5fb9 seg11:0x5978 seg11:0x5a16]

### Key messages (seg11:0x6b64)
- **Gate.** seg11:0x6b64 returns without acting while the player's dword +0x84 > now or the player's `state` ≠ 0, except for WM_KEYUP 'P' and WM_KEYDOWN 'H'. On WM_KEYUP it first handles ESC, and 'R' when DGROUP:0x95bc = 0, as in [shell.md](shell.md#exit). While DGROUP:0x95bc = 0 (the demo) WM_KEYDOWN accepts only VK_ADD, VK_SUBTRACT, 'W', VK_OEM_MINUS and VK_OEM_PLUS. [BYTES seg11:0x6b64 seg11:0x6cde seg11:0x6cfd]
- **Decode.** The WM_KEYDOWN decode at seg11:0x6e42–0x6ee4 tests VK_OEM_MINUS (0xbd) first and sends every key above 0xbd (unsigned) to seg11:0x72a1. It then sends Tab to seg11:0x71e3, Enter and Z to seg11:0x6f58, Delete to seg11:0x6ee7, A to seg11:0x7065, B to seg11:0x71d4, C to seg11:0x7019, D to seg11:0x6f20, E to seg11:0x70a5, F to seg11:0x6feb, G to seg11:0x6fff, H to seg11:0x6f7a, I to seg11:0x716c, M to seg11:0x71a9, R to seg11:0x718b, S to seg11:0x7033, T to seg11:0x7132, U to seg11:0x7183, V to seg11:0x7187, W to seg11:0x7127, X to seg11:0x6f3a, Y to seg11:0x717f, VK_ADD (0x6b) and VK_OEM_PLUS (0xbb) to seg11:0x71ef, and VK_SUBTRACT (0x6d) and VK_OEM_MINUS to seg11:0x724e. Any other key goes to seg11:0x72a1. [BYTES seg11:0x6e45 seg11:0x6e4c seg11:0x6e50 seg11:0x6e92 seg11:0x6e9e seg11:0x6eaa seg11:0x6eb0 seg11:0x6eb6 seg11:0x6ec6 seg11:0x6ed2 seg11:0x6ed8 seg11:0x6ede seg11:0x6ee4]
- **Keys:**

  | Key | Message | Action | Evidence |
  |---|---|---|---|
  | VK_ADD, VK_OEM_PLUS | WM_KEYDOWN, no Ctrl or Alt | radar zoom DGROUP:0xa6c = min(zoom + 8, 500), then the [scrollbar](#scrollbar) and the radar | [BYTES seg11:0x7215 seg11:0x7233 seg11:0x7242 seg11:0x724e] |
  | VK_SUBTRACT, VK_OEM_MINUS | WM_KEYDOWN, no Ctrl or Alt | zoom = max(zoom − 8, 100), then the same | [BYTES seg11:0x7268 seg11:0x7279] |
  | T | WM_KEYDOWN, no Ctrl | seg11:0x772e(ctrl, 0) | [BYTES seg11:0x772e seg11:0x719c seg11:0x71c6] |
  | Ctrl+T | WM_KEYDOWN | toggles DGROUP:0xa62; turning it on calls seg13:0x2206 and seg3:0xf4; either way posts WM_PAINT to DGROUP:0xa7c | [BYTES seg11:0x772e seg11:0x719c seg11:0x71c6] |
  | R, I (no Alt) | WM_KEYDOWN | seg11:0x772e(ctrl, 1) and seg11:0x772e(ctrl, 2) | [BYTES seg11:0x772e seg11:0x719c seg11:0x71c6] |
  | Y, V, U | WM_KEYDOWN | seg11:0x772e(ctrl, 3), (ctrl, 4) and (ctrl, 5) | [BYTES seg11:0x772e seg11:0x719c seg11:0x71c6] |
  | Alt+I | WM_KEYDOWN | posts 0x9aa to the main window (the icon column, [shell.md](shell.md)) | [BYTES seg11:0x71c6] |
  | Alt+R | WM_KEYDOWN | posts 0x9b4 to the main window | [BYTES seg11:0x71c6] |
  | M | WM_KEYDOWN, outside the demo | sets the IS_INFO mode DGROUP:0xa76 to 3 unless it is already 3, then posts WM_PAINT to DGROUP:0xa7c when that window exists | [BYTES seg11:0x6e9a seg11:0x71a9 seg11:0x71b0 seg11:0x71b6 seg11:0x71cc] |

- The plus and minus keys with Alt (or message 0x99b) go to seg10:0x7e1c(±1) instead, and with Ctrl (or message 0x99e) to seg10:0x9a2a(±1). Keyboard auto-repeat repeats each step. [BYTES seg11:0x7215 seg11:0x7268]
- **Target codes.** seg11:0x772e(ctrl, code) sets DGROUP:0xa62 = 1. If the target display mode DGROUP:0xa76 ≠ 2, it sets the mode to 2, posts WM_PAINT to DGROUP:0xa7c and returns when seg3:0x65e(DGROUP:0x6e54, DGROUP:0x6e52, code) is non-zero. If the mode is already 2 and ctrl is set, it sets DGROUP:0x6e54 = 3 and DGROUP:0x6e52 = 0. It then calls seg3:0x45a(code). [BYTES seg11:0x772e]
- **Suit test.** seg3:0x65e(pool, rec, code) accepts only a record that seg3:0x0(pool, rec) accepts, never the player's ship, and then by code: 0 any, 1 pool 14, 2 pools 0 and 11, 3 pool 8 with word DGROUP:0x3d6c + 0x34·rec = 2 and DGROUP:0x3d5a + 0x34·rec > 0, 4 pool 9, 5 pool 2, and any other code (compared unsigned) any. [BYTES seg3:0x66d seg3:0x675 seg3:0x67b-0x687 seg3:0x6a2-0x6b8 seg3:0x6cb seg3:0x6d3]
- **Retarget.** seg3:0x45a(code) starts from the current target (DGROUP:0x6e54, DGROUP:0x6e52). Code 6 starts from (DGROUP:0x6e90, DGROUP:0x6e8e) instead and becomes code 1 when that pool is 14, else code 5. [BYTES seg3:0x45a seg3:0x46d seg3:0x47f]
  1. When the current target does not suit the code, it picks the nearest suitable record within 20000 (seg8:0x285a from the player) among pools 0 … 14, skipping records at or before the current one in its pool. [BYTES seg3:0x4ad seg3:0x4b5 seg3:0x4b9 seg3:0x4d4 seg3:0x51c seg3:0x524]
  2. When the target suits, or nothing is near enough, it picks the next suitable record after the current one, then wraps to the start. Both passes start at pool 14 for code 1. [BYTES seg3:0x54b seg3:0x55b seg3:0x56e seg3:0x5c4]
  3. Otherwise it picks pool 12 record 1. [BYTES seg3:0x612]

### Joystick
- **Poll.** With INI `JoyStick` (DGROUP:0x8ff4) on, seg11:0x7504 runs every wave frame. It clears DGROUP:0x103e, DGROUP:0x1042, DGROUP:0x1044 and DGROUP:0x1046 and reads 4 buttons through seg11:0x7588. [BYTES seg7:0xb37 seg7:0xb5f seg11:0x7504]
- **Read.** Every 50 ms ([Update loop](#update-loop), step 10) seg11:0x72b2 reads joystick 0 through ISPACE.DLL ordinal 41 (seg10:0x6b54, its address from GetProcAddress at seg10:0x6ad3) while the player's ship byte DGROUP:0x7172 + 0x15a·DGROUP:0x9646 is 0, and clears DGROUP:0x8ff4 when the read returns 0xa7. [BYTES seg11:0x72c2 seg11:0x72c9 seg11:0x72ce seg11:0x72d3 seg10:0x6b65 seg10:0x6ad3]
- **Buttons.** seg11:0x7588 acts on a button on every call while it is held when its configuration is 0 or 3, and once per press for the other configurations; it dispatches configurations 0 … 6 through the jump table at seg11:0x7639, and larger values do nothing. [BYTES seg11:0x75a3 seg11:0x75a9 seg11:0x75b0 seg11:0x762c seg11:0x7634 seg11:0x7639]
- **Stick.** It maps the axes x/0x3444 and y/0x3444 (unsigned, each clamped to 4) through the 5 × 5 byte table DGROUP:0x1018, indexed y·5 + x, to a target heading or −1 at the centre: [BYTES seg11:0x7504 DGROUP:0x1018]

  | y \ x | 0 | 1 | 2 | 3 | 4 |
  |---|---|---|---|---|---|
  | 0 | 14 | 15 | 0 | 1 | 2 |
  | 1 | 13 | −1 | −1 | −1 | 3 |
  | 2 | 12 | −1 | −1 | −1 | 4 |
  | 3 | 11 | −1 | −1 | −1 | 5 |
  | 4 | 10 | 9 | 8 | 7 | 6 |

- **Steering.** seg11:0x7650(target) in action mode sets DGROUP:0x1040 (thrust) = 1 only when target = heading. Otherwise it clears DGROUP:0x1040 and, for target ≥ 0, sets DGROUP:0x1044 (left) when \|target − heading\| < 9 and target ≤ heading, or when \|target − heading\| ≥ 9 and heading < target, and DGROUP:0x1046 (right) otherwise. The stick turns the short way and thrusts once aligned. [BYTES seg11:0x7650 seg11:0x7695 seg11:0x769e seg11:0x76fe seg11:0x771f]
- The OverThruster takes the joystick flag DGROUP:0x103e and fire DGROUP:0x1042 ([Thrust](#thrust), [Polled keys](#polled-keys)). [BYTES seg11:0x6466 seg11:0x4dde]

### Mouse
- seg11:0x592a, seg11:0x5ab6 and seg11:0x63b4 read no mouse input. [BYTES seg11:0x592a seg11:0x5ab6 seg11:0x63b4]

## View
- **Origin.** seg8:0x0 runs each wave frame at seg7:0xb8c, before the ships move at seg7:0xcd3. With DGROUP:0x951a = 0, Ww = W << s and Hw = H << s (16-bit), it sets the origin DGROUP:0x2400 = x − Ww/2 and DGROUP:0x2402 = y − Hw/2 from the player's ship, the far corner DGROUP:0x2404/0x2406 = origin + (Ww, Hw), and the sign-extended copies DGROUP:0x240c/0x2410. [BYTES seg8:0x0 seg7:0xb8c seg8:0x14 seg8:0x1b seg8:0x24 seg8:0x747 seg8:0x757]
- **Shake.** With DGROUP:0x238c ≠ 0 a screen-shake offset (DGROUP:0x2390, DGROUP:0x2392)·8 is added to the origin before the far corner is computed. [BYTES seg8:0x0]
- **World to screen.** seg8:0x29ea maps a point to the screen as (x − X0) >> s (32-bit `sar`). It first adds DGROUP:0x5a0·8 when the shifted difference is negative, or subtracts it otherwise, when \|x − (W << (s − 1)) − X0\| > DGROUP:0x5a0·4, and the same for y. The wrap is skipped in kind-6 waves unless DGROUP:0x9eb5 = 1, and entirely while DGROUP:0x9130 ≠ 0. [BYTES seg8:0x2a1b seg8:0x2a27 seg8:0x2a35 seg8:0x2a4a seg8:0x2a51 seg8:0x2a69 seg8:0x2a7e seg8:0x2aab seg8:0x2ab7]
  - The x test is 16-bit: x − (W << (s − 1)) − the low word DGROUP:0x240c, taken with the 16-bit `abs` seg9:0x88ba and compared signed with 12800. The y test takes a 32-bit difference with the dword DGROUP:0x2410, the 32-bit `abs` seg9:0x88dc and a 32-bit compare with DGROUP:0x5a2·4 sign-extended. [BYTES seg8:0x2a2f seg8:0x2a53 seg8:0x2abf seg8:0x2afc seg9:0x88ba seg9:0x88dc]
- **Pinned player.** seg7:0x11cc sets the player's screen position to (W/2, H/2), so the player stays centred and the world scrolls. At 1024×768 that is (512, 341), which gives the oracle's ship centre (528, 357) for a 32-px cell. [BYTES seg7:0x11e8 seg7:0x11fc] [ORACLE wave-0001]
- **Activity rectangles.** seg8:0x0 also builds the activity rectangles at DGROUP:0x23e0, counted in DGROUP:0x9158 (1, 2 or 4): the view grown by 600 units on each side, split at the 25600 wrap into two or four rectangles when the player is within view/2 + 600 of an edge. A race before DGROUP:0x9eb5 = 1 uses one rectangle. [BYTES seg8:0x0]

## Drawing
- **Surface.** All wave drawing goes straight to the window DC DGROUP:0x13dc. There is no back buffer; memory DCs serve only as blit sources. [BYTES seg11:0xb33 seg8:0xb76]

### Sprites (seg8:0xb76)
seg8:0xb76 is Pascal (`retf 8`) with the parameters (pool·256 + rec, kind, srcDC, masked) in declaration order. [BYTES seg8:0xb76]
1. **Early exits.** It returns 0 without drawing when +0x1a = 0, the pool is not 7, kind is not 4 or 5 and +0x32 = −1; this test runs for every record, the player's included. Kind 4 returns 1 without drawing, and so does kind 5 when +0x1a = 0. [BYTES seg8:0xb76 seg8:0xba8 seg8:0xbc5 seg8:0xc85 seg8:0xc8d]
2. **Position.** For a record other than the player's, or for the player while DGROUP:0x951a ≠ 0, it saves (+0x00, +0x02) in +0x08/+0x0a and sets (+0x00, +0x02) = seg8:0x29ea(dword +0x04). [BYTES seg8:0xb76]
3. **Source.** It stores the caller's srcDC in object word +0x3a. When byte +0x2e ≠ 0 it takes srcDC from +0x3c and the source (+0x10, +0x12) from (+0x24, +0x24). It does the same for pool 7 and for ship DGROUP:0x952e while DGROUP:0x9e49 = 3, or 6 with DGROUP:0x9e47 = 15. [BYTES seg8:0xcaa seg8:0xcbb seg8:0xcfd seg8:0xd51]
   - **Draw flag.** The flag holds for +0x2e = 0x00 … 0x7f; the test is signed (`cmp byte, 0xff; jg`), so any value from 0x80 to 0xff, 0xfe included, clears it. Without the flag kind 6 and every kind other than 7 draw nothing, while kind 7 draws its dot. [BYTES seg8:0xcc5 seg8:0xcf1 seg8:0xcf6 seg8:0xcf8 seg8:0xd73 seg8:0xdb5 seg8:0xde7]
4. **Erase.** With dx, dy the move (each lowered by 2 more when negative, with MERGECOPY only): [BYTES seg8:0xb76]
   - when \|dx\| and \|dy\| are both ≤ the gap +0x24, it erases nothing and makes one larger BitBlt of (size + \|dx\|) × (size + \|dy\|) from the source, offset by max(d, 0);
   - when both are < the size +0x22, it PatBlts BLACKNESS over the uncovered L-shaped strips;
   - otherwise it blacks the whole old cell at (+0x08, +0x0a).
5. **Blit.** It BitBlts the size × size cell from srcDC at (+0x10, +0x12), SRCCOPY when masked = 0 and MERGECOPY 0x00C000CA otherwise. [BYTES seg8:0xb76]
- **Player.** For the player's ship with DGROUP:0x951a = 0, while DGROUP:0x238e = 0 it neither recomputes the position nor erases: it BitBlts SRCCOPY, whatever the masked flag, at the stored (+0x00, +0x02). When +0x2e = 0xfe it also PatBlts ROP 0x00A000C9 with brush DGROUP:0x5ea8 over the cell. When DGROUP:0x238e ≠ 0 and DGROUP:0x238c = 0 it pins the position to (W/2, H/2), clears DGROUP:0x238e and takes the normal erase and blit path; with DGROUP:0x238c ≠ 0 it neither pins nor clears. [BYTES seg8:0xb76]
  - The pinned path (seg8:0x10c9) branches at seg8:0xbe5 past the kind-4 and kind-5 exits and never reads the kind. It also stores srcDC in +0x3a. +0x2e = 0 or 0xfe keeps the caller's source, any other value takes the alternate source, and only 0xff clears the draw flag. It takes the alternate source too when DGROUP:0x952e = DGROUP:0x9646 in the modes of step 3, and when DGROUP:0x913a or DGROUP:0x913c = 2. [BYTES seg8:0xbe5 seg8:0x10c9 seg8:0x10da seg8:0x10e4 seg8:0x10eb seg8:0x1117 seg8:0x1123 seg8:0x1126-0x11a9]
  - The re-pin through DGROUP:0x238e (seg8:0xc4b) does not save (+0x00, +0x02) into +0x08/+0x0a; only the path at seg8:0xc00 does. [BYTES seg8:0xc00 seg8:0xc4b seg8:0xc6c seg8:0xc7b]
- **Kinds 6 and 7.** Kind 7 draws a 3 × 3 WHITENESS (0x00FF0062) dot at (+0x00, +0x02). Kind 6 blits with SRCPAINT 0x00EE0086 and does not erase. [BYTES seg8:0xb76]

### The player's sprite (seg7:0x1208)
- **Conditions.** seg7:0x1208 draws the player's ship only in state 1 with player `state` = 0, DGROUP:0x951a = 0, DGROUP:0x9130 = 0 and DGROUP:0x3a38 ≠ 0. DGROUP:0x3a38 is slot 0x28 of the DC table DGROUP:0x39e8 ([structs.md](structs.md#image-slots)), the image of ship record 0, so the test concerns the player's image only when DGROUP:0x9646 = 0. [BYTES seg7:0x1257 seg13:0xa9b seg13:0xb4f seg11:0xaf23 seg14:0x243] It skips the draw when DGROUP:0x9e49 is 3 … 6 and the player is DGROUP:0x952e. It also skips it when ship byte +0xec = 0 and an entry of the effect table DGROUP:0x1e90 (indices 0 … DGROUP:0x9434) has +6 = 0x0e, +8 = the player and +0 in 1 … 8. [BYTES seg7:0x1208 seg7:0x12e4 seg7:0x139d]
- **Frame.** It sets DGROUP:0x1e5c to the cell inset by 2. For ship types (word +0x10, DGROUP:0x70a0) 0 and 7 it sets the source x +0x10 = heading·(size + gap) + gap, with size +0x22 and gap +0x24; for other types, 16·size + 17·gap. It sets the source y +0x12 = 2·gap + size for types > 3, gap for types ≤ 3 with ship byte +0xe9 (DGROUP:0x7179) set, and leaves it unchanged otherwise. The products are 16-bit. [BYTES seg7:0x1208 seg7:0x12e4 seg7:0x139d]
- **Blit.** It calls seg8:0xb76(0x0e00 + ship, 1, word DGROUP:0x39e8[slot + 0x28], 0) and clears DGROUP:0x58a. The slot is the ship index when ship byte +0xe4 ≠ 0, else word +0x50 (DGROUP:0x70e0). [BYTES seg7:0x1208 seg7:0x139d]
- Rocks take their frame from the rotation +0x26 ([Rocks](#rocks-seg70x3bae)). [BYTES seg7:0x3bae]

### Icon dissolve (seg7:0x3a42)
- seg7:0x3a42(i) is the icon-dissolve step. It decrements record byte +0x51 by DGROUP:0x8ffe, with a floor of 0. [BYTES seg7:0x3a42 seg7:0x3a6a seg7:0x3a79]
- **Off screen.** For a record with word +0x1a = 0 it returns at once when seg13:0x59e8(0, i) returns 0, skipping the blit, the redraw and the end handling for that frame. [BYTES seg7:0x3a86 seg7:0x3a92 seg7:0x3a9c]
- **Blit.** It StretchBlts (COLORONCOLOR) the icon's cell in the icon strip DC DGROUP:0x3a02 at x = (gap + size)·i + gap from a source 2 px larger on each side, then BitBlts the cell to the icon's screen square. Size +0x22 and gap +0x24 come from pool-0 record 0, not from record i. [BYTES seg7:0x3a42 seg7:0x3a55 seg7:0x3a5c]
- **Player redraw.** After the blit it redraws the player's ship (seg7:0x1208) when record word +0x48 = DGROUP:0x9646, `Detail` (DGROUP:0x9208) > 1 and byte +0x51 is odd. [BYTES seg7:0x3b24 seg7:0x3b2a seg7:0x3b31 seg7:0x3b38]
- **End.** When the counter has fallen below 1 it blacks both the screen square and the strip cell, then calls seg7:0x65aa(0, i) and seg7:0x2080(0, −1). [BYTES seg7:0x3b49 seg7:0x3b8d seg7:0x3b9d]

### Rock images (seg10:0x3a2e)
- seg10:0x3a2e returns at once while the slot-6 DC DGROUP:0x39f4 is non-zero. Otherwise it sets DGROUP:0x740 = 0 and creates the rock images of 18 cells, big 43 px and small 21 px with 5-px gaps: slot 6 as CreateBitmap(869, 53, 1, 1) and slot 7 as a compatible 869 × 53 bitmap, slot 8 as CreateBitmap(473, 31, 1, 1) and slot 9 as a compatible 473 × 31 bitmap. [BYTES seg10:0x3a3d seg10:0x3a46 seg10:0x3a4b seg10:0x3aa1 seg10:0x3acd seg10:0x3af7 seg10:0x3b05 seg10:0x3b28 seg10:0x3b36]
- It BitBlts bitmap ROCKS (DGROUP:0x745) SRCCOPY into slot 6 at (48k + 5, 5), 43 × 43 from (43k, 0), and stretches each cell into slot 8 at (26k + 5, 5), 21 × 21, in stretch mode 2. [BYTES seg10:0x3a2e DGROUP:0x745]
- With DGROUP:0x90f0 or DGROUP:0x8fe8 set it does the same with seg10:0x6c02(50, 3, −1) into slots 7 and 9 in stretch mode 3. Otherwise it MERGECOPYs slot 6 into 7 and slot 8 into 9 with a 0x808080 brush. It ends with seg10:0x6308(7/6/9/8, −1, −1, −1). [BYTES seg10:0x3a61 seg10:0x3a68 seg10:0x3e80]
- The mover's slot choice is in [Rocks](#rocks-seg70x3bae). [BYTES seg7:0x40b2]

### Colour depth
- The radar's brush table has variants for DGROUP:0x90f0 = 0 (fewer than 8 bits per pixel, [shell.md](shell.md#start-up)) ([Radar colours](#radar-colours)). [BYTES seg11:0x0]
- The target display's text is white when DGROUP:0x90ee = 1 ([IS_INFO displays](#is_info-displays)). [BYTES seg11:0x238e]

## Play-area frame and status display
- **Gate.** The whole HUD block seg11:0xb77–0x15b0 (layout, child windows, border, panels) runs only when seg11:0x2120 returns 1: brush DGROUP:0x500a ≠ 0, DGROUP:0x9130 = 0 and state DGROUP:0x90f4 is 1 or 7. Otherwise seg11:0xafe goes straight to the state dispatch at seg11:0x15b3 ([shell.md](shell.md#screen-states)). [BYTES seg11:0xb6e seg11:0x212a seg11:0x2131 seg11:0x2138 seg11:0x213f]
- **DC.** seg11:0xafe draws on DGROUP:0x13dc. When that is 0 it takes GetDC(main DGROUP:0x16) and releases it at the end. It selects and realises the palette DGROUP:0x920e when that is non-zero. [BYTES seg11:0xb33 seg11:0xb43 seg11:0xb5c seg11:0xb68 seg11:0x16f7]
- **Repaint path.** The main window's repaint path seg2:0x970 calls seg11:0xafe only when DGROUP:0x8f64 ≠ 0. It first forces DGROUP:0x8f64 = 1 in states 6 and 8, and in state 0 when DGROUP:0x95b8 is 4 or 5. When DGROUP:0x8f64 = 0 it skips the repaint once and sets the flag to 1. [BYTES seg2:0x97a seg2:0x9a9 seg2:0x9ae seg2:0x9b0 seg2:0x9b7]
  - The wave set-up seg9:0x6624 clears DGROUP:0x8f64 at seg9:0x6649 only when the window DGROUP:0x934c exists, which it then destroys; seg9:0x6975 does the same later in the routine. [BYTES seg9:0x6649 seg9:0x6975]
  - The flag is also cleared at seg10:0x844a and seg10:0xa0c6, set at seg2:0xb61 when DGROUP:0x1e ≠ 0, and set from seg13:0x65f2(0) by seg11:0x3e57/0x3e5f. [BYTES seg10:0x844a seg10:0xa0c6 seg2:0xb61 seg11:0x3e57 seg11:0x3e5f]
- **Damage-lamp repaint.** After composing the lamps ([Status panels](#status-panels)), seg11:0x3cc2 checks DGROUP:0x951a. When it is non-zero it blits only the damage panel to (DGROUP:0x93c0, H' + 24) and calls seg11:0x5fe(1). Otherwise it runs the full repaint seg11:0xafe and then sets DGROUP:0x8f64 = (seg13:0x65f2(0) ≠ 0). [BYTES seg11:0x3de9 seg11:0x3e28 seg11:0x3e41 seg11:0x3e48 seg11:0x3e4d seg11:0x3e57 seg11:0x3e5f]
- The oracle shows the full status display (circuit-board border, bevel, SCORE, fuel, clock, damage) only after a while into wave 1. [ORACLE wave-0001]

### Layout
seg11:0xafe computes the layout for any W, with W/2 a signed halving toward zero (`cwd`/`sub`/`sar`). [BYTES seg11:0xb77 seg11:0xb8d seg11:0xba3 seg11:0xbaf seg11:0xbc1 seg11:0xbcf seg11:0xbe9 seg11:0xbfd seg11:0xc0b seg11:0xc19 seg11:0xc26 seg11:0xc2c seg11:0xc35 seg11:0xc51]

| Global | Formula | At 1024 | Evidence |
|---|---|---|---|
| DGROUP:0x93b8 (L), DGROUP:0x93dc (Lw) | (1, 0x49) if W < 660; (1, 0x5a) if W < 760; else (4, 0x86) | 4, 134 | [BYTES seg11:0xb77–0xc58] |
| DGROUP:0x5024 | W/2 − Lw − 96, +9 if W < 700 | 282 | [BYTES seg11:0xb77–0xc58] |
| DGROUP:0x93ba | Lw + L + 4, −2 if W < 700 | 142 | [BYTES seg11:0xb77–0xc58] |
| DGROUP:0x93be | W/2 − 50 | 462 | [BYTES seg11:0xb77–0xc58] |
| DGROUP:0x93bc | DGROUP:0x93be − 30, −4 more if W ≥ 700 | 428 | [BYTES seg11:0xb77–0xc58] |
| DGROUP:0x93c0 | W/2 + 68, −14 in a race (DGROUP:0x94e ≠ 0) | 580 | [BYTES seg11:0xb77–0xc58] |
| DGROUP:0x93c2 | DGROUP:0x93c0 + 28, after the race adjustment | 608 | [BYTES seg11:0xb77–0xc58] |
| DGROUP:0x93c4 | DGROUP:0x93c0 + 88 | 668 | [BYTES seg11:0xb77–0xc58] |
| DGROUP:0x5026 | W/2 − 160, +4 if W < 700, +14 in a race | 352 | [BYTES seg11:0xb77–0xc58] |

- At 1024×768, H' = 683 and the formulas reproduce the oracle's child geometry: the scrollbar at (562, 683) 14 × 85, IS_INFO at (147, 688) 273 × 76 and IS_INFO at (673, 688) 343 × 76. [BYTES seg11:0xcd9 seg11:0xd4e seg11:0xd9f seg2:0xc4e] [ORACLE newgame-0002]

### Child windows
Every HUD repaint destroys and recreates three child windows, parent DGROUP:0x16 and hInstance DGROUP:0x8e66. [BYTES seg11:0xc5d seg11:0xc97 seg11:0xca2 seg11:0xca7 seg11:0xcd9 seg11:0xceb seg11:0xd00 seg11:0xd10 seg11:0xd4e seg11:0xd61 seg11:0xd9f]

| Window | Class, id | Style | Position | Size | Evidence |
|---|---|---|---|---|---|
| DGROUP:0xa7a | "scrollbar", 0xc8 | 0x50000001 | (W/2 + 50, H') | 14 × 85 | [BYTES seg11:0xca7 seg11:0xcd9] |
| DGROUP:0xa7c | IS_INFO, 0xd2 | 0x50000000 | (DGROUP:0x93ba + 5, H' + 5) | (DGROUP:0x5024 − 9) × 76 | [BYTES seg11:0xd4e] |
| DGROUP:0xa7e | IS_INFO, 0xdc | 0x50000000 | (DGROUP:0x93c4 + 5, H' + 5) | (DGROUP:0x5026 − 9) × 76 | [BYTES seg11:0xd9f] |

- The scrollbar is created only when the race flag DGROUP:0x94e = 0, though its DestroyWindow runs in a race too ([Scrollbar](#scrollbar)). [BYTES seg11:0xc97 seg11:0xca7]
- The oracle's 18 creations in wave-0001 fit this repaint after each dialog (6 repaints × 3). [ORACLE wave-0001]

### Border and bevel
1. **Strip.** The repaint fills the HUD strip with PatBlt(hdc, 0, H', W, 85, PATCOPY) with brush DGROUP:0x500a (the METAL pattern) selected for the call. [BYTES seg11:0xda7 seg11:0xdcc]
2. **Border switch.** The circuit-board border and the bevel are drawn only when DGROUP:0xa78 ≠ 0. DGROUP:0xa78 is INSPACE.INI [Inner Space] `Border` (default 1), read at seg6:0xd17–0xd20 and stored at seg6:0xd51, its only writer. [BYTES seg11:0xddd seg6:0xd17 seg6:0xd20 seg6:0xd51] The manual's Game Border option turns on a 3D border, and turning it off gives more play area. [MANUAL Display Options] [ORACLE dialogs-0001]
3. **Tiles.** Let off = (58 − W % 58)/2 (10 at W = 1024; 29 when W % 58 = 0), n = W/58 (+1 when off ≠ 0). All tiles are 58 wide, BitBlt SRCCOPY: [BYTES seg11:0xdec seg11:0xe16 seg11:0xe4b seg11:0xe68 seg11:0xe79 seg11:0xea9 seg11:0xec1 seg11:0xefe seg11:0xf1b seg11:0xf3b seg11:0xf7d]
   - the top row at (58i − off, 0) for i = 0 … n, 58 × 58;
   - the left column at (−off, 58i) for i = 1 … H'/58 − 1, then one partial tile of height H' % 58 at (−off, 58·(H'/58));
   - the right column the same at x = off + W − 58.
4. **Tile choice.** Each tile's source is (58·t, 0) in bitmap DGROUP:0x5044, with t from seg11:0x170a. seg11:0x170a keeps the counter DGROUP:0x1852: a non-zero argument resets it to −1, and every call then increments it, sets it to 0 when it exceeds 6, and maps 0 … 6 to t = 0, 1, 2, 1, 0, 2, 1. [BYTES seg11:0xe19 seg11:0xe40 seg11:0x171d seg11:0x1723 seg11:0x1727 seg11:0x175b]
   - The reset call seg11:0xe19(1) runs before the top-row loop and its result is discarded, so it uses up counter value 0. The tiles therefore run t = 1, 2, 1, 0, 2, 1, 0, 1, 2, … on through the top row, the left column and its partial tile, and the right column and its partial tile. [BYTES seg11:0xe19] [ORACLE wave-0001]
5. **Bevel.** For i = 0 … 9, four PatBlt PATCOPY calls use a solid brush PALETTERGB(v, v, v): [BYTES seg11:0xf9e seg11:0xfbb seg11:0xff5 seg11:0x101e seg11:0x107e seg11:0x10fd seg11:0x1184]

   | Band | v | Rectangle (x, y, w, h) | Evidence |
   |---|---|---|---|
   | top | (14 − i)·10 | (2i + 58 − off, 2i + 58, W + 2·off − 116 − 4i, 2) | [BYTES seg11:0xf9e–0x1184] |
   | left | (14 − i)·10 | (2i + 58 − off, 2i + 58, 2, H' − 58 − 4i) | [BYTES seg11:0xf9e–0x1184] |
   | bottom | (12 − i)·20 | (2i + 58 − off, H' − 2 − 2i, W + 2·off − 116 − 4i, 2) | [BYTES seg11:0xf9e–0x1184] |
   | right | (12 − i)·20 | (W + off − 60 − 2i, 2i + 58, 2, H' − 58 − 4i) | [BYTES seg11:0xf9e–0x1184] |

   At 1024×768 the bevel leaves the play area x 68 … 955, y 78 … 662 inclusive. [BYTES seg11:0x1184] [ORACLE wave-0001]
6. **HUD line.** With or without the border, the repaint draws a 1-px PS_SOLID line in 0x00C0C0C0 from (0, H') to (W, H'). [BYTES seg11:0x11ae seg11:0x11cc seg11:0x11dd]

### Clip region and message line
- seg2:0xca4 sets the play-area clip region DGROUP:0x12 = CreateRectRgn(left, top, W − left, H − bottom), deleting the old region first. With `Border` on, left = 78 − off (68 at 1024), top = 78 and bottom = 20; with `Border` off all three are 0. `PlayMode` DGROUP:0x9134 ≠ 0 adds 20 to bottom. [BYTES seg2:0xcb1 seg2:0xcca seg2:0xcd7 seg2:0xce2 seg2:0xceb seg2:0xcf4 seg2:0xd03 seg2:0xd12]
- It also sets the message-line rectangle DGROUP:0x1d28 … 0x1d2e (x, y, w, h): (left, H − 40, W − 2·left, 20) with `Border` on, and (0, DGROUP:0x10 − 105, W, 20) with it off. [BYTES seg2:0xd1b seg2:0xd22 seg2:0xd54 seg2:0xd69]
- **At wave start.** seg2:0x1ac8 creates DGROUP:0x12 as (0, 0, W, H) on a state change, and the briefing entry seg9:0x5cca then replaces it through seg2:0xba4 → seg2:0xca4 (seg9:0x5d14). seg9:0x60bc enters state 1 without a state change, so the region in force at wave start is seg2:0xca4's. [BYTES seg2:0x1b5d seg9:0x5d14 seg2:0xc52 seg2:0xcb1]
- **Use.** The wave pass selects DGROUP:0x12 into DGROUP:0x13dc ([Update loop](#update-loop), step 2). seg2:0x1d58 selects it into a fresh GetDC before it tears the old state down and deletes it. [BYTES seg7:0x842 seg7:0x84a seg2:0x1d65 seg2:0x1d76 seg2:0x1d9e]

## Status panels
The repaint draws, in code order: the SCORE panel, the target frame, the fuel gauge, the radar frame, the damage panel, the clock and the weapon frame. [BYTES seg11:0x11f4 seg11:0x1407 seg11:0x141a seg11:0x147d seg11:0x1490 seg11:0x14f2 seg11:0x15ad]

### Graphics
- **Loads.** seg11:0x0 loads bitmap METAL into DGROUP:0x5032 and makes the pattern brush DGROUP:0x500a = CreatePatternBrush(METAL) at seg11:0xe9. Through seg10:0x6c02(id, mode, −1) it loads graphic 250 into DGROUP:0x5034, graphic 240 into DGROUP:0x5036, graphic 45 (the 174 × 58 strip of three border tiles, [BYTES IGRAPHIC.DLL 4000 45]) into DGROUP:0x5044, and graphic 260 into DGROUP:0x503a in mode 3 when DGROUP:0x90f0 = 0, else mode 0. It loads the bitmaps DIGITAL into DGROUP:0x5038, RADSHIP into DGROUP:0x5046 and SYSTEMS into DGROUP:0x5030, and ends by setting DGROUP:0x93d8 = 1 at seg11:0x1d2. [BYTES seg11:0x4c seg11:0x57 seg11:0x5e seg11:0x69 seg11:0x70 seg11:0x7b seg11:0x87 seg11:0x91 seg11:0x94 seg11:0xa5 seg11:0xb0 seg11:0xb4 seg11:0xbf seg11:0xc3 seg11:0xce seg11:0xd2 seg11:0xdd seg11:0xe9 seg11:0x1d2]
- **DIGITAL** is 140 × 10 at 1 bpp: fourteen 10-px cells, the digits 0 … 9, then 10 ':', 11 'C', 12 'R' and 13 'E'. **SYSTEMS** is 32 × 77 at 4 bpp, seven rows of 11 px, the first 'Fuel' and the rest the lamp labels. [BYTES seg11:0xb4 seg11:0xd2] [BYTES IGRAPHIC.DLL BITMAP DIGITAL] [BYTES IGRAPHIC.DLL BITMAP SYSTEMS]
- **Panels.** seg11:0x344 composes the damage panel DGROUP:0x503c as CreateCompatibleBitmap(84, 61) with frame(0, 0, 83, 60, 2), and for k = 0 … 3 frame(5, 14k + 5, 28, 7, 3) and SYSTEMS row k + 1 (source y = 11(k + 1)) at (41, 14k + 5), 32 × 11. It composes the fuel background DGROUP:0x503e as 30 × 85 with frame(0, 0, 29, 84, 2), frame(6, 18, 16, 62, 3) and SYSTEMS row 0 at (5, 5), 21 × 11. The frames are seg10:0x949c (cdecl: dc, x, y, a, b, style). [BYTES seg11:0x37d seg11:0x46c]
- **Status tick.** seg11:0x1bcc calls seg11:0x4a0(0) when the dword DGROUP:0x5018 differs from the score DGROUP:0x1e58. Every 300 ms (deadline DGROUP:0x1854, unsigned) it toggles DGROUP:0x93fe between 0 and 1 and calls seg11:0x4a0(1), and it calls seg11:0x4a0(7) when the low word of DGROUP:0x1e68 / 36 (seg5:0x2b4) differs from DGROUP:0x934e. After the [fuel and mass tick](#mass-thrust-and-drag), which runs only once the 1000 ms deadline DGROUP:0x5074 has passed, it calls seg11:0x4a0(6) and seg13:0x2dcc; it always ends with seg11:0x1d7c. [BYTES seg11:0x1bcc seg11:0x1c36 seg11:0x1c3e seg11:0x1d64 seg11:0x1d6b seg11:0x1d70 seg11:0x1d7b]

### SCORE
- **Box and letters.** At W ≥ 760 the repaint PatBlts BLACKNESS at (L = 4, H' + 12, Lw = 134, 60), then draws the 5 characters of string 2063 "SCORE" (LoadString on DGROUP:0x9212) one by one with seg10:0x94c4(font 0, an 18 × 20 cell; align 0; colour 0x000000FF; x = 22i + L + 25; y = H' + 16). [BYTES seg11:0x11f4 seg11:0x1202 seg11:0x1211 seg11:0x1237 seg11:0x1297 seg11:0x1322 seg11:0x1341 seg11:0x1355] [BYTES ENG_LANG.DLL STRING 129] id 2063
  - At 660 ≤ W < 760 the box is 60 high and x = 18i + L. Below 660 the box is 45 high and seg11:0x2154 draws at x = 14i + L + 6. [BYTES seg11:0x1237 seg11:0x1297]
- **Digits.** seg11:0x4ea draws only when the dword score DGROUP:0x1e58 differs from DGROUP:0x5018. It draws "000" for a score of 0 and otherwise seg6:0xeb8 of the score, right-aligned (align 2): below W = 660 through seg11:0x2154 at (Lw + L − 2, H' + 45), below 760 in font 1 at (Lw + L − 1, H' + 49), and otherwise in font 0 at (Lw + L − 3, H' + 49) (seg10:0x94c4). The colour is 0x02FFC800 or 0x02FFFFFF by DGROUP:0x104c when DGROUP:0x951c ≠ 0, else 0x0200A0FF when DGROUP:0x90f0 ≠ 0 and 0x000080FF otherwise. [BYTES seg11:0x4ff seg11:0x504 seg11:0x511 seg11:0x519 seg11:0x52d seg11:0x56f seg11:0x599 seg11:0x5a1 seg11:0x5f1]
- **DIGITAL text.** seg11:0x2154 draws text in 10 × 10 DIGITAL cells, BitBlt from DGROUP:0x5038 with the colour brush selected, starting at x for align 0, x − 10·len for align 2 and x − (10·len)/2 for align 6; any other align leaves the start uninitialised. Digits draw their cells, the letters S, C, O, R and E the cells 5, 11, 0, 12 and 13, and any other character, ':' included, a BLACKNESS 10 × 10 PatBlt. [BYTES seg11:0x2167-0x21dc seg11:0x21de-0x2246 seg11:0x2248-0x2278 seg11:0x22a0-0x22cb]
- **Digits and edge.** It then sets dword DGROUP:0x5018 = −1, which forces the score digits to redraw, and calls seg11:0x4ea ([globals.md](globals.md#scoring)). It draws a 0x00C0C0C0 line from (Lw + L, H' + 12) to (Lw + L, H' + 12 + h) to (L, H' + 12 + h), h being 45 or 60. [BYTES seg11:0x1366 seg11:0x1370 seg11:0x137b seg11:0x13a2 seg11:0x13be seg11:0x13d6]

### Panel frames
- seg11:0x1774(style, x0, y0, x1, y1) draws three bevelled frames: (2, DGROUP:0x93ba, H', DGROUP:0x93ba + DGROUP:0x5024, H' + 84) round the target display, (4, DGROUP:0x93be, H', DGROUP:0x93be + 99, H' + 84) round the radar, and (2, DGROUP:0x93c4, H', DGROUP:0x93c4 + DGROUP:0x5026, H' + 84) round the weapon display. [BYTES seg11:0x1407 seg11:0x147d seg11:0x15ad]
- seg11:0x1774 (cdecl, the rectangle set with SetRect) draws the bottom edges and the inner top-left in a grey 0x02808080 pen, the right edge in BLACK_PEN (in grey for style 4), the inner bottom-right and outer left and top in WHITE_PEN, then n Rectangles with the silver pen 0x02C0C0C0 and NULL_BRUSH, or LTGRAY_BRUSH for style −1, which otherwise draws as style 2. It ends with the corner pixels (r, t) grey and (r − 1, t + 1), (l, b) and (l + 1, b − 1) silver. [BYTES seg11:0x1774 seg11:0x1781-0x1aa9]

### Fuel gauge
- The fuel gauge background is bitmap DGROUP:0x503e, BitBlt SRCCOPY to (DGROUP:0x93bc, H') at 30 × 85. The repaint then sets DGROUP:0x93d4 = −1 and calls seg11:0x978. [BYTES seg11:0x141a seg11:0x1442 seg11:0x145a seg11:0x1461]
- **Level.** seg11:0x978 computes v = min(59, fuel / (design word DGROUP:0x96f0 / 60)), signed, and does nothing more when v equals the cached DGROUP:0x93d4. [BYTES seg11:0x992-0x9b5 seg11:0x9bc]
- **Low-fuel message.** On a change it raises message 9 (seg3:0x6ec(player, 7, 0, 9)) for each of 250, 190, 130 and 70 that the fuel has dropped below since the last value at DGROUP:0x1840 + 2·ship, only while the 30 s timer DGROUP:0x502c has passed; each message resets the timer to now + 30000. It stores the fuel as the new last value even when the timer blocks the check. [BYTES seg11:0x9c3-0xa0b seg11:0xa1d]
- **Paint.** It paints LTGRAY above the level at (fuel x + 8, H' + 19, 14, 60 − v), then blits graphic 260's column at (fuel x + 8, H' + 79 − v), 13 × v, from row 60 − v. [BYTES seg11:0xa5d seg11:0xacd]

### Damage panel
- The damage panel is bitmap DGROUP:0x503c, BitBlt SRCCOPY to (DGROUP:0x93c0, H' + 24) at 84 × 61. The repaint then calls seg11:0x5fe(1), sets dword DGROUP:0x5028 = now and DGROUP:0x93da = (DGROUP:0x93d8 = 0), and calls seg11:0x894. [BYTES seg11:0x1490 seg11:0x14bb seg11:0x14d6 seg11:0x14de seg11:0x14eb seg11:0x14ef]
- **Lamps.** seg11:0x3cc2(ship) composes the four damage lamps into bitmap DGROUP:0x503c. For k = 0 … 2 the lamp row s is word DGROUP:0x96f4 + 0xad·class + 2k, class being the ship's design index (word +0x4e, DGROUP:0x70de). For k = 3, s is 5 when ship byte +0xe9 (DGROUP:0x7179) ≠ 0, else 4 when ship byte +0xe7 (DGROUP:0x7177) ≠ 0, else the same table entry. [BYTES seg11:0x3d0e seg11:0x3d21 seg11:0x3d2c seg11:0x3d62 seg11:0x3d97 seg11:0x3db2]
  - When s = −1 it paints a 32 × 11 LTGRAY_BRUSH PATCOPY at (41, 14k + 5). Otherwise it BitBlts 32 × 11 SRCCOPY from bitmap DGROUP:0x5030 at (0, 11·(s + 1)) to (41, 14k + 5). [BYTES seg11:0x3d62 seg11:0x3d97]
- The panel's lamps are Engine, Gun, Thrust and Shield. [ORACLE wave-0001]
- **Lamp states.** seg11:0x5fe sets each of the 4 lamps k from the player's ship word +0x26 + 2k: above 75 row 0, above 50 row 1, above 25 row 2, above 0 blinking (0x1e), else grey (0x2a); a design word DGROUP:0x96f4 + 0xad·n + 2k of −1 is grey. It caches the state at DGROUP:0xaa2 + 2k (−2 in the file) and skips the redraw when its argument is 0, the state is not 0x1e and the cache already holds it. A blinking lamp shows row 2 while DGROUP:0x93fe ≠ 0, else grey. A lit lamp is graphic 250's 27 × 6 row (source y = 6·row, SRCCOPY) at (DGROUP:0x93c0 + 6, H' + 30 + 14k); a grey lamp is a 27 × 6 PATCOPY PatBlt of the stock light-grey brush at the same place. [BYTES seg11:0x632 seg11:0x64c seg11:0x666 seg11:0x680 seg11:0x848 seg11:0x6ed seg11:0x6f6 seg11:0x762 seg11:0x768 seg11:0x796 seg11:0x7a2 seg11:0x7fb seg11:0x805 DGROUP:0xaa2]
- **Lamp messages.** seg11:0x5fe raises messages through seg3:0x6ec(player, 7, 0, id) when a state changes against the cache: 0xa when lamp 0 drops to 0x1e, 0xb and 0xc when lamp 1 or 2 drops to 0x2a, and 0x52 and 0x53 when lamp 1 or 2 rises to row 0 while DGROUP:0x951a = 0 and the player is not DGROUP:0x952e. It raises none while DGROUP:0x934e < 4, and none after 0xa in the same call. [BYTES seg11:0x60c seg11:0x691 seg11:0x6a7 seg11:0x70a seg11:0x70e seg11:0x715 seg11:0x736 seg11:0x753 seg11:0x85e seg11:0x866 seg11:0x86a seg11:0x878 seg11:0x880 seg11:0x885]
- **Red button.** seg11:0x894 blinks the red button. Once the dword DGROUP:0x5028 has passed (it then becomes now + 300), it sets DGROUP:0x93d8 = (DGROUP:0x93d8 = 0) when DGROUP:0x9e49 = 0 and the player is DGROUP:0x952e, and DGROUP:0x93d8 = 1 otherwise. When that differs from DGROUP:0x93da it BitBlts SRCCOPY the 20 × 20 cell DGROUP:0x93d8 of graphic 240 (source x = 20·DGROUP:0x93d8) to (DGROUP:0x93c0, H' + 2), x 580 … 599 at W = 1024, and then updates DGROUP:0x93da. [BYTES seg11:0x8a1 seg11:0x96d seg11:0x6c seg11:0x7b] [ORACLE probe-0001]

### Clock
- The clock box is seg10:0x949c(hdc, DGROUP:0x93c2, H' + 1, 55, 17, 0). With a solid 0x0000FF00 brush selected, the repaint then BitBlts (DGROUP:0x93c2 + 23, H' + 4, 10, 10) from bitmap DGROUP:0x5038 at (100, 0) with MERGECOPY 0x00C000CA, deletes the brush and calls seg11:0x1ef6(1). [BYTES seg11:0x14f2 seg11:0x1505 seg11:0x151f seg11:0x1533 seg11:0x1562 seg11:0x158f]
- That blit is the clock's colon, DIGITAL cell 10, at x = 631 at 1024 wide (DGROUP:0x93c2 = 608 in the wave dump). The red button is [seg11:0x894](#damage-panel), at the oracle's x 583 … 597. [BYTES seg11:0x1533 seg11:0x1543 seg11:0x155c] [ORACLE probe-0001] [ORACLE wave-0001]
- **Time.** While seg11:0x2120 passes (DGROUP:0x500a ≠ 0, DGROUP:0x9130 = 0, state 1 or 7), the clock seg11:0x1ef6 computes seconds = dword DGROUP:0x1e68 / 36 (seg5:0x2b4, unsigned long division) and stores its low word in DGROUP:0x934e. With m = seconds / 60 and s = seconds % 60 (signed 16-bit) it draws only when s differs from DGROUP:0x1868, which it then updates, or when its argument is non-zero. [BYTES seg11:0x1f05 seg11:0x2120 seg11:0x1f17 seg11:0x1f1e seg11:0x1f34 seg11:0x1f47 seg11:0x1f5b seg11:0x1f67 seg5:0x2b4]
- **Digits.** The minutes are formatted as %2d (a leading " ", DGROUP:0xac7, below 10) and the seconds as %02d (a leading "0", DGROUP:0xac9). Each digit is a DIGITAL cell blitted MERGECOPY with a solid 0x00FF00 brush at y = H' + 4: minute digit i at clock x + 5 + 10i (ROP 0x00C00746; a blank is a BLACKNESS PatBlt) and second digit i at clock x + 30 + 10i (ROP 0x00C000CA). [BYTES seg11:0x1f9f seg11:0x1fab seg11:0x1f0e-0x20f3 DGROUP:0xac7 DGROUP:0xac9]

## IS_INFO displays
- **Window procedure.** seg11:0x22fa (Pascal, `retf 0xa`) handles two messages. WM_PAINT: InvalidateRect(hwnd, NULL, TRUE), BeginPaint, seg11:0x238e(hwnd, hdc), EndPaint, return 0. WM_LBUTTONDBLCLK: DGROUP:0x93c6 = (GetDlgCtrlID(hwnd) = 0xd2), then seg2:0x88e() with no arguments (the help window, [globals.md](globals.md#game-state-globals)), return 0. Everything else goes to DefWindowProc. [BYTES seg11:0x22fa]
- **Paint.** For every id, seg11:0x238e first takes GetDC(hwnd) when hdc = 0, selects the palette DGROUP:0x920e and fills the client rectangle with BLACKNESS. [BYTES seg11:0x238e]
- **Mode.** DGROUP:0xa76 selects the left window's (id 0xd2) content: 2 the target display, 3 the message display, 4 the frame-rate display. seg9:0x60bc sets it to 2 at wave start ([shell.md](shell.md#screen-states)). [BYTES seg11:0x238e seg9:0x60bc]
  - seg10:0x7e1c sets DGROUP:0xa76 = 4 and DGROUP:0x870c = now + 5000 and posts WM_PAINT to DGROUP:0xa7c only while that window exists ([timing.md](timing.md#speed-keys)). [BYTES seg10:0x7e2a seg10:0x7e35 seg10:0x7f0b]
- **Refresh.** The wave frame recomputes the direction marks every 280 ms ([Update loop](#update-loop), step 19; [Direction mark](#direction-mark)). [BYTES seg7:0xc24]

### Target display (id 0xd2, mode 2)
1. It selects font DGROUP:0x90fe, TA_LEFT and TRANSPARENT, and sets the text colour 0x00FFFFFF when DGROUP:0x90ee = 1, else 0x0000FF00. [BYTES seg11:0x238e]
2. In a race (DGROUP:0x94e ≠ 0) it calls seg11:0x41bc(hdc, 1, 1) instead of the target display ([globals.md](globals.md#game-state-globals)). [BYTES seg11:0x238e]
3. It draws the icon through seg11:0x2ce6(hdc, 0) and seg13:0x58dc(hdc, 1). [BYTES seg11:0x238e]
4. It draws TextOut(1, 0) of the title: DGROUP:0xacb ("") when W < 700, else string 930 "CURRENT "; then string 931 "TARGET  ("; then string 932 "AUTO" when DGROUP:0xa62 = 0, else 933 "MANUAL"; then DGROUP:0xacc ")". [BYTES seg11:0x238e] [BYTES ENG_LANG.DLL STRING 59] ids 930–933
5. With TA_RIGHT at x = DGROUP:0x5024 − m (m = 12 if W < 700, else 15) it draws DGROUP:0x9628 at y 18 in the title colour. [BYTES seg11:0x238e]
   - When the player's ship (first argument = DGROUP:0x9646) targets pool 0 record rec and seg3:0x0(0, rec) or seg13:0x371c(0, rec) returns non-zero, seg3:0xf4 copies the icon record's name DGROUP:0x57b4 + 0x23·rec into DGROUP:0x9628, after first setting it from DGROUP:0x13bf/0x13c1. [BYTES seg3:0x118 seg3:0x11b seg3:0x12f seg3:0x13d seg3:0x149 seg3:0x1cb seg3:0x259 seg3:0x263 seg3:0x26c]
6. It sets the colour by the target. For a ship target (pool DGROUP:0x6e54 = DGROUP:0x9324) the colour comes from r = seg13:0x2688(target, player): 0x02FFB900 when r > 80, 0x02006EFF when r < −80, else 0x02AAAAAA. For any other target it is 0x00008000. In that colour it draws DGROUP:0x8718 at y 32 and DGROUP:0x960a at y 46. [BYTES seg11:0x238e]
7. For ship targets it also calls seg11:0x3f2e(r) and draws DGROUP:0x93e0 at y 60. [BYTES seg11:0x238e]
8. It restores colour and font and draws a 0x00008000 line (0, 14)–(width, 14). [BYTES seg11:0x238e]

### Message display (id 0xd2, mode 3)
- It draws the icon through seg11:0x2ce6(hdc, 1) and seg13:0x58dc(hdc, 1), then TextOut(1, 0) of string 929 "MESSAGE from". [BYTES seg11:0x238e] [BYTES ENG_LANG.DLL STRING 59] id 929
- It draws the sender DGROUP:0x8740 at (1, 18), in a relation colour when the sender is a ship; DGROUP:0x93e0 at (1, 32) unless the sender is the player; and DGROUP:0x8768 at (1, 46) and DGROUP:0x87b8 at (1, 60) in 0x0000FFFF (yellow). [BYTES seg11:0x238e]
- It ends with a 0x00008000 rule (0, 14)–(80, 14). [BYTES seg11:0x238e]

### Frame-rate display (id 0xd2, mode 4)
- It draws TextOut(1, 0) of string 928 "GAME STATUS". At (1, 22) it draws string 2191 "FULL OUT" when DGROUP:0x59c = 1200, else the number DGROUP:0x59c + string 927 " frames/ second" + string 2192 " SET". At (1, 38) it draws DGROUP:0x59a + string 927 + string 2193 " ACTUAL", then the 0x00008000 line (0, 14)–(width, 14). [BYTES seg11:0x238e] [BYTES ENG_LANG.DLL STRING 59] id 928 [BYTES ENG_LANG.DLL STRING 58] id 927 [BYTES ENG_LANG.DLL STRING 137] id 2191 [BYTES ENG_LANG.DLL STRING 138] ids 2192–2193
- DGROUP:0x59c is `FrameRateSet` ([timing.md](timing.md#rate-settings)). [BYTES DGROUP:0x59c]

### Weapon display (id 0xdc)
1. It selects font DGROUP:0x90fe, TA_LEFT, TRANSPARENT and colour 0x000000FF. [BYTES seg11:0x238e]
2. It draws TextOut(1, 0) of string 934 "WEAPON SYSTEM" and TextOut(1, 17) of a prefix and the weapon name. [BYTES seg11:0x238e] [BYTES ENG_LANG.DLL STRING 59] ids 934–935
   - Let j = ship·0x15a + slot, slot = ship word +0x18 (DGROUP:0x70a8). The weapon w is the signed ship byte +0xb8 + slot (DGROUP:0x7148 + j), and its name id is word DGROUP:0x2f54 + 0x50·w.
   - When word DGROUP:0x2f5e + 0x50·w ≠ 0 and word DGROUP:0x2f82 + 0x50·w = 0, the prefix is the decimal count, signed byte DGROUP:0x713e + j, followed by " - " (DGROUP:0xace). Otherwise it is "" (DGROUP:0xad2) for name ids 0xc9, 0xba and 0xbc, and string 935 "FUEL - " for any other id.
3. It restores colour and font and draws a 1-px 0x00FF00FF line (0, 15)–(width, 15), magenta on screen. [BYTES seg11:0x238e seg11:0x2b6b] [ORACLE wave-0001]
4. When DGROUP:0x5040 ≠ 0 it BitBlts that bitmap with SRCPAINT 0x00EE0086 at x = DGROUP:0x5026/2 − bmWidth/2. y is 28 when word DGROUP:0x2f5e + 0x50·w ≠ 0 and the name id is not 0xc4, 0xc6 or 0xcb; 40 when the name id is 0xcb; and 35 otherwise. [BYTES seg11:0x238e]

## Scrollbar
- **Creation.** The scrollbar DGROUP:0xa7a ([Child windows](#child-windows)) gets SetScrollRange(SB_CTL, 100, 500, FALSE) and SetScrollPos(SB_CTL, 600 − DGROUP:0xa6c, TRUE). [BYTES seg11:0xceb seg11:0xcf6 seg11:0xcf9]
- **Inverted zoom.** It shows the radar zoom DGROUP:0xa6c inverted: position = 600 − zoom over 100 … 500. Every SetScrollPos uses 600 − the low word. The oracle's initial position 480 is zoom 120. [BYTES seg11:0xcf6 seg11:0x723a seg11:0x7848] [ORACLE newgame-0002]
- **Handler.** seg11:0x7798 sets the zoom to zoom + 20 for SB_LINEUP, zoom − 20 for SB_LINEDOWN, 600 − pos for SB_THUMBPOSITION and SB_THUMBTRACK, 500 for SB_TOP and 100 for SB_BOTTOM; SB_PAGEUP and SB_PAGEDOWN do nothing. It clamps the zoom to 100 … 500 and, when it changed, calls seg11:0x33cc and SetScrollPos(DGROUP:0xa7a, SB_CTL, 600 − zoom, TRUE). [BYTES seg11:0x7798]
- The plus and minus keys step the zoom by 8 ([Key messages](#key-messages-seg110x6b64)). [BYTES seg11:0x7215]

## Radar
- **Bitmap.** The radar is drawn off-screen into an 86 × 71 bitmap, CreateCompatibleBitmap(DGROUP:0x13dc, DGROUP:0xa72 = 86, DGROUP:0xa74 = 71) into DGROUP:0x5042, selected into the memory DC DGROUP:0x93ce. seg11:0x0 creates it once, while DGROUP:0x5042 is 0. [BYTES seg11:0x37 seg11:0x43 seg11:0x182 seg11:0x18f DGROUP:0xa72 DGROUP:0xa74]
- **Placement.** seg11:0x33cc BitBlts the 86 × 71 radar SRCCOPY to (W/2 − 43, H' + 7), centred in the play width and 7 px below the HUD top, taking GetDC(main) when DGROUP:0x13dc is 0. [BYTES seg11:0x3629 seg11:0x3635 seg11:0x363c seg11:0x3655 seg11:0x3418 seg11:0x3668]
- **RadarLeft and RadarTop.** DGROUP:0x1dd4 and DGROUP:0x1dd6 are read only at start-up and written back while DGROUP:0x12a ≠ 0. No other instruction reads them, so they position nothing, and the radar has no window of its own. [BYTES seg6:0x19b seg6:0x1b1 seg6:0x3b5 seg6:0x3db seg6:0x3a9 seg6:0x6c4]

### Scale and zoom
- **Centre.** seg11:0xafe sets the radar centre DGROUP:0x5050 = 86/2 = 43 and DGROUP:0x5052 = 71/2 = 35. [BYTES seg11:0xb0c seg11:0xb14 seg11:0xb1f]
- **Clip.** seg11:0x33cc sets the clip rectangle DGROUP:0x5048 = (1 − (43 − cx), 1 − (35 − cy), 43 + cx − 1, 35 + cy − 1) = (1, 1, 85, 69). Every radar callback draws only when PtInRect accepts the mapped point (x 1 … 84, y 1 … 68). [BYTES seg11:0x357e seg11:0x35b2 seg11:0x384d seg11:0x39ad seg11:0x3a71 seg11:0x3c47]
- **Divisors.** seg11:0x0 sets DGROUP:0x501c = 100·DGROUP:0x5a0 and DGROUP:0x5020 = 100·DGROUP:0x5a2 (16 × 16 → 32 `imul`), so at zoom 100 the world width spans the 86-pixel radar. [BYTES seg11:0x1b seg11:0x22 seg11:0x29 seg11:0x30]
- **Mapping.** Each radar point is: [BYTES seg11:0x3923 seg11:0x3954 seg11:0x3959 seg11:0x395e seg11:0x3963 seg11:0x3999 seg5:0x110]

  ```
  X = (d * zoom * 86) / DGROUP:0x501c + 43
  Y = (e * zoom * 71) / DGROUP:0x5020 + 35
  ```

  zoom is the dword DGROUP:0xa6c, 86 and 71 the dwords DGROUP:0xa64 and DGROUP:0xa68, the products 32-bit signed and the division seg5:0x110 truncating toward zero. [BYTES seg11:0x3923 seg11:0x3999 seg5:0x110]
- **Wrapped deltas.** seg11:0x1ab6(X) returns the shortest signed torus distance in 1/8-unit cells from a centre c to X' = X >> 3 (32-bit `sar`), W' = DGROUP:0x5a0: [BYTES seg11:0x1ac4 seg11:0x1acb seg11:0x1ae3 seg11:0x1af9 seg11:0x1b03 seg11:0x1b14 seg11:0x1b1f seg11:0x1b33 seg11:0x1b44]
  - c = (DGROUP:0x5a0·4) >> 3 when DGROUP:0x94e ≠ 0; the player's x >> 3 when DGROUP:0x951a = 0; else DGROUP:0x6b50 >> 3;
  - for X' > c it returns X' − c, unless c − X' + W' < X' − c, then −(c − X' + W');
  - for X' ≤ c it returns X' − c + W' when that is < c − X', else X' − c.
  - seg11:0x1b44 does the same for y with DGROUP:0x5a2, +0x06 and DGROUP:0x6b52.
- **Zoom.** DGROUP:0xa6c holds 120 in the file. The plus key path at seg11:0x7215 adds 8 capped at 500, the minus path at seg11:0x7268 subtracts 8 floored at 100, and both call SetScrollPos(DGROUP:0xa7a, SB_CTL, 600 − zoom, TRUE) and repaint the radar at once (seg11:0x7247). [BYTES DGROUP:0xa6c seg11:0x7215 seg11:0x7226 seg11:0x7268 seg11:0x7279 seg11:0x723a seg11:0x7242 seg11:0x7247]

### What the radar shows
1. **Clear and view.** Outside races the repaint clears with PatBlt(0, 0, 86, 71, BLACKNESS) and FrameRects with brush DGROUP:0x5004 the view from mapped (DGROUP:0x2400, DGROUP:0x2402) to mapped (DGROUP:0x2400 + (W << s), DGROUP:0x2402 + (H << s)). It repeats the identical FrameRect DGROUP:0x9158 times. [BYTES seg11:0x343e seg11:0x3452 seg11:0x346e seg11:0x34e0 seg11:0x352a seg11:0x356c seg11:0x3574]
2. **Objects.** It visits pools DGROUP:0x9326 … DGROUP:0x9324, records 0 … in-use − 1, and calls `radar_draw`(pool, record, DGROUP:0x93ce) only for `state` = 0 and signed word +0x44 > 0, with brush DGROUP:0x5004 selected and restored after. [BYTES seg11:0x35c7 seg11:0x35da seg11:0x35ef seg11:0x35f6 seg11:0x35fb seg11:0x3605 seg11:0x3624]
3. **Race.** In a race (DGROUP:0x94e ≠ 0) seg11:0x367c replaces the repaint. It sets zoom = 100, restores each ship's previous 5 × 5 dot from DC DGROUP:0x93d0 at (DGROUP:0x5054 + 4k − 2, DGROUP:0x5056 + 4k − 2) for k = 0 … DGROUP:0x9316, then draws only pool-14 records with `state` 0, centred on the world centre, so the whole course shows. [BYTES seg11:0x3430 seg11:0x3438 seg11:0x36c3 seg11:0x36ed seg11:0x370a seg11:0x3777 seg11:0x377f seg11:0x375f]
   - Its pool loop runs DGROUP:0x9326 … DGROUP:0x9324 but skips every pool except 14, takes records below pool 14's in-use count DGROUP:0x9422 and calls their `radar_draw`(14, i, DGROUP:0x93ce) without the `mass` test. [BYTES seg11:0x370a seg11:0x3736 seg11:0x375f seg11:0x377f]
- **Cloak.** The radar routines (seg11:0x33cc, seg11:0x367c, seg11:0x37a0, seg11:0x3914, seg11:0x39d8, seg11:0x3bae) read only object words +0x04, +0x06, +0x18, +0x28, +0x44, +0x46, +0x4c and the ship byte DGROUP:0x71d7 + 0x15a·k (ship record +0x147). A ship is drawn when +0x18 = 0, +0x44 > 0 (normal path only) and that byte is 0. [BYTES seg11:0x37ba seg11:0x35ef seg11:0x35f6] The manual says even cloaked ships appear on the radar. [MANUAL General]
  - The byte's writers and readers are in [structs.md](structs.md#ship-records-8--0x15a-bytes-at-dgroup0x7090) (+0x147). [BYTES seg16:0x446 seg14:0x14d3 seg13:0x3a93]

### Refresh
- The gameplay pass calls seg11:0x3396 at seg7:0x83d under the gate of [Update loop](#update-loop), step 1. [BYTES seg7:0x83d]
- DGROUP:0x93d6 is 0 (BSS) until seg11:0x33cc first passes its DGROUP:0x93ce and seg11:0x2120 checks and writes it; while it is 0, every seg11:0x3396 call calls seg11:0x33cc. Nothing resets it, so this holds once per process. [BYTES seg11:0x33ad seg11:0x33b1 seg11:0x33b5 seg11:0x33b9 seg11:0x33d9 seg11:0x33e1 seg11:0x3405 seg11:0x3410] The probe's title and navigation dumps hold 0 and its wave dump 6. [ORACLE probe-0001]
- seg11:0x3396 increments DGROUP:0x93de and, when `((((8 − Detail) << 4) >> 6) · DGROUP:0x93d6` (16-bit, `Detail` = DGROUP:0x9208) ≤ the counter, zeroes it and repaints with seg11:0x33cc. At `Detail` ≥ 5 every call repaints. [BYTES seg11:0x33a0 seg11:0x33a7 seg11:0x33aa seg11:0x33ad seg11:0x33b5 seg11:0x33bb]
- Each repaint sets DGROUP:0x93d6 = min(26, lo16(DGROUP:0x9008·1000) / (zoom + 500)), the division seg5:0x110. DGROUP:0x9008 always ends as 4 (seg6:0xe30 overwrites the computed value), giving 6 at zoom 100 … 166 down to 4 at zoom 500. [BYTES seg11:0x33eb seg11:0x33f7 seg11:0x3400 seg11:0x3408 seg6:0xe22 seg6:0xe2d seg6:0xe30]

### Radar colours
- **Brush table.** seg11:0x0 creates the brush table DGROUP:0x5004 + 2i, i = 0 … 7, in the order 3, 0, 7, 6, 2, 4, 1, 5. Entries 0, 2, 5 and 7 are PALETTERGB. [BYTES seg11:0xe9 seg11:0x104 seg11:0x10d seg11:0x115 seg11:0x123 seg11:0x144 seg11:0x155 seg11:0x163 seg11:0x170]

  | i | Colour | With DGROUP:0x90f0 = 0 | Evidence |
  |---|---|---|---|
  | 0 | RGB(0, 0x60, 0), the view frame | RGB(0, 0x40, 0) | [BYTES seg11:0xe9–0x170] |
  | 1 | RGB(255, 192, 64) | — | [BYTES seg11:0xe9–0x170] |
  | 2 | RGB(0, 128, 255) | RGB(0, 63, 255) | [BYTES seg11:0xe9–0x170] |
  | 3 | pattern brush of bitmap METAL (DGROUP:0x500a, the HUD strip) | — | [BYTES seg11:0xe9–0x170] |
  | 4 | RGB(192, 192, 192) | — | [BYTES seg11:0xe9–0x170] |
  | 5 | RGB(255, 64, 0) | — | [BYTES seg11:0xe9–0x170] |
  | 6 | RGB(128, 160, 255) | — | [BYTES seg11:0xe9–0x170] |
  | 7 | RGB(0, 200, 255) | — | [BYTES seg11:0xe9–0x170] |

- **Per pool.** seg10:0xb94 and the placement set the radar fields: [BYTES seg10:0xc9a seg10:0xd90 seg10:0xe98 seg10:0xea2 seg10:0xf16 seg10:0xfb7 seg10:0xfd1 seg10:0x103a seg10:0x10a6 seg10:0x115f seg10:0x1183 seg10:0x1242 seg10:0x1327 seg10:0x1331 seg10:0x13e2]

  | Pool | Callback | Brush or colour | Evidence |
  |---|---|---|---|
  | 0 (icons) | seg11:0x39d8 box | brush 2 (blue); +0x44 = 0xa0 | [BYTES seg10:0xbed seg10:0xc01 seg10:0xc0b] |
  | 1 | seg11:0x3914 pixel | 0x00808000 | [BYTES seg10:0xc9a–0x13e2] |
  | 2 | seg11:0x3914 pixel | 0x0000FFFF | [BYTES seg10:0xc9a–0x13e2] |
  | 3 (rocks) | seg11:0x3914 pixel, or seg11:0x3bae for big rocks | brush 4, 0x00C0C0C0 | [BYTES seg10:0xc9a–0x13e2] |
  | 4 | seg11:0x3bae | brush 5, 0x000000FF | [BYTES seg10:0xc9a–0x13e2] |
  | 5 | seg11:0x3914 pixel | 0x000000FF | [BYTES seg10:0xc9a–0x13e2] |
  | 6 | seg11:0x3914 pixel | 0x00007F00 | [BYTES seg10:0xc9a–0x13e2] |
  | 7, record 0 | seg11:0x37a0 | brush 1 | [BYTES seg10:0xc9a–0x13e2] |
  | 8 | seg11:0x39d8 box | brush 1 or 4 ([Hazards](#hazards)) | [BYTES seg9:0x1218 seg9:0x18cc seg9:0x1be0] |
  | 9 | seg11:0x39d8 box | brush 6 | [BYTES seg10:0xc9a–0x13e2] |
  | 10 | seg11:0x3914 pixel | brush 4, 0x007F7F7F | [BYTES seg10:0xc9a–0x13e2] |
  | 11 | seg11:0x39d8 box | brush 2 | [BYTES seg10:0xc9a–0x13e2] |
  | 14 (ships) | seg11:0x37a0 | brush by attitude (below) | [BYTES seg14:0x2141] |

  The manual lists blue as icons. [MANUAL General]
- **Ships.** Ships get `radar_draw` seg11:0x37a0 (seg14:0x2141) and the brush from design word DGROUP:0x96fe + 0xad·n (seg14:0x20d7), which seg14:0x1e08 sets to 7 on every path. seg13:0x2dcc (called from seg9:0x611c and seg11:0x1d6b) then sets each ship k = 0 … DGROUP:0x9316 to brush 5 (red) when seg13:0x2688(k, player) < −80, 4 (grey) when < 80, else 7 (blue). The player's own record is forced to attitude 500 (seg13:0x2e51) and gets 7, though seg11:0x37a0 draws it with WHITE_BRUSH. [BYTES seg14:0x1e08 seg14:0x20d7 seg14:0x20e6 seg14:0x2141 seg13:0x2def seg13:0x2e04 seg13:0x2e0c seg13:0x2e21 seg13:0x2e38 seg13:0x2e49 seg9:0x611c] The manual gives blue friendly, grey neutral and red enemy. [MANUAL General]

### Callbacks
- **seg11:0x3914** (Pascal pool, record, hdc; `retf 6`) plots SetPixel(x, y, dword +0x4c `radar_color`). [BYTES seg11:0x39b6 seg11:0x39c3 seg11:0x39c8 seg11:0x39d5]
- **seg11:0x39d8** selects brush DGROUP:0x5004[+0x46] and PatBlts PATCOPY with its top-left at (x, y) a box (zoom − 200)/150 + 2 wide and (zoom − 200)/200 + 2 high (16-bit): 2 × 2 at zoom 100 … 349, 4 × 3 at 500. It does not restore the brush. [BYTES seg11:0x3a81 seg11:0x3a8b seg11:0x3a99 seg11:0x3aa5 seg11:0x3ab0 seg11:0x3aba]
- **seg11:0x3bae** draws a square box of side (zoom − 200)/150 + 2 with brush +0x46 when the dword zoom > 200, else one SetPixel in `radar_color`. [BYTES seg11:0x3c50 seg11:0x3c62 seg11:0x3c7d seg11:0x3c8f seg11:0x3cad seg11:0x3cb2]
- **seg11:0x37a0** skips a pool-14 record whose byte DGROUP:0x71d7 + 0x15a·record ≠ 0. It selects WHITE_BRUSH for the player's ship and brush DGROUP:0x5004[+0x46] otherwise, and BitBlts MERGECOPY (0x00C000CA) a 5 × 5 cell of the RADSHIP DC DGROUP:0x93cc from source (5·word +0x28, 0) to (x − 2, y − 2). For pool 7 it copies 4 × 4 from source (80, 0). It does not restore the brush. [BYTES seg11:0x37af seg11:0x37ba seg11:0x3866 seg11:0x3884 seg11:0x3899 seg11:0x38c0 seg11:0x38df seg11:0x38e3 seg11:0x38e9]
  - While DGROUP:0x94e ≠ 0 it stores the mapped point of every record it does not skip in the words DGROUP:0x5054 + 4·record (x) and DGROUP:0x5056 + 4·record (y), whether or not PtInRect accepted the point. [BYTES seg11:0x38ee seg11:0x3901 seg11:0x3905]

## Icons
The walk that turns icons into pool-0 objects is in [Programs and icons](#programs-and-icons). [BYTES seg9:0x0]

### Extraction
- **Count.** seg9:0x156 calls ExtractIcon(hInst DGROUP:0x8e66, path, −1) for the icon count n. With n > 0 it calls ExtractIcon(hInst, path, k) for k = 0 … n − 1 while DGROUP:0x9206 ≤ 9. [BYTES seg9:0x176 seg9:0x178 seg9:0x1a2 seg9:0x1a5 seg9:0x21f seg9:0x229]
- **Cell.** An extracted HICON > 1 (unsigned) is drawn with DrawIcon into the strip DC DGROUP:0x3a02 at (38·slot + 6, 6), after a PatBlt BLACKNESS of the 32 × 32 cell, and is then destroyed with DestroyIcon. The game keeps only the 32 × 32 cell. [BYTES seg9:0x1ad seg9:0x1b6 seg9:0x1d2 seg9:0x1e0 seg9:0x1e8]
- **Strip.** seg10:0x5b30 creates the strip as CreateCompatibleBitmap(DGROUP:0x13dc, 38·N + 6, 44), N = the pool-0 in-use count, blacked out, so icon cells are fixed 32 × 32 with 6-px gaps whatever the `Size`. [BYTES seg10:0x5b3d seg10:0x5b42 seg10:0x5bac seg10:0x5bbd seg10:0x5c0e]
- **Blank cells.** seg9:0x8da compares every pixel of the cell (x … x + 31, 6 … 37) with strip pixel (0, 0). A uniform cell is replaced by the placeholder seg9:0x278(0, slot); otherwise seg9:0x9b0(slot) activates the icon. [BYTES seg9:0x1fc seg9:0x204 seg9:0x20c seg9:0x218 seg9:0x8ef seg9:0x918]

### Placeholder
- **No icons.** A file with icon count 0 calls ExtractIcon(hInst, path, 0) and seg9:0x278(type, −1), type 0x107 when that returned non-zero, else 0. [BYTES seg9:0x24f seg9:0x251 seg9:0x25b seg9:0x264 seg9:0x268]
- **seg9:0x278** cuts the last 4 characters off the found name, picks flag 0 for types 0x106 … 0x107 and 0x20 otherwise, takes a free slot with seg9:0x94c when slot = −1, and calls seg9:0x352(hdc DGROUP:0x3a02, 38·slot + 6, flag, 32, 6, name) and then seg9:0x9b0(slot). [BYTES seg9:0x2f1 seg9:0x2fa seg9:0x307 seg9:0x33c]
- **seg9:0x352**(hdc, x, flag, size, y, name) is cdecl. It first PatBlts BLACKNESS over (x, y, size, size) and drops a 4-character ".ext" from the name. [BYTES seg9:0x352 seg9:0x3c2]
  - With flag ≠ 0 it draws a GRAY 32 × 21 box, a 0x00800000 32 × 9 bar, an LTGRAY 8 × 8 cell, white top and left lines, 0xC0C0C0 bottom and right lines, and black and grey ticks with two black pixels. [BYTES seg9:0x3c7 seg9:0x3cb seg9:0x3f4 seg9:0x41e seg9:0x5c6 seg9:0x5cb]
  - With flag 0 it draws a white top and left frame (to x + 32), a 0x808080 bottom and right frame at y + 20 and x + size − 1, black corner pixels and 0xC0C0C0 inner corners, then the prompt "C:>" (its first letter + DGROUP:0x94ac) in Arial height 10, green 0x0000FF00, at (x + 2, y + 2). [BYTES seg9:0x5ce seg9:0x6aa seg9:0x706 seg9:0x70e seg9:0x7a6]
  - Both then draw the name in "Small Fonts" height 7, white 0x00FFFFFF, TA_CENTER, at (x + 16, y + 26), and end with PatBlt BLACKNESS at (x − y, y, y, size) and (x + size, y, y, size). [BYTES seg9:0x7e1 seg9:0x861 seg9:0x8b1 seg9:0x8cb]

### Icon column
- **Creation.** seg7:0x641a creates the icon column only when DGROUP:0xa7c exists, DGROUP:0x9458 = 0 and DGROUP:0x91fc ≠ 0, as CreateWindow("IS_ICON", "", 0x10CC0000, `IconLeft` DGROUP:0x1de4, `IconTop` DGROUP:0x1de6, 50, 150, main). Before that it sets the rows DGROUP:0x6ea8 + 0x1e·i = {2, 46i + 2, i, 0} for i < DGROUP:0x91fe. SetActiveWindow(main) follows on every path. [BYTES seg7:0x6427 seg7:0x642e seg7:0x6435 seg7:0x6447 seg7:0x6454 seg7:0x645a seg7:0x6476 seg7:0x6483 seg7:0x6496] [ORACLE wave-0001]
- **Size.** IS_ICON's WM_GETMINMAXINFO sets both the minimum and the maximum track size to width 50 >> VScale (DGROUP:0xb6e) and height SM_CYCAPTION + 46·DGROUP:0x91fe + 6 (VScale 0) or SM_CYCAPTION + 23·DGROUP:0x91fe + 3 (VScale non-zero). The size is fixed. [BYTES seg7:0x62d9 seg7:0x62e1 seg7:0x62fd seg7:0x631c]
- **Half scale.** WM_SYSCOMMAND with (wParam & 0xfff0) = 0xf0, the appended "1/2x Scale" item, sets VScale = (VScale < 1), updates the check with seg7:0x63b6 and reapplies the size with MoveWindow. [BYTES seg7:0x61e9 seg7:0x622a]
- **Paint.** WM_PAINT blacks the client, and seg7:0x64ac draws each row i < DGROUP:0x91fe with seg13:0x5a6a(hdc, 0, 46i + 4, 0, i, i + 3, flag, VScale). When DGROUP:0x91fc is 0 the window destroys itself (seg7:0x63f2). [BYTES seg7:0x6365 seg7:0x6372 seg7:0x6525 seg7:0x6532 seg7:0x658e seg7:0x659e seg7:0x638f]
- Alt+I and message 0x9aa toggle the column ([shell.md](shell.md)). [BYTES seg11:0x71c6]

### Direction mark
The target display (d = 0) and two further displays (d = 1, 2) carry a mark that points from the player to the target. [BYTES seg13:0x24aa]
1. **Target.** seg13:0x24aa(d), called from seg7:0xc4c and seg7:0xc5b, returns for d = 0 in a race (DGROUP:0x94e) or when the player's target pool (ship +0x3e, DGROUP:0x70ce) is −1. When seg3:0x0(target) returns 0 and seg13:0x371c(target) ≠ 1, it first retargets through seg13:0x2206 and seg3:0xf4(player, …). [BYTES seg13:0x24bf seg13:0x24ce seg13:0x2520]
2. **Source.** It takes the pool and record from DGROUP:0x70ce/0x70d0 (d = 0), DGROUP:0x6e72/0x6e70 (d = 1) or DGROUP:0x6e90/0x6e8e (d = 2), and returns if the pool is < 0. [BYTES seg13:0x2577 seg13:0x257e]
3. **Direction.** It takes the target's screen point (+0x00/+0x02 when word +0x1a ≠ 0, else seg8:0x29ea of dword +0x04) minus the player's +0x00/+0x02, and stores seg8:0x19b0(dx, dy)/2 (16 directions to 8, truncating toward zero) in DGROUP:0x6e38 + 0x1e·d. [BYTES seg13:0x259e seg13:0x25c0 seg13:0x25cb seg13:0x25d3]
4. **Colour.** The pen colour DGROUP:0x6e44 + 0x1e·d follows the distance from seg8:0x1290, fed dx·dx + dy·dy and \|dx\| + \|dy\|: < 300 0x02FFFFFF (white), < 480 0x0200FFFF (yellow), < 740 0x0200FF00 (green), else 0x02007200 (dark green). It then calls seg11:0x3ed8(d). [BYTES seg13:0x25db seg13:0x25e1]
   - seg8:0x1290 (`retf 8`, g the dword at [bp+6] and n the dword at [bp+0xa]) is one Newton step of a square root: with g' = max(g, 3) (signed 32-bit) it returns (n / g' + g') >> 1 in DX:AX. The loop after it, seg8:0x12c6–0x1314, is unreachable. [BYTES seg8:0x129d seg8:0x12c4 seg8:0x1316]
5. **Origin.** seg11:0x2ce6 sets the frame origin to (2, 17) for d = 0, (DGROUP:0x5024 − 56, 2) for d = 1 and (2, 2) for d = 2. It erases the previous direction DGROUP:0x6e3a with the stock BLACK_PEN when it differs, and draws the current one with CreatePen(PS_SOLID, 1, colour) when DGROUP:0x6e3c ≠ 0, else in black. [BYTES seg11:0x2ce6]
6. **Segment.** seg11:0x2dac draws the mark as one segment of a 43 × 43 frame at the origin, with MoveTo/LineTo (end pixel excluded). Values above 7 (unsigned) draw nothing. [BYTES seg11:0x2dac]

   | Direction | Segment | Evidence |
   |---|---|---|
   | 0 | (14, 0)–(29, 0) | [BYTES seg11:0x2dac] |
   | 1 | (29, 0)–(43, 0)–(43, 14) | [BYTES seg11:0x2dac] |
   | 2 | (43, 14)–(43, 29) | [BYTES seg11:0x2dac] |
   | 3 | (29, 43)–(43, 43)–(43, 29) | [BYTES seg11:0x2dac] |
   | 4 | (14, 43)–(29, 43) | [BYTES seg11:0x2dac] |
   | 5 | (14, 43)–(0, 43)–(0, 29) | [BYTES seg11:0x2dac] |
   | 6 | (0, 14)–(0, 29) | [BYTES seg11:0x2dac] |
   | 7 | (14, 0)–(0, 0)–(0, 14) | [BYTES seg11:0x2dac] |

- **Table.** The entry DGROUP:0x6e38 + 0x1e·d holds the direction at +0, the pen colour at +0xc and the target at +0x1a (record) and +0x1c (pool): DGROUP:0x6e52/0x6e54 for d = 0, which seg3:0xf4 writes for the player, DGROUP:0x6e70/0x6e72 for d = 1 and DGROUP:0x6e8e/0x6e90 for d = 2. [BYTES seg11:0x2d00 seg13:0x2539 seg13:0x2553 seg3:0x115 seg3:0x122]

## Open questions
- **Start heading.** Where does the run-to-run difference of the player's start heading arise, given that seg14:0x15fa writes 0: in the state-100 entry seg13:0x0, seg3:0x45a or seg11:0x3cc2? [OPEN]
- **Collision geometry.** What pair test does seg8:0x6afa apply to the screen squares of seg8:0x50aa, how does seg8:0x29ea's wrap enter it, and what does the race-only path at seg8:0x50eb do? [OPEN]
- **DGROUP:0x994.** What does DGROUP:0x994 count, which raises DGROUP:0x9338 and enters the draw loop's budget test ([Difficulty inputs](#difficulty-inputs))? [OPEN]
- **Probe seed.** Which `srand` seed generated the probe-0001 wave, and does a port replay from it reproduce the [probe wave](#probe-wave) values? A `GetCurrentTime` trace of the generation, which the probe trace leaves out, would settle it. [OPEN]
- **Pool roles.** Which object kinds do pools 1, 2, 4, 5, 6, 7, 9, 11 and 13, and the three pool-8 kinds set at seg9:0x11e3, seg9:0x18b4 and seg9:0x1bd2, hold in the game? [OPEN]
- **On-screen flag.** Can the player's +0x1a be 0 after the [visibility pass](#visibility-pass-seg80x4e14), which would triple its turn word outside a race? [OPEN]
- **Wave end.** What ends a wave and leads to state 2, and what does the LASTGAME string DGROUP:0xfb3 used at seg6:0x8980 do? [OPEN]
- **Rock clamp.** Can DGROUP:0x933a + DGROUP:0x933c exceed 127, and if so what happens when seg10:0xb94 indexes the 920 allocated records with 16-bit offsets, which wrap past record 585? [OPEN]
- **Help page MASS.** Where does the ship help page compute MASS (strings 254, 255 and 264)? [OPEN]
- **Alternate move.** What does seg13:0x3b88 do for the player when DGROUP:0x913a or DGROUP:0x913c = 2, and who writes those words? [OPEN]
- **Tumbling.** How does seg13:0x100c set the heading while ship byte +0xe2 (DGROUP:0x7172) is set, and who writes that byte? [OPEN]
- **Turn accumulator.** What value does ship word +0x16 (DGROUP:0x70a6) hold at wave start, given that its writers are seg13:0x135, seg13:0x654 and seg13:0x688? [OPEN]
- **Thrust table.** What does the 10 × 4 table DGROUP:0x5b2c (stride 0x16), which the OverThruster resets, do? [OPEN]
- **Pause.** Does the wave have a pause key, or does it pause only on focus loss (seg2:0x1327) and SC_MINIMIZE (seg2:0xd9e → seg2:0x9c4)? What does the WM_KEYUP 'P' path through DGROUP:0x1032 and seg11:0xaece do? [OPEN]
- **Mouse.** Do the WndProc mouse cases in seg2 feed flight or targeting? [OPEN]
- **Status display at wave start.** Which message does seg2:0x330 dispatch to seg2:0x970 at wave start, and does BeginPaint's erase wipe the HUD that seg11:0x3cc2 → seg11:0xafe drew? Settle at seg2:0x2b8, seg2:0x330 and the seg9:0x60bc–0x6649 sequence. [OPEN]
- **Draw order.** What do the frame's calls seg8:0x158a, seg12:0x3778, seg10:0x7f88, seg12:0x4d8e, seg18:0xd94, seg13:0xba04, seg13:0xb93a, seg7:0x158e and seg13:0x447a ([Update loop](#update-loop)) draw, and which of them draw the gates, shots and effects? [OPEN]
- **Ship bitmaps.** Which images fill the slots of DGROUP:0x39e8 other than the rocks (6 … 9), the icon strip (13) and the ships (0x28 + i), slot 0x20 (DGROUP:0x3a28) among them, and how do the 8 bpp and true-colour paths differ? [OPEN]
- **Target text.** Where are DGROUP:0x8718 and DGROUP:0x960a filled, and what timer in seg13:0x24aa returns DGROUP:0xa76 from 3 to 2? [OPEN]
- **Icon label.** Does seg7:0x5a80–0x5ab0 format the target display's "Icon N" line from string 152 "Icon ^N" with N = DGROUP:0x57ae + 0x23·slot, as the oracle's ALPHA 0, BRAVO 1, CHARLIE 2 suggests? [OPEN]
- **Icon strip scale.** How is the 32 × 32 icon strip scaled for `Size` 0 and 2, where the briefing shows cells of (32 << `Size`)/2 ([shell.md](shell.md#screen-states)) (seg10:0x5088–0x50f9, the DGROUP:0x3a28 strip, seg10:0x9f75)? [OPEN]
- **Icon rows.** What does seg13:0x5a6a draw in each IS_ICON row, and how do the row table DGROUP:0x6ea8 and a capture change it? [OPEN]
- **Other direction marks.** Which windows show direction marks d = 1 and d = 2 (sources DGROUP:0x6e70/0x6e72 and DGROUP:0x6e8e/0x6e90)? [OPEN]
- **Zoom at wave start.** Which writers of DGROUP:0xa6c besides the race path seg11:0x367c set the zoom, given that the file holds 120? [OPEN]
- **Cloak.** Where does the game keep a ship's cloak state, and does cloaking change any field the radar reads (object +0x04, +0x06, +0x18, +0x28, +0x44, +0x46, +0x4c, ship byte +0x147)? [OPEN]
- **Ship byte +0x147.** What does the byte that hides a ship from the radar mean (set at seg16:0x446, cleared at seg14:0x14d3 and seg13:0x3a93, tested at seg8:0x45f4 and seg16:0x8f8)? [OPEN]
- **Repeated frame.** Why does seg11:0x33cc repeat the view FrameRect DGROUP:0x9158 times with an unused loop index? [OPEN]
