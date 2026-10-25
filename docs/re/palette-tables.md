# ISPACE.DLL colour tables

ISPACE.DLL seg5:0x5846 fills two colour tables in ISPACE.DLL DGROUP at run time ([shell.md](shell.md#palette)). This page lists their contents after the call and the title's 8-bpp palette built from them. Colours are hex RRGGBB; the tables store bytes in R, G, B order. [BYTES ISPACE.DLL seg5:0x5846] [BYTES ISPACE.DLL seg5:0x61b2 seg5:0x2839] [ORACLE probe-0001]

## Ten-colour schemes
ISPACE.DLL DGROUP:0x117c holds 18 rows of 10 triples (30 bytes per row, ending at ISPACE.DLL DGROUP:0x1397). Command 7 copies a row into ten palette entries; seg5:0x26a2 recolours a graphic's entries 0–9 with a row; ordinal 14 blends with them. The c column gives the argument that selects the row in command 7's switch and in the switch of the ordinal 12 helper ISPACE.DLL seg7:0x4ee8, which share the map; the values marked "(seg7:0x4ee8)" have a case only there. Rows 9, 11, 12 and 16 are reached only through seg5:0x26a2 (rows a and b) and ordinal 14 (ISPACE.DLL seg5:0x50cc). Rows marked "interpolated" keep the middles that loop 1 computes; row 12 stays all zero and is filled per graphic by seg5:0x26a2. [BYTES ISPACE.DLL seg5:0x6292 seg5:0x6297 seg7:0x4fc8 seg7:0x4fcd seg7:0x505d seg5:0x50cc seg5:0x597c seg5:0x2839]

| Row | c | Entries 0–9 | Middles |
|---|---|---|---|
| 0 | 2 (seg7:0x4ee8) | ffffff e6e6e8 cdcdd0 b4b4b8 9b9ba0 828288 696970 505058 373740 1e1e28 | interpolated |
| 1 | 5 | ffe66e ecd162 d8bc56 c4a74a b0923e 9c7d31 886825 745319 603e0d 4c2800 | interpolated |
| 2 | 13 | f0d264 dcbe5a d2aa50 c89b46 b48c3c a07d32 8c6928 73501e 5a3c14 46280a | constants |
| 3 | 6 | ffeb00 ebd700 d7c300 c3af00 af9b00 9b8700 877300 735f00 5f4b00 4b3700 | interpolated |
| 4 | 1 (seg7:0x4ee8) | fffa3c ffc000 ffa000 ff6e00 f52800 e00000 c00000 a00000 6c0000 4b0000 | constants |
| 5 | 7 | f555f5 dc37dc cf2dd6 be00cd aa00b4 8c0091 820091 6c0073 4b0050 2d0037 | constants |
| 6 | 12 | 966ec3 8962b6 7c56a8 6e4a9a 613e8c 54317e 462570 391962 2c0d54 1e0046 | interpolated |
| 7 | 8 | c8b0c0 be96c8 b480e0 a060ff 9050ff 8040ff 7020e0 6000c0 0000a0 00005a | constants |
| 8 | 0 (seg7:0x4ee8) | 00aaff 0096f0 0082e1 006ecd 0055be 003caa 001e96 000a87 00006e 00004b | constants |
| 9 |  | 96f5ff 5fc3f0 4bb4e6 2891e0 1469d6 0055d0 003cbe 0023a2 000f7e 00004b | constants |
| 10 | 4 (seg7:0x4ee8) | aaff00 98e900 85d200 72bb00 5fa400 4c8e00 397700 266000 134900 003200 | interpolated |
| 11 |  | c0ff00 80f000 40dc00 3ab300 00a000 009100 008700 006600 006000 004b00 | constants |
| 12 |  | 000000 000000 000000 000000 000000 000000 000000 000000 000000 000000 | interpolated |
| 13 | 10 | 00ffeb 00f0dc 00dcc8 00d2be 00beaa 00a591 008c78 007864 006450 004b3c | constants |
| 14 | 11 | ffdcdc ebbebe e1a5a5 d78c8c c37878 b46464 a05050 8c3c3c 642828 4b0000 | constants |
| 15 | 3 | a49c7d 908e74 838773 747d70 6e7365 676857 615c49 5a5341 504b39 363327 | constants |
| 16 |  | ffffff f8f8f8 eeeeee dfdfdf d2d2d2 b9b9b9 a5a5a5 7d7d7d 5f5f5f 3c3c3c | constants |
| 17 | 9 | ffd2a0 ebb987 d7a573 cd915f be8250 b47846 a06432 8c501e 824614 783c0a | constants |

## Five-colour schemes
ISPACE.DLL DGROUP:0x1cdc holds 18 rows of 5 triples (15 bytes per row, ending at ISPACE.DLL DGROUP:0x1de9). seg5:0x26a2 recolours a graphic's entries 10–14 with row b, and seg7:0x4ee8 reads the table too. [BYTES ISPACE.DLL seg5:0x2924 seg7:0x53f0 seg5:0x59e3]

| Row | Entries 10–14 | Middles |
|---|---|---|
| 0 | ffffff c7c7ca 8f8f94 57575e 1e1e28 | interpolated |
| 1 | ffd21e d2a817 a57d0f 785308 4b2800 | interpolated |
| 2 | f0d264 d0a046 aa7d2d 785019 46280a | constants |
| 3 | ffe600 d2b800 a58a00 785c00 4b2d00 | interpolated |
| 4 | fffa3c ff9600 fa3c00 c00000 4b0000 | constants |
| 5 | ff00ff d000d2 a000a5 700078 40004b | interpolated |
| 6 | b46eb4 87538f 5a376a 2d1c45 000020 | interpolated |
| 7 | c8b0c0 b480e0 9050ff 6000c0 00004b | constants |
| 8 | 00dcdc 00a5b8 006e94 003770 00004b | interpolated |
| 9 | 6ee6ff 50a5b9 326496 193c82 00144b | constants |
| 10 | c8ff00 96d200 64a500 327800 004b00 | interpolated |
| 11 | aaff00 72c600 00a000 008000 004b00 | constants |
| 12 | 000000 000000 000000 000000 000000 | interpolated |
| 13 | 00ffeb 00d2c1 00a596 00786c 004b41 | interpolated |
| 14 | ffdcdc d2aaaa aa8787 823232 500000 | constants |
| 15 | a49c7d 8e896e 736d55 554e3f 363327 | constants |
| 16 | ffffff bebebe 8c8c8c 646464 3c3c3c | constants |
| 17 | ffd2a0 d7a573 be8250 a06432 783c0a | constants |

## Title palette at 8 bpp
After title step 0 the working palette holds the entries below ([shell.md](shell.md#palette) describes the commands that write them). Entries 10–245 keep peFlags PC_RESERVED; 79 of them are black (10, 123–184 and 230–245). [BYTES seg15:0x5c35 seg15:0x6cfb seg15:0x757c] [BYTES ISPACE.DLL seg5:0x38bf seg5:0x2e87 seg5:0x3200 seg5:0x33ab] [ORACLE probe-0001] [ORACLE wave-0001]

| Entries | Content |
|---|---|
| 0–9, 246–255 | the system's static colours (command 0) |
| 10 | black |
| 11–102 | ISPACE.DLL #5500 100 |
| 103–112 | DGROUP:0x117c row 7 (scheme 8) |
| 113–122 | DGROUP:0x117c row 1 (title command 7, c = 5) |
| 123–184 | black |
| 185–212 | ffe66e f9df6a f3d866 ecd162 e6ca5e e0c35a d8bc56 d2b552 ccae4e c4a74a bea046 b89942 b0923e aa8b3a a48436 9c7d31 96762d 906f29 886825 826121 7c5a1d 745319 6e4c15 684511 603e0d 5a3709 543005 4c2800 |
| 213–229 | d7d7d6 d0d0cf c9c9c8 c2c2c1 b9baba b3b3b3 adacac a7a5a5 9e9e9e 949494 8a8a8a 808080 737373 6c6c6b 656563 5e5e5b 565552 |
| 230–245 | black |

- Entries 185–229 are the ramps that ordinal 12 builds from graphic 300's colour table c: entry 185 + 3k + s = c[k] + s · trunc((c[k+1] − c[k])/3) for k = 0–8 and s = 0–2, entry 212 = c[9]; entry 213 + 4(k − 10) + s = c[k] + s · trunc((c[k+1] − c[k])/4) for k = 10–13 and s = 0–3, entry 229 = c[14]. The start offsets are ISPACE.DLL DGROUP:0x178 = 0x2e4 and DGROUP:0x17c = 0x354. [BYTES ISPACE.DLL seg5:0x2e87 seg5:0x3200 seg5:0x322c seg5:0x33ab DGROUP:0x178 DGROUP:0x17c] [BYTES ISPACE.DLL 4000 300]
- The final title emblem uses exactly entries 188–229 plus black (43 colours): graphic 300 never uses its colour 0, so entries 185–187 do not appear. The depth-24 title screenshot shows all of 188–229 and none of 185–187. [BYTES ISPACE.DLL 4000 300] [ORACLE probe-0001] [ORACLE wave-0001]
