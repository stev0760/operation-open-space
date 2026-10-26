# Data-file formats

This page describes the files the game reads and writes at run time. They are the ship definitions, saved games, the score list, race records, the INI keys and the resource records that code loads as data. The page owns the byte layouts and the meaning of each field. [DESIGN]

Related pages: the static resource inventory is in [resources.md](resources.md) and the in-memory ship and pool records in [structs.md](structs.md). The timing keys are in [timing.md](timing.md) and the ISPACE.DLL exports in [ispace.md](ispace.md). [DESIGN]

Addresses without a module name are INSPACE.EXE, whose DGROUP is seg19. ISPACE.DLL's DGROUP is seg11 and both FACTORY.EXE builds have DGROUP seg6. `FACTORY.EXE` means the shipdisk Ship Factory 1.29.02 and `theworks/FACTORY.EXE` the Enhanced Ship Factory 1.49.00. [DESIGN] [BYTES FACTORY.EXE] [BYTES theworks/FACTORY.EXE]

## Conventions
- Numbers are little-endian. A word is 16 bits and a dword 32 bits. A string field is a fixed-size cp1252 buffer holding a NUL-terminated string. [DESIGN]
- **Block writers write `GlobalSize` bytes.** The MASTER.BOD and MASTER.SHP writer and the saved-game writer copy a moveable, zero-filled global block (`GlobalAlloc(0x42, n)`) to the file with a length of `GlobalSize(h)`, not `n`. [BYTES FACTORY.EXE seg4:0x26d4 seg4:0x26dc] [BYTES seg6:0x8aec seg6:0x8afa seg6:0x8b05 seg6:0x8b1e seg6:0x8b2f seg6:0x8b48]
  - Wine's krnl386 rounds every block of up to 1 MB up to a multiple of 32 bytes, and `GlobalSize` returns the rounded size. [REF Wine 11.0 dlls/krnl386.exe16/global.c GLOBAL_Alloc] The granularity of Windows 3.x is [OPEN].
  - The shipped MASTER.BOD (52 × 44 = 2,288 bytes of records, 2,304 = 72 × 32 bytes in all) and MASTER.SHP (118 × 65 = 7,670 bytes of records, 7,680 = 240 × 32 bytes in all) end with the padding that 32-byte rounding gives. Rounding to 16 bytes would leave MASTER.BOD at 2,288 bytes. [BYTES MASTER.BOD] [BYTES MASTER.SHP]
- **Readers fail on a short read.** The INSPACE read helper seg14:0x4006 reads in chunks of at most 0x7fff bytes. It returns 0 as soon as `_lread` delivers fewer bytes than asked. [BYTES seg14:0x403f seg14:0x4044 seg14:0x4049]

## File locations

| File | Directory | Name | Read by | Written by |
|---|---|---|---|---|
| MASTER.BOD, MASTER.SHP | INI `ShipPath`, with `\` appended when missing | fixed | seg14:0x3db6, seg14:0x3eda; ISPACE.DLL seg10:0x3f70, ISPACE.DLL seg10:0x406a; FACTORY.EXE seg4:0x277c, FACTORY.EXE seg4:0x283e | FACTORY.EXE seg4:0x24fa, FACTORY.EXE seg4:0x2316 |
| `*.sav` | `LocalPath` + `Games` + `\` | INI `Restore`, `lastgame` or `demogame` | seg6:0x930a; ISPACE.DLL seg10:0x1a80 | seg6:0x885c |
| `*.ded`, `*.don` | `LocalPath` + `Dead` + `\` | INI `Restore` or the pilot name, plus a counter when taken | seg6:0x6576 (score list) | seg6:0x885c |
| `<course>.rac` | INI `RacePath`, with `\` appended when missing | `LAZYLOOP`, `SDISPEED`, `GRANDPRI` | seg6:0x8b90 | seg6:0x8dfe |
| `INSPACE.INI` | Windows directory (no path in the name) | fixed | everywhere | everywhere |

[BYTES seg14:0x3e2f seg14:0x3f57 seg6:0x93d1 seg6:0x8a78 seg6:0x8bda seg6:0x8e49 seg6:0x6ef] [BYTES ISPACE.DLL seg10:0x3fdc seg10:0x40da seg10:0x1ba6] [BYTES FACTORY.EXE seg4:0x27b6 seg4:0x2878 seg4:0x26bf]

- seg6:0x6d8 builds the directory strings at start-up. It reads `Path` into DGROUP:0x8f7a and `LocalPath` into DGROUP:0x9078, and appends `\` to `Path` when missing. It reads `Dead` into DGROUP:0x96c5 and `Games` into DGROUP:0x96b8 (each at most 10 characters) and appends `\` to both. It reads `RacePath` into DGROUP:0x900a and appends `\` when missing. [BYTES seg6:0x70a seg6:0x725 seg6:0x978 seg6:0x9a0 seg6:0x9c8]
- The saved-game code prefixes `LocalPath` (DGROUP:0x9078). The one exception is the renaming loop for `.ded`/`.don` files, which prefixes `Path` (DGROUP:0x8f7a). [BYTES seg6:0x88be seg6:0x89f0]
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
| +0x19 | 1 | signed byte | the import dialog stores (bitmap measure − 8). seg14:0x1b84 stores its negation in DGROUP:0x96d6 + 0xad·n. Meaning [OPEN] | [BYTES FACTORY.EXE seg5:0x1ff4 seg5:0x2001] [BYTES seg14:0x1c7f seg14:0x1c86 seg14:0x1c88] |
| +0x1a | 1 | signed byte | the import dialog stores 10. seg14:0x1b84 stores (value << `Size`) / 4 in DGROUP:0x970e + 0xad·n. Meaning [OPEN] | [BYTES FACTORY.EXE seg5:0x2009] [BYTES seg14:0x2070 seg14:0x2077] |
| +0x1b | 1 | signed byte | strength base. seg14:0x1b84 computes base × 10 × (SHP armor / 4 + 100) / 100, stores it in DGROUP:0x9704 + 0xad·n and in word +0x44 of the ship's pool record, and stores that / 22 in DGROUP:0x9702 + 0xad·n and pool word +0x48 | [BYTES seg14:0x1e50 seg14:0x1e68 seg14:0x20bc seg14:0x210a] |
| +0x1c | 1 | signed byte | stored in DGROUP:0x9712 + 0xad·n, which reaches ship-record word +0x0e. Meaning [OPEN] | [BYTES seg14:0x1ca1 seg14:0x1caa seg14:0x21b1] |
| +0x1d | 1 | signed byte | stored in DGROUP:0x96f2 + 0xad·n. seg14:0x1b84 sets ship-record word +0x14 to value × 1200 / (SHP thruster × 5); a ship with a roster record divides by that × (roster byte +0x33 + 10) / 10, and the player's ship then gets ¾ of the result when DGROUP:0x95bc ≠ 0 | [BYTES seg14:0x1d30 seg14:0x1d37 seg14:0x2274 seg14:0x2299 seg14:0x22a7 seg14:0x22cc seg14:0x22e4 seg14:0x22f2] |
| +0x1e | 1 | byte | the import dialog stores 0; no INSPACE reader found [OPEN] | [BYTES FACTORY.EXE seg5:0x2012] |
| +0x1f | 1 | byte | the import dialog stores (bitmap measure − 10). Meaning [OPEN] | [BYTES FACTORY.EXE seg5:0x201b seg5:0x2020] |
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
| 0x01 | 301 Body Thrust | DGROUP:0x9718 + 0xad·n = 1 when the bit is clear [BYTES seg14:0x1f41] |
| 0x02 | 310 Shield | special ability 5 when bits 0x04 and SHP 0x08 are clear; DGROUP:0x9779 + 0xad·n = 1 and sound 5 is passed to seg10:0x754a [BYTES seg14:0x1dbd seg14:0x1dc9 seg14:0x1f9f seg14:0x1fb9] |
| 0x04 | 302 Pull-off Wings | special ability 3; DGROUP:0x9775 + 0xad·n = 6. When clear, the ship-record bytes +0xac and +0xad are zeroed [BYTES seg14:0x1d80 seg14:0x1d8c seg14:0x1f1f seg14:0x1ded seg14:0x1dfb] |
| 0x08 | 309 Thrust Behind Ship | DGROUP:0x971a + 0xad·n = 1 [BYTES seg14:0x1d55 seg14:0x1d5c] |
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
  - 0x10 sets DGROUP:0x9714 to 1.
  - 0x01 sets DGROUP:0x9716 to 0, else 1, and 0x20 then ORs 0x20 into it.
  - 0x08 sets DGROUP:0x971c to 1 and selects special ability 4.
  - 0x04 sets DGROUP:0x971e to 1. [BYTES seg14:0x1eb4 seg14:0x1ed6 seg14:0x1efd seg14:0x1fd6 seg14:0x1ffd seg14:0x1d9c]
- In the Factory, FACTORY.EXE seg4:0x4cb0 sets or clears one bit of FACTORY.EXE DGROUP:0xf99, the edited ship's copy of +0x09. [BYTES FACTORY.EXE seg4:0x46b5 seg4:0x46b9 seg4:0x4ccb seg4:0x4cdc]
  - Bit 0x01 follows combo 180: FACTORY.EXE seg2:0xbe4 sets it when `CB_GETCURSEL` returns a non-zero index and clears it otherwise. [BYTES FACTORY.EXE seg2:0x1047 seg2:0x1100 seg2:0xc0d seg2:0xc1b seg2:0xc55 seg2:0xc5a]
  - Menu item 498 sets bit 0x20 and item 499 clears it. FACTORY.EXE seg2:0xbbb checks item 498 when the bit is set, and item 499 when it is clear. [BYTES FACTORY.EXE seg2:0x10a4 seg2:0x10a9 seg2:0x1285 seg2:0x12b6 seg2:0xbb4 seg2:0xbbb seg2:0xb5d]
  - The items are `&Male` (498) and `&Female` (499) of a `&Gender` menu. The Factory's start-up function FACTORY.EXE seg2:0x0 appends that menu only when the command line is `GENDER` or `gender`. [BYTES FACTORY.EXE seg2:0x4c seg2:0x5d seg2:0x6e seg2:0x80 seg2:0x91 seg2:0xa3]
- Bytes +0x04 and +0x05 are copied to DGROUP:0x9776 and DGROUP:0x9777 + 0xad·n. [BYTES seg14:0x2040]
- In the Factory the sound combo's first 15 entries map to ids 6, 7, 8, 18, 30, 9, 16, 10, 14, 15, 13, 12, 11, 23 and 31. Any later entry is a WAV file: the Factory writes 0xffff and the file name without `.WAV`. [BYTES FACTORY.EXE seg2:0xcae]
- The Factory's strength preview adds Fuel Tank × 2 to the INSPACE formula. [BYTES FACTORY.EXE seg4:0x4748 seg4:0x4758]

## Saved games

### Names and triggers
seg6:0x885c(named, kind) writes a saved game. `named` = 0 uses `lastgame`, or `demogame` when DGROUP:0x95bc = 0. Any other `named` uses the INI `Restore` value (at most 8 characters). `kind` 0 writes `.sav` into the Games directory, 1 `.ded` and 2 `.don` into the Dead directory. [BYTES seg6:0x88d2 seg6:0x88dd seg6:0x8980 seg6:0x898b seg6:0x8906 seg6:0x899f seg6:0x89b0 seg6:0x89bb]

| Call site | Arguments | File |
|---|---|---|
| seg7:0x687, seg9:0x31f0 | (0, 0) | `lastgame.sav` or `demogame.sav` |
| seg6:0x9e56, seg10:0x8657 | (1, 0) | `<Restore>.sav` |
| seg6:0x5e39 | (1, 1) | `<name>.ded` |
| seg16:0x7235, after adding 50,000 to the score | (1, 2) | `<name>.don` |

[BYTES seg7:0x687 seg9:0x31f0 seg6:0x9e56 seg10:0x8657 seg6:0x5e39 seg16:0x7228 seg16:0x7235]

- The manual names LASTGAME (the end of each wave) and DEMOGAME (the demo pilot). [MANUAL High Scores]
- A `.ded` or `.don` file needs a score of at least 1000. [BYTES seg6:0x887a]
- For `.ded`/`.don`, when the fourth character of `Restore` is a space (as in `NEW GAME`, the value read back after New Game OK writes ` NEW GAME` [ORACLE newgame-0001]), the name is the pilot name (DGROUP:0x380c) cut to 7 characters, with spaces turned into NULs. [BYTES seg6:0x8911 seg6:0x891f seg6:0x8947]
- seg6:0x8776 compares the game id of an existing file. If the file belongs to another game, the name becomes the first 5 characters plus a counter 1, 2, … until it is free. [BYTES seg6:0x89dc seg6:0x89f0 seg6:0x89fe]
- With DGROUP:0x920c = 0 and DGROUP:0x95bc ≠ 0 the routine writes nothing. A manual save (`named` ≠ 0, `kind` = 0) then shows string 2324: "Game saving is a handy feature of the full version of Inner Space." [BYTES seg6:0x8887 seg6:0x888e seg6:0x88a3] [BYTES ENG_LANG.DLL STRING 146] id 2324 [ORACLE wave-0001]
- seg6:0x930a loads `LocalPath` + Games + (`Restore`, or `demogame` when DGROUP:0x95bc = 0) + `.sav`. [BYTES seg6:0x9383 seg6:0x9396 seg6:0x93ae seg6:0x93bc]

### Layout
A file is up to three blocks back to back. Each block's length is its `GlobalSize`: the allocation rounded up to 32 bytes under Wine (see [Conventions](#conventions)). [BYTES seg6:0x8aec seg6:0x8b05 seg6:0x8b2f]

| Block | Allocation | Present in | Memory after loading |
|---|---|---|---|
| header | 0x1a9 bytes (0x1c0 in the file under Wine) | all | freed after copying into globals |
| ship roster | (header +0x34 + 1) × 0x7c | all | far pointer DGROUP:0x6e24, handle DGROUP:0x95a4 |
| directory records | header +0x32 × 0x297 | `.sav` only | far pointer DGROUP:0x69f4, handle DGROUP:0x94ae |

[BYTES seg6:0x93e0 seg6:0x9409 seg6:0x9492 seg6:0x9497 seg6:0x94a4 seg6:0x952a seg6:0x9531 seg6:0x953d seg6:0x8b5b]

- The reader allocates each block with the same size, asks `GlobalSize` and reads that many bytes, so it fails on a file without the padding. [BYTES seg6:0x9409 seg6:0x944b seg6:0x94cc seg6:0x950c seg6:0x9565 seg6:0x95a5]
- In memory the roster grows by `GlobalReAlloc` to (n + 1) × 0x7c as ships join, starting from one 0x7c block, and the directory block grows by 0x297 per record. [BYTES seg14:0x256 seg14:0xee5 seg14:0xeeb seg6:0x370a seg6:0x3758 seg6:0x3760]

### Header
seg6:0x8eea fills the header and seg6:0x930a reads it back. "Player ship" fields come from the ship record of DGROUP:0x9646 ([structs.md](structs.md#ship-records-8--0x15a-bytes-at-dgroup0x7090)). [BYTES seg6:0x8eea seg6:0x930a]

| Offset | Size | Content | Evidence |
|---|---|---|---|
| +0x00 | dword | game id: `time()` when the game began (DGROUP:0x939a, set by the new-game routine seg10:0x82a2). The score list and the name check use it | [BYTES seg6:0x90b4 seg6:0x90b7 seg10:0x805d seg10:0x8069 seg10:0x84ea seg10:0x84ee] |
| +0x04 | 20 | pilot name, from DGROUP:0x380c | [BYTES seg6:0x912c] |
| +0x18 | byte ×3 | start date: `tm_mon` (0-based), `tm_mday`, `tm_year` (years since 1900), from DGROUP:0x939e and DGROUP:0x93a0 | [BYTES seg6:0x90c1 seg6:0x90c8 seg6:0x90cf seg10:0x8080 seg10:0x8088 seg10:0x808c seg10:0x84fc seg10:0x84ff] |
| +0x1b | byte ×3 | save date, same encoding, from seg10:0x804c(0) | [BYTES seg6:0x90d5 seg6:0x90e3 seg6:0x90e7 seg6:0x90ee] |
| +0x1e | dword | score (DGROUP:0x1e58) | [BYTES seg6:0x8f93 seg6:0x968a] |
| +0x22 | word | SHP index of the player's roster record | [BYTES seg6:0x903d seg6:0x9043] |
| +0x24 | word | DGROUP:0x91f8 | [BYTES seg6:0x8f5e] |
| +0x26, +0x28 | word | player ship bytes +0x5d and +0x5e, sign-extended | [BYTES seg6:0x8f48 seg6:0x8f57] |
| +0x2a | word | DGROUP:0x94a2: index of the current directory record | [BYTES seg6:0x8f65] |
| +0x2c | word | DGROUP:0x931c: the player's roster index | [BYTES seg6:0x8f6c] |
| +0x2e, +0x30 | word | DGROUP:0x9432, DGROUP:0x964a | [BYTES seg6:0x8f73 seg6:0x8f7a] |
| +0x32 | word | DGROUP:0x94a4: directory record count | [BYTES seg6:0x8f81] |
| +0x34 | word | DGROUP:0x931a: highest roster index | [BYTES seg6:0x8f88] |
| +0x36 | 64 words | the 8 × 8 word table DGROUP:0x3824 (see [ISPACE.DLL #5600 135](#ispacedll-5600-135)) | [BYTES seg6:0x91dd] |
| +0xb6, +0xe6, +0x116 | 24 words each | 8 × 3 word tables DGROUP:0x4eec, DGROUP:0x4fd0, DGROUP:0x6b10 | [BYTES seg6:0x9206 seg6:0x920f seg6:0x9218] |
| +0x146 | 8 words | DGROUP:0x6b40 | [BYTES seg6:0x9236] |
| +0x156, +0x158 | word | DGROUP:0x945c, DGROUP:0x945e | [BYTES seg6:0x924d seg6:0x9255] |
| +0x15a, +0x15c | word | player ship words +0xd2 and +0xd0 | [BYTES seg6:0x8fd6 seg6:0x8fc7] |
| +0x15e | word | 100–103: the average of the player's 3 or 4 words at ship +0x26 banded at 71, 46 and 28. A value below 100 marks an older file: the loader divides roster word +0x02 by 10 | [BYTES seg6:0x9185 seg6:0x9196 seg6:0x91a7 seg6:0x91b3 seg6:0x98ce seg6:0x98e7] |
| +0x160 | word | number of directory records whose byte +0x1d0 is 2 | [BYTES seg6:0x9102 seg6:0x910d] |
| +0x162 | word | player ship dword +0xdc, capped at 0x4fff6, / 10 | [BYTES seg6:0x9011] |
| +0x164 | word | player ship word +0xce | [BYTES seg6:0x9020] |
| +0x166, +0x168 | word | DGROUP:0x93ac, DGROUP:0x94ca | [BYTES seg6:0x9061 seg6:0x9069] |
| +0x16a | dword | rating (DGROUP:0x5078) | [BYTES seg6:0x8fa2 seg6:0x969b] |
| +0x16e | byte | DGROUP:0x9134 (INI `PlayMode`); the loader forces 1 when DGROUP:0x95bc = 0 | [BYTES seg6:0x9051 seg6:0x9727 seg6:0x9737] |
| +0x16f | byte | DGROUP:0x94ac | [BYTES seg6:0x9059] |
| +0x170 | byte | when player ship byte +0xe4 ≠ 0: bit 0 = ship byte +0xac is 0, bit 1 = ship byte +0xad is 0; the Restore Game preview passes it to ISPACE.DLL seg10:0x2aac | [BYTES seg6:0x8efb seg6:0x8f17 seg6:0x8f2d] [BYTES ISPACE.DLL seg10:0x1c49 seg10:0x1c57] |
| +0x171 | byte | INI `Size` (DGROUP:0x9156). A loaded value that differs is written back to the INI | [BYTES seg6:0x8f38 seg6:0x95b8 seg6:0x95e8] |
| +0x172 | byte | player ship word +0x18 | [BYTES seg6:0x902f] |
| +0x173 | byte | DGROUP:0x1318 | [BYTES seg6:0x92aa] |
| +0x174, +0x17e | 10 bytes each | DGROUP:0xa14d, DGROUP:0xa157 | [BYTES seg6:0x9090 seg6:0x909c] |
| +0x188, +0x189 | byte | read into DGROUP:0xa161 and DGROUP:0xa162; the writer leaves them 0 | [BYTES seg6:0x96f7 seg6:0x96ff] |
| +0x18a | byte | DGROUP:0x967c | [BYTES seg6:0x9079] |
| +0x18b | dword | DGROUP:0x5a8, the wave-frame counter of [globals.md](globals.md) | [BYTES seg6:0x8fb3] |
| +0x18f | byte | DGROUP:0x9346 | [BYTES seg6:0x9071] |
| +0x190, +0x191 | byte | DGROUP:0xa1cd / 128 and DGROUP:0xa1cd mod 128 | [BYTES seg6:0x9266 seg6:0x9274] |
| +0x192 | 4 bytes | DGROUP:0xa193–0xa196 | [BYTES seg6:0x9296] |
| +0x196 | byte | DGROUP:0xa1cf | [BYTES seg6:0x927c] |

- The meanings of the fields that only name a DGROUP word are [OPEN].
- Loading resets DGROUP:0x951c and DGROUP:0x104c to 0 and sets DGROUP:0x92e4 to 1. [BYTES seg6:0x98bb seg6:0x98be seg6:0x98c4]

### Ship roster records (0x7c bytes)
structs.md calls the index of one of these the ship's `roster_index`. Before a manual save, seg6:0x99b6 updates the record of each ship that has one, then calls seg6:0x885c(1, 0). [BYTES seg6:0x99b6 seg6:0x9e56]

| Offset | Size | Content | Evidence |
|---|---|---|---|
| +0x00 | word | SHP index. The loader sets it to 1 when it is not below the SHP count, or for record 0 when seg14:0x455c rejects the ship | [BYTES seg6:0x9884 seg6:0x9892 seg6:0x98a4] |
| +0x02 | word | ship dword +0xdc, capped at 0x4fff6, / 10 | [BYTES seg6:0x9b92] |
| +0x04 | word | ship word +0xce | [BYTES seg6:0x9b55] |
| +0x06 | word | ship word +0x82 | [BYTES seg6:0x9ae7] |
| +0x1c | byte | ship word +0x1e / 40 | [BYTES seg6:0x9b00] |
| +0x1d | byte | ship byte +0xcb, a count n | [BYTES seg6:0x9ac9] |
| +0x1e, +0x28 | n bytes each | ship bytes +0xae… and +0xb8… | [BYTES seg6:0x9a9b seg6:0x9a84] |
| +0x33 | byte | seg14:0x1b84 divides ship-record word +0x14 by (value + 10) / 10 for a ship that has a roster record ([MASTER.BOD](#masterbod-ship-bodies) +0x1d) | [BYTES seg14:0x2274 seg14:0x2279 seg14:0x2294 seg14:0x22a7] |
| +0x38 | 4 bytes | ship words +0x26…+0x2c, cut to bytes; +0x38 is 0 when ship byte +0xec is 0. seg18:0x3d6 copies them back into the player's ship (DGROUP:0x9646) only | [BYTES seg6:0x9a39 seg6:0x9a5b] [BYTES seg18:0x4a7 seg18:0x4b6 seg18:0x4c0 seg18:0x4ca] |
| +0x3c + k | byte | for roster record k: ship word +0x62 + 2j / 10, where j is the ship holding record k | [BYTES seg6:0x9a02] |
| +0x64 | byte | the loader turns the value 7 into 0 | [BYTES seg6:0x986d seg6:0x9874] |
| +0x72, +0x73 | byte | ship bytes +0xac and +0xad | [BYTES seg6:0x9b33 seg6:0x9b44] |
| +0x74, +0x75 | byte | ship byte +0x24 and the low byte of ship word +0x0a | [BYTES seg6:0x9b11 seg6:0x9b22] |
| +0x76 | byte | class 1–10 of a value derived from ship dword +0xdc and word +0xd8, banded [0, 28), [28, 34), [34, 40), [40, 48), [48, 55), [55, 63), [63, 70), [70, 80), [80, 97), [97, 114) by the tables DGROUP:0x952 and DGROUP:0x96c | [BYTES seg6:0x9bd4 seg6:0x9bda seg6:0x9bf7] [BYTES DGROUP:0x952] |

- The derivation of the +0x76 value through the runtime's long-arithmetic helpers, and the meanings of these fields, are [OPEN].

### Directory records (0x297 bytes)
There is one record per directory the player has entered. DGROUP:0x94a2 indexes the current one. [BYTES seg6:0x36ce seg6:0x36f3]

| Offset | Size | Content | Evidence |
|---|---|---|---|
| +0xa0, +0xa1 | byte | seg6:0x99b6 counts pool-3 records with word +0x18 = 0 and word +0x44 = 350, and the same for 1400 | [BYTES seg6:0x9d20 seg6:0x9d30 seg6:0x9d75 seg6:0x9d92 seg6:0x9dbc] |
| +0xa3 | byte | the same count for pool-9 records with word +0x18 = 0 | [BYTES seg6:0x9d10 seg6:0x9d41 seg6:0x9d56] |
| +0xad | byte | the loader sets it to 1 in the current record when 0 | [BYTES seg6:0x9645] |
| +0xae | words | one per pool-8 record: word +0x1e, negated when that record's entry in the 0x34-byte table at DGROUP:0x3d5a holds −3 | [BYTES seg6:0x9dd7 seg6:0x9df2 seg6:0x9dfb seg6:0x9e23] |
| +0xfe | words | the matching word +0x24 of that table entry | [BYTES seg6:0x9e2c seg6:0x9e41] |
| +0x152 | string | directory path; a new record copies it from DGROUP:0x6990 and a lookup compares it with DGROUP:0x6990 | [BYTES seg6:0x36e4 seg6:0x377f] |
| +0x1c0 | 8 bytes | roster indices present in the directory, 0xff = free | [BYTES seg6:0x9c53 seg6:0x9cc4 seg6:0x9cd5] |
| +0x1d0 | byte | 2 counts towards header +0x160 | [BYTES seg6:0x9102] |

- The meanings of these fields and the rest of the record are [OPEN].

## Score list
seg6:0x6576 builds the "Champions of INNER SPACE" list from the saved games. [BYTES seg6:0x6576] [MANUAL High Scores] The title is string 925. [BYTES seg6:0x6318] [BYTES ENG_LANG.DLL STRING 58] id 925

- It scans `Games\*.sav`, `Dead\*.ded` and `Dead\*.don` under `LocalPath` and reads the first 0x26 bytes of each file's header. [BYTES seg6:0x6864 seg6:0x66a8 seg6:0x669c seg6:0x6759 seg6:0x676d]
- Each entry is 0x31 bytes: header +0x00…+0x25, the file name without its extension at +0x26 (10 bytes) and a status byte at +0x30. [BYTES seg6:0x6718]
  - Status 0 is a `.sav` file, shown as string 2105 ALIVE. Status 1 is `.ded`, DEAD, and 2 is `.don`, DONE!. [BYTES seg6:0x7019] [BYTES ENG_LANG.DLL STRING 132] ids 2105–2107
- Entry 0 is the running game, built from the globals with file name `--`. A saved file of the same game id whose score is not higher is dropped. [BYTES seg6:0x65bc seg6:0x6663 seg6:0x678d seg6:0x67a0 seg6:0x67b4]
- Entries are ranked by the score dword +0x1e, highest first, and only the best entry of each game id is kept. The running game's rank goes to DGROUP:0x93a2. [BYTES seg6:0x6a35 seg6:0x6a81]
- The list stays in memory as 0x31-byte entries behind DGROUP:0x6af8, with the count in DGROUP:0x94c6. [BYTES seg6:0x65b2 seg6:0x6a35]
- Each line sent to the list window reads `<rank> - <pilot> - <ship>`, then the markers `\t230` score, `\t292` start date ` - ` save date, `\t415` status, `\t475` `(<file>)`. Dates print as month + 1, day and the stored `tm_year`. [BYTES seg6:0x6ebb seg6:0x6ed2 seg6:0x6f3e seg6:0x6f44 seg6:0x700d] How the window reads the `\t` markers is [OPEN].

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
| `SndPath` | S 0x6d | "" | [BYTES ISPACE.DLL seg9:0x152] [BYTES FACTORY.EXE seg4:0x4c9] | [BYTES seg6:0x8a0] | sound directory ([ispace.md](ispace.md)) |
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
| `SavePrompt` | I | 1 | [BYTES seg10:0x85a5] [BYTES ISPACE.DLL seg9:0x2d6a] | [BYTES ISPACE.DLL seg9:0x2f18] | [OPEN] |
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
| `RadarLeft`, `RadarTop` | 0 | DGROUP:0x1dd4, 0x1dd6 | [BYTES seg6:0x196 seg6:0x1ac] | [BYTES seg6:0x3d1 seg6:0x3f7] |
| `WatchLeft`, `WatchTop` | 0 | DGROUP:0x1ddc, 0x1dde | [BYTES seg6:0x1c2 seg6:0x1d8] | [BYTES seg6:0x424 seg6:0x470] |
| `WatchRight` | `WatchLeft` + 280 | DGROUP:0x1de0 | [BYTES seg6:0x1e8 seg6:0x1eb seg6:0x1f3] | [BYTES seg6:0x44a] |
| `IconLeft` | screen width − 48 | DGROUP:0x1de4 | [BYTES seg6:0x205 seg6:0x20a seg6:0x212] | [BYTES seg6:0x535] |
| `IconTop`, `SmLeft`, `SmTop`, `HelpLeft`, `HelpTop` | 0 | DGROUP:0x1de6, 0x1dec, 0x1dee, 0x1dc4, 0x1dc6 | [BYTES seg6:0x245 seg6:0x25b seg6:0x28e seg6:0x2a4 seg6:0x2ba] | [BYTES seg6:0x55b seg6:0x588 seg6:0x5ae seg6:0x5db seg6:0x601] |
| `NavLeft`, `NavTop`, `NavRight`, `NavBottom` | 30, 30, 560, 350 | DGROUP:0x6af0, 0x6af2, 0x6af4, 0x6af6 | [BYTES seg6:0x1cb6 seg6:0x1ccc seg6:0x1ce3 seg6:0x1cfa] | [BYTES seg6:0x2f07 seg6:0x2f59 seg6:0x2f32 seg6:0x2f84] |

- The game reads `IconLeft` with a default of `GetSystemMetrics(SM_CXSCREEN)` − 48, so the oracle at 1024 pixels saw 976. [BYTES seg6:0x205 seg6:0x20a] [ORACLE newgame-0001]

**Settings.** The INSPACE reads are in seg6:0xb3e unless stated. ISPACE.DLL reads these keys in ISPACE.DLL seg9:0x22ca, ISPACE.DLL seg9:0x15cc and ISPACE.DLL seg9:0x2bf8. It writes them in four functions: ISPACE.DLL seg9:0x1fae (`Complex` … `FrameRateSet`), ISPACE.DLL seg9:0x2dc6 (`ShipAnim` … `SavePrompt`), ISPACE.DLL seg9:0x331a (`StereoEffect` … `StereoSwap`) and ISPACE.DLL seg9:0x4712 (`JoyStick` … `JBtn`n). Which dialog control feeds which key is [OPEN]. [BYTES ISPACE.DLL seg9:0x1fae seg9:0x2dc6 seg9:0x331a seg9:0x4712]

| Key | Type | Default | Read | Written | Meaning |
|---|---|---|---|---|---|
| `GameSpeed`, `FrameRateSet`, `Calibrate` | I | see [timing.md](timing.md) | [BYTES seg6:0xb5c seg6:0xb7d seg15:0x5f98] [BYTES ISPACE.DLL seg9:0x1a25] [BYTES FACTORY.EXE seg4:0x5b5 seg4:0x5cc seg4:0x3b3e] | [BYTES seg6:0xb31 seg10:0x7f0b] [BYTES ISPACE.DLL seg9:0x21fb] | timing ([timing.md](timing.md)). The Factory reads `Calibrate` with default 1000 |
| `Detail` | I | 3 | [BYTES seg6:0xc76] [BYTES ISPACE.DLL seg9:0x2592] | [BYTES ISPACE.DLL seg9:0x2196] | detail level → DGROUP:0x9208 |
| `Quick` | I | 0 (INSPACE, Factory); 1 (ISPACE.DLL) | [BYTES seg6:0xc60] [BYTES ISPACE.DLL seg9:0x25a8] [BYTES FACTORY.EXE seg4:0x3bc3] | [BYTES ISPACE.DLL seg9:0x2170] | → DGROUP:0x931e |
| `Enhance` | I | 1 | [BYTES seg6:0xc8c] [BYTES ISPACE.DLL seg9:0x257c] | [BYTES ISPACE.DLL seg9:0x20fe] | → DGROUP:0x8fe8 |
| `Complex` | I | 0 (INSPACE seg6:0x1204); 2 (ISPACE.DLL) | [BYTES seg6:0x1204] [BYTES ISPACE.DLL seg9:0x1766] | [BYTES ISPACE.DLL seg9:0x20b3] | → DGROUP:0x8ff8 |
| `Braking` | I | 0 | [BYTES seg6:0xcf4] [BYTES ISPACE.DLL seg9:0x16fb] | [BYTES ISPACE.DLL seg9:0x20d8] | → DGROUP:0x8ff6 |
| `MixChannels` | I | 1 (INSPACE); 4 (ISPACE.DLL) | [BYTES seg6:0xc9c seg6:0xca7] [BYTES ISPACE.DLL seg9:0x55d seg9:0x571] | [BYTES ISPACE.DLL seg9:0x3493] | → DGROUP:0x920a |
| `BadVideo` | I | 1 | [BYTES seg6:0xcda] [BYTES ISPACE.DLL seg10:0x16ab] | — | → DGROUP:0x742 |
| `QuickIntro` | I | 0 | [BYTES seg6:0x318 seg6:0xcc4] [BYTES ISPACE.DLL seg9:0x2c77] | [BYTES ISPACE.DLL seg9:0x2e84] | → DGROUP:0x8f70 |
| `ShipAnim` | I | 1 | [BYTES seg6:0xd0a] [BYTES ISPACE.DLL seg9:0x2d48] | [BYTES ISPACE.DLL seg9:0x2e5f] | → DGROUP:0x131c |
| `Border` | I | 1 | [BYTES seg6:0xd20] [BYTES ISPACE.DLL seg9:0x2d26] | [BYTES ISPACE.DLL seg9:0x2ece] | — |
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
| `AllowSound`, `ForceVol` | I | 0 | [BYTES ISPACE.DLL seg9:0x2444 seg9:0xbde] | — | `ForceVol` see [ispace.md](ispace.md) |
| `SaverVolume` | I | 0 | [BYTES ISPACE.DLL seg9:0x6e] | — | → ISPACE.DLL DGROUP:0x69e ([ispace.md](ispace.md)) |
| `Breakfast`, `Coffee`, `Lunch`, `Tea` | I | 800, 1000, 1200, 1530 | [BYTES seg10:0x96df seg10:0x970b seg10:0x96b2 seg10:0x9737] | — | special times of day [MANUAL Special Times of Day] |

**Race, duel and demo options**

| Key | Type | Default | Read | Written | Meaning |
|---|---|---|---|---|---|
| `Racers`, `RaceDiff`, `RaceLaps`, `RaceOpp` | I | 1, 0, 1, 2 | [BYTES seg6:0x2773 seg6:0x2930 seg6:0x2aee seg6:0x2c3a] | [BYTES seg6:0x3339 seg6:0x3392 seg6:0x3426 seg6:0x33dc] | seg6:0x23d4 selects each value in a combo box with `CB_SETCURSEL` [BYTES seg6:0x2859 seg6:0x285e] |
| `Duelers`, `DuelDiff`, `DuelOpp` | I | 0, 0, 2 | [BYTES seg6:0x2851 seg6:0x2a0f seg6:0x2d7f seg6:0x34ca] | [BYTES seg6:0x351d seg6:0x3569 seg6:0x35b3] | the same for the duel combo boxes; seg6:0x34ca stores `DuelDiff` in DGROUP:0x94a8 |
| `DRacers`, `DDiff`, `DLaps` | I | 0 | [BYTES seg6:0x3208 seg6:0x3262 seg6:0x327e] [BYTES ISPACE.DLL seg10:0x3809 seg10:0x376a seg10:0x395f] | [BYTES ISPACE.DLL seg10:0x3d65 seg10:0x3da2 seg10:0x3d28] | `DDiff` → DGROUP:0x94a8 |
| `DemoType` | I | 519 | [BYTES seg15:0x5aa6] [BYTES ISPACE.DLL seg10:0x3670] | [BYTES ISPACE.DLL seg10:0x3df4] | → DGROUP:0x95ba |
| `DScale` | I | 0 | [BYTES seg9:0x236b] [BYTES ISPACE.DLL seg10:0x3510] | [BYTES ISPACE.DLL seg10:0x3ceb] | — |
| `Track` | S 0x6d or 20 | "" | [BYTES seg6:0x3c8a] [BYTES ISPACE.DLL seg10:0x36d5] | [BYTES ISPACE.DLL seg10:0x3e43] | → DGROUP:0x6990 |
| `Ship1`, `Ship2` | S 20 | "" | [BYTES seg14:0x236c seg14:0x239c seg15:0x5af0] [BYTES ISPACE.DLL seg10:0x399a seg10:0x39b7] | [BYTES ISPACE.DLL seg10:0x3e90] | SHP names; seg15:0x5962 reads `Ship1` with `DemoType` |
| `Tree` | I | 0 | [BYTES seg6:0x37ef] | — | → DGROUP:0x94bc |

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
- INSPACE passes ids 100 (seg15:0xd9b, seg15:0x3405), 105 (seg15:0x31fd), 110 (seg15:0x3284), 115 (seg15:0x903) and 120 (seg15:0x332b) through seg10:0x9526, which places them in ordinal 32's id argument. [BYTES seg15:0x903 seg15:0x911 seg15:0xd9b seg15:0xda9 seg15:0x31fd seg15:0x3284 seg15:0x332b seg15:0x3405 seg10:0x9553 seg10:0x955d]
- **Header colours.** Words +0x00–+0x11 hold three RGB colours, +0x24 the colour count, +0x26 one divisor per colour and +0x2c–+0x3d three dark colours; ordinal 32 builds one shade ramp per colour from them ([ispace.md](ispace.md#ordinal-notes), ordinal 32). Scene 100 has the colours (255, 255, 255), (0, 224, 0) and (220, 202, 70), the count 3, the divisors 4, 4 and 8 and the dark colours (32, 32, 32), (0, 100, 0) and (120, 108, 38). [BYTES ISPACE.DLL seg8:0x14e2 seg8:0x1512 seg8:0x1537] [BYTES IGRAPHIC.DLL 6600]
  - A record whose slot byte v is below −1 draws only when header byte 0x48 − v names a device that ISPACE.DLL found: v = −10 reads +0x52 (3 in scene 100), v = −11 reads +0x53 (5) and v = −20 reads +0x5c (0xf4, the low byte of the dword 500), which matches no case. [BYTES ISPACE.DLL seg8:0x1641] [BYTES IGRAPHIC.DLL 6600]
- **Records.** The 0x26-byte records start at offset 0x80; scene 100 has 62 and its record 61 is all zero. These are the fields the draw loop uses: [BYTES ISPACE.DLL seg8:0x1afb seg8:0x1b31 seg8:0x1b38 seg8:0x28de] [BYTES IGRAPHIC.DLL 6600]

  | Offset | Size | Field |
  |---|---|---|
  | +0x19 | word | string id, type 1 only: a value above 1 is loaded with LoadString; −13, −12, −11 and −10 copy the CPU, display-driver, sound-card and net-card texts |
  | +0x1b, +0x1c | byte | parent record and second parent (the end point of a line) |
  | +0x1d | signed byte | colour index, −1 for none |
  | +0x1e | byte | primitive: 1 box (ISPACE.DLL seg8:0x28de), 2 line (seg8:0x5470), 3 seg8:0x3874, 4 seg8:0x50ec, 5 seg8:0x4206 |
  | +0x1f | signed byte | type 1 only: output slot when ≥ 0, condition when below −1 |
  | +0x20 | byte | brightness: the shade level is the colour's peak channel × value / 50 |
  | +0x21 | byte | type 1 only: 100 takes a pattern brush from BITMAP METAL, 120 one from graphic 76 |

  - Scene 100's slots are record 42 (slot 0, the monitor screen), 17 (1, the PC tower front), 5 (2), 18 (3) and 60 (4, colour −1, the meal spot). Its conditional records are 36 and 38 (v = −11) and 37 and 39 (v = −10). Every word +0x19 and every byte +0x21–+0x25 is 0, so scene 100 draws no text and loads no bitmap. [BYTES IGRAPHIC.DLL 6600]
  - Bytes +0x00–+0x18 (a resolved flag, positions, sizes and per-axis reference codes) and the layout solver that reads them are [OPEN] ([ispace.md](ispace.md#ordinal-notes)); so is the text path of scenes 105–120 through ISPACE.DLL seg6:0x7f52. [OPEN] [BYTES ISPACE.DLL seg8:0xa36 seg6:0x7f52]

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
- **INSPACE.EXE #5500 110, the install record.** 10 bytes: the dword D at +0, the install drive at +4 (a word, 1 for A:) and zero bytes +6–+9. The shipdisk record is all zero. [BYTES ISPACE.DLL seg10:0x4b99 seg10:0x4bee seg10:0x5050] [BYTES INSPACE.EXE 5500 110]
  - Its writer ISPACE.DLL seg10:0x4b1a (ordinal 24 commands 0, 11 and 13) sets the drive to the first byte of INI `LocalPath` − 0x40 and D to seg10:0x4440(drive), the drive's volume serial, or to rand() % 7651 + 5432 (sign-extended) when that is 0, and caches D in ISPACE.DLL DGROUP:0x14e8. It writes the block with seg10:0x4636 and then calls the INI writer seg10:0x5b24(0x17, x), or (0x38, x) for command 13, with x = (D & 0xFFFF) mod 653 + 654 (unsigned). [BYTES ISPACE.DLL seg10:0x4b99 seg10:0x4ba7 seg10:0x4bb8 seg10:0x4bcb seg10:0x4bd8 seg10:0x4bee seg10:0x4c0e seg10:0x4c10 seg10:0x4c26 seg10:0x4c5a]
  - The installer writes it at file offset 0x4b840 and keeps the file's DOS date and time. Under Wine seg10:0x4440 returns 0 ([ispace.md](ispace.md#ordinal-notes), ordinal 24), so D is the random value. [ORACLE install-0001]
- **INSPACE.EXE #5000 143, the name record.** 50 bytes. The installer fills them with Name (GetWindowText, at most 0x12 bytes) + " - " + Organization (at most 0x16), and ordinal 24 command 9 scrambles them with ISPACE.DLL seg10:0x4dba(buf, 50, 1) and writes all 50, bytes after the NUL included, keeping the file's date and time. The shipdisk record holds "Unregistered Copy   ILLEGAL" in clear. [BYTES ISPACE.DLL seg10:0x4d0c seg10:0x4d26] [BYTES INSPACE.EXE 5000 143] [ORACLE install-0001]
  - seg10:0x4dba(buf, n, mode) calls srand(GetCurrentTime()) in both modes and gives byte i the key {0x2A, 0x32, 0x2D}[i mod 3]. Mode 1 (scramble) turns a byte that is ≤ 0 as a signed value into −(rand() % 96 + 2) and XORs every other byte with the key. Mode 0 (descramble) turns a byte ≥ 0x80 into 0 and XORs every other byte with the key. So a NUL comes back as 0, and name bytes ≥ 0x80 are lost. [BYTES ISPACE.DLL seg10:0x4dc9 seg10:0x4ddb seg10:0x4deb seg10:0x4e13 seg10:0x4e1b seg10:0x4e20]
  - Command 10 (seg10:0x4c68 in mode 0) reads the 50 bytes from <LocalPath>INSPACE.EXE, descrambles them and returns 1 when the resource is found. When decoded byte 0, 1 or 2 is 0 it returns 0 and moves bytes 3–49 down to 0–46. INSPACE seg10:0x946a calls it and stores 0 at +0x30, so the name shown is at most 48 characters. [BYTES ISPACE.DLL seg10:0x4d45 seg10:0x4d5a seg10:0x4d63 seg10:0x4d77 seg10:0x4d82] [BYTES seg10:0x9482 seg10:0x948c]
- **The INI values.** ISPACE.DLL seg10:0x5b24(mode, x) writes SYSTEM.INI [386Enh] `vidsp` and `32BitSP` and WIN.INI [windows] `PDev` ([INI files](#winini-systemini-and-other-files)). It calls srand(GetCurrentTime()) and draws s = rand() % 1111 + rand() % 8999 + 1987. [BYTES ISPACE.DLL seg10:0x5b39 seg10:0x5b4a seg10:0x5b57 seg10:0x5b5f seg10:0x5b95 seg10:0x5bf7 seg10:0x5c35 seg10:0x5c41 seg10:0x5c4c seg10:0x5cc7 seg10:0x5d2e seg10:0x5d4a seg10:0x5d6e seg10:0x5dc3 seg10:0x5ded]
  - The new PDev is x + s for modes 0x17 and 0x38, (x with its low byte XORed with 0xFF) + s for 0x54, 0x5b and 0xbe, and s − rand() % 200 − 199 for 0x75.
  - vidsp is written as s when its stored value is 0 or the mode is 0x17, 0x38, 0x75 or 0x5b; otherwise s takes the stored value. PDev is written when its stored value is 0 or the mode is 0x17, 0x38, 0x75, 0x9d or 0x5b, mode 0x9d writing the negated stored value; otherwise the new PDev takes the stored value.
  - With X = the low word of 2s − PDev − 1231, 32BitSP becomes X for mode 0x5b and otherwise rand(), minus 1 when that equals X; mode 0x54 keeps a non-zero stored value. Mode 0x5b also writes INSPACE.INI [Inner Space] `Accept` = 1.
  - Mode 0xbe first reads SYSTEM.INI [386Enh] `XCal123`. When it is 0 it writes rand() % 6777 + 3913 there and goes on as mode 0x5b; otherwise it goes on as mode 0x54 and returns 0.
- **What an install leaves.** After modes 0x17 and 0x38, vidsp = s lies in 1987–12095, PDev − vidsp = (D & 0xFFFF) mod 653 + 654 lies in 654–1306, and 32BitSP is a random value other than 2·vidsp − PDev − 1231. Ordinal 24 command 2 then accepts the record through the PDev − vidsp relation when seg10:0x4440 returns 0, and seg10:0x885e never reaches its 32BitSP test ([shell.md](shell.md#intro-and-title-screen)). Two installs give different values because s and D are random, not because of the date or the name. [BYTES ISPACE.DLL seg10:0x4c10 seg10:0x5b95 seg10:0x5252] [BYTES seg10:0x8983] [ORACLE install-0001]
- **The 35-day code.** The Order dialog's Register button calls seg10:0x5b24(0x5b, day), with day the date number of the day the code is entered ([shell.md](shell.md#game-start-dialogs)). That leaves PDev − vidsp = day with its low byte inverted, 32BitSP = 2·vidsp − PDev − 1231 and Accept = 1. With the shipdisk's all-zero #5500 110, command 2 returns 3, and seg10:0x885e's day block counts the days since the code. [BYTES ISPACE.DLL seg10:0x8c3 seg10:0x5c41 seg10:0x5d69] [BYTES seg10:0x8a91 seg10:0x8ab2]
- The layouts of IGRAPHIC.DLL #5500 110 (the serial that ISPACE.DLL seg10:0x5dfe shows in the Order dialog) and of INSTALL.EXE #5500 110 and #5500 120 are [OPEN]. So is which of commands 0, 11 and 13 INSTALL.EXE issues after command 9; INSTALL.EXE is not disassembled, and its relay shows ISPACE.DLL seg10:0x4b7e and seg10:0x4c5a writing #5500 110 and then seg10:0x5b24. [OPEN] [BYTES ISPACE.DLL seg10:0x490b seg10:0x4933] [ORACLE install-0001]
- The port reads the user's files and never writes them. [DESIGN]
