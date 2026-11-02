# The core loop: collisions, damage, capture, the wave end, weapons and the economy

This page holds the rules of play inside a wave: who collides with whom and what a hit does, how a ship takes damage, repairs and dies, how an icon is captured and scored, how a wave ends, how the gun and the weapon systems fire, and how fuel, credit and the ambulance work. Addresses without a module name are in INSPACE.EXE, whose DGROUP is seg19. The update loop, the movers' order, the rock mover, the flight model and the sprites are in [world.md](world.md), the pools and the object and ship records in [structs.md](structs.md), the score and rating tables in [globals.md](globals.md#scoring), `rand()` in [rng.md](rng.md), the sound ids in [sound.md](sound.md), the saved game in [formats.md](formats.md#saved-games) and the screen states in [shell.md](shell.md#screen-states). [DESIGN]

## Notation
- `/`, `>>` and `lo16` are as in [world.md](world.md#notation). Object fields are named as in [structs.md](structs.md#object-record-0x70-bytes); ship-record fields are `+0xNN` relative to DGROUP:0x7090 + 0x15a·i ([structs.md](structs.md#ship-records-8--0x15a-bytes-at-dgroup0x7090)), design words `DGROUP:0x96xx + 0xad·n` relative to the design record of `design_index` n ([structs.md](structs.md#design-records)), and roster bytes relative to the 0x7c-byte record at far DGROUP:0x6e24 ([formats.md](formats.md#ship-roster-records-0x7c-bytes)). [DESIGN]
- Money is counted in hundredths of an icon: 150 is "1.50 ICONS". [BYTES seg14:0x3cc] [ORACLE wave-0001]
- Symbols used below:

  | Symbol | Global | Meaning | Evidence |
  |---|---|---|---|
  | player | record DGROUP:0x9646 of the ship pool DGROUP:0x9324 (14) | the player's ship | [BYTES seg13:0xb7d seg3:0x6fa] |
  | now | DGROUP:0x1e50 | the tick clock in ms ([world.md](world.md#notation)) | [BYTES seg8:0x4659] |
  | frame# | dword DGROUP:0x1e68 | the wave frame counter, 36 per second ([world.md](world.md#update-loop), step 8) | [BYTES seg11:0x1d92 seg11:0x1c24] |
  | s | DGROUP:0x9150 | 4 − `Size`; one pixel is `1 << s` world units ([world.md](world.md#notation)) | [BYTES seg8:0x5785] |
  | W, H | DGROUP:0x90ea, DGROUP:0x90ec | play width and height in pixels | [BYTES seg8:0x25b seg8:0x266] |
  | `PlayMode` | DGROUP:0x9134 | INI `PlayMode`: 0 Action, F12 toggles it ([formats.md](formats.md#inspaceini-inner-space)) | [BYTES seg11:0xadf0 seg11:0x4dba] |
  | demo | DGROUP:0x95bc = 0 | the computer-pilot demo; ≠ 0 is a player game | [BYTES seg11:0x6d19 seg16:0xabc] |
  | seg18 mode | DGROUP:0x9130 ≠ 0 | the separate game mode of seg18 ([structs.md](structs.md#pool-roles)); every rule below is for DGROUP:0x9130 = 0 unless it says otherwise | [BYTES seg8:0x50d7 seg8:0x530f] |
  | served ship | DGROUP:0x952e | the ship the ambulance is serving, −1 when idle | [BYTES seg16:0xbf8 seg16:0x8a0] |
  | amb_state | DGROUP:0x9e49 | the ambulance's state ([The ambulance](#the-ambulance)) | [BYTES seg16:0xbfb seg9:0x610b] |
  | W(w)+n | word DGROUP:0x2f54 + 0x50·w + n | field n of the weapon record of weapon w ([structs.md](structs.md#weapon-records-28--0x50-bytes-at-dgroup0x2f54)); n = −8 … −2 are the last words of block w − 1 | [BYTES seg10:0x22d8 seg11:0xad10] |

## Collisions

### Who tests whom (seg8:0x50aa)
- Each mover calls the pair finder seg8:0x50aa(pool·256 + rec) for its own on-screen records, after storing its pool in DGROUP:0x943a and the record in DGROUP:0x943c. [BYTES seg13:0xb75 seg13:0xb7d seg13:0xb8a]
- **Callers and pools.** The movers run in the order of [world.md](world.md#update-loop) step 21 and register these pools: [BYTES seg7:0x3151 seg7:0x339a seg8:0x864b seg8:0x8e2d seg7:0x3f2c seg12:0xb6d seg7:0x43c9 seg12:0x332 seg16:0x7e1 seg12:0x1cc1 seg12:0x3d8b seg12:0x62c8 seg12:0x5459 seg12:0x3307 seg12:0x6c91 seg13:0xb7d]

  | Mover | Pool | Sites |
  |---|---|---|
  | seg13:0x0 | 14 (DGROUP:0x9324, ships) | seg13:0xb7d |
  | seg7:0x3bae | 3 (DGROUP:0x934a, rocks) | seg7:0x3f2c |
  | seg12:0x532c | 9 | seg12:0x5459 |
  | seg12:0x633e and seg12:0x1776, seg12:0x390e | 8 | seg12:0x62c8, seg12:0x1cc1, seg12:0x3d8b |
  | seg8:0x8256 | 2 | seg8:0x864b, seg8:0x8e2d |
  | seg7:0x309c | 0 (icons) | seg7:0x3151 |
  | seg7:0x32d0 | 1 | seg7:0x339a |
  | seg7:0x40c4 | 5 (DGROUP:0x9328) | seg7:0x43c9 |
  | seg12:0x0 | 6 | seg12:0x332 |
  | seg12:0x3148 | 10 | seg12:0x3307 |
  | seg12:0x6b1e | 11 | seg12:0x6c91 |
  | seg12:0xae4 | 4 | seg12:0xb6d |
  | seg16:0xa2 (the ambulance) | 7 | seg16:0x7e1 |

  - seg18 registers pools 15 and 20. [BYTES seg18:0x3ce0 seg18:0x4945]
- **Pool order.** In the visibility-list loop seg8:0x50aa skips candidates whose pool is below the caller's, and in the caller's own pool skips the caller itself. A pair of pools p < q is therefore found only by pool p's mover. [BYTES seg8:0x52ae seg8:0x52b3 seg8:0x52b6 seg8:0x52b8 seg8:0x52bf seg8:0x52c2]
  - The ship mover (pool 14) finds only other ships in a normal wave, since no normal-wave mover registers a pool above 14. Ship against rock is found by the rock mover, ship against icon by the icon mover, ship against shots by the pool-5, 6 and 10 movers, shot against rock by the rock mover. [BYTES seg13:0xb79 seg13:0xb8a seg8:0x52b6]
- **One collision per object per frame.** seg8:0x50aa returns 0xFFFF at once when the caller's `collided` (+0x1c) ≠ 0, and the visibility loop skips candidates whose `collided` ≠ 0. A hit sets `collided` = 1 on both parties, and the [visibility pass](world.md#visibility-pass-seg80x4e14) clears it before the movers, so the first mover to find a pair uses up both objects for that frame. [BYTES seg8:0x50d7 seg8:0x50de seg8:0x52e3 seg8:0x5305 seg8:0x545c seg8:0x6cda seg8:0x6cfa]
- **Candidates.** The normal path walks the visibility list DGROUP:0x507c (pool) / DGROUP:0x50e0 (record), indices 0 … DGROUP:0x9146 − 1, and returns the first hit as pool·256 + rec, else 0xFFFF. [BYTES seg8:0x5224 seg8:0x52a0 seg8:0x545c]
- **Race-track path.** In a kind-6 wave (DGROUP:0x9320 = 6), a caller whose `on_screen` is 0 with `Detail` DGROUP:0x9208 > 1 re-projects its own world position into `scr_x`/`scr_y` through seg8:0x29ea and builds the same squares. It then tests every ship 0 … DGROUP:0x9316, skipping those with `on_screen` ≠ 0 or `state` ≠ 0 and itself when the caller is a ship, and returns 0x0e00 | i on a hit. This path has no pool-order rule and no `collided` test. Ships never take it, because the ship mover calls only for on-screen ships. [BYTES seg8:0x50e1 seg8:0x50eb seg8:0x50f2 seg8:0x511c seg8:0x5165 seg8:0x51a7 seg8:0x51be seg8:0x51c5 seg8:0x51cc seg8:0x520b seg8:0x5218]
- **seg18 path.** In seg18 mode seg8:0x530f–0x545a sets DGROUP:0x9446 = 20000 and can return 0x1000 ([world.md](world.md#collisions)). [BYTES seg8:0x530f seg8:0x545a]

### Pair test (seg8:0x6afa)
- **Squares.** In screen pixels, the caller C has square A (DGROUP:0x5160) = (`scr_x`, `scr_y`, `scr_x` + `size`, `scr_y` + `size`). [BYTES seg8:0x5224 seg8:0x5253]
- **Search box.** Box B (DGROUP:0x5148) = (`scr_x` − h, `scr_y` − h, `scr_x` + `size` + q, `scr_y` + `size` + q) with h = (70 << `Size`) >> 1 and q = (15 << `Size`) >> 1, `Size` = DGROUP:0x9156. [BYTES seg8:0x525c seg8:0x5260 seg8:0x5263 seg8:0x5265 seg8:0x5283 seg8:0x5288 seg8:0x5294]
- seg8:0x6afa(C, K) (Pascal, `retf 4`) returns 0, no hit, when both have `fixed` (+0x54) ≠ 0. [BYTES seg8:0x6b3b seg8:0x6b4d seg8:0x6b53]
- It then needs the candidate K's `state` = 0, K's top-left point in box B (PtInRect) and a non-empty IntersectRect of A with K's square. [BYTES seg8:0x6b65 seg8:0x6b7d seg8:0x6ba6 seg8:0x6bb7]
- **Exclusions,** each in both roles:
  - a pool-10 object and its owner (`owner_pool`/`owner_rec`) never collide; [BYTES seg8:0x6bc0 seg8:0x6bcf seg8:0x6bd8 seg8:0x6be1 seg8:0x6bfa seg8:0x6c03]
  - an object with byte +0x2e ≠ 0 does not collide unless it is a ship whose ship word +0x2e (DGROUP:0x70be) = 2; [BYTES seg8:0x6c1a seg8:0x6c24 seg8:0x6c31 seg8:0x6c49 seg8:0x6c5d]
  - a pool-6 missile does not collide with its launcher (DGROUP:0x66e8/0x66ea + 0x1c·rec) while its age DGROUP:0x66ec + 0x1c·rec < 10. [BYTES seg8:0x6c67 seg8:0x6c71 seg8:0x6c7b seg8:0x6c84 seg8:0x6c97]
- **On a hit** a party in pool 7 with DGROUP:0x9e49 = 1 sets DGROUP:0x9e49 = 0, and both parties get `collided` = 1. [BYTES seg8:0x6cb3 seg8:0x6cbf seg8:0x6cc6 seg8:0x6cda seg8:0x6cfa]
- **Roles A and B.** The orientation flag [bp−0xc] is written by four tests in this order, the last write winning: K is the player → 1, C is the player → 0, K `fixed` (+0x54) ≠ 0 → 1, C `fixed` (+0x54) ≠ 0 → 0. Nothing initialises it. [BYTES seg8:0x6b04 seg8:0x6d0e seg8:0x6d23 seg8:0x6d3d seg8:0x6d57 seg8:0x6d5c]
  - Flag 1 makes K party A: pool DGROUP:0x944a, record DGROUP:0x944e, velocity DGROUP:0x52e0/0x52e2, screen position DGROUP:0x52e4/0x52e6, mass DGROUP:0x9446. Flag 0 makes K party B: DGROUP:0x944c, DGROUP:0x9450, DGROUP:0x52dc/0x52de, DGROUP:0x52e8/0x52ea, DGROUP:0x9448. Only K's velocity, position and mass are stored here. [BYTES seg8:0x6d79 seg8:0x6db9 seg8:0x6dd0 seg8:0x6e44 seg8:0x6e5e seg8:0x6e9e seg8:0x6ea8 seg8:0x6f29]
  - A `fixed` object (a gate post) is therefore always party A, and otherwise the player's ship is A. [BYTES seg8:0x6d3d seg8:0x6d57]
  - Both extents (A: DGROUP:0x9442, B: DGROUP:0x9444) are `extent` + 2, or `size` in seg18 mode. [BYTES seg8:0x6ddb seg8:0x6dee seg8:0x6df2 seg8:0x6e18 seg8:0x6ec0 seg8:0x6f11]

### Response (seg8:0x546c)
- **Call.** seg8:0x546c (Pascal, `retf 0xe`) takes [bp+6] the caller's `mass`, [bp+8] its `extent` + 2 (never read), [bp+0xa] a far pointer to its `vy`, [bp+0xe] one to its `vx`, and [bp+0x12] the hit object from seg8:0x50aa. It returns the damage to the caller, or −1. [BYTES seg8:0x5aab seg8:0x57f5 seg8:0x57fe seg13:0xbaf seg13:0xbb8 seg13:0xbc8]
1. **Shot bookkeeping.** When party A is a pool-5 (DGROUP:0x9328) object, it returns −1 if A's last-hit entry DGROUP:0x5a54/0x5a56 + 0x14·rec already names B; otherwise it plays B's hit sound (B's `hit_sound` through seg10:0x754a, or for a ship B the design word DGROUP:0x9700 + 0xad·n through seg10:0x7944) and stores B there. The same holds with A and B swapped. [BYTES seg8:0x549a seg8:0x54b4 seg8:0x5520 seg8:0x5530 seg8:0x5580 seg8:0x559c seg8:0x55ac seg8:0x55b3]
2. **Ambulance pass-through.** With DGROUP:0x9e49 in 3 … 6, a pool-7 party against the ship whose record is DGROUP:0x952e returns −1, in both orientations. [BYTES seg8:0x55b7 seg8:0x55be seg8:0x55c5 seg8:0x55d5 seg8:0x55e1 seg8:0x55f1]
3. **Branches.** When the hit object is party A (seg8:0x5611 path) the caller is B: B's mass is the caller's mass, A's mass is multiplied by 10 when A is pool 0 and B pool 2, and A's mass becomes 20000 when A's `fixed` (+0x54) ≠ 0. When the caller is A (seg8:0x5870 path), A's mass is the caller's mass and becomes 20000 for a `fixed` party; this branch has no pool-0/pool-2 rule. [BYTES seg8:0x5611 seg8:0x56a8 seg8:0x56af seg8:0x56b9 seg8:0x56bd seg8:0x56d0 seg8:0x56d7 seg8:0x5870 seg8:0x58ef seg8:0x5905]
4. **Capture pass-through.** It returns −1, no bounce and no damage, when one party is a ship with bytes +0xea = 0 and +0xe8 = 0 and design word DGROUP:0x9714 (Captures Icons) ≠ 0, and the other has `capturable` (+0x53) ≠ 0 and signed byte +0x2f ≥ 0; both orientations, both branches. [BYTES seg8:0x56dd seg8:0x56ec seg8:0x56f3 seg8:0x5700 seg8:0x5719 seg8:0x5720 seg8:0x5727 seg8:0x572a seg8:0x5774 seg8:0x5912 seg8:0x595c seg8:0x595f seg8:0x59a9]
5. **Units.** Both extents become (extent >> s) + 1. [BYTES seg8:0x5777 seg8:0x5785 seg8:0x5787 seg8:0x578e seg8:0x5790 seg8:0x59b3 seg8:0x59c6]
6. **Physics.** It calls [the bounce](#bounce-ispacedll-ordinal-29) through seg10:0x7dd4, except that with a pool-5 party it takes the result as 1 without calling it. [BYTES seg8:0x5794 seg8:0x579b seg8:0x57a1 seg8:0x57db seg8:0x59c9 seg8:0x59d6 seg8:0x547b]
7. **Result 0 (separating).** It calls seg8:0x698a(0) or seg8:0x6a44(0): the hit object takes its scaled slot velocity and `collided` = 1 and is not dispatched; the caller's velocity is not written, and the function returns −1. The callers pass that −1 to the hit dispatcher seg8:0x5df2, which tests for −1 at entry. [BYTES seg8:0x57e3 seg8:0x57e9 seg8:0x5a18 seg8:0x5a1e seg8:0x5e46 seg8:0x5e93]
8. **Result 1.** Both branches write B's world `x`/`y` from seg8:0x2bbe(DGROUP:0x52e8, DGROUP:0x52ea) unless B's `fixed` (+0x54) ≠ 0. The hit party's damage gains 5 × DGROUP:0x9454 when it is A (into DGROUP:0x9438) or 4 × DGROUP:0x9454 when it is B (into DGROUP:0x9436). Then seg8:0x698a(1) or seg8:0x6a44(1) writes the hit party's `collided` = 1 and its velocity from its slot (0 for a `fixed` party) and calls seg8:0x5df2(hit party, other party, its damage). The function returns DGROUP:0x9436 when the caller is B and DGROUP:0x9438 when it is A. [BYTES seg8:0x5824 seg8:0x5837 seg8:0x5853 seg8:0x5a59 seg8:0x5a6c seg8:0x5a88 seg8:0x585b seg8:0x5860 seg8:0x5a90 seg8:0x5a96 seg8:0x5867 seg8:0x586a seg8:0x5a9d seg8:0x5aa0 seg8:0x698a seg8:0x69dc seg8:0x69ff seg8:0x6a1f seg8:0x6a44 seg8:0x6a96 seg8:0x6ab5 seg8:0x6ad5]
- **DGROUP:0x9454** is the moving rock's own signed heat byte +0x2f: the rock mover sets it before seg8:0x546c, after its pool-5 heating step, and clears it after. Every other caller leaves it 0. [BYTES seg7:0x400c seg7:0x4011 seg7:0x4049 seg7:0x4068]

### Bounce (ISPACE.DLL ordinal 29)
- seg10:0x7dd4 forwards its 12 dwords unchanged through the far pointer DGROUP:0x3c6c, which holds ISPACE.DLL ordinal 29, and pops 0x30 bytes. [BYTES seg10:0x7dde seg10:0x7e0a seg10:0x7e0e seg10:0x7e18]
- Its parameters (A's and B's damage words DGROUP:0x9438/0x9436, masses, extents, positions DGROUP:0x52e4 … 0x52ea and velocities DGROUP:0x52dc … 0x52e2), the slow-contact step, the moving-apart test that returns 0 and the elastic exchange that returns 1 and writes both damages are in [ispace.md](ispace.md#ordinal-notes) (29). It draws no `rand()`. [BYTES ISPACE.DLL seg2:0x3c] [BYTES seg8:0x57a3 seg8:0x57db]

### Ship against ship (seg13:0x0)
- Per ship i, after its move, flight and draw ([world.md](world.md#ships-seg130x0)), a ship whose `on_screen` ≠ 0 calls seg8:0x50aa(14·256 + i) and, on a hit, amount = seg8:0x546c(`mass`, `extent` + 2, &`vy`, &`vx`, hit). [BYTES seg13:0xb69 seg13:0xb75 seg13:0xb7d seg13:0xb8a seg13:0xbcc]
- A hit object with `capturable` (+0x53) ≠ 0, while the ship's byte +0xea = 0, goes to the [touch routine](#the-touch-routine-seg130x16ac) seg13:0x16ac(i, hit) instead of the dispatcher. [BYTES seg13:0xbee seg13:0xbfa seg13:0xc08 seg13:0xc0e]
- A pool-6 hit object whose weapon kind (W(w)+0x24, DGROUP:0x2f78 via DGROUP:0x66e4) is 6 while the ship's selected weapon's kind is also 6 gets `capturable` = 1 and goes to seg13:0x16ac, and the ship's count byte +0xae + `weapon_slot` rises by 1. [BYTES seg13:0xc11 seg13:0xc18 seg13:0xc77]
- Otherwise the damage goes to seg8:0x5df2(14, i, hit pool, hit rec, amount). [BYTES seg13:0xc79 seg13:0xc89 seg13:0xc8d]
- Both branches above see only ships in a normal wave ([pool order](#who-tests-whom-seg80x50aa)); the pool-6 missile mover reaches seg13:0x16ac itself ([Pool-6 mover](#pool-6-mover-seg120x0)). [BYTES seg8:0x52b6 seg12:0x320]

## Damage

### The ship's systems
- The `systems` words +0x26, +0x28, +0x2a and +0x2c are the health 0 … 100 of lamps 0 … 3; the lamp rows are design words DGROUP:0x96f4 + 2k ([world.md](world.md#damage-panel)). [BYTES seg11:0x1df7 seg11:0x632]
- The systems are named ENGINE, GUN, THRUST, WINGS and CLOAK. [BYTES ENG_LANG.DLL STRING 53] ids 840–844 The damage display shows Engine, Guns and Thruster, and the wings or the cloak as the fourth system. [MANUAL General] The TRANSFORMER's panel reads Engine, Gun, Thrust and Shield. [ORACLE wave-0001]
- **Cloak.** seg11:0x4c72 toggles `cloaked` (+0xe8). Both directions need `has_cloak` (+0xe7) ≠ 0 and byte +0xeb = 0; a computer ship (or the player with `PlayMode` ≠ 0) also needs 150 frames since its last toggle (dword +0x152 against frame#). Switching on also needs `fuel` ≥ 120 and word +0x2c ≠ 0 (≥ 41 for a computer ship). Byte +0xe8 is the cloak-on flag, and `has_cloak` (+0xe7), which picks lamp row 4 CLOAK, is the ship's cloak. [BYTES seg11:0x4c72 seg11:0x4cc6 seg11:0x4d08 seg11:0x4da0 seg11:0x3d0e] The cloak is toggled with CAPSLOCK and uses fuel. [MANUAL Special Ship Systems]
- Byte +0xe9 picks lamp row 5 SHIELD and the shop's SHIELD repair, so it is the ship's shield. [BYTES seg11:0x3db2 seg16:0x4bd8] Whether byte +0xea is the shield-on flag is [OPEN] ([Open questions](#open-questions)).

### Damage to a ship (seg8:0x45e0)
seg8:0x45e0 is the ship pool's `on_damage` (cdecl: pool, ship, d). In order: [BYTES seg8:0x45e0 seg8:0x4d5d]
1. **No damage** for a ship with byte +0x147 = 1 outside seg18 mode, and for the player while DGROUP:0x104c ≠ 0. [BYTES seg8:0x45f4 seg8:0x4607 seg8:0x4614]
2. **Dead ship.** A ship with `alive` (+0xec) = 0 hit by anything but itself (DGROUP:0x943a/0x943c) and not by pool 11 explodes at once through seg12:0x274a when dword +0x84 − 5500 < now; in seg18 mode it also sets DGROUP:0x9530 = 6 and calls seg18:0x633c. It returns 0. [BYTES seg8:0x461b seg8:0x4622 seg8:0x4638 seg8:0x463f seg8:0x4659 seg8:0x4662]
3. **Scale.** d = d·7/8; for the player d = d·(DGROUP:0x9000 + 6)/8 and the rating loss of [globals.md](globals.md#scoring). [BYTES seg8:0x468a seg8:0x469d seg8:0x46a5]
   - In a Demon's Gate wave (DGROUP:0x92e8 ≠ 0) a hit on ship 1 sets DGROUP:0x8e30 = now + 20000. [BYTES seg8:0x4776 seg8:0x4790]
4. **Attacker.** A gun shot (DGROUP:0x943a = 0x1e, the [shot slot](#shot-slots) DGROUP:0x943c) owned by a ship calls seg4:0x5e8e; when that owner is the player and `PlayMode` = 0 it records DGROUP:0x943e/0x9440 and DGROUP:0x52d8. [BYTES seg8:0x47c3 seg8:0x47e4 seg8:0x4817 seg8:0x4827]
   - Any other attacker scales d by 3/4, and halves it again in a kind-6 wave when the attacker is a ship. [BYTES seg8:0x4865 seg8:0x486f seg8:0x4875 seg8:0x4887]
   - The shot rules below are keyed on the local [bp−0xc] = 2, which a shot sets and which a non-shot attacker whose word +0x22 is 2 also produces. [BYTES seg8:0x47c3 seg8:0x485e seg8:0x49e4]
5. **Last hit.** Ship dword +0x14a = the ship's world `x`/`y` and dword +0x14e = now. [BYTES seg8:0x48aa seg8:0x48b9]
6. **Side.** Object word +0x26 = +1 when the 32-bit cross product of the relative position and velocity, [bp−0x12]·[bp−0x10] − [bp−0x14]·[bp−0xe], is above 0, else −1. [BYTES seg8:0x4931 seg8:0x4948]
7. **Sound.** With DGROUP:0xb0a ≠ 0 it plays bounce sound 7 here. [BYTES seg8:0x494e seg8:0x4960]
8. **Mode factor.** With byte +0xea ≠ 0, d = 4d/3, at least 1; else with `cloaked` (+0xe8) ≠ 0, d = 3d, at least 2; else d = 2d, at least 1. It stores d in `damage`. [BYTES seg8:0x496d seg8:0x497a seg8:0x497e seg8:0x4987 seg8:0x498e seg8:0x49a9 seg8:0x49b8]
9. With DGROUP:0xb0a = 0 it plays the design's hit sound through seg10:0x7944. [BYTES seg8:0x49dc]
10. **Systems.** Each `systems` word +0x26 + 2k (k = 0 … 3) loses max(1, d / divisor), the divisor being design word DGROUP:0x96e2 + 2k (+0x10 + 2k), or DGROUP:0x96da + 2k (+0x08 + 2k) for a shot. With a roster record (`roster_index` ≠ −1) and p = roster byte +0x34 + k, the quotient is first scaled by (20 − 3p)/20, or (10 − p)/10 for a shot. All four words are then clamped at ≥ 0. [BYTES seg8:0x4a1e seg8:0x4a2a seg8:0x4a31 seg8:0x4a55 seg8:0x4a6f seg8:0x4a99 seg8:0x4aa5 seg8:0x4aae seg8:0x4ad1 seg8:0x4aeb seg8:0x4b07 seg8:0x4b11]
11. **Wings.** For a wing ship (design word DGROUP:0x96fa = 3): [BYTES seg8:0x4b23 seg8:0x4b58 seg8:0x4b69 seg8:0x4b82 seg8:0x4bc2 seg8:0x4be1 seg8:0x4bec seg8:0x4c06 seg8:0x4c14 seg8:0x4c34]
    - at word +0x2c = 0 it tears off every wing still on (seg8:0x2cd8 side 1 for byte +0xad, side 0 for +0xac), clears ship words +0x10 and byte +0xea, and sets ship words +0x20 = design word +0x2a − 14, +0x22 = 7, +0x24 = min(+0x24, 1) and byte +0xe0 = 1;
    - below 50 with both wings on it tears the wing on the hit side (object +0x26 = 1: side 1, +0xad; else side 0, +0xac), clears +0x10 and +0xea, sets +0xe0/+0xe1 when +0x24 = 2, and sets object +0x26 = 0.
12. **Thruster out.** Word +0x2a = 0 with design word DGROUP:0x96f8 = 2 and `thrusting` ≠ 0 sets `thrusting` = 0 and calls seg13:0x6716 (flame off). [BYTES seg8:0x4c6c seg8:0x4c78 seg8:0x4c7f seg8:0x4c86 seg8:0x4c8f]
13. **Death.** Engine word +0x26 < 1, unless the attacker is the ship itself or pool 11, calls the ship's `on_destroy` seg16:0x8e4. Every [defender entry](structs.md#other-wave-tables) k < DGROUP:0x940a, DGROUP:0x52fe + 0x24k, aimed at this ship then gets +0 = 15 and the killer, and the function returns 1. [BYTES seg8:0x4c9a seg8:0x4ca4 seg8:0x4cc9]
    - With the ship itself or pool 11 as attacker the ship keeps flying at engine 0. [BYTES seg8:0x4cb5 seg8:0x4cbf]
14. **Otherwise** it clears DGROUP:0xb0a and calls seg13:0x33d6, which rewrites `kill_value` ([world.md](world.md#ship-rating-seg130x33d6)), and returns 0. [BYTES seg8:0x4d43 seg8:0x4d4c seg13:0x3662 seg13:0x366a]

### Lamp messages
- The lamp states and when seg11:0x5fe raises a message are in [world.md](world.md#damage-panel). The message switch seg3:0x1351 indexes its 192-entry jump table at seg3:0x1356 with code − 1. [BYTES seg3:0x1346 seg3:0x1347 seg3:0x134e]
- **Codes 10, 11, 12** play sound 0x6e and then 0x2d (code 10) or 0x2c (11, 12) through seg10:0x744c. [BYTES seg3:0xd2d seg3:0xd2f seg3:0xd35 seg3:0xd41]
- They format string 134 into DGROUP:0x8db0 with %S = string code + 0x75 (127 ENGINE, 128 GUN, 129 THRUSTER), %? = (code = 10, 519, 135) and the %< %> part on W > 700: code 10 reads "DANGER: ENGINE TROUBLE. REPAIR OR ABORT MISSION.", codes 11 and 12 "DANGER: GUN FAILURE. …" and "DANGER: THRUSTER FAILURE. …". [BYTES seg3:0xd47 seg3:0xd57 seg3:0xd5d seg3:0xd6e seg3:0xd74 seg3:0xd7b] [BYTES ENG_LANG.DLL STRING 8] id 127 [BYTES ENG_LANG.DLL STRING 9] ids 128, 129, 134, 135 [BYTES ENG_LANG.DLL STRING 33] id 519
- **Codes 82, 83** format string 517 "%S SYSTEM OK REPAIRS COMPLETE" with %S = code + 0x2e (128 GUN, 129 THRUSTER). [BYTES seg3:0xd86 seg3:0xd89 seg3:0xd8f] [BYTES ENG_LANG.DLL STRING 33] id 517
- **Code 84** formats string 138 "Repairs Complete, %s. Try To Avoid Trouble." with the pilot name DGROUP:0x380c, with no sound. [BYTES seg3:0xd95 seg3:0xd9b] [BYTES ENG_LANG.DLL STRING 9] id 138

### Wings torn off (seg8:0x2cd8)
- seg8:0x2cd8(ship, side) (cdecl) sets ship dword +0x156 = now − 10000 and clears byte +0xad (side ≠ 0) or +0xac (side 0). [BYTES seg8:0x2cee seg8:0x2cf6 seg8:0x2d01 seg8:0x2d0e seg8:0x2d1e]
- With DGROUP:0x95a6 = 0 it recomputes design word DGROUP:0x970e from the body table through seg14:0x3eda and seg14:0x4074. [BYTES seg8:0x2d23 seg8:0x2d29 seg8:0x2d8d seg8:0x2d91]
- The taunts and the debris below need DGROUP:0x92e4 = 0. [BYTES seg8:0x2d96]
- **Taunts.** Each ship j with `goal_pool` = 14, `goal_rec` = the damaged ship and word +0x38 = 1 gets one taunt seg3:0x6ec(j, 14, ship, code), code 0x19 … 0x1e, chosen by both ships' wing bytes and seg4:0x5d9c(ship, j) > 0x5f: strings 373, 375, 383, 377, 379 and 381. [BYTES seg8:0x2da6 seg8:0x2dcb seg8:0x2ddc seg8:0x2df5 seg8:0x2e0a seg8:0x2e34 seg8:0x2e57 seg3:0xda9 seg3:0xdb9 seg3:0xdc1 seg3:0xdd1 seg3:0xde1 seg3:0xde9] [BYTES ENG_LANG.DLL STRING 24] ids 373–383
- **Debris.** With a free entry (first word 0) in the 3 × 0x22-byte table DGROUP:0x9176 the torn wing plays sound 0x6a through seg10:0x754a(0x6a, 14, ship, 1). [BYTES seg8:0x33c2 seg8:0x33cd seg8:0x33da seg8:0x33e9]
- A torn-off wing cannot be repaired by the ship. [MANUAL Special Ship Systems]

### Death and the explosion
- **seg16:0x8e4(pool, ship)**, the ship's `on_destroy`, returns at once for byte +0x147 = 1 outside seg18 mode. [BYTES seg16:0x8f8]
- It starts up to 8 entries of the [effect table](structs.md#effect-table-45--0x14-bytes-at-dgroup0x1e90) DGROUP:0x1e90, each in a free one of the 45 slots, with start times now + 20k, word +0x02 = 10, and DGROUP:0x9430 + 1 each. [BYTES seg16:0x924 seg16:0x939 seg16:0x9c1 seg16:0x9d8 seg16:0x9e7]
  - Entry 0 takes screen offset (16, 16); entries 1 … 7 draw two `rand()` % 60 − 12 offsets, x first. [BYTES seg16:0x95d seg16:0x96e seg16:0x991 seg16:0x9a4]
- It then sets `tumbling` (+0xe2) = 1 (tumbling), object +0x26 = 1, dword +0x84 = now + 6000, calls seg13:0x4710 and sets `alive` (+0xec) = 0 (dead). [BYTES seg16:0x9ff seg16:0xa19 seg16:0xa2d seg16:0xa37 seg16:0xa3d]
- In a kind-7 (duel) wave it sets DGROUP:0x668a = −1 when the player dies or no other live, idle ship remains. [BYTES seg16:0xa42 seg16:0xa9b]
- It plays seg10:0x754a(0x45, 14, ship, 1). [BYTES seg16:0xaac]
- **The player's death.** A player game sets DGROUP:0x1e84 = now + 8000 and the rating to 4/5; the demo sets DGROUP:0x1e80 = now + 8000, DGROUP:0x95a8 = 1 and halves the rating, and with DGROUP:0x951a = 0 calls seg11:0xaece. [BYTES seg16:0xabc seg16:0xacd seg16:0xae6 seg16:0xaea seg16:0xb19 seg16:0xb24 seg16:0xb2b]
- Outside seg18 mode it ends with seg13:0x498e(ship, 1). [BYTES seg16:0xb3c]
- **Tumble.** A dead ship tumbles in the ship mover ([world.md](world.md#ships-seg130x0)) and explodes through seg12:0x274a once dword +0x84 < now, clearing `tumbling` (+0xe2) and object +0x26 first: 6 s after death. A hit in the last 5.5 s explodes it early (step 2 of [Damage to a ship](#damage-to-a-ship-seg80x45e0)). [BYTES seg13:0x14b seg13:0x156 seg13:0x160 seg13:0x174 seg13:0x181 seg16:0xa2d seg8:0x463f]
- **Fragments.** seg12:0x274a, the generic destroy handler ([structs.md](structs.md#object-record-0x70-bytes)), makes 1 fragment for `blast_size` 1, else 30, 60, 100 or 130 % of `blast_size` for `Detail` 0 … 3, at least 2; with DGROUP:0x9430 > 10 the count becomes (35 − DGROUP:0x9430)·n/25, at least 1. A `Detail` outside 0 … 3 leaves the count unset. [BYTES seg12:0x2816 seg12:0x2824 seg12:0x2834 seg12:0x2844 seg12:0x2849 seg12:0x2852 seg12:0x2860 seg12:0x2880]
  - Each fragment j, when a free slot exists, draws `rand()` % 110 for its start time now + r + 30j; for j > `Detail`/2 + 2 it draws `rand()` % 10 and, when that is ≤ 6, `rand()` % 4; for j > 0 it draws x and then y as (`rand()` % (2(n + 40)) − n − 40)·8. [BYTES seg12:0x28ec seg12:0x292d seg12:0x2938 seg12:0x293d seg12:0x29a0 seg12:0x29f6]
- The manual's "on fire (almost dead)" ship [MANUAL Resource Management] has no identified state in the bytes ([Open questions](#open-questions)). [OPEN]

### Rocks (seg7:0x5090)
- seg7:0x5090 is the rock pool's `on_damage` (cdecl: pool, rec, d). It reads the attacker from DGROUP:0x944c/0x9450, the B party of the last collision, so a hit that did not come through a collision sees stale values. [BYTES seg7:0x509f seg7:0x50e7 seg7:0x50f0]
- It adds no damage when DGROUP:0x9450 exceeds the in-use count of pool DGROUP:0x944c. [BYTES seg7:0x509f seg7:0x50a8 seg7:0x50ac]
- **Heat.** For a rock with d > 1, d becomes 3d/2. Unless B is a pool-5 (DGROUP:0x9328) object of mass 200, the heat byte +0x2f rises by d/4, at most 9. Every such hit sets dword +0x6a = now + 3000. [BYTES seg7:0x50c5 seg7:0x50e7 seg7:0x50fe seg7:0x5104 seg7:0x5106 seg7:0x5129 seg7:0x5131 seg7:0x5135 seg7:0x5150 seg7:0x5154]
- **Break.** It adds d to `damage` at seg7:0x5163; a rock breaks only when `damage` > `max_damage` and its heat before the hit was above 6 (or in seg18 mode). [BYTES seg7:0x50df seg7:0x50e4 seg7:0x5163 seg7:0x5171 seg7:0x5179 seg7:0x517f]
  - A small rock (`mass` 350) is destroyed through seg12:0x274a and seg7:0x2080(pool, −1), returning 1. [BYTES seg7:0x5196 seg7:0x51a2 seg7:0x51af]
  - A big rock splits into three small rocks: `mass` 350, `extent` 168, `max_damage` 300 (100 in seg18 mode), `damage` 0, heat 9, at the parent's `x`, `y` plus 8·(0, −10), 8·(15, 12) and 8·(−15, 13). [BYTES seg7:0x521b seg7:0x5223 seg7:0x5234 seg7:0x524c seg7:0x5450 seg7:0x547f seg7:0x55cd seg7:0x5611 seg7:0x5627 seg7:0x5632]
- **Otherwise** it adds d again at seg7:0x564e, so a rock hit with d > 1 that neither destroys nor splits adds 2 × (3d/2). With d ≤ 1, or outside the rock pool, d is added once. [BYTES seg7:0x5175 seg7:0x563d seg7:0x564e]
- The rock mover's heating step for pool-5 hits runs before the response ([world.md](world.md#rocks-seg70x3bae), Heating). [BYTES seg7:0x3f65 seg7:0x3ff7]

### Auto-repair (seg11:0x1d7c)
- seg11:0x1bcc calls seg11:0x1d7c at its end. It acts when frame# exceeds dword DGROUP:0x5014 and then sets DGROUP:0x5014 = frame# + 180, so it runs every 181 frames, about 5 s. [BYTES seg11:0x1d71 seg11:0x1d92 seg11:0x1d97 seg11:0x1d9b seg11:0x1da1]
- **Countdown.** Per ship i the word DGROUP:0x1858 + 2i is decremented on every tick where it is > 0. When it is ≤ 0 that tick does no repair and reloads it to max(1, 9 − `mass`/75). [BYTES seg11:0x1eab seg11:0x1e6f seg11:0x1e75 seg11:0x1e97]
- **Repair** runs on a tick with the countdown > 0 when design word DGROUP:0x971e (Repair System) ≠ 0, the ship is the player or the mode is not seg18, and `fuel` > 90, tested once before the lamps. [BYTES seg11:0x1ebd seg11:0x1ece seg11:0x1edc]
  - It skips a lamp whose row DGROUP:0x96f4 + 2k is −1, a health ≥ 82, and lamp 3 at health 0 or 50. [BYTES seg11:0x1df7 seg11:0x1e08 seg11:0x1e15 seg11:0x1e1c]
  - Each repaired lamp gains 2 (the player, k ≠ 3) or 1, one more in a kind-6 wave, and costs 2 `fuel`, so one tick can spend 8 and end below 90. [BYTES seg11:0x1db0 seg11:0x1dba seg11:0x1dbf seg11:0x1dd0 seg11:0x1dd9 seg11:0x1e3d]
- On every tick with the countdown > 0, repaired or not, ship dword +0x8c = max(27, 4·(75 − word +0x28)) / 27; +0x8c is the gun's extra interval ([The gun](#the-gun-seg110x4e0a)). [BYTES seg11:0x1e44 seg11:0x1e4c seg11:0x1e53 seg11:0x1e62 seg11:0x4e51]
- Repair stops at 82 or 83, so a lamp passes 75 and gives its completion message, and only the ambulance brings a system back to 100. [BYTES seg11:0x1e08 seg11:0x6a7]
- The manual stops auto-repair at 15 % fuel; the bytes test 90 absolute, about 7 % of the TRANSFORMER's 1320. [MANUAL Special Ship Systems] [MANUAL Resource Management] [BYTES seg11:0x1edc] [ORACLE probe-0001]

## Capture

### The touch routine (seg13:0x16ac)
- seg13:0x16ac(ship, pool·256 + rec) (cdecl) is reached from the ship mover ([Ship against ship](#ship-against-ship-seg130x0)) and the pool-6 mover. Early exits, in order: [BYTES seg13:0x16be seg13:0x1fb8]
  1. the object's `capturable` (+0x53) = 0; [BYTES seg13:0x16d7 seg13:0x16db]
  2. the ship is DGROUP:0x952e while DGROUP:0x9e49 is 1 … 6; [BYTES seg13:0x16df seg13:0x16f3]
  3. a dead ship (`alive` (+0xec) = 0) takes seg8:0x45e0(14, ship, 1), one point of damage, and returns, unless the object is pool 11 with DGROUP:0x856a + 0x34·rec = ship; [BYTES seg13:0x16fc seg13:0x1703 seg13:0x170f seg13:0x1715 seg13:0x171e]
  4. ship byte +0xea ≠ 0 or `cloaked` (+0xe8) ≠ 0; [BYTES seg13:0x172e seg13:0x1737 seg13:0x1740]
  5. a pool-0 object whose byte +0x2f is negative. [BYTES seg13:0x174d seg13:0x1752]
- Every touch that gets this far PatBlts the object's square BLACKNESS, calls seg13:0x6716(ship) and seg8:0x7d70, and sets the **player's** ship dword +0x156 = now − 1, whichever ship touched. [BYTES seg13:0x1756 seg13:0x177e seg13:0x1782 seg13:0x178b seg13:0x17f6 seg13:0x1802 seg13:0x180e]
- The manual: icons are captured by flying into them, frozen icons cannot be captured until thawed, and neither cloak nor shield lets a ship capture. [MANUAL QuickStart] [MANUAL Icons] [MANUAL Special Ship Systems]

### Who captures
- The pool-0 branch runs only when the icon's byte +0x51 = 0 (not dissolving) and the ship's design word DGROUP:0x9714 ≠ 0. [BYTES seg13:0x1816 seg13:0x1822 seg13:0x1830 seg13:0x1836]
- seg14 writes DGROUP:0x9714 = 1 or 0 from the SHP flag 0x10, Captures Icons ([formats.md](formats.md#mastershp-ships)). seg7:0x1e4e and seg11:0xaffe force 1 for the player's design, so the player always captures; seg13:0xb63a writes 0. [BYTES seg14:0x1eb4 seg14:0x1ebb seg14:0x1ec8 seg7:0x1e4e seg11:0xaffe seg13:0xb63a]

### Capturing an icon
1. The icon gets `state` = 1, byte +0x51 = 11 and word +0x48 = the capturing ship. [BYTES seg13:0x1842 seg13:0x184a seg13:0x1854]
2. seg13:0x3008(pool·256 + rec, ship). [BYTES seg13:0x185d]
3. In a kind-6 wave, when the capturer is not seg4:0x8d3a(), it calls seg13:0xb6f2(ship, 8) and twice seg13:0x2714(seg4:0x8d3a(), ship, −50, −80, 1, 2). [BYTES seg13:0x1863 seg13:0x18a5]
4. seg7:0x5ae8(rec, 1), the [counters](#counters-seg70x5ae8-and-seg70x5a46). [BYTES seg13:0x18ac]
5. seg7:0x3a42(rec), the first [dissolve](world.md#icon-dissolve-seg70x3a42) step, at once. [BYTES seg13:0x18b7]
6. seg13:0x4eec(ship, 35). seg13:0x4eec(ship, v) adds (1000 − w)·v / 1000 to ship word +0x82 (32-bit, seg5:0x110) for v > 0, and for v ≤ 0 sets w = (w + v)·1000 / (v + 1000). [BYTES seg13:0x18c3 seg13:0x4eec seg13:0x4ef7 seg13:0x4f18]
7. **Virused icon** (`capturable` (+0x53) = 2), then return without score:
   - `resources` (+0xce) −= 100, floored at 0, and seg3:0x6ec(ship, 7, 0, 0x48): sound 0x67 through seg10:0x744c and string 652 "You're In Trouble! You Captured A Virused Icon!"; [BYTES seg13:0x18d7 seg13:0x18e5 seg13:0x18e9 seg13:0x18ee seg13:0x18f4 seg13:0x18fd seg3:0xfdd seg3:0xfdf seg3:0xfe7] [BYTES ENG_LANG.DLL STRING 41] id 652
   - for the player with `PlayMode` < 2: in a player game DGROUP:0x95bc = 0 and `PlayMode` = 2 (from 0) or 3 (from non-zero); in the demo with `PlayMode` ≠ 0, `PlayMode` = 4. When `PlayMode` is then > 2 it puts strings 1700 + 1701 "Out Of Control. That" "Icon Didn't Taste So Good!" into DGROUP:0x8630 with DGROUP:0x58c = 1 and DGROUP:0x5c0 = now + 1200; [BYTES seg13:0x1917 seg13:0x1925 seg13:0x192b seg13:0x1941 seg13:0x1947 seg13:0x195d seg13:0x1963 seg13:0x196e seg13:0x198f seg13:0x19ac seg13:0x19bc] [BYTES ENG_LANG.DLL STRING 107] ids 1700–1701
   - ship dword +0x94 = now + 15000 and DGROUP:0x8690 = now + 7500; the nearest `state`-0 record of pools 3, 8 (word DGROUP:0x3d6c + 0x34·rec = 2 only) and 9, by seg8:0x285a from 20000, goes into ship words +0x06 and `goal_pool` (pool) and +0x08 and `goal_rec` (record), with 2 in +0x36 … +0x3c. [BYTES seg13:0x19c7 seg13:0x19d2 seg13:0x19de seg13:0x19e4 seg13:0x19ed seg13:0x1a03 seg13:0x1a4d seg13:0x1a87 seg13:0x1a8d seg13:0x1a93 seg13:0x1ab0 seg13:0x1ac3 seg13:0x1ad6]
   - The manual: a virused icon gives no resources, and the ship flies out of control and bashes into things while auto-repair purges the virus. [MANUAL Icons]
8. **Good icon:**
   - **Value.** DGROUP:0x9604 = (`damage` × −90)/210 + 100 (16-bit imul, signed idiv); a negative value increments DGROUP:0x9498 and becomes 50. [BYTES seg13:0x1ae7 seg13:0x1aec seg13:0x1af0 seg13:0x1af2 seg13:0x1afc seg13:0x1b00]
   - The capturer's `resources` gain DGROUP:0x9604 and its captured count +0xd2 gains 1. [BYTES seg13:0x1b0e seg13:0x1b12]
   - **Teammate.** A non-player capturer whose team (design word DGROUP:0x970a) is the player's sends seg3:0x6ec(player, 14, ship, 0x6f), string 1062 "I've Captured An Icon. Score 1 For The %s." with the team name from seg13:0x218a, and adds 1000 to the score and to the rating. [BYTES seg13:0x1b19 seg13:0x1b28 seg13:0x1b32 seg13:0x1b38 seg13:0x1b42 seg13:0x1b4a seg13:0x1b53 seg3:0x118f seg3:0x11a3 seg3:0x11b2] [BYTES ENG_LANG.DLL STRING 67] id 1062
   - **Defenders.** Every pool-2 record k < DGROUP:0x940a with `state` 0, DGROUP:0x5304 + 0x24k = 0 and DGROUP:0x5306 + 0x24k = the icon's record passes to the capturer (DGROUP:0x5304 = 14, DGROUP:0x5306 = ship) and counts in DGROUP:0x9602, cleared first. With DGROUP:0x9602 ≠ 0 it calls seg2:0x170e and seg10:0x744c(0x69). [BYTES seg13:0x1b5e seg13:0x1b6e seg13:0x1b79 seg13:0x1b83 seg13:0x1b8c seg13:0x1b93 seg13:0x1b97 seg13:0x1ba4 seg13:0x1ba6 seg13:0x1bad seg13:0x1bb2 seg13:0x1bb4] The manual: the defenders of a captured icon become yours. [MANUAL Icons]
   - **Sound** seg10:0x754a(id, 0, rec, 1): 0x23 when (DGROUP:0x9602 = 0 and the icon's `fixed` (+0x54) = 0) or the wave is kind 6; else 0x26 when DGROUP:0x9602 < 2; else 0x66. [BYTES seg13:0x1bba seg13:0x1bcf seg13:0x1bd6 seg13:0x1be5 seg13:0x1be9 seg13:0x1bf8 seg13:0x1c04 seg13:0x1c06]
     - 0x23 is CAPTURED.WAV. ISPACE.DLL's switch sends 0x26 to the voice C_NOTBAD.WAV ("Not bad."), or 0x23 while its DGROUP:0x696 (VoiceFX) = 0, and 0x66 to one of 0x47 … 0x49 (YORE_HOT, PERFORM, SLICK) by its own `rand()` % 3, a repeat of the last moving to the next. [BYTES ISPACE.DLL seg9:0xc72 seg9:0xd7d seg9:0xd0e seg3:0x668 DGROUP:0x696 DGROUP:0xcfc] [REF re/artifacts/sounds.tsv]
   - **Message.** seg3:0x6ec(ship, 7, 0, 0x55), shown only for the player ([Messages](#messages-seg30x6ec)): with DGROUP:0x9602 = 0 string 574 "Good Icon Capture! %s Resource Gained." with seg6:0x1010(DGROUP:0x9604, 100, 2); with defenders string 575 "Good Icon Capture! %# Acquired." with (DGROUP:0x9602, string 2530 "Defender"), %# taking 12530 "Defenders" for a count ≠ 1. [BYTES seg13:0x1c0e seg13:0x1c10 seg13:0x1c12 seg13:0x1c17 seg3:0xe21 seg3:0xe26 seg3:0xe2d seg3:0xe2f seg3:0xe31 seg3:0xe35 seg3:0xe44 seg3:0xe4a seg3:0xe4d seg3:0xe53] [BYTES ENG_LANG.DLL STRING 36] ids 574–575 [BYTES ENG_LANG.DLL STRING 159] id 2530 [BYTES ENG_LANG.DLL STRING 784] id 12530
   - **Score.** For the player, (DGROUP:0x9602 + 4) × 1000 is added to the score DGROUP:0x1e58 and to the rating DGROUP:0x5078: 4000 without defenders. [BYTES seg13:0x1c1f seg13:0x1c25 seg13:0x1c27 seg13:0x1c2a seg13:0x1c2d seg13:0x1c32 seg13:0x1c3a seg13:0x1c3e] [ORACLE wave-0001]
- **Number format.** seg6:0x1010(v, d, n, dest) writes "-" (DGROUP:0x491) and negates when v < 0, then decimal(v / d), "." (DGROUP:0x493), and decimal(v % d) through seg6:0x10ce cut to n characters, without zero padding: 100 → "1.0", 150 → "1.50", 7 → "0.7". [BYTES seg6:0x1010 seg6:0x102e seg6:0x1066 seg6:0x10ce] [ORACLE wave-0001]
- The oracle's two captures scored 4000 each and the next ambulance transferred 1.75, credit 3.25; which capture was worth what is not observed. A capture worth 0.75 has `damage` 60, because the icon's damage moves in steps of 3. [ORACLE wave-0001] [BYTES seg13:0x1ae7 seg7:0x2375]

### Other touches
- **Pool 4 (fuel tank).** With ship byte +0xea = 0 and pickup byte +0x51 = 0: `state` 1, +0x48 = ship, `fuel` = min(tank, `fuel` + 250), seg13:0x4eec(ship, 30), seg10:0x754a(0x71, 14, ship, 1), +0x51 = 10, seg13:0x3008, seg12:0xe5c(rec) and DGROUP:0x698c = now + 10000. [BYTES seg13:0x1c42 seg13:0x1c4d seg13:0x1c6d seg13:0x1c8a seg13:0x1c90 seg13:0x1c9e seg13:0x1cb5 seg13:0x1cb7 seg13:0x1cc3 seg13:0x1cd8 seg13:0x1ce8]
- **Pool 6.** A pool-6 record whose weapon word W(w)+0x42 (DGROUP:0x2f96) ≠ 0 adds 20 `fuel`, capped at the tank; the touch also sets object word +0x26 = 1, `tumbling` (+0xe2) = 1 and dword +0x84 = now + 1200. [BYTES seg13:0x1e8d seg13:0x1edb seg13:0x1ee8]

### Counters (seg7:0x5ae8 and seg7:0x5a46)
- seg7:0x5ae8(rec, captured) first calls seg7:0x5a46. With captured ≠ 0 it adds 1 to directory word +0x150 (CAPTURED) and to DGROUP:0x945c; with captured = 0 it adds 1 to +0x14e (DESTROYED) and DGROUP:0x945e. [BYTES seg7:0x5af4 seg7:0x5b07 seg7:0x5b0c seg7:0x5b1c seg7:0x5b3d]
  - With captured = 0, when the icon's byte DGROUP:0x57b2 + 0x23·rec is the ship pool, ship word +0xd0 (destroyed) of ship DGROUP:0x57b3 + 0x23·rec gains 1. [BYTES seg7:0x5b21 seg7:0x5b39]
  - It sets directory byte +0x1d0 = 2 (complete) when DGROUP:0x91fc = 0 and the byte is not 1. [BYTES seg7:0x5b41 seg7:0x5b5a]
- seg7:0x5a46 sets **DGROUP:0x91fc, the live-icon count**, to the number of pool-0 records 0 … 7 (always 8) with `state` 0, byte +0x2e = 0 and byte +0x51 = 0. [BYTES seg7:0x5a55 seg7:0x5a5f seg7:0x5a66 seg7:0x5a78 seg7:0x5a7f seg7:0x5a86 seg7:0x5a8b]
- For every other record 0 … 7 whose icon number n = DGROUP:0x57ae + 0x23·i is ≥ 0 it sets bit n % 8 of byte +0x1e2 + n/8 of the current directory record DGROUP:0x69f4 + 0x297·DGROUP:0x94a2: captured and destroyed icons alike. [BYTES seg7:0x5a8d seg7:0x5a91 seg7:0x5a98 seg7:0x5aa4 seg7:0x5ab1 seg7:0x5ab7 seg7:0x5ad8]
- At the briefing seg9:0xb2a sets DGROUP:0x57ae + 0x23·i = −1 for every pool-0 record below the in-use count and `state` 1 on pools 0 and 1, and calls seg9:0x0(8); in a new directory (DGROUP:0x94b0 ≠ 0) it also sets icon `damage` and directory word +0x1d2 + 2i to −1, clears directory byte +0x1d1 and word +0x150, and copies +0x150 to +0x14e. [BYTES seg9:0xb43 seg9:0xb49 seg9:0xb58 seg9:0xb77 seg9:0xbc6 seg9:0xbd6 seg9:0xbf0]

### The icon after capture
- `Detail` sets the dissolve step DGROUP:0x8ffe (seg6:0xdaa): 3 for `Detail` 0, 2 for 1, 1 for 2 and 3 (and DGROUP:0x9154 to 4, 7, 13, 18). [BYTES seg6:0xe36 seg6:0xe41 seg6:0xe4f seg6:0xe5d seg6:0xe6d seg6:0xe72 seg6:0xe75 seg6:0xe78]
- The dissolve's end call seg7:0x2080(0, −1) does nothing: seg7:0x2080 changes in-use counts only for pools 1, 2, 4 … 11 and 19. [BYTES seg7:0x208d seg7:0x20c9 seg7:0x20cd seg7:0x3b9d]
- **Icon column.** seg7:0x65aa(0, rec) calls seg7:0x64ac(0, 1, rec), which returns at once when DGROUP:0x9458 = 0 and otherwise draws row rec through seg13:0x5a6a(hdc, 0, 46·rec + 4, 0, rec, rec + 3, 1, DGROUP:0xb6e); it destroys the column with seg7:0x63f2 when DGROUP:0x91fc = 0 and it released its own DC. [BYTES seg7:0x65aa seg7:0x65c5 seg7:0x64ac seg7:0x63f2]
  - With flag 1, seg13:0x5a6a ([world.md](world.md#object-pictures-seg130x5a6a)) clears the cell to black and always calls seg13:0x5c22, which draws a pool-0 picture only for `state` 0. A captured icon (`state` 1, +0x51 = 0) therefore leaves its row black, and its strip cell is the one the dissolve blacked. [BYTES seg13:0x5ad3 seg13:0x5be2 seg13:0x5d5f seg7:0x3b88]
  - seg7:0x65aa also calls seg13:0x6450 for a non-player ship, and seg13:0x58dc(0, 1) when the record is in the target slot (DGROUP:0x6e52/0x6e54) with DGROUP:0xa76 = 2 or 3. [BYTES seg7:0x65aa]
- **Retarget.** The 280 ms IS_INFO step seg13:0x24aa(0), outside a race and with the player's `target_pool` ≠ −1, calls seg3:0xf4(player, seg13:0x2206()) when seg3:0x0(target) = 0 and seg13:0x371c(target) ≠ 1. A captured icon (`state` 1) fails seg3:0x0, so the display moves on within 280 ms; no AUTO/MANUAL test is on this path. [BYTES seg13:0x24b9 seg13:0x24bf seg13:0x24ce seg13:0x24e2 seg13:0x24ec seg13:0x24fa seg13:0x2500 seg13:0x2504 seg13:0x2518]

### Shooting an icon (seg7:0x233c)
- The icon's `on_damage` seg7:0x233c skips damage when DGROUP:0x57a8 + 0x23·rec ≠ 0 or byte +0x2f is −100 or −2; otherwise it adds 3 × amount to `damage`, capped at 211, and copies it to directory word +0x1d2 + 2·rec. [BYTES seg7:0x234f seg7:0x2370 seg7:0x2375 seg7:0x2386]
- It shatters the icon through seg7:0x26c4 when the amount ≠ 0 and `damage` > 208, or when byte +0x2f < 0. [BYTES seg7:0x24da seg7:0x24ef]
- seg7:0x26c4 sets `damage` and directory word +0x1d2 + 2·rec to −1, blacks the square, plays seg10:0x754a(0x1f, 0, rec, 1), sets `state` 1, calls seg7:0x2768(rec) and seg7:0x65aa(0, rec), and then seg7:0x5ae8(rec, 0), a destroyed icon. [BYTES seg7:0x26c4 seg7:0x26d8 seg7:0x2758]
- With `damage` ≤ 211 a captured icon is worth at least 100 − 90 = 10, so the "negative → 50" fallback does not arise from these paths. [BYTES seg7:0x2386 seg13:0x1afc]
- Destroying a virused icon is legal and destroying a good one is vandalism. [MANUAL Icons] [MANUAL Laws of Inner Space]

### Target validity (seg3:0x0)
- seg3:0x0(pool, rec) returns 0 for a negative pool or record, a pool above DGROUP:0x9324 or a record at or above the allocated count DGROUP:0x92ec + 2·pool. [BYTES seg3:0xe seg3:0x14 seg3:0x1a seg3:0x2f seg3:0x33]
- For pool 14 (the constant) and a record other than the player it returns 1 for `state` 0, and for another state when ship byte +0x147 ≠ 0. [BYTES seg3:0x35 seg3:0x3b seg3:0x51 seg3:0x5d seg3:0x62 seg3:0x64]
- Otherwise `state` ≠ 0 gives 0; pools 0, 7, 1, 9 and 11 give 1; pool 2 gives 1 when DGROUP:0x52fe + 0x24·rec = 15 or (DGROUP:0x5304 + 0x24·rec = 14 and DGROUP:0x5306 + 0x24·rec = the player); pool 8 gives 1 when DGROUP:0x3d6c + 0x34·rec = 2 and DGROUP:0x3d5a + 0x34·rec > 0; any other record gives 1 only when seg13:0x371c returns 2. [BYTES seg3:0x78 seg3:0x7f seg3:0x85 seg3:0x8b seg3:0x9b seg3:0xa2 seg3:0xac seg3:0xb2 seg3:0xb8 seg3:0xc8 seg3:0xcf seg3:0xda seg3:0xe2 seg3:0xe5]
- seg13:0x371c(pool, rec) returns 0 outside pool 12, 1 for an even pool-12 record and 2 for an odd one. [BYTES seg13:0x3729 seg13:0x3733 seg13:0x3739 seg13:0x373e]
- **Names.** seg3:0x1542 names a pool-0 record "Icon " + its record index (string 152 with the last 2 characters cut, seg6:0xf1a), with caps ≠ 0 adding " (" + the file name DGROUP:0x57b4 + 0x23·rec + ")"; pool 12 (seg13:0x371c ≠ 0) is 585 "Exit Gate", pool 3 153 "Rock", pool 9 170 "Virus", pool 2 154 "Defender", pool 7 347 "Ambulance" and pool 1 892 "Icon Fragment". Pools 3 and 9 add 10000 to the id in mode 7. [BYTES seg3:0x15ce seg3:0x15e0 seg3:0x15e4 seg3:0x164c seg3:0x1659 seg3:0x166f seg3:0x167c seg3:0x168c seg3:0x169e seg3:0x1747 seg3:0x1772 seg3:0x16f8 seg3:0x16e6 seg3:0x16c6 seg3:0x16ab] [BYTES ENG_LANG.DLL STRING 10] ids 152–154 [BYTES ENG_LANG.DLL STRING 11] id 170 [BYTES ENG_LANG.DLL STRING 37] id 585 [BYTES ENG_LANG.DLL STRING 22] id 347 [BYTES ENG_LANG.DLL STRING 56] id 892 The oracle's icons read ALPHA 0, BRAVO 1, CHARLIE 2. [ORACLE wave-0001]

## Scoring
- The score and rating words, the kill scorer and the other awards are in [globals.md](globals.md#scoring). The capture awards are listed there as seg13:0x1c32 and seg13:0x1b4a; they are paid as [Capturing an icon](#capturing-an-icon) describes. [DESIGN]
- **Display.** seg11:0x1bcc calls seg11:0x4a0(0) when the score differs from DGROUP:0x5018, which reaches the digit routine seg11:0x4ea when seg11:0x2120 passes ([world.md](world.md#score)). [BYTES seg11:0x1bda seg11:0x1bde seg11:0x1be3 seg11:0x1be8 seg11:0x4b8]
- **No wave-end bonus.** The additions to DGROUP:0x1e58 are seg4:0x2ef8, seg7:0x196a, seg8:0x6785, seg8:0x67a6, seg8:0x7971, seg8:0x7c3b, seg8:0x92cd, seg13:0x1b4a, seg13:0x1c32 and seg16:0x7228; the only other writes are the stores at seg9:0xc4e and seg6:0x968a. None lies in seg9:0x6bc2, seg9:0x6cc4, seg9:0x2828 or the debriefing seg9:0x6fee. [BYTES seg4:0x2ef8 seg7:0x196a seg8:0x6785 seg8:0x67a6 seg8:0x7971 seg8:0x7c3b seg8:0x92cd seg13:0x1b4a seg13:0x1c32 seg16:0x7228 seg9:0xc4e seg6:0x968a]

## The wave end

### Leaving through the gate (seg12:0x1776)
- The gate mover seg12:0x1776(p) tests gate p's beam with seg12:0x1f7a(p) ([world.md](world.md#gates)). [BYTES seg12:0x1dca]
- **The player.** When seg12:0x1f7a returns the player's ship with object byte +0x2e = 0 or `cloaked` (+0xe8) ≠ 0, it calls seg12:0x2288(14, player) and sets DGROUP:0x1e80 = now + 1000. Nothing on this path tests DGROUP:0x91fc or directory byte +0x1d0: the player can leave with icons left. [BYTES seg12:0x1dca seg12:0x1dd5 seg12:0x1dff seg12:0x1e11 seg12:0x1e17 seg12:0x1e28 seg12:0x1e35 seg12:0x1e3b]
- **Anything else** returned with byte +0x2e = 0 (or a +0xe8 ship) gets seg12:0x2288, a BLACKNESS PatBlt of its square and seg10:0x754a(0x27, pool, rec, 1). [BYTES seg12:0x1e45 seg12:0x1e63 seg12:0x1e81 seg12:0x1eb2 seg12:0x1ebb seg12:0x1ebd]
- **Exit sign.** While DGROUP:0x91fc = 0 the gate toggles DGROUP:0xd0a every 400 ms (deadline DGROUP:0x1928) and BitBlts two halves of bitmap OUT_SIGN (DGROUP:0xd0c) into DC DGROUP:0x39ea at frame x = 13·DGROUP:0xd0a, scaled. [BYTES seg12:0x1b3b seg12:0x1b42 seg12:0x1b54 seg12:0x1b6a seg12:0x1b79 seg12:0x1c02 seg12:0x1c58 seg12:0x1c64]
- The manual's "all icons must be captured or destroyed before you can return from Inner Space" is the game's end, not a condition for leaving a wave. [MANUAL Icons] [MANUAL General]

### seg12:0x2288(pool, rec): an object goes through
- A pool-5 (DGROUP:0x9328) object plays 0x27, gets +0x2e = 0xff and returns. A ship with `alive` (+0xec) = 0 goes to seg12:0x274a. [BYTES seg12:0x2288 seg12:0x236d]
- An on-screen object (`on_screen` ≠ 0) gets seg12:0x2556 and seg7:0x13b2(1, pool, rec); an off-screen one gets `state` 1, seg7:0x2080 and sound 0x27. A ship gets word +0x2e (DGROUP:0x70be) = 1. [BYTES seg12:0x237b seg12:0x23f5]
- A pool-0 object calls seg7:0x5ae8(rec, 0): an icon that drifts through a gate counts as destroyed, with no score and no resource. [BYTES seg12:0x24d7 seg12:0x24e2]
- DGROUP:0x93a4 = 1 (the Hall of Justice) at seg12:0x2472 and seg12:0x24b1 in two team-7 cases; the first also sets DGROUP:0x1e80. [BYTES seg12:0x2472 seg12:0x24b1]
- With DGROUP:0x92e8 ≠ 0, ship 1 clears the player's `fixed` (+0x54); in a kind-7 wave DGROUP:0x668a = −1 when no other live ship remains. [BYTES seg12:0x2288]

### The tick takes it up (seg7:0x610)
- When DGROUP:0x1e80 ≠ 0 and not above now, in a player game, the tick sets `PlayMode` = 1 when it is above 1, sets DGROUP:0x1e84 = now (death, [shell.md](shell.md#screen-states)) when the player's `alive` (+0xec) = 0, else calls seg9:0x6bc2, and then clears DGROUP:0x1e80. [BYTES seg7:0x610 seg7:0x622 seg7:0x62b seg7:0x6dd seg7:0x6e4 seg7:0x6f0 seg7:0x6f7 seg7:0x700 seg7:0x6a0 seg7:0x705]
- In the demo the same expiry runs seg6:0x92b8 when directory byte +0x1d0 = 2 and DGROUP:0xd26 ≠ 0, goes to state 8 when DGROUP:0x93a4 ≠ 0, and otherwise calls seg9:0x2828, seg6:0x885c(0, 0) when the player is alive and DGROUP:0x9516 = 0, and seg9:0xd6c. [BYTES seg7:0x62b seg7:0x652 seg7:0x659 seg7:0x66a seg7:0x675 seg7:0x67c seg7:0x687 seg7:0x68f seg7:0x69a]
- **seg9:0x6bc2** starts with seg10:0x7480(−1, 1). In a race wave each other ship with `goal_pool` = 0 is credited every pool-0 record still in `state` 0: `resources` += (`damage` × −90)/210 + 100, +0xd2 += 1 and seg7:0x5ae8(rec, 1), with no clamp and `state` left at 0, so every such ship is credited the same icons. [BYTES seg9:0x6bc2 seg9:0x6be3 seg9:0x6c22 seg9:0x6c59]
  - With DGROUP:0x93a4 ≠ 0 it sets state 8, DGROUP:0x929c = 0, calls seg16:0x6928(0), sets DGROUP:0x9346 = −1 and clears `alive` (+0xec) of team-7 ships escorting the player; otherwise it calls seg9:0x6cc4(0). [BYTES seg9:0x6c5b seg9:0x6c62 seg9:0x6cb8]
- **seg9:0x6cc4(arg)** calls seg10:0x7db8 and seg13:0x65f2(1), sets state 2 (the debriefing), clears DGROUP:0x1e74 and DGROUP:0x92e4, and in a kind-6 wave calls seg9:0x6db0, which fills every ship's `fuel` to its tank. [BYTES seg9:0x6cd8 seg9:0x6ce1 seg9:0x6ce5 seg9:0x6ceb seg9:0x6cf6 seg9:0x6dc5 seg9:0x6dd4]
  - It frees DGROUP:0x92e0, calls seg9:0x1092, seg12:0x26a4(pool, rec) for every allocated record of pools 0 … 14, seg11:0x2f0 and seg7:0x49b6, blacks the client, sets DGROUP:0x93b2 = DGROUP:0x9332, calls seg9:0x2828 and seg6:0x7266, sets DGROUP:0x94c4 = 1 and DGROUP:0x9330 = 0, and draws the debriefing frame seg9:0x6dec(1) only when arg ≠ 0. [BYTES seg9:0x6d5c seg9:0x6d7a seg9:0x6d85 seg9:0x6da4]

### seg9:0x2828: the wave into the directory record
- Its difficulty outputs (DGROUP:0x9332 and the one `rand()` of DGROUP:0x932c) are in [world.md](world.md#difficulty-inputs). [BYTES seg9:0x2a17 seg9:0x2b25 seg9:0x2b3d seg9:0x2b4d]
- It calls seg13:0x2688 for every ship and sets DGROUP:0x9346 from a team-7 ship; in kind-6 waves with DGROUP:0x91f8 ≠ 0 and (DGROUP:0x8f70 ≠ 0 or the demo) it writes the race tables through seg6:0x8b90, seg6:0x8cb2, seg6:0x8dfe and seg6:0x8eb4. [BYTES seg9:0x2828]
- With DGROUP:0x91f8 > 0 it calls seg10:0x9672, seg10:0x552(4, 1) and seg10:0x565a. [BYTES seg9:0x2828]
- **Remaining icons.** It packs the `state`-0 pool-0 records into directory slots 0, 1, …, storing `damage` in word +0x1d2 + 2s and byte +0x1c8 + s = 2 for heat +0x2f < 0 and 1 for `capturable` (+0x53) = 2 (1 overwriting 2), and zeroes the remaining slots up to 8; it does not clear +0x1c8 + s for a plain icon. [BYTES seg9:0x2f82 seg9:0x3077]
- **LASTGAME.** At seg9:0x31f0 it saves `lastgame` (`demogame` in the demo) through seg6:0x885c(0, 0), under the conditions of [formats.md](formats.md#names-and-triggers). [BYTES seg9:0x31f0] [MANUAL High Scores]

### The debriefing
- Strings 3006 (seg9:0x7173) and 3007 (seg9:0x71b3) take directory words +0x14e/+0x150 and DGROUP:0x94a6. The ship's words +0xd2/+0xd0 minus DGROUP:0x9370/0x9372 feed string 3012 ", %<CAPTURING %#…". [BYTES seg9:0x701f seg9:0x702a seg9:0x7173 seg9:0x71b3 seg9:0x7fd5 seg9:0x7fdd] [BYTES ENG_LANG.DLL STRING 188] ids 3006–3007 [BYTES ENG_LANG.DLL STRING 189] id 3012
- The debriefing, Navigation and the next briefing are in [shell.md](shell.md#screen-states) and [nav.md](nav.md#enter). [BYTES seg9:0x6ce5]

## Weapons

### Two fire paths
- **Gun.** seg11:0x4db0 calls seg11:0x4e0a(player) only when `PlayMode` = 0 and the player's dword +0x84 is below now (unsigned), on GetAsyncKeyState(VK_SHIFT) bit 0x8000, else GetAsyncKeyState(VK_SPACE), else the joystick word DGROUP:0x1042 ≠ 0. [BYTES seg11:0x4dba seg11:0x4dc8 seg11:0x4dce seg11:0x4dd6 seg11:0x4ddc seg11:0x4de3 seg11:0x4de8 seg11:0x4def seg11:0x4df4 seg11:0x4e00]
  - The player-`state` test is in the caller seg7:0x87a, which skips the poll while the player's `state` ≠ 0 ([world.md](world.md#update-loop) step 4). [BYTES seg7:0x871 seg7:0x87a seg7:0x87f seg7:0x881]
- **Weapon system.** On WM_KEYUP, VK_RETURN and 'Z' reach seg11:0xac86 through seg11:0x6e11 when `PlayMode` = 0. On WM_KEYDOWN, Enter and Z call seg11:0xac86 only when `PlayMode` = 0 and the selected weapon byte is 13 (WILDCARD). [BYTES seg11:0x6d2e seg11:0x6d30 seg11:0x6d4a seg11:0x6e11 seg11:0x6e16 seg11:0x6f58 seg11:0x6f5f seg11:0x6f69 seg11:0x6f6e seg11:0x6f74]
  - In the demo seg11:0x6b64 ignores every WM_KEYUP after the ESC/'R' block, so Z and Enter fire nothing. [BYTES seg11:0x6d19 seg11:0x6d1e]
- The manual: the gun fires on SHIFT or SPACE and a weapon system on Z or RETURN, and Tab changes the weapon. [MANUAL QuickStart] [MANUAL General] The oracle's Z launches one missile per press and release. [ORACLE wave-0001]

### The gun (seg11:0x4e0a)
- seg11:0x4e0a(s) (Pascal) stores 7 in DGROUP:0x594, the gun slot count. [BYTES seg11:0x4e19]
- **Gate**, all needed:
  - not (s = DGROUP:0x952e with DGROUP:0x9e49 in 3 … 6); [BYTES seg11:0x4e1f seg11:0x4e3a]
  - gun health, ship word +0x28, ≠ 0; [BYTES seg11:0x4e43]
  - ship dword +0x8c = 0 or +0x8c + dword +0xa0 ≤ frame#; [BYTES seg11:0x4e51 seg11:0x4e68]
  - dword +0xa0 + 5 ≤ frame#, so at most one shot per 5 frames; +0xa0 takes frame# on each shot; [BYTES seg11:0x4e6f seg11:0x4e7f seg11:0x50b7 seg11:0x50bb]
  - `on_screen` ≠ 0, or `goal_pool` ∈ {0, 9, 14}. [BYTES seg11:0x4e93 seg11:0x4eb4]
- **Aim.** A computer ship, or the player with `PlayMode` ≠ 0, fires only when the heading seg8:0x19b0 of the screen delta to object (`goal_pool`, `goal_rec`) equals its `frame`, unless that object is a ship with `cloaked` (+0xe8) ≠ 0, which always passes; it then calls seg11:0x5ab6(5, s). [BYTES seg11:0x4ec9 seg11:0x4fcb seg11:0x4fe2]
- The player's shot stores now in DGROUP:0x8e28. [BYTES seg11:0x4ff1]
- **Slots.** A slot owned by (14, s) whose age +0x76 ≤ W / ((DGROUP:0x594 − 2)·10) refuses the shot. A shot needs two free slots, or a slot owned by the player, or s = the player. [BYTES seg11:0x5009 seg11:0x5011 seg11:0x501a seg11:0x5025 seg11:0x502e seg11:0x5032 seg11:0x5034 seg11:0x5038 seg11:0x503f seg11:0x5045 seg11:0x504b seg11:0x5053]
- **Bookkeeping.** Ship byte +0xcc = max(1, 7 − 2·DGROUP:0x9000 − DGROUP:0x9334); dword +0x14a = the ship's object dword +0x04 (`x`, `y`); dword +0x14e = now. A player shot in Strategy mode calls seg7:0x1208. [BYTES seg11:0x5098 seg11:0x509d seg11:0x50a2 seg11:0x50a6 seg11:0x50b3 seg11:0x50cf seg11:0x50d5 seg11:0x50dc seg11:0x50e1 seg11:0x50e6 seg11:0x50fa]
- **Fill.** It takes the first free slot (+0x76 = −1) and sets +0x76, +0x7e and +0x80 to 0. [BYTES seg11:0x55ff seg11:0x5606 seg11:0x5608 seg11:0x560c seg11:0x5610]
  - Trail points are set to −100, with seg8:0x764 per point. [BYTES seg11:0x5118 seg11:0x5135]
  - Both barrels' world x (+0x30, +0x3c) = the ship's `x` and world y (+0x48, +0x54) = its `y`. [BYTES seg11:0x515f seg11:0x5168 seg11:0x516c seg11:0x5170 seg11:0x5174 seg11:0x5178]
  - Owner +0x7a = 14, +0x7c = s; barrel count +0x78 = `barrels`; barrel-alive words +0x84/+0x86 = the sign-extended bytes +0xe0/+0xe1. [BYTES seg11:0x517f seg11:0x5186 seg11:0x518e seg11:0x5192 seg11:0x5196 seg11:0x519b seg11:0x519f seg11:0x51a4]
  - Velocity +0x70 = 3·DGROUP:0x1050[4·`frame`] + ((`vx`/10)·10)/DGROUP:0x9006, and +0x72 the same with DGROUP:0x1052 and `vy`. [BYTES seg11:0x51a8 seg11:0x51af seg11:0x51c8 seg11:0x51d0 seg11:0x51d6 seg11:0x51da seg11:0x51ed]
  - Dword +0x90 = 0. [BYTES seg11:0x51f4]
  - The player's shot starts at the ship's position minus its velocity, a computer ship's at plus it. [BYTES seg11:0x51f9 seg11:0x5205 seg11:0x520b seg11:0x5243]
  - Lifetime +0x74 = W/10. [BYTES seg11:0x525f seg11:0x5263 seg11:0x526a]
  - Each player shot adds 1 to DGROUP:0x9432 unless `goal_pool` = 9. [BYTES seg11:0x5274 seg11:0x527a seg11:0x5281]
  - seg13:0x4eec(s, 1). [BYTES seg11:0x5285 seg11:0x528a]
  - +0x82 = 0, and byte +0x55 + j of every in-use record of pools DGROUP:0x9326 … 14 = 0. [BYTES seg11:0x5297 seg11:0x529d seg11:0x52a5 seg11:0x52b0 seg11:0x52c8 seg11:0x52df]
- **Muzzle.** One barrel: offset 8 × the pair DGROUP:0x7b60/0x7b62 + 4·(64·`design_index` + `frame`), then x = (x + 25600) mod 25600 through seg5:0x8c0 (shift) and seg5:0x1aa (the RTL signed 32-bit remainder), y alike with DGROUP:0x5a2. [BYTES seg11:0x52ef seg11:0x530b seg11:0x5314 seg11:0x5318 seg11:0x532d seg11:0x5347 seg11:0x535b seg11:0x538c seg5:0x8c0 seg5:0x1aa]
- Two barrels: barrel a adds 8 × the pair DGROUP:0x7be0/0x7be2 + 4·(16·(4·`design_index` + a) + `frame`). +0x68/+0x6a = 16·(barrel 1 − barrel 0) / the seg8:0x1290 length, at least 1, and dword +0x6c = 0x003e001c. [BYTES seg11:0x53e8 seg11:0x5400 seg11:0x541b seg11:0x54a8 seg11:0x556e seg11:0x5572]
- **Sound.** The player's shot calls seg10:0x744c(0x69) first. Then seg10:0x754a(id, 0x1e, j, 2) with id 3 for one barrel; for two barrels id 4 when bytes +0xe0 and +0xe1 are both ≠ 0, else 3. [BYTES seg11:0x5398 seg11:0x539a seg11:0x53a0 seg11:0x53a9 seg11:0x53ae seg11:0x5583 seg11:0x5585 seg11:0x558b seg11:0x5590 seg11:0x5597 seg11:0x55a5 seg11:0x55b0 seg11:0x55b2]
- A shooter that already owned a slot starts the new slot at age 15. [BYTES seg11:0x53bc seg11:0x55ba seg11:0x55c5]
- A race wave (kind 6 with byte DGROUP:0x9eb5 = 0) then calls seg11:0x5626(j); in seg18 mode seg18:0x49d8(j, lifetime − age) runs instead. [BYTES seg11:0x53c2 seg11:0x55ea]
- seg11:0x4e0a's own body calls no `rand()`; its callee chain seg11:0x5ab6 → seg11:0x63b4 does ([rand() sites](#rand-sites)). [BYTES seg11:0x4e0a]

### Shot slots
- The gun's shots live in nine 0x94-byte slots at DGROUP:0x6098, whose fields are in [structs.md](structs.md#shot-slots-9--0x94-bytes-at-dgroup0x6098). [BYTES seg8:0x3aec seg11:0x4e0a]

### Shot update (seg8:0x3aec)
- The wave frame calls seg8:0x3aec(i, j) for i = 0 … 4 and every slot j ([world.md](world.md#update-loop) step 6), one sub-step each. It acts while 0 ≤ age < lifetime. [BYTES seg7:0x938 seg7:0x942 seg7:0x946 seg7:0x95d seg7:0x965 seg8:0x3aec]
- Trail length T = ((`Detail` + 1) >> 1) + 3 and dot side e = (2 << `Size`) >> 1. [BYTES seg8:0x3aec]
- **Hit.** r = seg8:0x3688(j); any r ≠ 0 on a slot owned by the player increments +0x7e. r = 2 frees the slot (+0x76 = −1), blacks every trail point of each live barrel and returns 1. [BYTES seg8:0x3aec]
- **Ricochet.** When dword +0x90 equals the sub-step counter DGROUP:0x5cc, it plays seg10:0x754a(0x11, 0x1e, j, 2) and takes velocity (+0x88, +0x8a). Who writes +0x88/+0x8a/+0x90 besides the fire is [OPEN]. [BYTES seg8:0x3aec seg7:0x95d]
- **Draw.** Brush DGROUP:0x3b4c[age·8/lifetime + 10·ship byte +0xe3] for a ship's shot, DGROUP:0x3b60[age·8/lifetime] otherwise. Each live barrel's screen head moves by the owner's (`vx`, `vy`)·i / −50 and is drawn with PatBlt 0x00F00021, e × e, when 0 < x < W and 0 < y < H. [BYTES seg8:0x3aec seg8:0x3cf6 seg8:0x3d24 seg8:0x3d75]
- **Move.** Each live barrel's world point adds the velocity; outside seg18 mode a coordinate above DGROUP:0x5a0·8 (y: DGROUP:0x5a2·8) becomes 0 and one below 0 becomes the edge. [BYTES seg8:0x3aec]
- **Convergence.** With two barrels and (age & 0xf) = 0xf, barrel 0 gains (+0x68 >> 2, +0x6a >> 2) and barrel 1 loses it. [BYTES seg8:0x3aec]
- **Expiry.** At age ≥ lifetime the age becomes −1 and 6 trail points are blacked. The routine returns 0 only when the slot is free and DGROUP:0x9136 = 0. [BYTES seg8:0x3aec]
- A race wave calls seg11:0x5626. [BYTES seg8:0x3aec]

### Shot hit test (seg8:0x3688 and seg8:0x6f3c)
- seg8:0x3688 tests every visibility-list record outside pool DGROUP:0x9328 with seg8:0x6f3c. A ship's shot also tests its owner's target when that target is off screen. [BYTES seg8:0x3688 seg8:0x3779]
- It returns 2 when a barrel's world head lies in a gate rectangle (+0, +2 − 5, +4, +6 + 5) of the 0x1c-byte table DGROUP:0x6678, for DGROUP:0x9318 entries: shots stop at gate posts. [BYTES seg8:0x3688]
- **Gates of seg8:0x6f3c:** the target's `state` = 0; byte +0x55 + j < 2, or slot +0x82 ≠ 0, or seg18 mode; byte +0x2e = 0, or a ship whose ship word +0x2e = 2; and not the shooter itself while age ≤ 23. [BYTES seg8:0x6f3c]
- **Power.** p = 14 − age·14/lifetime, or 23 while age < 20; × 3/2 for the player's shot, or × 5/2 for a shooter whose team word DGROUP:0x970a = 7 and whose ship byte +0x5e = 0. For any ship's shot p = DGROUP:0x9704 (design word)·p / 250; × 3 in Demon's Gate for a shooter other than the player; and with a roster record p = (roster byte +0x32 + 5)·p·20/100. [BYTES seg8:0x7174 seg8:0x7197 seg8:0x71b7 seg8:0x71c5 seg8:0x71e5 seg8:0x71f8]
- **Impact.** seg10:0x7dd4 gets the velocities × 5 by reference. On a non-zero result the target's velocity becomes the mean of old and new (0 for a `fixed` target) and the slot's velocity the new one / 5. [BYTES seg8:0x7422 seg8:0x7489 seg8:0x74e8 seg8:0x74ec seg8:0x7504]
- **Sound.** Pool 4 and some pool-8 records play seg10:0x754a(0x1e, pool, rec, 1), the others seg10:0x754a(0x11, 0x1e, j, 2). [BYTES seg8:0x7508 seg8:0x7546]
- DGROUP:0x9432 gains damage/3 or damage/2 on the paths at seg8:0x7659 and seg8:0x7683. [BYTES seg8:0x7659 seg8:0x7683]
- **Damage.** It calls the target's `on_damage`(pool, rec, DGROUP:0x9438) and returns 2 when that reports destruction, else 1; only 2 frees the slot, so a shot that does not destroy flies on at the new velocity. [BYTES seg8:0x768f seg8:0x76ab seg8:0x7b20 seg8:0x7b2b]
- **Ship target.** For a ship with `alive` (+0xec) ≠ 0 it halves DGROUP:0x9438 in Demon's Gate unless the target is the player, sets object +0x26 = 1 when it is 0, `tumbling` (+0xe2) = 1 and a lock-out +0x84 = now + 12·DGROUP:0x9438. A pool-8 target gets DGROUP:0x3d54 + 0x34·rec = now + 120·DGROUP:0x9438. [BYTES seg8:0x7a95 seg8:0x7aa1 seg8:0x7aa3 seg8:0x7abb seg8:0x7ad6 seg8:0x7add seg8:0x7b00 seg8:0x7b1c]

### The weapon system (seg11:0xac86)
- It returns when ship byte +0xea or +0xe8 ≠ 0 or the player's object byte +0x2e ≠ 0. [BYTES seg11:0xac91 seg11:0xac97 seg11:0xac9e seg11:0xacb5] The manual: no weapon but the gun fires while cloaked or shielded. [MANUAL Special Ship Systems]
- w = the weapon byte +0xb8 + `weapon_slot`. When W(w)+0x46 = 0 and `goal_pool` ≠ 9, DGROUP:0x9432 += 3. [BYTES seg11:0xacc5 seg11:0xacc9 seg11:0xacd1 seg11:0xacd8 seg11:0xacdf]
- Every use calls seg13:0x4eec(player, 4) and stores now in DGROUP:0x8e28. [BYTES seg11:0xace4 seg11:0xacfa]
- W(w)+0x0a ≠ 0 calls the [launcher](#launch-seg110x7c12) seg11:0x7c12(player, 14, w). [BYTES seg11:0xad10 seg11:0xad17 seg11:0xad21]
- Otherwise its 23-entry switch on w − 1 at seg11:0xadba sends 1 to seg11:0x8a42, 2 to seg11:0x947a(…, 2), 3 to seg11:0x9fdc, 4 and 5 to seg13:0x1318(4 or 5, 0x1e, player, 14), 13 to seg11:0x9078, 15 to seg11:0x947a(…, 0xf) and 23 to seg11:0x9daa; every other case does nothing. [BYTES seg11:0xadac seg11:0xadb5 seg11:0xadba]
- seg13:0x1318(4) makes a pool-5 (DGROUP:0x1e0c) record of `mass` 200, SNOWBALLS, and seg13:0x1318(5) one of `mass` 120, FIREBALLS; seg7:0x40c4 draws `mass` 120 from image slot 10 and 200 from slot 11. [BYTES seg13:0x1318 seg7:0x40c4]
- seg13:0x1318 subtracts W(w)+0x2e (DGROUP:0x2f82) from `fuel`, floored at 0, only when `fuel` > 40. [BYTES seg13:0x135e seg13:0x136c seg13:0x1370 seg13:0x1374]
- seg11:0x947a (GRAPPLING HOOK, HANDCUFFS) makes pool-10 (DGROUP:0x1e20) records with link entries DGROUP:0x65d6 + 0x20·k. [BYTES seg11:0x947a]

### The shock (seg11:0x9daa)
- seg11:0x9daa, case 22 (code 23, UNCERTAINTY) of the switch, needs DGROUP:0x238c = 0 and for a ship user `fuel` ≥ 250, and sets DGROUP:0x2408 = now + 2000, DGROUP:0x238c = 9 and DGROUP:0x238e = 6. [BYTES seg11:0xad90 seg11:0xade6 seg11:0x9db8 seg11:0x9dce seg11:0x9de2 seg11:0x9de6]
- Every other ship gets `tumbling` (+0xe2) = 1, +0x84 = now + 3500 and object +0x26 = ±1 by record parity; live ships recover, since the tumble explodes only ships with `alive` = 0. [BYTES seg11:0x9e05 seg11:0x9e10 seg11:0x9e19 seg11:0x9e4c seg11:0x9e61 seg13:0x14b]
- Every record of the pool behind DGROUP:0x1e18 (DGROUP:0x9416 records, without the user's own when it is pool 8) gets +0x26 = ±1 and DGROUP:0x3d54 + 0x34i = now + 3000. [BYTES seg11:0x9e7d seg11:0x9e98]
- It then calls seg11:0x9078; in a Demon's Gate wave with one ship it sets the player's `fixed` (+0x54) = 1. [BYTES seg11:0x9ed4 seg11:0x9ef5]
- **View shake.** While DGROUP:0x238c ≠ 0, seg8:0x0 plays sound 0x69, a positioned sound seg10:0x754a(8, x, y, 4) with y = `rand()` % H drawn before x = `rand()` % W, adds DGROUP:0x238c/0x238e to the offsets DGROUP:0x2390/0x2392 and then a `rand()` % 3 − 2 jitter (−2 … 0) to each, bounces an offset past ±5 with velocity ∓9 (x) or ∓6 (y), adds 8 × offset to the view origin DGROUP:0x2400/0x2402, adds the offset to the player's `scr_x`/`scr_y` and redraws it with seg7:0x1208. [BYTES seg8:0x251 seg8:0x25b seg8:0x266 seg8:0x273 seg8:0x27b seg8:0x285 seg8:0x289 seg8:0x29a seg8:0x2b2 seg8:0x2c1 seg8:0x2ce seg8:0x2dd seg8:0x2e9 seg8:0x2f3]
- Once now > DGROUP:0x2408 it sets DGROUP:0x238c = 0, saves `scr_x`/`scr_y` into `old_scr_x`/`old_scr_y` and redraws the sprite. [BYTES seg8:0x357 seg8:0x35c seg8:0x360 seg8:0x367 seg8:0x3a0]
- No collision path starts the shake. [BYTES seg11:0x9daa]

### Launch (seg11:0x7c12)
- Pool 6 (DGROUP:0x1e10, in-use count DGROUP:0x9412) holds every launched weapon; seg11:0x7c12(s, pool, w) runs only while DGROUP:0x9412 < 10. Each pool-6 record m has a missile entry at DGROUP:0x66e0 + 0x1c·m ([structs.md](structs.md#other-wave-tables)). [BYTES seg11:0x7c12 seg8:0x6c67]
- When it grows pool 6 by a record, the new record gets `state` 1, `mass` 20, `hit_sound` 100, `radar_color` 0x7f00, `max_damage` −1, `blast_size` 5, `radar_draw` seg11:0x3914 and `on_damage` seg7:0x2652. [BYTES seg11:0x7c12]
- It returns without launching when a pool-6 record of the same launcher has 0 < age < 15. [BYTES seg11:0x7c12]
- **Ammo.** For a ship launcher it lowers the slot's count byte +0xae + l by 1 when W(w)+0x2e (DGROUP:0x2f82) = 0, else `fuel` by that word, floored at 0, and then posts WM_PAINT to DGROUP:0xa7e. [BYTES seg11:0x7c12]
- **Record.** `extent` = W(w)−8 << 3, `mass` = W(w)+0x10, `max_damage` = W(w)+0x08, `blast_size` = W(w)+0x22; `on_destroy` = seg12:0x274a, or seg7:0x2e02 when W(w)+0x24 is 4 or 6. [BYTES seg11:0x7c12]
- Weapon 16 sets `capturable` (+0x53) = 1 and points every design-7 ship at the missile. Kind 2 (W(w)+0x0a) calls seg3:0xf4; kind 5 picks its own target. [BYTES seg11:0x7c12]
- **Front launch** (W(w)+0x04 = 1, or a launcher outside the ship pool): velocity 4·heading plus the launcher's (for kinds other than 5), the nose offset (sizes/2)·heading/15, and a ship launcher's dword +0xa0 = frame# + 36. [BYTES seg11:0x7c12]
- A rear launch (W(w)+0x04 = 2) takes its point from the muzzle table DGROUP:0x7ba0, entries 16 … 31 of the design's 64. [BYTES seg11:0x7c12]
- With W(w)−2 = −1 (no graphic) it calls `rand()` once, at seg11:0x80c5, for seg6:0x84fc((r % 2 + 1)·10). [BYTES seg11:0x8087 seg11:0x80c5]
- **Sound.** The player's launch calls seg10:0x744c(0x69) and then seg10:0x7944(W(w)+0x18, 6, m, 1, DGROUP:0x1118); weapon 8's is 0x1c. [BYTES seg11:0x7c12 seg11:0x89fd seg10:0x2308]
- It ends with seg7:0x2080(6, −1) and increments DGROUP:0x9498 when record 0 is free. [BYTES seg11:0x7c12]
- Z launches one missile, lowering the display's count from 20 to 19; Tab changes nothing with TRANSFORMER's single weapon. [ORACLE wave-0001]

### Pool-6 mover (seg12:0x0)
- **Retarget.** seg12:0x586(age << 3, m) runs when age % 13 = 0, age > 8, the kind W(w)+0x0a is neither 1 nor 5, and (W(w)+0x3c ≠ 0, no target, or age = 13). [BYTES seg12:0x0 seg12:0xe5]
- A missile with no lock (+0x1a of the table = 0) at age = lifetime/2 jumps to lifetime − 5 (lifetime W(w)+0x1e). [BYTES seg12:0x0]
- With W(w)+0x3a = 0 an off-screen record older than 20 frames is set to its lifetime; weapon 8's word is 1. It animates through W(w)+0x06, applies drag through W(w)+0x40 and calls seg4:0x5514 every 4 frames when W(w)+0x38 ≠ 0. [BYTES seg12:0x0]
- It draws with seg8:0xb76(0, DGROUP:0x39e8[W(w)−4], 3, 0x600 | m) when W(w)−2 ≠ −1, only while `state` = 0. [BYTES seg12:0x2f9 seg12:0x307 seg12:0x30d seg12:0x313]
- **Collision.** An on-screen record older than 3 frames calls seg8:0x50aa and seg8:0x546c and passes the hit to seg8:0x5df2(6, m, pool, rec, result). [BYTES seg12:0x320 seg12:0x32b seg12:0x332 seg12:0x0]
- **End.** At age ≥ lifetime with W(w)+0x24 = 1 it calls seg12:0x274a(6, m) and seg7:0x2080(6, m) and sets `state` 1. [BYTES seg12:0x0]
- **seg12:0x586** skips pool 2, the missile itself and pool DGROUP:0x9328 when W(w)+0x14 = 1; a candidate needs `state` 0, `mass` > 30, kind 3 or byte +0x2f ≠ 0, and a seg8:0x285a distance below the radius. The launcher counts × 4, a ship of the launcher's design type × 7 outside Demon's Gate, ships × 2/3 for kinds other than 4, and kind 4 subtracts 25·byte +0x2f and halves pool DGROUP:0x9328. The target changes only when it is new or age > 60. [BYTES seg12:0x586]
- **seg12:0x7c0 (steer).** It calls `rand()` twice on every call, at seg12:0x8ad and seg12:0x8c6, adding (r % 300 − 150)·c·8 to the target x and y. c = 1 only for a ship target with `cloaked` (+0xe8) ≠ 0 in homing mode (DGROUP:0x66f4 = 2) with W(w)+0x3e = 0; c = 1 also clears the lock flag DGROUP:0x66fa. [BYTES seg12:0x886 seg12:0x896 seg12:0x8a1 seg12:0x8a8 seg12:0x8ad seg12:0x8b2 seg12:0x8ba seg12:0x8c0 seg12:0x8c6]
- **Fuse.** With c = 0 and a seg8:0x285a distance below W(w)+0x20 it doubles `blast_size` and calls seg12:0x274a and seg7:0x2080; weapon 8's word is 40. [BYTES seg12:0x7c0 seg10:0x22cc]
- **Thrust.** Velocity += heading (from seg13:0xd6c) · W(w)+0x0c, doubled when DGROUP:0x66f4 ≠ 2, then seg8:0x1cbe(W(w)+0x0e, 0x600 | m); weapon 8's words are 10 and 120. [BYTES seg12:0x7c0 seg10:0x22ba seg10:0x22c0]

### Movers and pools
- seg12:0x532c moves pool 9, seg8:0x8256 pool 2, seg7:0x309c pool 0, seg7:0x32d0 pool 1, seg12:0x3148 pool 10, seg12:0x6b1e pool 11, seg12:0xae4 pool 4 and seg12:0x3e98 pool 8, by the pool pointers DGROUP:0x1e1c, 0x1e00, 0x1df8, 0x1dfc, 0x1e20, 0x1e24, 0x1e08 and 0x1e18; seg12:0x3e98 and seg8:0x8256 also read pool 14. [BYTES seg12:0x532c seg8:0x8256 seg7:0x309c seg7:0x32d0 seg12:0x3148 seg12:0x6b1e seg12:0xae4 seg12:0x3e98]

### Selection and the weapon display
- **Tab** (seg11:0xadf0): with slot count byte +0xcb < 2 it selects slot 0; otherwise slot + 1, + 2 when that slot's weapon byte is 0, wrapping to 1 in Action mode and 0 in Strategy mode. [BYTES seg11:0xadf0 seg11:0x6d6c]
- **Wave start** (seg11:0xae94): unless DGROUP:0x92e4 ≠ 0, slot 1 when byte +0xcb ≥ 2, else 0. [BYTES seg11:0xae94]
- **seg11:0x2f8c** deletes DGROUP:0x5040, rebuilds it with seg11:0x2ff6(w) when the slot ≠ 0, and posts WM_PAINT to DGROUP:0xa7e when that window exists. [BYTES seg11:0x2f8c]
- **Picture seg11:0x2ff6(w).** When the kind W(w)+0x0a is 0 or W(w)−2 = −1, and w ≠ 24, it returns seg10:0x6c02(W(w)+0x02) directly. [BYTES seg11:0x2ff6]
  - Without an overlay graphic the bitmap is square, of side 2·W(w)−8, or 4·W(w)−8/3 for names 0xc4, 0xc6 and 0xc3. [BYTES seg11:0x30d0 seg11:0x2ff6]
  - It StretchBlts in mode 3 from DC DGROUP:0x39e8[W(w)−4] at source x 4·cell + 5·gap and y gap; kind 5 and names 0xc6/0xc3 use x = gap and W(w)+0x06 = 4 uses (W(w)+0x26 − 1)·(cell + gap) + gap. Weapon 8 at `Size` 1 takes 24 × 24 from (116, 4) into a 46 × 46 bitmap. [BYTES seg11:0x2ff6 seg11:0x32e5]
- **Image slot** seg10:0x533a(w) gives weapon w a slot 0x30 … 0x38 in W(w)−4, reusing a loaded slot of a launched weapon with the same W(w)−2, else the first empty one; it skips weapons 22 and 27. [BYTES seg10:0x5375 seg10:0x5389 seg10:0x542d]

## Fuel

### The words
- `fuel` is ship word +0x1e; its cap, the tank, is design word DGROUP:0x96f0. [BYTES seg11:0x1d03 seg14:0x1915]
- **Wave start without a roster record:** `fuel` = 40 × SHP byte +0 (8-bit imul), `resources` = 100 and `credit` = 0. [BYTES seg14:0x1b25 seg14:0x1b2f seg14:0x1b48 seg14:0x1b51]
- **With a roster record:** credit = roster word +0x02 × 10 and resources = roster word +0x04. The `fuel` is capped at the tank, and only a capped value is written back to roster +0x1c as `fuel`/40. A computer ship starts with at least 2/3 of its tank. [BYTES seg14:0x1997 seg14:0x199e seg14:0x19a6 seg14:0x19aa seg14:0x1915 seg14:0x1919 seg14:0x191f seg14:0x1931 seg14:0x18e8 seg14:0x1903]
- A new game's player roster record 0 gets +0x02 = 15 and +0x04 = 0: the player starts with 1.50 icons of credit and no resources. [BYTES seg14:0x3cc seg14:0x3d8] [ORACLE wave-0001]
- seg9:0x6db0 fills every ship's `fuel` to its tank; it is called from seg9:0x6909 (in the placement seg9:0x6624) and seg9:0x6cf6 (in seg9:0x6cc4). [BYTES seg9:0x6dc5 seg9:0x6dd4 seg9:0x6909 seg9:0x6cf6]

### Consumption (seg11:0x1bcc, every 1000 ms)
- While `thrusting`: `fuel` = max(0, `fuel` + `thrust`/−40 − 5·`cloaked` (+0xe8)); otherwise `fuel` = max(0, `fuel` − 5·`cloaked` (+0xe8)). [BYTES seg11:0x1d3a seg11:0x1d43 seg11:0x1d55 seg11:0x1d5d seg11:0x1c5c seg11:0x1c6f seg11:0x1c71]
- With `cloaked` (+0xe8) ≠ 0 it subtracts 3 more without a floor, and below 120 calls seg11:0x4c72(i), the cloak toggle, and seg3:0x332e(0x04090418). A computer ship's 150-frame toggle wait can block that, so its fuel can go negative. [BYTES seg11:0x1c83 seg11:0x1c88 seg11:0x1c92 seg11:0x1c9e seg11:0x4c72]
- The TRANSFORMER (`thrust` 205) burns 5 per second thrusting and 13 cloaked and thrusting. [BYTES seg11:0x1d55 seg11:0x1c83] [ORACLE probe-0001]
- The `mass` update of the same tick is in [world.md](world.md#mass-thrust-and-drag). [BYTES seg11:0x1cfe seg11:0x1d26]
- No instruction of the ship damage handler seg8:0x45e0 touches `fuel`; the shield uses no fuel. [BYTES seg8:0x45e0] [MANUAL Special Ship Systems]
- The low-fuel message (code 9) is raised by the fuel gauge ([world.md](world.md#fuel-gauge)); it plays voice 0x2c and shows string 121 "FUEL LEVEL LOW. SEEK FUEL ASAP". [BYTES seg3:0xcee seg3:0xcf0 seg3:0xcf8] [BYTES ENG_LANG.DLL STRING 8] id 121

## The ambulance

### Calling it (H)
- WM_KEYDOWN 'H' without Ctrl or Alt calls seg16:0xb4e(player) only in a player game. Ctrl+H calls seg13:0x81c2(player) and Alt+H seg13:0x9ae8(player); Ctrl+Alt+H runs both. [BYTES seg11:0x6e8e seg11:0x6cdc seg11:0x6cfb seg11:0x6f7a seg11:0x6f84 seg11:0x6f94 seg11:0x6f9c seg11:0x6fa2 seg11:0x6fb0]
- **Reply.** seg3:0x6ec(player, 7, 0, code) with code 7 when seg16:0xb4e returns non-zero; when it returns 0, code 132 when lo16(credit) + resources < −300, else code 8. [BYTES seg11:0x6fb6 seg11:0x6fc4 seg11:0x6fcc seg11:0x6fd1 seg11:0x6fd6 seg11:0x6fe0]
  - Code 7: voice 0x33 and string 110 "Hold Your Horses, %s. Rescue Unit Dispatched." with the pilot name DGROUP:0x380c. [BYTES seg3:0xcbb seg3:0xcc4 seg3:0xcc9] [BYTES ENG_LANG.DLL STRING 7] id 110 [ORACLE wave-0001]
  - Code 8: string 348 "Sorry! Ambulance Service Is Busy At This Time."; code 132: string 1136 "Sorry! You Have No Credit. Help Is Not Coming."; both with voice 0x2c. [BYTES seg3:0xcde seg3:0xce8 seg3:0xcce seg3:0xcd8] [BYTES ENG_LANG.DLL STRING 22] id 348 [BYTES ENG_LANG.DLL STRING 72] id 1136
  - seg16:0xb4e refuses below −350 but the reply says "no credit" below −300, so a busy refusal with credit + resources below −300 reads "no credit". [BYTES seg16:0xb92 seg11:0x6fcc]
- **seg16:0xb4e(ship)** returns 0 while the clock DGROUP:0x934e < 3, DGROUP:0x913a ≠ 0, DGROUP:0x913c ≠ 0, (DGROUP:0x92e8 ≠ 0 and DGROUP:0x94b2 = 0), or the wave kind is 6 or 7. [BYTES seg16:0xb5c seg16:0xb68 seg16:0xb6f seg16:0xb76 seg16:0xb84 seg16:0xb8b seg16:0xb90]
  - With r = resources + credit (32-bit) it refuses when r < −350 and resources < 200, and for a ship other than the player also when r < 200. [BYTES seg16:0xb92 seg16:0xb9e seg16:0xba2 seg16:0xbac seg16:0xbb3 seg16:0xbb8 seg16:0xbbe seg16:0xbc6 seg16:0xbcc seg16:0xbd1]
  - It refuses when (the ambulance's byte +0x2e ≠ 0 and its `state` = 0) or DGROUP:0x9e49 < 9. [BYTES seg16:0xbd7 seg16:0xbde seg16:0xbe5 seg16:0xbea]
- **Dispatch.** DGROUP:0x9e4b (the distance) = 1400, DGROUP:0x952e = ship, DGROUP:0x9e49 = 0. [BYTES seg16:0xbef seg16:0xbf8 seg16:0xbfb]
- **Placement** of a parked ambulance: the gate p whose centre, pool 12 record 2p, is nearest the ship (seg8:0x285a from 20000; the index stays unset when none is nearer) gives `x` = pool 8 record 2p `x` + 480 and `y` = its `y` − the ambulance's `extent`, with `vx` −20, `vy` −70, `scr_x` = `scr_y` = −1000, `old_scr_x` = `old_scr_y` = 100, `on_screen` 0, then seg7:0x2080(7, −1). [BYTES seg16:0xc44 seg16:0xc48 seg16:0xc79 seg16:0xc81 seg16:0xc98 seg16:0xcd5 seg16:0xcdf]
- The wave set-up sets DGROUP:0x9e49 = 10; the park routine seg16:0x8a0 calls seg12:0x2556(7, 0) and seg7:0x13b2(1, 7, 0) and sets DGROUP:0x9e49 = 10, DGROUP:0x952e = −1, DGROUP:0x111e = 0 and DGROUP:0x6da8 = 0. [BYTES seg9:0x610b seg16:0x8a0]
- The manual: H calls the ambulance; repairs are not free; a ship far enough in debt is refused. [MANUAL Resource Management]

### The mover (seg16:0xa2)
- The frame calls seg16:0xa2 while pool 7 record 0 has `state` 0 ([world.md](world.md#update-loop) step 20). [BYTES seg7:0xc61 seg7:0xc65 seg7:0xc6c]
- **State 0 (approach).** Below distance 120 the velocity is divided by 3 per frame; with both \|v\| < 10 it stops: velocity 0, the ship's `fixed` (+0x54) = 1, the ambulance's `mass` = 100, DGROUP:0x9e49 = 1, DGROUP:0x1c42 = −1. [BYTES seg16:0x271 seg16:0x286 seg16:0x2a3 seg16:0x2b6 seg16:0x2f0 seg16:0x302]
- **State 1 (pickup),** on frames 0 and 4 of DGROUP:0x9e51, once the ship's heading is unchanged or its `alive` (+0xec) = 0, and the lift DGROUP:0x9e47 = 0: DGROUP:0x9e5b = the sign-extended `alive` (+0xec), ship bytes `alive` and +0x147 = 1, the player's DGROUP:0x1e84 and DGROUP:0x1e80 = 0, seg10:0x754a(0x23, 7, 0, 1), a 750 ms deadline, DGROUP:0x9e49 = 2, the ambulance's `fixed` (+0x54) = 1 and DGROUP:0x959e = `Size`. [BYTES seg16:0x43c seg16:0x441 seg16:0x446 seg16:0x44a seg16:0x45a seg16:0x46a seg16:0x476 seg16:0x480]
- **State 2,** after the deadline: sound 0x2f, a 1300 ms deadline, DGROUP:0x9e49 = 3, DGROUP:0x9596 = 1, seg12:0x2556 on the ship and on the ambulance, DGROUP:0x9e53 = the distance. [BYTES seg16:0x4b2 seg16:0x4c5 seg16:0x4d1 seg16:0x4db]
- **States 3 and 4,** with the lift 0 and the deadline passed: DGROUP:0x1e88 = now + 1000 for the player and now + 4000 for another ship, which leads to state 6 ([shell.md](shell.md#screen-states)). [BYTES seg16:0x549 seg16:0x556 seg16:0x562 seg16:0x568]
- **Abort.** In states 1 and 2 with distance > 140: DGROUP:0x9e41 = −1, DGROUP:0x9e43 = 50, DGROUP:0x9e45 = 5, DGROUP:0x111e = 0 and DGROUP:0x9e49 = 0. [BYTES seg16:0x5f9 seg16:0x600 seg16:0x608 seg16:0x60f seg16:0x623]
- **Steering,** in states 0 and 9 only, when the cooldown DGROUP:0x9e4d = 0 and DGROUP:0x9e51 < 2 (2 frames in 8): h = seg8:0x2658(target, 0, 7) and each axis gains (tabx[h]·500 − v·40)/`mass`. The target is the served ship, or pool 12 record 1 in state 9. [BYTES seg16:0x649 seg16:0x67a seg16:0x68e seg16:0x6b0]
  - In state 0 it is pushed off gate posts within 150; in state 9 it parks (seg16:0x8a0) at distance < 200 with `on_screen` 0, or < 110. [BYTES seg16:0xa2]

### The visit (state 6, seg16:0xcfe)
- **Phase 4.** The shape line uses the mean of systems 0 … 2, or 0 … 3 when lamp row 3 ≠ −1: string 311 GOOD (≥ 71), 312 FAIR (≥ 46), 313 BAD (≥ 28) or 314 SORRY; 2021 "YOU'RE DEAD, %s" when DGROUP:0x9e5b = 0; 2020 when DGROUP:0x93a6 ≠ 0; shown by seg16:0x1014. [BYTES seg16:0x1014] [BYTES ENG_LANG.DLL STRING 20] ids 311–314 [BYTES ENG_LANG.DLL STRING 127] ids 2020–2021
  - Strings 311 … 315 are also picked from a record word +0x15e at seg10:0xa2be and seg15:0x560a, not from the system words. [BYTES seg10:0xa2b9 seg10:0xa2be seg15:0x5605 seg15:0x560a] [BYTES ENG_LANG.DLL STRING 20] ids 311–315
- **Phase 8.** Resources < 1 show 2527 "%D HAS NO RESOURCES..."; else 1173 "TRANSFERRING %s ICONS... " with seg6:0x1010(resources, 100, 2): "TRANSFERRING 1.0 ICONS". [BYTES seg16:0x1149 seg16:0x1165] [BYTES ENG_LANG.DLL STRING 158] id 2527 [BYTES ENG_LANG.DLL STRING 74] id 1173 [ORACLE wave-0001]
- **Phase 13.** `credit` (dword) += `resources`, and `resources` is zeroed: "YOUR CREDIT IS NOW 2.50". [BYTES seg16:0x11ec seg16:0x11f2 seg16:0x11f8 seg16:0x11fd] [ORACLE wave-0001]
- **Phase 15.** A dead ship (DGROUP:0x9e5b = 0) gets seg10:0x82a2(6), which dispatches ISPACE.DLL ordinal 10 (the body chooser); every system with a lamp row is set to 100, the new-body flag DGROUP:0x9563 = 1, the cost DGROUP:0x9e55 = 600, and the to-do flags are cleared. [BYTES seg16:0x1365 seg16:0x139a seg16:0x139f]
  - A live ship gets DialogBox(DGROUP:0x9212, 0xc2, DGROUP:0x16, seg16:0x4104), the [options dialog](#the-options-dialog-dialog-0xc2), followed in the demo by seg16:0x3b18(player, 0). [BYTES seg16:0x1339 seg16:0x133f seg16:0x1344] [ORACLE wave-0001]
- **Repairs.** Every 50 ms each system below 100 gains 2, with no clamp, so an odd value ends at 101: phase 24 for systems 0 … 2 (and roster bytes +0x38 … +0x3a = 100), phase 29 for system 3 (and roster +0x3b = 100). [BYTES seg16:0x147e seg16:0x150d seg16:0x1527 seg16:0x1537 seg16:0x153b seg16:0x1543 seg16:0x1630 seg16:0x1653 seg16:0x1672]
- **Refuel.** Every 50 ms `fuel` gains 16 up to the **player's** design tank, not the served ship's, and the gauge shows the player's fuel. [BYTES seg16:0x1776 seg16:0x177a seg16:0x1780 seg16:0x1786 seg16:0x178c seg16:0x1799 seg16:0x17ad]
- **Phase 58.** Credit −= DGROUP:0x9e55 − DGROUP:0x9e5d (32-bit, no floor), then DGROUP:0x9e55 = 0; with both 0 it goes to phase 60. Credit can go into debt. [BYTES seg16:0x1a87 seg16:0x1aa0 seg16:0x1aef]
- **Phase 64.** Distance 1200, the ship's and the ambulance's `state` = 1, DGROUP:0x6da8 = now + 3000; then seg16:0x872 and seg11:0xafe. [BYTES seg16:0xcfe]
  - **Free spot.** The rectangle x − 800 … x + 2240, y ± 1200 around a candidate point is tested; each retry moves x and then y by (`rand()` % 150 + 100)·8 (seg16:0x1d0a, seg16:0x1d1e), a coordinate past the field edge wrapping to 0. [BYTES seg16:0x1d0a seg16:0x1d1e]
- **seg16:0x872** calls seg16:0x3784(DGROUP:0x952e, 0), which applies the purchases, and seg3:0x6ec(DGROUP:0x952e, 7, 0, 0x54): code 84, "Repairs Complete, %s. …" ([Lamp messages](#lamp-messages)), the message the oracle sees back in the wave. [BYTES seg16:0x87e seg16:0x883 seg16:0x889 seg16:0x893] [ORACLE wave-0001]
- **seg16:0x3784(ship, mode 0)** first adds resources to credit; for the player it saves the credit after that and restores it at the end, so the helpers' costs do not reach the player, who pays at phase 58; a computer ship pays each helper's cost. It ends by clearing `thrusting` and object word +0x26, posting WM_PAINT to DGROUP:0xa7e/0xa7c for the player in mode 1, and setting DGROUP:0x9e5b = 1. [BYTES seg16:0x3784]

### The options dialog (dialog 0xc2)
- **Entry text** (seg16:0x5762(list, price, flag, name)): price ≤ 0 adds nothing; the text is " " when the flag is not 0 or −1, another " " for flag 2, "a" for price > 999, seg6:0x1010(price, 100, 1), " - " and the name. [BYTES seg16:0x5762 DGROUP:0x115b DGROUP:0x115c DGROUP:0x115e DGROUP:0x1160 DGROUP:0x1162]
- **Charge.** seg16:0x0 skips up to two ' '/'a' characters and reads "d.d" as d·100 + d·10 and "dd.d" as d·1000 + d·100 + d·10; Add>> charges that value, the shown price to one decimal. [BYTES seg16:0x0 seg16:0x44fa]
- **Grey.** The owner-draw routine seg16:0x590c draws an entry in COLOR_GRAYTEXT when its parsed price > the shopping credit DGROUP:0x9e57 (32-bit) and its text does not start with ' '. [BYTES seg16:0x590c] [ORACLE wave-0001]
- **Entries** (seg16:0x4bd8), for the player: [BYTES seg16:0x4bd8]

  | Entry | Price | Flag | Listed when | Evidence |
  |---|---|---|---|---|
  | NEW SHIP BODY (865), dead ship | 600 | 2 | the only entry; DGROUP:0x11aa ≠ 0 or DGROUP:0x920c ≠ 3 | [BYTES seg16:0x4bd8] [BYTES ENG_LANG.DLL STRING 55] id 865 |
  | NEW SHIP BODY, live ship | 600 | 0 | a player game, DGROUP:0x11aa ≠ 0 or DGROUP:0x920c ≠ 3 | [BYTES seg16:0x4bd8] |
  | BASIC REPAIRS (2516) | Σ(100 − system k), k = 0 … 2 | 1 (2 in the demo); 0 when DGROUP:0x9e57 < −350 | a player game, or the sum > 100 | [BYTES seg16:0x4cfc] [BYTES ENG_LANG.DLL STRING 158] id 2516 [ORACLE wave-0001] |
  | REFUEL (1176) | (tank − `fuel`)/7 | 1 (2 in the demo) | a player game, or `fuel` < tank/4; a zero price adds nothing | [BYTES seg16:0x4d6b] [BYTES ENG_LANG.DLL STRING 74] id 1176 [ORACLE wave-0001] |
  | WING / WING\SHIELD / WING\CLOAK REPAIR | (100 − system 3), twice with `has_shield` (+0xe9) (SHIELD) or +0xe7 (CLOAK) | 1 | a live ship in a player game with DGROUP:0x9e57 ≥ −349 and a wing lamp row | [BYTES seg16:0x4bd8 DGROUP:0x1152] [BYTES ENG_LANG.DLL STRING 73] ids 1152–1156 |
  | DOUBLE GUNS (1185) | 250 | 0 | ship word +0x0a ≠ 2 | [BYTES seg16:0x4f1f seg16:0x4f38] [BYTES ENG_LANG.DLL STRING 75] id 1185 [ORACLE wave-0001] |
  | UNIFY FORCES (1970) | 250 | 0 | the ship holds weapons 24 and 25 | [BYTES seg16:0x4bd8] [BYTES ENG_LANG.DLL STRING 124] id 1970 |
  | POWERUP + 1973 + k | (b + 4)·100, − 200 for k ≥ 2, b = roster byte +0x32 + k | 0 | b < 5, and k < 5 or a wing lamp row | [BYTES seg16:0x502e] [BYTES ENG_LANG.DLL STRING 124] ids 1973–1978 [ORACLE wave-0001] |
  | weapon w = 1 … 26, string 186 + w | W(w)+0x34, or "RELOAD " + name at (20 − count)·10 for an owned ammo weapon | 0 | DGROUP:0x11aa ≠ 0 or W(w)+0x36 = −1; owned weapons without ammo are skipped | [BYTES seg16:0x5104 seg16:0x5169] [ORACLE wave-0001] |

  - The powerups k = 0 … 5 are GUNS, THRUSTERS, ENGINE ARMOUR, GUN ARMOUR, THRUST ARMOUR and WING ARMOUR. [BYTES seg16:0x4bd8] [BYTES ENG_LANG.DLL STRING 124] ids 1973–1978
  - With DGROUP:0x920c ≠ 3, seg10:0x1fb2 lists weapons 4, 5, 7, 8, 10, 11, 14, 15, 16, 17 and 21 at 650, 700, 800, 750, 450, 450, 900, 750, 500, 600 and 650; weapons 22 … 26 cost 0 and are not added. Without the owned weapon 8 these are exactly the oracle's ten weapon entries and prices. [BYTES seg10:0x1fb2] [ORACLE wave-0001]
  - The wing entry's text is "WING" (1152), "\" (DGROUP:0x1152) with a shield or cloak, the system name and " REPAIR" (1153); the oracle reads "REPAIRS" ([Open questions](#open-questions)). [BYTES seg16:0x4bd8] [ORACLE wave-0001]
- **Add>>** (seg16:0x44fa) refuses an entry whose price exceeds DGROUP:0x9e57 when its text does not start with ' ', and a new weapon when DGROUP:0x958e > 10, with a MessageBox of strings 2023 + 2024. [BYTES seg16:0x44fa seg16:0x46c0 seg16:0x46d1] [BYTES ENG_LANG.DLL STRING 127] ids 2023–2024
  - In the Limited edition (DGROUP:0x920c = 0) of a player game it refuses a new weapon or a POWERUP with seg16:0x68d6(hDlg, 2320, 2321). [BYTES seg16:0x44fa] [BYTES ENG_LANG.DLL STRING 146] ids 2320–2321
- **Powerup bought.** seg16:0x4030(ship, k) adds 1 to roster byte +0x32 + k, even with `roster_index` −1; with a record it recomputes `turn` from roster +0x33 and design word DGROUP:0x96f2. It returns (new + 3)·100 for k < 2 and (new + 1)·100 for k ≥ 2, the listed price. [BYTES seg16:0x4030]
  - Five purchases (0 → 5) against the manual's 4 stages hold only if the bytes start at 0, which is [OPEN]. [MANUAL Resource Management] [OPEN]
- **Sell** (seg16:0x541c) prices an owned weapon at W(w)+0x34, or count × W(w)+0x34 / 20 for an ammo weapon, at least 250: RADAR-GUIDED 20/20 gives 7.5 and 19/20 gives 7.1. [BYTES seg16:0x541c] [ORACLE wave-0001]
- The manual: only items you can afford can be bought, except basic repairs and refueling. [MANUAL Resource Management]

### Credit and resources
- `credit` is ship dword +0xdc (DGROUP:0x716c), `resources` word +0xce (DGROUP:0x715e), captured word +0xd2 (DGROUP:0x7162) and destroyed word +0xd0 (DGROUP:0x7160). [BYTES seg16:0x11f8 seg13:0x1b0e seg13:0x1b12 seg7:0x5b39]
- Resources come from captures ([Capturing an icon](#capturing-an-icon)) and move to credit at visit phase 13 and in seg16:0x3784. [BYTES seg13:0x1b0e seg16:0x11f2 seg16:0x3784]
- The briefing's ship page shows the player's credit with seg6:0xf54(·, 100, 2) and resources with seg6:0x1010(·, 100, 2). [BYTES seg17:0x33fd seg17:0x3417 seg17:0x3433]

## Messages (seg3:0x6ec)
- seg3:0x6ec(recipient, pool, rec, code) shows a message only when the recipient is the player; it also returns for sender pool 14 = the player, for message 0xb7 from a ship heading to pool 12, and for senders other than −1, 14, 7 and 2. [BYTES seg3:0x6fa seg3:0x6fd seg3:0x700 seg3:0x704 seg3:0x70d seg3:0x711 seg3:0x72b seg3:0x7d6]
- Sender pool −1 loads string 2213 "NO MESSAGE" with an end time now + 3000. [BYTES seg3:0x77d] [BYTES ENG_LANG.DLL STRING 139] id 2213
- For senders 14, 7 and 2 the name comes from seg3:0x1542(0, pool, rec, DGROUP:0x8740, 0), and the display switches to message mode with seg7:0x119c(3, 0) unless DGROUP:0xa76 = 3. [BYTES seg3:0x75e seg3:0x765 seg3:0x769 seg3:0x7da seg3:0x7e7]
- **End time** DGROUP:0x870c: now + 7000 outside a race (DGROUP:0x94e = 0); in a race now + 2500 for pool 7 and now + 5000 for pools 14 and 2. The cases of the switch may override it (case 0 sets 9000). [BYTES seg3:0x7c6 seg3:0x7f3 seg3:0x7fa seg3:0x800 seg3:0x807 seg3:0x80c seg3:0x813 seg3:0x818 seg3:0x81f seg3:0x825 seg3:0x845]
- seg10:0xa332 in mode 2 splits DGROUP:0x8db0 into the lines DGROUP:0x8768 and DGROUP:0x87b8 with seg3:0x3498, and seg3:0x14d6 upper-cases both when W > 635: "GOOD ICON CAPTURE! / 1.0 RESOURCE GAINED.". [BYTES seg10:0xa332 seg10:0xa831 seg10:0xa843 seg3:0x14d6 seg3:0x14e8 seg3:0x14f6 seg3:0x1524] [ORACLE wave-0001]

## rand() sites
In the order they run within a frame and per event; the generator is in [rng.md](rng.md). [DESIGN]
1. **Rock mover.** The cold-rock `rand()` % 7 ([world.md](world.md#rocks-seg70x3bae)), before its collision test. [BYTES seg7:0x3c54]
2. **Steering a missile.** seg12:0x7c0: two draws on every call, x (seg12:0x8ad) then y (seg12:0x8c6). [BYTES seg12:0x8ad seg12:0x8c6]
3. **View shake.** seg8:0x0 while DGROUP:0x238c ≠ 0: y position % H, x position % W, x jitter % 3, y jitter % 3. [BYTES seg8:0x25b seg8:0x266 seg8:0x289 seg8:0x29a]
4. **A ship's death.** seg16:0x8e4: for entries 1 … 7, x then y % 60, each only with a free effect slot. [BYTES seg16:0x95d seg16:0x96e seg16:0x991 seg16:0x9a4]
5. **An explosion.** seg12:0x274a per fragment with a free slot: % 110; for j > `Detail`/2 + 2, % 10 and, when that is ≤ 6, % 4; for j > 0, x then y. [BYTES seg12:0x28ec seg12:0x292d seg12:0x2938 seg12:0x293d seg12:0x29a0 seg12:0x29f6]
6. **A launch with W(w)−2 = −1.** One draw at seg11:0x80c5. [BYTES seg11:0x80c5]
7. **The wave end.** seg9:0x2828 draws at most one ([world.md](world.md#difficulty-inputs)). [BYTES seg9:0x2b25 seg9:0x2b3d seg9:0x2b4d]
8. **The visit.** Two per free-spot retry at phase 64, x then y. [BYTES seg16:0x1d0a seg16:0x1d1e]
- The player's capture path draws no INSPACE `rand()`; capture sound 0x66 draws from ISPACE.DLL's own. [BYTES ISPACE.DLL seg9:0xd0e seg3:0x668]
- ISPACE.DLL ordinal 29 draws none. [BYTES ISPACE.DLL seg2:0x3c]
- Draws reachable through callees and not yet placed: seg11:0x4e0a → seg11:0x5ab6 → seg11:0x63b4, and seg8:0x6f3c → seg3:0x6ec, seg8:0x6f3c → seg13:0x2714 → seg3:0x6ec. Whether they run on each shot or hit is [OPEN]. [BYTES seg11:0x5ab6 seg11:0x63b4 seg8:0x6f3c seg13:0x2714]

## Open questions
- **Roles A and B.** When neither party of seg8:0x6afa is the player nor a `fixed` object, which value does the uninitialised flag [bp−0xc] hold: the previous iteration's (the slot is the same across the seg8:0x52a0 loop) or the frame's residue at the first? A Wine debugger break at seg8:0x6d5c, or an ADR choosing a rule, settles it. [OPEN]
- **Bounce arithmetic.** The open parts of ordinal 29 (the integer sequence, the slow-contact sign, the contact-time step) are in [ispace.md](ispace.md#ordinal-notes) (29); an oracle bounce off a rock at a known speed would check a port of the listing. [OPEN]
- **Hit dispatcher.** Which amount rules does seg8:0x5df2 apply before `on_damage`: amount < −1 negated, 2000 … 3000 reduced by 2000 with a flag, a pool-5 mass-120 hit doubled to at least 20, the player ramming pools 3, 8 or 9 with the OverThruster raising message 0xd and seg13:0xb6f2(player, 9), and one `rand()` when the target's `owner_pool` ≠ −1 (`rand()` % 12 + 30 < amount gating seg11:0x947a)? A verifier read of seg8:0x5df2 settles it. [OPEN]
- **Shield flag.** Is ship byte +0xea the shield-on flag? It gives damage 4d/3 instead of 2d and uses no fuel, which fits the shield, but every wing loss clears it. A CAPSLOCK oracle run on the TRANSFORMER reading DGROUP:0x717a, or a read of seg11:0x4b00, settles it. [OPEN]
- **Lamp 3 at 50.** What sets a wing ship's word +0x2c to exactly 50, which auto-repair skips? [OPEN]
- **On fire.** Which state is the manual's "on fire (almost dead)" ship that must buy a new body: the 6 s tumble after death or the blinking Engine lamp? seg13:0x4710, seg13:0x498e and the effect kinds of seg8:0x3fbe, with an oracle run that loses the Engine, settle it. [OPEN]
- **Debris.** Which routine moves and draws the wing debris table DGROUP:0x9176, and does it collide? What do ship bytes +0xe0/+0xe1 do after a wing loss? [OPEN]
- **WING DAMAGE.** Which of the pushes of 0x82 at seg13:0x97a2, seg10:0xa365 and seg15:0x6346 is string 130 "WING DAMAGE"? [OPEN]
- **Hazards and shots.** How do the pool-8 and pool-9 movers seg12:0x1776, seg12:0x390e, seg12:0x5636 and seg12:0x532c treat a ship they find, and what does seg8:0x7d70 do in the touch routine? [OPEN]
- **Frozen icon.** Is a negative icon heat +0x2f, which blocks the touch and which seg9:0x2828 stores as 2, the manual's frozen icon? [OPEN]
- **Gate exit of an icon.** On the seg7:0x13b2 path seg12:0x2288 does not set `state`; does seg7:0x5ae8's recount still count that icon as live? [OPEN]
- **Virused player.** What hands a virused player's ship back from the computer pilot, given that the tick sets `PlayMode` 1 at the wave end (seg7:0x6dd)? [OPEN]
- **seg13:0x4eec.** What does ship word +0x82 (0 … 1000) stand for? The roster's +0x06 stores it. [OPEN]
- **Shot ricochet.** Who writes shot slot words +0x88, +0x8a and +0x90 besides the 0 at fire, and who reads +0x6c/+0x6e? [OPEN]
- **Muzzle table.** Which seg14 routine fills the 64-pair-per-design table DGROUP:0x7b60? [OPEN]
- **Steering helpers.** seg13:0xd6c (the turn toward a point) and seg8:0x1cbe (the speed limit), which the missile and the ambulance use, are unread. [OPEN]
- **Pools 1, 4 and 11.** What do the movers seg7:0x32d0, seg12:0xae4 and seg12:0x6b1e move? [OPEN]
- **Noble forces.** Where do weapons 24 … 26 act, given no case in seg11:0xac86? [OPEN]
- **Fuel weapons.** Does `fuel` ≤ 40 block a seg13:0x1318 shot (its local [bp−0xc] stays 0) past seg13:0x137d? [OPEN]
- **seg3:0x332e(0x04090418)** in the cloak's auto-off: what does it do? [OPEN]
- **Powerup start.** Where do roster bytes +0x32 … +0x37 start in a new game (seg14:0x0), and can a powerup be bought five times? An oracle run buying one five times settles it. [OPEN]
- **The rest of the visit.** The phases of seg16:0xcfe other than 4, 8, 13, 15, 24, 29, 33 … 36, 58 and 64 (landing, required-work line, transfer complete, monitors seg16:0x2e3c, double guns, unify, powerups, unloading and loading, totals, launch), the order of seg16:0x3784's helpers and their costs, and the computer ships' shopping seg16:0x3b18 with its `rand()` draws are read only by one analyst. A verifier read settles them. [OPEN]
- **Shop gaps.** Sell OK seg16:0x5db0, Remove seg16:0x4900, the body chooser seg10:0x82a2(−100), seg16:0x2144 and the repair helper seg16:0x3468 are unread; the wing entry's price without a wing lamp row is left from the previous entry. [OPEN]
- **REPAIR or REPAIRS.** The bytes build "WING\SHIELD REPAIR" while the oracle digest reads "REPAIRS"; a crop of the wave-explore shot settles it. [OPEN]
- **Carry-over.** Which routine writes credit/10 and resources into the roster between waves, outside the save path, so that the credit loses its last digit? [OPEN]
- **Ship re-entry.** What do seg16:0x33a2 and seg16:0x3306 do at DGROUP:0x6da8, 3000 ms after the visit? [OPEN]
- **Game completion.** How does Demon's Gate end the game (state 9, DGROUP:0x93a6, the 50000 award at seg16:0x7228)? [OPEN]
