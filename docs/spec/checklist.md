# Feature checklist

The master ledger of what the port reproduces from Operation: Inner Space v1.29.08. Progress is the status columns of this file and nothing else. Only the orchestrator edits it. [DESIGN]

## Columns
| Column | Content |
|---|---|
| ID | `AREA-nn`, stable: never renumbered or reused [DESIGN] |
| Feature | One behaviour, in the original's own words where it has them |
| Evidence | The tags that establish the feature |
| Spec, RE, Port, Edition | Statuses from the vocabularies below. A new row starts at RE `none` and Port `todo` [DESIGN] |

## Evidence
- A MANUAL tag names a topic of the 2000 help file. Three Ship Factory topics exist only in the 1998 file: Customizing Ships, Editing Ship Bitmaps and Sharing Custom Ships With Others.
- A STRING tag names an ENG_LANG.DLL string-table block, as [resources](../re/resources.md) defines it. The `id n` after the tag gives the string id.
- Other resource tags name a type and an id. DGROUP and segN tags give the address of a key name or a text string.
- [OPEN] inside a feature marks the open part of that row.

## Vocabularies
Spec names the strongest source, in the order of its rows below. [DESIGN]

| Column | Value | Meaning |
|---|---|---|
| Spec | `specified` | A file under `docs/spec/` specifies the feature |
| Spec | `oracle` | An oracle digest records it in the running original |
| Spec | `manual` | A help topic describes it |
| Spec | `code` | Only the original's code shows it, as a `docs/re/` file describes |
| Spec | `strings` | Only resource or data-segment strings name it |
| RE | `none` | No address yet |
| RE | `located` | A `docs/re/` file gives its address |
| RE | `understood` | `docs/re/` explains the code path |
| RE | `verified` | An independent verifier confirmed it |
| Port | `todo` | Not started |
| Port | `wip` | In progress |
| Port | `done` | Implemented |
| Port | `verified` | Matches the oracle |
| Edition | `demo` | The unregistered build has it: an oracle run saw it, or the demo's own text offers it |
| Edition | `registered` | Full version only, per the Why Register Inner Space?, Ordering Instructions or Ship Factory topics, or a refusal the demo shows |
| Edition | `unknown` | Not yet established |

## KEY: Key bindings and mouse

One row per binding. MAINACCEL and FACTACCEL are the accelerator tables in ENG_LANG.DLL. The in-game key list is strings 1200–1258, and strings 1300–1340 give its meanings.

| ID | Feature | Evidence | Spec | RE | Port | Edition |
|---|---|---|---|---|---|---|
| KEY-01 | Up Arrow: thrust | [MANUAL QuickStart] [BYTES ENG_LANG.DLL STRING 77] id 1223 [ORACLE runtime-0001] | oracle | none | todo | demo |
| KEY-02 | Ctrl+Up Arrow: OverThruster (turbo thrust) | [MANUAL QuickStart] [BYTES ENG_LANG.DLL STRING 77] id 1224 | manual | none | todo | unknown |
| KEY-03 | Left/Right Arrow: turn | [MANUAL QuickStart] [BYTES ENG_LANG.DLL STRING 77] id 1226 | manual | none | todo | unknown |
| KEY-04 | Shift or Space: fire gun | [MANUAL QuickStart] [BYTES ENG_LANG.DLL STRING 77] id 1227 | manual | none | todo | unknown |
| KEY-05 | Down Arrow: brake | [MANUAL QuickStart] [BYTES ENG_LANG.DLL STRING 77] id 1225 | manual | none | todo | unknown |
| KEY-06 | Z or Return: fire weapon system | [MANUAL QuickStart] [BYTES ENG_LANG.DLL STRING 77] id 1228 | manual | none | todo | unknown |
| KEY-07 | H: call the ambulance | [MANUAL QuickStart] [MANUAL Resource Management] [BYTES ENG_LANG.DLL STRING 77] id 1229 | manual | none | todo | unknown |
| KEY-08 | F1: online help (MAINACCEL command 240) | [MANUAL QuickStart] [BYTES ENG_LANG.DLL STRING 76] id 1207 [BYTES ENG_LANG.DLL ACCELERATOR MAINACCEL] [BYTES seg2:0x164b] | manual | located | wip | unknown |
| KEY-09 | F2: online help (MAINACCEL command 256) | [BYTES ENG_LANG.DLL STRING 76] id 1207 [BYTES ENG_LANG.DLL ACCELERATOR MAINACCEL] [BYTES seg2:0x1652] | strings | located | done | unknown |
| KEY-10 | T: next target (any type); shows targeting instead of a message | [MANUAL General] [BYTES ENG_LANG.DLL STRING 76] id 1200 | manual | none | todo | unknown |
| KEY-11 | R: ship target ("Smart Target") | [MANUAL General] [BYTES ENG_LANG.DLL STRING 76] id 1201 [BYTES ENG_LANG.DLL STRING 82] id 1301 | manual | none | todo | unknown |
| KEY-12 | I: icon target | [MANUAL General] [BYTES ENG_LANG.DLL STRING 76] id 1203 | manual | none | todo | unknown |
| KEY-13 | V: virus target | [MANUAL General] [BYTES ENG_LANG.DLL STRING 76] id 1204 | manual | none | todo | unknown |
| KEY-14 | Y: hazard target | [MANUAL General] [BYTES ENG_LANG.DLL STRING 76] id 1202 | manual | none | todo | unknown |
| KEY-15 | U: defender target | [MANUAL General] [BYTES ENG_LANG.DLL STRING 76] id 1205 | manual | none | todo | unknown |
| KEY-16 | Ctrl + a targeting key: target the closest object of that type | [MANUAL General] | manual | none | todo | unknown |
| KEY-17 | M: show the most recent message | [MANUAL General] [BYTES ENG_LANG.DLL STRING 76] id 1206 | manual | none | todo | unknown |
| KEY-18 | Tab: change the current weapon | [MANUAL General] [BYTES ENG_LANG.DLL STRING 77] id 1230 | manual | none | todo | unknown |
| KEY-19 | CapsLock: cloak or shield on/off | [MANUAL Special Ship Systems] [BYTES ENG_LANG.DLL STRING 77] id 1231 | manual | none | todo | unknown |
| KEY-20 | P: perspective shift to a teammate and back | [MANUAL Special Ship Systems] [BYTES ENG_LANG.DLL STRING 76] id 1215 | manual | none | todo | unknown |
| KEY-21 | B: show the game speed on the target display | [MANUAL Game Settings] [BYTES ENG_LANG.DLL STRING 76] id 1208 [BYTES seg11:0x71d4] | manual | located | todo | unknown |
| KEY-22 | Alt+Plus / Alt+Minus (also Alt+=, Alt+-): game speed up/down (commands 401/411) | [MANUAL Game Settings] [BYTES ENG_LANG.DLL STRING 77] id 1221 [BYTES ENG_LANG.DLL ACCELERATOR MAINACCEL] [BYTES seg10:0x7e1c] | manual | understood | todo | unknown |
| KEY-23 | Ctrl+Plus / Ctrl+Minus: rescale the game (1/2x, 1x, 2x) | [MANUAL Game Settings] [BYTES ENG_LANG.DLL STRING 77] id 1220 [BYTES seg10:0x9a2a] [BYTES seg6:0xb07] | manual | understood | todo | unknown |
| KEY-24 | Plus / Minus: radar zoom | [BYTES ENG_LANG.DLL STRING 77] id 1219 [BYTES ENG_LANG.DLL STRING 83] id 1319 [MANUAL General] [BYTES seg11:0x7215] | strings | located | todo | unknown |
| KEY-25 | F12: toggle Action and Strategy modes | [MANUAL Dealing With Other Ships] [BYTES ENG_LANG.DLL STRING 77] id 1222 [BYTES seg11:0x6d19] | manual | understood | todo | unknown |
| KEY-26 | Alt+G: Game Settings dialog (command 192) | [MANUAL QuickStart] [BYTES ENG_LANG.DLL STRING 76] id 1209 [BYTES ENG_LANG.DLL ACCELERATOR MAINACCEL] [BYTES seg10:0x7f18] | manual | understood | todo | unknown |
| KEY-27 | Alt+I: open/close the Icon Watch (command 491) | [MANUAL Icons] [BYTES ENG_LANG.DLL STRING 76] id 1210 [BYTES ENG_LANG.DLL ACCELERATOR MAINACCEL] [BYTES seg11:0x6e94] | manual | understood | todo | unknown |
| KEY-28 | Alt+R: Ship Watch (command 471) | [BYTES ENG_LANG.DLL STRING 76] id 1211 [BYTES ENG_LANG.DLL STRING 82] id 1311 [BYTES ENG_LANG.DLL ACCELERATOR MAINACCEL] [BYTES seg11:0x6ea0] | code | understood | todo | unknown |
| KEY-29 | Alt+B: Boss Key (command 412) | [BYTES ENG_LANG.DLL STRING 76] id 1212 [BYTES ENG_LANG.DLL STRING 83] id 1312 [BYTES ENG_LANG.DLL ACCELERATOR MAINACCEL] [BYTES seg2:0x1486] | code | understood | todo | unknown |
| KEY-30 | Alt+X: exit the game (command 176) | [MANUAL QuickStart] [BYTES ENG_LANG.DLL STRING 76] id 1213 [BYTES ENG_LANG.DLL ACCELERATOR MAINACCEL] [ORACLE smoke-0001] [BYTES seg2:0x1625] [BYTES seg2:0xe07] | oracle | understood | todo | demo |
| KEY-31 | Alt+S: save the game (command 422) | [MANUAL High Scores] [BYTES ENG_LANG.DLL STRING 76] id 1214 [BYTES ENG_LANG.DLL ACCELERATOR MAINACCEL] [BYTES ENG_LANG.DLL STRING 146] id 2324 [BYTES seg10:0x8568] | manual | located | todo | registered |
| KEY-32 | Alt+X then S: save on exit | [MANUAL High Scores] [BYTES ENG_LANG.DLL DIALOG 244] | manual | none | todo | registered |
| KEY-33 | Ctrl+Alt+X: self destruct | [BYTES ENG_LANG.DLL STRING 77] id 1216 [BYTES ENG_LANG.DLL STRING 83] id 1316 | strings | none | todo | unknown |
| KEY-34 | ESC while playing: abort to the intro, offering a save | [MANUAL High Scores] | manual | none | todo | unknown |
| KEY-35 | ESC on a race track: abort the race | [MANUAL Race Courses] | manual | none | todo | unknown |
| KEY-36 | ESC in the black hole: return to Inner Space | [MANUAL Inner Demon] | manual | none | todo | registered |
| KEY-37 | ESC on the title or intro: skip it; opens Restore Game, or Navigation after the desk scene | [MANUAL QuickStart] [MANUAL Game Settings] [BYTES ENG_LANG.DLL STRING 77] id 1217 [ORACLE smoke-0001] [ORACLE newgame-0001] | oracle | none | todo | demo |
| KEY-38 | ESC in the pre-wave briefing: skip it and start the wave | [MANUAL Game Settings] [ORACLE runtime-0001] [BYTES seg7:0x2cc] | oracle | understood | todo | demo |
| KEY-39 | Space in the intro or briefing: skip a minor section | [MANUAL Game Settings] [BYTES ENG_LANG.DLL STRING 77] id 1218 | manual | none | todo | unknown |
| KEY-40 | Ctrl held on entering the ambulance or Hall of Justice: skip the landing | [MANUAL Game Settings] | manual | none | todo | unknown |
| KEY-41 | S during the intro: start a new demo and choose its ship | [MANUAL QuickStart] | manual | none | todo | unknown |
| KEY-42 | C during the intro: continue the most recent demo | [MANUAL QuickStart] | manual | none | todo | unknown |
| KEY-43 | ESC during the demo: stop it and return to the intro | [MANUAL QuickStart] | manual | none | todo | unknown |
| KEY-44 | R on the title screen: order the full game | [BYTES ENG_LANG.DLL STRING 145] id 2310 [BYTES ENG_LANG.DLL DIALOG 799] [ORACLE smoke-0001] | oracle | none | todo | demo |
| KEY-45 | W: open the Ship Watch; each press watches another ship | [MANUAL Dealing With Other Ships] [BYTES ENG_LANG.DLL STRING 78] id 1237 [BYTES seg11:0x7127] | manual | understood | todo | unknown |
| KEY-46 | Ctrl+W: open the Ship Watch, or close it when open | [MANUAL Dealing With Other Ships] [BYTES ENG_LANG.DLL STRING 78] id 1246 | manual | none | todo | unknown |
| KEY-47 | Alt+W: watch one of your defenders (command 481) | [MANUAL Dealing With Other Ships] [BYTES ENG_LANG.DLL STRING 79] id 1255 [BYTES ENG_LANG.DLL ACCELERATOR MAINACCEL] | manual | none | todo | unknown |
| KEY-48 | Ctrl+A: ask ships to attack my target | [MANUAL Dealing With Other Ships] [BYTES ENG_LANG.DLL STRING 78] id 1242 | manual | none | todo | unknown |
| KEY-49 | Ctrl+D: ask ships to defend my target | [MANUAL Dealing With Other Ships] [BYTES ENG_LANG.DLL STRING 78] id 1243 | manual | none | todo | unknown |
| KEY-50 | Ctrl+H: ask ships to help me | [MANUAL Dealing With Other Ships] [BYTES ENG_LANG.DLL STRING 79] id 1248 | manual | none | todo | unknown |
| KEY-51 | Ctrl+F: challenge the targeted ship to a fight | [MANUAL Dealing With Other Ships] [BYTES ENG_LANG.DLL STRING 79] id 1249 | manual | none | todo | unknown |
| KEY-52 | Ctrl+E: ask ships to exit the wave | [MANUAL Dealing With Other Ships] [BYTES ENG_LANG.DLL STRING 78] id 1247 | manual | none | todo | unknown |
| KEY-53 | Ctrl+S: ask ships to self-preserve | [MANUAL Dealing With Other Ships] [BYTES ENG_LANG.DLL STRING 78] id 1245 | manual | none | todo | unknown |
| KEY-54 | Ctrl+C: ask ships to capture my target | [BYTES ENG_LANG.DLL STRING 78] id 1241 [BYTES ENG_LANG.DLL STRING 84] id 1332 [MANUAL Dealing With Other Ships] | strings | none | todo | unknown |
| KEY-55 | Ctrl+G: ask ships to get a gas tank | [BYTES ENG_LANG.DLL STRING 78] id 1244 [BYTES ENG_LANG.DLL STRING 84] id 1335 | strings | none | todo | unknown |
| KEY-56 | Strategy mode Q (manual) or C (key list): capture my target; which key is live is [OPEN] | [MANUAL Dealing With Other Ships] [BYTES ENG_LANG.DLL STRING 78] id 1232 | manual | none | todo | unknown |
| KEY-57 | Strategy mode A: attack my target | [MANUAL Dealing With Other Ships] [BYTES ENG_LANG.DLL STRING 78] id 1233 | manual | none | todo | unknown |
| KEY-58 | Strategy mode D: defend my target | [MANUAL Dealing With Other Ships] [BYTES ENG_LANG.DLL STRING 78] id 1234 | manual | none | todo | unknown |
| KEY-59 | Strategy mode G: refuel (get gas) | [MANUAL Dealing With Other Ships] [BYTES ENG_LANG.DLL STRING 78] id 1235 | manual | none | todo | unknown |
| KEY-60 | Strategy mode S: self-preserve | [MANUAL Dealing With Other Ships] [BYTES ENG_LANG.DLL STRING 78] id 1236 | manual | none | todo | unknown |
| KEY-61 | Strategy mode E: exit the wave | [MANUAL Dealing With Other Ships] [BYTES ENG_LANG.DLL STRING 78] id 1238 | manual | none | todo | unknown |
| KEY-62 | Strategy mode H: help me (key list); H also calls the ambulance [OPEN] | [BYTES ENG_LANG.DLL STRING 78] id 1239 [BYTES ENG_LANG.DLL STRING 84] id 1339 | strings | none | todo | unknown |
| KEY-63 | Alt+A: defenders attack my target (command 441) | [MANUAL Icons] [BYTES ENG_LANG.DLL STRING 79] id 1251 [BYTES ENG_LANG.DLL ACCELERATOR MAINACCEL] | manual | none | todo | unknown |
| KEY-64 | Alt+D: defenders defend my target; MAINACCEL has no Alt+D entry [OPEN] | [MANUAL Icons] [BYTES ENG_LANG.DLL STRING 79] id 1252 | manual | none | todo | unknown |
| KEY-65 | Alt+E: defenders exit the wave (command 461) | [MANUAL Icons] [BYTES ENG_LANG.DLL STRING 79] id 1256 [BYTES ENG_LANG.DLL ACCELERATOR MAINACCEL] | manual | none | todo | unknown |
| KEY-66 | Alt+H: defenders return and defend me (command 451) | [MANUAL Icons] [BYTES ENG_LANG.DLL STRING 79] id 1257 [BYTES ENG_LANG.DLL ACCELERATOR MAINACCEL] | manual | none | todo | unknown |
| KEY-67 | Double-click mid-screen: toggle full screen and window | [MANUAL General] [BYTES INSTALL.EXE DIALOG 1774] [BYTES seg2:0x1152] | manual | understood | todo | unknown |
| KEY-68 | Double-click the score / target / radar / damage / weapon display: matching help page | [MANUAL General] [MANUAL High Scores] | manual | none | todo | unknown |
| KEY-69 | Double-click a score in the score list: that game's description | [MANUAL High Scores] | manual | none | todo | unknown |
| KEY-70 | Joystick: 2-axis steering; button 1 fires the gun, button 2 a weapon by default | [MANUAL Joystick Settings] | manual | none | todo | unknown |
| KEY-71 | Factory Alt+G: options (command 192) | [BYTES ENG_LANG.DLL ACCELERATOR FACTACCEL] | strings | none | todo | registered |
| KEY-72 | Factory Alt+X: exit (command 176) | [BYTES ENG_LANG.DLL ACCELERATOR FACTACCEL] | strings | none | todo | registered |
| KEY-73 | Factory Alt+S: throw a rock at the ship to play its hit sound (command 191) | [MANUAL Customizing Ships] [BYTES ENG_LANG.DLL ACCELERATOR FACTACCEL] | manual | none | todo | registered |
| KEY-74 | Factory Alt+A (command 131); matches the N&AME: label [OPEN] | [BYTES ENG_LANG.DLL ACCELERATOR FACTACCEL] [BYTES ENG_LANG.DLL STRING 144] id 2291 | strings | none | todo | registered |
| KEY-75 | Factory Alt+R (command 141); matches the C&REATOR: label [OPEN] | [BYTES ENG_LANG.DLL ACCELERATOR FACTACCEL] [BYTES ENG_LANG.DLL STRING 144] id 2292 | strings | none | todo | registered |
| KEY-76 | Factory Alt+B (command 151); matches the &BODY: label [OPEN] | [BYTES ENG_LANG.DLL ACCELERATOR FACTACCEL] [BYTES ENG_LANG.DLL STRING 143] id 2283 | strings | none | todo | registered |
| KEY-77 | Factory Alt+P (command 171); matches the &PAINT: label [OPEN] | [BYTES ENG_LANG.DLL ACCELERATOR FACTACCEL] [BYTES ENG_LANG.DLL STRING 144] id 2295 | strings | none | todo | registered |
| KEY-78 | Factory Alt+T (command 231); matches the &TRIM: label [OPEN] | [BYTES ENG_LANG.DLL ACCELERATOR FACTACCEL] [BYTES ENG_LANG.DLL STRING 144] id 2296 | strings | none | todo | registered |
| KEY-79 | Factory Alt+U (command 181); matches the THR&UST: label [OPEN] | [BYTES ENG_LANG.DLL ACCELERATOR FACTACCEL] [BYTES ENG_LANG.DLL STRING 144] id 2298 | strings | none | todo | registered |
| KEY-80 | Factory Alt+N (command 240); matches the &New button [OPEN] | [BYTES ENG_LANG.DLL ACCELERATOR FACTACCEL] [BYTES ENG_LANG.DLL STRING 144] id 2301 | strings | none | todo | registered |
| KEY-81 | Factory Alt+D (command 245); matches the &Delete button [OPEN] | [BYTES ENG_LANG.DLL ACCELERATOR FACTACCEL] [BYTES ENG_LANG.DLL STRING 144] id 2302 | strings | none | todo | registered |
| KEY-82 | Factory Alt+H (command 210); matches the &Handling.. button [OPEN] | [BYTES ENG_LANG.DLL ACCELERATOR FACTACCEL] [BYTES ENG_LANG.DLL STRING 144] id 2303 | strings | none | todo | registered |
| KEY-83 | Factory Alt+V (command 220); matches the Beha&vior.. button [OPEN] | [BYTES ENG_LANG.DLL ACCELERATOR FACTACCEL] [BYTES ENG_LANG.DLL STRING 145] id 2304 | strings | none | todo | registered |
| KEY-84 | Factory Tab / Shift+Tab (commands 500/510) | [BYTES ENG_LANG.DLL ACCELERATOR FACTACCEL] | strings | none | todo | registered |
| KEY-85 | Factory F1 (command 256) | [BYTES ENG_LANG.DLL ACCELERATOR FACTACCEL] | strings | none | todo | registered |

## WIN: Windows, dialogs and panels

| ID | Feature | Evidence | Spec | RE | Port | Edition |
|---|---|---|---|---|---|---|
| WIN-01 | Main window: class `Inner Space`, WS_POPUP full screen by default; title "Limited Inner Space" in the demo | [MANUAL General] [ORACLE newgame-0001] [BYTES ENG_LANG.DLL STRING 147] id 2338 [BYTES seg6:0x12b6] [BYTES seg6:0x146f] | oracle | understood | done | demo |
| WIN-02 | Resizable windowed mode | [MANUAL General] [BYTES INSTALL.EXE DIALOG 1774] [BYTES seg2:0x3eb] [BYTES seg6:0x146a] | manual | understood | done | unknown |
| WIN-03 | Title screen: OPERATION:, edition and version line, copyright, demo-day line, Press 'R', "ESC to play - Alt-X to Exit" | [ORACLE smoke-0001] [ORACLE depth8-0001] [ORACLE intro-0001] [BYTES ENG_LANG.DLL STRING 133] id 2121 [BYTES ENG_LANG.DLL STRING 159] id 2542 [BYTES seg15:0x5809] [ORACLE install-0001] | oracle | verified | done | demo |
| WIN-04 | Title-screen hint "For best results, change Windows to 256 color mode" above 8 bpp | [ORACLE depth8-0001] [BYTES DGROUP:0x129d] [BYTES seg6:0x1716] | oracle | understood | todo | demo |
| WIN-05 | Paused state: title "Inner Space - PAUSED..", PAUSED, CLICK HERE TO PLAY | [ORACLE smoke-0001] [BYTES ENG_LANG.DLL STRING 128] id 2044 [BYTES ENG_LANG.DLL STRING 127] id 2028 [BYTES ENG_LANG.DLL STRING 127] id 2027 [BYTES seg2:0x9c4] | oracle | located | todo | demo |
| WIN-06 | Status display: score, target, radar, damage, play time, current weapon | [MANUAL General] [ORACLE runtime-0001] [BYTES seg11:0xafe] | oracle | located | todo | demo |
| WIN-07 | Score readout, lower left of the status display | [MANUAL High Scores] [BYTES seg11:0x4ea] | manual | located | todo | unknown |
| WIN-08 | Target display: direction arrow, team and attitude in blue/grey/red, starfield at High or Highest detail; CURRENT TARGET (AUTO/MANUAL) | [MANUAL General] [ORACLE runtime-0001] [BYTES ENG_LANG.DLL STRING 59] id 931 [BYTES ENG_LANG.DLL STRING 59] id 932 | oracle | none | todo | demo |
| WIN-09 | Messages in the target display, shown for a few seconds | [MANUAL General] | manual | none | todo | unknown |
| WIN-10 | Radar screen: colour coding, cloaked ships shown, zoom | [MANUAL General] [BYTES seg11:0x375f] | manual | located | todo | unknown |
| WIN-11 | Damage display: Engine, Guns, Thruster, plus Wings or Cloak; green/amber/red/blinking red/grey | [MANUAL General] [BYTES ENG_LANG.DLL STRING 53] ids 840–844 | manual | none | todo | unknown |
| WIN-12 | Weapon display: WEAPON SYSTEM, missiles left | [MANUAL General] [ORACLE runtime-0001] [BYTES ENG_LANG.DLL STRING 59] id 934 | oracle | none | todo | demo |
| WIN-13 | Play-time clock (wave clock) in the status display | [MANUAL General] [ORACLE runtime-0001] [BYTES seg11:0x1f0e] | oracle | understood | todo | demo |
| WIN-14 | Game menu bar: menu 101 (Game, Ship Cmds, Defenders, Watch, Help) and menu 102 (adds Strategy) | [MANUAL QuickStart] [MANUAL Display Options] [BYTES ENG_LANG.DLL MENU 101] [BYTES ENG_LANG.DLL MENU 102] [BYTES seg2:0x13b2] [BYTES seg11:0x6d19] | manual | understood | todo | unknown |
| WIN-15 | Popup menu: Options Menu, Game Options.., Exit Game | [BYTES ENG_LANG.DLL MENU POPUPMENU] [BYTES seg2:0x13b2] | strings | located | todo | unknown |
| WIN-16 | 3D game border | [MANUAL Display Options] [BYTES seg2:0xcb1] | manual | understood | todo | unknown |
| WIN-17 | Logo intro: spinning SD_LOGO and "presents.. " | [ORACLE smoke-0001] [ORACLE depth8-0001] [ORACLE intro-0001] [BYTES ENG_LANG.DLL STRING 133] id 2118 [BYTES seg15:0x5809] | oracle | verified | done | demo |
| WIN-18 | Intro desk scene: the monitor types a weekday and time-of-day greeting with the pilot name, then the mission | [ORACLE newgame-0001] [BYTES ENG_LANG.DLL 5000 210] [BYTES ENG_LANG.DLL STRING 190] id 3032 [BYTES seg11:0x7aaa] [BYTES ISPACE.DLL seg8:0xa36] | oracle | verified | wip | demo |
| WIN-19 | Launch: ship on a launch pad flies through a monitor; "PREPARE TO LAUNCH.. GOOD LUCK, (pilot)!" | [MANUAL General] [ORACLE runtime-0001] [BYTES ENG_LANG.DLL STRING 190] id 3030 [BYTES seg15:0x59b3] | oracle | understood | wip | demo |
| WIN-20 | "Analysing Drive" display before the Navigation window | [ORACLE runtime-0001] [BYTES ENG_LANG.DLL STRING 134] id 2141 [BYTES seg15:0x59b3] [ORACLE start-0001] | oracle | understood | done | demo |
| WIN-21 | Navigation window: class IS_NAV, "Inner Space Navigation", 560x350 at 30,30, child layout | [MANUAL General] [ORACLE newgame-0001] [BYTES ENG_LANG.DLL STRING 134] id 2140 [BYTES seg7:0x373] [ORACLE start-0001] | oracle | understood | wip | demo |
| WIN-22 | Restore Game dialog (#010b): GAME, PILOT, SHIP, SCORE, description | [MANUAL High Scores] [BYTES ENG_LANG.DLL DIALOG 267] [ORACLE newgame-0001] [BYTES ISPACE.DLL seg10:0x438b] [BYTES seg11:0x7a02] [ORACLE start-0001] | oracle | understood | wip | demo |
| WIN-23 | New Game dialog (#0141): PILOT, SHIP, TEAM, PLAY, DRIVE, ZOOM LEVEL, 96x96 ship preview | [MANUAL General] [BYTES ENG_LANG.DLL DIALOG 321] [ORACLE newgame-0001] [BYTES ISPACE.DLL seg10:0x442b] [BYTES ISPACE.DLL seg10:0x436f] [ORACLE start-0001] | oracle | understood | wip | demo |
| WIN-24 | New Demo dialog (#024d): SHIP, TEAM, DRIVE | [MANUAL QuickStart] [BYTES ENG_LANG.DLL DIALOG 589] [BYTES ISPACE.DLL seg10:0xf26] [BYTES seg11:0x7959] | manual | understood | done | unknown |
| WIN-25 | Save Game dialog (#00a7): GAME name | [MANUAL High Scores] [BYTES ENG_LANG.DLL DIALOG 167] [BYTES ISPACE.DLL seg10:0xdc0] | manual | located | wip | registered |
| WIN-26 | Exit dialog (#00f4): Save & Exit, Exit, Cancel | [MANUAL High Scores] [BYTES ENG_LANG.DLL DIALOG 244] [BYTES ISPACE.DLL seg10:0x103c] [BYTES seg2:0xd85] [ORACLE start-0001] | oracle | understood | verified | unknown |
| WIN-27 | Game Settings dialog "Inner Space Options/Setup" (#0064) | [MANUAL Game Settings] [BYTES ENG_LANG.DLL DIALOG 100] [BYTES ISPACE.DLL seg9:0x15cc] | manual | located | todo | unknown |
| WIN-28 | Audio Options dialog (#03e7) | [MANUAL Game Settings] [BYTES ENG_LANG.DLL DIALOG 999] | manual | none | todo | unknown |
| WIN-29 | Display Options dialog (#029a) | [MANUAL Display Options] [BYTES ENG_LANG.DLL DIALOG 666] | manual | none | todo | unknown |
| WIN-30 | Joystick Configuration dialog (#03cf) | [MANUAL Joystick Settings] [BYTES ENG_LANG.DLL DIALOG 975] [BYTES ISPACE.DLL seg9:0x40da] | manual | located | todo | unknown |
| WIN-31 | "Loading ship graphics....please hold on." box (#0065) | [BYTES ENG_LANG.DLL DIALOG 101] | strings | none | todo | unknown |
| WIN-32 | "Resizing graphics...Please hold on." message | [BYTES ENG_LANG.DLL STRING 142] id 2269 [BYTES seg10:0x9a39] | code | understood | todo | unknown |
| WIN-33 | Ambulance Options (To Buy) dialog (#00c2): Options, To Be Done, Add, Remove, Sell.., Total Cost, Credit, Remaining | [MANUAL Resource Management] [BYTES ENG_LANG.DLL DIALOG 194] | manual | none | todo | unknown |
| WIN-34 | Ship Options (To Sell) dialog (#017a) | [MANUAL Resource Management] [BYTES ENG_LANG.DLL DIALOG 378] | manual | none | todo | unknown |
| WIN-35 | New Ship Body dialog (#011e): body and cost | [MANUAL Resource Management] [BYTES ENG_LANG.DLL DIALOG 286] [BYTES ISPACE.DLL seg10:0xf9c] | manual | located | todo | unknown |
| WIN-36 | Ambulance landing scene: "Incoming Jet", "Approaching Landing Pad.." | [MANUAL Game Settings] [BYTES ENG_LANG.DLL STRING 134] id 2137 [BYTES ENG_LANG.DLL STRING 134] id 2138 [BYTES seg7:0x5ce] | manual | understood | todo | unknown |
| WIN-37 | Icon Watch window (IS_ICON): every icon in the wave | [MANUAL Icons] [BYTES DGROUP:0x52d] [BYTES seg11:0x6e94] | manual | understood | todo | unknown |
| WIN-38 | Ship Watch window (IS_WATCH): position, direction, actions; race time gaps | [MANUAL Dealing With Other Ships] [MANUAL Race Courses] [BYTES DGROUP:0x51f] [BYTES seg11:0x7127] | manual | understood | todo | unknown |
| WIN-39 | Watch menu: Icons, One Ship, All Ships | [BYTES ENG_LANG.DLL MENU 101] [BYTES seg2:0x15e0] [BYTES seg11:0x6ea0] | code | understood | todo | unknown |
| WIN-40 | IS_SMART window; purpose [OPEN] | [BYTES DGROUP:0x53a] [BYTES seg11:0x6ea0] | code | understood | todo | unknown |
| WIN-41 | IS_INFO window: the two status panels of the wave HUD | [BYTES DGROUP:0x55c] [BYTES seg11:0xafe] [BYTES seg11:0xca2] | code | understood | todo | unknown |
| WIN-42 | Online help window (IS_HELP, "Inner Space Online Help"): Main Menus, Radar Display, Search, Prev, Next, OK, See Also | [MANUAL General] [BYTES ENG_LANG.DLL STRING 137] ids 2182–2190 | manual | none | todo | unknown |
| WIN-43 | Dynamic help topics: SPACE SHIPS, USEFUL OBJECTS, HAZARDS, OTHER OBJECTS, TEAMS, KEYBOARD CMDS, HOW TO PLAY, RACING, GAMES, WEAPONS, LAWS | [MANUAL General] [BYTES ENG_LANG.DLL STRING 51] ids 800–809 [BYTES ENG_LANG.DLL STRING 75] id 1198 | manual | none | todo | unknown |
| WIN-44 | Two dynamic help pages per ship: statistics, damage, behaviour, attitudes | [MANUAL General] | manual | none | todo | unknown |
| WIN-45 | Help Search dialog (#0194) | [BYTES ENG_LANG.DLL DIALOG 404] | strings | none | todo | unknown |
| WIN-46 | High-score list "Champions of INNER SPACE": SHIP, SCORE, DATE, STATUS, GAME FILE | [MANUAL High Scores] [BYTES ENG_LANG.DLL STRING 58] id 925 [BYTES ENG_LANG.DLL STRING 129] ids 2062–2063 [BYTES ENG_LANG.DLL STRING 130] ids 2064–2066 [BYTES seg6:0x6576] [BYTES seg6:0x5de3] | manual | understood | todo | unknown |
| WIN-47 | Pre-wave mission briefing: "WAVE n MISSION BRIEFING", directory, icon count, program names | [MANUAL Game Settings] [ORACLE runtime-0001] [BYTES ENG_LANG.DLL STRING 159] id 2537 [BYTES seg9:0x5d19] [ORACLE start-0001] | oracle | verified | wip | demo |
| WIN-48 | Wave debriefing: "WAVE n MISSION DEBRIEFING", score, captured, destroyed, credit, resources | [BYTES ENG_LANG.DLL STRING 188] id 3003 [BYTES ENG_LANG.DLL STRING 190] id 3035 [BYTES seg9:0x6e00] | code | understood | todo | unknown |
| WIN-49 | Game over screens: GAME OVER, "YOU'RE DEAD, (pilot)" | [BYTES ENG_LANG.DLL STRING 21] id 325 [BYTES ENG_LANG.DLL STRING 127] id 2021 [BYTES seg9:0xddd] | code | understood | todo | unknown |
| WIN-50 | Order Inner Space Now! dialog (#02b5): trial day, Serial #, Registration #, More Info.., Order Form.. | [MANUAL Ordering Instructions] [BYTES ENG_LANG.DLL DIALOG 693] [BYTES ISPACE.DLL seg10:0x664] [BYTES seg11:0x79d5] [ORACLE start-0001] [BYTES ISPACE.DLL seg10:0x492] | oracle | verified | wip | demo |
| WIN-51 | Registration Benefits dialog (#031f) with the benefits text | [MANUAL Why Register Inner Space?] [BYTES ENG_LANG.DLL DIALOG 799] [BYTES ENG_LANG.DLL 5000 330] [BYTES ISPACE.DLL seg10:0x5f96] [BYTES seg2:0xe1a] [ORACLE start-0001] | oracle | understood | done | demo |
| WIN-52 | Important Information! dialog (#03dc): licence terms, I Accept, Cancel | [BYTES ENG_LANG.DLL DIALOG 988] [BYTES ENG_LANG.DLL 5000 105] [BYTES ISPACE.DLL seg10:0x124a] [BYTES seg11:0x7a02] | code | understood | done | unknown |
| WIN-53 | Inner Space Plus Pack! dialog (#0750) | [BYTES ENG_LANG.DLL DIALOG 1872] [BYTES ENG_LANG.DLL 5000 138] [BYTES ISPACE.DLL seg10:0x5f96] | strings | located | done | registered |
| WIN-54 | Low-resource dialogs: free GDI resources warning and abort prompt | [BYTES ENG_LANG.DLL STRING 191] id 3043 [BYTES ENG_LANG.DLL STRING 191] id 3042 [BYTES seg7:0x39] | strings | located | todo | unknown |
| WIN-55 | While a game palette exists (DGROUP:0x920e ≠ 0), WM_QUERYNEWPALETTE reselects and realises it and repaints; WM_PALETTECHANGED from another window realises it and calls UpdateColors | [BYTES seg2:0x5cb] [BYTES seg2:0x56d] [BYTES seg2:0x5cb] [ORACLE start-0001] | code | verified | done | unknown |
| WIN-56 | Intro credits: the growing logo, six typed small-capitals entries with lightning bolts, the erase and the logo-only tail | [ORACLE intro-0001] [BYTES seg15:0x7b34] [BYTES seg15:0x7cda] | oracle | verified | done | demo |
| WIN-57 | Intro text pages: name and version heading in the bitmap font, two 11-line pages of #5000 136, "press ESC to play Inner Space.. " | [ORACLE intro-0001] [BYTES ENG_LANG.DLL 5000 136] [BYTES ENG_LANG.DLL STRING 133] id 2119 [BYTES seg15:0x6890] | oracle | verified | done | demo |
| WIN-58 | Intro "Basic Key Commands" page: heading and 11 key rows in the bitmap font with graded colours | [ORACLE intro-0001] [BYTES ENG_LANG.DLL STRING 133] id 2120 [BYTES ENG_LANG.DLL STRING 77] [BYTES seg15:0x6b52] | oracle | verified | done | demo |

## NAV: Navigation, directories and waves

| ID | Feature | Evidence | Spec | RE | Port | Edition |
|---|---|---|---|---|---|---|
| NAV-01 | Drive list in New Game: C: to Z: probed, A: and B: never; entries `L: label`, unlabelled drives `Unknown_HD` | [MANUAL General] [ORACLE newgame-0001] [ORACLE start-0001] [BYTES ISPACE.DLL seg10:0x2104] | oracle | verified | done | demo |
| NAV-02 | Directory tree: lower case, sorted, green root, yellow real directories, red locked levels; double-click expands | [MANUAL General] [ORACLE newgame-0001] | oracle | none | todo | demo |
| NAV-03 | Directory walk with findfirst/findnext (attr 0x15), one level ahead of the expanded node | [ORACLE newgame-0001] [BYTES seg5:0x352] [BYTES seg5:0x364] [BYTES DGROUP:0x1260] | oracle | located | todo | demo |
| NAV-04 | Directory status line: icons and files counted, visited or not, "No apps in this directory" | [MANUAL General] [ORACLE newgame-0001] [BYTES ENG_LANG.DLL STRING 59] ids 938–943 | oracle | none | todo | demo |
| NAV-05 | File pane: program names in lower case, multicolumn | [ORACLE newgame-0001] | oracle | none | todo | demo |
| NAV-06 | Icon preview: the first program's icon at 2x | [ORACLE newgame-0001] | oracle | none | todo | demo |
| NAV-07 | Icon count per directory from ExtractIcon count queries on *.exe and *.dll | [ORACLE newgame-0001] | oracle | none | todo | demo |
| NAV-08 | List-text flag digits after each directory name (DEEP111); meaning [OPEN] | [ORACLE newgame-0001] | oracle | none | todo | demo |
| NAV-09 | Enter: fly through a gate into the chosen directory | [MANUAL General] [ORACLE newgame-0001] [BYTES ENG_LANG.DLL STRING 134] id 2143 | oracle | none | todo | demo |
| NAV-10 | Built-in levels in the tree: demon's gate, dodge city - duel, grand prix - race, lazy loop - race, sdi speedway - race | [ORACLE newgame-0001] [BYTES ENG_LANG.DLL STRING 47] ids 736–742 [BYTES ENG_LANG.DLL STRING 47] id 750 [BYTES ENG_LANG.DLL STRING 48] id 760 | oracle | none | todo | demo |
| NAV-11 | RACE box: Solo/2/3/4 Ships, Normal/Difficult/Dangerous!, 1/5/10 Laps, Fastest..Slowest or Same Ships | [MANUAL Race Courses] [ORACLE newgame-0001] [BYTES ENG_LANG.DLL STRING 135] ids 2145–2159 [BYTES ENG_LANG.DLL STRING 136] ids 2160–2165 [BYTES seg6:0x23d4] | oracle | located | todo | demo |
| NAV-12 | DUEL box: 1/2/3 Opponents, difficulty, Toughest..Weakest | [ORACLE newgame-0001] [BYTES ENG_LANG.DLL STRING 135] ids 2146–2159 [BYTES ENG_LANG.DLL STRING 136] ids 2160–2169 [BYTES seg6:0x23d4] | oracle | located | todo | demo |
| NAV-13 | A wave per entered directory: its programs' icons, hazards and ships | [MANUAL General] [MANUAL Your Mission] [BYTES seg9:0x3202] | manual | located | todo | unknown |
| NAV-14 | Wave counter and START OF WAVE; first 3–5 waves easy; difficulty most noticeable above wave 7 | [MANUAL Game Settings] [BYTES ENG_LANG.DLL STRING 21] id 324 | manual | none | todo | unknown |
| NAV-15 | Entry and exit gates; leave through the gate when done | [BYTES ENG_LANG.DLL 5000 101] [BYTES ENG_LANG.DLL STRING 10] id 156 [BYTES ENG_LANG.DLL STRING 37] id 585 | strings | none | todo | unknown |
| NAV-16 | Completed-directory state ("This directory is complete") | [BYTES ENG_LANG.DLL STRING 48] id 765 | strings | none | todo | unknown |
| NAV-17 | Demo directory limit: "play will be limited to only a few directories" | [BYTES ENG_LANG.DLL STRING 145] id 2315 | strings | none | todo | demo |
| NAV-18 | Briefing remarks on the wave: viruses, hazard mix, hazard blocks, spinning hazards, big directories | [BYTES ENG_LANG.DLL STRING 75] ids 1190–1193 [BYTES ENG_LANG.DLL STRING 141] id 2242 | strings | none | todo | unknown |
| NAV-19 | One directory record (0x297 bytes: path, roster indices, object counts) per entered directory, kept in the saved game | [BYTES seg6:0x36ce] | code | understood | todo | unknown |

## FLT: Flight model

| ID | Feature | Evidence | Spec | RE | Port | Edition |
|---|---|---|---|---|---|---|
| FLT-01 | Thrust along the heading, using fuel | [MANUAL QuickStart] [BYTES ENG_LANG.DLL 5000 301] [ORACLE runtime-0001] | oracle | none | todo | demo |
| FLT-02 | Turning at the ship's turn rate | [MANUAL QuickStart] [BYTES ENG_LANG.DLL STRING 16] id 255 | manual | none | todo | unknown |
| FLT-03 | Braking | [MANUAL QuickStart] | manual | none | todo | unknown |
| FLT-04 | OverThruster / turbo thrust: extra thrust at high fuel cost | [MANUAL QuickStart] [MANUAL Race Courses] | manual | none | todo | unknown |
| FLT-05 | Auto braking: brake whenever thrust stops | [MANUAL Game Settings] | manual | none | todo | unknown |
| FLT-06 | Drift with drag when auto braking is off | [MANUAL Game Settings] | manual | none | todo | unknown |
| FLT-07 | Mass from armour, thruster and tank; heavier ships turn slower, use more fuel, top out lower | [MANUAL Customizing Ships] [BYTES ENG_LANG.DLL STRING 16] ids 254–255 [BYTES ENG_LANG.DLL STRING 17] ids 256–269 | manual | none | todo | unknown |
| FLT-08 | Collisions with rocks and objects | [MANUAL QuickStart] [BYTES ENG_LANG.DLL 5000 302] [BYTES ISPACE.DLL seg2:0x3c] | manual | located | todo | unknown |
| FLT-09 | Loss of control after capturing a virused icon, until auto-repair purges it | [MANUAL Icons] | manual | none | todo | unknown |
| FLT-10 | Loss of control after eating a doughnut | [BYTES ENG_LANG.DLL 5000 125] | strings | none | todo | unknown |
| FLT-11 | Body-thrusting ships: frames animate instead of a thruster flame | [MANUAL Editing Ship Bitmaps] | manual | none | todo | unknown |
| FLT-12 | Wings modelled as thin planes and the fuselage as a cone; wings shield the fuselage | [MANUAL Editing Ship Bitmaps] | manual | none | todo | unknown |
| FLT-13 | Game time scales with the frame rate; calibrated for 36 frames/s | [MANUAL Game Settings] [BYTES seg7:0xb32] | manual | understood | todo | unknown |
| FLT-14 | Being pulled by a fan | [BYTES ENG_LANG.DLL STRING 33] id 515 [BYTES ENG_LANG.DLL STRING 137] id 2179 | strings | none | todo | unknown |
| FLT-15 | The wave world wraps: x past DGROUP:0x5a0 × 8 and y past DGROUP:0x5a2 × 8 return to 0, negative values to the far edge | [BYTES seg8:0x1b54] | code | understood | todo | unknown |

## WPN: Weapons

Every weapon ENG_LANG names (strings 186–207 and the descriptions in #5000 110–130). The Noble Weapons are in DMN. #5000 138, the Plus Pack text, lists ten weapons as new. Those rows say registered.

| ID | Feature | Evidence | Spec | RE | Port | Edition |
|---|---|---|---|---|---|---|
| WPN-01 | Gun: shots in the thrust colour; DOUBLE GUNS upgrade | [MANUAL QuickStart] [MANUAL Customizing Ships] [BYTES ENG_LANG.DLL STRING 75] id 1185 [BYTES ENG_LANG.DLL STRING 143] id 2285 | manual | none | todo | unknown |
| WPN-02 | NO WEAPON (empty weapon system) | [BYTES ENG_LANG.DLL STRING 12] id 186 | strings | none | todo | unknown |
| WPN-03 | RING OF DEATH: ring of fiery explosions around the ship; heavy fuel use | [BYTES ENG_LANG.DLL STRING 12] id 187 [BYTES ENG_LANG.DLL 5000 110] [BYTES ENG_LANG.DLL 5000 138] | strings | none | todo | registered |
| WPN-04 | GRAPPLING HOOK: pulls objects in, swings them as a shield, releases as a slingshot | [BYTES ENG_LANG.DLL STRING 12] id 188 [BYTES ENG_LANG.DLL 5000 111] [BYTES ENG_LANG.DLL 5000 138] | strings | none | todo | registered |
| WPN-05 | LIGHTNING: electric damage; rocks store and discharge charge | [MANUAL Special Ship Systems] [BYTES ENG_LANG.DLL STRING 12] id 189 [BYTES ENG_LANG.DLL 5000 112] [BYTES ENG_LANG.DLL 5000 138] | manual | none | todo | registered |
| WPN-06 | SNOWBALLS: fuel-made, moderate damage, cools hot objects | [MANUAL Dealing With Other Ships] [BYTES ENG_LANG.DLL STRING 12] id 190 [BYTES ENG_LANG.DLL 5000 113] | manual | none | todo | unknown |
| WPN-07 | FIREBALLS: fuel-made, damage, heats objects | [MANUAL Dealing With Other Ships] [BYTES ENG_LANG.DLL STRING 12] id 191 [BYTES ENG_LANG.DLL 5000 114] | manual | none | todo | unknown |
| WPN-08 | TARGETTING MISSILES: pursue the selected target, dodge obstacles | [BYTES ENG_LANG.DLL STRING 13] id 192 [BYTES ENG_LANG.DLL 5000 115] [BYTES ENG_LANG.DLL 5000 138] | strings | none | todo | registered |
| WPN-09 | HEATSEEKER MISSILES: seek hot objects, may return to the launcher | [MANUAL Special Ship Systems] [BYTES ENG_LANG.DLL STRING 13] id 193 [BYTES ENG_LANG.DLL 5000 116] | manual | none | todo | unknown |
| WPN-10 | RADAR-GUIDED MISSILES: seek the closest object in range | [BYTES ENG_LANG.DLL STRING 13] id 194 [BYTES ENG_LANG.DLL 5000 117] | strings | none | todo | unknown |
| WPN-11 | DEATH HEADS: rear mines that seek heat, shoot, then explode with a shock blast | [BYTES ENG_LANG.DLL STRING 13] id 195 [BYTES ENG_LANG.DLL 5000 118] [BYTES ENG_LANG.DLL 5000 138] | strings | none | todo | registered |
| WPN-12 | REAR-DEPLOYED MINES: damage and a stunning shock blast | [BYTES ENG_LANG.DLL STRING 13] id 196 [BYTES ENG_LANG.DLL 5000 119] | strings | none | todo | unknown |
| WPN-13 | DUMB MISSILES: straight line; shock blast on a near miss | [BYTES ENG_LANG.DLL STRING 13] id 197 [BYTES ENG_LANG.DLL 5000 120] | strings | none | todo | unknown |
| WPN-14 | FIRE DEMONS: spinning heat-seekers that shoot fire all round | [BYTES ENG_LANG.DLL STRING 13] id 198 [BYTES ENG_LANG.DLL 5000 121] [BYTES ENG_LANG.DLL 5000 138] | strings | none | todo | registered |
| WPN-15 | WILDCARD DEFENSE: turns threatening objects in range into harmless fruit | [MANUAL Special Ship Systems] [BYTES ENG_LANG.DLL STRING 13] id 199 [BYTES ENG_LANG.DLL 5000 122] [BYTES ENG_LANG.DLL 5000 138] | manual | none | todo | registered |
| WPN-16 | BATTERIES: heat-seekers that discharge lightning at a hot object | [BYTES ENG_LANG.DLL STRING 13] id 200 [BYTES ENG_LANG.DLL 5000 123] | strings | none | todo | unknown |
| WPN-17 | HANDCUFFS: forcefield rope between two objects; Enforcers drag offenders out | [MANUAL Laws of Inner Space] [BYTES ENG_LANG.DLL STRING 13] id 201 [BYTES ENG_LANG.DLL 5000 124] | manual | none | todo | unknown |
| WPN-18 | DOUGHNUTS: eaten cause loss of control, else explode; best against Enforcers | [BYTES ENG_LANG.DLL STRING 13] id 202 [BYTES ENG_LANG.DLL 5000 125] | strings | none | todo | unknown |
| WPN-19 | PROTECTIVE PELLETS: fast interceptors of nearby threats | [BYTES ENG_LANG.DLL STRING 13] id 203 [BYTES ENG_LANG.DLL 5000 126] | strings | none | todo | unknown |
| WPN-20 | BEER BOTTLES: straight-line projectiles that smash into a shock blast | [BYTES ENG_LANG.DLL STRING 13] id 204 [BYTES ENG_LANG.DLL 5000 127] [BYTES ENG_LANG.DLL 5000 138] | strings | none | todo | registered |
| WPN-21 | BOOMERANGS: return to the sender if they miss; reusable when caught | [BYTES ENG_LANG.DLL STRING 13] id 205 [BYTES ENG_LANG.DLL 5000 128] [BYTES ENG_LANG.DLL 5000 138] | strings | none | todo | registered |
| WPN-22 | SPUD MISSILES: heavy flying potatoes | [BYTES ENG_LANG.DLL STRING 13] id 206 [BYTES ENG_LANG.DLL 5000 129] [BYTES ENG_LANG.DLL 5000 138] | strings | none | todo | registered |
| WPN-23 | THROWING STARS: light, fast, high damage | [BYTES ENG_LANG.DLL STRING 13] id 207 [BYTES ENG_LANG.DLL 5000 130] | strings | none | todo | unknown |
| WPN-24 | Weapon effects on icons: burn, crack, smash, freeze, melt; frozen icons need thawing before capture | [MANUAL Icons] [MANUAL Dealing With Other Ships] | manual | none | todo | unknown |
| WPN-25 | Each ship starts with an initial weapon | [MANUAL General] [MANUAL Customizing Ships] | manual | none | todo | unknown |
| WPN-26 | Full version: more than 20 weapons | [MANUAL Why Register Inner Space?] [BYTES ENG_LANG.DLL 5000 330] | manual | none | todo | registered |
| WPN-27 | Generic damage and destruction: a hit adds its amount to damage (pool 15 ignored; an object already past max_damage takes no more, except in pool 9); once damage > max_damage, on_destroy runs. The generic destroy handler blanks the object's screen square when seg8:0x7ba8 allows, fills effect-table fragment entries (count from blast_size, other values scaled by Detail), sets state 1 and calls the sound wrapper with sound 0x45 | [BYTES seg7:0x2652] [BYTES seg12:0x274a] | code | understood | todo | unknown |

## HAZ: Hazards

Every hazard ENG_LANG names: strings 2170–2181 (plurals 12170–12181), plus rocks, viruses and the blocks.

| ID | Feature | Evidence | Spec | RE | Port | Edition |
|---|---|---|---|---|---|---|
| HAZ-01 | Viruses: plasma balls that infect icons with lightning, strike nearby objects, then recharge | [MANUAL Hazards] [BYTES ENG_LANG.DLL 5000 304] [BYTES ENG_LANG.DLL STRING 11] id 170 | manual | none | todo | unknown |
| HAZ-02 | Virused icons: green slime, no resources, legal to destroy | [MANUAL Icons] [BYTES ENG_LANG.DLL 5000 321] | manual | none | todo | unknown |
| HAZ-03 | Rocks: various sizes, massive and hard; hot rocks melt icons | [MANUAL QuickStart] [MANUAL Dealing With Other Ships] [BYTES ENG_LANG.DLL 5000 302] [BYTES ENG_LANG.DLL STRING 10] id 153 [ORACLE runtime-0001] | oracle | none | todo | demo |
| HAZ-04 | FIRE BLOCK | [BYTES ENG_LANG.DLL STRING 136] id 2170 | strings | none | todo | unknown |
| HAZ-05 | SNOW BLOCK | [BYTES ENG_LANG.DLL STRING 136] id 2171 | strings | none | todo | unknown |
| HAZ-06 | COURSE FLAG | [BYTES ENG_LANG.DLL STRING 136] id 2172 [MANUAL Race Courses] [BYTES seg4:0x215c] | strings | located | todo | unknown |
| HAZ-07 | WALL | [BYTES ENG_LANG.DLL STRING 136] id 2173 [MANUAL Race Courses] | strings | none | todo | unknown |
| HAZ-08 | GUN TURRET (help keyword Turrets) | [BYTES ENG_LANG.DLL STRING 136] id 2174 | strings | none | todo | unknown |
| HAZ-09 | MINE LAYER | [BYTES ENG_LANG.DLL STRING 136] id 2175 | strings | none | todo | unknown |
| HAZ-10 | MISSILE LAUNCHER | [BYTES ENG_LANG.DLL STRING 137] id 2176 | strings | none | todo | unknown |
| HAZ-11 | FLAME THROWER | [BYTES ENG_LANG.DLL STRING 137] id 2177 | strings | none | todo | unknown |
| HAZ-12 | SNOW BLOWER | [BYTES ENG_LANG.DLL STRING 137] id 2178 | strings | none | todo | unknown |
| HAZ-13 | FAN | [BYTES ENG_LANG.DLL STRING 137] id 2179 [BYTES ENG_LANG.DLL STRING 33] id 515 | strings | none | todo | unknown |
| HAZ-14 | DOUGHNUT LAUNCHER | [BYTES ENG_LANG.DLL STRING 137] id 2180 [MANUAL Laws of Inner Space] | strings | none | todo | unknown |
| HAZ-15 | BATTERY LAUNCHER | [BYTES ENG_LANG.DLL STRING 137] id 2181 | strings | none | todo | unknown |
| HAZ-16 | Gate Block | [BYTES ENG_LANG.DLL STRING 23] id 362 | strings | none | todo | unknown |
| HAZ-17 | Hazard Block (help keyword Blocks) | [BYTES ENG_LANG.DLL STRING 23] id 363 [BYTES ENG_LANG.DLL STRING 75] id 1192 | strings | none | todo | unknown |
| HAZ-18 | Hazard count by wave and difficulty; Gentle has fewer objects | [MANUAL Game Settings] [MANUAL Race Courses] | manual | none | todo | unknown |
| HAZ-19 | Hazard summary text: harmless or shooting at nearby ships, how hard to destroy | [BYTES ENG_LANG.DLL STRING 190] id 3025 | strings | none | todo | unknown |

## SHIP: Ship systems and powerups

| ID | Feature | Evidence | Spec | RE | Port | Edition |
|---|---|---|---|---|---|---|
| SHIP-01 | Ship systems Engine, Gun, Thruster, Wings, Cloak, each with its own damage | [MANUAL General] [BYTES ENG_LANG.DLL STRING 53] ids 840–844 | manual | none | todo | unknown |
| SHIP-02 | Auto-repair: fuel-driven, off below 15% fuel, cannot regrow wings; completion messages | [MANUAL Special Ship Systems] [MANUAL Resource Management] [BYTES ENG_LANG.DLL STRING 33] id 517 [BYTES ENG_LANG.DLL STRING 9] id 138 | manual | none | todo | unknown |
| SHIP-03 | Cloaking: invisible to ships, no capture or weapons but the gun, uses fuel, drops out under damage | [MANUAL Special Ship Systems] [BYTES ENG_LANG.DLL STRING 65] id 1031 [BYTES ENG_LANG.DLL STRING 65] id 1033 | manual | none | todo | unknown |
| SHIP-04 | Shielding: multi-frame ships only, extra damage resistance, no capture or weapons but the gun, no fuel | [MANUAL Special Ship Systems] [BYTES ENG_LANG.DLL STRING 65] id 1030 [BYTES ENG_LANG.DLL STRING 65] id 1032 | manual | none | todo | unknown |
| SHIP-05 | Perspective shifting: trade places with a teammate in the wave | [MANUAL Special Ship Systems] | manual | none | todo | unknown |
| SHIP-06 | Wings torn off by damage; wing taunts | [MANUAL Special Ship Systems] [BYTES ENG_LANG.DLL STRING 24] ids 373–383 | manual | none | todo | unknown |
| SHIP-07 | Ship on fire (almost dead): must buy a new body in the ambulance | [MANUAL Resource Management] | manual | none | todo | unknown |
| SHIP-08 | Damage warnings: "DANGER: ... REPAIR OR ABORT MISSION", TROUBLE, FAILURE; shape GOOD/FAIR/BAD/SORRY | [BYTES ENG_LANG.DLL STRING 9] id 134 [BYTES ENG_LANG.DLL STRING 9] id 135 [BYTES ENG_LANG.DLL STRING 33] id 519 [BYTES ENG_LANG.DLL STRING 20] ids 311–315 | strings | none | todo | unknown |
| SHIP-09 | Powerups: Engine, Gun, Thruster and Wing armour and gun power, 4 stages each at rising cost | [MANUAL Resource Management] [MANUAL Why Register Inner Space?] [BYTES ENG_LANG.DLL STRING 124] ids 1969–1978 | manual | none | todo | registered |
| SHIP-10 | Ship roster: over 100 ships in the full version; the demo allows only the Transformer | [MANUAL Why Register Inner Space?] [ORACLE newgame-0001] [BYTES ENG_LANG.DLL STRING 146] id 2326 [BYTES ENG_LANG.DLL STRING 146] id 2327 | oracle | none | todo | registered |
| SHIP-11 | Ship list from MASTER.BOD and MASTER.SHP: the demo's MASTER.SHP holds 118 ships; the New Game list shows 61 | [ORACLE newgame-0001] [BYTES seg14:0x3eda] | oracle | located | todo | demo |
| SHIP-12 | Toughness ratings BEYOND REASON .. DEFENSELESS | [BYTES ENG_LANG.DLL STRING 45] ids 709–719 [BYTES ENG_LANG.DLL STRING 46] ids 720–722 | strings | none | todo | unknown |
| SHIP-13 | Mass, speed and turning descriptors (MASSIVE .. MOVES LIKE A COW) | [BYTES ENG_LANG.DLL STRING 49] ids 770–783 | strings | none | todo | unknown |
| SHIP-14 | Opponents matched to the quality of the player's ship | [MANUAL Editing Ship Bitmaps] | manual | none | todo | unknown |
| SHIP-15 | Damage to ships is scaled by 7/8; damage to the player's ship then by (DGROUP:0x9000 + 6)/8 | [BYTES seg8:0x45e0] | code | understood | todo | unknown |
| SHIP-16 | One special ability per ship, by precedence: pull-off wings (3, body flag 0x04), else cloak (4, SHP +0x09 bit 0x08), else shield (5, body flag 0x02), else none (0xffff) | [BYTES seg14:0x1d8c] | code | understood | todo | unknown |
| SHIP-17 | Design values from MASTER.SHP and MASTER.BOD: Fuel Tank × 40, Thruster × 5, strength = body base × 10 × (armor/4 + 100)/100 (also pool word +0x44, and / 22 into +0x48) | [BYTES seg14:0x1e50] [BYTES seg14:0x1d1c] | code | understood | todo | unknown |

## TEAM: Teams and attitudes

| ID | Feature | Evidence | Spec | RE | Port | Edition |
|---|---|---|---|---|---|---|
| TEAM-01 | Avengers: team behaviour (e.g. Sky Talon, Transformer, Biplane) | [MANUAL Avengers] [BYTES ENG_LANG.DLL 5000 306] [MANUAL Dealing With Other Ships] | manual | none | todo | unknown |
| TEAM-02 | Predators: team behaviour (e.g. Jaguar, Tiger, Shark) | [MANUAL Predators] [BYTES ENG_LANG.DLL 5000 307] [MANUAL Dealing With Other Ships] | manual | none | todo | unknown |
| TEAM-03 | Fuzzy Ones: team behaviour (e.g. Rubber Duck, Teddy Bear) | [MANUAL Fuzzy Ones] [BYTES ENG_LANG.DLL 5000 308] [MANUAL Dealing With Other Ships] | manual | none | todo | unknown |
| TEAM-04 | Pirates: team behaviour (e.g. Rogue, Grim Reaper, Scoundrel, Lackey) | [MANUAL Pirates] [BYTES ENG_LANG.DLL 5000 309] [MANUAL Dealing With Other Ships] | manual | none | todo | unknown |
| TEAM-05 | Enforcers: team behaviour (e.g. Patrolman, Interceptor, Terminator) | [MANUAL Enforcers] [BYTES ENG_LANG.DLL 5000 313] [MANUAL Dealing With Other Ships] | manual | none | todo | unknown |
| TEAM-06 | Knights: team behaviour (e.g. Samurai, Excalibur, Dolphin, ZenMaster) | [MANUAL Knights] [BYTES ENG_LANG.DLL 5000 310] [MANUAL Dealing With Other Ships] | manual | none | todo | unknown |
| TEAM-07 | Speed Demons: team behaviour (e.g. Screaming Rod, Viper, Thunderbird) | [MANUAL Speed Demons] [BYTES ENG_LANG.DLL 5000 311] [MANUAL Dealing With Other Ships] | manual | none | todo | unknown |
| TEAM-08 | Renegades: team behaviour (e.g. Outlaw Falcon, Midnight Dragon) | [MANUAL Renegades] [BYTES ENG_LANG.DLL 5000 312] [MANUAL Dealing With Other Ships] | manual | none | todo | unknown |
| TEAM-09 | Team names from INSPACE.INI [Teams] Team1..Team8 | [ORACLE newgame-0001] [BYTES INSPACE.INI] [BYTES seg13:0x218a] | oracle | located | todo | demo |
| TEAM-10 | New Game TEAM field follows the ship (TRANSFORMER: AVENGERS) | [ORACLE newgame-0001] | oracle | none | todo | demo |
| TEAM-11 | Attitude to the player shown in the target display: friend, neutral, enemy | [MANUAL General] | manual | none | todo | unknown |
| TEAM-12 | Relation levels NEMESIS, HOSTILE, NEUTRAL, FRIENDLY, ALLY | [BYTES ENG_LANG.DLL STRING 37] ids 580–584 | strings | none | todo | unknown |
| TEAM-13 | Ship attitude to friends (LOYAL, HELPFUL, FRIENDLY, DISINTERESTED) and enemies (PSYCHOPATHIC, HOSTILE, UNFRIENDLY, DISINTERESTED) | [MANUAL Customizing Ships] [BYTES ENG_LANG.DLL STRING 16] ids 246–253 | manual | none | todo | unknown |
| TEAM-14 | Consequences: attacks make enemies of the victim's friends, defending makes friends, killers face revenge | [MANUAL Dealing With Other Ships] [BYTES ENG_LANG.DLL STRING 54] ids 850–857 [BYTES ENG_LANG.DLL STRING 75] id 1187 | manual | none | todo | unknown |
| TEAM-15 | Game violence follows the player's violence | [MANUAL Dealing With Other Ships] | manual | none | todo | unknown |
| TEAM-16 | Memory of past encounters ("I Remember That You Killed a ...") | [BYTES ENG_LANG.DLL STRING 190] id 3033 [BYTES ENG_LANG.DLL STRING 128] id 2046 [BYTES ENG_LANG.DLL STRING 57] ids 900–907 | strings | none | todo | unknown |
| TEAM-17 | Team relations overview in help | [BYTES ENG_LANG.DLL STRING 21] id 320 [BYTES ENG_LANG.DLL STRING 57] id 898 [BYTES ENG_LANG.DLL STRING 50] id 789 | strings | none | todo | unknown |
| TEAM-18 | When the player targets a ship while DGROUP:0x92e8 = 0, seg3:0xf4 loads TEAMMATE (string 322) into DGROUP:0x960a for a ship of the player's team, and otherwise passes the target's team and DGROUP:0x960a to seg13:0x218a, which reads that team's name from [Teams] | [BYTES seg3:0xf4] [BYTES ENG_LANG.DLL STRING 21] id 322 | code | understood | todo | unknown |

## LAW: Laws, Enforcers and the Hall of Justice

The nine crimes of the Laws of Inner Space topic, with the charge text the Hall of Justice reads (strings 1093–1107), then the charges the manual does not list.

| ID | Feature | Evidence | Spec | RE | Port | Edition |
|---|---|---|---|---|---|---|
| LAW-01 | Crime: Stealing Doughnuts from an Enforcer (charge "Taking An Enforcer's Doughnut") | [MANUAL Laws of Inner Space] [BYTES ENG_LANG.DLL STRING 69] id 1102 [BYTES ENG_LANG.DLL STRING 61] id 961 | manual | none | todo | unknown |
| LAW-02 | Crime: Destroying Doughnut Launchers ("Destroying Doughnut Dispensors") | [MANUAL Laws of Inner Space] [BYTES ENG_LANG.DLL STRING 70] id 1104 [BYTES ENG_LANG.DLL STRING 61] id 962 | manual | none | todo | unknown |
| LAW-03 | Crime: Attacking a Fleeing Ship ("Attacking The Defenseless") | [MANUAL Laws of Inner Space] [BYTES ENG_LANG.DLL STRING 69] id 1101 [BYTES ENG_LANG.DLL STRING 60] id 950 | manual | none | todo | unknown |
| LAW-04 | Crime: Taking a Dead Ship's Resource Pack ("Removing Evidence") | [MANUAL Laws of Inner Space] [BYTES ENG_LANG.DLL STRING 69] id 1100 [BYTES ENG_LANG.DLL STRING 36] id 572 | manual | none | todo | unknown |
| LAW-05 | Crime: Destroying Good (Non-virused) Icons ("Destroying An Icon") | [MANUAL Laws of Inner Space] [BYTES ENG_LANG.DLL STRING 69] id 1099 [BYTES ENG_LANG.DLL STRING 36] id 570 | manual | none | todo | unknown |
| LAW-06 | Crime: Unsportsmanlike Conduct ("Unsportsmanlike Race Conduct") | [MANUAL Laws of Inner Space] [BYTES ENG_LANG.DLL STRING 70] id 1106 [BYTES ENG_LANG.DLL STRING 61] id 963 | manual | none | todo | unknown |
| LAW-07 | Crime: Resisting Arrest | [MANUAL Laws of Inner Space] [BYTES ENG_LANG.DLL STRING 70] id 1105 | manual | none | todo | unknown |
| LAW-08 | Crime: Attacking An Enforcer ("Firing Upon An Officer", "Murdering An Enforcer") | [MANUAL Laws of Inner Space] [BYTES ENG_LANG.DLL STRING 69] id 1103 [BYTES ENG_LANG.DLL STRING 69] id 1098 | manual | none | todo | unknown |
| LAW-09 | Crime: Reckless Thrusting ("Thrusting Recklessly") | [MANUAL Laws of Inner Space] [BYTES ENG_LANG.DLL STRING 69] id 1093 [BYTES ENG_LANG.DLL STRING 61] id 964 | manual | none | todo | unknown |
| LAW-10 | Charge: "Using Excessive Force" | [BYTES ENG_LANG.DLL STRING 61] id 960 | strings | none | todo | unknown |
| LAW-11 | Charge: "Being An Idiot" ("It's Been A Slow Day") | [BYTES ENG_LANG.DLL STRING 70] id 1107 [BYTES ENG_LANG.DLL STRING 70] id 1108 | strings | none | todo | unknown |
| LAW-12 | Enforcers act only on crimes they witness | [MANUAL Laws of Inner Space] | manual | none | todo | unknown |
| LAW-13 | Patrol Class (Patrolman, Constable): patrol, warn, call an Interceptor | [MANUAL Laws of Inner Space] [BYTES ENG_LANG.DLL STRING 46] id 730 | manual | none | todo | unknown |
| LAW-14 | Intercept Class (Interceptor, Lethal Weapon): handcuff and escort out the gate | [MANUAL Laws of Inner Space] [BYTES ENG_LANG.DLL STRING 35] id 551 | manual | none | todo | unknown |
| LAW-15 | Terminator Class (Terminator, Eliminator): destroy those who resist | [MANUAL Laws of Inner Space] [BYTES ENG_LANG.DLL STRING 37] id 576 | manual | none | todo | unknown |
| LAW-16 | Enforcer warnings and lines | [BYTES ENG_LANG.DLL STRING 35] ids 550–558 [BYTES ENG_LANG.DLL STRING 54] ids 860–863 | strings | none | todo | unknown |
| LAW-17 | Arrest: handcuffed and escorted to the Hall of Justice; roughed up when resisting | [MANUAL Laws of Inner Space] [BYTES seg7:0x652] | manual | understood | todo | unknown |
| LAW-18 | Outstanding arrest warrant brings another Enforcer later | [MANUAL Laws of Inner Space] [BYTES ENG_LANG.DLL STRING 19] id 291 [BYTES ENG_LANG.DLL STRING 122] id 1940 | manual | none | todo | unknown |
| LAW-19 | Hall of Justice: charges, arrest event and punishment by prior arrests, severity and wealth | [MANUAL Laws of Inner Space] [BYTES ENG_LANG.DLL STRING 57] ids 908–911 [BYTES ENG_LANG.DLL STRING 58] ids 912–923 [BYTES ENG_LANG.DLL STRING 46] id 723 [BYTES seg16:0x697f] | manual | understood | todo | unknown |
| LAW-20 | Punishments: fine in icons, confiscation, insults | [BYTES ENG_LANG.DLL STRING 46] id 725 [BYTES ENG_LANG.DLL STRING 46] id 726 [BYTES ENG_LANG.DLL STRING 40] ids 630–637 [BYTES seg16:0x697f] | code | understood | todo | unknown |
| LAW-21 | Team lawfulness: Knights lawful, Pirates flout the law | [MANUAL Laws of Inner Space] | manual | none | todo | unknown |
| LAW-22 | No Enforcers in duel directories | [MANUAL Duelling] | manual | none | todo | unknown |
| LAW-23 | Taking race icons without winning is illegal | [MANUAL Race Courses] | manual | none | todo | unknown |
| LAW-24 | Enforcers arrest computer ships too ("Arresting %D For") | [BYTES ENG_LANG.DLL STRING 32] id 508 | strings | none | todo | unknown |

## ECO: Ambulance, credit and fuel

| ID | Feature | Evidence | Spec | RE | Port | Edition |
|---|---|---|---|---|---|---|
| ECO-01 | Ambulance call: dispatch, busy and no-credit replies | [MANUAL Resource Management] [BYTES ENG_LANG.DLL STRING 7] id 110 [BYTES ENG_LANG.DLL STRING 22] id 348 [BYTES ENG_LANG.DLL STRING 72] id 1136 [BYTES seg7:0x5ce] | manual | understood | todo | unknown |
| ECO-02 | Icon resources become credit on landing ("TRANSFERRING ... ICONS") | [MANUAL Resource Management] [BYTES ENG_LANG.DLL STRING 74] id 1173 [BYTES ENG_LANG.DLL STRING 74] id 1174 | manual | none | todo | unknown |
| ECO-03 | Basic and wing repairs, available in debt | [MANUAL Resource Management] [BYTES ENG_LANG.DLL STRING 158] id 2516 [BYTES ENG_LANG.DLL STRING 158] id 2517 | manual | none | todo | unknown |
| ECO-04 | Refuelling in the ambulance, available in debt | [MANUAL Resource Management] [BYTES ENG_LANG.DLL STRING 74] id 1176 [BYTES ENG_LANG.DLL STRING 74] id 1177 | manual | none | todo | unknown |
| ECO-05 | Deep debt: the ambulance stops coming | [MANUAL Resource Management] | manual | none | todo | unknown |
| ECO-06 | Buying weapons: affordable items only; weapon limit, sell before buying more | [MANUAL Resource Management] [BYTES ENG_LANG.DLL STRING 127] id 2023 [BYTES ENG_LANG.DLL STRING 127] id 2024 | manual | none | todo | unknown |
| ECO-07 | Selling weapons | [MANUAL Resource Management] [BYTES ENG_LANG.DLL STRING 73] id 1159 | manual | none | todo | unknown |
| ECO-08 | Reloading missiles | [MANUAL General] [BYTES ENG_LANG.DLL STRING 75] id 1186 [BYTES ENG_LANG.DLL STRING 74] id 1182 | manual | none | todo | unknown |
| ECO-09 | New body: forced when on fire, any body including other teams', team unchanged | [MANUAL Resource Management] [BYTES ENG_LANG.DLL STRING 55] id 865 [BYTES ENG_LANG.DLL STRING 55] id 866 | manual | none | todo | unknown |
| ECO-10 | Demo refuses to sell powerful items | [BYTES ENG_LANG.DLL STRING 146] id 2320 [BYTES ENG_LANG.DLL STRING 146] id 2321 | strings | none | todo | demo |
| ECO-11 | Cost and credit readouts (TOTAL COST OF WORK, YOUR CREDIT IS NOW) | [BYTES ENG_LANG.DLL STRING 74] id 1179 [BYTES ENG_LANG.DLL STRING 73] id 1158 | strings | none | todo | unknown |
| ECO-12 | Other ships use the ambulance ("%D HAS LANDED..", "REFUELING %D...") | [MANUAL Resource Management] [BYTES ENG_LANG.DLL STRING 75] id 1184 [BYTES ENG_LANG.DLL STRING 159] id 2540 | manual | none | todo | unknown |
| ECO-13 | Icon capture by flying into the icon; resources gained | [MANUAL Icons] [MANUAL QuickStart] [BYTES ENG_LANG.DLL STRING 36] id 574 [BYTES ENG_LANG.DLL STRING 36] id 575 | manual | none | todo | unknown |
| ECO-14 | Resource packs left by dead ships | [BYTES ENG_LANG.DLL STRING 58] id 926 [BYTES ENG_LANG.DLL STRING 43] id 680 | strings | none | todo | unknown |
| ECO-15 | Icon fragments | [BYTES ENG_LANG.DLL STRING 56] id 892 | strings | none | todo | unknown |
| ECO-16 | Fuel tanks: captured by flying over; volatile, may explode | [MANUAL QuickStart] [BYTES ENG_LANG.DLL 5000 301] [BYTES ENG_LANG.DLL STRING 10] id 155 | manual | none | todo | unknown |
| ECO-17 | Fuel use by thrust, weapons, repair, cloak and turbo | [MANUAL Special Ship Systems] [BYTES ENG_LANG.DLL 5000 301] | manual | none | todo | unknown |
| ECO-18 | Low fuel warning "FUEL LEVEL LOW. SEEK FUEL ASAP" and fuel readout | [BYTES ENG_LANG.DLL STRING 8] id 121 [BYTES ENG_LANG.DLL STRING 59] id 935 | strings | none | todo | unknown |

## AI: Computer ships and defenders

| ID | Feature | Evidence | Spec | RE | Port | Edition |
|---|---|---|---|---|---|---|
| AI-01 | Computer pilots capture, attack, defend, refuel and flee by team and attitude | [MANUAL Dealing With Other Ships] | manual | none | todo | unknown |
| AI-02 | Commands are requests: friendly, idle ships comply; refusal replies | [MANUAL Dealing With Other Ships] [BYTES ENG_LANG.DLL STRING 116] ids 1850–1855 [BYTES ENG_LANG.DLL STRING 117] ids 1856–1871 [BYTES ENG_LANG.DLL STRING 118] ids 1872–1874 [BYTES ENG_LANG.DLL STRING 121] ids 1920–1924 | manual | none | todo | unknown |
| AI-03 | Activity reports on request ("Capturing ...", "Fleeing From ...") | [MANUAL Dealing With Other Ships] [BYTES ENG_LANG.DLL STRING 29] ids 456–463 [BYTES ENG_LANG.DLL STRING 30] ids 464–479 [BYTES ENG_LANG.DLL STRING 31] ids 480–495 [BYTES ENG_LANG.DLL STRING 32] ids 496–511 [BYTES ENG_LANG.DLL STRING 33] ids 512–516 [BYTES ENG_LANG.DLL STRING 66] ids 1040–1048 | manual | none | todo | unknown |
| AI-04 | Messages from ships: help requests, progress, challenges | [MANUAL General] [BYTES ENG_LANG.DLL STRING 88] ids 1404–1407 [BYTES ENG_LANG.DLL STRING 189] id 3020 [BYTES seg3:0x6ec] | manual | located | todo | unknown |
| AI-05 | Fight challenges accepted or declined | [MANUAL Dealing With Other Ships] [BYTES ENG_LANG.DLL STRING 21] ids 332–335 [BYTES ENG_LANG.DLL STRING 22] ids 336–338 [BYTES ENG_LANG.DLL STRING 23] ids 354–358 | manual | none | todo | unknown |
| AI-06 | Defenders: created by icons, more and fiercer at higher waves; attack attackers; hunt you when their icon dies | [MANUAL Icons] [BYTES ENG_LANG.DLL 5000 305] [BYTES ENG_LANG.DLL STRING 10] id 154 | manual | none | todo | unknown |
| AI-07 | Captured icon's defenders orbit and defend the player and take Alt commands | [MANUAL Icons] [BYTES ENG_LANG.DLL 5000 305] | manual | none | todo | unknown |
| AI-08 | Mid-wave arrivals announced (Pirate, Knight, Patrolman) | [BYTES ENG_LANG.DLL STRING 44] ids 695–703 [BYTES ENG_LANG.DLL STRING 45] ids 704–707 [BYTES ENG_LANG.DLL STRING 46] id 730 | strings | none | todo | unknown |
| AI-09 | All ships oppose viruses | [MANUAL Hazards] | manual | none | todo | unknown |
| AI-10 | Pirates steal and loot | [MANUAL Pirates] [BYTES ENG_LANG.DLL STRING 190] id 3031 [BYTES ENG_LANG.DLL STRING 44] id 701 | manual | none | todo | unknown |
| AI-11 | Faster ships' pilots think faster | [MANUAL Editing Ship Bitmaps] | manual | none | todo | unknown |
| AI-12 | Death lines and duel taunts | [BYTES ENG_LANG.DLL STRING 66] ids 1050–1055 [BYTES ENG_LANG.DLL STRING 67] id 1056 [BYTES ENG_LANG.DLL STRING 107] ids 1710–1711 [BYTES ENG_LANG.DLL STRING 108] ids 1712–1727 [BYTES ENG_LANG.DLL STRING 109] ids 1728–1743 [BYTES ENG_LANG.DLL STRING 110] ids 1744–1747 | strings | none | todo | unknown |

## RACE: Races and duels

| ID | Feature | Evidence | Spec | RE | Port | Edition |
|---|---|---|---|---|---|---|
| RACE-01 | Race-track directories in play: multi-lap race, the winner takes the icons | [MANUAL Race Courses] [BYTES ENG_LANG.DLL STRING 121] id 1930 [BYTES ENG_LANG.DLL STRING 121] id 1932 [BYTES seg4:0x215c] | manual | located | todo | unknown |
| RACE-02 | Test track Lazy Loop - RACE (enterable in the demo) | [MANUAL Race Courses] [ORACLE newgame-0001] [BYTES ENG_LANG.DLL STRING 47] id 740 [BYTES ENG_LANG.DLL STRING 48] id 764 | oracle | none | todo | demo |
| RACE-03 | Test track SDi Speedway - RACE | [MANUAL Race Courses] [ORACLE newgame-0001] [BYTES ENG_LANG.DLL STRING 47] id 741 [BYTES ENG_LANG.DLL STRING 146] id 2323 | oracle | none | todo | registered |
| RACE-04 | Test track Grand Prix - RACE | [MANUAL Race Courses] [ORACLE newgame-0001] [BYTES ENG_LANG.DLL STRING 47] id 742 [BYTES ENG_LANG.DLL STRING 146] id 2323 | oracle | none | todo | registered |
| RACE-05 | Race options: competitors, course difficulty, length, competitor speed | [MANUAL Race Courses] [BYTES seg6:0x23d4] | manual | located | todo | unknown |
| RACE-06 | Refuel at the start and end of every race | [MANUAL Race Courses] | manual | none | todo | unknown |
| RACE-07 | Race runs until every ship finishes | [MANUAL Race Courses] | manual | none | todo | unknown |
| RACE-08 | Weapons stay enabled in races; more hazards on higher waves | [MANUAL Race Courses] | manual | none | todo | unknown |
| RACE-09 | Race HUD: PREPARE TO RACE, GO, LAP, BEST LAP, LAPS TO GO, PLACE, RACE TIME, FINAL TIME | [BYTES ENG_LANG.DLL STRING 138] ids 2194–2204 [BYTES seg11:0x41bc] | strings | located | todo | unknown |
| RACE-10 | Ship Watch time gaps updated at each flag | [MANUAL Race Courses] [BYTES ENG_LANG.DLL STRING 68] ids 1080–1085 | manual | none | todo | unknown |
| RACE-11 | Race records per ship and course: best lap, 5 and 10 laps, human and computer | [MANUAL Race Records] [BYTES ENG_LANG.DLL 5000 137] [BYTES ENG_LANG.DLL STRING 130] ids 2067–2070 [BYTES seg6:0x8cb2] | manual | located | todo | unknown |
| RACE-12 | Record announcements (course and lap records) | [BYTES ENG_LANG.DLL STRING 141] ids 2252–2255 [BYTES ENG_LANG.DLL STRING 142] ids 2256–2265 [BYTES ENG_LANG.DLL STRING 189] id 3015 [BYTES ENG_LANG.DLL STRING 189] id 3016 | strings | none | todo | unknown |
| RACE-13 | Race course descriptions and strategy hints | [BYTES ENG_LANG.DLL STRING 101] ids 1600–1605 [BYTES ENG_LANG.DLL STRING 131] ids 2084–2088 | strings | none | todo | unknown |
| RACE-14 | Start gun; waiting for the start | [BYTES ENG_LANG.DLL 5000 103] [BYTES ENG_LANG.DLL STRING 68] id 1086 | strings | none | todo | unknown |
| RACE-15 | Duel directories (- DUEL, Dodge City): no icons, no exit until opponents fall, no help, no Enforcers | [MANUAL Duelling] [ORACLE newgame-0001] [BYTES ENG_LANG.DLL STRING 47] id 750 [BYTES ENG_LANG.DLL STRING 146] id 2323 | oracle | none | todo | registered |
| RACE-16 | Duel arena briefing | [BYTES ENG_LANG.DLL 5000 104] | strings | none | todo | unknown |
| RACE-17 | Invitation to Dodge City | [BYTES ENG_LANG.DLL STRING 40] id 629 | strings | none | todo | unknown |
| RACE-18 | Duel result summary (hits, damage, margin) | [BYTES ENG_LANG.DLL STRING 124] id 1983 [BYTES ENG_LANG.DLL STRING 125] ids 1984–1997 [BYTES ENG_LANG.DLL STRING 189] id 3018 | strings | none | todo | unknown |
| RACE-19 | Race place: 1 + the number of other ships ahead by \|laps\| × flags per lap + flag index, ties broken by the smaller word at DGROUP:0x8e48 + 2i; a ship counts only with ship-record byte +0xec ≠ 0 and pool state 0; computed only while the player's pool-14 byte +0x51 is 0 | [BYTES seg4:0x8b82] | code | understood | todo | unknown |
| RACE-20 | Race bonus to score and rating, n = ships − 1: place 1 with n ≠ 0 ((9 − DGROUP:0x94aa) × n + 4 × (DGROUP:0x94a8 + 5)) × 500; place 2 with n > 1 (8 + DGROUP:0x94a8) × 500 + (8 − DGROUP:0x94aa) × n × 200; 16-bit products; none when the player's laps − DGROUP:0x9ea0 = 1 | [BYTES seg4:0x2da2] | code | understood | todo | unknown |

## DMN: Inner Demon and Noble Weapons

| ID | Feature | Evidence | Spec | RE | Port | Edition |
|---|---|---|---|---|---|---|
| DMN-01 | Black hole: the Inner Demon pulls the player in from time to time ("APPROACHING EVENT HORIZON") | [MANUAL Inner Demon] [MANUAL General] [BYTES ENG_LANG.DLL STRING 127] id 2029 [BYTES ENG_LANG.DLL STRING 146] id 2322 | manual | none | todo | registered |
| DMN-02 | Demon battle is a dream: no lasting damage, return as on entry | [MANUAL Inner Demon] | manual | none | todo | registered |
| DMN-03 | Dragons curled around energy balls; feed them energy | [MANUAL Inner Demon] | manual | none | todo | registered |
| DMN-04 | The demon wields the Noble Weapons; win them by working out how | [MANUAL Inner Demon] [BYTES ENG_LANG.DLL STRING 122] id 1942 [BYTES ENG_LANG.DLL STRING 67] id 1065 [BYTES ENG_LANG.DLL STRING 138] id 2207 | manual | none | todo | registered |
| DMN-05 | Ambulance mini Inner Demon game for practice | [MANUAL Inner Demon] [BYTES ENG_LANG.DLL STRING 146] id 2322 | manual | none | todo | registered |
| DMN-06 | ENLIGHTENMENT | [MANUAL Inner Demon] [BYTES ENG_LANG.DLL STRING 14] id 208 [BYTES ENG_LANG.DLL 5000 131] | manual | none | todo | registered |
| DMN-07 | UNCERTAINTY | [MANUAL Inner Demon] [BYTES ENG_LANG.DLL STRING 14] id 209 [BYTES ENG_LANG.DLL 5000 132] | manual | none | todo | registered |
| DMN-08 | ATTRACTIVE FORCE | [MANUAL Inner Demon] [BYTES ENG_LANG.DLL STRING 14] id 211 [BYTES ENG_LANG.DLL 5000 134] | manual | none | todo | registered |
| DMN-09 | REPULSIVE FORCE | [MANUAL Inner Demon] [BYTES ENG_LANG.DLL STRING 14] id 210 [BYTES ENG_LANG.DLL 5000 133] | manual | none | todo | registered |
| DMN-10 | INNER FORCE: Unify Forces in the ambulance once both forces are held | [MANUAL Inner Demon] [BYTES ENG_LANG.DLL STRING 14] id 212 [BYTES ENG_LANG.DLL 5000 135] [BYTES ENG_LANG.DLL STRING 124] id 1970 [BYTES ENG_LANG.DLL STRING 124] id 1971 | manual | none | todo | registered |
| DMN-11 | Noble Weapons cannot be bought or sold | [MANUAL Inner Demon] | manual | none | todo | registered |
| DMN-12 | Demon's Gate: final challenge once all icons are done or with all Noble Weapons | [MANUAL General] [MANUAL Inner Demon] [ORACLE newgame-0001] [BYTES ENG_LANG.DLL STRING 48] ids 760–762 [BYTES ENG_LANG.DLL 5000 102] | oracle | none | todo | registered |
| DMN-13 | Game completion text ("YOU'VE DONE IT", "PREPARE TO LEAVE INNER SPACE") | [MANUAL General] [BYTES ENG_LANG.DLL STRING 133] id 2126 [BYTES ENG_LANG.DLL STRING 127] id 2022 [BYTES ENG_LANG.DLL STRING 191] id 3044 | manual | none | todo | unknown |

## SET: Settings dialogs

Every control of the Game Settings (#0064), Audio Options (#03e7), Display Options (#029a) and Joystick Configuration (#03cf) dialogs. The Detail Settings topic covers the Game Settings detail group.

| ID | Feature | Evidence | Spec | RE | Port | Edition |
|---|---|---|---|---|---|---|
| SET-01 | Game Settings: Master Volume scrollbar (Softer/Louder) | [MANUAL Game Settings] [BYTES ENG_LANG.DLL DIALOG 100] | manual | none | todo | unknown |
| SET-02 | Game Settings: sound system No Sound, Standard Sound, Dynamic Sound, 3D Dynamic Sound | [MANUAL Game Settings] [BYTES ENG_LANG.DLL DIALOG 100] [BYTES ENG_LANG.DLL STRING 140] ids 2233–2236 | manual | none | todo | unknown |
| SET-03 | Game Settings: Audio.. button | [MANUAL Game Settings] [BYTES ENG_LANG.DLL DIALOG 100] | manual | none | todo | unknown |
| SET-04 | Game Settings: Display.. button | [MANUAL QuickStart] [BYTES ENG_LANG.DLL DIALOG 100] | manual | none | todo | unknown |
| SET-05 | Game Settings: Joystick.. button | [MANUAL Joystick Settings] [BYTES ENG_LANG.DLL DIALOG 100] | manual | none | todo | unknown |
| SET-06 | Game Settings: Quickload Always, Mid-Wave, Never | [MANUAL Detail Settings] [BYTES ENG_LANG.DLL DIALOG 100] [BYTES ENG_LANG.DLL STRING 140] ids 2225–2227 | manual | none | todo | unknown |
| SET-07 | Game Settings: Play Detail Highest, High, Medium, Low | [MANUAL Detail Settings] [BYTES ENG_LANG.DLL DIALOG 100] [BYTES ENG_LANG.DLL STRING 139] ids 2221–2223 [BYTES ENG_LANG.DLL STRING 140] id 2224 | manual | none | todo | unknown |
| SET-08 | Game Settings: Zoom Level 2x, 1x, 1/2x | [MANUAL Game Settings] [BYTES ENG_LANG.DLL DIALOG 100] [BYTES ENG_LANG.DLL STRING 140] ids 2230–2232 [BYTES seg6:0xb07] | manual | understood | todo | unknown |
| SET-09 | Game Settings: Enhanced Graphics | [MANUAL Detail Settings] [BYTES ENG_LANG.DLL DIALOG 100] | manual | none | todo | unknown |
| SET-10 | Game Settings: Ship Auto Braking | [MANUAL Game Settings] [BYTES ENG_LANG.DLL DIALOG 100] | manual | none | todo | unknown |
| SET-11 | Game Settings: Difficulty Gentle, Easier, Normal, Harder, Extreme! | [MANUAL Game Settings] [BYTES ENG_LANG.DLL DIALOG 100] [BYTES ENG_LANG.DLL STRING 65] ids 1035–1039 | manual | none | todo | unknown |
| SET-12 | Game Settings: Play Speed scrollbar, 4 frames/s to Full Out ("Play Speed %s") | [MANUAL Game Settings] [BYTES ENG_LANG.DLL DIALOG 100] [BYTES ISPACE.DLL DGROUP:0x874] [BYTES ISPACE.DLL seg9:0x2242] | manual | understood | todo | unknown |
| SET-13 | Game Settings: Normal Speed button | [BYTES ENG_LANG.DLL DIALOG 100] | strings | none | todo | unknown |
| SET-14 | Game Settings: read-only Language, Sound FX and Voice FX (Normal or Compressed), version and release date | [BYTES ENG_LANG.DLL DIALOG 100] [BYTES ENG_LANG.DLL STRING 145] id 2309 [BYTES ENG_LANG.DLL STRING 145] id 2314 | strings | none | todo | unknown |
| SET-15 | Audio: Sound FX | [MANUAL Game Settings] [BYTES ENG_LANG.DLL DIALOG 999] | manual | none | todo | unknown |
| SET-16 | Audio: Voice Effects | [MANUAL Game Settings] [BYTES ENG_LANG.DLL DIALOG 999] | manual | none | todo | unknown |
| SET-17 | Audio: Music Soundtrack | [MANUAL Game Settings] [BYTES ENG_LANG.DLL DIALOG 999] | manual | none | todo | unknown |
| SET-18 | Audio: Effects vs. Music scrollbar | [MANUAL Game Settings] [BYTES ENG_LANG.DLL DIALOG 999] | manual | none | todo | unknown |
| SET-19 | Audio: Mix Channels 1–8 | [MANUAL Game Settings] [BYTES ENG_LANG.DLL DIALOG 999] | manual | none | todo | unknown |
| SET-20 | Audio: Stereo Effect drop-down; values [OPEN] (strings 2413–2416 Minimal, Less, Normal, More fit) | [BYTES ENG_LANG.DLL DIALOG 999] [BYTES ENG_LANG.DLL STRING 151] ids 2413–2415 [BYTES ENG_LANG.DLL STRING 152] id 2416 | strings | none | todo | unknown |
| SET-21 | Audio: Keep Sounds in Memory (faster loading) | [MANUAL Troubleshooting] [BYTES ENG_LANG.DLL DIALOG 999] | manual | none | todo | unknown |
| SET-22 | Audio: Swap left/right Channels | [BYTES ENG_LANG.DLL DIALOG 999] | strings | none | todo | unknown |
| SET-23 | Display: Enhanced Brightness | [MANUAL Display Options] [BYTES ENG_LANG.DLL DIALOG 666] | manual | none | todo | unknown |
| SET-24 | Display: App Menu | [MANUAL Display Options] [BYTES ENG_LANG.DLL DIALOG 666] | manual | none | todo | unknown |
| SET-25 | Display: Game Border | [MANUAL Display Options] [BYTES ENG_LANG.DLL DIALOG 666] [BYTES seg2:0xcb1] | manual | understood | todo | unknown |
| SET-26 | Display: Animated Ship Watches | [MANUAL Display Options] [BYTES ENG_LANG.DLL DIALOG 666] | manual | none | todo | unknown |
| SET-27 | Display: Always Ask for Save Name | [BYTES ENG_LANG.DLL DIALOG 666] | strings | none | todo | unknown |
| SET-28 | Display: Skip Sequences Skip None, Skip Some, Skip All | [MANUAL Game Settings] [BYTES ENG_LANG.DLL DIALOG 666] [BYTES ENG_LANG.DLL STRING 151] ids 2410–2412 | manual | none | todo | unknown |
| SET-29 | Joystick: Joystick Enabled | [MANUAL Joystick Settings] [BYTES ENG_LANG.DLL DIALOG 975] [BYTES ENG_LANG.DLL STRING 151] id 2404 | manual | none | todo | unknown |
| SET-30 | Joystick: Calibrate (upper left, button, lower right, button) | [MANUAL Joystick Settings] [BYTES ENG_LANG.DLL DIALOG 975] [BYTES ENG_LANG.DLL STRING 150] id 2399 [BYTES ENG_LANG.DLL STRING 151] ids 2400–2403 | manual | none | todo | unknown |
| SET-31 | Joystick: test area crosshair; button presses play the function's sound | [MANUAL Joystick Settings] [BYTES ENG_LANG.DLL DIALOG 975] | manual | none | todo | unknown |
| SET-32 | Joystick: 4 Buttons | [MANUAL Joystick Settings] [BYTES ENG_LANG.DLL DIALOG 975] | manual | none | todo | unknown |
| SET-33 | Joystick: Button 1–4 functions Fire Gun, Fire Weapon, Change Weapon, Turbo Thrust, Call Ambulance, New Ship Target, Cloak/Shield | [MANUAL Joystick Settings] [BYTES ENG_LANG.DLL DIALOG 975] [BYTES ENG_LANG.DLL STRING 152] ids 2420–2426 | manual | none | todo | unknown |

## INI: INI keys

Keys are in `INSPACE.INI` `[Inner Space]` unless the row names another file or section. A default is the value the demo build passed with its read in newgame-0001 (the run's bulk digest lists them). A control in parentheses is the dialog control the key name matches. The binding is [OPEN] until RE confirms it.

| ID | Feature | Evidence | Spec | RE | Port | Edition |
|---|---|---|---|---|---|---|
| INI-01 | `Path`: install directory; regenerated from the module path when missing | [ORACLE smoke-0001] [ORACLE newgame-0001] [BYTES seg6:0x70a] | oracle | understood | done | demo |
| INI-02 | `LocalPath`: regenerated with Path | [ORACLE smoke-0001] [ORACLE newgame-0001] [BYTES seg6:0x725] | oracle | understood | done | demo |
| INI-03 | `ShipPath`: ships directory (MASTER.BOD, MASTER.SHP) | [ORACLE smoke-0001] [ORACLE newgame-0001] [BYTES seg14:0x3dea] | oracle | understood | todo | demo |
| INI-04 | `SndPath`: sounds directory | [ORACLE smoke-0001] [ORACLE newgame-0001] [BYTES ISPACE.DLL seg9:0x152] | oracle | located | todo | demo |
| INI-05 | `RacePath`: directory of the race records (.rac) | [MANUAL Race Records] [ORACLE smoke-0001] [ORACLE newgame-0001] [BYTES seg6:0x9c8] | oracle | understood | todo | demo |
| INI-06 | `Dead`: regenerated as RETIRED | [ORACLE smoke-0001] [ORACLE newgame-0001] [BYTES seg6:0x978] | oracle | understood | todo | demo |
| INI-07 | `Games`: regenerated as GAMES | [ORACLE smoke-0001] [ORACLE newgame-0001] [BYTES seg6:0x9a0] | oracle | understood | todo | demo |
| INI-08 | `Language`: language code (ENG) for the _LANG, _FX and _HELP files | [ORACLE newgame-0001] [BYTES DGROUP:0x3a2] [BYTES seg10:0x668b] | oracle | located | todo | demo |
| INI-09 | `IgnoreRes`: default 0 | [ORACLE newgame-0001] [BYTES seg6:0x1401] | oracle | understood | todo | demo |
| INI-10 | `Full`: full screen, default 1 | [ORACLE newgame-0001] [BYTES seg6:0x14] [BYTES seg6:0x180e] | oracle | understood | done | demo |
| INI-11 | `ShowLeft`: default 0 | [ORACLE newgame-0001] [BYTES seg6:0x2b] [BYTES seg6:0x75] | oracle | understood | todo | demo |
| INI-12 | `ShowTop`: default 0 | [ORACLE newgame-0001] [BYTES seg6:0x41] [BYTES seg6:0x75] | oracle | understood | todo | demo |
| INI-13 | `ShowRight`: default 0 | [ORACLE newgame-0001] [BYTES seg6:0x57] [BYTES seg6:0x75] | oracle | understood | todo | demo |
| INI-14 | `ShowBottom`: default 0 | [ORACLE newgame-0001] [BYTES seg6:0x6d] [BYTES seg6:0x75] | oracle | understood | todo | demo |
| INI-15 | `IconWnd`: default 1 | [ORACLE newgame-0001] [BYTES seg6:0x112] [BYTES seg2:0x4f7] | oracle | understood | todo | demo |
| INI-16 | `VScale`: default 0 | [ORACLE newgame-0001] [BYTES seg6:0x128] [BYTES seg2:0x4f7] | oracle | understood | todo | demo |
| INI-17 | `SmWnd`: default 0 | [ORACLE newgame-0001] [BYTES seg6:0x13e] [BYTES seg2:0x4f7] | oracle | understood | todo | demo |
| INI-18 | `SScale`: default 0 | [ORACLE newgame-0001] [BYTES seg6:0x154] [BYTES seg2:0x4f7] | oracle | understood | todo | demo |
| INI-19 | `ShowLaunch`: default 2 | [ORACLE newgame-0001] [BYTES seg6:0x16a] | oracle | located | todo | demo |
| INI-20 | `Menu`: (App Menu), default 0 | [ORACLE newgame-0001] [BYTES seg6:0x180] [BYTES seg2:0xd88] | oracle | understood | todo | demo |
| INI-21 | `RadarLeft`: default 0 | [ORACLE newgame-0001] [BYTES seg6:0x196] | oracle | located | todo | demo |
| INI-22 | `RadarTop`: default 0 | [ORACLE newgame-0001] [BYTES seg6:0x1ac] | oracle | located | todo | demo |
| INI-23 | `WatchLeft`: default 0 | [ORACLE newgame-0001] [BYTES seg6:0x1c2] | oracle | located | todo | demo |
| INI-24 | `WatchTop`: default 0 | [ORACLE newgame-0001] [BYTES seg6:0x1d8] | oracle | located | todo | demo |
| INI-25 | `WatchRight`: default `WatchLeft` + 280 | [ORACLE newgame-0001] [BYTES seg6:0x1e8] | oracle | located | todo | demo |
| INI-26 | `IconLeft`: default screen width − 48 (976 at 1024 pixels) | [ORACLE newgame-0001] [BYTES seg6:0x205] | oracle | located | todo | demo |
| INI-27 | `IconTop`: default 0 | [ORACLE newgame-0001] [BYTES seg6:0x245] | oracle | located | todo | demo |
| INI-28 | `SmLeft`: default 0 | [ORACLE newgame-0001] [BYTES seg6:0x25b] | oracle | located | todo | demo |
| INI-29 | `SmTop`: default 0 | [ORACLE newgame-0001] [BYTES seg6:0x28e] | oracle | located | todo | demo |
| INI-30 | `HelpLeft`: default 0 | [ORACLE newgame-0001] [BYTES seg6:0x2a4] | oracle | located | todo | demo |
| INI-31 | `HelpTop`: default 0 | [ORACLE newgame-0001] [BYTES seg6:0x2ba] | oracle | located | todo | demo |
| INI-32 | `Aux`: default 0 | [ORACLE newgame-0001] [BYTES seg6:0x2fb] | oracle | understood | todo | demo |
| INI-33 | `QuickIntro`: (Skip Sequences), default 0; 1 does not shorten the desk scene | [ORACLE newgame-0001] [BYTES seg6:0x318] | oracle | located | todo | demo |
| INI-34 | `Rate`: default 0 | [ORACLE newgame-0001] [BYTES seg6:0x32e] | oracle | located | todo | demo |
| INI-35 | `OnTop`: default 1 | [ORACLE newgame-0001] [BYTES seg2:0x101d] | oracle | located | todo | demo |
| INI-36 | `SoundFX`: (Sound FX), default 1 | [ORACLE newgame-0001] [BYTES ISPACE.DLL seg9:0x22e5] | oracle | located | todo | demo |
| INI-37 | `VoiceFX`: (Voice Effects), default 1 | [ORACLE newgame-0001] [BYTES ISPACE.DLL seg9:0x22fb] | oracle | located | todo | demo |
| INI-38 | `Music`: (Music Soundtrack), default 1 | [ORACLE newgame-0001] [BYTES ISPACE.DLL seg9:0x2311] | oracle | located | todo | demo |
| INI-39 | `SoundVolume`: (Master Volume), default 100 | [ORACLE newgame-0001] [BYTES ISPACE.DLL seg9:0x2327] | oracle | located | todo | demo |
| INI-40 | `FXMusic`: (Effects vs. Music), default 50 | [ORACLE newgame-0001] [BYTES ISPACE.DLL seg9:0x233d] | oracle | located | todo | demo |
| INI-41 | `StereoEffect`: (Stereo Effect), default 2 | [ORACLE newgame-0001] [BYTES ISPACE.DLL seg9:0x2353] | oracle | located | todo | demo |
| INI-42 | `PurgeSound`: (Keep Sounds in Memory), default 1 | [ORACLE newgame-0001] [BYTES ISPACE.DLL seg9:0x2369] | oracle | located | todo | demo |
| INI-43 | `StereoSwap`: (Swap left/right Channels), default 0 | [ORACLE newgame-0001] [BYTES ISPACE.DLL seg9:0x237f] | oracle | located | todo | demo |
| INI-44 | `JoyStick`: (Joystick Enabled), default 0 | [ORACLE newgame-0001] [BYTES seg6:0xc4a] | oracle | located | todo | demo |
| INI-45 | `4Btns`: (4 Buttons), default 0 | [ORACLE newgame-0001] [BYTES ISPACE.DLL seg9:0x23b1] | oracle | located | todo | demo |
| INI-46 | `JMinX`: joystick calibration, default 0 | [ORACLE newgame-0001] [BYTES ISPACE.DLL seg9:0x23c7] | oracle | located | todo | demo |
| INI-47 | `JMinY`: joystick calibration, default 0 | [ORACLE newgame-0001] [BYTES ISPACE.DLL seg9:0x23dd] | oracle | located | todo | demo |
| INI-48 | `JRangeX`: joystick calibration, default 0 | [ORACLE newgame-0001] [BYTES ISPACE.DLL seg9:0x23f3] | oracle | located | todo | demo |
| INI-49 | `JRangeY`: joystick calibration, default 0 | [ORACLE newgame-0001] [BYTES ISPACE.DLL seg9:0x2409] | oracle | located | todo | demo |
| INI-50 | `JBtn0`: (Button 1 function), default 0 | [ORACLE newgame-0001] [BYTES seg6:0xbeb] | oracle | located | todo | demo |
| INI-51 | `JBtn1`: (Button 2 function), default 1 | [ORACLE newgame-0001] [BYTES seg6:0xbeb] | oracle | located | todo | demo |
| INI-52 | `JBtn2`: (Button 3 function), default 2 | [ORACLE newgame-0001] [BYTES seg6:0xbeb] | oracle | located | todo | demo |
| INI-53 | `JBtn3`: (Button 4 function), default 3 | [ORACLE newgame-0001] [BYTES seg6:0xbeb] | oracle | located | todo | demo |
| INI-54 | `SoundSystem`: (sound system), default -1; regenerated as 0 | [ORACLE smoke-0001] [ORACLE newgame-0001] [BYTES ISPACE.DLL seg9:0x2538] | oracle | located | todo | demo |
| INI-55 | `Enhance`: (Enhanced Graphics), default 1 | [ORACLE newgame-0001] [BYTES seg6:0xc8c] | oracle | located | todo | demo |
| INI-56 | `Detail`: (Play Detail), default 3 | [ORACLE newgame-0001] [BYTES seg6:0xc76] [BYTES INSTALL.EXE seg2:0x2b53] | oracle | understood | done | demo |
| INI-57 | `Quick`: (Quickload), default 0 (INSPACE, Factory), 1 (ISPACE.DLL) | [ORACLE newgame-0001] [BYTES seg6:0xc60] | oracle | located | todo | demo |
| INI-58 | `Calibrate`: speed measure rewritten 1.5 s after each launch; guides the detail level | [MANUAL Optimizing for Your Computer] [ORACLE newgame-0001] [ORACLE smoke-0001] [BYTES seg6:0xa6c] | oracle | understood | done | demo |
| INI-59 | `GameSpeed`: default 20000 | [ORACLE newgame-0001] [BYTES seg6:0xb5c] | oracle | located | todo | demo |
| INI-60 | `FrameRateSet`: (Play Speed), default 36 | [ORACLE newgame-0001] [BYTES seg6:0xb7d] | oracle | understood | done | demo |
| INI-61 | `MixChannels`: (Mix Channels), default 1 (INSPACE), 4 (ISPACE.DLL) | [ORACLE newgame-0001] [BYTES seg6:0xc9c] | oracle | located | todo | demo |
| INI-62 | `BadVideo`: default 1 | [ORACLE newgame-0001] [BYTES seg6:0xcda] | oracle | located | todo | demo |
| INI-63 | `Complex`: default 0 (INSPACE), 2 (ISPACE.DLL) | [ORACLE newgame-0001] [BYTES seg6:0x1204] [BYTES INSTALL.EXE seg2:0x34a6] | oracle | understood | done | demo |
| INI-64 | `Braking`: (Ship Auto Braking), default 0 | [ORACLE newgame-0001] [BYTES seg6:0xcf4] [BYTES INSTALL.EXE seg2:0x34a6] | oracle | understood | done | demo |
| INI-65 | `ShipAnim`: (Animated Ship Watches), default 1 | [ORACLE newgame-0001] [BYTES seg6:0xd0a] | oracle | located | todo | demo |
| INI-66 | `Border`: (Game Border), default 1 | [ORACLE newgame-0001] [BYTES seg6:0xd20] [BYTES seg2:0xcb1] | oracle | understood | todo | demo |
| INI-67 | `Bright`: (Enhanced Brightness), default 0 | [ORACLE newgame-0001] [BYTES seg6:0xe88] | oracle | located | todo | demo |
| INI-68 | `Size`: default 1; New Game OK writes it | [ORACLE newgame-0001] [BYTES seg6:0xaf8] [BYTES seg10:0x9a5f] | oracle | understood | done | demo |
| INI-69 | `SetSize`: default 1; New Game OK writes it | [ORACLE newgame-0001] [BYTES ISPACE.DLL seg10:0x1ff7] | oracle | located | done | demo |
| INI-70 | `Accept`: (I Accept), default 0 | [ORACLE newgame-0001] [BYTES seg11:0x7a28] | oracle | located | done | demo |
| INI-71 | `Restore`: Restore Game choice; OK writes " NEW GAME" with a leading space | [ORACLE newgame-0001] [BYTES seg6:0x9396] | oracle | understood | done | demo |
| INI-72 | `Drive`: (DRIVE), default C | [ORACLE newgame-0001] [BYTES seg10:0x8525] | oracle | located | done | demo |
| INI-73 | `ThePlayer`: (PILOT), default "" (INSPACE), AutoPilot (ISPACE.DLL) | [ORACLE newgame-0001] [BYTES seg10:0x8c] | oracle | located | done | demo |
| INI-74 | `MainShip`: (SHIP), default EAGLE; regenerated as TRANSFORMER | [ORACLE smoke-0001] [ORACLE newgame-0001] [BYTES seg14:0x23bf] | oracle | located | done | demo |
| INI-75 | `PlayMode`: (PLAY), default 0 | [ORACLE newgame-0001] [BYTES seg10:0x8478] | oracle | located | done | demo |
| INI-76 | `Breakfast`: default 800 | [MANUAL Special Times of Day] [ORACLE newgame-0001] [BYTES seg10:0x96df] | oracle | located | todo | demo |
| INI-77 | `Coffee`: default 1000 | [MANUAL Special Times of Day] [ORACLE newgame-0001] [BYTES seg10:0x970b] | oracle | located | todo | demo |
| INI-78 | `Lunch`: default 1200 | [MANUAL Special Times of Day] [ORACLE newgame-0001] [BYTES seg10:0x96b2] | oracle | located | todo | demo |
| INI-79 | `Tea`: default 1530 | [MANUAL Special Times of Day] [ORACLE newgame-0001] [BYTES seg10:0x9737] | oracle | located | todo | demo |
| INI-80 | `NavLeft`: Navigation window, default 30 | [ORACLE newgame-0001] [BYTES seg6:0x1cb6] | oracle | located | done | demo |
| INI-81 | `NavTop`: default 30 | [ORACLE newgame-0001] [BYTES seg6:0x1ccc] | oracle | located | done | demo |
| INI-82 | `NavRight`: default 560 | [ORACLE newgame-0001] [BYTES seg6:0x1ce3] | oracle | located | done | demo |
| INI-83 | `NavBottom`: default 350 | [ORACLE newgame-0001] [BYTES seg6:0x1cfa] | oracle | located | done | demo |
| INI-84 | `Racers`: (RACE competitors), default 1 | [ORACLE newgame-0001] [BYTES seg6:0x2773] | oracle | located | todo | demo |
| INI-85 | `RaceDiff`: (RACE difficulty), default 0 | [ORACLE newgame-0001] [BYTES seg6:0x2930] | oracle | located | todo | demo |
| INI-86 | `RaceLaps`: (RACE laps), default 1 | [ORACLE newgame-0001] [BYTES seg6:0x2aee] | oracle | located | todo | demo |
| INI-87 | `RaceOpp`: (RACE speed), default 2 | [ORACLE newgame-0001] [BYTES seg6:0x2c3a] | oracle | located | todo | demo |
| INI-88 | `Duelers`: (DUEL opponents) | [BYTES DGROUP:0xdaa] [BYTES seg6:0x2851] | strings | located | todo | unknown |
| INI-89 | `DuelDiff`: (DUEL difficulty), default 0 | [ORACLE newgame-0001] [BYTES seg6:0x2a0f] | oracle | located | todo | demo |
| INI-90 | `DuelOpp`: (DUEL toughness), default 2 | [ORACLE newgame-0001] [BYTES seg6:0x2d7f] | oracle | located | todo | demo |
| INI-91 | `Tree`: default 0 | [ORACLE newgame-0001] [BYTES seg6:0x37ef] | oracle | located | todo | demo |
| INI-92 | `SavePrompt`: (Always Ask for Save Name) | [BYTES DGROUP:0x865] [BYTES ISPACE.DLL DGROUP:0x9dd] [BYTES seg10:0x85a5] | strings | located | done | unknown |
| INI-93 | `PlusCount` | [BYTES DGROUP:0x879] [BYTES seg10:0x880e] | strings | located | done | unknown |
| INI-94 | `StratAsk` | [BYTES DGROUP:0x915] [BYTES ISPACE.DLL DGROUP:0x3c0] [BYTES seg10:0xa988] | strings | located | done | unknown |
| INI-95 | `DemoType`: (Demo Options mode) | [BYTES DGROUP:0x121e] [BYTES ISPACE.DLL DGROUP:0x41c] [BYTES seg15:0x5aa6] | strings | located | todo | unknown |
| INI-96 | `Ship1`: (SHIP #1) | [BYTES DGROUP:0x11ae] [BYTES ISPACE.DLL DGROUP:0x445] [BYTES seg14:0x236c] | strings | located | todo | unknown |
| INI-97 | `Ship2`: (SHIP #2) | [BYTES DGROUP:0x11b5] [BYTES ISPACE.DLL DGROUP:0x44c] [BYTES seg14:0x239c] | strings | located | todo | unknown |
| INI-98 | `Track`: (Demo Options track) | [BYTES DGROUP:0xe99] [BYTES ISPACE.DLL DGROUP:0x426] [BYTES seg6:0x3c8a] | strings | located | todo | unknown |
| INI-99 | `DRacers`: (Demo Options racers) | [BYTES DGROUP:0xe34] [BYTES ISPACE.DLL DGROUP:0x434] [BYTES seg6:0x3208] | strings | located | todo | unknown |
| INI-100 | `DDiff`: (Demo Options difficulty) | [BYTES DGROUP:0xe3c] [BYTES ISPACE.DLL DGROUP:0x42c] [BYTES seg6:0x3262] | strings | located | todo | unknown |
| INI-101 | `DLaps`: (Demo Options laps) | [BYTES DGROUP:0xe42] [BYTES ISPACE.DLL DGROUP:0x43e] [BYTES seg6:0x327e] | strings | located | todo | unknown |
| INI-102 | `DScale`: (Demo Options Zoom:) | [BYTES DGROUP:0x9ab] [BYTES ISPACE.DLL DGROUP:0x3fb] [BYTES seg9:0x236b] | strings | located | todo | unknown |
| INI-103 | `AllowSound`: 1 forces audio on cards that misreport themselves | [MANUAL Troubleshooting] [BYTES ISPACE.DLL DGROUP:0x95b] [BYTES ISPACE.DLL seg9:0x2444] | manual | located | todo | unknown |
| INI-104 | `ForceVol` | [BYTES ISPACE.DLL DGROUP:0x828] [BYTES ISPACE.DLL seg9:0xbde] | code | understood | todo | unknown |
| INI-105 | `SaverVolume`: (screen saver Audio Volume) | [BYTES ISPACE.DLL DGROUP:0x788] [BYTES INSPACE.SCR seg2:0x54] [BYTES INRUN.AD seg2:0x10a] [BYTES ISPACE.DLL seg9:0x6e] | strings | located | todo | unknown |
| INI-106 | `Play`: (Allow Game Play) read by INSPACE.SCR; section [OPEN] | [BYTES INSPACE.SCR seg2:0x9a] | strings | none | todo | unknown |
| INI-107 | `WIN.INI [Inner Space] CDDrive`: CD drive letter | [ORACLE newgame-0001] [BYTES DGROUP:0x88e] [BYTES seg10:0x88db] | oracle | located | todo | demo |
| INI-108 | `SYSTEM.INI [386Enh] vidsp`: default 0; purpose [OPEN] | [ORACLE newgame-0001] [BYTES seg10:0x8a26] | oracle | located | todo | demo |
| INI-109 | `SYSTEM.INI [386Enh] 32BitSP`: default 0; purpose [OPEN] | [ORACLE newgame-0001] [BYTES seg10:0x8a3f] | oracle | located | todo | demo |
| INI-110 | `SYSTEM.INI [386Enh] XCal123`: purpose [OPEN] | [BYTES ISPACE.DLL DGROUP:0x5c6] [BYTES ISPACE.DLL seg10:0x5bf7] | strings | located | todo | unknown |
| INI-111 | `WIN.INI [windows] PDev`: default 0; purpose [OPEN] | [ORACLE newgame-0001] [BYTES seg10:0x8a52] | oracle | located | todo | demo |
| INI-112 | `SYSTEM.INI [Boot.description] display.drv`: video card name | [ORACLE newgame-0001] [BYTES ISPACE.DLL seg8:0x5cd5] | oracle | located | todo | demo |
| INI-113 | `SYSTEM.INI [drivers] Wave`: wave driver | [ORACLE newgame-0001] [BYTES ISPACE.DLL seg8:0x5d30] | oracle | located | todo | demo |
| INI-114 | `CONTROL.INI [drivers.desc]`: sound card description | [ORACLE newgame-0001] [BYTES ISPACE.DLL seg8:0x5d6b] | oracle | located | todo | demo |
| INI-115 | `PROTOCOL.INI [network.setup] netcard`: network card | [ORACLE newgame-0001] [BYTES ISPACE.DLL seg8:0x5e51] | oracle | located | todo | demo |
| INI-116 | `NETWORK.INF [netcard]`: network card | [ORACLE newgame-0001] [BYTES ISPACE.DLL seg8:0x5eb2] | oracle | located | todo | demo |
| INI-117 | `[Teams] Team1`: team name, AVENGERS in the shipped INI | [ORACLE newgame-0001] [BYTES INSPACE.INI] [BYTES seg13:0x21cf] | oracle | located | todo | demo |
| INI-118 | `[Teams] Team2`: PREDATORS | [BYTES INSPACE.INI] [BYTES seg13:0x21cf] | strings | located | todo | unknown |
| INI-119 | `[Teams] Team3`: FUZZY ONES | [BYTES INSPACE.INI] [BYTES seg13:0x21cf] | strings | located | todo | unknown |
| INI-120 | `[Teams] Team4`: PIRATES | [BYTES INSPACE.INI] [BYTES seg13:0x21cf] | strings | located | todo | unknown |
| INI-121 | `[Teams] Team5`: KNIGHTS | [BYTES INSPACE.INI] [BYTES seg13:0x21cf] | strings | located | todo | unknown |
| INI-122 | `[Teams] Team6`: SPEED DEMONS | [BYTES INSPACE.INI] [BYTES seg13:0x21cf] | strings | located | todo | unknown |
| INI-123 | `[Teams] Team7`: RENEGADES | [BYTES INSPACE.INI] [BYTES seg13:0x21cf] | strings | located | todo | unknown |
| INI-124 | `[Teams] Team8`: ENFORCERS | [BYTES INSPACE.INI] [BYTES seg13:0x21cf] | strings | located | todo | unknown |
| INI-125 | `[Ship Factory] Ship`: Factory's current ship, DRAGON in the shipped INI | [BYTES INSPACE.INI] [BYTES FACTORY.EXE DGROUP:0xc1] [BYTES FACTORY.EXE seg2:0x5a3] | strings | located | todo | registered |
| INI-126 | `BitmapEditor`: Enhanced Factory bitmap editor path, default PBRUSH | [MANUAL Editing Ship Bitmaps] [BYTES theworks/FACTORY.EXE DGROUP:0xcf] [BYTES theworks/FACTORY.EXE seg2:0x1255] | manual | located | todo | registered |
| INI-127 | `ShowLeft`: Ship Factory window, section [Ship Factory] | [BYTES FACTORY.EXE DGROUP:0x106] [BYTES FACTORY.EXE seg4:0x18] | strings | located | todo | registered |
| INI-128 | `ShowTop`: Ship Factory window, section [Ship Factory] | [BYTES FACTORY.EXE DGROUP:0x10f] [BYTES FACTORY.EXE seg4:0x2e] | strings | located | todo | registered |
| INI-129 | `BodyLeft`: Factory body window, section [Ship Factory] | [BYTES FACTORY.EXE DGROUP:0x117] [BYTES FACTORY.EXE seg4:0x86] | strings | located | todo | registered |
| INI-130 | `BodyTop`: section [Ship Factory] | [BYTES FACTORY.EXE DGROUP:0x120] [BYTES FACTORY.EXE seg4:0x9c] | strings | located | todo | registered |
| INI-131 | `ShowTeam`: Factory, section [Ship Factory] | [BYTES FACTORY.EXE DGROUP:0x128] [BYTES FACTORY.EXE seg4:0xf4] | strings | located | todo | registered |
| INI-132 | `BehavLeft`: Factory Behaviour window, section [Ship Factory] | [BYTES FACTORY.EXE DGROUP:0x131] [BYTES FACTORY.EXE seg4:0x10a] | strings | located | todo | registered |
| INI-133 | `BehavTop`: section [Ship Factory] | [BYTES FACTORY.EXE DGROUP:0x13b] [BYTES FACTORY.EXE seg4:0x120] | strings | located | todo | registered |
| INI-134 | `HandLeft`: Factory Handling window, section [Ship Factory] | [BYTES FACTORY.EXE DGROUP:0x144] [BYTES FACTORY.EXE seg4:0x178] | strings | located | todo | registered |
| INI-135 | `HandTop`: section [Ship Factory] | [BYTES FACTORY.EXE DGROUP:0x14d] [BYTES FACTORY.EXE seg4:0x18e] | strings | located | todo | registered |
| INI-136 | `FrameShow`: (Factory menu Frame Rate), section [Ship Factory] | [BYTES FACTORY.EXE DGROUP:0x155] [BYTES FACTORY.EXE seg4:0x1fc] | strings | located | todo | registered |
| INI-137 | `RotateBigShip`: (Factory menu Rotate Big Ships), section [Ship Factory] | [BYTES FACTORY.EXE DGROUP:0x15f] [BYTES FACTORY.EXE seg4:0x237] | strings | located | todo | registered |
| INI-138 | `CONTROL.INI PWProtected`: screen-saver password protection; section ScreenSaver.Inner Space or ScreenSaver [OPEN] | [BYTES INSPACE.SCR STRING 63] | strings | none | todo | unknown |
| INI-139 | `CONTROL.INI Password`: screen-saver password; section [OPEN] | [BYTES INSPACE.SCR STRING 63] | strings | none | todo | unknown |
| INI-140 | Unnamed reads in newgame-0001: an empty key after Racers, `Th`, and byte 0x1d [OPEN] | [ORACLE newgame-0001] | oracle | none | todo | demo |
| INI-141 | WAVEMIX.INI: WaveMix 1.70 settings (ShowDevices, WaveOutDevice, Remix, GoodWavePos, WaveBlocks, SamplesPerSec, per-device sections) | [BYTES WAVEMIX.INI] | strings | none | todo | unknown |
| INI-142 | INSTALL.INI: installer manifest ([Vers] V=12908, [Dirs], numbered file groups, [Temp]) | [BYTES INSTALL.INI] | strings | none | todo | unknown |
| INI-143 | The Restore Game dialog loads LocalPath\GAMES\<name>.sav and ignores the Games key | [BYTES ISPACE.DLL seg10:0x1b37] | code | understood | todo | unknown |

## SND: Sound and music

| ID | Feature | Evidence | Spec | RE | Port | Edition |
|---|---|---|---|---|---|---|
| SND-01 | Sound effects: 46 WAVs in SND_FX.DLL | [MANUAL Game Settings] [BYTES SND_FX.DLL 100] [BYTES ISPACE.DLL seg9:0xc42] | manual | located | todo | unknown |
| SND-02 | Voice effects: 32 WAVs in the language voice DLL (eng_fx.dll) | [MANUAL Game Settings] [BYTES eng_fx.dll 100] [BYTES ISPACE.DLL DGROUP:0x7ea] | manual | none | todo | unknown |
| SND-03 | WaveMix mixing over 1–8 channels; 1 channel plays without the wavemixer | [MANUAL Game Settings] [MANUAL Troubleshooting] [BYTES ISPACE.DLL seg9:0x362e] | manual | located | todo | unknown |
| SND-04 | Dynamic sound: volume by distance | [MANUAL Game Settings] [BYTES ISPACE.DLL seg9:0x76c] | manual | located | todo | unknown |
| SND-05 | 3D Dynamic sound: left/right placement | [MANUAL Game Settings] [BYTES ISPACE.DLL seg9:0x76c] | manual | located | todo | unknown |
| SND-06 | MIDI music main, race, anthem, demon (.mid) through the MCI sequencer | [MANUAL Game Settings] [BYTES ISPACE.DLL DGROUP:0xae4] [BYTES ISPACE.DLL DGROUP:0xaef] [BYTES ISPACE.DLL seg9:0x3906] | manual | located | todo | unknown |
| SND-07 | "Midi Mapper is not available. Continue?" | [BYTES ENG_LANG.DLL STRING 139] id 2219 | strings | none | todo | unknown |
| SND-08 | No sound card: silent; PC-speaker drivers refused | [MANUAL Game Settings] [MANUAL Troubleshooting] | manual | none | todo | unknown |
| SND-09 | Compressed and uncompressed sound sets | [MANUAL Troubleshooting] [BYTES ENG_LANG.DLL STRING 145] id 2314 | manual | none | todo | unknown |
| SND-10 | Ship hit sounds CLUNK .. SHATTER; custom WAVs from the SOUNDS directory | [MANUAL Customizing Ships] [BYTES ENG_LANG.DLL STRING 39] ids 610–623 [BYTES ENG_LANG.DLL STRING 40] id 624 | manual | none | todo | registered |
| SND-11 | Focus change: while sound is on, when another task's window becomes active the effects stop, music pauses and the system MIDI and wave volumes are restored; back in the game's task (not iconic) the game volumes and music return (ISPACE ordinal 7 commands 3 and 4) | [BYTES ISPACE.DLL seg9:0xaf2] [BYTES seg2:0x12e7] | code | understood | todo | unknown |
| SND-12 | Effects volume (ISPACE.DLL DGROUP:0x6a0) = SoundVolume/SaverVolume (ISPACE.DLL DGROUP:0x69e) while FXMusic (ISPACE.DLL DGROUP:0x6a2) is at most 50, and that × 2 × (100 − FXMusic)/100 above 50; while it is 0, ordinal 9 plays no effect except ids 0x68, 0x6f, 0x70 and 0x75 | [BYTES ISPACE.DLL seg9:0xa9e] | code | understood | todo | unknown |
| SND-13 | Effect hold: most effects play only once the busy-until time set by the previous effect has passed; ordinal 9 adds a per-id hold when its q argument is non-zero (900 ms for ids 0x26, 0x2d, 0x2f); ids 0x68, 0x6f, 0x70, 0x75 skip the check; ids 0x69 and 0x6e clear it; some ids pick one of three variants with ISPACE.DLL's rand() | [BYTES ISPACE.DLL seg9:0xc42] | code | understood | todo | unknown |

## FAC: Ship Factory

FACTORY.EXE 1.29.02 ships with the full game. The THEWORKS FACTORY.EXE 1.49.00 is the Enhanced Ship Factory of the Ship Builder's Kit. The Factory's dialogs and menu live in ENG_LANG.DLL.

| ID | Feature | Evidence | Spec | RE | Port | Edition |
|---|---|---|---|---|---|---|
| FAC-01 | Ship Factory program, full version only | [MANUAL Ship Factory] [MANUAL Why Register Inner Space?] | manual | none | todo | registered |
| FAC-02 | Enhanced Ship Factory (Ship Builder's Kit add-on) | [MANUAL Ship Factory] [MANUAL Sharing Custom Ships With Others] | manual | none | todo | registered |
| FAC-03 | Figure-eight flag course showing each ship's handling | [MANUAL Ship Factory] | manual | none | todo | registered |
| FAC-04 | Ship list; built-in ships cannot be altered | [MANUAL Customizing Ships] | manual | none | todo | registered |
| FAC-05 | New: copy the current ship under a variant name | [MANUAL Customizing Ships] [BYTES ENG_LANG.DLL MENU 110] | manual | none | todo | registered |
| FAC-06 | Delete ship, with confirmation | [BYTES ENG_LANG.DLL MENU 110] [BYTES ENG_LANG.DLL STRING 12] id 185 | strings | none | todo | registered |
| FAC-07 | Name and Creator fields | [BYTES ENG_LANG.DLL STRING 144] id 2291 [BYTES ENG_LANG.DLL STRING 144] id 2292 | strings | none | todo | registered |
| FAC-08 | Paint and Trim colours (CHROME .. BROWN), Thrust RED or BLUE (shots follow it); colour morph at 256 colours | [MANUAL Customizing Ships] [BYTES ENG_LANG.DLL STRING 17] ids 270–271 [BYTES ENG_LANG.DLL STRING 18] ids 272–287 [BYTES ENG_LANG.DLL STRING 144] ids 2295–2300 | manual | none | todo | registered |
| FAC-09 | Sound list with custom WAVs | [MANUAL Customizing Ships] [BYTES FACTORY.EXE seg2:0xcae] | manual | located | todo | registered |
| FAC-10 | Ship Handling dialog (#008b): Armor, Thruster, Fuel Tank; Captures Icons, Repair System, Cloak; text overview | [MANUAL Customizing Ships] [BYTES ENG_LANG.DLL DIALOG 139] [BYTES FACTORY.EXE seg5:0x39ea] | manual | located | todo | registered |
| FAC-11 | Ship Behaviour dialog (#00a9): Team, Initial Weapon, To Friends, To Enemies | [MANUAL Customizing Ships] [BYTES ENG_LANG.DLL DIALOG 169] [BYTES FACTORY.EXE seg5:0x2808] | manual | located | todo | registered |
| FAC-12 | Weapons rated by ambulance cost in the Behaviour dialog | [MANUAL Customizing Ships] | manual | none | todo | registered |
| FAC-13 | Ship Body Setup: &BODY:, &THRUST POS:, &FUSELAGE, X and Y scrollbars | [BYTES ENG_LANG.DLL STRING 143] ids 2283–2287 | strings | none | todo | registered |
| FAC-14 | Import Ship Body dialog (#012c): Name, Body Thrust, Pull-off Wings, Shield, Thrust Behind Ship, *.DIB list | [MANUAL Editing Ship Bitmaps] [BYTES ENG_LANG.DLL DIALOG 300] [BYTES FACTORY.EXE seg5:0x1f8e] | manual | located | todo | registered |
| FAC-15 | Enhanced Import Ship Body dialog (#03c3): adds Multi-frames Looping or Flapping | [MANUAL Editing Ship Bitmaps] [BYTES theworks/FACTORY.EXE DIALOG 963] | manual | none | todo | registered |
| FAC-16 | Bitmap checks: 16 colours, no RLE, square frames, 17–45 pixels high, at most 4 frames | [MANUAL Editing Ship Bitmaps] [BYTES ENG_LANG.DLL STRING 152] ids 2427–2431 [BYTES ENG_LANG.DLL STRING 153] id 2432 | manual | none | todo | registered |
| FAC-17 | Multi-frame ships: transforming (shield) and body-thrusting (Looping 1,2,3,4; Flapping 1,2,3,4,3,2) | [MANUAL Editing Ship Bitmaps] | manual | none | todo | registered |
| FAC-18 | Custom palette (10 body + 5 trim shades + black) allows recolouring | [MANUAL Editing Ship Bitmaps] | manual | none | todo | registered |
| FAC-19 | 256-colour rendering: custom palette, 30 grey shades, smooth 1/2x and 2x rescale (1998 file) | [MANUAL Ship Factory] | manual | none | todo | unknown |
| FAC-20 | Export to .SDI (Enhanced) | [MANUAL Sharing Custom Ships With Others] [BYTES theworks/FACTORY.EXE STRING 160] | manual | none | todo | registered |
| FAC-21 | Import from .DIB with optional .SDI (Enhanced) | [MANUAL Sharing Custom Ships With Others] [BYTES theworks/FACTORY.EXE STRING 160] | manual | none | todo | registered |
| FAC-22 | Edit.. opens the ship bitmap in the BitmapEditor program (Enhanced) | [MANUAL Editing Ship Bitmaps] [BYTES theworks/FACTORY.EXE DGROUP:0xcf] [BYTES theworks/FACTORY.EXE seg2:0x1255] | manual | located | todo | registered |
| FAC-23 | Factory menu: Options.., Frame Rate, Rotate Big Ships, Exit | [BYTES ENG_LANG.DLL MENU 110] | strings | none | todo | registered |
| FAC-24 | Ship menu: New, Import.., Export.., Delete, Handling.., Behaviour..; Body menu: Body.., Edit.. | [BYTES ENG_LANG.DLL MENU 110] | strings | none | todo | registered |
| FAC-25 | Help menu: General Help, Using Ship Factory, About; Enhanced lists the four Factory topics | [BYTES ENG_LANG.DLL MENU 110] [BYTES theworks/FACTORY.EXE MENU 110] | strings | none | todo | registered |
| FAC-26 | Custom ships fly in the game as player and computer ships | [MANUAL Customizing Ships] [MANUAL Ship Factory] [BYTES seg14:0x1b84] | manual | located | todo | registered |
| FAC-27 | Missing-body and duplicate-body handling | [BYTES ENG_LANG.DLL STRING 38] ids 600–602 [BYTES ENG_LANG.DLL STRING 12] id 181 [BYTES ENG_LANG.DLL STRING 12] id 182 | strings | none | todo | registered |
| FAC-28 | Gender option (&Male, &Female) | [BYTES FACTORY.EXE DGROUP:0x9c] [BYTES FACTORY.EXE seg2:0x4c] | code | understood | todo | registered |
| FAC-29 | Factory single instance, memory and timer errors | [BYTES ENG_LANG.DLL STRING 144] id 2288 [BYTES ENG_LANG.DLL STRING 144] id 2289 | strings | none | todo | registered |

## SCR: Screen savers

| ID | Feature | Evidence | Spec | RE | Port | Edition |
|---|---|---|---|---|---|---|
| SCR-01 | Windows screen saver INSPACE.SCR with Setup dialog (#07d3) | [MANUAL Screen Savers] [BYTES INSPACE.SCR DIALOG 2003] | manual | none | todo | unknown |
| SCR-02 | After Dark module INRUN.AD ("Inner Space (tm) Starter") | [MANUAL Screen Savers] [BYTES INRUN.AD 2000 10] | manual | none | todo | unknown |
| SCR-03 | Allow Game Play: play when it comes up, else a demo that wakes on input | [MANUAL Screen Savers] | manual | none | todo | unknown |
| SCR-04 | Audio Volume scrollbar | [BYTES INSPACE.SCR DIALOG 2003] | strings | none | todo | unknown |
| SCR-05 | Demo Options dialog (#01b0): Random Demo, Game Demo, Race, 2 Ship Battle, SHIP #1, SHIP #2, Racing group, Zoom | [MANUAL Screen Savers] [BYTES ENG_LANG.DLL DIALOG 432] [BYTES ISPACE.DLL seg10:0x3346] | manual | located | todo | unknown |
| SCR-06 | Continuing demo with Ship #1; a new Ship #1 starts a new demo game | [MANUAL Screen Savers] | manual | none | todo | unknown |
| SCR-07 | Race demo with 1–4 ships; '.Random' Ship #2 and Ships #3–4 comparable to Ship #1 | [MANUAL Screen Savers] [BYTES ISPACE.DLL DGROUP:0x402] | manual | none | todo | unknown |
| SCR-08 | Battle demo: Ship #1 against Ship #2 in Dodge City | [MANUAL Screen Savers] | manual | none | todo | unknown |
| SCR-09 | Wake-up delay while loading graphics or on the Navigation window | [MANUAL Screen Savers] | manual | none | todo | unknown |
| SCR-10 | With game play allowed, the game must be exited normally | [MANUAL Screen Savers] | manual | none | todo | unknown |
| SCR-11 | Screen-saver password dialogs (Change Password, password prompt, incorrect password) | [BYTES INSPACE.SCR DIALOG 2000] | strings | none | todo | unknown |
| SCR-12 | INRUN.AD option records: Allow Game Play, Condition:, Configure.., Sound: | [BYTES INRUN.AD 1000] | strings | none | todo | unknown |
| SCR-13 | Refuses to start when the game is already running | [BYTES INRUN.AD seg2:0xc2] | strings | none | todo | unknown |
| SCR-14 | INSPACE.EXE, PLAY and DEMO strings in both saver modules, DEMO and PLAY in INSPACE.EXE; how the saver starts the game is [OPEN] | [BYTES INSPACE.SCR seg2:0x90] [BYTES INRUN.AD seg2:0xb6] [BYTES DGROUP:0x57b] [BYTES seg6:0x1c0c] | strings | located | todo | unknown |
| SCR-15 | Started with command line DEMO or PLAY, the game sets DGROUP:0x90f8 and posts private message 0x9f0 or 0x9fa; while DGROUP:0x8e5e is also set, input more than 4 s after start destroys the main window and earlier input is dropped | [BYTES seg6:0x1c0c] [BYTES seg2:0x70e] | code | understood | todo | unknown |

## MODE: Modes, demo, sessions and times of day

| ID | Feature | Evidence | Spec | RE | Port | Edition |
|---|---|---|---|---|---|---|
| MODE-01 | Action mode (default): the player flies | [MANUAL Dealing With Other Ships] [BYTES ENG_LANG.DLL STRING 74] id 1170 | manual | none | todo | unknown |
| MODE-02 | Strategy (Command) mode: the ship carries out commands; PLAY ACTION/COMMAND in New Game | [MANUAL Dealing With Other Ships] [ORACLE newgame-0001] [BYTES ENG_LANG.DLL STRING 74] id 1171 [BYTES ENG_LANG.DLL STRING 147] id 2345 [BYTES seg9:0x6fb9] | oracle | understood | todo | demo |
| MODE-03 | Command-mode explanation and confirmation prompt | [BYTES ENG_LANG.DLL STRING 179] ids 2850–2853 | strings | none | todo | unknown |
| MODE-04 | Demo: after the intro the computer pilot plays and narrates for a few minutes, then the intro returns | [MANUAL QuickStart] [BYTES seg7:0x62b] | manual | understood | todo | unknown |
| MODE-05 | Demo pilot continues DEMOGAME each run | [MANUAL QuickStart] [MANUAL High Scores] [BYTES seg6:0x930a] [BYTES seg15:0x5a8e] | manual | understood | todo | unknown |
| MODE-06 | Special time Breakfast (8:00–8:30): juice and croissants replace fuel tanks | [MANUAL Special Times of Day] [BYTES ENG_LANG.DLL 5000 183] [BYTES ENG_LANG.DLL STRING 16] id 241 [BYTES ENG_LANG.DLL STRING 16] id 242 | manual | none | todo | unknown |
| MODE-07 | Special time Coffee: coffee replaces fuel tanks | [MANUAL Special Times of Day] [BYTES ENG_LANG.DLL 5000 181] [BYTES ENG_LANG.DLL STRING 15] id 235 | manual | none | todo | unknown |
| MODE-08 | Special time Lunch: sandwiches and french fries; "PREPARE TO LUNCH" | [MANUAL Special Times of Day] [BYTES ENG_LANG.DLL 5000 180] [BYTES ENG_LANG.DLL STRING 15] id 237 [BYTES ENG_LANG.DLL STRING 190] id 3030 | manual | none | todo | unknown |
| MODE-09 | Special time Tea: tea replaces fuel tanks | [MANUAL Special Times of Day] [BYTES ENG_LANG.DLL 5000 182] [BYTES ENG_LANG.DLL STRING 15] id 236 | manual | none | todo | unknown |
| MODE-10 | Special times configurable in INSPACE.INI | [MANUAL Special Times of Day] [ORACLE newgame-0001] [BYTES seg10:0x96b2] | oracle | located | todo | demo |
| MODE-11 | Weekday and time-of-day greeting from the host clock | [ORACLE newgame-0001] [BYTES ENG_LANG.DLL STRING 37] ids 590–591 [BYTES ENG_LANG.DLL STRING 38] ids 592–598 [BYTES ENG_LANG.DLL STRING 40] id 639 [BYTES ENG_LANG.DLL STRING 41] ids 640–642 [BYTES seg15:0x5338] | oracle | understood | todo | demo |
| MODE-12 | Difficulty effects: Gentle less violent with fewer objects | [MANUAL Game Settings] | manual | none | todo | unknown |
| MODE-13 | New game flow: Restore Game, NEW GAME, New Game dialog, launch, Navigation | [MANUAL QuickStart] [ORACLE newgame-0001] [BYTES ISPACE.DLL seg10:0x42c6] [BYTES seg6:0x368c] | oracle | understood | todo | demo |
| MODE-14 | Save game, named | [MANUAL High Scores] [BYTES ENG_LANG.DLL STRING 146] id 2324 [BYTES seg6:0x885c] | manual | located | todo | registered |
| MODE-15 | Restore game | [MANUAL High Scores] [BYTES ENG_LANG.DLL STRING 146] id 2325 [ORACLE newgame-0001] [BYTES seg6:0x930a] [BYTES seg11:0x7a82] | manual | understood | todo | registered |
| MODE-16 | LASTGAME saved at the end of every wave | [MANUAL High Scores] [BYTES seg6:0x885c] | manual | located | todo | unknown |
| MODE-17 | Save files: .sav in LocalPath + Games, .ded and .don in LocalPath + Dead, race records .rac in RacePath | [ORACLE newgame-0001] [BYTES DGROUP:0xcaf] [BYTES DGROUP:0xfde] [BYTES seg6:0x885c] | oracle | understood | todo | demo |
| MODE-18 | Abort to the intro with a save offer | [MANUAL High Scores] | manual | none | todo | unknown |
| MODE-19 | Scoring: wave score, team and individual scores, essence captured | [MANUAL High Scores] [BYTES ENG_LANG.DLL STRING 128] ids 2032–2039 [BYTES seg8:0x7be8] | manual | located | todo | unknown |
| MODE-20 | High-score ranking with game descriptions | [MANUAL High Scores] [BYTES ENG_LANG.DLL STRING 131] ids 2093–2095 [BYTES ENG_LANG.DLL STRING 132] ids 2096–2109 [BYTES seg6:0x6576] | manual | located | todo | unknown |
| MODE-21 | Demo death message with an order offer | [BYTES ENG_LANG.DLL STRING 146] id 2328 [BYTES ENG_LANG.DLL STRING 146] id 2329 [BYTES seg6:0x5dbd] | code | understood | todo | demo |
| MODE-22 | Boss key (Alt+B): restores the window frame style, then minimizes the window | [BYTES ENG_LANG.DLL STRING 83] id 1312 [BYTES seg2:0x1486] [BYTES seg2:0xd85] | code | understood | todo | unknown |
| MODE-23 | Rating (not in the manual): raised when the player's ship is credited with kills and hits (P and W tables, ship kill_value; a kill's P term caps the rating at 500 000), by the race bonus and fixed awards; lowered by damage to the player's ship; halved (× 4/5 when DGROUP:0x95bc ≠ 0) when that ship is destroyed; 12 thresholds 26 000 … 700 000 give the rating level DGROUP:0x9334 that the wave generator reads | [BYTES seg8:0x7be8] [BYTES seg9:0x484c] | code | understood | todo | unknown |
| MODE-24 | Kill points: when the player's ship is credited with destroying an object, the score rises by the object's pool entry of the DGROUP:0xb0e table (pools 0–13: 1000, 600, 1200, 900, 200, 0, 1000, 0, 3400, 500, 500, 0, 0, 5000; 0 for pools 14–20) | [BYTES seg8:0x7c3b] | code | understood | todo | unknown |
| MODE-25 | .ded (DEAD) and .don (DONE!) files are written only with a score of at least 1000; the .don save at seg16:0x7235 follows a 50 000-point award | [BYTES seg6:0x887a] [BYTES seg16:0x7228] [BYTES seg6:0x5dbd] | code | understood | todo | unknown |
| MODE-26 | Save-name clash: a .ded/.don name whose existing file belongs to another game (header game id) becomes its first 5 characters plus a counter 1, 2, … until free | [BYTES seg6:0x8776] | code | understood | todo | unknown |

## SYS: Install, calibration, protection and help

| ID | Feature | Evidence | Spec | RE | Port | Edition |
|---|---|---|---|---|---|---|
| SYS-01 | Installer: components Inner Space, Ship Factory, Sound Effects, Voice Effects, Music Soundtrack, Screen Savers; directory; disk space | [MANUAL Installing] [BYTES INSTALL.EXE DIALOG 300] [BYTES INSTALL.EXE DIALOG 300] [BYTES INSTALL.EXE seg2:0x73c] | manual | verified | done | unknown |
| SYS-02 | Installer: personalise name and organisation | [BYTES INSTALL.EXE DIALOG 135] [ORACLE install-0001] [BYTES INSTALL.EXE DIALOG 135] [BYTES INSTALL.EXE seg2:0x2942] | oracle | verified | done | unknown |
| SYS-03 | Installer: initial difficulty and speed (Slow 20, Normal 36, Fast 54, Red Hot! 72 frames/s) | [BYTES INSTALL.EXE DIALOG 1922] [ORACLE install-0001] [BYTES INSTALL.EXE DIALOG 1922] [BYTES INSTALL.EXE seg2:0x34a6] | oracle | verified | done | unknown |
| SYS-04 | Installer: full screen or resizable window | [BYTES INSTALL.EXE DIALOG 1774] [ORACLE install-0001] [BYTES INSTALL.EXE DIALOG 1774] [BYTES INSTALL.EXE seg2:0x33fa] | oracle | verified | done | unknown |
| SYS-05 | DeInstall and ReInstall | [MANUAL Installing] [BYTES INSTALL.EXE DIALOG 175] | manual | none | todo | unknown |
| SYS-06 | Master-disk copy protection: install from the master, one computer, one backup | [MANUAL Installing] | manual | none | todo | unknown |
| SYS-07 | INSTALL RELEASE: Security # and Release Code reset the master disk | [BYTES INSTALL.EXE DIALOG 891] | strings | none | todo | unknown |
| SYS-08 | Add-on installer (THEWORKS SETUP.EXE): upgrades keep registration | [BYTES theworks/SETUP.EXE DIALOG 300] | strings | none | todo | unknown |
| SYS-09 | Empty [Inner Space] `Path`: message, then INSPACE.INI regenerated | [ORACLE smoke-0001] [BYTES seg6:0x6d8] [BYTES seg6:0x1323] | oracle | understood | done | demo |
| SYS-10 | Calibrate measured at launch and stored | [MANUAL Optimizing for Your Computer] [ORACLE newgame-0001] [BYTES seg15:0x5f68] | oracle | understood | done | demo |
| SYS-11 | Install-time speed test picks the detail level | [MANUAL Detail Settings] [ORACLE install-0001] [BYTES INSTALL.EXE seg2:0x2b53] | oracle | verified | done | unknown |
| SYS-12 | Registration: the installer's personalization step (Name, Organization) registers the copy; ISPACE.DLL rewrites INSPACE.EXE #5500 110 and #5000 143 and writes SYSTEM.INI vidsp and 32BitSP and WIN.INI PDev; the title then reads Plus Edition with "Reg User" and no trial line | [MANUAL Ordering Instructions] [BYTES ENG_LANG.DLL DIALOG 693] [BYTES ENG_LANG.DLL STRING 142] id 2270 [BYTES ENG_LANG.DLL STRING 142] id 2271 [BYTES seg6:0x13e6] [ORACLE install-0001] [BYTES ISPACE.DLL seg10:0x4c68] [ORACLE start-0001] [BYTES INSTALL.EXE seg2:0x43da] | oracle | verified | done | registered |
| SYS-13 | Demo trial-day counter on the title screen | [ORACLE smoke-0001] [BYTES ENG_LANG.DLL STRING 147] id 2341 [BYTES ENG_LANG.DLL STRING 147] id 2342 [BYTES seg15:0x6f9d] | oracle | verified | done | demo |
| SYS-14 | Hard-drive validity check ("The game isn't valid on this hard drive.") | [BYTES ENG_LANG.DLL STRING 147] id 2343 [BYTES ENG_LANG.DLL STRING 147] id 2344 [BYTES seg10:0x885e] | strings | understood | done | unknown |
| SYS-15 | CD checks ("Drive %s is not a CD drive", "The CD in drive %s is not Inner Space") | [BYTES ENG_LANG.DLL STRING 159] id 2543 [BYTES ENG_LANG.DLL STRING 160] id 2544 | strings | none | todo | unknown |
| SYS-16 | Registered/Unregistered copy marks; INSPACE.EXE #5000 143 "Unregistered Copy ILLEGAL" | [BYTES ENG_LANG.DLL STRING 133] ids 2122–2124 [BYTES INSPACE.EXE 5000 143] [BYTES ISPACE.DLL seg10:0x4c68] [BYTES seg6:0x13e6] [ORACLE install-0001] [ORACLE start-0001] | oracle | understood | done | unknown |
| SYS-17 | Single instance ("Another version of Inner Space is already running!") | [BYTES ENG_LANG.DLL STRING 133] id 2114 [BYTES DGROUP:0x4a0] [BYTES seg6:0x12c4] | code | understood | todo | unknown |
| SYS-18 | Startup failures: insufficient memory, no free timer | [BYTES ENG_LANG.DLL STRING 133] id 2115 [BYTES ENG_LANG.DLL STRING 133] id 2116 [BYTES seg6:0x9f8] [BYTES DGROUP:0x4a0] | code | understood | todo | unknown |
| SYS-19 | Language DLL, voice DLL and help file chosen by Language; missing-file errors | [ORACLE newgame-0001] [BYTES DGROUP:0x779] [BYTES DGROUP:0x783] [BYTES seg10:0x6656] [BYTES seg6:0x13d0] | oracle | understood | todo | demo |
| SYS-20 | Static help: WinHelp file named from Language plus `_HELP.HLP`, with a context map | [MANUAL Contents] [BYTES DGROUP:0x113] [BYTES seg2:0x156b] | manual | understood | done | unknown |
| SYS-21 | Help menu: About Inner Space, User Manual, Playing Inner Space, General, Scores, This Game, Target, Weapons | [BYTES ENG_LANG.DLL MENU 101] [BYTES seg2:0x1559] [BYTES seg6:0x17f5] | code | understood | wip | unknown |
| SYS-22 | ORDER.TXT and README.TXT opened in Notepad | [BYTES ISPACE.DLL DGROUP:0x1d4] [BYTES ISPACE.DLL DGROUP:0x1dd] [BYTES ISPACE.DLL DGROUP:0x1f0] [BYTES INSTALL.EXE seg2:0x1346] | strings | verified | done | unknown |
| SYS-23 | CTL3D 3D dialogs | [BYTES DGROUP:0x26] [BYTES seg2:0x83] | code | understood | todo | unknown |
| SYS-24 | 8-bit palette: system colours read, entries 10–245 animated | [ORACLE depth8-0001] [BYTES ISPACE.DLL seg5:0x3596] [BYTES ISPACE.DLL seg5:0x34eb] | oracle | understood | todo | demo |
| SYS-25 | Display depth: 256 colours recommended; ships dithered at 16 colours | [MANUAL Ship Factory] [ORACLE depth8-0001] [BYTES seg6:0x1716] | oracle | understood | todo | demo |
| SYS-26 | Timers: one 1000 ms window timer; idle-loop frame pacing with timeGetTime and GetCurrentTime | [ORACLE newgame-0001] [BYTES seg7:0x0] [BYTES seg2:0x390] | oracle | understood | todo | demo |
| SYS-27 | Yields to background tasks | [MANUAL Troubleshooting] [MANUAL Optimizing for Your Computer] | manual | none | todo | unknown |
| SYS-28 | "Setup/Options Dialog is already in use by another app." | [BYTES ENG_LANG.DLL STRING 139] id 2220 | strings | none | todo | unknown |
| SYS-29 | Random numbers: INSPACE's one Microsoft C rand() (state × 214013 + 2531011, bits 16–30), seeded from the low word of GetCurrentTime on each screen-state change, when the wave HUD is built and in seg11:0xa4a6 | [BYTES seg5:0xdc] [BYTES seg2:0x1b6b] [ORACLE probe-0001] | oracle | understood | todo | demo |
| SYS-30 | Adaptive frame threshold: every 250 ms (frames off target by more than 1) and every 1000 ms (frames strictly within 9 of target) the pass threshold is multiplied by frames/target, toward FrameRateSet in the wave and toward fixed targets in states 0, 2, 3, 6, 8, 9 and the help-window pass (72 frames/s; 32, or a record field, in parts of state 6); state 5 never adapts; 16-bit products wrap | [BYTES seg7:0x9ed] [BYTES seg7:0x2206] | code | understood | todo | unknown |
| SYS-31 | Cursor nudge: while DGROUP:0x95bc is clear, from 10 s after start and at most every 20 s, the timer handler sets the cursor to x−1, x, x+1 of the position saved at start-up, in a 3-step cycle; purpose [OPEN] | [BYTES seg2:0x1244] | code | understood | todo | unknown |
| SYS-32 | Debug traces (GMAIN, WndProc, GameFrame 1/2/3 and Frame, each with iMainSmart) reach OutputDebugString only while INI Aux is set; the seg2 traces print only in game state 1 | [BYTES seg13:0x4eca] | code | understood | todo | unknown |
| SYS-33 | When <Windows directory>\INSPACE.INI is missing and GRMSETUP.EXE is in the program directory, seg6:0x6d8 runs it with WinExec and returns 0 | [BYTES seg6:0x791] | code | understood | todo | unknown |
| SYS-34 | Saved games and MASTER.BOD/MASTER.SHP are written as GlobalSize-rounded blocks (32 bytes under Wine), and the save reader fails without the padding | [BYTES seg6:0x8aec] [BYTES FACTORY.EXE seg4:0x26d4] | code | understood | todo | unknown |
| SYS-35 | Message formatter directive R picks one of n consecutive strings with rand() % n + base | [BYTES seg10:0xa332] | code | understood | todo | unknown |
