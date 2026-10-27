# NE resources

`isre res` decodes the Win16 resources of every NE module in the distribution. `re/tests/test_res.py` asserts every count on this page. A `STRING` evidence tag names the string-table block (block id = string id >> 4, plus 1). Other tags name the resource type and id.

Regenerate with `re/.venv/bin/isre res --extracted work/extracted --out work/assets`. A module directory is named after its lower-cased file name. Modules from THEWORKS get a `theworks-` prefix, as in `theworks-factory.exe`. [DESIGN]

## Output per module
- `index.json` lists every resource with its type, id, table size, file offset, flags and output file. It adds decoded facts such as dimensions, palette size, WAV format, or a `hex_preview` for binary data. `same_as` names a loose distribution file with identical content: the WAV samples, or the whole bitmap file. [DESIGN]
- `strings.tsv` (id, then text with `\t \r \n \\` escaped) and `strings.json`. [DESIGN]
- `dialogs.json` + `dialogs.rc`, `menus.json` + `menus.rc`, `accelerators.json` + `accelerators.rc`, `version.json`. [DESIGN]
- `<TYPE>/<id>.<ext>`: `BITMAP` becomes `.bmp`, `ICON`/`GROUP_ICON` `.ico`, and `CURSOR`/`GROUP_CURSOR` `.cur`. The dumper sniffs RCDATA and custom types to `.wav`, `.bmp` or `.txt` (UTF-8 from cp1252), else `.bin`. A resource that fails to decode stays a `.bin` and appears under `errors`. No game resource fails. [DESIGN]
- `work/assets/modules.json` sums up all modules. Unchanged files are not rewritten. [DESIGN]

## Win16 layouts
These differ from the Win32 PE layouts. [REF Wine 11.0 dlls/user.exe16, dlls/ver.dll16]

| Type | Layout |
|---|---|
| STRING | 16 slots per block, each a length byte + cp1252 text; length 0 = no string |
| DIALOG | style DWORD, item count BYTE, x y cx cy WORDs, menu (0, 0xFF+WORD, or ASCIIZ), class ASCIIZ, caption ASCIIZ, point size WORD + face ASCIIZ when `DS_SETFONT`. Items: x y cx cy id WORDs, style DWORD, class byte 0x80–0x85 (BUTTON EDIT STATIC LISTBOX SCROLLBAR COMBOBOX) or ASCIIZ, text (0xFF+WORD or ASCIIZ), extra-byte count + bytes |
| MENU | version WORD, offset WORD, items: flags WORD, id WORD unless `MF_POPUP`, ASCIIZ text; `MF_END` 0x80 closes a level |
| ACCELERATOR | 5-byte entries: flags BYTE, key WORD, command WORD; flag 0x80 marks the last |
| VERSION | nodes of length WORD, value length WORD, ASCIIZ key, value, each padded to a DWORD; no `wType` |
| BITMAP, ICON | packed DIB with no `BITMAPFILEHEADER`; an icon carries an AND mask after the XOR bits |

- **Padding:** resource lengths in the NE table are rounded up to the alignment unit, 16 bytes (shift 4) in every module except WAVEMIX.DLL (shift 9). The padding holds stale bytes, so INRUN.AD #2000 20 reads `Inner Space!\0ta\0`. Decoders take the real size from the content. [BYTES INRUN.AD 2000 20]

## Inventory

| Module (NE name) | Resources | Contents |
|---|---|---|
| `eng_lang.dll` (IN_DLOG) | 147 STRING blocks (1,446 strings), 22 DIALOG, 4 MENU, 2 ACCELERATOR, 57 #5000, 1 RCDATA | English UI: strings, dialogs, menus, accelerators, help and briefing texts |
| `igraphic.dll` (INGRAPHIC) | 113 #4000, 15 BITMAP, 5 #6600, 1 #5500 | sprite and ship DIBs, UI bitmaps, data records |
| `ispace.dll` (ISPACE) | 3 #4000, 1 BITMAP, 3 ICON, 3 GROUP_ICON, #5000, #5500, #5600, 1 STRING block | game DLL: three DIBs, `SD_LOGO`, icons, data tables |
| `inspace.exe` (INSPACE) | 2 ICON, 1 GROUP_ICON, #5000, #5500, VERSION, 4 STRING blocks | main program: `GAME` icon (32×32 and 16×16, 4 bpp), FileVersion 1.29.08 |
| `snd_fx.dll` (ISND_FX) | 46 #100 | sound-effect WAVs |
| `eng_fx.dll` (VOICE_FX) | 32 #100 | English voice WAVs |
| `factory.exe` (IFACTORY) | ICON, GROUP_ICON, VERSION, 4 STRING blocks | Ship Factory 1.29.02 (shipdisk). It has no dialogs or menus of its own |
| `theworks-factory.exe` (IFACTORY) | ICON, GROUP_ICON, RCDATA, DIALOG, MENU, VERSION, 5 STRING blocks | Enhanced Ship Factory 1.49.00 |
| `inspace.scr` (RUNINSPACE) | ICON, GROUP_ICON, RCDATA, 4 DIALOG, 2 STRING blocks | screen saver |
| `inrun.ad` (SDI_IS_AD) | ICON, GROUP_ICON, 4 #1000, 3 #2000 | "Inner Space (tm) Starter by Bill Stewart" screen-saver module |
| `install.exe` (I_INSTALL) | ICON, GROUP_ICON, 2 #5500, RCDATA, 9 DIALOG | installer |
| `theworks-setup.exe` (I_INSTALL) | ICON, GROUP_ICON, RCDATA, 4 DIALOG, 4 STRING blocks | add-on installer |
| `ctl3d.dll` (CTL3D), `wavemix.dll` (WAVEMIX) | CTL3D: BITMAP, VERSION; WAVEMIX: VERSION | Microsoft CTL3D 2.05 and WaveMix 1.70 |

[BYTES ENG_LANG.DLL] [BYTES IGRAPHIC.DLL] [BYTES ISPACE.DLL] [BYTES INSPACE.EXE] [BYTES SND_FX.DLL] [BYTES ENG_FX.DLL] [BYTES FACTORY.EXE] [BYTES theworks/FACTORY.EXE] [BYTES INSPACE.SCR] [BYTES INRUN.AD] [BYTES INSTALL.EXE] [BYTES theworks/SETUP.EXE] [BYTES CTL3D.DLL] [BYTES WAVEMIX.DLL]

- Each RCDATA is the dialog editor's `DLGINCLUDE` path, for example `C:\INTEXT\DEMODLG.H` in ENG_LANG. [BYTES ENG_LANG.DLL RCDATA DLGINCLUDE]
- INSPACE.EXE strings 167, 233, 607 and 1800 read `129`, `08`, `Aug 9/2000` and `Inner Space`. [BYTES INSPACE.EXE STRING 11] [BYTES INSPACE.EXE STRING 15] [BYTES INSPACE.EXE STRING 38] [BYTES INSPACE.EXE STRING 113]

## Dialogs
ENG_LANG.DLL holds the game's dialogs. The Contents column restates the caption and controls. [BYTES ENG_LANG.DLL DIALOG]

| Id | Caption | Contents |
|---|---|---|
| 100 #0064 | Inner Space Options/Setup | speed, joystick, audio, display, braking settings |
| 101 #0065 | Inner Space | "Loading ship graphics....please hold on." |
| 139 #008b | Ship Handling | captures icons, repair, cloak, armor, thruster |
| 167 #00a7 | Save Game | game name, Save, Cancel |
| 169 #00a9 | Ship Behaviour | team, initial weapon, attitude to friends and enemies |
| 194 #00c2 | Ambulance Options (To Buy) | Options, To Be Done, Total Cost, Credit, Remaining, Sell |
| 244 #00f4 | Inner Space | Save & Exit, Exit, Cancel |
| 267 #010b | Restore Game | game, ship, pilot, score; opened from ISPACE.DLL seg10:0x438b [ORACLE newgame-0001] |
| 286 #011e | New Ship Body | body choice and cost |
| 300 #012c | Import Ship Body | thrust, wings, shield options |
| 321 #0141 | New Game | see below; opened from ISPACE.DLL seg10:0x442b [ORACLE newgame-0001] |
| 378 #017a | Ship Options (To Sell) | items to sell, cost, credit |
| 404 #0194 | Search | help subtopic search |
| 432 #01b0 | Inner Space Demo Options | SHIP #1, SHIP #2, Random Demo |
| 589 #024d | New Demo | ship, team, drive |
| 666 #029a | Display Options | brightness, app menu, border, animated watches |
| 693 #02b5 | Order Inner Space Now! | order form and phone numbers |
| 799 #031f | Registration Benefits | order prompt |
| 975 #03cf | Joystick Configuration | enable, calibrate, buttons |
| 988 #03dc | Important Information! | licence terms, I Accept, Cancel |
| 999 #03e7 | Audio Options | sound FX, voice, music, mix channels |
| 1872 #0750 | Inner Space Plus Pack! | upgrade offer |

**New Game (#0141)**, 198×117 dialog units, font 8 pt Helv: [BYTES ENG_LANG.DLL DIALOG 321]
- Labels: `PILOT:` `SHIP:` `TEAM:` `PLAY:` `DRIVE:` `ZOOM LEVEL:`, all right-aligned statics.
- Inputs: 322 pilot edit, 324 ship combo (drop-down list, `CBS_SORT`), 326 empty team static, and drop-down lists 325 play, 330 drive and 332 zoom.
- Buttons: 1 `&OK` (default) and 333 `&Cancel`.
- These match the fields the oracle shows. [ORACLE newgame-0001] The template has no `LevClass` control, so code outside the template creates the 96×96 ship preview [OPEN].

Other modules [BYTES INSPACE.SCR DIALOG] [BYTES INSTALL.EXE DIALOG] [BYTES theworks/FACTORY.EXE DIALOG 963] [BYTES theworks/SETUP.EXE DIALOG]:
- INSPACE.SCR: 2003 "Operation: Inner Space" (screen-saver setup: Allow Game Play, Configure, volume), 2000 "Change Password", 2001 password prompt, 2002 "Incorrect password" notice (OK button only).
- INSTALL.EXE set-up steps: 135 personalisation (name, organisation), 300 component choice, 1922 "Game Difficulty and Speed", 1774 "Play Full Screen or in a Window", 200 "Installing..." and 100 "Installation Complete!".
- INSTALL.EXE maintenance: 175 DeInstall/ReInstall/Exit, 333 "Master Disk", 891 release code.
- THEWORKS FACTORY.EXE: 963 "Import Ship Body", with Looping and Flapping options beyond ENG_LANG's 300.
- THEWORKS SETUP.EXE: 100, 300 "Inner Space Add-on Install", 200, `STARTDLG`.

## Menus and accelerators
[BYTES ENG_LANG.DLL MENU] [BYTES ENG_LANG.DLL ACCELERATOR]
- `POPUPMENU`: one popup "FOO-Dummy Popup" holding 208 "Options Menu", a separator, 192 "&Game Options.." and 176 "E&xit Game".
- 101: Game, Ship Cmds, Defenders, Watch, Help. 102: the same plus a Strategy popup (108–113). Which play mode loads which bar is [OPEN].
- 110: the Ship Factory bar (Factory, Ship, Body, Help). THEWORKS FACTORY.EXE has its own 110 with a longer Help popup. [BYTES theworks/FACTORY.EXE MENU 110]
- `MAINACCEL` has 26 entries and `FACTACCEL` 29. Both map Alt+G to 192 and Alt+X to 176, the `POPUPMENU` commands. `MAINACCEL` maps F1 to 240.

## Strings
ENG_LANG.DLL holds 1,446 strings, ids 110–12,546. [BYTES ENG_LANG.DLL STRING] Texts that the oracle saw at run time:
- 2326 + 2327: "With Limited Inner Space, you can only use the Transformer ship." + " If you order the full game package, all ships are available." This is the demo-limit message box. [BYTES ENG_LANG.DLL STRING 146] [ORACLE newgame-0001]
- 2338 "Limited Inner Space" is the main window title. 2140 "Inner Space Navigation" titles the Navigation window. 1149 "NEW GAME, STARTING AT WAVE 1." fills the Restore Game description. [BYTES ENG_LANG.DLL STRING 147] [BYTES ENG_LANG.DLL STRING 134] [BYTES ENG_LANG.DLL STRING 72] [ORACLE newgame-0001]
- 3032 `IT'S %<%S %S%>%<%S%>, %s!%NI HOPE YOU'RE READY TO PLAY..%N%N`, with 597 "Saturday" and 639 "Morning" nearby. These are the first intro lines. The `%<`, `%>`, `%S` and `%N` escapes are [OPEN]. [BYTES ENG_LANG.DLL STRING 190] [ORACLE newgame-0001]

## Custom types

### #4000: sprite DIBs (IGRAPHIC.DLL 113, ISPACE.DLL 3)
- Each is a file-format DIB (with `BITMAPFILEHEADER`), 4 bpp, `BI_RGB`, with its own 16-entry palette. [BYTES IGRAPHIC.DLL 4000]
- Palette entry 15 is black in 110 of the 113 IGRAPHIC images, and all four corner pixels are index 15 in 90 of them. The bytes are consistent with index 15 as a transparent background. How the code masks it is [OPEN]. [BYTES IGRAPHIC.DLL 4000]
- Some images, rendered, show animation frames side by side in one horizontal strip: IGRAPHIC #4000 60 (132×33, 4 frames), #340 (9 frames) and ISPACE #4000 221 (360×40, 9 frames). IGRAPHIC #4000 170 (248×52) holds 8 frames that grow in width, and #200 (60×90) is a 3×6 grid. Many are single pictures, such as #45, #290 (the SDi logo), #500 and #610, including some whose width is a multiple of their height. The code slices a strip by a fixed cell width: ISPACE.DLL seg5:0x17c2 takes 40×40 cells of #221 and the credits take the cells of IGRAPHIC #340 ([shell.md](shell.md#intro-and-title-screen), Logo graphics and Credits). The other strips are [OPEN]. [BYTES IGRAPHIC.DLL 4000 60] [BYTES IGRAPHIC.DLL 4000 170] [BYTES ISPACE.DLL 4000 221]
- 53 of the 113 are byte-identical to the ship bitmaps that THEWORKS ships as loose `.DIB` files. #60 is `XFORM.DIB` (the Transformer), and `index.json` names the rest under `same_as`. [BYTES IGRAPHIC.DLL 4000 60]
- The 15 named BITMAPs are `ALPHABET BW_BLOCKS BW_MBLOCKS CORNER CRACKS DEF DIGITAL FILES FLDR METAL OUT_SIGN PIECE RADSHIP ROCKS SYSTEMS`: packed DIBs, 10 at 1 bpp and 5 at 4 bpp. [BYTES IGRAPHIC.DLL BITMAP]

### #100: WAV files (SND_FX.DLL 46, eng_fx.dll 32)
- All 78 are RIFF WAVE, PCM, 11,025 Hz, 8-bit, mono. SND_FX ids run 100–178 and eng_fx ids 138–180. [BYTES SND_FX.DLL 100] [BYTES ENG_FX.DLL 100]
- Sound slot s plays resource s + 100 ([sound.md](sound.md#slots)). Four ids of that range are in neither DLL: 112, 142, 165 and 181, the resources of slots 0x0c, 0x2a, 0x41 and 0x51. [BYTES SND_FX.DLL 100] [BYTES ENG_FX.DLL 100] [BYTES ISPACE.DLL seg9:0x2780]
- [re/artifacts/sounds.tsv](../../re/artifacts/sounds.tsv) catalogues every slot with its resource, length, transcript and call sites ([sound.md](sound.md#catalogue)). [DESIGN]
- The samples of 45 SND_FX and 25 eng_fx sounds equal loose THEWORKS `.WAV` files, which gives them names (`same_as`), for example eng_fx #156 = `ATTACK.WAV`. [BYTES ENG_FX.DLL 100 156]

### #5000: text and binary data
- ENG_LANG.DLL holds 57 NUL-terminated cp1252 texts with CRLF line breaks. They cover level intros (101–104), the licence (105) and weapon descriptions (110–135). Further groups are order texts (136, 138, 330), racing and course records (137), meal-time fuel (180–183), the intro (200, 210), game-element topics (300–305), team descriptions (306–313), and laws and icons (320, 321). #210 begins "YOUR COMPUTER HAS BEEN INVADED!", the intro text the oracle shows. [BYTES ENG_LANG.DLL 5000 210] [ORACLE newgame-0001]
- INSPACE.EXE #143: "Unregistered Copy   ILLEGAL", space-padded to 50 characters. [BYTES INSPACE.EXE 5000 143]
- ISPACE.DLL #150 is binary: 88 distinct non-zero bytes followed by zero padding. Its use is [OPEN]. [BYTES ISPACE.DLL 5000 150]

### Other data types
- **#5500 110** (32 zero bytes) appears in IGRAPHIC.DLL, INSPACE.EXE and INSTALL.EXE. INSTALL.EXE also has **#5500 120** (128 zero bytes). Their use is [OPEN]. [BYTES INSPACE.EXE 5500 110] [BYTES INSTALL.EXE 5500 120]
- **ISPACE.DLL #5500 100**: 288 bytes, 96 three-byte colour entries. They form about five ramps (cyan to blue, yellow to brown, grey-brown, white to grey, yellow-green to green). The last 12 bytes are zero; the code uses the first 0x114 ([formats.md](formats.md#ispacedll-5500-100)). Each entry is red, green, blue ([formats.md](formats.md#ispacedll-5500-100)). [BYTES ISPACE.DLL 5500 100]
- **ISPACE.DLL #5600 135**: 128 bytes, 64 signed words within ±250. Its use is [OPEN]. [BYTES ISPACE.DLL 5600 135]
- **IGRAPHIC.DLL #6600** 100, 105, 110, 115, 120: binary records of 2,496, 3,520, 2,432, 1,056 and 1,088 bytes. Each starts `ff 00 ff 00 ff 00`. Their use is [OPEN]. [BYTES IGRAPHIC.DLL 6600]
- **INRUN.AD #1000** 1–4: option records holding labels (`Allow Game Play`, `Condition:`, `Configure..`, `Sound:`) and binary fields. **#2000** 10, 20, 30: credit, title and help texts. [BYTES INRUN.AD 1000] [BYTES INRUN.AD 2000]
