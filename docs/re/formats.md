# Data-file formats

This page describes the files the game reads and writes at run time. They are the ship definitions, saved games, the score list, race records, the INI keys and the resource records that code loads as data. The page owns the byte layouts and the meaning of each field. [DESIGN]

Related pages: the static resource inventory is in [resources.md](resources.md) and the in-memory ship and pool records in [structs.md](structs.md). The timing keys are in [timing.md](timing.md) and the ISPACE.DLL exports in [ispace.md](ispace.md). [DESIGN]

Addresses without a module name are INSPACE.EXE, whose DGROUP is seg19. ISPACE.DLL's DGROUP is seg11 and both FACTORY.EXE builds have DGROUP seg6. `FACTORY.EXE` means the game Ship Factory 1.29.02 and `theworks/FACTORY.EXE` the Enhanced Ship Factory 1.49.00. [DESIGN] [BYTES FACTORY.EXE] [BYTES theworks/FACTORY.EXE]

## Conventions
- Numbers are little-endian. A word is 16 bits and a dword 32 bits. A string field is a fixed-size cp1252 buffer holding a NUL-terminated string. [DESIGN]
- **Block writers write `GlobalSize` bytes.** The MASTER.BOD and MASTER.SHP writer and the saved-game writer copy a moveable, zero-filled global block (`GlobalAlloc(0x42, n)`) to the file with a length of `GlobalSize(h)`, not `n`. [BYTES FACTORY.EXE seg4:0x26d4 seg4:0x26dc] [BYTES seg6:0x8aec seg6:0x8afa seg6:0x8b05 seg6:0x8b1e seg6:0x8b2f seg6:0x8b48]
  - Wine's krnl386 rounds every block of up to 1 MB up to a multiple of 32 bytes, and `GlobalSize` returns the rounded size. [REF Wine 11.0 dlls/krnl386.exe16/global.c GLOBAL_Alloc] Windows 3.0 and 3.1 size global blocks in steps of 32 bytes: in 386 enhanced mode a block is an even multiple of 16 (32, 64, 96 …), in standard mode an odd one (16, 48, 80 …). [REF Microsoft KB Q92942, GlobalReAlloc() and GMEM_ZEROINIT Clarified] The granularity of the 16-bit heap of Windows 95 and 98 is [OPEN] ([ADR 0014](../decisions/0014-saved-games.md)).
  - The shipped MASTER.BOD (52 × 44 = 2,288 bytes of records, 2,304 = 72 × 32 bytes in all) and MASTER.SHP (118 × 65 = 7,670 bytes of records, 7,680 = 240 × 32 bytes in all) end with the padding that 32-byte rounding gives. Rounding to 16 bytes would leave MASTER.BOD at 2,288 bytes. [BYTES MASTER.BOD] [BYTES MASTER.SHP]
- **Readers fail on a short read.** The INSPACE read helper seg14:0x4006 reads in chunks of at most 0x7fff bytes. It returns 0 as soon as `_lread` delivers fewer bytes than asked. [BYTES seg14:0x403f seg14:0x4044 seg14:0x4049]

## File locations

| File | Directory | Name | Read by | Written by |
|---|---|---|---|---|
| MASTER.BOD, MASTER.SHP | INI `ShipPath`, with `\` appended when missing | fixed | seg14:0x3db6, seg14:0x3eda; ISPACE.DLL seg10:0x3f70, ISPACE.DLL seg10:0x406a; FACTORY.EXE seg4:0x277c, FACTORY.EXE seg4:0x283e | FACTORY.EXE seg4:0x24fa, FACTORY.EXE seg4:0x2316 |
| `*.sav` | `LocalPath` + `Games` + `\` | INI `Restore`, `lastgame` or `demogame` | seg6:0x930a; ISPACE.DLL seg10:0x1a80 | seg6:0x885c |
| `*.ded`, `*.don` | `LocalPath` + `Dead` + `\`; a renamed file `Path` + `Dead` + `\` | INI `Restore` or the pilot name, plus a counter when taken | seg6:0x6576 (score list), seg6:0x8776 (name check), seg6:0x7042 (purge, `.ded` only) | seg6:0x885c |
| `<course>.rac` | INI `RacePath`, with `\` appended when missing | `LAZYLOOP`, `SDISPEED`, `GRANDPRI` | seg6:0x8b90 | seg6:0x8dfe |
| `INSPACE.INI` | Windows directory (no path in the name) | fixed | everywhere | everywhere |

[BYTES seg14:0x3e2f seg14:0x3f57 seg6:0x93d1 seg6:0x8a78 seg6:0x8bda seg6:0x8e49 seg6:0x6ef] [BYTES ISPACE.DLL seg10:0x3fdc seg10:0x40da seg10:0x1ba6] [BYTES FACTORY.EXE seg4:0x27b6 seg4:0x2878 seg4:0x26bf]

- seg6:0x6d8 builds the directory strings at start-up. It reads `Path` into DGROUP:0x8f7a and `LocalPath` into DGROUP:0x9078, and appends `\` to `Path` when missing. It reads `Dead` into DGROUP:0x96c5 and `Games` into DGROUP:0x96b8 (each at most 10 characters) and appends `\` to both. It reads `RacePath` into DGROUP:0x900a and appends `\` when missing. [BYTES seg6:0x70a seg6:0x725 seg6:0x978 seg6:0x9a0 seg6:0x9c8]
- The saved-game code prefixes `LocalPath` (DGROUP:0x9078). The two exceptions prefix `Path` (DGROUP:0x8f7a): the renaming loop for `.ded`/`.don` files and the purge of `.ded` files ([Writing](#writing)). [BYTES seg6:0x88be seg6:0x89f0 seg6:0x7042]
  - When `Path` and `LocalPath` differ, the purge never sees the `.ded` files in `LocalPath`. A first attempt that finds this game's own `.ded` there overwrites it, because seg6:0x8776 accepts a file with the same game id. [BYTES seg6:0x7042 seg6:0x8776]
- ISPACE.DLL's Restore Game dialog builds `LocalPath` + the fixed string `GAMES\` + name + `.sav`, so it ignores the INI `Games` key. [BYTES ISPACE.DLL seg10:0x1b37 seg10:0x1b55]

## MASTER.BOD: ship bodies

The file is an array of 44-byte (0x2c) records with no header. Every reader takes the record count as file length / 44 by integer division and reads count × 44 bytes, so trailing padding is ignored. [BYTES seg14:0x3e47 seg14:0x3e57 seg14:0x3e5c seg14:0x3e61] [BYTES ISPACE.DLL seg10:0x3f70] [BYTES FACTORY.EXE seg4:0x277c]

- INSPACE keeps the records behind the far pointer DGROUP:0x6e1c, the count in DGROUP:0x95ae and the block handle in DGROUP:0x95ac. [BYTES seg14:0x3e5c seg14:0x3e75 seg14:0x3e7e]
- INSPACE reaches a body through the SHP record's +0x21. "+ 0xad·n" in the table names a field of design record n ([MASTER.SHP](#mastershp-ships)). [BYTES seg14:0x1bc3]
- The shipped file holds 52 records and 16 zero bytes. [BYTES MASTER.BOD] On Enter the game reads 0x8f0 = 52 × 44 bytes from it. [ORACLE newgame-0002]
- The Ship Factory writes the file from a new zero-filled block. It copies the fields below one by one, so bytes outside them, and the bytes after each string's NUL, are written as 0. [BYTES FACTORY.EXE seg4:0x24fa]

| Offset | Size | Type | Meaning | Evidence |
|---|---|---|---|---|
| +0x00 | 21 | string | body name. A record whose name is empty is free; the import dialog reuses it and writes the name from its edit control | [BYTES FACTORY.EXE seg5:0x1f8e seg5:0x20ae] |
| +0x15 | 2 | word | taken into DGROUP:0x977b + 0xad·n when the ship's SHP +0x07 is 12. All shipped records hold 1. Meaning [OPEN] | [BYTES seg14:0x1bce] [BYTES FACTORY.EXE seg4:0x45f8] [BYTES MASTER.BOD] |
| +0x17 | 2 | word | the same for DGROUP:0x977d + 0xad·n and SHP +0x08 | [BYTES seg14:0x1c04] |
| +0x19 | 1 | signed byte | flame anchor radius. The import dialog stores (bitmap measure − 8). seg14:0x1b84 stores its negation in design word +0x04 (DGROUP:0x96d6 + 0xad·n). seg14:0xc42 turns a negative design value v into 4 − v − s, where s is the value it also writes to design words +0x00 and +0x02. seg10:0x16ba(n) then places the five flame anchors, design words +0x50 + 4k (DGROUP:0x9722 + 0xad·n + 4k, x then y, k = 0 … 4), at radius r = 2·(design +0x04 + 1), less 1 for k = 0, 3 and 4 and 1 more for k = 0 | [BYTES FACTORY.EXE seg5:0x1ff4 seg5:0x2001] [BYTES seg14:0x1c7f seg14:0x1c86 seg14:0x1c88 seg14:0xc50 seg14:0xc54 seg14:0xc58-0xc68] [BYTES seg10:0x16ea-0x170a seg10:0x1715-0x17f8 seg10:0x1861-0x18a4] |
| +0x1a | 1 | signed byte | the wing band: seg14:0x1b84 stores trunc(((value << `Size`) sar 1) / 2) in design word +0x3c (DGROUP:0x970e + 0xad·n), the half-width in pixels of the fuselage band that ISPACE.DLL ordinal 15 keeps when a wing tears off ([core.md](core.md#wings-torn-off-seg80x2cd8)). The import dialog stores 10 | [BYTES FACTORY.EXE seg5:0x2009] [BYTES seg14:0x2070-0x208b seg8:0x2d6f-0x2d8d] |
| +0x1b | 1 | signed byte | strength base. seg14:0x1b84 computes base × 10 × (SHP armor / 4 + 100) / 100, stores it in DGROUP:0x9704 + 0xad·n and in word +0x44 of the ship's pool record, and stores that / 22 in DGROUP:0x9702 + 0xad·n and pool word +0x48 | [BYTES seg14:0x1e50 seg14:0x1e68 seg14:0x20bc seg14:0x210a] |
| +0x1c | 1 | signed byte | stored in DGROUP:0x9712 + 0xad·n, which reaches ship-record word +0x0e. Meaning [OPEN] | [BYTES seg14:0x1ca1 seg14:0x1caa seg14:0x21b1] |
| +0x1d | 1 | signed byte | stored in DGROUP:0x96f2 + 0xad·n. seg14:0x1b84 sets ship-record word +0x14 to value × 1200 / (SHP thruster × 5); a ship with a roster record divides by that × (roster byte +0x33 + 10) / 10, and the player's ship then gets ¾ of the result when DGROUP:0x95bc ≠ 0 | [BYTES seg14:0x1d30 seg14:0x1d37 seg14:0x2274 seg14:0x2299 seg14:0x22a7 seg14:0x22cc seg14:0x22e4 seg14:0x22f2] |
| +0x1e | 1 | signed byte | the gun barrels' x offset: seg14:0x11a8 sign-extends it into design word +0x68 = value − trunc(side/2) − `Size` and +0x6c = −(+0x68 + `Size`) ([core.md](core.md#the-muzzle-tables-seg100x1460-seg100x1620-seg100x1502)). The import dialog stores 0; TALON holds 1 and TRANSFORMER 4 | [BYTES FACTORY.EXE seg5:0x2012] [BYTES seg14:0x13a2 seg14:0x13a6] [BYTES MASTER.BOD] |
| +0x1f | 1 | signed byte | the gun barrels' y offset: design words +0x6a = +0x6e = value − trunc(side/2). The import dialog stores (bitmap measure − 10); TALON holds 16 and TRANSFORMER 6 | [BYTES FACTORY.EXE seg5:0x201b seg5:0x2020] [BYTES seg14:0x13c3 seg14:0x13c7] [BYTES MASTER.BOD] |
| +0x20 | 1 | flags | see below | |
| +0x21 | 2 | word | IGRAPHIC.DLL #4000 resource id of the body bitmap, or 0xffff for a bitmap file named by +0x23. Copied to DGROUP:0x9708 + 0xad·n | [BYTES seg14:0x1ea9] [BYTES FACTORY.EXE seg5:0x2028] [BYTES MASTER.BOD] |
| +0x23 | 9 | string | base name of the bitmap file (`.DIB` stripped) for an imported body; copied to DGROUP:0x9756 + 0xad·n | [BYTES FACTORY.EXE seg5:0x2091] [BYTES seg14:0x1c3c seg14:0x1c43] |

- The last shipped record has an empty name, id 0xffff and file name `IDRAGON`, which matches `theworks/IDRAGON.DIB`. [BYTES MASTER.BOD]
- The "bitmap measure" is the word FACTORY.EXE DGROUP:0xe4e, which the import dialog fills from the chosen bitmap. Which dimension it holds is [OPEN]. [BYTES FACTORY.EXE seg5:0x1ff4 seg5:0x201b]
- The import dialog fills +0x1b, +0x1c and +0x1d through FACTORY.EXE seg4:0x362e, a thunk through the far pointer FACTORY.EXE DGROUP:0xba0. FACTORY.EXE seg4:0x323f sets that pointer with `GetProcAddress` ordinal 19 of the ISPACE.DLL that FACTORY.EXE seg4:0x30c4 loads. Ordinal 19 is ISPACE.DLL seg6:0x79b8 ([ispace.md](ispace.md)). [BYTES FACTORY.EXE seg4:0x3057 seg4:0x30c4 seg4:0x323d seg4:0x3244 seg4:0x3649]
  - A first call passes the body's +0x21 and a far pointer to its +0x23, and discards the result. [BYTES FACTORY.EXE seg5:0x21bc seg5:0x21bd seg5:0x21c3]
  - Three calls with a mode argument of 0, 1 and 2 then return +0x1b, +0x1c and +0x1d. Only the +0x1b result is divided by 10. [BYTES FACTORY.EXE seg5:0x21cb seg5:0x21d5 seg5:0x21e0 seg5:0x21e6 seg5:0x21ea seg5:0x21f4 seg5:0x2200 seg5:0x2204 seg5:0x220e seg5:0x221a]
  - What ordinal 19 computes is [OPEN].

**Flags at +0x20.** The Import Ship Body dialog (ENG_LANG #300) clears the byte and sets one bit per checkbox through FACTORY.EXE seg5:0x72e. Body Thrust excludes the other three. [BYTES FACTORY.EXE seg5:0x2114 seg5:0x211c seg5:0x212a seg5:0x2131 seg5:0x213f seg5:0x214e seg5:0x215c seg5:0x216b seg5:0x2179 seg5:0x217f] [BYTES ENG_LANG.DLL DIALOG 300]

| Bit | Checkbox | What seg14:0x1b84 does with it |
|---|---|---|
| 0x01 | 301 Body Thrust | design word +0x46 (DGROUP:0x9718 + 0xad·n) = 1 when the bit is clear, else 0. seg14:0x1b84 copies it to ship byte +0xe4, which lets the flame be drawn ([structs.md](structs.md#ship-records-8--0x15a-bytes-at-dgroup0x7090)) [BYTES seg14:0x1f41-0x1f5a seg14:0x21da seg14:0x21de] |
| 0x02 | 310 Shield | special ability 5 when bits 0x04 and SHP 0x08 are clear; DGROUP:0x9779 + 0xad·n = 1, else 0, and sound 5 is passed to seg10:0x754a. seg14:0x1b84 copies DGROUP:0x9779 to ship byte +0xe9 (DGROUP:0x7179) [BYTES seg14:0x1dbd seg14:0x1dc9 seg14:0x1f9f seg14:0x1fab seg14:0x1fb9 seg14:0x1fc8 seg14:0x21f2 seg14:0x21f6] |
| 0x04 | 302 Pull-off Wings | special ability 3; DGROUP:0x9775 + 0xad·n = 6. When clear, the ship-record bytes +0xac and +0xad are zeroed [BYTES seg14:0x1d80 seg14:0x1d8c seg14:0x1f1f seg14:0x1ded seg14:0x1dfb] |
| 0x08 | 309 Thrust Behind Ship | design word +0x48 (DGROUP:0x971a + 0xad·n) = 1. seg10:0x5d3c then draws the ship over the flame; with the word 0 it draws the flame over the ship [BYTES seg14:0x1d55 seg14:0x1d5c] [BYTES seg10:0x5ecc-0x5f9b seg10:0x5fa2-0x6019] |
| 0x40 | none | DGROUP:0x9774 + 0xad·n = 1 [BYTES seg14:0x1f71] |

- The special-ability word is DGROUP:0x96fa + 0xad·n: 3 for bit 0x04, else 4 for the SHP cloak bit, else 5 for bit 0x02, else 0xffff. [BYTES seg14:0x1d8c seg14:0x1da8 seg14:0x1dc9 seg14:0x1dd6]

## MASTER.SHP: ships

The file is an array of 65-byte (0x41) records with no header, counted like MASTER.BOD (length / 65). INSPACE keeps them behind the far pointer DGROUP:0x6e20 with the count in DGROUP:0x95aa. [BYTES seg14:0x3f6e seg14:0x3f88 seg14:0x3f8d] [BYTES ISPACE.DLL seg10:0x406a] [BYTES FACTORY.EXE seg4:0x283e]

- The shipped file holds 118 records and 10 zero bytes. [BYTES MASTER.SHP] On Enter the game reads 0x1df6 = 118 × 65 bytes. [ORACLE newgame-0002]
- The Factory writer copies the fields below into a zero-filled block. theworks/FACTORY.EXE also upper-cases a–z in the name before writing. [BYTES FACTORY.EXE seg4:0x2316] [BYTES theworks/FACTORY.EXE seg4:0x2446 seg4:0x2190]
- seg14:0x1b84 takes a ship slot, a design index n and an SHP index. It fills the 0xad-byte design record n at DGROUP:0x96d2 + 0xad·n from the SHP record and its body, and sets up the ship slot from it. The tables on this page write "+ 0xad·n" for fields of that record. structs.md calls the design index `design_index`. [BYTES seg14:0x1b93 seg14:0x1b9e seg14:0x1df6]

| Offset | Size | Type | Meaning | Evidence |
|---|---|---|---|---|
| +0x00 | 1 | byte | Fuel Tank (Ship Handling scrollbar 142), at least 10. seg14:0x1b84 stores value × 40 in DGROUP:0x96f0 + 0xad·n | [BYTES FACTORY.EXE seg4:0x4642 seg5:0x3ba7 seg5:0x3beb seg5:0x3c14] [BYTES seg14:0x1d1c seg14:0x1d1f] |
| +0x01 | 1 | byte | Thruster (scrollbar 141). The Factory clamps it to 30–60; the shipped MASTER.SHP holds 23–60. seg14:0x1b84 stores value × 5 in DGROUP:0x9710 + 0xad·n, which reaches ship-record word +0x0c | [BYTES FACTORY.EXE seg4:0x4656 seg5:0x3ae4 seg5:0x3af2 seg5:0x3b51] [BYTES seg14:0x1c90 seg14:0x1c94 seg14:0x21a9] [BYTES MASTER.SHP] |
| +0x02 | 1 | byte | Armor (scrollbar 140), 10–50. Feeds the strength formula, eight words value/4 or value/5 at DGROUP:0x96da–0x96e8 + 0xad·n, and the byte DGROUP:0x9778 + 0xad·n | [BYTES FACTORY.EXE seg4:0x46a1 seg5:0x39ea seg5:0x39f8 seg5:0x3a21] [BYTES seg14:0x1cae seg14:0x1d16] |
| +0x03 | 1 | byte | team, 0-based: index into `[Teams]` Team1…Team8 (Ship Behaviour combo 103) | [BYTES FACTORY.EXE seg4:0x4649 seg5:0x2658 seg5:0x2bd4] |
| +0x04 | 1 | byte | attitude to friends (combo 108): 0 LOYAL, 1 HELPFUL, 2 FRIENDLY, 3 DISINTERESTED | [BYTES FACTORY.EXE seg4:0x4628 seg5:0x281a seg5:0x297b seg5:0x2c6d] [BYTES ENG_LANG.DLL STRING 16] ids 246–249 |
| +0x05 | 1 | byte | attitude to enemies (combo 109): 0 PSYCHOPATHIC, 1 HOSTILE, 2 UNFRIENDLY, 3 DISINTERESTED | [BYTES FACTORY.EXE seg5:0x28c9 seg5:0x28f3 seg5:0x291d seg5:0x2947 seg5:0x2997] [BYTES ENG_LANG.DLL STRING 16] ids 250–252, 249 |
| +0x06 | 1 | byte | initial weapon (combo 107), 0–21: string 186 + n (NO WEAPON … THROWING STARS) | [BYTES FACTORY.EXE seg4:0x46a9 seg5:0x2808 seg5:0x2c16] [BYTES ENG_LANG.DLL STRING 12] ids 186–191 [BYTES ENG_LANG.DLL STRING 13] ids 192–207 |
| +0x07 | 1 | byte | set by a Factory list control (id 170). Stored in DGROUP:0x96ea + 0xad·n; the value 12 makes seg14:0x1b84 take the body's +0x15 into DGROUP:0x977b + 0xad·n. Meaning [OPEN] | [BYTES FACTORY.EXE seg2:0xbe4] [BYTES seg14:0x1baa seg14:0x1bbc seg14:0x1bd4] |
| +0x08 | 1 | byte | the same with control 230, DGROUP:0x96ec, the body's +0x17 and DGROUP:0x977d + 0xad·n. Meaning [OPEN] | [BYTES seg14:0x1bb3 seg14:0x1bea seg14:0x1c0a] |
| +0x09 | 1 | flags | 0x01 Factory combo 180, 0x04 Repair System (checkbox 144), 0x08 Cloak (145), 0x10 Captures Icons (143), 0x20 Factory menu items 498 and 499 (see below) | [BYTES FACTORY.EXE seg5:0x2f69 seg5:0x2f87 seg5:0x2fa5 seg2:0xc5a seg2:0x1285 seg2:0x12b6] [BYTES seg14:0x1ffd seg14:0x1fd6 seg14:0x1eb4 seg14:0x1ed6 seg14:0x1efd] |
| +0x0a | 21 | string | ship name; copied to DGROUP:0x9742 + 0xad·n | [BYTES seg14:0x2091 seg14:0x209e] |
| +0x1f | 2 | word | hit sound id, played when the ship is hit [MANUAL Customizing Ships]; 0xffff means the WAV named at +0x38. seg14:0x1b84 stores it in DGROUP:0x9700 + 0xad·n and in word +0x42 of the ship's pool record ([structs.md](structs.md#object-record-0x70-bytes)), and passes a positive id to seg10:0x754a | [BYTES FACTORY.EXE seg2:0xcae] [BYTES seg14:0x1e18 seg14:0x1e2d seg14:0x20c0 seg14:0x20d3] |
| +0x21 | 2 | word | MASTER.BOD record index | [BYTES seg14:0x1bc3] [BYTES FACTORY.EXE seg4:0x45de] |
| +0x23 | 21 | string | author, which the Factory takes from a window text | [BYTES FACTORY.EXE seg4:0x4834 seg4:0x4860 seg4:0x486a] [BYTES MASTER.SHP] |
| +0x38 | 9 | string | sound file base name (`.WAV` stripped); copied to DGROUP:0x9765 + 0xad·n | [BYTES FACTORY.EXE seg2:0xcae] [BYTES seg14:0x1c5a seg14:0x1c63] |

- The flag bits of +0x09 set design-record words (each + 0xad·n):
  - 0x10 sets DGROUP:0x9714 to 1. [BYTES seg14:0x1eb4]
  - 0x01 sets DGROUP:0x9716 (design word +0x44, the flame row) to 0, else 1, and 0x20 then ORs 0x20 into it. [BYTES seg14:0x1ed6-0x1eef seg14:0x1efd seg14:0x1f02-0x1f09]
  - 0x08 (Cloak) sets DGROUP:0x971c (design word +0x4a) to 1, else 0, and selects special ability 4. seg14:0x1b84 copies the word to ship byte +0xe7 (DGROUP:0x7177). [BYTES seg14:0x1fd6 seg14:0x1fe2 seg14:0x1fef seg14:0x1d9c seg14:0x21ea seg14:0x21ee]
  - 0x04 sets DGROUP:0x971e to 1. [BYTES seg14:0x1ffd]
- **The flame row.** seg10:0x5d3c reads the ship's flame from row (design +0x44)·f of the two-row flame bitmap that ISPACE.DLL ordinal 18 renders ([ispace.md](ispace.md)), f being the strip cell. So a ship with bit 0x01 set takes row 0 and one with it clear row 1. [BYTES seg10:0x5da7 seg10:0x5feb seg10:0x5fee]
  - With bit 0x20 set the word is 0x20 or 0x21, a row far outside the bitmap. Whether another step masks the bit, and what the blit then shows, is [OPEN]. [BYTES seg14:0x1f09 seg10:0x5feb]
- In the Factory, FACTORY.EXE seg4:0x4cb0 sets or clears one bit of FACTORY.EXE DGROUP:0xf99, the edited ship's copy of +0x09. [BYTES FACTORY.EXE seg4:0x46b5 seg4:0x46b9 seg4:0x4ccb seg4:0x4cdc]
  - Bit 0x01 follows combo 180: FACTORY.EXE seg2:0xbe4 sets it when `CB_GETCURSEL` returns a non-zero index and clears it otherwise. [BYTES FACTORY.EXE seg2:0x1047 seg2:0x1100 seg2:0xc0d seg2:0xc1b seg2:0xc55 seg2:0xc5a]
  - Menu item 498 sets bit 0x20 and item 499 clears it. FACTORY.EXE seg2:0xbbb checks item 498 when the bit is set, and item 499 when it is clear. [BYTES FACTORY.EXE seg2:0x10a4 seg2:0x10a9 seg2:0x1285 seg2:0x12b6 seg2:0xbb4 seg2:0xbbb seg2:0xb5d]
  - The items are `&Male` (498) and `&Female` (499) of a `&Gender` menu. The Factory's start-up function FACTORY.EXE seg2:0x0 appends that menu only when the command line is `GENDER` or `gender`. [BYTES FACTORY.EXE seg2:0x4c seg2:0x5d seg2:0x6e seg2:0x80 seg2:0x91 seg2:0xa3]
- Bytes +0x04 and +0x05 are copied to DGROUP:0x9776 and DGROUP:0x9777 + 0xad·n. [BYTES seg14:0x2040]
- In the Factory the sound combo's first 15 entries map to ids 6, 7, 8, 18, 30, 9, 16, 10, 14, 15, 13, 12, 11, 23 and 31. Any later entry is a WAV file: the Factory writes 0xffff and the file name without `.WAV`. [BYTES FACTORY.EXE seg2:0xcae]
- The Factory's strength preview adds Fuel Tank × 2 to the INSPACE formula. [BYTES FACTORY.EXE seg4:0x4748 seg4:0x4758]

## Saved games

### Names and triggers
seg6:0x885c(named, kind) writes a saved game. `named` = 0 uses `lastgame`, or `demogame` when DGROUP:0x95bc = 0. Any other `named` uses the INI `Restore` value, read into a 9-byte buffer (at most 8 characters). `kind` 0 writes `.sav` into the Games directory, 1 `.ded` and 2 `.don` into the Dead directory. [BYTES seg6:0x886b seg6:0x88e5 seg6:0x8993 seg6:0x88d2 seg6:0x88dd seg6:0x8973 seg6:0x8980 seg6:0x898b seg6:0x8900 seg6:0x8906 seg6:0x899f seg6:0x89a4 seg6:0x89b0 seg6:0x89bb]

| Call site | Arguments | File | When |
|---|---|---|---|
| seg9:0x31f0 | (0, 0) | `lastgame.sav` or `demogame.sav` | at the end of seg9:0x2828, the wave-end roster update, when the player ship's byte +0xec ≠ 0, DGROUP:0x91f8 > 0, and DGROUP:0x95bc ≠ 0 or DGROUP:0x9516 = 0. seg9:0x6cc4, the entry of state 2 (the debriefing), clears DGROUP:0x92e4 and calls seg9:0x2828 |
| seg7:0x687 | (0, 0) | `demogame.sav` | the demo's wave end ([shell.md](shell.md#screen-states)) |
| seg6:0x9e56 | (1, 0) | `<Restore>.sav` | at the end of seg6:0x99b6, the save-time roster update ([Ship roster records](#ship-roster-records-0x7c-bytes)) |
| seg10:0x8657 | (1, 0) | `<Restore>.sav` | the save step seg10:0x8568 with argument 0 or −1; arguments 1 to 3 go through seg6:0x99b6 ([shell.md](shell.md#exit)) |
| seg6:0x5e39 | (1, 1) | `<name>.ded` | seg6:0x5d58, the entry of state 5 ([Retired games](#retired-games)) |
| seg16:0x7235 | (1, 2) | `<name>.don` | after adding 50,000 to the score |

[BYTES seg7:0x687 seg9:0x31d0 seg9:0x31d7 seg9:0x31de seg9:0x31e5 seg9:0x31f0 seg9:0x6ceb seg9:0x6d85 seg6:0x9e56 seg10:0x8632 seg10:0x863d seg10:0x8657 seg6:0x5e39 seg16:0x7228 seg16:0x7235]

- The manual names LASTGAME (the end of each wave) and DEMOGAME (the demo pilot). It offers a save with Alt-S, on exit (Alt-X-S) and before ESC aborts to the intro. [MANUAL High Scores] The menu, the keys and the Save Game dialog, which writes `Restore`, are in [shell.md](shell.md#exit).
- The demo saves and restores `demogame` only while DGROUP:0x9516 = 0, which DemoType 520 gives ([shell.md](shell.md#intro-and-title-screen)). [BYTES seg9:0x31e5]
- A `.ded` or `.don` file needs a score of at least 1000: the writer returns 0 when the score dword DGROUP:0x1e58 is negative or below 1000. [BYTES seg6:0x8871 seg6:0x8876 seg6:0x8878 seg6:0x887a seg6:0x8880]
- For `.ded`/`.don`, when the fourth character of `Restore` is a space (as in `NEW GAME`, the value read back after New Game OK writes ` NEW GAME` [ORACLE newgame-0001]), the name is the pilot name (DGROUP:0x380c) cut to 7 characters and then at its first space: the pilot is copied unbounded into a stack buffer, cut at index 7, copied over the `Restore` name, and a loop bounded by the shrinking lstrlen turns the first space into NUL. [BYTES seg6:0x8911 seg6:0x891f seg6:0x8927 seg6:0x8944-0x8964 seg6:0x8947 seg6:0x894e]
- **The limited trial.** With DGROUP:0x920c = 0 and DGROUP:0x95bc ≠ 0 the routine writes nothing and returns 0. Only a manual save (`named` ≠ 0, `kind` = 0) shows a message first, string 2324: "Game saving is a handy feature of the full version of Inner Space." The automatic save and the `.ded`/`.don` saves fail silently. [BYTES seg6:0x8887 seg6:0x888e seg6:0x8895 seg6:0x889b seg6:0x88a1 seg6:0x88a3 seg6:0x88aa] [BYTES ENG_LANG.DLL STRING 146] id 2324
  - After Alt-S and Save in the trial the oracle saw the `Restore=<name>` INI write, the 2324 MessageBox (caption "Inner Space", flags 0x30, from seg16:0x6920) and no file write. [ORACLE wave-0001]
- seg6:0x930a loads `LocalPath` + Games + (`Restore`, or `demogame` when DGROUP:0x95bc = 0) + `.sav` ([Reading](#reading)), with no space test: a `Restore` of `NEW GAME` opens `NEW GAME.sav`. [BYTES seg6:0x9383 seg6:0x9396 seg6:0x93ae seg6:0x93bc seg6:0x9377-0x93d1]
- In core-0003's registered run the manual save of wave 1 wrote `Restore=ORACLE03` and then `C:\SPACE\GAMES\ORACLE03.sav` with mode 0x1002, and the wave end wrote `lastgame.sav`, in lower case, 1.0 s after the gate's HYPER sound. [ORACLE core-0003]

### Writing
seg6:0x885c works in this order. [BYTES seg6:0x885c]

1. The score gate for `.ded`/`.don` and the trial gate above.
2. seg6:0x7042 purges this game's retired files, for every kind. It scans `Path` + `Dead` + `*.ded`, reads the first 0x26 bytes of each file with seg14:0x4006, closes it and deletes it (`OpenFile` mode 0x200, OF_DELETE) when its whole dword +0x00 equals the game id DGROUP:0x939a/0x939c. `.don` files are never purged. A short read moves to the next file without `_lclose`, which leaks the handle. [BYTES seg6:0x88b5 seg6:0x7075 seg6:0x70b4 seg6:0x70f4 seg6:0x7101 seg6:0x710b seg6:0x7122 seg6:0x7127-0x7149 seg6:0x714e]
3. The name ([Names and triggers](#names-and-triggers)).
4. For `.ded`/`.don` only, the free-name loop. seg6:0x8776(name) returns 1 when OpenFile(name, OF_READ) fails, before any allocation; otherwise it reads GlobalSize(GlobalAlloc(0x42, 0x1a9)) bytes with seg14:0x4006, returns 1 on a short read or for the same game id and 0 only when the file reads in full and its dword +0x00 differs, closing the file on both read paths. The GlobalAlloc result is never tested. On 0 the name becomes `Path` + `Dead` + the base name cut in place to 5 characters ([bp−0x8d]) + itoa of a counter raised before each use, so the first suffix is 1, + `.don` or `.ded`, and is tested again. The first test is on the `LocalPath` name, the retries on `Path` names. [BYTES seg6:0x8776 seg6:0x879c-0x87a1 seg6:0x87a8 seg6:0x87be seg6:0x8807 seg6:0x8811 seg6:0x8819-0x882f seg6:0x884b seg6:0x89cc seg6:0x89dc seg6:0x89f0 seg6:0x89fe seg6:0x8a06 seg6:0x8a21 seg6:0x8a29 seg6:0x8a4d seg6:0x8a58]
5. `OpenFile` with mode 0x1002, OF_CREATE \| OF_READWRITE; an error returns 0. [BYTES seg6:0x8a75 seg6:0x8a78]
6. The header block: `GlobalAlloc(0x42, 0x1a9)`, filled by seg6:0x8eea ([Header](#header)). A failed GlobalLock frees the header, closes the file, which stays behind empty, and returns 0. [BYTES seg6:0x8a86 seg6:0x8a8a seg6:0x8aa4 seg6:0x8aa8 seg6:0x8ab4 seg6:0x8ab9]
7. When the header handle, DGROUP:0x94ae (directory records) or DGROUP:0x95a4 (roster) is 0, the writer closes the file and returns 0, for every kind, although only `.sav` writes the directory block. The header block is neither unlocked nor freed, and the created file stays empty. [BYTES seg6:0x8ac8-0x8adb seg6:0x8acf seg6:0x8ad6 seg6:0x8ab1 seg6:0x8ab4 seg6:0x8b25]
8. The blocks ([Layout](#layout)): the header, the roster from DGROUP:0x6e24 with `GlobalSize(DGROUP:0x95a4)` bytes and, for `.sav` only, the directory records from DGROUP:0x69f4 with `GlobalSize(DGROUP:0x94ae)` bytes. [BYTES seg6:0x8aec seg6:0x8afa seg6:0x8b05 seg6:0x8b1e seg6:0x8b25 seg6:0x8b2f seg6:0x8b48]
   - seg6:0x9e66(size, ptr, file) (`retf 0xa`) writes 0x7fff-byte chunks while the remaining dword exceeds 0x7fff, advancing the huge pointer with __AHINCR, then one `_lwrite` of the low word of the rest. It returns the original size in DX:AX, or 0 on any short write, which makes the writer unlock and free the header, close the file and return 0. [BYTES seg6:0x9e66 seg6:0x9e8b-0x9e96 seg6:0x9ea4 seg6:0x9ea9 seg6:0x9eae seg6:0x9ed8 seg6:0x9edd seg6:0x9ee2 seg6:0x8b4f-0x8b58]
9. On success it closes the file, unlocks and frees the header and returns 1. Its callers drop the result: the save step seg10:0x8568 returns the dialog's result, 1 without a dialog. [BYTES seg6:0x8add-0x8b85 seg10:0x8657 seg10:0x865c seg10:0x862d seg10:0x867c]
- The save step seg10:0x8568 runs seg6:0x99b6 for arguments 1 to 3 before any trial test, so in the trial the roster update of [Ship roster records](#ship-roster-records-0x7c-bytes) still runs; the trial test is inside seg6:0x885c. [BYTES seg10:0x863d seg6:0x9e56 seg6:0x8887]

### Reading
seg6:0x930a restores a `.sav` file. Its callers are the ESC flow at the title and the demo start ([shell.md](shell.md#game-start-dialogs)). In the trial (DGROUP:0x920c = 0, DGROUP:0x95bc ≠ 0) it pushes the string id 0x915, 2325, to seg16:0x68d6(DGROUP:0x16, 2325, −1) and returns 0. [BYTES seg6:0x930a seg6:0x9319-0x933a seg6:0x9327 seg6:0x9329 seg6:0x9330] [BYTES ENG_LANG.DLL STRING 146] id 2325

1. Before opening the file it sets the game id DGROUP:0x939a to `time()` and the start date DGROUP:0x939e, DGROUP:0x93a0 to the current date. [BYTES seg6:0x933f seg6:0x9345 seg6:0x934e seg6:0x9354 seg6:0x9357]
2. It opens the name ([Names and triggers](#names-and-triggers)) with `OpenFile` mode 0, and returns 0 when that fails. [BYTES seg6:0x93d1]
3. It allocates the 0x1a9-byte header and reads `GlobalSize` bytes of it ([Layout](#layout)). [BYTES seg6:0x93e0 seg6:0x9409 seg6:0x944b]
4. It frees the running game's roster (DGROUP:0x95a4) and directory block (DGROUP:0x94ae), and only then allocates the new ones with the sizes from the header and reads each. [BYTES seg6:0x944b seg6:0x945a seg6:0x946e seg6:0x9473 seg6:0x9487 seg6:0x94a4]
5. **Failures.** A short read, or a failed header lock, calls seg6:0x9932(header, file, 0), which closes the file, frees the header and frees whatever DGROUP:0x94ae and DGROUP:0x95a4 hold, zeroing them, then returns 0. [BYTES seg6:0x9403 seg6:0x9457 seg6:0x9518 seg6:0x9522 seg6:0x9932]
   - When the header read fails, those handles are still the running game's blocks, so a failed restore destroys the current game's roster and directory records. [BYTES seg6:0x944b seg6:0x9518]
   - A failed `GlobalAlloc` (seg6:0x93f0, seg6:0x94b0, seg6:0x9549) returns 0 without closing the file. At the last two the old blocks are already freed, and DGROUP:0x6e24 or DGROUP:0x69f4 still points into a freed block. [BYTES seg6:0x93f0 seg6:0x94b0 seg6:0x9549]
6. A header +0x171 that differs from `Size` (DGROUP:0x9156) is stored there, written to the INI and sets DGROUP:0x92e2 = 1. [BYTES seg6:0x95be seg6:0x95ca seg6:0x95e8 seg6:0x95ed]
7. It loads the globals from the header ([Header](#header)), in this order: the game id ← +0x00; DGROUP:0x939e ← (+0x18 << 8) OR the sign-extended +0x19 and DGROUP:0x93a0 ← the sign-extended +0x1a; ship 0 bytes +0x5d, +0x5e ← +0x26, +0x28; DGROUP:0x91f8 ← +0x24; DGROUP:0x94a2 ← +0x2a; DGROUP:0x931c, DGROUP:0x9432, DGROUP:0x964a, DGROUP:0x94a4 and DGROUP:0x931a ← +0x2c … +0x34; the score ← +0x1e, the rating ← +0x16a, DGROUP:0x93ac ← +0x166, DGROUP:0x94ca ← +0x168, DGROUP:0x9346 ← +0x18f, DGROUP:0x967c ← +0x18a; the two 10-byte tables, DGROUP:0xa161 ← +0x188 and DGROUP:0xa162 ← +0x189; ship DGROUP:0x9646's +0x18 ← +0x172, ship 0's +0xd2 ← +0x15a and +0xd0 ← +0x15c; DGROUP:0x9134 ← +0x16e, DGROUP:0x94ac ← +0x16f, DGROUP:0x5a8 ← +0x18b; the pilot ← +0x04; the 8 × 8, the three 8 × 3 and the 8-word tables; DGROUP:0x945c, DGROUP:0x945e ← +0x156, +0x158; DGROUP:0xa1cd, DGROUP:0xa1cf ← +0x196 and DGROUP:0xa193 … 0xa196 ← +0x192 …; DGROUP:0x1318 ← +0x173. [BYTES seg6:0x95f3-0x9856]
   - Every byte field is sign-extended into its word global: +0x16e, +0x16f, +0x172, +0x173, +0x18a, +0x18f and +0x196. [BYTES seg6:0x972c seg6:0x9746 seg6:0x970c seg6:0x9855 seg6:0x96c0 seg6:0x96b7 seg6:0x9824]
   - The pilot is copied with an unbounded lstrcpy, so a file whose +0x04 … +0x17 hold no NUL copies on past the field ([ADR 0014](../decisions/0014-saved-games.md) decision 9). [BYTES seg6:0x975b-0x9764]
   - Header +0x26 and +0x28 go to bytes +0x5d and +0x5e, and +0x15a and +0x15c to words +0xd2 and +0xd0, of ship record 0 (DGROUP:0x70ed, DGROUP:0x70ee, DGROUP:0x7162, DGROUP:0x7160), not of the record DGROUP:0x9646 that the writer read. [BYTES seg6:0x962d seg6:0x9634 seg6:0x971c seg6:0x9724 seg6:0x8f43 seg6:0x8f52]
   - Header +0x172 goes to word +0x18 of ship record DGROUP:0x9646. [BYTES seg6:0x9707-0x9713]
   - Header +0x24 goes to DGROUP:0x91f8 and +0x2a to DGROUP:0x94a2. The current directory record's byte +0xad becomes 1 when it is 0; the index +0x2a is not checked against +0x32 ([ADR 0014](../decisions/0014-saved-games.md) decision 9). [BYTES seg6:0x963b seg6:0x9642 seg6:0x9645 seg6:0x964d seg6:0x9655]
   - `PlayMode` DGROUP:0x9134 is forced to 1 when DGROUP:0x95bc = 0. [BYTES seg6:0x972d seg6:0x9737]
   - Header +0x22, +0x160, +0x162, +0x164, +0x170 and the save date +0x1b … +0x1d are not read back; +0x15e is only tested (step 10). [BYTES seg6:0x95b4-0x9856 seg6:0x98ce]
8. **The roster check.** With MASTER.BOD and MASTER.SHP loaded, records k = 0 … DGROUP:0x931a − 1 are checked; the last record, DGROUP:0x931a, is not. A byte +0x64 of 7 becomes 0. A word +0x00 not below the SHP count DGROUP:0x95aa becomes 1, and so does record 0's when seg14:0x455c rejects its ship. [BYTES seg6:0x986d seg6:0x9874 seg6:0x9881 seg6:0x9884 seg6:0x9892 seg6:0x98a4 seg6:0x98ac seg6:0x98af seg6:0x98b2]
   - seg14:0x455c returns 1 while DGROUP:0x11aa ≠ 0. Otherwise it returns 0 for the ships whose MASTER.BOD word +0x21 is 178, 310, 320, 400, 440, 450, 520, 550, 560, 590, 600, 604, 605, 710, 720, 730, 750, 890, 900, 930 or 950, the list of ISPACE.DLL seg10:0x611c ([shell.md](shell.md#game-start-dialogs)). [BYTES seg14:0x455c seg14:0x4589-0x45f0]
9. It sets DGROUP:0x92e4 = 1 and clears DGROUP:0x951c, DGROUP:0x104c and DGROUP:0x104e. [BYTES seg6:0x98bb seg6:0x98be seg6:0x98c1 seg6:0x98c4]
10. When header +0x15e is below 100, every checked roster word +0x02 is divided by 10, unsigned for a value below −15000 as a signed word. [BYTES seg6:0x98ce seg6:0x9900]
11. It closes the file, frees the header and returns 1. [BYTES seg6:0x9925]
- **The block reads.** seg14:0x4006(file, ptr, size) reads min(remaining, 0x7fff) bytes per `_lread` (unsigned dword), returns 0 on a short read and 1 once the count reaches 0, also for a count of 0 on entry. So a file longer than its blocks is accepted and the rest ignored. [BYTES seg14:0x401b seg14:0x4022 seg14:0x403f seg14:0x4044 seg14:0x406a]
- **The clean-up.** seg6:0x9932(header, file, keep) closes the file, frees the header when its handle is non-zero and, for keep = 0, frees DGROUP:0x94ae and DGROUP:0x95a4 when non-zero and zeroes each handle with its far pointer (DGROUP:0x69f4/6, DGROUP:0x6e24/6). [BYTES seg6:0x993f seg6:0x9944 seg6:0x995a seg6:0x9979 seg6:0x9984 seg6:0x99a0 seg6:0x99ab]

### Resuming
- At the title the ESC flow skips the #5600 135 table reload for a restored game and keeps DGROUP:0x91f8, because DGROUP:0x92e4 = 1 ([shell.md](shell.md#game-start-dialogs)). [BYTES seg11:0x7a8b seg11:0x7a99 seg11:0x7ac4 seg11:0x7acb]
- Navigation Enter then requests state 1 because DGROUP:0x92e4 ≠ 0. State 1 runs seg9:0xc06, which skips its new-game reset, and the briefing seg9:0x5cca, which raises DGROUP:0x91f8. So a game saved during wave N resumes at wave N + 1. [BYTES seg6:0x368c seg6:0x3693 seg9:0xc44 seg9:0xd5f seg9:0x5fbf] core-0003 restores a game saved in wave 1 straight to the desk, without the New Game dialog, and Navigation Enter shows "WAVE 2 MISSION BRIEFING"; systems, credit, ammo and score come back exact and `fuel` as the roster byte × 40. [ORACLE core-0003]
- The wave set-up seg9:0x6624 calls seg14:0x0 with DGROUP:0x91f8 raised by one. seg14:0x0 builds a fresh one-record roster only when DGROUP:0x91f8 ≤ 1. [BYTES seg14:0x232 seg14:0x237 seg9:0x5e58 seg9:0x5e5d seg9:0x5e60]
- The fresh record comes from GlobalAlloc(0x42, 0x7c), zero-filled, and seg14:0x0 writes no byte of +0x30 … +0x37. It takes byte +0x64 = the SHP team byte +0x03, also stored in DGROUP:0x970a. It sets +0x38 … +0x3b = 100, +0x02 = 15, +0x1c = SHP byte +0x00, and +0x72 … +0x75 = 1. With SHP byte +0x06 ≠ 0 it sets +0x1d = 2, +0x1f = 20 and +0x29 = SHP +0x06, or, when DGROUP:0x11aa = 0 and word DGROUP:0x2f8a + 0x50·w ≠ −1, that word's low byte; otherwise +0x1d = 1. [BYTES seg14:0x252 seg14:0x254 seg14:0x256 seg14:0x258 seg14:0x2b0-0x3f3 seg14:0x32a seg14:0x335 seg14:0x33a seg14:0x34d seg14:0x354 seg14:0x36b seg14:0x373 seg14:0x37e] It clears +0x3c … +0x63, +0x04 and +0x06 and the relation table (+0x08 and +0x12 = 0, +0x17 = 0xff), sets word +0x00 to seg14:0x2300's result and DGROUP:0x931a to 0. [BYTES seg14:0x335 seg14:0x373 seg14:0x37e seg14:0x391 seg14:0x39e seg14:0x3a6 seg14:0x3af seg14:0x3c4 seg14:0x3cc seg14:0x3e4 seg14:0x3e9 seg14:0x3f3]

### Retired games
- On GAME OVER, seg6:0x5d58 builds the score list with seg6:0x6576 first, so the running game is its entry 0, and then writes the `.ded` with seg6:0x885c(1, 1). It writes nothing when entered from state 9 (DGROUP:0x94cc ≠ 0), the path of the completion ending. [BYTES seg6:0x5df9 seg6:0x5e2d seg6:0x5e39 seg6:0x5d65]
- The completion ending writes the `.don` at seg16:0x7235 ([Names and triggers](#names-and-triggers)).
- The score list has no file of its own ([Score list](#score-list)). Its update at the game's end is the `.ded` or `.don` write together with the purge of the game's older `.ded` files ([Writing](#writing)).
- A `.ded` or `.don` holds the header and the roster but no directory records. [BYTES seg6:0x8b25]

### Layout
A file is up to three blocks back to back. Each block's length is its `GlobalSize`: the allocation rounded up to 32 bytes under Wine (see [Conventions](#conventions)). [BYTES seg6:0x8aec seg6:0x8b05 seg6:0x8b2f]

| Block | Allocation | Present in | Memory after loading |
|---|---|---|---|
| header | 0x1a9 bytes (0x1c0 in the file under Wine) | all | freed after copying into globals |
| ship roster | (header +0x34 + 1) × 0x7c | all | far pointer DGROUP:0x6e24, handle DGROUP:0x95a4 |
| directory records | header +0x32 × 0x297 | `.sav` only | far pointer DGROUP:0x69f4, handle DGROUP:0x94ae |

[BYTES seg6:0x93e0 seg6:0x9409 seg6:0x9492 seg6:0x9497 seg6:0x94a4 seg6:0x952a seg6:0x9531 seg6:0x953d seg6:0x8b5b]

- The reader allocates each block with the same size, asks `GlobalSize` and reads that many bytes, so it fails on a file without the padding. [BYTES seg6:0x9409 seg6:0x944b seg6:0x94cc seg6:0x950c seg6:0x9565 seg6:0x95a5]
- Under Wine the blocks are 32-byte multiples: core-0003's `ORACLE03.sav` and `lastgame.sav` are 1248 = 0x1c0 + 0x80 + 0x2a0 bytes (one roster record, one directory record) and `ORACLE3X.sav` 1504 = 0x1c0 + 0x180 + 0x2a0 (three roster records), written with three `_lwrite`s and zero padding. The reader's GlobalAlloc(0x1a9) is read as 0x1c0 bytes. [ORACLE core-0003]
- In memory the roster grows by `GlobalReAlloc` to (n + 1) × 0x7c as ships join, starting from one 0x7c block, and the directory block grows by 0x297 per record. [BYTES seg14:0x256 seg14:0xee5 seg14:0xeeb seg6:0x370a seg6:0x3758 seg6:0x3760]

### Header
seg6:0x8eea fills the header and seg6:0x930a reads it back. "Player ship" fields come from the ship record of DGROUP:0x9646 ([structs.md](structs.md#ship-records-8--0x15a-bytes-at-dgroup0x7090)). [BYTES seg6:0x8eea seg6:0x930a]

| Offset | Size | Content | Evidence |
|---|---|---|---|
| +0x00 | dword | game id: `time()` when the game began (DGROUP:0x939a, set by the new-game routine seg10:0x82a2). The score list and the name check use it | [BYTES seg6:0x90b4 seg6:0x90b7 seg10:0x805d seg10:0x8069 seg10:0x84ea seg10:0x84ee] |
| +0x04 | 20 | pilot name, copied from DGROUP:0x380c with an unbounded lstrcpy after the dates. It is the only text of the header and can hold the user's name: INSTALL.EXE writes the default `ThePlayer` as the registration Name cut at its first space ([install.md](install.md)) | [BYTES seg6:0x90c1 seg6:0x90ee seg6:0x912c] [BYTES INSTALL.EXE seg2:0x2aa3-0x2b4e] |
| +0x18 | byte ×3 | start date: `tm_mon` (0-based), `tm_mday`, `tm_year` (years since 1900), from DGROUP:0x939e and DGROUP:0x93a0 | [BYTES seg6:0x90c1 seg6:0x90c8 seg6:0x90cf seg10:0x8080 seg10:0x8088 seg10:0x808c seg10:0x84fc seg10:0x84ff] |
| +0x1b | byte ×3 | save date, same encoding, from seg10:0x804c(0); not read back | [BYTES seg6:0x90d5 seg6:0x90e3 seg6:0x90e7 seg6:0x90ee] |
| +0x1e | dword | score (DGROUP:0x1e58) | [BYTES seg6:0x8f93 seg6:0x968a] |
| +0x22 | word | SHP index (word +0x00) of the player's roster record DGROUP:0x931c; not read back | [BYTES seg6:0x9034-0x9043 seg6:0x903d seg6:0x9043] |
| +0x24 | word | DGROUP:0x91f8, the wave number; Restore Game prints it as the wave reached (string 3000) | [BYTES seg6:0x8f5e] [BYTES seg10:0xa255] [BYTES ENG_LANG.DLL STRING 188] id 3000 |
| +0x26, +0x28 | word | player ship bytes +0x5d and +0x5e, sign-extended; read back as bytes into ship record 0 | [BYTES seg6:0x8f48 seg6:0x8f57 seg6:0x962d seg6:0x9634] |
| +0x2a | word | DGROUP:0x94a2: index of the current directory record | [BYTES seg6:0x8f65] |
| +0x2c | word | DGROUP:0x931c: the player's roster index | [BYTES seg6:0x8f6c] |
| +0x2e, +0x30 | word | DGROUP:0x9432, DGROUP:0x964a | [BYTES seg6:0x8f73 seg6:0x8f7a] |
| +0x32 | word | DGROUP:0x94a4: directory record count | [BYTES seg6:0x8f81] |
| +0x34 | word | DGROUP:0x931a: highest roster index | [BYTES seg6:0x8f88] |
| +0x36 | 64 words | the 8 × 8 word table DGROUP:0x3824 (see [ISPACE.DLL #5600 135](#ispacedll-5600-135)) | [BYTES seg6:0x91dd] |
| +0xb6, +0xe6, +0x116 | 24 words each | 8 × 3 word tables DGROUP:0x4eec, DGROUP:0x4fd0, DGROUP:0x6b10 | [BYTES seg6:0x9206 seg6:0x920f seg6:0x9218] |
| +0x146 | 8 words | DGROUP:0x6b40 | [BYTES seg6:0x9236] |
| +0x156, +0x158 | word | DGROUP:0x945c, DGROUP:0x945e | [BYTES seg6:0x924d seg6:0x9255] |
| +0x15a, +0x15c | word | player ship words +0xd2 and +0xd0: the icons captured and destroyed. The Restore Game description prints them as CAPTURED and DESTROYED of a total +0x156 + +0x158 (string 3001, only when either is non-zero) | [BYTES seg6:0x8fd6 seg6:0x8fc7] [BYTES seg10:0xa265 seg10:0xa26e seg10:0xa276] [BYTES ENG_LANG.DLL STRING 188] id 3001 |
| +0x15e | word | the ship's shape, 100–103: 100 when the average of the player ship's words +0x26 … is above 70, 101 above 45, 102 above 27, else 103. The average takes 4 words when the special-ability word of the ship's design (design word +0x28, DGROUP:0x96fa + 0xad·n, n = ship word +0x4e) is not −1, else 3. Restore Game prints string 311 + (value − 100): GOOD, FAIR, BAD, SORRY. A value below 100 marks an older file: the loader divides roster word +0x02 by 10 | [BYTES seg6:0x9131 seg6:0x9137 seg6:0x913d seg6:0x917d seg6:0x9185 seg6:0x9196 seg6:0x91a7 seg6:0x91b3 seg6:0x98ce seg6:0x98e7] [BYTES seg10:0xa184] [BYTES ENG_LANG.DLL STRING 20] ids 311–314 |
| +0x160 | word | number of directory records whose byte +0x1d0 is 2, the completed directories. Restore Game compares it with +0x32 to choose strings 2500–2503 | [BYTES seg6:0x9102 seg6:0x910d] [BYTES seg10:0xa1f4] [BYTES ENG_LANG.DLL STRING 157] ids 2500–2503 |
| +0x162 | word | the ship's credit: player ship dword +0xdc, in hundredths of an icon, capped at 0x4fff6 in the ship record itself, / 10. Restore Game prints v / 10 "." \|v mod 10\| ICONS OF CREDIT (string 3002) | [BYTES seg6:0x8fed-0x9003 seg6:0x9011] [BYTES seg10:0xa2c9 seg10:0xa2cc seg10:0xa2da] [BYTES ENG_LANG.DLL STRING 188] id 3002 |
| +0x164 | word | player ship word +0xce | [BYTES seg6:0x9020] |
| +0x166, +0x168 | word | DGROUP:0x93ac, DGROUP:0x94ca | [BYTES seg6:0x9061 seg6:0x9069] |
| +0x16a | dword | rating (DGROUP:0x5078) | [BYTES seg6:0x8fa2 seg6:0x969b] |
| +0x16e | byte | DGROUP:0x9134 (INI `PlayMode`); the loader forces 1 when DGROUP:0x95bc = 0 | [BYTES seg6:0x9051 seg6:0x9727 seg6:0x9737] |
| +0x16f | byte | DGROUP:0x94ac | [BYTES seg6:0x9059] |
| +0x170 | byte | 0, then, when player ship byte +0xe4 ≠ 0, (byte +0xac = 0) + 2·(byte +0xad = 0); the Restore Game preview passes it to ISPACE.DLL seg10:0x2aac | [BYTES seg6:0x8efb seg6:0x8f07 seg6:0x8f0e seg6:0x8f17 seg6:0x8f22 seg6:0x8f2d] [BYTES ISPACE.DLL seg10:0x1c49 seg10:0x1c57] |
| +0x171 | byte | INI `Size` (DGROUP:0x9156). A loaded value that differs is written back to the INI | [BYTES seg6:0x8f38 seg6:0x95b8 seg6:0x95e8] |
| +0x172 | byte | player ship word +0x18 (`weapon_slot`), read back into ship DGROUP:0x9646 | [BYTES seg6:0x902f seg6:0x9707-0x9713] |
| +0x173 | byte | DGROUP:0x1318 | [BYTES seg6:0x92aa] |
| +0x174, +0x17e | 10 bytes each | DGROUP:0xa14d, DGROUP:0xa157 | [BYTES seg6:0x9090 seg6:0x909c] |
| +0x188, +0x189 | byte | read into DGROUP:0xa161 and DGROUP:0xa162; the writer leaves them 0 | [BYTES seg6:0x96f7 seg6:0x96ff] |
| +0x18a | byte | DGROUP:0x967c | [BYTES seg6:0x9079] |
| +0x18b | dword | DGROUP:0x5a8, the play time in frames ([timing.md](timing.md)). Restore Game prints it / 36 as minutes and seconds (string 3000) and writes the quotient back into its header buffer | [BYTES seg6:0x8fb3] [BYTES seg10:0xa130 seg10:0xa13f] [BYTES ENG_LANG.DLL STRING 188] id 3000 |
| +0x18f | byte | DGROUP:0x9346 | [BYTES seg6:0x9071] |
| +0x190, +0x191 | byte | DGROUP:0xa1cd / 128 toward zero (`cwd; and dx, 0x7f; add; sar 7`) and the idiv remainder; the reader rebuilds (int8)+0x190 << 7 plus (int8)+0x191, exact for −16511 … 16383 | [BYTES seg6:0x925d-0x9266 seg6:0x926e-0x9274 seg6:0x9809-0x981b] |
| +0x192 | 4 bytes | DGROUP:0xa193–0xa196: both loops run i = 2 … 5 over header +0x190 + i and DGROUP:0xa191 + i | [BYTES seg6:0x9281-0x92a2 seg6:0x9828-0x984a] |
| +0x196 | byte | DGROUP:0xa1cf | [BYTES seg6:0x927c] |
| +0x197 | 18 bytes | never written: 0 from GMEM_ZEROINIT, and not read | [BYTES seg6:0x8a86 seg6:0x8eea-0x92b6] |

- The block is GlobalAlloc(0x42, 0x1a9), so every byte the writer skips (+0x188, +0x189 and +0x197 … +0x1a8) is 0. [BYTES seg6:0x8a86 seg6:0x8eea-0x92b6]
- The meanings of the fields that only name a DGROUP word are [OPEN].
- What the reader does with the fields is in [Reading](#reading).
- "Restore Game" in this table is INSPACE export 13, which writes the description of a save ([shell.md](shell.md#game-start-dialogs)). [BYTES seg10:0xa118]
- The writers of DGROUP:0x5a8, and the [OPEN] question whether a new game resets it, are in [globals.md](globals.md#global-table). [DESIGN]
- seg6:0x8eea has three more callers, seg16:0x7473, seg17:0x7e02 and seg17:0x83c7, which fill a header for the progress texts; each caps the player's credit in place too. [BYTES seg16:0x7473 seg17:0x7e02 seg17:0x83c7]

### The header's readers
- **Restore Game's description,** INSPACE export 13 ([shell.md](shell.md#game-start-dialogs), item 3), divides header dword +0x18b by 36 with an unsigned 32-bit `div` and writes the quotient back. It prints "ALL OF THEM" (2501) when +0x160 = +0x32 ≠ 1, both 0 included, "IT" (2503) when both are 1, "NONE OF THEM" (2502) when +0x160 = 0 ≠ +0x32, and 2500 "%d OF THEM" otherwise. String 3027 takes, in order, the flag lstrcmp(header +0x04, DGROUP:0x380c) = 0, its negation, the pilot copy cased by seg11:0x3fd4(…, 1), (sum = 0), sum, sum and 304, sum being the 16-bit total of the 8 words +0x146 (the high word of the dword zeroed at seg10:0xa18a). [BYTES seg10:0xa12a-0xa13f seg10:0xa18a-0xa1e4 seg10:0xa1ef-0xa219 seg10:0xa2f4-0xa321] [BYTES ENG_LANG.DLL STRING 157] ids 2500–2503 [BYTES ENG_LANG.DLL STRING 190] id 3027
  - core-0003 shows the three descriptions of a registered game's saves, for example "SO FAR, YOU HAVE REACHED WAVE 1 AFTER PLAYING FOR 1 MIN 51 SEC, VISITING 1 DIRECTORY AND COMPLETING NONE OF THEM. YOUR SHIP HAS 1.5 ICONS OF CREDIT AND IS IN GOOD SHAPE." for +0x18b = 4005. [ORACLE core-0003]
- **The desk monitor** seg15:0x52e4, after a restore with QuickIntro < 2, reads INI `Restore` (9 bytes) and skips its progress text when a space stands at any index below lstrlen, unlike the writer's index-3 test. Otherwise it opens `LocalPath` + Games + `Restore` + `.sav` and reads the header with seg14:0x4006 without testing the result. It prints the counts as export 13 does, except that +0x160 ≠ +0x32 with +0x160 = 0 and +0x32 ≤ 1 gives 2529 "SOME OF IT", and it ends with string 3031 taking 0, (+0x164 > 400, signed) and 311 + +0x15e. [BYTES seg15:0x53e2-0x5405 seg15:0x5449 seg15:0x54af seg15:0x54b7 seg15:0x5521 seg15:0x5545-0x555d seg15:0x5601-0x562c] [BYTES ENG_LANG.DLL STRING 159] id 2529 [BYTES ENG_LANG.DLL STRING 190] id 3031
  - core-0003's desk after restoring a wave-1 save types "… VISITING 1 DIRECTORY AND COMPLETING SOME OF IT. / YOUR SHIP IS IN GOOD SHAPE.", where Restore Game reads "NONE OF THEM" for the same file. [ORACLE core-0003]

### Ship roster records (0x7c bytes)
structs.md calls the index of one of these the ship's `roster_index` (+0xd6). The wave end seg9:0x2828 writes each ship's record when the ship has one and DGROUP:0x91f8 > 0, after seg14:0x421c(i); before a manual save seg6:0x99b6 does the same and then calls seg6:0x885c(1, 0) ([Writing](#writing)). [BYTES seg9:0x2b14 seg9:0x2b19 seg9:0x2c5a seg9:0x2e5c-0x2e7f seg6:0x99b6 seg6:0x99c5 seg6:0x99ca seg6:0x9e51 seg6:0x9e53 seg6:0x9e56]
- **One body, two copies.** The per-ship bodies seg9:0x2b5d–0x2e59 and seg6:0x99d7–0x9cda have the same effect and differ in three instruction sequences: the credit cap compare (seg9:0x2cdf `mov eax; cmp eax, 0x4fff6; jle` against seg6:0x9b5f `cmp dx, 4; jl; jg; cmp ax, 0xfff6; jbe`), the credit push before seg5:0x314 (one `push dword` at seg9:0x2d21 against two `push word`), and the loop exit (`jge seg9:0x2e59` at seg9:0x2da4 against `jl; jmp` at seg6:0x9c24). [BYTES seg9:0x2cdf-0x2cee seg6:0x9b5f-0x9b6b seg9:0x2d21 seg9:0x2da4 seg6:0x9c24-0x9c26]
- seg6:0x99b6 calls only seg14:0x421c, seg5:0x110, seg5:0x314 and seg6:0x885c(1, 0) (a `push cs; call` near call at seg6:0x9e56); it makes no seg11:0xb09c call and has no loop over the roster records updating +0x06. It skips its body, of the roster and of the current directory record, when DGROUP:0x91f8 ≤ 0, and sets DGROUP:0x931c to the player's `roster_index`. seg9:0x2828's ship loop also calls seg11:0xb09c(0, i) for every ship. [BYTES seg6:0x9ad4 seg6:0x9b86 seg6:0x9ba6 seg6:0x9bba seg6:0x9e56 seg6:0x9d03 seg9:0x2e5c-0x2e7f]
- **Back into the ship.** seg14:0x1430 copies a record into its ship at every placement, for a roster index R ≠ −1; the fields are in the table. For R = −1 it sets `fuel` = 40 × SHP +0 (signed `imul byte`), +0xac = +0xad = 1, +0x24 = +0x0a = 2, +0xce = 100, `credit` 0, +0x82 = 0 and the four systems 100. [BYTES seg14:0x16c5 seg14:0x1b25-0x1b56 seg14:0x1ae4]

| Offset | Size | Content | Evidence |
|---|---|---|---|
| +0x00 | word | SHP index. The loader sets it to 1 when it is not below the SHP count, or for record 0 when seg14:0x455c rejects the ship | [BYTES seg6:0x9884 seg6:0x9892 seg6:0x98a4] |
| +0x02 | word | ship dword +0xdc, capped at 0x4fff6 in the ship record itself, / 10. The cap is a signed compare, so a negative value stays. The placement sets `credit` = 10 × the word (signed 16 × 16 → 32) | [BYTES seg6:0x9b61 seg6:0x9b6d seg6:0x9b73 seg6:0x9b86 seg6:0x9b92 seg14:0x1997 seg14:0x199a] |
| +0x04 | word | ship word +0xce (`resources`), both ways | [BYTES seg6:0x9b55 seg9:0x2cdb seg14:0x199e seg14:0x19a2] |
| +0x06 | word | ship word +0x82, both ways ([+0x06 at the wave end](#the-0x06-word-at-the-wave-end)) | [BYTES seg6:0x9ae7 seg9:0x2c60 seg14:0x19a6 seg14:0x19b2] |
| +0x08 + 2a, +0x12 + a, +0x17 + a | words, bytes, bytes, a = 0 … 4 | a five-entry table that seg14:0x421c keeps sorted; +0x17 + a holds another ship's roster index. Its meaning is [OPEN] | [BYTES seg14:0x421c seg6:0x9ad4] |
| +0x1c | byte | the low byte of ship word +0x1e / 40 (signed), written unconditionally; read back as `fuel` = 40 × byte ([core.md](core.md#the-words)) | [BYTES seg6:0x9b00 seg9:0x2c6d seg9:0x2c71 seg14:0x18c9] |
| +0x1d | byte | ship byte +0xcb, the slot count n, both ways | [BYTES seg6:0x9ac9 seg9:0x2c4f seg14:0x16da seg14:0x1551] |
| +0x1e + k, +0x28 + k | 10 bytes each, k = 0 … 9 | ship bytes +0xae + k (count) and +0xb8 + k (weapon), written for k < n. The placement copies k = 1 … n − 1 back and sets slot 0 to weapon 0 with count 1, so slot 0 is saved but not read back; it clears ship +0xb8 + k for k = n … 9 first. When DGROUP:0x11aa = 0 and word DGROUP:0x2f8a + 0x50·(sign-extended +0x28 + k) ≠ −1 it writes that word's low byte back into +0x28 + k before the copy, so the roster itself is remapped. +0x30 and +0x31 are the weapons of slots 8 and 9 | [BYTES seg6:0x9a9b seg6:0x9a84 seg9:0x2bf4 seg9:0x2c0a seg9:0x2c10 seg9:0x2c21 seg14:0x1531 seg14:0x1536 seg14:0x1564-0x1587 seg14:0x16e5-0x170a seg14:0x1722 seg14:0x172f seg14:0x1738 seg14:0x173b seg14:0x174e seg14:0x1756] |
| +0x32 + k | 6 bytes, k = 0 … 5 | the powerups GUNS, THRUSTERS, ENGINE ARMOUR, GUN ARMOUR, THRUST ARMOUR and WING ARMOUR, each bought up to five times from 0 ([core.md](core.md#purchases-seg160x3784)). Writers: seg16:0x4065 and the cheat keys seg11:0x72fa (+0x32 = 6) and seg11:0x7496 (+0x33 = 6). +0x33 scales the thrust at seg11:0x664d and the turn at seg14:0x2274: seg14:0x1b84 divides ship word +0x14 by (value + 10) / 10 ([MASTER.BOD](#masterbod-ship-bodies) +0x1d). A reader of +0x32 is [OPEN] | [BYTES seg16:0x4065 seg11:0x72fa seg11:0x7496 seg11:0x664d seg14:0x2274 seg14:0x2279 seg14:0x2294 seg14:0x22a7] |
| +0x38 | 4 bytes | ship words +0x26…+0x2c cut to bytes, +0x38 = 0 when ship byte +0xec is 0; the placement copies them back sign-extended into every ship with a record, and seg18:0x3d6 into the player's ship. The visit writes 100 into the player's record ([core.md](core.md#repairs-and-refuel)) | [BYTES seg6:0x9a39 seg6:0x9a5b seg9:0x2baa seg9:0x2bbf seg9:0x2bd1 seg9:0x2be1 seg14:0x1775-0x179b] [BYTES seg18:0x4a7 seg18:0x4b6 seg18:0x4c0 seg18:0x4ca] |
| +0x3c + k | byte | for roster record k: ship word +0x62 + 2j / 10, where j is the ship holding record k | [BYTES seg6:0x9a02] |
| +0x64 | byte | team: the fresh record takes the SHP team byte +0x03. The loader turns the value 7 into 0 | [BYTES seg14:0x3e4 seg14:0x3e9 seg14:0x3f3] [BYTES seg6:0x986d seg6:0x9874] |
| +0x72, +0x73 | byte | ship bytes +0xac and +0xad. The placement copies them back, or sets both to 1 when byte +0xe4 of ship DGROUP:0x9646, not of the ship being placed, is 0 | [BYTES seg6:0x9b33 seg6:0x9b44 seg9:0x2ca8 seg9:0x2cac seg14:0x1935 seg14:0x193b seg14:0x1940 seg14:0x1949 seg14:0x195f seg14:0x1970] |
| +0x74, +0x75 | byte | the low bytes of ship words +0x24 and +0x0a, read back sign-extended | [BYTES seg6:0x9b11 seg6:0x9b22 seg14:0x1980 seg14:0x198a seg14:0x198e] |
| +0x76 | byte | class 1–10 of v = (ship dword +0xdc × 3 + word +0xd8) / 100: the first k with word DGROUP:0x952 + 2k ≤ v < word DGROUP:0x96c + 2k (signed), which gives the bands [0, 28), [28, 34), [34, 40), [40, 48), [48, 55), [55, 63), [63, 70), [70, 80), [80, 97), [97, 114). For a v outside them the byte takes the class of the last earlier ship of the same update that matched, or an uninitialised stack value when none did | [BYTES seg6:0x9ba6 seg6:0x9bb4 seg6:0x9bba seg6:0x9bcd seg6:0x9bd4 seg6:0x9bda seg6:0x9be3 seg6:0x9be6 seg6:0x9bf7 seg9:0x2d49-0x2d5e] [BYTES DGROUP:0x952] |
| +0x77, +0x78, +0x79 | byte | wave counters, kept by seg14:0x1430: when +0x79 = DGROUP:0x91f8 − 1 it adds 1 to +0x78; else when +0x78 > 1 (signed byte) it sets ship word +0x00 = 4 and +0x78 = 1; else +0x78 = 0. It adds 1 to +0x77 when +0x79 ≠ DGROUP:0x91f8, and then stores DGROUP:0x91f8's low byte in +0x79 | [BYTES seg14:0x19b6 seg14:0x19bc-0x19cc seg14:0x19d6-0x19f3 seg14:0x1a04-0x1a1e] |
| +0x7a, +0x7b | byte | the player's record gives the visit's ship size s4 = 4·(+0x7a + (+0x7b − 6)/2) ([core.md](core.md#phase-2)); the writers are [OPEN] | [BYTES seg16:0xdc1 seg16:0xdd4] |

- The meanings of the fields marked [OPEN] above, the class of +0x76 included, are [OPEN].
- seg14:0x421c inserts into the +0x08/+0x12/+0x17 table only when a flag is set that it overwrites for every ship of its loop, so in effect the last ship decides. [BYTES seg14:0x421c]
- seg9:0x2828 writes no roster field from ship +0xd0 or +0xd2; it reads +0xd8 for v. [BYTES seg9:0x2b5d-0x2e59 seg9:0x2d2d]

#### The +0x06 word at the wave end
After the per-ship bodies, seg9:0x2828 changes word +0x06 of every roster record r = 0 … DGROUP:0x931a while the frame counter dword DGROUP:0x1e68 > 500 (unsigned). With c = byte +0x76 and D = DGROUP:0x9334: d = +0x06 / −3 when c + 3 < D, else −(+0x06 / 8) (toward zero, by `sar 3` with a sign fix) when c + 1 < D, else +0x06 / −14. Then d += lo16(+0x06 × \|SHP byte +0x05 of record +0x00 − DGROUP:0x9332\|) / −20, and +0x06 = max(0, +0x06 + d), the clamp following the sign bit of the 16-bit sum, so it wraps on overflow. seg14:0x3eda and seg14:0x4074 bracket it when DGROUP:0x95a6 = 0. [BYTES seg9:0x2e97-0x2ecc seg9:0x2ed6 seg9:0x2edd seg9:0x2efb seg9:0x2f00 seg9:0x2f07 seg9:0x2f14 seg9:0x2f1b seg9:0x2f21 seg9:0x2f2a seg9:0x2f3b seg9:0x2f3e seg9:0x2f40 seg9:0x2f42 seg9:0x2f49-0x2f7f seg9:0x88ba]

#### Records added and freed
- **Added.** seg14:0xdd6 adds a record only while DGROUP:0x95b4 > DGROUP:0x95b2; otherwise it uses its caller's index. While DGROUP:0x931a ≤ 38 it raises DGROUP:0x931a and calls GlobalReAlloc(h, (DGROUP:0x931a + 1)·0x7c, 0x42). Above that it reuses the record 1 … 39 with the lowest signed word +0x06 below 1000 not held by ships 0 … slot − 1, after clearing to 0xff every directory byte +0x1c0 + j (j < 8, records below DGROUP:0x94a4) that names it; with none it calls MessageBeep(0) and takes record 10. [BYTES seg14:0xde5-0xdec seg14:0xdf0 seg14:0xdf5 seg14:0xdf9-0xe67 seg14:0xe69-0xe76 seg14:0xe7b-0xec6 seg14:0xecd seg14:0xee1-0xeeb seg14:0x10d0]
  - The new or reused record gets +0x00 = the SHP index, +0x06 = 15, +0x02 = `rand()` % ((DGROUP:0x9334 + 1)·10) + 10, +0x04 = `rand()` % ((DGROUP:0x9334 + 1)·50) + 150 (signed idiv), +0x77 = 0, +0x64 = SHP +3 and the +0x1d/+0x1f/+0x29 rule of the fresh record ([Resuming](#resuming)), then seg9:0x5be0. [BYTES seg14:0xf03 seg14:0x101b seg14:0x106c seg14:0x1071-0x1083 seg14:0x1087-0x109f]
- **Freed.** seg10:0x3864 frees the roster block DGROUP:0x95a4 and the directory block DGROUP:0x94ae and zeroes DGROUP:0x94a4, DGROUP:0x94a2 and DGROUP:0x931a. seg10:0x22e calls it unconditionally, and seg10:0x22e has three callers: seg6:0x19e4, seg9:0xda9 and seg10:0x217, the last in seg10:0x1fe when DGROUP:0x90f4 = 1. [BYTES seg10:0x386e-0x38d6 seg10:0x256 seg6:0x19e4 seg9:0xda9 seg10:0x217]

### Directory records (0x297 bytes)
There is one record per directory the player has entered. DGROUP:0x94a2 indexes the current one. [BYTES seg6:0x36ce seg6:0x36f3]

| Offset | Size | Content | Evidence |
|---|---|---|---|
| +0xa0 … +0xa4 | bytes | the generator's object counts, copied back to the briefing on a revisit ([world.md](world.md#difficulty-inputs)); +0xa0, +0xa1: seg6:0x99b6 counts pool-3 records with word +0x18 = 0 and word +0x44 = 350, and the same for 1400 | [BYTES seg6:0x9d20 seg6:0x9d30 seg6:0x9d75 seg6:0x9d92 seg6:0x9dbc] |
| +0xa3 | byte | the same count for pool-9 records with word +0x18 = 0 | [BYTES seg6:0x9d10 seg6:0x9d41 seg6:0x9d56] |
| +0xad | byte | the gate count a revisited non-race directory takes ([world.md](world.md#difficulty-inputs)); the loader sets it to 1 in the current record when 0 | [BYTES seg6:0x9645] [BYTES seg9:0x3214] |
| +0x14e, +0x150 | word | the DESTROYED and CAPTURED counts of the Navigation status line ([nav.md](nav.md#directory-records)), counted by seg7:0x5ae8 ([core.md](core.md#counters-seg70x5ae8-and-seg70x5a46)) | [BYTES seg6:0x5b20 seg6:0x5994] [BYTES seg7:0x5b07 seg7:0x5b1c] |
| +0xae | words | one per pool-8 record: word +0x1e, negated when that record's entry in the 0x34-byte table at DGROUP:0x3d5a holds −3 | [BYTES seg6:0x9dd7 seg6:0x9df2 seg6:0x9dfb seg6:0x9e23] |
| +0xfe | words | the matching word +0x24 of that table entry | [BYTES seg6:0x9e2c seg6:0x9e41] |
| +0x152 | 110 bytes | directory path; a new record copies it from DGROUP:0x6990 and a lookup compares it with DGROUP:0x6990 ([nav.md](nav.md#buffers)) | [BYTES seg6:0x36e4 seg6:0x377f] |
| +0x1c0 | 8 bytes | roster indices present in the directory, 0xff = free. In the current record seg6:0x99b6 removes a ship's index when its byte +0xec is 0, its word +0x82 is negative or it is the player's ship, and otherwise adds it at the first free byte when absent; with no free byte it is dropped | [BYTES seg6:0x9c03 seg6:0x9c0a seg6:0x9c11 seg6:0x9c53 seg6:0x9cc4 seg6:0x9cd5] |
| +0x1d0 | byte | the directory's icon state: 2 complete (counts towards header +0x160), 1 icons remain, 0 placed with none left over ([world.md](world.md#programs-and-icons), [nav.md](nav.md#completion)) | [BYTES seg6:0x9102 seg6:0x598c seg9:0x2a seg9:0x114 seg9:0x99a seg9:0xab1] |
| +0x1c8 + s | 8 bytes | icon slot s's flag at the wave end: 2 when the icon's byte +0x2f < 0 and 1 when its `capturable` (+0x53) = 2, 1 winning; written by seg9:0x2828 ([core.md](core.md#seg90x2828-the-wave-into-the-directory-record)) | [BYTES seg9:0x2f82 seg9:0x3077] |
| +0x1d1 | byte | cleared at the briefing of a new directory ([core.md](core.md#counters-seg70x5ae8-and-seg70x5a46)); its meaning is [OPEN] | [BYTES seg9:0xbf0] |
| +0x1d2 + 2s | word | icon slot s's damage, clamped to 0 when negative ([world.md](world.md#programs-and-icons)); seg7:0x233c mirrors an icon's `damage` into it ([core.md](core.md#shooting-an-icon-seg70x233c)) | [BYTES seg9:0x156] [BYTES seg7:0x2386] |
| +0x1e2 | bitmap | icons already taken, captured or destroyed, bit n for icon number n, set by seg7:0x5a46 ([nav.md](nav.md#directory-records), [core.md](core.md#counters-seg70x5ae8-and-seg70x5a46)) | [BYTES seg9:0x156 seg9:0xac4] [BYTES seg7:0x5ad8] |

- The Navigation fields are in [nav.md](nav.md#directory-records); the rest of the record is [OPEN].

## Score list
seg6:0x6576 builds the "Champions of INNER SPACE" list from the saved games. [BYTES seg6:0x6576] [MANUAL High Scores] The title is string 925. [BYTES seg6:0x6318] [BYTES ENG_LANG.DLL STRING 58] id 925 Its callers are the state-5 entry seg6:0x5d58 ([shell.md](shell.md#screen-states), State 5) and the Online Help's list seg17:0x6b65, seg17:0x7351 and seg17:0x89d5. [BYTES seg6:0x5df9 seg17:0x6b65 seg17:0x7351 seg17:0x89d5]

1. **Kept list.** While the far pointer DGROUP:0x6af8 is non-null it skips the scan and only writes the output (step 6) from the kept list; seg6:0x717a frees it. [BYTES seg6:0x658b seg6:0x6592 seg6:0x6594 seg6:0x6a2d]
2. **Entry 0** is the running game, built from the globals with file name `--` and status 0. [BYTES seg6:0x65bc seg6:0x6663]
3. **Scan.** It scans `Games\*.sav`, `Dead\*.ded` and `Dead\*.don`, every pattern and every file path under `LocalPath`, and reads the first 0x26 bytes of each file's header into a 0x31-byte entry: header +0x00 … +0x25, the file name without its last 4 characters at +0x26 (10 bytes) and the status byte +0x30 = the pass, 0 `.sav`, 1 `.ded`, 2 `.don`. A saved file of the running game whose score is not higher is dropped (its id zeroed). [BYTES seg6:0x66a8 seg6:0x669c seg6:0x66c9-0x6704 seg6:0x6718 seg6:0x6759 seg6:0x676d seg6:0x678d seg6:0x67a0 seg6:0x67b4 seg6:0x680d seg6:0x6836-0x6840 seg6:0x6864]
   - **A file that does not open, or yields fewer than 0x26 bytes, ends the routine at once** (jmp seg6:0x7039): no list entries, DGROUP:0x93a2 not set, the open file and the two 0x31 blocks left behind, and seg14:0x4074 not called ([ADR 0014](../decisions/0014-saved-games.md) decision 9). [BYTES seg6:0x6752 seg6:0x6754 seg6:0x6777 seg6:0x6779 seg6:0x7034 seg6:0x7039]
4. **Rank.** DGROUP:0x93a2, whose only writer is seg6:0x68eb, gets p + 1 when the running game (entry 0) is the pass-p maximum of the entries with a score ≥ 0. After each pick the entries with the picked game id, from index 0 when the pick is entry 0 and from index 1 otherwise, are taken out of the next passes. [BYTES seg6:0x686a-0x69b5 seg6:0x68e0 seg6:0x68e7 seg6:0x68eb seg6:0x6905-0x691b]
5. **Order.** The entries go into the kept list behind DGROUP:0x6af8, highest score first, with the count in DGROUP:0x94c6, only the best entry of each game id kept by the same start-index rule; so a saved file of the running game with a higher score than the running game keeps entry 0 too, and the running game appears twice. An entry's ship +0x22 becomes 0 when it is above DGROUP:0x95aa − 1 (signed) or seg14:0x455c refuses it. [BYTES seg6:0x65b2 seg6:0x69b7-0x6c6c seg6:0x6a35 seg6:0x6a81 seg6:0x6ac2-0x6af3 seg6:0x6bff-0x6c15]
6. **Output,** for i below DGROUP:0x94c6: [BYTES seg6:0x6dd3-0x7031]
   - With the help list DGROUP:0x9488 ≠ 0, a line `<i + 1> - <pilot> - <ship>`, then `\t230` score, `\t292` start date ` - ` save date (month + 1, day and the stored `tm_year`, sign-extended), `\t415` LoadString(2105 + status) ALIVE, DEAD or DONE!, `\t475` and `(<file>)`, sent with SendMessage(DGROUP:0x9488, 0x401, 0, line). A status above 2 would skip the LoadString and append stale text. How the window reads the `\t` markers is [OPEN]. [BYTES seg6:0x6c7e seg6:0x6c9b seg6:0x6cec seg6:0x6ebb seg6:0x6ed2 seg6:0x6f3e seg6:0x6f44 seg6:0x700d seg6:0x7019 seg6:0x7031] [BYTES ENG_LANG.DLL STRING 132] ids 2105–2107
   - Otherwise, only in state 5 (DGROUP:0x90f4 = 5), entry i goes into the next 0x2e-byte slot of the champions table DGROUP:0x6b04 when (DGROUP:0x93a2 < 8 and i < 10), or i < 3, or DGROUP:0x93a2 − 5 < i < DGROUP:0x93a2 + 3. A slot holds +0x00 = pilot + " - " (DGROUP:0xfa0) + the MASTER.SHP name (+0x0a of the 0x41-byte record), +0x28 = the score dword and +0x2c = i + 1. The table's drawing is in [shell.md](shell.md#screen-states) (State 5). [BYTES seg6:0x6d08-0x6d3e seg6:0x6d41-0x6dc8]
7. seg14:0x4074 frees the MASTER tables at the end. [BYTES seg6:0x7034]

## Race records
The file `RacePath` + course name + `.rac` keeps the best times of each ship on one course. [MANUAL Race Records] [BYTES seg6:0x8bb5 seg6:0x8bc5]

- The course names are a 9-byte table at DGROUP:0xa12c: 0 `LAZYLOOP`, 1 `SDISPEED`, 2 `GRANDPRI`. DGROUP:0x9322 selects the course. [BYTES seg10:0x3215 seg10:0x33bb seg10:0x3592 seg9:0x2893]
- The file is an array of 66-byte (0x42) records with no header, counted as length / 66. A missing file gives 0 records. [BYTES seg6:0x8be7 seg6:0x8bec seg6:0x8bfb seg6:0x8c11]
- The writer seg6:0x8dfe writes exactly count × 66 bytes. [BYTES seg6:0x8e63 seg6:0x8e73]

| Offset | Size | Content | Evidence |
|---|---|---|---|
| +0x00 | 20 | ship name, the key of the record (design-record name, DGROUP:0x9742 + 0xad·n) | [BYTES seg6:0x8cd0 seg6:0x8cff] |
| +0x14 + 10c | 10 each, c = 0…2 | holder of the last improved time in category c (see below) | [BYTES seg6:0x8dc1 seg6:0x8de5 seg6:0x8deb] |
| +0x32 + 4c + 2s | word | best time for category c and slot s; 0 means none | [BYTES seg6:0x8d7f seg6:0x8d85 seg6:0x8d9c] |

- seg6:0x8cb2 appends a record for an unknown ship and stores a time that is lower than the stored one or replaces 0. [BYTES seg6:0x8d21 seg6:0x8d2b seg6:0x8d7f]
- With each stored time it writes the holder. For the player's ship while DGROUP:0x95bc ≠ 0, seg17:0x8e60 supplies the name; every other case writes `----`. [BYTES seg6:0x8da0 seg6:0x8da8 seg6:0x8dc9 seg6:0x8deb]
  - seg17:0x8e60 copies the ship's roster string +0x65, which seg9:0x5be0 fills with the pilot name DGROUP:0x380c for the player's ship. [BYTES seg17:0x8e74 seg17:0x8e8c seg9:0x5bf4 seg9:0x5cb2]
  - For a ship without a roster record it builds a name from string 788 `Officer` and a number. [BYTES seg17:0x8e79 seg17:0x8eaf seg17:0x8ef1] [BYTES ENG_LANG.DLL STRING 50] id 788
- seg9:0x2828 posts the results: category 0 for 1 lap, 1 for 5 laps and 2 for 10 laps (DGROUP:0x9ea0). It posts each ship's total time DGROUP:0x9e60 + 4i in that category when the category is 1 or 2 [BYTES seg9:0x28e2], and its time DGROUP:0x9e90 + 4i in category 0. Times are capped at 0xfa00. [BYTES seg9:0x289d seg9:0x28b1 seg9:0x28be seg9:0x28c5 seg9:0x28f3 seg9:0x28f8 seg9:0x294a]
- Slot 1 receives every ship's times. Slot 0 receives the player's times only, while DGROUP:0x95bc ≠ 0. [BYTES seg9:0x28e8 seg9:0x291b seg9:0x29a9]
- The totals in DGROUP:0x9e60 count wave frames ([globals.md](globals.md#global-table)). The unit of DGROUP:0x9e90, and whether it is the best lap, are [OPEN].

## INI files

### Files and sections
- `INSPACE.INI` is named without a path, so the profile functions use the one in the Windows directory. INSPACE copies the name to DGROUP:0x96a9 and its section `Inner Space` to DGROUP:0x9690. [BYTES seg6:0x6ef seg2:0x17] [REF Win16 SDK GetPrivateProfileString] [ORACLE newgame-0001]
- If `<Windows directory>\INSPACE.INI` does not exist and the program directory holds `GRMSETUP.EXE`, seg6:0x6d8 starts it with `WinExec` and returns 0. [BYTES seg6:0x791 seg6:0x7c1 seg6:0x7d1 seg6:0x7d6]
- **Regeneration.** When `Path` is empty, seg6:0x6d8 shows "Some game settings are missing…" and writes the keys below. [BYTES seg6:0x7e9 seg6:0x7f2 seg6:0x814 seg6:0x845 seg6:0x86b seg6:0x8a0 seg6:0x8d5 seg6:0x8ea seg6:0x8ff seg6:0x914 seg6:0x929 seg6:0x93e] [ORACLE smoke-0001]
  - `Path` and `LocalPath` become the program's directory.
  - `ShipPath` becomes that + `SHIPS\`, `SndPath` + `SOUNDS\` and `RacePath` + `GAMES\`.
  - `Dead` becomes `RETIRED`, `Games` `GAMES`, `Language` `ENG`, `MainShip` `TRANSFORMER` and `ThePlayer` `your name`.
- ISPACE.DLL uses its own copies of the section and file names (ISPACE.DLL DGROUP:0x674, ISPACE.DLL DGROUP:0x680), as do both Factory builds. [BYTES ISPACE.DLL seg9:0x6e] [BYTES FACTORY.EXE seg4:0x18] [BYTES theworks/FACTORY.EXE seg4:0x18]
- Integer keys go through `GetPrivateProfileInt`. Integer writes format the value in decimal into a buffer first (INSPACE seg6:0xf1a, ISPACE.DLL seg3:0x236). [BYTES seg6:0x2b] [BYTES ISPACE.DLL seg9:0x21d]
- INSPACE seg6:0x1290 is a helper that reads an integer from `[Inner Space]`. [BYTES seg6:0x12a9]

### INSPACE.INI [Inner Space]
Type I is an integer and S a string with its buffer size. "→" names the DGROUP word an INSPACE read stores to. Where readers pass different defaults, each is listed with its reader. [DESIGN]

**Paths, identity and game choice**

| Key | Type | Default | Read | Written | Meaning |
|---|---|---|---|---|---|
| `Path` | S 0x6d | "" | [BYTES seg6:0x70a] [BYTES ISPACE.DLL seg9:0xde] [BYTES FACTORY.EXE seg4:0x470] | [BYTES seg6:0x814] | program directory → DGROUP:0x8f7a; regenerated when empty |
| `LocalPath` | S 0x6d | "" | [BYTES seg6:0x725] [BYTES ISPACE.DLL seg9:0x118 seg10:0x4b3e] | [BYTES seg6:0x845] | base of the save directories → DGROUP:0x9078; ISPACE.DLL also finds INSPACE.EXE there |
| `ShipPath` | S 0x6d | "" | [BYTES seg14:0x3dea seg14:0x3f12] [BYTES ISPACE.DLL seg5:0x1d9b seg10:0x3f97 seg10:0x4095] [BYTES FACTORY.EXE seg4:0x503] | [BYTES seg6:0x86b] | directory of MASTER.BOD and MASTER.SHP |
| `SndPath` | S 0x6d | "" | [BYTES ISPACE.DLL seg9:0x152] [BYTES FACTORY.EXE seg4:0x4c9] | [BYTES seg6:0x8a0] | sound directory ([sound.md](sound.md#settings)) |
| `RacePath` | S 0x6d | "" | [BYTES seg6:0x9c8] | [BYTES seg6:0x8d5] | directory of the `.rac` files → DGROUP:0x900a |
| `Games` | S 11 | "" | [BYTES seg6:0x9a0] | [BYTES seg6:0x8ff] | subdirectory of `LocalPath` for `.sav` → DGROUP:0x96b8 |
| `Dead` | S 11 | "" | [BYTES seg6:0x978] | [BYTES seg6:0x8ea] | subdirectory for `.ded` and `.don` → DGROUP:0x96c5 |
| `Language` | S 5 | "" | [BYTES seg2:0x1952 seg10:0x668b] [BYTES ISPACE.DLL seg9:0x1cd seg9:0x2ac seg9:0x32d7] [BYTES FACTORY.EXE seg2:0x1388 seg4:0x2fa9] | [BYTES seg6:0x914] | language code of `<code>_LANG.DLL` and the other language files ([ispace.md](ispace.md)) |
| `ThePlayer` | S 12 or 20 | "" (INSPACE); `AutoPilot` (ISPACE.DLL) | [BYTES seg10:0x8c seg10:0x8496] [BYTES ISPACE.DLL seg10:0x233e] | [BYTES seg6:0x93e] [BYTES ISPACE.DLL seg10:0x1de4 seg10:0x2687] | pilot name → DGROUP:0x380c; saved in header +0x04 |
| `MainShip` | S 20 | `EAGLE`; "" (ISPACE.DLL seg10:0x18e1) | [BYTES seg14:0x23bf seg15:0x2a50 seg16:0x3574] [BYTES ISPACE.DLL seg10:0x18e1 seg10:0x235a] | [BYTES seg6:0x929 seg14:0x2420 seg15:0x5b77 seg16:0x49cc] [BYTES ISPACE.DLL seg10:0x1949 seg10:0x1dcf seg10:0x23e1 seg10:0x259f seg10:0x28f5] | SHP name of the player's ship |
| `Restore` | S 9 or 20 | "" | [BYTES seg6:0x8906 seg6:0x9396 seg10:0x84b2 seg10:0x85c8 seg15:0x53d2] [BYTES ISPACE.DLL seg10:0xd2b seg10:0x1698] | [BYTES ISPACE.DLL seg10:0xe8c seg10:0x1d96] | name of the game to restore and save; New Game OK writes ` NEW GAME` [ORACLE newgame-0001] |
| `PlayMode` | I | 0 | [BYTES seg10:0x8478] [BYTES ISPACE.DLL seg10:0x23fb] | [BYTES ISPACE.DLL seg10:0x26c8] | → DGROUP:0x9134; saved in header +0x16e |
| `Drive` | S 20 or 10 | `C` | [BYTES seg10:0x8525] [BYTES ISPACE.DLL seg10:0x20a4] | [BYTES ISPACE.DLL seg10:0x25f7] | written by New Game OK; meaning [OPEN] |
| `Size` | I | 1 | [BYTES seg6:0xaf8 seg9:0x23f9 seg10:0x7fa9] [BYTES ISPACE.DLL seg9:0x17fe] | [BYTES seg6:0x95e8 seg9:0x23df seg10:0xa09d] [BYTES ISPACE.DLL seg9:0x21bb seg10:0x2646] | scale level → DGROUP:0x9156; saved in header +0x171 |
| `SetSize` | I | 1 | [BYTES ISPACE.DLL seg10:0x1ff7] | [BYTES seg10:0xa0ba] [BYTES ISPACE.DLL seg9:0x21d0 seg10:0x265b] | written with `Size`; meaning [OPEN] |
| `StratAsk` | I | 0 | [BYTES seg10:0xa988] [BYTES ISPACE.DLL seg10:0x26f5] | [BYTES seg10:0xaa6f] [BYTES ISPACE.DLL seg10:0x27d9] | while it is 0 or 10, New Game OK in Command mode asks strings 2850–2853, and Yes writes it + 1 ([shell.md](shell.md#game-start-dialogs)) [BYTES ISPACE.DLL seg10:0x26cd seg10:0x27a0 seg10:0x27d9] |
| `SavePrompt` | I | 1 | [BYTES seg10:0x85a5] [BYTES ISPACE.DLL seg9:0x2d6a] | [BYTES ISPACE.DLL seg9:0x2f18] | 0 skips the Save Game dialog on Save & Exit and on the abort's save while `Restore` holds a name (non-empty, no space at index 3), and the save goes ahead under that name ([shell.md](shell.md#exit)) [BYTES seg10:0x8585 seg10:0x85a5 seg10:0x85cd seg10:0x85d3] |
| `Accept` | I | 0 | [BYTES seg11:0x7a28] | [BYTES ISPACE.DLL seg10:0x1290 seg10:0x5ded] | the dialog procedure ISPACE ordinal 39 (ISPACE.DLL seg10:0x124a) writes it when button 1 is pressed. It is the procedure of dialog #988 Important Information! ([ispace.md](ispace.md)), whose button 1 is I Accept. I Accept writes 42 (itoa) and seg10:0x5b24 in mode 0x5b writes 1. ESC at the title with DGROUP:0x920c = 2 opens #988 while the value is not 42 ([shell.md](shell.md#game-start-dialogs)) [BYTES ISPACE.DLL seg10:0x1290 seg10:0x5ded] [BYTES seg11:0x7a28 seg11:0x7a34] [BYTES ENG_LANG.DLL DIALOG 988] |
| `PlusCount` | I | 13 | [BYTES seg10:0x880e] | [BYTES seg10:0x8852] | exit counter: seg10:0x87de adds 1 on each exit while DGROUP:0x11aa = 0 and DGROUP:0x920c is −1 or 3, and above 12 stores 0 and opens dialog #1872 ([shell.md](shell.md#exit)) [BYTES seg10:0x87eb seg10:0x8816 seg10:0x881f seg10:0x882d] |

**Window state and positions.** All are integers. seg6:0x0 reads most of them at start-up and seg6:0x33e writes them back. `Full` and `OnTop` go through seg6:0x1290. The columns give every site. [BYTES seg6:0x0 seg6:0x33e]

| Key | Default | → | Read | Written |
|---|---|---|---|---|
| `Full` | 1 | DGROUP:0x20 | [BYTES seg6:0x14] | [BYTES seg6:0x37e] |
| `OnTop` | 1 | — | [BYTES seg2:0x101d seg6:0x18b7] | — |
| `ShowLeft`, `ShowTop`, `ShowRight`, `ShowBottom` | 0 | DGROUP:0x1d20, 0x1d22, 0x1d24, 0x1d26 | [BYTES seg6:0x2b seg6:0x41 seg6:0x57 seg6:0x6d] | [BYTES seg6:0x630 seg6:0x67c seg6:0x656 seg6:0x6a2] |
| `IconWnd`, `VScale`, `SmWnd`, `SScale` | 1, 0, 0, 0 | DGROUP:0x8f72, 0xb6e, 0x8f74, 0x95f8 | [BYTES seg6:0x112 seg6:0x128 seg6:0x13e seg6:0x154] | [BYTES seg6:0x496 seg6:0x4bc seg6:0x4e2 seg6:0x508] |
| `ShowLaunch` | 2 | DGROUP:0x9298 | [BYTES seg6:0x16a] | — |
| `Menu` | 0 | DGROUP:0x90f6 | [BYTES seg6:0x180 seg6:0xd36] [BYTES ISPACE.DLL seg9:0x2d04] | [BYTES seg6:0x3a4] [BYTES ISPACE.DLL seg9:0x2ef3] |
| `RadarLeft`, `RadarTop` | 0 | DGROUP:0x1dd4, 0x1dd6; read and written back, they position nothing ([world.md](world.md#radar)) | [BYTES seg6:0x196 seg6:0x1ac] | [BYTES seg6:0x3d1 seg6:0x3f7] |
| `WatchLeft`, `WatchTop` | 0 | DGROUP:0x1ddc, 0x1dde | [BYTES seg6:0x1c2 seg6:0x1d8] | [BYTES seg6:0x424 seg6:0x470] |
| `WatchRight` | `WatchLeft` + 280 | DGROUP:0x1de0 | [BYTES seg6:0x1e8 seg6:0x1eb seg6:0x1f3] | [BYTES seg6:0x44a] |
| `IconLeft` | screen width − 48 | DGROUP:0x1de4 | [BYTES seg6:0x205 seg6:0x20a seg6:0x212] | [BYTES seg6:0x535] |
| `IconTop`, `SmLeft`, `SmTop`, `HelpLeft`, `HelpTop` | 0 | DGROUP:0x1de6, 0x1dec, 0x1dee, 0x1dc4, 0x1dc6 | [BYTES seg6:0x245 seg6:0x25b seg6:0x28e seg6:0x2a4 seg6:0x2ba] | [BYTES seg6:0x55b seg6:0x588 seg6:0x5ae seg6:0x5db seg6:0x601] |
| `NavLeft`, `NavTop`, `NavRight`, `NavBottom` | 30, 30, 560, 350 | DGROUP:0x6af0, 0x6af2, 0x6af4, 0x6af6 | [BYTES seg6:0x1cb6 seg6:0x1ccc seg6:0x1ce3 seg6:0x1cfa] | [BYTES seg6:0x2f07 seg6:0x2f59 seg6:0x2f32 seg6:0x2f84] |

- The game reads `IconLeft` with a default of `GetSystemMetrics(SM_CXSCREEN)` − 48, so the oracle at 1024 pixels saw 976. [BYTES seg6:0x205 seg6:0x20a] [ORACLE newgame-0001]

**Settings.** The INSPACE reads are in seg6:0xb3e unless stated. ISPACE.DLL reads these keys in ISPACE.DLL seg9:0x22ca, ISPACE.DLL seg9:0x15cc and ISPACE.DLL seg9:0x2bf8. It writes them in four functions: ISPACE.DLL seg9:0x1fae (`Complex` … `FrameRateSet`), ISPACE.DLL seg9:0x2dc6 (`ShipAnim` … `SavePrompt`), ISPACE.DLL seg9:0x331a (`StereoEffect` … `StereoSwap`) and ISPACE.DLL seg9:0x4712 (`JoyStick` … `JBtn`n). Which dialog control feeds which key is [OPEN], except for the sound keys, whose controls are in [sound.md](sound.md#audio-options-and-options). [BYTES ISPACE.DLL seg9:0x1fae seg9:0x2dc6 seg9:0x331a seg9:0x4712]

| Key | Type | Default | Read | Written | Meaning |
|---|---|---|---|---|---|
| `GameSpeed`, `FrameRateSet`, `Calibrate` | I | see [timing.md](timing.md) | [BYTES seg6:0xb5c seg6:0xb7d seg15:0x5f98] [BYTES ISPACE.DLL seg9:0x1a25] [BYTES FACTORY.EXE seg4:0x5b5 seg4:0x5cc seg4:0x3b3e] | [BYTES seg6:0xb31 seg10:0x7f0b] [BYTES ISPACE.DLL seg9:0x21fb] | timing ([timing.md](timing.md)). The Factory reads `Calibrate` with default 1000 |
| `Detail` | I | 3 | [BYTES seg6:0xc76] [BYTES ISPACE.DLL seg9:0x2592] | [BYTES ISPACE.DLL seg9:0x2196] | detail level → DGROUP:0x9208 |
| `Quick` | I | 0 (INSPACE, Factory); 1 (ISPACE.DLL) | [BYTES seg6:0xc60] [BYTES ISPACE.DLL seg9:0x25a8] [BYTES FACTORY.EXE seg4:0x3bc3] | [BYTES ISPACE.DLL seg9:0x2170] | → DGROUP:0x931e |
| `Enhance` | I | 1 | [BYTES seg6:0xc8c] [BYTES ISPACE.DLL seg9:0x257c] | [BYTES ISPACE.DLL seg9:0x20fe] | → DGROUP:0x8fe8 |
| `Complex` | I | 0 (INSPACE seg6:0x1204); 2 (ISPACE.DLL) | [BYTES seg6:0x1204] [BYTES ISPACE.DLL seg9:0x1766] | [BYTES ISPACE.DLL seg9:0x20b3] | → DGROUP:0x8ff8, and the wave budget DGROUP:0x932a and DGROUP:0x9000 ([world.md](world.md#difficulty-inputs)) |
| `Braking` | I | 0 | [BYTES seg6:0xcf4] [BYTES ISPACE.DLL seg9:0x16fb] | [BYTES ISPACE.DLL seg9:0x20d8] | → DGROUP:0x8ff6, the auto-brake ([world.md](world.md#drift-and-drag)) |
| `MixChannels` | I | 1 (INSPACE); 4 (ISPACE.DLL) | [BYTES seg6:0xc9c seg6:0xca7] [BYTES ISPACE.DLL seg9:0x55d seg9:0x571] | [BYTES ISPACE.DLL seg9:0x3493] | → DGROUP:0x920a; ISPACE.DLL DGROUP:0x219e, the channel count ([sound.md](sound.md#session)) |
| `BadVideo` | I | 1 | [BYTES seg6:0xcda] [BYTES ISPACE.DLL seg10:0x16ab] | — | → DGROUP:0x742 |
| `QuickIntro` | I | 0 | [BYTES seg6:0x318 seg6:0xcc4] [BYTES ISPACE.DLL seg9:0x2c77] | [BYTES ISPACE.DLL seg9:0x2e84] | → DGROUP:0x8f70 |
| `ShipAnim` | I | 1 | [BYTES seg6:0xd0a] [BYTES ISPACE.DLL seg9:0x2d48] | [BYTES ISPACE.DLL seg9:0x2e5f] | → DGROUP:0x131c |
| `Border` | I | 1 | [BYTES seg6:0xd20] [BYTES ISPACE.DLL seg9:0x2d26] | [BYTES ISPACE.DLL seg9:0x2ece] | → DGROUP:0xa78, the border and bevel switch ([world.md](world.md#border-and-bevel)); seg6:0xd51 is its only writer |
| `Bright` | I | 0 | [BYTES seg6:0xe88] [BYTES ISPACE.DLL seg9:0x2ce2] | [BYTES ISPACE.DLL seg9:0x2ea9] | — |
| `IgnoreRes` | I | 0 | [BYTES seg6:0x1401] | — | → DGROUP:0x9132 |
| `Aux` | I | 0 | [BYTES seg6:0x2fb] | — | debug traces ([globals.md](globals.md)) → DGROUP:0x131a |
| `Rate` | I | 0 | [BYTES seg6:0x32e] | — | → DGROUP:0x8fec |
| `JoyStick` | I | 0 | [BYTES seg6:0xc4a] [BYTES ISPACE.DLL seg9:0x239b seg9:0x4603] | [BYTES ISPACE.DLL seg9:0x4755] | joystick on → DGROUP:0x8ff4 ([timing.md](timing.md)) |
| `4Btns` | I | 0 | [BYTES ISPACE.DLL seg9:0x23b1 seg9:0x45ed] | [BYTES ISPACE.DLL seg9:0x4786] | → ISPACE.DLL DGROUP:0x2290 |
| `JMinX`, `JMinY`, `JRangeX`, `JRangeY` | I | 0 | [BYTES ISPACE.DLL seg9:0x23c7 seg9:0x23dd seg9:0x23f3 seg9:0x2409] | [BYTES ISPACE.DLL seg9:0x47ac seg9:0x47d2 seg9:0x47f8 seg9:0x481e] | → ISPACE.DLL DGROUP:0x19e8–0x19ee |
| `JBtn0`…`JBtn3` | I | n | [BYTES seg6:0xbeb seg6:0xc25] [BYTES ISPACE.DLL seg9:0x4557 seg9:0x4591] | [BYTES ISPACE.DLL seg9:0x489f] | the key is `JBtn` + n in decimal |
| `SoundFX`, `VoiceFX`, `Music` | I | 1 | [BYTES ISPACE.DLL seg9:0x22e5 seg9:0x22fb seg9:0x2311] | [BYTES ISPACE.DLL seg9:0x3448 seg9:0x346e seg9:0x3422 seg9:0x235 seg9:0x2483 seg9:0x24a9 seg9:0x24cf] | → ISPACE.DLL DGROUP:0x694, 0x696, 0x221e |
| `SoundVolume`, `FXMusic` | I | 100, 50 | [BYTES ISPACE.DLL seg9:0x2327 seg9:0x233d] | [BYTES ISPACE.DLL seg9:0x2124 seg9:0x33fc] | → ISPACE.DLL DGROUP:0x69e, 0x6a2 |
| `StereoEffect`, `PurgeSound`, `StereoSwap` | I | 2, 1, 0 | [BYTES ISPACE.DLL seg9:0x2353 seg9:0x2369 seg9:0x237f] | [BYTES ISPACE.DLL seg9:0x33d6 seg9:0x34b9 seg9:0x34df] | → ISPACE.DLL DGROUP:0x228c, 0x698, 0x69a |
| `SoundSystem` | I | −1 | [BYTES ISPACE.DLL seg9:0x2538] | [BYTES ISPACE.DLL seg9:0x2569 seg9:0x214a] | → ISPACE.DLL DGROUP:0x2280 |
| `AllowSound`, `ForceVol` | I | 0 | [BYTES ISPACE.DLL seg9:0x2444 seg9:0xbde] | — | see [sound.md](sound.md#modes-and-device-test) |
| `SaverVolume` | I | 0 | [BYTES ISPACE.DLL seg9:0x6e] | — | → ISPACE.DLL DGROUP:0x69e in place of `SoundVolume` ([sound.md](sound.md#settings)) |
| `Breakfast`, `Coffee`, `Lunch`, `Tea` | I | 800, 1000, 1200, 1530 | [BYTES seg10:0x96df seg10:0x970b seg10:0x96b2 seg10:0x9737] | — | special times of day [MANUAL Special Times of Day] |

**Race, duel and demo options**

| Key | Type | Default | Read | Written | Meaning |
|---|---|---|---|---|---|
| `Racers`, `RaceDiff`, `RaceLaps`, `RaceOpp` | I | 1, 0, 1, 2 | [BYTES seg6:0x2773 seg6:0x2930 seg6:0x2aee seg6:0x2c3a] | [BYTES seg6:0x3339 seg6:0x3392 seg6:0x3426 seg6:0x33dc] | seg6:0x23d4 selects each value in a combo box with `CB_SETCURSEL` [BYTES seg6:0x2859 seg6:0x285e] |
| `Duelers`, `DuelDiff`, `DuelOpp` | I | 0, 0, 2 | [BYTES seg6:0x2851 seg6:0x2a0f seg6:0x2d7f seg6:0x34ca] | [BYTES seg6:0x351d seg6:0x3569 seg6:0x35b3] | the same for the duel combo boxes; seg6:0x34ca stores `DuelDiff` in DGROUP:0x94a8 |
| `DRacers`, `DDiff`, `DLaps` | I | 0 | [BYTES seg6:0x3208 seg6:0x3262 seg6:0x327e] [BYTES ISPACE.DLL seg10:0x3809 seg10:0x376a seg10:0x395f] | [BYTES ISPACE.DLL seg10:0x3d65 seg10:0x3da2 seg10:0x3d28] | `DDiff` → DGROUP:0x94a8 |
| `DemoType` | I | 519 | [BYTES seg15:0x5aa6] [BYTES ISPACE.DLL seg10:0x3670] | [BYTES ISPACE.DLL seg10:0x3df4] | → DGROUP:0x95ba |
| `DScale` | I | 0 | [BYTES seg9:0x236b] [BYTES ISPACE.DLL seg10:0x3510] | [BYTES ISPACE.DLL seg10:0x3ceb] | — |
| `Track` | S 0x6d or 20 | "" | [BYTES seg6:0x3c8a] [BYTES ISPACE.DLL seg10:0x36d5] | [BYTES ISPACE.DLL seg10:0x3e43] | → DGROUP:0x6990, whose 100 bytes the 0x6d-byte read at seg6:0x3c84 exceeds ([nav.md](nav.md#buffers)) |
| `Ship1`, `Ship2` | S 20 | "" | [BYTES seg14:0x236c seg14:0x239c seg15:0x5af0] [BYTES ISPACE.DLL seg10:0x399a seg10:0x39b7] | [BYTES ISPACE.DLL seg10:0x3e90] | SHP names; seg15:0x5962 reads `Ship1` with `DemoType` |
| `Tree` | I | 0 | [BYTES seg6:0x37ef] | — | → DGROUP:0x94bc ([nav.md](nav.md#tree-ini-key)) |

- ISPACE.DLL seg10:0x33e4 reads `DScale`, `DemoType`, `Track`, `DDiff`, `DRacers`, `DLaps`, `Ship1` and `Ship2` together, and ISPACE.DLL seg10:0x3c9c writes them. [BYTES ISPACE.DLL seg10:0x3510 seg10:0x399a seg10:0x3ceb seg10:0x3e90]
- ISPACE.DLL seg10:0x250c writes `MainShip`, `Drive`, `Size`, `SetSize`, `ThePlayer` and `PlayMode`: the keys the oracle saw New Game OK write. [BYTES ISPACE.DLL seg10:0x259f seg10:0x25f7 seg10:0x2646 seg10:0x265b seg10:0x2687 seg10:0x26c8] [ORACLE newgame-0001]
- Meanings given only as "→" or "—" are [OPEN].
- newgame-0001 logs three reads with an empty key, `Th` and the byte 0x1d. The first falls between `Racers` and `RaceDiff`, where the code reads `Duelers` at seg6:0x2851. Why the relay showed those names is [OPEN]. [ORACLE newgame-0001] [BYTES seg6:0x2851]

### INSPACE.INI [Teams]
- `Team1`…`Team8` are team names, at most 19 characters. INSPACE takes the section name from ENG_LANG string 175 `Teams`. [BYTES seg13:0x21cf seg13:0x21fa] [BYTES ENG_LANG.DLL STRING 11] id 175 [BYTES ISPACE.DLL seg10:0x4243]
- The Factory's team list reads them with the default `Mystery Team`. [BYTES FACTORY.EXE seg5:0x25b6] [BYTES theworks/FACTORY.EXE seg5:0x293e]
- The shipped INI names the eight teams. [BYTES INSPACE.INI]

### INSPACE.INI [Ship Factory]
Only the two Factory builds use this section. The window keys sit at the same addresses in both builds. [BYTES FACTORY.EXE seg4:0x0 seg4:0x246] [BYTES theworks/FACTORY.EXE seg4:0x0 seg4:0x246]

| Key | Type | Default | Read | Written | Meaning |
|---|---|---|---|---|---|
| `Ship` | S 20 | "" | [BYTES FACTORY.EXE seg2:0x5a3] | [BYTES FACTORY.EXE seg4:0x223f] [BYTES theworks/FACTORY.EXE seg4:0x236f] | name of the ship being edited (FACTORY.EXE DGROUP:0xf9a) |
| `ShowLeft`, `ShowTop` | I | 20 | [BYTES FACTORY.EXE seg4:0x18 seg4:0x2e] | [BYTES FACTORY.EXE seg4:0x306 seg4:0x32d] | Factory window position |
| `BodyLeft`, `BodyTop`, `BehavLeft`, `BehavTop`, `HandLeft`, `HandTop` | I | 50 | [BYTES FACTORY.EXE seg4:0x86 seg4:0x9c seg4:0x10a seg4:0x120 seg4:0x178 seg4:0x18e] | [BYTES FACTORY.EXE seg4:0x35b seg4:0x382 seg4:0x3b0 seg4:0x3d7 seg4:0x405 seg4:0x42c] | positions of the body, behaviour and handling windows |
| `ShowTeam`, `FrameShow`, `RotateBigShip` | I | 0 | [BYTES FACTORY.EXE seg4:0xf4 seg4:0x1fc seg4:0x237] | [BYTES FACTORY.EXE seg4:0x2d8 seg4:0x2aa seg4:0x27c] | display switches |
| `BitmapEditor` | S 0x82 | "" | [BYTES theworks/FACTORY.EXE seg2:0x1255] | [BYTES theworks/FACTORY.EXE seg2:0x12a8] | editor program for ship bitmaps; when empty the Enhanced Factory explains the option, writes `PBRUSH.EXE` and runs that [MANUAL Editing Ship Bitmaps] |

- The Factory also reads `[Inner Space]` `Path`, `SndPath`, `ShipPath`, `Language`, `GameSpeed`, `Calibrate`, `FrameRateSet` and `Quick` (table above). [BYTES FACTORY.EXE seg4:0x450 seg4:0x59c seg4:0x3b00]

### WIN.INI, SYSTEM.INI and other files

| File | Section | Key | Type, default | Read | Written |
|---|---|---|---|---|---|
| WIN.INI | Inner Space | `CDDrive` | S 40, "" | [BYTES seg10:0x88db seg15:0x6fda] [BYTES ISPACE.DLL seg10:0x6146] | — |
| WIN.INI | windows | `PDev` | I, 0 | [BYTES seg10:0x8a52] [BYTES ISPACE.DLL seg10:0x5107 seg10:0x5227 seg10:0x5cdb] | [BYTES ISPACE.DLL seg10:0x5d2e] |
| SYSTEM.INI | 386Enh | `vidsp` | I, 0 | [BYTES seg10:0x8a26] [BYTES ISPACE.DLL seg10:0x50f4 seg10:0x5214 seg10:0x5c81] | [BYTES ISPACE.DLL seg10:0x5cc7] |
| SYSTEM.INI | 386Enh | `32BitSP` | I, 0 | [BYTES seg10:0x8a3f] [BYTES ISPACE.DLL seg10:0x5d8f] | [BYTES ISPACE.DLL seg10:0x5dc3] |
| SYSTEM.INI | 386Enh | `XCal123` | I, 0 | [BYTES ISPACE.DLL seg10:0x5bf7] | [BYTES ISPACE.DLL seg10:0x5c35] |
| SYSTEM.INI | Boot.description | `display.drv` | S 0x5a, `Video Card` | [BYTES ISPACE.DLL seg8:0x5cd5] | — |
| SYSTEM.INI | drivers | `Wave` | S 40, "" | [BYTES ISPACE.DLL seg8:0x5d30] | — |
| CONTROL.INI | drivers.desc | the `Wave` value | S 0x5a, `Sound Card` | [BYTES ISPACE.DLL seg8:0x5d6b] | — |
| PROTOCOL.INI | network.setup | `netcard` | S 40, "" | [BYTES ISPACE.DLL seg8:0x5e51] | — |
| NETWORK.INF (system directory) | netcard | the `netcard` value | S 0x5a, "" | [BYTES ISPACE.DLL seg8:0x5eb2] | — |

- ISPACE.DLL seg10:0x5b24 writes `XCal123`, `vidsp`, `PDev` and `32BitSP` together with `[Inner Space]` `Accept`; [Resources rewritten in place](#resources-rewritten-in-place) gives what the values encode. [BYTES ISPACE.DLL seg10:0x5b24 seg10:0x5ded]
- ISPACE.DLL seg8:0x5c86 collects the video, sound and network descriptions into ISPACE.DLL DGROUP:0x1b34, 0x1b8e and 0x1be8. [BYTES ISPACE.DLL seg8:0x5cd5 seg8:0x5d6b seg8:0x5eb2] Their use is [OPEN].
- The oracle runs show these files under `C:\windows`. [ORACLE newgame-0001]
- INSTALL.EXE also writes SYSTEM.INI [drivers] `MSACM.imaadpcm` and, for the screen saver, SYSTEM.INI [boot] `SCRNSAVE.EXE`, and reads AD_PREFS.INI [After Dark] `Path`; its INSPACE.INI writes are in [install.md](install.md). [BYTES INSTALL.EXE seg2:0x13bb seg2:0x1a18 seg2:0x1a3d]

## Resource records loaded as data
This section covers what the loading code shows. [resources.md](resources.md) holds the inventory and the raw bytes. [DESIGN]

### ISPACE.DLL #5600 135
- seg10:0x38e0 loads it from ISPACE.DLL (handle DGROUP:0x9210) and copies its 64 words row by row into the 8 × 8 word table at DGROUP:0x3824. [BYTES seg10:0x38f6 seg10:0x38fb seg10:0x38ff seg10:0x3947 seg10:0x3963]
- The table is saved in the game header at +0x36, so the resource gives the initial values of per-game state. [BYTES seg6:0x91dd] seg11:0x7902 calls the loader at seg11:0x797a and seg11:0x7a99. [BYTES seg11:0x797a seg11:0x7a99]
- What the rows and columns index, for example the eight teams, is [OPEN].

### ISPACE.DLL #5500 100
- ISPACE.DLL seg5:0x254c copies the first 0x114 bytes (92 three-byte entries) of the resource ([resources.md](resources.md)) into a new global block behind ISPACE.DLL DGROUP:0x115c. [BYTES ISPACE.DLL seg5:0x2566 seg5:0x2569 seg5:0x25b1 seg5:0x25c5 seg5:0x25d9]
- The copy stops at 0x114. ISPACE.DLL aligns its resources to 16 bytes, which rounds 0x114 up to the resource's 288 bytes, and the 12 bytes after 0x114 are zero. [BYTES ISPACE.DLL NE resource table] [BYTES ISPACE.DLL 5500 100]
- ISPACE.DLL seg5:0x2626 copies the 92 entries into palette entries 11–102 of the LOGPALETTE at far pointer ISPACE.DLL DGROUP:0x116c + 4·n, byte 0 to peRed, byte 1 to peGreen and byte 2 to peBlue, and seg5:0x25ec then frees the block. seg5:0x3619 writes the table's palVersion 0x300 and seg5:0x38e0 passes it to CreatePalette. [BYTES ISPACE.DLL seg5:0x263a seg5:0x265e seg5:0x2674 seg5:0x2688 seg5:0x2692 seg5:0x3619 seg5:0x38e0 seg5:0x467d seg5:0x4683]

### IGRAPHIC.DLL #6600
- ISPACE.DLL seg8:0x5b76 loads `#6600 <id>` from the module whose handle is ISPACE.DLL DGROUP:0x692. Only IGRAPHIC.DLL has this type. [BYTES ISPACE.DLL seg8:0x5b9a seg8:0x5bad] [BYTES IGRAPHIC.DLL 6600]
- It copies the first 0x6c bytes into a 0x6c-byte block (the header, far pointer ISPACE.DLL DGROUP:0xd10). [BYTES ISPACE.DLL seg8:0x5bec seg8:0x5c12 seg8:0x5c24]
- It then copies the rest, starting at offset `GlobalSize` of that block, into a second block (the body, far pointer ISPACE.DLL DGROUP:0xd50). [BYTES ISPACE.DLL seg8:0x5c07 seg8:0x5c3e seg8:0x5c6f]
  - With 32-byte rounding the body starts at 0x80. Bytes 0x6c–0x7f are zero in all five resources. [BYTES IGRAPHIC.DLL 6600]
  - The body block is `GlobalAlloc(0x42, n − GlobalSize(header))`, where n is the low word of `SizeofResource` (ISPACE.DLL DGROUP:0x1cd2). It is zero-filled, so records that reach into its own `GlobalSize` rounding count too. [BYTES ISPACE.DLL seg8:0x5bad seg8:0x5bb2 seg8:0x5c2c seg8:0x5c31 seg8:0x5c33 seg8:0x5c3e]
- ISPACE ordinal 32 ([ispace.md](ispace.md)) loads the record when its id argument is not −1 and treats the body as 0x26-byte records, count = `GlobalSize(body)` / 0x26. [BYTES ISPACE.DLL seg8:0x3a seg8:0x44 seg8:0x72 seg8:0x78]
- Header fields that ordinal 32 reads and writes:
  - Bytes +0x50 and +0x51 select one of three modes each (6, 7 or 8) for the dwords +0x5c and +0x60. [BYTES ISPACE.DLL seg8:0xd0 seg8:0xe1 seg8:0x109 seg8:0x11b seg8:0x154]
  - When either of two word arguments is non-zero, ordinal 32 overwrites both dwords +0x5c and +0x60 with the two arguments sign-extended, so a zero argument writes 0. [BYTES ISPACE.DLL seg8:0x87 seg8:0x8d seg8:0x96 seg8:0x9b seg8:0xa6 seg8:0xab]
  - When the body holds more than two records and body byte +0x6b is 0xec, it adds an argument to body word +0x51. [BYTES ISPACE.DLL seg8:0xb3 seg8:0xbe seg8:0xc8]
  - Dwords +0x5c, +0x60, +0x64 and +0x68 feed the projection constants of ordinal 32 ([ispace.md](ispace.md#ordinal-notes)); scene 100 holds 500, 217, 1000 and 0. [BYTES ISPACE.DLL seg8:0xd0 seg8:0x11b seg8:0x162 seg8:0x18c] [BYTES IGRAPHIC.DLL 6600 100]
- INSPACE passes ids 100 (seg15:0xd9b, seg15:0x3405), 105 (seg15:0x31fd), 110 (seg15:0x3284), 115 (seg15:0x903) and 120 (seg15:0x332b) through seg10:0x9526, which places them in ordinal 32's id argument. [BYTES seg15:0x903 seg15:0x911 seg15:0xd9b seg15:0xda9 seg15:0x31fd seg15:0x3284 seg15:0x332b seg15:0x3405 seg10:0x9553 seg10:0x955d]
- **Header colours.** Words +0x00–+0x11 hold three RGB colours, +0x24 the colour count, +0x26 one divisor per colour and +0x2c–+0x3d three dark colours; ordinal 32 builds one shade ramp per colour from them ([ispace.md](ispace.md#ordinal-notes), ordinal 32). Scene 100 has the colours (255, 255, 255), (0, 224, 0) and (220, 202, 70), the count 3, the divisors 4, 4 and 8 and the dark colours (32, 32, 32), (0, 100, 0) and (120, 108, 38). [BYTES ISPACE.DLL seg8:0x14e2 seg8:0x1512 seg8:0x1537] [BYTES IGRAPHIC.DLL 6600]
  - A record whose slot byte v is below −1 draws only when header byte 0x48 − v names a condition that holds ([ispace.md](ispace.md#ordinal-notes), ordinal 32): v = −10 reads +0x52 (3 in scene 100), v = −11 reads +0x53 (5) and v = −20 reads +0x5c (0xf4, the low byte of the dword 500), which matches no case. [BYTES ISPACE.DLL seg8:0x1641] [BYTES IGRAPHIC.DLL 6600]
- **Records.** The 0x26-byte records start at offset 0x80; scene 100 has 62 and its record 61 is all zero. These are the fields the draw loop uses: [BYTES ISPACE.DLL seg8:0x1afb seg8:0x1b31 seg8:0x1b38 seg8:0x28de] [BYTES IGRAPHIC.DLL 6600]

  | Offset | Size | Field |
  |---|---|---|
  | +0x00 | byte | resolved flag of the layout solver; record 0 has it set |
  | +0x01, +0x03, +0x05 | word | X, Y (depth), Z (height) |
  | +0x07, +0x09, +0x0b | word | SX, SY, SZ; the end point of a line |
  | +0x0d, +0x0f, +0x11 | word | position codes per axis |
  | +0x13, +0x15, +0x17 | word | size codes per axis (a line's end-point codes) |
  | +0x19 | word | string id, type 1 only: a value above 1 is loaded with LoadString; −13, −12, −11 and −10 copy the CPU, display-driver, sound-card and net-card texts |
  | +0x1b, +0x1c | byte | parent record and second parent (the end point of a line) |
  | +0x1d | signed byte | colour index, −1 for none |
  | +0x1e | byte | primitive: 1 box (ISPACE.DLL seg8:0x28de), 2 line (seg8:0x5470), 3 seg8:0x3874, 4 seg8:0x50ec, 5 seg8:0x4206 |
  | +0x1f | signed byte | type 1 only: output slot when ≥ 0, condition when below −1 |
  | +0x20 | byte | brightness: the shade level is the colour's peak channel × value / 50 |
  | +0x21 | byte | type 1 only: 100 takes a pattern brush from BITMAP METAL, 120 one from graphic 76 |

  - Scene 100's slots are record 42 (slot 0, the monitor screen), 17 (1, the PC tower front), 5 (2), 18 (3) and 60 (4, colour −1, the meal spot). Its conditional records are 36 and 38 (v = −11) and 37 and 39 (v = −10). Every word +0x19 and every byte +0x21–+0x25 is 0, so scene 100 draws no text and loads no bitmap. [BYTES IGRAPHIC.DLL 6600]
  - The layout solver seg8:0xa36 that reads +0x00–+0x17 is in [ispace.md](ispace.md#ordinal-notes) (ordinal 32); it never reads the codes at +0x0f and +0x15. [BYTES ISPACE.DLL seg8:0xa36 seg8:0xa57 seg8:0xdaa]
  - The text path of scenes 105–120 through ISPACE.DLL seg6:0x7f52 is [OPEN]. [BYTES ISPACE.DLL seg6:0x7f52]

### ISPACE.DLL #5000 150
- No `FindResource` in INSPACE.EXE or ISPACE.DLL names this resource. Its reader is [OPEN]. [BYTES ISPACE.DLL 5000 150]

### Resources rewritten in place
ISPACE.DLL has its own NE resource walker, ISPACE.DLL seg10:0x4696. It reads the MZ header, follows `e_lfanew` to the NE header and walks the resource table for a type and id. It then reads the resource, or writes it with `_lwrite` when its last argument is non-zero. [BYTES ISPACE.DLL seg10:0x46b9 seg10:0x46d5 seg10:0x46f0 seg10:0x4813 seg10:0x47dc]

- ISPACE.DLL seg10:0x4666 (read) and ISPACE.DLL seg10:0x4636 (write) wrap it. [BYTES ISPACE.DLL seg10:0x4643 seg10:0x4673]
- Through them ISPACE.DLL seg10:0x4c68 reads and writes 50 bytes of INSPACE.EXE #5000 143, passing them through ISPACE.DLL seg10:0x4dba. [BYTES ISPACE.DLL seg10:0x4d1a seg10:0x4d26 seg10:0x4d45]
- Further callers read or write #5500 resources. [BYTES ISPACE.DLL seg10:0x4c1b seg10:0x4f0b seg10:0x5037 seg10:0x5e6b seg10:0x5325 seg10:0x53df seg10:0x5461 seg10:0x557f]
  - #5500 110 of INSPACE.EXE: ISPACE.DLL seg10:0x4b1a, ISPACE.DLL seg10:0x4e5a and ISPACE.DLL seg10:0x4f9c. [BYTES ISPACE.DLL seg10:0x4b76 seg10:0x4ef9 seg10:0x5010]
  - #5500 110 of IGRAPHIC.DLL: ISPACE.DLL seg10:0x5dfe. [BYTES ISPACE.DLL seg10:0x5e59]
  - #5500 120 and #5500 110 of INSTALL.EXE: ISPACE.DLL seg10:0x5266 and ISPACE.DLL seg10:0x5416. [BYTES ISPACE.DLL seg10:0x5289 seg10:0x543a]
- **INSPACE.EXE #5500 110, the install record.** 10 bytes: the dword D at +0, the install drive at +4 (a word, 1 for A:) and zero bytes +6–+9. The game record is all zero. [BYTES ISPACE.DLL seg10:0x4b99 seg10:0x4bee seg10:0x5050] [BYTES INSPACE.EXE 5500 110]
  - Its writer ISPACE.DLL seg10:0x4b1a (ordinal 24 commands 0, 11 and 13) sets the drive to the first byte of INI `LocalPath` − 0x40 and D to seg10:0x4440(drive), the drive's volume serial, or to rand() % 7651 + 5432 (sign-extended) when that is 0, and caches D in ISPACE.DLL DGROUP:0x14e8. It writes the block with seg10:0x4636 and then calls the INI writer seg10:0x5b24(0x17, x), or (0x38, x) for command 13, with x = (D & 0xFFFF) mod 653 + 654 (unsigned). [BYTES ISPACE.DLL seg10:0x4b99 seg10:0x4ba7 seg10:0x4bb8 seg10:0x4bcb seg10:0x4bd8 seg10:0x4bee seg10:0x4c0e seg10:0x4c10 seg10:0x4c26 seg10:0x4c5a]
  - The installer writes it at file offset 0x4b840 and keeps the file's DOS date and time. Under Wine seg10:0x4440 returns 0 ([ispace.md](ispace.md#ordinal-notes), ordinal 24), so D is the random value. [ORACLE install-0001]
- **INSPACE.EXE #5000 143, the name record.** 50 bytes. The installer fills them with Name (GetWindowText, at most 0x12 bytes) + " - " + Organization (at most 0x16), and ordinal 24 command 9 scrambles them with ISPACE.DLL seg10:0x4dba(buf, 50, 1) and writes all 50, bytes after the NUL included, keeping the file's date and time. The game record holds "Unregistered Copy   ILLEGAL" in clear. [BYTES ISPACE.DLL seg10:0x4d0c seg10:0x4d26] [BYTES INSPACE.EXE 5000 143] [ORACLE install-0001]
  - seg10:0x4dba(buf, n, mode) calls srand(GetCurrentTime()) in both modes and gives byte i the key {0x2A, 0x32, 0x2D}[i mod 3]. Mode 1 (scramble) turns a byte that is ≤ 0 as a signed value into −(rand() % 96 + 2) and XORs every other byte with the key. Mode 0 (descramble) turns a byte ≥ 0x80 into 0 and XORs every other byte with the key. So a NUL comes back as 0, and name bytes ≥ 0x80 are lost. [BYTES ISPACE.DLL seg10:0x4dc9 seg10:0x4ddb seg10:0x4deb seg10:0x4e13 seg10:0x4e1b seg10:0x4e20]
  - Command 10 (seg10:0x4c68 in mode 0) reads the 50 bytes from <LocalPath>INSPACE.EXE, descrambles them and returns 1 when the resource is found. When decoded byte 0, 1 or 2 is 0 it returns 0 and moves bytes 3–49 down to 0–46. INSPACE seg10:0x946a calls it and stores 0 at +0x30, so the name shown is at most 48 characters. [BYTES ISPACE.DLL seg10:0x4d45 seg10:0x4d5a seg10:0x4d63 seg10:0x4d77 seg10:0x4d82] [BYTES seg10:0x9482 seg10:0x948c]
- **The INI values.** ISPACE.DLL seg10:0x5b24(mode, x) writes SYSTEM.INI [386Enh] `vidsp` and `32BitSP` and WIN.INI [windows] `PDev` ([INI files](#winini-systemini-and-other-files)). It calls srand(GetCurrentTime()) and draws s = rand() % 1111 + rand() % 8999 + 1987. [BYTES ISPACE.DLL seg10:0x5b39 seg10:0x5b4a seg10:0x5b57 seg10:0x5b5f seg10:0x5b95 seg10:0x5bf7 seg10:0x5c35 seg10:0x5c41 seg10:0x5c4c seg10:0x5cc7 seg10:0x5d2e seg10:0x5d4a seg10:0x5d6e seg10:0x5dc3 seg10:0x5ded]
  - The new PDev is x + s for modes 0x17 and 0x38, (x with its low byte XORed with 0xFF) + s for 0x54, 0x5b and 0xbe, and s − rand() % 200 − 199 for 0x75.
  - vidsp is written as s when its stored value is 0 or the mode is 0x17, 0x38, 0x75 or 0x5b; otherwise s takes the stored value. PDev is written when its stored value is 0 or the mode is 0x17, 0x38, 0x75, 0x9d or 0x5b, mode 0x9d writing the negated stored value; otherwise the new PDev takes the stored value.
  - With X = the low word of 2s − PDev − 1231, 32BitSP becomes X for mode 0x5b and otherwise rand(), minus 1 when that equals X; mode 0x54 keeps a non-zero stored value. Mode 0x5b also writes INSPACE.INI [Inner Space] `Accept` = 1.
  - Mode 0xbe first reads SYSTEM.INI [386Enh] `XCal123`. When it is 0 it writes rand() % 6777 + 3913 there and goes on as mode 0x5b; otherwise it goes on as mode 0x54 and returns 0.
- **What an install leaves.** After modes 0x17 and 0x38, vidsp = s lies in 1987–12095, PDev − vidsp = (D & 0xFFFF) mod 653 + 654 lies in 654–1306, and 32BitSP is a random value other than 2·vidsp − PDev − 1231. Ordinal 24 command 2 then accepts the record through the PDev − vidsp relation when seg10:0x4440 returns 0, and seg10:0x885e never reaches its 32BitSP test ([shell.md](shell.md#intro-and-title-screen)). Two installs give different values because s and D are random, not because of the date or the name. [BYTES ISPACE.DLL seg10:0x4c10 seg10:0x5b95 seg10:0x5252] [BYTES seg10:0x8983] [ORACLE install-0001]
- **The 35-day code.** The Order dialog's Register button calls seg10:0x5b24(0x5b, day), with day the date number of the day the code is entered ([shell.md](shell.md#game-start-dialogs)). That leaves PDev − vidsp = day with its low byte inverted, 32BitSP = 2·vidsp − PDev − 1231 and Accept = 1. With the game's all-zero #5500 110, command 2 returns 3, and seg10:0x885e's day block counts the days since the code. [BYTES ISPACE.DLL seg10:0x8c3 seg10:0x5c41 seg10:0x5d69] [BYTES seg10:0x8a91 seg10:0x8ab2]
- **IGRAPHIC.DLL #5500 110, the Order serial.** ISPACE.DLL seg10:0x5dfe reads and writes 10 bytes of <LocalPath>IGRAPHIC.DLL #5500 110 (ISPACE.DLL DGROUP:0x2056 + "IGRAPHIC.DLL", seg10:0x4666 and seg10:0x4636 with type 5500 and id 110): the dword at +0 = seg10:0x4440(drive), the volume serial (0 under WF_WINNT); a counter word at +4; and the serial dword at +6. The ship-disk resource is 32 zero bytes. How the Order dialog uses it is in [shell.md](shell.md#game-start-dialogs). [BYTES ISPACE.DLL seg10:0x5e17 seg10:0x5e4d seg10:0x5e59 seg10:0x5e8c seg10:0x5eef seg10:0x5f1c] [BYTES IGRAPHIC.DLL 5500 110]
- **INSTALL.EXE #5500 120 and #5500 110, the install-set records.** Ordinal 24 command 1 (ISPACE.DLL seg10:0x5416) reads 0x78 bytes of the source INSTALL.EXE's #5500 120 (a serial dword at +0, a drive word at +4 and the previous install directory as a string at +0xa) and 0x10 bytes of #5500 110 (a string at +4). Both are all zero in the game (32 and 128 bytes), so command 1 answers a new install. INSTALL.EXE issues command 0 after personalization, which writes INSPACE.EXE #5500 110 above; how it uses the answers is in [install.md](install.md). [BYTES ISPACE.DLL seg10:0x5416 seg10:0x546c seg10:0x558a] [BYTES INSTALL.EXE 5500 110] [BYTES INSTALL.EXE 5500 120] [BYTES INSTALL.EXE seg2:0x4433]
- The port reads the user's files and never writes them. [DESIGN]
