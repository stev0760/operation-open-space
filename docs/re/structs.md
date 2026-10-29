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

- **DGROUP:0x9326 is the first pool the radar draws.** The radar repaint visits pools DGROUP:0x9326 … DGROUP:0x9324. seg9:0x2588 sets it to 1 when DGROUP:0x92e8 ≠ 0 (Demon's Gate), so that wave's radar leaves pool 0 out. [BYTES seg11:0x35c7 seg11:0x35da seg9:0x2581 seg9:0x2588]
- **DGROUP:0x934a is the rock pool, 3.** The rock mover seg7:0x3bae indexes the pool table with it. [BYTES seg7:0x3bd2 seg7:0x3c00]
- **DGROUP:0x9328 is pool 5.** seg7:0x40c4 moves its records in the wave frame, and the integrator seg8:0x1b54 sets DGROUP:0x9006 to 10 while it moves one of them. [BYTES seg7:0x4146 seg7:0x414d seg8:0x1bb2 seg8:0x1bc0] What pool 5 holds is [OPEN].
- **Wave contents.** In a wave the pools hold these objects; seg9:0x22b4 sets their counts ([world.md](world.md#pool-counts)):

  | Pool | Content | Evidence |
  |---|---|---|
  | 0 | the wave's icons, at most 8 | [BYTES seg9:0x24b5 seg9:0x24d3 seg10:0xbed] |
  | 1, 2, 4–7, 9–11, 13 | [OPEN]; pool 2 holds 8 records per unit of DGROUP:0x932e, and pool 4's count DGROUP:0x940e comes from the wave generator | [BYTES seg9:0x24e0 seg9:0x24e3 seg9:0x3e36 seg9:0x3e80] |
  | 3 | rocks: the first DGROUP:0x92f2 − DGROUP:0x92ea records are big, the rest small | [BYTES seg10:0xed3 seg10:0xedb seg10:0xedf seg10:0xee2] |
  | 8 | gate posts, two per gate (records 0 … 2 × DGROUP:0x9318 − 1), then hazards | [BYTES seg9:0x2561 seg9:0x2563 seg9:0x10d4] |
  | 12 | two records per gate; record 2p is the centre of gate p | [BYTES seg9:0x2567 seg9:0x256a seg9:0x10d4] |
  | 14 | ships ([Ship records](#ship-records-8--0x15a-bytes-at-dgroup0x7090)) | [BYTES seg14:0xa2f seg14:0xa33] |

- The record templates of pools 0–11 come from seg10:0xb94 and seg9:0x10d4 ([world.md](world.md#record-templates)); the object record table below names the fields they fill. [BYTES seg10:0xb94 seg9:0x10d4] What pools 15–19 hold is [OPEN].
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
| +0x08 | word | `old_scr_x` | screen x before the last move: the sprite draw seg8:0xb76 saves `scr_x` here before it recomputes it, and erases from it. Placement sets it to −1000 [BYTES seg8:0xb76 seg10:0x8c7 seg10:0x799] | seg10:0x8c7 | seg8:0xb76 |
| +0x0a | word | `old_scr_y` | screen y before the last move, as +0x08 [BYTES seg8:0xb76 seg10:0x8e4] | seg10:0x8e4 | seg8:0xb76 |
| +0x0c | word | `vx` | velocity x, in world units per step of the integrator seg8:0x1b54 [BYTES seg8:0x1bdc seg8:0x1c02] | seg10:0x90c | seg8:0x1bdc |
| +0x0e | word | `vy` | velocity y [BYTES seg8:0x1bea seg8:0x1c13] | seg10:0x924 | seg8:0x1bea |
| +0x10 | word | `src_x` | x of the sprite cell in the source DC that seg8:0xb76 copies. For the player's ship of types 0 and 7 it is heading × (`size` + `gap`) + `gap` [BYTES seg7:0x132e seg7:0x133d seg8:0xb76] | seg7:0x133d | seg8:0xb76 |
| +0x12 | word | `src_y` | y of that cell [BYTES seg7:0x1384 seg8:0xb76]. seg9:0x6624 sets it to `gap` for every allocated record of pools 0–14 at wave start [BYTES seg9:0x6624] | seg7:0x1384, seg9:0x6624 | seg8:0xb76 |
| +0x18 | word | `state` | 0 = in play. The generic destroy handler writes 1. Loops that draw, place or count objects skip records whose `state` is not 0 [BYTES seg12:0x2a97 seg11:0x372f seg4:0x2de8 seg4:0x8c50]. Setup gives pool 7 record 0 the value 1 [BYTES seg10:0x114b] | seg12:0x2a97, seg10:0x114b | seg11:0x372f, seg18:0x213 |
| +0x1a | word | `on_screen` | 0 while the object is off screen. seg8:0xb76 returns without drawing for such a record outside pool 7 unless the draw kind is 4 or 5 or `owner_pool` is set, and the rock mover moves such a rock only on frames where DGROUP:0x9142 is a multiple of 3 [BYTES seg8:0xbc5 seg7:0x3bae]. A ship turning while its word is 0 outside a race adds its turn word three times ([world.md](world.md#turning)) [BYTES seg13:0x631 seg13:0x638]. The visibility pass seg8:0x4e14 sets it to 1 for a record inside an activity rectangle, to −1 for one that has just left them and to 0 otherwise ([world.md](world.md#visibility-pass-seg80x4e14)) [BYTES seg8:0x4ebe seg8:0x4edb seg8:0x4f0a seg8:0x4fe1] | seg8:0x4ebe, seg8:0x4f0a, seg8:0x4fe1 | seg8:0xbc5, seg7:0x3bae, seg13:0x631 |
| +0x1c | word | — | [OPEN]. The collision test seg8:0x50aa returns at once when the caller's word is not 0 [BYTES seg8:0x50d7]. The visibility pass seg8:0x4e14 sets it to 0 for every record it lists [BYTES seg8:0x4e89 seg8:0x4fc0] | seg8:0x4e89, seg8:0x4fc0 | seg8:0x50d7 |
| +0x1e | word | `damage` | damage taken. The generic damage handler adds the hit amount [BYTES seg7:0x2699] | seg7:0x2699, seg14:0x2119 | seg7:0x267c |
| +0x20 | word | `extent` | side of the object's square in world units, used by the placement overlap tests: 256 for icons, 344 and 168 for big and small rocks, 272 for gate posts [BYTES seg10:0xbc5 seg10:0xf02 seg10:0xdf6 seg9:0x60bc] | seg10:0xbc5, seg10:0xf02 | seg9:0x60bc |
| +0x22 | word | `size` | side in pixels of the object's sprite cell, (side << Size) >> 1 with Size = DGROUP:0x9156; the destroy handler blanks a square of this side [BYTES seg10:0xbae seg10:0xbb1 seg10:0xbb3 seg10:0xbbd seg12:0x27f7 seg12:0x27fb] | seg10:0xbbd, seg10:0x1112 | seg12:0x27f7, seg8:0xb76 |
| +0x24 | word | `gap` | gap in pixels between sprite cells in the source, (gap << Size) >> 1 [BYTES seg10:0xbd3 seg10:0xbda seg7:0x12bc] | seg10:0xbda | seg7:0x12bc |
| +0x26 | word | — | per pool: a rock's spin angle 0–1799, whose hundreds give its frame, and rand() % 900 for pool 9 at placement. The turn and thrust code clears the ship's word [BYTES seg10:0x98a seg10:0x994 seg10:0xa26 seg7:0x3bae seg11:0x6437] | seg10:0x994 | seg7:0x3bae |
| +0x28 | word | `frame` | sprite frame: a ship's heading 0–15, an icon's index, a rock's spin angle / 100 [BYTES seg10:0xbe5 seg7:0x132e seg7:0x3bae seg11:0x592a] | seg10:0xbe5 | seg7:0x132e, seg11:0x38c0 |
| +0x2c | word | `max_damage` | the generic damage handler destroys the object once `damage` > `max_damage` [BYTES seg7:0x2678 seg7:0x26a7] | seg10:0x10b6, seg10:0x1155 | seg7:0x2678 |
| +0x2e | byte | — | [OPEN]. When it is not 0, seg8:0xb76 copies from the DC in word +0x3c at (`gap`, `gap`) instead, and 0xff draws nothing for most kinds. seg9:0x6624 clears it at wave start [BYTES seg8:0xb76 seg9:0x6624] | seg9:0x6624, seg10:0x972 | seg8:0xb76 |
| +0x2f | byte | — | a ship's flame length, (thrust × multiplier) / 20 [BYTES seg11:0x684a seg11:0x6865]. Gate posts hold 2 [BYTES seg9:0x10d4]. A rock's signed heat: above 0 the rock draws from the hot image and cools by 1 per 3000 ms, below 0 it rises by 1 per frame ([world.md](world.md#rocks-seg70x3bae)) [BYTES seg7:0x40a0 seg7:0x3c1f seg7:0x3c8a seg7:0x3f85 seg7:0x3fe9] | seg11:0x6865, seg7:0x3c1f, seg7:0x3f85 | seg7:0x40a0 |
| +0x32 | word | `owner_pool` | pool of the object credited with this object's hits. −1 = none. seg11:0x9a19 gives a new pool-10 record the pool its caller passes [BYTES seg8:0x5f0b seg8:0x1ba0 seg11:0x9a16 seg11:0x9a19]. seg9:0x6624 sets it and word +0x36 to −1 for every allocated record of pools 0–14 at wave start. The integrator calls seg8:0x1e32 for a record whose `owner_pool` is not −1, outside pool 10 [BYTES seg9:0x6624 seg8:0x1ba0 seg8:0x1ba7 seg8:0x1baf] | seg11:0x9a19, seg9:0x6624 | seg8:0x5f0b, seg8:0x5f2e, seg8:0x1ba0 |
| +0x3a | word | — | the source DC of the last seg8:0xb76 call: both of its paths store the caller's srcDC here [BYTES seg8:0xcaa seg8:0xcbb seg8:0x10c9 seg8:0x10da] | seg8:0xcbb, seg8:0x10da | — |
| +0x3c | word | — | the alternate source DC that seg8:0xb76 takes when byte +0x2e ≠ 0 and in the special-wave modes ([world.md](world.md#sprites-seg80xb76)) [BYTES seg8:0xcfd seg8:0x1126-0x11a9] | — | seg8:0xcfd |
| +0x34 | word | `owner_rec` | record of that object; seg11:0x9a24 stores the caller's record. The hit dispatcher credits a hit to ship `owner_rec` of the attacker when the attacker's `owner_pool` is the ship pool, and to ship `owner_rec` of the target when the target's `owner_pool` is the ship pool. When both hold, the target's wins [BYTES seg11:0x9a24 seg8:0x5f0b seg8:0x5f14 seg8:0x5f2e seg8:0x5f37 seg8:0x5f3b] | seg11:0x9a24 | seg8:0x5f14, seg8:0x5f37 |
| +0x42 | word | `hit_sound` | sound id. When the hit object is outside the ship pool DGROUP:0x9324, the hit dispatcher passes it as the first argument of the sound wrapper seg10:0x754a ([ispace.md](ispace.md)) [BYTES seg8:0x643b seg8:0x6441 seg8:0x6459 seg8:0x645d]. For a ship, seg14:0x1b84 stores the design's hit sound id ([formats.md](formats.md#mastershp-ships)) [BYTES seg14:0x20c0 seg14:0x20d3] | seg10:0x109c, seg10:0x1141, seg14:0x20d3 | seg8:0x6459 |
| +0x44 | word | `mass` | the m that thrust, brake and drag divide by [BYTES seg11:0x6613 seg11:0x6894]. Every 1000 ms seg11:0x1bcc sets a ship's to its design mass + fuel / 20 [BYTES seg11:0x1cfe seg11:0x1d26]. Templates: 160 for icons, 1400 and 350 for big and small rocks, 3500 for gate posts [BYTES seg10:0xbed seg10:0xf0c seg10:0xe00 seg9:0x11e7]. The radar draws only records whose `mass` is above 0 [BYTES seg11:0x35f6 seg11:0x35fb]. The hit dispatcher reads it for the target and for the attacker, and kill scoring tests it for pool 8 [BYTES seg8:0x5e18 seg8:0x5e3a seg8:0x7ca2] | seg10:0x1092, seg10:0x1137, seg11:0x1d26 | seg8:0x5e18, seg11:0x35f6 |
| +0x46 | word | `radar_brush` | index into the brush table at DGROUP:0x5004 that the radar callbacks select [BYTES seg11:0x3884] | seg10:0x115f | seg11:0x3884 |
| +0x48 | word | `blast_size` | fragment count of the destruction effect; 1 means a single fragment. The destroy handler scales other values by the detail level DGROUP:0x9208 [BYTES seg12:0x2816 seg12:0x2824 seg12:0x2844] | seg10:0x10c0, seg10:0x1169 | seg12:0x2816 |
| +0x4c | dword | `radar_color` | colour that seg11:0x3914 and seg11:0x3bae plot with SetPixel [BYTES seg11:0x39c3 seg11:0x39c8 seg11:0x3cad seg11:0x3cb2] | seg10:0x10a6, seg10:0xea2 | seg11:0x39c3 |
| +0x51 | byte | `race_status` | in pool 14, race code writes 2 to a ship's byte and sets every 0 byte to 1, and the race placing routine runs only while the player's byte is 0 [BYTES seg4:0x26b7 seg4:0x273e seg4:0x2744 seg4:0x8b99]. In pool 0 the icon dissolve seg7:0x3a42 counts the byte down to 0 [BYTES seg7:0x3a42]. The values' meanings in pool 14, and the byte's use in other pools, are [OPEN] | seg4:0x26b7, seg4:0x2744 | seg4:0x8b99, seg7:0x3a42 |
| +0x53 | byte | — | [OPEN]. seg10:0xb94 sets it to 1 for icons and to 0 for rocks [BYTES seg10:0xc2b seg10:0xec2] | seg10:0xc2b, seg10:0xec2 | — |
| +0x54 | byte | — | [OPEN]. Kill scoring adds a second bonus for a pool-8 record whose byte is not 0 [BYTES seg8:0x7c6d]. Gate posts hold 1 [BYTES seg9:0x10d4]. The integrator leaves a pool-14 record whose byte is not 0 in place while DGROUP:0x92e8 ≠ 0 and DGROUP:0x9e49 = 10 [BYTES seg8:0x1b72 seg8:0x1b7e seg8:0x1b84 seg8:0x1b8b] | seg10:0x10f3 | seg8:0x7c6d, seg8:0x1b7e |
| +0x5e | far ptr | `on_damage` | damage handler, called cdecl as (pool, record, amount). It returns non-zero when the object is destroyed [BYTES seg8:0x6465 seg8:0x647c seg8:0x6480 seg8:0x6483] | seg10:0x10da, seg14:0x2158 | seg8:0x647c |
| +0x62 | far ptr | `radar_draw` | radar callback, Pascal (pool, record, hDC); it pops its arguments with `retf 6`. seg11:0x375f calls it with the hDC in DGROUP:0x93ce [BYTES seg11:0x374a seg11:0x374e seg11:0x375f seg11:0x39d5 seg11:0x3911] | seg10:0x10ca, seg10:0x1183, seg14:0x2141 | seg11:0x375f |
| +0x66 | far ptr | `on_destroy` | destroy handler, called cdecl as (pool, record) [BYTES seg7:0x26ad seg7:0x26b1 seg7:0x26b5] | seg10:0x1173, seg14:0x212a | seg7:0x26b1 |
| +0x6a | dword | — | a rock's heat timer on the DGROUP:0x1e50 clock. seg10:0x864 clears it when it places a rock. The rock mover lowers the heat +0x2f once it has passed and sets it 3000 ms ahead, and a heating hit sets it 5000 ms ahead [BYTES seg10:0x97d seg10:0x981 seg7:0x3c17 seg7:0x3c2a seg7:0x3c3b seg7:0x3fa1] | seg10:0x981, seg7:0x3c3b, seg7:0x3fa1 | seg7:0x3c17 |

- **Damage path.** The hit dispatcher seg8:0x5df2 takes (target pool, target record, attacker pool, attacker record, amount). It calls the target's `on_damage`, then calls the kill scorer seg8:0x7be8 when the credited object is the player's ship and the target is not. [BYTES seg8:0x5e0a seg8:0x5e2c seg8:0x647c seg8:0x6489 seg8:0x6492 seg8:0x64ae]
- **Generic damage handler seg7:0x2652.** It ignores pool 15. It returns 0 when the object is already past `max_damage`, except in pool 9. Otherwise it adds the amount to `damage`, and once `damage` > `max_damage` it calls `on_destroy` and returns 1. [BYTES seg7:0x265e seg7:0x2680 seg7:0x2682 seg7:0x2699 seg7:0x26a7 seg7:0x26b1 seg7:0x26b8]
- **Generic destroy handler seg12:0x274a.** It blanks the object's square when seg8:0x7ba8 returns non-zero for the point that seg8:0x29ea derives from `x` and `y`. A pool-8 record whose word DGROUP:0x3d6c + 0x34·record is 1 skips the blank. [BYTES seg12:0x27a7 seg12:0x27b0 seg12:0x27ce seg12:0x27da seg12:0x27e4] It fills effect-table entries (below), sets `state` to 1 and calls seg10:0x754a(0x45, pool, record, 1). [BYTES seg12:0x2803 seg12:0x28d8 seg12:0x2a97 seg12:0x2b0b seg12:0x2b0d] For the player's ship it also sets DGROUP:0x1e80 to DGROUP:0x1e50 + 1500, or DGROUP:0x1e84 instead when DGROUP:0x95bc ≠ 0. [BYTES seg12:0x2aa5 seg12:0x2aad seg12:0x2ac1 seg12:0x2adf]
- **Radar callbacks.** seg11:0x3914 maps `x`/`y` into the rectangle at DGROUP:0x5048, scaled by DGROUP:0xa6c, and plots one pixel in `radar_color`. seg11:0x37a0 maps the same way and copies a cell from the DC in DGROUP:0x93cc with BitBlt, taking the source x from `frame` × 5. seg11:0x39d8 fills a box with PatBlt in brush `radar_brush`. seg11:0x3bae fills a square in that brush when the scale is above 200 and plots one pixel otherwise. [BYTES seg11:0x392d seg11:0x39ad seg11:0x39c8 seg11:0x384d seg11:0x38c0 seg11:0x38e9 seg11:0x3aba seg11:0x3c50 seg11:0x3c8f seg11:0x3cb2] The status display has a zoomable radar view that colour-codes objects. [MANUAL General]
- **Callbacks of the ship pool.** seg14 gives ship records `on_damage` seg8:0x45e0, `radar_draw` seg11:0x37a0 and `on_destroy` seg16:0x8e4. [BYTES seg14:0x2158 seg14:0x2141 seg14:0x212a]
- **Other handlers set by seg10:0xb94.** `on_damage` is seg7:0x2652, or seg16:0x85e, which returns 0 so the object takes no damage. Icons take seg7:0x233c and rocks seg7:0x5090. `on_destroy` is seg12:0x274a. `radar_draw` is seg11:0x3914, seg11:0x39d8, seg11:0x3bae or seg11:0x37a0. [BYTES seg10:0x10da seg10:0x1193 seg16:0x868 seg10:0xc1b seg10:0xeb2 seg10:0x1173 seg10:0x10ca seg10:0xc0b seg10:0xf16 seg10:0x1183]
- **Radar fields per pool.** The `radar_draw`, `radar_brush` and `radar_color` values each pool gets are in [world.md](world.md#radar-colours). [BYTES seg10:0xb94 seg9:0x10d4]
- Every other offset of the record that code touches is [OPEN].

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
  | +0x02 | DGROUP:0x7092 | word | `thrusting` | 1 from the start of thrust until the brake or drift turns the flame off [BYTES seg11:0x63e2 seg11:0x6894 seg11:0x600d] |
  | +0x0c | DGROUP:0x709c | word | `thrust` | thrust force T: the design's signed SHP byte +1 (thruster) × 5 [BYTES seg14:0x1c8c seg14:0x21a9] |
  | +0x0e | DGROUP:0x709e | word | `drag` | drag D: body byte +0x1c of the design [BYTES seg14:0x1ca1 seg14:0x21b1] |
  | +0x10 | DGROUP:0x70a0 | word | — | [OPEN]. Sprite layout type: for 0 and 7 the player's sprite cell is chosen by heading [BYTES seg7:0x12ee seg7:0x1315 seg7:0x131b] |
  | +0x12 | DGROUP:0x70a2 | word | `spin` | −1, 0 or 1: Alt with a turn key sets it, and seg4:0x0 adds it to the heading every frame [BYTES seg11:0x599b seg11:0x5a39 seg4:0x0] |
  | +0x14 | DGROUP:0x70a4 | word | `turn` | turn delay: larger turns slower [BYTES seg14:0x22a2 seg14:0x22c2 seg13:0x616] |
  | +0x16 | DGROUP:0x70a6 | word | — | turn counter: turn input is read on a frame only while it is below 10 [BYTES seg13:0x616] |
  | +0x18 | DGROUP:0x70a8 | word | `weapon_slot` | selected weapon slot, the index into the bytes at +0xae and +0xb8. seg11:0xac86 switches on the selected weapon [BYTES seg11:0xacc5 seg11:0xacc9 seg11:0xad31 seg11:0x2a66] |
  | +0x1c | DGROUP:0x70ac | word | — | thrust multiplier k: OverThruster sets 5, and every call of seg11:0x5ab6 lowers it by 1 down to 1 [BYTES seg11:0x5c21 seg11:0x5c33 seg14:0x21bb] |
  | +0x1e | DGROUP:0x70ae | word | `fuel` | fuel. OverThruster subtracts 25, and seg11:0x1bcc burns it every 1000 ms [BYTES seg11:0x65d7 seg11:0x1c83 seg11:0x1cfe]. It drops by 5 for each second the probe holds Up [ORACLE probe-0001] |
  | +0x26 | DGROUP:0x70b6 | 4 words | — | [OPEN]. For the player's ship, seg18:0x3d6 copies bytes +0x38…+0x3b of its 0x7c-byte roster record (`roster_index`, +0xd6) into these words, sign-extended [BYTES seg18:0x4a7 seg18:0x4b6 seg18:0x4bc seg18:0x4ca]. Thrust is cut to a third while word +0x2a is 0 and scaled down while it is below 35 [BYTES seg11:0x662b seg11:0x66a9] |
  | +0x32 | DGROUP:0x70c2 | word | — | [OPEN]. Set to 8 when the ship passes a race flag [BYTES seg4:0x216e seg4:0x2191] |
  | +0x34 | DGROUP:0x70c4 | word | — | [OPEN]. Set to the checkpoint + 2 × DGROUP:0x9318 when the ship passes a race flag [BYTES seg4:0x2174 seg4:0x2181 seg4:0x219c] |
  | +0x3a | DGROUP:0x70ca | word | — | [OPEN]. Thrust adds 2 to the multiplier when it is 5 and the multiplier is above 1 [BYTES seg11:0x6613] |
  | +0x3e | DGROUP:0x70ce | word | `target_pool` | pool of the ship's target. seg3:0xf4(ship, target pool, target record) stores it [BYTES seg3:0x102 seg3:0x10a]. For the player's ship it also stores both in DGROUP:0x6e54 and DGROUP:0x6e52, and when seg3:0x0 or seg13:0x371c accepts the target it posts WM_PAINT to the targeting display window DGROUP:0xa7c, if that window exists [BYTES seg3:0x115 seg3:0x122 seg3:0x128 seg3:0x42d seg3:0x434 seg3:0x440]. A pool of −1 fails both checks, so the routine increments DGROUP:0x9498 and returns, and the strings 2205/2206 branch at seg3:0x16a never runs [BYTES seg3:0x12 seg13:0x372f seg3:0x149 seg3:0x14d seg3:0x16a] |
  | +0x40 | DGROUP:0x70d0 | word | `target_rec` | record of the ship's target, stored by seg3:0xf4 [BYTES seg3:0x10e seg3:0x111] |
  | +0x4e | DGROUP:0x70de | word | `design_index` | index n of the ship's 0xad-byte design record at DGROUP:0x96d2 + 0xad·n ([formats.md](formats.md#mastershp-ships)). Fields of record 0 span DGROUP:0x96d2–0x977d [BYTES seg3:0x208 seg3:0x20e]. When the player targets a ship (target pool 14) while DGROUP:0x92e8 is 0, seg3:0xf4 compares the targeted ship's word DGROUP:0x970a (field +0x38 of its design record) with the player's. When they are equal it loads string 322 "TEAMMATE" into DGROUP:0x960a, otherwise it passes the target's word and DGROUP:0x960a to seg13:0x218a [BYTES seg3:0x1cb seg3:0x1e7 seg3:0x202 seg3:0x21d seg3:0x227 seg3:0x23f] [BYTES ENG_LANG.DLL STRING 21] id 322. The target display shows a targeted ship's team [MANUAL General], so field +0x38 is the ship's team |
  | +0x50 | DGROUP:0x70e0 | word | — | [OPEN]. Sprite slot of a body-thrust ship (+0xe4 = 0): seg7:0x1208 draws the player's ship from DC word DGROUP:0x39e8 + 2 × (slot + 0x28) [BYTES seg7:0x12fa seg7:0x130b seg7:0x1397] |
  | +0x5a | DGROUP:0x70ea | byte | — | step of the thrust animation of a body-thrust ship, 1 or −1 when it bounces [BYTES seg13:0x2c0 seg13:0x2fa seg13:0x311] |
  | +0x5b | DGROUP:0x70eb | byte | — | last frame of that animation [BYTES seg13:0x2d3 seg13:0x2f0] |
  | +0x5c | DGROUP:0x70ec | byte | — | frame of that animation; the ship's sprite source y is frame × (`size` + `gap`) + `gap` [BYTES seg13:0x2c4 seg13:0x31b seg13:0x326 seg13:0x328] |
  | +0x60 | DGROUP:0x70f0 | byte | — | OverThruster cooldown: a burst needs it below 4 and sets it to 8, or to 0x1c for a computer-flown ship whose byte was not 0; the wave frame lowers it by 1 every 8 frames [BYTES seg11:0x64b6 seg7:0xafd seg7:0xb1e] |
| +0x61 | DGROUP:0x70f1 | byte | — | [OPEN]. While it is not 0 and word +0x2a > 75, seg11:0x5ab6 sets the speed cap +0xda, unless word +0x32 = 12 [BYTES seg11:0x5c5d seg11:0x5c64 seg11:0x5c75] |
  | +0x72 | DGROUP:0x7102 | 8 words | — | [OPEN]. One word per other ship j, at +0x72 + 2j [BYTES seg4:0x7055 seg4:0x705a seg4:0x7060] |
  | +0x84 | DGROUP:0x7114 | dword | — | control lockout: turn, thrust and fire input wait while it is not below the tick clock DGROUP:0x1e50 [BYTES seg11:0x5938 seg11:0x596c seg11:0x5c03 seg11:0x4db0] |
  | +0x88 | DGROUP:0x7118 | dword | — | entry time on the DGROUP:0x1e50 clock. Set at wave start: GetCurrentTime + 500000 × ship index for a ship sent off-world, GetCurrentTime for a resident ship [BYTES seg9:0x60bc]. The ship entry seg7:0x1d0c enters a waiting ship once it has passed, and pushes the next waiting ship's to now + 5000 ([world.md](world.md#ship-entry-seg70x1d0c)) [BYTES seg7:0x1efe seg7:0x1d70] |
  | +0x90 | DGROUP:0x7120 | dword | — | end of the 250 ms flame window: thrust start and brake act on the flame only once it has passed, and set it 250 ms ahead [BYTES seg11:0x63e2 seg11:0x6894]. It stays 0 until the probe first presses Up [ORACLE probe-0001] |
  | +0xae | DGROUP:0x713e | bytes | — | count per weapon slot, shown before the weapon name [BYTES seg11:0x2a66] |
  | +0xb8 | DGROUP:0x7148 | bytes | — | weapon per slot, indexed by +0x18. The byte w selects the 0x50-byte weapon record at DGROUP:0x2f54 + 0x50w ([globals.md](globals.md#global-table)), whose word +0 is the name id [BYTES seg11:0xacc9 seg11:0xacce seg11:0xad10 seg11:0xacd1 seg11:0x238e] |
  | +0xd6 | DGROUP:0x7166 | word | `roster_index` | index of the ship's 0x7c-byte roster record ([formats.md](formats.md#ship-roster-records-0x7c-bytes)) in the table whose far pointer is DGROUP:0x6e24 [BYTES seg18:0x4ad seg18:0x4b2 seg18:0x4a3] |
  | +0xd8 | DGROUP:0x7168 | word | `kill_value` | rating bonus for hitting or destroying this ship ([globals.md](globals.md#scoring)) [BYTES seg8:0x7c50 seg8:0x7cc2] |
  | +0xda | DGROUP:0x716a | word | `laps` | race lap counter; placing uses its absolute value. At the last flag of a lap it becomes −1 − `laps`. What the sign means is [OPEN] [BYTES seg4:0x8bb7 seg4:0x8bec seg4:0x8c6b seg4:0x21a0 seg4:0x21a7]. Outside a kind-6 wave (DGROUP:0x9320 ≠ 6) seg11:0x5ab6 sets it to lo16(+0x0c · 19) / +0x0e (signed 16-bit), a speed cap, for the player's ship while `PlayMode` DGROUP:0x9134 = 0 and for any ship whose byte +0x61 ≠ 0 and word +0x2a > 75 while word +0x32 ≠ 12; the autopilot step sets it to min(+0xda / 2, 40) ([world.md](world.md#thrust)) [BYTES seg11:0x5c53 seg11:0x5c5d seg11:0x5ca6 seg11:0x5cab seg11:0x5cb2 seg11:0x5cb8 seg11:0x5cc6 seg11:0x5ccc seg11:0x5cd0 seg11:0x5f32]. TRANSFORMER gives lo16(205 · 19) / 25 = 155, the value in the wave probe dump [ORACLE probe-0001] |
  | +0xe2 | DGROUP:0x7172 | byte | — | [OPEN]. While it is not 0, seg13:0x0 skips the player's action-mode flight calls, and the drift step returns early when DGROUP:0x9130 = 0 [BYTES seg13:0x224 seg11:0x600d] |
  | +0xe4 | DGROUP:0x7174 | byte | — | 0 for a body-thrust ship, whose design body byte +0x20 has bit 0x01 set; seg11:0xb09c draws a flame only when it is not 0 [BYTES seg14:0x1f41 seg11:0xb122] |
  | +0xe6 | DGROUP:0x7176 | byte | — | thrust animation of a body-thrust ship: not 0 wraps the frame to 0, 0 bounces it [BYTES seg13:0x25a seg13:0x33a] |
  | +0xe7, +0xe9 | DGROUP:0x7177, DGROUP:0x7179 | bytes | — | [OPEN]. They pick damage-lamp rows 4 and 5 for the fourth lamp [BYTES seg11:0x3d0e seg11:0x3db2], and +0xe9 also moves the player's sprite row [BYTES seg7:0x1371] |
  | +0xeb | DGROUP:0x717b | byte | — | [OPEN]. Turn input is ignored while it is not 0 [BYTES seg11:0x5938] |
  | +0xec | DGROUP:0x717c | byte | — | [OPEN]. Race placing counts a ship only when this byte is not 0 [BYTES seg4:0x8c3f]. seg7:0x1208 skips the player's sprite while it is 0 and an effect-table entry of kind 1–8 with word +6 = 0x0e names the player [BYTES seg7:0x1266 seg7:0x1283] |
  | +0xee | DGROUP:0x717e | 8 bytes | — | [OPEN]. One byte per other ship j, at +0xee + j [BYTES seg4:0x7044 seg4:0x704d] |
  | +0x147 | DGROUP:0x71d7 | byte | — | [OPEN]. The radar leaves a ship out while it is not 0. seg16:0x446 sets it to 1 for ship DGROUP:0x952e, seg14:0x14d3 and seg13:0x3a93 clear it, and seg8:0x45f4 and seg16:0x8f8 test it [BYTES seg11:0x37af seg11:0x37ba seg16:0x446 seg14:0x14d3 seg13:0x3a93 seg8:0x45f4 seg16:0x8f8] |
  | +0x148 | DGROUP:0x71d8 | byte | `checkpoint` | race flag index within the lap. It rises by 1 at each flag and returns to 0 after flag DGROUP:0x9ea2 − 1 [BYTES seg4:0x215c seg4:0x2166 seg4:0x216a seg4:0x218c] |
  | +0x156 | DGROUP:0x71e6 | dword | — | a due time on the DGROUP:0x1e50 clock. For the player's ship, seg7:0x0 acts once it has passed and DGROUP:0x951a = 0, and sets it 1500 ms ahead [BYTES seg7:0xdbc seg7:0xdc6 seg7:0xdce seg7:0xdd7 seg7:0xddd]. What it times is [OPEN] |

- **Probe values.** In the wave probe dump the player's ship (record 0, design TRANSFORMER) holds words +0x0c = 205, +0x0e = 25, +0x14 = 21, +0x26 … +0x2c = 100, +0x1e = 1320 (40 × the SHP tank byte 33), +0x18 = 1, bytes +0xae = 1 and +0xaf = 20, +0xb9 = 8 (the SHP weapon), +0xcb = 2, +0xe4 = +0xe5 = +0xe9 = +0xec = 1 and +0x148 = +0x149 = 0xff. [ORACLE probe-0001]
- Every other offset in the record that code touches is [OPEN].

## Image slots
- DGROUP:0x3974, DGROUP:0x39e8, DGROUP:0x3a5c and DGROUP:0x3ad0 are parallel word arrays 0x74 bytes apart, so each has 58 slots; seg10:0x522 frees slots 0 … 0x39 through seg10:0x552. DGROUP:0x39e8 + 2k holds slot k's memory DC, DGROUP:0x3974 + 2k the object it restores on release, DGROUP:0x3a5c + 2k its palette and DGROUP:0x3ad0 + 2k its bitmap. [BYTES seg10:0x544 seg10:0x552 seg10:0x562 seg10:0x5c2 seg10:0x5d3 seg10:0x5e1 seg10:0x5e7 seg10:0x5fb seg10:0x3afc]
- Known slots of DGROUP:0x39e8: 6 … 9 the rock images ([world.md](world.md#rock-images-seg100x3a2e)), 13 (DGROUP:0x3a02) the icon strip DC, and 0x28 + i the image of ship record i ([world.md](world.md#ships-seg130x0)). Slot 0x20 is DGROUP:0x3a28; what it holds is [OPEN]. [BYTES seg7:0x3b05 seg13:0xa9b seg7:0x1257]
