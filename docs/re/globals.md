# Globals and main loop

This page covers the DGROUP globals that drive the game, the code that reads them (message loop, window procedure, game tick), and scoring. Every address is in INSPACE.EXE, and DGROUP is seg19. The object pools and ship records are in [structs.md](structs.md). [DESIGN]

## Main loop

### Debug traces
- The original carries debug traces that name its own routines. Each prints a label, the decimal value of DGROUP:0x9646 and `\n\r`. The WndProc trace also prints `: Message is ` (DGROUP:0xc5) and the message number before `\n\r`. [BYTES seg2:0x115 seg2:0x125 seg2:0x13f seg2:0x263 seg2:0x273 seg2:0x28c]
- The traces in seg2 run only while DGROUP:0x90f4 = 1. The `Frame` trace has no test of its own: it sits in the tick case that serves states 1 and 7 and every value outside 0–9. [BYTES seg2:0x10d seg2:0x231 seg2:0x1c15 seg2:0x1c97 seg2:0x1ce7 seg7:0x48f seg7:0x492 seg7:0x4b0]
- The trace routine seg13:0x4eca calls OutputDebugString only when DGROUP:0x131a ≠ 0. seg6:0x2fb reads that flag with GetPrivateProfileInt, key `Aux` (DGROUP:0x1ea), section DGROUP:0x9690, file DGROUP:0x96a9. [BYTES seg13:0x4ed4 seg13:0x4edf seg6:0x2f2 seg6:0x2fb seg6:0x300]

  | Label string | Printed at | Routine |
  |---|---|---|
  | `GMAIN iMainSmart = ` (DGROUP:0x98) | seg2:0x115 | message loop of seg2:0x0 |
  | `WndProc iMainSmart = ` (DGROUP:0xaf) | seg2:0x239 | seg2:0x224 |
  | `GameFrame 2` / `3` / `1 iMainSmart = ` (DGROUP:0x13e0, DGROUP:0x13fd, DGROUP:0x141a) | seg2:0x1c1d, seg2:0x1c9f, seg2:0x1cef | seg2:0x1c08 |
  | `Frame iMainSmart = ` (DGROUP:0x5e5) | seg7:0x4b0 | gameplay case of seg7:0x0 |

  [BYTES DGROUP:0x98 DGROUP:0xaf DGROUP:0x13e0 DGROUP:0x13fd DGROUP:0x141a DGROUP:0x5e5]
- This page uses those names: `iMainSmart` for DGROUP:0x9646, and WinMain, WndProc and GameFrame for the routines. [DESIGN]

### WinMain (seg2:0x0)
- The Borland start-up code calls it, and it returns with `retf 0xa`. [BYTES seg5:0x2090 seg2:0x220]
- It copies `Inner Space` (DGROUP:0x82) to DGROUP:0x9690 and the command line to DGROUP:0x1d60. [BYTES seg2:0x17 seg2:0x24]
- It moves the cursor to (screen width / 2, screen height) and stores the resulting position in DGROUP:0x1d48. [BYTES seg2:0x39 seg2:0x41 seg2:0x46 seg2:0x4c seg2:0x55]
- It calls the start-up routine seg6:0x12b6, which registers the window class with WndProc, and quits when that returns 0. [BYTES seg2:0x6d seg2:0x74 seg6:0x1474 seg6:0x14c7]
- It stores the start time in DGROUP:0x1d3c and loads the accelerator table `MainAccel` (DGROUP:0x8e) from the module in DGROUP:0x9212. [BYTES seg2:0xaa seg2:0xb1 seg2:0xb9]
- Its message loop polls with PeekMessage while the game runs and calls GameFrame (seg2:0x1c08) when the queue is empty. The polling condition, the idle step and frame pacing are in [timing.md](timing.md#message-loop-and-idle-step). [BYTES seg2:0xfa seg2:0x171]
- A message goes through TranslateAccelerator (only while the main window is active), IsDialogMessage for DGROUP:0x9460, IsDialogMessage for DGROUP:0xd1e, then TranslateMessage and DispatchMessage. WM_QUIT ends the loop. [BYTES seg2:0x196 seg2:0x1a5 seg2:0x1c3 seg2:0x1dc seg2:0x1ea seg2:0x1f4 seg2:0x103]

### WndProc (seg2:0x224, export ordinal 1)
- It takes Pascal arguments: hwnd at `[bp+0xe]`, msg at `[bp+0xc]`, wParam at `[bp+0xa]` and lParam at `[bp+6]`. [BYTES seg2:0x297 seg2:0x38d seg2:0x445 seg2:0x450]
- It dispatches with a compare chain:

  | Message | Action | Evidence |
  |---|---|---|
  | WM_CREATE | stores hwnd in DGROUP:0x16, calls seg6:0x18e0, sets DGROUP:0x1c = 1 | [BYTES seg2:0x29b seg2:0x390 seg2:0x393 seg2:0x398] |
  | WM_DESTROY | seg6:0x19bc, then PostQuitMessage(0) | [BYTES seg2:0x2a0 seg2:0x833 seg2:0x83a] |
  | WM_MOVE | seg2:0x11c0 | [BYTES seg2:0x2a5 seg2:0x81b] |
  | WM_SIZE | seg2:0xa74, then DefWindowProc | [BYTES seg2:0x2ab seg2:0x43f] |
  | WM_SETFOCUS | seg2:0xa06 when the window is not iconic and DGROUP:0x8f68 = 0 | [BYTES seg2:0x2b1 seg2:0x7fa seg2:0x803 seg2:0x80d] |
  | WM_PAINT | when DGROUP:0x1e ≠ 0: seg2:0x970 while DGROUP:0x90f4 is 1 or 7, otherwise posts private message 0x978. Then DefWindowProc | [BYTES seg2:0x2b8 seg2:0x7ac seg2:0x7c4 seg2:0x7d7] |
  | WM_ENDSESSION | seg6:0x19bc when wParam ≠ 0, then DefWindowProc | [BYTES seg2:0x2bf seg2:0x823 seg2:0x82b] |
  | WM_GETMINMAXINFO | when DGROUP:0x22 ≠ 0, minimum track size 600 × 400. Otherwise minimum and maximum track size = the current window size | [BYTES seg2:0x2c6 seg2:0x3eb seg2:0x3f6 seg2:0x410 seg2:0x422] |
  | WM_KEYDOWN, WM_KEYUP, MM_JOY1MOVE, MM_JOY1BUTTONDOWN, MM_JOY1BUTTONUP, private 0x99b, 0x99e and 0x9c5 | input path (below) | [BYTES seg2:0x2d4 seg2:0x316 seg2:0x323 seg2:0x345 seg2:0x366] |
  | WM_COMMAND | seg2:0x13b2 when DGROUP:0x1212 ≠ 0 | [BYTES seg2:0x2db seg2:0x646 seg2:0x655] |
  | WM_SYSCOMMAND | seg2:0xd78(wParam); DefWindowProc runs only when it returns non-zero | [BYTES seg2:0x2e0 seg2:0x63b seg2:0x63f seg2:0x643] |
  | WM_TIMER | seg2:0x11f8 | [BYTES seg2:0x2e5 seg2:0x814] |
  | WM_VSCROLL | seg11:0x7798 | [BYTES seg2:0x2eb seg2:0x6ae] |
  | WM_MOUSEMOVE | input path when DGROUP:0x8e5e ≠ 0, DGROUP:0xd24 = 0 and the cursor is far from DGROUP:0x1d48: \|dx\| > 6, or \|dy\| > 6 with \|dx\| > 2. Otherwise DefWindowProc | [BYTES seg2:0x2f2 seg2:0x6ba seg2:0x6e4 seg2:0x6ea seg2:0x6ef seg2:0x6fa] |
  | WM_LBUTTONDOWN | seg16:0x638a | [BYTES seg2:0x2f7 seg2:0x683] |
  | WM_LBUTTONDBLCLK | seg2:0x1046 when DGROUP:0x1212 ≠ 0 and DGROUP:0x22 ≠ 0 | [BYTES seg2:0x2fd seg2:0x65e seg2:0x665 seg2:0x671] |
  | WM_RBUTTONDOWN | seg2:0x134c when DGROUP:0x1212 ≠ 0 | [BYTES seg2:0x302 seg2:0x68e seg2:0x6a1] |
  | WM_QUERYNEWPALETTE | when DGROUP:0x920e ≠ 0: selects and realizes that palette, then invalidates and updates the window | [BYTES seg2:0x309 seg2:0x5cb seg2:0x5e6 seg2:0x5f1 seg2:0x5ff seg2:0x607] |
  | WM_PALETTECHANGED | when DGROUP:0x920e ≠ 0 and wParam is not this window: selects and realizes that palette, then UpdateColors | [BYTES seg2:0x30f seg2:0x56d seg2:0x577 seg2:0x58f seg2:0x59a seg2:0x5a2] |
  | private 0x914 | ShowWindow(SW_SHOWNORMAL) and SetActiveWindow | [BYTES seg2:0x329 seg2:0x7e5 seg2:0x7ee] |
  | private 0x978 | seg2:0x970 | [BYTES seg2:0x330 seg2:0x7a6] |
  | private 0x9cf | gets a DC into DGROUP:0x13dc, selects the palette, calls seg10:0x6e26, seg10:0x6e00 and seg10:0x6e76(8, 2), then posts 0x9d0 | [BYTES seg2:0x36d seg2:0x458 seg2:0x466 seg2:0x469 seg2:0x478 seg2:0x484 seg2:0x4b6] |
  | private 0x9d0 | sets DGROUP:0x8f66 = 1 (idle frames allowed) | [BYTES seg2:0x372 seg2:0x4be] |
  | anything else | DefWindowProc | [BYTES seg2:0x2cd seg2:0x38a seg2:0x450] |

- Private messages 0x991, 0x9a0, 0x9a5, 0x9aa, 0x9b4, 0x9f0, 0x9fa and 0xa04 have handlers of their own, whose roles are [OPEN]. [BYTES seg2:0x337 seg2:0x34a seg2:0x351 seg2:0x358 seg2:0x35f seg2:0x379 seg2:0x386]

### Input path
- When DGROUP:0x90f8 ≠ 0 and DGROUP:0x8e5e ≠ 0, any input more than 4000 ms after the start time DGROUP:0x1d3c destroys the main window. Earlier input is dropped. [BYTES seg2:0x70e seg2:0x715 seg2:0x729 seg2:0x732 seg2:0x73a seg2:0x742]
- When DGROUP:0x90f8 or DGROUP:0x8e5e is 0, the input needs DGROUP:0x1212 ≠ 0, DGROUP:0x8f68 ≠ 0 and DGROUP:0x3b94 ≤ DGROUP:0x1e50. [BYTES seg2:0x713 seg2:0x71a seg2:0x74a seg2:0x751 seg2:0x75c]
  - With no frame running (DGROUP:0x968c = 0), WndProc stores the message as pending input and calls GameFrame. The pending input is DGROUP:0x8e60 = 1, msg in DGROUP:0x8e62, wParam in DGROUP:0x8e64 and lParam in DGROUP:0x1d38. [BYTES seg2:0x766 seg2:0x783 seg2:0x78c seg2:0x792 seg2:0x799 seg2:0x79d]
  - During a frame, it stores msg in DGROUP:0x8e62 and calls the input router seg11:0x6aa8(msg, wParam, lParam) at once. [BYTES seg2:0x770 seg2:0x77b]
- **Input router seg11:0x6aa8** (Pascal, `retf 8`). It does nothing when DGROUP:0x8e62 = 0. While DGROUP:0x9130 ≠ 0 it passes the input to seg18:0xd4e. Otherwise it remaps the key code of WM_KEYDOWN and WM_KEYUP through seg11:0xb306 and dispatches on DGROUP:0x90f4: state 0 → seg11:0x7902, 1 → seg11:0x6b64, 2 → seg11:0x7b7a, 3 → seg11:0x7bbc, 6 → seg11:0x785e. It then clears DGROUP:0x8e62. [BYTES seg11:0x6ab2 seg11:0x6acc seg11:0x6ae8 seg11:0x6b42 seg11:0x6b54 seg11:0x6b60]
- seg11:0x6b64, the gameplay input handler, compares msg and wParam directly. No jump table switches on a key code there. [BYTES seg11:0x6b72 seg11:0x6b7b seg11:0x6cd5 seg11:0x6cf4]

### WM_COMMAND handler (seg2:0x13b2)
- It turns command ids into posted messages or direct calls. The ids come from the menus 101, 102 and POPUPMENU and the accelerator table MAINACCEL of ENG_LANG.DLL. Menu 102 holds the items of menu 101 plus the Strategy popup (ids 0x6c–0x71). [BYTES seg2:0x13bf seg2:0x15a0] [BYTES ENG_LANG.DLL MENU 101] [BYTES ENG_LANG.DLL MENU 102] [BYTES ENG_LANG.DLL MENU POPUPMENU] [BYTES ENG_LANG.DLL ACCELERATOR MAINACCEL]

  | Id | Menu item or key | Action | Evidence |
  |---|---|---|---|
  | 0x67 | Abort | WM_KEYUP, VK_ESCAPE | [BYTES seg2:0x15a3 seg2:0x14d2] |
  | 0x68 | Options.. | seg10:0x7f18 | [BYTES seg2:0x15a8 seg2:0x14ca] |
  | 0xc0 | Game Options.. (POPUPMENU), Alt+G | seg10:0x7f18 | [BYTES seg2:0x162c seg2:0x14ca] |
  | 0x69 | Action Mode | WM_KEYUP, VK_F12 | [BYTES seg2:0x15ad seg2:0x14de] |
  | 0x6a | Exit | WM_SYSCOMMAND, SC_CLOSE | [BYTES seg2:0x15b2 seg2:0x14ea] |
  | 0xb0 | Exit Game (POPUPMENU), Alt+X | WM_SYSCOMMAND, SC_CLOSE | [BYTES seg2:0x1625 seg2:0x14ea] |
  | 0x6b | Save.. | seg10:0x8568(2) while DGROUP:0x90f4 = 1, seg10:0x8568(0) while it is 2 | [BYTES seg2:0x15b7 seg2:0x14f7 seg2:0x14fe seg2:0x1500 seg2:0x1509 seg2:0x1512] |
  | 0x1a6 | Alt+S | as 0x6b | [BYTES seg2:0x166c seg2:0x14f7] |
  | 0x6c–0x71 | Strategy: Capture, Attack, Defend, Refuel, Self-Preserve, Exit Wave | WM_KEYUP with `Q`, `A`, `D`, `G`, `S`, `E` | [BYTES seg2:0x15bc seg2:0x1516 seg2:0x1548] |
  | 0x73 | Challenge to Fight | private 0x99e, `F` | [BYTES seg2:0x15db seg2:0x147a] |
  | 0x74 | Icons | private 0x99b, `I` | [BYTES seg2:0x15e0 seg2:0x13c5] |
  | 0x1eb | Alt+I | private 0x99b, `I` | [BYTES seg2:0x1696 seg2:0x13c5] |
  | 0x75 | One Ship | WM_KEYUP, `W` | [BYTES seg2:0x15e5 seg2:0x13f0] |
  | 0x1e1 | Alt+W | private 0x99b, `W` | [BYTES seg2:0x168f seg2:0x1427 seg2:0x13f7] |
  | 0x76 | All Ships | private 0x99b, `R` | [BYTES seg2:0x15ea seg2:0x13fb] |
  | 0x1d7 | Alt+R | private 0x99b, `R` | [BYTES seg2:0x1688 seg2:0x13fb] |
  | 0x77–0x7b | Help: General, Scores, This Game, Target, Weapons | DGROUP:0x93c6 = 4, 5, 6, 1, 0, then seg2:0x88e | [BYTES seg2:0x1559 seg2:0x1580 seg2:0x1598 seg2:0x1560] |
  | 0xf0 | F1 | when DGROUP:0xa7c ≠ 0, as 0x77: DGROUP:0x93c6 = 4, then seg2:0x88e. Otherwise seg2:0x190a(0, 0) | [BYTES seg2:0x164b seg2:0x1552 seg2:0x1559 seg2:0x1560 seg2:0x1566 seg2:0x156b] |
  | 0x100 | F2 | seg2:0x190a(0, 0) | [BYTES seg2:0x1652 seg2:0x1566 seg2:0x156b] |
  | 0x7c–0x82 | Ship Cmds: Capture, Attack, Defend, Refuel, Self-Preserve, Exit Wave, Help Me | private 0x99e with `Q`, `A`, `D`, `G`, `S`, `E`, `H` | [BYTES seg2:0x1600 seg2:0x1430 seg2:0x1471] |
  | 0xc9, 0xca, 0xcb | About Inner Space, User Manual, Playing Inner Space | seg2:0x190a with first argument 0x1c, 0 and 5 | [BYTES seg2:0x1633 seg2:0x1638 seg2:0x163d seg2:0x156b] |
  | 0x191 | Alt+=, Alt with keypad + | private 0x99b, VK_ADD | [BYTES seg2:0x1656 seg2:0x13da seg2:0x13e1] |
  | 0x19b | Alt+-, Alt with keypad − | private 0x99b, VK_SUBTRACT | [BYTES seg2:0x165d seg2:0x13e5 seg2:0x13ec] |
  | 0x19c | Alt+B | restores caption style bits when DGROUP:0x20 ≠ 0, then WM_SYSCOMMAND SC_MINIMIZE | [BYTES seg2:0x1665 seg2:0x1486 seg2:0x14b1] |
  | 0x1b9, 0x1c3, 0x1cd | Defenders: Attack, Help Me, Exit Wave; Alt+A, Alt+H, Alt+E | private 0x99b with `A`, `H`, `E` | [BYTES seg2:0x1673 seg2:0x167a seg2:0x1681] |
  | 0xe0 | none in ENG_LANG.DLL; the sender is [OPEN] | private 0x8e2 | [BYTES seg2:0x1644 seg2:0x14be seg2:0x14c2] |

- Most commands reach the game as key events through the input path: the handler posts WM_KEYUP or the private message 0x99b or 0x99e to the main window. The others call routines directly or post WM_SYSCOMMAND or 0x8e2. [BYTES seg2:0x13d2 seg2:0x2d4 seg2:0x14ca seg2:0x1500 seg2:0x1560 seg2:0x156b]

### WM_TIMER handler and GameFrame
- WM_TIMER goes to seg2:0x11f8 and runs no game step. GameFrame (seg2:0x1c08) runs the game tick seg7:0x0, or seg17:0x60d8 while DGROUP:0x9460 ≠ 0, with a DC in DGROUP:0x13dc, and sets DGROUP:0x968c while it runs. Both are described in [timing.md](timing.md). [BYTES seg2:0x814 seg2:0x1c72 seg2:0x1cdb seg2:0x1ce2 seg2:0x1d4a]
- The first timer tick after WM_CREATE sets DGROUP:0x1e = 1, which enables WM_PAINT handling. [BYTES seg2:0x1205 seg2:0x123b seg2:0x7ac]

### Game tick (seg7:0x0)
- The tick samples the clocks and runs the per-pass work of the current state. The clocks, the frame gate and each state's frame function are in [timing.md](timing.md#per-pass-work-in-seg70x0). [BYTES seg7:0x24 seg7:0x497]
- **State changes.** Code requests a change by storing the next state in DGROUP:0x968e and setting DGROUP:0x13de. GameFrame then calls seg2:0x1d58, which copies DGROUP:0x968e into DGROUP:0x90f4 ([timing.md](timing.md#message-loop-and-idle-step)). [BYTES seg2:0x1c64 seg2:0x1db0 seg2:0x1db6 seg2:0x1db9]
  - The requested state is 0 (seg7:0xe9, seg7:0x402, seg7:0x69a, seg11:0x6c61, seg11:0x78e7), 1 (seg6:0x36a0) or 4 (seg7:0x44d). [BYTES seg7:0xe3 seg7:0xe9 seg7:0x402 seg7:0x69a seg11:0x6c61 seg11:0x78e7 seg6:0x369a seg6:0x36a0 seg7:0x44d]
  - Most changes instead write DGROUP:0x90f4 directly ([Game state globals](#game-state-globals)).
- When DGROUP:0x8f6c ≠ 0 the tick requests state 0, clears DGROUP:0x8f6c and the timers DGROUP:0x1e80 and DGROUP:0x1e84, and calls seg9:0xd6c. [BYTES seg7:0xd6 seg7:0xdd seg7:0xe9 seg7:0xec seg7:0xf0 seg7:0xf4]
- It then dispatches on the game state DGROUP:0x90f4 through a 10-entry jump table. "Pending input" below means a call with (DGROUP:0x8e62, DGROUP:0x8e64, DGROUP:0x1d38) while DGROUP:0x8e62 ≠ 0. [BYTES seg7:0xf9 seg7:0x48f seg7:0x497]
- The table lists where each case sends pending input and its work besides the frame function, frame gate and deadline checks of [timing.md](timing.md#the-frame-gate).

  | State | Case | Pending input to | Other work | Evidence |
  |---|---|---|---|---|
  | 0 | seg7:0x20a | seg11:0x7902 | DGROUP:0x22 = 0 when DGROUP:0x9294 ≠ 0. seg6:0x1e68(0) instead of the frame when DGROUP:0xd1e ≠ 0, DGROUP:0x9134 ≠ 0 and DGROUP:0x95bc = 0 | [BYTES seg7:0x20a seg7:0x211 seg7:0x22b seg7:0x233 seg7:0x23a seg7:0x241 seg7:0x24a] |
  | 1, 7 and any value outside 0–9 | seg7:0x4b0 | seg11:0x6aa8. A pass that routes pending input (DGROUP:0x8e60 ≠ 0) does nothing else | the wave pass and frame ([timing.md](timing.md#the-frame-gate)). GameFrame does not call the tick in state 7 ([Game state globals](#game-state-globals)) | [BYTES seg7:0x492 seg7:0x4e6 seg7:0x50e seg7:0x513 seg7:0x519] |
  | 2 | seg7:0x363 | seg11:0x7b7a | seg6:0x1e68(0) instead of the rest of the pass when DGROUP:0xd1e ≠ 0 and DGROUP:0x95bc = 0. seg9:0x5cca or seg6:0x1c72(1) when one of the state's two deadlines passes | [BYTES seg7:0x363 seg7:0x36a seg7:0x36f seg7:0x38f seg7:0x3ac seg7:0x3c8] |
  | 3 | seg7:0x2cc | seg11:0x7bbc | seg9:0x60bc when the state's deadline passes | [BYTES seg7:0x2e8 seg7:0x33a] |
  | 4 | seg7:0x3f1 | seg11:0x78bc | none | [BYTES seg7:0x421] |
  | 5 | seg7:0x42c | seg11:0x7b30 | at the state's deadline, seg9:0xd6c and seg6:0x626c and DGROUP:0x1a = 0 before the change to state 4 | [BYTES seg7:0x437 seg7:0x43c seg7:0x441 seg7:0x46a] |
  | 6 | seg7:0xff | seg11:0x785e | DGROUP:0x22 = 0 | [BYTES seg7:0xff seg7:0x119] |
  | 8 | seg7:0x197 | none | DGROUP:0x22 = 0 | [BYTES seg7:0x197] |
  | 9 | seg7:0x1bf | none | DGROUP:0x22 = 0. seg6:0x5d58 when the state's deadline passes | [BYTES seg7:0x1bf seg7:0x1e1] |

- The gameplay frame enters state 6 when the timer DGROUP:0x1e88 expires, `iMainSmart` = DGROUP:0x952e and DGROUP:0x951a = 0. It sets DGROUP:0x914a = 0 and runs seg16:0xcfe at once. [BYTES seg7:0x5ce seg7:0x5ea seg7:0x5f3 seg7:0x5fa seg7:0x600 seg7:0x608]
- When the timer DGROUP:0x1e80 expires with DGROUP:0x95bc = 0, it enters state 8 if DGROUP:0x93a4 ≠ 0 and otherwise requests state 0. [BYTES seg7:0x610 seg7:0x62b seg7:0x652 seg7:0x659 seg7:0x694 seg7:0x69a]

### Jump tables
- The five largest jump tables that `isre asm` resolves in INSPACE are below. The game tick's 10-entry table at seg7:0x497 is described under [Game tick](#game-tick-seg70x0).

  | Jump | Entries | Switch operand | Function | Evidence |
  |---|---|---|---|---|
  | seg3:0x1351 | 192 | message code − 1 | seg3:0x6ec, the message dispatcher | [BYTES seg3:0x838 seg3:0x1346 seg3:0x1347 seg3:0x1351] |
  | seg16:0x1e69 | 65 | DGROUP:0x914a | seg16:0xcfe, the state 6 frame function | [BYTES seg16:0xd71 seg16:0x1e5f seg16:0x1e69] |
  | seg11:0xadb5 | 23 | the player's byte at ship record +0xb8 + (word +0x18), minus 1 | seg11:0xac86 | [BYTES seg11:0xad2d seg11:0xad31 seg11:0xadac seg11:0xadb5] |
  | seg17:0x350 | 19 | [OPEN] | seg17:0x0, the state 3 frame function | [BYTES seg17:0x348 seg17:0x350] |
  | seg17:0x5d1 | 17 | [OPEN] | seg17:0x3b2 | [BYTES seg17:0x5c8 seg17:0x5d1] |

- **seg3:0x6ec, the message dispatcher.** It takes cdecl arguments (recipient, sender pool, sender record, code). It does nothing unless the recipient is `iMainSmart`, and nothing when the sender is the player's own ship in pool 14. [BYTES seg3:0x6fa seg3:0x700 seg3:0x704 seg3:0x70a] seg4:0x70cf, for example, passes (`iMainSmart`, 14, ship, 0x88). [BYTES seg4:0x70c6 seg4:0x70cf]
- Many cases format an ENG_LANG string into DGROUP:0x8db0 through seg10:0xa332. Codes 190 and 191 pick ids 2005 and 2002. [BYTES seg3:0x8d9 seg3:0x8dd seg3:0x130b seg3:0x1313] [BYTES ENG_LANG.DLL STRING 126] ids 2002–2005. The Target display shows messages from other ships. [MANUAL General]

## Game state globals
- **DGROUP:0x90f4, the game state.** It selects the tick case (table above). [BYTES seg7:0xf9 seg7:0x497]
  - Writers of each value: 0 at seg6:0x18f0. 1 at seg9:0x60ff, seg16:0x1bcf, seg16:0x6ac5 and seg17:0x5fff. 2 at seg9:0x6ce5, 3 at seg9:0x5d19, 4 at seg9:0xd9b and 5 at seg6:0x5dbd. 6 at seg7:0x5fa and seg18:0xff, 7 at seg17:0x57f5, 8 at seg7:0x659 and seg9:0x6c62, 9 at seg2:0xe2d, seg10:0x8824 and seg16:0x1bc7. seg2:0x1db9 copies the requested next state, which is 0, 1 or 4 ([Game tick](#game-tick-seg70x0)). [BYTES seg6:0x18ee seg6:0x18f0 seg9:0x60ff seg16:0x1bcf seg16:0x6ac5 seg17:0x5fff seg9:0x6ce5 seg9:0x5d19 seg9:0xd9b seg6:0x5dbd] [BYTES seg7:0x5fa seg18:0xff seg17:0x57f5 seg7:0x659 seg9:0x6c62 seg2:0xe2d seg10:0x8824 seg16:0x1bc7 seg2:0x1db9]
  - **State 7.** seg17:0x5782 is the window procedure of class IS_HELP [BYTES seg6:0x1645 seg6:0x168b seg6:0x1698]. On WM_CREATE it disables the main window, sets state 7 and stores its window in DGROUP:0x9460. On WM_CLOSE it enables the main window again, sets state 1, destroys the window and clears DGROUP:0x9460 [BYTES seg17:0x5791 seg17:0x5795 seg17:0x57a3 seg17:0x57cc seg17:0x57f0 seg17:0x57f5 seg17:0x57fe seg17:0x5ffa seg17:0x5fff seg17:0x6009 seg17:0x600e]. seg2:0x88e creates that window, titled string 2182 "Inner Space Online Help", after the caller sets the help topic DGROUP:0x93c6 [BYTES seg2:0x8e9 seg2:0x8f9 seg2:0x92a seg2:0x1559 seg2:0x1560] [BYTES ENG_LANG.DLL STRING 137] id 2182. seg17:0x57f5 is the only write of 7 to DGROUP:0x90f4, and seg17:0x57fe and seg17:0x600e are the only writes of DGROUP:0x9460. [BYTES seg17:0x57f5 seg17:0x57fe seg17:0x600e]
  - While DGROUP:0x9460 is non-zero, GameFrame runs seg17:0x60d8, a gated step of seg17:0x0, instead of the tick. So the tick's gameplay case does not run in state 7, and the input router has no case for state 7. [BYTES seg2:0x1cd4 seg2:0x1cdb seg17:0x60e9 seg17:0x610b seg11:0x6b42 seg11:0x6b52] State 7 is the Online Help window.
  - Which screens states 2–6, 8 and 9 stand for is [OPEN].
- **DGROUP:0x9320, the wave kind.**
  - Only the wave generator seg9:0x3202 writes it. Every write stores one of the constants −1, 0, 2, 3, 4, 5, 6 and 7, except seg9:0x3928, which stores `rand()` % 8 − 2. [BYTES seg9:0x364e seg9:0x3972 seg9:0x3769 seg9:0x3719 seg9:0x36de seg9:0x3756 seg9:0x3ac4 seg9:0x3e00 seg9:0x366f seg9:0x36a6 seg9:0x3928]
  - seg9:0x6fee starts the wave title with "RACE TRACK" for 6 and "DUEL" for 7, and with "DIRECTORY" in every other case. A path in DGROUP:0x38c8 with `:` as its second character also gets "DIRECTORY". [BYTES seg9:0x7035 seg9:0x703c seg9:0x7043 seg9:0x7054 seg9:0x705b seg9:0x706b] [BYTES ENG_LANG.DLL STRING 140] id 2239, [BYTES ENG_LANG.DLL STRING 135] id 2146, [BYTES ENG_LANG.DLL STRING 20] id 318
  - The manual describes race tracks and duelling directories as special waves. [MANUAL Race Courses] [MANUAL Duelling]
  - Most of its readers compare it with 6 or 7. What the other values mean is [OPEN]. In probe-0001 it reads 0 in all six dumps. [ORACLE probe-0001]
- **DGROUP:0x9684, the player's race place.**
  - seg4:0x8b82 computes it, with 1 = leading, and only while the player's pool-14 byte +0x51 is 0. It starts at 1 and adds 1 for each other ship that is ahead. A ship counts only when its ship-record byte +0xec is not 0 and its pool-14 `state` is 0. [BYTES seg4:0x8b99 seg4:0x8bda seg4:0x8c0d seg4:0x8c2b seg4:0x8c3f seg4:0x8c50]
  - A ship is ahead when \|`laps`\| × DGROUP:0x9ea2 + `checkpoint` is larger than the player's. On a tie, the ship with the smaller word at DGROUP:0x8e48 + 2i is ahead. [BYTES seg4:0x8bc4 seg4:0x8bd0 seg4:0x8c08 seg4:0x8c25]
  - seg11:0x41bc prints it with a suffix from seg17:0x7ba6 ("ST", "ND", "RD", "TH") and " PLACE". [BYTES seg11:0x4274 seg11:0x427f seg11:0x4283 seg11:0x4291] [BYTES ENG_LANG.DLL STRING 131] id 2095, [BYTES ENG_LANG.DLL STRING 132] ids 2096–2098, [BYTES ENG_LANG.DLL STRING 138] id 2195
  - The race bonus reads it ([Scoring](#scoring)). In probe-0001 it reads 0 in every dump. [ORACLE probe-0001]
- **DGROUP:0x90f8.** seg6:0x1c0c sets it to 1 when the command line (DGROUP:0x1d60) is `DEMO` or `PLAY`, and posts private message 0x9f0 or 0x9fa. [BYTES seg6:0x1c16 seg6:0x1c24 seg6:0x1c2d seg6:0x1c37 seg6:0x1c4a seg6:0x1c5d]

## Scoring
- **DGROUP:0x1e58 (dword), the score.**
  - The status display shows the score. [MANUAL High Scores] seg11:0x4ea draws it when it differs from the copy in DGROUP:0x5018, and draws "000" when it is 0. [BYTES seg11:0x4f7 seg11:0x4ff seg11:0x508 seg11:0x519 seg11:0x528]
  - seg9:0xc06 clears it and the rating when DGROUP:0x92e4 = 0. seg6:0x930a, which reads a `.sav` file, loads both. [BYTES seg9:0xc44 seg9:0xc4e seg9:0xc52 seg6:0x93bc seg6:0x968a seg6:0x969b]
  - Additions:

    | Site | Amount | Evidence |
    |---|---|---|
    | seg8:0x7c3b | kill points P[pool] (below) | [BYTES seg8:0x7c3b] |
    | seg4:0x2ef8 | race bonus (below) | [BYTES seg4:0x2ef8] |
    | seg7:0x196a | 800 | [BYTES seg7:0x196a] |
    | seg8:0x6785, seg8:0x7971 | 3000 | [BYTES seg8:0x6785 seg8:0x7971] |
    | seg8:0x67a6, seg13:0x1b4a | 1000 | [BYTES seg8:0x67a6 seg13:0x1b4a] |
    | seg8:0x92cd | 400 | [BYTES seg8:0x92cd] |
    | seg16:0x7228 | 50000 | [BYTES seg16:0x7228] |
    | seg13:0x1c32 | a computed amount, [OPEN] | [BYTES seg13:0x1c32] |

  - What earns the fixed amounts is [OPEN].
- **DGROUP:0x5078 (dword), the rating.**
  - seg9:0x484c sets DGROUP:0x9334 to the number of entries in the 12-dword table at DGROUP:0x9bc that are below the rating. The table holds 26000, 40000, 55000, 70000, 90000, 120000, 150000, 200000, 280000, 400000, 550000 and 700000. [BYTES seg9:0x485b seg9:0x4870 seg9:0x4878 seg9:0x4882 seg9:0x4888 DGROUP:0x9bc]
  - The wave generator seg9:0x3202 reads and adjusts DGROUP:0x9334. [BYTES seg9:0x35bc seg9:0x3654 seg9:0x38fc seg9:0x39b6]
  - It changes at these sites:

    | Site | Change | Evidence |
    |---|---|---|
    | seg8:0x7c34 | + P[pool], capped at 500000 | [BYTES seg8:0x7c1a seg8:0x7c22 seg8:0x7c29 seg8:0x7c34] |
    | seg8:0x7c57, seg8:0x7c8c, seg8:0x7cad, seg8:0x7cd0 | the other kill-scoring terms (below) | [BYTES seg8:0x7c57 seg8:0x7c8c seg8:0x7cad seg8:0x7cd0] |
    | seg4:0x2ef0 | + race bonus | [BYTES seg4:0x2ef0] |
    | seg13:0x1b53 | + 1000 | [BYTES seg13:0x1b53] |
    | seg13:0x1c3a | + the computed amount of seg13:0x1c32, [OPEN] | [BYTES seg13:0x1c3a] |
    | seg8:0x46f4 | damage to the player's ship (below) | [BYTES seg8:0x46f4 seg8:0x4736 seg8:0x476c] |
    | seg16:0xb19 | the player's ship is destroyed: halved, or × 4/5 when DGROUP:0x95bc ≠ 0 | [BYTES seg16:0xab4 seg16:0xabc seg16:0xafb seg16:0xb06 seg16:0xb16 seg16:0xb19] |
    | seg16:0x34e0 | × (w + 200) / 300, where w is word +0x26 of ship record [bp+6], when w < 40 and [bp+8] = 0. When this runs is [OPEN] | [BYTES seg16:0x34ac seg16:0x34b7 seg16:0x34c0 seg16:0x34cc seg16:0x34d6 seg16:0x34dd seg16:0x34e0] |

  - The manual does not describe the rating. What it stands for is [OPEN].
- **Kill scorer seg8:0x7be8** (pool, record, amount, destroyed).
  - The hit dispatcher calls it after `on_damage`, when the credited object is the player's ship and the target is not. seg8:0x7a5e and seg8:0x7d66 also call it, under conditions that are [OPEN]. [BYTES seg8:0x6489 seg8:0x6492 seg8:0x649a seg8:0x64ae seg8:0x7a5e seg8:0x7d66]
  - It returns at once when the target is the player's ship. [BYTES seg8:0x7bf3 seg8:0x7bfb seg8:0x7c03]
  - **destroyed ≠ 0:** rating = min(rating + P[pool], 500000) and score += P[pool]. For the ship pool, rating += 2 × the ship's `kill_value`. For pool 8 with byte +0x54 ≠ 0, rating += P[8]. [BYTES seg8:0x7c11 seg8:0x7c34 seg8:0x7c3b seg8:0x7c50 seg8:0x7c57 seg8:0x7c6d seg8:0x7c79]
  - **destroyed = 0:** rating += W[pool] × amount. For pool 8 with word +0x44 > 2000, a further 2 × that product. For the ship pool, rating += `kill_value` / 8, rounded toward zero. Each product is 16-bit and sign-extended. [BYTES seg8:0x7c84 seg8:0x7c88 seg8:0x7c8b seg8:0x7ca2 seg8:0x7caa seg8:0x7cc2 seg8:0x7ccc]
  - P is the word table at DGROUP:0xb0e: 1000, 600, 1200, 900, 200, 0, 1000, 0, 3400, 500, 500, 0, 0, 5000, then 0 for pools 14–20. [BYTES DGROUP:0xb0e]
  - W is the word table at DGROUP:0xb38: 1, 1, 1, 2, 0, 0, 0, 0, 4, 3, 0, 0, 0, 0, 8, then 0 for pools 15–20. [BYTES DGROUP:0xb38]
- **Damage to the player's ship (seg8:0x45e0, the ship pool's `on_damage`).**
  - At seg8:0x468a it scales the amount d to d × 7 / 8. [BYTES seg8:0x468a seg8:0x4694 seg8:0x4697]
  - For the player's ship it scales d again by (DGROUP:0x9000 + 6) / 8. [BYTES seg8:0x469d seg8:0x46a5 seg8:0x46b4]
  - With DGROUP:0x95bc = 0: rating += min(rating, 35000) × d / −4000. When the rating is then above 200000, a further rating × d / −6000. [BYTES seg8:0x46c1 seg8:0x46d7 seg8:0x46e8 seg8:0x46ef seg8:0x46f4 seg8:0x4708 seg8:0x4713 seg8:0x4736]
  - With DGROUP:0x95bc ≠ 0: rating += min(rating, 35000) × d / −9000. [BYTES seg8:0x46bf seg8:0x473f seg8:0x4754 seg8:0x476c]
  - seg5:0x314 multiplies two 32-bit values. seg5:0x110 is a signed 32-bit divide: the dividend is the dword pushed last (`[bp+6]`) and the divisor the dword pushed first (`[bp+0xa]`). [BYTES seg5:0x118 seg5:0x130 seg5:0x14c seg5:0x14f]
- **Race bonus (seg4:0x2da2).**
  - It adds the same amount to the score and the rating. Let n = DGROUP:0x9316. [BYTES seg4:0x2ef0 seg4:0x2ef8]
  - Place 1 (DGROUP:0x9684 = 1) with n ≠ 0: ((9 − DGROUP:0x94aa) × n + 4 × (DGROUP:0x94a8 + 5)) × 500. [BYTES seg4:0x2e98 seg4:0x2e9f seg4:0x2ea6 seg4:0x2eb8 seg4:0x2ebd]
  - Place 2 with n > 1: (8 + DGROUP:0x94a8) × 500 + (8 − DGROUP:0x94aa) × n × 200. [BYTES seg4:0x2ec3 seg4:0x2eca seg4:0x2ed8 seg4:0x2ee9 seg4:0x2eed]
  - The products are 16-bit and sign-extended before the 32-bit add. [BYTES seg4:0x2eef]
  - It skips the bonus when the player's `laps` − DGROUP:0x9ea0 = 1. [BYTES seg4:0x2e0a seg4:0x2e0e seg4:0x2e13]
  - The wave generator writes constants 0–2 to DGROUP:0x94a8 and 0–5 to DGROUP:0x94aa, and race set-up code in seg6 writes both. What they stand for is [OPEN]. [BYTES seg9:0x3bbd seg9:0x3c0e seg9:0x3cba seg9:0x3cb1 seg9:0x3b2d seg6:0x3267 seg6:0x326a]

## Global table

| Address | Size | Type | Meaning | Writers | Readers |
|---|---|---|---|---|---|
| DGROUP:0x16 | 2 | HWND | main window [BYTES seg2:0x390] | seg2:0x390 | seg2:0x196, seg6:0xa02 |
| DGROUP:0x18 | 2 | BOOL | the message loop runs while non-zero (GetMessage result) [BYTES seg2:0xe7 seg2:0x189 seg2:0x1f9] | seg2:0xe7, seg2:0x189 | seg2:0x1f9 |
| DGROUP:0x1c | 2 | BOOL | first timer tick pending [BYTES seg2:0x398 seg2:0x1205 seg2:0x120e] | seg2:0x398, seg2:0x120e | seg2:0x1205 |
| DGROUP:0x1e | 2 | BOOL | WM_PAINT handling enabled [BYTES seg2:0x123b seg2:0x7ac] | seg2:0x123b | seg2:0x7ac |
| DGROUP:0x9bc | 48 | long[12] | rating thresholds, initialised data [BYTES DGROUP:0x9bc] | — | seg9:0x4870 |
| DGROUP:0xb0e | 42 | int[21] | kill points per pool, initialised data [BYTES DGROUP:0xb0e] | — | seg8:0x7c11 |
| DGROUP:0xb38 | 42 | int[21] | rating weight per pool, initialised data [BYTES DGROUP:0xb38] | — | seg8:0x7c84 |
| DGROUP:0x131a | 2 | BOOL | debug traces reach OutputDebugString; INI key `Aux` [BYTES seg6:0x300 seg13:0x4ed4] | seg6:0x300 | seg13:0x4ed4 |
| DGROUP:0x13dc | 2 | HDC | frame DC while GameFrame runs [BYTES seg2:0x1c7e seg2:0x1d44] | seg2:0x1c7e, seg2:0x1d44 | seg12:0x27e6 |
| DGROUP:0x13de | 2 | BOOL | state change pending: GameFrame calls seg2:0x1d58 instead of the tick [BYTES seg2:0x1c64 seg2:0x1db0 seg7:0xdd seg7:0x3fc] | seg7:0xdd, seg7:0x3fc, seg7:0x447, seg2:0x1db0 | seg2:0x1c64 |
| DGROUP:0x1d38 | 4 | LPARAM | pending input lParam [BYTES seg2:0x799] | seg2:0x799 | seg7:0x10c, seg18:0x1ef |
| DGROUP:0x1d3c | 4 | DWORD ms | start time [BYTES seg2:0xaa] | seg2:0xaa | seg2:0x721, seg2:0x126e |
| DGROUP:0x1d48 | 4 | POINT | cursor position saved at start [BYTES seg2:0x55 seg2:0x6c1] | seg2:0x55 | seg2:0x6c1, seg2:0x12a4 |
| DGROUP:0x1d60 | string | char[] | command line [BYTES seg2:0x24] | seg2:0x24 | seg6:0x1c1d |
| DGROUP:0x1df8 | 84 | far ptr[21] | object pools ([structs.md](structs.md#pool-table)) [BYTES seg6:0x1b8d seg18:0x1dfb] | seg6:0x1b8d, seg18:0x1dfb, seg7:0x5314 | seg18:0x20f, seg11:0x372b |
| DGROUP:0x1e30 | 4 | far ptr | pool 14 entry of that table. Code in most segments reads it directly instead of through DGROUP:0x9324 [BYTES seg4:0x8b95 seg7:0x11dc] | — | seg4:0x8b95, seg7:0x11dc |
| DGROUP:0x1e34 | 4 | far ptr | pool 15 entry, read only by seg18 ([structs.md](structs.md#pool-roles)) [BYTES seg18:0xe81] | — | seg18:0xe81 |
| DGROUP:0x1e4c | 4 | far ptr | locked 8-byte block, handle DGROUP:0x912a. Content [OPEN] [BYTES seg6:0x1bda seg6:0x1bec] | seg6:0x1bec | — |
| DGROUP:0x1e50 | 4 | DWORD ms | tick clock, GetCurrentTime at each tick ([timing.md](timing.md#per-pass-work-in-seg70x0)) [BYTES seg7:0x24] | seg7:0x24 | seg2:0x758, seg12:0x2ab4 |
| DGROUP:0x1e58 | 4 | long | score ([Scoring](#scoring)) | seg8:0x7c3b, seg9:0xc4e, seg6:0x968a | seg11:0x4f7 |
| DGROUP:0x1e68 | 4 | DWORD | wave frame counter ([timing.md](timing.md#game-time)) [BYTES seg7:0xb32 seg4:0x2e6e] | seg7:0xb32 | seg4:0x2e6e |
| DGROUP:0x1e80 | 4 | DWORD ms | due time that the gameplay frame acts on; set when the player's ship is destroyed [BYTES seg7:0x610 seg7:0x622 seg12:0x2adf seg16:0xae6] | seg12:0x2adf, seg16:0xae6 | seg7:0x622 |
| DGROUP:0x1e84 | 4 | DWORD ms | as DGROUP:0x1e80, used instead when DGROUP:0x95bc ≠ 0 [BYTES seg12:0x2ac1 seg16:0xacd seg7:0x6b8] | seg12:0x2ac1, seg16:0xacd | seg7:0x6b8, seg7:0x1d1 |
| DGROUP:0x1e88 | 4 | DWORD ms | due time that leads to state 6 [BYTES seg7:0x5ce seg7:0x5da] | — | seg7:0x5da |
| DGROUP:0x1e90 | 900 | struct[45] | effect table ([structs.md](structs.md#effect-table-45--0x14-bytes-at-dgroup0x1e90)) [BYTES seg18:0x9b6] | seg12:0x28d8, seg18:0x9b6 | seg7:0x129a |
| DGROUP:0x3b94 | 4 | DWORD ms | input is ignored until DGROUP:0x1e50 reaches it [BYTES seg2:0x75c] | — | seg2:0x75c |
| DGROUP:0x5018 | 4 | long | score as last drawn; −1 forces a redraw [BYTES seg11:0x508 seg9:0xcdd] | seg11:0x508, seg9:0xcdd | seg11:0x4ff |
| DGROUP:0x5078 | 4 | long | rating ([Scoring](#scoring)) | seg8:0x7c34, seg16:0xb19, seg9:0xc52 | seg9:0x4863 |
| DGROUP:0x7090 | 0xad0 | struct[8] | ship records ([structs.md](structs.md#ship-records-8--0x15a-bytes-at-dgroup0x7090)) [BYTES seg4:0x7084] | seg4:0x7091 | seg4:0x7084 |
| DGROUP:0x8e60 | 2 | BOOL | pending input present [BYTES seg2:0x783 seg7:0x4e6 seg7:0x513] | seg2:0x783, seg7:0x513 | seg7:0x4e6 |
| DGROUP:0x8e62 | 2 | UINT | pending input msg [BYTES seg2:0x78c seg11:0x6b54] | seg2:0x770, seg2:0x78c, seg11:0x6b54 | seg7:0x105, seg11:0x6ab2 |
| DGROUP:0x8e64 | 2 | WPARAM | pending input wParam [BYTES seg2:0x792] | seg2:0x792 | seg7:0x111 |
| DGROUP:0x8f66 | 2 | BOOL | idle frames allowed; private message 0x9d0 sets it [BYTES seg2:0x4be seg2:0x14a] | seg2:0x4be | seg2:0x14a, seg2:0x12b6 |
| DGROUP:0x8f68 | 2 | BOOL | 0 while the game is paused ([timing.md](timing.md#window-timers)); the input path ignores input then [BYTES seg2:0x9d7 seg2:0xa66 seg2:0x751] | seg2:0x9d7, seg2:0xa66 | seg2:0xc9, seg2:0x751 |
| DGROUP:0x90f4 | 2 | int | game state ([Game state globals](#game-state-globals)) | see that section | seg7:0xf9, seg11:0x6aef |
| DGROUP:0x90f8 | 2 | BOOL | command line was `DEMO` or `PLAY` [BYTES seg6:0x1c2d seg6:0x1c53] | seg6:0x1c16, seg6:0x1c2d, seg6:0x1c53 | seg2:0x70e |
| DGROUP:0x9100 | 42 | HGLOBAL[21] | pool handles [BYTES seg6:0x1bc2 seg18:0x1e2f] | seg6:0x1bc2, seg18:0x1e2f | seg6:0x1b7c |
| DGROUP:0x9130 | 2 | BOOL | seg18 game mode active ([structs.md](structs.md#pool-roles)). seg18 sets it to 1 and seg10:0xd clears it [BYTES seg18:0x397 seg18:0x8e1 seg10:0xd seg11:0x6abb] | seg18:0x397, seg18:0x8e1, seg10:0xd | seg11:0x6abb, seg7:0x58f |
| DGROUP:0x9144 | 2 | int | pass counter of the frame gate ([timing.md](timing.md#the-frame-gate)) [BYTES seg7:0x1a0 seg7:0x1b3] | seg7:0x1a0, seg7:0x1b3 | seg7:0x1a4 |
| DGROUP:0x9212 | 2 | HINSTANCE | ENG_LANG.DLL, the source of strings and accelerators [BYTES seg2:0xb1 seg3:0x223] [ORACLE probe-0001] | — | seg2:0xb1, seg3:0x223 |
| DGROUP:0x92ec | 42 | int[21] | allocated records per pool [BYTES seg7:0x52c7] | seg7:0x52c7, seg14:0xa33 | seg7:0x52de, seg12:0x276b |
| DGROUP:0x9316 | 2 | int | index of the last ship record (ships − 1) [BYTES seg14:0xa23 seg4:0x8c32] | seg14:0xa23 | seg4:0x8c32, seg7:0xb25 |
| DGROUP:0x9320 | 2 | int | wave kind: 6 race track, 7 duel ([Game state globals](#game-state-globals)) | seg9:0x3ac4, seg9:0x3e00 | seg9:0x703c, seg4:0x7071 |
| DGROUP:0x9324 | 2 | int | ship pool index, 14 or 20 [BYTES seg18:0x43b seg18:0x973 seg9:0x22c2] | seg9:0x22c2, seg18:0x43b, seg18:0x973 | seg18:0x208, seg8:0x643b |
| DGROUP:0x9326 | 2 | int | pool index 0, 1 or 15. Role [OPEN] [BYTES seg18:0x428 seg18:0x979] | seg18:0x428, seg18:0x979 | seg11:0x371a |
| DGROUP:0x9328 | 2 | int | pool index 5 or 17. Role [OPEN] [BYTES seg18:0x441 seg18:0x97f] | seg18:0x441, seg18:0x97f | seg8:0x1bb2 |
| DGROUP:0x9334 | 2 | int | rating level from the DGROUP:0x9bc thresholds [BYTES seg9:0x4882] | seg9:0x4882, seg9:0x38fc | seg9:0x3654 |
| DGROUP:0x934a | 2 | int | pool index 3 or 19. Role [OPEN] [BYTES seg18:0x447 seg18:0x985] | seg18:0x447, seg18:0x985 | seg7:0x52d4, seg8:0x5eb8 |
| DGROUP:0x9406 | 42 | int[21] | records in use per pool ([structs.md](structs.md#pool-table)) [BYTES seg11:0x376e] | seg7:0x10aa, seg14:0xa2f | seg11:0x376e, seg12:0x2761 |
| DGROUP:0x9460 | 2 | HWND | the Online Help window (class IS_HELP), open during state 7 ([Game state globals](#game-state-globals)) [BYTES seg17:0x57fe seg17:0x600e seg2:0x1ba] | seg17:0x57fe, seg17:0x600e | seg2:0x1ba, seg2:0x1c52 |
| DGROUP:0x9646 | 2 | int | `iMainSmart`, the player's ship record [BYTES seg18:0x203 seg18:0x213] | seg14:0x243, seg11:0xaf29 | seg18:0x203, seg3:0x6fa |
| DGROUP:0x9684 | 2 | int | player's race place ([Game state globals](#game-state-globals)) | seg4:0x8bda | seg4:0x2e98, seg11:0x427f |
| DGROUP:0x968c | 2 | BOOL | a frame is running [BYTES seg2:0x1c72 seg2:0x1d4a seg2:0x766] | seg2:0x1c72, seg2:0x1d4a | seg2:0x766 |
| DGROUP:0x968e | 2 | int | next game state for a pending change: 0, 1 or 4 [BYTES seg2:0x1db6 seg7:0x44d seg6:0x36a0] | seg7:0x44d, seg7:0x69a, seg6:0x36a0 | seg2:0x1db6 |
| DGROUP:0x9e60 | 4 × n | long[] | per-ship race time; at the finish the code stores the wave frame counter minus the value held there [BYTES seg4:0x2e6e seg4:0x2e7c seg4:0x2e84] | seg4:0x2e84 | seg4:0x2e7c |
| DGROUP:0x9ea0 | 2 | int | race length in laps, compared with `laps` [BYTES seg4:0x261d seg4:0x2dc8] | — | seg4:0x261d |
| DGROUP:0x9ea2 | 2 | int | race flags per lap [BYTES seg4:0x2161 seg4:0x8bc4] | — | seg4:0x2161, seg4:0x8bc4 |

- A "—" in the writer column means that no writer is listed here, not that none exists. [DESIGN]
