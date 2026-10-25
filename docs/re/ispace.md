# ISPACE.DLL contract

ISPACE.DLL (module `ISPACE`, description "Inner Space Resource DLL") holds the game's palette manager, bitmap loading and sprite helpers, sound and music, the option and game dialogs and the joystick reader. INSPACE.EXE has no import records for it. It reaches the DLL through 23 far pointers that it fetches with `GetProcAddress` by ordinal. [BYTES ISPACE.DLL NE header] [BYTES seg10:0x67e7]

## Loading and unloading
- seg6:0x13d0 calls the loader seg10:0x6656 during start-up. The loader first opens `<Language>_LANG.DLL`, where `Language` is the INI key of that name. `LoadLibrary` at seg10:0x6714 stores the handle in DGROUP:0x9212. [BYTES seg6:0x13d0] [BYTES seg10:0x6714]
- It then opens `ISPACE.DLL` from the game directory (DGROUP:0x8f7a). `LoadLibrary` at seg10:0x67c0 stores the handle in DGROUP:0x9210. [BYTES seg10:0x6754] [BYTES seg10:0x67c0]
- Failures:
  - A missing language DLL shows "Can't find <name>. Try reinstalling, do not preserve previous settings." (DGROUP:0x783, DGROUP:0x78f). [BYTES seg10:0x66ce]
  - A language handle below 32 shows "<name> is faulty" (DGROUP:0x7c6). [BYTES seg10:0x6739]
  - A missing ISPACE.DLL shows ENG_LANG string 2117 "Can't Find " with the path, then the loader still calls `LoadLibrary`. [BYTES seg10:0x6782] [BYTES ENG_LANG.DLL STRING 133]
  - An ISPACE.DLL handle below 32, or a NULL result from any of the 23 lookups, shows the path with string 2026 " Is Faulty!". [BYTES seg10:0x6b14] [BYTES ENG_LANG.DLL STRING 127]
  - In each case seg10:0x6656 returns 0, and seg6:0x13db calls seg10:0x6b70 to free both libraries. [BYTES seg10:0x6709] [BYTES seg6:0x13db]
- After the lookups the loader calls ordinal 7 with commands 5 and 7, which make ISPACE.DLL take its own references to `<Language>_LANG.DLL` and `IGRAPHIC.DLL`. A zero result from either fails the load. [BYTES seg10:0x6b2f] [BYTES seg10:0x6b3e]
- seg10:0x6b70 unloads. With argument 0 it first calls ordinal 7 commands 6 and 8, then frees ISPACE.DLL and the language DLL. Its callers are seg2:0x77, seg6:0x13db and seg6:0x1a5a. [BYTES seg10:0x6b8e] [BYTES seg10:0x6ba1]
- The oracle sees the 23 lookups return non-NULL, and finds each result in the pointer block. [ORACLE probe-0001]
- DGROUP:0x3c60 lies inside the pointer block, but no lookup writes it. It is the first word of an array that seg10:0x5c6e–0x5dfa indexes. [BYTES seg10:0x5c86] [BYTES seg10:0x5d12]

## Calling convention
- Every export returns with `retf N` and so pops its own arguments (Pascal order). The tables list arguments in push order: the first one listed is pushed first and sits at the highest stack offset. A dword argument is pushed high word first. [BYTES ISPACE.DLL seg5:0x3592] [BYTES ISPACE.DLL seg9:0xb7] [REF Win16 SDK]
- For each of the 23 ordinals, the bytes that the INSPACE call site pushes equal the export's `retf` operand. So INSPACE.EXE and ISPACE.DLL agree on the argument bytes of every call. Equal totals do not show that each single argument has the same size on both sides. [BYTES seg10:0x6c57] [BYTES ISPACE.DLL seg5:0x402]
- Every call through the pointer block sits in INSPACE seg10, in a small cdecl wrapper. No other code reads the 23 pointer slots. [BYTES seg10:0x6c57] [BYTES seg10:0x9956]
- The wrappers supply three recurring arguments:
  - `hPal` is the game palette in DGROUP:0x920e, which ordinal 4 command 0 creates (seg10:0x6e00). [BYTES seg10:0x6e1c]
  - `hdc` is the current DC in DGROUP:0x13dc. The idle step holds it while a frame runs (see [timing.md](timing.md)). When it is 0, wrapper seg10:0x6c02 takes `GetDC` of the main window for the call. [BYTES seg10:0x6c20]
  - `lib` is a far pointer to a module name. Every INSPACE call site passes an empty string (DGROUP:0x744, DGROUP:0x7de, DGROUP:0x7e4). [BYTES seg10:0x6c4a] [BYTES DGROUP:0x7de]
- ISPACE.DLL keeps state for up to three palettes in a table of palette handles at ISPACE.DLL DGROUP:0x1cd6. The graphics exports start by looking up `hPal` there. [BYTES ISPACE.DLL seg5:0x402] [BYTES ISPACE.DLL seg10:0x42ef]

## The 23 ordinals INSPACE uses
The "retf" column is the number of argument bytes. The Wrapper column of the next table names the INSPACE function that holds the indirect call.

| Ord | Pointer | ISPACE.DLL entry | retf | Arguments (push order) → result | Function |
|---|---|---|---|---|---|
| 1 | DGROUP:0x3c28 | seg5:0x402 | 14 | hPal, id, lib, hdc, mode, arg → HBITMAP | Load graphic resource `id` and convert it to a device bitmap. It records the bitmap's height for ordinal 5 |
| 2 | DGROUP:0x3c30 | seg6:0x0 | 30 | hPal, id, lib, hdc, six words, three bytes, sel → none | Render graphic resource `id` |
| 3 | DGROUP:0x3c2c | seg5:0x612 | 4 | name (far) → HBITMAP | `LoadBitmap` from IGRAPHIC.DLL, or from ISPACE.DLL for `SD_LOGO` |
| 4 | DGROUP:0x3c50 | seg5:0x34c8 | 14 | hPal, hwnd, hdc, a, b, c, cmd → word | Palette manager, commands 0–8 |
| 5 | DGROUP:0x3c4c | seg6:0xb0 | 0 | → height | Height of the last bitmap that ordinal 1 made |
| 6 | DGROUP:0x3c40 | seg5:0x650 | 6 | hPal, hbm, hdc → HBITMAP | Monochrome mask of a bitmap |
| 7 | DGROUP:0x3c54 | seg9:0x0 | 6 | arg (dword), cmd → BOOL | Module and sound manager, commands 0–11 |
| 8 | DGROUP:0x3c58 | seg9:0x76c | 12 | p (dword), a, b, area (dword) → word | Stereo and distance volume for a sound at position `p` |
| 9 | DGROUP:0x3c5c | seg9:0xc42 | 24 | id, p (dword), q, r, s, area (dword), now (dword), name (far) → BOOL | Play sound effect `id`, or the WAV file `name` when `id` is −1 |
| 10 | DGROUP:0x3c64 | seg9:0x145e | 14 | hwnd, hdc, hPal, x4, x5, x6, cmd → int | Dialog dispatcher, commands 0–15 |
| 12 | DGROUP:0x3c34 | seg6:0xc4 | 24 | hPal, id, lib, hdc, five words, two bytes → word | Render graphic resource `id` through ISPACE.DLL seg7:0x12d8 |
| 14 | DGROUP:0x3c7c | seg5:0x50cc | 10 | hPal, mode, percent, hwnd, hdc → none | Scale or blend the palette's colours by `percent` |
| 15 | DGROUP:0x3c44 | seg5:0x11a4 | 18 | hdc, five words, hPal, frame, flag → word | Draw direction `frame` (0–15) of a sprite strip |
| 17 | DGROUP:0x3c38 | seg6:0x5538 | 28 | hPal, id, lib, hdc, eight values, scale → word | Render graphic resource `id` as a sprite |
| 18 | DGROUP:0x3c3c | seg6:0x5e76 | 12 | hPal, 1, 1, x, 5, y → handle | Build a bitmap from graphic resource 280 |
| 24 | DGROUP:0x3c68 | seg10:0x48d2 | 10 | a (far), b (far), cmd → word | String service, commands 0–21 |
| 29 | DGROUP:0x3c6c | seg2:0x3c | 48 | twelve dwords → word | Integer collision-response arithmetic |
| 32 | DGROUP:0x3c78 | seg8:0x0 | 34 | hdc, hPal, fifteen words → word | Scene renderer over a list of 0x26-byte records |
| 33 | DGROUP:0x3c74 | seg8:0x37a | 22 | hdc, hPal, cell (dword), align, colour (dword), x, y, text (far) → none | Draw text in the game's bitmap font |
| 34 | DGROUP:0x3c70 | seg8:0x5ac | 12 | hdc, x, y, w, h, style → none | Draw a bevelled frame |
| 35 | DGROUP:0x3c80 | seg8:0x768 | 12 | hdc, x, y, w, h, value → word | Draw a bevelled bar filled in proportion to `value` |
| 36 | DGROUP:0x3c48 | seg10:0x2e1a | 24 | hdcDst, x, y, w, h, hdcSrc, xs, ys, ws, hs, rop (dword) → none | Stretch blit that mirrors on negative extents |
| 41 | DGROUP:0x3c84 | seg9:0x3e8a | 6 | joystick, info (far) → word | Read the joystick position and four buttons |

The ordinal-to-slot pairs come from the lookups at seg10:0x67e7–0x6ad3. The entry addresses come from the ISPACE.DLL entry table. The `retf` operands are at the ends of the exports. [BYTES seg10:0x67e7] [BYTES seg10:0x6ad3] [BYTES ISPACE.DLL entry table]

### Call sites in INSPACE
| Ord | Indirect call | Wrapper (fixed arguments) | Wrapper call sites |
|---|---|---|---|
| 1 | seg10:0x6c57, seg10:0x6cfa | seg10:0x6c02 (id, mode, arg); seg10:0x6c96 | 30 in seg7, seg10, seg11, seg13, seg15, seg16; seg10:0x6c96 has no callers |
| 2 | seg10:0x6f76, seg10:0x7026, seg10:0x39dc | seg10:0x6ee6 (per-slot cache); seg10:0x6fec; seg10:0x39a8 (id −1000) | 6; 4 in seg18; 4 |
| 3 | seg10:0x6d46 | seg10:0x6d38 | 18 |
| 4 | seg10:0x6d9a, seg10:0x6dc4, seg10:0x6df1, seg10:0x6e18, seg10:0x6e40, seg10:0x6e6a, seg10:0x6e90, seg10:0x6ebb | seg10:0x6d7a (cmd 6), seg10:0x6da6 (4), seg10:0x6dd4 (5), seg10:0x6e00 (0), seg10:0x6e26 (2), seg10:0x6e4c (3), seg10:0x6e76 (1), seg10:0x6ea0 (7) | 3, 5, 6, 2, 2, 8, 16, 2 |
| 5 | seg10:0x86d7 | seg10:0x86ca | 5 |
| 6 | seg10:0x6edb | seg10:0x6ec6 | 20 |
| 7 | seg10:0x6b2f, seg10:0x6b3e, seg10:0x6b8e, seg10:0x6b99, seg10:0x7400, seg10:0x7441, seg10:0x753e, seg10:0x7dac, seg10:0x7dc9, seg10:0x869d, seg10:0x86bf | loader seg10:0x6656 (5, 7); seg10:0x6b70 (6, 8); seg10:0x73e2 (0); seg10:0x740c (9); seg10:0x7480 (11); seg10:0x7d8e (1); seg10:0x7db8 (2); seg10:0x8686 (3); seg10:0x86a8 (4) | 1, 3, 2, 37, 36, 1, 2, 1, 1 |
| 8 | seg10:0x7d3f, seg10:0x7d82 | seg10:0x7b48; seg10:0x7d58 | seg7:0xcaf; 6 |
| 9 | seg10:0x785e, seg10:0x7aef, seg10:0x7b11 | seg10:0x754a; seg10:0x7944 | 114; 4 |
| 10 | seg10:0x7f4e, seg10:0x8314, seg10:0x8332, seg10:0x83e3, seg10:0x8606 | seg10:0x7f18 (cmd 0); seg10:0x82a2; seg10:0x8568 | seg2:0x14ca; 10; 3 |
| 12 | seg10:0x73d6 | seg10:0x73a6 | seg15:0x2165, seg15:0x2b8f, seg15:0x757c |
| 14 | seg10:0x9956 | seg10:0x993a | 8 in seg15 |
| 15 | seg10:0x739a | seg10:0x7370 | seg8:0x30d4, seg8:0x347d |
| 17 | seg10:0x7365 | seg10:0x7328 | seg11:0x4c32 |
| 18 | seg10:0x7051 | seg10:0x7032 | seg10:0x5c80, seg15:0x21ca, seg15:0x2be1 |
| 24 | seg10:0x88f5, seg10:0x8983, seg10:0x9482 | seg10:0x885e; seg10:0x946a (cmd 10) | 5; 2 |
| 29 | seg10:0x7e0e | seg10:0x7dd4 | seg8:0x39f6, seg8:0x57db, seg8:0x5a10, seg8:0x7422, seg18:0x2ddb |
| 32 | seg10:0x955d | seg10:0x9526 | 6 in seg15 |
| 33 | seg10:0x951b | seg10:0x94c4 (font 0, 1 or 2) | 39 |
| 34 | seg10:0x94b8 | seg10:0x949c | 11 |
| 35 | seg10:0x6d6e | seg10:0x6d52 | seg9:0x281c and 5 in seg16 |
| 36 | seg10:0x6bf6 | seg10:0x6bca | seg10:0x99f3 |
| 41 | seg10:0x6b65 | seg10:0x6b54 (joystick 0, info = far pointer in DGROUP:0x1e4c) | seg11:0x72c9 |

[BYTES seg10:0x6c57] [BYTES seg10:0x7f4e] [BYTES seg10:0x9956]

### Ordinal notes
- **1, load graphic:** ISPACE.DLL seg5:0x1b8e reads graphic resource `id` from a file or from IGRAPHIC.DLL, where it has type #4000. seg5:0x2080 turns it into a device bitmap with `CreateDIBitmap`, and the bitmap's height goes to ISPACE.DLL DGROUP:0x1c72 for ordinal 5. [BYTES ISPACE.DLL seg5:0x402] [BYTES ISPACE.DLL seg5:0x2080]
  - Ids 280 and 650 take separate paths. A `mode` or `arg` below −100 is a flag, and ISPACE.DLL removes it by adding 150. [BYTES ISPACE.DLL seg5:0x402]
  - The oracle shows `FindResource` calls for IGRAPHIC #4000 ids that return to ISPACE.DLL seg5:0x1c5e. [ORACLE newgame-0002]
  - **Resource source.** seg5:0x1b8e takes ids 220, 221 and 300 from ISPACE.DLL itself (instance ISPACE.DLL DGROUP:0x1a30) and every other id from IGRAPHIC.DLL (DGROUP:0x692), which has no 220, 221 or 300. It calls `FindResource(instance, id, 4000)`, requires the `BM` file header (0x4d42) and keeps the packed DIB after its 14 bytes. [BYTES ISPACE.DLL seg5:0x1c23 seg5:0x1c38 seg5:0x1c3d seg5:0x1c59 seg5:0x1cba] [BYTES IGRAPHIC.DLL resource table]
  - ISPACE.DLL #4000 220 is a 40×40 4-bpp DIB (colours 0–10 greys a0a0a0 … 323232, 11–14 white, 15 black); #4000 221 is 360×40 at 4 bpp, nine 40×40 cells (colours ffffff … 5a0000, 11–15 black); #4000 300 is 138×138 at 4 bpp (colours 0–9 the gold ramp ffe66e … 4c2800 with colour 0 unused, 10–14 the greys d7d7d6 b9baba 9e9e9e 737373 565552, 15 black). [BYTES ISPACE.DLL 4000 220] [BYTES ISPACE.DLL 4000 221] [BYTES ISPACE.DLL 4000 300]
  - **Arguments.** Ordinal 1 is pascal (`retf 0xe`). It stores max(BITSPIXEL, PLANES) in ISPACE.DLL DGROUP:0x1e9a and SIZEPALETTE in DGROUP:0x1e98 (32 and 16 in the probe). At depth ≥ 8 `mode` 3 becomes 0. Ids other than −2, 0x118 with a palette kind ≠ 0 and 0x28a, with `arg` ≥ −100, go to seg5:0x1b8e(hPal, id, name, d, 12, 12, 0): d = 3 for a kind-0 palette (DGROUP:0x1e92[slot] = 0) at depth ≥ 8, 2 for kind ≠ 0 and 1 below 8 bpp. At depth < 8 with `mode` 3 it adds ISPACE.DLL seg7:0x0(hPal, dib, arg) (pascal, `retf 6`), and with SIZEPALETTE ≥ 256 and `mode` ≠ 0 it adds seg5:0x75e. seg5:0x2080 then makes the device bitmap. So at 8 bpp and above the title's calls (300, 0, −1) and (300, 3, −1) are the same plain load. What seg7:0x0 and seg5:0x75e do is [OPEN]. [BYTES ISPACE.DLL seg5:0x42d seg5:0x43a seg5:0x458 seg5:0x469 seg5:0x475 seg5:0x47a seg5:0x491 seg5:0x4c6 seg5:0x4e2 seg5:0x562 seg5:0x597 seg5:0x5a0 seg5:0x5c5 seg5:0x5d1 seg5:0x5ec seg5:0x3649 seg5:0x367b] [ORACLE probe-0001]
  - The wrapper seg10:0x6c02(id, mode, arg) passes hPal DGROUP:0x920e, id, name DGROUP:0x7de (""), hdc DGROUP:0x13dc, `mode` and `arg`. When DGROUP:0x13dc is 0 it takes GetDC(main window) with the palette selected for the call, then restores the palette, releases the DC and zeroes DGROUP:0x13dc. [BYTES seg10:0x6c14 seg10:0x6c24 seg10:0x6c42 seg10:0x6c57]
- **2, render image:**
  - Id −1000 goes to ISPACE.DLL seg5:0x17c2(hPal, hdc, gap), with gap the second of the six words ([bp+0x16]). `sel` = 0 goes to the renderer at seg6:0x11c, other values go to seg6:0x78ba. [BYTES ISPACE.DLL seg6:0x0 seg6:0xd seg6:0x1d]
  - **Logo strip (id −1000).** INSPACE passes gap g = 6 (seg10:0x39a8). seg5:0x17c2 uses cell c = 40 and makes CreateCompatibleBitmap(14c + 15g, c + 2g) = 650×52, cleared with PatBlt BLACKNESS. Cell i occupies x = 6 + 46i … + 39, y = 6 … 45. It first copies graphic 220 into every cell 0–13 with BitBlt SRCCOPY. It then composites cells of graphic 221 over the ring with the ordinal 6 mask (1 where the pixel is black): BitBlt SRCAND 0x8800C6 from the mask, then SRCPAINT 0xEE0086 from the colour cells. Strip cells 1–4 get 221 cells 0–3. Cells 9, 8, 7 and 6 get 221 cells 0, 1, 2 and 3 by StretchBlt to (46(9 − k) + 45, 45) with extents (−40, −40), turned 180°. Cell 5 gets 221 cell 4 and cells 10–13 get 221 cells 5–8; cell 0 stays the bare ring. Nothing is rotated at run time: the frames are the drawn 221 cells and their 180° copies. [BYTES ISPACE.DLL seg5:0x17f5 seg5:0x1869 seg5:0x18bb seg5:0x18ea seg5:0x1946 seg5:0x1951 seg5:0x19dd seg5:0x19fb seg5:0x1a3c seg5:0x1a60 seg5:0x1a9e seg5:0x1abc seg5:0x1aff seg5:0x1b1d] [BYTES ISPACE.DLL 4000 220] [BYTES ISPACE.DLL 4000 221]
  - The mirrored StretchBlt targets x = 46(9 − k) + 45 and y = 45 with −40 extents. Under the Windows rule that a negative extent covers x + w + 1 … x, the turned cells land exactly on columns 46(9 − k) + 6 … + 45 and rows 6 … 45; the explicit −1 in the code implies this rule. [BYTES ISPACE.DLL seg5:0x1a03 seg5:0x1a14 seg5:0x1a16 seg5:0x1a19 seg5:0x1a1e] [REF Wine gdi32 tests test_StretchBlt]
  - Wrapper seg10:0x6ee6 forces the last of the six words to 0 or 1. It keeps the result per slot in DGROUP:0x3ad0 and a memory DC per slot in DGROUP:0x39e8. [BYTES seg10:0x6f7f] [BYTES seg10:0x6f8e]
  - For ids other than −1000, the meaning of the six words and three bytes is [OPEN].
- **3, LoadBitmap:** the instance is ISPACE.DLL DGROUP:0x692, the IGRAPHIC.DLL handle. When `name` equals `SD_LOGO` (ISPACE.DLL DGROUP:0x26), the instance is ISPACE.DLL's own (ISPACE.DLL DGROUP:0x1a30). [BYTES ISPACE.DLL seg5:0x612] The oracle sees `LoadBitmap` of `ROCKS`, `BW_BLOCKS`, `BW_MBLOCKS` and `OUT_SIGN` return to ISPACE.DLL seg5:0x646. [ORACLE newgame-0002]
  - ISPACE.DLL BITMAP SD_LOGO is 130×32 at 1 bpp with colour table [ffffff, 000000], the letters at index 0. INSPACE loads it through seg10:0x6d38 into DGROUP:0x95f4 ([shell.md](shell.md#intro-and-title-screen)). [BYTES ISPACE.DLL BITMAP SD_LOGO] [BYTES seg15:0x591f seg15:0x5927 seg10:0x6d46]
  - ISPACE.DLL loads the font bitmap `ALPHABET` (ISPACE.DLL DGROUP:0x192) itself: inside ordinal 4 command 0, while ISPACE.DLL DGROUP:0x1ea2 is 0, seg5:0x3712 calls seg5:0x612 and stores the handle there. seg5:0x3aae deletes and zeroes it at seg5:0x3b95–0x3b9e. Its only other reference is the read in ordinal 33 at seg8:0x3cb, with no test for 0. [BYTES ISPACE.DLL seg5:0x3707 seg5:0x3712 seg5:0x3717 seg5:0x637 seg5:0x3b95 seg5:0x3b9e seg8:0x3cb]
  - INSPACE keeps its own `ALPHABET` handle in DGROUP:0x912c (loaded at seg6:0x199e, deleted at seg10:0x50a); nothing draws from it. [BYTES seg6:0x1993 seg6:0x199e seg10:0x503 seg10:0x50a seg10:0x513]
- **4, palette manager:** a jump table at ISPACE.DLL seg5:0x3578 picks the command routine. [BYTES ISPACE.DLL seg5:0x3573]

  | cmd | ISPACE.DLL routine | Arguments it reads | Effect |
  |---|---|---|---|
  | 0 | seg5:0x3596 | a | Create the game palette (`GetDeviceCaps`, `GetSystemPaletteEntries`, `CreatePalette`) and return it. INSPACE stores it in DGROUP:0x920e |
  | 1 | seg5:0x44ac | b, c | Load a palette scheme. INSPACE passes a dword |
  | 2 | seg5:0x3aae | none | Delete the palette and its working objects |
  | 3 | seg5:0x4378 | hwnd, hdc | Apply the working colours with `AnimatePalette` |
  | 4 | seg5:0x484a | b, c, hdc | Change a colour range |
  | 5 | seg5:0x474c | b, hdc | Select stored colour set `b` |
  | 6 | seg5:0x3dee | a, b, c | Return a computed value |
  | 7 | seg5:0x446e | c | Remap through ISPACE.DLL seg5:0x61b2 |
  | 8 | seg5:0x3bae | c | Not known |

  [BYTES ISPACE.DLL seg5:0x34d8] [BYTES ISPACE.DLL seg5:0x355c]
  - Every command routine except command 0's also receives `hPal`. Command 0's routine receives only `a`. Before it runs, the dispatcher stores `hPal` in ISPACE.DLL DGROUP:0x1ac when `hwnd` is non-zero and that word is −1. [BYTES ISPACE.DLL seg5:0x34d8] [BYTES ISPACE.DLL seg5:0x34e8] [BYTES ISPACE.DLL seg5:0x34eb]
  - Wrappers seg10:0x6da6, seg10:0x6dd4 and seg10:0x6e76 call command 3 after their own command. [BYTES seg10:0x6dc9] [BYTES seg10:0x6e95]
  - The oracle's `AnimatePalette(hpal, 10, 236, …)` calls return to ISPACE.DLL seg5:0x441f, inside the command 3 routine. [ORACLE depth8-0001]
  - Commands 1, 4, 5, 6, 7 and 8 load a scheme, save and optionally blank entries 10–245, restore a stored set, allocate a colour, allocate a colour ramp, and swap black and (1, 11, 111) entries of a kind-1 palette ([shell.md](shell.md#palette)). [BYTES ISPACE.DLL seg5:0x45f8 seg5:0x4675 seg5:0x491f seg5:0x481b seg5:0x3ff2 seg5:0x643e seg5:0x3ca4 seg5:0x3d0e]
- **6, mask:** creates a 1-bpp bitmap of the source's size and copies the source into it with background colour RGB(0,0,0). Source pixels of that colour become 1, all others 0. [BYTES ISPACE.DLL seg5:0x650] [REF Win16 SDK BitBlt]
- **7, module and sound manager:** a jump table at ISPACE.DLL seg9:0x96 picks the command. [BYTES ISPACE.DLL seg9:0x91]
  - Module handles live at ISPACE.DLL DGROUP:0x68c + 2·k, with reference counts at ISPACE.DLL DGROUP:0x19d8 + 2·k. k is 0 for `SND_FX.DLL`, 1 for `<Language>_FX.DLL`, 2 for `<Language>_LANG.DLL` and 3 for `IGRAPHIC.DLL`. [BYTES ISPACE.DLL seg9:0x17b] [BYTES ISPACE.DLL seg9:0x34a]
  - `SndPath`, `Path` and `LocalPath` come from INSPACE.INI. [BYTES ISPACE.DLL seg9:0x141]

  | cmd | Effect |
  |---|---|
  | 0 | Start sound. A negative `arg` low word is negated and sets a reduced mode. A non-zero `arg` goes to ISPACE.DLL DGROUP:0x19c8 (two words). It loads modules 0 and 1, calls ISPACE.DLL seg9:0x4e0 when sound is on (ISPACE.DLL DGROUP:0x2280), and prepares 82 (0x52) sound slots, or only slot 0x23 in the reduced mode. INSPACE passes DGROUP:0x5a0, which holds the words 3200 and 3200 |
  | 1 | Stop sound: release the 82 slots, release modules 0 and 1, free the sample buffer and set the saved system MIDI and wave volumes |
  | 2 | Call ISPACE.DLL seg9:0x1174, then reset the 82 slots |
  | 3 | Focus lost: stop the playing effects through ISPACE.DLL seg9:0x1174, which flushes the WaveMix channel or calls `sndPlaySound(NULL, 0)`. Read the current MIDI and wave volumes into ISPACE.DLL DGROUP:0x6a8 and ISPACE.DLL DGROUP:0x6a4, and pause the music, when it is playing, with `mciSendCommand(MCI_PAUSE)` (ISPACE.DLL seg9:0x3a48). Set the saved system volumes (ISPACE.DLL DGROUP:0x6b0, ISPACE.DLL DGROUP:0x6ac) and call `WaveMixActivate(FALSE)` |
  | 4 | Focus gained: read the system volumes into ISPACE.DLL DGROUP:0x6b0 and ISPACE.DLL DGROUP:0x6ac, set the game volumes from ISPACE.DLL DGROUP:0x6a8 and ISPACE.DLL DGROUP:0x6a4, call `WaveMixActivate(TRUE)` and resume music. With INI `ForceVol` set, the saved system volumes become 0xFFFF, so focus loss and stop set the system volumes to maximum |
  | 5, 7 | Take a reference to `<Language>_LANG.DLL` (5) or `IGRAPHIC.DLL` (7), loading it on first use; return success |
  | 6, 8 | Release those references |
  | 9 | `WaveMixPump`, while a WaveMix session is open and active |
  | 10 | Re-read INI `SaverVolume` into ISPACE.DLL DGROUP:0x69e and recompute the volumes |
  | 11 | Music: `arg` holds the track number (−3, −2 and −1 are control codes), which ISPACE.DLL seg9:0x3906 handles |

  [BYTES ISPACE.DLL seg9:0x448] [BYTES ISPACE.DLL seg9:0x6a0] [BYTES ISPACE.DLL seg9:0x73a] [BYTES ISPACE.DLL seg9:0xaf2] [BYTES ISPACE.DLL seg9:0xb04] [BYTES ISPACE.DLL seg9:0xb1f] [BYTES ISPACE.DLL seg9:0x11e8] [BYTES ISPACE.DLL seg9:0x3a80] [BYTES ISPACE.DLL seg9:0xbde] [BYTES ISPACE.DLL seg9:0x360a] [BYTES ISPACE.DLL seg9:0x6e] [BYTES ISPACE.DLL seg9:0x7c]
  - Commands 3 and 4 do nothing while sound is off (ISPACE.DLL DGROUP:0x2280 = 0). [BYTES ISPACE.DLL seg9:0xafc] [BYTES ISPACE.DLL seg9:0xbb0]
  - Ordinal 8 uses the two words from command 0 as wrap-around periods for sound positions. Whether 3200 × 3200 is the world size is [OPEN]. [BYTES ISPACE.DLL seg9:0x76c] [BYTES DGROUP:0x5a0]
- **8, sound position:** with `a` non-zero, ordinal 8 stores `a`, `b` and the two words of `area` (DGROUP:0x90ea, the play area's width and height). It computes left and right volumes from the offset of `p` from the point (`a` + width/2, `b` + height/2). Each offset wraps at the command-0 periods. [BYTES ISPACE.DLL seg9:0x76c] [BYTES seg10:0x7d3a]
  - A `p` low word of −6000 gives fixed volumes 80, 80 and 100. Nothing happens unless the sound mode ISPACE.DLL DGROUP:0x2280 is at least 2. [BYTES ISPACE.DLL seg9:0x76c]
  - INSPACE calls it once per wave frame (seg7:0xcaf → seg10:0x7b48). [BYTES seg7:0xcaf]
  - That `p` is a world position and (`a`, `b`) the view origin is [OPEN].
- **9, play sound:** an `id` of −1 plays `SndPath` + `name` + `.WAV`. Other ids load their sound slot on demand, and ordinal 9 passes `p`, `r`, `s` and `area` to ordinal 8 as `p`, `a`, `b` and `area`. [BYTES ISPACE.DLL seg9:0xc42] [BYTES seg10:0x7b11]
  - It returns 0 at once while sound is off (ISPACE.DLL DGROUP:0x2280 = 0), and for `id` −1 while ISPACE.DLL DGROUP:0x696 is 0. [BYTES ISPACE.DLL seg9:0xc50] [BYTES ISPACE.DLL seg9:0xc63]
  - Before it plays, most ids pass two checks. When ISPACE.DLL DGROUP:0x6a0 is 0, ordinal 9 returns 0 without playing. Otherwise the sound plays only once `now` has reached the busy-until time (dword ISPACE.DLL DGROUP:0xcf2). INSPACE passes its `GetCurrentTime` sample DGROUP:0x1e50 as `now`. [BYTES ISPACE.DLL seg9:0xce5] [BYTES ISPACE.DLL seg9:0xcec] [BYTES ISPACE.DLL seg9:0xcf6] [BYTES seg10:0x7b08]
  - Once the checks pass or are skipped and ISPACE.DLL seg9:0x10ae does not return −1, ordinal 9 sets the busy-until time to `now`, adds a per-id hold when `q` is non-zero (for example 900 ms for ids 0x26, 0x2d and 0x2f), and then calls the player ISPACE.DLL seg9:0x11f4. The time stays set when the play fails. Ids 0x69 and 0x6e play nothing: they clear the busy-until time to 0 and return 1. [BYTES ISPACE.DLL seg9:0xea6] [BYTES ISPACE.DLL seg9:0xec8] [BYTES ISPACE.DLL seg9:0x101b] [BYTES ISPACE.DLL seg9:0xdaa]
  - seg9:0xa9e computes ISPACE.DLL DGROUP:0x6a0 from `SaverVolume` (ISPACE.DLL DGROUP:0x69e) and ISPACE.DLL DGROUP:0x6a2: it is `SaverVolume` while ISPACE.DLL DGROUP:0x6a2 is at most 50, and `SaverVolume` × 2 × (100 − ISPACE.DLL DGROUP:0x6a2) / 100 above that. [BYTES ISPACE.DLL seg9:0xaab] [BYTES ISPACE.DLL seg9:0xaca]
  - Ids 0x68, 0x6f, 0x70 and 0x75 skip both checks. They set a local word to 0x14, 0x14, 8 and 7, which ordinal 9 passes on to ISPACE.DLL seg9:0x10ae and seg9:0x11f4. [BYTES ISPACE.DLL seg9:0xcdb] [BYTES ISPACE.DLL seg9:0xdbe] [BYTES ISPACE.DLL seg9:0xe6a] [BYTES ISPACE.DLL seg9:0xfef]
  - For some ids it picks one of three variants with ISPACE.DLL's own `rand()` ([rng.md](rng.md)). [BYTES ISPACE.DLL seg9:0xcca]
- **10, dialogs:** the dispatcher first stores `x6` and `cmd` (ISPACE.DLL DGROUP:0x2136, ISPACE.DLL DGROUP:0x213c) and copies `x4` and `x5` into globals for some commands. [BYTES ISPACE.DLL seg9:0x1468]
  - Command 0 opens Options/Setup (#0064) from the language DLL with dialog procedure ordinal 11. [BYTES ISPACE.DLL seg9:0x15ae]
  - Commands 1–8, 11, 12 and 14 go to ISPACE.DLL seg10:0x42c6. Command 9 goes to ISPACE.DLL seg10:0x5b24. Command 15 stores `hPal`, `x4` and `x5` in ISPACE.DLL DGROUP:0x2050–0x2054 and returns 1. Commands 10, 13 and values above 15 do nothing. [BYTES ISPACE.DLL seg9:0x1508] [BYTES ISPACE.DLL seg9:0x14c3]
  - INSPACE's command-0 call passes the effective `GameSpeed` (DGROUP:0x8ffa) as `x4` and `Calibrate` (DGROUP:0x8ffc) as `x5`. ISPACE.DLL stores them in ISPACE.DLL DGROUP:0x2278 and ISPACE.DLL DGROUP:0x227a, and no instruction reads those words. [BYTES seg10:0x7f42] [BYTES ISPACE.DLL seg9:0x1586]

  | cmd | Template in `<Language>_LANG.DLL` (ENG_LANG caption) | Dialog procedure |
  |---|---|---|
  | 1 | #010b Restore Game; when it returns 1, #0141 New Game | ordinal 25 (seg10:0xec2), then ordinal 22 (seg10:0xf26) |
  | 2 | #01b0 Inner Space Demo Options | ordinal 23 (seg10:0x3346) |
  | 3 | #00a7 Save Game | ordinal 26 (seg10:0xdc0) |
  | 4 | #024d New Demo | ordinal 22 |
  | 5, 7 | #00f4 Inner Space (buttons Save & Exit, Exit, Cancel) | ordinal 27 (seg10:0x103c) |
  | 6 | #011e New Ship Body | ordinal 28 (seg10:0xf9c) |
  | 8 | #02b5 Order Inner Space Now!; when it returns 1, #03dc Important Information! | ordinal 30 (seg10:0x664), then ordinal 39 (seg10:0x124a) |
  | 11 | #03dc Important Information! | ordinal 39 |
  | 12 | #031f Registration Benefits | ordinal 40 (seg10:0x5f96) |
  | 14 | #0750 Inner Space Plus Pack! | ordinal 40 |

  [BYTES ISPACE.DLL seg10:0x4386] [BYTES ISPACE.DLL seg10:0x4426] [BYTES ENG_LANG.DLL DIALOG 267] [BYTES ENG_LANG.DLL DIALOG 244]
  - seg10:0x42c6 treats every command it does not list as 3. The dispatcher never passes it 9, 10, 13 or 15. [BYTES ISPACE.DLL seg10:0x4414] [BYTES ISPACE.DLL seg9:0x151c]
  - The template instance is ISPACE.DLL DGROUP:0x690, the module handle of `<Language>_LANG.DLL` (k = 2). [BYTES ISPACE.DLL seg10:0x4374]
  - The oracle shows Restore Game opened from ISPACE.DLL seg10:0x438b and New Game from seg10:0x442b, the return addresses of the two `DialogBox` calls. [ORACLE newgame-0001]
  - What command 9 does is [OPEN].
- **12, render image:** stores `hdc` in ISPACE.DLL DGROUP:0x1a32 and clears DGROUP:0x1ea4. It returns 0 when byte b1 ([bp+8]) or b2 ([bp+6]) is 0xff, and otherwise calls ISPACE.DLL seg7:0x12d8 (pascal, `retf 0x16`, 15374 bytes, seg7:0x12d8–0x4ee5) without `hdc`. [BYTES ISPACE.DLL seg6:0xc4 seg6:0xce seg6:0xda seg6:0xe0 seg6:0x10d]
  - seg7:0x12d8 sets ISPACE.DLL DGROUP:0x188 = 1 and loads the DIB with seg5:0x1b8e(hPal, id, name, d 0, b1 1, b2 12, palette kind ≠ 0). Scheme 1 of DGROUP:0x117c is graphic 300's own gold ramp, so its colours stay unchanged; at 8 bpp the load writes ramps into palette entries 185–229 and animates ([shell.md](shell.md#palette)). When the first word a1 is negative, both a1 and a2 become −a × biHeight; the title passes −2, −2, so 2 × 138 = 276. [BYTES ISPACE.DLL seg7:0x1367 seg7:0x138d seg7:0x143e seg7:0x1444] [ORACLE probe-0001]
  - The output for graphic 300 is a smoothed 2× upscale that uses palette entries 188–229 ([palette-tables.md](palette-tables.md#title-palette-at-8-bpp)); the exact per-pixel rule is [OPEN] ([shell.md](shell.md#open-questions)). [ORACLE probe-0001]
- **14, palette scale:** two branches, picked by the palette's word in ISPACE.DLL DGROUP:0x1e92. [BYTES ISPACE.DLL seg5:0x50cc]
  - When that word is 0, it multiplies the red, green and blue bytes of a range of the working palette by `percent`/100, clamped to 255. `mode` 3 selects the range.
  - Otherwise it caps `percent` at 40 and blends between two stored colour sets. For `mode` 0 it blends ten colours from ISPACE.DLL DGROUP:0x117c. For `mode` 1 and every other mode it blends five colours from ISPACE.DLL DGROUP:0x1cdc. [BYTES ISPACE.DLL seg5:0x5268] [BYTES ISPACE.DLL seg5:0x52af] [BYTES ISPACE.DLL seg5:0x5308] [BYTES ISPACE.DLL seg5:0x532d] [BYTES ISPACE.DLL seg5:0x53a2]
  - INSPACE calls it in pairs, mode 3 then mode 2 with a third of the step, as stepwise fades of entries 185–229 and 230–245 ([shell.md](shell.md#palette)). [BYTES seg15:0x299 seg15:0x2b0 seg15:0x3e99 seg15:0x3eb0] [BYTES ISPACE.DLL seg5:0x50cc]
- **15, direction frames:** `frame` 0–15 maps to one of five cells of a 90-pixel strip: 0 and 8 take cell 0, and 4 and 12 take cell 4. The other directions reuse cells 1–3 in mirror pairs. Mirrored directions are drawn with `StretchBlt` and negative extents. [BYTES ISPACE.DLL seg5:0x11a4]
  - The sprite blits use two ROPs. 0x8800C6 (SRCAND) goes with the `StretchBlt` calls at ISPACE.DLL seg5:0x140b, seg5:0x144d, seg5:0x1483, seg5:0x165a, seg5:0x168f, seg5:0x16cb and seg5:0x175c and the `BitBlt` at seg5:0x14fe. 0x220326 goes with the blits at seg5:0x133e, seg5:0x1372, seg5:0x15b4 and seg5:0x15fd. Which pass draws what is [OPEN]. [BYTES ISPACE.DLL seg5:0x140b] [BYTES ISPACE.DLL seg5:0x14fe] [BYTES ISPACE.DLL seg5:0x133e] [BYTES ISPACE.DLL seg5:0x15fd]
  - With `flag` non-zero, the export first creates a memory bitmap, fills it with `PatBlt` BLACKNESS and copies into it with `BitBlt` SRCCOPY. The sprite blits follow. [BYTES ISPACE.DLL seg5:0x11f6] [BYTES ISPACE.DLL seg5:0x124c] [BYTES ISPACE.DLL seg5:0x1276] [BYTES ISPACE.DLL seg5:0x12d9]
- **17, sprite:** the wrapper passes the scale level DGROUP:0x9156 (INI `Size`) as the last argument. The export calls `timeGetTime`. [BYTES seg10:0x7361] [BYTES ISPACE.DLL seg6:0x5548] Its other arguments are [OPEN].
- **18:** loads graphic resource 280 through ISPACE.DLL seg5:0x1b8e and builds a bitmap from it. Its arguments are [OPEN]. [BYTES ISPACE.DLL seg6:0x5f4a]
- **24, string service:** INSPACE's callers read WIN.INI `CDDrive` and `PDev` and SYSTEM.INI `[386Enh]` keys around it. [BYTES ISPACE.DLL seg10:0x48d2] [BYTES seg10:0x88cc] Its role is [OPEN].
- **29, collision arithmetic:** works on positions and velocities through pointers and uses ISPACE.DLL seg2:0x850, an integer approximation of √(a² + b²). [BYTES ISPACE.DLL seg2:0x3c] [BYTES ISPACE.DLL seg2:0x850] The exact physics is [OPEN].
- **32, scene renderer:** draws a global block of 0x26-byte records with fonts, pens and bitmaps. [BYTES ISPACE.DLL seg8:0x0] Which scene it draws is [OPEN].
- **33, bitmap font:** maps each character to a glyph of the font bitmap in ISPACE.DLL DGROUP:0x1ea2. It stretches the glyph into `hdc` with a brush of `colour`. `align` 2 right-aligns and 6 centres. The INSPACE wrapper passes cells of 18×20, 12×18 or 9×10 pixels for fonts 0, 1 and 2. [BYTES ISPACE.DLL seg8:0x37a] [BYTES seg10:0x94d6]
  - **Font bitmap.** IGRAPHIC.DLL BITMAP `ALPHABET` is a 1-bpp BI_RGB DIB of 828×20 with colour table [ffffff, 000000]: glyph pixels are index 0 (white), the background index 1. It is one row of 46 cells of 18×20; cell n starts at (18n, 0). Cells 0–25 are A–Z, 26 `\`, 27 '(', 28 ')', 29 ':', 30 '!', 31 '?', 32 ',', 33 '.', 34 '-', 35–44 '0'–'9' and 45 '%'; letters and digits are slanted, segmented shapes. [BYTES IGRAPHIC.DLL BITMAP ALPHABET] [BYTES ISPACE.DLL seg8:0x519 seg8:0x520 seg8:0x522]
  - **Frame.** Ordinal 33 is pascal (`retf 0x16`): hdc [bp+0x1a], hPal [bp+0x18], cell height [bp+0x16], cell width [bp+0x14], align [bp+0x12], colour dword [bp+0xe], x [bp+0xc], y [bp+0xa], text far [bp+6]. [BYTES ISPACE.DLL seg8:0x37a seg8:0x5a9 seg8:0x4a7]
  - **Characters.** With c the sign-extended byte: '0'–'9' → c − 0x0d; 'a'–'z' → c − 0x61 and 'A'–'Z' → c − 0x41 (case folded); ' ' and '_' → 0x84 (blank); '!' → 30, '%' → 45, '(' → 27, ')' → 28, ',' → 32, '-' → 34, '.' → 33, '/' → 26, ':' → 29, '?' → 31, `\` → 26. Every other byte, control codes, quotes, `+ # & ; = @ [ ]`, 0x7b–0x7f and all bytes ≥ 0x80 included, maps to 0xff and is skipped: nothing is drawn and x does not advance. So '/' draws the backslash glyph. [BYTES ISPACE.DLL seg8:0x548 seg8:0x54e seg8:0x562 seg8:0x42a seg8:0x434 seg8:0x444 seg8:0x44a seg8:0x47c seg8:0x48a seg8:0x491 seg8:0x4c1 seg8:0x500]
  - **Start x.** `align` 0 → x; 2 → x − len·cw; 6 → x − (len·cw)/2, signed and truncating, with len = `lstrlen`, which counts skipped characters. Any other value leaves x as the uninitialised word [bp−8]. y is always the top of the cell. All 39 calls of the wrapper pass align 0, 2 or 6. [BYTES ISPACE.DLL seg8:0x388 seg8:0x39c seg8:0x3a5 seg8:0x3ff seg8:0x410 seg8:0x414 seg8:0x422]
  - **Drawing.** Setup: SelectObject(hdc, CreateSolidBrush(colour)) with the colour unchanged; mdc = CreateCompatibleDC(hdc); SelectObject(mdc, ALPHABET); when hPal ≠ 0, oldPal = SelectPalette(mdc, hPal, FALSE); SetStretchBltMode(hdc, COLORONCOLOR), never restored. For each character (lstrlen is re-read on every pass): cell 0xff draws nothing; cell 0x84 does PatBlt(hdc, cx, y, cw, ch, BLACKNESS); any other cell does StretchBlt(hdc, cx, y, cw, ch, mdc, 18·cell, 0, 18, 20, MERGECOPY 0x00C000CA). After a blank or a glyph cx += cw: no spacing, no mask pass and no SetTextColor or SetBkColor. Teardown: DeleteObject(SelectObject(hdc, oldBrush)), SelectObject(mdc, oldBmp), SelectPalette(mdc, oldPal, FALSE) when the saved [bp−0x10] ≠ 0, DeleteDC(mdc). [bp−0x10] is written only when hPal ≠ 0, so with hPal = 0 the restore test reads an uninitialised stack word. [BYTES ISPACE.DLL seg8:0x3b1 seg8:0x3c2 seg8:0x3cf seg8:0x3e5 seg8:0x3ea seg8:0x3f2 seg8:0x496 seg8:0x4ba seg8:0x52a seg8:0x52f seg8:0x535 seg8:0x53e seg8:0x571 seg8:0x582 seg8:0x587 seg8:0x595 seg8:0x59d]
  - **Colours.** A monochrome-to-colour StretchBlt maps source bit 1 (the white glyph ink) to the DC's background colour and bit 0 to its text colour. With text black and background white the glyph is white on black, and MERGECOPY (source AND brush) draws the glyph in the brush colour and the rest of the cw × ch cell black: each cell is an opaque box. The result depends on the colours current in DGROUP:0x13dc, which other routines change and restore (for example seg15:0x694e–0x6994 set background 0 and text 0x02FF8080, restored at seg15:0x6aab and seg15:0x6ac0). At 8 bpp the AND works on palette indices (black 0, white 255), so PALETTERGB brushes and static colours give the same result. [BYTES ISPACE.DLL seg8:0x524 seg8:0x52a seg8:0x4ba] [BYTES seg15:0x694e seg15:0x6aab seg15:0x6ac0] [REF Win16 SDK BitBlt]
  - **Wrapper.** seg10:0x94c4 is cdecl: font, align, colour dword, x, y, text far. Font 0 passes the cell 18×20, font 1 12×18 and font 2 9×10; any other font leaves the cell uninitialised. It passes hdc DGROUP:0x13dc and hPal DGROUP:0x920e, calls through DGROUP:0x3c74 and converts no colour, so the caller chooses RGB (0x0000FF00 at seg6:0x1e13, 0x000000FF at seg15:0x6b90, 0x00FF008C at seg15:0x6909) or PALETTERGB (0x0200FF00 at seg9:0x2333, 0x020080FF at seg9:0x6a52, 0x02FFFFFF at seg6:0x655b, 0x02003DF5 at seg18:0x6144). Font 0's cell equals the glyph, so font 0 text is a 1:1 MERGECOPY of the glyphs; fonts 1 and 2 shrink both axes under COLORONCOLOR. [BYTES seg10:0x94c4 seg10:0x94d6 seg10:0x94e0 seg10:0x94ea seg10:0x951b seg6:0x1e13 seg15:0x6b90 seg15:0x6909 seg9:0x2333 seg9:0x6a52 seg6:0x655b seg18:0x6144]
  - Which rows and columns COLORONCOLOR keeps when fonts 1 and 2 shrink a glyph is decided by the driver, not the code (font-2 call sites seg9:0x233d and seg18:0x1d70; the HUD counter in depth8-0002 does not match font 2). Whether any call site runs with non-default text or background colours (the cell background would then be tinted), and whether a plain-RGB brush that is not a static colour (for example 0x00FF008C at seg15:0x6909, or phase 5's RGB(255 − 13i, 0, 12i + 11)) dithers at 8 bpp, are [OPEN]. [BYTES seg9:0x233d seg18:0x1d70 seg15:0x6909] [ORACLE depth8-0002]
- **34, frame:** fills the rectangle with `PatBlt`, then draws the right and bottom edges with one pen and the top and left edges with another. [BYTES ISPACE.DLL seg8:0x5ac]
  - It takes hdc, x, y, w, h and style (`retf 0xc`) and fills PatBlt(x, y, w, h, PATCOPY). Pens are PS_SOLID, width 1, with the grey byte in R, G and B. Pen 1 draws (x + w, y) → (x + w, y + h) → (x, y + h): column x + w rows y … y + h − 1 and row y + h columns x + 1 … x + w. Pen 2 (none for style 0) draws (x + w, y) → (x, y) → (x, y + h): row y columns x + 1 … x + w and column x rows y … y + h − 1. Style 2 then draws (x, y + h) → (x + w, y + h) in grey 0x808080. Any style above 3 uses uninitialised brush and pens. [BYTES ISPACE.DLL seg8:0x602 seg8:0x60b seg8:0x611 seg8:0x633 seg8:0x657 seg8:0x6c2 seg8:0x707 seg8:0x71a]

  | style | Fill | Right and bottom | Top and left |
  |---|---|---|---|
  | 0 | black | RGB(0xC0,0xC0,0xC0) | none |
  | 1 | light grey | black | white |
  | 2 | light grey | black | white, plus a grey line along the bottom |
  | 3 | light grey | white | RGB(0x80,0x80,0x80) |

- **35, bar:** takes hdc, x, y, w, h and value and computes fw = value·w/100 (signed). It always draws white (x − 2, y + h + 1, w + 4, 1) and (x + w + 1, y − 2, 1, h + 3), dark grey (x − 2, y − 2, 1, h + 3) and (x − 1, y − 2, w + 2, 1), and black (x + fw, y, w − fw, h). If fw = 0 it stops there. Otherwise it draws green 0x00FF00 (x + 3, y + 3, fw − 6, h − 6), yellow 0x00FFFF (x, y, fw, 3) and (x, y, 3, h), dark green 0x008000 (x, y + h − 3, fw, 3) and (x + fw − 3, y, 3, h), and yellow pixels at (x, y + h − 3), (x, y + h − 2), (x + 1, y + h − 3), (x + fw − 3, y), (x + fw − 2, y) and (x + fw − 3, y + 1). [BYTES ISPACE.DLL seg8:0x768 seg8:0x783 seg8:0x7c8 seg8:0x7e8 seg8:0x886 seg8:0x8cd seg8:0x90d seg8:0x972 seg8:0x9bc seg8:0xa26]
- **36, mirror blit:** a negative `w` or `h` mirrors the copy. ISPACE.DLL seg5:0x62 builds the transformed bitmap. [BYTES ISPACE.DLL seg10:0x2e1a] INSPACE reaches it only from seg10:0x9962, its StretchBlt helper, through seg10:0x6bca ([shell.md](shell.md#intro-and-title-screen)). [BYTES seg10:0x99f3 seg10:0x6bf6]
- **41, joystick:** returns 0xA5 for a joystick number of 2 or more, and 0xA7 with a `MessageBeep` when the read fails. These are the values of `JOYERR_PARMS` and `JOYERR_UNPLUGGED`. On success it fills `info` with two position words and four button bytes and returns 0. [BYTES ISPACE.DLL seg9:0x3e8a] [REF Win16 SDK mmsystem.h]
  - It always scales each position word to position × 771 / 1000 + 14 (ISPACE.DLL DGROUP:0x229e and ISPACE.DLL DGROUP:0x22a0). A calibration, when set up, scales the result further. [BYTES ISPACE.DLL seg9:0x3ed4] [BYTES ISPACE.DLL seg9:0x3f06] [BYTES ISPACE.DLL seg9:0x3f51]

## What ISPACE.DLL owns
| Area | Owner | Evidence |
|---|---|---|
| Bitmap loading from resources and files, and conversion to device bitmaps | ISPACE.DLL: ordinals 1, 2, 12, 17 and 18 through seg5:0x1b8e and seg5:0x2080 | [BYTES ISPACE.DLL seg5:0x1b8e] [BYTES ISPACE.DLL seg5:0x2080] |
| Sprite helpers: masks, direction frames, mirror blit, bitmap font, frames, bars | ISPACE.DLL: ordinals 6, 15 and 33–36 | see Ordinal notes |
| Drawing the game world each frame | INSPACE. It calls `BitBlt`, `StretchBlt` and `PatBlt` itself with the bitmaps that ISPACE.DLL returns | [BYTES INSPACE.EXE relocation records] |
| Palettes: create, animate, fade | ISPACE.DLL: ordinals 4 and 14. INSPACE selects DGROUP:0x920e into its DCs and calls `RealizePalette` | [BYTES ISPACE.DLL seg5:0x3596] [BYTES seg2:0x1c8f] |
| Sound effects, WaveMix, MIDI music, volumes | ISPACE.DLL: ordinals 7–9 | [BYTES ISPACE.DLL seg9:0x3677] |
| Options/Setup and its sub-dialogs | ISPACE.DLL: ordinal 10 command 0, dialog procedures 11, 37, 38 and 42 | [BYTES ISPACE.DLL seg9:0x15ae] |
| Restore Game, New Game, Save Game, exit, demo and order dialogs | ISPACE.DLL: ordinal 10 | [BYTES ISPACE.DLL seg10:0x4386] |
| Reading and writing saved-game data | [OPEN]. Both modules do file I/O. INSPACE calls `OpenFile` 20 times, `_lread` once and `_lwrite` twice. Its runtime library's open routine seg5:0xe64 calls `DOS3Call` to open (seg5:0xea6, seg5:0x1075) and create (seg5:0x102f) files. It also reads one byte (seg5:0xfa5) to strip a trailing Ctrl-Z and truncates with zero-length writes (seg5:0xf2c, seg5:0xfdf). ISPACE.DLL calls `OpenFile` 8 times, `_lread` 9 times and `_lwrite` once, in seg10:0x4696 | [BYTES seg5:0xea6] [BYTES seg5:0x102f] [BYTES seg5:0xf2c] [BYTES seg5:0xfb3] [BYTES ISPACE.DLL seg10:0x47dc] |
| Joystick reading | ISPACE.DLL: ordinal 41 | [BYTES ISPACE.DLL seg9:0x3e8a] |
| Frame pacing and game time | INSPACE ([timing.md](timing.md)) | [BYTES seg7:0x0] |

- **WaveMix:** ISPACE.DLL seg9:0x362e loads `WAVEMIX.DLL` once. It checks with `OpenFile(<Windows directory>\WAVEMIX.DLL, OF_EXIST)` that the file exists there, and skips WaveMix when it does not. Otherwise it calls `LoadLibrary("WAVEMIX.DLL")` with the bare name, stores the handle in ISPACE.DLL DGROUP:0x6b4, and resolves eleven functions by name from the name table at ISPACE.DLL DGROUP:0x772. [BYTES ISPACE.DLL seg9:0x364a] [BYTES ISPACE.DLL seg9:0x366b] [BYTES ISPACE.DLL seg9:0x3671] [BYTES ISPACE.DLL seg9:0x3677]

  | Function | Pointer (ISPACE.DLL DGROUP) |
  |---|---|
  | WaveMixGetInfo | 0x17cc |
  | WaveMixConfigureInit | 0x17d0 |
  | WaveMixOpenChannel | 0x19d0 |
  | WaveMixActivate | 0x17d4 |
  | WaveMixFlushChannel | 0x19e0 |
  | WaveMixOpenWave | 0x19cc |
  | WaveMixPlay | 0x19d4 |
  | WaveMixFreeWave | 0x1730 |
  | WaveMixPump | 0x1738 |
  | WaveMixCloseChannel | 0x1718 |
  | WaveMixCloseSession | 0x1734 |

- **Imports:** among INSPACE.EXE, ISPACE.DLL and both FACTORY.EXE builds, only ISPACE.DLL imports `CreatePalette` (GDI.360), `AnimatePalette` (GDI.367), `GetSystemPaletteEntries` (GDI.375), `GetDIBits` (GDI.441) and `CreateDIBitmap` (GDI.442). None of the four imports `StretchDIBits` (GDI.439) or `SetDIBitsToDevice` (GDI.443). [BYTES ISPACE.DLL relocation records] [BYTES INSPACE.EXE relocation records] [BYTES FACTORY.EXE relocation records] [BYTES theworks/FACTORY.EXE relocation records]
  - ISPACE.DLL's MMSYSTEM imports are `sndPlaySound`, `midiOutGetDevCaps`, `midiOutGetVolume`, `midiOutSetVolume`, `waveOutGetNumDevs`, `waveOutGetDevCaps`, `waveOutGetVolume`, `waveOutSetVolume`, `timeGetTime` and `mciSendCommand`. [BYTES ISPACE.DLL relocation records]
- **Timer:** ISPACE.DLL sets one window timer, timer 1 at 50 ms on the window in ISPACE.DLL DGROUP:0x21ae, with `SetTimer` at ISPACE.DLL seg9:0x46d4. It sits in seg9:0x4298, which the Joystick dialog procedure (ordinal 42) calls, and seg9:0x4712 kills it. [BYTES ISPACE.DLL seg9:0x46d4] [BYTES ISPACE.DLL seg9:0x48d9]

## Exports INSPACE does not request
| Ordinals | ISPACE.DLL entries | Role |
|---|---|---|
| 11, 22, 23, 25–28, 30, 37–40, 42 | seg9:0x15cc, seg10:0xf26, seg10:0x3346, seg10:0xec2, seg10:0xdc0, seg10:0x103c, seg10:0xf9c, seg10:0x664, seg9:0x2bf8, seg9:0x2f30, seg10:0x124a, seg10:0x5f96, seg9:0x40da | Dialog procedures. ISPACE.DLL passes their addresses to `DialogBox` |
| 21, 31 | seg10:0x309c, seg9:0x2ac4 | Window procedures. ISPACE.DLL writes them into the `WNDCLASS` at ISPACE.DLL DGROUP:0x2022 and calls `RegisterClass` (seg10:0x42b9, seg9:0x2ab8) |
| 13, 16, 19, 20 | seg5:0x505a, seg5:0x700a, seg6:0x79b8, seg6:0x8266 | Requested by FACTORY.EXE only |
| 43 | seg2:0x26 | `WEP` (resident name), returns 1 |
| 44 | seg3:0xbc2 | Returns 0. No module requests it |

[BYTES ISPACE.DLL seg9:0x15a8] [BYTES ISPACE.DLL seg10:0x4266] [BYTES ISPACE.DLL seg9:0x2a76] [BYTES ISPACE.DLL seg2:0x26] [BYTES ISPACE.DLL seg3:0xbc2]

- FACTORY.EXE from the shipdisk requests 21 ordinals from ISPACE.DLL: 1–10, 12–20, 29 and 36. [BYTES FACTORY.EXE seg4:0x30eb] [BYTES FACTORY.EXE seg4:0x3393]
- INSPACE.SCR contains the string `ISPACE.DLL`. Whether the screen saver loads it, and which ordinals it uses, is [OPEN].
