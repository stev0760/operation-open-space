# Data structures

The game keeps its moving objects in 21 pools of 0x70-byte records, and keeps per-ship state in a DGROUP array of eight 0x15a-byte records. This page gives their layouts. Every address is in INSPACE.EXE, and DGROUP is seg19. The globals that index these structures, the main loop and scoring are in [globals.md](globals.md). [DESIGN]

Field names come from behaviour. A field whose meaning is unknown is marked [OPEN]. The writer and reader columns list example sites, not every site. [DESIGN]

## Object pools

### Pool table
- DGROUP:0x1df8 holds 21 far pointers. Pool k (k = 0…20) is at DGROUP:0x1df8 + 4k, so the table ends at DGROUP:0x1e4b. The two creation loops store pointers for k = 0…14 and k = 15…20. [BYTES seg6:0x1bab seg6:0x1b8d seg18:0x1e19 seg18:0x1dfb]
- Code reaches a pool with `les si, [4k + 0x1df8]` and record i at `es:[si + 0x70·i]`. [BYTES seg18:0x203 seg18:0x20f seg11:0x3722 seg11:0x372b]
- Three word arrays run in parallel to the table:

  | Array | Content | Evidence |
  |---|---|---|
  | DGROUP:0x9100 + 2k | global-memory handle of pool k | [BYTES seg6:0x1bc2 seg6:0x1b7c] |
  | DGROUP:0x92ec + 2k | allocated records of pool k | [BYTES seg7:0x52c7 seg7:0x52de seg7:0x52f5] |
  | DGROUP:0x9406 + 2k | records in use; loops over a pool run i = 0 … in-use − 1 | [BYTES seg11:0x3766 seg11:0x376e] |

- seg7:0x1090 clamps the in-use count of each pool 0…DGROUP:0x9324: it resets the count to the allocated count when it is negative or larger. [BYTES seg7:0x1090 seg7:0x109b seg7:0x10aa seg7:0x10b1]
- The generic destroy handler seg12:0x274a first checks the record index. When it is at or above the in-use count, the handler increments DGROUP:0x9498 and returns if the index is also at or above the allocated count, or if the pool is the ship pool DGROUP:0x9324 and the index exceeds DGROUP:0x9316. An index below the in-use count skips both tests. [BYTES seg12:0x2761 seg12:0x2765 seg12:0x2767 seg12:0x276b seg12:0x2771 seg12:0x2777 seg12:0x277f seg12:0x2784]

### Allocation
- seg6:0x1b62 creates pools 0–14 at start-up. Each gets GlobalAlloc(0x42, 0x460), a moveable zero-filled block with room for ten records, which it locks at once. seg6:0x12b6, the start-up routine that registers the window class, calls it. [BYTES seg6:0x1bb1 seg6:0x1bb5 seg6:0x1b82 seg6:0x1461]
- seg18:0x1dd0 creates pools 15–20 with one 0x70-byte record each, and seg18:0x1e46 unlocks and frees them. [BYTES seg18:0x1dde seg18:0x1e23 seg18:0x1e77 seg18:0x1e90]
- seg7:0x52c7 grows pool DGROUP:0x934a. When the new record index is at or above the allocated count, it sets the count to the index + 1 and unlocks the block. It then calls GlobalReAlloc with max(count, 1)·0x70 bytes and flags 0x42, relocks the block and stores the new far pointer in the table. [BYTES seg7:0x52b8 seg7:0x52be seg7:0x52c7 seg7:0x52cf seg7:0x52e2 seg7:0x52f5 seg7:0x5308 seg7:0x5314]
- Other code resizes pools the same way. seg9:0x22b4 reallocates every pool 0…DGROUP:0x9324 to its allocated count, and seg14:0x0 reallocates the ship pool. [BYTES seg9:0x2619 seg9:0x262e seg9:0x263f seg14:0xa61 seg14:0xa80]
- The ship set-up routine seg14:0x0 sets both counts of the ship pool to the number of ships n, and DGROUP:0x9316 to n − 1. [BYTES seg14:0xa23 seg14:0xa2f seg14:0xa33]

### Pool roles
- DGROUP:0x9324 holds the index of the ship pool. The player's ship is record DGROUP:0x9646 of that pool. [BYTES seg18:0x203 seg18:0x208 seg18:0x213]
- Three routines assign the role indices:

  | Routine | DGROUP:0x9324 (ships) | DGROUP:0x9326 | DGROUP:0x9328 | DGROUP:0x934a | Evidence |
  |---|---|---|---|---|---|
  | seg18:0x3d6 | 14 | 0, or 1 when DGROUP:0x92e8 ≠ 0 | 5 | 3 | [BYTES seg18:0x428 seg18:0x435 seg18:0x43b seg18:0x441 seg18:0x447] |
  | seg9:0x22b4 | 14 | 0 (seg9:0x2588 later writes 1 when DGROUP:0x92e8 ≠ 0) | 5 | 3 | [BYTES seg9:0x22c2 seg9:0x22cd seg9:0x22d0 seg9:0x22d6 seg9:0x2581 seg9:0x2588] |
  | seg18:0x8bc | 20 | 15 | 17 | 19 | [BYTES seg18:0x973 seg18:0x979 seg18:0x97f seg18:0x985] |

- The roles of DGROUP:0x9326, DGROUP:0x9328 and DGROUP:0x934a, and what objects pools 0–13 and 15–19 hold, are [OPEN].
- seg8:0x5df2, the hit dispatcher, treats pools 8 and 9 and pool DGROUP:0x934a alike in one test. [BYTES seg8:0x5eb8 seg8:0x5ec0 seg8:0x5ec6]
- **Pools 15–20 belong to the seg18 game mode.** seg18:0x8bc points the roles at pools 15–20, and seg18:0x1eac sets both counts of pool 15 to 1 and both counts of pool 20 to DGROUP:0x9316 + 1. [BYTES seg18:0x1ebe seg18:0x1ec1 seg18:0x1eda seg18:0x1ede seg18:0x1ee1] seg18 also sets DGROUP:0x9130 to 1, and while it is non-zero the input router sends input to seg18:0xd4e and the gameplay frame takes a separate path. [BYTES seg18:0x397 seg18:0x8e1 seg11:0x6abb seg11:0x6acc seg7:0x58f] What this mode is in the game is [OPEN].
- DGROUP:0x1e34 is the far pointer to pool 15. Every instruction that references it lies in seg18. [BYTES seg18:0xe81 seg18:0x10b7 seg18:0x18fc]
- seg4 reads pool 14 through DGROUP:0x1e30 directly instead of through DGROUP:0x9324, for example in the race placing routine. [BYTES seg4:0x70a8 seg4:0x8b95 seg4:0x2de4]

## Object record (0x70 bytes)

| Offset | Size | Name | Meaning | Writers | Readers |
|---|---|---|---|---|---|
| +0x00 | word | `scr_x` | screen x of the square the destroy handler blanks with PatBlt(BLACKNESS) [BYTES seg12:0x27ee seg12:0x2801 seg12:0x2803]. seg7:0x11cc sets the player's ship to half of DGROUP:0x90ea [BYTES seg7:0x11e0 seg7:0x11e8] | seg7:0x11e8 | seg12:0x27ee |
| +0x02 | word | `scr_y` | screen y of that square. seg7:0x11cc sets the player's ship to half of DGROUP:0x90ec [BYTES seg12:0x27f3 seg7:0x11eb seg7:0x11fc] | seg7:0x11fc | seg12:0x27f3 |
| +0x04 | word | `x` | world x. seg8:0x1b54 adds `vx` times a step count. Values above DGROUP:0x5a0 × 8 wrap to 0, and negative values wrap to DGROUP:0x5a0 × 8 [BYTES seg8:0x1be0 seg8:0x1c09 seg8:0x1c41 seg8:0x1c62] | seg8:0x1be0, seg8:0x1c41 | seg11:0x3940, seg12:0x27c4 |
| +0x06 | word | `y` | world y, moved by `vy` and wrapped at DGROUP:0x5a2 × 8 the same way [BYTES seg8:0x1c1a seg8:0x1c80 seg8:0x1ca1] | seg8:0x1c1a | seg11:0x397d, seg12:0x27c0 |
| +0x0c | word | `vx` | velocity x [BYTES seg8:0x1bdc seg8:0x1c02] | — | seg8:0x1bdc |
| +0x0e | word | `vy` | velocity y [BYTES seg8:0x1bea seg8:0x1c13] | — | seg8:0x1bea |
| +0x18 | word | `state` | 0 = in play. The generic destroy handler writes 1. Loops that draw, place or count objects skip records whose `state` is not 0 [BYTES seg12:0x2a97 seg11:0x372f seg4:0x2de8 seg4:0x8c50]. Setup gives pool 7 record 0 the value 1 [BYTES seg10:0x114b] | seg12:0x2a97, seg10:0x114b | seg11:0x372f, seg18:0x213 |
| +0x1e | word | `damage` | damage taken. The generic damage handler adds the hit amount [BYTES seg7:0x2699] | seg7:0x2699, seg14:0x2119 | seg7:0x267c |
| +0x22 | word | `blank_size` | side in pixels of the square the destroy handler blanks [BYTES seg12:0x27f7 seg12:0x27fb] | seg10:0x1112 | seg12:0x27f7 |
| +0x2c | word | `max_damage` | the generic damage handler destroys the object once `damage` > `max_damage` [BYTES seg7:0x2678 seg7:0x26a7] | seg10:0x10b6, seg10:0x1155 | seg7:0x2678 |
| +0x32 | word | `owner_pool` | pool of the object credited with this object's hits. −1 = none. seg11:0x9a19 gives a new pool-10 record the pool its caller passes [BYTES seg8:0x5f0b seg8:0x1ba0 seg11:0x9a16 seg11:0x9a19] | seg11:0x9a19 | seg8:0x5f0b, seg8:0x5f2e |
| +0x34 | word | `owner_rec` | record of that object; seg11:0x9a24 stores the caller's record. The hit dispatcher credits a hit to ship `owner_rec` of the attacker when the attacker's `owner_pool` is the ship pool, and to ship `owner_rec` of the target when the target's `owner_pool` is the ship pool. When both hold, the target's wins [BYTES seg11:0x9a24 seg8:0x5f0b seg8:0x5f14 seg8:0x5f2e seg8:0x5f37 seg8:0x5f3b] | seg11:0x9a24 | seg8:0x5f14, seg8:0x5f37 |
| +0x42 | word | `hit_sound` | sound id. When the hit object is outside the ship pool DGROUP:0x9324, the hit dispatcher passes it as the first argument of the sound wrapper seg10:0x754a ([ispace.md](ispace.md)) [BYTES seg8:0x643b seg8:0x6441 seg8:0x6459 seg8:0x645d]. For a ship, seg14:0x1b84 stores the design's hit sound id ([formats.md](formats.md#mastershp-ships)) [BYTES seg14:0x20c0 seg14:0x20d3] | seg10:0x109c, seg10:0x1141, seg14:0x20d3 | seg8:0x6459 |
| +0x44 | word | — | [OPEN]. The hit dispatcher reads it for the target and for the attacker. Kill scoring tests it for pool 8 [BYTES seg8:0x5e18 seg8:0x5e3a seg8:0x7ca2] | seg10:0x1092, seg10:0x1137 | seg8:0x5e18 |
| +0x46 | word | `radar_brush` | index into the brush table at DGROUP:0x5004 that the radar callbacks select [BYTES seg11:0x3884] | seg10:0x115f | seg11:0x3884 |
| +0x48 | word | `blast_size` | fragment count of the destruction effect; 1 means a single fragment. The destroy handler scales other values by the detail level DGROUP:0x9208 [BYTES seg12:0x2816 seg12:0x2824 seg12:0x2844] | seg10:0x10c0, seg10:0x1169 | seg12:0x2816 |
| +0x4c | dword | `radar_color` | colour that seg11:0x3914 plots with SetPixel [BYTES seg11:0x39c3 seg11:0x39c8] | seg10:0x10a6 | seg11:0x39c3 |
| +0x51 | byte | `race_status` | in pool 14, race code writes 2 to a ship's byte and sets every 0 byte to 1, and the race placing routine runs only while the player's byte is 0 [BYTES seg4:0x26b7 seg4:0x273e seg4:0x2744 seg4:0x8b99]. The values' meanings, and the byte's use in other pools, are [OPEN] | seg4:0x26b7, seg4:0x2744 | seg4:0x8b99 |
| +0x54 | byte | — | [OPEN]. Kill scoring adds a second bonus for a pool-8 record whose byte is not 0 [BYTES seg8:0x7c6d] | seg10:0x10f3 | seg8:0x7c6d |
| +0x5e | far ptr | `on_damage` | damage handler, called cdecl as (pool, record, amount). It returns non-zero when the object is destroyed [BYTES seg8:0x6465 seg8:0x647c seg8:0x6480 seg8:0x6483] | seg10:0x10da, seg14:0x2158 | seg8:0x647c |
| +0x62 | far ptr | `radar_draw` | radar callback, Pascal (pool, record, hDC); it pops its arguments with `retf 6`. seg11:0x375f calls it with the hDC in DGROUP:0x93ce [BYTES seg11:0x374a seg11:0x374e seg11:0x375f seg11:0x39d5 seg11:0x3911] | seg10:0x10ca, seg10:0x1183, seg14:0x2141 | seg11:0x375f |
| +0x66 | far ptr | `on_destroy` | destroy handler, called cdecl as (pool, record) [BYTES seg7:0x26ad seg7:0x26b1 seg7:0x26b5] | seg10:0x1173, seg14:0x212a | seg7:0x26b1 |

- **Damage path.** The hit dispatcher seg8:0x5df2 takes (target pool, target record, attacker pool, attacker record, amount). It calls the target's `on_damage`, then calls the kill scorer seg8:0x7be8 when the credited object is the player's ship and the target is not. [BYTES seg8:0x5e0a seg8:0x5e2c seg8:0x647c seg8:0x6489 seg8:0x6492 seg8:0x64ae]
- **Generic damage handler seg7:0x2652.** It ignores pool 15. It returns 0 when the object is already past `max_damage`, except in pool 9. Otherwise it adds the amount to `damage`, and once `damage` > `max_damage` it calls `on_destroy` and returns 1. [BYTES seg7:0x265e seg7:0x2680 seg7:0x2682 seg7:0x2699 seg7:0x26a7 seg7:0x26b1 seg7:0x26b8]
- **Generic destroy handler seg12:0x274a.** It blanks the object's square when seg8:0x7ba8 returns non-zero for the point that seg8:0x29ea derives from `x` and `y`. A pool-8 record whose word DGROUP:0x3d6c + 0x34·record is 1 skips the blank. [BYTES seg12:0x27a7 seg12:0x27b0 seg12:0x27ce seg12:0x27da seg12:0x27e4] It fills effect-table entries (below), sets `state` to 1 and calls seg10:0x754a(0x45, pool, record, 1). [BYTES seg12:0x2803 seg12:0x28d8 seg12:0x2a97 seg12:0x2b0b seg12:0x2b0d] For the player's ship it also sets DGROUP:0x1e80 to DGROUP:0x1e50 + 1500, or DGROUP:0x1e84 instead when DGROUP:0x95bc ≠ 0. [BYTES seg12:0x2aa5 seg12:0x2aad seg12:0x2ac1 seg12:0x2adf]
- **Radar callbacks.** seg11:0x3914 maps `x`/`y` into the rectangle at DGROUP:0x5048, scaled by DGROUP:0xa6c, and plots one pixel in `radar_color`. seg11:0x37a0 maps the same way and copies a cell from the DC in DGROUP:0x93cc with BitBlt, taking the source x from word +0x28 × 5. seg11:0x39d8 fills a box with PatBlt. [BYTES seg11:0x392d seg11:0x39ad seg11:0x39c8 seg11:0x384d seg11:0x38c0 seg11:0x38e9 seg11:0x3aba] The status display has a zoomable radar view that colour-codes objects. [MANUAL General]
- **Callbacks of the ship pool.** seg14 gives ship records `on_damage` seg8:0x45e0, `radar_draw` seg11:0x37a0 and `on_destroy` seg16:0x8e4. [BYTES seg14:0x2158 seg14:0x2141 seg14:0x212a]
- **Other handlers set by seg10:0xb94.** `on_damage` is seg7:0x2652, or seg16:0x85e, which returns 0 so the object takes no damage. `on_destroy` is seg12:0x274a. `radar_draw` is seg11:0x3914 or seg11:0x37a0. [BYTES seg10:0x10da seg10:0x1193 seg16:0x868 seg10:0x1173 seg10:0x10ca seg10:0x1183]
- The other offsets from +0x08 to +0x6e that code touches have no established meaning yet. [OPEN]

## Effect table (45 × 0x14 bytes at DGROUP:0x1e90)

| Offset | Size | Meaning |
|---|---|---|
| +0x00 | word | kind. 0xffff = free; seg18:0x8bc marks all 45 entries free. The destroy handler writes 1. Other code writes and tests other values, whose meanings are [OPEN] [BYTES seg18:0x9b2 seg18:0x9b6 seg18:0x9bf seg12:0x28d8 seg7:0x129a] |
| +0x02 | word | the destroy handler writes 10 [BYTES seg12:0x291b] |
| +0x06 | word | pool of the destroyed object [BYTES seg12:0x2958 seg12:0x295f] |
| +0x08 | word | record of the destroyed object [BYTES seg12:0x2963 seg12:0x2966] |
| +0x0a | word | the destroy handler writes 4 for the first fragment and 1 for the others [BYTES seg12:0x2a6f seg12:0x28ce] |
| +0x10 | dword | a time on the DGROUP:0x1e50 clock [BYTES seg12:0x2913 seg12:0x2917 seg12:0x294e] |

- Fields +0x04, +0x0c and +0x0e are [OPEN].

## Ship records (8 × 0x15a bytes at DGROUP:0x7090)
- **Base and stride.** Every DGROUP operand indexed by a multiple of 0x15a lies in DGROUP:0x7090–0x71e9, which is exactly one record. The lowest is the word at DGROUP:0x7090 (seg4:0x7084), and the highest is the word at DGROUP:0x71e8 (seg7:0xdbc). Code also addresses fields of record 1 directly, from DGROUP:0x71ea = DGROUP:0x7090 + 0x15a. [BYTES seg4:0x7084 seg7:0xdbc seg4:0x22f]
- **Count.** No operand reaches DGROUP:0x7282–0x7b5f; the one operand that reaches DGROUP:0x7280 is the dword store at seg11:0x9fbc to DGROUP:0x727e, field +0x94 of record 1 [BYTES seg11:0x9fbc]. A new table starts at DGROUP:0x7b60 = DGROUP:0x7090 + 8 × 0x15a. Per-ship arrays inside the record have eight entries (+0x72, +0xee), and seg14:0x0 runs a loop over the ship slots from n to 7. So the array holds eight records. [BYTES seg10:0x14be seg4:0x7060 seg4:0x704d seg14:0xa19 seg14:0xa0e]
- **Index.** Record i belongs to record i of the ship pool. Loops index both with the same i and run i = 0…DGROUP:0x9316. [BYTES seg4:0x8c3a seg4:0x8c46 seg4:0x8c50 seg4:0x8c32]
- **Fields:**

  | Offset | Address of record 0 | Size | Name | Meaning |
  |---|---|---|---|---|
  | +0x00 | DGROUP:0x7090 | word | — | [OPEN]. seg4:0x702e decrements it while it is not 0 and below 4. It passes message code 0x88 to the message dispatcher when the value is 4 [BYTES seg4:0x7084 seg4:0x708a seg4:0x7091 seg4:0x70bf seg4:0x70cf] |
  | +0x18 | DGROUP:0x70a8 | word | — | [OPEN]. Index into the byte array at +0xb8. seg11:0xac86 switches on the selected byte [BYTES seg11:0xacc5 seg11:0xacc9 seg11:0xad31] |
  | +0x1e | DGROUP:0x70ae | word | — | [OPEN]. It drops by 5 for each second the probe holds Up [ORACLE probe-0001]. seg11:0x1c83 subtracts 3 [BYTES seg11:0x1c83] |
  | +0x26 | DGROUP:0x70b6 | 4 words | — | [OPEN]. For the player's ship, seg18:0x3d6 copies bytes +0x38…+0x3b of its 0x7c-byte roster record (`roster_index`, +0xd6) into these words, sign-extended [BYTES seg18:0x4a7 seg18:0x4b6 seg18:0x4bc seg18:0x4ca] |
  | +0x32 | DGROUP:0x70c2 | word | — | [OPEN]. Set to 8 when the ship passes a race flag [BYTES seg4:0x216e seg4:0x2191] |
  | +0x34 | DGROUP:0x70c4 | word | — | [OPEN]. Set to the checkpoint + 2 × DGROUP:0x9318 when the ship passes a race flag [BYTES seg4:0x2174 seg4:0x2181 seg4:0x219c] |
  | +0x3e | DGROUP:0x70ce | word | `target_pool` | pool of the ship's target. seg3:0xf4(ship, target pool, target record) stores it [BYTES seg3:0x102 seg3:0x10a]. For the player's ship it also stores both in DGROUP:0x6e54 and DGROUP:0x6e52, and when seg3:0x0 or seg13:0x371c accepts the target it posts WM_PAINT to the targeting display window DGROUP:0xa7c, if that window exists [BYTES seg3:0x115 seg3:0x122 seg3:0x128 seg3:0x42d seg3:0x434 seg3:0x440]. A pool of −1 fails both checks, so the routine increments DGROUP:0x9498 and returns, and the strings 2205/2206 branch at seg3:0x16a never runs [BYTES seg3:0x12 seg13:0x372f seg3:0x149 seg3:0x14d seg3:0x16a] |
  | +0x40 | DGROUP:0x70d0 | word | `target_rec` | record of the ship's target, stored by seg3:0xf4 [BYTES seg3:0x10e seg3:0x111] |
  | +0x4e | DGROUP:0x70de | word | `design_index` | index n of the ship's 0xad-byte design record at DGROUP:0x96d2 + 0xad·n ([formats.md](formats.md#mastershp-ships)). Fields of record 0 span DGROUP:0x96d2–0x977d [BYTES seg3:0x208 seg3:0x20e]. When the player targets a ship (target pool 14) while DGROUP:0x92e8 is 0, seg3:0xf4 compares the targeted ship's word DGROUP:0x970a (field +0x38 of its design record) with the player's. When they are equal it loads string 322 "TEAMMATE" into DGROUP:0x960a, otherwise it passes the target's word and DGROUP:0x960a to seg13:0x218a [BYTES seg3:0x1cb seg3:0x1e7 seg3:0x202 seg3:0x21d seg3:0x227 seg3:0x23f] [BYTES ENG_LANG.DLL STRING 21] id 322. The target display shows a targeted ship's team [MANUAL General], so field +0x38 is the ship's team |
  | +0x72 | DGROUP:0x7102 | 8 words | — | [OPEN]. One word per other ship j, at +0x72 + 2j [BYTES seg4:0x7055 seg4:0x705a seg4:0x7060] |
  | +0x90 | DGROUP:0x7120 | dword | — | [OPEN]. A clock value that stays 0 until the probe first presses Up [ORACLE probe-0001] |
  | +0xb8 | DGROUP:0x7148 | bytes | — | [OPEN]. Indexed by +0x18. The selected byte indexes a table of 0x50-byte entries; entry 0 has fields at DGROUP:0x2f5e and DGROUP:0x2f9a [BYTES seg11:0xacc9 seg11:0xacce seg11:0xad10 seg11:0xacd1] |
  | +0xd6 | DGROUP:0x7166 | word | `roster_index` | index of the ship's 0x7c-byte roster record ([formats.md](formats.md#ship-roster-records-0x7c-bytes)) in the table whose far pointer is DGROUP:0x6e24 [BYTES seg18:0x4ad seg18:0x4b2 seg18:0x4a3] |
  | +0xd8 | DGROUP:0x7168 | word | `kill_value` | rating bonus for hitting or destroying this ship ([globals.md](globals.md#scoring)) [BYTES seg8:0x7c50 seg8:0x7cc2] |
  | +0xda | DGROUP:0x716a | word | `laps` | race lap counter; placing uses its absolute value. At the last flag of a lap it becomes −1 − `laps`. What the sign means is [OPEN] [BYTES seg4:0x8bb7 seg4:0x8bec seg4:0x8c6b seg4:0x21a0 seg4:0x21a7] |
  | +0xec | DGROUP:0x717c | byte | — | [OPEN]. Race placing counts a ship only when this byte is not 0 [BYTES seg4:0x8c3f] |
  | +0xee | DGROUP:0x717e | 8 bytes | — | [OPEN]. One byte per other ship j, at +0xee + j [BYTES seg4:0x7044 seg4:0x704d] |
  | +0x148 | DGROUP:0x71d8 | byte | `checkpoint` | race flag index within the lap. It rises by 1 at each flag and returns to 0 after flag DGROUP:0x9ea2 − 1 [BYTES seg4:0x215c seg4:0x2166 seg4:0x216a seg4:0x218c] |
  | +0x156 | DGROUP:0x71e6 | dword | — | a due time on the DGROUP:0x1e50 clock. For the player's ship, seg7:0x0 acts once it has passed and DGROUP:0x951a = 0, and sets it 1500 ms ahead [BYTES seg7:0xdbc seg7:0xdc6 seg7:0xdce seg7:0xdd7 seg7:0xddd]. What it times is [OPEN] |

- Every other offset in the record that code touches is [OPEN].
