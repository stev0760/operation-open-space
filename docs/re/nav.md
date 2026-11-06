# Navigation: the directory tree and the filesystem walk

This page covers what the Navigation window shows and how it reads the user's drive: the directory tree and its row suffix, the built-in levels, the directory records, the file pane and its icons, the status line, the race and duel boxes, what Enter hands to the wave set-up, the demo pilot and every filesystem call the walk makes. Addresses without a module name are in INSPACE.EXE, whose DGROUP is seg19. The window's creation, child controls, geometry, Help button, close routine, path building (seg6:0x5318) and the Nav INI keys are in [shell.md](shell.md#game-start-dialogs) (items 11 and 12), the INI key table in [formats.md](formats.md#inspaceini-inner-space), the edition flag DGROUP:0x920c in [shell.md](shell.md#intro-and-title-screen) (**Edition check**), the tick DGROUP:0x1e50 in [globals.md](globals.md#global-table) and rand() in [rng.md](rng.md). [DESIGN]

The oracle runs use fixture `basic` as drive D: (`GAMES`, `TOOLS`, `EMPTY`, `DEEP\A\B`), described in [newgame-0001](../../oracle/digests/newgame-0001.md). [ORACLE newgame-0001]

## Tree walk
The tree is listbox 0x65 of the IS_NAV window. seg6:0x37ca fills it when the window is created. [BYTES seg6:0x37ca seg6:0x24d1]

### Row text and suffix
- Every row is a name followed by three digit characters. seg6:0x4118(buf, depth, sub, state) lstrcats "0" (DGROUP:0xeb2, DGROUP:0xeb4, DGROUP:0xeb6) three times and adds depth, sub and state to the last byte after each, so the suffix reads depth, sub, state. Its call sites are seg6:0x38d5, seg6:0x39f0, seg6:0x3a58, seg6:0x3b04, seg6:0x3ba5 and seg6:0x50c7. [BYTES seg6:0x4118 seg6:0x38c7 seg6:0x38d5 DGROUP:0xeb2]
- newgame-0002 shows the rows `GAMES100`, `DEEP110`, `DEEP120`, `A210`, `A220`, `B300`, `DEMON'S GATE103`, `Lazy Loop - RACE100`, `Grand Prix - RACE103`, `Dodge City - DUEL103` and ` D:\112`. [ORACLE newgame-0002]

| Digit | Values | Set by | Evidence |
|---|---|---|---|
| depth | 1 for the root row and every top-level row; parent depth + 1 for an expanded child, read from the parent's third-to-last character | constant push 1; seg6:0x4f46 | [BYTES seg6:0x38cd seg6:0x39e8 seg6:0x4ff4-0x4ffb seg6:0x50bc-0x50c0] |
| sub | 0 no expandable subdirectory, 1 collapsed with subdirectories, 2 expanded | seg6:0x4748, seg6:0x4650; expand writes '2', collapse writes '1' at the second-to-last character | [BYTES seg6:0x4f46 seg6:0x511c seg6:0x4fae seg6:0x5182] |
| state | 0 new, 1 visited, 2 complete, 3 locked | seg6:0x3f4c ([Directory records](#directory-records)) | [BYTES seg6:0x3f4c] |

### Filling the tree
seg6:0x37ca runs these steps: [BYTES seg6:0x37ca]

1. It sends WM_SETREDRAW 0 and LB_RESETCONTENT to listbox 0x65 and resets the row count DGROUP:0x94b6. [BYTES seg6:0x3831 seg6:0x3843 seg6:0x3813]
2. It sets DGROUP:0x94b2 = 1 when DGROUP:0x920c is −1 or 2, else 0. [BYTES seg6:0x37f7 seg6:0x380d]
3. **Root row.** It adds " " + the drive root DGROUP:0xd18 ("X:\") + suffix. The state digit is 2 when seg6:0x45be(root) = 0, that is when the root holds no *.exe, and seg6:0x3f4c(root) otherwise; the sub digit is seg6:0x4748 && seg6:0x4650. The fixture gives ` D:\112`. [BYTES seg6:0x386a seg6:0x387f seg6:0x388f seg6:0x389e seg6:0x38a5 seg6:0x38b2 seg6:0x3909] [ORACLE newgame-0002]
   - newgame-0001 shows the root `d:\` with a green folder and a '+'. [ORACLE newgame-0001]
4. **Top-level rows.** It enumerates DGROUP:0xd18 + "*.*" with find-first attribute 0x15 into the find buffer DGROUP:0x69f8 and LB_ADDSTRINGs every entry with attribute bit 0x10 whose name does not start with '.'. It skips an entry only when INI `Tree` (DGROUP:0x94bc, default 0) is non-zero and seg6:0x45be rejects it. The find-first result is stored but never tested before the find buffer is examined. [BYTES seg6:0x37ef seg6:0x3937 seg6:0x3942 seg6:0x394b seg6:0x3970 seg6:0x397d seg6:0x3a04]
   - Depth is 1. The sub digit is seg6:0x4748(dir) when `Tree` = 0 and seg6:0x4748 && seg6:0x4650 when `Tree` ≠ 0. The state digit is seg6:0x3f4c(dir). [BYTES seg6:0x3989 seg6:0x3994 seg6:0x399e seg6:0x39ab]
5. A root or top-level row whose state is below 2 (signed) clears DGROUP:0x94b2; built-in rows and children never clear it. [BYTES seg6:0x38c1 seg6:0x39ce]
6. **Built-in rows** follow ([Built-in levels](#built-in-levels)). [BYTES seg6:0x3a2a]

### Probes
| Routine | Returns true when | Evidence |
|---|---|---|
| seg6:0x4190(dir) | find-first `dir\*.exe` with attribute 0 succeeds | [BYTES seg6:0x4190 seg6:0x41bb seg6:0x41db seg6:0x41e4 seg6:0x41eb] |
| seg6:0x4748(dir) | `dir\*.*` attribute 0x15 holds a subdirectory whose name does not start with '.' | [BYTES seg6:0x4748 seg6:0x479c] |
| seg6:0x4650(dir) | some such subdirectory `sub` of `dir` has seg6:0x4190(`dir\sub`) true: the one-level-ahead look | [BYTES seg6:0x4650 seg6:0x46a1] |
| seg6:0x45be(path) | seg6:0x4190(path), or path differs from the drive root DGROUP:0xd18 (lstrcmp before the '\' is appended) and seg6:0x4748 and seg6:0x4650 are both true | [BYTES seg6:0x45be] |

- seg6:0x45be filters the top-level rows only when `Tree` ≠ 0, and filters expanded children always. [BYTES seg6:0x397d seg6:0x5065]
- seg6:0x4748 stores its find-first result and, when it failed, goes straight to find-next (seg5:0x352); seg6:0x4650 stores the result and never tests it, examines the find buffer's attribute byte and then also calls find-next. [BYTES seg6:0x47ab seg6:0x47b0 seg6:0x47cf seg6:0x47d3 seg6:0x46b0 seg6:0x46b3]
- newgame-0002 shows the walk on opening: for each root entry the game issues `D:\<NAME>\*.*` attribute 0x15 and reads entries until it meets a subdirectory or the end. [ORACLE newgame-0002]
- Attribute 0x15 is directory, system and read-only, without hidden (0x02), so hidden directories are left out. [BYTES seg6:0x3931] [REF MS-DOS int 21h AH=4Eh attribute semantics]
- Every name is the 8.3 name that DOS find-first returns at find-buffer +0x1e, so long-named directories appear as their short aliases; start-0001 lists `prog~fp2`. [BYTES seg6:0x3931 seg6:0x5020 seg6:0x46a1 seg6:0x479c] [ORACLE start-0001]

### Sort order
- Listbox 0x65 has style 0x50B30153, which includes LBS_SORT, and the IS_NAV procedure has no WM_COMPAREITEM case. The root, top-level and built-in rows enter by LB_ADDSTRING, so the listbox's own comparison orders them by the whole item text, suffix included; the root's leading space puts it first. [BYTES seg6:0x24d1 seg6:0x3909 seg6:0x3a04 seg6:0x3a6c seg6:0x3b18 seg6:0x3bb9]
- Expanded children enter by LB_INSERTSTRING at an index and are not sorted. [BYTES seg6:0x50db]
- newgame-0001 lists deep, demon's gate, dodge city - duel, empty, games, grand prix - race and so on, which fits that order. [ORACLE newgame-0001]

### Expand and collapse
1. **Dispatch.** In the IS_NAV procedure, LBN_SELCHANGE of listbox 0x65 runs seg6:0x5318 when LB_GETCURSEL differs from DGROUP:0x94b4, storing it there. LBN_DBLCLK, and the Enter button when seg6:0x301a returns 0, read the sub digit with seg6:0x5260 and expand (seg6:0x4f46) on 1 or collapse (seg6:0x511c) on 2. [BYTES seg6:0x1f22 seg6:0x2305 seg6:0x2329 seg6:0x239f]
2. seg6:0x5260 on row 0 copies DGROUP:0xd18 and reads ':' − '0' = 10, so the root never expands or collapses. [BYTES seg6:0x5260]
3. **Expand.** seg6:0x4f46 rewrites the selected row with sub digit '2' (LB_DELETESTRING, LB_INSERTSTRING and LB_SETCURSEL at the same index) and increments the insertion index once. It then enumerates DGROUP:0x6990 + "*.*" with attribute 0x15 and inserts each child that is a directory, does not start with '.' and passes seg6:0x45be, with LB_INSERTSTRING at that fixed index (parent + 1), so siblings show in the reverse of the find-next order. It returns 1 when it added a row. [BYTES seg6:0x4f46 seg6:0x4ffe seg6:0x5020 seg6:0x5026 seg6:0x5065 seg6:0x50d0 seg6:0x50db]
   - A child's depth is the parent's + 1, its state seg6:0x3f4c(child path) and its sub digit seg6:0x4748 && seg6:0x4650. [BYTES seg6:0x4ff4 seg6:0x50bc seg6:0x507a seg6:0x508a]
4. **Collapse.** seg6:0x511c rewrites the sub digit to '1' (LB_DELETESTRING, LB_INSERTSTRING and LB_SETCURSEL at the same index) and then deletes the following rows (LB_DELETESTRING at parent + 1) while their depth digit is greater than the row's depth (signed) and within the original count. [BYTES seg6:0x511c seg6:0x5182 seg6:0x51ad]
   - Expand and collapse both run between WM_SETREDRAW 0 and WM_SETREDRAW 1 on listbox 0x65. [BYTES seg6:0x4f72 seg6:0x4fd6 seg6:0x50fe seg6:0x5144 seg6:0x5251]
5. The row count DGROUP:0x94b6 is incremented for every row added and never decremented by a collapse; its only other writer is the reset in seg6:0x37ca. [BYTES seg6:0x390e seg6:0x3a09 seg6:0x3a71 seg6:0x3b1d seg6:0x3bbe seg6:0x50e0 seg6:0x3813]
6. newgame-0002 expands `deep`, then `a`; each child probe is two `D:\DEEP\A\*.*` attribute 0x15 searches. [ORACLE newgame-0002]

### Initial selection
- When the window opens with DGROUP:0x91f8 = 0 and no DEMO or PLAY command line, DGROUP:0x6990 is the drive root (length < 4), so the selection DGROUP:0x94b4 is row 1 when DGROUP:0x95bc ≠ 0 and row 0 in the demo. [BYTES seg6:0x3be3 seg6:0x3c06 seg6:0x3d93 seg6:0x3d9c seg6:0x3da1]
  - newgame-0001 runs `D:\DEEP\*.exe` once after opening (row 1 is `deep`). [ORACLE newgame-0001]
- With DGROUP:0x91f8 ≠ 0, DGROUP:0x6990 becomes the current record's +0x152, prefixed with DGROUP:0xd18 when its second byte is not ':'. [BYTES seg6:0x3e17]
  1. The first component below the root is selected by LB_FINDSTRINGEXACT(−1), then LB_FINDSTRING(−1) (prefix). When both fail the selection is row 0 and DGROUP:0xd18 is appended to DGROUP:0x6990. [BYTES seg6:0x3e29 seg6:0x3e40 seg6:0x3e47-0x3e55]
  2. When the path has two or more components, each component in turn, the last included, is selected and expanded by seg6:0x4f46 when its sub digit is 1; the first keeps the existing selection. A later component is searched with LB_FINDSTRINGEXACT from the current selection DGROUP:0x94b4 and, when that fails, with LB_FINDSTRING from the top of the list (start index −1); the result goes to LB_SETCURSEL. Every row carries the 3-digit suffix and the component does not, so the exact search never matches and the first prefix match from the top of the list wins. [BYTES seg6:0x3ec5 seg6:0x3ee0 seg6:0x3efa seg6:0x3f0e seg6:0x4118] During this walk DGROUP:0x6990 is cut to that component's path, so seg6:0x4f46 enumerates the right directory; it is restored afterwards. A path with one component expands nothing. [BYTES seg6:0x3e6e-0x3f31 seg6:0x3f34]
- A DEMO or PLAY command line selects the row of [Demo pilot](#demo-pilot). [BYTES seg6:0x3c43]

### Drawing the tree
- The folder glyphs are 15×11 cells of IGRAPHIC.DLL BITMAP FLDR (135×11, 4 bpp), loaded by name ("FLDR", DGROUP:0xd56) through seg10:0x6d38 (ISPACE ordinal 3, [ispace.md](ispace.md#the-23-ordinals-inspace-uses)) into DGROUP:0x6aec. [BYTES IGRAPHIC.DLL BITMAP FLDR] [BYTES seg6:0x2401 seg6:0x2404]

  | x | Cell |
  |---|---|
  | 0 | yellow (new) |
  | 15 | yellow with '+' |
  | 30 | yellow-green (visited) |
  | 45 | visited with '+' |
  | 60 | green (complete) |
  | 75 | green with '+' |
  | 90 | AND mask |
  | 105 | red (locked) |
  | 120 | red with '+' |

  [BYTES IGRAPHIC.DLL BITMAP FLDR]
- seg6:0x49c0 picks x = (sub = 1 ? 15 : 0) + {0, 30, 60, 105} indexed by the state; other sub and state values add 0. [BYTES seg6:0x4aed seg6:0x4bc9 seg6:0x4bd0]
- seg6:0x49c0 draws a row with indent I = depth × 16 − 6; row 0 has its leading space removed and I = 0. [BYTES seg6:0x49c0 seg6:0x4a4c]
  1. FillRect of the whole row in GetSysColor(COLOR_WINDOW), then, with ODS_SELECTED, FillRect from left + I in COLOR_HIGHLIGHT. [BYTES seg6:0x4b44]
  2. BitBlt 15×11 of the mask cell (x 90, SRCAND 0x8800C6), then of the glyph cell (SRCPAINT 0xEE0086), at (left + I + 1, top + 1). [BYTES seg6:0x4b6b seg6:0x4b7f]
  3. TextOut in font DGROUP:0x90fe, transparent, COLOR_WINDOWTEXT or COLOR_HIGHLIGHTTEXT, at (left + I + 18, top), of the name without its suffix, lowercased by seg11:0x3fd4 mode 0 ([shell.md](shell.md#intro-and-title-screen), **Strings and fonts**). [BYTES seg6:0x4d8c seg11:0x3fd4]
- **Lines.** For rows other than 0, seg6:0x49c0 draws with the DC's current pen (none selected): [BYTES seg6:0x4ba5 seg6:0x4bbe seg6:0x4bd7]
  - a trunk x = left + 4 from top to bottom, or to top + 6 on the last row (itemID ≥ count − 1, unsigned); [BYTES seg6:0x4c13 seg6:0x4c2a]
  - for depth d ≠ 0, a line x = left + 14d − 10 from top to bottom, or to top + 6 when the row is the last or, for d > 1, when the next row's depth differs (a deeper child included); [BYTES seg6:0x4c5e seg6:0x4c72 seg6:0x4cbc]
  - and a tick from (left + 14d − 10, top + 6) to (left + 14d − 5, top + 6). [BYTES seg6:0x4cde]

- **Focus.** The IS_NAV procedure seg6:0x1f22 dispatches WM_DRAWITEM: itemID −1 goes to seg6:0x4e04, which draws DrawFocusRect; otherwise ODA_FOCUS goes to seg6:0x4e04 (DrawFocusRect when ODS_FOCUS is set, else seg6:0x4ea8), ODA_DRAWENTIRE goes to seg6:0x4ea8 (seg6:0x47e8, then DrawFocusRect when ODS_FOCUS is set) and ODA_SELECT goes to seg6:0x47e8. Both focus rectangles are rcItem with, for control 0x65 and an item other than 0, left moved by (the third-to-last character of the item's LB_GETTEXT text − '0') × 16 − 6, the depth digit, as for the row indent. [BYTES seg6:0x1f58 seg6:0x1f95 seg6:0x1fa7 seg6:0x4e28 seg6:0x4e4d seg6:0x4e5e seg6:0x4e68 seg6:0x4e7a seg6:0x4e81 seg6:0x4e91 seg6:0x4ebb seg6:0x4ed4 seg6:0x4f0a seg6:0x4f26 seg6:0x4f36]

### Tree INI key
`Tree` (default 0, DGROUP:0x94bc) is read by seg6:0x37ca. Non-zero applies seg6:0x45be to the top-level rows and requires seg6:0x4650 for their sub digit; 0 lists every non-hidden top-level directory. Its table row is in [formats.md](formats.md#inspaceini-inner-space). [BYTES seg6:0x37ef seg6:0x397d seg6:0x399e]

## Built-in levels
After the real directories seg6:0x37ca adds the built-in rows with LB_ADDSTRING, depth 1 and sub digit 0 (the dword stores clear the sub and state locals). [BYTES seg6:0x3a2a seg6:0x3a2f seg6:0x3ad9 seg6:0x3b7f]

| Row | Strings | Locked (state 3) when | Evidence |
|---|---|---|---|
| DEMON'S GATE | 760 | DGROUP:0x920c is neither −1 nor 2 | [BYTES seg6:0x3a2f seg6:0x3a45] |
| Lazy Loop - RACE, SDi Speedway - RACE, Grand Prix - RACE | 740 + i, then 739 " - RACE"; i < 1 when DGROUP:0x920c = 3, else i < 3 | DGROUP:0x920c = 0, i > 0 (signed) and DGROUP:0x95bc ≠ 0 | [BYTES seg6:0x3a75 seg6:0x3a94 seg6:0x3ac4 seg6:0x3ae0 seg6:0x3af1] |
| Dodge City - DUEL | 750, then 736 " - DUEL" | DGROUP:0x920c = 0 and DGROUP:0x95bc ≠ 0 | [BYTES seg6:0x3b3a seg6:0x3b6a seg6:0x3b86 seg6:0x3b92] |

[BYTES ENG_LANG.DLL STRING 47] ids 736–750 [BYTES ENG_LANG.DLL STRING 48] id 760

- newgame-0001, a player game with DGROUP:0x920c = 0, shows lazy loop unlocked and the other races, the duel and demon's gate red. [ORACLE newgame-0001]
- The selection handler seg6:0x5524 copies DGROUP:0x6990, drops its last character and classifies it by case-sensitive ends-with (seg16:0x5f0e), testing 760, then 739, then 736: [BYTES seg6:0x5524 seg6:0x55bd seg16:0x5f0e]

  | Ends with | Box | Icon key | Status ([Status line](#status-line)) | Evidence |
  |---|---|---|---|---|
  | 760 | seg6:0x2dba(0), none | "S" (DGROUP:0xee9) | 761 + " " + 762 | [BYTES seg6:0x55dc seg6:0x55f2 seg6:0x5634] |
  | 739 | seg6:0x2dba(1), RACE ids 0x8c–0x90 | "R" (DGROUP:0xeec) | 763 + ". " + 764 | [BYTES seg6:0x56bc seg6:0x56e6] |
  | 736 | seg6:0x2dba(2), DUEL ids 0x93, 0x91, 0x92, 0x94 | "D" (DGROUP:0xeef) | 766 + ". " + 767 | [BYTES seg6:0x5799 seg6:0x57c3] |
  | anything else | seg6:0x2dba(0) | — | ordinary directory | [BYTES seg6:0x586a] |

- A built-in row whose seg6:0x3f4c (on the path with byte 1 set to ' ') returns 3 keeps its first string and separator and gets 2323 "To enter, you must pay for the full game version." in place of its second string: 761 + " " + 2323, 763 + ". " + 2323 or 766 + ". " + 2323. All three built-in cases end in EnableWindow(Enter id 1, TRUE). [BYTES seg6:0x5617 seg6:0x5634 seg6:0x569d seg6:0x573e seg6:0x581b] [BYTES ENG_LANG.DLL STRING 146] id 2323
  - newgame-0001 shows "This is an inner space race track. To enter, you must pay for the full game version." [ORACLE newgame-0001]
- **Icons.** The built-in rows call seg6:0x4368(key, ""). seg6:0x43d0–0x43e8 maps a one-character key 'S' to 105, 'R' to 106 and 'D' to 107; seg6:0x4368 then calls ExtractIcon("S\" | "R\" | "D\", −1), adds the result to DGROUP:0x94a6 unclamped, and loads ISPACE.DLL GROUP_ICON 105, 106 or 107 with LoadIcon(DGROUP:0x9210, id). [BYTES seg6:0x43d8 seg6:0x442c seg6:0x4473 seg6:0x447c seg6:0x448c seg6:0x4498] [BYTES ISPACE.DLL GROUP_ICON 105] [BYTES ISPACE.DLL GROUP_ICON 106] [BYTES ISPACE.DLL GROUP_ICON 107]
  - newgame-0001 logs `EXTRACTICON("S\" | "D\" | "R\", 0xffff)`. [ORACLE newgame-0001]
- Enter on a built-in row is in [Enter](#enter). [BYTES seg6:0x301a]

## Directory records
There is one 0x297-byte record per directory the player has entered, in a global block behind the far pointer DGROUP:0x69f4 (handle DGROUP:0x94ae, count DGROUP:0x94a4, current index DGROUP:0x94a2). The saved-game layout and the fields the wave and loader use are in [formats.md](formats.md#directory-records-0x297-bytes); this table holds the fields Navigation and the wave file loader use. [BYTES seg6:0x36b6 seg6:0x3bee]

| Offset | Size | Content | Evidence |
|---|---|---|---|
| +0x14e | word | count shown as "%# DESTROYED" in the status line; signed sum with +0x150 tests completion | [BYTES seg6:0x5b20 seg6:0x5994] |
| +0x150 | word | count shown as "%# CAPTURED" | [BYTES seg6:0x5b20 seg6:0x5994] |
| +0x152 | 110 bytes, to +0x1bf | directory path as DGROUP:0x6990 held it, with its trailing '\' | [BYTES seg6:0x36e4 seg6:0x3788] |
| +0x1c0 | 8 bytes | set to 0xFF in a new record | [BYTES seg6:0x3791] |
| +0x1d0 | byte | 2 = complete | [BYTES seg6:0x598c seg6:0x40ff seg6:0x4107] |
| +0x1e2 | bitmap | icons already taken; seg9:0xac4 tests bit DGROUP:0x9200 | [BYTES seg9:0x156] |

### Match rule and state digit
seg6:0x3f4c(path) gives the state digit: [BYTES seg6:0x3f4c]

1. It copies the path and appends '\' when missing. [BYTES seg6:0x3f8e]
2. When DGROUP:0x920c = 0 and DGROUP:0x95bc ≠ 0: 3 when the path ends with 739 " - RACE" + "\" but not with 740 "Lazy Loop" + 739 + "\", or ends with 736 " - DUEL" + "\". [BYTES seg6:0x3ff3 seg6:0x400f]
3. Else 3 when DGROUP:0x920c is neither −1 nor 2 and the path ends with 760 "DEMON'S GATE" + "\". [BYTES seg6:0x404e seg6:0x406b]
4. Else, for the first record whose +0x152 equals the path (USER.430 lstrcmp, case-sensitive): 2 when its byte +0x1d0 is 2, 1 otherwise. [BYTES seg6:0x40ff seg6:0x4107]
5. Else 0. [BYTES seg6:0x3f4c]

- The ends-with routine seg16:0x5f0e compares bytes from the ends, case-sensitive, with no length guard on the path. [BYTES seg16:0x5f0e]

### Adding a record
- On acceptance Enter sets DGROUP:0x94b0 = 1 and looks DGROUP:0x6990 up among the DGROUP:0x94a4 records (lstrcmp of +0x152, USER.430). A match sets DGROUP:0x94b0 = 0 and DGROUP:0x94a2 = its index. [BYTES seg6:0x36b6 seg6:0x36ed seg6:0x36f3] [ORACLE start-0001]
- When none matches it grows the block (GlobalAlloc(0x42, 0x297) for the first record, GlobalReAlloc(h, count × 0x297 as a 32-bit product, 0x42) later), copies DGROUP:0x6990 to +0x152, sets +0x1c0 … +0x1c7 to 0xFF and sets DGROUP:0x94a2 = count − 1. [BYTES seg6:0x36b6 seg6:0x36e4 seg6:0x370a seg6:0x3760 seg6:0x3788 seg6:0x3791]
- Records are addressed with the 16-bit product index × 0x297, so 98 records fit: the 99th would end past 0x10000 (98 × 0x297 = 0xFDD6). [BYTES seg6:0x3775]

### Completion
- A visited directory counts as complete when its byte +0x1d0 is 2 or the signed sum of words +0x14e and +0x150 exceeds the icon count DGROUP:0x94a6. The status then starts with 765, Enter is disabled, and when +0x1d0 was not 2 it is set to 2 and the tree row's state digit is rewritten to '2' (LB_DELETESTRING, LB_INSERTSTRING). [BYTES seg6:0x598c seg6:0x5994 seg6:0x599e seg6:0x59a2 seg6:0x59b6 seg6:0x59f0 seg6:0x5a32 seg6:0x5a51 seg6:0x5a6d]
- The rewrite sends no LB_SETCURSEL, and neither does seg6:0x5318 after it, so the rewritten row loses its selection highlight. [BYTES seg6:0x59d9 seg6:0x5a32 seg6:0x5a51 seg6:0x5a6d seg6:0x5516]
- The wave file loader also writes +0x1d0 ([Enter](#enter), step 7). [BYTES seg9:0x2a]

### Buffers
- DGROUP:0x6990 is 100 bytes, up to DGROUP:0x69f3, followed by the record far pointer DGROUP:0x69f4. [BYTES DGROUP:0x6990 DGROUP:0x69f4]
- Record +0x152 is 110 bytes, and seg6:0x37ca's stack buffers are 128 bytes (bp − 0x80) and 110 bytes (bp − 0xee). [BYTES seg6:0x37d4]
- The INI `Track` read at seg6:0x3c84 allows 0x6d = 109 bytes into the 100-byte DGROUP:0x6990, and no lstrcpy or lstrcat on these paths checks length. [BYTES seg6:0x3c84 seg6:0x5318]

## File pane and icons
seg6:0x5318 sends WM_SETREDRAW 0 and LB_RESETCONTENT to listbox 0x6b, fills the pane through seg6:0x5524, sends WM_SETREDRAW 1 and ends with SetFocus on listbox 0x65. [BYTES seg6:0x5318 seg6:0x532b seg6:0x534c seg6:0x54ee seg6:0x54fd seg6:0x5516]

For an ordinary directory seg6:0x5524 clears the icon count DGROUP:0x94a6 and calls seg6:0x420a for each pattern. [BYTES seg6:0x5536 seg6:0x5877 seg6:0x5889]

1. **Walk.** seg6:0x420a(dir, pattern) lists DGROUP:0x6990 + "*.exe", then + "*.dll", with find-first attribute 0, in find order. For each file it calls seg6:0x4368(dir, name) and LB_ADDSTRINGs "" + name + " " + "0" (digit 0) into listbox 0x6b; it returns the number of files found. [BYTES seg6:0x420a seg6:0x4263 seg6:0x426a]
   - Listbox 0x6b has style 0x50A30B53 (LBS_SORT, LBS_MULTICOLUMN, owner-draw fixed, has-strings), so the pane is sorted and the DLLs appear in it. [BYTES seg6:0x420a]
   - The file count is exes + dlls. [BYTES seg6:0x5974]
2. **Count.** seg6:0x4368 calls SHELL.ExtractIcon(hInstance DGROUP:0x8e66, dir + "\" + name, −1) for every listed file and adds the result to DGROUP:0x94a6, raised to 1 when below 1 (signed). [BYTES seg6:0x442a seg6:0x442c seg6:0x443e seg6:0x4441 seg6:0x444a]
   - newgame-0002 logs `SHELL.EXTRACTICON(..., 0xffff)` per exe. [ORACLE newgame-0002]
3. After both lists seg6:0x589b calls seg6:0x4368("", "") (DGROUP:0xefe, DGROUP:0xefd): ExtractIcon("\", −1), added unclamped. The '\' is appended when the byte before the empty buffer is not '\'. [BYTES seg6:0x589b]
4. The icon count DGROUP:0x94a6 is therefore the sum of max(1, ExtractIcon(file, −1)) over the *.exe and *.dll files plus ExtractIcon("\", −1). Under the oracle runtime ExtractIcon returns 0 for "\" and for "S\", "R\", "D\": `tools` shows 4 icons in 2 files (ECHO 3 + DELTA 1) and `games` 3 icons in 3 files. [ORACLE wave-0001] [ORACLE newgame-0001]

### Preview
- The preview is a 32×32 compatible bitmap DGROUP:0x94ba. While no preview is set (DGROUP:0x94c0 = 0) each file gets ExtractIcon(path, 0). [BYTES seg6:0x444e seg6:0x4469]
  1. The first file whose handle is above 1 (unsigned) gets PatBlt BLACKNESS and DrawIcon at (0, 0) and ends the search (DGROUP:0x94c0 = 1). [BYTES seg6:0x44a4 seg6:0x44cb seg6:0x44ec]
  2. A file without an icon, while no file has reached the bitmap (DGROUP:0x94c2 = 0) and DGROUP:0x94a6 ≠ 0, gets PatBlt BLACKNESS and the program-name placeholder seg9:0x352(dc, 0, 0, 32, 0, name); a later file with an icon replaces it. [BYTES seg6:0x451f seg6:0x4534 seg6:0x455c seg6:0x4564]
  3. The trailing seg6:0x4368("", "") sets DGROUP:0x94c0 = 1 and blacks the bitmap only when no file reached it. Built-in rows put their LoadIcon handle here. [BYTES seg6:0x4564]
- newgame-0001 calls ExtractIcon(path, 0) for the first exe only and the preview shows ALPHA.EXE. [ORACLE newgame-0001]
- seg6:0x5b4c StretchBlts DGROUP:0x94ba (0, 0, 32, 32) to (W − 92, 89, 64, 64) SRCCOPY in the IS_NAV client, W being the client right. Only on WM_PAINT (argument 1, pushed at seg6:0x22cf) does it first draw the frame seg11:0x1774(4, W − 96, 85, W − 24, 157) and PatBlt(W − 92, 89, 64, 64, BLACKNESS). [BYTES seg6:0x22cf seg6:0x5bea seg6:0x5bf8 seg6:0x5c0c seg6:0x5c2e seg6:0x5c72] [ORACLE newgame-0001]

### Drawing the file rows
- seg6:0x47e8 draws the owner-draw items and hands CtlID 0x65 to seg6:0x49c0. A file row gets FillRect in COLOR_WINDOW or COLOR_HIGHLIGHT, the IGRAPHIC.DLL FILES glyph 11×12 at (left + 1, top + 1) and the name at (left + 15, top). [BYTES seg6:0x47e8 seg6:0x2417 seg6:0x429a seg6:0x42c8]
  - FILES is 33×12 at 1 bpp: the mask at x 22 (SRCAND), then the image at x 0 (SRCPAINT), with the text colour set first to 0x00FF0000 when the last digit is 1 and 0x000000FF when 2. [BYTES IGRAPHIC.DLL BITMAP FILES]
  - seg6:0x5cbc strips the " n" suffix and a leading space, and seg11:0x3fd4 mode 0 lowercases the name. [BYTES seg6:0x5cbc seg6:0x4953 seg6:0x4827]
- seg6:0x420a always writes digit 0, so the coloured variants never occur from the Navigation code. [BYTES seg6:0x420a]

## Status line
The status text DGROUP:0x6a24 is drawn by seg6:0x5b4c over the band seg11:0x1774(−1, 0, H − 25, W, H), cased by seg11:0x3fd4 mode 3 and written by seg9:0x8bec(dc, 5, H − 20, 14, 0, 0, text), H being the client bottom. [BYTES seg6:0x5bb5 seg6:0x5bc3 seg6:0x5be2]

- **Mode 3** uppercases the first character and the first non-space character after '.', '!', CR or LF, keeps the 'I' of " I " upper when it ends before the last three characters, and lowercases every other A–Z. [BYTES seg11:0x3fd4 seg6:0x5bc3]

| Case | Text | Enter | Evidence |
|---|---|---|---|
| Demon's Gate | 761 "Gateway to the ultimate challenge!" + " " (DGROUP:0xee6) + 762 "Enter when you have what it takes.." | enabled | [BYTES seg6:0x55f2 seg6:0x569d] [BYTES ENG_LANG.DLL STRING 48] ids 761–762 |
| race | 763 "This is an Inner Space race track" + ". " (DGROUP:0x9ee) + 764 "Enter here for a thrill!" | enabled | [BYTES seg6:0x56e6] [BYTES ENG_LANG.DLL STRING 48] ids 763–764 |
| duel | 766 "This is the arena for Inner Space duels" + ". " + 767 "If you have a dispute, take him here!" | enabled | [BYTES seg6:0x57c3] [BYTES ENG_LANG.DLL STRING 48] ids 766–767 |
| locked built-in | the row's first string and separator + 2323 "To enter, you must pay for the full game version." | enabled | [BYTES seg6:0x5634 seg6:0x573e seg6:0x581b] [BYTES ENG_LANG.DLL STRING 146] id 2323 |
| no *.exe | 938 "No apps" + 942 " IN THIS DIRECTORY" + ". " + 939 "Select another directory to enter." | disabled, unless the row's sub digit is 1 or 2, which leaves it as it was | [BYTES seg6:0x58a1 seg6:0x58b8 seg6:0x58dd seg6:0x5911 seg6:0x5948 seg6:0x5953 seg6:0x596e] [BYTES ENG_LANG.DLL STRING 59] ids 938–942 |
| at least one *.exe | seg10:0xa332(3028 "%? %# IN %#.", mode 1, DGROUP:0x94a6 = 1, 2056 "THERE ARE", 2055 "THERE IS", DGROUP:0x94a6, 2515 "ICON", files, 941 "FILE"), then the not-visited or visited text below | enabled | [BYTES seg6:0x5a74 seg6:0x5a82 seg6:0x5a87] [BYTES ENG_LANG.DLL STRING 190] id 3028 [BYTES ENG_LANG.DLL STRING 129] ids 2055–2056 [BYTES ENG_LANG.DLL STRING 158] id 2515 |
| … not visited | + 943 "YOU HAVE NOT BEEN HERE BEFORE" + ". " | enabled | [BYTES seg6:0x5ab4] [BYTES ENG_LANG.DLL STRING 59] id 943 |
| … visited | + seg10:0xa332(3029 "%# CAPTURED AND %# DESTROYED.", mode 0, +0x150, 2506 "ICON WAS", +0x14e, 2506) | enabled | [BYTES seg6:0x5ad1 seg6:0x5aee seg6:0x5af8 seg6:0x5b20] [BYTES ENG_LANG.DLL STRING 190] id 3029 [BYTES ENG_LANG.DLL STRING 157] id 2506 |
| complete | 765 "This directory is complete" + ". " + the 3029 text only; the jump to seg6:0x5abc skips the 3028 text | disabled | [BYTES seg6:0x598c seg6:0x5a6d seg6:0x5a72 seg6:0x5abc seg6:0x5af8] [BYTES ENG_LANG.DLL STRING 48] id 765 |

- The no-exe test reads only the exe count; DLLs are not consulted. [BYTES seg6:0x58a1]
- seg10:0xa332 mode 1 overwrites the buffer and mode 0 appends to it. [BYTES seg6:0x5a82 seg6:0x5b20]
- Its `%?` directive takes three arguments in the order flag, id0, id1 and loads string ids[idx] with idx = (flag > 1 ? 1 : flag), signed: flag 0 gives id0, any flag of 1 or more gives id1, and a negative flag indexes before the array. In the 3028 call the flag is (DGROUP:0x94a6 = 1), so one icon gives 2055 "THERE IS" and any other count 2056 "THERE ARE". [BYTES seg10:0xa3ee seg10:0xa713 seg10:0xa73a seg10:0xa740 seg10:0xa74e]
- newgame-0001 shows "There are 3 icons in 3 files.You have not been here before." and "No apps in this directory. Select another directory to enter.". [ORACLE newgame-0001]

## Race and duel boxes
seg6:0x2dba(kind) shows the box of the selected row with SW_SHOW: kind 1 the RACE controls 0x8c–0x90, kind 2 the DUEL controls 0x93, 0x91, 0x92 and 0x94, kind 0 neither. The controls are created in seg6:0x23d4 ([shell.md](shell.md#game-start-dialogs), item 11). [BYTES seg6:0x2dba seg6:0x55df seg6:0x56e9 seg6:0x57c6]

- 0x8c and 0x93 are group boxes (class "button", style 0x50000007) captioned 2145 "RACE" and 2146 "DUEL". [BYTES seg6:0x2587 seg6:0x2595 seg6:0x25e5] [BYTES ENG_LANG.DLL STRING 135] ids 2145–2146
- The seven combos have style 0x50210003 (CBS_DROPDOWNLIST, unsorted). seg6:0x23d4 fills each with CB_ADDSTRING of LoadStrings into a 0x28-byte buffer and ends each with CB_SETCURSEL: [BYTES seg6:0x267f seg6:0x26da-0x2d7f]

  | Control | Strings |
  |---|---|
  | 0x8d | 2147 "Solo", 2148 "2 Ships", 2149 "3 Ships", 2150 "4 Ships" |
  | 0x91 | 2151 "1 Opponent", 2152 "2 Opponents", 2153 "3 Opponents" |
  | 0x8e, 0x92 | 2154 "Normal", 2155 "Difficult", 2156 "Dangerous!" |
  | 0x8f | 2157 "1 Lap", 2158 "5 Laps", 2159 "10 Laps" |
  | 0x90 | 2160 "Fastest", 2161 "Faster", 2162 "Similar", 2163 "Same Ships", 2164 "Slower", 2165 "Slowest" |
  | 0x94 | 2166 "Toughest", 2167 "Tougher", 2162 "Similar", 2163 "Same Ships", 2168 "Weaker", 2169 "Weakest" |

  [BYTES ENG_LANG.DLL STRING 135] ids 2147–2159 [BYTES ENG_LANG.DLL STRING 136] ids 2160–2169
- DGROUP:0x94b8 is the height of GetTextExtent("Test", 4) (DGROUP:0xd51) in font DGROUP:0x90fe, measured in seg6:0x23d4. The WM_SIZE layout of the boxes starts at y0 = client height − 0x9b. [BYTES seg6:0x23e3 seg6:0x2642 seg6:0x2658 seg6:0x2692 seg6:0x211e]

| Control | Box | INI key | Enter stores | Evidence |
|---|---|---|---|---|
| 0x8d | race | `Racers` | DGROUP:0x94e = (index = 0 ? 1 : index + 1) + 4i, i the course | [BYTES seg6:0x3339] |
| 0x8e | race | `RaceDiff` | DGROUP:0x94a8 = index | [BYTES seg6:0x3392] |
| 0x90 | race | `RaceOpp` | DGROUP:0x94aa = index | [BYTES seg6:0x33dc] |
| 0x8f | race | `RaceLaps` | DGROUP:0x9ea0 = 1, 5 or 10 laps for index 0, 1, 2; another index leaves it | [BYTES seg6:0x3426 seg6:0x343e] |
| 0x91 | duel | `Duelers` | DGROUP:0x950 = index + 2 | [BYTES seg6:0x351d] |
| 0x92 | duel | `DuelDiff` | DGROUP:0x94a8 = index | [BYTES seg6:0x3569] |
| 0x94 | duel | `DuelOpp` | DGROUP:0x94aa = index (opponent strength) | [BYTES seg6:0x35b3] |

- Enter reads each box with CB_GETCURSEL (0x407), turns the index into decimal text with seg6:0xf1a and writes it with WritePrivateProfileString in the order of the table. The keys' defaults and read sites are in [formats.md](formats.md#inspaceini-inner-space). [BYTES seg6:0x32fa-0x34aa seg6:0x34de-0x365f]

## Enter
seg6:0x301a is the Enter routine; its callers, the state request and the briefing label are in [shell.md](shell.md#game-start-dialogs), item 12. It returns 0 at once when DGROUP:0xd20 = 0 or DGROUP:0x90f4 = 1. [BYTES seg6:0x301a seg6:0x3029-0x303a]

1. It copies DGROUP:0x6990 to DGROUP:0x38c8. [BYTES seg6:0x3044]
2. **Demon's Gate.** For a path ending in 760 "DEMON'S GATE" it returns 0 in the demo (DGROUP:0x95bc = 0) or when seg6:0x3f4c of the path with its ':' replaced by ' ' returns 3 (which also clears DGROUP:0x92e8); else it sets DGROUP:0x92e8 = 1 and DGROUP:0x38c8 = DGROUP:0x6990 = "DEMON'S GATE" and goes to the acceptance code seg6:0x3686. [BYTES seg6:0x3049 seg6:0x306b seg6:0x30a1 seg6:0x30ad seg6:0x30c5 seg6:0x30ed]
3. **Race and duel gate.** An Enter on a race (duel) row while DGROUP:0x94e (DGROUP:0x950) is 0 sets it to 2 only when DGROUP:0xe32 = 0, or DGROUP:0x90f8 ≠ 0 and DemoType DGROUP:0x95ba = 521 (522). In the demo (DGROUP:0x95bc = 0) this also sets DGROUP:0xe32 = 1. Every Enter that leaves both DGROUP:0x94e and DGROUP:0x950 at 0, the refused race or duel included, clears DGROUP:0xe32, so after a race or duel the demo's next race or duel Enter is refused and the one after it can succeed. [BYTES seg6:0x3147 seg6:0x316a seg6:0x31a3 seg6:0x31c6 seg6:0x31cc-0x31da]
4. A race or duel whose seg6:0x3f4c (path with byte 1 set to ' ') returns 3 clears DGROUP:0x94e and DGROUP:0x950 and returns 0. [BYTES seg6:0x321c]
5. **Race.** From the boxes ([Race and duel boxes](#race-and-duel-boxes)): DGROUP:0x94e = (racers index 0 ? 1 : index + 1) + 4i for the course 740 + i that the path minus "\" minus 739 ends with, DGROUP:0x94a8 = difficulty index, DGROUP:0x94aa = opponent index, DGROUP:0x9ea0 = laps, and DGROUP:0x38c8 = DGROUP:0x6990 = the course name. [BYTES seg6:0x31d3 seg6:0x32a6 seg6:0x32b3 seg6:0x3339 seg6:0x3392 seg6:0x33dc seg6:0x3426 seg6:0x343e seg6:0x348d]
   - With DGROUP:0x90f8 ≠ 0 and DGROUP:0x8e5e ≠ 0 a race takes INI `DRacers` (DGROUP:0x94e = DRacers + 1), `DDiff` (DGROUP:0x94a8) and `DLaps` instead of the boxes, with DGROUP:0x94aa = 2. `DLaps` 1, 2 and 3 give 1, 5 and 10 laps; another non-zero value leaves DGROUP:0x9ea0 unchanged; `DLaps` = 0 takes rand() % 7 (seg5:0xdc, signed idiv): 4 gives 1, 5 gives 10, anything else 5. [BYTES seg6:0x3208 seg6:0x3262 seg6:0x327e seg6:0x328b-0x32a6 seg6:0x32ae seg6:0x3434 seg6:0x3439]
6. **Duel.** From the boxes: DGROUP:0x950 = opponents index + 2, DGROUP:0x94a8 = difficulty index, DGROUP:0x94aa = opponent-strength index, and DGROUP:0x38c8 = DGROUP:0x6990 = 750 "Dodge City". Whenever DGROUP:0x90f8 ≠ 0, without a DGROUP:0x8e5e test, a duel takes INI `DuelDiff` with DGROUP:0x94aa = 2 and DGROUP:0x950 = 2. [BYTES seg6:0x34b5 seg6:0x34ca seg6:0x351d seg6:0x3569 seg6:0x35b3 seg6:0x3642]
7. **Ordinary directory.** It is accepted only when seg6:0x4190(DGROUP:0x38c8) finds a *.exe (attribute 0) and seg6:0x3f4c(DGROUP:0x6990) is below 2 (signed), so complete and locked directories cannot be entered and visited ones can. [BYTES seg6:0x3661-0x3683 seg6:0x3666 seg6:0x366e seg6:0x3675 seg6:0x367e]
8. **Acceptance** (seg6:0x3686) adds or finds the directory record ([Directory records](#adding-a-record)) and continues as [shell.md](shell.md#game-start-dialogs) item 12 describes. [BYTES seg6:0x3686 seg6:0x36b6]

### Values handed to the wave set-up
| Global | Meaning | Written at | Readers |
|---|---|---|---|
| DGROUP:0x38c8 | directory of the wave: the path, "DEMON'S GATE", the course name or "Dodge City" | seg6:0x3044, seg6:0x30ed, seg6:0x348d, seg6:0x3642 | wave file loader seg9:0x0 |
| DGROUP:0x92e8 | 1 = Demon's Gate | seg6:0x30e7 | seg9:0x2581, and the generator seg9:0x3202 at seg9:0x3e83, seg9:0x44ae and seg9:0x4767 ([world.md](world.md#difficulty-inputs)) |
| DGROUP:0x94e | race: 4 × course + racers code | seg6:0x3339 | seg9:0x3db6 |
| DGROUP:0x950 | duel: opponents code | seg6:0x351d | seg9:0x3e00 |
| DGROUP:0x94a8 | race or duel difficulty index | seg6:0x3267, seg6:0x339a, seg6:0x34cf, seg6:0x3571 | seg4:0x2eb1, seg4:0x2ed4, seg9:0x4120, seg9:0x4211, seg9:0x601d, seg11:0x48b6 |
| DGROUP:0x94aa | opponent index, 2 under DEMO | seg6:0x326a, seg6:0x33e4, seg6:0x34d5, seg6:0x35bb | seg4:0x2ea9, seg4:0x2ee1, seg9:0x3bdc, seg14:0x2793, seg14:0x2ad6, seg14:0x348f |
| DGROUP:0x9ea0 | race laps 1, 5 or 10 | seg6:0x32a6, seg6:0x32b3, seg6:0x343e | seg4:0x1f78–0x2640, seg4:0x2b10–0x2bb2, seg4:0x2c51–0x2e0e, seg9:0x289d, seg9:0x417d, seg9:0x71f4, seg9:0x76ca, seg9:0x7c88, seg11:0x41f7, seg11:0x42cf, seg11:0x4432, seg11:0x73c8, seg17:0x7a7a, seg17:0x7aad |

[BYTES seg6:0x3044 seg6:0x30e7 seg6:0x30ed seg6:0x3339 seg6:0x343e seg6:0x348d seg6:0x351d seg6:0x3642 seg9:0x3db6 seg9:0x3e00 seg9:0x2581 seg9:0x3e83 seg9:0x44ae seg9:0x4767] [BYTES seg4:0x2640 seg4:0x2b10 seg4:0x2e0e seg9:0x3bdc seg9:0x4120 seg9:0x4211 seg9:0x601d seg11:0x48b6 seg17:0x7a7a seg6:0x3267 seg6:0x35bb]

- The generator seg9:0x3202 also writes DGROUP:0x9ea0 (seg9:0x3ae5, seg9:0x3afd), DGROUP:0x94aa (seg9:0x3b2d, seg9:0x3b64, seg9:0x3bb7, seg9:0x3c27, seg9:0x3c6d, seg9:0x3c86, seg9:0x3cb1) and DGROUP:0x94a8 (seg9:0x3bbd, seg9:0x3c0e, seg9:0x3cba). These are all the direct references; the listings hold no other addressing form for the three words. [BYTES seg9:0x3ae5 seg9:0x3afd seg9:0x3b2d seg9:0x3b64 seg9:0x3bb7 seg9:0x3bbd seg9:0x3c0e seg9:0x3c27 seg9:0x3c6d seg9:0x3c86 seg9:0x3cb1 seg9:0x3cba]

- **Decoding.** The wave set-up decodes a race as DGROUP:0x9320 = 6, DGROUP:0x9338 = (DGROUP:0x94e − 1) mod 4 (opponents) and DGROUP:0x9322 = (DGROUP:0x94e − 1) / 4 (course 0 Lazy Loop, 1 SDi Speedway, 2 Grand Prix), both by signed idiv (quotient rounded toward 0). A duel gives DGROUP:0x9320 = 7 and DGROUP:0x9338 = (DGROUP:0x950 − 1) mod 4; the duel overrides when both are set. [BYTES seg9:0x3db6 seg9:0x3dbd seg9:0x3dd7 seg9:0x3de6 seg9:0x3e00 seg9:0x3e10]
- **Wave file loader.** seg9:0x0(8), called from seg9:0xbf7: [BYTES seg9:0xbf5 seg9:0x2a]
  1. It sets the current record's byte +0x1d0 = 2. [BYTES seg9:0x2a]
  2. It walks DGROUP:0x38c8 + "*.exe", then + "*.dll", with find-first attribute 1 into DGROUP:0x3948 (name at DGROUP:0x3966). For each pattern it tests DGROUP:0x9206 > 9 after every successful find (find-first seg5:0x364, then find-next seg5:0x352) and before it processes the candidate with seg9:0x156; it calls find-next after each processed candidate, so the find-next that returns the candidate past the limit runs before the test ends the pattern, and a failed find ends it without the test. [BYTES seg9:0x46 seg9:0x51 seg9:0x58 seg9:0x62 seg9:0x67 seg9:0x8e seg9:0x94 seg9:0xa0 seg9:0xd4]
  3. Per file seg9:0x156 calls ExtractIcon(path, −1). A count of 0 gets ExtractIcon(path, 0) and seg9:0x278. Otherwise each index i below the count, while DGROUP:0x9206 ≤ 9, is skipped when seg9:0xac4 finds bit DGROUP:0x9200 set in the record bitmap +0x1e2, and else gets ExtractIcon(path, i). [BYTES seg9:0x156 seg9:0x178 seg9:0x1a5 seg9:0x251]
  4. Afterwards +0x1d0 becomes 0 when DGROUP:0x9202 ≠ 0 and +0x1d0 ≠ 1, and DGROUP:0x91fa = DGROUP:0x91fc = DGROUP:0x91fe = min(DGROUP:0x9206, 8). [BYTES seg9:0x145]

## Demo pilot
The demo pilot walks the tree in the demo; when the tick runs it is in [shell.md](shell.md#screen-states) (**Demo auto-navigation**). [BYTES seg6:0x1e68]

1. seg6:0x1e68(arg) sets DGROUP:0x1c34 = GetCurrentTime() + 1200 when arg ≠ 0 (the creation call at seg6:0x1d82). [BYTES seg6:0x1e7b seg6:0x1d82]
2. It clears DGROUP:0xd26 on every call. [BYTES seg6:0x1e8d]
3. When DGROUP:0x1c34 < the tick DGROUP:0x1e50 (unsigned) it sets DGROUP:0x1c34 = tick + 600 and tries Enter seg6:0x301a. [BYTES seg6:0x1e9b seg6:0x1ea2 seg6:0x1eb0]
4. When Enter fails and DGROUP:0x94b6 > DGROUP:0x94b4, it expands the selected row if its sub digit is 1 and selects row DGROUP:0x94b4 + 1 (LB_SETCURSEL, seg6:0x5318); otherwise it sets DGROUP:0xd26 = 1. [BYTES seg6:0x1ecb seg6:0x1ed8 seg6:0x1ede seg6:0x1f04]
5. After the attempt it also sets DGROUP:0xd26 = 1 when DGROUP:0x94b6 − 1 ≤ DGROUP:0x94b4. [BYTES seg6:0x1f0a seg6:0x1f14]

- When the demo timer DGROUP:0x1e80 expires (DGROUP:0x1e80 ≠ 0 and ≤ the tick) with DGROUP:0x95bc = 0, the current record complete (+0x1d0 = 2) and DGROUP:0xd26 ≠ 0, seg6:0x92b8 deletes `LocalPath` (DGROUP:0x9078) + `Games` (DGROUP:0x96b8) + "demogame.sav" with OpenFile(OF_DELETE 0x200). [BYTES seg7:0x63e seg7:0x646 seg7:0x64d seg6:0x92b8 seg6:0x92fe]

### DEMO and PLAY command lines
Under a DEMO or PLAY command line (DGROUP:0x90f8 ≠ 0, [shell.md](shell.md#start-up)) the initial selection follows DemoType DGROUP:0x95ba: [BYTES seg6:0x3c43 seg6:0x3c4c]

| DemoType | Selection | Evidence |
|---|---|---|
| 520 | the root | [BYTES seg6:0x3c5e] |
| 521 | reads INI `Track` (up to 0x6d bytes, default "") into DGROUP:0x6990 and sets DGROUP:0x94e = 4i + 1 for the race name 740 + i it equals; the row searched is that name + 739 | [BYTES seg6:0x3c84 seg6:0x3c8a seg6:0x3cf6 seg6:0x3d00] |
| 522 | reads `Track`, then sets the name to "Dodge City" (DGROUP:0xe9f) and DGROUP:0x950 = 1; the row searched is that name + 736 | [BYTES seg6:0x3d4a seg6:0x3d56] |
| any other (the default 519 included) | DGROUP:0x6990 unchanged and DGROUP:0x94be = 0 | [BYTES seg6:0x3c4c-0x3d8e] |

## Filesystem calls
Every call the Navigation code and the wave file loader make on the user's drive. Find-first is seg5:0x364 (int 21h AH = 4Eh); the INI calls are in [formats.md](formats.md#inspaceini-inner-space) and the New Game drive list in [shell.md](shell.md#game-start-dialogs), item 4. [BYTES seg5:0x364] [REF MS-DOS int 21h AH=4Eh attribute semantics]

| Routine | Call | Path pattern | Attribute | What is read | Evidence |
|---|---|---|---|---|---|
| seg6:0x37ca | find-first/find-next into DGROUP:0x69f8 | `X:\*.*` | 0x15 | names and attribute byte (DGROUP:0x6a0d, name DGROUP:0x6a16) | [BYTES seg6:0x3931 seg6:0x3942] |
| seg6:0x4748 | find-first/find-next | `dir\*.*` | 0x15 | names until the first subdirectory | [BYTES seg6:0x4748 seg6:0x479c] |
| seg6:0x4650 | find-first/find-next | `dir\*.*` | 0x15 | names until a subdirectory passes seg6:0x4190 | [BYTES seg6:0x4650 seg6:0x46a1] |
| seg6:0x4190 | find-first | `dir\*.exe` | 0 | success only | [BYTES seg6:0x4190 seg6:0x41e4] |
| seg6:0x4f46 | find-first/find-next | DGROUP:0x6990 + `*.*` | 0x15 | names and attributes | [BYTES seg6:0x5020] |
| seg6:0x420a | find-first/find-next | DGROUP:0x6990 + `*.exe`, then `*.dll` | 0 | names | [BYTES seg6:0x420a seg6:0x4263] |
| seg6:0x4368 | SHELL.ExtractIcon(DGROUP:0x8e66, path, −1) | each listed file; `\`; `S\`, `R\`, `D\` | — | icon count from the file's resources | [BYTES seg6:0x442c] |
| seg6:0x4368 | SHELL.ExtractIcon(DGROUP:0x8e66, path, 0) | listed files until one yields an icon | — | first icon | [BYTES seg6:0x4469] |
| seg6:0x301a via seg6:0x4190 | find-first | DGROUP:0x38c8 + `*.exe` | 0 | success only | [BYTES seg6:0x3666] |
| seg9:0x0 | find-first/find-next into DGROUP:0x3948 | DGROUP:0x38c8 + `*.exe`, then `*.dll` | 1 | names | [BYTES seg9:0x46 seg9:0x51 seg9:0x58] |
| seg9:0x156 | SHELL.ExtractIcon(path, −1), (path, 0), (path, i) | each found file | — | icon count and icons | [BYTES seg9:0x156 seg9:0x178 seg9:0x1a5 seg9:0x251] |
| seg6:0x92b8 | OpenFile(OF_DELETE 0x200) | `LocalPath` + `Games` + `demogame.sav` | — | deletes the program's own file | [BYTES seg6:0x92b8 seg6:0x92fe] |

- newgame-0002 shows the opening walk: after the label probe, `D:\*.exe` attribute 0, one call of `D:\*.*` attribute 0x15, then `D:\*.*` attribute 0x15 again with a `<entry>\*.exe` probe after each entry until `D:\GAMES\*.exe` finds ALPHA.EXE. [ORACLE newgame-0002]

## Open questions
- How the Win16 LBS_SORT comparison orders items with apostrophes, hyphens and the digit suffix, for example a directory whose name is a prefix of another (A, A-B, AB, A1)? An oracle fixture with such names would settle it. [OPEN] [BYTES seg6:0x24d1]
- What does Windows 3.1/9x SHELL.ExtractIcon return for index −1 on the path "\" and on "S\", "R\", "D\"? The patched oracle runtime and otvdm return 0. [OPEN] [BYTES seg6:0x442c seg6:0x589b] [ORACLE wave-0001]
- Which code loads string 2315 ("…play will be limited to only a few directories")? No INSPACE.EXE or ISPACE.DLL code pushes it as an immediate, and seg9:0x81cf builds 2311 + rand() % 3; an indirect-id search (tables, seg10:0xa332 callers) would settle it. [OPEN] [BYTES seg9:0x81cf] [BYTES ENG_LANG.DLL STRING 145] id 2315
- What do the readers seg7:0xe91, seg13:0x1d95 and seg16:0xb7d do with DGROUP:0x94b2, which is 1 only when DGROUP:0x920c is −1 or 2 and no root or top-level row has a state below 2? [OPEN] [BYTES seg6:0x37f7 seg6:0x38c1 seg6:0x39ce seg7:0xe91 seg13:0x1d95 seg16:0xb7d]
- What do record words +0x14e and +0x150 count beyond the status labels "DESTROYED" and "CAPTURED" (seg9:0xbeb copies +0x150 to +0x14e)? [OPEN] [BYTES seg9:0xbeb]
- What does the original do when a path exceeds DGROUP:0x6990's 100 bytes and overruns the record pointer DGROUP:0x69f4 behind it? [OPEN] [BYTES seg6:0x5318]
- seg10:0xa332's `%#` directive (the value, then the noun's string id) and its singular/plural choice are defined under [shell.md](shell.md#screen-states) (the formatter's "%# and %D"). [BYTES seg6:0x5a82] [ORACLE newgame-0001]
- What the readers of DGROUP:0x94a8, DGROUP:0x94aa and DGROUP:0x9ea0 in seg4, seg9, seg11, seg14 and seg17 ([Values handed to the wave set-up](#values-handed-to-the-wave-set-up)) do with the difficulty, opponent and lap values. [OPEN] [BYTES seg4:0x1f78 seg4:0x2ea9 seg9:0x3bdc seg9:0x4120 seg11:0x48b6 seg14:0x2793 seg17:0x7a7a]
- What the focus rectangle of an itemID −1 WM_DRAWITEM on listbox 0x65 shifts by: seg6:0x4e04 sends LB_GETTEXT for item −1 and reads the third-to-last byte of whatever the stack buffer holds. [OPEN] [BYTES seg6:0x4e3d seg6:0x4e4d seg6:0x4e5e]
